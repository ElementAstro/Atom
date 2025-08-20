#ifndef ATOM_UTILS_LINQ_HPP
#define ATOM_UTILS_LINQ_HPP

#include <algorithm>
#include <list>
#include <numeric>
#include <optional>
#include <ranges>

// Import high-performance containers
#include "atom/containers/high_performance.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/container/flat_set.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/irange.hpp>
#endif

namespace atom::utils {

/**
 * @brief Flattens a nested vector structure into a single-level vector
 * @tparam T Element type
 * @param nested_collection Nested vector to flatten
 * @return Flattened vector containing all elements
 */
template <typename T>
auto flatten(const atom::containers::Vector<atom::containers::Vector<T>>&
                 nested_collection) noexcept {
    atom::containers::Vector<T> result;
    size_t total_size = 0;

    // Calculate total size for optimal memory allocation
    for (const auto& sub_collection : nested_collection) {
        total_size += sub_collection.size();
    }
    result.reserve(total_size);

    // Flatten the structure
    for (const auto& sub_collection : nested_collection) {
        result.insert(result.end(), sub_collection.begin(),
                      sub_collection.end());
    }
    return result;
}

/**
 * @brief LINQ-style enumerable class providing functional programming
 * operations on collections
 * @tparam T Element type of the collection
 */
template <typename T>
class Enumerable {
public:
    /**
     * @brief Constructs an Enumerable from a vector
     * @param elements Vector of elements to wrap
     */
    explicit Enumerable(atom::containers::Vector<T> elements) noexcept
        : data_(std::move(elements)) {}

    // ======== Filters and Reorders ========

    /**
     * @brief Filters elements based on a predicate
     * @param predicate Function that returns true for elements to keep
     * @return New Enumerable with filtered elements
     */
    [[nodiscard]] auto where(auto predicate) const -> Enumerable<T> {
        atom::containers::Vector<T> result;
        result.reserve(data_.size());  // Optimize memory allocation

#ifdef ATOM_USE_BOOST
        boost::copy_if(data_, std::back_inserter(result), predicate);
#else
        for (const T& element : data_ | std::views::filter(predicate)) {
            result.push_back(element);
        }
#endif
        return Enumerable(std::move(result));
    }

    /**
     * @brief Reduces the collection to a single value using binary operation
     * @tparam U Result type
     * @param initial_value Initial value for the reduction
     * @param binary_operation Binary operation to apply
     * @return Reduced value
     */
    template <typename U, typename BinaryOperation>
    [[nodiscard]] auto reduce(U initial_value,
                              BinaryOperation binary_operation) const noexcept
        -> U {
        return std::accumulate(data_.begin(), data_.end(),
                               std::move(initial_value), binary_operation);
    }

    /**
     * @brief Filters elements with access to their index
     * @param predicate Function that takes element and index, returns true to
     * keep
     * @return New Enumerable with filtered elements
     */
    [[nodiscard]] auto whereIndexed(auto predicate) const -> Enumerable<T> {
        atom::containers::Vector<T> result;
        result.reserve(data_.size());

        for (size_t index = 0; index < data_.size(); ++index) {
            if (predicate(data_[index], index)) {
                result.push_back(data_[index]);
            }
        }
        return Enumerable(std::move(result));
    }

    /**
     * @brief Takes the first count elements
     * @param element_count Number of elements to take
     * @return New Enumerable with first count elements
     */
    [[nodiscard]] auto take(size_t element_count) const -> Enumerable<T> {
        const size_t actual_count = std::min(element_count, data_.size());
        return Enumerable(atom::containers::Vector<T>(
            data_.begin(), data_.begin() + actual_count));
    }

    /**
     * @brief Takes elements while predicate is true
     * @param predicate Function to test each element
     * @return New Enumerable with elements taken while predicate is true
     */
    [[nodiscard]] auto takeWhile(auto predicate) const -> Enumerable<T> {
        atom::containers::Vector<T> result;

        for (const auto& element : data_) {
            if (!predicate(element)) {
                break;
            }
            result.push_back(element);
        }
        return Enumerable(std::move(result));
    }

