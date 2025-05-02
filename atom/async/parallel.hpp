/*
 * parallel.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-24

Description: High-performance parallel algorithms library

**************************************************/

#ifndef ATOM_ASYNC_PARALLEL_HPP
#define ATOM_ASYNC_PARALLEL_HPP

#include <algorithm>
#include <concepts>
#include <coroutine>
#include <execution>
#include <future>
#include <numeric>
#include <optional>
#include <thread>
#include <type_traits>
#include <vector>

#include <barrier>
#include <latch>
#include <ranges>
#include <span>
#include <stop_token>

#if defined(_WIN32) || defined(_WIN64)
#define ATOM_PLATFORM_WINDOWS 1
#include <processthreadsapi.h>
#include <windows.h>
#elif defined(__APPLE__)
#define ATOM_PLATFORM_MACOS 1
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#elif defined(__linux__)
#define ATOM_PLATFORM_LINUX 1
#include <pthread.h>
#include <sched.h>
#endif

// SIMD 指令集检测
#if defined(__AVX512F__)
#define ATOM_SIMD_AVX512 1
#include <immintrin.h>
#elif defined(__AVX2__)
#define ATOM_SIMD_AVX2 1
#include <immintrin.h>
#elif defined(__AVX__)
#define ATOM_SIMD_AVX 1
#include <immintrin.h>
#elif defined(__ARM_NEON)
#define ATOM_SIMD_NEON 1
#include <arm_neon.h>
#endif

namespace atom::async {

/**
 * @brief C++20 协程任务类，用于异步并行计算
 *
 * @tparam T 任务结果类型
 */
template <typename T>
class [[nodiscard]] Task {
public:
    /**
     * @brief 协程任务的 Promise 类型
     */
    struct promise_type {
        std::optional<T> result;
        std::exception_ptr exception;

        Task get_return_object() noexcept {
            return Task{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() noexcept { return {}; }

        std::suspend_always final_suspend() noexcept { return {}; }

        void return_value(T value) noexcept { result = std::move(value); }

        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }
    };

    /**
     * @brief 销毁协程任务
     */
    ~Task() {
        if (handle && handle.done()) {
            handle.destroy();
        }
    }

    /**
     * @brief 禁用复制
     */
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    /**
     * @brief 启用移动
     */
    Task(Task&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle && handle.done()) {
                handle.destroy();
            }
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }

    /**
     * @brief 获取任务结果
     *
     * @return 结果值
     * @throws 如果协程抛出异常，则重新抛出该异常
     */
    T get() {
        if (!handle.done()) {
            handle.resume();
        }

        if (handle.promise().exception) {
            std::rethrow_exception(handle.promise().exception);
        }

        if (!handle.promise().result.has_value()) {
            throw std::runtime_error("协程没有返回值");
        }

        return std::move(handle.promise().result.value());
    }

    /**
     * @brief 检查任务是否完成
     */
    bool is_done() const { return handle.done(); }

private:
    explicit Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    std::coroutine_handle<promise_type> handle;
};

/**
 * @brief 空返回值的协程任务特化
 */
template <>
class Task<void> {
public:
    struct promise_type {
        std::exception_ptr exception;

        Task get_return_object() noexcept {
            return Task{
                std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_never initial_suspend() noexcept { return {}; }

        std::suspend_always final_suspend() noexcept { return {}; }

        void return_void() noexcept {}

        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }
    };

    ~Task() {
        if (handle && handle.done()) {
            handle.destroy();
        }
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    Task(Task&& other) noexcept : handle(other.handle) {
        other.handle = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle && handle.done()) {
                handle.destroy();
            }
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }

    void get() {
        if (!handle.done()) {
            handle.resume();
        }

        if (handle.promise().exception) {
            std::rethrow_exception(handle.promise().exception);
        }
    }

    bool is_done() const { return handle.done(); }

private:
    explicit Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    std::coroutine_handle<promise_type> handle;
};

/**
 * @brief Parallel algorithm utilities for high-performance computations
 */
class Parallel {
public:
    /**
     * @brief 平台特定线程优化设置类
     * 提供跨平台的线程亲和性和优先级设置
     */
    class ThreadConfig {
    public:
        /**
         * @brief 线程优先级枚举
         */
        enum class Priority { Lowest, Low, Normal, High, Highest };

        /**
         * @brief 设置当前线程的CPU亲和性
         * @param cpuId 要绑定的CPU核心ID
         * @return 是否成功
         */
        static bool setThreadAffinity(int cpuId) {
            if (cpuId < 0)
                return false;

#if defined(ATOM_PLATFORM_WINDOWS)
            HANDLE currentThread = GetCurrentThread();
            DWORD_PTR mask = 1ULL << cpuId;
            return SetThreadAffinityMask(currentThread, mask) != 0;
#elif defined(ATOM_PLATFORM_LINUX)
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(cpuId, &cpuset);
            return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t),
                                          &cpuset) == 0;
#elif defined(ATOM_PLATFORM_MACOS)
            // macOS不直接支持线程亲和性，但可以提供"偏好"设置
            thread_affinity_policy_data_t policy = {cpuId};
            return thread_policy_set(
                       pthread_mach_thread_np(pthread_self()),
                       THREAD_AFFINITY_POLICY, (thread_policy_t)&policy,
                       THREAD_AFFINITY_POLICY_COUNT) == KERN_SUCCESS;
#else
            return false;
#endif
        }

