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
#include <cmath>
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
#include "atom/containers/high_performance.hpp"
#include "atom/meta/concept.hpp"

namespace atom::utils {
using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;
template <typename T>
using Vector = atom::containers::Vector<T>;
}  // namespace atom::utils

// Define missing concepts using the ones from concept.hpp
template <typename T>
concept IsBuiltIn = atom::meta::IsBuiltIn<T>;

template <typename T>
concept String = atom::meta::StringType<T>;

template <typename T>
concept Char = atom::meta::AnyChar<T>;

template <typename T>
concept Number = atom::meta::Number<T>;

template <typename T>
concept Pointer = atom::meta::Pointer<T>;

template <typename T>
concept SmartPointer = atom::meta::SmartPointer<T>;

template <typename T>
concept CanBeStringified = requires(T t) {
    { toString(t) } -> std::convertible_to<atom::utils::String>;
};

template <typename T>
concept CanBeStringifiedToJson = requires(T t) {
    { toJson(t) } -> std::convertible_to<atom::utils::String>;
};

template <typename T>
concept CanBeStringifiedToXml = requires(T t, atom::utils::String tag) {
    { toXml(t, tag) } -> std::convertible_to<atom::utils::String>;
};

template <typename T>
concept CanBeStringifiedToYaml = requires(T t, atom::utils::String key) {
    { toYaml(t, key) } -> std::convertible_to<atom::utils::String>;
};

template <typename T>
concept CanBeStringifiedToToml = requires(T t, atom::utils::String key) {
    { toToml(t, key) } -> std::convertible_to<atom::utils::String>;
};

template <typename T>
[[nodiscard]] auto toString(const T &value, bool prettyPrint = false)
    -> atom::utils::String;

namespace {
std::mutex cacheMutex;
atom::utils::HashMap<std::size_t, atom::utils::String> conversionCache;

template <typename T>
std::size_t getTypeHash(const T &value) {
    std::size_t typeHash = typeid(T).hash_code();
    std::size_t valueHash = 0;
    if constexpr (std::is_trivially_copyable_v<T> &&
                  sizeof(T) <= sizeof(std::size_t)) {
        std::memcpy(&valueHash, &value, sizeof(T));
    }
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

template <std::ranges::input_range Container>
[[nodiscard]] auto toString(const Container &container,
                            bool prettyPrint = false) -> atom::utils::String {
    try {
        if (std::ranges::empty(container)) {
            return "[]";
        }

        if (auto cached = getCachedString(container)) {
            return *cached;
        }

        atom::utils::String result = "[";
        const atom::utils::String separator = prettyPrint ? ", " : ",";
        const atom::utils::String indent = prettyPrint ? "\n  " : "";

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

        if (prettyPrint) {
            result.erase(result.length() - indent.length() - separator.length(),
                         separator.length() + indent.length());
            result += "\n]";
        } else {
            result.erase(result.length() - separator.length(),
                         separator.length());
            result += "]";
        }

        if constexpr (std::ranges::sized_range<Container>) {
            if (std::ranges::size(container) < 1000) {
                cacheString(container, result);
            }
        }

        return result;
    } catch (const std::exception &e) {
        return std::format("Error converting container to string: {}", e.what())
            .c_str();
    }
}

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
        return std::format("Error converting map to string: {}", e.what())
            .c_str();
    }
}

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

template <typename T>
[[nodiscard]] auto toString(const T &value, bool prettyPrint)
    -> atom::utils::String {
    try {
        if constexpr (String<T> || Char<T>) {
            return atom::utils::String(value);
        } else if constexpr (std::is_same_v<T, bool>) {
            return value ? "true" : "false";
        } else if constexpr (Number<T>) {
            if constexpr (std::is_floating_point_v<T>) {
                return atom::utils::String(std::format("{:.6g}", value));
            } else {
                return atom::utils::String(std::to_string(value));
            }
        } else if constexpr (Pointer<T> || SmartPointer<T>) {
            if (value == nullptr) {
                return "nullptr";
            }
            return toString(*value, prettyPrint);
        } else if constexpr (requires { value.toString(); }) {
            return value.toString();
        } else {
            return "unknown type";
        }
    } catch (const std::exception &e) {
        return std::format("Error in toString: {}", e.what()).c_str();
    }
}

