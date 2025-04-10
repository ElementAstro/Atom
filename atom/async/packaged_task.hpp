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

// Add Boost.lockfree support
#ifdef ATOM_USE_LOCKFREE_QUEUE
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#endif

namespace atom::async {

/**
 * @class InvalidPackagedTaskException
 * @brief Exception thrown when an invalid packaged task is encountered.
 */
class InvalidPackagedTaskException : public atom::error::RuntimeError {
public:
    using atom::error::RuntimeError::RuntimeError;
};

/**
 * @def THROW_INVALID_PACKAGED_TASK_EXCEPTION
 * @brief Macro to throw an InvalidPackagedTaskException with file, line, and
 * function information.
 */
#define THROW_INVALID_PACKAGED_TASK_EXCEPTION(...)                     \
    throw InvalidPackagedTaskException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__);

/**
 * @def THROW_NESTED_INVALID_PACKAGED_TASK_EXCEPTION
 * @brief Macro to rethrow a nested InvalidPackagedTaskException with file,
 * line, and function information.
 */
#define THROW_NESTED_INVALID_PACKAGED_TASK_EXCEPTION(...) \
    InvalidPackagedTaskException::rethrowNested(          \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME,   \
        "Invalid packaged task: " __VA_ARGS__);

/**
 * @concept Invocable
 * @brief Concept to ensure that a function is invocable with given arguments
 */
template <typename F, typename R, typename... Args>
concept InvocableWithResult =
    std::invocable<F, Args...> &&
    (std::same_as<std::invoke_result_t<F, Args...>, R> ||
     std::same_as<R, void>);

/**
 * @class EnhancedPackagedTask
 * @brief A template class that extends the standard packaged task with
 * additional features, optimized with C++20 features.
 * @tparam ResultType The type of the result that the task will produce.
 * @tparam Args The types of the arguments that the task will accept.
 */
template <typename ResultType, typename... Args>
class EnhancedPackagedTask {
public:
    using TaskType = std::function<ResultType(Args...)>;

    /**
     * @brief Constructs an EnhancedPackagedTask with the given task.
     * @param task The task to be executed.
     * @throws InvalidPackagedTaskException if the task is invalid
     */
    explicit EnhancedPackagedTask(TaskType task) : cancelled_(false) {
        if (!task) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Provided task is invalid");
        }
        task_ = std::move(task);
        promise_ = std::make_unique<std::promise<ResultType>>();
        future_ = promise_->get_future().share();
    }

    // Disable copy operations to prevent accidental copies
    EnhancedPackagedTask(const EnhancedPackagedTask&) = delete;
    EnhancedPackagedTask& operator=(const EnhancedPackagedTask&) = delete;

    // Enable move operations
    EnhancedPackagedTask(EnhancedPackagedTask&& other) noexcept
        : task_(std::move(other.task_)),
          promise_(std::move(other.promise_)),
          future_(std::move(other.future_)),
          callbacks_(std::move(other.callbacks_)),
          cancelled_(other.cancelled_.load())
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
            cancelled_.store(other.cancelled_.load());
#ifdef ATOM_USE_LOCKFREE_QUEUE
            m_lockfreeCallbacks = std::move(other.m_lockfreeCallbacks);
#endif
        }
        return *this;
    }

    /**
     * @brief Gets the enhanced future associated with this task.
     * @return An EnhancedFuture object.
     * @throws InvalidPackagedTaskException if the future is not valid
     */
    [[nodiscard]] EnhancedFuture<ResultType> getEnhancedFuture() const {
        if (!future_.valid()) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Future is no longer valid");
        }
        return EnhancedFuture<ResultType>(future_);
    }

    /**
     * @brief Executes the task with the given arguments.
     * @param args The arguments to pass to the task.
     */
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
            ResultType result = std::invoke(task_, std::forward<Args>(args)...);
            promise_->set_value(result);
            runCallbacks(result);
        } catch (...) {
            try {
                promise_->set_exception(std::current_exception());
            } catch (const std::future_error&) {
                // Promise might have been fulfilled already, ignore
            }
        }
    }

#ifdef ATOM_USE_LOCKFREE_QUEUE
    /**
     * @brief Adds a callback to be called upon task completion using lockfree
     * queue.
     * @tparam F The type of the callback function.
     * @param func The callback function to add.
     * @throws InvalidPackagedTaskException if the callback is invalid
     */
    template <typename F>
        requires std::invocable<F, ResultType>
    void onComplete(F&& func) {
        if (!func) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION(
                "Provided callback is invalid");
        }

        // Initialize lockfree callback queue if not already initialized
        if (!m_lockfreeCallbacks) {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            if (!m_lockfreeCallbacks) {
                m_lockfreeCallbacks = std::make_unique<LockfreeCallbackQueue>(
                    CALLBACK_QUEUE_SIZE);
            }
        }

        // Try to add to lockfree queue first with retries
        auto wrappedCallback =
            std::make_shared<CallbackWrapper>(std::forward<F>(func));
        bool pushed = false;

        for (int i = 0; i < 3 && !pushed; ++i) {
            pushed = m_lockfreeCallbacks->push(wrappedCallback);
            if (!pushed) {
                std::this_thread::yield();
            }
        }

        // Fall back to mutex-protected vector if queue is full
        if (!pushed) {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacks_.emplace_back(
                [wrappedCallback](const ResultType& result) {
                    (*wrappedCallback)(result);
                });
        }
    }
