#ifndef ATOM_EXTRA_BOOST_CHARCONV_HPP
#define ATOM_EXTRA_BOOST_CHARCONV_HPP

#include <array>
#include <charconv>
#if __has_include(<boost/charconv.hpp>)
#include <boost/charconv.hpp>
#define ATOM_HAS_BOOST_CHARCONV 1
#else
#define ATOM_HAS_BOOST_CHARCONV 0
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <execution>
#include <memory_resource>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <vector>
#ifdef __AVX2__
#include <immintrin.h>  // For SIMD support
#endif

namespace atom::extra::boost {

constexpr int ALIGNMENT = 32;  // Increased for SIMD alignment
constexpr int DEFAULT_BASE = 10;
constexpr size_t BUFFER_SIZE = 256;  // Increased buffer size
constexpr size_t BATCH_SIZE = 64;    // For batch operations
constexpr size_t CACHE_SIZE = 1024;  // For caching frequently used conversions

/**
 * @brief Enum class representing different number formats
 */
enum class NumberFormat {
    GENERAL,
    SCIENTIFIC,
    FIXED,
    HEX,
    ENGINEERING,
    COMPACT
};

/**
 * @brief Enum class for locale-specific formatting
 */
enum class LocaleFormat { C, SYSTEM, CUSTOM };

/**
 * @brief Structure for advanced format options
 */
struct alignas(ALIGNMENT) FormatOptions {
    NumberFormat format = NumberFormat::GENERAL;
    std::optional<int> precision = std::nullopt;
    char thousandsSeparator = '\0';
    char decimalSeparator = '.';
    bool uppercase = false;
    bool showPositiveSign = false;
    bool padWithZeros = false;
    int minimumWidth = 0;
    LocaleFormat localeFormat = LocaleFormat::C;
    std::string customLocale;
    bool useGrouping = false;
    std::string currencySymbol;
};

/**
 * @brief Cache entry for frequently used conversions
 */
template <typename T>
struct CacheEntry {
    T value;
    std::string result;
    FormatOptions options;
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief High-performance class for converting numbers to and from strings
 * using Boost.CharConv with advanced features including SIMD optimization,
 * caching, and batch operations
 */
class BoostCharConv {
private:
    // Thread-local cache for frequently used conversions
    static thread_local std::unordered_map<std::string, std::string>
        conversion_cache_;
    static thread_local std::chrono::steady_clock::time_point
        last_cache_cleanup_;

    // Memory pool for efficient string allocations
    static thread_local std::pmr::unsynchronized_pool_resource memory_pool_;

public:
    /**
     * @brief Converts an integer to a string with advanced formatting and
     * caching
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

        // Check cache for frequently used conversions
        if (base == 10 && options.format == NumberFormat::GENERAL) {
            auto cache_key =
                std::to_string(value) + "_" +
                std::to_string(static_cast<int>(options.uppercase));
            if (auto cached = getCachedResult(cache_key); !cached.empty()) {
                return cached;
            }
        }

        alignas(ALIGNMENT) std::array<char, BUFFER_SIZE> buffer{};
        auto result = std::to_chars(buffer.data(),
                                    buffer.data() + buffer.size(), value, base);

        if ((result.ec == std::errc{})) [[likely]] {
            std::string str(buffer.data(), result.ptr);

            // Apply advanced formatting
            str = applyAdvancedFormatting(str, options);

            // Cache the result if it's a common conversion
            if (base == 10 && options.format == NumberFormat::GENERAL) {
                auto cache_key =
                    std::to_string(value) + "_" +
                    std::to_string(static_cast<int>(options.uppercase));
                cacheResult(cache_key, str);
            }

            return str;
        }
        throw std::runtime_error("Int to string conversion failed: " +
                                 std::make_error_code(result.ec).message());
    }

    /**
     * @brief Batch converts multiple integers to strings with SIMD optimization
     * @tparam T The type of the integers
     * @param values Span of integer values to convert
     * @param base The base for the conversion (default is 10)
     * @param options The format options for the conversion
     * @return Vector of converted strings
     */
    template <typename T>
    [[nodiscard]] static std::vector<std::string> batchIntToString(
        std::span<const T> values, int base = DEFAULT_BASE,
        const FormatOptions& options = {}) {
        static_assert(std::is_integral_v<T>,
                      "batchIntToString only works with integral types");

        std::vector<std::string> results;
        results.reserve(values.size());

        // Process in batches for better cache performance
        for (size_t i = 0; i < values.size(); i += BATCH_SIZE) {
            size_t batch_end = std::min(i + BATCH_SIZE, values.size());

            // Use parallel execution for large batches
            if (batch_end - i > 16) {
                std::vector<std::string> batch_results(batch_end - i);
                std::transform(std::execution::par_unseq, values.begin() + i,
                               values.begin() + batch_end,
                               batch_results.begin(),
                               [base, &options](T value) {
                                   return intToString(value, base, options);
                               });
                results.insert(results.end(), batch_results.begin(),
                               batch_results.end());
            } else {
                for (size_t j = i; j < batch_end; ++j) {
                    results.emplace_back(intToString(values[j], base, options));
                }
            }
        }

        return results;
    }