        /**
         * @brief 设置当前线程的优先级
         * @param priority 要设置的优先级
         * @return 是否成功
         */
        static bool setThreadPriority(Priority priority) {
#if defined(ATOM_PLATFORM_WINDOWS)
            int winPriority;
            switch (priority) {
                case Priority::Lowest:
                    winPriority = THREAD_PRIORITY_LOWEST;
                    break;
                case Priority::Low:
                    winPriority = THREAD_PRIORITY_BELOW_NORMAL;
                    break;
                case Priority::Normal:
                    winPriority = THREAD_PRIORITY_NORMAL;
                    break;
                case Priority::High:
                    winPriority = THREAD_PRIORITY_ABOVE_NORMAL;
                    break;
                case Priority::Highest:
                    winPriority = THREAD_PRIORITY_HIGHEST;
                    break;
                default:
                    winPriority = THREAD_PRIORITY_NORMAL;
                    break;
            }
            return SetThreadPriority(GetCurrentThread(), winPriority) != 0;
#elif defined(ATOM_PLATFORM_LINUX) || defined(ATOM_PLATFORM_MACOS)
            int policy;
            struct sched_param param{};

            if (pthread_getschedparam(pthread_self(), &policy, &param) != 0) {
                return false;
            }

            int minPriority = sched_get_priority_min(policy);
            int maxPriority = sched_get_priority_max(policy);
            int priorityRange = maxPriority - minPriority;

            switch (priority) {
                case Priority::Lowest:
                    param.sched_priority = minPriority;
                    break;
                case Priority::Low:
                    param.sched_priority = minPriority + priorityRange / 4;
                    break;
                case Priority::Normal:
                    param.sched_priority = minPriority + priorityRange / 2;
                    break;
                case Priority::High:
                    param.sched_priority = maxPriority - priorityRange / 4;
                    break;
                case Priority::Highest:
                    param.sched_priority = maxPriority;
                    break;
                default:
                    param.sched_priority = minPriority + priorityRange / 2;
                    break;
            }

            return pthread_setschedparam(pthread_self(), policy, &param) == 0;
#else
            return false;
#endif
        }
    };

    /**
     * @brief 使用C++20标准的jthread代替future进行并行for_each操作
     *
     * @tparam Iterator 迭代器类型
     * @tparam Function 函数类型
     * @param begin 范围起始
     * @param end 范围结束
     * @param func 应用的函数
     * @param numThreads 线程数量（0 = 硬件支持的线程数）
     */
    template <typename Iterator, typename Function>
        requires std::invocable<
            Function, typename std::iterator_traits<Iterator>::value_type>
    static void for_each_jthread(Iterator begin, Iterator end, Function func,
                                 size_t numThreads = 0) {
        if (numThreads == 0) {
            numThreads = std::thread::hardware_concurrency();
        }

        const auto range_size = std::distance(begin, end);
        if (range_size == 0)
            return;

        if (range_size <= numThreads || numThreads == 1) {
            // 对于小范围，直接使用std::for_each
            std::for_each(begin, end, func);
            return;
        }

        // 使用std::stop_source来协调线程停止
        std::stop_source stopSource;

        // 使用C++20的std::latch来进行同步
        std::latch completionLatch(numThreads - 1);

        std::vector<std::jthread> threads;
        threads.reserve(numThreads - 1);

        const auto chunk_size = range_size / numThreads;
        auto chunk_begin = begin;

        for (size_t i = 0; i < numThreads - 1; ++i) {
            auto chunk_end = std::next(chunk_begin, chunk_size);

            threads.emplace_back([=, &func, &completionLatch,
                                  stopToken = stopSource.get_token()]() {
                // 如果请求停止，则提前返回
                if (stopToken.stop_requested())
                    return;

                try {
                    // 尝试在特定平台上优化线程性能
                    ThreadConfig::setThreadAffinity(
                        i % std::thread::hardware_concurrency());

                    std::for_each(chunk_begin, chunk_end, func);
                } catch (...) {
                    // 如果一个线程失败，通知其他线程停止
                    stopSource.request_stop();
                }
                completionLatch.count_down();
            });

            chunk_begin = chunk_end;
        }

        // 在当前线程处理最后一个分块
        try {
            std::for_each(chunk_begin, end, func);
        } catch (...) {
            stopSource.request_stop();
            throw;  // 重新抛出异常
        }

        // 等待所有线程完成
        completionLatch.wait();

        // 不需要显式join，jthread会在析构时自动join
    }

