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
#include <regex>
#include <mutex>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include "atom/meta/concept.hpp"

// Enhanced version of concepts with better constraints
template <typename T>
concept CanBeStringified = requires(T t) {
    { toString(t) } -> std::convertible_to<std::string>;
};

template <typename T>
concept CanBeStringifiedToJson = requires(T t) {
    { toJson(t) } -> std::convertible_to<std::string>;
};

// New concept for XML serialization
template <typename T>
concept CanBeStringifiedToXml = requires(T t, std::string tag) {
    { toXml(t, tag) } -> std::convertible_to<std::string>;
};

// New concept for YAML serialization
template <typename T>
concept CanBeStringifiedToYaml = requires(T t, std::string key) {
    { toYaml(t, key) } -> std::convertible_to<std::string>;
};

// New concept for TOML serialization
template <typename T>
concept CanBeStringifiedToToml = requires(T t, std::string key) {
    { toToml(t, key) } -> std::convertible_to<std::string>;
};

// Forward declarations
template <typename T>
[[nodiscard]] auto toString(const T &value,
                            bool prettyPrint = false) -> std::string;

// Thread-safe string conversion cache using mutex
namespace {
std::mutex cacheMutex;
std::unordered_map<std::size_t, std::string> conversionCache;

template <typename T>
std::size_t getTypeHash(const T &value) {
    std::size_t typeHash = typeid(T).hash_code();
    std::size_t valueHash = 0;
    if constexpr (std::is_trivially_copyable_v<T> &&
                  sizeof(T) <= sizeof(std::size_t)) {
        std::memcpy(&valueHash, &value, sizeof(T));
    }
    return typeHash ^ valueHash;
}

template <typename T>
std::optional<std::string> getCachedString(const T &value) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    auto hash = getTypeHash(value);
    auto it = conversionCache.find(hash);
    if (it != conversionCache.end()) {
        return it->second;
    }
    return std::nullopt;
}

template <typename T>
void cacheString(const T &value, const std::string &str) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    auto hash = getTypeHash(value);
    conversionCache[hash] = str;
}
}  // namespace

// Improved ranges-based toString with validation and exception handling
template <std::ranges::input_range Container>
[[nodiscard]] auto toString(const Container &container,
                            bool prettyPrint = false) -> std::string {
    try {
        if (std::ranges::empty(container)) {
            return "[]";
        }

        // Try to get from cache first
        if (auto cached = getCachedString(container)) {
            return *cached;
        }

        std::string result = "[";
        const std::string separator = prettyPrint ? ", " : ",";
        const std::string indent = prettyPrint ? "\n  " : "";

        if (prettyPrint)
            result += indent;

        for (const auto &item : container) {
            if constexpr (IsBuiltIn<std::remove_cvref_t<decltype(item)>>) {
                result += toString(item, prettyPrint) + separator +
                          (prettyPrint ? indent : "");
            } else {
                result += "\"" + toString(item, prettyPrint) + "\"" +
                          separator + (prettyPrint ? indent : "");
            }
        }

        // Remove the last separator
        if (prettyPrint) {
            result.erase(result.length() - indent.length() - separator.length(),
                         separator.length() + indent.length());
            result += "\n]";
        } else {
            result.erase(result.length() - separator.length(),
                         separator.length());
            result += "]";
        }

        // Cache result for future use (only if container is small enough)
        if (std::ranges::distance(container) < 1000) {
            cacheString(container, result);
        }

        return result;
    } catch (const std::exception &e) {
        return std::format("Error converting container to string: {}",
                           e.what());
    }
}

// Improved map toString with validation
template <typename K, typename V>
[[nodiscard]] auto toString(const std::unordered_map<K, V> &map,
                            bool prettyPrint = false) -> std::string {
    try {
        if (map.empty()) {
            return "{}";
        }

        std::string result = "{";
        const std::string separator = prettyPrint ? ",\n  " : ", ";

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
            result.erase(result.length() - 2, 2);
            result += "}";
        }

        return result;
    } catch (const std::exception &e) {
        return std::format("Error converting map to string: {}", e.what());
    }
}

// Improved pair toString with better exception handling
template <typename T1, typename T2>
[[nodiscard]] auto toString(const std::pair<T1, T2> &pair,
                            bool prettyPrint = false) -> std::string {
    try {
        return "(" + toString(pair.first, prettyPrint) + ", " +
               toString(pair.second, prettyPrint) + ")";
    } catch (const std::exception &e) {
        return std::format("Error converting pair to string: {}", e.what());
    }
}

