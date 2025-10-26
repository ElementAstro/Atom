/*
 * error_recovery.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Implementation of error recovery framework

**************************************************/

#include "error_recovery.hpp"
#include <algorithm>
#include <cmath>
#include <future>
#include <random>
#include <thread>

#if __cplusplus >= 202002L
#include <semaphore>
#else
// Fallback for pre-C++20 compilers
#include <condition_variable>
#include <mutex>
#endif

namespace atom::error {

// FixedIntervalRetryPolicy implementation
FixedIntervalRetryPolicy::FixedIntervalRetryPolicy(
    int maxRetries, std::chrono::milliseconds interval)
    : maxRetries_(maxRetries), interval_(interval), currentAttempt_(0) {}

bool FixedIntervalRetryPolicy::shouldRetry(
    std::shared_ptr<ErrorContext> context) {
    (void)context;  // decision based solely on policy limits
    if (currentAttempt_ < maxRetries_) {
        ++currentAttempt_;
        return true;
    }
    return false;
}

std::chrono::milliseconds FixedIntervalRetryPolicy::getRetryDelay(
    [[maybe_unused]] int attemptNumber) {
    return interval_;
}

void FixedIntervalRetryPolicy::reset() { currentAttempt_ = 0; }

std::unique_ptr<RetryPolicy> FixedIntervalRetryPolicy::clone() const {
    return std::make_unique<FixedIntervalRetryPolicy>(maxRetries_, interval_);
}

// ExponentialBackoffRetryPolicy implementation
ExponentialBackoffRetryPolicy::ExponentialBackoffRetryPolicy(
    int maxRetries, std::chrono::milliseconds baseDelay, double multiplier,
    std::chrono::milliseconds maxDelay)
    : maxRetries_(maxRetries),
      baseDelay_(baseDelay),
      multiplier_(multiplier),
      maxDelay_(maxDelay),
      currentAttempt_(0) {}

bool ExponentialBackoffRetryPolicy::shouldRetry(
    std::shared_ptr<ErrorContext> context) {
    (void)context;  // decision based solely on policy limits
    if (currentAttempt_ < maxRetries_) {
        ++currentAttempt_;
        return true;
    }
    return false;
}

std::chrono::milliseconds ExponentialBackoffRetryPolicy::getRetryDelay(
    int attemptNumber) {
    auto delay = baseDelay_ *
                 static_cast<long long>(std::pow(multiplier_, attemptNumber));
    return std::min(delay, maxDelay_);
}

void ExponentialBackoffRetryPolicy::reset() { currentAttempt_ = 0; }

std::unique_ptr<RetryPolicy> ExponentialBackoffRetryPolicy::clone() const {
    return std::make_unique<ExponentialBackoffRetryPolicy>(
        maxRetries_, baseDelay_, multiplier_, maxDelay_);
}

// JitteredRetryPolicy implementation
JitteredRetryPolicy::JitteredRetryPolicy(
    std::unique_ptr<RetryPolicy> basePolicy, double jitterFactor)
    : basePolicy_(std::move(basePolicy)),
      jitterFactor_(jitterFactor),
      rng_(std::random_device{}()),
      distribution_(-jitterFactor, jitterFactor) {}

bool JitteredRetryPolicy::shouldRetry(std::shared_ptr<ErrorContext> context) {
    return basePolicy_->shouldRetry(context);
}

std::chrono::milliseconds JitteredRetryPolicy::getRetryDelay(
    int attemptNumber) {
    auto baseDelay = basePolicy_->getRetryDelay(attemptNumber);
    double jitter = distribution_(rng_);
    auto jitteredDelay =
        baseDelay + std::chrono::milliseconds(
                        static_cast<long long>(baseDelay.count() * jitter));
    return std::max(jitteredDelay, std::chrono::milliseconds(0));
}

void JitteredRetryPolicy::reset() { basePolicy_->reset(); }

std::unique_ptr<RetryPolicy> JitteredRetryPolicy::clone() const {
    return std::make_unique<JitteredRetryPolicy>(basePolicy_->clone(),
                                                 jitterFactor_);
}

// CircuitBreaker implementation
CircuitBreaker::CircuitBreaker(int failureThreshold,
                               std::chrono::milliseconds timeout,
                               int successThreshold)
    : state_(CircuitBreakerState::Closed),
      failureCount_(0),
      successCount_(0),
      failureThreshold_(failureThreshold),
      successThreshold_(successThreshold),
      timeout_(timeout),
      totalFailures_(0),
      totalSuccesses_(0) {}

void CircuitBreaker::recordSuccess() {
    std::lock_guard<std::mutex> lock(mutex_);
    totalSuccesses_++;

    if (state_ == CircuitBreakerState::HalfOpen) {
        successCount_++;
        if (successCount_ >= successThreshold_) {
            transitionToClosed();
        }
    } else if (state_ == CircuitBreakerState::Closed) {
        failureCount_ = 0;  // Reset failure count on success
    }
}

void CircuitBreaker::recordFailure() {
    std::lock_guard<std::mutex> lock(mutex_);
    totalFailures_++;
    lastFailureTime_ = std::chrono::steady_clock::now();

    if (state_ == CircuitBreakerState::Closed) {
        failureCount_++;
        if (failureCount_ >= failureThreshold_) {
            transitionToOpen();
        }
    } else if (state_ == CircuitBreakerState::HalfOpen) {
        transitionToOpen();
    }
}

CircuitBreakerState CircuitBreaker::getState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

std::unordered_map<std::string, int> CircuitBreaker::getStatistics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_map<std::string, int> stats;

