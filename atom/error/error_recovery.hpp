/*
 * error_recovery.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Error recovery framework with retry policies and fallback
strategies

**************************************************/

#ifndef ATOM_ERROR_RECOVERY_HPP
#define ATOM_ERROR_RECOVERY_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <random>
#include <unordered_map>
#include <vector>

#include "error_context.hpp"
#include "error_handler.hpp"

namespace atom::error {

/**
 * @brief Retry policy interface
 */
class RetryPolicy {
public:
    virtual ~RetryPolicy() = default;

    /**
     * @brief Check if operation should be retried
     */
    virtual bool shouldRetry(std::shared_ptr<ErrorContext> context) = 0;

    /**
     * @brief Get delay before next retry
     */
    virtual std::chrono::milliseconds getRetryDelay(int attemptNumber) = 0;

    /**
     * @brief Reset policy state
     */
    virtual void reset() = 0;

    /**
     * @brief Clone the policy
     */
    virtual std::unique_ptr<RetryPolicy> clone() const = 0;
};

/**
 * @brief Fixed interval retry policy
 */
class FixedIntervalRetryPolicy : public RetryPolicy {
public:
    FixedIntervalRetryPolicy(int maxRetries,
                             std::chrono::milliseconds interval);

    bool shouldRetry(std::shared_ptr<ErrorContext> context) override;
    std::chrono::milliseconds getRetryDelay(int attemptNumber) override;
    void reset() override;
    std::unique_ptr<RetryPolicy> clone() const override;

private:
    int maxRetries_;
    std::chrono::milliseconds interval_;
    int currentAttempt_;
};

/**
 * @brief Exponential backoff retry policy
 */
class ExponentialBackoffRetryPolicy : public RetryPolicy {
public:
    ExponentialBackoffRetryPolicy(
        int maxRetries, std::chrono::milliseconds baseDelay,
        double multiplier = 2.0,
        std::chrono::milliseconds maxDelay = std::chrono::minutes(5));

    bool shouldRetry(std::shared_ptr<ErrorContext> context) override;
    std::chrono::milliseconds getRetryDelay(int attemptNumber) override;
    void reset() override;
    std::unique_ptr<RetryPolicy> clone() const override;

private:
    int maxRetries_;
    std::chrono::milliseconds baseDelay_;
    double multiplier_;
    std::chrono::milliseconds maxDelay_;
    int currentAttempt_;
};

/**
 * @brief Jittered retry policy (adds randomness to prevent thundering herd)
 */
class JitteredRetryPolicy : public RetryPolicy {
public:
    JitteredRetryPolicy(std::unique_ptr<RetryPolicy> basePolicy,
                        double jitterFactor = 0.1);

    bool shouldRetry(std::shared_ptr<ErrorContext> context) override;
    std::chrono::milliseconds getRetryDelay(int attemptNumber) override;
    void reset() override;
    std::unique_ptr<RetryPolicy> clone() const override;

private:
    std::unique_ptr<RetryPolicy> basePolicy_;
    double jitterFactor_;
    mutable std::mt19937 rng_;
    mutable std::uniform_real_distribution<double> distribution_;
};

/**
 * @brief Circuit breaker states
 */
enum class CircuitBreakerState {
    Closed,   ///< Normal operation
    Open,     ///< Circuit is open, failing fast
    HalfOpen  ///< Testing if service has recovered
};

/**
 * @brief Circuit breaker for preventing cascading failures
 */
class CircuitBreaker {
public:
    CircuitBreaker(int failureThreshold, std::chrono::milliseconds timeout,
                   int successThreshold = 1);

    /**
     * @brief Execute operation with circuit breaker protection
     */
    template <typename Func, typename... Args>
    auto execute(Func&& func, Args&&... args) -> decltype(func(args...));

    /**
     * @brief Record successful operation
     */
    void recordSuccess();

    /**
     * @brief Record failed operation
     */
    void recordFailure();

    /**
     * @brief Get current state
     */
    CircuitBreakerState getState() const;

    /**
     * @brief Get failure statistics
     */
    std::unordered_map<std::string, int> getStatistics() const;

