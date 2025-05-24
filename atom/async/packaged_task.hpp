#ifndef ATOM_ASYNC_PACKAGED_TASK_HPP
#define ATOM_ASYNC_PACKAGED_TASK_HPP

#include <atomic>
#include <concepts>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

#include "atom/async/future.hpp"

#ifdef __cpp_lib_hardware_interference_size
using std::hardware_constructive_interference_size;
using std::hardware_destructive_interference_size;
#else
constexpr std::size_t hardware_constructive_interference_size = 64;
constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

#ifdef ATOM_USE_LOCKFREE_QUEUE
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#endif

#ifdef ATOM_USE_ASIO
#include <asio.hpp>
#endif

namespace atom::async {

class InvalidPackagedTaskException : public atom::error::RuntimeError {
public:
    using atom::error::RuntimeError::RuntimeError;
};

#define THROW_INVALID_PACKAGED_TASK_EXCEPTION(...)                     \
    throw InvalidPackagedTaskException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__);

#define THROW_NESTED_INVALID_PACKAGED_TASK_EXCEPTION(...) \
    InvalidPackagedTaskException::rethrowNested(          \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME,   \
        "Invalid packaged task: " __VA_ARGS__);

template <typename F, typename R, typename... Args>
concept InvocableWithResult =
    std::invocable<F, Args...> &&
    (std::same_as<std::invoke_result_t<F, Args...>, R> ||
     std::same_as<R, void>);

template <typename ResultType, typename... Args>
class alignas(hardware_constructive_interference_size) EnhancedPackagedTask {
public:
    using TaskType = std::function<ResultType(Args...)>;

    explicit EnhancedPackagedTask(TaskType task)
        : cancelled_(false), task_(std::move(task)) {
        if (!task_) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Provided task is invalid");
        }
        promise_ = std::make_unique<std::promise<ResultType>>();
        future_ = promise_->get_future().share();

#ifdef ATOM_USE_ASIO
        asioContext_ = nullptr;
#endif
    }

#ifdef ATOM_USE_ASIO
    EnhancedPackagedTask(TaskType task, asio::io_context* context)
        : cancelled_(false), task_(std::move(task)), asioContext_(context) {
        if (!task_) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Provided task is invalid");
        }
        promise_ = std::make_unique<std::promise<ResultType>>();
        future_ = promise_->get_future().share();
    }
#endif

    EnhancedPackagedTask(const EnhancedPackagedTask&) = delete;
    EnhancedPackagedTask& operator=(const EnhancedPackagedTask&) = delete;

    EnhancedPackagedTask(EnhancedPackagedTask&& other) noexcept
        : task_(std::move(other.task_)),
          promise_(std::move(other.promise_)),
          future_(std::move(other.future_)),
          callbacks_(std::move(other.callbacks_)),
          cancelled_(other.cancelled_.load(std::memory_order_acquire))
#ifdef ATOM_USE_LOCKFREE_QUEUE
          ,
          m_lockfreeCallbacks(std::move(other.m_lockfreeCallbacks))
#endif
#ifdef ATOM_USE_ASIO
          ,
          asioContext_(other.asioContext_)
