/*
 * thread_wrapper.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-13

Description: A high-performance wrapper of std::jthread with advanced concurrency optimizations

**************************************************/

#ifndef ATOM_ASYNC_THREAD_WRAPPER_HPP
#define ATOM_ASYNC_THREAD_WRAPPER_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <concepts>
// #include <condition_variable> // Not used
#include <coroutine>
#include <exception>
#include <functional>
#include <future> // Used for promise/future
#include <source_location>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#include <barrier>        // C++20 for thread synchronization
#include <bit>            // C++20 bit manipulation

#include "atom/type/noncopyable.hpp"

// Platform-specific includes for advanced features
#if defined(_WIN32)
#include <windows.h>
#include <processthreadsapi.h>
#elif defined(__linux__)
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <linux/futex.h>
#elif defined(__APPLE__)
#include <pthread.h>
#include <sched.h>
#include <mach/thread_policy.h>
#include <mach/thread_act.h>
#endif

namespace atom::async {

// Cache line size for false sharing prevention
inline constexpr std::size_t CACHE_LINE_SIZE = 64;

// Alignas for cache line optimization
template<typename T>
struct alignas(CACHE_LINE_SIZE) CacheAligned {
    T value;

    template<typename... Args>
    explicit CacheAligned(Args&&... args) : value(std::forward<Args>(args)...) {}

    operator T&() noexcept { return value; }
    operator const T&() const noexcept { return value; }
};

/**
 * @brief High-performance spin lock using atomic operations
 */
class SpinLock {
private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

public:
    void lock() noexcept {
        // Optimized spin with exponential backoff
        int spin_count = 0;
        while (flag_.test_and_set(std::memory_order_acquire)) {
            // Adaptive spinning with pause instruction
            if (spin_count < 16) {
                // Active spinning for short waits
                for (int i = 0; i < (1 << spin_count); ++i) {
                    #if defined(__x86_64__) || defined(__i386__)
                    __builtin_ia32_pause();
                    #elif defined(__aarch64__)
                    __asm__ __volatile__("yield" ::: "memory");
                    #else
                    std::this_thread::yield();
                    #endif
                }
                ++spin_count;
            } else {
                // Yield after excessive spinning
                std::this_thread::yield();
            }
        }
    }

    bool try_lock() noexcept {
        return !flag_.test_and_set(std::memory_order_acquire);
    }

    void unlock() noexcept {
        flag_.clear(std::memory_order_release);
    }
};

/**
 * @brief High-performance read-write spin lock
 */
class RWSpinLock {
private:
    std::atomic<std::uint32_t> counter_{0};
    static constexpr std::uint32_t WRITE_LOCK_FLAG = 0x80000000u;
    static constexpr std::uint32_t READ_COUNT_MASK = 0x7FFFFFFFu;

public:
    void lock() noexcept {  // Write lock
        std::uint32_t expected = 0;
        while (!counter_.compare_exchange_weak(expected, WRITE_LOCK_FLAG,
                                             std::memory_order_acquire,
                                             std::memory_order_relaxed)) {
            expected = 0;
            std::this_thread::yield();
        }
    }

    void lock_shared() noexcept {  // Read lock
        std::uint32_t expected = counter_.load(std::memory_order_relaxed);
        while (true) {
            if (expected & WRITE_LOCK_FLAG) {
                std::this_thread::yield();
                expected = counter_.load(std::memory_order_relaxed);
                continue;
            }

            if (counter_.compare_exchange_weak(expected, expected + 1,
                                             std::memory_order_acquire,
                                             std::memory_order_relaxed)) {
                break;
            }
        }
    }

    void unlock() noexcept {  // Write unlock
        counter_.store(0, std::memory_order_release);
    }

    void unlock_shared() noexcept {  // Read unlock
        counter_.fetch_sub(1, std::memory_order_release);
    }
};

/**
 * @brief Lock-free SPSC (Single Producer Single Consumer) queue
 */
template<typename T, std::size_t Size>
class SPSCQueue {
private:
    static_assert(std::has_single_bit(Size), "Size must be power of 2");

    struct alignas(CACHE_LINE_SIZE) Element {
        std::atomic<std::uint64_t> version{0};
        T data;
    };

