/*!
 * \file abi.hpp
 * \brief An enhanced C++ ABI wrapper for type demangling and introspection
 * \author Max Qian <lightapt.com>
 * \date 2024-5-25
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_ABI_HPP
#define ATOM_META_ABI_HPP

#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <source_location>
#include <stdexcept>
#include <string>
#include <typeinfo>

#include "atom/containers/high_performance.hpp"

#ifdef _MSC_VER
#include <dbghelp.h>
#include <windows.h>
#pragma comment(lib, "dbghelp.lib")
#else
#include <cxxabi.h>
#include <dlfcn.h>
#endif

#if defined(ENABLE_DEBUG) || defined(ATOM_META_ENABLE_VISUALIZATION)
#include <iostream>
#include <regex>
#include <sstream>
#endif

namespace atom::meta {

// Use the correct namespace path and ensure types are complete
using HashMap = containers::HashMap<containers::String, containers::String>;
using String = containers::String;
using Vector = containers::Vector<containers::String>;

/*!
 * \brief Configuration options for the ABI utilities
 */
struct AbiConfig {
    static constexpr std::size_t buffer_size = 2048;
    static constexpr std::size_t max_cache_size = 1024;
    static constexpr bool thread_safe_cache = true;
};

/*!
 * \brief Exception class for ABI-related errors
 */
class AbiException : public std::runtime_error {
public:
    explicit AbiException(const String& message)
        : std::runtime_error(std::string(message.begin(), message.end())) {}

    explicit AbiException(const char* message) : std::runtime_error(message) {}
};

/*!
 * \brief Enhanced helper class for C++ name demangling and type introspection
 */
class DemangleHelper {
public:
    /*!
     * \brief Demangle a type at compile time
     * \tparam T The type to demangle
     * \return A human-readable string representation of the type
     */
    template <typename T>
    static auto demangleType() -> String {
        return demangleInternal(typeid(T).name());
    }

    /*!
     * \brief Demangle the type of an instance
     * \tparam T The type to demangle
     * \param instance An instance of the type
     * \return A human-readable string representation of the type
     */
    template <typename T>
    static auto demangleType(const T& instance) -> String {
        return demangleInternal(typeid(instance).name());
    }

    /*!
     * \brief Get the demangled name with optional source location information
     * \param mangled_name The mangled name to demangle
     * \param location Optional source location information
     * \return The demangled name, optionally with source location
     */
    static auto demangle(std::string_view mangled_name,
                         const std::optional<std::source_location>& location =
                             std::nullopt) -> String {
        try {
            String demangled = demangleInternal(mangled_name);

            if (location) {
                demangled += " (";
                demangled += String(location->file_name());
                demangled += ":";
                demangled += String(std::to_string(location->line()));
                demangled += ")";
            }

            return demangled;
        } catch (const std::exception& e) {
            throw AbiException(String("Failed to demangle: ") +
                               String(mangled_name) + " - " + e.what());
        }
    }

    /*!
     * \brief Demangle multiple names at once
     * \param mangled_names A vector of mangled names
     * \param location Optional source location information
     * \return A vector of demangled names
     */
    static auto demangleMany(
        const containers::Vector<std::string_view>& mangled_names,
        const std::optional<std::source_location>& location = std::nullopt)
        -> Vector {
        Vector demangledNames;
        demangledNames.reserve(mangled_names.size());

        for (const auto& name : mangled_names) {
            demangledNames.push_back(demangle(name, location));
        }

        return demangledNames;
    }

    /*!
     * \brief Clear the internal demangling cache
     */
    static void clearCache() {
        if constexpr (AbiConfig::thread_safe_cache) {
            std::unique_lock lock(cacheMutex_);
            cache_.clear();
        } else {
            cache_.clear();
        }
    }

    /*!
     * \brief Get the current cache size
     * \return The number of entries in the cache
     */
    static std::size_t cacheSize() {
        if constexpr (AbiConfig::thread_safe_cache) {
            std::shared_lock lock(cacheMutex_);
            return cache_.size();
        } else {
            return cache_.size();
        }
    }

#if defined(ENABLE_DEBUG) || defined(ATOM_META_ENABLE_VISUALIZATION)
    /*!
     * \brief Visualize a demangled type name as a hierarchical structure
     * \param demangled_name The demangled type name to visualize
     * \return A string containing the hierarchical visualization
     */
    static auto visualize(const String& demangled_name) -> String {
        return visualizeType(demangled_name);
    }

    /*!
     * \brief Visualize a type as a hierarchical structure
     * \tparam T The type to visualize
     * \return A string containing the hierarchical visualization
     */
    template <typename T>
    static auto visualizeType() -> String {
        return visualize(demangleType<T>());
    }

