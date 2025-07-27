#ifndef ATOM_EXTRA_BEAST_CONCURRENCY_PRIMITIVES_HPP
#define ATOM_EXTRA_BEAST_CONCURRENCY_PRIMITIVES_HPP

#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <array>
#include <chrono>
#include <concepts>
#include <coroutine>
#include <immintrin.h>
#include <spdlog/spdlog.h>

namespace atom::beast::concurrency {

/**
 * @brief Cache line size for optimal memory alignment
 */
constexpr std::size_t CACHE_LINE_SIZE = 64;

/**
 * @brief Aligned storage for cache-friendly data structures
 */
template<typename T>
struct alignas(CACHE_LINE_SIZE) CacheAligned {
    T value;

    template<typename... Args>
    constexpr CacheAligned(Args&&... args) : value(std::forward<Args>(args)...) {}

    operator T&() noexcept { return value; }
    operator const T&() const noexcept { return value; }
};

/**
 * @brief High-performance hazard pointer implementation for lock-free memory management
 */
class HazardPointer {
public:
    static constexpr std::size_t MAX_HAZARD_POINTERS = 100;

    struct HazardRecord {
        std::atomic<std::thread::id> id{};
        std::atomic<void*> pointer{nullptr};
    };

    static HazardRecord hazard_pointers_[MAX_HAZARD_POINTERS];
    static std::atomic<std::size_t> hazard_pointer_count_{0};

    /**
     * @brief Acquires a hazard pointer for the current thread
     */
    static HazardRecord* acquire_hazard_pointer() noexcept {
        auto this_id = std::this_thread::get_id();

        // Try to find existing record for this thread
        for (std::size_t i = 0; i < hazard_pointer_count_.load(std::memory_order_acquire); ++i) {
            auto expected = std::thread::id{};
            if (hazard_pointers_[i].id.compare_exchange_strong(expected, this_id, std::memory_order_acq_rel)) {
                return &hazard_pointers_[i];
            }
            if (hazard_pointers_[i].id.load(std::memory_order_acquire) == this_id) {
                return &hazard_pointers_[i];
            }
        }

        // Allocate new record
        auto count = hazard_pointer_count_.fetch_add(1, std::memory_order_acq_rel);
        if (count < MAX_HAZARD_POINTERS) {
            hazard_pointers_[count].id.store(this_id, std::memory_order_release);
            return &hazard_pointers_[count];
        }

        hazard_pointer_count_.fetch_sub(1, std::memory_order_acq_rel);
        return nullptr;
    }

    /**
     * @brief Releases a hazard pointer
     */
    static void release_hazard_pointer(HazardRecord* record) noexcept {
        if (record) {
            record->pointer.store(nullptr, std::memory_order_release);
            record->id.store(std::thread::id{}, std::memory_order_release);
        }
    }

    /**
     * @brief Checks if a pointer is protected by any hazard pointer
     */
    static bool is_hazardous(void* ptr) noexcept {
        for (std::size_t i = 0; i < hazard_pointer_count_.load(std::memory_order_acquire); ++i) {
            if (hazard_pointers_[i].pointer.load(std::memory_order_acquire) == ptr) {
                return true;
            }
        }
        return false;
    }
};

/**
 * @brief Lock-free SPSC (Single Producer Single Consumer) queue with optimal performance
 */
template<typename T, std::size_t Size = 1024>
class SPSCQueue {
    static_assert((Size & (Size - 1)) == 0, "Size must be power of 2");

private:
    struct alignas(CACHE_LINE_SIZE) {
        std::atomic<std::size_t> head{0};
    };

    struct alignas(CACHE_LINE_SIZE) {
        std::atomic<std::size_t> tail{0};
    };

    alignas(CACHE_LINE_SIZE) std::array<T, Size> buffer_;

public:
    /**
     * @brief Attempts to enqueue an item (producer side)
     */
    [[nodiscard]] bool try_enqueue(T&& item) noexcept {
        const auto current_tail = tail.load(std::memory_order_relaxed);
        const auto next_tail = (current_tail + 1) & (Size - 1);

        if (next_tail == head.load(std::memory_order_acquire)) {
            return false; // Queue is full
        }

        buffer_[current_tail] = std::move(item);
        tail.store(next_tail, std::memory_order_release);
        return true;
    }