    /**
     * @brief Takes elements while indexed predicate is true
     * @param predicate Function that takes element and index
     * @return New Enumerable with elements taken while predicate is true
     */
    [[nodiscard]] auto takeWhileIndexed(auto predicate) const -> Enumerable<T> {
        atom::containers::Vector<T> result;

        for (size_t index = 0; index < data_.size(); ++index) {
            if (!predicate(data_[index], index)) {
                break;
            }
            result.push_back(data_[index]);
        }
        return Enumerable(std::move(result));
    }

    /**
     * @brief Skips the first count elements
     * @param element_count Number of elements to skip
     * @return New Enumerable without first count elements
     */
    [[nodiscard]] auto skip(size_t element_count) const -> Enumerable<T> {
        const size_t skip_count = std::min(element_count, data_.size());
        return Enumerable(atom::containers::Vector<T>(
            data_.begin() + skip_count, data_.end()));
    }

    /**
     * @brief Skips elements while predicate is true
     * @param predicate Function to test each element
     * @return New Enumerable without skipped elements
     */
    [[nodiscard]] auto skipWhile(auto predicate) const -> Enumerable<T> {
        auto iterator = std::find_if_not(data_.begin(), data_.end(), predicate);
        return Enumerable(atom::containers::Vector<T>(iterator, data_.end()));
    }

    /**
     * @brief Skips elements while indexed predicate is true
     * @param predicate Function that takes element and index
     * @return New Enumerable without skipped elements
     */
    [[nodiscard]] auto skipWhileIndexed(auto predicate) const -> Enumerable<T> {
        auto iterator =
            std::find_if_not(data_.begin(), data_.end(),
                             [index = 0, &predicate](const T& element) mutable {
                                 return predicate(element, index++);
                             });
        return Enumerable(atom::containers::Vector<T>(iterator, data_.end()));
    }

    /**
     * @brief Sorts elements in ascending order
     * @return New Enumerable with sorted elements
     */
    [[nodiscard]] auto orderBy() const -> Enumerable<T> {
        atom::containers::Vector<T> result = data_;
        std::sort(result.begin(), result.end());
        return Enumerable(std::move(result));
    }

    /**
     * @brief Sorts elements using a key selector
     * @param key_selector Function to extract comparison key
     * @return New Enumerable with sorted elements
     */
    [[nodiscard]] auto orderBy(auto key_selector) const -> Enumerable<T> {
        atom::containers::Vector<T> result = data_;
        std::sort(result.begin(), result.end(),
                  [&key_selector](const T& left, const T& right) {
                      return key_selector(left) < key_selector(right);
                  });
        return Enumerable(std::move(result));
    }

    /**
     * @brief Returns distinct elements
     * @return New Enumerable with unique elements
     */
    [[nodiscard]] auto distinct() const -> Enumerable<T> {
        atom::containers::Vector<T> result;
        atom::containers::HashSet<T> unique_set(data_.begin(), data_.end());
        result.assign(unique_set.begin(), unique_set.end());
        return Enumerable(std::move(result));
    }

    /**
     * @brief Returns distinct elements based on key selector
     * @param key_selector Function to extract comparison key
     * @return New Enumerable with unique elements
     */
    [[nodiscard]] auto distinct(auto key_selector) const -> Enumerable<T> {
        atom::containers::Vector<T> result;
        atom::containers::HashSet<
            std::invoke_result_t<decltype(key_selector), T>>
            seen_keys;

        for (const auto& element : data_) {
            auto key = key_selector(element);
            if (seen_keys.insert(key).second) {
                result.push_back(element);
            }
        }
        return Enumerable(std::move(result));
    }

    /**
     * @brief Appends elements to the end
     * @param items Elements to append
     * @return New Enumerable with appended elements
     */
    [[nodiscard]] auto append(const atom::containers::Vector<T>& items) const
        -> Enumerable<T> {
        atom::containers::Vector<T> result = data_;
        result.insert(result.end(), items.begin(), items.end());
        return Enumerable(std::move(result));
    }

    /**
     * @brief Prepends elements to the beginning
     * @param items Elements to prepend
     * @return New Enumerable with prepended elements
     */
    [[nodiscard]] auto prepend(const atom::containers::Vector<T>& items) const
        -> Enumerable<T> {
        atom::containers::Vector<T> result = items;
        result.insert(result.end(), data_.begin(), data_.end());
        return Enumerable(std::move(result));
    }