#endif

    /**
     * @brief Cancels the task.
     * @return True if the task was successfully cancelled, false if it was
     * already cancelled.
     */
    bool cancel() noexcept {
        bool expected = false;
        return cancelled_.compare_exchange_strong(expected, true);
    }

    /**
     * @brief Checks if the task is cancelled.
     * @return True if the task is cancelled, false otherwise.
     */
    [[nodiscard]] bool isCancelled() const noexcept {
        return cancelled_.load(std::memory_order_acquire);
    }

    /**
     * @brief Checks if the task is valid.
     * @return True if the task is valid, false otherwise.
     */
    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(task_) && !isCancelled() && future_.valid();
    }

protected:
    TaskType task_;  ///< The task to be executed.
    std::unique_ptr<std::promise<ResultType>>
        promise_;  ///< The promise associated with the task.
    std::shared_future<ResultType>
        future_;  ///< The shared future associated with the task.
    std::vector<std::function<void(ResultType)>>
        callbacks_;  ///< List of callbacks to be called on completion.
    std::atomic<bool>
        cancelled_;  ///< Flag indicating if the task has been cancelled.
    std::mutex callbacksMutex_;  ///< Mutex to protect callbacks vector.

#ifdef ATOM_USE_LOCKFREE_QUEUE
    // Type-erased callback wrapper that can be stored in lockfree queue
    struct CallbackWrapper {
        virtual ~CallbackWrapper() = default;
        virtual void operator()(const ResultType& result) = 0;

        template <typename F>
        CallbackWrapper(F&& func) : callback(std::forward<F>(func)) {}

        std::function<void(ResultType)> callback;

        void operator()(const ResultType& result) { callback(result); }
    };

    // Alias for the lockfree queue type
    static constexpr size_t CALLBACK_QUEUE_SIZE = 128;
    using LockfreeCallbackQueue =
        boost::lockfree::queue<std::shared_ptr<CallbackWrapper>>;

    // Lockfree queue for callbacks
    std::unique_ptr<LockfreeCallbackQueue> m_lockfreeCallbacks;
#endif

private:
#ifdef ATOM_USE_LOCKFREE_QUEUE
    /**
     * @brief Runs all the registered callbacks with the given result.
     * Optimized version that processes lockfree queue first.
     * @param result The result to pass to the callbacks.
     */
    void runCallbacks(const ResultType& result) {
        // First process callbacks from lockfree queue if available
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

        // Then process callbacks from vector
        std::vector<std::function<void(ResultType)>> callbacksCopy;
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacksCopy = callbacks_;
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
    /**
     * @brief Runs all the registered callbacks with the given result.
     * @param result The result to pass to the callbacks.
     */
    void runCallbacks(const ResultType& result) {
        std::vector<std::function<void(ResultType)>> callbacksCopy;
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacksCopy = callbacks_;
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

/**
 * @class EnhancedPackagedTask<void, Args...>
 * @brief Specialization of the EnhancedPackagedTask class for void result type.
 * @tparam Args The types of the arguments that the task will accept.
 */
template <typename... Args>
class EnhancedPackagedTask<void, Args...> {
public:
    using TaskType = std::function<void(Args...)>;

    /**
     * @brief Constructs an EnhancedPackagedTask with the given task.
     * @param task The task to be executed.
     * @throws InvalidPackagedTaskException if the task is invalid
     */
    explicit EnhancedPackagedTask(TaskType task) : cancelled_(false) {
        if (!task) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Provided task is invalid");
        }
        task_ = std::move(task);
        promise_ = std::make_unique<std::promise<void>>();
        future_ = promise_->get_future().share();
    }

    // Disable copy operations
    EnhancedPackagedTask(const EnhancedPackagedTask&) = delete;
    EnhancedPackagedTask& operator=(const EnhancedPackagedTask&) = delete;

    // Enable move operations
    EnhancedPackagedTask(EnhancedPackagedTask&& other) noexcept
        : task_(std::move(other.task_)),
          promise_(std::move(other.promise_)),
          future_(std::move(other.future_)),
          callbacks_(std::move(other.callbacks_)),
          cancelled_(other.cancelled_.load())
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
            cancelled_.store(other.cancelled_.load());
#ifdef ATOM_USE_LOCKFREE_QUEUE
            m_lockfreeCallbacks = std::move(other.m_lockfreeCallbacks);
#endif
        }
        return *this;
    }

    /**
     * @brief Gets the enhanced future associated with this task.
     * @return An EnhancedFuture object.
     * @throws InvalidPackagedTaskException if the future is not valid
     */
    [[nodiscard]] EnhancedFuture<void> getEnhancedFuture() const {
        if (!future_.valid()) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Future is no longer valid");
        }
        return EnhancedFuture<void>(future_);
    }

    /**
     * @brief Executes the task with the given arguments.
     * @param args The arguments to pass to the task.
     */
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
    /**
     * @brief Adds a callback to be called upon task completion using lockfree
     * queue.
     * @tparam F The type of the callback function.
     * @param func The callback function to add.
     * @throws InvalidPackagedTaskException if the callback is invalid
     */
    template <typename F>
        requires std::invocable<F>
    void onComplete(F&& func) {
        if (!func) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION(
                "Provided callback is invalid");
        }

        // Initialize lockfree callback queue if not already initialized
        if (!m_lockfreeCallbacks) {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            if (!m_lockfreeCallbacks) {
                m_lockfreeCallbacks = std::make_unique<LockfreeCallbackQueue>(
                    CALLBACK_QUEUE_SIZE);
            }
        }

        // Try to add to lockfree queue first with retries
        auto wrappedCallback =
            std::make_shared<CallbackWrapper>(std::forward<F>(func));
        bool pushed = false;

        for (int i = 0; i < 3 && !pushed; ++i) {
            pushed = m_lockfreeCallbacks->push(wrappedCallback);
            if (!pushed) {
                std::this_thread::yield();
            }
        }

        // Fall back to mutex-protected vector if queue is full
        if (!pushed) {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacks_.emplace_back(
                [wrappedCallback]() { (*wrappedCallback)(); });
        }
    }
