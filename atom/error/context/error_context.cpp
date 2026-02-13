/*
 * error_context.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Error context implementation

**************************************************/

#include "error_context.hpp"
#include "context_manager.hpp"

#include "atom/utils/random/random.hpp"

#include <algorithm>
#include <format>
#include <iomanip>
#include <ranges>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on
#else
#include <sys/utsname.h>
#include <unistd.h>
#endif

namespace atom::error {

namespace {
constexpr std::string_view HEX_CHARS = "0123456789abcdef";
constexpr std::array DASH_POSITIONS = {8, 13, 18, 23};

[[nodiscard]] auto createHexDigit(auto& rng) -> char {
    return HEX_CHARS[static_cast<size_t>(rng())];
}
}  // namespace

ErrorId generateErrorId() {
    thread_local ::atom::utils::Random<std::mt19937,
                                       std::uniform_int_distribution<int>>
        rng(0, 15);

    std::string result(36, '0');
    size_t pos = 0;

    for (size_t i = 0; i < 32; ++i) {
        if (std::ranges::contains(DASH_POSITIONS, pos)) {
            result[pos++] = '-';
        }
        result[pos++] = createHexDigit(rng);
    }
    return result;
}

ErrorContext::ErrorContext(int errorCode, std::string message)
    : errorId_(generateErrorId()),
      errorCode_(errorCode),
      message_(std::move(message)),
      timestamp_(std::chrono::system_clock::now()),
      threadId_(std::this_thread::get_id()),
      metadata_(ErrorCodeMapper::getMetadata(errorCode)),
      retryCount_(0),
      maxRetries_(metadata_.maxRetries) {
    initializeSystemInfo();
}

ErrorContext::ErrorContext(const ErrorContext& other) {
    std::scoped_lock lock(other.mutex_);
    errorId_ = other.errorId_;
    errorCode_ = other.errorCode_;
    message_ = other.message_;
    timestamp_ = other.timestamp_;
    threadId_ = other.threadId_;
    metadata_ = other.metadata_;
    userData_ = other.userData_;
    systemInfo_ = other.systemInfo_;
    tags_ = other.tags_;
    correlationId_ = other.correlationId_;
    parentErrorId_ = other.parentErrorId_;
    childErrorIds_ = other.childErrorIds_;
    retryCount_ = other.retryCount_;
    maxRetries_ = other.maxRetries_;
    stackTrace_ = other.stackTrace_;
}

ErrorContext::ErrorContext(ErrorContext&& other) noexcept {
    std::scoped_lock lock(other.mutex_);
    errorId_ = std::exchange(other.errorId_, {});
    errorCode_ = std::exchange(other.errorCode_, 0);
    message_ = std::exchange(other.message_, {});
    timestamp_ = std::exchange(other.timestamp_, {});
    threadId_ = std::exchange(other.threadId_, {});
    metadata_ = std::exchange(other.metadata_, {});
    userData_ = std::exchange(other.userData_, {});
    systemInfo_ = std::exchange(other.systemInfo_, {});
    tags_ = std::exchange(other.tags_, {});
    correlationId_ = std::exchange(other.correlationId_, {});
    parentErrorId_ = std::exchange(other.parentErrorId_, {});
    childErrorIds_ = std::exchange(other.childErrorIds_, {});
    retryCount_ = std::exchange(other.retryCount_, 0);
    maxRetries_ = std::exchange(other.maxRetries_, 0);
    stackTrace_ = std::exchange(other.stackTrace_, {});
}

ErrorContext& ErrorContext::operator=(const ErrorContext& other) {
    if (this != &other) {
        std::scoped_lock lock(mutex_, other.mutex_);
        errorId_ = other.errorId_;
        errorCode_ = other.errorCode_;
        message_ = other.message_;
        timestamp_ = other.timestamp_;
        threadId_ = other.threadId_;
        metadata_ = other.metadata_;
        userData_ = other.userData_;
        systemInfo_ = other.systemInfo_;
        tags_ = other.tags_;
        correlationId_ = other.correlationId_;
        parentErrorId_ = other.parentErrorId_;
        childErrorIds_ = other.childErrorIds_;
        retryCount_ = other.retryCount_;
        maxRetries_ = other.maxRetries_;
        stackTrace_ = other.stackTrace_;
    }
    return *this;
}

ErrorContext& ErrorContext::operator=(ErrorContext&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(mutex_, other.mutex_);
        errorId_ = std::exchange(other.errorId_, {});
        errorCode_ = std::exchange(other.errorCode_, 0);
        message_ = std::exchange(other.message_, {});
        timestamp_ = std::exchange(other.timestamp_, {});
        threadId_ = std::exchange(other.threadId_, {});
        metadata_ = std::exchange(other.metadata_, {});
        userData_ = std::exchange(other.userData_, {});
        systemInfo_ = std::exchange(other.systemInfo_, {});
        tags_ = std::exchange(other.tags_, {});
        correlationId_ = std::exchange(other.correlationId_, {});
        parentErrorId_ = std::exchange(other.parentErrorId_, {});
        childErrorIds_ = std::exchange(other.childErrorIds_, {});
        retryCount_ = std::exchange(other.retryCount_, 0);
        maxRetries_ = std::exchange(other.maxRetries_, 0);
        stackTrace_ = std::exchange(other.stackTrace_, {});
    }
    return *this;
}

