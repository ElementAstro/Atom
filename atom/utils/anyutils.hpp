/*
 * anyutils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-1

Description: A collection of useful functions with std::any Or Any

**************************************************/

#ifndef ATOM_EXPERIMENT_ANYUTILS_HPP
#define ATOM_EXPERIMENT_ANYUTILS_HPP

#include <algorithm>
#include <concepts>
#include <cstring>
#include <execution>
#include <format>
#include <future>
#include <mutex>
#include <optional>
#include <ranges>
#include <regex>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
#include "atom/containers/high_performance.hpp"  // Include high performance containers
#include "atom/meta/concept.hpp"

// Use type aliases from high_performance.hpp
namespace atom::utils {
using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;
template <typename T>
using Vector = atom::containers::Vector<T>;
}  // namespace atom::utils

// Enhanced version of concepts with better constraints
template <typename T>
concept CanBeStringified = requires(T t) {
    { toString(t) } -> std::convertible_to<atom::utils::String>;
};

template <typename T>
concept CanBeStringifiedToJson = requires(T t) {
    { toJson(t) } -> std::convertible_to<atom::utils::String>;
};

// New concept for XML serialization
template <typename T>
concept CanBeStringifiedToXml = requires(T t, atom::utils::String tag) {
    { toXml(t, tag) } -> std::convertible_to<atom::utils::String>;
};

// New concept for YAML serialization
template <typename T>
concept CanBeStringifiedToYaml = requires(T t, atom::utils::String key) {
    { toYaml(t, key) } -> std::convertible_to<atom::utils::String>;
};

// New concept for TOML serialization
template <typename T>
concept CanBeStringifiedToToml = requires(T t, atom::utils::String key) {
    { toToml(t, key) } -> std::convertible_to<atom::utils::String>;
};

// Forward declarations
template <typename T>
[[nodiscard]] auto toString(const T &value, bool prettyPrint = false)
    -> atom::utils::String;

// Thread-safe string conversion cache using mutex
namespace {
std::mutex cacheMutex;
// Use HashMap and String for cache
atom::utils::HashMap<std::size_t, atom::utils::String> conversionCache;

template <typename T>
std::size_t getTypeHash(const T &value) {
    std::size_t typeHash = typeid(T).hash_code();
    std::size_t valueHash = 0;
    if constexpr (std::is_trivially_copyable_v<T> &&
                  sizeof(T) <= sizeof(std::size_t)) {
        std::memcpy(&valueHash, &value, sizeof(T));
    }
    // Combine hashes (consider a better hash combination function if needed)
    return typeHash ^ (valueHash << 1);
}

template <typename T>
std::optional<atom::utils::String> getCachedString(const T &value) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    auto hash = getTypeHash(value);
    auto it = conversionCache.find(hash);
    if (it != conversionCache.end()) {
        return it->second;
    }
    return std::nullopt;
}

template <typename T>
void cacheString(const T &value, const atom::utils::String &str) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    auto hash = getTypeHash(value);
    conversionCache[hash] = str;
}
}  // namespace

// Improved ranges-based toString with validation and exception handling
template <std::ranges::input_range Container>
[[nodiscard]] auto toString(const Container &container,
                            bool prettyPrint = false) -> atom::utils::String {
    try {
        if (std::ranges::empty(container)) {
            return "[]";
        }

        // Try to get from cache first
        if (auto cached = getCachedString(container)) {
            return *cached;
        }

        atom::utils::String result = "[";
        const atom::utils::String separator = prettyPrint ? ", " : ",";
        const atom::utils::String indent = prettyPrint ? "\n  " : "";

        if (prettyPrint)
            result += indent;

        for (const auto &item : container) {
            // Assuming IsBuiltIn concept is compatible
            if constexpr (IsBuiltIn<std::remove_cvref_t<decltype(item)>>) {
                result += toString(item, prettyPrint) + separator +
                          (prettyPrint ? indent : "");
            } else {
                // Use String concatenation
                result += "\"" + toString(item, prettyPrint) + "\"" +
                          separator + (prettyPrint ? indent : "");
            }
        }

        // Remove the last separator
        if (prettyPrint) {
            // Adjust length calculation if String::length() differs from
            // std::string::length()
            result.erase(result.length() - indent.length() - separator.length(),
                         separator.length() + indent.length());
            result += "\n]";
        } else {
            result.erase(result.length() - separator.length(),
                         separator.length());
            result += "]";
        }

        // Cache result for future use (only if container is small enough)
        // Consider if std::ranges::distance is efficient for the chosen
        // container type
        if constexpr (std::ranges::sized_range<Container>) {
            if (std::ranges::size(container) < 1000) {
                cacheString(container, result);
            }
        } else {
            // Fallback or alternative check for non-sized ranges if needed
            // For simplicity, we might skip caching for non-sized ranges or use
            // a different limit
        }

        return result;
    } catch (const std::exception &e) {
        // Use std::format, assuming String is compatible or convertible
        return std::format("Error converting container to string: {}",
                           e.what())
            .c_str();  // Ensure conversion if needed
    }
}

