#ifndef ATOM_META_FIELD_COUNT_HPP
#define ATOM_META_FIELD_COUNT_HPP

#include <array>
#include <type_traits>

namespace atom::meta::details {

/**
 * \brief Universal type that can convert to any other type for field counting
 */
struct Any {
    constexpr Any(int) {}

    template <typename T>
        requires std::is_copy_constructible_v<T>
    constexpr operator T&() const;

    template <typename T>
        requires std::is_move_constructible_v<T>
    constexpr operator T&&() const;

    struct Empty {};

    template <typename T>
        requires(!std::is_copy_constructible_v<T> &&
                 !std::is_move_constructible_v<T> &&
                 !std::is_constructible_v<T, Empty>)
    constexpr operator T() const;
};

/**
 * \brief Check if a type can be initialized with N Any objects
 * \tparam T Type to check
 * \tparam N Number of Any objects
 * \return true if T can be initialized with N Any objects
 */
template <typename T, std::size_t N>
consteval auto canInitializeWithN() -> bool {
    return []<std::size_t... Is>(std::index_sequence<Is...>) {
        return requires { T{Any(Is)...}; };
    }(std::make_index_sequence<N>{});
}

/**
 * \brief Binary search to find the maximum number of fields
 * \tparam T Type to analyze
 * \tparam Low Lower bound
 * \tparam High Upper bound
 * \return Maximum number of fields that can initialize T
 */
template <typename T, std::size_t Low = 0, std::size_t High = 64>
consteval auto binarySearchFieldCount() -> std::size_t {
    if constexpr (Low == High) {
        return Low;
    } else {
        constexpr std::size_t Mid = Low + (High - Low + 1) / 2;
        if constexpr (canInitializeWithN<T, Mid>()) {
            return binarySearchFieldCount<T, Mid, High>();
        } else {
            return binarySearchFieldCount<T, Low, Mid - 1>();
        }
    }
}

/**
 * \brief Get the total count of aggregate initialization parameters
 * \tparam T Type to analyze
 * \return Total number of initialization parameters
 */
template <typename T>
consteval auto totalFieldCount() -> std::size_t {
    return binarySearchFieldCount<T>();
}

/**
 * \brief Check if type can be initialized with three parts
 * \tparam T Type to check
 * \tparam N1 Number of elements before aggregate
 * \tparam N2 Number of elements in aggregate
 * \tparam N3 Number of elements after aggregate
 * \return true if initialization is possible
 */
template <typename T, std::size_t N1, std::size_t N2, std::size_t N3>
consteval auto canInitializeWithThreeParts() -> bool {
    return []<std::size_t... I1, std::size_t... I2, std::size_t... I3>(
               std::index_sequence<I1...>, std::index_sequence<I2...>,
               std::index_sequence<I3...>) {
        return requires { T{Any(I1)..., {Any(I2)...}, Any(I3)...}; };
    }(std::make_index_sequence<N1>{}, std::make_index_sequence<N2>{},
           std::make_index_sequence<N3>{});
}

/**
 * \brief Check if N elements can be placed at a specific position
 * \tparam T Type to check
 * \tparam Position Position to place elements
 * \tparam N Number of elements to place
 * \return true if placement is possible
 */
template <typename T, std::size_t Position, std::size_t N>
consteval auto canPlaceNAtPosition() -> bool {
    constexpr auto Total = totalFieldCount<T>();
    if constexpr (N == 0) {
        return true;
    } else if constexpr (Position + N <= Total) {
        return canInitializeWithThreeParts<T, Position, N,
                                           Total - Position - N>();
    } else {
        return false;
    }
}

/**
 * \brief Check if position has aggregate elements (more than 1)
 * \tparam T Type to check
 * \tparam Position Position to check
 * \tparam N Current test size
 * \tparam MaxSize Maximum size to test
 * \return true if position has aggregate elements
 */
template <typename T, std::size_t Position, std::size_t N = 0,
          std::size_t MaxSize = 10>
consteval auto hasAggregateAtPosition() -> bool {
    constexpr auto Total = totalFieldCount<T>();
    if constexpr (canInitializeWithThreeParts<T, Position, N,
                                              Total - Position - 1>()) {
        return false;
    } else if constexpr (N + 1 <= MaxSize) {
        return hasAggregateAtPosition<T, Position, N + 1, MaxSize>();
    } else {
        return true;
    }
}

/**
 * \brief Find maximum size that can be placed at position
 * \tparam T Type to analyze
 * \tparam Position Position to search
 * \return Maximum size that fits at position
 */
template <typename T, std::size_t Position>
consteval auto maxSizeAtPosition() -> std::size_t {
    constexpr auto Total = totalFieldCount<T>();
    if constexpr (!hasAggregateAtPosition<T, Position>()) {
        return 1;
    } else {
        std::size_t result = 0;
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ((canPlaceNAtPosition<T, Position, Is>() ? result = Is : 0), ...);
        }(std::make_index_sequence<Total + 1>());
        return result;
    }
}

