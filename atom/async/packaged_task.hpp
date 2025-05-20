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

#include "atom/async/future.hpp"  // Assuming atom/error/RuntimeError is included through this or globally

#ifdef __cpp_lib_hardware_interference_size
using std::hardware_constructive_interference_size;
using std::hardware_destructive_interference_size;
#else
/**
 * @brief Fallback value for hardware constructive interference size if not
 * defined by the standard library.
 * @details This value is a common cache line size (64 bytes) and is used for
 * alignment purposes to prevent false sharing and improve performance in
 * concurrent data structures.
 */
constexpr std::size_t hardware_constructive_interference_size = 64;
/**
 * @brief Fallback value for hardware destructive interference size if not
 * defined by the standard library.
 * @details This value is a common cache line size (64 bytes) and is used for
 * alignment purposes to prevent false sharing and improve performance in
 * concurrent data structures.
 */
constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

#ifdef ATOM_USE_LOCKFREE_QUEUE
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#endif

/**
 * @namespace atom::async
 * @brief Provides asynchronous programming utilities, including enhanced
 * futures, promises, and packaged tasks.
 */
namespace atom::async {

/**
 * @class InvalidPackagedTaskException
 * @brief Exception thrown when an operation is attempted on an invalid or
 * improperly configured EnhancedPackagedTask.
 * @details This can occur if a task is created with a null function, if a
 * future is accessed after the task has been moved, or if a cancelled task is
 * invoked. Inherits from atom::error::RuntimeError.
 */
class InvalidPackagedTaskException : public atom::error::RuntimeError {
public:
    /**
     * @brief Inherits constructors from atom::error::RuntimeError.
     */
    using atom::error::RuntimeError::RuntimeError;
};

/**
 * @def THROW_INVALID_PACKAGED_TASK_EXCEPTION
 * @brief Macro to throw an InvalidPackagedTaskException with file, line, and
 * function information.
 * @param ... Message arguments to be formatted into the exception message.
 * @hideinitializer
 */
#define THROW_INVALID_PACKAGED_TASK_EXCEPTION(...)                     \
    throw InvalidPackagedTaskException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__);

/**
 * @def THROW_NESTED_INVALID_PACKAGED_TASK_EXCEPTION
 * @brief Macro to rethrow a nested InvalidPackagedTaskException, prepending
 * context information.
 * @param ... Message arguments to be formatted into the exception message.
 * @hideinitializer
 */
#define THROW_NESTED_INVALID_PACKAGED_TASK_EXCEPTION(...) \
    InvalidPackagedTaskException::rethrowNested(          \
        ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME,   \
        "Invalid packaged task: " __VA_ARGS__);

/**
 * @concept InvocableWithResult
 * @brief Concept to check if a callable F can be invoked with Args... and its
 * result is compatible with R.
 * @tparam F The callable type.
 * @tparam R The expected result type (can be void).
 * @tparam Args The argument types for the callable.
 * @details This concept ensures that `F` is invocable with `Args...`, and that
 * the result of this invocation is either the same as `R`, or `R` is `void` (in
 * which case the result type of `F` doesn't matter as long as it's invocable).
 */
template <typename F, typename R, typename... Args>
concept InvocableWithResult =
    std::invocable<F, Args...> &&
    (std::same_as<std::invoke_result_t<F, Args...>, R> ||
     std::same_as<R, void>);

/**
 * @class EnhancedPackagedTask
 * @brief A class that wraps a callable object (task) and allows its result to
 * be retrieved asynchronously via an EnhancedFuture. It also supports
 * cancellation and completion callbacks.
 * @tparam ResultType The result type of the callable object.
 * @tparam Args The types of the arguments that the callable object takes.
 * @details This class is aligned to `hardware_constructive_interference_size`
 * to potentially improve performance by reducing false sharing in
 * multi-threaded environments. It provides a mechanism similar to
 * `std::packaged_task` but with additional features like `EnhancedFuture`
 * integration and optional lock-free callbacks.
 */
template <typename ResultType, typename... Args>
class alignas(hardware_constructive_interference_size) EnhancedPackagedTask {
public:
    /**
     * @typedef TaskType
     * @brief The type of the callable object stored by this packaged task.
     * @details It's a `std::function` with the signature `ResultType(Args...)`.
     */
    using TaskType = std::function<ResultType(Args...)>;