// Improved map toString with validation (using HashMap)
template <typename K, typename V>
[[nodiscard]] auto toString(const atom::utils::HashMap<K, V> &map,
                            bool prettyPrint = false) -> atom::utils::String {
    try {
        if (map.empty()) {
            return "{}";
        }

        atom::utils::String result = "{";
        const atom::utils::String separator = prettyPrint ? ",\n  " : ", ";

        if (prettyPrint)
            result += "\n  ";

        for (const auto &pair : map) {
            result += toString(pair.first, prettyPrint) + ": " +
                      toString(pair.second, prettyPrint) + separator;
        }

        // Remove last separator
        if (prettyPrint) {
            result.erase(result.length() - separator.length(),
                         separator.length());
            result += "\n}";
        } else {
            result.erase(result.length() - 2, 2);  // Assuming separator ", "
            result += "}";
        }

        return result;
    } catch (const std::exception &e) {
        return std::format("Error converting map to string: {}", e.what())
            .c_str();
    }
}

// Improved pair toString with better exception handling
template <typename T1, typename T2>
[[nodiscard]] auto toString(const std::pair<T1, T2> &pair,
                            bool prettyPrint = false) -> atom::utils::String {
    try {
        return "(" + toString(pair.first, prettyPrint) + ", " +
               toString(pair.second, prettyPrint) + ")";
    } catch (const std::exception &e) {
        return std::format("Error converting pair to string: {}", e.what())
            .c_str();
    }
}

// Base toString implementation with improved type handling
template <typename T>
[[nodiscard]] auto toString(const T &value, bool prettyPrint)
    -> atom::utils::String {
    try {
        // Assuming String concept works with atom::containers::String
        if constexpr (String<T> || Char<T>) {
            // Explicit conversion might be needed depending on String
            // definition
            return atom::utils::String(value);
        } else if constexpr (std::is_same_v<T, bool>) {
            return value ? "true" : "false";
        } else if constexpr (Number<T>) {
            if constexpr (std::is_floating_point_v<T>) {
                // For floating point, use format with precision
                // std::format returns std::string, convert to
                // atom::utils::String
                return atom::utils::String(std::format("{:.6g}", value));
            } else {
                // std::to_string returns std::string, convert to
                // atom::utils::String
                return atom::utils::String(std::to_string(value));
            }
        } else if constexpr (Pointer<T> || SmartPointer<T>) {
            if (value == nullptr) {
                return "nullptr";
            }
            return toString(*value, prettyPrint);
        } else if constexpr (requires { value.toString(); }) {
            // Support for custom types with toString method returning String
            return value.toString();
        } else {
            return "unknown type";
        }
    } catch (const std::exception &e) {
        return std::format("Error in toString: {}", e.what()).c_str();
    }
}

// Forward declaration for toJson
template <typename T>
[[nodiscard]] auto toJson(const T &value, bool prettyPrint = false)
    -> atom::utils::String;

// Parallelized toJson for large containers
template <std::ranges::input_range Container>
[[nodiscard]] auto toJson(const Container &container, bool prettyPrint = false)
    -> atom::utils::String {
    try {
        if (std::ranges::empty(container)) {
            return "[]";
        }

        const atom::utils::String indent = prettyPrint ? "  " : "";
        const atom::utils::String nl = prettyPrint ? "\n" : "";
        atom::utils::String result = "[" + nl;

        // Determine size efficiently if possible
        std::optional<size_t> containerSize;
        if constexpr (std::ranges::sized_range<Container>) {
            containerSize = std::ranges::size(container);
        }

        // Use parallel execution for large containers
        if (containerSize && *containerSize > 1000) {
            // Use Vector for futures, future result remains std::string for
            // simplicity with std::async Or change lambda return type to String
            // if toJson returns String
            atom::utils::Vector<std::future<atom::utils::String>> futures;
            futures.reserve(*containerSize);

            // Process items in parallel
            for (const auto &item : container) {
                futures.push_back(std::async(
                    std::launch::async,
                    [&item, prettyPrint]() -> atom::utils::String {
                        return toJson(
                            item, prettyPrint);  // Ensure toJson returns String
                    }));
            }

            // Collect results
            for (auto &fut : futures) {
                result += (prettyPrint ? indent : "") + fut.get() + "," + nl;
            }
        } else {
            // Sequential processing for smaller or non-sized containers
            for (const auto &item : container) {
                result += (prettyPrint ? indent : "") +
                          toJson(item, prettyPrint) + "," + nl;
            }
        }

        // Check if items were added before erasing
        bool itemsAdded =
            (containerSize && *containerSize > 0) ||
            (!containerSize && result.length() > (1 + nl.length()));
        if (itemsAdded) {
            // Adjust length calculation based on String type
            result.erase(result.length() - (1 + nl.length()), 1 + nl.length());
        }

        result += nl + "]";
        return result;
    } catch (const std::exception &e) {
        return std::format("{{\"error\": \"Error converting to JSON: {}\"}}",
                           e.what())
            .c_str();
    }
}