    alignas(CACHE_LINE_SIZE) std::array<Element, Size> buffer_;
    alignas(CACHE_LINE_SIZE) std::atomic<std::uint64_t> head_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<std::uint64_t> tail_{0};

    static constexpr std::uint64_t INDEX_MASK = Size - 1;

public:
    template<typename U>
    bool try_push(U&& item) noexcept {
        const auto current_tail = tail_.load(std::memory_order_relaxed);
        auto& element = buffer_[current_tail & INDEX_MASK];

        if (element.version.load(std::memory_order_acquire) != current_tail) {
            return false;  // Queue full
        }

        element.data = std::forward<U>(item);
        element.version.store(current_tail + 1, std::memory_order_release);
        tail_.store(current_tail + 1, std::memory_order_relaxed);
        return true;
    }

    bool try_pop(T& item) noexcept {
        const auto current_head = head_.load(std::memory_order_relaxed);
        auto& element = buffer_[current_head & INDEX_MASK];

        if (element.version.load(std::memory_order_acquire) != current_head + 1) {
            return false;  // Queue empty
        }

        item = std::move(element.data);
        element.version.store(current_head + Size, std::memory_order_release);
        head_.store(current_head + 1, std::memory_order_relaxed);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        const auto current_head = head_.load(std::memory_order_relaxed);
        const auto& element = buffer_[current_head & INDEX_MASK];
        return element.version.load(std::memory_order_acquire) != current_head + 1;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);
        const auto head = head_.load(std::memory_order_relaxed);
        return tail - head;
    }
};

/**
 * @brief Optimized exception class with source location
 */
class ThreadException : public std::runtime_error {
public:
    explicit ThreadException(
        std::string_view message,
        const std::source_location& loc = std::source_location::current())
        : std::runtime_error(formatMessage(message, loc)) {}

private:
    static std::string formatMessage(std::string_view message,
                                   const std::source_location& loc) {
        // Use string concatenation instead of stringstream for better performance
        std::string result;
        result.reserve(message.size() + 256);  // Reserve space to avoid reallocations
        result += message;
        result += " (at ";
        result += loc.file_name();
        result += ':';
        result += std::to_string(loc.line());
        result += " in ";
        result += loc.function_name();
        result += ')';
        return result;
    }
};

// Enhanced concepts with more precise requirements
template <typename Callable, typename... Args>
concept ThreadCallable = requires(Callable c, Args... args) {
    { c(args...) } -> std::same_as<void>;
} || requires(Callable c, Args... args) {
    { c(args...) };
    !std::same_as<decltype(c(args...)), void>;
};

template <typename Callable, typename... Args>
concept StopTokenCallable = requires(Callable c, std::stop_token st, Args... args) {
    { c(st, args...) };
};

template <typename F>
concept PoolableFunction = std::invocable<std::decay_t<F>> &&
                          !std::is_void_v<std::decay_t<F>>;

/**
 * @brief High-performance thread wrapper with advanced optimizations
 */
class Thread : public NonCopyable {
public:
    // Thread priority enumeration
    enum class Priority {
        Lowest = -2,
        Low = -1,
        Normal = 0,
        High = 1,
        Highest = 2,
        RealTime = 3
    };

    // Thread affinity mask type
    using AffinityMask = std::uint64_t;

    Thread() noexcept = default;

    template <typename Callable, typename... Args>
        requires ThreadCallable<Callable, Args...>
    explicit Thread(Callable&& func, Args&&... args) {
        start(std::forward<Callable>(func), std::forward<Args>(args)...);
    }

