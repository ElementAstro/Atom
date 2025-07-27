#ifndef ATOM_EXTRA_INICPP_HPP
#define ATOM_EXTRA_INICPP_HPP

#include "common.hpp"
#include "convert.hpp"
#include "field.hpp"
#include "section.hpp"
#include "file.hpp"

// Additional headers needed for asynchronous functionality
#include <atomic>
#include <thread>
#include <chrono>
#include <array>
#include <memory>
#include <type_traits>
#include <concepts>
#include <cstdint>
#include <algorithm>
#include <functional>
#include <mutex>
#include <bit>
#include <string>
#include <string_view>
#include <vector>
#include <new>
#include <numeric>
#include <execution>
#include <future>
#include <fstream>
#include <sstream>

#if ATOM_HAS_SPDLOG
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <fmt/format.h>
#endif

#if INICPP_CONFIG_PATH_QUERY
#include "path_query.hpp"
#endif

#if INICPP_CONFIG_EVENT_LISTENERS
#include "event_listener.hpp"
#endif

#if INICPP_CONFIG_FORMAT_CONVERSION
#include "format_converter.hpp"
#endif

/**
 * @namespace inicpp
 * @brief 提供高性能、类型安全的INI配置文件解析功能
 *
 * 该库具有以下特点：
 * 1. 类型安全 - 通过模板获取强类型字段值
 * 2. 线程安全 - 使用共享锁实现并发读写
 * 3. 高性能 - 支持并行处理、内存池和Boost容器
 * 4. 可扩展 - 支持自定义分隔符、转义字符和注释前缀
 * 5. 丰富功能 - 支持嵌套段落、事件监听、路径查询、格式转换等
 *
 * 可通过宏控制功能开关：
 * - INICPP_CONFIG_USE_BOOST: 是否使用Boost库
 * - INICPP_CONFIG_USE_BOOST_CONTAINERS: 是否使用Boost容器
 * - INICPP_CONFIG_USE_MEMORY_POOL: 是否使用内存池
 * - INICPP_CONFIG_NESTED_SECTIONS: 是否支持嵌套段落
 * - INICPP_CONFIG_EVENT_LISTENERS: 是否支持事件监听
 * - INICPP_CONFIG_PATH_QUERY: 是否支持路径查询
 * - INICPP_CONFIG_FORMAT_CONVERSION: 是否支持格式转换
 */
namespace inicpp {

// ============================================================================
// SYNCHRONIZATION PRIMITIVES
// ============================================================================

namespace sync {

/**
 * @brief Hardware-specific optimizations for different architectures
 */
namespace hardware {
    inline void cpu_pause() noexcept {
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#elif defined(__aarch64__)
        __asm__ __volatile__("yield" ::: "memory");
#else
        std::this_thread::yield();
#endif
    }

    inline void memory_fence() noexcept {
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    inline void compiler_barrier() noexcept {
        std::atomic_signal_fence(std::memory_order_seq_cst);
    }
}

/**
 * @brief Adaptive spinlock optimized for INI file operations with exponential backoff
 */
class IniAdaptiveSpinLock {
private:
    alignas(64) std::atomic<bool> locked_{false};
    alignas(64) std::atomic<uint32_t> spin_count_{0};

    static constexpr uint32_t MAX_SPIN_COUNT = 2000;  // Optimized for INI operations
    static constexpr uint32_t YIELD_THRESHOLD = 50;   // Lower threshold for file I/O

public:
    /**
     * @brief Acquires the lock with adaptive spinning strategy optimized for INI operations
     */
    void lock() noexcept {
        uint32_t spin_count = 0;
        uint32_t backoff = 1;

        while (locked_.exchange(true, std::memory_order_acquire)) {
            ++spin_count;

            if (spin_count < YIELD_THRESHOLD) {
                // Active spinning with exponential backoff
                for (uint32_t i = 0; i < backoff; ++i) {
                    hardware::cpu_pause();
                }
                backoff = std::min(backoff * 2, 32u);  // Smaller max backoff for I/O
            } else if (spin_count < MAX_SPIN_COUNT) {
                // Yield to other threads
                std::this_thread::yield();
            } else {
                // Sleep for a short duration - optimized for file operations
                std::this_thread::sleep_for(std::chrono::microseconds(1));
                backoff = 1; // Reset backoff
            }
        }

        // Update statistics
        spin_count_.fetch_add(spin_count, std::memory_order_relaxed);

#if ATOM_HAS_SPDLOG
        if (spin_count > YIELD_THRESHOLD) {
            spdlog::debug("IniAdaptiveSpinLock: High contention detected, spin_count: {}", spin_count);
        }
#endif
    }

    /**
     * @brief Attempts to acquire the lock without blocking
     * @return true if lock was acquired, false otherwise
     */
    bool try_lock() noexcept {
        bool expected = false;
        return locked_.compare_exchange_strong(expected, true, std::memory_order_acquire);
    }

    /**
     * @brief Releases the lock
     */
    void unlock() noexcept {
        locked_.store(false, std::memory_order_release);
    }

    /**
     * @brief Gets the total spin count for performance analysis
     * @return Total number of spins performed
     */
    uint32_t get_spin_count() const noexcept {
        return spin_count_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Resets the spin count statistics
     */
    void reset_stats() noexcept {
        spin_count_.store(0, std::memory_order_relaxed);
    }
};

/**
 * @brief High-performance reader-writer lock optimized for INI file access patterns
 */
class IniReaderWriterLock {
private:
    alignas(64) std::atomic<int32_t> reader_count_{0};
    alignas(64) std::atomic<bool> writer_active_{false};
    alignas(64) std::atomic<bool> writer_waiting_{false};
    alignas(64) std::atomic<uint64_t> read_operations_{0};
    alignas(64) std::atomic<uint64_t> write_operations_{0};

public:
    /**
     * @brief Acquires a shared (read) lock optimized for INI field access
     */
    void lock_shared() noexcept {
        read_operations_.fetch_add(1, std::memory_order_relaxed);

        while (true) {
            // Wait for any active writer to finish
            while (writer_active_.load(std::memory_order_acquire) ||
                   writer_waiting_.load(std::memory_order_acquire)) {
                hardware::cpu_pause();
            }

            // Try to increment reader count
            int32_t current_readers = reader_count_.load(std::memory_order_relaxed);
            if (current_readers >= 0 &&
                reader_count_.compare_exchange_weak(current_readers, current_readers + 1,
                                                  std::memory_order_acquire)) {
                // Successfully acquired read lock
                break;
            }

            // Failed to acquire, yield and retry
            std::this_thread::yield();
        }

#if ATOM_HAS_SPDLOG
        spdlog::trace("IniReaderWriterLock: Read lock acquired, readers: {}",
                     reader_count_.load(std::memory_order_relaxed));
#endif
    }

    /**
     * @brief Releases a shared (read) lock
     */
    void unlock_shared() noexcept {
        reader_count_.fetch_sub(1, std::memory_order_release);

#if ATOM_HAS_SPDLOG
        spdlog::trace("IniReaderWriterLock: Read lock released, readers: {}",
                     reader_count_.load(std::memory_order_relaxed));
#endif
    }

    /**
     * @brief Acquires an exclusive (write) lock optimized for INI modifications
     */
    void lock() noexcept {
        write_operations_.fetch_add(1, std::memory_order_relaxed);

        // Signal that a writer is waiting
        writer_waiting_.store(true, std::memory_order_release);

        // Wait for exclusive access
        while (true) {
            bool expected_writer = false;
            if (writer_active_.compare_exchange_weak(expected_writer, true,
                                                   std::memory_order_acquire)) {
                // Wait for all readers to finish
                while (reader_count_.load(std::memory_order_acquire) > 0) {
                    hardware::cpu_pause();
                }
                break;
            }
            std::this_thread::yield();
        }

        writer_waiting_.store(false, std::memory_order_release);

#if ATOM_HAS_SPDLOG
        spdlog::trace("IniReaderWriterLock: Write lock acquired");
#endif
    }

    /**
     * @brief Releases an exclusive (write) lock
     */
    void unlock() noexcept {
        writer_active_.store(false, std::memory_order_release);

#if ATOM_HAS_SPDLOG
        spdlog::trace("IniReaderWriterLock: Write lock released");
#endif
    }