// Improved map toJson with validation (using HashMap)
template <typename K, typename V>
[[nodiscard]] auto toJson(const atom::utils::HashMap<K, V> &map,
                          bool prettyPrint = false) -> atom::utils::String {
    try {
        if (map.empty()) {
            return "{}";
        }

        const atom::utils::String indent = prettyPrint ? "  " : "";
        const atom::utils::String nl = prettyPrint ? "\n" : "";
        atom::utils::String result = "{" + nl;

        for (const auto &pair : map) {
            atom::utils::String key;
            // Check if K is String or compatible
            if constexpr (std::is_same_v<K, atom::utils::String> ||
                          std::is_convertible_v<K, const char *>) {
                // Assuming String has constructor from K or K is convertible to
                // const char*
                key = "\"" + atom::utils::String(pair.first) + "\"";
            } else {
                key = "\"" + toString(pair.first, prettyPrint) + "\"";
            }

            result += (prettyPrint ? indent : "") + key + ":" +
                      (prettyPrint ? " " : "") +
                      toJson(pair.second, prettyPrint) + "," + nl;
        }

        if (!map.empty()) {
            result.erase(result.length() - (1 + nl.length()), 1 + nl.length());
        }

        result += nl + "}";
        return result;
    } catch (const std::exception &e) {
        return std::format(
                   "{{\"error\": \"Error converting map to JSON: {}\"}}",
                   e.what())
            .c_str();
    }
}

// Improved pair toJson with validation
template <typename T1, typename T2>
[[nodiscard]] auto toJson(const std::pair<T1, T2> &pair,
                          bool prettyPrint = false) -> atom::utils::String {
    try {
        const atom::utils::String nl = prettyPrint ? "\n" : "";
        const atom::utils::String indent = prettyPrint ? "  " : "";

        atom::utils::String result = "{" + nl;
        result += (prettyPrint ? indent : "") +
                  "\"first\": " + toJson(pair.first, prettyPrint) + "," + nl;
        result += (prettyPrint ? indent : "") +
                  "\"second\": " + toJson(pair.second, prettyPrint) + nl;
        result += "}";

        return result;
    } catch (const std::exception &e) {
        return std::format(
                   "{{\"error\": \"Error converting pair to JSON: {}\"}}",
                   e.what())
            .c_str();
    }
}

// Base toJson implementation with improved type handling and validation
template <typename T>
[[nodiscard]] auto toJson(const T &value, bool prettyPrint)
    -> atom::utils::String {
    try {
        // Assuming String concept works with atom::containers::String
        if constexpr (String<T>) {
            // Escape special characters in strings
            atom::utils::String escaped;
            // Assuming String has reserve and value access (e.g., iterators or
            // operator[])
            escaped.reserve(atom::utils::String(value).size() + 10);

            for (char c :
                 atom::utils::String(value)) {  // Assuming iteration works
                switch (c) {
                    case '\"':
                        escaped += "\\\"";
                        break;
                    case '\\':
                        escaped += "\\\\";
                        break;
                    case '/':
                        escaped += "\\/";
                        break;
                    case '\b':
                        escaped += "\\b";
                        break;
                    case '\f':
                        escaped += "\\f";
                        break;
                    case '\n':
                        escaped += "\\n";
                        break;
                    case '\r':
                        escaped += "\\r";
                        break;
                    case '\t':
                        escaped += "\\t";
                        break;
                    default:
                        if (static_cast<unsigned char>(c) < 32) {
                            // Convert std::format result to String
                            escaped += atom::utils::String(std::format(
                                "\\u{:04x}", static_cast<unsigned int>(c)));
                        } else {
                            escaped += c;
                        }
                }
            }
            return "\"" + escaped + "\"";
        } else if constexpr (Char<T>) {
            if (static_cast<unsigned char>(value) < 32) {
                // Convert std::format result to String
                return atom::utils::String(std::format(
                    "\"\\u{:04x}\"", static_cast<unsigned int>(value)));
            }
            // Assuming String can be constructed from char
            return "\"" + atom::utils::String(1, value) + "\"";
        } else if constexpr (std::is_same_v<T, bool>) {
            return value ? "true" : "false";
        } else if constexpr (Number<T>) {
            if constexpr (std::is_floating_point_v<T>) {
                if (std::isnan(value))
                    return "null";
                if (std::isinf(value))
                    return "null";  // JSON doesn't support infinity
                // Convert std::format result to String
                return atom::utils::String(std::format("{:.12g}", value));
            } else {
                // Convert std::to_string result to String
                return atom::utils::String(std::to_string(value));
            }
        } else if constexpr (Pointer<T> || SmartPointer<T>) {
            if (value == nullptr) {
                return "null";
            }
            return toJson(*value, prettyPrint);
        } else if constexpr (requires { value.toJson(); }) {
            // Support for custom types with toJson method returning String
            return value.toJson();
        } else {
            // Return valid JSON for unknown types
            return "{}";
        }
    } catch (const std::exception &e) {
        return std::format("{{\"error\": \"Error in toJson: {}\"}}", e.what())
            .c_str();
    }
}

