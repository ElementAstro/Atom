/*
 * error_context.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Implementation of error context system

**************************************************/

#include "error_context.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <shared_mutex>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
#endif

namespace atom::error {

ErrorId generateErrorId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 32; ++i) {
        ss << dis(gen);
        if (i == 7 || i == 11 || i == 15 || i == 19) {
            ss << "-";
        }
    }
    return ss.str();
}

ErrorContext::ErrorContext(int errorCode, std::string message)
    : errorId_(generateErrorId())
    , errorCode_(errorCode)
    , message_(std::move(message))
    , timestamp_(std::chrono::system_clock::now())
    , threadId_(std::this_thread::get_id())
    , metadata_(ErrorCodeMapper::getMetadata(errorCode))
    , retryCount_(0)
    , maxRetries_(metadata_.maxRetries) {
    initializeSystemInfo();
}

ErrorContext::ErrorContext(const ErrorContext& other)
    : errorId_(other.errorId_)
    , errorCode_(other.errorCode_)
    , message_(other.message_)
    , timestamp_(other.timestamp_)
    , threadId_(other.threadId_)
    , metadata_(other.metadata_)
    , userData_(other.userData_)
    , systemInfo_(other.systemInfo_)
    , tags_(other.tags_)
    , correlationId_(other.correlationId_)
    , parentErrorId_(other.parentErrorId_)
    , childErrorIds_(other.childErrorIds_)
    , retryCount_(other.retryCount_)
    , maxRetries_(other.maxRetries_)
    , stackTrace_(other.stackTrace_) {
}

ErrorContext::ErrorContext(ErrorContext&& other) noexcept
    : errorId_(std::move(other.errorId_))
    , errorCode_(other.errorCode_)
    , message_(std::move(other.message_))
    , timestamp_(other.timestamp_)
    , threadId_(other.threadId_)
    , metadata_(std::move(other.metadata_))
    , userData_(std::move(other.userData_))
    , systemInfo_(std::move(other.systemInfo_))
    , tags_(std::move(other.tags_))
    , correlationId_(std::move(other.correlationId_))
    , parentErrorId_(std::move(other.parentErrorId_))
    , childErrorIds_(std::move(other.childErrorIds_))
    , retryCount_(other.retryCount_)
    , maxRetries_(other.maxRetries_)
    , stackTrace_(std::move(other.stackTrace_)) {
}

ErrorContext& ErrorContext::operator=(const ErrorContext& other) {
    if (this != &other) {
        std::lock_guard<std::mutex> lock(mutex_);
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
        std::lock_guard<std::mutex> lock(mutex_);
        errorId_ = std::move(other.errorId_);
        errorCode_ = other.errorCode_;
        message_ = std::move(other.message_);
        timestamp_ = other.timestamp_;
        threadId_ = other.threadId_;
        metadata_ = std::move(other.metadata_);
        userData_ = std::move(other.userData_);
        systemInfo_ = std::move(other.systemInfo_);
        tags_ = std::move(other.tags_);
        correlationId_ = std::move(other.correlationId_);
        parentErrorId_ = std::move(other.parentErrorId_);
        childErrorIds_ = std::move(other.childErrorIds_);
        retryCount_ = other.retryCount_;
        maxRetries_ = other.maxRetries_;
        stackTrace_ = std::move(other.stackTrace_);
    }
    return *this;
}

auto ErrorContext::setUserData(const std::string& key, std::any value) -> ErrorContext& {
    std::lock_guard<std::mutex> lock(mutex_);
    userData_[key] = std::move(value);
    return *this;
}

auto ErrorContext::getUserData(const std::string& key) const -> std::any {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = userData_.find(key);
    return it != userData_.end() ? it->second : std::any{};
}

auto ErrorContext::hasUserData(const std::string& key) const -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    return userData_.find(key) != userData_.end();
}

auto ErrorContext::setSystemInfo(const std::string& key, std::string value) -> ErrorContext& {
    std::lock_guard<std::mutex> lock(mutex_);
    systemInfo_[key] = std::move(value);
    return *this;
}

auto ErrorContext::getSystemInfo(const std::string& key) const -> std::string {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = systemInfo_.find(key);
    return it != systemInfo_.end() ? it->second : "";
}

auto ErrorContext::addTag(const std::string& tag) -> ErrorContext& {
    std::lock_guard<std::mutex> lock(mutex_);
    if (std::find(tags_.begin(), tags_.end(), tag) == tags_.end()) {
        tags_.push_back(tag);
    }
    return *this;
}

auto ErrorContext::getTags() const -> const std::vector<std::string>& {
    return tags_;
}

auto ErrorContext::hasTag(const std::string& tag) const -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::find(tags_.begin(), tags_.end(), tag) != tags_.end();
}

