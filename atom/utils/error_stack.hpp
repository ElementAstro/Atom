/*
 * error_stack.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-3-29

Description: Error Stack

**************************************************/

#ifndef ATOM_ERROR_STACK_HPP
#define ATOM_ERROR_STACK_HPP

#include <chrono>
#include <concepts>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "atom/containers/high_performance.hpp"
#include "atom/macro.hpp"

// Boost支持
#ifdef ATOM_HAS_BOOST
#ifdef ATOM_HAS_BOOST_CONTAINER
#include <boost/container/flat_map.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/container/vector.hpp>
#define ATOM_ERROR_STACK_USE_BOOST_CONTAINER
#endif

#ifdef ATOM_HAS_BOOST_LOCKFREE
#include <boost/lockfree/queue.hpp>
#define ATOM_ERROR_STACK_USE_BOOST_LOCKFREE
#endif

#ifdef ATOM_HAS_BOOST_UUID
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#define ATOM_ERROR_STACK_USE_BOOST_UUID
#endif

#ifdef ATOM_HAS_BOOST_SERIALIZATION
#include <boost/serialization/access.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>
#define ATOM_ERROR_STACK_USE_SERIALIZATION
#endif
#endif

namespace atom::error {

/**
 * @brief 错误级别枚举
 */
enum class ErrorLevel {
    Debug = 0,    ///< 调试信息
    Info = 1,     ///< 信息
    Warning = 2,  ///< 警告
    Error = 3,    ///< 错误
    Critical = 4  ///< 严重错误
};

/**
 * @brief 错误类别枚举
 */
enum class ErrorCategory {
    General = 0,        ///< 一般错误
    System = 1,         ///< 系统错误
    Network = 2,        ///< 网络错误
    Database = 3,       ///< 数据库错误
    Security = 4,       ///< 安全错误
    IO = 5,             ///< IO错误
    Memory = 6,         ///< 内存错误
    Configuration = 7,  ///< 配置错误
    Validation = 8,     ///< 验证错误
    Other = 9           ///< 其他错误
};

/**
 * @brief Error information structure.
 */
struct ErrorInfo {
    std::string errorMessage; /**< Error message. */
    std::string moduleName;   /**< Module name. */
    std::string functionName; /**< Function name where the error occurred. */
    int line;                 /**< Line number where the error occurred. */
    std::string fileName;     /**< File name where the error occurred. */
    time_t timestamp;         /**< Timestamp of the error. */
    std::string uuid;         /**< UUID of the error. */

    ErrorLevel level{ErrorLevel::Error};            /**< 错误级别 */
    ErrorCategory category{ErrorCategory::General}; /**< 错误类别 */
    int64_t errorCode{0};                           /**< 错误代码 */
    atom::containers::hp::flat_map<std::string, std::string>
        metadata{}; /**< 附加元数据 */

    // Add equality comparison
    bool operator==(const ErrorInfo& other) const noexcept {
        return errorMessage == other.errorMessage &&
               moduleName == other.moduleName &&
               functionName == other.functionName;
    }

#ifdef ATOM_ERROR_STACK_USE_SERIALIZATION
    // Boost序列化支持
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version) {
        ar & errorMessage;
        ar & moduleName;
        ar & functionName;
        ar & line;
        ar & fileName;
        ar & timestamp;
        ar & uuid;
        if (version > 0) {
            ar& static_cast<int>(level);
            ar& static_cast<int>(category);
            ar & errorCode;
            // 序列化元数据
            size_t size = metadata.size();
            ar & size;
            if (Archive::is_loading::value) {
                metadata.clear();
                for (size_t i = 0; i < size; ++i) {
                    std::string key, value;
                    ar & key;
                    ar & value;
                    metadata[key] = value;
                }
            } else {
                for (const auto& [key, value] : metadata) {
                    std::string k = key;
                    std::string v = value;
                    ar & k;
                    ar & v;
                }
            }
        }
    }
#endif
} ATOM_CACHE_ALIGN;

/**
 * @brief 错误信息构建器
 */
class ErrorInfoBuilder {
private:
    ErrorInfo info_;

public:
    ErrorInfoBuilder() = default;

    ErrorInfoBuilder& message(std::string_view message) {
        info_.errorMessage = std::string(message);
        return *this;
    }

    ErrorInfoBuilder& module(std::string_view module) {
        info_.moduleName = std::string(module);
        return *this;
    }

    ErrorInfoBuilder& function(std::string_view function) {
        info_.functionName = std::string(function);
        return *this;
    }

    ErrorInfoBuilder& file(std::string_view file, int line) {
        info_.fileName = std::string(file);
        info_.line = line;
        return *this;
    }

