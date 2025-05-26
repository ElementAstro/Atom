/*!
 * \file decorate.hpp
 * \brief An enhanced implementation of decorate function, inspired by Python's
 * decorator pattern.
 * \author Max Qian <lightapt.com> (Original)
 * \date 2025-03-12 (Updated)
 * \copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef ATOM_META_DECORATE_HPP
#define ATOM_META_DECORATE_HPP

#include <chrono>
#include <cmath>
#include <concepts>
#include <exception>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <source_location>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "func_traits.hpp"

namespace atom::meta {

class DecoratorError;

/**
 * \brief Concept to check if a function is callable with specific arguments and
 * return type
 * \tparam F Function type
 * \tparam R Expected return type
 * \tparam Args Argument types
 */
template <typename F, typename R, typename... Args>
concept CallableWithResult =
    std::invocable<F, Args...> && requires(F&& func, Args&&... args) {
        {
            std::invoke(std::forward<F>(func), std::forward<Args>(args)...)
        } -> std::convertible_to<R>;
    };

/**
 * \brief Concept to check if a function is nothrow callable
 * \tparam F Function type
 * \tparam Args Argument types
 */
template <typename F, typename... Args>
concept NoThrowCallable =
    std::invocable<F, Args...> && requires(F&& func, Args&&... args) {
        {
            noexcept(
                std::invoke(std::forward<F>(func), std::forward<Args>(args)...))
        };
    };

/**
 * \brief Exception class for decorator-related errors
 */
class DecoratorError : public std::exception {
    std::string message_;
    std::source_location location_;

public:
    explicit DecoratorError(
        std::string_view msg,
        const std::source_location& loc = std::source_location::current())
        : message_(std::format("Decorator error at {}:{}: {}", loc.file_name(),
                               loc.line(), msg)),
          location_(loc) {}

    [[nodiscard]] auto what() const noexcept -> const char* override {
        return message_.c_str();
    }

    [[nodiscard]] const std::source_location& location() const noexcept {
        return location_;
    }
};

/**
 * \brief Switchable decorator that allows changing the underlying function
 * \tparam Func Function type
 */
template <typename Func>
class Switchable {
public:
    using Traits = FunctionTraits<Func>;
    using R = typename Traits::return_type;
    using TupleArgs = typename Traits::argument_types;

    explicit Switchable(Func func) noexcept(
        std::is_nothrow_move_constructible_v<Func>)
        : func_(std::move(func)) {}

    template <typename F>
        requires std::invocable<F>
    void switchTo(F&& new_f) {
        func_ = std::forward<F>(new_f);
    }

    template <std::size_t... Is>
    [[nodiscard]] auto callImpl(const TupleArgs& args,
                                std::index_sequence<Is...> /*unused*/) const
        -> R {
        return std::invoke(func_, std::get<Is>(args)...);
    }

    template <typename... ArgsT>
    [[nodiscard]] auto operator()(ArgsT&&... args) const -> R {
        static_assert(
            std::is_same_v<std::tuple<std::decay_t<ArgsT>...>, TupleArgs>,
            "Argument types do not match the function signature");
        auto tupleArgs = std::forward_as_tuple(std::forward<ArgsT>(args)...);
        return callImpl(tupleArgs,
                        std::make_index_sequence<sizeof...(ArgsT)>{});
    }

private:
    std::function<Func> func_;
};

/**
 * \brief Base decorator class template
 * \tparam FuncType Function type to decorate
 */
template <typename FuncType>
class decorator {
protected:
    FuncType func_;

public:
    explicit decorator(FuncType func) noexcept(
        std::is_nothrow_move_constructible_v<FuncType>)
        : func_(std::move(func)) {}

    template <typename... Args>
    [[nodiscard]] auto operator()(Args&&... args) const
        noexcept(noexcept(std::invoke(func_, std::forward<Args>(args)...)))
            -> decltype(std::invoke(func_, std::forward<Args>(args)...)) {
        return std::invoke(func_, std::forward<Args>(args)...);
    }
};

/**
 * \brief Abstract base decorator class for inheritance-based decorators
 * \tparam R Return type
 * \tparam Args Argument types
 */
template <typename R, typename... Args>
class BaseDecorator {
public:
    using FuncType = std::function<R(Args...)>;
    virtual auto operator()(FuncType func, Args... args) -> R = 0;
    virtual ~BaseDecorator() = default;

    BaseDecorator() = default;
    BaseDecorator(const BaseDecorator&) = default;
    BaseDecorator& operator=(const BaseDecorator&) = default;
    BaseDecorator(BaseDecorator&&) noexcept = default;
    BaseDecorator& operator=(BaseDecorator&&) noexcept = default;
};