#endif
    {
    }

    EnhancedPackagedTask& operator=(EnhancedPackagedTask&& other) noexcept {
        if (this != &other) {
            task_ = std::move(other.task_);
            promise_ = std::move(other.promise_);
            future_ = std::move(other.future_);
            callbacks_ = std::move(other.callbacks_);
            cancelled_.store(other.cancelled_.load(std::memory_order_acquire),
                             std::memory_order_release);
#ifdef ATOM_USE_LOCKFREE_QUEUE
            m_lockfreeCallbacks = std::move(other.m_lockfreeCallbacks);
#endif
#ifdef ATOM_USE_ASIO
            asioContext_ = other.asioContext_;
#endif
        }
        return *this;
    }

    [[nodiscard]] EnhancedFuture<ResultType> getEnhancedFuture() const {
        if (!future_.valid()) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Future is no longer valid");
        }
        return EnhancedFuture<ResultType>(future_);
    }

    void operator()(Args... args) {
        if (isCancelled()) {
            promise_->set_exception(
                std::make_exception_ptr(InvalidPackagedTaskException(
                    ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME,
                    "Task has been cancelled")));
            return;
        }

        if (!task_) {
            promise_->set_exception(
                std::make_exception_ptr(InvalidPackagedTaskException(
                    ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME,
                    "Task function is invalid")));
            return;
        }

#ifdef ATOM_USE_ASIO
        if (asioContext_) {
            asio::post(*asioContext_, [this,
                                       ... capturedArgs =
                                           std::forward<Args>(args)]() mutable {
                try {
                    if constexpr (!std::is_void_v<ResultType>) {
                        ResultType result = std::invoke(
                            task_, std::forward<Args>(capturedArgs)...);
                        promise_->set_value(std::move(result));
                        runCallbacks(result);
                    } else {
                        std::invoke(task_, std::forward<Args>(capturedArgs)...);
                        promise_->set_value();
                        runCallbacks();
                    }
                } catch (...) {
                    try {
                        promise_->set_exception(std::current_exception());
                    } catch (const std::future_error&) {
                        // Promise might be already satisfied
                    }
                }
            });
            return;
        }
#endif

        try {
            if constexpr (!std::is_void_v<ResultType>) {
                ResultType result =
                    std::invoke(task_, std::forward<Args>(args)...);
                promise_->set_value(std::move(result));
                runCallbacks(result);
            } else {
                std::invoke(task_, std::forward<Args>(args)...);
                promise_->set_value();
                runCallbacks();
            }
        } catch (...) {
            try {
                promise_->set_exception(std::current_exception());
            } catch (const std::future_error&) {
                // Promise might have been fulfilled already
            }
        }
    }

#ifdef ATOM_USE_LOCKFREE_QUEUE
    template <typename F>
        requires std::invocable<F, ResultType>
    void onComplete(F&& func) {
        if (!func) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION(
                "Provided callback is invalid");
        }

        if (!m_lockfreeCallbacks) {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            if (!m_lockfreeCallbacks) {
                m_lockfreeCallbacks = std::make_unique<LockfreeCallbackQueue>(
                    CALLBACK_QUEUE_SIZE);
            }
        }

        auto wrappedCallback =
            std::make_shared<CallbackWrapperImpl<F>>(std::forward<F>(func));

        constexpr int MAX_RETRIES = 3;
        bool pushed = false;

        for (int i = 0; i < MAX_RETRIES && !pushed; ++i) {
            pushed = m_lockfreeCallbacks->push(wrappedCallback);
            if (!pushed) {
                std::this_thread::sleep_for(std::chrono::microseconds(1 << i));
            }
        }

        if (!pushed) {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacks_.emplace_back(
                [wrappedCallback](const ResultType& result) {
                    (*wrappedCallback)(result);
                });
        }
    }
#else
    template <typename F>
        requires std::invocable<F, ResultType>
    void onComplete(F&& func) {
        if (!func) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION(
                "Provided callback is invalid");
        }
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        callbacks_.emplace_back(std::forward<F>(func));
    }
#endif

    [[nodiscard]] bool cancel() noexcept {
        bool expected = false;
        return cancelled_.compare_exchange_strong(expected, true,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire);
    }

    [[nodiscard]] bool isCancelled() const noexcept {
        return cancelled_.load(std::memory_order_acquire);
    }

#ifdef ATOM_USE_ASIO
    void setAsioContext(asio::io_context* context) { asioContext_ = context; }

    [[nodiscard]] asio::io_context* getAsioContext() const {
        return asioContext_;
    }
#endif

    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(task_) && !isCancelled() && future_.valid();
    }

protected:
    alignas(hardware_destructive_interference_size) TaskType task_;
    std::unique_ptr<std::promise<ResultType>> promise_;
    std::shared_future<ResultType> future_;
    std::vector<std::function<void(const ResultType&)>> callbacks_;
    std::atomic<bool> cancelled_;
    mutable std::mutex callbacksMutex_;