    /**
     * @brief Reset circuit breaker
     */
    void reset();

private:
    void transitionToOpen();
    void transitionToHalfOpen();
    void transitionToClosed();
    bool shouldAttemptReset() const;

    mutable std::mutex mutex_;
    CircuitBreakerState state_;
    int failureCount_;
    int successCount_;
    int failureThreshold_;
    int successThreshold_;
    std::chrono::milliseconds timeout_;
    std::chrono::steady_clock::time_point lastFailureTime_;
    std::atomic<int> totalFailures_;
    std::atomic<int> totalSuccesses_;
};

/**
 * @brief Fallback strategy interface
 */
template <typename T>
class FallbackStrategy {
public:
    virtual ~FallbackStrategy() = default;

    /**
     * @brief Execute fallback operation
     */
    virtual T execute(std::shared_ptr<ErrorContext> context) = 0;

    /**
     * @brief Check if fallback is available
     */
    virtual bool isAvailable() const = 0;
};

/**
 * @brief Default value fallback strategy
 */
template <typename T>
class DefaultValueFallback : public FallbackStrategy<T> {
public:
    explicit DefaultValueFallback(T defaultValue)
        : defaultValue_(std::move(defaultValue)) {}

    T execute(std::shared_ptr<ErrorContext> context) override {
        static_cast<void>(
            context);  // context may carry metadata for other strategies
        return defaultValue_;
    }

    bool isAvailable() const override { return true; }

private:
    T defaultValue_;
};

/**
 * @brief Function-based fallback strategy
 */
template <typename T>
class FunctionFallback : public FallbackStrategy<T> {
public:
    explicit FunctionFallback(
        std::function<T(std::shared_ptr<ErrorContext>)> fallbackFunc)
        : fallbackFunc_(std::move(fallbackFunc)) {}

    T execute(std::shared_ptr<ErrorContext> context) override {
        return fallbackFunc_(context);
    }

    bool isAvailable() const override {
        return static_cast<bool>(fallbackFunc_);
    }

private:
    std::function<T(std::shared_ptr<ErrorContext>)> fallbackFunc_;
};

/**
 * @brief Error recovery executor
 */
template <typename T>
class ErrorRecoveryExecutor {
public:
    ErrorRecoveryExecutor() = default;

    /**
     * @brief Set retry policy
     */
    ErrorRecoveryExecutor& withRetryPolicy(std::unique_ptr<RetryPolicy> policy);

    /**
     * @brief Set circuit breaker
     */
    ErrorRecoveryExecutor& withCircuitBreaker(
        std::shared_ptr<CircuitBreaker> circuitBreaker);

    /**
     * @brief Set fallback strategy
     */
    ErrorRecoveryExecutor& withFallback(
        std::unique_ptr<FallbackStrategy<T>> fallback);

    /**
     * @brief Set timeout
     */
    ErrorRecoveryExecutor& withTimeout(std::chrono::milliseconds timeout);

    /**
     * @brief Execute operation with error recovery
     */
    template <typename Func, typename... Args>
    T execute(Func&& func, Args&&... args);

    /**
     * @brief Execute operation asynchronously with error recovery
     */
    template <typename Func, typename... Args>
    std::future<T> executeAsync(Func&& func, Args&&... args);

private:
    template <typename Func, typename... Args>
    T executeWithRecovery(Func&& func, Args&&... args);

    std::unique_ptr<RetryPolicy> retryPolicy_;
    std::shared_ptr<CircuitBreaker> circuitBreaker_;
    std::unique_ptr<FallbackStrategy<T>> fallbackStrategy_;
    std::chrono::milliseconds timeout_{std::chrono::seconds(30)};
};

/**
 * @brief Bulkhead pattern for resource isolation
 */
class Bulkhead {
public:
    Bulkhead(int maxConcurrentOperations);

    /**
     * @brief Execute operation with bulkhead protection
     */
    template <typename Func, typename... Args>
    auto execute(Func&& func,
                 Args&&... args) -> std::future<decltype(func(args...))>;