/**
 * \brief Decorator that executes a function multiple times in a loop
 * \tparam FuncType Function type to decorate
 */
template <typename FuncType>
class LoopDecorator : public decorator<FuncType> {
    using Base = decorator<FuncType>;

public:
    using Base::Base;

    template <typename ProgressCallback = std::nullptr_t, typename... Args>
    auto operator()(int loopCount, ProgressCallback&& callback = nullptr,
                    Args&&... args) {
        using ReturnType = std::invoke_result_t<FuncType, Args...>;

        if constexpr (std::is_void_v<ReturnType>) {
            for (int i = 0; i < loopCount; ++i) {
                if constexpr (!std::is_null_pointer_v<
                                  std::decay_t<ProgressCallback>>) {
                    if constexpr (std::invocable<ProgressCallback, int, int>) {
                        std::invoke(std::forward<ProgressCallback>(callback), i,
                                    loopCount);
                    }
                }
                std::invoke(this->func_, std::forward<Args>(args)...);
            }
        } else {
            std::optional<ReturnType> result;
            for (int i = 0; i < loopCount; ++i) {
                if constexpr (!std::is_null_pointer_v<
                                  std::decay_t<ProgressCallback>>) {
                    if constexpr (std::invocable<ProgressCallback, int, int>) {
                        std::invoke(std::forward<ProgressCallback>(callback), i,
                                    loopCount);
                    }
                }
                result = std::invoke(this->func_, std::forward<Args>(args)...);
            }
            return *result;
        }
    }
};

/**
 * \brief Decorator that retries function execution on failure with configurable
 * backoff
 * \tparam R Return type
 * \tparam Args Argument types
 */
template <typename R, typename... Args>
class RetryDecorator : public BaseDecorator<R, Args...> {
    using FuncType = std::function<R(Args...)>;
    int maxRetries_;
    std::chrono::milliseconds initialBackoff_;
    double backoffMultiplier_;
    bool useExponentialBackoff_;

public:
    RetryDecorator(int maxRetries,
                   std::chrono::milliseconds initialBackoff =
                       std::chrono::milliseconds(100),
                   double backoffMultiplier = 2.0,
                   bool useExponentialBackoff = true)
        : maxRetries_(maxRetries),
          initialBackoff_(initialBackoff),
          backoffMultiplier_(backoffMultiplier),
          useExponentialBackoff_(useExponentialBackoff) {}

    auto operator()(FuncType func, Args... args) -> R override {
        std::exception_ptr lastException;

        for (int attempt = 0; attempt < maxRetries_ + 1; ++attempt) {
            try {
                return func(std::forward<Args>(args)...);
            } catch (...) {
                lastException = std::current_exception();

                if (attempt < maxRetries_) {
                    auto delay = initialBackoff_;
                    if (useExponentialBackoff_ && attempt > 0) {
                        delay = std::chrono::milliseconds(static_cast<long>(
                            initialBackoff_.count() *
                            std::pow(backoffMultiplier_, attempt)));
                    }
                    std::this_thread::sleep_for(delay);
                }
            }
        }

        std::rethrow_exception(lastException);
    }
};

/**
 * \brief Standalone retry decorator for direct function wrapping
 * \tparam Func Function type
 */
template <typename Func>
class FunctionRetryDecorator {
    using Traits = FunctionTraits<Func>;
    using R = typename Traits::return_type;

    Func func_;
    int maxRetries_;
    std::chrono::milliseconds initialBackoff_;
    double backoffMultiplier_;
    bool useExponentialBackoff_;

public:
    FunctionRetryDecorator(Func func, int maxRetries,
                           std::chrono::milliseconds initialBackoff =
                               std::chrono::milliseconds(100),
                           double backoffMultiplier = 2.0,
                           bool useExponentialBackoff = true)
        : func_(std::move(func)),
          maxRetries_(maxRetries),
          initialBackoff_(initialBackoff),
          backoffMultiplier_(backoffMultiplier),
          useExponentialBackoff_(useExponentialBackoff) {}

    template <typename... Args>
    auto operator()(Args&&... args) {
        std::exception_ptr lastException;

        for (int attempt = 0; attempt < maxRetries_ + 1; ++attempt) {
            try {
                if constexpr (std::is_void_v<R>) {
                    func_(std::forward<Args>(args)...);
                    return;
                } else {
                    return func_(std::forward<Args>(args)...);
                }
            } catch (...) {
                lastException = std::current_exception();

                if (attempt < maxRetries_) {
                    auto delay = initialBackoff_;
                    if (useExponentialBackoff_ && attempt > 0) {
                        delay = std::chrono::milliseconds(static_cast<long>(
                            initialBackoff_.count() *
                            std::pow(backoffMultiplier_, attempt)));
                    }
                    std::this_thread::sleep_for(delay);
                }
            }
        }
        std::rethrow_exception(lastException);
    }
};