// Base toString implementation with improved type handling
template <typename T>
[[nodiscard]] auto toString(const T &value, bool prettyPrint) -> std::string {
    try {
        if constexpr (String<T> || Char<T>) {
            return std::string(value);
        } else if constexpr (std::is_same_v<T, bool>) {
            return value ? "true" : "false";
        } else if constexpr (Number<T>) {
            if constexpr (std::is_floating_point_v<T>) {
                // For floating point, use format with precision
                return std::format("{:.6g}", value);
            } else {
                return std::to_string(value);
            }
        } else if constexpr (Pointer<T> || SmartPointer<T>) {
            if (value == nullptr) {
                return "nullptr";
            }
            return toString(*value, prettyPrint);
        } else if constexpr (requires { value.toString(); }) {
            // Support for custom types with toString method
            return value.toString();
        } else {
            return "unknown type";
        }
    } catch (const std::exception &e) {
        return std::format("Error in toString: {}", e.what());
    }
}

// Forward declaration for toJson
template <typename T>
[[nodiscard]] auto toJson(const T &value,
                          bool prettyPrint = false) -> std::string;

// Parallelized toJson for large containers
template <std::ranges::input_range Container>
[[nodiscard]] auto toJson(const Container &container,
                          bool prettyPrint = false) -> std::string {
    try {
        if (std::ranges::empty(container)) {
            return "[]";
        }

        const std::string indent = prettyPrint ? "  " : "";
        const std::string nl = prettyPrint ? "\n" : "";
        std::string result = "[" + nl;

        // Use parallel execution for large containers
        if (std::ranges::distance(container) > 1000) {
            std::vector<std::future<std::string>> futures;
            futures.reserve(std::ranges::distance(container));

            // Process items in parallel
            for (const auto &item : container) {
                futures.push_back(std::async(
                    std::launch::async, [&item, prettyPrint]() -> std::string {
                        return toJson(item, prettyPrint);
                    }));
            }

            // Collect results
            for (auto &fut : futures) {
                result += (prettyPrint ? indent : "") + fut.get() + "," + nl;
            }
        } else {
            // Sequential processing for smaller containers
            for (const auto &item : container) {
                result += (prettyPrint ? indent : "") +
                          toJson(item, prettyPrint) + "," + nl;
            }
        }

        if (!container.empty()) {
            result.erase(result.length() - (1 + nl.length()), 1 + nl.length());
        }

        result += nl + "]";
        return result;
    } catch (const std::exception &e) {
        return std::format("{{\"error\": \"Error converting to JSON: {}\"}}",
                           e.what());
    }
}

