/*!
 * \file stepper.hpp
 * \brief Advanced Function Sequence Management
 * \author Max Qian <lightapt.com>, Enhanced by Claude
 * \date 2024-03-01, Updated 2025-05-26
 */

#ifndef ATOM_META_STEPPER_HPP
#define ATOM_META_STEPPER_HPP

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
#include <string_view>
#include <thread>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace atom::meta {

/**
 * @brief Result wrapper with success/error state for sequence steps
 *
 * Distinct from `atom::meta::Result` (the `type::expected`-based monadic result
 * in invoke.hpp): this is the value-semantics, string-diagnostic result used by
 * `FunctionSequence`. Kept under a separate name so the canonical `Result`
 * denotes exactly one type module-wide.
 *
 * @tparam T Type of the success value
 */
template <typename T>
class StepResult {
public:
    /**
     * @brief Default constructor. Initializes to an error state.
     */
    StepResult() : data_(std::string("StepResult not initialized")) {}

    /**
     * @brief Create a success result
     * @param value Success value
     * @return Result with success state
     */
    static StepResult<T> makeSuccess(T value) {
        return StepResult<T>(std::move(value));
    }

    /**
     * @brief Create an error result
     * @param error Error message
     * @return Result with error state
     */
    static StepResult<T> makeError(std::string error) {
        return StepResult<T>(std::move(error));
    }

    /**
     * @brief Check if result is success
     * @return True if success, false otherwise
     */
    [[nodiscard]] bool isSuccess() const noexcept {
        return std::holds_alternative<T>(data_);
    }

    /**
     * @brief Check if result is error
     * @return True if error, false otherwise
     */
    [[nodiscard]] bool isError() const noexcept {
        return std::holds_alternative<std::string>(data_);
    }

    /**
     * @brief Get success value
     * @return Success value
     * @throws std::runtime_error if result is error
     */
    [[nodiscard]] const T& value() const {
        if (isError()) {
            throw std::runtime_error("Cannot get value from error result: " +
                                     std::get<std::string>(data_));
        }
        return std::get<T>(data_);
    }

    /**
     * @brief Get error message
     * @return Error message
     * @throws std::runtime_error if result is success
     */
    [[nodiscard]] const std::string& error() const {
        if (isSuccess()) {
            throw std::runtime_error("Cannot get error from success result");
        }
        return std::get<std::string>(data_);
    }

    /**
     * @brief Get success value or a default
     * @param defaultValue Default value to return if error
     * @return Success value or default
     */
    [[nodiscard]] T valueOr(T defaultValue) const {
        if (isSuccess()) {
            return std::get<T>(data_);
        }
        return defaultValue;
    }

private:
    std::variant<T, std::string> data_;

    explicit StepResult(T value) : data_(std::move(value)) {}
    explicit StepResult(std::string error) : data_(std::move(error)) {}
};

/**
 * @brief Enhanced function sequence with modern C++ features
 */
class FunctionSequence {
public:
    using FunctionType = std::function<std::any(std::span<const std::any>)>;

    /**
     * @brief Execution statistics for monitoring performance
     */
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

    /**
     * @brief Execution policy for controlling how functions are executed
     */
    enum class ExecutionPolicy { Sequential, Parallel, ParallelAsync };

    /**
     * @brief Options for configuring function execution
     */
    struct ExecutionOptions {
        std::optional<std::chrono::milliseconds> timeout = std::nullopt;
        std::optional<size_t> retryCount = std::nullopt;
        bool enableCaching = false;
        bool enableLogging = false;
        ExecutionPolicy policy = ExecutionPolicy::Sequential;
        std::function<void(const std::any&)> notificationCallback = nullptr;
    };

    FunctionSequence() = default;
    FunctionSequence(const FunctionSequence&) = delete;
    FunctionSequence& operator=(const FunctionSequence&) = delete;
    ~FunctionSequence() { clearFunctions(); }

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

