/*
 * external_backends.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: External stacktrace backend implementations
             Supports: cpptrace, backward-cpp, boost::stacktrace,
                       libunwind, execinfo, libbacktrace, std::stacktrace,
                       Abseil

**************************************************/

#include "external_backends.hpp"

#include <cstdlib>
#include <cstring>
#include <exception>

#include "stacktrace_utils.hpp"

// ============================================================================
// External library includes
// ============================================================================

#ifdef ATOM_USE_CPPTRACE
#include <cpptrace/cpptrace.hpp>
#endif

#ifdef ATOM_USE_BACKWARD_CPP
#include <backward.hpp>
#endif

#ifdef ATOM_USE_BOOST_STACKTRACE
#include <boost/stacktrace.hpp>
#endif

#ifdef ATOM_USE_LIBUNWIND
#define UNW_LOCAL_ONLY
#include <cxxabi.h>
#include <libunwind.h>
#endif

#ifdef ATOM_USE_EXECINFO
#include <cxxabi.h>
#include <execinfo.h>
#endif

#ifdef ATOM_USE_LIBBACKTRACE
#include <backtrace.h>
#endif

#ifdef ATOM_STACKTRACE_BACKEND_STD
#include <stacktrace>
#endif

#ifdef ATOM_USE_ABSEIL_STACKTRACE
#include <absl/debugging/stacktrace.h>
#include <absl/debugging/symbolize.h>
#endif

