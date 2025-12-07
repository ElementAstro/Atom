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

#include <sstream>

namespace atom::error {

std::string StackFrame::toString(const StackTraceConfig& config) const {
    std::ostringstream oss;

    // Function name
    std::string funcName = function.empty() ? config.unknownFunction : function;
    if (config.demangle && !function.empty()) {
        funcName = stacktrace_utils::demangle(funcName);
    }
    oss << funcName;

    // Memory address
    if (config.includeAddresses && address != nullptr) {
        oss << " at "
            << stacktrace_utils::formatAddress(
                   reinterpret_cast<uintptr_t>(address));
    }

    // Module information
    if (config.includeModules && !module.empty()) {
        std::string modName = module == config.unknownModule
                                  ? module
                                  : stacktrace_utils::getBaseName(module);
        oss << " in " << modName;
        if (offset > 0) {
            oss << " (+" << std::hex << offset << std::dec << ")";
        }
    }

    // Source information
    if (config.includeSourceInfo && !sourceFile.empty() && sourceLine > 0) {
        oss << " (" << stacktrace_utils::getBaseName(sourceFile) << ":"
            << sourceLine << ")";
    }

    return oss.str();
}

}  // namespace atom::error