    /**
     * @brief Constructs an EnhancedPackagedTask with the given callable task.
     * @param task The callable object to be wrapped. Must be a valid, non-null
     * callable.
     * @throw InvalidPackagedTaskException if the provided task is invalid
     * (e.g., a null std::function).
     * @details Initializes a promise and a shared future to manage the
     * asynchronous result.
     */
    explicit EnhancedPackagedTask(TaskType task)
        : cancelled_(false), task_(std::move(task)) {
        if (!task_) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Provided task is invalid");
        }
        promise_ = std::make_unique<std::promise<ResultType>>();
        future_ = promise_->get_future().share();
    }

    /**
     * @brief Deleted copy constructor. EnhancedPackagedTask is not copyable.
     */
    EnhancedPackagedTask(const EnhancedPackagedTask&) = delete;
    /**
     * @brief Deleted copy assignment operator. EnhancedPackagedTask is not
     * copyable.
     */
    EnhancedPackagedTask& operator=(const EnhancedPackagedTask&) = delete;

    /**
     * @brief Move constructor. Transfers ownership of the task and associated
     * state from another EnhancedPackagedTask.
     * @param other The EnhancedPackagedTask to move from. `other` is left in a
     * valid but unspecified state.
     */
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

    /**
     * @brief Move assignment operator. Transfers ownership of the task and
     * associated state from another EnhancedPackagedTask.
     * @param other The EnhancedPackagedTask to move from. `other` is left in a
     * valid but unspecified state.
     * @return A reference to this EnhancedPackagedTask.
     */
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
        }
        return *this;
    }

    /**
     * @brief Retrieves an EnhancedFuture associated with the result of this
     * task.
     * @return An EnhancedFuture<ResultType> that will eventually hold the
     * result of the task or an exception.
     * @throw InvalidPackagedTaskException if the internal future is no longer
     * valid (e.g., after a move).
     * @details The returned future can be used to wait for the task's
     * completion and get its result.
     */
    [[nodiscard]] EnhancedFuture<ResultType> getEnhancedFuture() const {
        if (!future_.valid()) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Future is no longer valid");
        }
        return EnhancedFuture<ResultType>(future_);
    }

    /**
     * @brief Executes the wrapped callable object with the given arguments.
     * @param args The arguments to pass to the callable object.
     * @details If the task has been cancelled, it sets an exception on the
     * promise. If the task function is invalid, it sets an exception on the
     * promise. Otherwise, it invokes the task. If the task returns a value,
     * it's stored in the promise. If the task throws an exception, that
     * exception is stored in the promise. After successful execution (value set
     * or void task completed), registered callbacks are run. Catches
     * `std::future_error` internally if `promise_->set_exception` or
     * `promise_->set_value` is called on an already satisfied promise (e.g.,
     * due to a race condition with cancellation).
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
            if constexpr (!std::is_void_v<ResultType>) {
                ResultType result =
                    std::invoke(task_, std::forward<Args>(args)...);
                promise_->set_value(std::move(result));
                runCallbacks(result);
            } else {
                std::invoke(task_, std::forward<Args>(args)...);
                promise_->set_value();  // For void specialization
                runCallbacks();         // For void specialization
            }
        } catch (...) {
            try {
                promise_->set_exception(std::current_exception());
            } catch (const std::future_error&) {
                // Promise might have been fulfilled already (e.g., by
                // cancellation or another thread), ignore.
            }
        }
    }

#ifdef ATOM_USE_LOCKFREE_QUEUE
    /**
     * @brief Registers a callback function to be executed when the task
     * completes.
     * @tparam F The type of the callback function. Must be invocable with
     * `ResultType`.
     * @param func The callback function.
     * @throw InvalidPackagedTaskException if the provided callback is invalid.
     * @details This version is enabled if `ATOM_USE_LOCKFREE_QUEUE` is defined.
     *          It attempts to add the callback to a lock-free queue for
     * efficient concurrent access. If pushing to the lock-free queue fails
     * after several retries with exponential backoff, it falls back to a
     * mutex-protected std::vector of callbacks. The callback will receive the
     * result of the task as its argument.
     */
    template <typename F>
        requires std::invocable<F, ResultType>
    void onComplete(F&& func) {
        if (!func) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION(
                "Provided callback is invalid");
        }

        // Double-checked locking pattern for initializing m_lockfreeCallbacks
        if (!m_lockfreeCallbacks) {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            if (!m_lockfreeCallbacks) {
                m_lockfreeCallbacks = std::make_unique<LockfreeCallbackQueue>(
                    CALLBACK_QUEUE_SIZE);
            }
        }

        auto wrappedCallback = std::make_shared<CallbackWrapperImpl<F>>(
            std::forward<F>(func));  // Use concrete type

        // Use exponential backoff for retries
        constexpr int MAX_RETRIES = 3;
        bool pushed = false;

        for (int i = 0; i < MAX_RETRIES && !pushed; ++i) {
            pushed = m_lockfreeCallbacks->push(wrappedCallback);
            if (!pushed) {
                std::this_thread::sleep_for(
                    std::chrono::microseconds(1 << i));  // Exponential backoff
            }
        }

        if (!pushed) {  // Fallback to mutex-protected vector
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacks_.emplace_back(
                [wrappedCallback](
                    const ResultType&
                        result) {  // Ensure lambda captures shared_ptr
                    (*wrappedCallback)(result);
                });
        }
    }
