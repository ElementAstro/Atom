#ifndef ATOM_ALGORITHM_COMMON_PARALLEL_HPP
#define ATOM_ALGORITHM_COMMON_PARALLEL_HPP

#include <algorithm>
#include <cstddef>
#include <functional>
#include <future>
#include <span>
#include <thread>
#include <vector>

#include "atom/algorithm/core/rust_numeric.hpp"

namespace atom::algorithm {

/**
 * @brief Parallel processing utilities.
 */
namespace parallel {

/**
 * @brief Get the default number of threads for parallel operations.
 * @return The number of hardware threads, or 4 if unavailable.
 */
[[nodiscard]] inline auto getDefaultThreadCount() noexcept -> usize {
    auto count = std::thread::hardware_concurrency();
    return count > 0 ? static_cast<usize>(count) : 4;
}

/**
 * @brief Calculate optimal chunk size for parallel processing.
 * @param total_size Total number of elements.
 * @param num_threads Number of threads to use.
 * @param min_chunk_size Minimum chunk size (default: 1024).
 * @return The optimal chunk size.
 */
[[nodiscard]] constexpr auto calculateChunkSize(
    usize total_size, usize num_threads,
    usize min_chunk_size = 1024) noexcept -> usize {
    if (num_threads == 0 || total_size == 0) {
        return total_size;
    }

    usize chunk_size = (total_size + num_threads - 1) / num_threads;
    return std::max(chunk_size, min_chunk_size);
}

/**
 * @brief Process a range in parallel using multiple threads.
 *
 * @tparam T Element type.
 * @tparam Func Function type: void(std::span<T>, usize thread_index).
 * @param data The data to process.
 * @param processor Function to process each chunk.
 * @param num_threads Number of threads (0 = auto).
 * @param min_chunk_size Minimum elements per chunk.
 */
template <typename T, typename Func>
void parallelForEach(std::span<T> data, Func&& processor, usize num_threads = 0,
                     usize min_chunk_size = 1024) {
    if (data.empty()) {
        return;
    }

    if (num_threads == 0) {
        num_threads = getDefaultThreadCount();
    }

    const usize total_size = data.size();
    const usize chunk_size = calculateChunkSize(total_size, num_threads, min_chunk_size);

    // If data is too small, process sequentially
    if (total_size <= min_chunk_size || num_threads == 1) {
        processor(data, 0);
        return;
    }

    std::vector<std::future<void>> futures;
    futures.reserve(num_threads);

    for (usize i = 0; i < num_threads && i * chunk_size < total_size; ++i) {
        usize start = i * chunk_size;
        usize end = std::min(start + chunk_size, total_size);
        auto chunk = data.subspan(start, end - start);

        futures.push_back(
            std::async(std::launch::async,
                       [chunk, &processor, i]() { processor(chunk, i); }));
    }

    for (auto& future : futures) {
        future.get();
    }
}

/**
 * @brief Process a range in parallel and collect results.
 *
 * @tparam T Input element type.
 * @tparam R Result type from each chunk.
 * @tparam Func Function type: R(std::span<const T>, usize thread_index).
 * @param data The data to process.
 * @param processor Function to process each chunk.
 * @param num_threads Number of threads (0 = auto).
 * @param min_chunk_size Minimum elements per chunk.
 * @return Vector of results from each chunk.
 */
template <typename T, typename R, typename Func>
[[nodiscard]] auto parallelMap(std::span<const T> data, Func&& processor,
                               usize num_threads = 0,
                               usize min_chunk_size = 1024) -> std::vector<R> {
    if (data.empty()) {
        return {};
    }

    if (num_threads == 0) {
        num_threads = getDefaultThreadCount();
    }

    const usize total_size = data.size();
    const usize chunk_size = calculateChunkSize(total_size, num_threads, min_chunk_size);

    // If data is too small, process sequentially
    if (total_size <= min_chunk_size || num_threads == 1) {
        return {processor(data, 0)};
    }

    std::vector<std::future<R>> futures;
    futures.reserve(num_threads);

    for (usize i = 0; i < num_threads && i * chunk_size < total_size; ++i) {
        usize start = i * chunk_size;
        usize end = std::min(start + chunk_size, total_size);
        auto chunk = data.subspan(start, end - start);

        futures.push_back(std::async(std::launch::async, [chunk, &processor,
                                                          i]() -> R {
            return processor(chunk, i);
        }));
    }

    std::vector<R> results;
    results.reserve(futures.size());

    for (auto& future : futures) {
        results.push_back(future.get());
    }

    return results;
}

/**
 * @brief Process a range in parallel and reduce results.
 *
 * @tparam T Input element type.
 * @tparam R Accumulator type.
 * @tparam MapFunc Function type: R(std::span<const T>, usize thread_index).
 * @tparam ReduceFunc Function type: R(R, R).
 * @param data The data to process.
 * @param mapper Function to process each chunk.
 * @param reducer Function to combine results.
 * @param initial Initial value for reduction.
 * @param num_threads Number of threads (0 = auto).
 * @return The reduced result.
 */
template <typename T, typename R, typename MapFunc, typename ReduceFunc>
[[nodiscard]] auto parallelMapReduce(std::span<const T> data, MapFunc&& mapper,
                                     ReduceFunc&& reducer, R initial,
                                     usize num_threads = 0) -> R {
    auto partial_results = parallelMap<T, R>(
        data, std::forward<MapFunc>(mapper), num_threads);

    R result = initial;
    for (const auto& partial : partial_results) {
        result = reducer(result, partial);
    }

    return result;
}

}  // namespace parallel

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_COMMON_PARALLEL_HPP
