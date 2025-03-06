/*
 * error_stack.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-29

Description: Error Stack

**************************************************/

#ifndef ATOM_ERROR_STACK_HPP
#define ATOM_ERROR_STACK_HPP

#include <concepts>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

#include "atom/macro.hpp"

namespace atom::error {

/**
 * @brief Error information structure.
 */
struct ErrorInfo {
    std::string errorMessage; /**< Error message. */
    std::string moduleName;   /**< Module name. */
    std::string functionName; /**< Function name where the error occurred. */
    int line;                 /**< Line number where the error occurred. */
    std::string fileName;     /**< File name where the error occurred. */
    time_t timestamp;         /**< Timestamp of the error. */
    std::string uuid;         /**< UUID of the error. */

    // Add equality comparison
    bool operator==(const ErrorInfo& other) const noexcept {
        return errorMessage == other.errorMessage && 
               moduleName == other.moduleName &&
               functionName == other.functionName;
    }
} ATOM_ALIGNAS(128);

/**
 * @brief Overloaded stream insertion operator to print ErrorInfo object.
 * @param os Output stream.
 * @param error ErrorInfo object to be printed.
 * @return Reference to the output stream.
 */
auto operator<<(std::ostream &os, const ErrorInfo &error) -> std::ostream &;

/**
 * @brief Overloaded string concatenation operator to concatenate ErrorInfo
 * object with a string.
 * @param str Input string.
 * @param error ErrorInfo object to be concatenated.
 * @return Concatenated string.
 */
auto operator<<(const std::string &str, const ErrorInfo &error) -> std::string;

// Concept to check if a type is convertible to string_view
template <typename T>
concept StringViewConvertible = std::convertible_to<T, std::string_view>;

/// Represents a stack of errors and provides operations to manage and retrieve them.
class ErrorStack {
    std::vector<ErrorInfo> errorStack_;  ///< The stack of all errors.
    std::vector<ErrorInfo> compressedErrorStack_;  ///< The compressed stack of unique errors.
    std::vector<std::string> filteredModules_;  ///< Modules to be filtered out while printing errors.
    mutable std::mutex mutex_; ///< Mutex for thread safety

public:
    /// Default constructor.
    ErrorStack() = default;
    
    /// Destructor
    ~ErrorStack() = default;
    
    // Delete copy constructor and assignment operator to prevent accidental copies
    ErrorStack(const ErrorStack&) = delete;
    ErrorStack& operator=(const ErrorStack&) = delete;


    /// Create a shared pointer to an ErrorStack object.
    /// \return A shared pointer to the ErrorStack object.
    [[nodiscard]] static auto createShared() -> std::shared_ptr<ErrorStack>;

    /// Create a unique pointer to an ErrorStack object.
    /// \return A unique pointer to the ErrorStack object.
    [[nodiscard]] static auto createUnique() -> std::unique_ptr<ErrorStack>;

    /// Insert a new error into the error stack.
    /// \param errorMessage The error message.
    /// \param moduleName The module name where the error occurred.
    /// \param functionName The function name where the error occurred.
    /// \param line The line number where the error occurred.
    /// \param fileName The file name where the error occurred.
    /// \return true if insertion was successful, false otherwise.
    template<StringViewConvertible ErrorStr, StringViewConvertible ModuleStr, 
             StringViewConvertible FuncStr, StringViewConvertible FileStr>
    bool insertError(ErrorStr&& errorMessage, ModuleStr&& moduleName,
                     FuncStr&& functionName, int line, FileStr&& fileName);

    /// Set the modules to be filtered out while printing the error stack.
    /// \param modules The modules to be filtered out.
    template<typename Container>
    requires std::ranges::range<Container>
    void setFilteredModules(const Container& modules);

    /// Clear the list of filtered modules.
    void clearFilteredModules();

    /// Print the filtered error stack to the standard output.
    void printFilteredErrorStack() const;

    /// Get a vector of errors filtered by a specific module.
    /// \param moduleName The module name for which errors are to be retrieved.
    /// \return A vector of errors filtered by the given module.
    [[nodiscard]] auto getFilteredErrorsByModule(std::string_view moduleName) const 
        -> std::vector<ErrorInfo>;

    /// Get a string containing the compressed errors in the stack.
    /// \return A string containing the compressed errors.
    [[nodiscard]] auto getCompressedErrors() const -> std::string;
    
    /// Check if the error stack is empty
    /// \return true if the stack is empty, false otherwise
    [[nodiscard]] bool isEmpty() const noexcept;
    
    /// Get the number of errors in the stack
    /// \return The number of errors
    [[nodiscard]] size_t size() const noexcept;
    
    /// Get the most recent error
    /// \return Optional containing the most recent error, or empty if stack is empty
    [[nodiscard]] std::optional<ErrorInfo> getLatestError() const;
    
    /// Clear all errors in the stack
    void clear() noexcept;

private:
    /// Update the compressed error stack based on the current error stack.
    void updateCompressedErrors();

    /// Sort the compressed error stack based on the timestamp of errors.
    void sortCompressedErrorStack();
};

// Template method implementation
template<StringViewConvertible ErrorStr, StringViewConvertible ModuleStr, 
         StringViewConvertible FuncStr, StringViewConvertible FileStr>
bool ErrorStack::insertError(ErrorStr&& errorMessage, ModuleStr&& moduleName,
                 FuncStr&& functionName, int line, FileStr&& fileName) {
    try {
        // Validate inputs
        if (std::string_view(errorMessage).empty()) {
            return false;
        }
        
        if (line < 0) {
            return false;
        }

        auto currentTime = std::time(nullptr);
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto iter = std::ranges::find_if(errorStack_, [&errorMessage, &moduleName](const ErrorInfo& error) {
            return error.errorMessage == std::string(errorMessage) && 
                   error.moduleName == std::string(moduleName);
        });

        if (iter != errorStack_.end()) {
            iter->timestamp = currentTime;
        } else {
            errorStack_.emplace_back(ErrorInfo{
                std::string(errorMessage),
                std::string(moduleName),
                std::string(functionName),
                line,
                std::string(fileName),
                currentTime,
                ""  // UUID will be generated later
            });
        }

        updateCompressedErrors();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

template<typename Container>
requires std::ranges::range<Container>
void ErrorStack::setFilteredModules(const Container& modules) {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        filteredModules_.clear();
        filteredModules_.reserve(std::ranges::distance(modules));
        for (const auto& module : modules) {
            filteredModules_.emplace_back(module);
        }
    } catch (const std::exception&) {
        // In case of error, clear the filter list to avoid partial updates
        filteredModules_.clear();
    }
}

}  // namespace atom::error

#endif
