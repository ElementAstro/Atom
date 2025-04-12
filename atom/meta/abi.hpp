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
// #include <string> // Replaced by high_performance.hpp
#include <typeinfo>
// #include <unordered_map> // Replaced by high_performance.hpp
// #include <vector> // Replaced by high_performance.hpp

#include "atom/containers/high_performance.hpp"  // Include high performance containers

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
#include <iostream>  // Keep for std::cout if visualization prints directly
#include <regex>
#include <sstream>  // Keep for potential string stream usage in visualization
#endif

namespace atom::meta {

// Use type aliases from high_performance.hpp
using atom::containers::HashMap;
using atom::containers::String;
using atom::containers::Vector;

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
    // Use String for the message internally if desired, but std::runtime_error
    // expects const char* or std::string
    explicit AbiException(const String& message)
        : std::runtime_error(message.c_str()) {}
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
    static auto demangleType() -> String {  // Return String
        return demangleInternal(typeid(T).name());
    }

    /**
     * @brief Demangle the type of an instance
     * @tparam T The type to demangle
     * @param instance An instance of the type
     * @return A human-readable string representation of the type
     */
    template <typename T>
    static auto demangleType(const T& instance) -> String {  // Return String
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
                             std::nullopt) -> String {  // Return String
        try {
            String demangled = demangleInternal(mangled_name);  // Use String

            if (location) {
                // Assuming String supports operator+= and
                // construction/conversion from std::string/char*
                demangled += " (";
                demangled += location->file_name();
                demangled += ":";
                // std::to_string returns std::string, convert to String if
                // necessary
                demangled += String(std::to_string(location->line()));
                demangled += ")";
            }

            return demangled;
        } catch (const std::exception& e) {
            // Construct AbiException message using String concatenation if
            // needed
            throw AbiException(String("Failed to demangle: ") +
                               String(mangled_name) + " - " + e.what());
        }
    }

