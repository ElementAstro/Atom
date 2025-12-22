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

#include <format>

#ifdef ATOM_USE_CPPTRACE
#include <cpptrace/cpptrace.hpp>
#endif
#ifdef ATOM_USE_BOOST_STACKTRACE
#include <boost/stacktrace.hpp>
#endif

namespace atom::error {

auto Exception::what() const noexcept -> const char* {
    if (full_message_.empty()) {
        std::ostringstream tidOss;
        tidOss << thread_id_;

#ifdef ATOM_USE_CPPTRACE
        std::ostringstream stOss;
        stOss << cpptrace::generate();
        const auto stackStr = stOss.str();
#elif defined(ATOM_USE_BOOST_STACKTRACE)
        const auto stackStr = boost::stacktrace::to_string(stack_trace_);
#else
        const auto stackStr = stack_trace_.toString();
#endif

        full_message_ = std::format(
            "Exception occurred:\n"
            "  File: {}\n"
            "  Line: {}\n"
            "  Function: {}()\n"
            "  Thread ID: {}\n"
            "  Message: {}\n"
            "  Stack trace:\n{}",
            file_, line_, func_, tidOss.str(), message_, stackStr);
    }
    return full_message_.c_str();
}

auto Exception::getFile() const -> std::string { return file_; }
auto Exception::getLine() const -> int { return line_; }
auto Exception::getFunction() const -> std::string { return func_; }
auto Exception::getMessage() const -> std::string { return message_; }
auto Exception::getThreadId() const -> std::thread::id { return thread_id_; }

}  // namespace atom::error