auto ErrorContext::setUserData(const std::string& key,
                               std::any value) -> ErrorContext& {
    std::scoped_lock lock(mutex_);
    userData_.insert_or_assign(key, std::move(value));
    return *this;
}

auto ErrorContext::getUserData(const std::string& key) const -> std::any {
    std::scoped_lock lock(mutex_);
    if (auto it = userData_.find(key); it != userData_.end()) {
        return it->second;
    }
    return {};
}

auto ErrorContext::hasUserData(const std::string& key) const -> bool {
    std::scoped_lock lock(mutex_);
    return userData_.contains(key);
}

auto ErrorContext::setSystemInfo(const std::string& key,
                                 std::string value) -> ErrorContext& {
    std::scoped_lock lock(mutex_);
    systemInfo_.insert_or_assign(key, std::move(value));
    return *this;
}

auto ErrorContext::getSystemInfo(const std::string& key) const -> std::string {
    std::scoped_lock lock(mutex_);
    if (auto it = systemInfo_.find(key); it != systemInfo_.end()) {
        return it->second;
    }
    return {};
}

auto ErrorContext::addTag(const std::string& tag) -> ErrorContext& {
    std::scoped_lock lock(mutex_);
    if (!std::ranges::contains(tags_, tag)) {
        tags_.emplace_back(tag);
    }
    return *this;
}

auto ErrorContext::getTags() const -> const std::vector<std::string>& {
    return tags_;
}

auto ErrorContext::hasTag(const std::string& tag) const -> bool {
    std::scoped_lock lock(mutex_);
    return std::ranges::contains(tags_, tag);
}

auto ErrorContext::setCorrelationId(const std::string& correlationId)
    -> ErrorContext& {
    std::scoped_lock lock(mutex_);
    correlationId_ = correlationId;
    return *this;
}

auto ErrorContext::getCorrelationId() const -> const std::string& {
    return correlationId_;
}

auto ErrorContext::setParentErrorId(const ErrorId& parentId) -> ErrorContext& {
    std::scoped_lock lock(mutex_);
    parentErrorId_ = parentId;
    return *this;
}

auto ErrorContext::getParentErrorId() const -> const ErrorId& {
    return parentErrorId_;
}

auto ErrorContext::addChildErrorId(const ErrorId& childId) -> ErrorContext& {
    std::scoped_lock lock(mutex_);
    childErrorIds_.emplace_back(childId);
    return *this;
}

auto ErrorContext::getChildErrorIds() const -> const std::vector<ErrorId>& {
    return childErrorIds_;
}

auto ErrorContext::incrementRetryCount() -> ErrorContext& {
    std::scoped_lock lock(mutex_);
    ++retryCount_;
    return *this;
}

auto ErrorContext::getRetryCount() const -> int { return retryCount_; }

auto ErrorContext::setMaxRetries(int maxRetries) -> ErrorContext& {
    std::scoped_lock lock(mutex_);
    maxRetries_ = maxRetries;
    return *this;
}

auto ErrorContext::getMaxRetries() const -> int { return maxRetries_; }

auto ErrorContext::canRetry() const -> bool {
    return retryCount_ < maxRetries_ &&
           (metadata_.recovery == ErrorRecoveryStrategy::Retry);
}

auto ErrorContext::setStackTrace(const std::string& stackTrace)
    -> ErrorContext& {
    std::scoped_lock lock(mutex_);
    stackTrace_ = stackTrace;
    return *this;
}