#ifdef ATOM_USE_ASIO
    asio::io_context* asioContext_;
#endif

#ifdef ATOM_USE_LOCKFREE_QUEUE
    struct CallbackWrapperBase {
        virtual ~CallbackWrapperBase() = default;
        virtual void operator()(const ResultType& result) = 0;
    };

    template <typename F>
    struct CallbackWrapperImpl : CallbackWrapperBase {
        std::function<void(const ResultType&)> callback;

        explicit CallbackWrapperImpl(F&& func)
            : callback(std::forward<F>(func)) {}

        void operator()(const ResultType& result) override { callback(result); }
    };

    static constexpr size_t CALLBACK_QUEUE_SIZE = 128;
    using LockfreeCallbackQueue =
        boost::lockfree::queue<std::shared_ptr<CallbackWrapperBase>>;

    std::unique_ptr<LockfreeCallbackQueue> m_lockfreeCallbacks;
#endif

private:
#ifdef ATOM_USE_LOCKFREE_QUEUE
    void runCallbacks(const ResultType& result) {
        if (m_lockfreeCallbacks) {
            std::shared_ptr<CallbackWrapperBase> callback_ptr;
            while (m_lockfreeCallbacks->pop(callback_ptr)) {
                try {
                    (*callback_ptr)(result);
                } catch (...) {
                    // Log exception
                }
            }
        }

        std::vector<std::function<void(const ResultType&)>> callbacksCopy;
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacksCopy = std::move(callbacks_);
        }

        for (auto& callback : callbacksCopy) {
            try {
                callback(result);
            } catch (...) {
                // Log exception
            }
        }
    }
#else
    void runCallbacks(const ResultType& result) {
        std::vector<std::function<void(const ResultType&)>> callbacksCopy;
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacksCopy = std::move(callbacks_);
        }

        for (auto& callback : callbacksCopy) {
            try {
                callback(result);
            } catch (...) {
                // Log exception
            }
        }
    }
#endif
};

template <typename... Args>
class alignas(hardware_constructive_interference_size)
    EnhancedPackagedTask<void, Args...> {
public:
    using TaskType = std::function<void(Args...)>;

    explicit EnhancedPackagedTask(TaskType task)
        : cancelled_(false), task_(std::move(task)) {
        if (!task_) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Provided task is invalid");
        }
        promise_ = std::make_unique<std::promise<void>>();
        future_ = promise_->get_future().share();

#ifdef ATOM_USE_ASIO
        asioContext_ = nullptr;
#endif
    }

#ifdef ATOM_USE_ASIO
    EnhancedPackagedTask(TaskType task, asio::io_context* context)
        : cancelled_(false), task_(std::move(task)), asioContext_(context) {
        if (!task_) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Provided task is invalid");
        }
        promise_ = std::make_unique<std::promise<void>>();
        future_ = promise_->get_future().share();
    }
#endif

    EnhancedPackagedTask(const EnhancedPackagedTask&) = delete;
    EnhancedPackagedTask& operator=(const EnhancedPackagedTask&) = delete;

    EnhancedPackagedTask(EnhancedPackagedTask&& other) noexcept
        : task_(std::move(other.task_)),
          promise_(std::move(other.promise_)),
          future_(std::move(other.future_)),
          callbacks_(std::move(other.callbacks_)),
          cancelled_(other.cancelled_.load(std::memory_order_acquire))
#ifdef ATOM_USE_LOCKFREE_QUEUE
          ,
          m_lockfreeCallbacks(std::move(other.m_lockfreeCallbacks))
#endif
#ifdef ATOM_USE_ASIO
          ,
          asioContext_(other.asioContext_)
