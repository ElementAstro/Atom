/*
 * to_string.hpp
 *
 * Copyright (C) 2023-2024 Max Qian
 */

#ifndef ATOM_UTILS_TO_STRING_HPP
#define ATOM_UTILS_TO_STRING_HPP

#include <array>
#include <concepts>
#include <exception>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

namespace atom::utils {

/**
 * @brief Concept for string types.
 * @details This concept checks if a type is a string, const char*, char*, or string_view.
 */
template <typename T>
concept StringType = std::is_same_v<std::decay_t<T>, std::string> ||
                     std::is_same_v<std::decay_t<T>, const char*> ||
                     std::is_same_v<std::decay_t<T>, char*> ||
                     std::is_same_v<std::decay_t<T>, std::string_view>;

/**
 * @brief Concept for container types with C++20 syntax.
 * @details This concept checks if a type has begin and end functions compatible with std::ranges.
 */
template <typename T>
concept Container = std::ranges::range<T> && !StringType<T>;

/**
 * @brief Concept for map types with C++20 syntax.
 * @details This concept checks if a type has key_type, mapped_type, and is a range.
 */
template <typename T>
concept MapType = Container<T> && requires {
    typename T::key_type;
    typename T::mapped_type;
};

/**
 * @brief Concept for pointer types.
 * @details This concept checks if a type is a pointer but not a string type.
 */
template <typename T>
concept PointerType = std::is_pointer_v<T> && !StringType<T>;

/**
 * @brief Concept for enum types.
 * @details This concept checks if a type is an enum.
 */
template <typename T>
concept EnumType = std::is_enum_v<T>;

/**
 * @brief Concept for smart pointer types.
 * @details This concept checks if a type has dereference and get methods.
 */
template <typename T>
concept SmartPointer = requires(T smartPtr) {
    *smartPtr;
    { smartPtr.get() } -> std::convertible_to<void*>;
};

/**
 * @brief Concept for types that can be converted to a string using std::to_string.
 */
template <typename T>
concept HasStdToString = requires(T t) {
    { std::to_string(t) } -> std::convertible_to<std::string>;
};

/**
 * @brief Concept for types that can be inserted into an output stream.
 */
template <typename T>
concept Streamable = requires(std::ostream& os, T t) {
    { os << t } -> std::convertible_to<std::ostream&>;
};

/**
 * @brief Exception class for toString conversion errors
 */
class ToStringException : public std::exception {
private:
    std::string message_;

public:
    explicit ToStringException(std::string message)
        : message_(std::format("ToString conversion error: {}", std::move(message))) {}

    [[nodiscard]] const char* what() const noexcept override {
        return message_.c_str();
    }
};

/**
 * @brief Forward declaration for general toString to handle recursive cases
 */
template <typename T>
auto toString(const T& value) -> std::string;

/**
 * @brief Converts a string type to std::string.
 * @tparam T The type of the input value.
 * @param value The input value to be converted.
 * @return The converted std::string.
 * @throws ToStringException if conversion fails
 */
template <StringType T>
constexpr auto toString(T&& value) -> std::string {
    try {
        if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
            return std::forward<T>(value);
        } else if constexpr (std::is_same_v<std::decay_t<T>, std::string_view>) {
            return std::string(value);
        } else {
            if (value == nullptr) {
                return "null";
            }
            return std::string(value);
        }
    } catch (const std::exception& e) {
        throw ToStringException(std::format("String conversion failed: {}", e.what()));
    }
}

/**
 * @brief Converts a char type to std::string.
 * @param value The input char value to be converted.
 * @return The converted std::string.
 */
constexpr auto toString(char value) -> std::string {
    return std::string(1, value);
}

/**
 * @brief Converts an enum type to std::string.
 * @tparam T The type of the input value.
 * @param value The input enum value to be converted.
 * @return The converted std::string.
 * @throws ToStringException if conversion fails
 */
template <EnumType T>
constexpr auto toString(T value) -> std::string {
    try {
        return std::to_string(static_cast<std::underlying_type_t<T>>(value));
    } catch (const std::exception& e) {
        throw ToStringException(std::format("Enum conversion failed: {}", e.what()));
    }
}

/**
 * @brief Converts a pointer type to std::string.
 * @tparam T The type of the input pointer.
 * @param ptr The input pointer to be converted.
 * @return The converted std::string.
 * @throws ToStringException if conversion fails
 */
template <PointerType T>
auto toString(T ptr) -> std::string {
    try {
        if (ptr) {
            return std::format("Pointer({}, {})", static_cast<const void*>(ptr), toString(*ptr));
        }
        return "nullptr";
    } catch (const std::exception& e) {
        return std::format("Pointer({}) [Error: {}]", static_cast<const void*>(ptr), e.what());
    }
}

