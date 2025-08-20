/*
 * ranges.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-12

Description: Some ranges functions for C++20

**************************************************/

#ifndef ATOM_UTILS_RANGES_HPP
#define ATOM_UTILS_RANGES_HPP

#include <algorithm>
#include <array>
#include <concepts>
#include <coroutine>
#include <functional>
#include <map>
#include <numeric>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace atom::utils {

/**
 * @brief Filters elements in a range satisfying a predicate and transforms them
 * using a function
 * @tparam Range The type of the range
 * @tparam Pred The type of the predicate
 * @tparam Func The type of the function
 * @param range The input range
 * @param pred The predicate function
 * @param func The transformation function
 * @return The transformed range
 */
template <typename Range, typename Pred, typename Func>
[[nodiscard]] auto filterAndTransform(Range&& range, Pred&& pred, Func&& func) {
    return std::forward<Range>(range) |
           std::views::filter(std::forward<Pred>(pred)) |
           std::views::transform(std::forward<Func>(func));
}

/**
 * @brief Finds an element in a range
 * @tparam Range The type of the range
 * @tparam T The type of the value to find
 * @param range The input range
 * @param value The value to find
 * @return An optional containing the found value, or nullopt if not found
 */
template <typename Range, typename T>
[[nodiscard]] auto findElement(Range&& range, const T& value) noexcept
    -> std::optional<std::remove_cvref_t<std::ranges::range_value_t<Range>>> {
    auto it = std::ranges::find(std::forward<Range>(range), value);
    return it != std::ranges::end(range)
               ? std::optional<std::remove_cvref_t<decltype(*it)>>{*it}
               : std::nullopt;
}

/**
 * @brief Groups elements in a range by a key and aggregates values using an
 * aggregator function
 * @tparam Range The type of the range
 * @tparam KeySelector The type of the key selector function
 * @tparam Aggregator The type of the aggregator function
 * @param range The input range
 * @param key_selector The function to extract keys from elements
 * @param aggregator The function to aggregate values
 * @return A map containing grouped and aggregated results
 */
template <typename Range, typename KeySelector, typename Aggregator>
[[nodiscard]] auto groupAndAggregate(Range&& range, KeySelector&& key_selector,
                                     Aggregator&& aggregator) {
    using Key = std::invoke_result_t<KeySelector,
                                     std::ranges::range_reference_t<Range>>;
    using Value =
        std::invoke_result_t<Aggregator, std::ranges::range_reference_t<Range>>;

    std::map<Key, Value> result;
    for (auto&& item : std::forward<Range>(range)) {
        Key key = std::invoke(std::forward<KeySelector>(key_selector), item);
        Value value = std::invoke(std::forward<Aggregator>(aggregator), item);
        result[key] += value;
    }
    return result;
}

/**
 * @brief Drops the first n elements from a range
 * @tparam Range The type of the range
 * @param range The input range
 * @param n The number of elements to drop
 * @return The range with the first n elements dropped
 */
template <typename Range>
[[nodiscard]] auto drop(Range&& range,
                        std::ranges::range_difference_t<Range> n) {
    return std::forward<Range>(range) | std::views::drop(n);
}

/**
 * @brief Takes the first n elements from a range
 * @tparam Range The type of the range
 * @param range The input range
 * @param n The number of elements to take
 * @return The range with the first n elements taken
 */
template <typename Range>
[[nodiscard]] auto take(Range&& range,
                        std::ranges::range_difference_t<Range> n) {
    return std::forward<Range>(range) | std::views::take(n);
}

/**
 * @brief Takes elements from a range while a predicate is true
 * @tparam Range The type of the range
 * @tparam Pred The type of the predicate function
 * @param range The input range
 * @param pred The predicate function
 * @return The range with elements taken while the predicate is true
 */
template <typename Range, typename Pred>
[[nodiscard]] auto takeWhile(Range&& range, Pred&& pred) {
    return std::forward<Range>(range) |
           std::views::take_while(std::forward<Pred>(pred));
}