// Forward declaration for toXml
template <typename T>
[[nodiscard]] auto toXml(const T &value, const atom::utils::String &tagName)
    -> atom::utils::String;

// Optimized toXml for containers with validation
template <std::ranges::input_range Container>
[[nodiscard]] auto toXml(const Container &container,
                         const atom::utils::String &tagName)
    -> atom::utils::String {
    try {
        // Validate tag name (assuming String has find method)
        if (tagName.find('<') != atom::utils::String::npos ||
            tagName.find('>') != atom::utils::String::npos) {
            throw std::invalid_argument(
                "XML tag name contains invalid characters");
        }

        const atom::utils::String containerTag =
            tagName.empty() ? "items" : tagName;
        atom::utils::String result = "<" + containerTag + ">\n";

        // For large containers, process items in batches
        const size_t batchSize = 50;  // Adjust batch size as needed
        auto it = std::ranges::begin(container);
        const auto end = std::ranges::end(container);

        while (it != end) {
            auto batchEnd = it;
            size_t currentBatchSize = 0;

            while (batchEnd != end && currentBatchSize < batchSize) {
                ++batchEnd;
                ++currentBatchSize;
            }

            // Process this batch
            // Note: Parallel processing with string concatenation needs careful
            // synchronization Using a mutex for appending might serialize
            // execution significantly. Consider alternative parallel
            // aggregation strategies if performance is critical.
            atom::utils::String batchResult;
            std::mutex batchMutex;  // Mutex to protect batchResult

            std::for_each(
                std::execution::par_unseq, it, batchEnd,
                [&batchResult, &tagName, &batchMutex](const auto &item) {
                    const atom::utils::String itemTag =
                        tagName.empty() ? "item" : tagName + "_item";
                    atom::utils::String itemResult = toXml(item, itemTag);

                    // Thread-safe append to batch result
                    std::lock_guard<std::mutex> lock(batchMutex);
                    batchResult += itemResult;  // Ensure String += is
                                                // thread-safe or protected
                });

            result += batchResult;
            it = batchEnd;
        }

        result += "</" + containerTag + ">";
        return result;
    } catch (const std::exception &e) {
        return std::format("<error>Error converting to XML: {}</error>",
                           e.what())
            .c_str();
    }
}

// Improved map toXml with validation (using HashMap)
template <typename K, typename V>
[[nodiscard]] auto toXml(const atom::utils::HashMap<K, V> &map,
                         [[maybe_unused]] const atom::utils::String &tagName)
    -> atom::utils::String {
    try {
        // Validate tag name
        if (!tagName.empty() &&
            (tagName.find('<') != atom::utils::String::npos ||
             tagName.find('>') != atom::utils::String::npos)) {
            throw std::invalid_argument(
                "XML tag name contains invalid characters");
        }

        const atom::utils::String mapTag = tagName.empty() ? "map" : tagName;
        atom::utils::String result = "<" + mapTag + ">\n";

        for (const auto &pair : map) {
            atom::utils::String keyStr =
                toString(pair.first);  // Get string representation
            // Sanitize key for XML tag (assuming String has replace or similar)
            // This might be inefficient; consider a dedicated sanitization
            // function
            std::replace(keyStr.begin(), keyStr.end(), ' ', '_');
            std::replace(keyStr.begin(), keyStr.end(), '<', '_');
            std::replace(keyStr.begin(), keyStr.end(), '>', '_');
            std::replace(keyStr.begin(), keyStr.end(), '&', '_');
            // Add more replacements if needed (e.g., for starting with numbers,
            // etc.)
            if (keyStr.empty() || !std::isalpha(keyStr[0])) {
                keyStr = "_" + keyStr;  // Ensure valid tag start
            }

            result += toXml(pair.second, keyStr);
        }

        result += "</" + mapTag + ">";
        return result;
    } catch (const std::exception &e) {
        return std::format("<error>Error converting map to XML: {}</error>",
                           e.what())
            .c_str();
    }
}