    stats["state"] = static_cast<int>(state_);
    stats["failure_count"] = failureCount_;
    stats["success_count"] = successCount_;
    stats["total_failures"] = totalFailures_.load();
    stats["total_successes"] = totalSuccesses_.load();
    stats["failure_threshold"] = failureThreshold_;
    stats["success_threshold"] = successThreshold_;

    return stats;
}

void CircuitBreaker::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = CircuitBreakerState::Closed;
    failureCount_ = 0;
    successCount_ = 0;
    totalFailures_ = 0;
    totalSuccesses_ = 0;
}

void CircuitBreaker::transitionToOpen() {
    state_ = CircuitBreakerState::Open;
    lastFailureTime_ = std::chrono::steady_clock::now();
}

void CircuitBreaker::transitionToHalfOpen() {
    state_ = CircuitBreakerState::HalfOpen;
    successCount_ = 0;
}

void CircuitBreaker::transitionToClosed() {
    state_ = CircuitBreakerState::Closed;
    failureCount_ = 0;
    successCount_ = 0;
}

bool CircuitBreaker::shouldAttemptReset() const {
    return std::chrono::steady_clock::now() - lastFailureTime_ >= timeout_;
}

// ErrorRecoveryExecutor implementation

// Bulkhead implementation
Bulkhead::Bulkhead(int maxConcurrentOperations)
#if __cplusplus >= 202002L
    : semaphore_(maxConcurrentOperations)
#else
    : maxConcurrent_(maxConcurrentOperations),
      currentCount_(0)
#endif
      ,
      activeOperations_(0),
      totalOperations_(0),
      rejectedOperations_(0) {
}

std::unordered_map<std::string, int> Bulkhead::getStatistics() const {
    std::unordered_map<std::string, int> stats;
    stats["active_operations"] = activeOperations_.load();
    stats["total_operations"] = totalOperations_.load();
    stats["rejected_operations"] = rejectedOperations_.load();
    return stats;
}

// RecoveryStrategyFactory implementation
std::unique_ptr<RetryPolicy> RecoveryStrategyFactory::createFixedRetry(
    int maxRetries, std::chrono::milliseconds interval) {
    return std::make_unique<FixedIntervalRetryPolicy>(maxRetries, interval);
}

std::unique_ptr<RetryPolicy> RecoveryStrategyFactory::createExponentialBackoff(
    int maxRetries, std::chrono::milliseconds baseDelay) {
    return std::make_unique<ExponentialBackoffRetryPolicy>(maxRetries,
                                                           baseDelay);
}

std::unique_ptr<RetryPolicy> RecoveryStrategyFactory::createJitteredRetry(
    std::unique_ptr<RetryPolicy> basePolicy, double jitterFactor) {
    return std::make_unique<JitteredRetryPolicy>(std::move(basePolicy),
                                                 jitterFactor);
}

std::shared_ptr<CircuitBreaker> RecoveryStrategyFactory::createCircuitBreaker(
    int failureThreshold, std::chrono::milliseconds timeout) {
    return std::make_shared<CircuitBreaker>(failureThreshold, timeout);
}

template <typename T>
std::unique_ptr<FallbackStrategy<T>>
RecoveryStrategyFactory::createDefaultFallback(T defaultValue) {
    return std::make_unique<DefaultValueFallback<T>>(std::move(defaultValue));
}

template <typename T>
std::unique_ptr<FallbackStrategy<T>>
RecoveryStrategyFactory::createFunctionFallback(
    std::function<T(std::shared_ptr<ErrorContext>)> func) {
    return std::make_unique<FunctionFallback<T>>(std::move(func));
}

// Explicit template instantiations for common types
template std::unique_ptr<FallbackStrategy<int>>
RecoveryStrategyFactory::createDefaultFallback<int>(int);
template std::unique_ptr<FallbackStrategy<std::string>>
    RecoveryStrategyFactory::createDefaultFallback<std::string>(std::string);
template std::unique_ptr<FallbackStrategy<bool>>
RecoveryStrategyFactory::createDefaultFallback<bool>(bool);
template std::unique_ptr<FallbackStrategy<double>>
RecoveryStrategyFactory::createDefaultFallback<double>(double);

template std::unique_ptr<FallbackStrategy<int>>
    RecoveryStrategyFactory::createFunctionFallback<int>(
        std::function<int(std::shared_ptr<ErrorContext>)>);
template std::unique_ptr<FallbackStrategy<std::string>>
    RecoveryStrategyFactory::createFunctionFallback<std::string>(
        std::function<std::string(std::shared_ptr<ErrorContext>)>);
template std::unique_ptr<FallbackStrategy<bool>>
    RecoveryStrategyFactory::createFunctionFallback<bool>(
        std::function<bool(std::shared_ptr<ErrorContext>)>);
template std::unique_ptr<FallbackStrategy<double>>
    RecoveryStrategyFactory::createFunctionFallback<double>(
        std::function<double(std::shared_ptr<ErrorContext>)>);

// Explicit template instantiations for ErrorRecoveryExecutor
template class ErrorRecoveryExecutor<int>;
template class ErrorRecoveryExecutor<std::string>;
template class ErrorRecoveryExecutor<bool>;
template class ErrorRecoveryExecutor<double>;
template class ErrorRecoveryExecutor<void>;

}  // namespace atom::error
