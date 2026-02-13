#ifndef ATOM_SECRET_CORE_UTILS_HPP
#define ATOM_SECRET_CORE_UTILS_HPP

#include <chrono>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <string_view>

#include "types.hpp"

namespace atom::secret {

/**
 * @brief Utility functions for the secret module.
 */
class Utils {
public:
    // ========================================================================
    // Time Utilities
    // ========================================================================

    /**
     * @brief Formats a time point as ISO 8601 string.
     * @param tp Time point to format.
     * @return ISO 8601 formatted string (e.g., "2024-01-15T10:30:00Z").
     */
    static std::string formatTimeIso8601(TimePoint tp) {
        if (tp == TimePoint{}) {
            return "";
        }
        auto time = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream ss;
        ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }

    /**
     * @brief Parses an ISO 8601 string to time point.
     * @param str ISO 8601 formatted string.
     * @return Time point or epoch if parsing fails.
     */
    static TimePoint parseTimeIso8601(std::string_view str) {
        if (str.empty()) {
            return TimePoint{};
        }
        std::tm tm = {};
        std::istringstream ss(std::string(str));
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        if (ss.fail()) {
            return TimePoint{};
        }
#if defined(_WIN32)
        time_t time = _mkgmtime(&tm);
#else
        time_t time = timegm(&tm);
#endif
        return std::chrono::system_clock::from_time_t(time);
    }

    /**
     * @brief Formats a duration as human-readable string.
     * @param seconds Duration in seconds.
     * @return Human-readable string (e.g., "2 hours", "3 days").
     */
    static std::string formatDuration(int64_t seconds) {
        if (seconds < 60) {
            return std::to_string(seconds) + " seconds";
        }
        if (seconds < 3600) {
            return std::to_string(seconds / 60) + " minutes";
        }
        if (seconds < 86400) {
            return std::to_string(seconds / 3600) + " hours";
        }
        if (seconds < 2592000) {
            return std::to_string(seconds / 86400) + " days";
        }
        if (seconds < 31536000) {
            return std::to_string(seconds / 2592000) + " months";
        }
        return std::to_string(seconds / 31536000) + " years";
    }

    // ========================================================================
    // String Utilities
    // ========================================================================

    /**
     * @brief Trims whitespace from both ends of a string.
     * @param str String to trim.
     * @return Trimmed string.
     */
    static std::string trim(std::string_view str) {
        size_t start = 0;
        size_t end = str.length();

        while (start < end &&
               std::isspace(static_cast<unsigned char>(str[start]))) {
            ++start;
        }
        while (end > start &&
               std::isspace(static_cast<unsigned char>(str[end - 1]))) {
            --end;
        }

        return std::string(str.substr(start, end - start));
    }

    /**
     * @brief Converts string to lowercase.
     * @param str String to convert.
     * @return Lowercase string.
     */
    static std::string toLower(std::string_view str) {
        std::string result;
        result.reserve(str.length());
        for (char c : str) {
            result +=
                static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return result;
    }

    /**
     * @brief Converts string to uppercase.
     * @param str String to convert.
     * @return Uppercase string.
     */
    static std::string toUpper(std::string_view str) {
        std::string result;
        result.reserve(str.length());
        for (char c : str) {
            result +=
                static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return result;
    }

    /**
     * @brief Checks if string contains another string (case-insensitive).
     * @param haystack String to search in.
     * @param needle String to search for.
     * @return True if found.
     */
    static bool containsIgnoreCase(std::string_view haystack,
                                   std::string_view needle) {
        if (needle.empty())
            return true;
        if (haystack.length() < needle.length())
            return false;

        std::string lowerHaystack = toLower(haystack);
        std::string lowerNeedle = toLower(needle);

        return lowerHaystack.find(lowerNeedle) != std::string::npos;
    }

    /**
     * @brief Masks a string for display (e.g., password masking).
     * @param str String to mask.
     * @param visibleStart Number of characters to show at start.
     * @param visibleEnd Number of characters to show at end.
     * @param maskChar Character to use for masking.
     * @return Masked string.
     */
    static std::string mask(std::string_view str, size_t visibleStart = 0,
                            size_t visibleEnd = 0, char maskChar = '*') {
        if (str.length() <= visibleStart + visibleEnd) {
            return std::string(str.length(), maskChar);
        }

        std::string result;
        result.reserve(str.length());

        for (size_t i = 0; i < str.length(); ++i) {
            if (i < visibleStart || i >= str.length() - visibleEnd) {
                result += str[i];
            } else {
                result += maskChar;
            }
        }

        return result;
    }

    // ========================================================================
    // Validation Utilities
    // ========================================================================

    /**
     * @brief Validates an email address format.
     * @param email Email to validate.
     * @return True if valid format.
     */
    static bool isValidEmail(std::string_view email) {
        if (email.empty() || email.length() > 254) {
            return false;
        }

        size_t atPos = email.find('@');
        if (atPos == std::string_view::npos || atPos == 0 ||
            atPos == email.length() - 1) {
            return false;
        }

        // Check for multiple @ symbols
        if (email.find('@', atPos + 1) != std::string_view::npos) {
            return false;
        }

        // Check domain has at least one dot
        std::string_view domain = email.substr(atPos + 1);
        if (domain.find('.') == std::string_view::npos) {
            return false;
        }

        return true;
    }

    /**
     * @brief Validates a URL format.
     * @param url URL to validate.
     * @return True if valid format.
     */
    static bool isValidUrl(std::string_view url) {
        if (url.empty()) {
            return false;
        }

        // Check for common schemes
        if (url.substr(0, 7) != "http://" && url.substr(0, 8) != "https://" &&
            url.substr(0, 6) != "ftp://") {
            return false;
        }

        return true;
    }

    // ========================================================================
    // Random Utilities
    // ========================================================================

    /**
     * @brief Generates a random integer in range [min, max].
     * @param min Minimum value.
     * @param max Maximum value.
     * @return Random integer.
     */
    static int randomInt(int min, int max) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(min, max);
        return dist(gen);
    }

    /**
     * @brief Shuffles a string randomly.
     * @param str String to shuffle.
     * @return Shuffled string.
     */
    static std::string shuffleString(std::string str) {
        static std::random_device rd;
        static std::mt19937 gen(rd());

        for (size_t i = str.length() - 1; i > 0; --i) {
            std::uniform_int_distribution<size_t> dist(0, i);
            size_t j = dist(gen);
            std::swap(str[i], str[j]);
        }

        return str;
    }
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_CORE_UTILS_HPP
