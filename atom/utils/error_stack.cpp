/*
 * error_stack.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-29

Description: Error Stack

**************************************************/

#include "error_stack.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <execution>
#include <set>
#include <sstream>

#include "atom/log/loguru.hpp"
#include "atom/utils/time.hpp"

#ifdef ATOM_ERROR_STACK_USE_SERIALIZATION
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#endif

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

// 重载输出运算符
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

// 构造函数和析构函数
ErrorStack::ErrorStack() {
    // 预先分配内存以减少重新分配
    errorStack_.reserve(128);
    compressedErrorStack_.reserve(64);
    statistics_.firstErrorTime = std::chrono::system_clock::now();
    statistics_.lastErrorTime = statistics_.firstErrorTime;
}

ErrorStack::~ErrorStack() {
#ifdef ATOM_ERROR_STACK_USE_BOOST_LOCKFREE
    // 停止异步处理
    stopAsyncProcessing();
#endif

    // 清理资源
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        errorStack_.clear();
        compressedErrorStack_.clear();
        filteredModules_.clear();
        moduleErrorCount_.clear();
        messageErrorCount_.clear();
        errorCallbacks_.clear();
    } catch (...) {
        // 忽略析构函数中的异常
    }
}

// 移动构造和移动赋值
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
    // 停止另一个对象的异步处理
    other.stopAsyncProcessing();
    // 异步队列处理需要单独处理，这里简化处理
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
        // 停止另一个对象的异步处理
        other.stopAsyncProcessing();
        // 异步队列处理需要单独处理，这里简化处理
#endif
    }
    return *this;
}

// 静态创建方法
auto ErrorStack::createShared() -> std::shared_ptr<ErrorStack> {
    return std::make_shared<ErrorStack>();
}

auto ErrorStack::createUnique() -> std::unique_ptr<ErrorStack> {
    return std::make_unique<ErrorStack>();
}