// Improved pair toXml with validation
template <typename T1, typename T2>
[[nodiscard]] auto toXml(const std::pair<T1, T2> &pair,
                         const atom::utils::String &tagName)
    -> atom::utils::String {
    try {
        // Validate tag name
        if (tagName.find('<') != atom::utils::String::npos ||
            tagName.find('>') != atom::utils::String::npos) {
            throw std::invalid_argument(
                "XML tag name contains invalid characters");
        }

        const atom::utils::String pairTag = tagName.empty() ? "pair" : tagName;
        atom::utils::String result = "<" + pairTag + ">\n";
        result += toXml(pair.first, "key");     // Use String literal
        result += toXml(pair.second, "value");  // Use String literal
        result += "</" + pairTag + ">";
        return result;
    } catch (const std::exception &e) {
        return std::format("<error>Error converting pair to XML: {}</error>",
                           e.what())
            .c_str();
    }
}

// Base toXml implementation with improved type handling and validation
template <typename T>
[[nodiscard]] auto toXml(const T &value, const atom::utils::String &tagName)
    -> atom::utils::String {
    try {
        // Validate tag name
        if (tagName.empty()) {
            throw std::invalid_argument("XML tag name cannot be empty");
        }
        if (tagName.find('<') != atom::utils::String::npos ||
            tagName.find('>') != atom::utils::String::npos) {
            throw std::invalid_argument(
                "XML tag name contains invalid characters");
        }

        // Assuming String concept works with atom::containers::String
        if constexpr (String<T> || Char<T>) {
            atom::utils::String content = atom::utils::String(value);
            // Escape XML special characters using std::regex_replace
            // Note: std::regex_replace works on std::string. Conversion might
            // be needed.
            std::string std_content(content.begin(),
                                    content.end());  // Convert to std::string
            std_content =
                std::regex_replace(std_content, std::regex("&"), "&amp;");
            std_content =
                std::regex_replace(std_content, std::regex("<"), "&lt;");
            std_content =
                std::regex_replace(std_content, std::regex(">"), "&gt;");
            std_content =
                std::regex_replace(std_content, std::regex("\""), "&quot;");
            std_content =
                std::regex_replace(std_content, std::regex("'"), "&apos;");
            content = atom::utils::String(
                std_content.c_str());  // Convert back to String

            return "<" + tagName + ">" + content + "</" + tagName + ">";
        } else if constexpr (Number<T>) {
            // Convert std::to_string result to String
            return "<" + tagName + ">" +
                   atom::utils::String(std::to_string(value)) + "</" + tagName +
                   ">";
        } else if constexpr (Pointer<T> || SmartPointer<T>) {
            if (value == nullptr) [[unlikely]] {
                return "<" + tagName + " nil=\"true\"/>";
            } else [[likely]] {
                return toXml(*value, tagName);
            }
        } else if constexpr (requires { value.toXml(tagName); }) {
            // Support for custom types with toXml method returning String
            return value.toXml(tagName);
        } else {
            // Return self-closing tag for unknown types
            return "<" + tagName + "/>";
        }
    } catch (const std::exception &e) {
        return std::format("<error>Error in toXml: {}</error>", e.what())
            .c_str();
    }
}

// Forward declaration for toYaml
template <typename T>
[[nodiscard]] auto toYaml(const T &value, const atom::utils::String &key)
    -> atom::utils::String;

// Improved toYaml for containers with validation and performance
template <std::ranges::input_range Container>
[[nodiscard]] auto toYaml(const Container &container,
                          const atom::utils::String &key)
    -> atom::utils::String {
    try {
        atom::utils::String result = key.empty() ? "" : key + ":\n";

        if (std::ranges::empty(container)) {
            return key.empty() ? "[]" : key + ": []\n";
        }

        // Determine size efficiently if possible
        std::optional<size_t> containerSize;
        if constexpr (std::ranges::sized_range<Container>) {
            containerSize = std::ranges::size(container);
        }

        // For large containers, process in parallel
        if (containerSize && *containerSize > 100) {
            // Use Vector for results
            atom::utils::Vector<std::pair<size_t, atom::utils::String>> results;
            results.resize(*containerSize);

            size_t index = 0;
            for ([[maybe_unused]] const auto &item :
                 container) {  // Iterate just to fill indices
                results[index].first = index;
                ++index;
            }

            // Process items in parallel
            std::for_each(
                std::execution::par_unseq, results.begin(), results.end(),
                [&container, &key](auto &pair) {
                    auto it = std::ranges::begin(container);
                    std::advance(
                        it,
                        pair.first);  // Advance iterator to the correct item
                    // Ensure toYaml returns String
                    pair.second =
                        (key.empty() ? "- " : "  - ") + toYaml(*it, "") +
                        "\n";  // Assuming toYaml adds newline correctly
                });

            // Combine results in order
            for (const auto &pair : results) {
                result += pair.second;
            }
        } else {
            // Process sequentially for smaller or non-sized containers
            for (const auto &item : container) {
                result += (key.empty() ? "- " : "  - ") + toYaml(item, "") +
                          "\n";  // Assuming toYaml adds newline
            }
        }
        // Remove trailing newline if added by the loop/parallel processing
        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
        result += "\n";  // Ensure single trailing newline

        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           (key.empty() ? "items" : key.c_str()))
            .c_str();  // Use c_str() for format if needed
    }
}