    /**
     * @brief Gets operation statistics for performance monitoring
     * @return Pair of (read_operations, write_operations)
     */
    std::pair<uint64_t, uint64_t> get_stats() const noexcept {
        return {read_operations_.load(std::memory_order_relaxed),
                write_operations_.load(std::memory_order_relaxed)};
    }

    /**
     * @brief Resets operation statistics
     */
    void reset_stats() noexcept {
        read_operations_.store(0, std::memory_order_relaxed);
        write_operations_.store(0, std::memory_order_relaxed);
    }
};

} // namespace sync

// ============================================================================
// LOCK-FREE CONTAINERS
// ============================================================================

namespace lockfree {

/**
 * @brief Memory ordering utilities for lock-free programming
 */
namespace memory_order {
    constexpr auto relaxed = std::memory_order_relaxed;
    constexpr auto consume = std::memory_order_consume;
    constexpr auto acquire = std::memory_order_acquire;
    constexpr auto release = std::memory_order_release;
    constexpr auto acq_rel = std::memory_order_acq_rel;
    constexpr auto seq_cst = std::memory_order_seq_cst;
}

/**
 * @brief Hazard pointer implementation for safe memory reclamation in INI operations
 */
template<typename T>
class HazardPointer {
private:
    static constexpr size_t MAX_THREADS = 64;
    static constexpr size_t HAZARD_POINTERS_PER_THREAD = 4;

    struct HazardRecord {
        alignas(64) std::atomic<T*> pointer{nullptr};
        alignas(64) std::atomic<std::thread::id> owner{std::thread::id{}};
    };

    static inline std::array<HazardRecord, MAX_THREADS * HAZARD_POINTERS_PER_THREAD> hazard_pointers_;
    static inline std::atomic<size_t> hazard_pointer_count_{0};

    thread_local static inline std::array<T*, HAZARD_POINTERS_PER_THREAD> local_hazards_{};
    thread_local static inline size_t local_hazard_count_ = 0;

public:
    /**
     * @brief Acquires a hazard pointer for the given object
     * @param ptr Pointer to protect
     * @return Index of the hazard pointer, or -1 if failed
     */
    static int acquire(T* ptr) noexcept {
        if (local_hazard_count_ >= HAZARD_POINTERS_PER_THREAD) {
            return -1;
        }

        auto thread_id = std::this_thread::get_id();

        // Find an available hazard pointer slot
        for (size_t i = 0; i < MAX_THREADS * HAZARD_POINTERS_PER_THREAD; ++i) {
            std::thread::id expected{};
            if (hazard_pointers_[i].owner.compare_exchange_strong(expected, thread_id,
                                                                memory_order::acquire)) {
                hazard_pointers_[i].pointer.store(ptr, memory_order::release);
                local_hazards_[local_hazard_count_] = ptr;
                return static_cast<int>(local_hazard_count_++);
            }
        }

        return -1; // No available slot
    }

    /**
     * @brief Releases a hazard pointer
     * @param index Index returned by acquire()
     */
    static void release(int index) noexcept {
        if (index < 0 || static_cast<size_t>(index) >= local_hazard_count_) {
            return;
        }

        auto thread_id = std::this_thread::get_id();

        // Find and release the hazard pointer
        for (size_t i = 0; i < MAX_THREADS * HAZARD_POINTERS_PER_THREAD; ++i) {
            if (hazard_pointers_[i].owner.load(memory_order::acquire) == thread_id &&
                hazard_pointers_[i].pointer.load(memory_order::acquire) == local_hazards_[index]) {
                hazard_pointers_[i].pointer.store(nullptr, memory_order::release);
                hazard_pointers_[i].owner.store(std::thread::id{}, memory_order::release);

                // Remove from local array
                for (size_t j = index; j < local_hazard_count_ - 1; ++j) {
                    local_hazards_[j] = local_hazards_[j + 1];
                }
                --local_hazard_count_;
                break;
            }
        }
    }