// Improved map toJson with validation
template <typename K, typename V>
[[nodiscard]] auto toJson(const std::unordered_map<K, V> &map,
                          bool prettyPrint = false) -> std::string {
    try {
        if (map.empty()) {
            return "{}";
        }

        const std::string indent = prettyPrint ? "  " : "";
        const std::string nl = prettyPrint ? "\n" : "";
        std::string result = "{" + nl;

        for (const auto &pair : map) {
            std::string key;
            if constexpr (std::is_same_v<K, std::string> ||
                          std::is_same_v<K, const char *>) {
                key = "\"" + std::string(pair.first) + "\"";
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
            "{{\"error\": \"Error converting map to JSON: {}\"}}", e.what());
    }
}

// Improved pair toJson with validation
template <typename T1, typename T2>
[[nodiscard]] auto toJson(const std::pair<T1, T2> &pair,
                          bool prettyPrint = false) -> std::string {
    try {
        const std::string nl = prettyPrint ? "\n" : "";
        const std::string indent = prettyPrint ? "  " : "";

        std::string result = "{" + nl;
        result += (prettyPrint ? indent : "") +
                  "\"first\": " + toJson(pair.first, prettyPrint) + "," + nl;
        result += (prettyPrint ? indent : "") +
                  "\"second\": " + toJson(pair.second, prettyPrint) + nl;
        result += "}";

        return result;
    } catch (const std::exception &e) {
        return std::format(
            "{{\"error\": \"Error converting pair to JSON: {}\"}}", e.what());
    }
}

// Base toJson implementation with improved type handling and validation
template <typename T>
[[nodiscard]] auto toJson(const T &value, bool prettyPrint) -> std::string {
    try {
        if constexpr (String<T>) {
            // Escape special characters in strings
            std::string escaped;
            escaped.reserve(
                std::string(value).size() +
                10);  // Reserve extra space for potential escape chars

            for (char c : std::string(value)) {
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
                            escaped += std::format(
                                "\\u{:04x}", static_cast<unsigned int>(c));
                        } else {
                            escaped += c;
                        }
                }
            }

            return "\"" + escaped + "\"";
        } else if constexpr (Char<T>) {
            if (static_cast<unsigned char>(value) < 32) {
                return std::format("\"\\u{:04x}\"",
                                   static_cast<unsigned int>(value));
            }
            return "\"" + std::string(1, value) + "\"";
        } else if constexpr (std::is_same_v<T, bool>) {
            return value ? "true" : "false";
        } else if constexpr (Number<T>) {
            if constexpr (std::is_floating_point_v<T>) {
                // Handle special float values
                if (std::isnan(value))
                    return "null";
                if (std::isinf(value))
                    return value > 0 ? "null"
                                     : "null";  // JSON doesn't support infinity
                return std::format("{:.12g}",
                                   value);  // More precision for floating point
            } else {
                return std::to_string(value);
            }
        } else if constexpr (Pointer<T> || SmartPointer<T>) {
            if (value == nullptr) {
                return "null";
            }
            return toJson(*value, prettyPrint);
        } else if constexpr (requires { value.toJson(); }) {
            // Support for custom types with toJson method
            return value.toJson();
        } else {
            return "{}";
        }
    } catch (const std::exception &e) {
        return std::format("{{\"error\": \"Error in toJson: {}\"}}", e.what());
    }
}

// Forward declaration for toXml
template <typename T>
[[nodiscard]] auto toXml(const T &value,
                         const std::string &tagName) -> std::string;

// Optimized toXml for containers with validation
template <std::ranges::input_range Container>
[[nodiscard]] auto toXml(const Container &container,
                         const std::string &tagName) -> std::string {
    try {
        // Validate tag name
        if (tagName.find('<') != std::string::npos ||
            tagName.find('>') != std::string::npos) {
            throw std::invalid_argument(
                "XML tag name contains invalid characters");
        }

        const std::string containerTag = tagName.empty() ? "items" : tagName;
        std::string result = "<" + containerTag + ">\n";

        // For large containers, process items in batches
        const size_t batchSize = 50;
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
            std::string batchResult;
            std::for_each(std::execution::par_unseq, it, batchEnd,
                          [&batchResult, &tagName](const auto &item) {
                              const std::string itemTag =
                                  tagName.empty() ? "item" : tagName + "_item";
                              std::string itemResult = toXml(item, itemTag);

                              // Thread-safe append to batch result
                              std::lock_guard<std::mutex> lock(cacheMutex);
                              batchResult += itemResult;
                          });

            result += batchResult;
            it = batchEnd;
        }

        result += "</" + containerTag + ">";
        return result;
    } catch (const std::exception &e) {
        return std::format("<error>Error converting to XML: {}</error>",
                           e.what());
    }
}

// Improved map toXml with validation
template <typename K, typename V>
[[nodiscard]] auto toXml(const std::unordered_map<K, V> &map,
                         [[maybe_unused]] const std::string &tagName)
    -> std::string {
    try {
        // Validate tag name
        if (!tagName.empty() && (tagName.find('<') != std::string::npos ||
                                 tagName.find('>') != std::string::npos)) {
            throw std::invalid_argument(
                "XML tag name contains invalid characters");
        }

        const std::string mapTag = tagName.empty() ? "map" : tagName;
        std::string result = "<" + mapTag + ">\n";

        for (const auto &pair : map) {
            std::string keyStr = toString(pair.first);
            // Sanitize key for XML tag
            std::replace(keyStr.begin(), keyStr.end(), ' ', '_');
            std::replace(keyStr.begin(), keyStr.end(), '<', '_');
            std::replace(keyStr.begin(), keyStr.end(), '>', '_');
            std::replace(keyStr.begin(), keyStr.end(), '&', '_');

            result += toXml(pair.second, keyStr);
        }

        result += "</" + mapTag + ">";
        return result;
    } catch (const std::exception &e) {
        return std::format("<error>Error converting map to XML: {}</error>",
                           e.what());
    }
}

