/*
 * backend_interface.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Stacktrace backend interface

**************************************************/

#ifndef ATOM_ERROR_STACKTRACE_BACKEND_INTERFACE_HPP
#define ATOM_ERROR_STACKTRACE_BACKEND_INTERFACE_HPP

#include <memory>
#include <string>
#include <vector>

#include "stack_frame.hpp"

namespace atom::error {

/**
 * @brief Stacktrace backend interface for different implementations
 */
class StackTraceBackend {
public:
    virtual ~StackTraceBackend() = default;

    /**
     * @brief Capture current stack trace
     */
    virtual std::vector<StackFrame> capture(const StackTraceConfig& config) = 0;

    /**
     * @brief Get backend name
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Check if backend is available
     */
    virtual bool isAvailable() const = 0;
};

/**
 * @brief Factory for creating stacktrace backends
 */
class StackTraceBackendFactory {
public:
    /**
     * @brief Create backend by name
     * @param name Backend name ("builtin", "cpptrace", "backward", "boost",
     * "auto")
     * @return Unique pointer to backend, nullptr if not available
     */
    static std::unique_ptr<StackTraceBackend> create(const std::string& name);

    /**
     * @brief Get list of available backends
     * @return Vector of available backend names
     */
    static std::vector<std::string> getAvailable();

    /**
     * @brief Get the best available backend
     * @return Unique pointer to the best backend
     */
    static std::unique_ptr<StackTraceBackend> createBest();

private:
    static std::vector<std::string> getBackendPriority();
};

}  // namespace atom::error

#endif  // ATOM_ERROR_STACKTRACE_BACKEND_INTERFACE_HPP