/**
 * \brief Standalone timing decorator for direct function wrapping
 * \tparam Func Function type
 */
template <typename Func>
class FunctionTimingDecorator {
    using Traits = FunctionTraits<Func>;
    using R = typename Traits::return_type;

    std::string name_;
    std::function<void(std::string_view, std::chrono::microseconds)> callback_;
    Func func_;

public:
    FunctionTimingDecorator(
        Func f, std::string name,
        std::function<void(std::string_view, std::chrono::microseconds)> cb)
        : func_(std::move(f)),
          name_(std::move(name)),
          callback_(std::move(cb)) {}

    template <typename... Args>
    auto operator()(Args&&... args) {
        auto start = std::chrono::high_resolution_clock::now();

        if constexpr (std::is_void_v<R>) {
            func_(std::forward<Args>(args)...);
            auto end = std::chrono::high_resolution_clock::now();
            callback_(name_,
                      std::chrono::duration_cast<std::chrono::microseconds>(
                          end - start));
        } else {
            auto result = func_(std::forward<Args>(args)...);
            auto end = std::chrono::high_resolution_clock::now();
            callback_(name_,
                      std::chrono::duration_cast<std::chrono::microseconds>(
                          end - start));
            return result;
        }
    }
};

/**
 * \brief BaseDecorator-compatible timing decorator
 * \tparam R Return type
 * \tparam Args Argument types
 */
template <typename R, typename... Args>
class TimingDecorator : public BaseDecorator<R, Args...> {
    using FuncType = std::function<R(Args...)>;
    using TimingCallback =
        std::function<void(std::string_view, std::chrono::microseconds)>;

    TimingCallback callback_;
    std::string name_;

public:
    TimingDecorator(std::string name, TimingCallback callback)
        : name_(std::move(name)), callback_(std::move(callback)) {}

    auto operator()(FuncType func, Args... args) -> R override {
        auto start = std::chrono::high_resolution_clock::now();

        if constexpr (std::is_void_v<R>) {
            func(std::forward<Args>(args)...);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);
            callback_(name_, duration);
        } else {
            auto result = func(std::forward<Args>(args)...);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                      start);
            callback_(name_, duration);
            return result;
        }
    }
};

/**
 * \brief Condition checker decorator with improved interface
 * \tparam FuncType Function type
 */
template <typename FuncType>
class ConditionCheckDecorator : public decorator<FuncType> {
    using Base = decorator<FuncType>;

public:
    using Base::Base;

    template <typename ConditionFunc, typename Fallback, typename... TArgs>
        requires std::invocable<ConditionFunc> &&
                 std::convertible_to<std::invoke_result_t<ConditionFunc>, bool>
    auto operator()(ConditionFunc&& condition, Fallback&& fallback,
                    TArgs&&... args) const {
        using ReturnType = std::invoke_result_t<FuncType, TArgs...>;

        if (std::invoke(std::forward<ConditionFunc>(condition))) {
            return Base::operator()(std::forward<TArgs>(args)...);
        }

        if constexpr (std::is_invocable_v<Fallback, TArgs...>) {
            return std::invoke(std::forward<Fallback>(fallback),
                               std::forward<TArgs>(args)...);
        } else if constexpr (!std::is_void_v<ReturnType>) {
            return static_cast<ReturnType>(std::forward<Fallback>(fallback));
        }
    }

    template <typename ConditionFunc, typename... TArgs>
        requires std::invocable<ConditionFunc> &&
                 std::convertible_to<std::invoke_result_t<ConditionFunc>, bool>
    auto operator()(ConditionFunc&& condition, TArgs&&... args) const {
        using ReturnType = std::invoke_result_t<FuncType, TArgs...>;

        if (std::invoke(std::forward<ConditionFunc>(condition))) {
            return Base::operator()(std::forward<TArgs>(args)...);
        }

        if constexpr (!std::is_void_v<ReturnType>) {
            return ReturnType{};
        }
    }
};

/**
 * \brief Cache decorator with TTL and size limit
 * \tparam R Return type
 * \tparam Args Argument types
 */
template <typename R, typename... Args>
class CacheDecorator : public BaseDecorator<R, Args...> {
    struct CacheEntry {
        R value;
        std::chrono::steady_clock::time_point expiry;
    };