// Improved pair toXml with validation
template <typename T1, typename T2>
[[nodiscard]] auto toXml(const std::pair<T1, T2> &pair,
                         const std::string &tagName) -> std::string {
    try {
        // Validate tag name
        if (tagName.find('<') != std::string::npos ||
            tagName.find('>') != std::string::npos) {
            throw std::invalid_argument(
                "XML tag name contains invalid characters");
        }

        const std::string pairTag = tagName.empty() ? "pair" : tagName;
        std::string result = "<" + pairTag + ">\n";
        result += toXml(pair.first, "key");
        result += toXml(pair.second, "value");
        result += "</" + pairTag + ">";
        return result;
    } catch (const std::exception &e) {
        return std::format("<error>Error converting pair to XML: {}</error>",
                           e.what());
    }
}

// Base toXml implementation with improved type handling and validation
template <typename T>
[[nodiscard]] auto toXml(const T &value,
                         const std::string &tagName) -> std::string {
    try {
        // Validate tag name
        if (tagName.empty()) {
            throw std::invalid_argument("XML tag name cannot be empty");
        }

        if (tagName.find('<') != std::string::npos ||
            tagName.find('>') != std::string::npos) {
            throw std::invalid_argument(
                "XML tag name contains invalid characters");
        }

        if constexpr (String<T> || Char<T>) {
            std::string content = std::string(value);
            // Escape XML special characters
            content = std::regex_replace(content, std::regex("&"), "&amp;");
            content = std::regex_replace(content, std::regex("<"), "&lt;");
            content = std::regex_replace(content, std::regex(">"), "&gt;");
            content = std::regex_replace(content, std::regex("\""), "&quot;");
            content = std::regex_replace(content, std::regex("'"), "&apos;");

            return "<" + tagName + ">" + content + "</" + tagName + ">";
        } else if constexpr (Number<T>) {
            return "<" + tagName + ">" + std::to_string(value) + "</" +
                   tagName + ">";
        } else if constexpr (Pointer<T> || SmartPointer<T>) {
            if (value == nullptr) [[unlikely]] {
                return "<" + tagName + " nil=\"true\"/>";
            } else [[likely]] {
                return toXml(*value, tagName);
            }
        } else if constexpr (requires { value.toXml(tagName); }) {
            // Support for custom types with toXml method
            return value.toXml(tagName);
        } else {
            return "<" + tagName + "/>";
        }
    } catch (const std::exception &e) {
        return std::format("<error>Error in toXml: {}</error>", e.what());
    }
}

// Forward declaration for toYaml
template <typename T>
[[nodiscard]] auto toYaml(const T &value,
                          const std::string &key) -> std::string;

// Improved toYaml for containers with validation and performance
template <std::ranges::input_range Container>
[[nodiscard]] auto toYaml(const Container &container,
                          const std::string &key) -> std::string {
    try {
        std::string result = key.empty() ? "" : key + ":\n";

        if (std::ranges::empty(container)) {
            return key.empty() ? "[]" : key + ": []\n";
        }

        // For large containers, process in parallel
        if (std::ranges::distance(container) > 100) {
            std::vector<std::pair<size_t, std::string>> results;
            results.resize(std::ranges::distance(container));

            size_t index = 0;
            for (const auto &item : container) {
                results[index].first = index;
                ++index;
            }

            // Process items in parallel
            std::for_each(std::execution::par_unseq, results.begin(),
                          results.end(), [&container, &key](auto &pair) {
                              auto it = std::ranges::begin(container);
                              std::advance(it, pair.first);
                              pair.second = (key.empty() ? "- " : "  - ") +
                                            toYaml(*it, "") + "\n";
                          });

            // Combine results in order
            for (const auto &pair : results) {
                result += pair.second;
            }
        } else {
            // Process sequentially for smaller containers
            for (const auto &item : container) {
                result +=
                    (key.empty() ? "- " : "  - ") + toYaml(item, "") + "\n";
            }
        }

        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           key.empty() ? "items" : key);
    }
}

