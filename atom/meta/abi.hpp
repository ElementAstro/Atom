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
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

// Platform-specific includes
#ifdef _MSC_VER
// clang-format off
 #include <windows.h>
 #include <dbghelp.h>
 #pragma comment(lib, "dbghelp.lib")
// clang-format on
#else
#include <cxxabi.h>
#include <dlfcn.h>  // For symbol lookup on Unix-like systems
#endif

// Debugging support
#if defined(ENABLE_DEBUG) || defined(ATOM_META_ENABLE_VISUALIZATION)
#include <iostream>
#include <regex>
#include <sstream>
#endif

namespace atom::meta {

/**
 * @brief Configuration options for the ABI utilities
 */
struct AbiConfig {
    // Buffer size for demangling operations
    static constexpr std::size_t buffer_size = 2048;

    // Maximum cache size to prevent unbounded memory growth
    static constexpr std::size_t max_cache_size = 1024;

    // Enable thread safety (can be disabled for single-threaded contexts)
    static constexpr bool thread_safe_cache = true;
};

/**
 * @brief Exception class for ABI-related errors
 */
class AbiException : public std::runtime_error {
public:
    explicit AbiException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Enhanced helper class for C++ name demangling and type introspection
 */
class DemangleHelper {
public:
    /**
     * @brief Demangle a type at compile time
     * @tparam T The type to demangle
     * @return A human-readable string representation of the type
     */
    template <typename T>
    static auto demangleType() -> std::string {
        return demangleInternal(typeid(T).name());
    }

    /**
     * @brief Demangle the type of an instance
     * @tparam T The type to demangle
     * @param instance An instance of the type
     * @return A human-readable string representation of the type
     */
    template <typename T>
    static auto demangleType(const T& instance) -> std::string {
        return demangleInternal(typeid(instance).name());
    }

    /**
     * @brief Get the demangled name with optional source location information
     * @param mangled_name The mangled name to demangle
     * @param location Optional source location information
     * @return The demangled name, optionally with source location
     */
    static auto demangle(std::string_view mangled_name,
                         const std::optional<std::source_location>& location =
                             std::nullopt) -> std::string {
        try {
            std::string demangled = demangleInternal(mangled_name);

            if (location) {
                demangled += " (";
                demangled += location->file_name();
                demangled += ":";
                demangled += std::to_string(location->line());
                demangled += ")";
            }

            return demangled;
        } catch (const std::exception& e) {
            throw AbiException(std::string("Failed to demangle: ") +
                               std::string(mangled_name) + " - " + e.what());
        }
    }

    /**
     * @brief Demangle multiple names at once
     * @param mangled_names A vector of mangled names
     * @param location Optional source location information
     * @return A vector of demangled names
     */
    static auto demangleMany(
        const std::vector<std::string_view>& mangled_names,
        const std::optional<std::source_location>& location = std::nullopt)
        -> std::vector<std::string> {
        std::vector<std::string> demangledNames;
        demangledNames.reserve(mangled_names.size());

        for (const auto& name : mangled_names) {
            demangledNames.push_back(demangle(name, location));
        }

        return demangledNames;
    }

    /**
     * @brief Clear the internal demangling cache
     */
    static void clearCache() {
        if constexpr (AbiConfig::thread_safe_cache) {
            std::unique_lock lock(cacheMutex_);
            cache_.clear();
        } else {
            cache_.clear();
        }
    }

    /**
     * @brief Get the current cache size
     * @return The number of entries in the cache
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
    /**
     * @brief Visualize a demangled type name as a hierarchical structure
     * @param demangled_name The demangled type name to visualize
     * @return A string containing the hierarchical visualization
     */
    static auto visualize(const std::string& demangled_name) -> std::string {
        return visualizeType(demangled_name);
    }

    /**
     * @brief Visualize a type as a hierarchical structure
     * @tparam T The type to visualize
     * @return A string containing the hierarchical visualization
     */
    template <typename T>
    static auto visualizeType() -> std::string {
        return visualize(demangleType<T>());
    }

    /**
     * @brief Visualize the type of an object as a hierarchical structure
     * @param obj The object whose type will be visualized
     * @return A string containing the hierarchical visualization
     */
    template <typename T>
    static auto visualizeObject(const T& obj) -> std::string {
        return visualize(demangleType(obj));
    }
#endif

    /**
     * @brief Check if a type is a template specialization
     * @tparam T The type to check
     * @return true if the type is a template specialization
     */
    template <typename T>
    static constexpr bool isTemplateSpecialization() {
        std::string_view name = demangleType<T>();
        return name.find('<') != std::string_view::npos;
    }

