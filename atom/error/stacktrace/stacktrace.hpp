/*
 * stacktrace.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Main stacktrace class with support for multiple backends

**************************************************/

#ifndef ATOM_ERROR_STACKTRACE_STACKTRACE_HPP
#define ATOM_ERROR_STACKTRACE_STACKTRACE_HPP

#include <memory>
#include <string>
#include <vector>

#include "backend_interface.hpp"
#include "stack_frame.hpp"

namespace atom::error {

/**
 * @brief Enhanced stack trace class with support for multiple backends
 *
 * This class provides a unified interface for capturing and formatting stack
 * traces using different backend implementations. It supports external
 * libraries like cpptrace, backward-cpp, and boost::stacktrace, with fallback
 * to built-in platform-specific implementations.
 */
class StackTrace {
public:
    /**
     * @brief Default constructor that captures the current stack trace
     */
    StackTrace();

    /**
     * @brief Constructor with custom configuration
     * @param config Configuration options for stack trace capture
     */
    explicit StackTrace(const StackTraceConfig& config);

    /**
     * @brief Copy constructor
     */
    StackTrace(const StackTrace& other);

    /**
     * @brief Move constructor
     */
    StackTrace(StackTrace&& other) noexcept;

    /**
     * @brief Copy assignment operator
     */
    StackTrace& operator=(const StackTrace& other);

    /**
     * @brief Move assignment operator
     */
    StackTrace& operator=(StackTrace&& other) noexcept;

    /**
     * @brief Destructor
     */
    ~StackTrace() = default;

    /**
     * @brief Get the string representation of the stack trace
     * @return A string representing the captured stack trace
     */
    [[nodiscard]] std::string toString() const;

    /**
     * @brief Get the string representation with custom configuration
     * @param config Configuration for formatting
     * @return A string representing the stack trace with custom formatting
     */
    [[nodiscard]] std::string toString(const StackTraceConfig& config) const;

    /**
     * @brief Get individual stack frames
     * @return Vector of stack frames
     */
    [[nodiscard]] const std::vector<StackFrame>& getFrames() const;

    /**
     * @brief Get the number of captured frames
     * @return Number of frames in the stack trace
     */
    [[nodiscard]] size_t size() const;

    /**
     * @brief Check if stack trace is empty
     * @return true if no frames were captured
     */
    [[nodiscard]] bool empty() const;

    /**
     * @brief Get the backend used for capturing this stack trace
     * @return Name of the backend used
     */
    [[nodiscard]] std::string getBackendName() const;

    /**
     * @brief Set global default configuration
     * @param config Default configuration to use for new StackTrace instances
     */
    static void setDefaultConfig(const StackTraceConfig& config);

    /**
     * @brief Get global default configuration
     * @return Current default configuration
     */
    static const StackTraceConfig& getDefaultConfig();

    /**
     * @brief Get available backends
     * @return Vector of available backend names
     */
    static std::vector<std::string> getAvailableBackends();

    /**
     * @brief Force use of specific backend
     * @param backendName Name of backend to use ("auto" for automatic
     * selection)
     */
    static void setPreferredBackend(const std::string& backendName);

private:
    std::vector<StackFrame> frames_;
    std::string backendName_;
    StackTraceConfig config_;

    static StackTraceConfig defaultConfig_;
    static std::string preferredBackend_;

    /**
     * @brief Capture stack trace using the best available backend
     */
    void capture();

    /**
     * @brief Get the best available backend
     */
    static std::unique_ptr<StackTraceBackend> getBestBackend();

    /**
     * @brief Create backend by name
     */
    static std::unique_ptr<StackTraceBackend> createBackend(
        const std::string& name);
};

/**
 * @brief Convenience functions for quick stacktrace capture
 */
namespace stacktrace {
/**
 * @brief Capture current stack trace with default settings
 * @return String representation of stack trace
 */
std::string current();

/**
 * @brief Capture current stack trace with custom depth
 * @param maxDepth Maximum number of frames to capture
 * @return String representation of stack trace
 */
std::string current(int maxDepth);

/**
 * @brief Capture current stack trace with custom configuration
 * @param config Configuration options
 * @return String representation of stack trace
 */
std::string current(const StackTraceConfig& config);
}  // namespace stacktrace

}  // namespace atom::error

#endif  // ATOM_ERROR_STACKTRACE_STACKTRACE_HPP