    /**
     * @brief Get current statistics
     */
    std::unordered_map<std::string, int> getStatistics() const;

private:
#if __cplusplus >= 202002L
    std::counting_semaphore<> semaphore_;
#else
    // Fallback semaphore implementation for pre-C++20
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    int maxConcurrent_;
    int currentCount_;
#endif
    std::atomic<int> activeOperations_;
    std::atomic<int> totalOperations_;
    std::atomic<int> rejectedOperations_;
};

/**
 * @brief Factory for creating common recovery strategies
 */
class RecoveryStrategyFactory {
public:
    /**
     * @brief Create fixed interval retry policy
     */
    static std::unique_ptr<RetryPolicy> createFixedRetry(
        int maxRetries, std::chrono::milliseconds interval);

    /**
     * @brief Create exponential backoff retry policy
     */
    static std::unique_ptr<RetryPolicy> createExponentialBackoff(
        int maxRetries, std::chrono::milliseconds baseDelay);

    /**
     * @brief Create jittered retry policy
     */
    static std::unique_ptr<RetryPolicy> createJitteredRetry(
        std::unique_ptr<RetryPolicy> basePolicy, double jitterFactor = 0.1);

    /**
     * @brief Create circuit breaker
     */
    static std::shared_ptr<CircuitBreaker> createCircuitBreaker(
        int failureThreshold, std::chrono::milliseconds timeout);

    /**
     * @brief Create default value fallback
     */
    template <typename T>
    static std::unique_ptr<FallbackStrategy<T>> createDefaultFallback(
        T defaultValue);

    /**
     * @brief Create function-based fallback
     */
    template <typename T>
    static std::unique_ptr<FallbackStrategy<T>> createFunctionFallback(
        std::function<T(std::shared_ptr<ErrorContext>)> func);
};

/**
 * @brief Convenience macros for error recovery
 */
#define WITH_RETRY(maxRetries, interval)                                       \
    atom::error::ErrorRecoveryExecutor<decltype(operation())>()                \
        .withRetryPolicy(                                                      \
            atom::error::RecoveryStrategyFactory::createFixedRetry(maxRetries, \
                                                                   interval))

#define WITH_EXPONENTIAL_BACKOFF(maxRetries, baseDelay)                     \
    atom::error::ErrorRecoveryExecutor<decltype(operation())>()             \
        .withRetryPolicy(                                                   \
            atom::error::RecoveryStrategyFactory::createExponentialBackoff( \
                maxRetries, baseDelay))

#define WITH_CIRCUIT_BREAKER(failureThreshold, timeout)             \
    .withCircuitBreaker(                                            \
        atom::error::RecoveryStrategyFactory::createCircuitBreaker( \
            failureThreshold, timeout))

#define WITH_FALLBACK(defaultValue)                                            \
    .withFallback(atom::error::RecoveryStrategyFactory::createDefaultFallback( \
        defaultValue))

// =======================
// Template Implementations
// =======================

// CircuitBreaker inline template

template <typename Func, typename... Args>
auto atom::error::CircuitBreaker::execute(Func&& func, Args&&... args)
    -> decltype(func(args...)) {
    // Check and possibly transition state under lock, but do NOT hold the lock
    // while executing user code
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == CircuitBreakerState::Open) {
            if (shouldAttemptReset()) {
                transitionToHalfOpen();
            } else {
                throw std::runtime_error("Circuit breaker is open");
            }
        }
    }

    try {
        auto result = func(std::forward<Args>(args)...);
        // recordSuccess acquires the mutex internally; safe since we don't hold
        // it here
        recordSuccess();
        return result;
    } catch (...) {
        // recordFailure acquires the mutex internally; safe since we don't hold
        // it here
        recordFailure();
        throw;
    }
}

// ErrorRecoveryExecutor inline templates

template <typename T>
inline atom::error::ErrorRecoveryExecutor<T>&
atom::error::ErrorRecoveryExecutor<T>::withRetryPolicy(
    std::unique_ptr<RetryPolicy> policy) {
    retryPolicy_ = std::move(policy);
    return *this;
}

template <typename T>
inline atom::error::ErrorRecoveryExecutor<T>&
atom::error::ErrorRecoveryExecutor<T>::withCircuitBreaker(
    std::shared_ptr<CircuitBreaker> circuitBreaker) {
    circuitBreaker_ = std::move(circuitBreaker);
    return *this;
}