// Improved map toYaml with validation (using HashMap)
template <typename K, typename V>
[[nodiscard]] auto toYaml(const atom::utils::HashMap<K, V> &map,
                          const atom::utils::String &key)
    -> atom::utils::String {
    try {
        if (map.empty()) {
            return key.empty() ? "{}\n" : key + ": {}\n";
        }

        atom::utils::String result = key.empty() ? "" : key + ":\n";

        for (const auto &pair : map) {
            atom::utils::String keyStr;
            // Assuming String concept works with atom::containers::String
            if constexpr (String<K> || Char<K>) {
                atom::utils::String k = atom::utils::String(pair.first);
                // Check if quoting is needed (assuming String has find and
                // front/back)
                if (k.find(':') != atom::utils::String::npos ||
                    k.find('#') != atom::utils::String::npos ||
                    k.find('\n') != atom::utils::String::npos || k.empty() ||
                    (!k.empty() && (k.front() == ' ' || k.back() == ' '))) {
                    keyStr = "\"" + k + "\"";
                } else {
                    keyStr = k;
                }
            } else {
                keyStr = toString(pair.first);  // Use base toString
            }

            // Ensure toYaml returns the full line including key and newline
            result += (key.empty() ? "" : "  ") + toYaml(pair.second, keyStr);
        }

        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           (key.empty() ? "map" : key.c_str()))
            .c_str();
    }
}

// Improved pair toYaml with validation
template <typename T1, typename T2>
[[nodiscard]] auto toYaml(const std::pair<T1, T2> &pair,
                          const atom::utils::String &key)
    -> atom::utils::String {
    try {
        atom::utils::String result = key.empty() ? "" : key + ":\n";
        // Ensure nested toYaml calls handle indentation and newlines correctly
        // The base toYaml should return "key: value\n" format
        result += std::string((key.empty() ? "" : "  ")) +
                  toYaml(pair.first, "key");  // Pass "key" as the key
        result += std::string((key.empty() ? "" : "  ")) +
                  toYaml(pair.second, "value");  // Pass "value" as the key
        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           (key.empty() ? "pair" : key.c_str()))
            .c_str();
    }
}

// Base toYaml implementation with improved type handling and validation
template <typename T>
[[nodiscard]] auto toYaml(const T &value, const atom::utils::String &key)
    -> atom::utils::String {
    try {
        atom::utils::String formattedValue;
        // Assuming String concept works with atom::containers::String
        if constexpr (String<T> || Char<T>) {
            atom::utils::String strValue = atom::utils::String(value);
            bool needsQuotes =
                strValue.empty() ||
                strValue.find('\n') != atom::utils::String::npos ||
                strValue.find(':') != atom::utils::String::npos ||
                strValue.find('#') != atom::utils::String::npos ||
                (!strValue.empty() &&
                 (strValue.front() == ' ' || strValue.back() == ' '));

            formattedValue = needsQuotes ? "\"" + strValue + "\"" : strValue;

        } else if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
            formattedValue = value ? "true" : "false";
        } else if constexpr (Number<T>) {
            if constexpr (std::is_floating_point_v<T>) {
                if (std::isnan(value))
                    formattedValue = ".nan";
                else if (std::isinf(value))
                    formattedValue = (value > 0 ? ".inf" : "-.inf");
                else
                    formattedValue = atom::utils::String(std::format(
                        "{:.12g}", value));  // Convert format result
            } else {
                formattedValue = atom::utils::String(
                    std::to_string(value));  // Convert to_string result
            }
        } else if constexpr (Pointer<T> || SmartPointer<T>) {
            if (value == nullptr) [[unlikely]] {
                formattedValue = "null";
            } else [[likely]] {
                // Recursive call, need to handle key/indentation properly
                // This base function should return only the value part for
                // recursion
                return toYaml(*value,
                              key);  // Let the caller handle key/indentation
            }
        } else if constexpr (requires { value.toYaml(key); }) {
            // Support for custom types with toYaml method
            // Assume custom method returns the full "key: value\n" line or just
            // value
            return value.toYaml(key);  // Rely on custom implementation
        } else {
            formattedValue = "null";
        }

        // Construct the final line "key: value\n" or just "value" if key is
        // empty
        if (key.empty()) {
            return formattedValue;  // Return only value if key is empty (for
                                    // lists etc.)
        } else {
            return key + ": " + formattedValue + "\n";
        }

    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           (key.empty() ? "value" : key.c_str()))
            .c_str();
    }
}