    /**
     * @brief Applies a function to each element in a range in parallel
     *
     * @tparam Iterator Iterator type
     * @tparam Function Function type
     * @param begin Start of the range
     * @param end End of the range
     * @param func Function to apply
     * @param numThreads Number of threads to use (0 = hardware concurrency)
     */
    template <typename Iterator, typename Function>
        requires std::invocable<
            Function, typename std::iterator_traits<Iterator>::value_type>
    static void for_each(Iterator begin, Iterator end, Function func,
                         size_t numThreads = 0) {
        if (numThreads == 0) {
            numThreads = std::thread::hardware_concurrency();
        }

        const auto range_size = std::distance(begin, end);
        if (range_size == 0)
            return;

        if (range_size <= numThreads || numThreads == 1) {
            // For small ranges, just use std::for_each
            std::for_each(begin, end, func);
            return;
        }

        std::vector<std::future<void>> futures;
        futures.reserve(numThreads);

        const auto chunk_size = range_size / numThreads;
        auto chunk_begin = begin;

        for (size_t i = 0; i < numThreads - 1; ++i) {
            auto chunk_end = std::next(chunk_begin, chunk_size);

            futures.emplace_back(std::async(std::launch::async, [=, &func] {
                std::for_each(chunk_begin, chunk_end, func);
            }));

            chunk_begin = chunk_end;
        }

        // Process final chunk in this thread
        std::for_each(chunk_begin, end, func);

        // Wait for all other chunks
        for (auto& future : futures) {
            future.wait();
        }
    }

    /**
     * @brief Maps a function over a range in parallel and returns results
     *
     * @tparam Iterator Iterator type
     * @tparam Function Function type
     * @param begin Start of the range
     * @param end End of the range
     * @param func Function to apply
     * @param numThreads Number of threads to use (0 = hardware concurrency)
     * @return Vector of results from applying the function to each element
     */
    template <typename Iterator, typename Function>
        requires std::invocable<
            Function, typename std::iterator_traits<Iterator>::value_type>
    static auto map(Iterator begin, Iterator end, Function func,
                    size_t numThreads = 0)
        -> std::vector<std::invoke_result_t<
            Function, typename std::iterator_traits<Iterator>::value_type>> {
        using ResultType = std::invoke_result_t<
            Function, typename std::iterator_traits<Iterator>::value_type>;

        if (numThreads == 0) {
            numThreads = std::thread::hardware_concurrency();
        }

        const auto range_size = std::distance(begin, end);
        if (range_size == 0)
            return {};

        std::vector<ResultType> results(range_size);

        if (range_size <= numThreads || numThreads == 1) {
            // For small ranges, just process sequentially
            std::transform(begin, end, results.begin(), func);
            return results;
        }

        std::vector<std::future<void>> futures;
        futures.reserve(numThreads);

        const auto chunk_size = range_size / numThreads;
        auto chunk_begin = begin;
        auto result_begin = results.begin();

        for (size_t i = 0; i < numThreads - 1; ++i) {
            auto chunk_end = std::next(chunk_begin, chunk_size);
            auto result_end = std::next(result_begin, chunk_size);

            futures.emplace_back(std::async(std::launch::async, [=, &func] {
                std::transform(chunk_begin, chunk_end, result_begin, func);
            }));

            chunk_begin = chunk_end;
            result_begin = result_end;
        }

        // Process final chunk in this thread
        std::transform(chunk_begin, end, result_begin, func);

        // Wait for all other chunks
        for (auto& future : futures) {
            future.wait();
        }

        return results;
    }

    /**
     * @brief Reduces a range in parallel using a binary operation
     *
     * @tparam Iterator Iterator type
     * @tparam T Result type
     * @tparam BinaryOp Binary operation type
     * @param begin Start of the range
     * @param end End of the range
     * @param init Initial value
     * @param binary_op Binary operation to apply
     * @param numThreads Number of threads to use (0 = hardware concurrency)
     * @return Result of the reduction
     */
    template <typename Iterator, typename T, typename BinaryOp>
        requires std::invocable<
            BinaryOp, T, typename std::iterator_traits<Iterator>::value_type>
    static T reduce(Iterator begin, Iterator end, T init, BinaryOp binary_op,
                    size_t numThreads = 0) {
        if (numThreads == 0) {
            numThreads = std::thread::hardware_concurrency();
        }

        const auto range_size = std::distance(begin, end);
        if (range_size == 0)
            return init;

        if (range_size <= numThreads || numThreads == 1) {
            // For small ranges, just process sequentially
            return std::accumulate(begin, end, init, binary_op);
        }

        std::vector<std::future<T>> futures;
        futures.reserve(numThreads);

        const auto chunk_size = range_size / numThreads;
        auto chunk_begin = begin;

        for (size_t i = 0; i < numThreads - 1; ++i) {
            auto chunk_end = std::next(chunk_begin, chunk_size);

            futures.emplace_back(std::async(std::launch::async, [=,
                                                                 &binary_op] {
                return std::accumulate(chunk_begin, chunk_end, T{}, binary_op);
            }));

            chunk_begin = chunk_end;
        }

        // Process final chunk in this thread
        T result = std::accumulate(chunk_begin, end, T{}, binary_op);

        // Combine all results
        for (auto& future : futures) {
            result = binary_op(result, future.get());
        }

        // Combine with initial value
        return binary_op(init, result);
    }

