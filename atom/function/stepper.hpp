/*!
 * \file function_sequence.hpp
 * \brief Advanced Function Sequence Management
 * \author Max Qian <lightapt.com>, Enhanced by [Your Name]
 * \date 2024-03-01, Updated 2024-03-13
 */

#ifndef ATOM_META_FUNCTION_SEQUENCE_HPP
#define ATOM_META_FUNCTION_SEQUENCE_HPP

#include <any>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "atom/algorithm/hash.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/any.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#endif

namespace atom::meta {

// Forward declaration for result type
/**
 * @brief Result wrapper with success/error state
 * @tparam T Type of the success value
 */
template <typename T>
class Result {
public:
    // Create a success result
    static Result<T> makeSuccess(T value) {
        return Result<T>(std::move(value));
    }

    // Create an error result
    static Result<T> makeError(std::string error) {
        return Result<T>(std::move(error));
    }

    // Check if result is success
    [[nodiscard]] bool isSuccess() const noexcept {
        return std::holds_alternative<T>(data_);
    }

    // Check if result is error
    [[nodiscard]] bool isError() const noexcept {
        return std::holds_alternative<std::string>(data_);
    }

    // Get success value (throws if error)
    [[nodiscard]] const T& value() const {
        if (isError()) {
            throw std::runtime_error("Cannot get value from error result: " +
                                     std::get<std::string>(data_));
        }
        return std::get<T>(data_);
    }

    // Get error message (throws if success)
    [[nodiscard]] const std::string& error() const {
        if (isSuccess()) {
            throw std::runtime_error("Cannot get error from success result");
        }
        return std::get<std::string>(data_);
    }

    // Get success value or a default
    [[nodiscard]] T valueOr(T defaultValue) const {
        if (isSuccess()) {
            return std::get<T>(data_);
        }
        return defaultValue;
    }

private:
    std::variant<T, std::string> data_;

    explicit Result(T value) : data_(std::move(value)) {}
    explicit Result(std::string error) : data_(std::move(error)) {}
};

/**
 * @brief Enhanced function sequence with modern C++ features
 */
class FunctionSequence {
public:
    using FunctionType = std::function<std::any(std::span<const std::any>)>;
    using ResultType = std::any;
    using ErrorType = std::string;

    struct ExecutionStats {
        std::chrono::nanoseconds totalExecutionTime{0};
        std::size_t invocationCount{0};
        std::size_t cacheHits{0};
        std::size_t cacheMisses{0};
        std::size_t errorCount{0};

        void reset() noexcept {
            totalExecutionTime = std::chrono::nanoseconds{0};
            invocationCount = 0;
            cacheHits = 0;
            cacheMisses = 0;
            errorCount = 0;
        }
    };

    enum class ExecutionPolicy {
        Sequential,    // Execute functions sequentially
        Parallel,      // Execute functions in parallel
        ParallelAsync  // Execute functions asynchronously in parallel
    };

    struct ExecutionOptions {
        std::optional<std::chrono::milliseconds> timeout = std::nullopt;
        std::optional<size_t> retryCount = std::nullopt;
        bool enableCaching = false;
        bool enableLogging = false;
        ExecutionPolicy policy = ExecutionPolicy::Sequential;
        std::function<void(const std::any&)> notificationCallback = nullptr;
    };

    // Constructor and destructor
    FunctionSequence() = default;

    FunctionSequence(const FunctionSequence&) = delete;
    FunctionSequence& operator=(const FunctionSequence&) = delete;

    ~FunctionSequence() { clearFunctions(); }

    // Function registration methods
    /**
     * @brief Register a function to be part of the sequence
     * @param func Function to register
     * @return ID of the registered function
     */
    [[nodiscard]] std::size_t registerFunction(FunctionType func) {
        std::unique_lock lock(mutex_);
        functions_.emplace_back(std::move(func));
        return functions_.size() - 1;
    }

