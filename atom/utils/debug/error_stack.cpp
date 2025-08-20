/*
 * error_stack.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "error_stack.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <execution>
#include <set>
#include <sstream>

#include "atom/utils/time.hpp"

#ifdef ATOM_ERROR_STACK_USE_SERIALIZATION
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#endif

#include <spdlog/spdlog.h>

namespace atom::error {

std::string_view errorLevelToString(ErrorLevel level) {
    switch (level) {
        case ErrorLevel::Debug:
            return "Debug";
        case ErrorLevel::Info:
            return "Info";
        case ErrorLevel::Warning:
            return "Warning";
        case ErrorLevel::Error:
            return "Error";
        case ErrorLevel::Critical:
            return "Critical";
        default:
            return "Unknown";
    }
}

std::string_view errorCategoryToString(ErrorCategory category) {
    switch (category) {
        case ErrorCategory::General:
            return "General";
        case ErrorCategory::System:
            return "System";
        case ErrorCategory::Network:
            return "Network";
        case ErrorCategory::Database:
            return "Database";
        case ErrorCategory::Security:
            return "Security";
        case ErrorCategory::IO:
            return "IO";
        case ErrorCategory::Memory:
            return "Memory";
        case ErrorCategory::Configuration:
            return "Configuration";
        case ErrorCategory::Validation:
            return "Validation";
        case ErrorCategory::Other:
            return "Other";
        default:
            return "Unknown";
    }
}

std::ostream& operator<<(std::ostream& os, const ErrorInfo& error) {
    try {
        os << "{\n"
           << "  \"errorMessage\": \"" << error.errorMessage << "\",\n"
           << "  \"moduleName\": \"" << error.moduleName << "\",\n"
           << "  \"functionName\": \"" << error.functionName << "\",\n"
           << "  \"line\": " << error.line << ",\n"
           << "  \"fileName\": \"" << error.fileName << "\",\n"
           << "  \"timestamp\": \""
           << atom::utils::timeStampToString(error.timestamp) << "\",\n"
           << "  \"uuid\": \"" << error.uuid << "\",\n"
           << "  \"level\": \"" << std::string(errorLevelToString(error.level))
           << "\",\n"
           << "  \"category\": \""
           << std::string(errorCategoryToString(error.category)) << "\",\n"
           << "  \"errorCode\": " << error.errorCode;

        if (!error.metadata.empty()) {
            os << ",\n  \"metadata\": {\n";
            size_t count = 0;
            for (const auto& [key, value] : error.metadata) {
                os << "    \"" << key << "\": \"" << value << "\"";
                if (++count < error.metadata.size()) {
                    os << ",";
                }
                os << "\n";
            }
            os << "  }";
        }

        os << "\n}";
    } catch (const std::exception& e) {
        os << "{\"error\": \"Failed to format error info: " << e.what()
           << "\"}";
    }
    return os;
}

std::string operator<<([[maybe_unused]] const std::string& str,
                       const ErrorInfo& error) {
    try {
        std::stringstream ss;
        ss << error;
        return ss.str();
    } catch (const std::exception& e) {
        return std::string("{\"error\": \"Failed to format error info: ") +
               e.what() + "\"}";
    }
}

ErrorStack::ErrorStack() {
    errorStack_.reserve(128);
    compressedErrorStack_.reserve(64);
    statistics_.firstErrorTime = std::chrono::system_clock::now();
    statistics_.lastErrorTime = statistics_.firstErrorTime;
}

ErrorStack::~ErrorStack() {
#ifdef ATOM_ERROR_STACK_USE_BOOST_LOCKFREE
    stopAsyncProcessing();
#endif

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        errorStack_.clear();
        compressedErrorStack_.clear();
        filteredModules_.clear();
        moduleErrorCount_.clear();
        messageErrorCount_.clear();
        errorCallbacks_.clear();
    } catch (...) {
    }
}

ErrorStack::ErrorStack(ErrorStack&& other) noexcept {
    std::lock_guard<std::mutex> lock1(mutex_);
    std::lock_guard<std::mutex> lock2(other.mutex_);

    errorStack_ = std::move(other.errorStack_);
    compressedErrorStack_ = std::move(other.compressedErrorStack_);
    filteredModules_ = std::move(other.filteredModules_);
    moduleErrorCount_ = std::move(other.moduleErrorCount_);
    messageErrorCount_ = std::move(other.messageErrorCount_);
    statistics_ = std::move(other.statistics_);
    errorCallbacks_ = std::move(other.errorCallbacks_);

#ifdef ATOM_ERROR_STACK_USE_BOOST_LOCKFREE
    other.stopAsyncProcessing();
#endif
}

ErrorStack& ErrorStack::operator=(ErrorStack&& other) noexcept {
    if (this != &other) {
        std::lock_guard<std::mutex> lock1(mutex_);
        std::lock_guard<std::mutex> lock2(other.mutex_);

        errorStack_ = std::move(other.errorStack_);
        compressedErrorStack_ = std::move(other.compressedErrorStack_);
        filteredModules_ = std::move(other.filteredModules_);
        moduleErrorCount_ = std::move(other.moduleErrorCount_);
        messageErrorCount_ = std::move(other.messageErrorCount_);
        statistics_ = std::move(other.statistics_);
        errorCallbacks_ = std::move(other.errorCallbacks_);

#ifdef ATOM_ERROR_STACK_USE_BOOST_LOCKFREE
        other.stopAsyncProcessing();
#endif
    }
    return *this;
}

auto ErrorStack::createShared() -> std::shared_ptr<ErrorStack> {
    return std::make_shared<ErrorStack>();
}

auto ErrorStack::createUnique() -> std::unique_ptr<ErrorStack> {
    return std::make_unique<ErrorStack>();
}

bool ErrorStack::insertErrorInfo(const ErrorInfo& errorInfo) {
    try {
        if (errorInfo.errorMessage.empty()) {
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);

        auto iter = std::ranges::find_if(
            errorStack_, [&errorInfo](const ErrorInfo& error) {
                return error.errorMessage == errorInfo.errorMessage &&
                       error.moduleName == errorInfo.moduleName &&
                       error.functionName == errorInfo.functionName;
            });

        if (iter != errorStack_.end()) {
            iter->timestamp = errorInfo.timestamp;
            iter->level = errorInfo.level;
            iter->category = errorInfo.category;
            iter->errorCode = errorInfo.errorCode;

            for (const auto& [key, value] : errorInfo.metadata) {
                iter->metadata[key] = value;
            }
        } else {
            errorStack_.push_back(errorInfo);
            updateStatistics(errorStack_.back());

            for (const auto& callback : errorCallbacks_) {
                try {
                    callback(errorStack_.back());
                } catch (...) {
                }
            }
        }

        updateCompressedErrors();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

#ifdef ATOM_ERROR_STACK_USE_BOOST_LOCKFREE
bool ErrorStack::insertErrorAsync(const ErrorInfo& errorInfo) {
    try {
        asyncErrorQueue_.push(errorInfo);
        return true;
    } catch (...) {
        return false;
    }
}

size_t ErrorStack::processAsyncErrors() {
    size_t processedCount = 0;
    ErrorInfo error;

    while (asyncErrorQueue_.pop(error)) {
        insertErrorInfo(error);
        processedCount++;
    }

    return processedCount;
}

namespace {
std::atomic<bool> g_asyncProcessingRunning = false;
std::thread g_asyncProcessingThread;

void asyncProcessingThreadFunc(ErrorStack* stack, uint32_t intervalMs) {
    while (g_asyncProcessingRunning) {
        stack->processAsyncErrors();
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
    }
}
}  // namespace

void ErrorStack::startAsyncProcessing(uint32_t intervalMs) {
    if (!asyncProcessingActive_) {
        asyncProcessingActive_ = true;
        g_asyncProcessingRunning = true;

        if (g_asyncProcessingThread.joinable()) {
            g_asyncProcessingRunning = false;
            g_asyncProcessingThread.join();
        }

        g_asyncProcessingThread =
            std::thread(asyncProcessingThreadFunc, this, intervalMs);
    }
}

void ErrorStack::stopAsyncProcessing() {
    if (asyncProcessingActive_) {
        asyncProcessingActive_ = false;
        g_asyncProcessingRunning = false;

        if (g_asyncProcessingThread.joinable()) {
            g_asyncProcessingThread.join();
        }

        processAsyncErrors();
    }
}
#else
bool ErrorStack::insertErrorAsync(const ErrorInfo& errorInfo) {
    return insertErrorInfo(errorInfo);
}

size_t ErrorStack::processAsyncErrors() { return 0; }

void ErrorStack::startAsyncProcessing(uint32_t) {
    spdlog::warn(
        "Async error processing is not supported without Boost lockfree");
}

void ErrorStack::stopAsyncProcessing() {}
#endif

void ErrorStack::registerErrorCallback(ErrorCallback callback) {
    if (callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        errorCallbacks_.push_back(std::move(callback));
    }
}

void ErrorStack::clearFilteredModules() {
    std::lock_guard lock(mutex_);
    filteredModules_.clear();
}

void ErrorStack::printFilteredErrorStack() const {
    try {
        std::lock_guard lock(mutex_);

        for (const auto& error : errorStack_) {
            if (!std::ranges::contains(filteredModules_, error.moduleName)) {
                spdlog::error(
                    "{} [{}] [{}] {}",
                    std::string(errorLevelToString(error.level)),
                    std::string(errorCategoryToString(error.category)),
                    error.moduleName, error.errorMessage);
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to print error stack: {}", e.what());
    }
}

auto ErrorStack::getFilteredErrorsByModule(std::string_view moduleName) const
    -> std::vector<ErrorInfo> {
    try {
        std::lock_guard lock(mutex_);

        std::vector<ErrorInfo> errors;
        errors.reserve(errorStack_.size());

        std::string moduleNameStr(moduleName);
        std::copy_if(errorStack_.begin(), errorStack_.end(),
                     std::back_inserter(errors),
                     [&moduleNameStr, this](const ErrorInfo& error) {
                         return error.moduleName == moduleNameStr &&
                                !std::ranges::contains(filteredModules_,
                                                       error.moduleName);
                     });

        return errors;
    } catch (const std::exception& e) {
        spdlog::error("Failed to get filtered errors: {}", e.what());
        return {};
    }
}

auto ErrorStack::getFilteredErrorsByLevel(ErrorLevel level) const
    -> std::vector<ErrorInfo> {
    try {
        std::lock_guard lock(mutex_);

        std::vector<ErrorInfo> errors;
        errors.reserve(errorStack_.size() / 2);

        std::copy_if(errorStack_.begin(), errorStack_.end(),
                     std::back_inserter(errors),
                     [level, this](const ErrorInfo& error) {
                         return static_cast<int>(error.level) >=
                                    static_cast<int>(level) &&
                                !std::ranges::contains(filteredModules_,
                                                       error.moduleName);
                     });

        return errors;
    } catch (const std::exception& e) {
        spdlog::error("Failed to get errors by level: {}", e.what());
        return {};
    }
}

auto ErrorStack::getFilteredErrorsByCategory(ErrorCategory category) const
    -> std::vector<ErrorInfo> {
    try {
        std::lock_guard lock(mutex_);

        std::vector<ErrorInfo> errors;
        errors.reserve(errorStack_.size() / 5);

        std::copy_if(errorStack_.begin(), errorStack_.end(),
                     std::back_inserter(errors),
                     [category, this](const ErrorInfo& error) {
                         return error.category == category &&
                                !std::ranges::contains(filteredModules_,
                                                       error.moduleName);
                     });

        return errors;
    } catch (const std::exception& e) {
        spdlog::error("Failed to get errors by category: {}", e.what());
        return {};
    }
}

auto ErrorStack::getErrorsInTimeRange(time_t start, time_t end) const
    -> std::vector<ErrorInfo> {
    try {
        std::lock_guard lock(mutex_);

        std::vector<ErrorInfo> errors;

        if (start > end) {
            std::swap(start, end);
        }

        errors.reserve(errorStack_.size() / 3);

        std::copy_if(
            errorStack_.begin(), errorStack_.end(), std::back_inserter(errors),
            [start, end, this](const ErrorInfo& error) {
                return error.timestamp >= start && error.timestamp <= end &&
                       !std::ranges::contains(filteredModules_,
                                              error.moduleName);
            });

        return errors;
    } catch (const std::exception& e) {
        spdlog::error("Failed to get errors in time range: {}", e.what());
        return {};
    }
}

auto ErrorStack::getCompressedErrors() const -> std::string {
    try {
        std::lock_guard lock(mutex_);

        if (compressedErrorStack_.empty()) {
            return "";
        }

        std::stringstream compressedErrors;

        for (const auto& error : compressedErrorStack_) {
            compressedErrors
                << "[" << std::string(errorLevelToString(error.level)) << "] "
                << "[" << error.moduleName << "] " << error.errorMessage
                << " @ " << atom::utils::timeStampToString(error.timestamp)
                << "\n";
        }

        return compressedErrors.str();
    } catch (const std::exception& e) {
        spdlog::error("Failed to get compressed errors: {}", e.what());
        return "";
    }
}

void ErrorStack::updateCompressedErrors() {
    try {
        compressedErrorStack_.clear();
        compressedErrorStack_.reserve(errorStack_.size());

        for (const auto& error : errorStack_) {
            auto iter = std::ranges::find_if(
                compressedErrorStack_,
                [&error](const ErrorInfo& compressedError) {
                    return compressedError.errorMessage == error.errorMessage &&
                           compressedError.moduleName == error.moduleName;
                });

            if (iter != compressedErrorStack_.end()) {
                iter->timestamp = error.timestamp;
                iter->level = error.level;
                iter->category = error.category;
            } else {
                compressedErrorStack_.push_back(error);
            }
        }

        sortCompressedErrorStack();
    } catch (const std::exception& e) {
        spdlog::error("Failed to update compressed errors: {}", e.what());
        compressedErrorStack_.clear();
    }
}

void ErrorStack::sortCompressedErrorStack() {
    try {
        auto comparator = [](const ErrorInfo& error1, const ErrorInfo& error2) {
            if (error1.level != error2.level) {
                return static_cast<int>(error1.level) >
                       static_cast<int>(error2.level);
            }
            return error1.timestamp > error2.timestamp;
        };

        if (compressedErrorStack_.size() > 1000) {
            std::sort(std::execution::par_unseq, compressedErrorStack_.begin(),
                      compressedErrorStack_.end(), comparator);
        } else {
            std::sort(compressedErrorStack_.begin(),
                      compressedErrorStack_.end(), comparator);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to sort compressed error stack: {}", e.what());
    }
}

bool ErrorStack::isEmpty() const noexcept {
    std::lock_guard lock(mutex_);
    return errorStack_.empty();
}

size_t ErrorStack::size() const noexcept {
    std::lock_guard lock(mutex_);
    return errorStack_.size();
}

std::optional<ErrorInfo> ErrorStack::getLatestError() const {
    std::lock_guard lock(mutex_);
    if (errorStack_.empty()) {
        return std::nullopt;
    }

    auto latest = std::max_element(errorStack_.begin(), errorStack_.end(),
                                   [](const ErrorInfo& a, const ErrorInfo& b) {
                                       return a.timestamp < b.timestamp;
                                   });

    return *latest;
}

ErrorStatistics ErrorStack::getStatistics() const {
    std::lock_guard lock(mutex_);

    ErrorStatistics stats = statistics_;

    stats.topModules.clear();
    stats.topMessages.clear();

    std::vector<std::pair<std::string, size_t>> moduleCountVec;
    moduleCountVec.reserve(moduleErrorCount_.size());
    for (const auto& [module, count] : moduleErrorCount_) {
        moduleCountVec.emplace_back(module, count);
    }

    std::sort(moduleCountVec.begin(), moduleCountVec.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    size_t numToTake = std::min<size_t>(10, moduleCountVec.size());
    stats.topModules.assign(moduleCountVec.begin(),
                            moduleCountVec.begin() + numToTake);

    std::vector<std::pair<std::string, size_t>> messageCountVec;
    messageCountVec.reserve(messageErrorCount_.size());
    for (const auto& [message, count] : messageErrorCount_) {
        messageCountVec.emplace_back(message, count);
    }

    std::sort(messageCountVec.begin(), messageCountVec.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    numToTake = std::min<size_t>(10, messageCountVec.size());
    stats.topMessages.assign(messageCountVec.begin(),
                             messageCountVec.begin() + numToTake);

    return stats;
}

void ErrorStack::updateStatistics(const ErrorInfo& error) {
    statistics_.totalErrors++;

    size_t categoryIndex = static_cast<size_t>(error.category);
    if (categoryIndex < 10) {
        statistics_.errorsByCategory[categoryIndex]++;
    }

    size_t levelIndex = static_cast<size_t>(error.level);
    if (levelIndex < 5) {
        statistics_.errorsByLevel[levelIndex]++;
    }

    auto errorTime = std::chrono::system_clock::from_time_t(error.timestamp);
    statistics_.lastErrorTime = errorTime;

    moduleErrorCount_[error.moduleName]++;
    messageErrorCount_[error.errorMessage]++;
    statistics_.uniqueErrors = compressedErrorStack_.size();
}

void ErrorStack::clear() noexcept {
    try {
        std::lock_guard lock(mutex_);
        errorStack_.clear();
        compressedErrorStack_.clear();
        moduleErrorCount_.clear();
        messageErrorCount_.clear();

        statistics_ = ErrorStatistics{};
        statistics_.firstErrorTime = std::chrono::system_clock::now();
        statistics_.lastErrorTime = statistics_.firstErrorTime;
    } catch (...) {
    }
}

std::string ErrorStack::exportToJson() const {
    std::lock_guard lock(mutex_);

    std::stringstream json;
    json << "[\n";

    size_t count = 0;
    for (const auto& error : errorStack_) {
        json << "  " << error;

        if (++count < errorStack_.size()) {
            json << ",";
        }
        json << "\n";
    }

    json << "]";
    return json.str();
}

std::string ErrorStack::exportToCsv(bool includeMetadata) const {
    std::lock_guard lock(mutex_);

    std::stringstream csv;

    csv << "ErrorMessage,ModuleName,FunctionName,Line,FileName,Timestamp,UUID,"
           "Level,Category,ErrorCode";

    if (includeMetadata) {
        atom::containers::hp::flat_set<std::string> allMetadataKeys;
        for (const auto& error : errorStack_) {
            for (const auto& [key, _] : error.metadata) {
                allMetadataKeys.insert(key);
            }
        }

        for (const auto& key : allMetadataKeys) {
            csv << ",Metadata_" << key;
        }
    }

    csv << "\n";

    for (const auto& error : errorStack_) {
        auto escapeCsvField = [](const std::string& field) -> std::string {
            std::string result = field;
            if (field.find(',') != std::string::npos ||
                field.find('"') != std::string::npos ||
                field.find('\n') != std::string::npos) {
                size_t pos = 0;
                while ((pos = result.find('"', pos)) != std::string::npos) {
                    result.replace(pos, 1, "\"\"");
                    pos += 2;
                }
                result = "\"" + result + "\"";
            }
            return result;
        };

        csv << escapeCsvField(error.errorMessage) << ","
            << escapeCsvField(error.moduleName) << ","
            << escapeCsvField(error.functionName) << "," << error.line << ","
            << escapeCsvField(error.fileName) << ","
            << atom::utils::timeStampToString(error.timestamp) << ","
            << error.uuid << "," << std::string(errorLevelToString(error.level))
            << "," << std::string(errorCategoryToString(error.category)) << ","
            << error.errorCode;

        if (includeMetadata) {
            std::set<std::string> allMetadataKeys;
            for (const auto& err : errorStack_) {
                for (const auto& [key, _] : err.metadata) {
                    allMetadataKeys.insert(key);
                }
            }

            for (const auto& key : allMetadataKeys) {
                csv << ",";
                auto it = error.metadata.find(key);
                if (it != error.metadata.end()) {
                    csv << escapeCsvField(it->second);
                }
            }
        }

        csv << "\n";
    }

    return csv.str();
}

#ifdef ATOM_ERROR_STACK_USE_SERIALIZATION
std::vector<char> ErrorStack::serialize() const {
    try {
        std::lock_guard lock(mutex_);

        std::stringstream ss;
        boost::archive::binary_oarchive oa(ss);

        oa << *this;

        std::string str = ss.str();
        return std::vector<char>(str.begin(), str.end());
    } catch (const std::exception& e) {
        spdlog::error("Failed to serialize error stack: {}", e.what());
        return {};
    }
}

bool ErrorStack::deserialize(std::span<const char> data) {
    try {
        std::lock_guard lock(mutex_);

        std::string str(data.begin(), data.end());
        std::stringstream ss(str);

        boost::archive::binary_iarchive ia(ss);

        errorStack_.clear();
        compressedErrorStack_.clear();
        filteredModules_.clear();
        moduleErrorCount_.clear();
        messageErrorCount_.clear();

        ia >> *this;

        statistics_ = ErrorStatistics{};
        statistics_.totalErrors = errorStack_.size();
        statistics_.uniqueErrors = compressedErrorStack_.size();

        if (!errorStack_.empty()) {
            auto [minIt, maxIt] =
                std::minmax_element(errorStack_.begin(), errorStack_.end(),
                                    [](const ErrorInfo& a, const ErrorInfo& b) {
                                        return a.timestamp < b.timestamp;
                                    });

            statistics_.firstErrorTime =
                std::chrono::system_clock::from_time_t(minIt->timestamp);
            statistics_.lastErrorTime =
                std::chrono::system_clock::from_time_t(maxIt->timestamp);

            for (const auto& error : errorStack_) {
                size_t categoryIndex = static_cast<size_t>(error.category);
                if (categoryIndex < 10) {
                    statistics_.errorsByCategory[categoryIndex]++;
                }

                size_t levelIndex = static_cast<size_t>(error.level);
                if (levelIndex < 5) {
                    statistics_.errorsByLevel[levelIndex]++;
                }

                moduleErrorCount_[error.moduleName]++;
                messageErrorCount_[error.errorMessage]++;
            }
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to deserialize error stack: {}", e.what());
        return false;
    }
}
#endif

}  // namespace atom::error