    /*!
     * \brief Visualize the type of an object as a hierarchical structure
     * \param obj The object whose type will be visualized
     * \return A string containing the hierarchical visualization
     */
    template <typename T>
    static auto visualizeObject(const T& obj) -> String {
        return visualize(demangleType(obj));
    }
#endif

    /*!
     * \brief Check if a type is a template specialization
     * \tparam T The type to check
     * \return true if the type is a template specialization
     */
    template <typename T>
    static constexpr bool isTemplateSpecialization() {
        String name = demangleType<T>();
        return name.find('<') != String::npos;
    }

    /*!
     * \brief Check if a demangled name represents a template type
     * \param demangled_name The demangled type name to check
     * \return true if the name represents a template type
     */
    static bool isTemplateType(const String& demangled_name) {
        return demangled_name.find('<') != String::npos &&
               demangled_name.find('>') != String::npos;
    }

private:
    /*!
     * \brief Internal implementation of name demangling with caching
     * \param mangled_name The mangled name to demangle
     * \return The demangled name
     */
    static auto demangleInternal(std::string_view mangled_name) -> String {
        String cacheKey(mangled_name);

        if constexpr (AbiConfig::thread_safe_cache) {
            {
                std::shared_lock readLock(cacheMutex_);
                if (auto it = cache_.find(cacheKey); it != cache_.end()) {
                    return it->second;
                }
            }
        } else {
            if (auto it = cache_.find(cacheKey); it != cache_.end()) {
                return it->second;
            }
        }

        String demangled;

#ifdef _MSC_VER
        std::array<char, AbiConfig::buffer_size> buffer;
        DWORD length = UnDecorateSymbolName(mangled_name.data(), buffer.data(),
                                            static_cast<DWORD>(buffer.size()),
                                            UNDNAME_COMPLETE);

        if (length > 0) {
            demangled = String(buffer.data(), length);
        } else {
            DWORD error = GetLastError();
            if (error == ERROR_INSUFFICIENT_BUFFER) {
                throw AbiException("Buffer too small for demangling");
            }
            demangled = String(mangled_name);
        }
#else
        int status = -1;
        std::unique_ptr<char, void (*)(void*)> demangledName(
            abi::__cxa_demangle(mangled_name.data(), nullptr, nullptr, &status),
            std::free);

        if (status == 0 && demangledName) {
            demangled = String(demangledName.get());
        } else {
            switch (status) {
                case -1:
                    throw AbiException(
                        "Memory allocation failure during demangling");
                case -2:
                    demangled = String(mangled_name);
                    break;
                case -3:
                    throw AbiException("Invalid mangled name");
                default:
                    demangled = String(mangled_name);
            }
        }
#endif

        if constexpr (AbiConfig::thread_safe_cache) {
            std::unique_lock writeLock(cacheMutex_);
            if (cache_.size() >= AbiConfig::max_cache_size) {
                auto it = cache_.begin();
                std::size_t count = 0;
                const std::size_t limit = AbiConfig::max_cache_size / 2;
                while (count < limit && it != cache_.end()) {
                    it = cache_.erase(it);
                    ++count;
                }
            }
            cache_[cacheKey] = demangled;
        } else {
            if (cache_.size() >= AbiConfig::max_cache_size) {
                auto it = cache_.begin();
                std::size_t count = 0;
                const std::size_t limit = AbiConfig::max_cache_size / 2;
                while (count < limit && it != cache_.end()) {
                    it = cache_.erase(it);
                    ++count;
                }
            }
            cache_[cacheKey] = demangled;
        }

        return demangled;
    }

#if defined(ENABLE_DEBUG) || defined(ATOM_META_ENABLE_VISUALIZATION)
    /*!
     * \brief Visualize a type as a hierarchical structure
     * \param type_name The demangled type name
     * \param indent_level Indentation level for visualization
     * \return A string containing the hierarchical visualization
     */
    static auto visualizeType(const String& type_name, int indent_level = 0)
        -> String {
        String indent(indent_level * 4, ' ');
        String result;

        std::string type_name_std =
            std::string(type_name.begin(), type_name.end());

        static const std::regex templateRegex(R"((\w+)<(.*)>)");
        static const std::regex functionRegex(R"((.*)\s*->\s*(.*))");
        static const std::regex ptrRegex(R"((.+)\s*\*\s*)");
        static const std::regex refRegex(R"((.+)\s*&\s*)");
        static const std::regex constRegex(R"((const\s+)(.+))");
        static const std::regex arrayRegex(R"((.+)\[(\d*)\])");
        static const std::regex namespaceRegex(R"((\w+)::(.+))");
        std::smatch match;

        if (std::regex_match(type_name_std, match, templateRegex)) {
            result +=
                indent + "`-- " + String(match[1].str()) + " [template]\n";
            String params = String(match[2].str());
            result += visualizeTemplateParams(params, indent_level + 1);
        } else if (std::regex_match(type_name_std, match, functionRegex)) {
            result += indent + "`-- function\n";
            String params = String(match[1].str());
            String returnType = String(match[2].str());
            result += indent + "    `-- return: ";
            result += visualizeType(returnType, indent_level + 1)
                          .substr((indent_level + 1) * 4);
        } else if (std::regex_match(type_name_std, match, ptrRegex)) {
            result += indent + "`-- pointer to\n";
            result += visualizeType(String(match[1].str()), indent_level + 1);
        } else if (std::regex_match(type_name_std, match, refRegex)) {
            result += indent + "`-- reference to\n";
            result += visualizeType(String(match[1].str()), indent_level + 1);
        } else if (std::regex_match(type_name_std, match, constRegex)) {
            result += indent + "`-- const\n";
            result += visualizeType(String(match[2].str()), indent_level + 1);
        } else if (std::regex_match(type_name_std, match, arrayRegex)) {
            String sizeStr =
                match[2].matched ? String(match[2].str()) : String("unknown");
            result += indent + "`-- array [size=" + sizeStr + "]\n";
            result += visualizeType(String(match[1].str()), indent_level + 1);
        } else if (std::regex_match(type_name_std, match, namespaceRegex)) {
            result += indent + "`-- namespace " + String(match[1].str()) + "\n";
            result += visualizeType(String(match[2].str()), indent_level + 1);
        } else {
            result += indent + "`-- " + type_name + "\n";
        }

        return result;
    }

