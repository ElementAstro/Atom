/*
 * error_stack.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

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
 * @brief Error severity levels
 */
enum class ErrorLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Critical = 4
};

/**
 * @brief Error category types
 */
enum class ErrorCategory {
    General = 0,
    System = 1,
    Network = 2,
    Database = 3,
    Security = 4,
    IO = 5,
    Memory = 6,
    Configuration = 7,
    Validation = 8,
    Other = 9
};

/**
 * @brief Comprehensive error information structure
 */
struct ErrorInfo {
    std::string errorMessage; /**< Error message content */
    std::string moduleName;   /**< Module where error occurred */
    std::string functionName; /**< Function where error occurred */
    int line;                 /**< Line number where error occurred */
    std::string fileName;     /**< File name where error occurred */
    time_t timestamp;         /**< Error occurrence timestamp */
    std::string uuid;         /**< Unique error identifier */
    ErrorLevel level{ErrorLevel::Error};            /**< Error severity level */
    ErrorCategory category{ErrorCategory::General}; /**< Error category */
    int64_t errorCode{0};                           /**< Numeric error code */
    atom::containers::hp::flat_map<std::string, std::string>
        metadata{}; /**< Additional metadata */

    /**
     * @brief Equality comparison operator
     * @param other Other ErrorInfo to compare with
     * @return True if errors are considered equal
     */
    bool operator==(const ErrorInfo& other) const noexcept {
        return errorMessage == other.errorMessage &&
               moduleName == other.moduleName &&
               functionName == other.functionName;
    }

#ifdef ATOM_ERROR_STACK_USE_SERIALIZATION
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
 * @brief Builder pattern for constructing ErrorInfo objects
 */
class ErrorInfoBuilder {
private:
    ErrorInfo info_;

public:
    ErrorInfoBuilder() = default;

    /**
     * @brief Set error message
     * @param message Error message content
     * @return Reference to this builder
     */
    ErrorInfoBuilder& message(std::string_view message) {
        info_.errorMessage = std::string(message);
        return *this;
    }

    /**
     * @brief Set module name
     * @param module Module name
     * @return Reference to this builder
     */
    ErrorInfoBuilder& module(std::string_view module) {
        info_.moduleName = std::string(module);
        return *this;
    }

    /**
     * @brief Set function name
     * @param function Function name
     * @return Reference to this builder
     */
    ErrorInfoBuilder& function(std::string_view function) {
        info_.functionName = std::string(function);
        return *this;
    }

    /**
     * @brief Set file information
     * @param file File name
     * @param line Line number
     * @return Reference to this builder
     */
    ErrorInfoBuilder& file(std::string_view file, int line) {
        info_.fileName = std::string(file);
        info_.line = line;
        return *this;
    }

    /**
     * @brief Set error level
     * @param level Error severity level
     * @return Reference to this builder
     */
    ErrorInfoBuilder& level(ErrorLevel level) {
        info_.level = level;
        return *this;
    }

    /**
     * @brief Set error category
     * @param category Error category
     * @return Reference to this builder
     */
    ErrorInfoBuilder& category(ErrorCategory category) {
        info_.category = category;
        return *this;
    }

    /**
     * @brief Set error code
     * @param code Numeric error code
     * @return Reference to this builder
     */
    ErrorInfoBuilder& code(int64_t code) {
        info_.errorCode = code;
        return *this;
    }

    /**
     * @brief Add metadata key-value pair
     * @param key Metadata key
     * @param value Metadata value
     * @return Reference to this builder
     */
    ErrorInfoBuilder& addMetadata(std::string_view key,
                                  std::string_view value) {
        info_.metadata[std::string(key)] = std::string(value);
        return *this;
    }

    /**
     * @brief Build the final ErrorInfo object
     * @return Constructed ErrorInfo with timestamp and UUID
     */
    [[nodiscard]] ErrorInfo build() {
        info_.timestamp = std::time(nullptr);

#ifdef ATOM_ERROR_STACK_USE_BOOST_UUID
        boost::uuids::random_generator gen;
        boost::uuids::uuid uuid = gen();
        info_.uuid = boost::uuids::to_string(uuid);
#else
        info_.uuid =
            std::to_string(info_.timestamp) + "_" +
            std::to_string(std::hash<std::string>{}(info_.errorMessage));
#endif

        return info_;
    }
};

/**
 * @brief Stream insertion operator for ErrorInfo
 * @param os Output stream
 * @param error ErrorInfo object to output
 * @return Reference to output stream
 */
auto operator<<(std::ostream& os, const ErrorInfo& error) -> std::ostream&;

/**
 * @brief String concatenation operator for ErrorInfo
 * @param str Input string
 * @param error ErrorInfo object to concatenate
 * @return Concatenated string
 */
auto operator<<(const std::string& str, const ErrorInfo& error) -> std::string;

template <typename T>
concept StringViewConvertible = std::convertible_to<T, std::string_view>;

using ErrorCallback = std::function<void(const ErrorInfo&)>;

/**
 * @brief Statistical information about errors
 */
struct ErrorStatistics {
    size_t totalErrors{0};
    size_t errorsByCategory[10]{0};
    size_t errorsByLevel[5]{0};
    size_t uniqueErrors{0};
    std::chrono::system_clock::time_point firstErrorTime;
    std::chrono::system_clock::time_point lastErrorTime;
    containers::hp::pmr::vector<std::pair<std::string, size_t>> topModules;
    containers::hp::pmr::vector<std::pair<std::string, size_t>> topMessages;
};

/**
 * @brief Thread-safe error stack for managing and analyzing errors
 */
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