    /**
     * @brief Run the last function with each set of arguments provided
     * @param argsBatch Vector of argument sets
     * @return Vector of results
     */
    [[nodiscard]] std::vector<StepResult<std::any>> run(
        std::span<const std::vector<std::any>> argsBatch) const {
        std::vector<StepResult<std::any>> results;
        std::shared_lock lock(mutex_);

        if (functions_.empty()) {
            return {StepResult<std::any>::makeError(
                "No functions registered in the sequence")};
        }

        results.reserve(argsBatch.size());
        for (const auto& args : argsBatch) {
            try {
                auto& func = functions_.back();
                auto startTime = std::chrono::high_resolution_clock::now();
                auto result = func(args);
                auto endTime = std::chrono::high_resolution_clock::now();

                stats_.totalExecutionTime +=
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        endTime - startTime);
                stats_.invocationCount++;

                results.push_back(
                    StepResult<std::any>::makeSuccess(std::move(result)));
            } catch (const std::exception& e) {
                stats_.invocationCount++;
                stats_.errorCount++;
                results.push_back(StepResult<std::any>::makeError(
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
     */
    [[nodiscard]] std::vector<std::vector<StepResult<std::any>>> runAll(
        std::span<const std::vector<std::any>> argsBatch) const {
        std::vector<std::vector<StepResult<std::any>>> resultsBatch;
        std::shared_lock lock(mutex_);

        if (functions_.empty()) {
            return {std::vector<StepResult<std::any>>{StepResult<std::any>::makeError(
                "No functions registered in the sequence")}};
        }

        resultsBatch.reserve(argsBatch.size());
        for (const auto& args : argsBatch) {
            std::vector<StepResult<std::any>> results;
            results.reserve(functions_.size());

            for (const auto& func : functions_) {
                try {
                    auto startTime = std::chrono::high_resolution_clock::now();
                    auto result = func(args);
                    auto endTime = std::chrono::high_resolution_clock::now();

                    stats_.totalExecutionTime +=
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            endTime - startTime);
                    stats_.invocationCount++;

                    results.push_back(
                        StepResult<std::any>::makeSuccess(std::move(result)));
                } catch (const std::exception& e) {
                    stats_.invocationCount++;
                    stats_.errorCount++;
                    results.push_back(StepResult<std::any>::makeError(
                        std::string("Exception caught: ") + e.what()));
                }
            }

            resultsBatch.emplace_back(std::move(results));
        }

        return resultsBatch;
    }

    /**
     * @brief Execute with configurable options
     * @param argsBatch Vector of argument sets
     * @param options Execution options
     * @return Vector of results
     */
    [[nodiscard]] std::vector<StepResult<std::any>> execute(
        std::span<const std::vector<std::any>> argsBatch,
        const ExecutionOptions& options) const {
        if (options.policy == ExecutionPolicy::Parallel) {
            return executeParallel(argsBatch, options);
        } else if (options.policy == ExecutionPolicy::ParallelAsync) {
            return executeParallelAsync(argsBatch, options).get();
        }

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
    [[nodiscard]] std::vector<std::vector<StepResult<std::any>>> executeAll(
        std::span<const std::vector<std::any>> argsBatch,
        const ExecutionOptions& options) const {
        // Initialize result container
        std::vector<std::vector<StepResult<std::any>>> resultsBatch;

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
    [[nodiscard]] std::future<std::vector<StepResult<std::any>>> runAsync(
        std::vector<std::vector<std::any>> argsBatch) const {
        return std::async(std::launch::async,
                          [this, argsBatch = std::move(argsBatch)]() mutable {
                              return this->run(std::span(argsBatch));
                          });
    }

    /**
     * @brief Run all functions asynchronously
     * @param argsBatch Vector of argument sets
     * @return Future with results
     */
    [[nodiscard]] std::future<std::vector<std::vector<StepResult<std::any>>>>
    runAllAsync(std::vector<std::vector<std::any>> argsBatch) const {
        return std::async(std::launch::async,
                          [this, argsBatch = std::move(argsBatch)]() mutable {
                              return this->runAll(std::span(argsBatch));
                          });
    }

    /**
     * @brief Run with a per-argument-set timeout
     *
     * Each argument set is executed asynchronously and given the full
     * timeout budget; argument sets that do not finish in time yield a
     * timeout error result without affecting the other entries.
     *
     * @param argsBatch Vector of argument sets
     * @param timeout Timeout duration applied to each argument set
     * @return Vector of results
     */
    [[nodiscard]] std::vector<StepResult<std::any>> executeWithTimeout(
        std::span<const std::vector<std::any>> argsBatch,
        std::chrono::milliseconds timeout) const {
        std::shared_lock lock(mutex_);

        if (functions_.empty()) {
            return {StepResult<std::any>::makeError(
                "No functions registered in the sequence")};
        }

        const auto& func = functions_.back();

        // Launch all argument sets concurrently; futures are joined before
        // this function returns (std::async future destructors block), so
        // the references captured below remain valid.
        std::vector<std::future<std::any>> futures;
        futures.reserve(argsBatch.size());
        for (const auto& args : argsBatch) {
            futures.push_back(std::async(
                std::launch::async, [&func, &args]() { return func(args); }));
        }

        const auto deadline = std::chrono::steady_clock::now() + timeout;
        std::vector<StepResult<std::any>> results;
        results.reserve(futures.size());

        for (auto& future : futures) {
            if (future.wait_until(deadline) == std::future_status::timeout) {
                stats_.errorCount++;
                results.push_back(StepResult<std::any>::makeError(
                    "Function execution timed out"));
                continue;
            }

            try {
                auto value = future.get();
                stats_.invocationCount++;
                results.push_back(
                    StepResult<std::any>::makeSuccess(std::move(value)));
            } catch (const std::exception& e) {
                stats_.invocationCount++;
                stats_.errorCount++;
                results.push_back(StepResult<std::any>::makeError(
                    std::string("Exception caught: ") + e.what()));
            }
        }

        return results;
    }

    /**
     * @brief Run all functions with timeout
     * @param argsBatch Vector of argument sets
     * @param timeout Timeout duration
     * @return Vector of result vectors
     */
    [[nodiscard]] std::vector<std::vector<StepResult<std::any>>>
    executeAllWithTimeout(std::span<const std::vector<std::any>> argsBatch,
                          std::chrono::milliseconds timeout) const {
        std::vector<std::vector<std::any>> argsCopy(argsBatch.begin(),
                                                    argsBatch.end());
        auto future = runAllAsync(std::move(argsCopy));

        if (future.wait_for(timeout) == std::future_status::timeout) {
            stats_.errorCount++;
            return {
                {StepResult<std::any>::makeError("Function execution timed out")}};
        }

        try {
            return future.get();
        } catch (const std::exception& e) {
            stats_.errorCount++;
            return {{StepResult<std::any>::makeError(
                std::string("Exception during async execution: ") + e.what())}};
        }
    }

    /**
     * @brief Run with retries
     * @param argsBatch Vector of argument sets
     * @param retries Number of retry attempts
     * @return Vector of results
     */
    [[nodiscard]] std::vector<StepResult<std::any>> executeWithRetries(
        std::span<const std::vector<std::any>> argsBatch,
        size_t retries) const {
        std::vector<StepResult<std::any>> results;
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
                    return {StepResult<std::any>::makeError(
                        std::string("Failed after all retry attempts: ") +
                        e.what())};
                }
            }
            attempts++;

            if (attempts < retries) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(100 * (1 << attempts)));
            }
        } while (attempts <= retries);

        if (!success) {
            // Mark results that are still failing after exhausting retries
            for (auto& result : results) {
                if (result.isError()) {
                    result = StepResult<std::any>::makeError(
                        "Failed after all retry attempts: " + result.error());
                }
            }
        }

        return results;
    }

