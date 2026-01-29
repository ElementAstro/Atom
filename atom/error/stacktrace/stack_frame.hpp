/*
 * stack_frame.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Stack frame structure and configuration

**************************************************/

#ifndef ATOM_ERROR_STACKTRACE_STACK_FRAME_HPP
#define ATOM_ERROR_STACKTRACE_STACK_FRAME_HPP

#include <cstdint>
#include <functional>
#include <string>

namespace atom::error {

/**
 * @brief Configuration options for stacktrace capture and formatting
 */
struct StackTraceConfig {
    int maxDepth = 128;              ///< Maximum number of frames to capture
    int skipFrames = 1;              ///< Number of frames to skip from the top
    bool includeAddresses = true;    ///< Include memory addresses in output
    bool includeModules = true;      ///< Include module/library names
    bool includeSourceInfo = true;   ///< Include source file and line numbers
    bool demangle = true;            ///< Demangle C++ function names
    bool prettify = true;            ///< Apply prettification to output
    std::string framePrefix = "\t";  ///< Prefix for each frame line
    std::string unknownFunction =
        "<unknown function>";  ///< Placeholder for unknown functions
    std::string unknownModule =
        "<unknown module>";  ///< Placeholder for unknown modules

    /**
     * @brief Custom frame filter function
     * @param frameInfo String representation of the frame
     * @param frameIndex Index of the frame (0-based)
     * @return true if frame should be included, false to filter out
     */
    std::function<bool(const std::string&, int)> frameFilter;
};

/**
 * @brief Information about a single stack frame
 */
struct StackFrame {
    void* address = nullptr;  ///< Memory address of the frame
    std::string function;     ///< Function name (demangled if available)
    std::string module;       ///< Module/library name
    std::string sourceFile;   ///< Source file name
    int sourceLine = 0;       ///< Source line number
    uintptr_t offset = 0;     ///< Offset within the function/module

    /**
     * @brief Convert frame to string representation
     */
    std::string toString(const StackTraceConfig& config = {}) const;
};

}  // namespace atom::error

#endif  // ATOM_ERROR_STACKTRACE_STACK_FRAME_HPP