    /**
     * @brief Converts a floating-point number to a string with advanced
     * formatting
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

        // Handle special values first
        if (std::isnan(value)) [[unlikely]] {
            return options.uppercase ? "NAN" : "nan";
        }
        if (std::isinf(value)) [[unlikely]] {
            if (value > 0) {
                return options.uppercase ? "INF" : "inf";
            } else {
                return options.uppercase ? "-INF" : "-inf";
            }
        }

        alignas(ALIGNMENT) std::array<char, BUFFER_SIZE> buffer{};
        std::to_chars_result result;

#if ATOM_HAS_BOOST_CHARCONV
        auto format = getFloatFormat(options.format);
        result = options.precision
                     ? ::boost::charconv::to_chars(
                           buffer.data(), buffer.data() + buffer.size(), value,
                           format, *options.precision)
                     : ::boost::charconv::to_chars(
                           buffer.data(), buffer.data() + buffer.size(), value,
                           format);
#else
        // Fallback to standard library charconv
        if (options.precision) {
            result = std::to_chars(buffer.data(), buffer.data() + buffer.size(),
                                   value, getStdFloatFormat(options.format),
                                   *options.precision);
        } else {
            result = std::to_chars(buffer.data(), buffer.data() + buffer.size(),
                                   value, getStdFloatFormat(options.format));
        }
#endif

        if (result.ec == std::errc{}) [[likely]] {
            std::string str(buffer.data(), result.ptr);

            // Apply advanced formatting
            str = applyAdvancedFormatting(str, options);

            return str;
        }
        throw std::runtime_error("Float to string conversion failed: " +
                                 std::make_error_code(result.ec).message());
    }

    /**
     * @brief Batch converts multiple floating-point numbers to strings
     * @tparam T The type of the floating-point numbers
     * @param values Span of floating-point values to convert
     * @param options The format options for the conversion
     * @return Vector of converted strings
     */
    template <typename T>
    [[nodiscard]] static std::vector<std::string> batchFloatToString(
        std::span<const T> values, const FormatOptions& options = {}) {
        static_assert(
            std::is_floating_point_v<T>,
            "batchFloatToString only works with floating-point types");

        std::vector<std::string> results;
        results.reserve(values.size());

        // Use SIMD for batch processing when possible
        if constexpr (std::is_same_v<T, float> && sizeof(T) == 4) {
            return batchFloatToStringSimd(values, options);
        } else {
            std::transform(
                std::execution::par_unseq, values.begin(), values.end(),
                std::back_inserter(results),
                [&options](T value) { return floatToString(value, options); });
        }

        return results;
    }

