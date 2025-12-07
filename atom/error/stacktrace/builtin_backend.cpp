/*
 * builtin_backend.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Built-in stacktrace backend implementation

**************************************************/

#include "builtin_backend.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <regex>

// Platform-specific includes
#ifdef _WIN32
// Windows stacktrace temporarily disabled due to header conflicts
#elif defined(__APPLE__) || defined(__linux__)
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef __linux__
#include <link.h>
#endif
#endif

namespace atom::error {

// Thread-safe initialization
namespace {
std::once_flag initFlag;
std::atomic<bool> initialized{false};

void ensureInitialized() {
    std::call_once(initFlag, []() { initialized = true; });
}
}  // namespace

namespace backends {

std::vector<StackFrame> BuiltinBackend::capture(
    const StackTraceConfig& config) {
    ensureInitialized();
    std::vector<StackFrame> frames;

#ifdef _WIN32
    // Minimal Windows fallback: generate a synthetic frame
    if (config.maxDepth > 0) {
        StackFrame frame;
        frame.address =
            reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(this));
        frame.function = "<unknown function>";
        frame.module = "<unknown module>";
        frame.sourceFile = "";
        frame.sourceLine = 0;
        frames.push_back(std::move(frame));
    }
#elif defined(__APPLE__) || defined(__linux__)
    captureUnix(frames, config);
#endif

    return frames;
}

std::string BuiltinBackend::getName() const { return "builtin"; }

bool BuiltinBackend::isAvailable() const { return true; }

#if defined(__APPLE__) || defined(__linux__)
void BuiltinBackend::captureUnix(std::vector<StackFrame>& frames,
                                 const StackTraceConfig& config) {
    constexpr int MAX_FRAMES = 256;
    void* framePtrs[MAX_FRAMES];

    int numFrames = backtrace(
        framePtrs, std::min(config.maxDepth + config.skipFrames, MAX_FRAMES));
    if (numFrames <= config.skipFrames) {
        return;
    }

    // Skip the requested number of frames
    void** adjustedFrames = framePtrs + config.skipFrames;
    int adjustedCount = numFrames - config.skipFrames;

    char** symbols = backtrace_symbols(adjustedFrames, adjustedCount);
    if (!symbols) {
        return;
    }

    frames.reserve(adjustedCount);

    for (int i = 0; i < adjustedCount; ++i) {
        StackFrame frame;
        frame.address = adjustedFrames[i];
        processUnixFrame(frame, symbols[i]);
        frames.push_back(std::move(frame));
    }

    free(symbols);
}

void BuiltinBackend::processUnixFrame(StackFrame& frame, const char* symbol) {
    Dl_info dlInfo;
    if (dladdr(frame.address, &dlInfo)) {
        if (dlInfo.dli_fname) {
            frame.module = dlInfo.dli_fname;
        }

        if (dlInfo.dli_sname) {
            frame.function = dlInfo.dli_sname;
        }

        if (dlInfo.dli_fbase) {
            frame.offset = reinterpret_cast<uintptr_t>(frame.address) -
                           reinterpret_cast<uintptr_t>(dlInfo.dli_fbase);
        }
    }

    // Parse backtrace_symbols output for additional information
    if (symbol && frame.function.empty()) {
        std::string symbolStr(symbol);

        // Try to extract function name from symbol string
        std::regex functionRegex(R"(.*\s+(.+)\s+\+\s+0x[0-9a-f]+)");
        std::smatch matches;
        if (std::regex_search(symbolStr, matches, functionRegex) &&
            matches.size() > 1) {
            frame.function = matches[1].str();
        }
    }
}
#endif

}  // namespace backends
}  // namespace atom::error
