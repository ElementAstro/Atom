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
#include <array>
#include <atomic>
#include <mutex>
#include <regex>
#include <span>

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
    if (config.maxDepth > 0) [[likely]] {
        frames.emplace_back(StackFrame{.address = reinterpret_cast<void*>(
                                           reinterpret_cast<uintptr_t>(this)),
                                       .function = "<unknown function>",
                                       .module = "<unknown module>",
                                       .sourceFile = {},
                                       .sourceLine = 0});
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
    std::array<void*, MAX_FRAMES> framePtrs{};

    const int numFrames =
        backtrace(framePtrs.data(),
                  std::min(config.maxDepth + config.skipFrames, MAX_FRAMES));
    if (numFrames <= config.skipFrames) [[unlikely]] {
        return;
    }

    // Create a span for the frames we want to process
    const std::span adjustedFrames(
        framePtrs.data() + config.skipFrames,
        static_cast<size_t>(numFrames - config.skipFrames));

    char** symbols = backtrace_symbols(adjustedFrames.data(),
                                       static_cast<int>(adjustedFrames.size()));
    if (!symbols) [[unlikely]] {
        return;
    }

    frames.reserve(adjustedFrames.size());

    for (size_t i = 0; i < adjustedFrames.size(); ++i) {
        StackFrame frame{.address = adjustedFrames[i]};
        processUnixFrame(frame, symbols[i]);
        frames.emplace_back(std::move(frame));
    }

    free(symbols);
}

void BuiltinBackend::processUnixFrame(StackFrame& frame, const char* symbol) {
    if (Dl_info dlInfo; dladdr(frame.address, &dlInfo)) {
        if (dlInfo.dli_fname != nullptr) {
            frame.module = dlInfo.dli_fname;
        }

        if (dlInfo.dli_sname != nullptr) {
            frame.function = dlInfo.dli_sname;
        }

        if (dlInfo.dli_fbase != nullptr) {
            frame.offset = reinterpret_cast<uintptr_t>(frame.address) -
                           reinterpret_cast<uintptr_t>(dlInfo.dli_fbase);
        }
    }

    // Parse backtrace_symbols output for additional information
    if (symbol != nullptr && frame.function.empty()) {
        const std::string_view symbolView(symbol);

        // Try to extract function name from symbol string
        static const std::regex functionRegex(
            R"(.*\s+(.+)\s+\+\s+0x[0-9a-f]+)");
        if (std::smatch matches;
            std::regex_search(symbol, matches, functionRegex) &&
            matches.size() > 1) {
            frame.function = matches[1].str();
        }
    }
}
#endif

}  // namespace backends
}  // namespace atom::error