    /**
     * @brief Run all functions with retries
     * @param argsBatch Vector of argument sets
     * @param retries Number of retry attempts
     * @return Vector of result vectors
     */
    [[nodiscard]] std::vector<std::vector<StepResult<std::any>>>
    executeAllWithRetries(std::span<const std::vector<std::any>> argsBatch,
                          size_t retries) const {
        std::vector<std::vector<StepResult<std::any>>> resultsBatch;
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
                    return {{StepResult<std::any>::makeError(
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

    /**
     * @brief Run with caching
     * @param argsBatch Vector of argument sets
     * @return Vector of results
     */
    [[nodiscard]] std::vector<StepResult<std::any>> executeWithCaching(
        std::span<const std::vector<std::any>> argsBatch) const {
        std::vector<StepResult<std::any>> results;
        std::shared_lock lock(mutex_);

        if (functions_.empty()) {
            return {StepResult<std::any>::makeError(
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
                            StepResult<std::any>::makeSuccess(it->second));
                        continue;
                    }
                }

                stats_.cacheMisses++;
                auto startTime = std::chrono::high_resolution_clock::now();
                auto result = func(args);
                auto endTime = std::chrono::high_resolution_clock::now();

                stats_.totalExecutionTime +=
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        endTime - startTime);
                stats_.invocationCount++;

                {
                    std::unique_lock cacheLock(cacheMutex_);
                    cache_[key] = result;
                }

                results.push_back(
                    StepResult<std::any>::makeSuccess(std::move(result)));
            }
        } catch (const std::exception& e) {
            stats_.errorCount++;
            results.push_back(StepResult<std::any>::makeError(
                std::string("Exception caught: ") + e.what()));
        }

        return results;
    }

    /**
     * @brief Run all functions with caching
     * @param argsBatch Vector of argument sets
     * @return Vector of result vectors
     */
    [[nodiscard]] std::vector<std::vector<StepResult<std::any>>>
    executeAllWithCaching(
        std::span<const std::vector<std::any>> argsBatch) const {
        std::vector<std::vector<StepResult<std::any>>> resultsBatch;
        std::shared_lock lock(mutex_);

        if (functions_.empty()) {
            return {{StepResult<std::any>::makeError(
                "No functions registered in the sequence")}};
        }

        try {
            resultsBatch.reserve(argsBatch.size());

            for (const auto& args : argsBatch) {
                std::vector<StepResult<std::any>> results;
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
                                StepResult<std::any>::makeSuccess(it->second));
                            continue;
                        }
                    }

                    stats_.cacheMisses++;
                    auto startTime = std::chrono::high_resolution_clock::now();
                    auto result = func(args);
                    auto endTime = std::chrono::high_resolution_clock::now();

                    stats_.totalExecutionTime +=
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            endTime - startTime);
                    stats_.invocationCount++;