    ErrorInfoBuilder& level(ErrorLevel level) {
        info_.level = level;
        return *this;
    }

    ErrorInfoBuilder& category(ErrorCategory category) {
        info_.category = category;
        return *this;
    }

    ErrorInfoBuilder& code(int64_t code) {
        info_.errorCode = code;
        return *this;
    }

    ErrorInfoBuilder& addMetadata(std::string_view key,
                                  std::string_view value) {
        info_.metadata[std::string(key)] = std::string(value);
        return *this;
    }

    [[nodiscard]] ErrorInfo build() {
        // 设置时间戳
        info_.timestamp = std::time(nullptr);

        // 生成UUID
#ifdef ATOM_ERROR_STACK_USE_BOOST_UUID
        boost::uuids::random_generator gen;
        boost::uuids::uuid uuid = gen();
        info_.uuid = boost::uuids::to_string(uuid);
#else
        // 简单UUID生成（不保证唯一性，仅作演示）
        info_.uuid =
            std::to_string(info_.timestamp) + "_" +
            std::to_string(std::hash<std::string>{}(info_.errorMessage));
#endif

        return info_;
    }
};

/**
 * @brief Overloaded stream insertion operator to print ErrorInfo object.
 * @param os Output stream.
 * @param error ErrorInfo object to be printed.
 * @return Reference to the output stream.
 */
auto operator<<(std::ostream& os, const ErrorInfo& error) -> std::ostream&;

/**
 * @brief Overloaded string concatenation operator to concatenate ErrorInfo
 * object with a string.
 * @param str Input string.
 * @param error ErrorInfo object to be concatenated.
 * @return Concatenated string.
 */
auto operator<<(const std::string& str, const ErrorInfo& error) -> std::string;

// Concept to check if a type is convertible to string_view
template <typename T>
concept StringViewConvertible = std::convertible_to<T, std::string_view>;

// 错误处理回调函数类型
using ErrorCallback = std::function<void(const ErrorInfo&)>;

/**
 * @brief 错误统计信息
 */
struct ErrorStatistics {
    size_t totalErrors{0};
    size_t errorsByCategory[10]{0};  // 对应ErrorCategory枚举值
    size_t errorsByLevel[5]{0};      // 对应ErrorLevel枚举值
    size_t uniqueErrors{0};
    std::chrono::system_clock::time_point firstErrorTime;
    std::chrono::system_clock::time_point lastErrorTime;
    containers::hp::pmr::vector<std::pair<std::string, size_t>>
        topModules;  // 错误最多的模块
    containers::hp::pmr::vector<std::pair<std::string, size_t>>
        topMessages;  // 最常见的错误消息
};

/// Represents a stack of errors and provides operations to manage and retrieve
/// them.
class ErrorStack {
#ifdef ATOM_ERROR_STACK_USE_BOOST_CONTAINER
    using ErrorVector = boost::container::vector<ErrorInfo>;
    using CompressedErrorVector = boost::container::small_vector<ErrorInfo, 16>;
    using FilteredModulesVector =
        boost::container::small_vector<std::string, 8>;
    using ModuleErrorCountMap = boost::container::flat_map<std::string, size_t>;
    using MessageErrorCountMap =
        boost::container::flat_map<std::string, size_t>;
#else
    using ErrorVector = containers::hp::pmr::vector<ErrorInfo>;
    using CompressedErrorVector =
        atom::containers::hp::small_vector<ErrorInfo, 16>;
    using FilteredModulesVector =
        atom::containers::hp::small_vector<std::string, 8>;
    using ModuleErrorCountMap =
        atom::containers::hp::flat_map<std::string, size_t>;
    using MessageErrorCountMap =
        atom::containers::hp::flat_map<std::string, size_t>;
#endif

#ifdef ATOM_ERROR_STACK_USE_BOOST_LOCKFREE
    struct ErrorQueueItem {
        ErrorInfo info;
        std::atomic<ErrorQueueItem*> next{nullptr};
    };

    class LockFreeErrorQueue {
        std::atomic<ErrorQueueItem*> head_{nullptr};
        std::atomic<ErrorQueueItem*> tail_{nullptr};
        std::atomic<size_t> size_{0};

    public:
        LockFreeErrorQueue() {
            auto* dummy = new ErrorQueueItem{};
            head_.store(dummy);
            tail_.store(dummy);
        }

        ~LockFreeErrorQueue() {
            auto* current = head_.load();
            while (current) {
                auto* next = current->next.load();
                delete current;
                current = next;
            }
        }