    /**
     * @brief Register multiple functions at once
     * @param funcs Vector of functions to register
     * @return Vector of registered function IDs
     */
    [[nodiscard]] std::vector<std::size_t> registerFunctions(
        std::span<const FunctionType> funcs) {
        std::vector<std::size_t> ids;
        ids.reserve(funcs.size());

        std::unique_lock lock(mutex_);
        for (const auto& func : funcs) {
            functions_.emplace_back(func);
            ids.push_back(functions_.size() - 1);
        }

        return ids;
    }

    /**
     * @brief Remove all registered functions
     */
    void clearFunctions() noexcept {
        std::unique_lock lock(mutex_);
        functions_.clear();
    }

    /**
     * @brief Get the number of registered functions
     * @return Number of functions
     */
    [[nodiscard]] std::size_t functionCount() const noexcept {
        std::shared_lock lock(mutex_);
        return functions_.size();
    }

    // Standard execution methods
    /**
     * @brief Run the last function with each set of arguments provided
     * @param argsBatch Vector of argument sets
     * @return Vector of results
     * @throws Exception if no functions are registered or execution fails
     */
    [[nodiscard]] std::vector<Result<std::any>> run(
        std::span<const std::vector<std::any>> argsBatch) const {
        std::vector<Result<std::any>> results;
        std::shared_lock lock(mutex_);

        if (functions_.empty()) {
            return {Result<std::any>::makeError(
                "No functions registered in the sequence")};
        }

        results.reserve(argsBatch.size());
        for (const auto& args : argsBatch) {
            try {
                auto& func = functions_.back();
                auto startTime = std::chrono::high_resolution_clock::now();
                auto result = func(args);
                auto endTime = std::chrono::high_resolution_clock::now();

                // Update stats
                stats_.totalExecutionTime +=
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        endTime - startTime);
                stats_.invocationCount++;

                results.push_back(
                    Result<std::any>::makeSuccess(std::move(result)));
            } catch (const std::exception& e) {
                stats_.errorCount++;
                results.push_back(Result<std::any>::makeError(
                    std::string("Exception caught: ") + e.what()));
            }
        }

        return results;
    }

    /**
     * @brief Run all functions with each set of arguments and return all
     * results
     * @param argsBatch Vector of argument sets
     * @return Vector of result vectors
     * @throws Exception if no functions are registered or execution fails
     */
    [[nodiscard]] std::vector<std::vector<Result<std::any>>> runAll(
        std::span<const std::vector<std::any>> argsBatch) const {
        std::vector<std::vector<Result<std::any>>> resultsBatch;
        std::shared_lock lock(mutex_);

        if (functions_.empty()) {
            return {std::vector<Result<std::any>>{Result<std::any>::makeError(
                "No functions registered in the sequence")}};
        }

        resultsBatch.reserve(argsBatch.size());
        for (const auto& args : argsBatch) {
            std::vector<Result<std::any>> results;
            results.reserve(functions_.size());

            for (const auto& func : functions_) {
                try {
                    auto startTime = std::chrono::high_resolution_clock::now();
                    auto result = func(args);
                    auto endTime = std::chrono::high_resolution_clock::now();

                    // Update stats
                    stats_.totalExecutionTime +=
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            endTime - startTime);
                    stats_.invocationCount++;

                    results.push_back(
                        Result<std::any>::makeSuccess(std::move(result)));
                } catch (const std::exception& e) {
                    stats_.errorCount++;
                    results.push_back(Result<std::any>::makeError(
                        std::string("Exception caught: ") + e.what()));
                }
            }

            resultsBatch.emplace_back(std::move(results));
        }