    /**
     * @brief Concatenates with another enumerable
     * @param other Enumerable to concatenate with
     * @return New Enumerable with concatenated elements
     */
    [[nodiscard]] auto concat(const Enumerable<T>& other) const
        -> Enumerable<T> {
        return append(other.data_);
    }

    /**
     * @brief Reverses the order of elements
     * @return New Enumerable with reversed elements
     */
    [[nodiscard]] auto reverse() const -> Enumerable<T> {
        atom::containers::Vector<T> result = data_;
        std::reverse(result.begin(), result.end());
        return Enumerable(std::move(result));
    }

    /**
     * @brief Casts elements to a different type
     * @tparam U Target type
     * @return New Enumerable with cast elements
     */
    template <typename U>
    [[nodiscard]] auto cast() const -> Enumerable<U> {
        atom::containers::Vector<U> result;
        result.reserve(data_.size());

        for (const T& element : data_) {
            result.push_back(static_cast<U>(element));
        }
        return Enumerable<U>(std::move(result));
    }

    // ======== Transformers ========

    /**
     * @brief Projects each element to a new form
     * @tparam U Result element type
     * @param transformer Function to transform each element
     * @return New Enumerable with transformed elements
     */
    template <typename U>
    [[nodiscard]] auto select(auto transformer) const -> Enumerable<U> {
        atom::containers::Vector<U> result;
        result.reserve(data_.size());

        for (const auto& element : data_) {
            result.push_back(transformer(element));
        }
        return Enumerable<U>(std::move(result));
    }

    /**
     * @brief Projects each element with its index to a new form
     * @tparam U Result element type
     * @param transformer Function that takes element and index
     * @return New Enumerable with transformed elements
     */
    template <typename U>
    [[nodiscard]] auto selectIndexed(auto transformer) const -> Enumerable<U> {
        atom::containers::Vector<U> result;
        result.reserve(data_.size());

        for (size_t index = 0; index < data_.size(); ++index) {
            result.push_back(transformer(data_[index], index));
        }
        return Enumerable<U>(std::move(result));
    }

    /**
     * @brief Groups elements by a key selector
     * @tparam U Key type
     * @param key_selector Function to extract grouping key
     * @return New Enumerable with group keys
     */
    template <typename U>
    [[nodiscard]] auto groupBy(auto key_selector) const -> Enumerable<U> {
        atom::containers::HashMap<U, atom::containers::Vector<T>> groups;

        for (const T& element : data_) {
            groups[key_selector(element)].push_back(element);
        }

        atom::containers::Vector<U> keys;
        keys.reserve(groups.size());
        for (const auto& group : groups) {
            keys.push_back(group.first);
        }
        return Enumerable<U>(std::move(keys));
    }

    /**
     * @brief Projects each element to a sequence and flattens the result
     * @tparam U Result element type
     * @param transformer Function that returns a sequence for each element
     * @return New Enumerable with flattened results
     */
    template <typename U>
    [[nodiscard]] auto selectMany(auto transformer) const -> Enumerable<U> {
        atom::containers::Vector<atom::containers::Vector<U>> nested_results;
        nested_results.reserve(data_.size());

        for (const T& element : data_) {
            nested_results.push_back(transformer(element));
        }
        return Enumerable<U>(flatten(nested_results));
    }

    // ======== Aggregators ========

    /**
     * @brief Tests if all elements satisfy a condition
     * @param predicate Function to test each element
     * @return True if all elements satisfy the condition
     */
    [[nodiscard]] auto all(auto predicate = [](const T&) {
        return true;
    }) const noexcept -> bool {
        return std::all_of(data_.begin(), data_.end(), predicate);
    }

    /**
     * @brief Tests if any element satisfies a condition
     * @param predicate Function to test each element
     * @return True if any element satisfies the condition
     */
    [[nodiscard]] auto any(auto predicate = [](const T&) {
        return true;
    }) const noexcept -> bool {
        return std::any_of(data_.begin(), data_.end(), predicate);
    }

    /**
     * @brief Computes the sum of all elements
     * @return Sum of all elements
     */
    [[nodiscard]] auto sum() const noexcept -> T {
        return std::accumulate(data_.begin(), data_.end(), T{});
    }

