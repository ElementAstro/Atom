/*
 * retry.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file retry.hpp
 * @brief Retry mechanism for database operations.
 */

#ifndef ATOM_SEARCH_DATABASE_RETRY_HPP
#define ATOM_SEARCH_DATABASE_RETRY_HPP

#include <chrono>
#include <functional>
#include <optional>
#include <random>
#include <thread>
#include <type_traits>

#include <spdlog/spdlog.h>

namespace atom::search::database {

/**
 * @brief Retry policy configuration.
 */
struct RetryPolicy {
    size_t maxRetries{3};
    std::chrono::milliseconds initialDelay{100};
    std::chrono::milliseconds maxDelay{5000};
    double backoffMultiplier{2.0};
    bool useJitter{true};
    double jitterFactor{0.1};

    static RetryPolicy noRetry() {
        return RetryPolicy{0,
                           std::chrono::milliseconds{0},
                           std::chrono::milliseconds{0},
                           1.0,
                           false,
                           0.0};
    }

    static RetryPolicy aggressive() {
        return RetryPolicy{5,
                           std::chrono::milliseconds{50},
                           std::chrono::milliseconds{2000},
                           1.5,
                           true,
                           0.2};
    }

    static RetryPolicy conservative() {
        return RetryPolicy{3,
                           std::chrono::milliseconds{500},
                           std::chrono::milliseconds{10000},
                           2.0,
                           true,
                           0.1};
    }
};

/**
 * @brief Result of a retry operation.
 */
template <typename T>
struct RetryResult {
    std::optional<T> value;
    size_t attempts{0};
    bool success{false};
    std::string lastError;

    explicit operator bool() const noexcept { return success; }
};

template <>
struct RetryResult<void> {
    size_t attempts{0};
    bool success{false};
    std::string lastError;

    explicit operator bool() const noexcept { return success; }
};

/**
 * @brief Retry executor for database operations.
 */
class RetryExecutor {
public:
    explicit RetryExecutor(RetryPolicy policy = {})
        : policy_(std::move(policy)) {}

    /**
     * @brief Execute an operation with retry logic.
     * @tparam Func Callable type.
     * @param operation The operation to execute.
     * @param shouldRetry Optional predicate to determine if retry should occur.
     * @return RetryResult containing the result or error information.
     */
    template <typename Func>
    auto execute(Func&& operation,
                 std::function<bool(const std::exception&)> shouldRetry =
                     nullptr) -> RetryResult<std::invoke_result_t<Func>> {
        using ResultType = std::invoke_result_t<Func>;
        RetryResult<ResultType> result;

        std::chrono::milliseconds currentDelay = policy_.initialDelay;

        for (size_t attempt = 0; attempt <= policy_.maxRetries; ++attempt) {
            result.attempts = attempt + 1;

            try {
                if constexpr (std::is_void_v<ResultType>) {
                    operation();
                    result.success = true;
                    return result;
                } else {
                    result.value = operation();
                    result.success = true;
                    return result;
                }
            } catch (const std::exception& e) {
                result.lastError = e.what();

                if (attempt >= policy_.maxRetries) {
                    spdlog::error("Operation failed after {} attempts: {}",
                                  attempt + 1, e.what());
                    break;
                }

                if (shouldRetry && !shouldRetry(e)) {
                    spdlog::debug(
                        "Retry predicate returned false, not retrying");
                    break;
                }

                spdlog::warn(
                    "Operation failed (attempt {}), retrying in {}ms: {}",
                    attempt + 1, currentDelay.count(), e.what());

                std::this_thread::sleep_for(calculateDelay(currentDelay));
                currentDelay = calculateNextDelay(currentDelay);
            }
        }

        return result;
    }

    /**
     * @brief Execute with automatic exception propagation on final failure.
     */
    template <typename Func>
    auto executeOrThrow(Func&& operation,
                        std::function<bool(const std::exception&)> shouldRetry =
                            nullptr) -> std::invoke_result_t<Func> {
        auto result =
            execute(std::forward<Func>(operation), std::move(shouldRetry));

        if (!result.success) {
            throw std::runtime_error("Operation failed after " +
                                     std::to_string(result.attempts) +
                                     " attempts: " + result.lastError);
        }

        if constexpr (!std::is_void_v<std::invoke_result_t<Func>>) {
            return std::move(*result.value);
        }
    }

private:
    std::chrono::milliseconds calculateDelay(
        std::chrono::milliseconds baseDelay) {
        if (!policy_.useJitter) {
            return baseDelay;
        }

        static thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<> dis(1.0 - policy_.jitterFactor,
                                             1.0 + policy_.jitterFactor);
        auto jitteredDelay =
            static_cast<long long>(baseDelay.count() * dis(gen));
        return std::chrono::milliseconds{jitteredDelay};
    }

    std::chrono::milliseconds calculateNextDelay(
        std::chrono::milliseconds currentDelay) {
        auto nextDelay = static_cast<long long>(currentDelay.count() *
                                                policy_.backoffMultiplier);
        return std::chrono::milliseconds{
            std::min(nextDelay, policy_.maxDelay.count())};
    }

    RetryPolicy policy_;
};

/**
 * @brief Convenience function for one-off retry operations.
 */
template <typename Func>
auto withRetry(Func&& operation, RetryPolicy policy = {})
    -> RetryResult<std::invoke_result_t<Func>> {
    RetryExecutor executor(std::move(policy));
    return executor.execute(std::forward<Func>(operation));
}

}  // namespace atom::search::database

#endif  // ATOM_SEARCH_DATABASE_RETRY_HPP