    template <typename Callable, typename... Args>
        requires ThreadCallable<Callable, Args...>
    void start(Callable&& func, Args&&... args) {
        // Use promise/future for faster synchronization and exception propagation than latch
        std::promise<void> startup_promise;
        std::future<void> startup_future = startup_promise.get_future();

        thread_name_ = generateThreadName();

        thread_ = std::jthread([
            func = std::forward<Callable>(func),
            ...args = std::forward<Args>(args),
            startup_promise = std::move(startup_promise), // Move the promise into the lambda
            thread_name = thread_name_
        ](std::stop_token stop_token) mutable { // Make lambda mutable to move promise
            try {
                setCurrentThreadName(thread_name);
                // Signal successful startup
                startup_promise.set_value();

                if constexpr (StopTokenCallable<Callable, Args...>) {
                    func(stop_token, std::move(args)...);
                } else {
                    func(std::move(args)...);
                }
            } catch (...) {
                // Store exception in the promise
                startup_promise.set_exception(std::current_exception());
            }
        });

        // Wait for thread startup with timeout using the future
        auto status = startup_future.wait_for(std::chrono::milliseconds(500));

        // Check the status
        if (status == std::future_status::timeout) {
            // Timeout occurred, request stop and throw
            thread_.request_stop();
            throw ThreadException("Thread failed to start within timeout");
        }

        // If not timeout, get the result (which will rethrow any stored exception)
        // This also checks if set_exception was called.
        startup_future.get();
    }

    /**
     * @brief Set thread priority (platform-specific optimization)
     */
    void setPriority(Priority priority) {
        if (!running()) return;

        #if defined(_WIN32)
        int win_priority = THREAD_PRIORITY_NORMAL;
        switch (priority) {
            case Priority::Lowest: win_priority = THREAD_PRIORITY_LOWEST; break;
            case Priority::Low: win_priority = THREAD_PRIORITY_BELOW_NORMAL; break;
            case Priority::Normal: win_priority = THREAD_PRIORITY_NORMAL; break;
            case Priority::High: win_priority = THREAD_PRIORITY_ABOVE_NORMAL; break;
            case Priority::Highest: win_priority = THREAD_PRIORITY_HIGHEST; break;
            case Priority::RealTime: win_priority = THREAD_PRIORITY_TIME_CRITICAL; break;
        }

        HANDLE handle = OpenThread(THREAD_SET_INFORMATION, FALSE, GetThreadId(thread_.native_handle()));
        if (handle) {
            SetThreadPriority(handle, win_priority);
            CloseHandle(handle);
        }

        #elif defined(__linux__)
        int policy = SCHED_OTHER;
        struct sched_param param{};

        switch (priority) {
            case Priority::Lowest:
            case Priority::Low:
            case Priority::Normal:
                policy = SCHED_OTHER;
                param.sched_priority = 0;
                break;
            case Priority::High:
            case Priority::Highest:
                policy = SCHED_FIFO;
                param.sched_priority = static_cast<int>(priority);
                break;
            case Priority::RealTime:
                policy = SCHED_RR;
                param.sched_priority = sched_get_priority_max(SCHED_RR);
                break;
        }

        pthread_setschedparam(thread_.native_handle(), policy, &param);
        #endif
    }

    /**
     * @brief Set thread CPU affinity for better cache locality
     */
    void setAffinity(AffinityMask mask) {
        if (!running()) return;

        #if defined(_WIN32)
        HANDLE handle = OpenThread(THREAD_SET_INFORMATION, FALSE, GetThreadId(thread_.native_handle()));
        if (handle) {
            SetThreadAffinityMask(handle, mask);
            CloseHandle(handle);
        }

        #elif defined(__linux__)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);

        for (int i = 0; i < 64; ++i) {
            if (mask & (1ULL << i)) {
                CPU_SET(i, &cpuset);
            }
        }

        pthread_setaffinity_np(thread_.native_handle(), sizeof(cpu_set_t), &cpuset);
        #endif
    }

    /**
     * @brief High-performance periodic execution with precise timing
     */
    template <typename Callable, typename Rep, typename Period>
        requires std::invocable<Callable>
    void startPeriodicPrecise(Callable&& func,
                             const std::chrono::duration<Rep, Period>& interval) {
        start([func = std::forward<Callable>(func), interval]
              (std::stop_token stop_token) mutable {
            auto next_time = std::chrono::steady_clock::now() + interval;

            while (!stop_token.stop_requested()) {
                func();

                // Precise timing without drift accumulation
                next_time += interval;
                auto now = std::chrono::steady_clock::now();

                if (next_time > now) {
                    // Use high-resolution sleep
                    std::this_thread::sleep_until(next_time);
                } else {
                    // Catch up if we're behind
                    next_time = now + interval;
                }
            }
        });
    }

