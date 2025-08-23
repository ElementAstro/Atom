#ifndef ATOM_UTILS_SPAN_HPP
#define ATOM_UTILS_SPAN_HPP

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numeric>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#ifdef ATOM_USE_BOOST
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/iterator_range.hpp>
#endif

namespace atom::utils {

/**
 * @brief Computes the sum of elements in a span
 * @tparam T The type of elements in the span
 * @param data The span containing the elements
 * @return The sum of all elements
 */
template <typename T>
[[nodiscard]] auto sum(std::span<const T> data) noexcept -> T {
#ifdef ATOM_USE_BOOST
    return boost::accumulate(data, T{0});
#else
    return std::accumulate(data.begin(), data.end(), T{0});
#endif
}

/**
 * @brief Checks if a span contains a specific value
 * @tparam T The type of elements in the span
 * @param data The span to search
 * @param value The value to find
 * @return True if the value is found, false otherwise
 */
template <typename T>
[[nodiscard]] auto contains(std::span<const T> data, const T& value) noexcept
    -> bool {
#ifdef ATOM_USE_BOOST
    return boost::contains(data, value);
#else
    return std::find(data.begin(), data.end(), value) != data.end();
#endif
}

/**
 * @brief Sorts the elements in a span in-place
 * @tparam T The type of elements in the span
 * @param data The span to sort
 */
template <typename T>
void sortSpan(std::span<T> data) noexcept {
#ifdef ATOM_USE_BOOST
    boost::sort(data);
#else
    std::sort(data.begin(), data.end());
#endif
}

/**
 * @brief Filters elements in a span based on a predicate
 * @tparam T The type of elements in the span
 * @tparam Predicate The type of the predicate function
 * @param data The span to filter
 * @param predicate The predicate function to apply
 * @return A vector containing the filtered elements
 */
template <typename T, typename Predicate>
[[nodiscard]] auto filterSpan(std::span<const T> data, Predicate predicate)
    -> std::vector<T> {
    std::vector<T> result;
    result.reserve(data.size());  // Optimize memory allocation

#ifdef ATOM_USE_BOOST
    boost::copy_if(data, std::back_inserter(result), predicate);
#else
    std::copy_if(data.begin(), data.end(), std::back_inserter(result),
                 predicate);
#endif
    return result;
}

/**
 * @brief Counts the number of elements in a span that satisfy a predicate
 * @tparam T The type of elements in the span
 * @tparam Predicate The type of the predicate function
 * @param data The span to search
 * @param predicate The predicate function to apply
 * @return The number of elements that satisfy the predicate
 */
template <typename T, typename Predicate>
[[nodiscard]] auto countIfSpan(std::span<const T> data,
                               Predicate predicate) noexcept -> size_t {
#ifdef ATOM_USE_BOOST
    return boost::count_if(data, predicate);
#else
    return std::count_if(data.begin(), data.end(), predicate);
#endif
}

/**
 * @brief Finds the minimum element in a span
 * @tparam T The type of elements in the span
 * @param data The span to search
 * @return The minimum element
 * @throws std::runtime_error if the span is empty
 */
template <typename T>
[[nodiscard]] auto minElementSpan(std::span<const T> data) -> T {
    if (data.empty()) {
        throw std::runtime_error("Cannot find minimum of empty span");
    }

#ifdef ATOM_USE_BOOST
    return *boost::min_element(data);
#else
    return *std::min_element(data.begin(), data.end());
#endif
}

/**
 * @brief Finds the maximum element in a span
 * @tparam T The type of elements in the span
 * @param data The span to search
 * @return The maximum element
 * @throws std::runtime_error if the span is empty
 */
template <typename T>
[[nodiscard]] auto maxElementSpan(std::span<const T> data) -> T {
    if (data.empty()) {
        throw std::runtime_error("Cannot find maximum of empty span");
    }

#ifdef ATOM_USE_BOOST
    return *boost::max_element(data);
#else
    return *std::max_element(data.begin(), data.end());
#endif
}

/**
 * @brief Finds the index of the maximum element in a span
 * @tparam T The type of elements in the span
 * @param data The span to search
 * @return The index of the maximum element
 * @throws std::runtime_error if the span is empty
 */
template <typename T>
[[nodiscard]] auto maxElementIndex(std::span<const T> data) -> size_t {
    if (data.empty()) {
        throw std::runtime_error("Cannot find maximum index of empty span");
    }

#ifdef ATOM_USE_BOOST
    auto it = boost::max_element(data);
#else
    auto it = std::max_element(data.begin(), data.end());
#endif
    return std::distance(data.begin(), it);
}

/**
 * @brief Finds the index of the minimum element in a span
 * @tparam T The type of elements in the span
 * @param data The span to search
 * @return The index of the minimum element
 * @throws std::runtime_error if the span is empty
 */
template <typename T>
[[nodiscard]] auto minElementIndex(std::span<const T> data) -> size_t {
    if (data.empty()) {
        throw std::runtime_error("Cannot find minimum index of empty span");
    }

#ifdef ATOM_USE_BOOST
    auto it = boost::min_element(data);
#else
    auto it = std::min_element(data.begin(), data.end());
#endif
    return std::distance(data.begin(), it);
}

#ifdef __DEBUG__
/**
 * @brief Prints the elements of a span to standard output
 * @tparam T The type of elements in the span
 * @param data The span to print
 */
template <typename T>
void printSpan(std::span<const T> data) {
#ifdef ATOM_USE_BOOST
    boost::copy(data, std::ostream_iterator<T>(std::cout, " "));
#else
    for (const auto& element : data) {
        std::cout << element << " ";
    }
#endif
    std::cout << '\n';
}
#endif  // __DEBUG__

/**
 * @brief Transposes a matrix represented as a span
 * @tparam T The type of elements in the matrix
 * @tparam N The size of the span
 * @param matrix The span representing the matrix to transpose in-place
 * @param rows The number of rows in the matrix
 * @param cols The number of columns in the matrix
 * @throws std::invalid_argument if dimensions don't match span size
 */
template <typename T, size_t N>
void transposeMatrix(std::span<T, N> matrix, size_t rows, size_t cols) {
    if (rows * cols != matrix.size()) {
        throw std::invalid_argument("Matrix dimensions don't match span size");
    }

    std::vector<T> transposed;
    transposed.reserve(matrix.size());

    for (size_t j = 0; j < cols; ++j) {
        for (size_t i = 0; i < rows; ++i) {
            transposed.push_back(matrix[i * cols + j]);
        }
    }

#ifdef ATOM_USE_BOOST
    boost::copy(transposed, matrix.begin());
#else
    std::copy(transposed.begin(), transposed.end(), matrix.begin());
#endif
}

/**
 * @brief Normalizes the elements in a span to the range [0, 1]
 * @tparam T The type of elements in the span
 * @param data The span to normalize in-place
 */
template <typename T>
void normalize(std::span<T> data) noexcept {
    if (data.empty()) {
        return;
    }

    auto [min_it, max_it] = std::minmax_element(data.begin(), data.end());
    T min_val = *min_it;
    T max_val = *max_it;
    T range = max_val - min_val;

    if (range == T{0}) {
        return;  // Avoid division by zero
    }

#ifdef ATOM_USE_BOOST
    boost::for_each(data, [min_val, range](T& element) {
        element = (element - min_val) / range;
    });
#else
    for (auto& element : data) {
        element = (element - min_val) / range;
    }
#endif
}

/**
 * @brief Computes the arithmetic mean of elements in a span
 * @tparam T The type of elements in the span
 * @param data The span containing the elements
 * @return The mean of the elements as a double
 */
template <typename T>
[[nodiscard]] auto mean(std::span<const T> data) noexcept -> double {
    if (data.empty()) {
        return 0.0;
    }
    return static_cast<double>(sum(data)) / static_cast<double>(data.size());
}

/**
 * @brief Finds the median of elements in a span
 * @tparam T The type of elements in the span
 * @param data The span containing the elements
 * @return The median of the elements as a double
 */
template <typename T>
[[nodiscard]] auto median(std::span<const T> data) -> double {
    if (data.empty()) {
        return 0.0;
    }

    std::vector<T> sorted_data(data.begin(), data.end());
#ifdef ATOM_USE_BOOST
    boost::sort(sorted_data);
#else
    std::sort(sorted_data.begin(), sorted_data.end());
#endif

    size_t mid = sorted_data.size() / 2;
    if (sorted_data.size() % 2 == 0) {
        return (static_cast<double>(sorted_data[mid - 1]) +
                static_cast<double>(sorted_data[mid])) /
               2.0;
    }
    return static_cast<double>(sorted_data[mid]);
}

/**
 * @brief Finds the mode (most frequent value) of elements in a span
 * @tparam T The type of elements in the span
 * @param data The span containing the elements
 * @return The mode of the elements
 * @throws std::runtime_error if the span is empty
 */
template <typename T>
[[nodiscard]] auto mode(std::span<const T> data) -> T {
    if (data.empty()) {
        throw std::runtime_error("Cannot find mode of empty span");
    }

    std::unordered_map<T, size_t> frequency;
    frequency.reserve(data.size());  // Optimize hash map allocation

    for (const auto& element : data) {
        ++frequency[element];
    }

#ifdef ATOM_USE_BOOST
    return boost::max_element(
               frequency,
               [](const auto& a, const auto& b) { return a.second < b.second; })
        ->first;
#else
    return std::max_element(
               frequency.begin(), frequency.end(),
               [](const auto& a, const auto& b) { return a.second < b.second; })
        ->first;
#endif
}

/**
 * @brief Computes the standard deviation of elements in a span
 * @tparam T The type of elements in the span
 * @param data The span containing the elements
 * @return The standard deviation of the elements as a double
 */
template <typename T>
[[nodiscard]] auto standardDeviation(std::span<const T> data) noexcept
    -> double {
    if (data.empty()) {
        return 0.0;
    }

    double mean_value = mean(data);
    double sum_of_squares = std::accumulate(
        data.begin(), data.end(), 0.0,
        [mean_value](double acc, const T& value) {
            double diff = static_cast<double>(value) - mean_value;
            return acc + diff * diff;
        });

    return std::sqrt(sum_of_squares / static_cast<double>(data.size()));
}

/**
 * @brief Finds the top N maximum elements in a span
 * @tparam T The type of elements in the span
 * @param data The span containing the elements
 * @param n The number of top elements to find
 * @return A vector containing the top N elements in descending order
 */
template <typename T>
[[nodiscard]] auto topNElements(std::span<const T> data, size_t n)
    -> std::vector<T> {
    std::vector<T> result(data.begin(), data.end());
    size_t actual_n = std::min(n, result.size());

#ifdef ATOM_USE_BOOST
    boost::partial_sort(result, result.begin() + actual_n, result.end(),
                        [](const T& a, const T& b) { return a > b; });
#else
    std::partial_sort(result.begin(), result.begin() + actual_n, result.end(),
                      [](const T& a, const T& b) { return a > b; });
#endif

    result.resize(actual_n);
    return result;
}

/**
 * @brief Computes the variance of elements in a span
 * @tparam T The type of elements in the span
 * @param data The span containing the elements
 * @return The variance of the elements as a double
 */
template <typename T>
[[nodiscard]] auto variance(std::span<const T> data) noexcept -> double {
    if (data.empty()) {
        return 0.0;
    }

    double mean_value = mean(data);
    double sum_of_squares = std::accumulate(
        data.begin(), data.end(), 0.0,
        [mean_value](double acc, const T& value) {
            double diff = static_cast<double>(value) - mean_value;
            return acc + diff * diff;
        });

    return sum_of_squares / static_cast<double>(data.size());
}

/**
 * @brief Finds the bottom N minimum elements in a span
 * @tparam T The type of elements in the span
 * @param data The span containing the elements
 * @param n The number of bottom elements to find
 * @return A vector containing the bottom N elements in ascending order
 */
template <typename T>
[[nodiscard]] auto bottomNElements(std::span<const T> data, size_t n)
    -> std::vector<T> {
    std::vector<T> result(data.begin(), data.end());
    size_t actual_n = std::min(n, result.size());

#ifdef ATOM_USE_BOOST
    boost::partial_sort(result, result.begin() + actual_n, result.end(),
                        [](const T& a, const T& b) { return a < b; });
#else
    std::partial_sort(result.begin(), result.begin() + actual_n, result.end(),
                      [](const T& a, const T& b) { return a < b; });
#endif

    result.resize(actual_n);
    return result;
}

/**
 * @brief Computes the cumulative sum of elements in a span
 * @tparam T The type of elements in the span
 * @param data The span containing the elements
 * @return A vector containing the cumulative sums
 */
template <typename T>
[[nodiscard]] auto cumulativeSum(std::span<const T> data) -> std::vector<T> {
    std::vector<T> result;
    result.reserve(data.size());
    result.resize(data.size());

#ifdef ATOM_USE_BOOST
    boost::partial_sum(data, result.begin());
#else
    std::partial_sum(data.begin(), data.end(), result.begin());
#endif
    return result;
}

/**
 * @brief Computes the cumulative product of elements in a span
 * @tparam T The type of elements in the span
 * @param data The span containing the elements
 * @return A vector containing the cumulative products
 */
template <typename T>
[[nodiscard]] auto cumulativeProduct(std::span<const T> data)
    -> std::vector<T> {
    std::vector<T> result;
    result.reserve(data.size());
    result.resize(data.size());

#ifdef ATOM_USE_BOOST
    boost::partial_sum(data, result.begin(), std::multiplies<T>());
#else
    std::partial_sum(data.begin(), data.end(), result.begin(),
                     std::multiplies<T>());
#endif
    return result;
}

/**
 * @brief Finds the index of a specific value in a span
 * @tparam T The type of elements in the span
 * @param data The span to search
 * @param value The value to find
 * @return The index of the value if found, std::nullopt otherwise
 */
template <typename T>
[[nodiscard]] auto findIndex(std::span<const T> data, const T& value) noexcept
    -> std::optional<size_t> {
#ifdef ATOM_USE_BOOST
    auto it = boost::find(data, value);
#else
    auto it = std::find(data.begin(), data.end(), value);
#endif

    if (it != data.end()) {
        return std::distance(data.begin(), it);
    }
    return std::nullopt;
}

/**
 * @brief Computes the dot product of two spans
 * @tparam T The type of elements in the spans
 * @param lhs The first span
 * @param rhs The second span
 * @return The dot product of the two spans
 * @throws std::invalid_argument if spans have different sizes
 */
template <typename T>
[[nodiscard]] auto dotProduct(std::span<const T> lhs, std::span<const T> rhs)
    -> T {
    if (lhs.size() != rhs.size()) {
        throw std::invalid_argument(
            "Spans must have the same size for dot product");
    }

    return std::inner_product(lhs.begin(), lhs.end(), rhs.begin(), T{0});
}

/**
 * @brief Checks if all elements in a span satisfy a predicate
 * @tparam T The type of elements in the span
 * @tparam Predicate The type of the predicate function
 * @param data The span to check
 * @param predicate The predicate function to apply
 * @return True if all elements satisfy the predicate, false otherwise
 */
template <typename T, typename Predicate>
[[nodiscard]] auto allOf(std::span<const T> data, Predicate predicate) noexcept
    -> bool {
    return std::all_of(data.begin(), data.end(), predicate);
}

/**
 * @brief Checks if any element in a span satisfies a predicate
 * @tparam T The type of elements in the span
 * @tparam Predicate The type of the predicate function
 * @param data The span to check
 * @param predicate The predicate function to apply
 * @return True if any element satisfies the predicate, false otherwise
 */
template <typename T, typename Predicate>
[[nodiscard]] auto anyOf(std::span<const T> data, Predicate predicate) noexcept
    -> bool {
    return std::any_of(data.begin(), data.end(), predicate);
}

/**
 * @brief Checks if no elements in a span satisfy a predicate
 * @tparam T The type of elements in the span
 * @tparam Predicate The type of the predicate function
 * @param data The span to check
 * @param predicate The predicate function to apply
 * @return True if no elements satisfy the predicate, false otherwise
 */
template <typename T, typename Predicate>
[[nodiscard]] auto noneOf(std::span<const T> data, Predicate predicate) noexcept
    -> bool {
    return std::none_of(data.begin(), data.end(), predicate);
}

}  // namespace atom::utils

#endif  // ATOM_UTILS_SPAN_HPP
