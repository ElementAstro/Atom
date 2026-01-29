/*
 * external_backends.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: External stacktrace backend implementations
             Supports: cpptrace, backward-cpp, boost::stacktrace,
                       libunwind, execinfo, std::stacktrace (C++23)

**************************************************/

#ifndef ATOM_ERROR_STACKTRACE_EXTERNAL_BACKENDS_HPP
#define ATOM_ERROR_STACKTRACE_EXTERNAL_BACKENDS_HPP

#include "backend_interface.hpp"

// ============================================================================
// External library detection macros
// ============================================================================

// cpptrace - Modern C++ stack trace library
#ifdef ATOM_USE_CPPTRACE
#define ATOM_STACKTRACE_BACKEND_CPPTRACE
#endif

// backward-cpp - Stack trace library with source snippets
#ifdef ATOM_USE_BACKWARD_CPP
#define ATOM_STACKTRACE_BACKEND_BACKWARD
#endif

// Boost.Stacktrace - Boost stack trace library
#ifdef ATOM_USE_BOOST_STACKTRACE
#define ATOM_STACKTRACE_BACKEND_BOOST
#endif

// libunwind - Portable unwinding library
#ifdef ATOM_USE_LIBUNWIND
#define ATOM_STACKTRACE_BACKEND_LIBUNWIND
#endif

// execinfo - POSIX backtrace (glibc)
#ifdef ATOM_USE_EXECINFO
#define ATOM_STACKTRACE_BACKEND_EXECINFO
#endif

// libbacktrace - GCC's backtrace library
#ifdef ATOM_USE_LIBBACKTRACE
#define ATOM_STACKTRACE_BACKEND_LIBBACKTRACE
#endif

// std::stacktrace - C++23 standard library
#if defined(ATOM_USE_STD_STACKTRACE) || \
    (defined(__cpp_lib_stacktrace) && __cpp_lib_stacktrace >= 202011L)
#define ATOM_STACKTRACE_BACKEND_STD
#endif

// Abseil - Google's C++ library
#ifdef ATOM_USE_ABSEIL_STACKTRACE
#define ATOM_STACKTRACE_BACKEND_ABSEIL
#endif

namespace atom::error {
namespace backends {

// ============================================================================
// cpptrace backend
// ============================================================================
#ifdef ATOM_USE_CPPTRACE
/**
 * @brief cpptrace backend implementation
 *
 * High-quality cross-platform stack trace library with excellent symbol
 * resolution and source file information.
 *
 * @see https://github.com/jeremy-rifkin/cpptrace
 */
class CpptraceBackend : public StackTraceBackend {
public:
    std::vector<StackFrame> capture(const StackTraceConfig& config) override;
    std::string getName() const override;
    bool isAvailable() const override;
};
#endif

// ============================================================================
// backward-cpp backend
// ============================================================================
#ifdef ATOM_USE_BACKWARD_CPP
/**
 * @brief backward-cpp backend implementation
 *
 * Feature-rich stack trace library with source code snippets,
 * colorized output, and multiple unwinding backends.
 *
 * @see https://github.com/bombela/backward-cpp
 */
class BackwardBackend : public StackTraceBackend {
public:
    std::vector<StackFrame> capture(const StackTraceConfig& config) override;
    std::string getName() const override;
    bool isAvailable() const override;
};
#endif

// ============================================================================
// Boost.Stacktrace backend
// ============================================================================
#ifdef ATOM_USE_BOOST_STACKTRACE
/**
 * @brief boost::stacktrace backend implementation
 *
 * Part of Boost library, provides portable stack traces with
 * configurable backends (addr2line, libbacktrace, etc.)
 *
 * @see https://www.boost.org/doc/libs/release/doc/html/stacktrace.html
 */
class BoostBackend : public StackTraceBackend {
public:
    std::vector<StackFrame> capture(const StackTraceConfig& config) override;
    std::string getName() const override;
    bool isAvailable() const override;
};
#endif

// ============================================================================
// libunwind backend
// ============================================================================
#ifdef ATOM_USE_LIBUNWIND
/**
 * @brief libunwind backend implementation
 *
 * Portable and efficient C library for determining the call-chain
 * of a program. Works on many platforms including Linux, macOS, FreeBSD.
 *
 * @see https://www.nongnu.org/libunwind/
 */
class LibunwindBackend : public StackTraceBackend {
public:
    std::vector<StackFrame> capture(const StackTraceConfig& config) override;
    std::string getName() const override;
    bool isAvailable() const override;
};
#endif

// ============================================================================
// execinfo backend (POSIX backtrace)
// ============================================================================
#ifdef ATOM_USE_EXECINFO
/**
 * @brief execinfo/backtrace backend implementation
 *
 * Uses POSIX backtrace() and backtrace_symbols() functions.
 * Available on Linux (glibc), macOS, and some BSDs.
 *
 * @note Requires -rdynamic linker flag for symbol names
 */
class ExecinfoBackend : public StackTraceBackend {
public:
    std::vector<StackFrame> capture(const StackTraceConfig& config) override;
    std::string getName() const override;
    bool isAvailable() const override;
};
#endif

// ============================================================================
// libbacktrace backend
// ============================================================================
#ifdef ATOM_USE_LIBBACKTRACE
/**
 * @brief libbacktrace backend implementation
 *
 * GCC's backtrace library, provides DWARF-based symbol resolution.
 * Excellent for getting source file and line information.
 *
 * @see https://github.com/ianlancetaylor/libbacktrace
 */
class LibbacktraceBackend : public StackTraceBackend {
public:
    std::vector<StackFrame> capture(const StackTraceConfig& config) override;
    std::string getName() const override;
    bool isAvailable() const override;

private:
    static void errorCallback(void* data, const char* msg, int errnum);
    static int fullCallback(void* data, uintptr_t pc, const char* filename,
                            int lineno, const char* function);
};
#endif

// ============================================================================
// std::stacktrace backend (C++23)
// ============================================================================
#ifdef ATOM_STACKTRACE_BACKEND_STD
/**
 * @brief C++23 std::stacktrace backend implementation
 *
 * Uses the standard library's stacktrace facility introduced in C++23.
 * Provides portable stack traces without external dependencies.
 *
 * @note Requires C++23 compiler support and <stacktrace> header
 */
class StdStacktraceBackend : public StackTraceBackend {
public:
    std::vector<StackFrame> capture(const StackTraceConfig& config) override;
    std::string getName() const override;
    bool isAvailable() const override;
};
#endif

// ============================================================================
// Abseil backend
// ============================================================================
#ifdef ATOM_USE_ABSEIL_STACKTRACE
/**
 * @brief Abseil stacktrace backend implementation
 *
 * Uses Google's Abseil library for stack traces.
 * Provides efficient and portable stack unwinding.
 *
 * @see https://abseil.io/
 */
class AbseilBackend : public StackTraceBackend {
public:
    std::vector<StackFrame> capture(const StackTraceConfig& config) override;
    std::string getName() const override;
    bool isAvailable() const override;
};
#endif

}  // namespace backends
}  // namespace atom::error

#endif  // ATOM_ERROR_STACKTRACE_EXTERNAL_BACKENDS_HPP