                    {
                        std::unique_lock cacheLock(cacheMutex_);
                        cache_[key] = result;
                    }

                    results.push_back(
                        StepResult<std::any>::makeSuccess(std::move(result)));
                }

                resultsBatch.emplace_back(std::move(results));
            }
        } catch (const std::exception& e) {
            stats_.errorCount++;
            return {{StepResult<std::any>::makeError(
                std::string("Exception caught: ") + e.what())}};
        }

        return resultsBatch;
    }

    /**
     * @brief Run with notification callback
     * @param argsBatch Vector of argument sets
     * @param callback Callback function for notifications
     * @return Vector of results
     */
    [[nodiscard]] std::vector<StepResult<std::any>> executeWithNotification(
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

    /**
     * @brief Execute in parallel
     * @param argsBatch Vector of argument sets
     * @param options Execution options
     * @return Vector of results
     */
    [[nodiscard]] std::vector<StepResult<std::any>> executeParallel(
        std::span<const std::vector<std::any>> argsBatch,
        const ExecutionOptions& options) const {
        std::vector<StepResult<std::any>> results(argsBatch.size());
        std::shared_lock lock(mutex_);

        if (functions_.empty()) {
            return {StepResult<std::any>::makeError(
                "No functions registered in the sequence")};
        }

        auto& func = functions_.back();
        std::atomic<size_t> counter{0};
        std::atomic<size_t> errorCount{0};

        // Stats are accumulated into thread-local atomics and folded into the
        // non-atomic `stats_` once, after all workers have joined. Writing
        // `stats_` directly from workers is a data race (it is a plain struct).
        std::atomic<size_t> invocationCount{0};
        std::atomic<size_t> cacheHits{0};
        std::atomic<size_t> cacheMisses{0};
        std::atomic<long long> totalExecNs{0};

        std::vector<std::jthread> threads;
        const size_t numThreads =
            std::min(argsBatch.size(),
                     static_cast<size_t>(std::thread::hardware_concurrency()));
        threads.reserve(numThreads);

        auto worker = [&]() {
            while (true) {
                size_t index = counter.fetch_add(1, std::memory_order_relaxed);
                if (index >= argsBatch.size())
                    break;

                try {
                    std::string cacheKey;
                    if (options.enableCaching) {
                        cacheKey = generateCacheKey(argsBatch[index]);
                        bool hit = false;
                        std::any cached;
                        {
                            std::unique_lock cacheLock(cacheMutex_);
                            if (auto it = cache_.find(cacheKey);
                                it != cache_.end()) {
                                cacheHits.fetch_add(1,
                                                    std::memory_order_relaxed);
                                hit = true;
                                cached = it->second;
                            } else {
                                cacheMisses.fetch_add(1,
                                                      std::memory_order_relaxed);
                            }
                        }
                        if (hit) {
                            results[index] =
                                StepResult<std::any>::makeSuccess(std::move(cached));
                            if (options.notificationCallback) {
                                options.notificationCallback(
                                    results[index].value());
                            }
                            continue;
                        }
                    }

                    auto startTime = std::chrono::high_resolution_clock::now();
                    auto result = func(argsBatch[index]);
                    auto endTime = std::chrono::high_resolution_clock::now();

                    if (options.enableCaching) {
                        std::unique_lock cacheLock(cacheMutex_);
                        cache_[cacheKey] = result;
                    }

                    totalExecNs.fetch_add(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            endTime - startTime)
                            .count(),
                        std::memory_order_relaxed);
                    invocationCount.fetch_add(1, std::memory_order_relaxed);

                    results[index] =
                        StepResult<std::any>::makeSuccess(std::move(result));

                    if (options.notificationCallback &&
                        results[index].isSuccess()) {
                        options.notificationCallback(results[index].value());
                    }
                } catch (const std::exception& e) {
                    errorCount.fetch_add(1, std::memory_order_relaxed);
                    results[index] = StepResult<std::any>::makeError(
                        std::string("Exception in parallel execution: ") +
                        e.what());
                }
            }
        };

        for (size_t i = 0; i < numThreads; ++i) {
            threads.emplace_back(worker);
        }

        // Join before returning: `return results` moves the vector the
        // workers are still writing into otherwise.
        threads.clear();

        // All workers have joined: fold the per-thread counters into `stats_`
        // from this single thread.
        stats_.cacheHits += cacheHits.load(std::memory_order_relaxed);
        stats_.cacheMisses += cacheMisses.load(std::memory_order_relaxed);
        stats_.invocationCount += invocationCount.load(std::memory_order_relaxed);
        stats_.totalExecutionTime +=
            std::chrono::nanoseconds{totalExecNs.load(std::memory_order_relaxed)};
        stats_.errorCount += errorCount.load(std::memory_order_relaxed);
        return results;
    }

    /**
     * @brief Execute all functions in parallel
     * @param argsBatch Vector of argument sets
     * @param options Execution options
     * @return Vector of result vectors
     */
    [[nodiscard]] std::vector<std::vector<StepResult<std::any>>> executeAllParallel(
        std::span<const std::vector<std::any>> argsBatch,
        [[maybe_unused]] const ExecutionOptions& options) const {
        std::vector<std::vector<StepResult<std::any>>> resultsBatch(
            argsBatch.size());
        std::shared_lock lock(mutex_);

        if (functions_.empty()) {
            return {{StepResult<std::any>::makeError(
                "No functions registered in the sequence")}};
        }

        // Initialize result containers
        for (auto& results : resultsBatch) {
            results.reserve(functions_.size());
            for (size_t i = 0; i < functions_.size(); ++i) {
                results.emplace_back(
                    StepResult<std::any>::makeError("Placeholder"));
            }
        }

        std::atomic<size_t> counter{0};
        std::atomic<size_t> errorCount{0};

        // Per-thread counters folded into `stats_` after join (see
        // executeParallel): workers must not write the non-atomic `stats_`.
        std::atomic<size_t> invocationCount{0};
        std::atomic<long long> totalExecNs{0};

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

                    totalExecNs.fetch_add(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            endTime - startTime)
                            .count(),
                        std::memory_order_relaxed);
                    invocationCount.fetch_add(1, std::memory_order_relaxed);

                    resultsBatch[batchIndex][funcIndex] =
                        StepResult<std::any>::makeSuccess(std::move(result));
                } catch (const std::exception& e) {
                    errorCount.fetch_add(1, std::memory_order_relaxed);
                    resultsBatch[batchIndex][funcIndex] =
                        StepResult<std::any>::makeError(
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

        // All workers have joined: fold the per-thread counters into `stats_`.
        stats_.invocationCount += invocationCount.load(std::memory_order_relaxed);
        stats_.totalExecutionTime +=
            std::chrono::nanoseconds{totalExecNs.load(std::memory_order_relaxed)};
        stats_.errorCount += errorCount.load(std::memory_order_relaxed);

        return resultsBatch;
    }

    /**
     * @brief Execute in parallel asynchronously
     * @param argsBatch Vector of argument sets
     * @param options Execution options
     * @return Future with results
     */
    [[nodiscard]] std::future<std::vector<StepResult<std::any>>>
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
    [[nodiscard]] std::future<std::vector<std::vector<StepResult<std::any>>>>
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
    mutable std::vector<FunctionType> functions_;
    mutable std::shared_mutex mutex_;
    mutable std::unordered_map<std::string, std::any> cache_;
    mutable std::shared_mutex cacheMutex_;
    mutable ExecutionStats stats_{};
    size_t maxCacheSize_{1000};

    [[nodiscard]] static std::string generateCacheKey(
        const std::vector<std::any>& args,
        std::optional<size_t> functionIndex = std::nullopt) {
        std::string key;

        if (functionIndex) {
            key = "func" + std::to_string(*functionIndex) + "_";
        }

        for (const auto& arg : args) {
            key += std::to_string(hashArgument(arg)) + "_";
        }

        return key;
    }

    /**
     * @brief Hash a type-erased argument for cache key generation
     *
     * Combines the contained type's hash code with a value hash for the
     * common argument types using an FNV-1a style mix. Arguments of other
     * types fall back to a type-only hash.
     */
    [[nodiscard]] static std::size_t hashArgument(const std::any& arg) {
        std::size_t hash = arg.type().hash_code();
        auto combine = [&hash](std::size_t value) {
            constexpr std::size_t kFnvPrime = 0x100000001b3ULL;
            hash = (hash ^ value) * kFnvPrime;
        };

        if (const auto* i = std::any_cast<int>(&arg)) {
            combine(std::hash<int>{}(*i));
        } else if (const auto* u = std::any_cast<unsigned int>(&arg)) {
            combine(std::hash<unsigned int>{}(*u));
        } else if (const auto* l = std::any_cast<long long>(&arg)) {
            combine(std::hash<long long>{}(*l));
        } else if (const auto* sz = std::any_cast<std::size_t>(&arg)) {
            combine(std::hash<std::size_t>{}(*sz));
        } else if (const auto* d = std::any_cast<double>(&arg)) {
            combine(std::hash<double>{}(*d));
        } else if (const auto* f = std::any_cast<float>(&arg)) {
            combine(std::hash<float>{}(*f));
        } else if (const auto* b = std::any_cast<bool>(&arg)) {
            combine(std::hash<bool>{}(*b));
        } else if (const auto* s = std::any_cast<std::string>(&arg)) {
            combine(std::hash<std::string>{}(*s));
        } else if (const auto* sv = std::any_cast<std::string_view>(&arg)) {
            combine(std::hash<std::string_view>{}(*sv));
        }

        return hash;
    }

    void pruneCache() {
        std::unique_lock lock(cacheMutex_);
        if (cache_.size() <= maxCacheSize_)
            return;

        size_t itemsToRemove = cache_.size() - maxCacheSize_;
        auto it = cache_.begin();
        for (size_t i = 0; i < itemsToRemove && it != cache_.end(); ++i) {
            it = cache_.erase(it);
        }
    }
};