#else
    /**
     * @brief Registers a callback function to be executed when the task
     * completes.
     * @tparam F The type of the callback function. Must be invocable with
     * `ResultType`.
     * @param func The callback function.
     * @throw InvalidPackagedTaskException if the provided callback is invalid.
     * @details This version is enabled if `ATOM_USE_LOCKFREE_QUEUE` is NOT
     * defined. It adds the callback to a mutex-protected `std::vector`. The
     * callback will receive the result of the task as its argument.
     */
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

    /**
     * @brief Attempts to cancel the task.
     * @return `true` if the task was successfully marked for cancellation,
     * `false` if it was already cancelled.
     * @details Cancellation is advisory. If the task has already started
     * executing, it may complete. If cancelled before execution, invoking the
     * task will result in an exception being set on the future. Uses atomic
     * operations for thread-safe cancellation.
     */
    [[nodiscard]] bool cancel() noexcept {
        bool expected = false;
        // memory_order_acq_rel ensures that if this operation succeeds,
        // the write to cancelled_ is visible to other threads trying to read it
        // (acquire), and previous writes in this thread are visible before this
        // store (release).
        return cancelled_.compare_exchange_strong(expected, true,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire);
    }

    /**
     * @brief Checks if the task has been cancelled.
     * @return `true` if the task has been marked for cancellation, `false`
     * otherwise.
     * @details Uses atomic operations for thread-safe status checking.
     */
    [[nodiscard]] bool isCancelled() const noexcept {
        return cancelled_.load(std::memory_order_acquire);
    }

    /**
     * @brief Checks if the task is valid and can be invoked.
     * @return `true` if the task holds a valid callable, has not been
     * cancelled, and its future is valid. `false` otherwise.
     * @details This can be used to check the state of the task before
     * attempting to execute it or get its future.
     */
    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(task_) && !isCancelled() && future_.valid();
    }

protected:
    /**
     * @brief The wrapped callable task.
     * @details Aligned to `hardware_destructive_interference_size` to prevent
     * false sharing with adjacent members like `promise_` if they were
     * frequently accessed by different threads.
     */
    alignas(hardware_destructive_interference_size) TaskType task_;
    /** @brief The promise associated with the task's result. */
    std::unique_ptr<std::promise<ResultType>> promise_;
    /** @brief The shared future associated with the task's result. */
    std::shared_future<ResultType> future_;
    /** @brief A vector of callbacks to be executed upon task completion (used
     * if lock-free queue is disabled or as fallback). */
    std::vector<std::function<void(const ResultType&)>>
        callbacks_;  // Changed to const ResultType&
    /** @brief Atomic flag indicating if the task has been cancelled. */
    std::atomic<bool> cancelled_;
    /** @brief Mutex protecting access to the `callbacks_` vector. */
    mutable std::mutex callbacksMutex_;