        return resultsBatch;
    }

    // Advanced execution methods with configurable options
    /**
     * @brief Execute with configurable options
     * @param argsBatch Vector of argument sets
     * @param options Execution options
     * @return Vector of results
     */
    [[nodiscard]] std::vector<Result<std::any>> execute(
        std::span<const std::vector<std::any>> argsBatch,
        const ExecutionOptions& options) const {
        // Initialize result container
        std::vector<Result<std::any>> results;

        // Apply execution policy
        if (options.policy == ExecutionPolicy::Parallel) {
            return executeParallel(argsBatch, options);
        } else if (options.policy == ExecutionPolicy::ParallelAsync) {
            return executeParallelAsync(argsBatch, options).get();
        }

        // Standard sequential execution with options
        if (options.timeout) {
            return executeWithTimeout(argsBatch, *options.timeout);
        } else if (options.retryCount) {
            return executeWithRetries(argsBatch, *options.retryCount);
        } else if (options.enableCaching) {
            return executeWithCaching(argsBatch);
        } else if (options.notificationCallback) {
            return executeWithNotification(argsBatch,
                                           options.notificationCallback);
        } else {
            return run(argsBatch);
        }
    }

    /**
     * @brief Execute all functions with configurable options
     * @param argsBatch Vector of argument sets
     * @param options Execution options
     * @return Vector of result vectors
     */
    [[nodiscard]] std::vector<std::vector<Result<std::any>>> executeAll(
        std::span<const std::vector<std::any>> argsBatch,
        const ExecutionOptions& options) const {
        // Initialize result container
        std::vector<std::vector<Result<std::any>>> resultsBatch;

        // Apply execution policy
        if (options.policy == ExecutionPolicy::Parallel) {
            return executeAllParallel(argsBatch, options);
        } else if (options.policy == ExecutionPolicy::ParallelAsync) {
            return executeAllParallelAsync(argsBatch, options).get();
        }

        // Standard sequential execution with options
        if (options.timeout) {
            return executeAllWithTimeout(argsBatch, *options.timeout);
        } else if (options.retryCount) {
            return executeAllWithRetries(argsBatch, *options.retryCount);
        } else if (options.enableCaching) {
            return executeAllWithCaching(argsBatch);
        } else {
            return runAll(argsBatch);
        }
    }

    // Asynchronous execution methods
    /**
     * @brief Run the last function asynchronously
     * @param argsBatch Vector of argument sets
     * @return Future with results
     */
    [[nodiscard]] std::future<std::vector<Result<std::any>>> runAsync(
        std::vector<std::vector<std::any>> argsBatch) const {
#ifdef ATOM_USE_BOOST
        return boost::async(boost::launch::async,
                            [this, argsBatch = std::move(argsBatch)]() mutable {
                                return this->run(std::span(argsBatch));
                            });
#else
        return std::async(std::launch::async,
                          [this, argsBatch = std::move(argsBatch)]() mutable {
                              return this->run(std::span(argsBatch));
                          });
#endif
    }

    /**
     * @brief Run all functions asynchronously
     * @param argsBatch Vector of argument sets
     * @return Future with results
     */
    [[nodiscard]] std::future<std::vector<std::vector<Result<std::any>>>>
    runAllAsync(std::vector<std::vector<std::any>> argsBatch) const {
#ifdef ATOM_USE_BOOST
        return boost::async(boost::launch::async,
                            [this, argsBatch = std::move(argsBatch)]() mutable {
                                return this->runAll(std::span(argsBatch));
                            });
#else
        return std::async(std::launch::async,
                          [this, argsBatch = std::move(argsBatch)]() mutable {
                              return this->runAll(std::span(argsBatch));
                          });
#endif
    }

    // Execution with timeout
    /**
     * @brief Run with timeout
     * @param argsBatch Vector of argument sets
     * @param timeout Timeout duration
     * @return Vector of results
     */
    [[nodiscard]] std::vector<Result<std::any>> executeWithTimeout(
        std::span<const std::vector<std::any>> argsBatch,
        std::chrono::milliseconds timeout) const {
#ifdef ATOM_USE_BOOST
        boost::asio::io_context io;
        boost::asio::steady_timer timer(io, timeout);
        std::optional<std::vector<Result<std::any>>> resultOpt;
        std::atomic<bool> completed{false};

        std::jthread ioThread([&io]() { io.run(); });

        std::jthread funcThread([&]() {
            resultOpt = run(argsBatch);
            completed.store(true, std::memory_order_release);
            timer.cancel();
        });

        timer.async_wait([&](const boost::system::error_code& ec) {
            if (!ec && !completed.load(std::memory_order_acquire)) {
                stats_.errorCount++;
                resultOpt =
                    std::vector<Result<std::any>>{Result<std::any>::makeError(
                        "Function execution timed out")};
            }
        });

        funcThread.join();
        ioThread.join();

        return resultOpt.value_or(std::vector<Result<std::any>>{
            Result<std::any>::makeError("No result produced within timeout")});
#else
        std::vector<std::vector<std::any>> argsCopy(argsBatch.begin(),
                                                    argsBatch.end());
        auto future = runAsync(std::move(argsCopy));

        if (future.wait_for(timeout) == std::future_status::timeout) {
            stats_.errorCount++;
            return {
                Result<std::any>::makeError("Function execution timed out")};
        }

        try {
            return future.get();
        } catch (const std::exception& e) {
            stats_.errorCount++;
            return {Result<std::any>::makeError(
                std::string("Exception during async execution: ") + e.what())};
        }
#endif
    }

    /**
     * @brief Run all functions with timeout
     * @param argsBatch Vector of argument sets
     * @param timeout Timeout duration
     * @return Vector of result vectors
     */
    [[nodiscard]] std::vector<std::vector<Result<std::any>>>
    executeAllWithTimeout(std::span<const std::vector<std::any>> argsBatch,
                          std::chrono::milliseconds timeout) const {
#ifdef ATOM_USE_BOOST
        boost::asio::io_context io;
        boost::asio::steady_timer timer(io, timeout);
        std::optional<std::vector<std::vector<Result<std::any>>>> resultOpt;
        std::atomic<bool> completed{false};

        std::jthread ioThread([&io]() { io.run(); });

        std::jthread funcThread([&]() {
            resultOpt = runAll(argsBatch);
            completed.store(true, std::memory_order_release);
            timer.cancel();
        });

        timer.async_wait([&](const boost::system::error_code& ec) {
            if (!ec && !completed.load(std::memory_order_acquire)) {
                stats_.errorCount++;
                resultOpt = std::vector<std::vector<Result<std::any>>>{
                    {Result<std::any>::makeError(
                        "Function execution timed out")}};
            }
        });

        funcThread.join();
        ioThread.join();

        return resultOpt.value_or(std::vector<std::vector<Result<std::any>>>{
            {Result<std::any>::makeError(
                "No result produced within timeout")}});
#else
        std::vector<std::vector<std::any>> argsCopy(argsBatch.begin(),
                                                    argsBatch.end());
        auto future = runAllAsync(std::move(argsCopy));

        if (future.wait_for(timeout) == std::future_status::timeout) {
            stats_.errorCount++;
            return {
                {Result<std::any>::makeError("Function execution timed out")}};
        }

        try {
            return future.get();
        } catch (const std::exception& e) {
            stats_.errorCount++;
            return {{Result<std::any>::makeError(
                std::string("Exception during async execution: ") + e.what())}};
        }
#endif
    }

    // Execution with retries
    /**
     * @brief Run with retries
     * @param argsBatch Vector of argument sets
     * @param retries Number of retry attempts
     * @return Vector of results
     */
    [[nodiscard]] std::vector<Result<std::any>> executeWithRetries(
        std::span<const std::vector<std::any>> argsBatch,
        size_t retries) const {
        std::vector<Result<std::any>> results;
        size_t attempts = 0;
        bool success = false;

        do {
            try {
                results = run(argsBatch);
                success = std::all_of(
                    results.begin(), results.end(),
                    [](const auto& result) { return result.isSuccess(); });
                if (success)
                    break;
            } catch (const std::exception& e) {
                stats_.errorCount++;
                if (attempts == retries) {
                    return {Result<std::any>::makeError(
                        std::string("Failed after all retry attempts: ") +
                        e.what())};
                }
            }
            attempts++;

            // Exponential backoff
            if (attempts < retries) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(100 * (1 << attempts)));
            }
        } while (attempts <= retries);

        return results;
    }

    /**
     * @brief Run all functions with retries
     * @param argsBatch Vector of argument sets
     * @param retries Number of retry attempts
     * @return Vector of result vectors
     */
    [[nodiscard]] std::vector<std::vector<Result<std::any>>>
    executeAllWithRetries(std::span<const std::vector<std::any>> argsBatch,
                          size_t retries) const {
        std::vector<std::vector<Result<std::any>>> resultsBatch;
        size_t attempts = 0;
        bool success = false;

        do {
            try {
                resultsBatch = runAll(argsBatch);

                // Check if all results are successful
                success = true;
                for (const auto& results : resultsBatch) {
                    if (!std::all_of(results.begin(), results.end(),
                                     [](const auto& result) {
                                         return result.isSuccess();
                                     })) {
                        success = false;
                        break;
                    }
                }

                if (success)
                    break;
            } catch (const std::exception& e) {
                stats_.errorCount++;
                if (attempts == retries) {
                    return {{Result<std::any>::makeError(
                        std::string("Failed after all retry attempts: ") +
                        e.what())}};
                }
            }
            attempts++;

            // Exponential backoff
            if (attempts < retries) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(100 * (1 << attempts)));
            }
        } while (attempts <= retries);

        return resultsBatch;
    }

    // Execution with caching
    /**
     * @brief Run with caching
     * @param argsBatch Vector of argument sets
     * @return Vector of results
     */
    [[nodiscard]] std::vector<Result<std::any>> executeWithCaching(
        std::span<const std::vector<std::any>> argsBatch) const {
        std::vector<Result<std::any>> results;
        std::shared_lock lock(mutex_);

        if (functions_.empty()) {
            return {Result<std::any>::makeError(
                "No functions registered in the sequence")};
        }

        try {
            auto& func = functions_.back();
            results.reserve(argsBatch.size());

            for (const auto& args : argsBatch) {
                auto key = generateCacheKey(args);
                {
                    std::shared_lock cacheLock(cacheMutex_);
                    if (auto it = cache_.find(key); it != cache_.end()) {
                        stats_.cacheHits++;
                        results.push_back(
                            Result<std::any>::makeSuccess(it->second));
                        continue;
                    }
                }

                stats_.cacheMisses++;
                auto startTime = std::chrono::high_resolution_clock::now();
                auto result = func(args);
                auto endTime = std::chrono::high_resolution_clock::now();

                // Update stats
                stats_.totalExecutionTime +=
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        endTime - startTime);
                stats_.invocationCount++;

                {
                    std::unique_lock cacheLock(cacheMutex_);
                    cache_[key] = result;
                }

                results.push_back(
                    Result<std::any>::makeSuccess(std::move(result)));
            }
        } catch (const std::exception& e) {
            stats_.errorCount++;
            results.push_back(Result<std::any>::makeError(
                std::string("Exception caught: ") + e.what()));
        }

        return results;
    }

    /**
     * @brief Run all functions with caching
     * @param argsBatch Vector of argument sets
     * @return Vector of result vectors
     */
    [[nodiscard]] std::vector<std::vector<Result<std::any>>>
    executeAllWithCaching(
        std::span<const std::vector<std::any>> argsBatch) const {
        std::vector<std::vector<Result<std::any>>> resultsBatch;
        std::shared_lock lock(mutex_);

        if (functions_.empty()) {
            return {{Result<std::any>::makeError(
                "No functions registered in the sequence")}};
        }

        try {
            resultsBatch.reserve(argsBatch.size());

            for (const auto& args : argsBatch) {
                std::vector<Result<std::any>> results;
                results.reserve(functions_.size());

                for (size_t i = 0; i < functions_.size(); i++) {
                    const auto& func = functions_[i];
                    // Generate a cache key that includes the function index
                    auto key = generateCacheKey(args, i);

                    {
                        std::shared_lock cacheLock(cacheMutex_);
                        if (auto it = cache_.find(key); it != cache_.end()) {
                            stats_.cacheHits++;
                            results.push_back(
                                Result<std::any>::makeSuccess(it->second));
                            continue;
                        }
                    }

                    stats_.cacheMisses++;
                    auto startTime = std::chrono::high_resolution_clock::now();
                    auto result = func(args);
                    auto endTime = std::chrono::high_resolution_clock::now();

                    // Update stats
                    stats_.totalExecutionTime +=
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            endTime - startTime);
                    stats_.invocationCount++;

                    {
                        std::unique_lock cacheLock(cacheMutex_);
                        cache_[key] = result;
                    }

                    results.push_back(
                        Result<std::any>::makeSuccess(std::move(result)));
                }

                resultsBatch.emplace_back(std::move(results));
            }
        } catch (const std::exception& e) {
            stats_.errorCount++;
            return {{Result<std::any>::makeError(
                std::string("Exception caught: ") + e.what())}};
        }

        return resultsBatch;
    }

    // Execution with notification
    /**
     * @brief Run with notification callback
     * @param argsBatch Vector of argument sets
     * @param callback Callback function for notifications
     * @return Vector of results
     */
    [[nodiscard]] std::vector<Result<std::any>> executeWithNotification(
        std::span<const std::vector<std::any>> argsBatch,
        const std::function<void(const std::any&)>& callback) const {
        auto results = run(argsBatch);

        for (const auto& result : results) {
            if (result.isSuccess() && callback) {
                callback(result.value());
            }
        }

        return results;
    }

    // Parallel execution methods
    /**
     * @brief Execute in parallel
     * @param argsBatch Vector of argument sets
     * @param options Execution options
     * @return Vector of results
     */
    [[nodiscard]] std::vector<Result<std::any>> executeParallel(
        std::span<const std::vector<std::any>> argsBatch,
        const ExecutionOptions& options) const {
        std::vector<Result<std::any>> results;
        results.reserve(argsBatch.size());
        std::shared_lock lock(mutex_);

        if (functions_.empty()) {
            return {Result<std::any>::makeError(
                "No functions registered in the sequence")};
        }

        auto& func = functions_.back();
        std::atomic<size_t> counter{0};
        std::atomic<size_t> errorCount{0};

        // Use std::jthread for automatic joining
        std::vector<std::jthread> threads;
        threads.reserve(
            std::min(argsBatch.size(),
                     static_cast<size_t>(std::thread::hardware_concurrency())));

        // Create worker function
        auto worker = [&]() {
            while (true) {
                // Get next work item
                size_t index = counter.fetch_add(1, std::memory_order_relaxed);
                if (index >= argsBatch.size())
                    break;

                try {
                    auto startTime = std::chrono::high_resolution_clock::now();
                    auto result = func(argsBatch[index]);
                    auto endTime = std::chrono::high_resolution_clock::now();

                    // Update stats (thread-safe)
                    stats_.totalExecutionTime +=
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            endTime - startTime);
                    stats_.invocationCount++;

                    results[index] =
                        Result<std::any>::makeSuccess(std::move(result));

                    // Apply notification if configured
                    if (options.notificationCallback &&
                        results[index].isSuccess()) {
                        options.notificationCallback(results[index].value());
                    }
                } catch (const std::exception& e) {
                    errorCount.fetch_add(1, std::memory_order_relaxed);
                    results[index] = Result<std::any>::makeError(
                        std::string("Exception in parallel execution: ") +
                        e.what());
                }
            }
        };

        // Create and launch threads
        for (unsigned i = 0;
             i <
             std::min(argsBatch.size(),
                      static_cast<size_t>(std::thread::hardware_concurrency()));
             ++i) {
            threads.emplace_back(worker);
        }

        // Threads will auto-join due to std::jthread
        threads.clear();

        // Update stats with error count
        stats_.errorCount += errorCount.load(std::memory_order_relaxed);

        return results;
    }

    /**
     * @brief Execute all functions in parallel
     * @param argsBatch Vector of argument sets
     * @param options Execution options
     * @return Vector of result vectors
     */
    [[nodiscard]] std::vector<std::vector<Result<std::any>>> executeAllParallel(
        std::span<const std::vector<std::any>> argsBatch,
        [[maybe_unused]] const ExecutionOptions& options) const {
        std::vector<std::vector<Result<std::any>>> resultsBatch(
            argsBatch.size());
        std::shared_lock lock(mutex_);

        if (functions_.empty()) {
            return {{Result<std::any>::makeError(
                "No functions registered in the sequence")}};
        }

        // Initialize result containers
        for (auto& results : resultsBatch) {
            results.reserve(functions_.size());
            for (size_t i = 0; i < functions_.size(); ++i) {
                results.emplace_back(
                    Result<std::any>::makeError("Placeholder"));
            }
        }

        std::atomic<size_t> counter{0};
        std::atomic<size_t> errorCount{0};

        // Use std::jthread for automatic joining
        std::vector<std::jthread> threads;
        threads.reserve(
            std::min(argsBatch.size() * functions_.size(),
                     static_cast<size_t>(std::thread::hardware_concurrency())));

        // Create worker function
        auto worker = [&]() {
            while (true) {
                // Get next work item
                size_t index = counter.fetch_add(1, std::memory_order_relaxed);
                if (index >= argsBatch.size() * functions_.size())
                    break;

                // Calculate batch and function indices
                size_t batchIndex = index / functions_.size();
                size_t funcIndex = index % functions_.size();

                try {
                    auto startTime = std::chrono::high_resolution_clock::now();
                    auto result = functions_[funcIndex](argsBatch[batchIndex]);
                    auto endTime = std::chrono::high_resolution_clock::now();

                    // Update stats (thread-safe)
                    stats_.totalExecutionTime +=
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            endTime - startTime);
                    stats_.invocationCount++;

                    resultsBatch[batchIndex][funcIndex] =
                        Result<std::any>::makeSuccess(std::move(result));
                } catch (const std::exception& e) {
                    errorCount.fetch_add(1, std::memory_order_relaxed);
                    resultsBatch[batchIndex][funcIndex] =
                        Result<std::any>::makeError(
                            std::string("Exception in parallel execution: ") +
                            e.what());
                }
            }
        };

        // Create and launch threads
        for (unsigned i = 0;
             i <
             std::min(argsBatch.size() * functions_.size(),
                      static_cast<size_t>(std::thread::hardware_concurrency()));
             ++i) {
            threads.emplace_back(worker);
        }

        // Threads will auto-join due to std::jthread
        threads.clear();

        // Update stats with error count
        stats_.errorCount += errorCount.load(std::memory_order_relaxed);

        return resultsBatch;
    }

    /**
     * @brief Execute in parallel asynchronously
     * @param argsBatch Vector of argument sets
     * @param options Execution options
     * @return Future with results
     */
    [[nodiscard]] std::future<std::vector<Result<std::any>>>
    executeParallelAsync(std::span<const std::vector<std::any>> argsBatch,
                         const ExecutionOptions& options) const {
        std::vector<std::vector<std::any>> argsCopy(argsBatch.begin(),
                                                    argsBatch.end());
        return std::async(
            std::launch::async,
            [this, argsCopy = std::move(argsCopy), options]() mutable {
                return this->executeParallel(std::span(argsCopy), options);
            });
    }

    /**
     * @brief Execute all functions in parallel asynchronously
     * @param argsBatch Vector of argument sets
     * @param options Execution options
     * @return Future with results
     */
    [[nodiscard]] std::future<std::vector<std::vector<Result<std::any>>>>
    executeAllParallelAsync(std::span<const std::vector<std::any>> argsBatch,
                            const ExecutionOptions& options) const {
        std::vector<std::vector<std::any>> argsCopy(argsBatch.begin(),
                                                    argsBatch.end());
        return std::async(
            std::launch::async,
            [this, argsCopy = std::move(argsCopy), options]() mutable {
                return this->executeAllParallel(std::span(argsCopy), options);
            });
    }

    // Cache management
    /**
     * @brief Clear the function result cache
     */
    void clearCache() noexcept {
        std::unique_lock lock(cacheMutex_);
        cache_.clear();
    }

    /**
     * @brief Get the current cache size
     * @return Number of cached results
     */
    [[nodiscard]] size_t cacheSize() const noexcept {
        std::shared_lock lock(cacheMutex_);
        return cache_.size();
    }

    /**
     * @brief Set maximum cache size
     * @param size Maximum number of cached results
     */
    void setMaxCacheSize(size_t size) noexcept {
        maxCacheSize_ = size;
        pruneCache();
    }

    // Statistics and diagnostics
    /**
     * @brief Get execution statistics
     * @return Copy of current execution statistics
     */
    [[nodiscard]] ExecutionStats getStats() const noexcept { return stats_; }

    /**
     * @brief Reset execution statistics
     */
    void resetStats() noexcept { stats_.reset(); }

    /**
     * @brief Get average execution time
     * @return Average execution time in milliseconds
     */
    [[nodiscard]] double getAverageExecutionTime() const noexcept {
        if (stats_.invocationCount == 0)
            return 0.0;
        return static_cast<double>(stats_.totalExecutionTime.count()) /
               static_cast<double>(stats_.invocationCount) / 1000000.0;
    }

    /**
     * @brief Get cache hit ratio
     * @return Cache hit ratio (0.0-1.0)
     */
    [[nodiscard]] double getCacheHitRatio() const noexcept {
        size_t totalAccesses = stats_.cacheHits + stats_.cacheMisses;
        if (totalAccesses == 0)
            return 0.0;
        return static_cast<double>(stats_.cacheHits) /
               static_cast<double>(totalAccesses);
    }