    using FuncType = std::function<R(Args...)>;
    using CacheKey = std::tuple<std::decay_t<Args>...>;

    struct KeyHasher {
        auto operator()(const CacheKey& key) const noexcept -> std::size_t {
            return std::apply(
                [](const auto&... args) {
                    std::size_t seed = 0;
                    (hash_combine(seed, args), ...);
                    return seed;
                },
                key);
        }

    private:
        template <typename T>
        static void hash_combine(std::size_t& seed, const T& val) {
            seed ^=
                std::hash<T>{}(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
    };

    mutable std::unordered_map<CacheKey, CacheEntry, KeyHasher> cache_;
    mutable std::mutex cacheMutex_;
    std::chrono::milliseconds ttl_;
    std::size_t maxSize_;

    void cleanup() const {
        auto now = std::chrono::steady_clock::now();
        for (auto it = cache_.begin(); it != cache_.end();) {
            if (it->second.expiry < now) {
                it = cache_.erase(it);
            } else {
                ++it;
            }
        }

        if (cache_.size() > maxSize_) {
            std::size_t numToRemove = cache_.size() - maxSize_;
            auto it = cache_.begin();
            for (std::size_t i = 0; i < numToRemove && it != cache_.end();
                 ++i) {
                it = cache_.erase(it);
            }
        }
    }

public:
    CacheDecorator(std::chrono::milliseconds ttl = std::chrono::hours(1),
                   std::size_t maxSize = 1000)
        : ttl_(ttl), maxSize_(maxSize) {}

    auto operator()(FuncType func, Args... args) -> R override {
        std::lock_guard<std::mutex> lock(cacheMutex_);

        auto argsTuple = std::make_tuple(args...);
        auto now = std::chrono::steady_clock::now();

        if (cache_.size() >= maxSize_) {
            cleanup();
        }

        auto it = cache_.find(argsTuple);
        if (it != cache_.end() && it->second.expiry > now) {
            return it->second.value;
        }

        auto result = func(std::forward<Args>(args)...);
        cache_[argsTuple] = {result, now + ttl_};
        return result;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        cache_.clear();
    }

    void setTTL(std::chrono::milliseconds ttl) {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        ttl_ = ttl;
    }

    void setMaxSize(std::size_t maxSize) {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        maxSize_ = maxSize;
        if (cache_.size() > maxSize_) {
            cleanup();
        }
    }
};

/**
 * \brief Throttling decorator that limits execution frequency
 * \tparam R Return type
 * \tparam Args Argument types
 */
template <typename R, typename... Args>
class ThrottlingDecorator : public BaseDecorator<R, Args...> {
    using FuncType = std::function<R(Args...)>;

    std::chrono::milliseconds minInterval_;
    mutable std::mutex mutex_;
    mutable std::chrono::steady_clock::time_point lastCall_ =
        std::chrono::steady_clock::now() - minInterval_;

public:
    explicit ThrottlingDecorator(std::chrono::milliseconds minInterval)
        : minInterval_(minInterval) {}

    auto operator()(FuncType func, Args... args) -> R override {
        std::unique_lock<std::mutex> lock(mutex_);

        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - lastCall_;

        if (elapsed < minInterval_) {
            auto waitTime = minInterval_ - elapsed;
            lock.unlock();
            std::this_thread::sleep_for(waitTime);
            lock.lock();
        }

        lastCall_ = std::chrono::steady_clock::now();
        lock.unlock();

        return func(std::forward<Args>(args)...);
    }
};

/**
 * \brief Parameter validation decorator
 * \tparam R Return type
 * \tparam Args Argument types
 */
template <typename R, typename... Args>
class ValidationDecorator : public BaseDecorator<R, Args...> {
    using FuncType = std::function<R(Args...)>;
    using ValidatorFunc = std::function<bool(Args...)>;
    using ErrorMsgFunc = std::function<std::string(Args...)>;

    ValidatorFunc validator_;
    ErrorMsgFunc errorMsgGenerator_;

public:
    ValidationDecorator(ValidatorFunc validator, ErrorMsgFunc errorMsgGenerator)
        : validator_(std::move(validator)),
          errorMsgGenerator_(std::move(errorMsgGenerator)) {}

    auto operator()(FuncType func, Args... args) -> R override {
        if (!validator_(args...)) {
            throw DecoratorError(errorMsgGenerator_(args...));
        }
        return func(std::forward<Args>(args)...);
    }
};

/**
 * \brief Decorator composition system with enhanced interface
 * \tparam R Return type
 * \tparam Args Argument types
 */
template <typename R, typename... Args>
class DecorateStepper {
    using FuncType = std::function<R(Args...)>;
    using DecoratorPtr = std::shared_ptr<BaseDecorator<R, Args...>>;