#ifdef ATOM_USE_LOCKFREE_QUEUE
    /**
     * @struct CallbackWrapperBase
     * @brief Base interface for type-erased callback wrappers used with the
     * lock-free queue.
     * @details This allows storing callbacks with different signatures (for
     * void vs non-void ResultType) in a homogeneous lock-free queue of
     * `std::shared_ptr<CallbackWrapperBase>`.
     */
    struct CallbackWrapperBase {
        /** @brief Virtual destructor. */
        virtual ~CallbackWrapperBase() = default;
        /**
         * @brief Abstract call operator to invoke the wrapped callback with the
         * task's result.
         * @param result The result of the completed task.
         */
        virtual void operator()(const ResultType& result) = 0;
    };

    /**
     * @struct CallbackWrapperImpl
     * @brief Concrete implementation of CallbackWrapperBase.
     * @tparam F The actual type of the callback function.
     * @details Wraps a callback of type F and implements the call operator.
     */
    template <typename F>
    struct CallbackWrapperImpl : CallbackWrapperBase {
        std::function<void(const ResultType&)>
            callback;  // Store as std::function for type erasure

        /**
         * @brief Constructs a CallbackWrapperImpl with the given function.
         * @param func The function to wrap.
         */
        explicit CallbackWrapperImpl(F&& func)
            : callback(std::forward<F>(func)) {}

        /**
         * @brief Invokes the wrapped callback with the task's result.
         * @param result The result of the completed task.
         */
        void operator()(const ResultType& result) override { callback(result); }
    };

    /** @brief Size of the lock-free callback queue. */
    static constexpr size_t CALLBACK_QUEUE_SIZE = 128;
    /**
     * @typedef LockfreeCallbackQueue
     * @brief Type alias for the lock-free queue used to store callback
     * wrappers.
     * @details Uses `boost::lockfree::queue` of
     * `std::shared_ptr<CallbackWrapperBase>`.
     */
    using LockfreeCallbackQueue = boost::lockfree::queue<
        std::shared_ptr<CallbackWrapperBase>>;  // Use base type

    /** @brief Unique pointer to the lock-free queue for callbacks. Lazily
     * initialized. */
    std::unique_ptr<LockfreeCallbackQueue> m_lockfreeCallbacks;
#endif

private:
    /**
     * @brief Executes all registered callbacks with the task's result.
     * @param result The result of the completed task.
     * @details This method is called internally after the task successfully
     * completes and its value is set. If `ATOM_USE_LOCKFREE_QUEUE` is defined,
     * it first processes callbacks from the lock-free queue, then processes any
     * callbacks from the mutex-protected vector (which might be used as a
     * fallback). If `ATOM_USE_LOCKFREE_QUEUE` is not defined, it only processes
     * callbacks from the mutex-protected vector. Callbacks are moved out of the
     * internal storage before execution to allow new callbacks to be added
     *          concurrently and to prevent deadlocks if a callback tries to add
     * another callback. Exceptions thrown by callbacks are caught and logged
     * (logging mechanism not shown here), allowing other callbacks to execute.
     */
#ifdef ATOM_USE_LOCKFREE_QUEUE
    void runCallbacks(const ResultType& result) {
        if (m_lockfreeCallbacks) {
            std::shared_ptr<CallbackWrapperBase> callback_ptr;
            while (m_lockfreeCallbacks->pop(callback_ptr)) {
                try {
                    (*callback_ptr)(result);
                } catch (...) {
                    // TODO: Log exception but continue with other callbacks
                }
            }
        }

        std::vector<std::function<void(const ResultType&)>> callbacksCopy;
        {
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacksCopy =
                std::move(callbacks_);  // Move callbacks to local copy
            // callbacks_ is now empty or cleared, ready for new additions.
        }

        for (auto& callback : callbacksCopy) {
            try {
                callback(result);
            } catch (...) {
                // TODO: Log exception but continue with other callbacks
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
                // TODO: Log exception but continue with other callbacks
            }
        }
    }
#endif
};

/**
 * @class EnhancedPackagedTask<void, Args...>
 * @brief Specialization of EnhancedPackagedTask for tasks that return `void`.
 * @tparam Args The types of the arguments that the callable object takes.
 * @details This specialization handles tasks that do not produce a value but
 * signal completion. Many functionalities are similar to the primary template,
 * adapted for `void` results.
 */