#endif

    /**
     * @brief Cancels the task.
     * @return True if the task was successfully cancelled, false if it was
     * already cancelled.
     */
    bool cancel() noexcept {
        bool expected = false;
        return cancelled_.compare_exchange_strong(expected, true);
    }

    /**
     * @brief Checks if the task is cancelled.
     * @return True if the task is cancelled, false otherwise.
     */
    [[nodiscard]] bool isCancelled() const noexcept {
        return cancelled_.load(std::memory_order_acquire);
    }

    /**
     * @brief Checks if the task is valid.
     * @return True if the task is valid, false otherwise.
     */
    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(task_) && !isCancelled() && future_.valid();
    }

protected:
    TaskType task_;  ///< The task to be executed.
    std::unique_ptr<std::promise<void>>
        promise_;  ///< The promise associated with the task.
    std::shared_future<void>
        future_;  ///< The shared future associated with the task.
    std::vector<std::function<void()>>
        callbacks_;  ///< List of callbacks to be called on completion.
    std::atomic<bool>
        cancelled_;  ///< Flag indicating if the task has been cancelled.
    std::mutex callbacksMutex_;  ///< Mutex to protect callbacks vector.

#ifdef ATOM_USE_LOCKFREE_QUEUE
    // Type-erased callback wrapper that can be stored in lockfree queue
    struct CallbackWrapper {
        virtual ~CallbackWrapper() = default;
        virtual void operator()() = 0;

        template <typename F>
        CallbackWrapper(F&& func) : callback(std::forward<F>(func)) {}

        std::function<void()> callback;

        void operator()() { callback(); }
    };

    // Alias for the lockfree queue type
    static constexpr size_t CALLBACK_QUEUE_SIZE = 128;
    using LockfreeCallbackQueue =
        boost::lockfree::queue<std::shared_ptr<CallbackWrapper>>;

    // Lockfree queue for callbacks
    std::unique_ptr<LockfreeCallbackQueue> m_lockfreeCallbacks;
#endif

private:
#ifdef ATOM_USE_LOCKFREE_QUEUE
    /**
     * @brief Runs all the registered callbacks.
     * Optimized version that processes lockfree queue first.
     */
    void runCallbacks() {
        // First process callbacks from lockfree queue if available
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

        // Then process callbacks from vector
        std::vector<std::function<void()>> callbacksCopy;
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacksCopy = callbacks_;
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
    /**
     * @brief Runs all the registered callbacks.
     */
    void runCallbacks() {
        std::vector<std::function<void()>> callbacksCopy;
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacksCopy = callbacks_;
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

/**
 * @brief Helper function to create an EnhancedPackagedTask with deduced types
 * @tparam F Function type
 * @tparam Args Argument types
 * @param f Function to wrap in a packaged task
 * @return EnhancedPackagedTask object
 */
template <typename F, typename... Args>
    requires std::invocable<F, Args...>
auto make_enhanced_task(F&& f) {
    using ResultType = std::invoke_result_t<F, Args...>;
    return EnhancedPackagedTask<ResultType, Args...>(std::forward<F>(f));
}

}  // namespace atom::async

#endif  // ATOM_ASYNC_PACKAGED_TASK_HPP