    /**
     * @brief Partitions a range in parallel based on a predicate
     *
     * @tparam RandomIt Random access iterator type
     * @tparam Predicate Predicate type
     * @param begin Start of the range
     * @param end End of the range
     * @param pred Predicate to test elements
     * @param numThreads Number of threads to use (0 = hardware concurrency)
     * @return Iterator to the first element of the second group
     */
    template <typename RandomIt, typename Predicate>
        requires std::random_access_iterator<RandomIt> &&
                 std::predicate<Predicate, typename std::iterator_traits<
                                               RandomIt>::value_type>
    static RandomIt partition(RandomIt begin, RandomIt end, Predicate pred,
                              size_t numThreads = 0) {
        if (numThreads == 0) {
            numThreads = std::thread::hardware_concurrency();
        }

        const auto range_size = std::distance(begin, end);
        if (range_size <= 1)
            return end;

        if (range_size <= numThreads * 8 || numThreads == 1) {
            // For small ranges, just use standard partition
            return std::partition(begin, end, pred);
        }

        // Determine which elements satisfy the predicate in parallel
        std::vector<bool> satisfies(range_size);
        for_each(
            begin, end,
            [&satisfies, &pred, begin](const auto& item) {
                auto idx = std::distance(begin, &item);
                satisfies[idx] = pred(item);
            },
            numThreads);

        // Count true values to determine partition point
        size_t true_count =
            std::count(satisfies.begin(), satisfies.end(), true);

        // Create a copy of the range
        std::vector<typename std::iterator_traits<RandomIt>::value_type> temp(
            begin, end);

        // Place elements in the correct position
        size_t true_idx = 0;
        size_t false_idx = true_count;

        for (size_t i = 0; i < satisfies.size(); ++i) {
            if (satisfies[i]) {
                *(begin + true_idx++) = std::move(temp[i]);
            } else {
                *(begin + false_idx++) = std::move(temp[i]);
            }
        }

        return begin + true_count;
    }

    /**
     * @brief Filters elements in a range in parallel based on a predicate
     *
     * @tparam Iterator Iterator type
     * @tparam Predicate Predicate type
     * @param begin Start of the range
     * @param end End of the range
     * @param pred Predicate to test elements
     * @param numThreads Number of threads to use (0 = hardware concurrency)
     * @return Vector of elements that satisfy the predicate
     */
    template <typename Iterator, typename Predicate>
        requires std::predicate<
            Predicate, typename std::iterator_traits<Iterator>::value_type>
    static auto filter(Iterator begin, Iterator end, Predicate pred,
                       size_t numThreads = 0)
        -> std::vector<typename std::iterator_traits<Iterator>::value_type> {
        using ValueType = typename std::iterator_traits<Iterator>::value_type;

        if (numThreads == 0) {
            numThreads = std::thread::hardware_concurrency();
        }

        const auto range_size = std::distance(begin, end);
        if (range_size == 0)
            return {};

        if (range_size <= numThreads * 4 || numThreads == 1) {
            // For small ranges, just filter sequentially
            std::vector<ValueType> result;
            for (auto it = begin; it != end; ++it) {
                if (pred(*it)) {
                    result.push_back(*it);
                }
            }
            return result;
        }

        // Create vectors for each thread
        std::vector<std::vector<ValueType>> thread_results(numThreads);

        // Process chunks in parallel
        std::vector<std::future<void>> futures;
        futures.reserve(numThreads);

        const auto chunk_size = range_size / numThreads;
        auto chunk_begin = begin;

        for (size_t i = 0; i < numThreads - 1; ++i) {
            auto chunk_end = std::next(chunk_begin, chunk_size);

            futures.emplace_back(
                std::async(std::launch::async, [=, &pred, &thread_results] {
                    auto& result = thread_results[i];
                    for (auto it = chunk_begin; it != chunk_end; ++it) {
                        if (pred(*it)) {
                            result.push_back(*it);
                        }
                    }
                }));

            chunk_begin = chunk_end;
        }

        // Process final chunk in this thread
        auto& last_result = thread_results[numThreads - 1];
        for (auto it = chunk_begin; it != end; ++it) {
            if (pred(*it)) {
                last_result.push_back(*it);
            }
        }

        // Wait for all other chunks
        for (auto& future : futures) {
            future.wait();
        }

        // Combine results
        std::vector<ValueType> result;
        size_t total_size = 0;
        for (const auto& vec : thread_results) {
            total_size += vec.size();
        }

        result.reserve(total_size);
        for (auto& vec : thread_results) {
            result.insert(result.end(), std::make_move_iterator(vec.begin()),
                          std::make_move_iterator(vec.end()));
        }

        return result;
    }