    ErrorVector errorStack_;
    CompressedErrorVector compressedErrorStack_;
    FilteredModulesVector filteredModules_;
    ModuleErrorCountMap moduleErrorCount_;
    MessageErrorCountMap messageErrorCount_;
    ErrorStatistics statistics_;
    containers::hp::pmr::vector<ErrorCallback> errorCallbacks_;

#ifdef ATOM_ERROR_STACK_USE_BOOST_LOCKFREE
    LockFreeErrorQueue asyncErrorQueue_;
    std::atomic<bool> asyncProcessingActive_{false};
#endif

    mutable std::mutex mutex_;

public:
    /**
     * @brief Default constructor
     */
    ErrorStack();

    /**
     * @brief Destructor
     */
    ~ErrorStack();

    ErrorStack(const ErrorStack&) = delete;
    ErrorStack& operator=(const ErrorStack&) = delete;

    /**
     * @brief Move constructor
     * @param other Other ErrorStack to move from
     */
    ErrorStack(ErrorStack&& other) noexcept;

    /**
     * @brief Move assignment operator
     * @param other Other ErrorStack to move from
     * @return Reference to this ErrorStack
     */
    ErrorStack& operator=(ErrorStack&& other) noexcept;

    /**
     * @brief Create shared pointer to ErrorStack
     * @return Shared pointer to new ErrorStack instance
     */
    [[nodiscard]] static auto createShared() -> std::shared_ptr<ErrorStack>;

    /**
     * @brief Create unique pointer to ErrorStack
     * @return Unique pointer to new ErrorStack instance
     */
    [[nodiscard]] static auto createUnique() -> std::unique_ptr<ErrorStack>;

    /**
     * @brief Insert error with basic information
     * @param errorMessage Error message content
     * @param moduleName Module where error occurred
     * @param functionName Function where error occurred
     * @param line Line number where error occurred
     * @param fileName File name where error occurred
     * @return True if insertion was successful
     */
    template <StringViewConvertible ErrorStr, StringViewConvertible ModuleStr,
              StringViewConvertible FuncStr, StringViewConvertible FileStr>
    bool insertError(ErrorStr&& errorMessage, ModuleStr&& moduleName,
                     FuncStr&& functionName, int line, FileStr&& fileName);

    /**
     * @brief Insert error with level and category information
     * @param errorMessage Error message content
     * @param moduleName Module name
     * @param functionName Function name
     * @param line Line number
     * @param fileName File name
     * @param level Error severity level
     * @param category Error category
     * @param errorCode Optional error code
     * @return True if insertion was successful
     */
    template <StringViewConvertible ErrorStr, StringViewConvertible ModuleStr,
              StringViewConvertible FuncStr, StringViewConvertible FileStr>
    bool insertErrorWithLevel(ErrorStr&& errorMessage, ModuleStr&& moduleName,
                              FuncStr&& functionName, int line,
                              FileStr&& fileName, ErrorLevel level,
                              ErrorCategory category = ErrorCategory::General,
                              int64_t errorCode = 0);

    /**
     * @brief Insert fully constructed ErrorInfo object
     * @param errorInfo The error info object to insert
     * @return True if insertion was successful
     */
    bool insertErrorInfo(const ErrorInfo& errorInfo);

    /**
     * @brief Insert error asynchronously (lock-free when available)
     * @param errorInfo The error info object to insert
     * @return True if enqueued successfully
     */
    bool insertErrorAsync(const ErrorInfo& errorInfo);

    /**
     * @brief Process pending asynchronous errors
     * @return Number of errors processed
     */
    size_t processAsyncErrors();

    /**
     * @brief Start background processing of async errors
     * @param intervalMs Polling interval in milliseconds
     */
    void startAsyncProcessing(uint32_t intervalMs = 100);

    /**
     * @brief Stop background processing of async errors
     */
    void stopAsyncProcessing();

    /**
     * @brief Register error callback for new errors
     * @param callback The callback function to register
     */
    void registerErrorCallback(ErrorCallback callback);

    /**
     * @brief Set modules to filter out from output
     * @param modules Container of module names to filter
     */
    template <typename Container>
        requires std::ranges::range<Container>
    void setFilteredModules(const Container& modules);

    /**
     * @brief Clear the list of filtered modules
     */
    void clearFilteredModules();

    /**
     * @brief Print filtered error stack to output
     */
    void printFilteredErrorStack() const;