// Improved map toYaml with validation
template <typename K, typename V>
[[nodiscard]] auto toYaml(const std::unordered_map<K, V> &map,
                          const std::string &key) -> std::string {
    try {
        if (map.empty()) {
            return key.empty() ? "{}\n" : key + ": {}\n";
        }

        std::string result = key.empty() ? "" : key + ":\n";

        for (const auto &pair : map) {
            std::string keyStr;
            if constexpr (String<K> || Char<K>) {
                // For string keys, ensure proper YAML formatting
                std::string k = std::string(pair.first);
                if (k.find(':') != std::string::npos ||
                    k.find('#') != std::string::npos ||
                    k.find('\n') != std::string::npos || k.empty()) {
                    keyStr = "\"" + k + "\"";
                } else {
                    keyStr = k;
                }
            } else {
                keyStr = toString(pair.first);
            }

            result += (key.empty() ? "" : "  ") + toYaml(pair.second, keyStr);
        }

        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           key.empty() ? "map" : key);
    }
}

// Improved pair toYaml with validation
template <typename T1, typename T2>
[[nodiscard]] auto toYaml(const std::pair<T1, T2> &pair,
                          const std::string &key) -> std::string {
    try {
        std::string result = key.empty() ? "" : key + ":\n";
        result += std::string((key.empty() ? "" : "  ")) +
                  "key: " + toYaml(pair.first, "");
        result += std::string((key.empty() ? "" : "  ")) +
                  "value: " + toYaml(pair.second, "");
        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           key.empty() ? "pair" : key);
    }
}

// Base toYaml implementation with improved type handling and validation
template <typename T>
[[nodiscard]] auto toYaml(const T &value,
                          const std::string &key) -> std::string {
    try {
        if constexpr (String<T> || Char<T>) {
            std::string strValue = std::string(value);

            // Check if special formatting is needed
            bool needsQuotes =
                strValue.empty() || strValue.find('\n') != std::string::npos ||
                strValue.find(':') != std::string::npos ||
                strValue.find('#') != std::string::npos ||
                (strValue.front() == ' ' || strValue.back() == ' ');

            std::string formattedValue =
                needsQuotes ? "\"" + strValue + "\"" : strValue;

            return key.empty() ? formattedValue
                               : key + ": " + formattedValue + "\n";
        } else if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
            return key.empty() ? (value ? "true" : "false")
                               : key + ": " + (value ? "true" : "false") + "\n";
        } else if constexpr (Number<T>) {
            if constexpr (std::is_floating_point_v<T>) {
                // Handle special float values
                if (std::isnan(value)) {
                    return key.empty() ? ".nan" : key + ": .nan\n";
                }
                if (std::isinf(value)) {
                    return key.empty()
                               ? (value > 0 ? ".inf" : "-.inf")
                               : key + ": " + (value > 0 ? ".inf" : "-.inf") +
                                     "\n";
                }
                // Format with precision
                std::string numStr = std::format("{:.12g}", value);
                return key.empty() ? numStr : key + ": " + numStr + "\n";
            } else {
                return key.empty() ? std::to_string(value)
                                   : key + ": " + std::to_string(value) + "\n";
            }
        } else if constexpr (Pointer<T> || SmartPointer<T>) {
            if (value == nullptr) [[unlikely]] {
                return key.empty() ? "null" : key + ": null\n";
            } else [[likely]] {
                return toYaml(*value, key);
            }
        } else if constexpr (requires { value.toYaml(key); }) {
            // Support for custom types with toYaml method
            return value.toYaml(key);
        } else {
            return key.empty() ? "null" : key + ": null\n";
        }
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           key.empty() ? "value" : key);
    }
}

// Improved tuple toYaml with better type handling
template <typename... Ts>
[[nodiscard]] auto toYaml(const std::tuple<Ts...> &tuple,
                          const std::string &key) -> std::string {
    try {
        std::string result = key.empty() ? "" : key + ":\n";
        std::apply(
            [&result](const Ts &...args) {
                ((result += "- " + toYaml(args, "") + "\n"), ...);
            },
            tuple);
        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           key.empty() ? "tuple" : key);
    }
}

// Forward declaration for toToml
template <typename T>
[[nodiscard]] auto toToml(const T &value,
                          const std::string &key) -> std::string;

// Optimized toToml for containers
template <std::ranges::input_range Container>
[[nodiscard]] auto toToml(const Container &container,
                          const std::string &key) -> std::string {
    try {
        std::string result = key + " = [\n";
        for (const auto &item : container) {
            result += "  " + toToml(item, "") + ",\n";
        }
        if (!container.empty()) {
            result.erase(result.length() - 2, 1);  // Remove the last comma
        }
        result += "]\n";
        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           key.empty() ? "container" : key);
    }
}