//==============================================================================
// C++23 Enhanced Stepper Utilities
//==============================================================================

/**
 * @brief Concept for step functions
 */
template <typename F>
concept StepFunction = std::invocable<F, std::vector<std::any>> &&
                       requires(F f, std::vector<std::any> args) {
                           { f(args) } -> std::convertible_to<std::any>;
                       };

/**
 * @brief Concept for result types
 */
template <typename T>
concept ResultType = requires(T t) {
    { t.isSuccess() } -> std::convertible_to<bool>;
    { t.isError() } -> std::convertible_to<bool>;
};

/**
 * @brief Fluent stepper builder
 */
class StepperBuilder {
    std::shared_ptr<FunctionSequence> stepper_ =
        std::make_shared<FunctionSequence>();

public:
    StepperBuilder() = default;

    template <typename F>
    StepperBuilder& addStep(F&& func) {
        (void)stepper_->registerFunction(
            [f = std::forward<F>(func)](
                std::span<const std::any> args) -> std::any {
                return f(std::vector<std::any>(args.begin(), args.end()));
            });
        return *this;
    }

    template <typename F>
    StepperBuilder& addNamedStep(std::string name, F&& func) {
        // Add with metadata
        (void)stepper_->registerFunction(
            [f = std::forward<F>(func), n = std::move(name)](
                std::span<const std::any> args) -> std::any {
                return f(std::vector<std::any>(args.begin(), args.end()));
            });
        return *this;
    }