template <typename T>
[[nodiscard]] auto toJson(const T &value, bool prettyPrint = false)
    -> atom::utils::String;

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

        std::optional<size_t> containerSize;
        if constexpr (std::ranges::sized_range<Container>) {
            containerSize = std::ranges::size(container);
        }

        if (containerSize && *containerSize > 1000) {
            atom::utils::Vector<std::future<atom::utils::String>> futures;
            futures.reserve(*containerSize);

            for (const auto &item : container) {
                futures.push_back(
                    std::async(std::launch::async,
                               [&item, prettyPrint]() -> atom::utils::String {
                                   return toJson(item, prettyPrint);
                               }));
            }

            for (auto &fut : futures) {
                result += (prettyPrint ? indent : "") + fut.get() + "," + nl;
            }
        } else {
            for (const auto &item : container) {
                result += (prettyPrint ? indent : "") +
                          toJson(item, prettyPrint) + "," + nl;
            }
        }

        bool itemsAdded =
            (containerSize && *containerSize > 0) ||
            (!containerSize && result.length() > (1 + nl.length()));
        if (itemsAdded) {
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
            if constexpr (std::is_same_v<K, atom::utils::String> ||
                          std::is_convertible_v<K, const char *>) {
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

template <typename T>
[[nodiscard]] auto toJson(const T &value, bool prettyPrint)
    -> atom::utils::String {
    try {
        if constexpr (String<T>) {
            atom::utils::String escaped;
            escaped.reserve(atom::utils::String(value).size() + 10);

            for (char c : atom::utils::String(value)) {
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
                return atom::utils::String(std::format(
                    "\"\\u{:04x}\"", static_cast<unsigned int>(value)));
            }
            return "\"" + atom::utils::String(1, value) + "\"";
        } else if constexpr (std::is_same_v<T, bool>) {
            return value ? "true" : "false";
        } else if constexpr (Number<T>) {
            if constexpr (std::is_floating_point_v<T>) {
                if (std::isnan(value))
                    return "null";
                if (std::isinf(value))
                    return "null";
                return atom::utils::String(std::format("{:.12g}", value));
            } else {
                return atom::utils::String(std::to_string(value));
            }
        } else if constexpr (Pointer<T> || SmartPointer<T>) {
            if (value == nullptr) {
                return "null";
            }
            return toJson(*value, prettyPrint);
        } else if constexpr (requires { value.toJson(); }) {
            return value.toJson();
        } else {
            return "{}";
        }
    } catch (const std::exception &e) {
        return std::format("{{\"error\": \"Error in toJson: {}\"}}", e.what())
            .c_str();
    }
}

template <typename T>
[[nodiscard]] auto toXml(const T &value, const atom::utils::String &tagName)
    -> atom::utils::String;

template <std::ranges::input_range Container>
[[nodiscard]] auto toXml(const Container &container,
                         const atom::utils::String &tagName)
    -> atom::utils::String {
    try {
        if (tagName.find('<') != atom::utils::String::npos ||
            tagName.find('>') != atom::utils::String::npos) {
            throw std::invalid_argument(
                "XML tag name contains invalid characters");
        }

        const atom::utils::String containerTag =
            tagName.empty() ? "items" : tagName;
        atom::utils::String result = "<" + containerTag + ">\n";

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

            atom::utils::String batchResult;
            std::mutex batchMutex;

            std::for_each(
                std::execution::par_unseq, it, batchEnd,
                [&batchResult, &tagName, &batchMutex](const auto &item) {
                    const atom::utils::String itemTag =
                        tagName.empty() ? "item" : tagName + "_item";
                    atom::utils::String itemResult = toXml(item, itemTag);

                    std::lock_guard<std::mutex> lock(batchMutex);
                    batchResult += itemResult;
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

template <typename K, typename V>
[[nodiscard]] auto toXml(const atom::utils::HashMap<K, V> &map,
                         [[maybe_unused]] const atom::utils::String &tagName)
    -> atom::utils::String {
    try {
        if (!tagName.empty() &&
            (tagName.find('<') != atom::utils::String::npos ||
             tagName.find('>') != atom::utils::String::npos)) {
            throw std::invalid_argument(
                "XML tag name contains invalid characters");
        }

        const atom::utils::String mapTag = tagName.empty() ? "map" : tagName;
        atom::utils::String result = "<" + mapTag + ">\n";

        for (const auto &pair : map) {
            atom::utils::String keyStr = toString(pair.first);
            std::replace(keyStr.begin(), keyStr.end(), ' ', '_');
            std::replace(keyStr.begin(), keyStr.end(), '<', '_');
            std::replace(keyStr.begin(), keyStr.end(), '>', '_');
            std::replace(keyStr.begin(), keyStr.end(), '&', '_');
            if (keyStr.empty() || !std::isalpha(keyStr[0])) {
                keyStr = "_" + keyStr;
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

template <typename T1, typename T2>
[[nodiscard]] auto toXml(const std::pair<T1, T2> &pair,
                         const atom::utils::String &tagName)
    -> atom::utils::String {
    try {
        if (tagName.find('<') != atom::utils::String::npos ||
            tagName.find('>') != atom::utils::String::npos) {
            throw std::invalid_argument(
                "XML tag name contains invalid characters");
        }

        const atom::utils::String pairTag = tagName.empty() ? "pair" : tagName;
        atom::utils::String result = "<" + pairTag + ">\n";
        result += toXml(pair.first, "key");
        result += toXml(pair.second, "value");
        result += "</" + pairTag + ">";
        return result;
    } catch (const std::exception &e) {
        return std::format("<error>Error converting pair to XML: {}</error>",
                           e.what())
            .c_str();
    }
}

template <typename T>
[[nodiscard]] auto toXml(const T &value, const atom::utils::String &tagName)
    -> atom::utils::String {
    try {
        if (tagName.empty()) {
            throw std::invalid_argument("XML tag name cannot be empty");
        }
        if (tagName.find('<') != atom::utils::String::npos ||
            tagName.find('>') != atom::utils::String::npos) {
            throw std::invalid_argument(
                "XML tag name contains invalid characters");
        }

        if constexpr (String<T> || Char<T>) {
            atom::utils::String content = atom::utils::String(value);
            std::string std_content(content.begin(), content.end());
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
            content = atom::utils::String(std_content.c_str());

            return "<" + tagName + ">" + content + "</" + tagName + ">";
        } else if constexpr (Number<T>) {
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
            return value.toXml(tagName);
        } else {
            return "<" + tagName + "/>";
        }
    } catch (const std::exception &e) {
        return std::format("<error>Error in toXml: {}</error>", e.what())
            .c_str();
    }
}

template <typename T>
[[nodiscard]] auto toYaml(const T &value, const atom::utils::String &key)
    -> atom::utils::String;

template <std::ranges::input_range Container>
[[nodiscard]] auto toYaml(const Container &container,
                          const atom::utils::String &key)
    -> atom::utils::String {
    try {
        atom::utils::String result = key.empty() ? "" : key + ":\n";

        if (std::ranges::empty(container)) {
            return key.empty() ? "[]" : key + ": []\n";
        }

        std::optional<size_t> containerSize;
        if constexpr (std::ranges::sized_range<Container>) {
            containerSize = std::ranges::size(container);
        }

        if (containerSize && *containerSize > 100) {
            atom::utils::Vector<std::pair<size_t, atom::utils::String>> results;
            results.resize(*containerSize);

            size_t index = 0;
            for ([[maybe_unused]] const auto &item : container) {
                results[index].first = index;
                ++index;
            }

            std::for_each(std::execution::par_unseq, results.begin(),
                          results.end(), [&container, &key](auto &pair) {
                              auto it = std::ranges::begin(container);
                              std::advance(it, pair.first);
                              pair.second = (key.empty() ? "- " : "  - ") +
                                            toYaml(*it, "") + "\n";
                          });

            for (const auto &pair : results) {
                result += pair.second;
            }
        } else {
            for (const auto &item : container) {
                result +=
                    (key.empty() ? "- " : "  - ") + toYaml(item, "") + "\n";
            }
        }

        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
        result += "\n";

        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           (key.empty() ? "items" : key.c_str()))
            .c_str();
    }
}

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
            if constexpr (String<K> || Char<K>) {
                atom::utils::String k = atom::utils::String(pair.first);
                if (k.find(':') != atom::utils::String::npos ||
                    k.find('#') != atom::utils::String::npos ||
                    k.find('\n') != atom::utils::String::npos || k.empty() ||
                    (!k.empty() && (k.front() == ' ' || k.back() == ' '))) {
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
                           (key.empty() ? "map" : key.c_str()))
            .c_str();
    }
}

template <typename T1, typename T2>
[[nodiscard]] auto toYaml(const std::pair<T1, T2> &pair,
                          const atom::utils::String &key)
    -> atom::utils::String {
    try {
        atom::utils::String result = key.empty() ? "" : key + ":\n";
        result +=
            std::string((key.empty() ? "" : "  ")) + toYaml(pair.first, "key");
        result += std::string((key.empty() ? "" : "  ")) +
                  toYaml(pair.second, "value");
        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           (key.empty() ? "pair" : key.c_str()))
            .c_str();
    }
}

template <typename T>
[[nodiscard]] auto toYaml(const T &value, const atom::utils::String &key)
    -> atom::utils::String {
    try {
        atom::utils::String formattedValue;
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
                    formattedValue =
                        atom::utils::String(std::format("{:.12g}", value));
            } else {
                formattedValue = atom::utils::String(std::to_string(value));
            }
        } else if constexpr (Pointer<T> || SmartPointer<T>) {
            if (value == nullptr) [[unlikely]] {
                formattedValue = "null";
            } else [[likely]] {
                return toYaml(*value, key);
            }
        } else if constexpr (requires { value.toYaml(key); }) {
            return value.toYaml(key);
        } else {
            formattedValue = "null";
        }

        if (key.empty()) {
            return formattedValue;
        } else {
            return key + ": " + formattedValue + "\n";
        }

    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           (key.empty() ? "value" : key.c_str()))
            .c_str();
    }
}

template <typename... Ts>
[[nodiscard]] auto toYaml(const std::tuple<Ts...> &tuple,
                          const atom::utils::String &key)
    -> atom::utils::String {
    try {
        atom::utils::String result = key.empty() ? "" : key + ":\n";
        std::apply(
            [&result, &key](const Ts &...args) {
                ((result +=
                  (key.empty() ? "- " : "  - ") + toYaml(args, "") + "\n"),
                 ...);
            },
            tuple);

        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
        result += "\n";

        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n{}: null\n", e.what(),
                           (key.empty() ? "tuple" : key.c_str()))
            .c_str();
    }
}

template <typename T>
[[nodiscard]] auto toToml(const T &value, const atom::utils::String &key)
    -> atom::utils::String;

template <std::ranges::input_range Container>
[[nodiscard]] auto toToml(const Container &container,
                          const atom::utils::String &key)
    -> atom::utils::String {
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

template <typename K, typename V>
[[nodiscard]] auto toToml(const atom::utils::HashMap<K, V> &map,
                          const atom::utils::String &key)
    -> atom::utils::String {
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
            if constexpr (String<K> || Char<K>) {
                atom::utils::String k = atom::utils::String(pair.first);
                bool needsQuotes =
                    k.empty() ||
                    k.find_first_not_of(
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01"
                        "23456789_-") != atom::utils::String::npos;
                keyStr = needsQuotes ? "\"" + k + "\"" : k;
            } else {
                keyStr = toString(pair.first);
                if (keyStr.find_first_not_of(
                        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz01"
                        "23456789_-") != atom::utils::String::npos) {
                    keyStr = "\"" + keyStr + "\"";
                }
            }

            result += keyStr + " = " + toToml(pair.second, "");
            first = false;
        }

        result += key.empty() ? "\n" : " }\n";
        return result;
    } catch (const std::exception &e) {
        return std::format("# Error: {}\n# {}: null\n", e.what(),
                           (key.empty() ? "map" : key.c_str()))
            .c_str();
    }
}

template <typename T1, typename T2>
[[nodiscard]] auto toToml(const std::pair<T1, T2> &pair,
                          const atom::utils::String &key)
    -> atom::utils::String {
    try {
        if (key.empty()) {
            throw std::invalid_argument(
                "TOML requires a key for pair representation");
        }
        atom::utils::String result = key + " = { ";
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

template <typename T>
[[nodiscard]] auto toToml(const T &value, const atom::utils::String &key)
    -> atom::utils::String {
    try {
        atom::utils::String formattedValue;
        if constexpr (String<T> || Char<T>) {
            atom::utils::String strValue = atom::utils::String(value);
            bool needsQuotes = true;
            if (needsQuotes) {
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
                formattedValue = strValue;
            }

        } else if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
            formattedValue = value ? "true" : "false";
        } else if constexpr (Number<T>) {
            if constexpr (std::is_floating_point_v<T>) {
                if (std::isnan(value))
                    formattedValue = "nan";
                else if (std::isinf(value))
                    formattedValue = (value > 0 ? "inf" : "-inf");
                else
                    formattedValue =
                        atom::utils::String(std::format("{:.12g}", value));
            } else {
                formattedValue = atom::utils::String(std::to_string(value));
            }
        } else if constexpr (Pointer<T> || SmartPointer<T>) {
            if (value == nullptr) [[unlikely]] {
                throw std::runtime_error(
                    "Cannot represent nullptr directly in TOML value");
            } else [[likely]] {
                return toToml(*value, "");
            }
        } else if constexpr (requires { value.toToml(""); }) {
            return value.toToml("");
        } else {
            throw std::runtime_error(
                "Cannot represent unknown type in TOML value");
        }

        if (!key.empty()) {
            return key + " = " + formattedValue + "\n";
        } else {
            return formattedValue;
        }

    } catch (const std::exception &e) {
        return std::format("# Error: {}\n# {}: error\n", e.what(),
                           (key.empty() ? "value" : key.c_str()))
            .c_str();
    }
}

template <typename... Ts>
[[nodiscard]] auto toToml(const std::tuple<Ts...> &tuple,
                          const atom::utils::String &key)
    -> atom::utils::String {
    try {
        if (key.empty()) {
            throw std::invalid_argument(
                "TOML arrays require a key for tuple representation");
        }
        atom::utils::String result = key + " = [\n";
        bool first = true;
        std::apply(
            [&result, &first](const Ts &...args) {
                auto process_arg = [&](const auto &arg) {
                    if (!first) {
                        result += ",\n";
                    }
                    result += "  " + toToml(arg, "");
                    first = false;
                };
                (process_arg(args), ...);
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
