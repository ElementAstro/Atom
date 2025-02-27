#ifndef ATOM_EXTRA_INICPP_CONVERT_HPP
#define ATOM_EXTRA_INICPP_CONVERT_HPP

#include <cmath>
#include <iomanip>
#include <stdexcept>
#include <string>
#include "common.hpp"

namespace inicpp {

/**
 * @brief Template structure for converting between types and strings.
 * @tparam T The type to convert.
 */
template <typename T>
struct Convert {};

/**
 * @brief Specialization of Convert for bool type.
 */
template <>
struct Convert<bool> {
    /**
     * @brief Decodes a string view to a bool.
     * @param value The string view to decode.
     * @param result The resulting bool.
     * @throws std::invalid_argument if the string is not a valid boolean
     * representation.
     */
    void decode(std::string_view value, bool &result) const {
        std::string str(value);
        std::ranges::transform(str, str.begin(), [](char c) {
            return static_cast<char>(::toupper(c));
        });

        if (str == "TRUE" || str == "YES" || str == "1" || str == "ON") {
            result = true;
        } else if (str == "FALSE" || str == "NO" || str == "0" ||
                   str == "OFF") {
            result = false;
        } else {
            throw std::invalid_argument("Field is not a valid boolean value");
        }
    }

    /**
     * @brief Encodes a bool to a string.
     * @param value The bool to encode.
     * @param result The resulting string.
     */
    void encode(const bool value, std::string &result) const noexcept {
        result = value ? "true" : "false";
    }
};

/**
 * @brief Specialization of Convert for char type.
 */
template <>
struct Convert<char> {
    /**
     * @brief Decodes a string view to a char.
     * @param value The string view to decode.
     * @param result The resulting char.
     * @throws std::invalid_argument if the string is empty.
     */
    void decode(std::string_view value, char &result) const {
        if (value.empty())
            throw std::invalid_argument(
                "Field is empty, cannot convert to char");
        result = value.front();
    }

    /**
     * @brief Encodes a char to a string.
     * @param value The char to encode.
     * @param result The resulting string.
     */
    void encode(const char value, std::string &result) const noexcept {
        result = value;
    }
};

/**
 * @brief Specialization of Convert for unsigned char type.
 */
template <>
struct Convert<unsigned char> {
    /**
     * @brief Decodes a string view to an unsigned char.
     * @param value The string view to decode.
     * @param result The resulting unsigned char.
     * @throws std::invalid_argument if the string is empty.
     */
    void decode(std::string_view value, unsigned char &result) const {
        if (value.empty())
            throw std::invalid_argument("field is empty");
        result = value.front();
    }

    /**
     * @brief Encodes an unsigned char to a string.
     * @param value The unsigned char to encode.
     * @param result The resulting string.
     */
    void encode(const unsigned char value, std::string &result) const noexcept {
        result = value;
    }
};

/**
 * @brief Specialization of Convert for integral types using a concept.
 */
template <std::integral T>
    requires(!std::is_same_v<T, bool> && !std::is_same_v<T, char> &&
             !std::is_same_v<T, unsigned char>)
struct Convert<T> {
    /**
     * @brief Decodes a string view to an integral type.
     * @param value The string view to decode.
     * @param result The resulting integral value.
     * @throws std::invalid_argument if the string cannot be converted.
     */
    void decode(std::string_view value, T &result) const {
        try {
            if constexpr (std::is_signed_v<T>) {
                if (auto tmp = strToLong(value); tmp.has_value()) {
                    if (tmp.value() < std::numeric_limits<T>::min() ||
                        tmp.value() > std::numeric_limits<T>::max()) {
                        throw std::out_of_range(
                            "Value out of range for the specified integral "
                            "type");
                    }
                    result = static_cast<T>(tmp.value());
                } else {
                    throw std::invalid_argument(
                        "Cannot convert to integral type");
                }
            } else {
                if (auto tmp = strToULong(value); tmp.has_value()) {
                    if (tmp.value() > std::numeric_limits<T>::max()) {
                        throw std::out_of_range(
                            "Value out of range for the specified integral "
                            "type");
                    }
                    result = static_cast<T>(tmp.value());
                } else {
                    throw std::invalid_argument(
                        "Cannot convert to integral type");
                }
            }
        } catch (const std::exception &e) {
            throw std::invalid_argument(
                std::string("Failed to convert value: ") + e.what());
        }
    }