    /**
     * @brief Lock-free thread joining with timeout
     */
    template <typename Rep, typename Period>
    [[nodiscard]] bool tryJoinFor(
        const std::chrono::duration<Rep, Period>& timeout_duration) noexcept {
        if (!running()) return true;

        // Use atomic flag for lock-free status checking
        std::atomic<bool> joined{false};

        // Launch a separate thread to handle the join
        std::jthread join_thread([this, &joined]() {
            if (thread_.joinable()) {
                thread_.join();
                joined.store(true, std::memory_order_release);
            }
        });

        // Wait with timeout
        const auto start_time = std::chrono::steady_clock::now();
        const auto sleep_duration = std::chrono::microseconds(100);

        while (!joined.load(std::memory_order_acquire)) {
            if (std::chrono::steady_clock::now() - start_time > timeout_duration) {
                join_thread.request_stop();
                return false;
            }
            std::this_thread::sleep_for(sleep_duration);
        }

        return true;
    }

    /**
     * @brief Requests the thread to stop execution.
     */
    void requestStop() noexcept {
        if (thread_.joinable()) {
            thread_.request_stop();
        }
        if (timeout_thread_.joinable()) {
            timeout_thread_.request_stop();
        }
    }

    /**
     * @brief Waits for the thread to finish execution.
     *
     * @throws ThreadException if joining the thread throws an exception.
     */
    void join() {
        if (thread_.joinable()) {
            thread_.join();
        }
        if (timeout_thread_.joinable()) {
            timeout_thread_.join();
        }
    }

    /**
     * @brief Checks if the thread is currently running.
     * @return True if the thread is running, false otherwise.
     */
    [[nodiscard]] bool running() const noexcept {
        return thread_.joinable();
    }

    /**
     * @brief Gets the ID of the thread.
     * @return The ID of the thread.
     */
    [[nodiscard]] auto getId() const noexcept -> std::thread::id {
        return thread_.get_id();
    }

    /**
     * @brief Gets the thread name.
     * @return The name of the thread.
     */
    [[nodiscard]] const std::string& getName() const noexcept {
        return thread_name_;
    }

    /**
     * @brief Gets the underlying std::stop_token object.
     * @return The underlying std::stop_token object.
     */
    [[nodiscard]] auto getStopToken() const noexcept -> std::stop_token {
        return thread_.get_stop_token();
    }

    /**
     * @brief Gets the number of hardware concurrency units available to the
     * system.
     * @return Number of system threads.
     */
    [[nodiscard]] static unsigned int getHardwareConcurrency() noexcept {
        return std::thread::hardware_concurrency();
    }

    /**
     * @brief Default destructor that automatically joins the thread if
     * joinable.
     */
    ~Thread() {
        try {
            if (thread_.joinable()) {
                thread_.request_stop();
                thread_.join();
            }
            if (timeout_thread_.joinable()) {
                timeout_thread_.request_stop();
                timeout_thread_.join();
            }
        } catch (...) {
            // Ignore exceptions in destructor
        }
    }

private:
    std::jthread thread_;
    std::jthread timeout_thread_;
    std::string thread_name_;

    static std::string generateThreadName() {
        // Thread-safe counter with better performance than atomic
        static thread_local std::uint64_t counter = 0;
        static std::atomic<std::uint64_t> global_counter{0};

        if (counter == 0) {
            counter = global_counter.fetch_add(1, std::memory_order_relaxed);
        }

        return "Thread-" + std::to_string(counter);
    }

    static void setCurrentThreadName(const std::string& name) {
        #if defined(_WIN32)
        // Windows implementation
        #elif defined(__linux__)
        pthread_setname_np(pthread_self(), name.substr(0, 15).c_str());
        #elif defined(__APPLE__)
        pthread_setname_np(name.substr(0, 63).c_str());
        #endif
    }
};

/**
 * @brief Optimized parallel execution with work stealing
 */