    /*!
     * \brief Helper function to visualize template parameters
     * \param params The template parameters string
     * \param indent_level Indentation level
     * \return A visualization of the template parameters
     */
    static auto visualizeTemplateParams(const String& params, int indent_level)
        -> String {
        String indent(indent_level * 4, ' ');
        String result;
        int paramIndex = 0;

        size_t start = 0;
        int angleBrackets = 0;
        int parentheses = 0;
        bool inQuotes = false;

        for (size_t i = 0; i < params.size(); ++i) {
            char c = params[i];

            if (c == '"' && (i == 0 || params[i - 1] != '\\')) {
                inQuotes = !inQuotes;
            }

            if (!inQuotes) {
                if (c == '<')
                    ++angleBrackets;
                else if (c == '>')
                    --angleBrackets;
                else if (c == '(')
                    ++parentheses;
                else if (c == ')')
                    --parentheses;
            }

            if (c == ',' && angleBrackets == 0 && parentheses == 0 &&
                !inQuotes) {
                String prefix = String("├── ");
                result += indent + prefix +
                          String(std::to_string(paramIndex++)) + ": ";
                String paramType = params.substr(start, i - start);
                result += visualizeType(paramType, indent_level + 1)
                              .substr(indent.length() + 4);
                start = i + 1;
            }
        }

        String prefix = String("└── ");
        result += indent + prefix + String(std::to_string(paramIndex)) + ": ";
        String paramType = params.substr(start);
        result += visualizeType(paramType, indent_level + 1)
                      .substr(indent.length() + 4);

        return result;
    }

    /*!
     * \brief Helper function to visualize function parameters
     * \param params The function parameters string
     * \param indent_level Indentation level
     * \return A visualization of the function parameters
     */
    static auto visualizeFunctionParams(const String& params, int indent_level)
        -> String {
        if (params.empty()) {
            return String(indent_level * 4, ' ') + "    (no parameters)\n";
        }

        String indent(indent_level * 4, ' ');
        String result;
        int paramIndex = 0;

        size_t start = 0;
        int angleBrackets = 0;
        int parentheses = 0;
        bool inQuotes = false;

        for (size_t i = 0; i < params.size(); ++i) {
            char c = params[i];

            if (c == '"' && (i == 0 || params[i - 1] != '\\')) {
                inQuotes = !inQuotes;
            }

            if (!inQuotes) {
                if (c == '<')
                    ++angleBrackets;
                else if (c == '>')
                    --angleBrackets;
                else if (c == '(')
                    ++parentheses;
                else if (c == ')')
                    --parentheses;
            }

            if (c == ',' && angleBrackets == 0 && parentheses == 0 &&
                !inQuotes) {
                String prefix = String("├── ");
                result += indent + prefix + "param " +
                          String(std::to_string(paramIndex++)) + ": ";
                String paramType = params.substr(start, i - start);
                result += visualizeType(paramType, indent_level + 1)
                              .substr(indent.length() + 4);
                start = i + 1;
            }
        }

        if (!params.empty()) {
            String prefix = String("└── ");
            result += indent + prefix + "param " +
                      String(std::to_string(paramIndex)) + ": ";
            String paramType = params.substr(start);
            result += visualizeType(paramType, indent_level + 1)
                          .substr(indent.length() + 4);
        }

        return result;
    }
#endif

private:
    static inline HashMap cache_;
    static inline std::shared_mutex cacheMutex_;
};

}  // namespace atom::meta

#endif  // ATOM_META_ABI_HPP