auto ErrorContext::getStackTrace() const -> const std::string& {
    return stackTrace_;
}

void ErrorContext::initializeSystemInfo() {
#ifdef _WIN32
    systemInfo_.emplace("pid", std::to_string(GetCurrentProcessId()));
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    systemInfo_.emplace("cpu_count",
                        std::to_string(sysInfo.dwNumberOfProcessors));
#else
    systemInfo_.emplace("pid", std::to_string(getpid()));
    systemInfo_.emplace("cpu_count",
                        std::to_string(sysconf(_SC_NPROCESSORS_ONLN)));

    if (struct utsname unameData; uname(&unameData) == 0) {
        systemInfo_.emplace("os_name", unameData.sysname);
        systemInfo_.emplace("os_version", unameData.release);
        systemInfo_.emplace("hostname", unameData.nodename);
    }
#endif

    std::ostringstream oss;
    oss << threadId_;
    systemInfo_.emplace("thread_id", oss.str());
}

auto ErrorContext::create(int errorCode, const std::string& message)
    -> std::shared_ptr<ErrorContext> {
    auto context = std::make_shared<ErrorContext>(errorCode, message);
    ErrorContextManager::getInstance().registerContext(context);
    return context;
}

auto ErrorContext::createWithCorrelation(
    int errorCode, const std::string& correlationId,
    const std::string& message) -> std::shared_ptr<ErrorContext> {
    auto context = create(errorCode, message);
    context->setCorrelationId(correlationId);
    return context;
}

auto ErrorContext::toJson() const -> std::string {
    std::scoped_lock lock(mutex_);

    auto timeT = std::chrono::system_clock::to_time_t(timestamp_);
    std::ostringstream timeOss;
    timeOss << std::put_time(std::gmtime(&timeT), "%Y-%m-%dT%H:%M:%SZ");

    std::string sysInfoJson;
    for (bool first = true; const auto& [key, value] : systemInfo_) {
        if (!first) {
            sysInfoJson += ",\n";
        }
        sysInfoJson += std::format("    \"{}\": \"{}\"", key, value);
        first = false;
    }

    std::string tagsJson;
    for (bool first = true; const auto& tag : tags_) {
        if (!first) {
            tagsJson += ", ";
        }
        tagsJson += std::format("\"{}\"", tag);
        first = false;
    }

    return std::format(
        R"({{
  "errorId": "{}",
  "errorCode": {},
  "message": "{}",
  "severity": "{}",
  "category": "{}",
  "recovery": "{}",
  "timestamp": "{}",
  "systemInfo": {{
{}
  }},
  "tags": [{}],
  "correlationId": "{}",
  "parentErrorId": "{}",
  "retryCount": {},
  "maxRetries": {}
}})",
        errorId_, errorCode_, message_, severityToString(metadata_.severity),
        categoryToString(metadata_.category),
        recoveryStrategyToString(metadata_.recovery), timeOss.str(),
        sysInfoJson, tagsJson, correlationId_, parentErrorId_, retryCount_,
        maxRetries_);
}

auto ErrorContext::toString() const -> std::string {
    std::scoped_lock lock(mutex_);

    auto timeT = std::chrono::system_clock::to_time_t(timestamp_);
    std::ostringstream timeOss;
    timeOss << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S");

    std::string result = std::format(
        "Error Context [{}]\n"
        "  Code: {}\n"
        "  Message: {}\n"
        "  Severity: {}\n"
        "  Category: {}\n"
        "  Recovery: {}\n"
        "  Timestamp: {}\n",
        errorId_, errorCode_, message_, severityToString(metadata_.severity),
        categoryToString(metadata_.category),
        recoveryStrategyToString(metadata_.recovery), timeOss.str());

    if (!correlationId_.empty()) {
        result += std::format("  Correlation ID: {}\n", correlationId_);
    }

    if (!parentErrorId_.empty()) {
        result += std::format("  Parent Error: {}\n", parentErrorId_);
    }

    if (!tags_.empty()) {
        result += "  Tags: ";
        for (bool first = true; const auto& tag : tags_) {
            if (!first) {
                result += ", ";
            }
            result += tag;
            first = false;
        }
        result += '\n';
    }

    result += std::format("  Retry: {}/{}\n", retryCount_, maxRetries_);

    if (!stackTrace_.empty()) {
        result += std::format("  Stack Trace:\n{}\n", stackTrace_);
    }

    return result;
}

}  // namespace atom::error