    /**
     * @brief Computes the sum of transformed elements
     * @tparam U Result type
     * @param transformer Function to transform each element
     * @return Sum of transformed elements
     */
    template <typename U>
    [[nodiscard]] auto sum(auto transformer) const -> U {
        U result{};
        for (const auto& element : data_) {
            result += transformer(element);
        }
        return result;
    }

    /**
     * @brief Computes the average of all elements
     * @return Average as double
     */
    [[nodiscard]] auto average() const noexcept -> double {
        if (data_.empty())
            return 0.0;

        if constexpr (std::is_arithmetic_v<T>) {
            return static_cast<double>(sum()) / data_.size();
        } else {
            return static_cast<double>(data_.size());
        }
    }

    /**
     * @brief Computes the average of transformed elements
     * @tparam U Result type
     * @param transformer Function to transform each element
     * @return Average of transformed elements
     */
    template <typename U>
    [[nodiscard]] auto average(auto transformer) const -> U {
        return data_.empty()
                   ? U{}
                   : sum<U>(transformer) / static_cast<U>(data_.size());
    }

    /**
     * @brief Finds the minimum element
     * @return Minimum element, or default if empty
     */
    [[nodiscard]] auto min() const -> T {
        if (data_.empty())
            return T{};
        return *std::min_element(data_.begin(), data_.end());
    }

    /**
     * @brief Finds the minimum element using a key selector
     * @param key_selector Function to extract comparison key
     * @return Element with minimum key
     */
    [[nodiscard]] auto min(auto key_selector) const -> T {
        if (data_.empty())
            return T{};
        return *std::min_element(
            data_.begin(), data_.end(),
            [&key_selector](const T& left, const T& right) {
                return key_selector(left) < key_selector(right);
            });
    }

    /**
     * @brief Finds the maximum element
     * @return Maximum element, or default if empty
     */
    [[nodiscard]] auto max() const -> T {
        if (data_.empty())
            return T{};
        return *std::max_element(data_.begin(), data_.end());
    }

    /**
     * @brief Finds the maximum element using a key selector
     * @param key_selector Function to extract comparison key
     * @return Element with maximum key
     */
    [[nodiscard]] auto max(auto key_selector) const -> T {
        if (data_.empty())
            return T{};
        return *std::max_element(
            data_.begin(), data_.end(),
            [&key_selector](const T& left, const T& right) {
                return key_selector(left) < key_selector(right);
            });
    }

    /**
     * @brief Gets the number of elements
     * @return Number of elements
     */
    [[nodiscard]] auto count() const noexcept -> size_t { return data_.size(); }

    /**
     * @brief Counts elements that satisfy a condition
     * @param predicate Function to test each element
     * @return Number of elements satisfying the condition
     */
    [[nodiscard]] auto count(auto predicate) const -> size_t {
        return std::count_if(data_.begin(), data_.end(), predicate);
    }

    /**
     * @brief Tests if the collection contains a specific value
     * @param value Value to search for
     * @return True if value is found
     */
    [[nodiscard]] auto contains(const T& value) const noexcept -> bool {
        return std::find(data_.begin(), data_.end(), value) != data_.end();
    }

    /**
     * @brief Gets the element at a specific index
     * @param index Zero-based index
     * @return Element at the specified index
     * @throws std::out_of_range if index is invalid
     */
    [[nodiscard]] auto elementAt(size_t index) const -> T {
        return data_.at(index);
    }

    /**
     * @brief Gets the first element
     * @return First element, or default if empty
     */
    [[nodiscard]] auto first() const noexcept -> T {
        return data_.empty() ? T{} : data_.front();
    }

    /**
     * @brief Gets the first element that satisfies a condition
     * @param predicate Function to test each element
     * @return First matching element, or default if none found
     */
    [[nodiscard]] auto first(auto predicate) const -> T {
        auto it = std::find_if(data_.begin(), data_.end(), predicate);
        return it != data_.end() ? *it : T{};
    }

    /**
     * @brief Gets the first element or null if empty
     * @return Optional containing first element
     */
    [[nodiscard]] auto firstOrDefault() const noexcept -> std::optional<T> {
        return data_.empty() ? std::nullopt : std::optional<T>(data_.front());
    }