    /**
     * @brief Encodes an integral value to a string.
     * @param value The integral value to encode.
     * @param result The resulting string.
     */
    void encode(const T value, std::string &result) const noexcept {
        result = std::to_string(value);
    }
};

/**
 * @brief Specialization of Convert for floating point types.
 */
template <std::floating_point T>
struct Convert<T> {
    /**
     * @brief Decodes a string view to a floating point type.
     * @param value The string view to decode.
     * @param result The resulting floating point value.
     * @throws std::invalid_argument if the string cannot be converted.
     */
    void decode(std::string_view value, T &result) const {
        try {
            std::string str(value);
            result = static_cast<T>(std::stod(str));

            // Validate for infinity/NaN results
            if (!std::isfinite(result)) {
                throw std::out_of_range(
                    "Conversion resulted in non-finite value");
            }
        } catch (const std::exception &e) {
            throw std::invalid_argument(
                std::string("Failed to convert to floating point: ") +
                e.what());
        }
    }

    /**
     * @brief Encodes a floating point value to a string.
     * @param value The floating point value to encode.
     * @param result The resulting string.
     */
    void encode(const T value, std::string &result) const {
        // Handle special cases to ensure consistency
        if (std::isnan(value)) {
            result = "nan";
            return;
        }
        if (std::isinf(value)) {
            result = value > 0 ? "inf" : "-inf";
            return;
        }

        // Use precision-controlled conversion for better output
        std::ostringstream ss;
        ss << std::setprecision(std::numeric_limits<T>::max_digits10) << value;
        result = ss.str();
    }
};

/**
 * @brief Specialization of Convert for std::string type.
 */
template <>
struct Convert<std::string> {
    /**
     * @brief Decodes a string view to a std::string.
     * @param value The string view to decode.
     * @param result The resulting std::string.
     */
    void decode(std::string_view value, std::string &result) const noexcept {
        result = value;
    }

    /**
     * @brief Encodes a std::string to a string.
     * @param value The std::string to encode.
     * @param result The resulting string.
     */
    void encode(const std::string &value, std::string &result) const noexcept {
        result = value;
    }
};

#ifdef __cpp_lib_string_view
/**
 * @brief Specialization of Convert for std::string_view type.
 */
template <>
struct Convert<std::string_view> {
    /**
     * @brief Decodes a string view to a std::string_view.
     * @param value The string view to decode.
     * @param result The resulting std::string_view.
     */
    void decode(std::string_view value,
                std::string_view &result) const noexcept {
        result = value;
    }

    /**
     * @brief Encodes a std::string_view to a string.
     * @param value The std::string_view to encode.
     * @param result The resulting string.
     */
    void encode(std::string_view value, std::string &result) const noexcept {
        result = value;
    }
};
#endif

/**
 * @brief Specialization of Convert for const char* type.
 */
template <>
struct Convert<const char *> {
    /**
     * @brief Encodes a const char* to a string.
     * @param value The const char* to encode.
     * @param result The resulting string.
     */
    void encode(const char *const &value, std::string &result) const noexcept {
        result = value;
    }

    /**
     * @brief Decodes a string view to a const char*.
     * @param value The string view to decode.
     * @param result The resulting const char*.
     */
    void decode(std::string_view value, const char *&result) const noexcept {
        result = value.data();
    }
};

/**
 * @brief Specialization of Convert for char arrays.
 * @tparam N The size of the char array.
 */
template <size_t N>
struct Convert<char[N]> {
    /**
     * @brief Decodes a string to a char array.
     * @param value The string to decode.
     * @param result The resulting char array.
     * @throws std::invalid_argument if the string is too large for the char
     * array.
     */
    void decode(const std::string &value, char (&result)[N]) const {
        if (value.size() >= N)
            throw std::invalid_argument(
                "field value is too large for the char array");
        std::copy(value.begin(), value.end(), result);
        result[value.size()] = '\0';  // Null-terminate the char array
    }

    /**
     * @brief Encodes a char array to a string.
     * @param value The char array to encode.
     * @param result The resulting string.
     */
    void encode(const char (&value)[N], std::string &result) const noexcept {
        result = value;
    }
};

}  // namespace inicpp

#endif  // ATOM_EXTRA_INICPP_CONVERT_HPP
