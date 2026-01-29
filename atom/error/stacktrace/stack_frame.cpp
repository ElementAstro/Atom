/*
 * stack_frame.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Stack frame implementation

**************************************************/

#include "stack_frame.hpp"
#include "stacktrace_utils.hpp"

#include <format>

namespace atom::error {

std::string StackFrame::toString(const StackTraceConfig& config) const {
    // Function name
    std::string funcName = function.empty() ? config.unknownFunction : function;
    if (config.demangle && !function.empty()) {
        funcName = stacktrace_utils::demangle(funcName);
    }

    std::string result = funcName;

    // Memory address
    if (config.includeAddresses && address != nullptr) {
        result +=
            std::format(" at {}", stacktrace_utils::formatAddress(
                                      reinterpret_cast<uintptr_t>(address)));
    }

    // Module information
    if (config.includeModules && !module.empty()) {
        const std::string modName = (module == config.unknownModule)
                                        ? module
                                        : stacktrace_utils::getBaseName(module);
        result += std::format(" in {}", modName);
        if (offset > 0) {
            result += std::format(" (+{:x})", offset);
        }
    }

    // Source information
    if (config.includeSourceInfo && !sourceFile.empty() && sourceLine > 0) {
        result += std::format(
            " ({}:{})", stacktrace_utils::getBaseName(sourceFile), sourceLine);
    }

    return result;
}

}  // namespace atom::error