// Improved tuple toYaml with better type handling
template <typename... Ts>
[[nodiscard]] auto toYaml(const std::tuple<Ts...> &tuple,
                          const atom::utils::String &key)
    -> atom::utils::String {
    try {
        atom::utils::String result = key.empty() ? "" : key + ":\n";
        std::apply(
            [&result, &key](const Ts &...args) {
                // Pass empty key to nested calls, handle list format here
                ((result +=
                  (key.empty() ? "- " : "  - ") + toYaml(args, "") + "\n"),
                 ...);
            },
            tuple);
        // Remove trailing newline if added by the loop
        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
        result += "\n";  // Ensure single trailing newline

        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           (key.empty() ? "tuple" : key.c_str()))
            .c_str();
    }
}

// Forward declaration for toToml
template <typename T>
[[nodiscard]] auto toToml(const T &value, const atom::utils::String &key)
    -> atom::utils::String;

// Optimized toToml for containers
template <std::ranges::input_range Container>
[[nodiscard]] auto toToml(const Container &container,
                          const atom::utils::String &key)
    -> atom::utils::String {
    // Basic TOML array implementation
    try {
        if (key.empty()) {
            throw std::invalid_argument("TOML arrays require a key");
        }
        atom::utils::String result = key + " = [\n";
        bool first = true;
        for (const auto &item : container) {
            if (!first) {
                result += ",\n";
            }
            // Pass empty key to nested toToml, assuming it returns just the
            // value representation
            result += "  " + toToml(item, "");
            first = false;
        }
        result += "\n]\n";
        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n# {}: null\n", e.what(),
                           (key.empty() ? "container" : key.c_str()))
            .c_str();
    }
}

// Improved map toToml with validation (using HashMap)
template <typename K, typename V>
[[nodiscard]] auto toToml(const atom::utils::HashMap<K, V> &map,
                          const atom::utils::String &key)
    -> atom::utils::String {
    // TOML requires maps (tables) to be structured differently.
    // A simple key-value pair list isn't standard TOML for nested tables.
    // This implementation provides a basic inline table representation.
    // For complex nested structures, a full TOML library approach is better.
    try {
        if (map.empty()) {
            return key.empty() ? "{}\n" : key + " = {}\n";
        }

        atom::utils::String result = key.empty() ? "" : key + " = { ";
        bool first = true;

        for (const auto &pair : map) {
            if (!first) {
                result += ", ";
            }
            atom::utils::String keyStr;
            // Assuming String concept works with atom::containers::String
            if constexpr (String<K> || Char<K>) {
                atom::utils::String k = atom::utils::String(pair.first);
                // Basic TOML key quoting (only if necessary, prefer bare keys)
                // TOML bare key rules are stricter than YAML/JSON
                bool needsQuotes =
                    k.empty() ||
                    k.find_first_not_of(
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01"
                        "23456789_-") != atom::utils::String::npos;
                keyStr = needsQuotes ? "\"" + k + "\"" : k;  // Basic quoting
            } else {
                keyStr = toString(
                    pair.first);  // Fallback, might not be valid TOML key
                // Ensure keyStr is quoted if it contains invalid bare key chars
                if (keyStr.find_first_not_of(
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01"
                        "23456789_-") != atom::utils::String::npos) {
                    keyStr = "\"" + keyStr + "\"";
                }
            }

            // Pass empty key to nested toToml, expecting just the value
            // representation
            result += keyStr + " = " + toToml(pair.second, "");
            first = false;
        }

        result += key.empty() ? "\n" : " }\n";  // Close inline table
        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n# {}: null\n", e.what(),
                           (key.empty() ? "map" : key.c_str()))
            .c_str();
    }
}

// Improved pair toToml with validation
template <typename T1, typename T2>
[[nodiscard]] auto toToml(const std::pair<T1, T2> &pair,
                          const atom::utils::String &key)
    -> atom::utils::String {
    // Represent pair as an inline table in TOML: key = { key = val1, value =
    // val2 }
    try {
        if (key.empty()) {
            throw std::invalid_argument(
                "TOML requires a key for pair representation");
        }
        atom::utils::String result = key + " = { ";
        // Pass empty key to nested calls, expecting value representation
        result += "key = " + toToml(pair.first, "");
        result += ", value = " + toToml(pair.second, "");
        result += " }\n";
        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n# {}: null\n", e.what(),
                           (key.empty() ? "pair" : key.c_str()))
            .c_str();
    }
}