template <typename InputIt, typename Function>
void parallel_for_each_optimized(
    InputIt first, InputIt last, Function function,
    unsigned int num_threads = std::thread::hardware_concurrency()) {

    if (first == last) return;

    const auto length = std::distance(first, last);
    if (length <= 1) {
        std::for_each(first, last, function);
        return;
    }

    if (num_threads == 0) num_threads = 1;

    // Use work-stealing approach for better load balancing
    std::vector<std::atomic<std::ptrdiff_t>> work_indices(num_threads);
    std::atomic<std::ptrdiff_t> global_index{0};

    // Initialize work indices
    const auto chunk_size = length / num_threads;
    for (unsigned int i = 0; i < num_threads; ++i) {
        work_indices[i].store(i * chunk_size, std::memory_order_relaxed);
    }

    // Barrier for thread synchronization
    std::barrier sync_barrier(num_threads);

    std::vector<std::jthread> threads;
    threads.reserve(num_threads);

    for (unsigned int thread_id = 0; thread_id < num_threads; ++thread_id) {
        threads.emplace_back([&, thread_id]() {
            auto local_index = work_indices[thread_id].load(std::memory_order_relaxed);
            const auto max_index = (thread_id == num_threads - 1) ? length : (thread_id + 1) * chunk_size;

            // Process local work
            while (local_index < max_index) {
                auto it = first;
                std::advance(it, local_index);
                function(*it);
                local_index = work_indices[thread_id].fetch_add(1, std::memory_order_acq_rel);
            }

            // Work stealing phase
            while (true) {
                bool found_work = false;

                // Try to steal work from other threads
                for (unsigned int victim = 0; victim < num_threads; ++victim) {
                    if (victim == thread_id) continue;

                    const auto victim_max = (victim == num_threads - 1) ? length : (victim + 1) * chunk_size;
                    auto victim_index = work_indices[victim].load(std::memory_order_acquire);

                    if (victim_index < victim_max) {
                        // Try to steal work
                        auto expected = victim_index;
                        if (work_indices[victim].compare_exchange_weak(
                            expected, victim_index + 1, std::memory_order_acq_rel)) {

                            auto it = first;
                            std::advance(it, expected);
                            function(*it);
                            found_work = true;
                            break;
                        }
                    }
                }

                if (!found_work) break;
            }

            sync_barrier.arrive_and_wait();
        });
    }

    // Threads automatically join on destruction
}

/**
 * @brief High-performance task with better memory layout
 */
template <typename T = void>
class OptimizedTask {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        // Cache-aligned members to prevent false sharing
        alignas(CACHE_LINE_SIZE) std::atomic<bool> completed_{false};
        alignas(CACHE_LINE_SIZE) std::exception_ptr exception_;

        std::conditional_t<std::is_void_v<T>, std::monostate, T> result_;
        std::function<void()> completion_callback_;

        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }

        void unhandled_exception() noexcept {
            exception_ = std::current_exception();
            completed_.store(true, std::memory_order_release);
            if (completion_callback_) {
                completion_callback_();
            }
        }

        template <typename U = T>
            requires(!std::is_void_v<T>)
        void return_value(U&& value) {
            result_ = std::forward<U>(value);
            completed_.store(true, std::memory_order_release);
            if (completion_callback_) {
                completion_callback_();
            }
        }

        void return_void()
            requires std::same_as<T, void>
        {
            completed_.store(true, std::memory_order_release);
            if (completion_callback_) {
                completion_callback_();
            }
        }

        OptimizedTask get_return_object() {
            return OptimizedTask(handle_type::from_promise(*this));
        }

        [[nodiscard]] bool isCompleted() const noexcept {
            return completed_.load(std::memory_order_acquire);
        }

        decltype(auto) getResult() {
            if (exception_) {
                std::rethrow_exception(exception_);
            }

            if constexpr (std::is_void_v<T>) {
                return;
            } else {
                return std::move(result_);
            }
        }
    };

    explicit OptimizedTask(handle_type h) : handle_(h) {}

    OptimizedTask(OptimizedTask&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)) {}

    OptimizedTask& operator=(OptimizedTask&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    ~OptimizedTask() {
        if (handle_) handle_.destroy();
    }

    [[nodiscard]] bool isCompleted() const noexcept {
        return handle_ && handle_.promise().isCompleted();
    }

    decltype(auto) getResult() {
        if (!handle_) {
            throw std::runtime_error("Task has no valid coroutine handle");
        }
        return handle_.promise().getResult();
    }

private:
    handle_type handle_;
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_THREAD_WRAPPER_HPP
