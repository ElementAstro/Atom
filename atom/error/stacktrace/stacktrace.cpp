/*
 * stacktrace.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: StackTrace class implementation

**************************************************/

#include "stacktrace.hpp"
#include "stacktrace_utils.hpp"

#include <sstream>

namespace atom::error {

// Static member definitions
StackTraceConfig StackTrace::defaultConfig_;
std::string StackTrace::preferredBackend_ = "auto";

StackTrace::StackTrace() : config_(defaultConfig_) { capture(); }

StackTrace::StackTrace(const StackTraceConfig& config) : config_(config) {
    capture();
}

StackTrace::StackTrace(const StackTrace& other)
    : frames_(other.frames_),
      backendName_(other.backendName_),
      config_(other.config_) {}

StackTrace::StackTrace(StackTrace&& other) noexcept
    : frames_(std::move(other.frames_)),
      backendName_(std::move(other.backendName_)),
      config_(std::move(other.config_)) {}

StackTrace& StackTrace::operator=(const StackTrace& other) {
    if (this != &other) {
        frames_ = other.frames_;
        backendName_ = other.backendName_;
        config_ = other.config_;
    }
    return *this;
}

StackTrace& StackTrace::operator=(StackTrace&& other) noexcept {
    if (this != &other) {
        frames_ = std::move(other.frames_);
        backendName_ = std::move(other.backendName_);
        config_ = std::move(other.config_);
    }
    return *this;
}

std::string StackTrace::toString() const { return toString(config_); }

std::string StackTrace::toString(const StackTraceConfig& config) const {
    if (frames_.empty()) {
        return "Stack trace: <empty>\n";
    }

    std::ostringstream oss;
    oss << "Stack trace:\n";

    for (size_t i = 0; i < frames_.size(); ++i) {
        const auto& frame = frames_[i];
        std::string frameStr = frame.toString(config);

        // Apply frame filter if provided
        if (config.frameFilter &&
            !config.frameFilter(frameStr, static_cast<int>(i))) {
            continue;
        }

        oss << config.framePrefix << "[" << i << "] " << frameStr << "\n";
    }

    std::string result = oss.str();
    return config.prettify ? stacktrace_utils::prettify(result) : result;
}

const std::vector<StackFrame>& StackTrace::getFrames() const { return frames_; }

size_t StackTrace::size() const { return frames_.size(); }

bool StackTrace::empty() const { return frames_.empty(); }

std::string StackTrace::getBackendName() const { return backendName_; }

void StackTrace::setDefaultConfig(const StackTraceConfig& config) {
    defaultConfig_ = config;
}

const StackTraceConfig& StackTrace::getDefaultConfig() {
    return defaultConfig_;
}

std::vector<std::string> StackTrace::getAvailableBackends() {
    return StackTraceBackendFactory::getAvailable();
}

void StackTrace::setPreferredBackend(const std::string& backendName) {
    preferredBackend_ = backendName;
}

void StackTrace::capture() {
    auto backend = getBestBackend();
    if (backend) {
        backendName_ = backend->getName();
        frames_ = backend->capture(config_);
    } else {
        backendName_ = "none";
        frames_.clear();
    }
}

std::unique_ptr<StackTraceBackend> StackTrace::getBestBackend() {
    if (preferredBackend_ != "auto") {
        auto backend = StackTraceBackendFactory::create(preferredBackend_);
        if (backend && backend->isAvailable()) {
            return backend;
        }
    }

    return StackTraceBackendFactory::createBest();
}

std::unique_ptr<StackTraceBackend> StackTrace::createBackend(
    const std::string& name) {
    return StackTraceBackendFactory::create(name);
}

// Convenience functions
namespace stacktrace {

std::string current() {
    StackTrace trace;
    return trace.toString();
}

std::string current(int maxDepth) {
    StackTraceConfig config;
    config.maxDepth = maxDepth;
    StackTrace trace(config);
    return trace.toString();
}

std::string current(const StackTraceConfig& config) {
    StackTrace trace(config);
    return trace.toString();
}

}  // namespace stacktrace

}  // namespace atom::error