/**
 * @brief Drops elements from a range while a predicate is true
 * @tparam Range The type of the range
 * @tparam Pred The type of the predicate function
 * @param range The input range
 * @param pred The predicate function
 * @return The range with elements dropped while the predicate is true
 */
template <typename Range, typename Pred>
[[nodiscard]] auto dropWhile(Range&& range, Pred&& pred) {
    return std::forward<Range>(range) |
           std::views::drop_while(std::forward<Pred>(pred));
}

/**
 * @brief Reverses the elements in a range
 * @tparam Range The type of the range
 * @param range The input range
 * @return The reversed range
 */
template <typename Range>
[[nodiscard]] auto reverse(Range&& range) {
    return std::forward<Range>(range) | std::views::reverse;
}

/**
 * @brief Accumulates the elements in a range using a binary operation
 * @tparam Range The type of the range
 * @tparam T The type of the initial value
 * @tparam BinaryOp The type of the binary operation function
 * @param range The input range
 * @param init The initial value for accumulation
 * @param op The binary operation function
 * @return The result of the accumulation
 */
template <typename Range, typename T, typename BinaryOp>
[[nodiscard]] auto accumulate(Range&& range, T init, BinaryOp&& op) noexcept {
    return std::accumulate(std::begin(range), std::end(range), std::move(init),
                           std::forward<BinaryOp>(op));
}

/**
 * @brief Slices a container into a new container
 * @tparam Container The type of the container
 * @tparam Index The type of the index
 * @param container The input container
 * @param start The starting index of the slice
 * @param end The ending index of the slice (exclusive)
 * @return A new container containing the sliced elements
 */
template <typename Container, typename Index>
[[nodiscard]] auto slice(const Container& container, Index start, Index end) {
    if (start >= container.size()) {
        return std::vector<typename Container::value_type>{};
    }

    auto actual_end = std::min(static_cast<size_t>(end), container.size());
    auto first = std::begin(container) + start;
    auto last = std::begin(container) + actual_end;

    return std::vector<typename Container::value_type>(first, last);
}

/**
 * @brief Converts any range to a vector
 * @tparam Range The type of the range
 * @param range The input range
 * @return A vector containing all elements from the range
 */
template <typename Range>
[[nodiscard]] auto toVector(Range&& range) {
    using ValueType = std::ranges::range_value_t<std::remove_cvref_t<Range>>;
    std::vector<ValueType> result;

    if constexpr (std::ranges::sized_range<std::remove_cvref_t<Range>>) {
        result.reserve(std::ranges::size(range));
    }

    for (auto&& item : range) {
        result.push_back(std::forward<decltype(item)>(item));
    }
    return result;
}

/**
 * @brief Coroutine-based generator for lazy evaluation
 * @tparam T The type of values generated
 */
template <typename T>
struct Generator {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        T value;

        auto yield_value(T val) noexcept {
            this->value = std::move(val);
            return std::suspend_always{};
        }
        auto initial_suspend() noexcept { return std::suspend_always{}; }
        auto final_suspend() noexcept { return std::suspend_always{}; }
        auto get_return_object() noexcept {
            return Generator{handle_type::from_promise(*this)};
        }
        void unhandled_exception() { std::terminate(); }
        void return_void() {}
    };

    handle_type handle;

    explicit Generator(handle_type h) : handle(h) {}

    ~Generator() {
        if (handle) {
            handle.destroy();
        }
    }

    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    Generator(Generator&& other) noexcept
        : handle(std::exchange(other.handle, {})) {}

    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (handle) {
                handle.destroy();
            }
            handle = std::exchange(other.handle, {});
        }
        return *this;
    }

    struct iterator {
        handle_type handle = nullptr;
        bool done = true;

        iterator() = default;
        explicit iterator(handle_type h) : handle(h) {
            if (handle) {
                handle.resume();
                done = handle.done();
            }
        }

        iterator& operator++() {
            if (handle) {
                handle.resume();
                done = handle.done();
            }
            return *this;
        }

        const T& operator*() const { return handle.promise().value; }
        bool operator==(std::default_sentinel_t) const { return done; }
    };

    iterator begin() { return iterator{handle}; }
    std::default_sentinel_t end() { return {}; }
};