#endif
    {
    }

    EnhancedPackagedTask& operator=(EnhancedPackagedTask&& other) noexcept {
        if (this != &other) {
            task_ = std::move(other.task_);
            promise_ = std::move(other.promise_);
            future_ = std::move(other.future_);
            callbacks_ = std::move(other.callbacks_);
            cancelled_.store(other.cancelled_.load(std::memory_order_acquire),
                             std::memory_order_release);
#ifdef ATOM_USE_LOCKFREE_QUEUE
            m_lockfreeCallbacks = std::move(other.m_lockfreeCallbacks);
#endif
#ifdef ATOM_USE_ASIO
            asioContext_ = other.asioContext_;
#endif
        }
        return *this;
    }

    [[nodiscard]] EnhancedFuture<void> getEnhancedFuture() const {
        if (!future_.valid()) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Future is no longer valid");
        }
        return EnhancedFuture<void>(future_);
    }

    void operator()(Args... args) {
        if (isCancelled()) {
            promise_->set_exception(
                std::make_exception_ptr(InvalidPackagedTaskException(
                    ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME,
                    "Task has been cancelled")));
            return;
        }

        if (!task_) {
            promise_->set_exception(
                std::make_exception_ptr(InvalidPackagedTaskException(
                    ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME,
                    "Task function is invalid")));
            return;
        }

#ifdef ATOM_USE_ASIO
        if (asioContext_) {
            asio::post(
                *asioContext_,
                [this, ... capturedArgs = std::forward<Args>(args)]() mutable {
                    try {
                        std::invoke(task_, std::forward<Args>(capturedArgs)...);
                        promise_->set_value();
                        runCallbacks();
                    } catch (...) {
                        try {
                            promise_->set_exception(std::current_exception());
                        } catch (const std::future_error&) {
                            // Promise might be already satisfied
                        }
                    }
                });
            return;
        }
#endif

        try {
            std::invoke(task_, std::forward<Args>(args)...);
            promise_->set_value();
            runCallbacks();
        } catch (...) {
            try {
                promise_->set_exception(std::current_exception());
            } catch (const std::future_error&) {
                // Promise might have been fulfilled already
            }
        }
    }

#ifdef ATOM_USE_LOCKFREE_QUEUE
    template <typename F>
        requires std::invocable<F>
    void onComplete(F&& func) {
        if (!func) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION(
                "Provided callback is invalid");
        }

        if (!m_lockfreeCallbacks) {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            if (!m_lockfreeCallbacks) {
                m_lockfreeCallbacks = std::make_unique<LockfreeCallbackQueue>(
                    CALLBACK_QUEUE_SIZE);
            }
        }

        auto wrappedCallback =
            std::make_shared<CallbackWrapperImpl<F>>(std::forward<F>(func));
        bool pushed = false;

        for (int i = 0; i < 3 && !pushed; ++i) {
            pushed = m_lockfreeCallbacks->push(wrappedCallback);
            if (!pushed) {
                std::this_thread::sleep_for(std::chrono::microseconds(1 << i));
            }
        }

        if (!pushed) {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacks_.emplace_back(
                [wrappedCallback]() { (*wrappedCallback)(); });
        }
    }
#else
    template <typename F>
        requires std::invocable<F>
    void onComplete(F&& func) {
        if (!func) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION(
                "Provided callback is invalid");
        }
        std::lock_guard<std::mutex> lock(callbacksMutex_);
        callbacks_.emplace_back(std::forward<F>(func));
    }
#endif

    [[nodiscard]] bool cancel() noexcept {
        bool expected = false;
        return cancelled_.compare_exchange_strong(expected, true,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire);
    }

    [[nodiscard]] bool isCancelled() const noexcept {
        return cancelled_.load(std::memory_order_acquire);
    }

#ifdef ATOM_USE_ASIO
    void setAsioContext(asio::io_context* context) { asioContext_ = context; }

    [[nodiscard]] asio::io_context* getAsioContext() const {
        return asioContext_;
    }
#endif

    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(task_) && !isCancelled() && future_.valid();
    }

protected:
    TaskType task_;
    std::unique_ptr<std::promise<void>> promise_;
    std::shared_future<void> future_;
    std::vector<std::function<void()>> callbacks_;
    std::atomic<bool> cancelled_;
    mutable std::mutex callbacksMutex_;

#ifdef ATOM_USE_ASIO
    asio::io_context* asioContext_;
#endif