namespace atom::error {
namespace backends {

// ============================================================================
// cpptrace backend implementation
// ============================================================================
#ifdef ATOM_USE_CPPTRACE
std::vector<StackFrame> CpptraceBackend::capture(
    const StackTraceConfig& config) {
    std::vector<StackFrame> frames;

    try {
        auto trace =
            cpptrace::generate_trace(config.skipFrames, config.maxDepth);
        frames.reserve(trace.frames.size());

        for (const auto& cppFrame : trace.frames) {
            StackFrame frame;
            frame.address = reinterpret_cast<void*>(cppFrame.address);
            frame.function = cppFrame.symbol;
            frame.sourceFile = cppFrame.filename;
            frame.sourceLine = cppFrame.line.value_or(0);
            frame.module = cppFrame.object_path;

            frames.push_back(std::move(frame));
        }
    } catch (const std::exception&) {
        // Fall back to empty trace on error
    }

    return frames;
}

std::string CpptraceBackend::getName() const { return "cpptrace"; }

bool CpptraceBackend::isAvailable() const { return true; }
#endif

// ============================================================================
// backward-cpp backend implementation
// ============================================================================
#ifdef ATOM_USE_BACKWARD_CPP
std::vector<StackFrame> BackwardBackend::capture(
    const StackTraceConfig& config) {
    std::vector<StackFrame> frames;

    try {
        backward::StackTrace st;
        st.load_here(config.maxDepth + config.skipFrames);

        backward::TraceResolver resolver;
        resolver.load_stacktrace(st);

        // Skip frames as requested
        size_t startIdx =
            std::min(static_cast<size_t>(config.skipFrames), st.size());
        frames.reserve(st.size() - startIdx);

        for (size_t i = startIdx; i < st.size(); ++i) {
            StackFrame frame;
            frame.address = reinterpret_cast<void*>(st[i].addr);

            backward::ResolvedTrace trace = resolver.resolve(st[i]);

            if (!trace.source.function.empty()) {
                frame.function = trace.source.function;
            }
            if (!trace.source.filename.empty()) {
                frame.sourceFile = trace.source.filename;
                frame.sourceLine = static_cast<int>(trace.source.line);
            }
            if (!trace.object_filename.empty()) {
                frame.module = trace.object_filename;
            }

            frames.push_back(std::move(frame));
        }
    } catch (const std::exception&) {
        // Fall back to empty trace on error
    }

    return frames;
}

std::string BackwardBackend::getName() const { return "backward"; }

bool BackwardBackend::isAvailable() const { return true; }
#endif

// ============================================================================
// Boost.Stacktrace backend implementation
// ============================================================================
#ifdef ATOM_USE_BOOST_STACKTRACE
std::vector<StackFrame> BoostBackend::capture(const StackTraceConfig& config) {
    std::vector<StackFrame> frames;

    try {
        auto st =
            boost::stacktrace::stacktrace(config.skipFrames, config.maxDepth);
        frames.reserve(st.size());

        for (size_t i = 0; i < st.size(); ++i) {
            const auto& boostFrame = st[i];
            StackFrame frame;

            frame.address = const_cast<void*>(boostFrame.address());
            frame.function = boostFrame.name();
            frame.sourceFile = boostFrame.source_file();
            frame.sourceLine = static_cast<int>(boostFrame.source_line());

            frames.push_back(std::move(frame));
        }
    } catch (const std::exception&) {
        // Fall back to empty trace on error
    }

    return frames;
}

std::string BoostBackend::getName() const { return "boost"; }

bool BoostBackend::isAvailable() const { return true; }
#endif

// ============================================================================
// libunwind backend implementation
// ============================================================================
#ifdef ATOM_USE_LIBUNWIND
std::vector<StackFrame> LibunwindBackend::capture(
    const StackTraceConfig& config) {
    std::vector<StackFrame> frames;

    unw_cursor_t cursor;
    unw_context_t context;

    // Initialize cursor to current frame
    if (unw_getcontext(&context) != 0) {
        return frames;
    }

    if (unw_init_local(&cursor, &context) != 0) {
        return frames;
    }

    int frameCount = 0;
    int skipped = 0;

    while (unw_step(&cursor) > 0) {
        // Skip requested frames
        if (skipped < config.skipFrames) {
            skipped++;
            continue;
        }

        // Check max depth
        if (frameCount >= config.maxDepth) {
            break;
        }

        StackFrame frame;

        // Get instruction pointer
        unw_word_t ip;
        if (unw_get_reg(&cursor, UNW_REG_IP, &ip) == 0) {
            frame.address = reinterpret_cast<void*>(ip);
        }

        // Get function name
        char funcName[512];
        unw_word_t offset;
        if (unw_get_proc_name(&cursor, funcName, sizeof(funcName), &offset) ==
            0) {
            frame.function = StackTraceUtils::demangle(funcName);
            frame.offset = static_cast<size_t>(offset);
        }

        frames.push_back(std::move(frame));
        frameCount++;
    }

    return frames;
}

std::string LibunwindBackend::getName() const { return "libunwind"; }

bool LibunwindBackend::isAvailable() const { return true; }
#endif

// ============================================================================
// execinfo backend implementation
// ============================================================================
#ifdef ATOM_USE_EXECINFO
std::vector<StackFrame> ExecinfoBackend::capture(
    const StackTraceConfig& config) {
    std::vector<StackFrame> frames;

    constexpr int MAX_FRAMES = 128;
    void* buffer[MAX_FRAMES];

    int totalFrames = backtrace(buffer, MAX_FRAMES);
    if (totalFrames <= 0) {
        return frames;
    }

    char** symbols = backtrace_symbols(buffer, totalFrames);
    if (symbols == nullptr) {
        return frames;
    }

    int startIdx = config.skipFrames;
    int endIdx = std::min(totalFrames, startIdx + config.maxDepth);

    frames.reserve(endIdx - startIdx);

    for (int i = startIdx; i < endIdx; ++i) {
        StackFrame frame;
        frame.address = buffer[i];

        // Parse symbol string: "module(function+offset) [address]"
        std::string symbolStr(symbols[i]);

        // Try to extract function name
        size_t parenOpen = symbolStr.find('(');
        size_t parenClose = symbolStr.find(')');
        size_t plusSign = symbolStr.find('+');

        if (parenOpen != std::string::npos) {
            frame.module = symbolStr.substr(0, parenOpen);
        }

        if (parenOpen != std::string::npos && plusSign != std::string::npos &&
            plusSign > parenOpen) {
            std::string mangledName =
                symbolStr.substr(parenOpen + 1, plusSign - parenOpen - 1);
            if (!mangledName.empty()) {
                frame.function = StackTraceUtils::demangle(mangledName);
            }
        }

        if (plusSign != std::string::npos && parenClose != std::string::npos &&
            parenClose > plusSign) {
            std::string offsetStr =
                symbolStr.substr(plusSign + 1, parenClose - plusSign - 1);
            try {
                frame.offset = std::stoull(offsetStr, nullptr, 16);
            } catch (...) {
                frame.offset = 0;
            }
        }

        frames.push_back(std::move(frame));
    }

    free(symbols);
    return frames;
}

std::string ExecinfoBackend::getName() const { return "execinfo"; }

bool ExecinfoBackend::isAvailable() const { return true; }
#endif

// ============================================================================
// libbacktrace backend implementation
// ============================================================================
#ifdef ATOM_USE_LIBBACKTRACE
namespace {
struct BacktraceData {
    std::vector<StackFrame>* frames;
    int skipFrames;
    int maxDepth;
    int currentFrame;
};
}  // namespace

void LibbacktraceBackend::errorCallback(void* data, const char* msg,
                                        int errnum) {
    // Silently ignore errors
    (void)data;
    (void)msg;
    (void)errnum;
}

int LibbacktraceBackend::fullCallback(void* data, uintptr_t pc,
                                      const char* filename, int lineno,
                                      const char* function) {
    auto* btData = static_cast<BacktraceData*>(data);

    // Skip frames
    if (btData->currentFrame < btData->skipFrames) {
        btData->currentFrame++;
        return 0;
    }

    // Check max depth
    if (static_cast<int>(btData->frames->size()) >= btData->maxDepth) {
        return 1;  // Stop iteration
    }

    StackFrame frame;
    frame.address = reinterpret_cast<void*>(pc);

    if (function != nullptr) {
        frame.function = StackTraceUtils::demangle(function);
    }

    if (filename != nullptr) {
        frame.sourceFile = filename;
        frame.sourceLine = lineno;
    }

    btData->frames->push_back(std::move(frame));
    btData->currentFrame++;

    return 0;
}

std::vector<StackFrame> LibbacktraceBackend::capture(
    const StackTraceConfig& config) {
    std::vector<StackFrame> frames;

    static backtrace_state* state = backtrace_create_state(
        nullptr, 1 /* threaded */, errorCallback, nullptr);

    if (state == nullptr) {
        return frames;
    }

    BacktraceData data;
    data.frames = &frames;
    data.skipFrames = config.skipFrames;
    data.maxDepth = config.maxDepth;
    data.currentFrame = 0;

    backtrace_full(state, 0, fullCallback, errorCallback, &data);

    return frames;
}

std::string LibbacktraceBackend::getName() const { return "libbacktrace"; }

bool LibbacktraceBackend::isAvailable() const { return true; }
#endif

// ============================================================================
// std::stacktrace backend implementation (C++23)
// ============================================================================
#ifdef ATOM_STACKTRACE_BACKEND_STD
std::vector<StackFrame> StdStacktraceBackend::capture(
    const StackTraceConfig& config) {
    std::vector<StackFrame> frames;

    try {
        auto st = std::stacktrace::current(config.skipFrames, config.maxDepth);
        frames.reserve(st.size());

        for (const auto& entry : st) {
            StackFrame frame;

            // std::stacktrace_entry provides description and source_file/line
            frame.function = entry.description();
            frame.sourceFile = entry.source_file();
            frame.sourceLine = static_cast<int>(entry.source_line());

            // Native handle gives us the address
            frame.address = reinterpret_cast<void*>(entry.native_handle());

            frames.push_back(std::move(frame));
        }
    } catch (const std::exception&) {
        // Fall back to empty trace on error
    }

    return frames;
}

std::string StdStacktraceBackend::getName() const { return "std"; }

bool StdStacktraceBackend::isAvailable() const { return true; }
#endif

// ============================================================================
// Abseil backend implementation
// ============================================================================
#ifdef ATOM_USE_ABSEIL_STACKTRACE
std::vector<StackFrame> AbseilBackend::capture(const StackTraceConfig& config) {
    std::vector<StackFrame> frames;

    constexpr int MAX_FRAMES = 128;
    void* buffer[MAX_FRAMES];
    int sizes[MAX_FRAMES];

    int totalFrames = absl::GetStackTraceWithContext(
        buffer, sizes, MAX_FRAMES, config.skipFrames, nullptr, nullptr);

    if (totalFrames <= 0) {
        return frames;
    }

    int endIdx = std::min(totalFrames, config.maxDepth);
    frames.reserve(endIdx);

    for (int i = 0; i < endIdx; ++i) {
        StackFrame frame;
        frame.address = buffer[i];

        // Try to symbolize
        char symbolBuffer[1024];
        if (absl::Symbolize(buffer[i], symbolBuffer, sizeof(symbolBuffer))) {
            frame.function = StackTraceUtils::demangle(symbolBuffer);
        }

        frames.push_back(std::move(frame));
    }

    return frames;
}

std::string AbseilBackend::getName() const { return "abseil"; }

bool AbseilBackend::isAvailable() const { return true; }
#endif

}  // namespace backends
}  // namespace atom::error