    /**
     * @brief Sorts a range in parallel
     *
     * @tparam RandomIt Random access iterator type
     * @tparam Compare Comparison function type
     * @param begin Start of the range
     * @param end End of the range
     * @param comp Comparison function
     * @param numThreads Number of threads to use (0 = hardware concurrency)
     */
    template <typename RandomIt, typename Compare = std::less<>>
        requires std::random_access_iterator<RandomIt>
    static void sort(RandomIt begin, RandomIt end, Compare comp = Compare{},
                     size_t numThreads = 0) {
        if (numThreads == 0) {
            numThreads = std::thread::hardware_concurrency();
        }

        const auto range_size = std::distance(begin, end);
        if (range_size <= 1)
            return;

        if (range_size <= 1000 || numThreads == 1) {
            // For small ranges, just use standard sort
            std::sort(begin, end, comp);
            return;
        }

        try {
            // Use parallel execution policy if available
            std::sort(std::execution::par, begin, end, comp);
        } catch (const std::exception&) {
            // Fall back to manual parallel sort if parallel execution policy
            // fails
            parallelQuickSort(begin, end, comp, numThreads);
        }
    }

    /**
     * @brief 使用 C++20 的 std::span 进行并行映射操作
     *
     * @tparam T 输入元素类型
     * @tparam R 输出元素类型
     * @tparam Function 映射函数类型
     * @param input 输入数据视图
     * @param func 映射函数
     * @param numThreads 线程数量（0 = 硬件支持的线程数）
     * @return 映射结果的向量
     */
    template <typename T, typename Function>
        requires std::invocable<Function, T>
    static auto map_span(std::span<const T> input, Function func,
                         size_t numThreads = 0)
        -> std::vector<std::invoke_result_t<Function, T>> {
        using ResultType = std::invoke_result_t<Function, T>;

        if (numThreads == 0) {
            numThreads = std::thread::hardware_concurrency();
        }

        if (input.empty())
            return {};

        std::vector<ResultType> results(input.size());

        if (input.size() <= numThreads || numThreads == 1) {
            // 对于小范围，直接使用 std::transform
            std::transform(input.begin(), input.end(), results.begin(), func);
            return results;
        }

        // 使用C++20的std::barrier进行同步
        std::atomic<size_t> completedThreads{0};
        std::barrier sync_point(numThreads, [&completedThreads]() noexcept {
            ++completedThreads;
            return completedThreads.load() == 1;
        });

        std::vector<std::jthread> threads;
        threads.reserve(numThreads - 1);

        const auto chunk_size = input.size() / numThreads;

        for (size_t i = 0; i < numThreads - 1; ++i) {
            size_t start = i * chunk_size;
            size_t end = (i + 1) * chunk_size;

            threads.emplace_back(
                [start, end, &input, &results, &func, &sync_point]() {
                    // 平台特定优化
                    ThreadConfig::setThreadAffinity(
                        start % std::thread::hardware_concurrency());

                    // 处理当前数据块
                    for (size_t j = start; j < end; ++j) {
                        results[j] = func(input[j]);
                    }

                    // 同步点
                    sync_point.arrive_and_wait();
                });
        }

        // 在当前线程处理最后一块
        for (size_t j = (numThreads - 1) * chunk_size; j < input.size(); ++j) {
            results[j] = func(input[j]);
        }

        // 等待所有线程完成（同步点）
        sync_point.arrive_and_wait();

        return results;
    }

    /**
     * @brief 使用 C++20 ranges 进行并行过滤操作
     *
     * @tparam Range 范围类型
     * @tparam Predicate 谓词类型
     * @param range 输入范围
     * @param pred 谓词函数
     * @param numThreads 线程数量（0 = 硬件支持的线程数）
     * @return 过滤后的元素向量
     */
    template <std::ranges::input_range Range, typename Predicate>
        requires std::predicate<Predicate, std::ranges::range_value_t<Range>>
    static auto filter_range(Range&& range, Predicate pred,
                             size_t numThreads = 0)
        -> std::vector<std::ranges::range_value_t<Range>> {
        using ValueType = std::ranges::range_value_t<Range>;

        if (numThreads == 0) {
            numThreads = std::thread::hardware_concurrency();
        }

        // 使用 ranges 将范围转换为向量
        auto data = std::ranges::to<std::vector>(range);

        if (data.empty())
            return {};

        if (data.size() <= numThreads * 4 || numThreads == 1) {
            // 小范围直接使用 ranges 过滤
            auto filtered = data | std::views::filter(pred);
            return std::ranges::to<std::vector>(filtered);
        }

        // 为每个线程创建结果向量
        std::vector<std::vector<ValueType>> thread_results(numThreads);

        std::vector<std::jthread> threads;
        threads.reserve(numThreads - 1);

        const auto chunk_size = data.size() / numThreads;

        for (size_t i = 0; i < numThreads - 1; ++i) {
            size_t start = i * chunk_size;
            size_t end = (i + 1) * chunk_size;

            threads.emplace_back(
                [start, end, &data, &thread_results, &pred, i]() {
                    auto& result = thread_results[i];
                    auto chunk_span =
                        std::span(data.begin() + start, data.begin() + end);

                    for (const auto& item : chunk_span) {
                        if (pred(item)) {
                            result.push_back(item);
                        }
                    }
                });
        }

        // 在当前线程处理最后一块
        auto& last_result = thread_results[numThreads - 1];
        auto last_chunk =
            std::span(data.begin() + (numThreads - 1) * chunk_size, data.end());

        for (const auto& item : last_chunk) {
            if (pred(item)) {
                last_result.push_back(item);
            }
        }

        // 组合结果
        std::vector<ValueType> result;
        size_t total_size = 0;

        for (const auto& vec : thread_results) {
            total_size += vec.size();
        }

        result.reserve(total_size);

        for (auto& vec : thread_results) {
            result.insert(result.end(), std::make_move_iterator(vec.begin()),
                          std::make_move_iterator(vec.end()));
        }

        return result;
    }