    /**
     * @brief Gets the first element satisfying a condition or null
     * @param predicate Function to test each element
     * @return Optional containing first matching element
     */
    [[nodiscard]] auto firstOrDefault(auto predicate) const
        -> std::optional<T> {
        auto it = std::find_if(data_.begin(), data_.end(), predicate);
        return it != data_.end() ? std::optional<T>(*it) : std::nullopt;
    }

    /**
     * @brief Gets the last element
     * @return Last element, or default if empty
     */
    [[nodiscard]] auto last() const noexcept -> T {
        return data_.empty() ? T{} : data_.back();
    }

    /**
     * @brief Gets the last element that satisfies a condition
     * @param predicate Function to test each element
     * @return Last matching element, or default if none found
     */
    [[nodiscard]] auto last(auto predicate) const -> T {
        auto it = std::find_if(data_.rbegin(), data_.rend(), predicate);
        return it != data_.rend() ? *it : T{};
    }

    /**
     * @brief Gets the last element or null if empty
     * @return Optional containing last element
     */
    [[nodiscard]] auto lastOrDefault() const noexcept -> std::optional<T> {
        return data_.empty() ? std::nullopt : std::optional<T>(data_.back());
    }

    /**
     * @brief Gets the last element satisfying a condition or null
     * @param predicate Function to test each element
     * @return Optional containing last matching element
     */
    [[nodiscard]] auto lastOrDefault(auto predicate) const -> std::optional<T> {
        auto it = std::find_if(data_.rbegin(), data_.rend(), predicate);
        return it != data_.rend() ? std::optional<T>(*it) : std::nullopt;
    }

    // ======== Conversion Methods ========

    /**
     * @brief Converts to hash set
     * @return Hash set containing all elements
     */
    [[nodiscard]] auto toSet() const -> atom::containers::HashSet<T> {
        return atom::containers::HashSet<T>(data_.begin(), data_.end());
    }

    /**
     * @brief Converts to vector
     * @return Vector containing all elements
     */
    [[nodiscard]] auto toVector() const -> atom::containers::Vector<T> {
        return data_;
    }

    /**
     * @brief Converts to standard vector
     * @return Standard vector containing all elements
     */
    [[nodiscard]] auto toStdVector() const -> std::vector<T> {
        return std::vector<T>(data_.begin(), data_.end());
    }

    /**
     * @brief Converts to standard list
     * @return Standard list containing all elements
     */
    [[nodiscard]] auto toStdList() const -> std::list<T> {
        return std::list<T>(data_.begin(), data_.end());
    }

    /**
     * @brief Converts to standard deque
     * @return Standard deque containing all elements
     */
    [[nodiscard]] auto toStdDeque() const -> std::deque<T> {
        return std::deque<T>(data_.begin(), data_.end());
    }

    /**
     * @brief Converts to standard set
     * @return Standard set containing all unique elements
     */
    [[nodiscard]] auto toStdSet() const -> std::set<T> {
        return std::set<T>(data_.begin(), data_.end());
    }

private:
    atom::containers::Vector<T> data_;
};

/**
 * @brief Creates an Enumerable from any container
 * @tparam Container Container type
 * @param container Source container
 * @return Enumerable wrapping the container elements
 */
template <typename Container>
auto from(const Container& container) {
    atom::containers::Vector<typename Container::value_type> data(
        container.begin(), container.end());
    return Enumerable<typename Container::value_type>(std::move(data));
}

/**
 * @brief Creates an Enumerable from initializer list
 * @tparam T Element type
 * @param items Initializer list of items
 * @return Enumerable containing the items
 */
template <typename T>
auto from(std::initializer_list<T> items) {
    atom::containers::Vector<T> data(items.begin(), items.end());
    return Enumerable<T>(std::move(data));
}

/**
 * @brief Generates a range of values
 * @tparam T Value type
 * @param start Starting value (inclusive)
 * @param end Ending value (exclusive)
 * @param step Step size (default: 1)
 * @return Enumerable containing the range
 */
template <typename T>
auto range(T start, T end, T step = 1) {
    atom::containers::Vector<T> result;
    if (step > 0) {
        result.reserve(static_cast<size_t>((end - start + step - 1) / step));
        for (T i = start; i < end; i += step) {
            result.push_back(i);
        }
    }
    return Enumerable<T>(std::move(result));
}

}  // namespace atom::utils

#endif