    StepperBuilder& withCacheSize(std::size_t size) {
        stepper_->setMaxCacheSize(size);
        return *this;
    }

    [[nodiscard]] std::shared_ptr<FunctionSequence> build() {
        return std::move(stepper_);
    }
};

/**
 * @brief Create a stepper builder
 */
inline auto buildStepper() -> StepperBuilder { return StepperBuilder{}; }

/**
 * @brief Step with retry logic
 */
template <typename F>
class RetryStep {
    F func_;
    std::size_t max_retries_;
    std::chrono::milliseconds delay_;

public:
    RetryStep(F func, std::size_t retries, std::chrono::milliseconds delay)
        : func_(std::move(func)), max_retries_(retries), delay_(delay) {}

    auto operator()(std::vector<std::any> args) -> std::any {
        for (std::size_t attempt = 0; attempt < max_retries_; ++attempt) {
            try {
                return func_(args);
            } catch (...) {
                if (attempt + 1 < max_retries_) {
                    std::this_thread::sleep_for(delay_);
                }
            }
        }
        return std::any{};  // Return empty on all failures
    }
};

/**
 * @brief Create a retry step
 */
template <typename F>
auto makeRetryStep(F&& func, std::size_t retries = 3,
                   std::chrono::milliseconds delay = std::chrono::milliseconds{
                       100}) {
    return RetryStep<std::decay_t<F>>(std::forward<F>(func), retries, delay);
}

