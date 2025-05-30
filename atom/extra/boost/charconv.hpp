#ifndef ATOM_EXTRA_BOOST_CHARCONV_HPP
#define ATOM_EXTRA_BOOST_CHARCONV_HPP

#if __has_include(<boost/charconv.hpp>)
#include <array>
#include <boost/charconv.hpp>
#include <charconv>
#include <cmath>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace atom::extra::boost {

constexpr int ALIGNMENT = 16;
constexpr int DEFAULT_BASE = 10;
constexpr size_t BUFFER_SIZE = 128;

/**
 * @brief Enum class representing different number formats
 */
enum class NumberFormat { GENERAL, SCIENTIFIC, FIXED, HEX };

/**
 * @brief Struct for specifying format options for number conversion
 */
struct alignas(ALIGNMENT) FormatOptions {
    NumberFormat format = NumberFormat::GENERAL;
    std::optional<int> precision = std::nullopt;
    bool uppercase = false;
    char thousandsSeparator = '\0';
};

/**
 * @brief Class for converting numbers to and from strings using Boost.CharConv
 */
class BoostCharConv {
public:
    /**
     * @brief Converts an integer to a string
     * @tparam T The type of the integer
     * @param value The integer value to convert
     * @param base The base for the conversion (default is 10)
     * @param options The format options for the conversion
     * @return The converted string
     * @throws std::runtime_error if the conversion fails
     */
    template <typename T>
    [[nodiscard]] static std::string intToString(
        T value, int base = DEFAULT_BASE, const FormatOptions& options = {}) {
        static_assert(std::is_integral_v<T>,
                      "intToString only works with integral types");

        std::array<char, BUFFER_SIZE> buffer{};
        auto result = std::to_chars(buffer.data(),
                                    buffer.data() + buffer.size(), value, base);

        if ((result.ec == std::errc{})) [[likely]] {
            std::string str(buffer.data(), result.ptr);
            if (options.thousandsSeparator != '\0') {
                str = addThousandsSeparator(str, options.thousandsSeparator);
            }
            return options.uppercase ? toUpper(std::move(str)) : str;
        }
        throw std::runtime_error("Int to string conversion failed: " +
                                 std::make_error_code(result.ec).message());
    }

    /**
     * @brief Converts a floating-point number to a string
     * @tparam T The type of the floating-point number
     * @param value The floating-point value to convert
     * @param options The format options for the conversion
     * @return The converted string
     * @throws std::runtime_error if the conversion fails
     */
    template <typename T>
    [[nodiscard]] static std::string floatToString(
        T value, const FormatOptions& options = {}) {
        static_assert(std::is_floating_point_v<T>,
                      "floatToString only works with floating-point types");

        std::array<char, BUFFER_SIZE> buffer{};
        auto format = getFloatFormat(options.format);

        auto result = options.precision
                          ? ::boost::charconv::to_chars(
                                buffer.data(), buffer.data() + buffer.size(),
                                value, format, *options.precision)
                          : ::boost::charconv::to_chars(
                                buffer.data(), buffer.data() + buffer.size(),
                                value, format);

        if ((result.ec == std::errc{})) [[likely]] {
            std::string str(buffer.data(), result.ptr);
            if (options.thousandsSeparator != '\0') {
                str = addThousandsSeparator(str, options.thousandsSeparator);
            }
            return options.uppercase ? toUpper(std::move(str)) : str;
        }
        throw std::runtime_error("Float to string conversion failed: " +
                                 std::make_error_code(result.ec).message());
    }

    /**
     * @brief Converts a string to an integer
     * @tparam T The type of the integer
     * @param str The string to convert
     * @param base The base for the conversion (default is 10)
     * @return The converted integer
     * @throws std::runtime_error if the conversion fails
     */
    template <typename T>
    [[nodiscard]] static T stringToInt(std::string_view str,
                                       int base = DEFAULT_BASE) {
        static_assert(std::is_integral_v<T>,
                      "stringToInt only works with integral types");

        T value;
        auto result = ::boost::charconv::from_chars(
            str.data(), str.data() + str.size(), value, base);

        if ((result.ec == std::errc{} && result.ptr == str.data() + str.size()))
            [[likely]] {
            return value;
        }
        throw std::runtime_error("String to int conversion failed: " +
                                 std::make_error_code(result.ec).message());
    }

    /**
     * @brief Converts a string to a floating-point number
     * @tparam T The type of the floating-point number
     * @param str The string to convert
     * @return The converted floating-point number
     * @throws std::runtime_error if the conversion fails
     */
    template <typename T>
    [[nodiscard]] static T stringToFloat(std::string_view str) {
        static_assert(std::is_floating_point_v<T>,
                      "stringToFloat only works with floating-point types");

        T value;
        auto result = ::boost::charconv::from_chars(
            str.data(), str.data() + str.size(), value);

        if ((result.ec == std::errc{} && result.ptr == str.data() + str.size()))
            [[likely]] {
            return value;
        }
        throw std::runtime_error("String to float conversion failed: " +
                                 std::make_error_code(result.ec).message());
    }

    /**
     * @brief Converts a value to a string using the appropriate conversion
     * function
     * @tparam T The type of the value
     * @param value The value to convert
     * @param options The format options for the conversion
     * @return The converted string
     */
    template <typename T>
    [[nodiscard]] static std::string toString(
        T value, const FormatOptions& options = {}) {
        if constexpr (std::is_integral_v<T>) {
            return intToString(value, DEFAULT_BASE, options);
        } else if constexpr (std::is_floating_point_v<T>) {
            return floatToString(value, options);
        } else {
            static_assert(always_false_v<T>, "Unsupported type for toString");
        }
    }