        void push(const ErrorInfo& info) {
            auto* node = new ErrorQueueItem{info};

            while (true) {
                auto* last = tail_.load();
                auto* next = last->next.load();

                if (last == tail_.load()) {
                    if (next == nullptr) {
                        if (last->next.compare_exchange_weak(next, node)) {
                            tail_.compare_exchange_weak(last, node);
                            size_.fetch_add(1);
                            return;
                        }
                    } else {
                        tail_.compare_exchange_weak(last, next);
                    }
                }
            }
        }

        bool pop(ErrorInfo& info) {
            while (true) {
                auto* first = head_.load();
                auto* last = tail_.load();
                auto* next = first->next.load();

                if (first == head_.load()) {
                    if (first == last) {
                        if (next == nullptr) {
                            return false;
                        }
                        tail_.compare_exchange_weak(last, next);
                    } else {
                        info = next->info;
                        if (head_.compare_exchange_weak(first, next)) {
                            delete first;
                            size_.fetch_sub(1);
                            return true;
                        }
                    }
                }
            }
        }

        [[nodiscard]] size_t size() const { return size_.load(); }

        [[nodiscard]] bool empty() const { return size() == 0; }
    };
#endif

    ErrorVector errorStack_;  ///< The stack of all errors.
    CompressedErrorVector
        compressedErrorStack_;  ///< The compressed stack of unique errors.
    FilteredModulesVector filteredModules_;  ///< Modules to be filtered out
                                             ///< while printing errors.

    // 统计数据
    ModuleErrorCountMap moduleErrorCount_;
    MessageErrorCountMap messageErrorCount_;
    ErrorStatistics statistics_;

    // 错误处理回调
    containers::hp::pmr::vector<ErrorCallback> errorCallbacks_;

#ifdef ATOM_ERROR_STACK_USE_BOOST_LOCKFREE
    LockFreeErrorQueue asyncErrorQueue_;
    std::atomic<bool> asyncProcessingActive_{false};
#endif

    mutable std::mutex mutex_;  ///< Mutex for thread safety

public:
    /// Default constructor.
    ErrorStack();

    /// Destructor
    ~ErrorStack();

    // Delete copy constructor and assignment operator to prevent accidental
    // copies
    ErrorStack(const ErrorStack&) = delete;
    ErrorStack& operator=(const ErrorStack&) = delete;

    // 允许移动
    ErrorStack(ErrorStack&&) noexcept;
    ErrorStack& operator=(ErrorStack&&) noexcept;

    /// Create a shared pointer to an ErrorStack object.
    /// \return A shared pointer to the ErrorStack object.
    [[nodiscard]] static auto createShared() -> std::shared_ptr<ErrorStack>;

    /// Create a unique pointer to an ErrorStack object.
    /// \return A unique pointer to the ErrorStack object.
    [[nodiscard]] static auto createUnique() -> std::unique_ptr<ErrorStack>;

    /// Insert a new error into the error stack.
    /// \param errorMessage The error message.
    /// \param moduleName The module name where the error occurred.
    /// \param functionName The function name where the error occurred.
    /// \param line The line number where the error occurred.
    /// \param fileName The file name where the error occurred.
    /// \return true if insertion was successful, false otherwise.
    template <StringViewConvertible ErrorStr, StringViewConvertible ModuleStr,
              StringViewConvertible FuncStr, StringViewConvertible FileStr>
    bool insertError(ErrorStr&& errorMessage, ModuleStr&& moduleName,
                     FuncStr&& functionName, int line, FileStr&& fileName);

    /// Insert a new error with level and category information
    /// \param errorMessage The error message
    /// \param moduleName The module name
    /// \param functionName The function name
    /// \param line The line number
    /// \param fileName The file name
    /// \param level Error severity level
    /// \param category Error category
    /// \param errorCode Optional error code
    /// \return true if insertion was successful, false otherwise
    template <StringViewConvertible ErrorStr, StringViewConvertible ModuleStr,
              StringViewConvertible FuncStr, StringViewConvertible FileStr>
    bool insertErrorWithLevel(ErrorStr&& errorMessage, ModuleStr&& moduleName,
                              FuncStr&& functionName, int line,
                              FileStr&& fileName, ErrorLevel level,
                              ErrorCategory category = ErrorCategory::General,
                              int64_t errorCode = 0);

    /// Insert a fully constructed ErrorInfo object
    /// \param errorInfo The error info object to insert
    /// \return true if insertion was successful, false otherwise
    bool insertErrorInfo(const ErrorInfo& errorInfo);

