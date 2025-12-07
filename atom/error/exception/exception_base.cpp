/*
 * exception_base.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Base exception class implementation

**************************************************/

#include "exception_base.hpp"

#ifdef ATOM_USE_CPPTRACE
#include <cpptrace/cpptrace.hpp>
#endif
#ifdef ATOM_USE_BOOST_STACKTRACE
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
#ifdef ATOM_USE_CPPTRACE
        oss << "  Stack trace:\n" << cpptrace::generate();
#elif defined(ATOM_USE_BOOST_STACKTRACE)
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