auto ErrorContext::setCorrelationId(const std::string& correlationId) -> ErrorContext& {
    std::lock_guard<std::mutex> lock(mutex_);
    correlationId_ = correlationId;
    return *this;
}

auto ErrorContext::getCorrelationId() const -> const std::string& {
    return correlationId_;
}

auto ErrorContext::setParentErrorId(const ErrorId& parentId) -> ErrorContext& {
    std::lock_guard<std::mutex> lock(mutex_);
    parentErrorId_ = parentId;
    return *this;
}

auto ErrorContext::getParentErrorId() const -> const ErrorId& {
    return parentErrorId_;
}

auto ErrorContext::addChildErrorId(const ErrorId& childId) -> ErrorContext& {
    std::lock_guard<std::mutex> lock(mutex_);
    childErrorIds_.push_back(childId);
    return *this;
}

auto ErrorContext::getChildErrorIds() const -> const std::vector<ErrorId>& {
    return childErrorIds_;
}

auto ErrorContext::incrementRetryCount() -> ErrorContext& {
    std::lock_guard<std::mutex> lock(mutex_);
    ++retryCount_;
    return *this;
}

auto ErrorContext::getRetryCount() const -> int {
    return retryCount_;
}

auto ErrorContext::setMaxRetries(int maxRetries) -> ErrorContext& {
    std::lock_guard<std::mutex> lock(mutex_);
    maxRetries_ = maxRetries;
    return *this;
}

auto ErrorContext::getMaxRetries() const -> int {
    return maxRetries_;
}

auto ErrorContext::canRetry() const -> bool {
    return retryCount_ < maxRetries_ && 
           (metadata_.recovery == ErrorRecoveryStrategy::Retry);
}

auto ErrorContext::setStackTrace(const std::string& stackTrace) -> ErrorContext& {
    std::lock_guard<std::mutex> lock(mutex_);
    stackTrace_ = stackTrace;
    return *this;
}

auto ErrorContext::getStackTrace() const -> const std::string& {
    return stackTrace_;
}

void ErrorContext::initializeSystemInfo() {
    // Get process ID
    #ifdef _WIN32
    systemInfo_["pid"] = std::to_string(GetCurrentProcessId());
    #else
    systemInfo_["pid"] = std::to_string(getpid());
    #endif
    
    // Get system information
    #ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    systemInfo_["cpu_count"] = std::to_string(sysInfo.dwNumberOfProcessors);
    #else
    systemInfo_["cpu_count"] = std::to_string(sysconf(_SC_NPROCESSORS_ONLN));
    
    struct utsname unameData;
    if (uname(&unameData) == 0) {
        systemInfo_["os_name"] = unameData.sysname;
        systemInfo_["os_version"] = unameData.release;
        systemInfo_["hostname"] = unameData.nodename;
    }
    #endif
    
    // Thread ID as string
    std::stringstream ss;
    ss << threadId_;
    systemInfo_["thread_id"] = ss.str();
}

auto ErrorContext::create(int errorCode, const std::string& message) -> std::shared_ptr<ErrorContext> {
    auto context = std::make_shared<ErrorContext>(errorCode, message);
    ErrorContextManager::getInstance().registerContext(context);
    return context;
}

auto ErrorContext::createWithCorrelation(int errorCode, const std::string& correlationId,
                                        const std::string& message) -> std::shared_ptr<ErrorContext> {
    auto context = create(errorCode, message);
    context->setCorrelationId(correlationId);
    return context;
}

auto ErrorContext::toJson() const -> std::string {
    std::lock_guard<std::mutex> lock(mutex_);
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"errorId\": \"" << errorId_ << "\",\n";
    ss << "  \"errorCode\": " << errorCode_ << ",\n";
    ss << "  \"message\": \"" << message_ << "\",\n";
    ss << "  \"severity\": \"" << severityToString(metadata_.severity) << "\",\n";
    ss << "  \"category\": \"" << categoryToString(metadata_.category) << "\",\n";
    ss << "  \"recovery\": \"" << recoveryStrategyToString(metadata_.recovery) << "\",\n";

    // Timestamp
    auto time_t = std::chrono::system_clock::to_time_t(timestamp_);
    ss << "  \"timestamp\": \"" << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ") << "\",\n";

    // System info
    ss << "  \"systemInfo\": {\n";
    bool first = true;
    for (const auto& [key, value] : systemInfo_) {
        if (!first) ss << ",\n";
        ss << "    \"" << key << "\": \"" << value << "\"";
        first = false;
    }
    ss << "\n  },\n";

    // Tags
    ss << "  \"tags\": [";
    first = true;
    for (const auto& tag : tags_) {
        if (!first) ss << ", ";
        ss << "\"" << tag << "\"";
        first = false;
    }
    ss << "],\n";

    ss << "  \"correlationId\": \"" << correlationId_ << "\",\n";
    ss << "  \"parentErrorId\": \"" << parentErrorId_ << "\",\n";
    ss << "  \"retryCount\": " << retryCount_ << ",\n";
    ss << "  \"maxRetries\": " << maxRetries_ << "\n";
    ss << "}";

    return ss.str();
}