    std::vector<DecoratorPtr> decorators_;
    FuncType baseFunction_;

public:
    explicit DecorateStepper(FuncType func) : baseFunction_(std::move(func)) {}

    template <typename Decorator, typename... DArgs>
    auto addDecorator(DArgs&&... args) -> DecorateStepper& {
        decorators_.push_back(
            std::make_shared<Decorator>(std::forward<DArgs>(args)...));
        return *this;
    }

    auto addDecorator(DecoratorPtr decorator) -> DecorateStepper& {
        decorators_.push_back(std::move(decorator));
        return *this;
    }

    [[nodiscard]] auto execute(Args... args) -> R {
        try {
            FuncType currentFunction = baseFunction_;

            for (auto it = decorators_.rbegin(); it != decorators_.rend();
                 ++it) {
                auto& decorator = *it;
                auto nextFunction = currentFunction;
                currentFunction = [decorator,
                                   nextFunction](Args... innerArgs) -> R {
                    return (*decorator)(nextFunction,
                                        std::forward<Args>(innerArgs)...);
                };
            }

            return currentFunction(std::forward<Args>(args)...);
        } catch (const DecoratorError& e) {
            throw;
        } catch (const std::exception& e) {
            throw DecoratorError(
                std::format("Exception in decorated function: {}", e.what()));
        }
    }

    [[nodiscard]] auto operator()(Args... args) -> R {
        return execute(std::forward<Args>(args)...);
    }
};

/**
 * \brief Helper function to create DecorateStepper from function
 * \tparam Func Function type
 * \param func Function to wrap
 * \return DecorateStepper instance
 */
template <typename Func>
auto makeDecorateStepper(Func&& func) {
    using Traits = FunctionTraits<std::decay_t<Func>>;
    using ReturnType = typename Traits::return_type;
    using ArgumentTuple = typename Traits::argument_types;

    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return DecorateStepper<ReturnType,
                               std::tuple_element_t<Is, ArgumentTuple>...>(
            std::forward<Func>(func));
    }(std::make_index_sequence<std::tuple_size_v<ArgumentTuple>>{});
}

/**
 * \brief Helper function to create LoopDecorator
 * \tparam Func Function type
 * \param func Function to wrap
 * \return LoopDecorator instance
 */
template <typename Func>
auto makeLoopDecorator(Func&& func) {
    return LoopDecorator<std::decay_t<Func>>(std::forward<Func>(func));
}

/**
 * \brief Helper function to create RetryDecorator
 * \tparam Func Function type
 * \param func Function to wrap
 * \param retryCount Number of retries
 * \return RetryDecorator instance
 */
template <typename Func>
auto makeRetryDecorator(Func&& func, int retryCount) {
    using FuncType = std::decay_t<Func>;
    using Traits = FunctionTraits<FuncType>;
    using ReturnType = typename Traits::return_type;

    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        using ArgTuple = typename Traits::argument_types;
        return RetryDecorator<ReturnType,
                              std::tuple_element_t<Is, ArgTuple>...>(
            retryCount);
    }(std::make_index_sequence<Traits::arity>{});
}

/**
 * \brief Helper function to create ConditionCheckDecorator
 * \tparam Func Function type
 * \param func Function to wrap
 * \return ConditionCheckDecorator instance
 */
template <typename Func>
auto makeConditionCheckDecorator(Func&& func) {
    return ConditionCheckDecorator<std::decay_t<Func>>(
        std::forward<Func>(func));
}

/**
 * \brief Helper function to create TimingDecorator
 * \tparam Func Function type
 * \param func Function to wrap
 * \param name Timer name
 * \param callback Timing callback function
 * \return TimingDecorator instance
 */
template <typename Func>
auto makeTimingDecorator(
    Func&& func, std::string name,
    std::function<void(std::string_view, std::chrono::microseconds)> callback) {
    using FuncType = std::decay_t<Func>;
    using Traits = FunctionTraits<FuncType>;
    using ReturnType = typename Traits::return_type;

    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        using ArgTuple = typename Traits::argument_types;
        return TimingDecorator<ReturnType,
                               std::tuple_element_t<Is, ArgTuple>...>(
            std::move(name), std::move(callback));
    }(std::make_index_sequence<Traits::arity>{});
}

}  // namespace atom::meta

#endif  // ATOM_META_DECORATE_HPP
