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
    }

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
    {
    }

    EnhancedPackagedTask& operator=(EnhancedPackagedTask&& other) noexcept {
        if (this != &other) {
            task_ = std::move(other.task_);
            promise_ = std::move(other.promise_);
            future_ = std::move(other.future_);
            callbacks_ = std::move(other.callbacks_);
            cancelled_.store(other.cancelled_.load(std::memory_order_acquire));
#ifdef ATOM_USE_LOCKFREE_QUEUE
            m_lockfreeCallbacks = std::move(other.m_lockfreeCallbacks);
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
                // Promise might have been fulfilled already, ignore
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
            std::make_shared<CallbackWrapper>(std::forward<F>(func));

        // Use exponential backoff for retries
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
#endif

    [[nodiscard]] bool cancel() noexcept {
        bool expected = false;
        return cancelled_.compare_exchange_strong(expected, true,
                                                  std::memory_order_acq_rel);
    }

    [[nodiscard]] bool isCancelled() const noexcept {
        return cancelled_.load(std::memory_order_acquire);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(task_) && !isCancelled() && future_.valid();
    }

protected:
    alignas(hardware_destructive_interference_size) TaskType task_;
    std::unique_ptr<std::promise<ResultType>> promise_;
    std::shared_future<ResultType> future_;
    std::vector<std::function<void(ResultType)>> callbacks_;
    std::atomic<bool> cancelled_;
    mutable std::mutex callbacksMutex_;

#ifdef ATOM_USE_LOCKFREE_QUEUE
    struct CallbackWrapper {
        virtual ~CallbackWrapper() = default;
        virtual void operator()(const ResultType& result) = 0;

        template <typename F>
        explicit CallbackWrapper(F&& func) : callback(std::forward<F>(func)) {}

        std::function<void(ResultType)> callback;

        void operator()(const ResultType& result) { callback(result); }
    };

    static constexpr size_t CALLBACK_QUEUE_SIZE = 128;
    using LockfreeCallbackQueue =
        boost::lockfree::queue<std::shared_ptr<CallbackWrapper>>;

    std::unique_ptr<LockfreeCallbackQueue> m_lockfreeCallbacks;
#endif

private:
#ifdef ATOM_USE_LOCKFREE_QUEUE
    void runCallbacks(const ResultType& result) {
        if (m_lockfreeCallbacks) {
            std::shared_ptr<CallbackWrapper> callback;
            while (m_lockfreeCallbacks->pop(callback)) {
                try {
                    (*callback)(result);
                } catch (...) {
                    // Log exception but continue with other callbacks
                }
            }
        }

        std::vector<std::function<void(ResultType)>> callbacksCopy;
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacksCopy = std::move(callbacks_);
            callbacks_.clear();
        }

        for (auto& callback : callbacksCopy) {
            try {
                callback(result);
            } catch (...) {
                // Log exception but continue with other callbacks
            }
        }
    }
#else
    void runCallbacks(const ResultType& result) {
        std::vector<std::function<void(ResultType)>> callbacksCopy;
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacksCopy = std::move(callbacks_);
            callbacks_.clear();
        }

        for (auto& callback : callbacksCopy) {
            try {
                callback(result);
            } catch (...) {
                // Log exception but continue with other callbacks
            }
        }
    }
#endif
};

// Specialization for void result type
template <typename... Args>
class EnhancedPackagedTask<void, Args...> {
public:
    using TaskType = std::function<void(Args...)>;

    explicit EnhancedPackagedTask(TaskType task)
        : cancelled_(false), task_(std::move(task)) {
        if (!task_) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Provided task is invalid");
        }
        promise_ = std::make_unique<std::promise<void>>();
        future_ = promise_->get_future().share();
    }

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
    {
    }

    EnhancedPackagedTask& operator=(EnhancedPackagedTask&& other) noexcept {
        if (this != &other) {
            task_ = std::move(other.task_);
            promise_ = std::move(other.promise_);
            future_ = std::move(other.future_);
            callbacks_ = std::move(other.callbacks_);
            cancelled_.store(other.cancelled_.load(std::memory_order_acquire));
#ifdef ATOM_USE_LOCKFREE_QUEUE
            m_lockfreeCallbacks = std::move(other.m_lockfreeCallbacks);
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

        try {
            std::invoke(task_, std::forward<Args>(args)...);
            promise_->set_value();
            runCallbacks();
        } catch (...) {
            try {
                promise_->set_exception(std::current_exception());
            } catch (const std::future_error&) {
                // Promise might have been fulfilled already, ignore
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
            std::make_shared<CallbackWrapper>(std::forward<F>(func));
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
#endif

    [[nodiscard]] bool cancel() noexcept {
        bool expected = false;
        return cancelled_.compare_exchange_strong(expected, true,
                                                  std::memory_order_acq_rel);
    }

    [[nodiscard]] bool isCancelled() const noexcept {
        return cancelled_.load(std::memory_order_acquire);
    }

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

#ifdef ATOM_USE_LOCKFREE_QUEUE
    struct CallbackWrapper {
        virtual ~CallbackWrapper() = default;
        virtual void operator()() = 0;

        template <typename F>
        explicit CallbackWrapper(F&& func) : callback(std::forward<F>(func)) {}

        std::function<void()> callback;

        void operator()() { callback(); }
    };

    static constexpr size_t CALLBACK_QUEUE_SIZE = 128;
    using LockfreeCallbackQueue =
        boost::lockfree::queue<std::shared_ptr<CallbackWrapper>>;

    std::unique_ptr<LockfreeCallbackQueue> m_lockfreeCallbacks;
#endif

private:
#ifdef ATOM_USE_LOCKFREE_QUEUE
    void runCallbacks() {
        if (m_lockfreeCallbacks) {
            std::shared_ptr<CallbackWrapper> callback;
            while (m_lockfreeCallbacks->pop(callback)) {
                try {
                    (*callback)();
                } catch (...) {
                    // Log exception but continue with other callbacks
                }
            }
        }

        std::vector<std::function<void()>> callbacksCopy;
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacksCopy = std::move(callbacks_);
            callbacks_.clear();
        }

        for (auto& callback : callbacksCopy) {
            try {
                callback();
            } catch (...) {
                // Log exception but continue with other callbacks
            }
        }
    }
#else
    void runCallbacks() {
        std::vector<std::function<void()>> callbacksCopy;
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacksCopy = std::move(callbacks_);
            callbacks_.clear();
        }

        for (auto& callback : callbacksCopy) {
            try {
                callback();
            } catch (...) {
                // Log exception but continue with other callbacks
            }
        }
    }
#endif
};

template <typename F, typename... Args>
    requires std::invocable<F, Args...>
[[nodiscard]] auto make_enhanced_task(F&& f) {
    using ResultType = std::invoke_result_t<F, Args...>;
    return EnhancedPackagedTask<ResultType, Args...>(std::forward<F>(f));
}

}  // namespace atom::async

#endif  // ATOM_ASYNC_PACKAGED_TASK_HPP