// Base toToml implementation with improved type handling and validation
template <typename T>
[[nodiscard]] auto toToml(const T &value, const atom::utils::String &key)
    -> atom::utils::String {
    // This base function should return the TOML representation of the *value*
    // only. The key assignment (key = value) is handled by the calling function
    // (container/map/pair serializers).
    try {
        atom::utils::String formattedValue;
        // Assuming String concept works with atom::containers::String
        if constexpr (String<T> || Char<T>) {
            atom::utils::String strValue = atom::utils::String(value);
            // Basic TOML string quoting (prefer basic strings)
            // More complex escaping (multi-line, literal strings) not handled
            // here.
            bool needsQuotes = true;  // Always quote for simplicity here
            if (needsQuotes) {
                // Basic escaping for TOML basic strings
                std::string std_str(strValue.begin(), strValue.end());
                std_str =
                    std::regex_replace(std_str, std::regex("\\\\"), "\\\\");
                std_str = std::regex_replace(std_str, std::regex("\""), "\\\"");
                std_str = std::regex_replace(std_str, std::regex("\b"), "\\b");
                std_str = std::regex_replace(std_str, std::regex("\t"), "\\t");
                std_str = std::regex_replace(std_str, std::regex("\n"), "\\n");
                std_str = std::regex_replace(std_str, std::regex("\f"), "\\f");
                std_str = std::regex_replace(std_str, std::regex("\r"), "\\r");
                formattedValue =
                    "\"" + atom::utils::String(std_str.c_str()) + "\"";
            } else {
                formattedValue = strValue;  // Should ideally check if it's a
                                            // valid bare string
            }

        } else if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
            formattedValue = value ? "true" : "false";
        } else if constexpr (Number<T>) {
            if constexpr (std::is_floating_point_v<T>) {
                // TOML supports special float values
                if (std::isnan(value))
                    formattedValue = "nan";
                else if (std::isinf(value))
                    formattedValue = (value > 0 ? "inf" : "-inf");
                else
                    formattedValue = atom::utils::String(std::format(
                        "{:.12g}", value));  // Convert format result
            } else {
                formattedValue = atom::utils::String(
                    std::to_string(value));  // Convert to_string result
            }
        } else if constexpr (Pointer<T> || SmartPointer<T>) {
            // TOML doesn't have a standard null representation. Error or omit?
            // Omitting the key-value pair is common. Returning empty might
            // signal omission.
            if (value == nullptr) [[unlikely]] {
                throw std::runtime_error(
                    "Cannot represent nullptr directly in TOML value");
                // Or return an empty string to signal omission to the caller:
                // return "";
            } else [[likely]] {
                return toToml(*value,
                              "");  // Recursive call, get value representation
            }
        } else if constexpr (requires { value.toToml(""); }) {
            // Support for custom types with toToml method
            // Assume custom method returns the value representation when key is
            // empty
            return value.toToml("");
        } else {
            throw std::runtime_error(
                "Cannot represent unknown type in TOML value");
            // Or return empty: return "";
        }

        // If a key was provided (shouldn't happen in this base value function
        // ideally), format as key = value
        if (!key.empty()) {
            return key + " = " + formattedValue + "\n";
        } else {
            return formattedValue;  // Return only the value representation
        }

    } catch (const std::exception &e) {
        // Return error comment or rethrow
        return std::format("# Error: {}\n# {}: error\n", e.what(),
                           (key.empty() ? "value" : key.c_str()))
            .c_str();
    }
}

// Improved tuple toToml with better type handling
template <typename... Ts>
[[nodiscard]] auto toToml(const std::tuple<Ts...> &tuple,
                          const atom::utils::String &key)
    -> atom::utils::String {
    // Represent tuple as a TOML array
    try {
        if (key.empty()) {
            throw std::invalid_argument(
                "TOML arrays require a key for tuple representation");
        }
        atom::utils::String result = key + " = [\n";
        bool first = true;
        std::apply(
            [&result, &first](const Ts &...args) {
                // Pass empty key to nested calls, expecting value
                // representation
                auto process_arg = [&](const auto &arg) {
                    if (!first) {
                        result += ",\n";
                    }
                    result += "  " + toToml(arg, "");
                    first = false;
                };
                (process_arg(args), ...);  // Fold expression
            },
            tuple);
        result += "\n]\n";
        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n# {}: null\n", e.what(),
                           (key.empty() ? "tuple" : key.c_str()))
            .c_str();
    }
}

#endif  // ATOM_EXPERIMENT_ANYUTILS_HPP