    /**
     * @brief 使用协程异步执行任务
     *
     * @tparam Func 函数类型
     * @tparam Args 参数类型
     * @param func 要异步执行的函数
     * @param args 函数参数
     * @return 包含函数结果的协程任务
     */
    template <typename Func, typename... Args>
        requires std::invocable<Func, Args...>
    static auto async(Func&& func, Args&&... args)
        -> Task<std::invoke_result_t<Func, Args...>> {
        using ReturnType = std::invoke_result_t<Func, Args...>;

        if constexpr (std::is_void_v<ReturnType>) {
            std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
            co_return;
        } else {
            co_return std::invoke(std::forward<Func>(func),
                                  std::forward<Args>(args)...);
        }
    }

    /**
     * @brief 使用协程并行执行多个任务
     *
     * @tparam Tasks 任务类型参数包
     * @param tasks 要并行执行的协程任务
     * @return 包含所有任务结果的协程任务
     */
    template <typename... Tasks>
        requires(std::same_as<Tasks, Task<void>> && ...)
    static Task<void> when_all(Tasks&&... tasks) {
        // 使用折叠表达式调用每个任务的 get() 方法
        (tasks.get(), ...);
        co_return;
    }

    /**
     * @brief 使用协程并行执行一个函数在多个输入上
     *
     * @tparam T 输入类型
     * @tparam Func 函数类型
     * @param inputs 输入向量
     * @param func 要应用的函数
     * @param numThreads 线程数量（0 = 硬件支持的线程数）
     * @return 包含结果的协程任务
     */
    template <typename T, typename Func>
        requires std::invocable<Func, T>
    static auto parallel_for_each_async(std::span<const T> inputs, Func&& func,
                                        size_t numThreads = 0) -> Task<void> {
        if (numThreads == 0) {
            numThreads = std::thread::hardware_concurrency();
        }

        if (inputs.empty()) {
            co_return;
        }

        if (inputs.size() <= numThreads || numThreads == 1) {
            // 对于小范围，直接处理
            for (const auto& item : inputs) {
                std::invoke(func, item);
            }
            co_return;
        }

        // 将输入分成块，并为每个块创建一个任务
        std::vector<Task<void>> tasks;
        tasks.reserve(numThreads);

        const size_t chunk_size = inputs.size() / numThreads;

        for (size_t i = 0; i < numThreads - 1; ++i) {
            const size_t start = i * chunk_size;
            const size_t end = (i + 1) * chunk_size;

            tasks.push_back(async([&func, inputs, start, end]() {
                for (size_t j = start; j < end; ++j) {
                    std::invoke(func, inputs[j]);
                }
            }));
        }

        // 处理最后一块
        const size_t start = (numThreads - 1) * chunk_size;
        for (size_t j = start; j < inputs.size(); ++j) {
            std::invoke(func, inputs[j]);
        }

        // 等待所有任务完成
        for (auto& task : tasks) {
            task.get();
        }

        co_return;
    }

private:
    /**
     * @brief Helper function for parallel quicksort
     */
    template <typename RandomIt, typename Compare>
    static void parallelQuickSort(RandomIt begin, RandomIt end, Compare comp,
                                  size_t numThreads) {
        const auto range_size = std::distance(begin, end);

        if (range_size <= 1)
            return;

        if (range_size <= 1000 || numThreads <= 1) {
            std::sort(begin, end, comp);
            return;
        }

        auto pivot = *std::next(begin, range_size / 2);

        auto middle = std::partition(
            begin, end,
            [&pivot, &comp](const auto& elem) { return comp(elem, pivot); });

        std::future<void> future = std::async(std::launch::async, [&]() {
            parallelQuickSort(begin, middle, comp, numThreads / 2);
        });

        parallelQuickSort(middle, end, comp, numThreads / 2);

        future.wait();
    }
};

/**
 * @brief 增强的 SIMD 操作类，提供平台特定优化
 */
