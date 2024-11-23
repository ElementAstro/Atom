/*
 * to_string.hpp
 *
 * Copyright (C) 2023-2024 Max Qian
 */

#ifndef ATOM_UTILS_TO_STRING_HPP
#define ATOM_UTILS_TO_STRING_HPP

#include <array>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace atom::utils {

/**
 * @brief Concept for string types.
 *
 * This concept checks if a type is a string, const char*, or char*.
 */
template <typename T>
concept StringType = std::is_same_v<std::decay_t<T>, std::string> ||
                     std::is_same_v<std::decay_t<T>, const char*> ||
                     std::is_same_v<std::decay_t<T>, char*>;

/**
 * @brief Concept for container types.
 *
 * This concept checks if a type has begin and end iterators.
 */
template <typename T>
concept Container = requires(T container) {
    std::begin(container);
    std::end(container);
};

/**
 * @brief Concept for map types.
 *
 * This concept checks if a type has key_type and mapped_type, and has begin and
 * end iterators.
 */
template <typename T>
concept MapType = requires(T map) {
    typename T::key_type;
    typename T::mapped_type;
    std::begin(map);
    std::end(map);
};

/**
 * @brief Concept for pointer types.
 *
 * This concept checks if a type is a pointer but not a string type.
 */
template <typename T>
concept PointerType = std::is_pointer_v<T> && !StringType<T>;

/**
 * @brief Concept for enum types.
 *
 * This concept checks if a type is an enum.
 */
template <typename T>
concept EnumType = std::is_enum_v<T>;

/**
 * @brief Concept for smart pointer types.
 *
 * This concept checks if a type has dereference and get methods.
 */
template <typename T>
concept SmartPointer = requires(T smartPtr) {
    *smartPtr;
    smartPtr.get();
};

/**
 * @brief Converts a string type to std::string.
 *
 * @tparam T The type of the input value.
 * @param value The input value to be converted.
 * @return The converted std::string.
 */
template <StringType T>
auto toString(T&& value) -> std::string {
    if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
        return value;
    } else {
        return std::string(value);
    }
}

/**
 * @brief Converts a char type to std::string.
 *
 * @param value The input char value to be converted.
 * @return The converted std::string.
 */
inline auto toString(char value) -> std::string {
    return std::string(1, value);
}

/**
 * @brief Converts an enum type to std::string.
 *
 * @tparam T The type of the input value.
 * @param value The input enum value to be converted.
 * @return The converted std::string.
 */
template <EnumType T>
auto toString(T value) -> std::string {
    return std::to_string(static_cast<std::underlying_type_t<T>>(value));
}

/**
 * @brief Converts a pointer type to std::string.
 *
 * @tparam T The type of the input pointer.
 * @param ptr The input pointer to be converted.
 * @return The converted std::string.
 */
template <PointerType T>
auto toString(T ptr) -> std::string {
    if (ptr) {
        return "Pointer(" + toString(*ptr) + ")";
    }
    return "nullptr";
}

/**
 * @brief Converts a smart pointer type to std::string.
 *
 * @tparam T The type of the input smart pointer.
 * @param ptr The input smart pointer to be converted.
 * @return The converted std::string.
 */
template <SmartPointer T>
auto toString(const T& ptr) -> std::string {
    if (ptr) {
        return "SmartPointer(" + toString(*ptr) + ")";
    }
    return "nullptr";
}

/**
 * @brief Converts a container type to std::string.
 *
 * @tparam T The type of the input container.
 * @param container The input container to be converted.
 * @param separator The separator to be used between elements.
 * @return The converted std::string.
 */
template <Container T>
auto toString(const T& container,
              const std::string& separator = ", ") -> std::string {
    std::ostringstream oss;
    if constexpr (MapType<T>) {
        oss << "{";
        bool first = true;
        for (const auto& [key, value] : container) {
            if (!first) {
                oss << separator;
            }
            oss << toString(key) << ": " << toString(value);
            first = false;
        }
        oss << "}";
    } else {
        oss << "[";
        auto iter = std::begin(container);
        auto end = std::end(container);
        while (iter != end) {
            oss << toString(*iter);
            ++iter;
            if (iter != end) {
                oss << separator;
            }
        }
        oss << "]";
    }
    return oss.str();
}

/**
 * @brief Converts a general type to std::string.
 *
 * @tparam T The type of the input value.
 * @param value The input value to be converted.
 * @return The converted std::string.
 */