/**
 * @brief Converts a smart pointer type to std::string.
 * @tparam T The type of the input smart pointer.
 * @param ptr The input smart pointer to be converted.
 * @return The converted std::string.
 */
template <SmartPointer T>
auto toString(const T& ptr) -> std::string {
    try {
        if (ptr) {
            return std::format("SmartPointer({}, {})", 
                             static_cast<const void*>(ptr.get()), toString(*ptr));
        }
        return "nullptr";
    } catch (const std::exception& e) {
        return std::format("SmartPointer({}) [Error: {}]",
                         ptr ? static_cast<const void*>(ptr.get()) : nullptr, e.what());
    }
}

/**
 * @brief Converts a container type to std::string with specified separator.
 * @tparam T The type of the input container.
 * @param container The input container to be converted.
 * @param separator The separator to be used between elements.
 * @return The converted std::string.
 * @throws ToStringException if conversion fails
 */
template <Container T>
auto toString(const T& container, std::string_view separator) -> std::string {
    try {
        std::string result;
        result.reserve(std::ranges::size(container) * 16);

        if constexpr (MapType<T>) {
            result += "{";
            bool first = true;

            for (const auto& [key, value] : container) {
                if (!first) {
                    result += separator;
                }
                first = false;

                try {
                    result += std::format("{}: {}", toString(key), toString(value));
                } catch (const std::exception& e) {
                    result += std::format("[Error: {}]", e.what());
                }
            }
            result += "}";
        } else {
            result += "[";
            bool first = true;

            for (const auto& item : container) {
                if (!first) {
                    result += separator;
                }
                first = false;

                try {
                    result += toString(item);
                } catch (const std::exception& e) {
                    result += std::format("[Error: {}]", e.what());
                }
            }
            result += "]";
        }

        return result;
    } catch (const std::exception& e) {
        throw ToStringException(std::format("Container conversion failed: {}", e.what()));
    }
}

/**
 * @brief Overload for container type with default separator.
 * @tparam T The type of the input container.
 * @param container The input container to be converted.
 * @return The converted std::string.
 * @throws ToStringException if conversion fails
 */
template <Container T>
auto toString(const T& container) -> std::string {
    return toString(container, ", ");
}

/**
 * @brief Converts a general type to std::string.
 * @tparam T The type of the input value.
 * @param value The input value to be converted.
 * @return The converted std::string.
 * @throws ToStringException if conversion fails
 */
template <typename T>
    requires(!StringType<T> && !Container<T> && !PointerType<T> && 
             !EnumType<T> && !SmartPointer<T>)
auto toString(const T& value) -> std::string {
    try {
        if constexpr (HasStdToString<T>) {
            return std::to_string(value);
        } else if constexpr (Streamable<T>) {
            std::ostringstream oss;
            oss << value;
            return oss.str();
        } else {
            static_assert(HasStdToString<T> || Streamable<T>,
                          "Type cannot be converted to string");
            return "";
        }
    } catch (const std::exception& e) {
        throw ToStringException(std::format("General type conversion failed: {}", e.what()));
    }
}

/**
 * @brief Joins multiple arguments into a single command line string.
 * @tparam Args The types of the input arguments.
 * @param args The input arguments to be joined.
 * @return The joined command line string.
 * @throws ToStringException if conversion fails
 */
template <typename... Args>
auto joinCommandLine(const Args&... args) -> std::string {
    if constexpr (sizeof...(Args) == 0) {
        return "";
    }

    try {
        std::string result;
        result.reserve(sizeof...(args) * 32);
        
        bool first = true;
        ((first ? (result += toString(args), first = false) 
                : (result += " " + toString(args))), ...);
        
        return result;
    } catch (const std::exception& e) {
        throw ToStringException(std::format("Command line joining failed: {}", e.what()));
    }
}

/**
 * @brief Converts an array to std::string using C++20 ranges.
 * @tparam T The type of the input array.
 * @param array The input array to be converted.
 * @param separator The separator to be used between elements.
 * @return The converted std::string.
 * @throws ToStringException if conversion fails
 */
template <Container T>
auto toStringArray(const T& array, std::string_view separator = " ") -> std::string {
    try {
        std::string result;
        result.reserve(std::ranges::size(array) * 8);

        bool first = true;
        for (const auto& item : array) {
            if (!first) {
                result += separator;
            }
            first = false;

            try {
                result += toString(item);
            } catch (const std::exception& e) {
                result += std::format("[Error: {}]", e.what());
            }
        }

        return result;
    } catch (const std::exception& e) {
        throw ToStringException(std::format("Array conversion failed: {}", e.what()));
    }
}

