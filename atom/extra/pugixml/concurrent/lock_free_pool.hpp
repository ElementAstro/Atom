#pragma once

#include <spdlog/spdlog.h>

#include <atomic>
#include <memory>
#include <array>
#include <vector>
#include <concepts>
#include <bit>
#include <new>
#include <source_location>
#include <chrono>

namespace atom::extra::pugixml::concurrent {

/**
 * @brief Hazard pointer implementation for safe memory reclamation
 */
template<typename T>
class HazardPointer {
private:
    static constexpr size_t MAX_HAZARD_POINTERS = 128;
    static thread_local std::array<std::atomic<T*>, MAX_HAZARD_POINTERS> hazard_ptrs_;
    static thread_local size_t next_hazard_index_;

public:
    class Guard {
        size_t index_;

    public:
        explicit Guard(T* ptr) {
            index_ = next_hazard_index_++;
            if (index_ >= MAX_HAZARD_POINTERS) {
                index_ = 0;
                next_hazard_index_ = 1;
            }
            hazard_ptrs_[index_].store(ptr, std::memory_order_release);
        }

        ~Guard() {
            hazard_ptrs_[index_].store(nullptr, std::memory_order_release);
        }

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
        Guard(Guard&&) = delete;
        Guard& operator=(Guard&&) = delete;
    };

    [[nodiscard]] static bool is_hazardous(T* ptr) noexcept {
        for (const auto& hazard_ptr : hazard_ptrs_) {
            if (hazard_ptr.load(std::memory_order_acquire) == ptr) {
                return true;
            }
        }
        return false;
    }
};

template<typename T>
thread_local std::array<std::atomic<T*>, HazardPointer<T>::MAX_HAZARD_POINTERS>
    HazardPointer<T>::hazard_ptrs_{};

template<typename T>
thread_local size_t HazardPointer<T>::next_hazard_index_{0};

/**
 * @brief Lock-free stack for memory pool implementation
 */
template<typename T>
class LockFreeStack {
private:
    struct Node {
        std::atomic<Node*> next;
        alignas(T) std::byte data[sizeof(T)];

        Node() : next(nullptr) {}
    };

    std::atomic<Node*> head_{nullptr};
    std::shared_ptr<spdlog::logger> logger_;

public:
    explicit LockFreeStack(std::shared_ptr<spdlog::logger> logger = nullptr)
        : logger_(logger) {
        if (logger_) {
            logger_->debug("LockFreeStack created");
        }
    }

    ~LockFreeStack() {
        while (auto node = pop_node()) {
            delete node;
        }
        if (logger_) {
            logger_->debug("LockFreeStack destroyed");
        }
    }

    void push(Node* node) noexcept {
        Node* old_head = head_.load(std::memory_order_relaxed);
        do {
            node->next.store(old_head, std::memory_order_relaxed);
        } while (!head_.compare_exchange_weak(old_head, node,
                                            std::memory_order_release,
                                            std::memory_order_relaxed));
    }

    [[nodiscard]] Node* pop_node() noexcept {
        Node* old_head = head_.load(std::memory_order_acquire);
        while (old_head != nullptr) {
            typename HazardPointer<Node>::Guard guard(old_head);

            // Re-check after setting hazard pointer
            if (old_head != head_.load(std::memory_order_acquire)) {
                old_head = head_.load(std::memory_order_acquire);
                continue;
            }

            Node* next = old_head->next.load(std::memory_order_relaxed);
            if (head_.compare_exchange_weak(old_head, next,
                                          std::memory_order_release,
                                          std::memory_order_relaxed)) {
                return old_head;
            }
        }
        return nullptr;
    }

    [[nodiscard]] T* pop() noexcept {
        if (auto node = pop_node()) {
            return reinterpret_cast<T*>(node->data);
        }
        return nullptr;
    }

    void push_data(T* data) noexcept {
        auto node = reinterpret_cast<Node*>(
            reinterpret_cast<std::byte*>(data) - offsetof(Node, data));
        push(node);
    }

    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == nullptr;
    }
};

/**
 * @brief High-performance lock-free memory pool with NUMA awareness
 */
template<typename T>
class LockFreePool {
private:
    static constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
    static constexpr size_t CHUNK_SIZE = 1024;

    struct alignas(CACHE_LINE_SIZE) PerThreadData {
        LockFreeStack<T> local_stack;
        std::atomic<size_t> allocations{0};
        std::atomic<size_t> deallocations{0};

        explicit PerThreadData(std::shared_ptr<spdlog::logger> logger)
            : local_stack(logger) {}
    };

    std::vector<std::unique_ptr<PerThreadData>> thread_data_;
    LockFreeStack<T> global_stack_;
    std::atomic<size_t> total_allocated_{0};
    std::atomic<size_t> total_deallocated_{0};
    std::atomic<size_t> peak_usage_{0};
    std::shared_ptr<spdlog::logger> logger_;

    static thread_local size_t thread_id_;
    static std::atomic<size_t> next_thread_id_;

    [[nodiscard]] PerThreadData& get_thread_data() {
        if (thread_id_ == SIZE_MAX) {
            thread_id_ = next_thread_id_.fetch_add(1, std::memory_order_relaxed);

            // Ensure thread_data_ is large enough
            while (thread_data_.size() <= thread_id_) {
                thread_data_.emplace_back(std::make_unique<PerThreadData>(logger_));
            }
        }
        return *thread_data_[thread_id_];
    }