/**
 * @brief Specialization for reference types
 */
template <typename T>
struct Generator<T&> {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        T* value = nullptr;

        auto yield_value(T& val) noexcept {
            this->value = std::addressof(val);
            return std::suspend_always{};
        }
        auto initial_suspend() noexcept { return std::suspend_always{}; }
        auto final_suspend() noexcept { return std::suspend_always{}; }
        auto get_return_object() noexcept {
            return Generator{handle_type::from_promise(*this)};
        }
        void unhandled_exception() { std::terminate(); }
        void return_void() {}
    };

    handle_type handle;

    explicit Generator(handle_type h) : handle(h) {}

    ~Generator() {
        if (handle) {
            handle.destroy();
        }
    }

    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    Generator(Generator&& other) noexcept
        : handle(std::exchange(other.handle, {})) {}

    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (handle) {
                handle.destroy();
            }
            handle = std::exchange(other.handle, {});
        }
        return *this;
    }

    struct iterator {
        handle_type handle = nullptr;
        bool done = true;

        iterator() = default;
        explicit iterator(handle_type h) : handle(h) {
            if (handle) {
                handle.resume();
                done = handle.done();
            }
        }

        iterator& operator++() {
            if (handle) {
                handle.resume();
                done = handle.done();
            }
            return *this;
        }

        T& operator*() const { return *handle.promise().value; }
        bool operator==(std::default_sentinel_t) const { return done; }
    };

    iterator begin() { return iterator{handle}; }
    std::default_sentinel_t end() { return {}; }
};

/**
 * @brief Merges two sorted ranges into a single sorted range
 * @tparam R1 First range type
 * @tparam R2 Second range type
 * @param r1 First sorted range
 * @param r2 Second sorted range
 * @return Generator yielding merged sorted elements
 */
template <std::ranges::input_range R1, std::ranges::input_range R2>
    requires std::common_reference_with<std::ranges::range_reference_t<R1>,
                                        std::ranges::range_reference_t<R2>>
[[nodiscard]] auto merge(R1&& r1, R2&& r2)
    -> Generator<std::common_reference_t<std::ranges::range_reference_t<R1>,
                                         std::ranges::range_reference_t<R2>>> {
    auto it1 = std::ranges::begin(r1);
    auto it2 = std::ranges::begin(r2);
    auto end1 = std::ranges::end(r1);
    auto end2 = std::ranges::end(r2);

    while (it1 != end1 && it2 != end2) {
        if (*it1 <= *it2) {
            co_yield *it1;
            ++it1;
        } else {
            co_yield *it2;
            ++it2;
        }
    }

    while (it1 != end1) {
        co_yield *it1;
        ++it1;
    }

    while (it2 != end2) {
        co_yield *it2;
        ++it2;
    }
}

/**
 * @brief Zips multiple ranges together
 * @tparam Rs Range types
 * @param ranges Input ranges
 * @return Generator yielding tuples of corresponding elements
 */
template <std::ranges::input_range... Rs>
[[nodiscard]] auto zip(Rs&&... ranges)
    -> Generator<std::tuple<std::ranges::range_value_t<Rs>...>> {
    auto its = std::tuple{std::ranges::begin(ranges)...};
    auto ends = std::tuple{std::ranges::end(ranges)...};

    while ([&]<size_t... Is>(std::index_sequence<Is...>) {
        return ((std::get<Is>(its) != std::get<Is>(ends)) && ...);
    }(std::index_sequence_for<Rs...>{})) {
        co_yield [&]<size_t... Is>(std::index_sequence<Is...>) {
            return std::tuple{*std::get<Is>(its)...};
        }(std::index_sequence_for<Rs...>{});

        [&]<size_t... Is>(std::index_sequence<Is...>) {
            (++std::get<Is>(its), ...);
        }(std::index_sequence_for<Rs...>{});
    }
}