    /**
     * @brief Check if a demangled name represents a template type
     * @param demangled_name The demangled type name to check
     * @return true if the name represents a template type
     */
    static bool isTemplateType(const std::string& demangled_name) {
        return demangled_name.find('<') != std::string::npos &&
               demangled_name.find('>') != std::string::npos;
    }

private:
    /**
     * @brief Internal implementation of name demangling with caching
     * @param mangled_name The mangled name to demangle
     * @return The demangled name
     */
    static auto demangleInternal(std::string_view mangled_name) -> std::string {
        // Check cache first
        if constexpr (AbiConfig::thread_safe_cache) {
            // With thread safety
            {
                std::shared_lock readLock(cacheMutex_);
                if (auto it = cache_.find(std::string(mangled_name));
                    it != cache_.end()) {
                    return it->second;
                }
            }
        } else {
            // Without thread safety
            if (auto it = cache_.find(std::string(mangled_name));
                it != cache_.end()) {
                return it->second;
            }
        }

        // Not in cache, perform demangling
        std::string demangled;

#ifdef _MSC_VER
        // MSVC demangling
        std::array<char, AbiConfig::buffer_size> buffer;
        DWORD length = UnDecorateSymbolName(mangled_name.data(), buffer.data(),
                                            static_cast<DWORD>(buffer.size()),
                                            UNDNAME_COMPLETE);

        if (length > 0) {
            demangled = std::string(buffer.data(), length);
        } else {
            DWORD error = GetLastError();
            if (error == ERROR_INSUFFICIENT_BUFFER) {
                throw AbiException("Buffer too small for demangling");
            }
            // Fall back to the mangled name if demangling fails
            demangled = std::string(mangled_name);
        }
#else
        // GCC/Clang demangling
        int status = -1;
        std::unique_ptr<char, void (*)(void*)> demangledName(
            abi::__cxa_demangle(mangled_name.data(), nullptr, nullptr, &status),
            std::free);

        if (status == 0 && demangledName) {
            demangled = demangledName.get();
        } else {
            // Provide more detailed error information
            switch (status) {
                case -1:
                    throw AbiException(
                        "Memory allocation failure during demangling");
                case -2:
                    // This is common for built-in types, so we just use the
                    // mangled name
                    demangled = std::string(mangled_name);
                    break;
                case -3:
                    throw AbiException("Invalid mangled name");
                default:
                    demangled = std::string(mangled_name);
            }
        }
#endif

        // Add to cache with cache size management
        if constexpr (AbiConfig::thread_safe_cache) {
            std::unique_lock writeLock(cacheMutex_);
            // Check cache size limit
            if (cache_.size() >= AbiConfig::max_cache_size) {
                // Simple strategy: clear half the cache when full
                auto it = cache_.begin();
                for (size_t i = 0;
                     i < AbiConfig::max_cache_size / 2 && it != cache_.end();
                     ++i) {
                    it = cache_.erase(it);
                }
            }
            cache_[std::string(mangled_name)] = demangled;
        } else {
            // Check cache size limit
            if (cache_.size() >= AbiConfig::max_cache_size) {
                // Simple strategy: clear half the cache when full
                auto it = cache_.begin();
                for (size_t i = 0;
                     i < AbiConfig::max_cache_size / 2 && it != cache_.end();
                     ++i) {
                    it = cache_.erase(it);
                }
            }
            cache_[std::string(mangled_name)] = demangled;
        }

        return demangled;
    }

#if defined(ENABLE_DEBUG) || defined(ATOM_META_ENABLE_VISUALIZATION)
    /**
     * @brief Visualize a type as a hierarchical structure
     * @param type_name The demangled type name
     * @param indent_level Indentation level for visualization
     * @return A string containing the hierarchical visualization
     */
    static auto visualizeType(const std::string& type_name,
                              int indent_level = 0) -> std::string {
        std::string indent(indent_level * 4, ' ');  // 4 spaces per indent level
        std::string result;

        // Regular expressions for parsing
        static const std::regex templateRegex(R"((\w+)<(.*)>)");
        static const std::regex functionRegex(R"($(.*)$\s*->\s*(.*))");
        static const std::regex ptrRegex(R"((.+)\s*\*\s*)");
        static const std::regex refRegex(R"((.+)\s*&\s*)");
        static const std::regex constRegex(R"((const\s+)(.+))");
        static const std::regex arrayRegex(R"((.+)\s*$$
 (\d+)
 $$)");
        static const std::regex namespaceRegex(R"((\w+)::(.+))");
        std::smatch match;

        if (std::regex_match(type_name, match, templateRegex)) {
            // Template type
            result += indent + "`-- " + match[1].str() + " [template]\n";
            std::string params = match[2].str();
            result += visualizeTemplateParams(params, indent_level + 1);
        } else if (std::regex_match(type_name, match, functionRegex)) {
            // Function type
            result += indent + "`-- function\n";
            std::string params = match[1].str();
            std::string returnType = match[2].str();
            result += visualizeFunctionParams(params, indent_level + 1);
            result += indent + "    `-- return: " +
                      visualizeType(returnType, indent_level + 2)
                          .substr((indent_level + 1) * 4);
        } else if (std::regex_match(type_name, match, ptrRegex)) {
            // Pointer type
            result += indent + "`-- pointer to\n";
            result += visualizeType(match[1].str(), indent_level + 1);
        } else if (std::regex_match(type_name, match, refRegex)) {
            // Reference type
            result += indent + "`-- reference to\n";
            result += visualizeType(match[1].str(), indent_level + 1);
        } else if (std::regex_match(type_name, match, constRegex)) {
            // Const type
            result += indent + "`-- const\n";
            result += visualizeType(match[2].str(), indent_level + 1);
        } else if (std::regex_match(type_name, match, arrayRegex)) {
            // Array type
            result += indent + "`-- array [size=" + match[2].str() + "]\n";
            result += visualizeType(match[1].str(), indent_level + 1);
        } else if (std::regex_match(type_name, match, namespaceRegex)) {
            // Namespaced type
            result += indent + "`-- namespace " + match[1].str() + "\n";
            result += visualizeType(match[2].str(), indent_level + 1);
        } else {
            // Simple type
            result += indent + "`-- " + type_name + "\n";
        }

        return result;
    }