    /**
     * @brief Converts a string to a value using the appropriate conversion
     * function
     * @tparam T The type of the value
     * @param str The string to convert
     * @param base The base for the conversion (default is 10)
     * @return The converted value
     */
    template <typename T>
    [[nodiscard]] static T fromString(std::string_view str,
                                      int base = DEFAULT_BASE) {
        if constexpr (std::is_integral_v<T>) {
            return stringToInt<T>(str, base);
        } else if constexpr (std::is_floating_point_v<T>) {
            return stringToFloat<T>(str);
        } else {
            static_assert(always_false_v<T>, "Unsupported type for fromString");
        }
    }

    /**
     * @brief Converts special floating-point values (NaN, Inf) to strings
     * @tparam T The type of the floating-point value
     * @param value The floating-point value to convert
     * @return The converted string
     */
    template <typename T>
    [[nodiscard]] static std::string specialValueToString(T value) {
        static_assert(
            std::is_floating_point_v<T>,
            "specialValueToString only works with floating-point types");

        if ((std::isnan(value))) [[unlikely]] {
            return "NaN";
        }
        if ((std::isinf(value))) [[unlikely]] {
            return value > 0 ? "Inf" : "-Inf";
        }
        return toString(value);
    }

    /**
     * @brief Converts a string to a boolean value
     * @param str The string to convert (case-insensitive)
     * @return The converted boolean value
     * @throws std::invalid_argument if the string cannot be converted
     */
    [[nodiscard]] static bool stringToBool(std::string_view str) {
        auto trimmed = trim(str);
        auto lowerStr = toLower(trimmed);

        if ((lowerStr == "true" || lowerStr == "1")) {
            return true;
        }
        if ((lowerStr == "false" || lowerStr == "0")) {
            return false;
        }
        throw std::invalid_argument("Invalid boolean string: " +
                                    std::string(str));
    }

    /**
     * @brief Converts a boolean value to a string
     * @param value The boolean value to convert
     * @return The converted string ("true" or "false")
     */
    [[nodiscard]] static constexpr std::string_view boolToString(
        bool value) noexcept {
        return value ? "true" : "false";
    }

    /**
     * @brief Checks if a string represents a valid number
     * @param str The string to check
     * @return True if the string is a valid number, false otherwise
     */
    [[nodiscard]] static bool isValidNumber(std::string_view str) noexcept {
        if (str.empty()) {
            return false;
        }

        char* end = nullptr;
        std::strtod(str.data(), &end);
        return end == str.data() + str.size();
    }

private:
    template <typename T>
    static constexpr bool always_false_v = false;

    /**
     * @brief Gets the Boost.CharConv format for floating-point numbers
     * @param format The number format
     * @return The Boost.CharConv format
     */
    [[nodiscard]] static constexpr ::boost::charconv::chars_format
    getFloatFormat(NumberFormat format) noexcept {
        switch (format) {
            case NumberFormat::SCIENTIFIC:
                return ::boost::charconv::chars_format::scientific;
            case NumberFormat::FIXED:
                return ::boost::charconv::chars_format::fixed;
            case NumberFormat::HEX:
                return ::boost::charconv::chars_format::hex;
            default:
                return ::boost::charconv::chars_format::general;
        }
    }

    /**
     * @brief Adds a thousands separator to a string
     * @param str The string to modify
     * @param separator The character to use as a thousands separator
     * @return The modified string with thousands separators
     */
    [[nodiscard]] static std::string addThousandsSeparator(std::string_view str,
                                                           char separator) {
        std::string result;
        result.reserve(str.size() +
                       str.size() / 3);  // Reserve extra space for separators

        int count = 0;
        bool pastDecimalPoint = false;

        for (char ch : std::ranges::reverse_view(str)) {
            if (ch == '.') {
                pastDecimalPoint = true;
            }
            if (!pastDecimalPoint && count > 0 && count % 3 == 0) {
                result.push_back(separator);
            }
            result.push_back(ch);
            if (std::isdigit(ch)) {
                count++;
            }
        }

        std::ranges::reverse(result);
        return result;
    }

    /**
     * @brief Converts a string to uppercase
     * @param str The string to convert
     * @return The converted uppercase string
     */
    [[nodiscard]] static std::string toUpper(std::string str) {
        std::ranges::transform(str, str.begin(), [](char ch) {
            return static_cast<char>(
                std::toupper(static_cast<unsigned char>(ch)));
        });
        return str;
    }

    /**
     * @brief Converts a string to lowercase
     * @param str The string to convert
     * @return The converted lowercase string
     */
    [[nodiscard]] static std::string toLower(std::string str) {
        std::ranges::transform(str, str.begin(), [](char ch) {
            return static_cast<char>(
                std::tolower(static_cast<unsigned char>(ch)));
        });
        return str;
    }

    /**
     * @brief Trims whitespace from both ends of a string
     * @param str The string to trim
     * @return The trimmed string
     */
    [[nodiscard]] static std::string trim(std::string_view str) {
        constexpr std::string_view whitespace = " \t\n\r\f\v";

        auto start = str.find_first_not_of(whitespace);
        if (start == std::string_view::npos) {
            return {};
        }

        auto end = str.find_last_not_of(whitespace);
        return std::string(str.substr(start, end - start + 1));
    }
};

}  // namespace atom::extra::boost

#endif  // __has_include(<boost/charconv.hpp>)

#endif  // ATOM_EXTRA_BOOST_CHARCONV_HPP