    /**
     * @brief Get errors filtered by specific module
     * @param moduleName Module name to filter by
     * @return Vector of errors from the specified module
     */
    [[nodiscard]] auto getFilteredErrorsByModule(
        std::string_view moduleName) const -> std::vector<ErrorInfo>;

    /**
     * @brief Get errors filtered by severity level
     * @param level Minimum severity level to include
     * @return Vector of errors meeting or exceeding the given level
     */
    [[nodiscard]] auto getFilteredErrorsByLevel(ErrorLevel level) const
        -> std::vector<ErrorInfo>;

    /**
     * @brief Get errors filtered by category
     * @param category The error category to filter by
     * @return Vector of errors of the specified category
     */
    [[nodiscard]] auto getFilteredErrorsByCategory(ErrorCategory category) const
        -> std::vector<ErrorInfo>;

    /**
     * @brief Get compressed error representation
     * @return String containing compressed errors
     */
    [[nodiscard]] auto getCompressedErrors() const -> std::string;

    /**
     * @brief Check if error stack is empty
     * @return True if the stack is empty
     */
    [[nodiscard]] bool isEmpty() const noexcept;

    /**
     * @brief Get number of errors in stack
     * @return The number of errors
     */
    [[nodiscard]] size_t size() const noexcept;

    /**
     * @brief Get the most recent error
     * @return Optional containing the most recent error
     */
    [[nodiscard]] std::optional<ErrorInfo> getLatestError() const;

    /**
     * @brief Get errors within time range
     * @param start Start timestamp
     * @param end End timestamp
     * @return Vector of errors within the specified time range
     */
    [[nodiscard]] auto getErrorsInTimeRange(time_t start, time_t end) const
        -> std::vector<ErrorInfo>;

    /**
     * @brief Get error statistics
     * @return Current error statistics
     */
    [[nodiscard]] ErrorStatistics getStatistics() const;

    /**
     * @brief Clear all errors in the stack
     */
    void clear() noexcept;

    /**
     * @brief Export errors to JSON format
     * @return JSON string representation of all errors
     */
    [[nodiscard]] std::string exportToJson() const;

    /**
     * @brief Export errors to CSV format
     * @param includeMetadata Whether to include metadata columns
     * @return CSV string representation of all errors
     */
    [[nodiscard]] std::string exportToCsv(bool includeMetadata = false) const;

#ifdef ATOM_ERROR_STACK_USE_SERIALIZATION
    /**
     * @brief Serialize to binary format
     * @return Binary serialized data
     */
    [[nodiscard]] std::vector<char> serialize() const;

    /**
     * @brief Deserialize from binary format
     * @param data Binary data to deserialize from
     * @return True if deserialization was successful
     */
    bool deserialize(std::span<const char> data);

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
    void updateCompressedErrors();
    void sortCompressedErrorStack();
    void updateStatistics(const ErrorInfo& error);
};

template <StringViewConvertible ErrorStr, StringViewConvertible ModuleStr,
          StringViewConvertible FuncStr, StringViewConvertible FileStr>
bool ErrorStack::insertError(ErrorStr&& errorMessage, ModuleStr&& moduleName,
                             FuncStr&& functionName, int line,
                             FileStr&& fileName) {
    try {
        if (std::string_view(errorMessage).empty() || line < 0) {
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
            ErrorInfo error{std::string(errorMessage),
                            std::string(moduleName),
                            std::string(functionName),
                            line,
                            std::string(fileName),
                            currentTime,
                            ""};

#ifdef ATOM_ERROR_STACK_USE_BOOST_UUID
            boost::uuids::random_generator gen;
            boost::uuids::uuid uuid = gen();
            error.uuid = boost::uuids::to_string(uuid);
#else
            error.uuid =
                std::to_string(currentTime) + "_" +
                std::to_string(std::hash<std::string>{}(error.errorMessage));
#endif

            errorStack_.emplace_back(std::move(error));
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

template <StringViewConvertible ErrorStr, StringViewConvertible ModuleStr,
          StringViewConvertible FuncStr, StringViewConvertible FileStr>
bool ErrorStack::insertErrorWithLevel(ErrorStr&& errorMessage,
                                      ModuleStr&& moduleName,
                                      FuncStr&& functionName, int line,
                                      FileStr&& fileName, ErrorLevel level,
                                      ErrorCategory category,
                                      int64_t errorCode) {
    try {
        if (std::string_view(errorMessage).empty() || line < 0) {
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
                            "",
                            level,
                            category,
                            errorCode};

#ifdef ATOM_ERROR_STACK_USE_BOOST_UUID
            boost::uuids::random_generator gen;
            boost::uuids::uuid uuid = gen();
            error.uuid = boost::uuids::to_string(uuid);
#else
            error.uuid =
                std::to_string(currentTime) + "_" +
                std::to_string(std::hash<std::string>{}(error.errorMessage));
#endif

            errorStack_.emplace_back(std::move(error));
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
        filteredModules_.clear();
    }
}

}  // namespace atom::error

#endif