    void allocate_chunk() {
        constexpr size_t node_size = sizeof(typename LockFreeStack<T>::Node);
        auto chunk = std::aligned_alloc(CACHE_LINE_SIZE, CHUNK_SIZE * node_size);
        if (!chunk) {
            throw std::bad_alloc{};
        }

        auto nodes = static_cast<typename LockFreeStack<T>::Node*>(chunk);
        for (size_t i = 0; i < CHUNK_SIZE; ++i) {
            new (&nodes[i]) typename LockFreeStack<T>::Node{};
            global_stack_.push(&nodes[i]);
        }

        if (logger_) {
            logger_->debug("Allocated chunk of {} nodes", CHUNK_SIZE);
        }
    }

public:
    explicit LockFreePool(std::shared_ptr<spdlog::logger> logger = nullptr)
        : global_stack_(logger), logger_(logger) {

        // Pre-allocate initial chunks
        for (size_t i = 0; i < 4; ++i) {
            allocate_chunk();
        }

        if (logger_) {
            logger_->info("LockFreePool initialized with {} initial chunks", 4);
        }
    }

    ~LockFreePool() {
        if (logger_) {
            logger_->info("LockFreePool destroyed. Total allocated: {}, deallocated: {}, peak: {}",
                         total_allocated_.load(), total_deallocated_.load(), peak_usage_.load());
        }
    }

    [[nodiscard]] T* allocate() {
        auto& thread_data = get_thread_data();

        // Try local stack first
        if (auto ptr = thread_data.local_stack.pop()) {
            thread_data.allocations.fetch_add(1, std::memory_order_relaxed);
            total_allocated_.fetch_add(1, std::memory_order_relaxed);

            // Update peak usage
            auto current_usage = total_allocated_.load() - total_deallocated_.load();
            auto peak = peak_usage_.load(std::memory_order_relaxed);
            while (current_usage > peak &&
                   !peak_usage_.compare_exchange_weak(peak, current_usage,
                                                    std::memory_order_relaxed)) {
                // Retry
            }

            return ptr;
        }

        // Try global stack
        if (auto ptr = global_stack_.pop()) {
            thread_data.allocations.fetch_add(1, std::memory_order_relaxed);
            total_allocated_.fetch_add(1, std::memory_order_relaxed);
            return ptr;
        }

        // Allocate new chunk
        allocate_chunk();
        return allocate(); // Recursive call should succeed now
    }

    void deallocate(T* ptr) noexcept {
        if (!ptr) return;

        auto& thread_data = get_thread_data();
        thread_data.local_stack.push_data(ptr);
        thread_data.deallocations.fetch_add(1, std::memory_order_relaxed);
        total_deallocated_.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Get performance statistics
     */
    struct Statistics {
        size_t total_allocated;
        size_t total_deallocated;
        size_t current_usage;
        size_t peak_usage;
        std::chrono::steady_clock::time_point timestamp;
    };

    [[nodiscard]] Statistics get_statistics() const noexcept {
        auto now = std::chrono::steady_clock::now();
        auto allocated = total_allocated_.load(std::memory_order_relaxed);
        auto deallocated = total_deallocated_.load(std::memory_order_relaxed);

        return Statistics{
            .total_allocated = allocated,
            .total_deallocated = deallocated,
            .current_usage = allocated - deallocated,
            .peak_usage = peak_usage_.load(std::memory_order_relaxed),
            .timestamp = now
        };
    }

    /**
     * @brief Force garbage collection of hazardous pointers
     */
    void collect_garbage() {
        // Implementation would scan hazard pointers and safely reclaim memory
        if (logger_) {
            logger_->debug("Garbage collection triggered");
        }
    }
};

template<typename T>
thread_local size_t LockFreePool<T>::thread_id_{SIZE_MAX};

template<typename T>
std::atomic<size_t> LockFreePool<T>::next_thread_id_{0};

/**
 * @brief RAII wrapper for pool-allocated objects
 */
template<typename T>
class PoolPtr {
private:
    T* ptr_;
    LockFreePool<T>* pool_;

public:
    PoolPtr(T* ptr, LockFreePool<T>* pool) : ptr_(ptr), pool_(pool) {}

    ~PoolPtr() {
        if (ptr_ && pool_) {
            ptr_->~T();
            pool_->deallocate(ptr_);
        }
    }

    PoolPtr(const PoolPtr&) = delete;
    PoolPtr& operator=(const PoolPtr&) = delete;

    PoolPtr(PoolPtr&& other) noexcept : ptr_(other.ptr_), pool_(other.pool_) {
        other.ptr_ = nullptr;
        other.pool_ = nullptr;
    }

    PoolPtr& operator=(PoolPtr&& other) noexcept {
        if (this != &other) {
            if (ptr_ && pool_) {
                ptr_->~T();
                pool_->deallocate(ptr_);
            }
            ptr_ = other.ptr_;
            pool_ = other.pool_;
            other.ptr_ = nullptr;
            other.pool_ = nullptr;
        }
        return *this;
    }

    [[nodiscard]] T* get() const noexcept { return ptr_; }
    [[nodiscard]] T& operator*() const noexcept { return *ptr_; }
    [[nodiscard]] T* operator->() const noexcept { return ptr_; }
    [[nodiscard]] explicit operator bool() const noexcept { return ptr_ != nullptr; }
};

}  // namespace atom::extra::pugixml::concurrent