// Improved map toToml with validation
template <typename K, typename V>
[[nodiscard]] auto toToml(const std::unordered_map<K, V> &map,
                          const std::string &key) -> std::string {
    try {
        if (map.empty()) {
            return key.empty() ? "{}\n" : key + ": {}\n";
        }

        std::string result = key.empty() ? "" : key + ":\n";

        for (const auto &pair : map) {
            std::string keyStr;
            if constexpr (String<K> || Char<K>) {
                // For string keys, ensure proper YAML formatting
                std::string k = std::string(pair.first);
                if (k.find(':') != std::string::npos ||
                    k.find('#') != std::string::npos ||
                    k.find('\n') != std::string::npos || k.empty()) {
                    keyStr = "\"" + k + "\"";
                } else {
                    keyStr = k;
                }
            } else {
                keyStr = toString(pair.first);
            }

            result += (key.empty() ? "" : "  ") + toToml(pair.second, keyStr);
        }

        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           key.empty() ? "map" : key);
    }
}

// Improved pair toToml with validation
template <typename T1, typename T2>
[[nodiscard]] auto toToml(const std::pair<T1, T2> &pair,
                          const std::string &key) -> std::string {
    try {
        std::string result = key.empty() ? "" : key + ":\n";
        result += std::string((key.empty() ? "" : "  ")) +
                  "key: " + toToml(pair.first, "");
        result += std::string((key.empty() ? "" : "  ")) +
                  "value: " + toToml(pair.second, "");
        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           key.empty() ? "pair" : key);
    }
}

// Base toToml implementation with improved type handling and validation
template <typename T>
[[nodiscard]] auto toToml(const T &value,
                          const std::string &key) -> std::string {
    try {
        if constexpr (String<T> || Char<T>) {
            std::string strValue = std::string(value);

            // Check if special formatting is needed
            bool needsQuotes =
                strValue.empty() || strValue.find('\n') != std::string::npos ||
                strValue.find(':') != std::string::npos ||
                strValue.find('#') != std::string::npos ||
                (strValue.front() == ' ' || strValue.back() == ' ');

            std::string formattedValue =
                needsQuotes ? "\"" + strValue + "\"" : strValue;

            return key.empty() ? formattedValue
                               : key + ": " + formattedValue + "\n";
        } else if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
            return key.empty() ? (value ? "true" : "false")
                               : key + ": " + (value ? "true" : "false") + "\n";
        } else if constexpr (Number<T>) {
            if constexpr (std::is_floating_point_v<T>) {
                // Handle special float values
                if (std::isnan(value)) {
                    return key.empty() ? ".nan" : key + ": .nan\n";
                }
                if (std::isinf(value)) {
                    return key.empty()
                               ? (value > 0 ? ".inf" : "-.inf")
                               : key + ": " + (value > 0 ? ".inf" : "-.inf") +
                                     "\n";
                }
                // Format with precision
                std::string numStr = std::format("{:.12g}", value);
                return key.empty() ? numStr : key + ": " + numStr + "\n";
            } else {
                return key.empty() ? std::to_string(value)
                                   : key + ": " + std::to_string(value) + "\n";
            }
        } else if constexpr (Pointer<T> || SmartPointer<T>) {
            if (value == nullptr) [[unlikely]] {
                return key.empty() ? "null" : key + ": null\n";
            } else [[likely]] {
                return toToml(*value, key);
            }
        } else if constexpr (requires { value.toToml(key); }) {
            // Support for custom types with toToml method
            return value.toToml(key);
        } else {
            return key.empty() ? "null" : key + ": null\n";
        }
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           key.empty() ? "value" : key);
    }
}

// Improved tuple toToml with better type handling
template <typename... Ts>
[[nodiscard]] auto toToml(const std::tuple<Ts...> &tuple,
                          const std::string &key) -> std::string {
    try {
        std::string result = key.empty() ? "" : key + ":\n";
        std::apply(
            [&result](const Ts &...args) {
                ((result += "- " + toToml(args, "") + "\n"), ...);
            },
            tuple);
        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           key.empty() ? "tuple" : key);
    }
}

#endif  // ATOM_EXPERIMENT_ANYUTILS_HPP