// 错误插入方法
bool ErrorStack::insertErrorInfo(const ErrorInfo& errorInfo) {
    try {
        // 验证输入
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
            // 更新现有错误
            iter->timestamp = errorInfo.timestamp;
            iter->level = errorInfo.level;
            iter->category = errorInfo.category;
            iter->errorCode = errorInfo.errorCode;

            // 合并metadata
            for (const auto& [key, value] : errorInfo.metadata) {
                iter->metadata[key] = value;
            }
        } else {
            // 添加新错误
            errorStack_.push_back(errorInfo);

            // 更新统计数据
            updateStatistics(errorStack_.back());

            // 触发回调
            for (const auto& callback : errorCallbacks_) {
                try {
                    callback(errorStack_.back());
                } catch (...) {
                    // 忽略回调中的异常
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
// 异步错误处理相关方法
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
// 全局线程控制变量
std::atomic<bool> g_asyncProcessingRunning = false;
std::thread g_asyncProcessingThread;

// 异步处理线程函数
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

        // 如果已有线程在运行，先停止它
        if (g_asyncProcessingThread.joinable()) {
            g_asyncProcessingRunning = false;
            g_asyncProcessingThread.join();
        }

        // 启动新的处理线程
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

        // 处理剩余的错误
        processAsyncErrors();
    }
}
#else
// 非Boost环境下的空实现
bool ErrorStack::insertErrorAsync(const ErrorInfo& errorInfo) {
    return insertErrorInfo(errorInfo);
}

size_t ErrorStack::processAsyncErrors() { return 0; }

void ErrorStack::startAsyncProcessing(uint32_t) {
    // 非Boost环境下不支持此功能
    LOG_F(WARNING,
          "Async error processing is not supported without Boost lockfree");
}

void ErrorStack::stopAsyncProcessing() {
    // 非Boost环境下不支持此功能
}
#endif

// 错误回调注册
void ErrorStack::registerErrorCallback(ErrorCallback callback) {
    if (callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        errorCallbacks_.push_back(std::move(callback));
    }
}

// 模块过滤相关方法
void ErrorStack::clearFilteredModules() {
    std::lock_guard lock(mutex_);
    filteredModules_.clear();
}

void ErrorStack::printFilteredErrorStack() const {
    try {
        std::lock_guard lock(mutex_);

        for (const auto& error : errorStack_) {
            if (!std::ranges::contains(filteredModules_, error.moduleName)) {
                LOG_F(ERROR, "{} [{}] [{}] {}",
                      std::string(errorLevelToString(error.level)),
                      std::string(errorCategoryToString(error.category)),
                      error.moduleName, error.errorMessage);
            }
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to print error stack: {}", e.what());
    }
}

// 错误过滤方法
auto ErrorStack::getFilteredErrorsByModule(std::string_view moduleName) const
    -> std::vector<ErrorInfo> {
    try {
        std::lock_guard lock(mutex_);

        std::vector<ErrorInfo> errors;
        errors.reserve(errorStack_.size());  // Pre-allocate for efficiency

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
        LOG_F(ERROR, "Failed to get filtered errors: {}", e.what());
        return {};
    }
}

auto ErrorStack::getFilteredErrorsByLevel(ErrorLevel level) const
    -> std::vector<ErrorInfo> {
    try {
        std::lock_guard lock(mutex_);

        std::vector<ErrorInfo> errors;
        errors.reserve(errorStack_.size() / 2);  // 假设大约一半的错误会符合条件

        // 复制所有级别大于等于指定级别且不在过滤模块列表中的错误
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
        LOG_F(ERROR, "Failed to get errors by level: {}", e.what());
        return {};
    }
}

auto ErrorStack::getFilteredErrorsByCategory(ErrorCategory category) const
    -> std::vector<ErrorInfo> {
    try {
        std::lock_guard lock(mutex_);

        std::vector<ErrorInfo> errors;
        errors.reserve(errorStack_.size() /
                       5);  // 假设大约五分之一的错误属于这一类别

        // 复制所有属于指定类别且不在过滤模块列表中的错误
        std::copy_if(errorStack_.begin(), errorStack_.end(),
                     std::back_inserter(errors),
                     [category, this](const ErrorInfo& error) {
                         return error.category == category &&
                                !std::ranges::contains(filteredModules_,
                                                       error.moduleName);
                     });

        return errors;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to get errors by category: {}", e.what());
        return {};
    }
}

auto ErrorStack::getErrorsInTimeRange(time_t start, time_t end) const
    -> std::vector<ErrorInfo> {
    try {
        std::lock_guard lock(mutex_);

        std::vector<ErrorInfo> errors;

        // 确保start <= end
        if (start > end) {
            std::swap(start, end);
        }

        errors.reserve(errorStack_.size() /
                       3);  // 预估大约三分之一的错误会在这个时间范围内

        // 复制所有在指定时间范围内且不在过滤模块列表中的错误
        std::copy_if(
            errorStack_.begin(), errorStack_.end(), std::back_inserter(errors),
            [start, end, this](const ErrorInfo& error) {
                return error.timestamp >= start && error.timestamp <= end &&
                       !std::ranges::contains(filteredModules_,
                                              error.moduleName);
            });

        return errors;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to get errors in time range: {}", e.what());
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

        // 改进格式，使输出更加有用
        for (const auto& error : compressedErrorStack_) {
            compressedErrors
                << "[" << std::string(errorLevelToString(error.level)) << "] "
                << "[" << error.moduleName << "] " << error.errorMessage
                << " @ " << atom::utils::timeStampToString(error.timestamp)
                << "\n";
        }

        return compressedErrors.str();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to get compressed errors: {}", e.what());
        return "";
    }
}

void ErrorStack::updateCompressedErrors() {
    try {
        // No need for mutex lock as this is called from methods that already
        // have the lock
        compressedErrorStack_.clear();

        // Reserve space based on the original stack size for better performance
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
                // 更新级别和类别为最新的
                iter->level = error.level;
                iter->category = error.category;
            } else {
                compressedErrorStack_.push_back(error);
            }
        }

        sortCompressedErrorStack();
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to update compressed errors: {}", e.what());
        // Attempt to recover by clearing
        compressedErrorStack_.clear();
    }
}

void ErrorStack::sortCompressedErrorStack() {
    try {
        // 首先按错误级别排序（从高到低），然后按时间戳排序（从新到旧）
        auto comparator = [](const ErrorInfo& error1, const ErrorInfo& error2) {
            // 优先按级别排序
            if (error1.level != error2.level) {
                return static_cast<int>(error1.level) >
                       static_cast<int>(error2.level);
            }
            // 同级别的按时间戳排序
            return error1.timestamp > error2.timestamp;
        };

        // Using parallel execution policy for large collections
        if (compressedErrorStack_.size() > 1000) {
            std::sort(std::execution::par_unseq, compressedErrorStack_.begin(),
                      compressedErrorStack_.end(), comparator);
        } else {
            std::sort(compressedErrorStack_.begin(),
                      compressedErrorStack_.end(), comparator);
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to sort compressed error stack: {}", e.what());
    }
}

// 错误堆栈状态查询方法
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

    // 复制当前统计信息
    ErrorStatistics stats = statistics_;

    // 确保topModules和topMessages是最新的
    stats.topModules.clear();
    stats.topMessages.clear();

    // 将模块错误计数转化为vector以便排序
    std::vector<std::pair<std::string, size_t>> moduleCountVec;
    moduleCountVec.reserve(moduleErrorCount_.size());
    for (const auto& [module, count] : moduleErrorCount_) {
        moduleCountVec.emplace_back(module, count);
    }

    // 按错误计数从高到低排序
    std::sort(moduleCountVec.begin(), moduleCountVec.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // 取前10个（或更少）
    size_t numToTake = std::min<size_t>(10, moduleCountVec.size());
    stats.topModules.assign(moduleCountVec.begin(),
                            moduleCountVec.begin() + numToTake);

    // 同样处理错误消息
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
    // 不需要锁，因为调用此方法的函数已经有锁了

    // 更新总计数
    statistics_.totalErrors++;

    // 更新按类别的计数
    size_t categoryIndex = static_cast<size_t>(error.category);
    if (categoryIndex < 10) {  // 确保索引有效
        statistics_.errorsByCategory[categoryIndex]++;
    }

    // 更新按级别的计数
    size_t levelIndex = static_cast<size_t>(error.level);
    if (levelIndex < 5) {  // 确保索引有效
        statistics_.errorsByLevel[levelIndex]++;
    }

    // 更新时间戳
    auto errorTime = std::chrono::system_clock::from_time_t(error.timestamp);
    statistics_.lastErrorTime = errorTime;

    // 更新模块计数
    moduleErrorCount_[error.moduleName]++;

    // 更新消息计数
    messageErrorCount_[error.errorMessage]++;

    // 更新唯一错误计数（使用压缩错误堆栈的大小）
    statistics_.uniqueErrors = compressedErrorStack_.size();
}

void ErrorStack::clear() noexcept {
    try {
        std::lock_guard lock(mutex_);
        errorStack_.clear();
        compressedErrorStack_.clear();

        // 重置统计信息
        moduleErrorCount_.clear();
        messageErrorCount_.clear();

        statistics_ = ErrorStatistics{};
        statistics_.firstErrorTime = std::chrono::system_clock::now();
        statistics_.lastErrorTime = statistics_.firstErrorTime;
    } catch (...) {
        // Ensure we don't throw from a noexcept function
    }
}

// 导出格式支持
std::string ErrorStack::exportToJson() const {
    std::lock_guard lock(mutex_);

    std::stringstream json;
    json << "[\n";

    size_t count = 0;
    for (const auto& error : errorStack_) {
        json << "  ";
        json << error;

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

                // 用引号包围整个字段
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
            // 收集所有元数据键（与上面相同）
            std::set<std::string> allMetadataKeys;
            for (const auto& err : errorStack_) {
                for (const auto& [key, _] : err.metadata) {
                    allMetadataKeys.insert(key);
                }
            }

            // 按顺序添加每个元数据值
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

        // 序列化当前对象
        oa << *this;

        // 将stringstream转换为vector<char>
        std::string str = ss.str();
        return std::vector<char>(str.begin(), str.end());
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to serialize error stack: {}", e.what());
        return {};
    }
}

bool ErrorStack::deserialize(std::span<const char> data) {
    try {
        std::lock_guard lock(mutex_);

        // 将data转换为stringstream
        std::string str(data.begin(), data.end());
        std::stringstream ss(str);

        boost::archive::binary_iarchive ia(ss);

        // 清除当前数据
        errorStack_.clear();
        compressedErrorStack_.clear();
        filteredModules_.clear();
        moduleErrorCount_.clear();
        messageErrorCount_.clear();

        // 反序列化
        ia >> *this;

        // 更新统计信息
        statistics_ = ErrorStatistics{};
        statistics_.totalErrors = errorStack_.size();
        statistics_.uniqueErrors = compressedErrorStack_.size();

        if (!errorStack_.empty()) {
            // 查找最早和最晚的时间戳
            auto [minIt, maxIt] =
                std::minmax_element(errorStack_.begin(), errorStack_.end(),
                                    [](const ErrorInfo& a, const ErrorInfo& b) {
                                        return a.timestamp < b.timestamp;
                                    });

            statistics_.firstErrorTime =
                std::chrono::system_clock::from_time_t(minIt->timestamp);
            statistics_.lastErrorTime =
                std::chrono::system_clock::from_time_t(maxIt->timestamp);

            // 重新计算级别和类别统计
            for (const auto& error : errorStack_) {
                size_t categoryIndex = static_cast<size_t>(error.category);
                if (categoryIndex < 10) {
                    statistics_.errorsByCategory[categoryIndex]++;
                }

                size_t levelIndex = static_cast<size_t>(error.level);
                if (levelIndex < 5) {
                    statistics_.errorsByLevel[levelIndex]++;
                }

                // 更新模块计数和消息计数
                moduleErrorCount_[error.moduleName]++;
                messageErrorCount_[error.errorMessage]++;
            }
        }

        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to deserialize error stack: {}", e.what());
        return false;
    }
}
#endif

}  // namespace atom::error