    /**
     * @brief Helper function to visualize template parameters
     * @param params The template parameters string
     * @param indent_level Indentation level
     * @return A visualization of the template parameters
     */
    static auto visualizeTemplateParams(const std::string& params,
                                        int indent_level) -> std::string {
        std::string indent(indent_level * 4, ' ');
        std::string result;
        int paramIndex = 0;

        size_t start = 0;
        int angleBrackets = 0;
        int parentheses = 0;
        bool inQuotes = false;

        for (size_t i = 0; i < params.size(); ++i) {
            char c = params[i];

            // Update bracket counters
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
                std::string prefix =
                    (paramIndex < params.size() - 1) ? "├── " : "└── ";
                result += indent + prefix + std::to_string(paramIndex++) + ": ";
                std::string paramType = params.substr(start, i - start);
                // Trim whitespace
                paramType.erase(0, paramType.find_first_not_of(" \t\n\r\f\v"));
                paramType.erase(paramType.find_last_not_of(" \t\n\r\f\v") + 1);
                result += visualizeType(paramType, indent_level + 1)
                              .substr(indent.length() + 4);
                start = i + 1;
            }
        }

        // Add the last parameter
        std::string prefix = "└── ";
        result += indent + prefix + std::to_string(paramIndex) + ": ";
        std::string paramType = params.substr(start);
        // Trim whitespace
        paramType.erase(0, paramType.find_first_not_of(" \t\n\r\f\v"));
        paramType.erase(paramType.find_last_not_of(" \t\n\r\f\v") + 1);
        result += visualizeType(paramType, indent_level + 1)
                      .substr(indent.length() + 4);

        return result;
    }

    /**
     * @brief Helper function to visualize function parameters
     * @param params The function parameters string
     * @param indent_level Indentation level
     * @return A visualization of the function parameters
     */
    static auto visualizeFunctionParams(const std::string& params,
                                        int indent_level) -> std::string {
        if (params.empty()) {
            return std::string(indent_level * 4, ' ') + "    (no parameters)\n";
        }

        std::string indent(indent_level * 4, ' ');
        std::string result;
        int paramIndex = 0;

        size_t start = 0;
        int angleBrackets = 0;
        int parentheses = 0;
        bool inQuotes = false;

        for (size_t i = 0; i < params.size(); ++i) {
            char c = params[i];

            // Update bracket counters
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
                std::string prefix = (i < params.size() - 1) ? "├── " : "└── ";
                result += indent + prefix + "param " +
                          std::to_string(paramIndex++) + ": ";
                std::string paramType = params.substr(start, i - start);
                // Trim whitespace
                paramType.erase(0, paramType.find_first_not_of(" \t\n\r\f\v"));
                paramType.erase(paramType.find_last_not_of(" \t\n\r\f\v") + 1);
                result += visualizeType(paramType, indent_level + 1)
                              .substr(indent.length() + 4);
                start = i + 1;
            }
        }

        // Add the last parameter
        if (!params.empty()) {
            std::string prefix = "└── ";
            result +=
                indent + prefix + "param " + std::to_string(paramIndex) + ": ";
            std::string paramType = params.substr(start);
            // Trim whitespace
            paramType.erase(0, paramType.find_first_not_of(" \t\n\r\f\v"));
            paramType.erase(paramType.find_last_not_of(" \t\n\r\f\v") + 1);
            result += visualizeType(paramType, indent_level + 1)
                          .substr(indent.length() + 4);
        }

        return result;
    }
#endif

private:
    // Thread-safe cache implementation
    static inline std::unordered_map<std::string, std::string> cache_;

    // Mutex for cache access (only used when thread safety is enabled)
    static inline std::shared_mutex cacheMutex_;
};

}  // namespace atom::meta

#endif  // ATOM_META_ABI_HPP
