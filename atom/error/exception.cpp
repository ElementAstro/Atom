/*
 * exception.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-10

Description: Better Exception Library

**************************************************/

#include "exception.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if ENABLE_CPPTRACE
#include <cpptrace/cpptrace.hpp>
#endif
#ifdef ENABLE_BOOST_STACKTRACE
#include <boost/stacktrace.hpp>
#endif

namespace atom::error {
auto Exception::what() const noexcept -> const char* {
    if (full_message_.empty()) {
        std::ostringstream oss;
        oss << "Exception occurred:\n";
        oss << "  File: " << file_ << "\n";
        oss << "  Line: " << line_ << "\n";
        oss << "  Function: " << func_ << "()\n";
        oss << "  Thread ID: " << thread_id_ << "\n";
        oss << "  Message: " << message_ << "\n";
#if ENABLE_CPPTRACE
        oss << "  Stack trace:\n" << cpptrace::generate();
#elif defined(ENABLE_BOOST_STACKTRACE)
        oss << "  Stack trace:\n" << boost::stacktrace::to_string(stack_trace_);
#else
        oss << "  Stack trace:\n" << stack_trace_.toString();
#endif
        full_message_ = oss.str();
    }
    return full_message_.c_str();
}

auto Exception::getFile() const -> std::string { return file_; }
auto Exception::getLine() const -> int { return line_; }
auto Exception::getFunction() const -> std::string { return func_; }
auto Exception::getMessage() const -> std::string { return message_; }
auto Exception::getThreadId() const -> std::thread::id { return thread_id_; }
}  // namespace atom::error
