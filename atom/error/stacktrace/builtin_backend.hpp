/*
 * builtin_backend.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Built-in stacktrace backend using platform-specific APIs

**************************************************/

#ifndef ATOM_ERROR_STACKTRACE_BUILTIN_BACKEND_HPP
#define ATOM_ERROR_STACKTRACE_BUILTIN_BACKEND_HPP

#include "backend_interface.hpp"

namespace atom::error {
namespace backends {

/**
 * @brief Built-in stacktrace backend using platform-specific APIs
 */
class BuiltinBackend : public StackTraceBackend {
public:
    std::vector<StackFrame> capture(const StackTraceConfig& config) override;
    std::string getName() const override;
    bool isAvailable() const override;

private:
#if defined(__APPLE__) || defined(__linux__)
    void captureUnix(std::vector<StackFrame>& frames,
                     const StackTraceConfig& config);
    void processUnixFrame(StackFrame& frame, const char* symbol);
#endif
};

}  // namespace backends
}  // namespace atom::error

#endif  // ATOM_ERROR_STACKTRACE_BUILTIN_BACKEND_HPP