/**
 * @brief Chunks a range into fixed-size groups
 * @tparam R Range type
 * @param range Input range
 * @param chunk_size Size of each chunk
 * @return Generator yielding vectors of chunk_size elements
 */
template <std::ranges::input_range R>
[[nodiscard]] auto chunk(R&& range, size_t chunk_size)
    -> Generator<std::vector<std::ranges::range_value_t<R>>> {
    if (chunk_size == 0) {
        co_return;
    }

    std::vector<std::ranges::range_value_t<R>> current_chunk;
    current_chunk.reserve(chunk_size);

    for (auto&& elem : range) {
        current_chunk.push_back(std::forward<decltype(elem)>(elem));
        if (current_chunk.size() == chunk_size) {
            co_yield std::move(current_chunk);
            current_chunk.clear();
            current_chunk.reserve(chunk_size);
        }
    }

    if (!current_chunk.empty()) {
        co_yield std::move(current_chunk);
    }
}

/**
 * @brief Filters a range using a predicate
 * @tparam R Range type
 * @tparam Pred Predicate type
 * @param range Input range
 * @param pred Predicate function
 * @return Generator yielding filtered elements
 */
template <std::ranges::input_range R,
          std::invocable<std::ranges::range_reference_t<R>> Pred>
[[nodiscard]] auto filter(R&& range, Pred pred)
    -> Generator<std::ranges::range_reference_t<R>> {
    for (auto&& elem : range) {
        if (pred(elem)) {
            co_yield elem;
        }
    }
}

/**
 * @brief Transforms a range using a function
 * @tparam R Range type
 * @tparam F Function type
 * @param range Input range
 * @param func Transform function
 * @return Generator yielding transformed elements
 */
template <std::ranges::input_range R,
          std::invocable<std::ranges::range_reference_t<R>> F>
[[nodiscard]] auto transform(R&& range, F func)
    -> Generator<std::invoke_result_t<F, std::ranges::range_reference_t<R>>> {
    for (auto&& elem : range) {
        co_yield func(elem);
    }
}

/**
 * @brief Creates pairs of adjacent elements
 * @tparam R Range type
 * @param range Input range
 * @return Generator yielding pairs of adjacent elements
 */
template <std::ranges::forward_range R>
[[nodiscard]] auto adjacent(R&& range) -> Generator<
    std::pair<std::ranges::range_value_t<R>, std::ranges::range_value_t<R>>> {
    auto it = std::ranges::begin(range);
    auto end = std::ranges::end(range);
    if (it == end) {
        co_return;
    }

    auto prev = *it++;
    while (it != end) {
        co_yield {prev, *it};
        prev = *it++;
    }
}

/**
 * @brief Enumerates elements with their indices
 * @tparam R Range type
 * @param range Input range
 * @return Generator yielding pairs of index and element
 */
template <std::ranges::input_range R>
[[nodiscard]] auto enumerate(R&& range)
    -> Generator<std::pair<size_t, std::ranges::range_value_t<R>>> {
    size_t index = 0;
    for (auto&& elem : range) {
        co_yield {index++, std::forward<decltype(elem)>(elem)};
    }
}

/**
 * @brief Flattens a range of ranges
 * @tparam R Range of ranges type
 * @param range Input range of ranges
 * @return Generator yielding flattened elements
 */
template <std::ranges::input_range R>
    requires std::ranges::input_range<std::ranges::range_value_t<R>>
[[nodiscard]] auto flatten(R&& range)
    -> Generator<std::ranges::range_value_t<std::ranges::range_value_t<R>>> {
    for (auto&& inner_range : range) {
        for (auto&& elem : inner_range) {
            co_yield elem;
        }
    }
}

}  // namespace atom::utils

#endif  // ATOM_UTILS_RANGES_HPP