    /**
     * @brief Demangle multiple names at once
     * @param mangled_names A vector of mangled names
     * @param location Optional source location information
     * @return A vector of demangled names
     */
    static auto demangleMany(
        const Vector<std::string_view>& mangled_names,  // Use Vector
        const std::optional<std::source_location>& location = std::nullopt)
        -> Vector<String> {             // Return Vector<String>
        Vector<String> demangledNames;  // Use Vector<String>
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
    static auto visualize(const String& demangled_name)
        -> String {  // Param and return String
        return visualizeType(demangled_name);
    }

    /**
     * @brief Visualize a type as a hierarchical structure
     * @tparam T The type to visualize
     * @return A string containing the hierarchical visualization
     */
    template <typename T>
    static auto visualizeType() -> String {  // Return String
        return visualize(demangleType<T>());
    }

    /**
     * @brief Visualize the type of an object as a hierarchical structure
     * @param obj The object whose type will be visualized
     * @return A string containing the hierarchical visualization
     */
    template <typename T>
    static auto visualizeObject(const T& obj) -> String {  // Return String
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
        // Demangle and check the resulting String
        String name = demangleType<T>();
        // Assuming String has find method similar to std::string
        return name.find('<') != String::npos;
    }

    /**
     * @brief Check if a demangled name represents a template type
     * @param demangled_name The demangled type name to check
     * @return true if the name represents a template type
     */
    static bool isTemplateType(const String& demangled_name) {  // Param String
        // Assuming String has find method similar to std::string
        return demangled_name.find('<') != String::npos &&
               demangled_name.find('>') != String::npos;
    }

private:
    /**
     * @brief Internal implementation of name demangling with caching
     * @param mangled_name The mangled name to demangle
     * @return The demangled name
     */
    static auto demangleInternal(std::string_view mangled_name)
        -> String {  // Return String
        // Convert string_view to String for cache lookup/insertion key
        String cacheKey(mangled_name);

        // Check cache first
        if constexpr (AbiConfig::thread_safe_cache) {
            // With thread safety
            {
                std::shared_lock readLock(cacheMutex_);
                if (auto it = cache_.find(cacheKey); it != cache_.end()) {
                    return it->second;
                }
            }
        } else {
            // Without thread safety
            if (auto it = cache_.find(cacheKey); it != cache_.end()) {
                return it->second;
            }
        }

        // Not in cache, perform demangling
        String demangled;  // Use String

#ifdef _MSC_VER
        // MSVC demangling
        std::array<char, AbiConfig::buffer_size> buffer;
        DWORD length = UnDecorateSymbolName(mangled_name.data(), buffer.data(),
                                            static_cast<DWORD>(buffer.size()),
                                            UNDNAME_COMPLETE);

        if (length > 0) {
            // Construct String from char buffer
            demangled = String(buffer.data(), length);
        } else {
            DWORD error = GetLastError();
            if (error == ERROR_INSUFFICIENT_BUFFER) {
                throw AbiException("Buffer too small for demangling");
            }
            // Fall back to the mangled name if demangling fails
            demangled = String(mangled_name);
        }
#else
        // GCC/Clang demangling
        int status = -1;
        std::unique_ptr<char, void (*)(void*)> demangledName(
            abi::__cxa_demangle(mangled_name.data(), nullptr, nullptr, &status),
            std::free);

        if (status == 0 && demangledName) {
            // Construct String from char*
            demangled = String(demangledName.get());
        } else {
            // Provide more detailed error information
            switch (status) {
                case -1:
                    throw AbiException(
                        "Memory allocation failure during demangling");
                case -2:
                    // This is common for built-in types, so we just use the
                    // mangled name
                    demangled = String(mangled_name);
                    break;
                case -3:
                    throw AbiException("Invalid mangled name");
                default:
                    demangled = String(mangled_name);
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
                std::size_t count = 0;
                const std::size_t limit = AbiConfig::max_cache_size / 2;
                while (count < limit && it != cache_.end()) {
                    it = cache_.erase(it);
                    ++count;
                }
            }
            cache_[cacheKey] = demangled;  // Use cacheKey (String)
        } else {
            // Check cache size limit
            if (cache_.size() >= AbiConfig::max_cache_size) {
                // Simple strategy: clear half the cache when full
                auto it = cache_.begin();
                std::size_t count = 0;
                const std::size_t limit = AbiConfig::max_cache_size / 2;
                while (count < limit && it != cache_.end()) {
                    it = cache_.erase(it);
                    ++count;
                }
            }
            cache_[cacheKey] = demangled;  // Use cacheKey (String)
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
    static auto visualizeType(const String& type_name,  // Param String
                              int indent_level = 0)
        -> String {  // Return String
        String indent(indent_level * 4,
                      ' ');  // Assuming String constructor from size and char
        String result;       // Use String

        // Regular expressions for parsing - std::regex works on char sequences
        // Need to convert type_name to std::string or char* if regex doesn't
        // accept String directly Assuming String has a .c_str() or similar
        // method, or can be implicitly converted
        std::string type_name_std = std::string(
            type_name.begin(),
            type_name.end());  // Convert String to std::string for regex

        static const std::regex templateRegex(R"((\w+)<(.*)>)");
        static const std::regex functionRegex(
            R"((.*)\s*->\s*(.*))");  // Adjusted function regex
        static const std::regex ptrRegex(R"((.+)\s*\*\s*)");
        static const std::regex refRegex(R"((.+)\s*&\s*)");
        static const std::regex constRegex(R"((const\s+)(.+))");
        static const std::regex arrayRegex(
            R"((.+)\[(\d*)\])");  // Adjusted array regex
        static const std::regex namespaceRegex(R"((\w+)::(.+))");
        std::smatch match;

        if (std::regex_match(type_name_std, match, templateRegex)) {
            // Template type
            result += indent + "`-- " + String(match[1].str()) +
                      " [template]\n";  // Convert submatch to String
            String params =
                String(match[2].str());  // Convert submatch to String
            result += visualizeTemplateParams(params, indent_level + 1);
        } else if (std::regex_match(type_name_std, match, functionRegex)) {
            // Function type - Needs careful parsing of parameters vs return
            // type This regex might be too simple. Assuming a basic structure
            // for now.
            result += indent + "`-- function\n";
            String params =
                String(match[1].str());  // Potential parameters part
            String returnType = String(match[2].str());  // Return type part
            // Need a robust way to parse function signature parameters
            // result += visualizeFunctionParams(params, indent_level + 1);
            result += indent + "    `-- return: ";
            result +=
                visualizeType(returnType, indent_level + 1)
                    .substr((indent_level + 1) * 4);  // Assuming substr works

        } else if (std::regex_match(type_name_std, match, ptrRegex)) {
            // Pointer type
            result += indent + "`-- pointer to\n";
            result +=
                visualizeType(String(match[1].str()),
                              indent_level + 1);  // Convert submatch to String
        } else if (std::regex_match(type_name_std, match, refRegex)) {
            // Reference type
            result += indent + "`-- reference to\n";
            result +=
                visualizeType(String(match[1].str()),
                              indent_level + 1);  // Convert submatch to String
        } else if (std::regex_match(type_name_std, match, constRegex)) {
            // Const type
            result += indent + "`-- const\n";
            result +=
                visualizeType(String(match[2].str()),
                              indent_level + 1);  // Convert submatch to String
        } else if (std::regex_match(type_name_std, match, arrayRegex)) {
            // Array type
            String sizeStr =
                match[2].matched ? String(match[2].str()) : String("unknown");
            result += indent + "`-- array [size=" + sizeStr + "]\n";
            result +=
                visualizeType(String(match[1].str()),
                              indent_level + 1);  // Convert submatch to String
        } else if (std::regex_match(type_name_std, match, namespaceRegex)) {
            // Namespaced type
            result += indent + "`-- namespace " + String(match[1].str()) +
                      "\n";  // Convert submatch to String
            result +=
                visualizeType(String(match[2].str()),
                              indent_level + 1);  // Convert submatch to String
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
    static auto visualizeTemplateParams(const String& params,  // Param String
                                        int indent_level)
        -> String {                            // Return String
        String indent(indent_level * 4, ' ');  // Use String
        String result;                         // Use String
        int paramIndex = 0;

        size_t start = 0;
        int angleBrackets = 0;
        int parentheses = 0;
        bool inQuotes = false;

        // Assuming String supports iteration, size(), operator[], substr
        for (size_t i = 0; i < params.size(); ++i) {
            char c = params[i];  // Assuming operator[] returns char or similar

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
                String prefix = String("├── ");  // Use String
                result += indent + prefix +
                          String(std::to_string(paramIndex++)) +
                          ": ";  // Convert index to String
                String paramType =
                    params.substr(start, i - start);  // Assuming substr exists
                // Trim whitespace (assuming String has methods similar to
                // std::string or requires manual trim) Manual trim example
                // (needs String methods like find_first_not_of,
                // find_last_not_of) size_t first =
                // paramType.find_first_not_of(" \t\n\r\f\v"); if (String::npos
                // != first) {
                //     size_t last = paramType.find_last_not_of(" \t\n\r\f\v");
                //     paramType = paramType.substr(first, (last - first + 1));
                // } else {
                //     paramType.clear();
                // }
                result +=
                    visualizeType(paramType, indent_level + 1)
                        .substr(indent.length() + 4);  // Assuming substr exists
                start = i + 1;
            }
        }

        // Add the last parameter
        String prefix = String("└── ");  // Use String
        result += indent + prefix + String(std::to_string(paramIndex)) +
                  ": ";                           // Convert index to String
        String paramType = params.substr(start);  // Assuming substr exists
        // Trim whitespace (similar to above)
        // size_t first = paramType.find_first_not_of(" \t\n\r\f\v");
        // if (String::npos != first) {
        //     size_t last = paramType.find_last_not_of(" \t\n\r\f\v");
        //     paramType = paramType.substr(first, (last - first + 1));
        // } else {
        //     paramType.clear();
        // }
        result += visualizeType(paramType, indent_level + 1)
                      .substr(indent.length() + 4);  // Assuming substr exists

        return result;
    }

    /**
     * @brief Helper function to visualize function parameters
     * @param params The function parameters string
     * @param indent_level Indentation level
     * @return A visualization of the function parameters
     */
    static auto visualizeFunctionParams(const String& params,  // Param String
                                        int indent_level)
        -> String {            // Return String
        if (params.empty()) {  // Assuming empty() exists
            return String(indent_level * 4, ' ') +
                   "    (no parameters)\n";  // Use String
        }

        String indent(indent_level * 4, ' ');  // Use String
        String result;                         // Use String
        int paramIndex = 0;

        size_t start = 0;
        int angleBrackets = 0;
        int parentheses = 0;
        bool inQuotes = false;

        // Assuming String supports iteration, size(), operator[], substr
        for (size_t i = 0; i < params.size(); ++i) {
            char c = params[i];  // Assuming operator[] returns char or similar

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
                String prefix = String("├── ");  // Use String
                result += indent + prefix + "param " +
                          String(std::to_string(paramIndex++)) +
                          ": ";  // Convert index to String
                String paramType =
                    params.substr(start, i - start);  // Assuming substr exists
                // Trim whitespace (as in visualizeTemplateParams)
                result +=
                    visualizeType(paramType, indent_level + 1)
                        .substr(indent.length() + 4);  // Assuming substr exists
                start = i + 1;
            }
        }

        // Add the last parameter
        if (!params.empty()) {               // Assuming empty() exists
            String prefix = String("└── ");  // Use String
            result += indent + prefix + "param " +
                      String(std::to_string(paramIndex)) +
                      ": ";                           // Convert index to String
            String paramType = params.substr(start);  // Assuming substr exists
            // Trim whitespace (as in visualizeTemplateParams)
            result +=
                visualizeType(paramType, indent_level + 1)
                    .substr(indent.length() + 4);  // Assuming substr exists
        }

        return result;
    }
#endif

private:
    // Thread-safe cache implementation using HashMap and String
    static inline HashMap<String, String>
        cache_;  // Use HashMap<String, String>

    // Mutex for cache access (only used when thread safety is enabled)
    static inline std::shared_mutex cacheMutex_;
};

}  // namespace atom::meta

#endif  // ATOM_META_ABI_HPP