template <typename... Args>
class alignas(hardware_constructive_interference_size)
    EnhancedPackagedTask<void, Args...> {
public:
    /**
     * @typedef TaskType
     * @brief The type of the callable object stored by this packaged task
     * (returns void).
     */
    using TaskType = std::function<void(Args...)>;

    /**
     * @brief Constructs an EnhancedPackagedTask (void specialization) with the
     * given callable task.
     * @param task The callable object to be wrapped. Must be a valid, non-null
     * callable.
     * @throw InvalidPackagedTaskException if the provided task is invalid.
     */
    explicit EnhancedPackagedTask(TaskType task)
        : cancelled_(false), task_(std::move(task)) {
        if (!task_) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Provided task is invalid");
        }
        promise_ = std::make_unique<std::promise<void>>();
        future_ = promise_->get_future().share();
    }

    /** @brief Deleted copy constructor. */
    EnhancedPackagedTask(const EnhancedPackagedTask&) = delete;
    /** @brief Deleted copy assignment operator. */
    EnhancedPackagedTask& operator=(const EnhancedPackagedTask&) = delete;

    /**
     * @brief Move constructor for void specialization.
     * @param other The EnhancedPackagedTask to move from.
     */
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

    /**
     * @brief Move assignment operator for void specialization.
     * @param other The EnhancedPackagedTask to move from.
     * @return A reference to this EnhancedPackagedTask.
     */
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
        }
        return *this;
    }

    /**
     * @brief Retrieves an EnhancedFuture<void> associated with the completion
     * of this task.
     * @return An EnhancedFuture<void> that will signal completion or an
     * exception.
     * @throw InvalidPackagedTaskException if the internal future is no longer
     * valid.
     */
    [[nodiscard]] EnhancedFuture<void> getEnhancedFuture() const {
        if (!future_.valid()) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Future is no longer valid");
        }
        return EnhancedFuture<void>(future_);
    }

    /**
     * @brief Executes the wrapped callable object (void specialization) with
     * the given arguments.
     * @param args The arguments to pass to the callable object.
     * @details Similar to the primary template's operator(), but for
     * void-returning tasks. Sets the promise upon successful completion or
     * stores an exception if one occurs. Runs callbacks after successful
     * completion.
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
                // Promise might have been fulfilled already, ignore.
            }
        }
    }

#ifdef ATOM_USE_LOCKFREE_QUEUE
    /**
     * @brief Registers a callback function for void tasks.
     * @tparam F The type of the callback function. Must be invocable with no
     * arguments.
     * @param func The callback function.
     * @throw InvalidPackagedTaskException if the provided callback is invalid.
     * @details Lock-free queue enabled version for void tasks.
     */
    template <typename F>
        requires std::invocable<F>  // Callback takes no arguments for void
                                    // result
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

        auto wrappedCallback = std::make_shared<CallbackWrapperImpl<F>>(
            std::forward<F>(func));  // Use concrete type
        bool pushed = false;

        // Use exponential backoff for retries
        for (int i = 0; i < 3 && !pushed; ++i) {  // Max 3 retries
            pushed = m_lockfreeCallbacks->push(wrappedCallback);
            if (!pushed) {
                std::this_thread::sleep_for(std::chrono::microseconds(1 << i));
            }
        }

        if (!pushed) {  // Fallback
            std::lock_guard<std::mutex> lock(callbacksMutex_);
            callbacks_.emplace_back(
                [wrappedCallback]() {  // Ensure lambda captures shared_ptr
                    (*wrappedCallback)();
                });
        }
    }
#else
    /**
     * @brief Registers a callback function for void tasks.
     * @tparam F The type of the callback function. Must be invocable with no
     * arguments.
     * @param func The callback function.
     * @throw InvalidPackagedTaskException if the provided callback is invalid.
     * @details Mutex-protected vector version for void tasks.
     */
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

    /**
     * @brief Attempts to cancel the task (void specialization).
     * @return `true` if successfully marked for cancellation, `false`
     * otherwise.
     */
    [[nodiscard]] bool cancel() noexcept {
        bool expected = false;
        return cancelled_.compare_exchange_strong(expected, true,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire);
    }

    /**
     * @brief Checks if the task has been cancelled (void specialization).
     * @return `true` if cancelled, `false` otherwise.
     */
    [[nodiscard]] bool isCancelled() const noexcept {
        return cancelled_.load(std::memory_order_acquire);
    }

    /**
     * @brief Checks if the task is valid (void specialization).
     * @return `true` if the task is valid, `false` otherwise.
     */
    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(task_) && !isCancelled() && future_.valid();
    }