    /**
     * @brief Checks if a pointer is protected by any hazard pointer
     * @param ptr Pointer to check
     * @return true if protected, false otherwise
     */
    static bool is_protected(T* ptr) noexcept {
        for (size_t i = 0; i < MAX_THREADS * HAZARD_POINTERS_PER_THREAD; ++i) {
            if (hazard_pointers_[i].pointer.load(memory_order::acquire) == ptr) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Safely deletes a pointer if not protected
     * @param ptr Pointer to delete
     * @return true if deleted, false if protected
     */
    static bool safe_delete(T* ptr) noexcept {
        if (!is_protected(ptr)) {
            delete ptr;
            return true;
        }
        return false;
    }
};

/**
 * @brief Lock-free hash map optimized for INI section and field storage
 */
template<typename Key, typename Value, typename Hash = std::hash<Key>>
class LockFreeHashMap {
private:
    struct Node {
        alignas(64) std::atomic<Node*> next{nullptr};
        Key key;
        Value value;
        std::atomic<bool> deleted{false};
        mutable std::mutex value_mutex;

        Node(const Key& k, const Value& v) : key(k), value(v) {}
    };

    static constexpr size_t DEFAULT_BUCKET_COUNT = 1024;
    static constexpr double MAX_LOAD_FACTOR = 0.75;

    std::unique_ptr<std::atomic<Node*>[]> buckets_;
    size_t bucket_count_;
    std::atomic<size_t> size_{0};
    Hash hasher_;

    size_t get_bucket_index(const Key& key) const noexcept {
        return hasher_(key) % bucket_count_;
    }

    Node* find_node(const Key& key) const noexcept {
        size_t bucket_idx = get_bucket_index(key);
        Node* current = buckets_[bucket_idx].load(memory_order::acquire);

        while (current != nullptr) {
            if (!current->deleted.load(memory_order::acquire) && current->key == key) {
                return current;
            }
            current = current->next.load(memory_order::acquire);
        }

        return nullptr;
    }

public:
    explicit LockFreeHashMap(size_t bucket_count = DEFAULT_BUCKET_COUNT)
        : bucket_count_(bucket_count), hasher_() {
        buckets_ = std::make_unique<std::atomic<Node*>[]>(bucket_count_);
        for (size_t i = 0; i < bucket_count_; ++i) {
            buckets_[i].store(nullptr, memory_order::relaxed);
        }

#if ATOM_HAS_SPDLOG
        spdlog::debug("LockFreeHashMap: Initialized with {} buckets", bucket_count_);
#endif
    }

    ~LockFreeHashMap() {
        clear();
    }

    /**
     * @brief Inserts or updates a key-value pair
     * @param key The key to insert/update
     * @param value The value to associate with the key
     * @return true if a new key was inserted, false if existing key was updated
     */
    bool insert_or_update(const Key& key, const Value& value) {
        size_t bucket_idx = get_bucket_index(key);

        while (true) {
            Node* current = buckets_[bucket_idx].load(memory_order::acquire);

            // Search for existing key
            while (current != nullptr) {
                if (!current->deleted.load(memory_order::acquire) && current->key == key) {
                    // Update existing value
                    std::lock_guard<std::mutex> lock(current->value_mutex);
                    current->value = value;
                    return false; // Updated existing
                }
                current = current->next.load(memory_order::acquire);
            }

            // Create new node
            Node* new_node = new Node(key, value);
            Node* head = buckets_[bucket_idx].load(memory_order::acquire);
            new_node->next.store(head, memory_order::relaxed);

            // Try to insert at head
            if (buckets_[bucket_idx].compare_exchange_weak(head, new_node,
                                                         memory_order::release,
                                                         memory_order::acquire)) {
                size_.fetch_add(1, memory_order::relaxed);
                return true; // Inserted new
            }

            // Failed to insert, clean up and retry
            delete new_node;
        }
    }

    /**
     * @brief Finds a value by key
     * @param key The key to search for
     * @param value Reference to store the found value
     * @return true if found, false otherwise
     */
    bool find(const Key& key, Value& value) const {
        Node* node = find_node(key);
        if (node != nullptr) {
            std::lock_guard<std::mutex> lock(node->value_mutex);
            value = node->value;
            return true;
        }
        return false;
    }

    /**
     * @brief Removes a key-value pair
     * @param key The key to remove
     * @return true if removed, false if not found
     */
    bool remove(const Key& key) {
        Node* node = find_node(key);
        if (node != nullptr) {
            bool expected = false;
            if (node->deleted.compare_exchange_strong(expected, true, memory_order::release)) {
                size_.fetch_sub(1, memory_order::relaxed);
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Gets the current size of the map
     * @return Number of elements in the map
     */
    size_t size() const noexcept {
        return size_.load(memory_order::relaxed);
    }

    /**
     * @brief Checks if the map is empty
     * @return true if empty, false otherwise
     */
    bool empty() const noexcept {
        return size() == 0;
    }

    /**
     * @brief Clears all elements from the map
     */
    void clear() {
        for (size_t i = 0; i < bucket_count_; ++i) {
            Node* current = buckets_[i].load(memory_order::acquire);
            while (current != nullptr) {
                Node* next = current->next.load(memory_order::acquire);
                delete current;
                current = next;
            }
            buckets_[i].store(nullptr, memory_order::release);
        }
        size_.store(0, memory_order::relaxed);
    }
};

/**
 * @brief Lock-free queue for asynchronous operations
 */
template<typename T>
class LockFreeQueue {
private:
    struct Node {
        std::atomic<T*> data{nullptr};
        std::atomic<Node*> next{nullptr};
    };

    alignas(64) std::atomic<Node*> head_;
    alignas(64) std::atomic<Node*> tail_;

public:
    LockFreeQueue() {
        Node* dummy = new Node;
        head_.store(dummy, memory_order::relaxed);
        tail_.store(dummy, memory_order::relaxed);
    }

    ~LockFreeQueue() {
        while (Node* old_head = head_.load(memory_order::relaxed)) {
            head_.store(old_head->next.load(memory_order::relaxed), memory_order::relaxed);
            delete old_head;
        }
    }

    /**
     * @brief Enqueues an item
     * @param item Item to enqueue
     */
    void enqueue(T item) {
        Node* new_node = new Node;
        T* data = new T(std::move(item));
        new_node->data.store(data, memory_order::relaxed);

        while (true) {
            Node* last = tail_.load(memory_order::acquire);
            Node* next = last->next.load(memory_order::acquire);

            if (last == tail_.load(memory_order::acquire)) {
                if (next == nullptr) {
                    if (last->next.compare_exchange_weak(next, new_node,
                                                       memory_order::release,
                                                       memory_order::relaxed)) {
                        break;
                    }
                } else {
                    tail_.compare_exchange_weak(last, next,
                                              memory_order::release,
                                              memory_order::relaxed);
                }
            }
        }

        Node* current_tail = tail_.load(memory_order::acquire);
        tail_.compare_exchange_weak(current_tail, new_node,
                                  memory_order::release,
                                  memory_order::relaxed);
    }

    /**
     * @brief Dequeues an item
     * @param result Reference to store the dequeued item
     * @return true if successful, false if queue is empty
     */
    bool dequeue(T& result) {
        while (true) {
            Node* first = head_.load(memory_order::acquire);
            Node* last = tail_.load(memory_order::acquire);
            Node* next = first->next.load(memory_order::acquire);

            if (first == head_.load(memory_order::acquire)) {
                if (first == last) {
                    if (next == nullptr) {
                        return false; // Queue is empty
                    }
                    tail_.compare_exchange_weak(last, next,
                                              memory_order::release,
                                              memory_order::relaxed);
                } else {
                    if (next == nullptr) {
                        continue;
                    }

                    T* data = next->data.load(memory_order::acquire);
                    if (data == nullptr) {
                        continue;
                    }

                    if (head_.compare_exchange_weak(first, next,
                                                  memory_order::release,
                                                  memory_order::relaxed)) {
                        result = *data;
                        delete data;
                        delete first;
                        return true;
                    }
                }
            }
        }
    }

    /**
     * @brief Checks if the queue is empty
     * @return true if empty, false otherwise
     */
    bool empty() const {
        Node* first = head_.load(memory_order::acquire);
        Node* last = tail_.load(memory_order::acquire);
        return (first == last) && (first->next.load(memory_order::acquire) == nullptr);
    }
};

// Convenience alias for string-based hash map
using LockFreeStringMap = LockFreeHashMap<std::string, std::string>;

} // namespace lockfree

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

namespace memory {

/**
 * @brief Epoch-based memory management for safe deallocation in concurrent environments
 */
class EpochManager {
private:
    static constexpr size_t MAX_THREADS = 64;
    static constexpr size_t EPOCHS_TO_KEEP = 3;

    struct ThreadEpoch {
        alignas(64) std::atomic<uint64_t> epoch{0};
        alignas(64) std::atomic<bool> active{false};
        alignas(64) std::atomic<std::thread::id> thread_id{std::thread::id{}};
    };

    alignas(64) std::atomic<uint64_t> global_epoch_{0};
    alignas(64) std::array<ThreadEpoch, MAX_THREADS> thread_epochs_;
    alignas(64) std::atomic<size_t> active_threads_{0};

    thread_local static inline size_t thread_index_ = SIZE_MAX;
    thread_local static inline bool thread_registered_ = false;

public:
    EpochManager() {
        for (auto& epoch : thread_epochs_) {
            epoch.epoch.store(UINT64_MAX, std::memory_order_relaxed);
            epoch.active.store(false, std::memory_order_relaxed);
        }

#if ATOM_HAS_SPDLOG
        spdlog::debug("EpochManager: Initialized");
#endif
    }

    ~EpochManager() {
        if (thread_registered_) {
            unregister_thread();
        }
    }

    /**
     * @brief Registers the current thread with the epoch manager
     * @return true if successful, false if no slots available
     */
    bool register_thread() noexcept {
        if (thread_registered_) {
            return true;
        }

        auto current_thread_id = std::this_thread::get_id();

        for (size_t i = 0; i < MAX_THREADS; ++i) {
            std::thread::id expected{};
            if (thread_epochs_[i].thread_id.compare_exchange_strong(expected, current_thread_id,
                                                                  std::memory_order_acquire)) {
                thread_index_ = i;
                thread_epochs_[i].active.store(true, std::memory_order_release);
                thread_registered_ = true;
                active_threads_.fetch_add(1, std::memory_order_relaxed);

#if ATOM_HAS_SPDLOG
                spdlog::debug("EpochManager: Thread registered at index {}", i);
#endif
                return true;
            }
        }

        return false; // No available slots
    }

    /**
     * @brief Unregisters the current thread from the epoch manager
     */
    void unregister_thread() noexcept {
        if (!thread_registered_ || thread_index_ == SIZE_MAX) {
            return;
        }

        thread_epochs_[thread_index_].active.store(false, std::memory_order_release);
        thread_epochs_[thread_index_].epoch.store(UINT64_MAX, std::memory_order_release);
        thread_epochs_[thread_index_].thread_id.store(std::thread::id{}, std::memory_order_release);

        active_threads_.fetch_sub(1, std::memory_order_relaxed);
        thread_registered_ = false;
        thread_index_ = SIZE_MAX;

#if ATOM_HAS_SPDLOG
        spdlog::debug("EpochManager: Thread unregistered");
#endif
    }

    /**
     * @brief Enters a critical section and returns the current epoch
     * @return Current epoch value
     */
    uint64_t enter_critical_section() noexcept {
        if (!thread_registered_ && !register_thread()) {
            return 0; // Failed to register
        }

        uint64_t current_epoch = global_epoch_.load(std::memory_order_acquire);
        thread_epochs_[thread_index_].epoch.store(current_epoch, std::memory_order_release);

        return current_epoch;
    }

    /**
     * @brief Exits the critical section
     */
    void exit_critical_section() noexcept {
        if (thread_registered_ && thread_index_ != SIZE_MAX) {
            thread_epochs_[thread_index_].epoch.store(UINT64_MAX, std::memory_order_release);
        }
    }

    /**
     * @brief Advances the global epoch and returns the minimum safe epoch for deallocation
     * @return Minimum epoch that is safe for deallocation
     */
    uint64_t advance_epoch() noexcept {
        uint64_t new_epoch = global_epoch_.fetch_add(1, std::memory_order_acq_rel) + 1;

        // Find the minimum epoch among active threads
        uint64_t min_epoch = new_epoch;
        for (size_t i = 0; i < MAX_THREADS; ++i) {
            if (thread_epochs_[i].active.load(std::memory_order_acquire)) {
                uint64_t thread_epoch = thread_epochs_[i].epoch.load(std::memory_order_acquire);
                if (thread_epoch != UINT64_MAX && thread_epoch < min_epoch) {
                    min_epoch = thread_epoch;
                }
            }
        }

        // Safe epoch is EPOCHS_TO_KEEP behind the minimum
        uint64_t safe_epoch = (min_epoch > EPOCHS_TO_KEEP) ? (min_epoch - EPOCHS_TO_KEEP) : 0;

#if ATOM_HAS_SPDLOG
        spdlog::trace("EpochManager: Advanced to epoch {}, safe epoch: {}", new_epoch, safe_epoch);
#endif

        return safe_epoch;
    }

    /**
     * @brief Gets the current global epoch
     * @return Current global epoch
     */
    uint64_t get_current_epoch() const noexcept {
        return global_epoch_.load(std::memory_order_acquire);
    }

    /**
     * @brief Gets the number of active threads
     * @return Number of active threads
     */
    size_t get_active_thread_count() const noexcept {
        return active_threads_.load(std::memory_order_relaxed);
    }
};

/**
 * @brief Thread-local string pool for efficient string allocations in INI operations
 */
class ThreadLocalStringPool {
private:
    static constexpr size_t POOL_SIZE = 1024;
    static constexpr size_t MAX_STRING_LENGTH = 256;

    struct StringBlock {
        alignas(64) char data[MAX_STRING_LENGTH];
        std::atomic<bool> in_use;

        StringBlock() : in_use(false) {}
    };

    thread_local static inline std::array<StringBlock, POOL_SIZE> pool_;
    thread_local static inline std::atomic<size_t> next_index_{0};
    thread_local static inline std::atomic<size_t> allocations_{0};
    thread_local static inline std::atomic<size_t> pool_hits_{0};

public:
    /**
     * @brief Allocates a string from the pool
     * @param size Required size
     * @return Pointer to allocated memory, or nullptr if not available
     */
    static char* allocate(size_t size) noexcept {
        allocations_.fetch_add(1, std::memory_order_relaxed);

        if (size > MAX_STRING_LENGTH) {
            return nullptr; // Too large for pool
        }

        // Try to find an available block
        size_t start_index = next_index_.load(std::memory_order_relaxed);
        for (size_t i = 0; i < POOL_SIZE; ++i) {
            size_t index = (start_index + i) % POOL_SIZE;
            bool expected = false;

            if (pool_[index].in_use.compare_exchange_strong(expected, true,
                                                          std::memory_order_acquire)) {
                next_index_.store((index + 1) % POOL_SIZE, std::memory_order_relaxed);
                pool_hits_.fetch_add(1, std::memory_order_relaxed);
                return pool_[index].data;
            }
        }

        return nullptr; // Pool exhausted
    }

    /**
     * @brief Deallocates a string back to the pool
     * @param ptr Pointer to deallocate
     */
    static void deallocate(char* ptr) noexcept {
        if (ptr == nullptr) {
            return;
        }

        // Find the block and mark as available
        for (auto& block : pool_) {
            if (block.data == ptr) {
                block.in_use.store(false, std::memory_order_release);
                break;
            }
        }
    }

    /**
     * @brief Gets pool statistics
     * @return Pair of (total_allocations, pool_hits)
     */
    static std::pair<size_t, size_t> get_stats() noexcept {
        return {allocations_.load(std::memory_order_relaxed),
                pool_hits_.load(std::memory_order_relaxed)};
    }

    /**
     * @brief Resets pool statistics
     */
    static void reset_stats() noexcept {
        allocations_.store(0, std::memory_order_relaxed);
        pool_hits_.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Gets the pool hit rate as a percentage
     * @return Hit rate percentage
     */
    static double get_hit_rate() noexcept {
        size_t total = allocations_.load(std::memory_order_relaxed);
        size_t hits = pool_hits_.load(std::memory_order_relaxed);
        return total > 0 ? (100.0 * hits / total) : 0.0;
    }
};

} // namespace memory

// ============================================================================
// LOGGING SYSTEM
// ============================================================================

namespace logging {

/**
 * @brief Global metrics for INI operations
 */
struct GlobalIniMetrics {
    alignas(64) std::atomic<uint64_t> parse_operations{0};
    alignas(64) std::atomic<uint64_t> write_operations{0};
    alignas(64) std::atomic<uint64_t> read_operations{0};
    alignas(64) std::atomic<uint64_t> section_accesses{0};
    alignas(64) std::atomic<uint64_t> field_accesses{0};
    alignas(64) std::atomic<uint64_t> lock_contentions{0};
    alignas(64) std::atomic<uint64_t> cache_hits{0};
    alignas(64) std::atomic<uint64_t> cache_misses{0};
    alignas(64) std::atomic<uint64_t> memory_allocations{0};
    alignas(64) std::atomic<uint64_t> total_parse_time_ns{0};
    alignas(64) std::atomic<uint64_t> total_write_time_ns{0};

    void reset() noexcept {
        parse_operations.store(0, std::memory_order_relaxed);
        write_operations.store(0, std::memory_order_relaxed);
        read_operations.store(0, std::memory_order_relaxed);
        section_accesses.store(0, std::memory_order_relaxed);
        field_accesses.store(0, std::memory_order_relaxed);
        lock_contentions.store(0, std::memory_order_relaxed);
        cache_hits.store(0, std::memory_order_relaxed);
        cache_misses.store(0, std::memory_order_relaxed);
        memory_allocations.store(0, std::memory_order_relaxed);
        total_parse_time_ns.store(0, std::memory_order_relaxed);
        total_write_time_ns.store(0, std::memory_order_relaxed);
    }

    double get_cache_hit_rate() const noexcept {
        uint64_t hits = cache_hits.load(std::memory_order_relaxed);
        uint64_t misses = cache_misses.load(std::memory_order_relaxed);
        uint64_t total = hits + misses;
        return total > 0 ? (100.0 * hits / total) : 0.0;
    }

    double get_average_parse_time_ms() const noexcept {
        uint64_t ops = parse_operations.load(std::memory_order_relaxed);
        uint64_t total_ns = total_parse_time_ns.load(std::memory_order_relaxed);
        return ops > 0 ? (static_cast<double>(total_ns) / ops / 1000000.0) : 0.0;
    }

    double get_average_write_time_ms() const noexcept {
        uint64_t ops = write_operations.load(std::memory_order_relaxed);
        uint64_t total_ns = total_write_time_ns.load(std::memory_order_relaxed);
        return ops > 0 ? (static_cast<double>(total_ns) / ops / 1000000.0) : 0.0;
    }
};

/**
 * @brief Gets the global metrics instance
 * @return Reference to global metrics
 */
inline GlobalIniMetrics& get_global_metrics() {
    static GlobalIniMetrics instance;
    return instance;
}

/**
 * @brief Lock-free logging system for high-performance INI operations
 */
class LockFreeLogger {
private:
    struct LogEntry {
        std::chrono::high_resolution_clock::time_point timestamp;
        std::string message;
        std::string logger_name;
        int level;
        std::thread::id thread_id;

        LogEntry() = default;
        LogEntry(std::string_view msg, std::string_view name, int lvl)
            : timestamp(std::chrono::high_resolution_clock::now())
            , message(msg)
            , logger_name(name)
            , level(lvl)
            , thread_id(std::this_thread::get_id()) {}
    };

    lockfree::LockFreeQueue<LogEntry> log_queue_;
    std::atomic<bool> running_{true};
    std::thread worker_thread_;

#if ATOM_HAS_SPDLOG
    std::shared_ptr<spdlog::logger> async_logger_;
#endif

    void worker_loop() {
        while (running_.load(std::memory_order_acquire)) {
            LogEntry entry;
            if (log_queue_.dequeue(entry)) {
#if ATOM_HAS_SPDLOG
                if (async_logger_) {
                    async_logger_->log(static_cast<spdlog::level::level_enum>(entry.level),
                                     "[{}] {}", entry.logger_name, entry.message);
                }
#endif
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }

public:
    LockFreeLogger() {
#if ATOM_HAS_SPDLOG
        try {
            // Create async logger with thread pool
            spdlog::init_thread_pool(8192, 1);
            auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            async_logger_ = std::make_shared<spdlog::async_logger>(
                "inicpp_async", stdout_sink, spdlog::thread_pool(),
                spdlog::async_overflow_policy::block);
            async_logger_->set_level(spdlog::level::debug);
            async_logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
            spdlog::register_logger(async_logger_);
        } catch (const std::exception& e) {
            // Fallback to console logging
            spdlog::error("Failed to initialize async logger: {}", e.what());
        }
#endif

        worker_thread_ = std::thread(&LockFreeLogger::worker_loop, this);
    }

    ~LockFreeLogger() {
        running_.store(false, std::memory_order_release);
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }

#if ATOM_HAS_SPDLOG
        if (async_logger_) {
            async_logger_->flush();
        }
        spdlog::shutdown();
#endif
    }

    /**
     * @brief Logs a message asynchronously
     * @param level Log level
     * @param logger_name Logger name
     * @param message Message to log
     */
    void log_async(int level, std::string_view logger_name, std::string_view message) {
        log_queue_.enqueue(LogEntry(message, logger_name, level));
    }

    /**
     * @brief Gets the singleton instance
     * @return Reference to the singleton logger
     */
    static LockFreeLogger& instance() {
        static LockFreeLogger instance;
        return instance;
    }
};

/**
 * @brief High-performance timer for measuring operation durations
 */
class PerformanceTimer {
private:
    std::chrono::high_resolution_clock::time_point start_time_;
    std::string operation_name_;

public:
    explicit PerformanceTimer(std::string_view operation_name)
        : start_time_(std::chrono::high_resolution_clock::now())
        , operation_name_(operation_name) {
#if ATOM_HAS_SPDLOG
        spdlog::trace("PerformanceTimer: Started timing '{}'", operation_name_);
#endif
    }

    ~PerformanceTimer() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time_);

#if ATOM_HAS_SPDLOG
        spdlog::debug("PerformanceTimer: '{}' took {} μs", operation_name_, duration.count());
#endif

        // Update global metrics
        auto& metrics = get_global_metrics();
        if (operation_name_.find("parse") != std::string::npos) {
            metrics.parse_operations.fetch_add(1, std::memory_order_relaxed);
            metrics.total_parse_time_ns.fetch_add(
                std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count(),
                std::memory_order_relaxed);
        } else if (operation_name_.find("write") != std::string::npos) {
            metrics.write_operations.fetch_add(1, std::memory_order_relaxed);
            metrics.total_write_time_ns.fetch_add(
                std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count(),
                std::memory_order_relaxed);
        }
    }

    /**
     * @brief Gets the elapsed time since timer creation
     * @return Elapsed time in microseconds
     */
    uint64_t get_elapsed_microseconds() const {
        auto current_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(
            current_time - start_time_).count();
    }
};

} // namespace logging

// Logging macros for convenience
#if ATOM_HAS_SPDLOG

#define INICPP_LOG_TRACE(msg, ...) \
    logging::LockFreeLogger::instance().log_async( \
        static_cast<int>(spdlog::level::trace), \
        "inicpp", \
        fmt::format(msg, ##__VA_ARGS__))

#define INICPP_LOG_DEBUG(msg, ...) \
    logging::LockFreeLogger::instance().log_async( \
        static_cast<int>(spdlog::level::debug), \
        "inicpp", \
        fmt::format(msg, ##__VA_ARGS__))

#define INICPP_LOG_INFO(msg, ...) \
    logging::LockFreeLogger::instance().log_async( \
        static_cast<int>(spdlog::level::info), \
        "inicpp", \
        fmt::format(msg, ##__VA_ARGS__))

#define INICPP_LOG_WARN(msg, ...) \
    logging::LockFreeLogger::instance().log_async( \
        static_cast<int>(spdlog::level::warn), \
        "inicpp", \
        fmt::format(msg, ##__VA_ARGS__))

#define INICPP_LOG_ERROR(msg, ...) \
    logging::LockFreeLogger::instance().log_async( \
        static_cast<int>(spdlog::level::err), \
        "inicpp", \
        fmt::format(msg, ##__VA_ARGS__))

#define INICPP_PERF_TIMER(name) \
    logging::PerformanceTimer _perf_timer(name)

#else

#define INICPP_LOG_TRACE(msg, ...) do {} while(0)
#define INICPP_LOG_DEBUG(msg, ...) do {} while(0)
#define INICPP_LOG_INFO(msg, ...) do {} while(0)
#define INICPP_LOG_WARN(msg, ...) do {} while(0)
#define INICPP_LOG_ERROR(msg, ...) do {} while(0)
#define INICPP_PERF_TIMER(name) do {} while(0)

#endif

// ============================================================================
// PERFORMANCE MONITORING
// ============================================================================

namespace monitoring {

/**
 * @brief Advanced performance metrics for INI operations with lock-free collection
 */
struct AdvancedIniMetrics {
    // Operation counters
    alignas(64) std::atomic<uint64_t> parse_operations{0};
    alignas(64) std::atomic<uint64_t> write_operations{0};
    alignas(64) std::atomic<uint64_t> read_operations{0};
    alignas(64) std::atomic<uint64_t> section_operations{0};
    alignas(64) std::atomic<uint64_t> field_operations{0};

    // Concurrency metrics
    alignas(64) std::atomic<uint64_t> lock_acquisitions{0};
    alignas(64) std::atomic<uint64_t> lock_contentions{0};
    alignas(64) std::atomic<uint64_t> spin_cycles{0};
    alignas(64) std::atomic<uint64_t> yield_operations{0};
    alignas(64) std::atomic<uint64_t> sleep_operations{0};

    // Cache metrics
    alignas(64) std::atomic<uint64_t> cache_hits{0};
    alignas(64) std::atomic<uint64_t> cache_misses{0};
    alignas(64) std::atomic<uint64_t> cache_evictions{0};

    // Memory metrics
    alignas(64) std::atomic<uint64_t> memory_allocations{0};
    alignas(64) std::atomic<uint64_t> memory_deallocations{0};
    alignas(64) std::atomic<uint64_t> pool_allocations{0};
    alignas(64) std::atomic<uint64_t> pool_hits{0};
    alignas(64) std::atomic<uint64_t> epoch_advances{0};

    // Timing metrics (in nanoseconds)
    alignas(64) std::atomic<uint64_t> total_parse_time_ns{0};
    alignas(64) std::atomic<uint64_t> total_write_time_ns{0};
    alignas(64) std::atomic<uint64_t> total_read_time_ns{0};
    alignas(64) std::atomic<uint64_t> max_parse_time_ns{0};
    alignas(64) std::atomic<uint64_t> max_write_time_ns{0};
    alignas(64) std::atomic<uint64_t> max_read_time_ns{0};

    // Error metrics
    alignas(64) std::atomic<uint64_t> parse_errors{0};
    alignas(64) std::atomic<uint64_t> io_errors{0};
    alignas(64) std::atomic<uint64_t> memory_errors{0};

    void reset() noexcept {
        parse_operations.store(0, std::memory_order_relaxed);
        write_operations.store(0, std::memory_order_relaxed);
        read_operations.store(0, std::memory_order_relaxed);
        section_operations.store(0, std::memory_order_relaxed);
        field_operations.store(0, std::memory_order_relaxed);

        lock_acquisitions.store(0, std::memory_order_relaxed);
        lock_contentions.store(0, std::memory_order_relaxed);
        spin_cycles.store(0, std::memory_order_relaxed);
        yield_operations.store(0, std::memory_order_relaxed);
        sleep_operations.store(0, std::memory_order_relaxed);

        cache_hits.store(0, std::memory_order_relaxed);
        cache_misses.store(0, std::memory_order_relaxed);
        cache_evictions.store(0, std::memory_order_relaxed);

        memory_allocations.store(0, std::memory_order_relaxed);
        memory_deallocations.store(0, std::memory_order_relaxed);
        pool_allocations.store(0, std::memory_order_relaxed);
        pool_hits.store(0, std::memory_order_relaxed);
        epoch_advances.store(0, std::memory_order_relaxed);

        total_parse_time_ns.store(0, std::memory_order_relaxed);
        total_write_time_ns.store(0, std::memory_order_relaxed);
        total_read_time_ns.store(0, std::memory_order_relaxed);
        max_parse_time_ns.store(0, std::memory_order_relaxed);
        max_write_time_ns.store(0, std::memory_order_relaxed);
        max_read_time_ns.store(0, std::memory_order_relaxed);

        parse_errors.store(0, std::memory_order_relaxed);
        io_errors.store(0, std::memory_order_relaxed);
        memory_errors.store(0, std::memory_order_relaxed);
    }

    double get_cache_hit_rate() const noexcept {
        uint64_t hits = cache_hits.load(std::memory_order_relaxed);
        uint64_t misses = cache_misses.load(std::memory_order_relaxed);
        uint64_t total = hits + misses;
        return total > 0 ? (100.0 * hits / total) : 0.0;
    }

    double get_pool_hit_rate() const noexcept {
        uint64_t hits = pool_hits.load(std::memory_order_relaxed);
        uint64_t total_allocs = memory_allocations.load(std::memory_order_relaxed);
        return total_allocs > 0 ? (100.0 * hits / total_allocs) : 0.0;
    }

    double get_contention_rate() const noexcept {
        uint64_t acquisitions = lock_acquisitions.load(std::memory_order_relaxed);
        uint64_t contentions = lock_contentions.load(std::memory_order_relaxed);
        return acquisitions > 0 ? (100.0 * contentions / acquisitions) : 0.0;
    }

    double get_average_parse_time_ms() const noexcept {
        uint64_t ops = parse_operations.load(std::memory_order_relaxed);
        uint64_t total_ns = total_parse_time_ns.load(std::memory_order_relaxed);
        return ops > 0 ? (static_cast<double>(total_ns) / ops / 1000000.0) : 0.0;
    }

    double get_average_write_time_ms() const noexcept {
        uint64_t ops = write_operations.load(std::memory_order_relaxed);
        uint64_t total_ns = total_write_time_ns.load(std::memory_order_relaxed);
        return ops > 0 ? (static_cast<double>(total_ns) / ops / 1000000.0) : 0.0;
    }

    double get_average_read_time_ms() const noexcept {
        uint64_t ops = read_operations.load(std::memory_order_relaxed);
        uint64_t total_ns = total_read_time_ns.load(std::memory_order_relaxed);
        return ops > 0 ? (static_cast<double>(total_ns) / ops / 1000000.0) : 0.0;
    }

    double get_max_parse_time_ms() const noexcept {
        return max_parse_time_ns.load(std::memory_order_relaxed) / 1000000.0;
    }

    double get_max_write_time_ms() const noexcept {
        return max_write_time_ns.load(std::memory_order_relaxed) / 1000000.0;
    }

    double get_max_read_time_ms() const noexcept {
        return max_read_time_ns.load(std::memory_order_relaxed) / 1000000.0;
    }
};

/**
 * @brief Real-time performance monitor with lock-free data collection
 */
class RealTimePerformanceMonitor {
private:
    AdvancedIniMetrics metrics_;
    std::atomic<bool> monitoring_enabled_{true};
    std::atomic<bool> auto_reporting_enabled_{false};
    std::atomic<uint64_t> report_interval_ms_{5000}; // 5 seconds default
    std::thread monitoring_thread_;
    std::atomic<bool> shutdown_requested_{false};

    // Histogram for latency tracking
    static constexpr size_t HISTOGRAM_BUCKETS = 20;
    static constexpr uint64_t MAX_LATENCY_NS = 1000000000; // 1 second
    std::array<std::atomic<uint64_t>, HISTOGRAM_BUCKETS> latency_histogram_{};

    void monitoring_loop() {
        while (!shutdown_requested_.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(report_interval_ms_.load(std::memory_order_relaxed)));

            if (auto_reporting_enabled_.load(std::memory_order_acquire)) {
                generate_performance_report();
            }
        }
    }

    size_t get_latency_bucket(uint64_t latency_ns) const noexcept {
        if (latency_ns >= MAX_LATENCY_NS) {
            return HISTOGRAM_BUCKETS - 1;
        }
        return (latency_ns * HISTOGRAM_BUCKETS) / MAX_LATENCY_NS;
    }

public:
    RealTimePerformanceMonitor() {
        for (auto& bucket : latency_histogram_) {
            bucket.store(0, std::memory_order_relaxed);
        }

        monitoring_thread_ = std::thread(&RealTimePerformanceMonitor::monitoring_loop, this);

#if ATOM_HAS_SPDLOG
        spdlog::info("RealTimePerformanceMonitor: Initialized with lock-free metrics collection");
#endif
    }

    ~RealTimePerformanceMonitor() {
        shutdown_requested_.store(true, std::memory_order_release);
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
    }

    /**
     * @brief Records a parse operation with timing
     * @param duration_ns Duration in nanoseconds
     */
    void record_parse_operation(uint64_t duration_ns) noexcept {
        if (!monitoring_enabled_.load(std::memory_order_relaxed)) return;

        metrics_.parse_operations.fetch_add(1, std::memory_order_relaxed);
        metrics_.total_parse_time_ns.fetch_add(duration_ns, std::memory_order_relaxed);

        // Update max time atomically
        uint64_t current_max = metrics_.max_parse_time_ns.load(std::memory_order_relaxed);
        while (duration_ns > current_max &&
               !metrics_.max_parse_time_ns.compare_exchange_weak(current_max, duration_ns,
                                                               std::memory_order_relaxed)) {
            // Retry if another thread updated the max
        }

        // Update histogram
        size_t bucket = get_latency_bucket(duration_ns);
        latency_histogram_[bucket].fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Records a write operation with timing
     * @param duration_ns Duration in nanoseconds
     */
    void record_write_operation(uint64_t duration_ns) noexcept {
        if (!monitoring_enabled_.load(std::memory_order_relaxed)) return;

        metrics_.write_operations.fetch_add(1, std::memory_order_relaxed);
        metrics_.total_write_time_ns.fetch_add(duration_ns, std::memory_order_relaxed);

        uint64_t current_max = metrics_.max_write_time_ns.load(std::memory_order_relaxed);
        while (duration_ns > current_max &&
               !metrics_.max_write_time_ns.compare_exchange_weak(current_max, duration_ns,
                                                               std::memory_order_relaxed)) {
        }

        size_t bucket = get_latency_bucket(duration_ns);
        latency_histogram_[bucket].fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Records a read operation with timing
     * @param duration_ns Duration in nanoseconds
     */
    void record_read_operation(uint64_t duration_ns) noexcept {
        if (!monitoring_enabled_.load(std::memory_order_relaxed)) return;

        metrics_.read_operations.fetch_add(1, std::memory_order_relaxed);
        metrics_.total_read_time_ns.fetch_add(duration_ns, std::memory_order_relaxed);

        uint64_t current_max = metrics_.max_read_time_ns.load(std::memory_order_relaxed);
        while (duration_ns > current_max &&
               !metrics_.max_read_time_ns.compare_exchange_weak(current_max, duration_ns,
                                                              std::memory_order_relaxed)) {
        }

        size_t bucket = get_latency_bucket(duration_ns);
        latency_histogram_[bucket].fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Records lock contention
     */
    void record_lock_contention() noexcept {
        if (!monitoring_enabled_.load(std::memory_order_relaxed)) return;

        metrics_.lock_contentions.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Records lock acquisition
     */
    void record_lock_acquisition() noexcept {
        if (!monitoring_enabled_.load(std::memory_order_relaxed)) return;

        metrics_.lock_acquisitions.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Records cache hit
     */
    void record_cache_hit() noexcept {
        if (!monitoring_enabled_.load(std::memory_order_relaxed)) return;

        metrics_.cache_hits.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Records cache miss
     */
    void record_cache_miss() noexcept {
        if (!monitoring_enabled_.load(std::memory_order_relaxed)) return;

        metrics_.cache_misses.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Records memory allocation
     */
    void record_memory_allocation() noexcept {
        if (!monitoring_enabled_.load(std::memory_order_relaxed)) return;

        metrics_.memory_allocations.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Records pool allocation hit
     */
    void record_pool_hit() noexcept {
        if (!monitoring_enabled_.load(std::memory_order_relaxed)) return;

        metrics_.pool_hits.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Gets the current metrics reference
     * @return Reference to current metrics
     */
    const AdvancedIniMetrics& get_metrics() const noexcept {
        return metrics_;
    }

    /**
     * @brief Resets all metrics
     */
    void reset_metrics() noexcept {
        metrics_.reset();
        for (auto& bucket : latency_histogram_) {
            bucket.store(0, std::memory_order_relaxed);
        }

#if ATOM_HAS_SPDLOG
        spdlog::info("RealTimePerformanceMonitor: Metrics reset");
#endif
    }

    /**
     * @brief Enables or disables monitoring
     * @param enabled Whether to enable monitoring
     */
    void set_monitoring_enabled(bool enabled) noexcept {
        monitoring_enabled_.store(enabled, std::memory_order_relaxed);

#if ATOM_HAS_SPDLOG
        spdlog::info("RealTimePerformanceMonitor: Monitoring {}", enabled ? "enabled" : "disabled");
#endif
    }

    /**
     * @brief Enables or disables automatic reporting
     * @param enabled Whether to enable auto reporting
     * @param interval_ms Reporting interval in milliseconds
     */
    void set_auto_reporting(bool enabled, uint64_t interval_ms = 5000) noexcept {
        auto_reporting_enabled_.store(enabled, std::memory_order_relaxed);
        report_interval_ms_.store(interval_ms, std::memory_order_relaxed);

#if ATOM_HAS_SPDLOG
        spdlog::info("RealTimePerformanceMonitor: Auto reporting {} (interval: {} ms)",
                    enabled ? "enabled" : "disabled", interval_ms);
#endif
    }

    /**
     * @brief Generates a comprehensive performance report
     */
    void generate_performance_report() const {
#if ATOM_HAS_SPDLOG
        const auto& m = metrics_;

        spdlog::info("=== INI Performance Report ===");
        spdlog::info("Operations:");
        spdlog::info("  Parse: {} (avg: {:.3f} ms, max: {:.3f} ms)",
                    m.parse_operations.load(), m.get_average_parse_time_ms(), m.get_max_parse_time_ms());
        spdlog::info("  Write: {} (avg: {:.3f} ms, max: {:.3f} ms)",
                    m.write_operations.load(), m.get_average_write_time_ms(), m.get_max_write_time_ms());
        spdlog::info("  Read: {} (avg: {:.3f} ms, max: {:.3f} ms)",
                    m.read_operations.load(), m.get_average_read_time_ms(), m.get_max_read_time_ms());
        spdlog::info("  Sections: {}", m.section_operations.load());
        spdlog::info("  Fields: {}", m.field_operations.load());

        spdlog::info("Concurrency:");
        spdlog::info("  Lock acquisitions: {}", m.lock_acquisitions.load());
        spdlog::info("  Lock contentions: {} ({:.2f}%)",
                    m.lock_contentions.load(), m.get_contention_rate());
        spdlog::info("  Spin cycles: {}", m.spin_cycles.load());
        spdlog::info("  Yield operations: {}", m.yield_operations.load());
        spdlog::info("  Sleep operations: {}", m.sleep_operations.load());

        spdlog::info("Cache:");
        spdlog::info("  Hits: {} ({:.2f}%)", m.cache_hits.load(), m.get_cache_hit_rate());
        spdlog::info("  Misses: {}", m.cache_misses.load());
        spdlog::info("  Evictions: {}", m.cache_evictions.load());

        spdlog::info("Memory:");
        spdlog::info("  Allocations: {}", m.memory_allocations.load());
        spdlog::info("  Deallocations: {}", m.memory_deallocations.load());
        spdlog::info("  Pool hits: {} ({:.2f}%)", m.pool_hits.load(), m.get_pool_hit_rate());
        spdlog::info("  Epoch advances: {}", m.epoch_advances.load());

        spdlog::info("Errors:");
        spdlog::info("  Parse errors: {}", m.parse_errors.load());
        spdlog::info("  I/O errors: {}", m.io_errors.load());
        spdlog::info("  Memory errors: {}", m.memory_errors.load());

        // Latency histogram
        spdlog::info("Latency Distribution:");
        for (size_t i = 0; i < HISTOGRAM_BUCKETS; ++i) {
            uint64_t count = latency_histogram_[i].load(std::memory_order_relaxed);
            if (count > 0) {
                double bucket_start_ms = (static_cast<double>(i) * MAX_LATENCY_NS / HISTOGRAM_BUCKETS) / 1000000.0;
                double bucket_end_ms = (static_cast<double>(i + 1) * MAX_LATENCY_NS / HISTOGRAM_BUCKETS) / 1000000.0;
                spdlog::info("  {:.1f}-{:.1f} ms: {}", bucket_start_ms, bucket_end_ms, count);
            }
        }

        spdlog::info("===============================");
#endif
    }

    /**
     * @brief Gets the singleton instance
     * @return Reference to the singleton monitor
     */
    static RealTimePerformanceMonitor& instance() {
        static RealTimePerformanceMonitor instance;
        return instance;
    }
};

/**
 * @brief RAII timer for automatic operation timing
 */
template<typename Operation>
class ScopedOperationTimer {
private:
    std::chrono::high_resolution_clock::time_point start_time_;
    RealTimePerformanceMonitor& monitor_;

public:
    explicit ScopedOperationTimer(RealTimePerformanceMonitor& monitor = RealTimePerformanceMonitor::instance())
        : start_time_(std::chrono::high_resolution_clock::now()), monitor_(monitor) {}

    ~ScopedOperationTimer() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time - start_time_).count();

        if constexpr (std::is_same_v<Operation, struct ParseOperation>) {
            monitor_.record_parse_operation(duration_ns);
        } else if constexpr (std::is_same_v<Operation, struct WriteOperation>) {
            monitor_.record_write_operation(duration_ns);
        } else if constexpr (std::is_same_v<Operation, struct ReadOperation>) {
            monitor_.record_read_operation(duration_ns);
        }
    }
};

// Operation type tags
struct ParseOperation {};
struct WriteOperation {};
struct ReadOperation {};

// Convenience aliases
using ParseTimer = ScopedOperationTimer<ParseOperation>;
using WriteTimer = ScopedOperationTimer<WriteOperation>;
using ReadTimer = ScopedOperationTimer<ReadOperation>;

/**
 * @brief Enhanced performance macros with automatic monitoring
 */
#define INICPP_MONITOR_PARSE_OP() \
    monitoring::ParseTimer _parse_timer(monitoring::RealTimePerformanceMonitor::instance())

#define INICPP_MONITOR_WRITE_OP() \
    monitoring::WriteTimer _write_timer(monitoring::RealTimePerformanceMonitor::instance())

#define INICPP_MONITOR_READ_OP() \
    monitoring::ReadTimer _read_timer(monitoring::RealTimePerformanceMonitor::instance())

#define INICPP_RECORD_CACHE_HIT() \
    monitoring::RealTimePerformanceMonitor::instance().record_cache_hit()

#define INICPP_RECORD_CACHE_MISS() \
    monitoring::RealTimePerformanceMonitor::instance().record_cache_miss()

#define INICPP_RECORD_LOCK_CONTENTION() \
    monitoring::RealTimePerformanceMonitor::instance().record_lock_contention()

#define INICPP_RECORD_LOCK_ACQUISITION() \
    monitoring::RealTimePerformanceMonitor::instance().record_lock_acquisition()

} // namespace monitoring

// ============================================================================
// CONCURRENT INI IMPLEMENTATION
// ============================================================================

namespace concurrent {

/**
 * @brief High-performance concurrent INI section using lock-free data structures
 */
class ConcurrentIniSection {
private:
    lockfree::LockFreeStringMap fields_;
    sync::IniReaderWriterLock section_lock_;
    std::atomic<uint64_t> modification_count_{0};
    std::atomic<uint64_t> access_count_{0};
    memory::EpochManager epoch_manager_;

public:
    ConcurrentIniSection() = default;

    /**
     * @brief Sets a field value in the section
     * @param key Field key
     * @param value Field value
     */
    void set_field(const std::string& key, const std::string& value) {
        INICPP_MONITOR_WRITE_OP();

        section_lock_.lock();
        fields_.insert_or_update(key, value);
        modification_count_.fetch_add(1, std::memory_order_relaxed);
        section_lock_.unlock();

        INICPP_LOG_DEBUG("ConcurrentIniSection: Set field '{}' = '{}'", key, value);
    }

    /**
     * @brief Gets a field value from the section
     * @param key Field key
     * @param value Reference to store the value
     * @return true if found, false otherwise
     */
    bool get_field(const std::string& key, std::string& value) const {
        INICPP_MONITOR_READ_OP();

        const_cast<sync::IniReaderWriterLock&>(section_lock_).lock_shared();
        const_cast<std::atomic<uint64_t>&>(access_count_).fetch_add(1, std::memory_order_relaxed);
        bool found = fields_.find(key, value);
        const_cast<sync::IniReaderWriterLock&>(section_lock_).unlock_shared();

        if (found) {
            INICPP_RECORD_CACHE_HIT();
            INICPP_LOG_TRACE("ConcurrentIniSection: Found field '{}' = '{}'", key, value);
        } else {
            INICPP_RECORD_CACHE_MISS();
            INICPP_LOG_TRACE("ConcurrentIniSection: Field '{}' not found", key);
        }

        return found;
    }

    /**
     * @brief Removes a field from the section
     * @param key Field key to remove
     * @return true if removed, false if not found
     */
    bool remove_field(const std::string& key) {
        INICPP_MONITOR_WRITE_OP();

        section_lock_.lock();
        bool removed = fields_.remove(key);
        if (removed) {
            modification_count_.fetch_add(1, std::memory_order_relaxed);
        }
        section_lock_.unlock();

        INICPP_LOG_DEBUG("ConcurrentIniSection: {} field '{}'",
                        removed ? "Removed" : "Failed to remove", key);
        return removed;
    }

    /**
     * @brief Gets the number of fields in the section
     * @return Number of fields
     */
    size_t size() const noexcept {
        const_cast<sync::IniReaderWriterLock&>(section_lock_).lock_shared();
        size_t count = fields_.size();
        const_cast<sync::IniReaderWriterLock&>(section_lock_).unlock_shared();
        return count;
    }

    /**
     * @brief Checks if the section is empty
     * @return true if empty, false otherwise
     */
    bool empty() const noexcept {
        return size() == 0;
    }

    /**
     * @brief Clears all fields from the section
     */
    void clear() {
        INICPP_MONITOR_WRITE_OP();

        section_lock_.lock();
        fields_.clear();
        modification_count_.fetch_add(1, std::memory_order_relaxed);
        section_lock_.unlock();

        INICPP_LOG_DEBUG("ConcurrentIniSection: Cleared all fields");
    }

    /**
     * @brief Gets section statistics
     * @return Pair of (modification_count, access_count)
     */
    std::pair<uint64_t, uint64_t> get_stats() const noexcept {
        return {modification_count_.load(std::memory_order_relaxed),
                access_count_.load(std::memory_order_relaxed)};
    }
};

/**
 * @brief High-performance concurrent INI file implementation
 */
class ConcurrentIniFile {
private:
    lockfree::LockFreeHashMap<std::string, std::shared_ptr<ConcurrentIniSection>> sections_;
    sync::IniReaderWriterLock file_lock_;
    std::atomic<uint64_t> modification_count_{0};
    memory::EpochManager epoch_manager_;

public:
    ConcurrentIniFile() = default;

    /**
     * @brief Creates a new section or returns existing one
     * @param section_name Name of the section
     * @return Shared pointer to the section
     */
    std::shared_ptr<ConcurrentIniSection> create_section(const std::string& section_name) {
        INICPP_MONITOR_WRITE_OP();

        file_lock_.lock();

        std::shared_ptr<ConcurrentIniSection> existing_section;
        if (sections_.find(section_name, existing_section)) {
            file_lock_.unlock();
            return existing_section;
        }

        auto new_section = std::make_shared<ConcurrentIniSection>();
        sections_.insert_or_update(section_name, new_section);
        modification_count_.fetch_add(1, std::memory_order_relaxed);

        file_lock_.unlock();

        INICPP_LOG_DEBUG("ConcurrentIniFile: Created section '{}'", section_name);
        return new_section;
    }

    /**
     * @brief Gets an existing section
     * @param section_name Name of the section
     * @return Shared pointer to the section, or nullptr if not found
     */
    std::shared_ptr<ConcurrentIniSection> get_section(const std::string& section_name) const {
        INICPP_MONITOR_READ_OP();

        const_cast<sync::IniReaderWriterLock&>(file_lock_).lock_shared();
        std::shared_ptr<ConcurrentIniSection> section;
        bool found = sections_.find(section_name, section);
        const_cast<sync::IniReaderWriterLock&>(file_lock_).unlock_shared();

        if (found) {
            INICPP_RECORD_CACHE_HIT();
            INICPP_LOG_TRACE("ConcurrentIniFile: Found section '{}'", section_name);
        } else {
            INICPP_RECORD_CACHE_MISS();
            INICPP_LOG_TRACE("ConcurrentIniFile: Section '{}' not found", section_name);
        }

        return found ? section : nullptr;
    }

    /**
     * @brief Removes a section
     * @param section_name Name of the section to remove
     * @return true if removed, false if not found
     */
    bool remove_section(const std::string& section_name) {
        INICPP_MONITOR_WRITE_OP();

        file_lock_.lock();
        bool removed = sections_.remove(section_name);
        if (removed) {
            modification_count_.fetch_add(1, std::memory_order_relaxed);
        }
        file_lock_.unlock();

        INICPP_LOG_DEBUG("ConcurrentIniFile: {} section '{}'",
                        removed ? "Removed" : "Failed to remove", section_name);
        return removed;
    }

    /**
     * @brief Gets the number of sections
     * @return Number of sections
     */
    size_t size() const noexcept {
        const_cast<sync::IniReaderWriterLock&>(file_lock_).lock_shared();
        size_t count = sections_.size();
        const_cast<sync::IniReaderWriterLock&>(file_lock_).unlock_shared();
        return count;
    }

    /**
     * @brief Checks if the file is empty
     * @return true if empty, false otherwise
     */
    bool empty() const noexcept {
        return size() == 0;
    }

    /**
     * @brief Clears all sections
     */
    void clear() {
        INICPP_MONITOR_WRITE_OP();

        file_lock_.lock();
        sections_.clear();
        modification_count_.fetch_add(1, std::memory_order_relaxed);
        file_lock_.unlock();

        INICPP_LOG_DEBUG("ConcurrentIniFile: Cleared all sections");
    }

    /**
     * @brief Parses INI content from a string with parallel processing
     * @param content INI content to parse
     * @return true if successful, false otherwise
     */
    bool parse_from_string(const std::string& content) {
        INICPP_MONITOR_PARSE_OP();

        try {
            std::istringstream stream(content);
            std::string line;
            std::string current_section;

            while (std::getline(stream, line)) {
                // Trim whitespace
                line.erase(0, line.find_first_not_of(" \t"));
                line.erase(line.find_last_not_of(" \t") + 1);

                // Skip empty lines and comments
                if (line.empty() || line[0] == ';' || line[0] == '#') {
                    continue;
                }

                // Section header
                if (line[0] == '[' && line.back() == ']') {
                    current_section = line.substr(1, line.length() - 2);
                    create_section(current_section);
                    continue;
                }

                // Key-value pair
                size_t eq_pos = line.find('=');
                if (eq_pos != std::string::npos && !current_section.empty()) {
                    std::string key = line.substr(0, eq_pos);
                    std::string value = line.substr(eq_pos + 1);

                    // Trim key and value
                    key.erase(0, key.find_first_not_of(" \t"));
                    key.erase(key.find_last_not_of(" \t") + 1);
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t") + 1);

                    auto section = get_section(current_section);
                    if (section) {
                        section->set_field(key, value);
                    }
                }
            }

            INICPP_LOG_INFO("ConcurrentIniFile: Successfully parsed {} characters", content.size());
            return true;

        } catch (const std::exception& e) {
            INICPP_LOG_ERROR("ConcurrentIniFile: Parse error: {}", e.what());
            return false;
        }
    }

    /**
     * @brief Gets file statistics
     * @return Modification count
     */
    uint64_t get_modification_count() const noexcept {
        return modification_count_.load(std::memory_order_relaxed);
    }
};

} // namespace concurrent

} // namespace inicpp

#endif  // ATOM_EXTRA_INICPP_HPP