    /// Insert an error asynchronously (thread-safe without locking)
    /// \param errorInfo The error info object to insert
    /// \return true if enqueued successfully
    bool insertErrorAsync(const ErrorInfo& errorInfo);

    /// Process any pending asynchronous errors
    /// \return Number of errors processed
    size_t processAsyncErrors();

    /// Start background processing of async errors
    /// \param intervalMs Polling interval in milliseconds
    void startAsyncProcessing(uint32_t intervalMs = 100);

    /// Stop background processing of async errors
    void stopAsyncProcessing();

    /// Register an error callback that will be triggered for each new error
    /// \param callback The callback function to register
    void registerErrorCallback(ErrorCallback callback);

    /// Set the modules to be filtered out while printing the error stack.
    /// \param modules The modules to be filtered out.
    template <typename Container>
        requires std::ranges::range<Container>
    void setFilteredModules(const Container& modules);

    /// Clear the list of filtered modules.
    void clearFilteredModules();

    /// Print the filtered error stack to the standard output.
    void printFilteredErrorStack() const;

    /// Get a vector of errors filtered by a specific module.
    /// \param moduleName The module name for which errors are to be retrieved.
    /// \return A vector of errors filtered by the given module.
    [[nodiscard]] auto getFilteredErrorsByModule(
        std::string_view moduleName) const -> std::vector<ErrorInfo>;

    /// Get errors filtered by severity level
    /// \param level Minimum severity level to include
    /// \return Vector of errors meeting or exceeding the given level
    [[nodiscard]] auto getFilteredErrorsByLevel(ErrorLevel level) const
        -> std::vector<ErrorInfo>;

    /// Get errors filtered by category
    /// \param category The error category to filter by
    /// \return Vector of errors of the specified category
    [[nodiscard]] auto getFilteredErrorsByCategory(ErrorCategory category) const
        -> std::vector<ErrorInfo>;

    /// Get a string containing the compressed errors in the stack.
    /// \return A string containing the compressed errors.
    [[nodiscard]] auto getCompressedErrors() const -> std::string;

    /// Check if the error stack is empty
    /// \return true if the stack is empty, false otherwise
    [[nodiscard]] bool isEmpty() const noexcept;

    /// Get the number of errors in the stack
    /// \return The number of errors
    [[nodiscard]] size_t size() const noexcept;

    /// Get the most recent error
    /// \return Optional containing the most recent error, or empty if stack is
    /// empty
    [[nodiscard]] std::optional<ErrorInfo> getLatestError() const;

    /// Get errors within a time range
    /// \param start Start timestamp
    /// \param end End timestamp
    /// \return Vector of errors within the specified time range
    [[nodiscard]] auto getErrorsInTimeRange(time_t start, time_t end) const
        -> std::vector<ErrorInfo>;

    /// Get error statistics
    /// \return Current error statistics
    [[nodiscard]] ErrorStatistics getStatistics() const;

    /// Clear all errors in the stack
    void clear() noexcept;

    /// Export errors to JSON format
    /// \return JSON string representation of all errors
    [[nodiscard]] std::string exportToJson() const;

    /// Export errors to CSV format
    /// \param includeMetadata Whether to include metadata columns
    /// \return CSV string representation of all errors
    [[nodiscard]] std::string exportToCsv(bool includeMetadata = false) const;

#ifdef ATOM_ERROR_STACK_USE_SERIALIZATION
    /// Serialize to binary format
    /// \return Binary serialized data
    [[nodiscard]] std::vector<char> serialize() const;

    /// Deserialize from binary format
    /// \param data Binary data to deserialize from
    /// \return true if deserialization was successful
    bool deserialize(std::span<const char> data);

    // Boost序列化支持
    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int version) {
        ar & errorStack_;
        ar & compressedErrorStack_;
        ar & filteredModules_;

        if (version > 0) {
            size_t moduleMapSize = moduleErrorCount_.size();
            ar & moduleMapSize;

            if (Archive::is_loading::value) {
                moduleErrorCount_.clear();
                for (size_t i = 0; i < moduleMapSize; ++i) {
                    std::string key;
                    size_t value;
                    ar & key;
                    ar & value;
                    moduleErrorCount_[key] = value;
                }
            } else {
                for (const auto& [key, value] : moduleErrorCount_) {
                    std::string k = key;
                    size_t v = value;
                    ar & k;
                    ar & v;
                }
            }

            size_t messageMapSize = messageErrorCount_.size();
            ar & messageMapSize;

            if (Archive::is_loading::value) {
                messageErrorCount_.clear();
                for (size_t i = 0; i < messageMapSize; ++i) {
                    std::string key;
                    size_t value;
                    ar & key;
                    ar & value;
                    messageErrorCount_[key] = value;
                }
            } else {
                for (const auto& [key, value] : messageErrorCount_) {
                    std::string k = key;
                    size_t v = value;
                    ar & k;
                    ar & v;
                }
            }
        }
    }