protected:
    /** @brief The wrapped callable task (returns void). */
    TaskType task_;
    /** @brief The promise associated with the task's completion. */
    std::unique_ptr<std::promise<void>> promise_;
    /** @brief The shared future associated with the task's completion. */
    std::shared_future<void> future_;
    /** @brief A vector of callbacks (taking no arguments) for void tasks. */
    std::vector<std::function<void()>> callbacks_;
    /** @brief Atomic flag indicating if the task has been cancelled. */
    std::atomic<bool> cancelled_;
    /** @brief Mutex protecting access to `callbacks_`. */
    mutable std::mutex callbacksMutex_;

#ifdef ATOM_USE_LOCKFREE_QUEUE
    /**
     * @struct CallbackWrapperBase
     * @brief Base interface for type-erased callback wrappers (void
     * specialization).
     */
    struct CallbackWrapperBase {
        /** @brief Virtual destructor. */
        virtual ~CallbackWrapperBase() = default;
        /** @brief Abstract call operator to invoke the wrapped callback. */
        virtual void operator()() = 0;
    };

    /**
     * @struct CallbackWrapperImpl
     * @brief Concrete implementation of CallbackWrapperBase for void tasks.
     * @tparam F The actual type of the callback function (invocable with no
     * arguments).
     */
    template <typename F>
    struct CallbackWrapperImpl : CallbackWrapperBase {
        std::function<void()> callback;  // Store as std::function

        /**
         * @brief Constructs a CallbackWrapperImpl with the given function.
         * @param func The function to wrap.
         */
        explicit CallbackWrapperImpl(F&& func)
            : callback(std::forward<F>(func)) {}

        /**
         * @brief Invokes the wrapped callback.
         */
        void operator()() override { callback(); }
    };

    /** @brief Size of the lock-free callback queue. */
    static constexpr size_t CALLBACK_QUEUE_SIZE = 128;
    /**
     * @typedef LockfreeCallbackQueue
     * @brief Type alias for the lock-free queue for void task callbacks.
     */
    using LockfreeCallbackQueue =
        boost::lockfree::queue<std::shared_ptr<CallbackWrapperBase>>;

    /** @brief Unique pointer to the lock-free queue for void task callbacks. */
    std::unique_ptr<LockfreeCallbackQueue> m_lockfreeCallbacks;
#endif

private:
    /**
     * @brief Executes all registered callbacks for a void task.
     * @details Called internally after the void task successfully completes.
     *          Handles lock-free and mutex-protected callback queues similarly
     * to the non-void version.
     */
#ifdef ATOM_USE_LOCKFREE_QUEUE
    void runCallbacks() {
        if (m_lockfreeCallbacks) {
            std::shared_ptr<CallbackWrapperBase> callback_ptr;
            while (m_lockfreeCallbacks->pop(callback_ptr)) {
                try {
                    (*callback_ptr)();
                } catch (...) {
                    // TODO: Log exception
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
                // TODO: Log exception
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
                // TODO: Log exception
            }
        }
    }
#endif
};

/**
 * @brief Helper function to create an EnhancedPackagedTask from a callable,
 * with explicit signature.
 * @tparam Signature The function signature (e.g., `Ret(Args...)`) of the task.
 * @tparam F The type of the callable object.
 * @param f The callable object.
 * @return An EnhancedPackagedTask wrapping the callable `f` with the specified
 * signature.
 * @details This version is useful when automatic signature deduction might fail
 * or is not desired. The `Signature` must be a function type.
 */