template <typename T>
inline atom::error::ErrorRecoveryExecutor<T>&
atom::error::ErrorRecoveryExecutor<T>::withFallback(
    std::unique_ptr<FallbackStrategy<T>> fallback) {
    fallbackStrategy_ = std::move(fallback);
    return *this;
}

template <typename T>
inline atom::error::ErrorRecoveryExecutor<T>&
atom::error::ErrorRecoveryExecutor<T>::withTimeout(
    std::chrono::milliseconds timeout) {
    timeout_ = timeout;
    return *this;
}

template <typename T>
template <typename Func, typename... Args>
inline T atom::error::ErrorRecoveryExecutor<T>::execute(Func&& func,
                                                        Args&&... args) {
    return executeWithRecovery(std::forward<Func>(func),
                               std::forward<Args>(args)...);
}

template <typename T>
template <typename Func, typename... Args>
inline std::future<T> atom::error::ErrorRecoveryExecutor<T>::executeAsync(
    Func&& func, Args&&... args) {
    return std::async(std::launch::async,
                      [this, func = std::forward<Func>(func),
                       ... args = std::forward<Args>(args)]() mutable {
                          return executeWithRecovery(
                              std::move(func), std::forward<Args>(args)...);
                      });
}

template <typename T>
template <typename Func, typename... Args>
inline T atom::error::ErrorRecoveryExecutor<T>::executeWithRecovery(
    Func&& func, Args&&... args) {
    std::shared_ptr<ErrorContext> lastError;
    int attemptNumber = 0;

    if (retryPolicy_) {
        retryPolicy_->reset();
    }

    while (true) {
        try {
            if (circuitBreaker_) {
                return circuitBreaker_->execute(std::forward<Func>(func),
                                                std::forward<Args>(args)...);
            } else {
                return std::forward<Func>(func)(std::forward<Args>(args)...);
            }
        } catch (const std::exception& e) {
            lastError = ErrorContext::create(
                static_cast<int>(ErrorCodeBase::Failed), e.what());
            lastError->incrementRetryCount();

            if (retryPolicy_ && retryPolicy_->shouldRetry(lastError)) {
                auto delay = retryPolicy_->getRetryDelay(attemptNumber);
                std::this_thread::sleep_for(delay);
                attemptNumber++;
                continue;
            }

            if (fallbackStrategy_ && fallbackStrategy_->isAvailable()) {
                return fallbackStrategy_->execute(lastError);
            }

            throw;
        }
    }
}

// Bulkhead::execute inline template

template <typename Func, typename... Args>
inline auto atom::error::Bulkhead::execute(Func&& func, Args&&... args)
    -> std::future<decltype(func(args...))> {
    totalOperations_++;

#if __cplusplus >= 202002L
    if (!semaphore_.try_acquire()) {
        rejectedOperations_++;
        throw std::runtime_error("Bulkhead capacity exceeded");
    }
#else
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (currentCount_ >= maxConcurrent_) {
            rejectedOperations_++;
            throw std::runtime_error("Bulkhead capacity exceeded");
        }
        currentCount_++;
    }
#endif

    activeOperations_++;

    return std::async(std::launch::async,
                      [this, func = std::forward<Func>(func),
                       ... args = std::forward<Args>(args)]() mutable {
                          try {
                              auto result = func(std::forward<Args>(args)...);
                              activeOperations_--;
#if __cplusplus >= 202002L
                              semaphore_.release();
#else
            {
                std::lock_guard<std::mutex> lock(mutex_);
                currentCount_--;
                condition_.notify_one();
            }
#endif
                              return result;
                          } catch (...) {
                              activeOperations_--;
#if __cplusplus >= 202002L
                              semaphore_.release();
#else
            {
                std::lock_guard<std::mutex> lock(mutex_);
                currentCount_--;
                condition_.notify_one();
            }
#endif
                              throw;
                          }
                      });
}

}  // namespace atom::error

#endif  // ATOM_ERROR_RECOVERY_HPP