/**
 * \brief Recursively populate array with aggregate sizes
 * \tparam T Type to analyze
 * \tparam N Current position
 * \param array Array to populate
 */
template <typename T, std::size_t N = 0>
consteval auto populateAggregateSizes(auto&& array) -> void {
    constexpr auto total = totalFieldCount<T>();
    constexpr auto size = std::max<std::size_t>(maxSizeAtPosition<T, N>(), 1);
    array[N] = size;
    if constexpr (N + size < total) {
        populateAggregateSizes<T, N + size>(array);
    }
}

/**
 * \brief Calculate the true field count by accounting for aggregates
 * \tparam T Type to analyze
 * \return True number of fields
 */
template <typename T>
consteval auto trueFieldCount() -> std::size_t {
    constexpr auto maxFields = totalFieldCount<T>();
    if constexpr (maxFields == 0) {
        return 0;
    } else {
        std::array<std::size_t, maxFields> aggregateSizes{};
        std::fill(aggregateSizes.begin(), aggregateSizes.end(), 1);

        populateAggregateSizes<T>(aggregateSizes);

        std::size_t fieldCount = maxFields;
        std::size_t index = 0;

        while (index < maxFields) {
            auto aggregateSize = aggregateSizes[index];
            fieldCount -= (aggregateSize - 1);
            index += aggregateSize;
        }

        return fieldCount;
    }
}

}  // namespace atom::meta::details

namespace atom::meta {

/**
 * \brief Type information specialization point
 * \tparam T Type to get information for
 */
template <typename T>
struct type_info;

/**
 * \brief Concept for aggregate types
 * \tparam T Type to check
 */
template <typename T>
concept AggregateType = std::is_aggregate_v<T>;

/**
 * \brief Retrieve the count of fields of an aggregate struct
 *
 * This function uses compile-time reflection techniques to determine
 * the number of fields in an aggregate type. It handles nested aggregates
 * and provides an accurate count of actual fields.
 *
 * \warning Cannot get the count of fields of a struct which has reference
 * type members in GCC 13 due to internal compiler errors when using:
 * \code
 * struct Number { operator int&(); };
 * int& x = { Number{} };
 * \endcode
 *
 * \tparam T Aggregate type to analyze
 * \return Number of fields in the aggregate type
 */
template <AggregateType T>
consteval auto fieldCountOf() -> std::size_t {
    if constexpr (requires { type_info<T>::count; }) {
        return type_info<T>::count;
    } else {
        return details::trueFieldCount<T>();
    }
}

/**
 * \brief Retrieve the count of fields for non-aggregate types
 * \tparam T Non-aggregate type
 * \return Always returns 0 for non-aggregate types
 */
template <typename T>
    requires(!AggregateType<T>)
consteval auto fieldCountOf() -> std::size_t {
    return 0;
}

}  // namespace atom::meta

#endif  // ATOM_META_FIELD_COUNT_HPP