template <typename T>
    requires(!StringType<T> && !Container<T> && !PointerType<T> &&
             !EnumType<T> && !SmartPointer<T>)
auto toString(const T& value) -> std::string {
    if constexpr (requires { std::to_string(value); }) {
        return std::to_string(value);
    } else {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }
}

/**
 * @brief Joins multiple arguments into a single command line string.
 *
 * @tparam Args The types of the input arguments.
 * @param args The input arguments to be joined.
 * @return The joined command line string.
 */
template <typename... Args>
auto joinCommandLine(const Args&... args) -> std::string {
    std::ostringstream oss;
    ((oss << toString(args) << ' '), ...);
    std::string result = oss.str();
    if (!result.empty()) {
        result.pop_back();  // Remove trailing space
    }
    return result;
}

/**
 * @brief Converts an array to std::string.
 *
 * @tparam T The type of the input array.
 * @param array The input array to be converted.
 * @param separator The separator to be used between elements.
 * @return The converted std::string.
 */
template <Container T>
auto toStringArray(const T& array,
                   const std::string& separator = " ") -> std::string {
    std::ostringstream oss;
    bool first = true;
    for (const auto& item : array) {
        if (!first) {
            oss << separator;
        }
        oss << toString(item);
        first = false;
    }
    return oss.str();
}

/**
 * @brief Converts a range to std::string.
 *
 * @tparam Iterator The type of the input iterators.
 * @param begin The beginning iterator of the range.
 * @param end The ending iterator of the range.
 * @param separator The separator to be used between elements.
 * @return The converted std::string.
 */
template <typename Iterator>
auto toStringRange(Iterator begin, Iterator end,
                   const std::string& separator = ", ") -> std::string {
    std::ostringstream oss;
    oss << "[";
    for (auto iter = begin; iter != end; ++iter) {
        oss << toString(*iter);
        if (std::next(iter) != end) {
            oss << separator;
        }
    }
    oss << "]";
    return oss.str();
}

/**
 * @brief Converts a std::array to std::string.
 *
 * @tparam T The type of the elements in the array.
 * @tparam N The size of the array.
 * @param array The input array to be converted.
 * @return The converted std::string.
 */
template <typename T, std::size_t N>
auto toString(const std::array<T, N>& array) -> std::string {
    return toStringRange(array.begin(), array.end());
}

/**
 * @brief Helper function to convert a tuple to std::string.
 *
 * @tparam Tuple The type of the input tuple.
 * @tparam I The indices of the tuple elements.
 * @param tpl The input tuple to be converted.
 * @param separator The separator to be used between elements.
 * @return The converted std::string.
 */
template <typename Tuple, std::size_t... I>
auto tupleToStringImpl(const Tuple& tpl, std::index_sequence<I...>,
                       const std::string& separator) -> std::string {
    std::ostringstream oss;
    oss << "(";
    ((oss << toString(std::get<I>(tpl))
          << (I < sizeof...(I) - 1 ? separator : "")),
     ...);
    oss << ")";
    return oss.str();
}

/**
 * @brief Converts a std::tuple to std::string.
 *
 * @tparam Args The types of the elements in the tuple.
 * @param tpl The input tuple to be converted.
 * @param separator The separator to be used between elements.
 * @return The converted std::string.
 */
template <typename... Args>
auto toString(const std::tuple<Args...>& tpl,
              const std::string& separator = ", ") -> std::string {
    return tupleToStringImpl(tpl, std::index_sequence_for<Args...>(),
                             separator);
}

/**
 * @brief Converts a std::optional to std::string.
 *
 * @tparam T The type of the value in the optional.
 * @param opt The input optional to be converted.
 * @return The converted std::string.
 */
template <typename T>
auto toString(const std::optional<T>& opt) -> std::string {
    if (opt.has_value()) {
        return "Optional(" + toString(*opt) + ")";
    }
    return "nullopt";
}

/**
 * @brief Converts a std::variant to std::string.
 *
 * @tparam Ts The types of the values in the variant.
 * @param var The input variant to be converted.
 * @return The converted std::string.
 */
template <typename... Ts>
auto toString(const std::variant<Ts...>& var) -> std::string {
    return std::visit(
        [](const auto& value) -> std::string { return toString(value); }, var);
}

}  // namespace atom::utils

#endif  // ATOM_UTILS_TO_STRING_HPP