/**
 * @brief Converts a range to std::string using C++20 spans.
 * @tparam Iterator The type of the input iterators.
 * @param begin The beginning iterator of the range.
 * @param end The ending iterator of the range.
 * @param separator The separator to be used between elements.
 * @return The converted std::string.
 * @throws ToStringException if conversion fails
 */
template <typename Iterator>
auto toStringRange(Iterator begin, Iterator end, std::string_view separator = ", ") -> std::string {
    try {
        std::string result = "[";
        result.reserve(std::distance(begin, end) * 16 + 2);

        bool first = true;
        for (auto iter = begin; iter != end; ++iter) {
            if (!first) {
                result += separator;
            }
            first = false;

            try {
                result += toString(*iter);
            } catch (const std::exception& e) {
                result += std::format("[Error: {}]", e.what());
            }
        }

        result += "]";
        return result;
    } catch (const std::exception& e) {
        throw ToStringException(std::format("Range conversion failed: {}", e.what()));
    }
}

/**
 * @brief Converts a std::array to std::string.
 * @tparam T The type of the elements in the array.
 * @tparam N The size of the array.
 * @param array The input array to be converted.
 * @return The converted std::string.
 * @throws ToStringException if conversion fails
 */
template <typename T, std::size_t N>
auto toString(const std::array<T, N>& array) -> std::string {
    try {
        std::span<const T, N> arraySpan{array};
        return toStringRange(arraySpan.begin(), arraySpan.end());
    } catch (const std::exception& e) {
        throw ToStringException(std::format("std::array conversion failed: {}", e.what()));
    }
}

/**
 * @brief Helper function to convert a tuple to std::string.
 * @tparam Tuple The type of the input tuple.
 * @tparam I The indices of the tuple elements.
 * @param tpl The input tuple to be converted.
 * @param separator The separator to be used between elements.
 * @return The converted std::string.
 * @throws ToStringException if conversion fails
 */
template <typename Tuple, std::size_t... I>
auto tupleToStringImpl(const Tuple& tpl, std::index_sequence<I...>, 
                      std::string_view separator) -> std::string {
    try {
        std::string result = "(";
        result.reserve(sizeof...(I) * 16 + 2);

        bool first = true;
        (((!first ? result += separator : result),
          first = false,
          [&result, &tpl]() {
              try {
                  result += toString(std::get<I>(tpl));
              } catch (const std::exception& e) {
                  result += std::format("[Error: {}]", e.what());
              }
          }()), ...);

        result += ")";
        return result;
    } catch (const std::exception& e) {
        throw ToStringException(std::format("Tuple conversion failed: {}", e.what()));
    }
}

/**
 * @brief Converts a std::tuple to std::string.
 * @tparam Args The types of the elements in the tuple.
 * @param tpl The input tuple to be converted.
 * @param separator The separator to be used between elements.
 * @return The converted std::string.
 * @throws ToStringException if conversion fails
 */
template <typename... Args>
auto toString(const std::tuple<Args...>& tpl, std::string_view separator = ", ") -> std::string {
    return tupleToStringImpl(tpl, std::index_sequence_for<Args...>(), separator);
}

/**
 * @brief Converts a std::optional to std::string.
 * @tparam T The type of the value in the optional.
 * @param opt The input optional to be converted.
 * @return The converted std::string.
 * @throws ToStringException if conversion fails
 */
template <typename T>
auto toString(const std::optional<T>& opt) -> std::string {
    try {
        if (opt.has_value()) {
            try {
                return std::format("Optional({})", toString(*opt));
            } catch (const std::exception& e) {
                return std::format("Optional([Error: {}])", e.what());
            }
        }
        return "nullopt";
    } catch (const std::exception& e) {
        throw ToStringException(std::format("Optional conversion failed: {}", e.what()));
    }
}

/**
 * @brief Converts a std::variant to std::string.
 * @tparam Ts The types of the values in the variant.
 * @param var The input variant to be converted.
 * @return The converted std::string.
 * @throws ToStringException if conversion fails
 */
template <typename... Ts>
auto toString(const std::variant<Ts...>& var) -> std::string {
    try {
        return std::visit([](const auto& value) -> std::string {
            try {
                return toString(value);
            } catch (const std::exception& e) {
                return std::format("Variant(error: {})", e.what());
            }
        }, var);
    } catch (const std::bad_variant_access& e) {
        return std::format("Variant(bad_access: {})", e.what());
    } catch (const std::exception& e) {
        throw ToStringException(std::format("Variant conversion failed: {}", e.what()));
    }
}

}  // namespace atom::utils

#endif  // ATOM_UTILS_TO_STRING_HPP
