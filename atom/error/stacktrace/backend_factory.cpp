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

namespace atom::error {

std::unique_ptr<StackTraceBackend> StackTraceBackendFactory::create(
    const std::string& name) {
    if (name == "auto") {
        return createBest();
    }

    // ========================================================================
    // High-quality external backends (best symbol resolution)
    // ========================================================================

#ifdef ATOM_USE_CPPTRACE
    if (name == "cpptrace") {
        return std::make_unique<backends::CpptraceBackend>();
    }
#endif

#ifdef ATOM_USE_BACKWARD_CPP
    if (name == "backward") {
        return std::make_unique<backends::BackwardBackend>();
    }
#endif

#ifdef ATOM_USE_BOOST_STACKTRACE
    if (name == "boost") {
        return std::make_unique<backends::BoostBackend>();
    }
#endif

    // ========================================================================
    // C++23 standard library backend
    // ========================================================================

#ifdef ATOM_STACKTRACE_BACKEND_STD
    if (name == "std" || name == "std::stacktrace") {
        return std::make_unique<backends::StdStacktraceBackend>();
    }
#endif

    // ========================================================================
    // System-level backends
    // ========================================================================

#ifdef ATOM_USE_LIBBACKTRACE
    if (name == "libbacktrace") {
        return std::make_unique<backends::LibbacktraceBackend>();
    }
#endif

#ifdef ATOM_USE_LIBUNWIND
    if (name == "libunwind") {
        return std::make_unique<backends::LibunwindBackend>();
    }
#endif

#ifdef ATOM_USE_EXECINFO
    if (name == "execinfo" || name == "backtrace") {
        return std::make_unique<backends::ExecinfoBackend>();
    }
#endif

#ifdef ATOM_USE_ABSEIL_STACKTRACE
    if (name == "abseil" || name == "absl") {
        return std::make_unique<backends::AbseilBackend>();
    }
#endif

    // ========================================================================
    // Built-in fallback backend
    // ========================================================================

    if (name == "builtin") {
        return std::make_unique<backends::BuiltinBackend>();
    }

    return nullptr;
}

std::vector<std::string> StackTraceBackendFactory::getAvailable() {
    std::vector<std::string> available;

    // High-quality external backends
#ifdef ATOM_USE_CPPTRACE
    available.push_back("cpptrace");
#endif

#ifdef ATOM_USE_BACKWARD_CPP
    available.push_back("backward");
#endif

#ifdef ATOM_USE_BOOST_STACKTRACE
    available.push_back("boost");
#endif

    // C++23 standard library
#ifdef ATOM_STACKTRACE_BACKEND_STD
    available.push_back("std");
#endif

    // System-level backends
#ifdef ATOM_USE_LIBBACKTRACE
    available.push_back("libbacktrace");
#endif

#ifdef ATOM_USE_LIBUNWIND
    available.push_back("libunwind");
#endif

#ifdef ATOM_USE_EXECINFO
    available.push_back("execinfo");
#endif

#ifdef ATOM_USE_ABSEIL_STACKTRACE
    available.push_back("abseil");
#endif

    // Built-in is always available
    available.push_back("builtin");

    return available;
}

std::unique_ptr<StackTraceBackend> StackTraceBackendFactory::createBest() {
    // Priority order for backends (best quality first)
    for (const auto& name : getBackendPriority()) {
        auto backend = create(name);
        if (backend && backend->isAvailable()) {
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