class SimdOps {
public:
    /**
     * @brief 使用 SIMD 指令（如可用）对两个数组进行元素级加法
     *
     * @tparam T 元素类型
     * @param a 第一个数组
     * @param b 第二个数组
     * @param result 结果数组
     * @param size 数组大小
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    static void add(const T* a, const T* b, T* result, size_t size) {
        // 空指针检查
        if (!a || !b || !result) {
            throw std::invalid_argument("输入数组不能为空");
        }

// 基于不同的 SIMD 指令集优化
#if defined(ATOM_SIMD_AVX512) && defined(__AVX512F__) && !defined(__APPLE__)
        if constexpr (std::is_same_v<T, float> && size >= 16) {
            simd_add_avx512(a, b, result, size);
            return;
        }
#elif defined(ATOM_SIMD_AVX2) && defined(__AVX2__)
        if constexpr (std::is_same_v<T, float> && size >= 8) {
            simd_add_avx2(a, b, result, size);
            return;
        }
#elif defined(ATOM_SIMD_NEON) && defined(__ARM_NEON)
        if constexpr (std::is_same_v<T, float> && size >= 4) {
            simd_add_neon(a, b, result, size);
            return;
        }
#endif

        // 标准实现使用 std::execution::par_unseq
        std::transform(std::execution::par_unseq, a, a + size, b, result,
                       std::plus<T>());
    }

    /**
     * @brief 使用 SIMD 指令（如可用）对两个数组进行元素级乘法
     *
     * @tparam T 元素类型
     * @param a 第一个数组
     * @param b 第二个数组
     * @param result 结果数组
     * @param size 数组大小
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    static void multiply(const T* a, const T* b, T* result, size_t size) {
        // 空指针检查
        if (!a || !b || !result) {
            throw std::invalid_argument("输入数组不能为空");
        }

// 基于不同的 SIMD 指令集优化
#if defined(ATOM_SIMD_AVX512) && defined(__AVX512F__) && !defined(__APPLE__)
        if constexpr (std::is_same_v<T, float> && size >= 16) {
            simd_multiply_avx512(a, b, result, size);
            return;
        }
#elif defined(ATOM_SIMD_AVX2) && defined(__AVX2__)
        if constexpr (std::is_same_v<T, float> && size >= 8) {
            simd_multiply_avx2(a, b, result, size);
            return;
        }
#elif defined(ATOM_SIMD_NEON) && defined(__ARM_NEON)
        if constexpr (std::is_same_v<T, float> && size >= 4) {
            simd_multiply_neon(a, b, result, size);
            return;
        }
#endif

        // 标准实现使用 std::execution::par_unseq
        std::transform(std::execution::par_unseq, a, a + size, b, result,
                       std::multiplies<T>());
    }

    /**
     * @brief 使用 SIMD 指令（如可用）计算两个向量的点积
     *
     * @tparam T 元素类型
     * @param a 第一个向量
     * @param b 第二个向量
     * @param size 向量大小
     * @return 点积结果
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    static T dotProduct(const T* a, const T* b, size_t size) {
        // 空指针检查
        if (!a || !b) {
            throw std::invalid_argument("输入数组不能为空");
        }

// 基于不同的 SIMD 指令集优化
#if defined(ATOM_SIMD_AVX512) && defined(__AVX512F__) && !defined(__APPLE__)
        if constexpr (std::is_same_v<T, float> && size >= 16) {
            return simd_dot_product_avx512(a, b, size);
        }
#elif defined(ATOM_SIMD_AVX2) && defined(__AVX2__)
        if constexpr (std::is_same_v<T, float> && size >= 8) {
            return simd_dot_product_avx2(a, b, size);
        }
#elif defined(ATOM_SIMD_NEON) && defined(__ARM_NEON)
        if constexpr (std::is_same_v<T, float> && size >= 4) {
            return simd_dot_product_neon(a, b, size);
        }
#endif

        // 使用 std::transform_reduce 并行化
        return std::transform_reduce(std::execution::par_unseq, a, a + size, b,
                                     T{0}, std::plus<T>(),
                                     std::multiplies<T>());
    }

    /**
     * @brief 使用 C++20 的 std::span 进行向量点积计算
     *
     * @tparam T 元素类型
     * @param a 第一个向量视图
     * @param b 第二个向量视图
     * @return 点积结果
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    static T dotProduct(std::span<const T> a, std::span<const T> b) {
        if (a.size() != b.size()) {
            throw std::invalid_argument("向量长度必须相同");
        }

        return dotProduct(a.data(), b.data(), a.size());
    }

private:
// AVX-512 特定优化实现
#if defined(ATOM_SIMD_AVX512) && defined(__AVX512F__) && !defined(__APPLE__)
    static void simd_add_avx512(const float* a, const float* b, float* result,
                                size_t size) {
        size_t i = 0;
        const size_t simdSize = size - (size % 16);

        for (; i < simdSize; i += 16) {
            __m512 va = _mm512_loadu_ps(a + i);
            __m512 vb = _mm512_loadu_ps(b + i);
            __m512 vr = _mm512_add_ps(va, vb);
            _mm512_storeu_ps(result + i, vr);
        }

        // 处理剩余元素
        for (; i < size; ++i) {
            result[i] = a[i] + b[i];
        }
    }

    static void simd_multiply_avx512(const float* a, const float* b,
                                     float* result, size_t size) {
        size_t i = 0;
        const size_t simdSize = size - (size % 16);

        for (; i < simdSize; i += 16) {
            __m512 va = _mm512_loadu_ps(a + i);
            __m512 vb = _mm512_loadu_ps(b + i);
            __m512 vr = _mm512_mul_ps(va, vb);
            _mm512_storeu_ps(result + i, vr);
        }

        // 处理剩余元素
        for (; i < size; ++i) {
            result[i] = a[i] * b[i];
        }
    }

    static float simd_dot_product_avx512(const float* a, const float* b,
                                         size_t size) {
        size_t i = 0;
        const size_t simdSize = size - (size % 16);
        __m512 sum = _mm512_setzero_ps();

        for (; i < simdSize; i += 16) {
            __m512 va = _mm512_loadu_ps(a + i);
            __m512 vb = _mm512_loadu_ps(b + i);
            __m512 mul = _mm512_mul_ps(va, vb);
            sum = _mm512_add_ps(sum, mul);
        }

        float result = _mm512_reduce_add_ps(sum);

        // 处理剩余元素
        for (; i < size; ++i) {
            result += a[i] * b[i];
        }

        return result;
    }
#endif

// AVX2 特定优化实现
#if defined(ATOM_SIMD_AVX2) && defined(__AVX2__)
    static void simd_add_avx2(const float* a, const float* b, float* result,
                              size_t size) {
        size_t i = 0;
        const size_t simdSize = size - (size % 8);

        for (; i < simdSize; i += 8) {
            __m256 va = _mm256_loadu_ps(a + i);
            __m256 vb = _mm256_loadu_ps(b + i);
            __m256 vr = _mm256_add_ps(va, vb);
            _mm256_storeu_ps(result + i, vr);
        }

        // 处理剩余元素
        for (; i < size; ++i) {
            result[i] = a[i] + b[i];
        }
    }

    static void simd_multiply_avx2(const float* a, const float* b,
                                   float* result, size_t size) {
        size_t i = 0;
        const size_t simdSize = size - (size % 8);

        for (; i < simdSize; i += 8) {
            __m256 va = _mm256_loadu_ps(a + i);
            __m256 vb = _mm256_loadu_ps(b + i);
            __m256 vr = _mm256_mul_ps(va, vb);
            _mm256_storeu_ps(result + i, vr);
        }

        // 处理剩余元素
        for (; i < size; ++i) {
            result[i] = a[i] * b[i];
        }
    }

    static float simd_dot_product_avx2(const float* a, const float* b,
                                       size_t size) {
        size_t i = 0;
        const size_t simdSize = size - (size % 8);
        __m256 sum = _mm256_setzero_ps();

        for (; i < simdSize; i += 8) {
            __m256 va = _mm256_loadu_ps(a + i);
            __m256 vb = _mm256_loadu_ps(b + i);
            __m256 mul = _mm256_mul_ps(va, vb);
            sum = _mm256_add_ps(sum, mul);
        }

        // 水平求和
        __m128 half = _mm_add_ps(_mm256_extractf128_ps(sum, 0),
                                 _mm256_extractf128_ps(sum, 1));
        half = _mm_hadd_ps(half, half);
        half = _mm_hadd_ps(half, half);
        float result = _mm_cvtss_f32(half);

        // 处理剩余元素
        for (; i < size; ++i) {
            result += a[i] * b[i];
        }

        return result;
    }
#endif

// ARM NEON 特定优化实现
#if defined(ATOM_SIMD_NEON) && defined(__ARM_NEON)
    static void simd_add_neon(const float* a, const float* b, float* result,
                              size_t size) {
        size_t i = 0;
        const size_t simdSize = size - (size % 4);

        for (; i < simdSize; i += 4) {
            float32x4_t va = vld1q_f32(a + i);
            float32x4_t vb = vld1q_f32(b + i);
            float32x4_t vr = vaddq_f32(va, vb);
            vst1q_f32(result + i, vr);
        }

        // 处理剩余元素
        for (; i < size; ++i) {
            result[i] = a[i] + b[i];
        }
    }

    static void simd_multiply_neon(const float* a, const float* b,
                                   float* result, size_t size) {
        size_t i = 0;
        const size_t simdSize = size - (size % 4);

        for (; i < simdSize; i += 4) {
            float32x4_t va = vld1q_f32(a + i);
            float32x4_t vb = vld1q_f32(b + i);
            float32x4_t vr = vmulq_f32(va, vb);
            vst1q_f32(result + i, vr);
        }

        // 处理剩余元素
        for (; i < size; ++i) {
            result[i] = a[i] * b[i];
        }
    }

    static float simd_dot_product_neon(const float* a, const float* b,
                                       size_t size) {
        size_t i = 0;
        const size_t simdSize = size - (size % 4);
        float32x4_t sum = vdupq_n_f32(0.0f);

        for (; i < simdSize; i += 4) {
            float32x4_t va = vld1q_f32(a + i);
            float32x4_t vb = vld1q_f32(b + i);
            sum = vmlaq_f32(sum, va, vb);
        }

        // 水平求和
        float32x2_t sum2 = vadd_f32(vget_low_f32(sum), vget_high_f32(sum));
        sum2 = vpadd_f32(sum2, sum2);
        float result = vget_lane_f32(sum2, 0);

        // 处理剩余元素
        for (; i < size; ++i) {
            result += a[i] * b[i];
        }

        return result;
    }
#endif
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_PARALLEL_HPP