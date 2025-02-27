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
#include <execution>
#include <future>
#include <numeric>
#include <thread>
#include <type_traits>
#include <vector>

namespace atom::async {

/**
 * @brief Parallel algorithm utilities for high-performance computations
 */
class Parallel {
public:
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
        requires std::invocable<Function, typename std::iterator_traits<
                                              Iterator>::value_type>
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
        requires std::predicate<Predicate, typename std::iterator_traits<
                                               Iterator>::value_type>
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
 * @brief SIMD-enabled operations for high-performance computing
 *
 * This class provides optimizations using SIMD instructions where available
 */
class SimdOps {
public:
    /**
     * @brief Adds two arrays element-wise using SIMD instructions if possible
     *
     * @tparam T Element type
     * @param a First array
     * @param b Second array
     * @param result Result array
     * @param size Array size
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    static void add(const T* a, const T* b, T* result, size_t size) {
        // This would normally use SIMD instructions based on architecture
        // For now, we just implement a standard loop with some optimizations

        // Check for nullptr
        if (!a || !b || !result) {
            throw std::invalid_argument("Input arrays cannot be null");
        }

        // Ensure proper alignment for SIMD operations
        constexpr size_t align = 32;  // AVX-256 alignment

        if (size >= 4) {
            size_t i = 0;

            // Vectorized loop for aligned data
            if (reinterpret_cast<uintptr_t>(a) % align == 0 &&
                reinterpret_cast<uintptr_t>(b) % align == 0 &&
                reinterpret_cast<uintptr_t>(result) % align == 0) {
                // Use std::execution::par for potential SIMD parallelization
                std::transform(std::execution::par_unseq, a, a + size, b,
                               result, std::plus<T>());
                return;
            }

            // Standard processing for non-aligned data
            for (; i < size; ++i) {
                result[i] = a[i] + b[i];
            }
        } else {
            // Scalar loop for small arrays
            for (size_t i = 0; i < size; ++i) {
                result[i] = a[i] + b[i];
            }
        }
    }

    /**
     * @brief Multiplies two arrays element-wise using SIMD instructions if
     * possible
     *
     * @tparam T Element type
     * @param a First array
     * @param b Second array
     * @param result Result array
     * @param size Array size
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    static void multiply(const T* a, const T* b, T* result, size_t size) {
        // This would normally use SIMD instructions based on architecture
        // For now, we just implement a standard loop with some optimizations

        // Check for nullptr
        if (!a || !b || !result) {
            throw std::invalid_argument("Input arrays cannot be null");
        }

        // Use std::execution::par_unseq for potential SIMD parallelization
        std::transform(std::execution::par_unseq, a, a + size, b, result,
                       std::multiplies<T>());
    }

    /**
     * @brief Calculates the dot product of two vectors using SIMD if possible
     *
     * @tparam T Element type
     * @param a First vector
     * @param b Second vector
     * @param size Vector size
     * @return Dot product result
     */
    template <typename T>
        requires std::is_arithmetic_v<T>
    static T dotProduct(const T* a, const T* b, size_t size) {
        // Check for nullptr
        if (!a || !b) {
            throw std::invalid_argument("Input arrays cannot be null");
        }

        // Use std::inner_product with parallel reduction if possible
        return std::transform_reduce(std::execution::par_unseq, a, a + size, b,
                                     T{0}, std::plus<T>(),
                                     std::multiplies<T>());
    }
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_PARALLEL_HPP