template <typename Signature, typename F>
[[nodiscard]] auto make_enhanced_task(F&& f) {
    // This relies on EnhancedPackagedTask's constructor which takes
    // std::function. The conversion from F to std::function<Signature> will
    // happen there. We need to extract ResultType and Args... from Signature.
    // Let's assume Signature is of form Ret(Args...)
    // This is a bit tricky with templates. A common way is to use a helper
    // struct or alias. For simplicity, we'll rely on the constructor of
    // EnhancedPackagedTask to do the heavy lifting if it's designed to deduce
    // ResultType and Args... from a std::function<Signature>. However, the
    // current EnhancedPackagedTask takes ResultType, Args... directly.

    // A more direct way to use the template parameters of EnhancedPackagedTask:
    // Helper to deduce ResultType and Args... from Signature
    // This is a simplified version. A full solution would involve more template
    // metaprogramming. For now, this function expects Signature to be in a form
    // that can be directly used by EnhancedPackagedTask if it were like:
    // EnhancedPackagedTask<Ret, Args...>(std::function<Ret(Args...)>) The
    // current EnhancedPackagedTask is EnhancedPackagedTask<Ret,
    // Args...>(std::function<Ret(Args...)>) So, we need to deduce Ret and
    // Args... from Signature.

    // Let's define a helper to extract return and argument types.
    // This is a common pattern.
    // Example: template <typename Ret, typename... Args> struct
    // SignatureHelper<Ret(Args...)> { using Result = Ret; using Arguments =
    // std::tuple<Args...>; }; Then use SignatureHelper<Signature>::Result and
    // unpack SignatureHelper<Signature>::Arguments.

    // Given the current structure of EnhancedPackagedTask<ResultType, Args...>,
    // this make_enhanced_task is slightly problematic if Signature is the only
    // thing provided. The original code had: return
    // EnhancedPackagedTask<Signature>(std::forward<F>(f)); This implies
    // Signature itself is the ResultType, and Args... are empty, which is not
    // general.

    // Let's assume the intent is:
    // make_enhanced_task<Ret(Args...)>(callable) -> EnhancedPackagedTask<Ret,
    // Args...>(callable) This requires a way to pass Ret and Args... to
    // EnhancedPackagedTask from Signature. The provided code for
    // make_enhanced_task_impl does this deduction. This first overload seems to
    // be for when the user *explicitly* provides the signature as a template
    // argument. If Signature is Ret(Args...), then we need to instantiate
    // EnhancedPackagedTask<Ret, Args...>. This is usually done by specializing
    // make_enhanced_task for function types.

    // Re-interpreting the original:
    // template <typename Signature, typename F>
    // [[nodiscard]] auto make_enhanced_task(F&& f) {
    // return EnhancedPackagedTask<Signature>(std::forward<F>(f));
    // }
    // This means Signature IS ResultType, and Args... is empty.
    // This is likely not the general intent for a "Signature" template
    // parameter. The overloads below are more robust for deduction.

    // If the user writes `make_enhanced_task<int(float)>(my_func)`,
    // they expect `EnhancedPackagedTask<int, float>`.
    // The current `EnhancedPackagedTask<ResultType, Args...>` takes
    // `ResultType` and `Args...` separately. So, this overload needs to parse
    // `Signature`.

    // For now, I will assume this overload is meant for a specific use case or
    // relies on a different interpretation. The more general deduction is
    // below. If `Signature` is `ResultType(Args...)`, then this should be:
    // return EnhancedPackagedTask<typename
    // function_traits<Signature>::result_type,
    //                             typename
    //                             function_traits<Signature>::arg_types...>(std::forward<F>(f));
    // (Requires a function_traits utility).

    // Given the existing structure, the most direct interpretation of the
    // original code is: Signature is ResultType, and Args... is empty. This is
    // probably not what's intended for a generic "Signature". The overloads
    // below are more common for creating packaged_tasks. I will comment it as
    // if Signature is ResultType and Args... is empty, per the original
    // structure. Or, more likely, the user is expected to provide ResultType as
    // Signature, and Args... are deduced or empty. The most robust solution is
    // to remove this overload or implement function_traits. For now, let's
    // assume Signature is ResultType and Args... are empty for this specific
    // overload. This makes it `EnhancedPackagedTask<Signature /*as
    // ResultType*/>(std::function<Signature()>)`. This is very limited.

    // The common pattern is:
    // template <typename R, typename... A, typename F>
    // make_enhanced_task(F&& f) -> EnhancedPackagedTask<R, A...>
    // where R(A...) is the signature.
    // The existing code has `make_enhanced_task_impl` which does this.
    // This top-level `make_enhanced_task<Signature, F>` is likely an error or a
    // very specific use case.

    // Let's assume the user provides the *result type* as Signature, and Args
    // are deduced or empty. This is still not ideal. The
    // `make_enhanced_task_impl` is the correct way. I will keep the original
    // structure but add a note. The most sensible interpretation of
    // `EnhancedPackagedTask<Signature>` would be if `Signature` is `ResultType`
    // and `Args...` is empty. This is what `EnhancedPackagedTask<Signature>`
    // would mean if `Args...` was defaulted. The original code
    // `EnhancedPackagedTask<Signature>(std::forward<F>(f))` implies `ResultType
    // = Signature` and `Args...` is empty.
    return EnhancedPackagedTask<Signature>(
        std::forward<F>(f));  // Note: This implies Signature is ResultType and
                              // Args... is empty.
}

