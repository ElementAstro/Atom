/*
 * backend_factory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Stacktrace backend factory implementation
             Supports multiple backends with priority-based selection

**************************************************/

#include "backend_interface.hpp"
#include "builtin_backend.hpp"
#include "external_backends.hpp"

#include <string_view>

namespace atom::error {

namespace {
using namespace std::string_view_literals;
}  // namespace

std::unique_ptr<StackTraceBackend> StackTraceBackendFactory::create(
    const std::string& name) {
    const std::string_view nameView = name;

    if (nameView == "auto"sv) {
        return createBest();
    }

    // ========================================================================
    // High-quality external backends (best symbol resolution)
    // ========================================================================

#ifdef ATOM_USE_CPPTRACE
    if (nameView == "cpptrace"sv) {
        return std::make_unique<backends::CpptraceBackend>();
    }
#endif

#ifdef ATOM_USE_BACKWARD_CPP
    if (nameView == "backward"sv) {
        return std::make_unique<backends::BackwardBackend>();
    }
#endif

#ifdef ATOM_USE_BOOST_STACKTRACE
    if (nameView == "boost"sv) {
        return std::make_unique<backends::BoostBackend>();
    }
#endif

    // ========================================================================
    // C++23 standard library backend
    // ========================================================================

#ifdef ATOM_STACKTRACE_BACKEND_STD
    if (nameView == "std"sv || nameView == "std::stacktrace"sv) {
        return std::make_unique<backends::StdStacktraceBackend>();
    }
#endif

    // ========================================================================
    // System-level backends
    // ========================================================================

#ifdef ATOM_USE_LIBBACKTRACE
    if (nameView == "libbacktrace"sv) {
        return std::make_unique<backends::LibbacktraceBackend>();
    }
#endif

#ifdef ATOM_USE_LIBUNWIND
    if (nameView == "libunwind"sv) {
        return std::make_unique<backends::LibunwindBackend>();
    }
#endif

#ifdef ATOM_USE_EXECINFO
    if (nameView == "execinfo"sv || nameView == "backtrace"sv) {
        return std::make_unique<backends::ExecinfoBackend>();
    }
#endif

#ifdef ATOM_USE_ABSEIL_STACKTRACE
    if (nameView == "abseil"sv || nameView == "absl"sv) {
        return std::make_unique<backends::AbseilBackend>();
    }
#endif

    // ========================================================================
    // Built-in fallback backend
    // ========================================================================

    if (nameView == "builtin"sv) {
        return std::make_unique<backends::BuiltinBackend>();
    }

    return nullptr;
}

std::vector<std::string> StackTraceBackendFactory::getAvailable() {
    std::vector<std::string> available;
    available.reserve(10);  // Pre-allocate for typical max backends

    // High-quality external backends
#ifdef ATOM_USE_CPPTRACE
    available.emplace_back("cpptrace");
#endif

#ifdef ATOM_USE_BACKWARD_CPP
    available.emplace_back("backward");
#endif

#ifdef ATOM_USE_BOOST_STACKTRACE
    available.emplace_back("boost");
#endif

    // C++23 standard library
#ifdef ATOM_STACKTRACE_BACKEND_STD
    available.emplace_back("std");
#endif

    // System-level backends
#ifdef ATOM_USE_LIBBACKTRACE
    available.emplace_back("libbacktrace");
#endif

#ifdef ATOM_USE_LIBUNWIND
    available.emplace_back("libunwind");
#endif

#ifdef ATOM_USE_EXECINFO
    available.emplace_back("execinfo");
#endif

#ifdef ATOM_USE_ABSEIL_STACKTRACE
    available.emplace_back("abseil");
#endif

    // Built-in is always available
    available.emplace_back("builtin");

    return available;
}

std::unique_ptr<StackTraceBackend> StackTraceBackendFactory::createBest() {
    // Priority order for backends (best quality first)
    for (const auto& name : getBackendPriority()) {
        if (auto backend = create(name); backend && backend->isAvailable())
            [[likely]] {
            return backend;
        }
    }

    // Fallback to builtin
    return std::make_unique<backends::BuiltinBackend>();
}

std::vector<std::string> StackTraceBackendFactory::getBackendPriority() {
    // Priority order:
    // 1. cpptrace - Best cross-platform support with excellent symbol
    // resolution
    // 2. backward-cpp - Great for development with source snippets
    // 3. std::stacktrace - C++23 standard, no external deps
    // 4. boost::stacktrace - Mature and well-tested
    // 5. libbacktrace - GCC's DWARF-based resolver
    // 6. libunwind - Efficient and portable
    // 7. execinfo - Basic POSIX support
    // 8. abseil - Google's implementation
    // 9. builtin - Platform-specific fallback

    return {
#ifdef ATOM_USE_CPPTRACE
        "cpptrace",
#endif
#ifdef ATOM_USE_BACKWARD_CPP
        "backward",
#endif
#ifdef ATOM_STACKTRACE_BACKEND_STD
        "std",
#endif
#ifdef ATOM_USE_BOOST_STACKTRACE
        "boost",
#endif
#ifdef ATOM_USE_LIBBACKTRACE
        "libbacktrace",
#endif
#ifdef ATOM_USE_LIBUNWIND
        "libunwind",
#endif
#ifdef ATOM_USE_EXECINFO
        "execinfo",
#endif
#ifdef ATOM_USE_ABSEIL_STACKTRACE
        "abseil",
#endif
        "builtin"};
}

}  // namespace atom::error