#ifdef ATOM_USE_LOCKFREE_QUEUE
    struct CallbackWrapperBase {
        virtual ~CallbackWrapperBase() = default;
        virtual void operator()() = 0;
    };

    template <typename F>
    struct CallbackWrapperImpl : CallbackWrapperBase {
        std::function<void()> callback;

        explicit CallbackWrapperImpl(F&& func)
            : callback(std::forward<F>(func)) {}

        void operator()() override { callback(); }
    };

    static constexpr size_t CALLBACK_QUEUE_SIZE = 128;
    using LockfreeCallbackQueue =
        boost::lockfree::queue<std::shared_ptr<CallbackWrapperBase>>;

    std::unique_ptr<LockfreeCallbackQueue> m_lockfreeCallbacks;
#endif

private:
#ifdef ATOM_USE_LOCKFREE_QUEUE
    void runCallbacks() {
        if (m_lockfreeCallbacks) {
            std::shared_ptr<CallbackWrapperBase> callback_ptr;
            while (m_lockfreeCallbacks->pop(callback_ptr)) {
                try {
                    (*callback_ptr)();
                } catch (...) {
                    // Log exception
                }
            }
        }

        std::vector<std::function<void()>> callbacksCopy;
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacksCopy = std::move(callbacks_);
        }

        for (auto& callback : callbacksCopy) {
            try {
                callback();
            } catch (...) {
                // Log exception
            }
        }
    }
#else
    void runCallbacks() {
        std::vector<std::function<void()>> callbacksCopy;
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacksCopy = std::move(callbacks_);
        }

        for (auto& callback : callbacksCopy) {
            try {
                callback();
            } catch (...) {
                // Log exception
            }
        }
    }
#endif
};

template <typename Signature, typename F>
[[nodiscard]] auto make_enhanced_task(F&& f) {
    return EnhancedPackagedTask<Signature>(std::forward<F>(f));
}

template <typename F>
[[nodiscard]] auto make_enhanced_task(F&& f) {
    return make_enhanced_task_impl(std::forward<F>(f),
                                   &std::decay_t<F>::operator());
}

template <typename F, typename Ret, typename C, typename... Args>
[[nodiscard]] auto make_enhanced_task_impl(F&& f, Ret (C::*)(Args...) const) {
    return EnhancedPackagedTask<Ret, Args...>(
        std::function<Ret(Args...)>(std::forward<F>(f)));
}

template <typename F, typename Ret, typename C, typename... Args>
[[nodiscard]] auto make_enhanced_task_impl(F&& f, Ret (C::*)(Args...)) {
    return EnhancedPackagedTask<Ret, Args...>(
        std::function<Ret(Args...)>(std::forward<F>(f)));
}

#ifdef ATOM_USE_ASIO
template <typename Signature, typename F>
[[nodiscard]] auto make_enhanced_task_with_asio(F&& f,
                                                asio::io_context* context) {
    return EnhancedPackagedTask<Signature>(std::forward<F>(f), context);
}

template <typename F>
[[nodiscard]] auto make_enhanced_task_with_asio(F&& f,
                                                asio::io_context* context) {
    return make_enhanced_task_with_asio_impl(
        std::forward<F>(f), &std::decay_t<F>::operator(), context);
}

template <typename F, typename Ret, typename C, typename... Args>
[[nodiscard]] auto make_enhanced_task_with_asio_impl(
    F&& f, Ret (C::*)(Args...) const, asio::io_context* context) {
    return EnhancedPackagedTask<Ret, Args...>(
        std::function<Ret(Args...)>(std::forward<F>(f)), context);
}

template <typename F, typename Ret, typename C, typename... Args>
[[nodiscard]] auto make_enhanced_task_with_asio_impl(
    F&& f, Ret (C::*)(Args...), asio::io_context* context) {
    return EnhancedPackagedTask<Ret, Args...>(
        std::function<Ret(Args...)>(std::forward<F>(f)), context);
}
#endif

}  // namespace atom::async

#endif  // ATOM_ASYNC_PACKAGED_TASK_HPP