    /**
     * @brief Converts a string to an integer with enhanced error handling
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

        // Preprocess string to handle locale-specific formatting
        auto cleaned_str = preprocessNumericString(str);

        T value;
#if ATOM_HAS_BOOST_CHARCONV
        auto result = ::boost::charconv::from_chars(
            cleaned_str.data(), cleaned_str.data() + cleaned_str.size(), value,
            base);
#else
        auto result = std::from_chars(cleaned_str.data(),
                                      cleaned_str.data() + cleaned_str.size(),
                                      value, base);
#endif

        if ((result.ec == std::errc{} &&
             result.ptr == cleaned_str.data() + cleaned_str.size()))
            [[likely]] {
            return value;
        }
        throw std::runtime_error("String to int conversion failed: " +
                                 std::make_error_code(result.ec).message());
    }

    /**
     * @brief Safely converts a string to an integer with optional result
     * @tparam T The type of the integer
     * @param str The string to convert
     * @param base The base for the conversion (default is 10)
     * @return Optional containing the converted integer or nullopt if
     * conversion fails
     */
    template <typename T>
    [[nodiscard]] static std::optional<T> tryStringToInt(
        std::string_view str, int base = DEFAULT_BASE) noexcept {
        static_assert(std::is_integral_v<T>,
                      "tryStringToInt only works with integral types");

        try {
            auto cleaned_str = preprocessNumericString(str);
            T value;
#if ATOM_HAS_BOOST_CHARCONV
            auto result = ::boost::charconv::from_chars(
                cleaned_str.data(), cleaned_str.data() + cleaned_str.size(),
                value, base);
#else
            auto result = std::from_chars(
                cleaned_str.data(), cleaned_str.data() + cleaned_str.size(),
                value, base);
#endif

            if (result.ec == std::errc{} &&
                result.ptr == cleaned_str.data() + cleaned_str.size()) {
                return value;
            }
        } catch (...) {
            // Ignore exceptions and return nullopt
        }
        return std::nullopt;
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
#if ATOM_HAS_BOOST_CHARCONV
        auto result = ::boost::charconv::from_chars(
            str.data(), str.data() + str.size(), value);
#else
        auto result =
            std::from_chars(str.data(), str.data() + str.size(), value);
#endif

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
     * @brief Gets cached conversion result
     * @param key Cache key
     * @return Cached result or empty string if not found
     */
    [[nodiscard]] static std::string getCachedResult(
        const std::string& key) noexcept {
        cleanupCacheIfNeeded();
        auto it = conversion_cache_.find(key);
        return (it != conversion_cache_.end()) ? it->second : std::string{};
    }

    /**
     * @brief Caches a conversion result
     * @param key Cache key
     * @param result Result to cache
     */
    static void cacheResult(const std::string& key,
                            const std::string& result) noexcept {
        if (conversion_cache_.size() < CACHE_SIZE) {
            conversion_cache_[key] = result;
        }
    }

    /**
     * @brief Cleans up cache if needed
     */
    static void cleanupCacheIfNeeded() noexcept {
        auto now = std::chrono::steady_clock::now();
        if (now - last_cache_cleanup_ > std::chrono::minutes(5)) {
            conversion_cache_.clear();
            last_cache_cleanup_ = now;
        }
    }

    /**
     * @brief Applies advanced formatting to a numeric string
     * @param str The string to format
     * @param options Formatting options
     * @return Formatted string
     */
    [[nodiscard]] static std::string applyAdvancedFormatting(
        std::string str, const FormatOptions& options) {
        // Apply thousands separator
        if (options.thousandsSeparator != '\0' && options.useGrouping) {
            str = addThousandsSeparator(str, options.thousandsSeparator);
        }

        // Apply decimal separator
        if (options.decimalSeparator != '.') {
            std::replace(str.begin(), str.end(), '.', options.decimalSeparator);
        }

        // Apply case conversion
        if (options.uppercase) {
            str = toUpper(std::move(str));
        }

        // Apply positive sign
        if (options.showPositiveSign && !str.empty() && str[0] != '-') {
            str = "+" + str;
        }

        // Apply minimum width with padding
        if (options.minimumWidth > 0 &&
            static_cast<int>(str.length()) < options.minimumWidth) {
            if (options.padWithZeros) {
                // Find position to insert zeros (after sign if present)
                size_t insert_pos = (str[0] == '+' || str[0] == '-') ? 1 : 0;
                str.insert(insert_pos, options.minimumWidth - str.length(),
                           '0');
            } else {
                str =
                    std::string(options.minimumWidth - str.length(), ' ') + str;
            }
        }

        return str;
    }

    /**
     * @brief Preprocesses numeric string to handle locale-specific formatting
     * @param str Input string
     * @return Cleaned string suitable for parsing
     */
    [[nodiscard]] static std::string preprocessNumericString(
        std::string_view str) {
        std::string result(str);

        // Remove whitespace
        result.erase(std::remove_if(result.begin(), result.end(), ::isspace),
                     result.end());

        // Handle common thousands separators
        result.erase(std::remove(result.begin(), result.end(), ','),
                     result.end());
        result.erase(std::remove(result.begin(), result.end(), ' '),
                     result.end());

        // Replace common decimal separators with '.'
        std::replace(result.begin(), result.end(), ',', '.');

        return result;
    }

    /**
     * @brief SIMD-optimized batch float to string conversion
     * @tparam T The floating-point type
     * @param values Span of values to convert
     * @param options Formatting options
     * @return Vector of converted strings
     */
    template <typename T>
    [[nodiscard]] static std::vector<std::string> batchFloatToStringSimd(
        std::span<const T> values, const FormatOptions& options) {
        std::vector<std::string> results;
        results.reserve(values.size());

// Process 8 floats at a time using AVX2 if available
#ifdef __AVX2__
        constexpr size_t simd_width = 8;
        size_t simd_count = values.size() / simd_width;

        for (size_t i = 0; i < simd_count * simd_width; i += simd_width) {
            // Load 8 floats into AVX2 register
            __m256 vec = _mm256_loadu_ps(&values[i]);

            // Process each float individually (SIMD string conversion is
            // complex)
            alignas(32) float temp[8];
            _mm256_storeu_ps(temp, vec);

            for (size_t j = 0; j < simd_width; ++j) {
                results.emplace_back(floatToString(temp[j], options));
            }
        }

        // Process remaining elements
        for (size_t i = simd_count * simd_width; i < values.size(); ++i) {
            results.emplace_back(floatToString(values[i], options));
        }
#else
        // Fallback to regular processing
        for (const auto& value : values) {
            results.emplace_back(floatToString(value, options));
        }
#endif

        return results;
    }

#if ATOM_HAS_BOOST_CHARCONV
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
            case NumberFormat::ENGINEERING:
                // Engineering notation is a variant of scientific notation
                return ::boost::charconv::chars_format::scientific;
            case NumberFormat::COMPACT:
                // Compact format uses the shortest representation
                return ::boost::charconv::chars_format::general;
            default:
                return ::boost::charconv::chars_format::general;
        }
    }
#endif

    /**
     * @brief Gets the standard library chars_format for floating-point numbers
     * @param format The number format
     * @return The std::chars_format
     */
    [[nodiscard]] static constexpr std::chars_format getStdFloatFormat(
        NumberFormat format) noexcept {
        switch (format) {
            case NumberFormat::SCIENTIFIC:
            case NumberFormat::ENGINEERING:
                return std::chars_format::scientific;
            case NumberFormat::FIXED:
                return std::chars_format::fixed;
            case NumberFormat::HEX:
                return std::chars_format::hex;
            case NumberFormat::COMPACT:
            case NumberFormat::GENERAL:
            default:
                return std::chars_format::general;
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

// Static member definitions
inline thread_local std::unordered_map<std::string, std::string>
    BoostCharConv::conversion_cache_{};
inline thread_local std::chrono::steady_clock::time_point
    BoostCharConv::last_cache_cleanup_{};
inline thread_local std::pmr::unsynchronized_pool_resource
    BoostCharConv::memory_pool_{};

}  // namespace atom::extra::boost

#endif  // ATOM_EXTRA_BOOST_CHARCONV_HPP