auto ErrorContext::toString() const -> std::string {
    std::lock_guard<std::mutex> lock(mutex_);
    std::stringstream ss;

    ss << "Error Context [" << errorId_ << "]\n";
    ss << "  Code: " << errorCode_ << "\n";
    ss << "  Message: " << message_ << "\n";
    ss << "  Severity: " << severityToString(metadata_.severity) << "\n";
    ss << "  Category: " << categoryToString(metadata_.category) << "\n";
    ss << "  Recovery: " << recoveryStrategyToString(metadata_.recovery) << "\n";

    auto time_t = std::chrono::system_clock::to_time_t(timestamp_);
    ss << "  Timestamp: " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "\n";

    if (!correlationId_.empty()) {
        ss << "  Correlation ID: " << correlationId_ << "\n";
    }

    if (!parentErrorId_.empty()) {
        ss << "  Parent Error: " << parentErrorId_ << "\n";
    }

    if (!tags_.empty()) {
        ss << "  Tags: ";
        for (size_t i = 0; i < tags_.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << tags_[i];
        }
        ss << "\n";
    }

    ss << "  Retry: " << retryCount_ << "/" << maxRetries_ << "\n";

    if (!stackTrace_.empty()) {
        ss << "  Stack Trace:\n" << stackTrace_ << "\n";
    }

    return ss.str();
}

// ErrorContextManager implementation
auto ErrorContextManager::getInstance() -> ErrorContextManager& {
    static ErrorContextManager instance;
    return instance;
}

void ErrorContextManager::registerContext(std::shared_ptr<ErrorContext> context) {
    if (!context) return;

    std::unique_lock<std::shared_mutex> lock(contextsMutex_);
    contexts_[context->getErrorId()] = context;
}

auto ErrorContextManager::getContext(const ErrorId& errorId) const -> std::shared_ptr<ErrorContext> {
    std::shared_lock<std::shared_mutex> lock(contextsMutex_);
    auto it = contexts_.find(errorId);
    return it != contexts_.end() ? it->second : nullptr;
}

auto ErrorContextManager::getContextsByCorrelation(const std::string& correlationId) const
    -> std::vector<std::shared_ptr<ErrorContext>> {
    std::shared_lock<std::shared_mutex> lock(contextsMutex_);
    std::vector<std::shared_ptr<ErrorContext>> result;

    for (const auto& [id, context] : contexts_) {
        if (context && context->getCorrelationId() == correlationId) {
            result.push_back(context);
        }
    }

    return result;
}

auto ErrorContextManager::getStatistics() const -> std::unordered_map<std::string, int> {
    std::shared_lock<std::shared_mutex> lock(contextsMutex_);
    std::unordered_map<std::string, int> stats;

    stats["total_contexts"] = static_cast<int>(contexts_.size());

    std::unordered_map<ErrorSeverity, int> severityCount;
    std::unordered_map<ErrorCategory, int> categoryCount;

    for (const auto& [id, context] : contexts_) {
        if (context) {
            severityCount[context->getSeverity()]++;
            categoryCount[context->getCategory()]++;
        }
    }

    for (const auto& [severity, count] : severityCount) {
        stats[std::string(severityToString(severity))] = count;
    }

    for (const auto& [category, count] : categoryCount) {
        stats[std::string(categoryToString(category))] = count;
    }

    return stats;
}

void ErrorContextManager::cleanup(std::chrono::minutes maxAge) {
    std::unique_lock<std::shared_mutex> lock(contextsMutex_);
    auto cutoff = std::chrono::system_clock::now() - maxAge;

    auto it = contexts_.begin();
    while (it != contexts_.end()) {
        if (it->second && it->second->getTimestamp() < cutoff) {
            it = contexts_.erase(it);
        } else {
            ++it;
        }
    }
}

void ErrorContextManager::clear() {
    std::unique_lock<std::shared_mutex> lock(contextsMutex_);
    contexts_.clear();
}

// ScopedErrorContext implementation
ScopedErrorContext::ScopedErrorContext(std::shared_ptr<ErrorContext> context)
    : context_(std::move(context)) {
}

ScopedErrorContext::~ScopedErrorContext() {
    // Context is automatically managed by shared_ptr
}

auto ScopedErrorContext::getContext() const -> std::shared_ptr<ErrorContext> {
    return context_;
}

} // namespace atom::error