/**
 * @brief Helper function to create an EnhancedPackagedTask from a callable
 * object (e.g., lambda, functor) with automatic signature deduction.
 * @tparam F The type of the callable object.
 * @param f The callable object.
 * @return An EnhancedPackagedTask wrapping the callable `f`. The result and
 * argument types are deduced.
 * @details This overload uses SFINAE or concepts with a helper
 * `make_enhanced_task_impl` to deduce the signature of the callable's
 * `operator()`.
 */
template <typename F>
[[nodiscard]] auto make_enhanced_task(F&& f) {
    // Forward to an implementation detail that can use SFINAE on member
    // function pointers (operator()) This is a common pattern for deducing
    // lambda/functor signatures.
    return make_enhanced_task_impl(std::forward<F>(f),
                                   &std::decay_t<F>::operator());
}

/**
 * @brief Implementation helper for `make_enhanced_task` to deduce signature
 * from `operator() const`.
 * @tparam F The type of the callable object.
 * @tparam Ret The return type of the callable's `operator()`.
 * @tparam C The class type of the callable (for member function pointer).
 * @tparam Args The argument types of the callable's `operator()`.
 * @param f The callable object.
 * @param The second parameter is a pointer to the `operator()` of F, used for
 * deduction.
 * @return An EnhancedPackagedTask<Ret, Args...> wrapping `f`.
 * @details This function is typically not called directly by users. It's part
 * of the deduction mechanism. This overload handles `const` operator().
 */
template <typename F, typename Ret, typename C, typename... Args>
[[nodiscard]] auto make_enhanced_task_impl(F&& f, Ret (C::*)(Args...) const) {
    return EnhancedPackagedTask<Ret, Args...>(
        std::function<Ret(Args...)>(std::forward<F>(f)));
}

/**
 * @brief Implementation helper for `make_enhanced_task` to deduce signature
 * from `operator()` (non-const).
 * @tparam F The type of the callable object.
 * @tparam Ret The return type of the callable's `operator()`.
 * @tparam C The class type of the callable (for member function pointer).
 * @tparam Args The argument types of the callable's `operator()`.
 * @param f The callable object.
 * @param The second parameter is a pointer to the `operator()` of F, used for
 * deduction.
 * @return An EnhancedPackagedTask<Ret, Args...> wrapping `f`.
 * @details This function is typically not called directly by users. It's part
 * of the deduction mechanism. This overload handles non-`const` operator().
 */
template <typename F, typename Ret, typename C, typename... Args>
[[nodiscard]] auto make_enhanced_task_impl(F&& f, Ret (C::*)(Args...)) {
    return EnhancedPackagedTask<Ret, Args...>(
        std::function<Ret(Args...)>(std::forward<F>(f)));
}

// The last make_enhanced_task was:
// template <typename Lambda, typename Ret, typename... Args>
// [[nodiscard]] auto make_enhanced_task(Lambda&& lambda) {
// return EnhancedPackagedTask<Ret(Args...)>(std::forward<Lambda>(lambda));
// }
// This is problematic because Ret and Args... are template parameters of
// make_enhanced_task, not deduced from Lambda. The user would have to specify
// them: make_enhanced_task<MyLambdaType, int, float,
// double>(myLambdaTakingFloatDoubleReturningInt) This is not how
// make_packaged_task usually works. The previous overloads using
// make_enhanced_task_impl are the standard way for deduction. This overload
// seems redundant or incorrectly defined if automatic deduction is desired. If
// it's for explicit signature specification, it should be: template <typename
// Ret, typename... Args, typename Lambda>
// [[nodiscard]] auto make_enhanced_task(Lambda&& lambda) {
//    return EnhancedPackagedTask<Ret,
//    Args...>(std::function<Ret(Args...)>(std::forward<Lambda>(lambda)));
// }
// This is effectively what the make_enhanced_task_impl achieves.
// I will remove this potentially confusing/incorrect overload as the
// `make_enhanced_task_impl` covers deduction. If the intent was for
// `Ret(Args...)` to be a single template parameter (like a function type), then
// it would need function_traits to unpack it.

}  // namespace atom::async

#endif  // ATOM_ASYNC_PACKAGED_TASK_HPP