#endif

private:
    /// Update the compressed error stack based on the current error stack.
    void updateCompressedErrors();

    /// Sort the compressed error stack based on the timestamp of errors.
    void sortCompressedErrorStack();

    /// Update error statistics
    void updateStatistics(const ErrorInfo& error);
};

// Template method implementation
template <StringViewConvertible ErrorStr, StringViewConvertible ModuleStr,
          StringViewConvertible FuncStr, StringViewConvertible FileStr>
bool ErrorStack::insertError(ErrorStr&& errorMessage, ModuleStr&& moduleName,
                             FuncStr&& functionName, int line,
                             FileStr&& fileName) {
    try {
        // Validate inputs
        if (std::string_view(errorMessage).empty()) {
            return false;
        }

        if (line < 0) {
            return false;
        }

        auto currentTime = std::time(nullptr);

        std::lock_guard<std::mutex> lock(mutex_);

        auto iter = std::ranges::find_if(
            errorStack_, [&errorMessage, &moduleName](const ErrorInfo& error) {
                return error.errorMessage == std::string(errorMessage) &&
                       error.moduleName == std::string(moduleName);
            });

        if (iter != errorStack_.end()) {
            iter->timestamp = currentTime;
        } else {
            ErrorInfo error{
                std::string(errorMessage),
                std::string(moduleName),
                std::string(functionName),
                line,
                std::string(fileName),
                currentTime,
                ""  // UUID will be generated later
            };

            // 生成UUID
#ifdef ATOM_ERROR_STACK_USE_BOOST_UUID
            boost::uuids::random_generator gen;
            boost::uuids::uuid uuid = gen();
            error.uuid = boost::uuids::to_string(uuid);
#else
            // 简单UUID生成
            error.uuid =
                std::to_string(currentTime) + "_" +
                std::to_string(std::hash<std::string>{}(error.errorMessage));
#endif

            errorStack_.emplace_back(std::move(error));

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

template <StringViewConvertible ErrorStr, StringViewConvertible ModuleStr,
          StringViewConvertible FuncStr, StringViewConvertible FileStr>
bool ErrorStack::insertErrorWithLevel(ErrorStr&& errorMessage,
                                      ModuleStr&& moduleName,
                                      FuncStr&& functionName, int line,
                                      FileStr&& fileName, ErrorLevel level,
                                      ErrorCategory category,
                                      int64_t errorCode) {
    try {
        // Validate inputs
        if (std::string_view(errorMessage).empty()) {
            return false;
        }

        if (line < 0) {
            return false;
        }

        auto currentTime = std::time(nullptr);

        std::lock_guard<std::mutex> lock(mutex_);

        auto iter = std::ranges::find_if(
            errorStack_, [&errorMessage, &moduleName](const ErrorInfo& error) {
                return error.errorMessage == std::string(errorMessage) &&
                       error.moduleName == std::string(moduleName);
            });

        if (iter != errorStack_.end()) {
            iter->timestamp = currentTime;
            iter->level = level;
            iter->category = category;
            iter->errorCode = errorCode;
        } else {
            ErrorInfo error{std::string(errorMessage),
                            std::string(moduleName),
                            std::string(functionName),
                            line,
                            std::string(fileName),
                            currentTime,
                            "",  // UUID will be generated later
                            level,
                            category,
                            errorCode};

            // 生成UUID
#ifdef ATOM_ERROR_STACK_USE_BOOST_UUID
            boost::uuids::random_generator gen;
            boost::uuids::uuid uuid = gen();
            error.uuid = boost::uuids::to_string(uuid);
#else
            // 简单UUID生成
            error.uuid =
                std::to_string(currentTime) + "_" +
                std::to_string(std::hash<std::string>{}(error.errorMessage));
#endif

            errorStack_.emplace_back(std::move(error));

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

template <typename Container>
    requires std::ranges::range<Container>
void ErrorStack::setFilteredModules(const Container& modules) {
    try {
        std::lock_guard<std::mutex> lock(mutex_);
        filteredModules_.clear();
        filteredModules_.reserve(std::ranges::distance(modules));
        for (const auto& module : modules) {
            filteredModules_.emplace_back(module);
        }
    } catch (const std::exception&) {
        // In case of error, clear the filter list to avoid partial updates
        filteredModules_.clear();
    }
}

}  // namespace atom::error

#endif