private:
    // Member variables
    mutable std::vector<FunctionType> functions_;
    mutable std::shared_mutex mutex_;
    mutable std::unordered_map<std::string, std::any> cache_;
    mutable std::shared_mutex cacheMutex_;
    mutable ExecutionStats stats_{};
    size_t maxCacheSize_{1000};

    // Helper methods
    /**
     * @brief Generate a cache key for a set of arguments
     * @param args Arguments to hash
     * @param functionIndex Optional function index for multi-function caching
     * @return String hash key
     */
    [[nodiscard]] static std::string generateCacheKey(
        const std::vector<std::any>& args,
        std::optional<size_t> functionIndex = std::nullopt) {
        std::string key;

        // Add function index to key if provided
        if (functionIndex) {
            key = "func" + std::to_string(*functionIndex) + "_";
        }

        // Hash each argument and append to key
        for (const auto& arg : args) {
            key += std::to_string(algorithm::computeHash(arg)) + "_";
        }

        return key;
    }

    /**
     * @brief Prune cache if it exceeds maximum size
     */
    void pruneCache() {
        std::unique_lock lock(cacheMutex_);
        if (cache_.size() <= maxCacheSize_)
            return;

        // Simple pruning strategy: remove oldest entries
        // In a real implementation, consider LRU or other caching strategies
        size_t itemsToRemove = cache_.size() - maxCacheSize_;
        auto it = cache_.begin();
        for (size_t i = 0; i < itemsToRemove && it != cache_.end(); ++i, ++it) {
            cache_.erase(it);
        }
    }
};

}  // namespace atom::meta

#endif  // ATOM_META_FUNCTION_SEQUENCE_HPP