    /**
     * @brief Attempts to dequeue an item (consumer side)
     */
    [[nodiscard]] bool try_dequeue(T& item) noexcept {
        const auto current_head = head.load(std::memory_order_relaxed);

        if (current_head == tail.load(std::memory_order_acquire)) {
            return false; // Queue is empty
        }

        item = std::move(buffer_[current_head]);
        head.store((current_head + 1) & (Size - 1), std::memory_order_release);
        return true;
    }

    /**
     * @brief Returns approximate queue size
     */
    [[nodiscard]] std::size_t size() const noexcept {
        const auto current_tail = tail.load(std::memory_order_acquire);
        const auto current_head = head.load(std::memory_order_acquire);
        return (current_tail - current_head) & (Size - 1);
    }

    /**
     * @brief Checks if queue is empty
     */
    [[nodiscard]] bool empty() const noexcept {
        return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
    }
};

/**
 * @brief High-performance spinlock with exponential backoff
 */
class AdaptiveSpinLock {
private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
    mutable std::atomic<std::uint32_t> contention_count_{0};

public:
    /**
     * @brief Acquires the lock with adaptive spinning
     */
    void lock() noexcept {
        std::uint32_t spin_count = 0;
        constexpr std::uint32_t MAX_SPINS = 4000;

        while (flag_.test_and_set(std::memory_order_acquire)) {
            if (++spin_count < MAX_SPINS) {
                // CPU pause instruction for better performance
                _mm_pause();

                // Exponential backoff
                if (spin_count > 100) {
                    for (std::uint32_t i = 0; i < (1u << std::min(spin_count / 100, 10u)); ++i) {
                        _mm_pause();
                    }
                }
            } else {
                // Yield to scheduler after excessive spinning
                std::this_thread::yield();
                spin_count = 0;
                contention_count_.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    /**
     * @brief Attempts to acquire the lock without blocking
     */
    [[nodiscard]] bool try_lock() noexcept {
        return !flag_.test_and_set(std::memory_order_acquire);
    }

    /**
     * @brief Releases the lock
     */
    void unlock() noexcept {
        flag_.clear(std::memory_order_release);
    }

    /**
     * @brief Returns contention statistics
     */
    [[nodiscard]] std::uint32_t contention_count() const noexcept {
        return contention_count_.load(std::memory_order_relaxed);
    }
};

/**
 * @brief Lock-free reference counter for shared ownership
 */
template<typename T>
class LockFreeSharedPtr {
private:
    struct ControlBlock {
        std::atomic<std::size_t> ref_count{1};
        T* ptr;

        explicit ControlBlock(T* p) : ptr(p) {}

        void add_ref() noexcept {
            ref_count.fetch_add(1, std::memory_order_relaxed);
        }

        bool release() noexcept {
            return ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1;
        }
    };

    std::atomic<ControlBlock*> control_block_{nullptr};

public:
    explicit LockFreeSharedPtr(T* ptr = nullptr) {
        if (ptr) {
            control_block_.store(new ControlBlock(ptr), std::memory_order_release);
        }
    }

    LockFreeSharedPtr(const LockFreeSharedPtr& other) noexcept {
        auto* cb = other.control_block_.load(std::memory_order_acquire);
        if (cb) {
            cb->add_ref();
            control_block_.store(cb, std::memory_order_release);
        }
    }

    ~LockFreeSharedPtr() {
        reset();
    }

    void reset() noexcept {
        auto* cb = control_block_.exchange(nullptr, std::memory_order_acq_rel);
        if (cb && cb->release()) {
            delete cb->ptr;
            delete cb;
        }
    }

    T* get() const noexcept {
        auto* cb = control_block_.load(std::memory_order_acquire);
        return cb ? cb->ptr : nullptr;
    }

    T& operator*() const noexcept { return *get(); }
    T* operator->() const noexcept { return get(); }

    explicit operator bool() const noexcept { return get() != nullptr; }
};

/**
 * @brief Thread-local storage with NUMA awareness
 */
template<typename T>
class NUMAAwareThreadLocal {
private:
    thread_local static T instance_;

public:
    static T& get() noexcept {
        return instance_;
    }

    template<typename... Args>
    static void initialize(Args&&... args) {
        instance_ = T(std::forward<Args>(args)...);
    }
};

template<typename T>
thread_local T NUMAAwareThreadLocal<T>::instance_{};

} // namespace atom::beast::concurrency

#endif // ATOM_EXTRA_BEAST_CONCURRENCY_PRIMITIVES_HPP