/**
 * @brief Conditional step execution
 */
template <typename Condition, typename F>
class ConditionalStep {
    Condition condition_;
    F func_;

public:
    ConditionalStep(Condition cond, F func)
        : condition_(std::move(cond)), func_(std::move(func)) {}

    auto operator()(std::vector<std::any> args) -> std::any {
        if (condition_(args)) {
            return func_(args);
        }
        return std::any{};  // Skip if condition not met
    }
};

/**
 * @brief Create a conditional step
 */
template <typename Condition, typename F>
auto makeConditionalStep(Condition&& cond, F&& func) {
    return ConditionalStep<std::decay_t<Condition>, std::decay_t<F>>(
        std::forward<Condition>(cond), std::forward<F>(func));
}

/**
 * @brief Parallel step execution
 */
class ParallelStepper {
public:
    using StepType = std::function<std::any(std::vector<std::any>)>;

private:
    std::vector<StepType> steps_;

public:
    template <typename F>
    void addStep(F&& func) {
        steps_.emplace_back(
            [f = std::forward<F>(func)](
                std::vector<std::any> args) -> std::any { return f(args); });
    }

    auto executeAll(std::vector<std::any> args) -> std::vector<std::any> {
        std::vector<std::future<std::any>> futures;
        futures.reserve(steps_.size());

        for (const auto& step : steps_) {
            futures.push_back(std::async(std::launch::async, step, args));
        }

        std::vector<std::any> results;
        results.reserve(futures.size());

        for (auto& future : futures) {
            results.push_back(future.get());
        }

        return results;
    }

    [[nodiscard]] std::size_t stepCount() const { return steps_.size(); }
};

/**
 * @brief Step execution observer
 */
class StepObserver {
public:
    using BeforeCallback =
        std::function<void(std::size_t, const std::vector<std::any>&)>;
    using AfterCallback = std::function<void(std::size_t, const std::any&)>;
    using ErrorCallback =
        std::function<void(std::size_t, const std::exception&)>;

private:
    std::vector<BeforeCallback> before_callbacks_;
    std::vector<AfterCallback> after_callbacks_;
    std::vector<ErrorCallback> error_callbacks_;

public:
    void onBefore(BeforeCallback callback) {
        before_callbacks_.push_back(std::move(callback));
    }

    void onAfter(AfterCallback callback) {
        after_callbacks_.push_back(std::move(callback));
    }

    void onError(ErrorCallback callback) {
        error_callbacks_.push_back(std::move(callback));
    }

    void notifyBefore(std::size_t step, const std::vector<std::any>& args) {
        for (const auto& cb : before_callbacks_)
            cb(step, args);
    }

    void notifyAfter(std::size_t step, const std::any& result) {
        for (const auto& cb : after_callbacks_)
            cb(step, result);
    }

    void notifyError(std::size_t step, const std::exception& e) {
        for (const auto& cb : error_callbacks_)
            cb(step, e);
    }
};

}  // namespace atom::meta

#endif  // ATOM_META_STEPPER_HPP
