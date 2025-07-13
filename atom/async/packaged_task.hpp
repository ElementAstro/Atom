#ifndef ATOM_ASYNC_PACKAGED_TASK_HPP
#define ATOM_ASYNC_PACKAGED_TASK_HPP

#include <atomic>
#include <concepts>
#include <functional>
#include <future>
#include <type_traits>
#include <utility>

#include "atom/async/future.hpp"
#include "atom/error/exception.hpp"

#ifdef __cpp_lib_hardware_interference_size
using std::hardware_constructive_interference_size;
using std::hardware_destructive_interference_size;
#else
constexpr std::size_t hardware_constructive_interference_size = 64;
constexpr std::size_t hardware_destructive_interference_size = 64;
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

namespace internal {
// Base for continuations to allow for a intrusive lock-free list
template <typename ResultType>
struct ContinuationBase {
    virtual ~ContinuationBase() = default;
    // Changed run signature to take shared_future by const reference
    virtual void run(const std::shared_future<ResultType>& future) = 0;
    ContinuationBase* next = nullptr;
};

template <typename ResultType, typename F>
struct Continuation : ContinuationBase<ResultType> {
    F func;
    explicit Continuation(F&& f) : func(std::move(f)) {}

    // Changed run signature to take shared_future by const reference
    void run(const std::shared_future<ResultType>& future) override {
        if constexpr (std::is_void_v<ResultType>) {
            future.get();  // Check for exceptions
            func();
        } else {
            func(future.get());
        }
    }
};
}  // namespace internal

template <typename ResultType, typename... Args>
class alignas(hardware_constructive_interference_size) PackagedTask {
public:
    using TaskType = std::function<ResultType(Args...)>;

    explicit PackagedTask(TaskType task) : task_(std::move(task)) {
        if (!task_) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Provided task is invalid");
        }
    }

#ifdef ATOM_USE_ASIO
    PackagedTask(TaskType task, asio::io_context* context)
        : task_(std::move(task)), asioContext_(context) {
        if (!task_) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Provided task is invalid");
        }
    }
#endif

    PackagedTask(const PackagedTask&) = delete;
    PackagedTask& operator=(const PackagedTask&) = delete;

    PackagedTask(PackagedTask&& other) noexcept = default;
    PackagedTask& operator=(PackagedTask&& other) noexcept = default;

    [[nodiscard]] EnhancedFuture<ResultType> getEnhancedFuture() {
        return EnhancedFuture<ResultType>(promise_.get_future().share());
    }

    void operator()(Args... args) {
        State expected = State::Pending;
        if (!state_.compare_exchange_strong(expected, State::Executing,
                                            std::memory_order_acq_rel)) {
            return;  // Already executed or cancelled
        }

        auto execute = [this, ... largs = std::forward<Args>(args)]() mutable {
            try {
                if constexpr (!std::is_void_v<ResultType>) {
                    promise_.set_value(
                        std::invoke(task_, std::forward<Args>(largs)...));
                } else {
                    std::invoke(task_, std::forward<Args>(largs)...);
                    promise_.set_value();
                }
            } catch (...) {
                promise_.set_exception(std::current_exception());
            }
            state_.store(State::Completed, std::memory_order_release);
            runContinuations();
        };

#ifdef ATOM_USE_ASIO
        if (asioContext_) {
            asio::post(*asioContext_, std::move(execute));
        } else {
            execute();
        }
#else
        execute();
#endif
    }

    template <typename F>
    void onComplete(F&& func) {
        auto* continuation =
            new internal::Continuation<ResultType, std::decay_t<F>>(
                std::forward<F>(func));

        // Capture the shared_future here to ensure it's valid when passed to
        // continuation->run This is the fix for the potential use-after-free if
        // promise_ is moved or destroyed before the continuation runs.
        auto shared_fut = promise_.get_future().share();

        if (state_.load(std::memory_order_acquire) == State::Completed) {
            // If already completed, run immediately
            continuation->run(shared_fut);
            delete continuation;
            return;
        }

        internal::ContinuationBase<ResultType>* old_head =
            continuations_.load(std::memory_order_relaxed);
        do {
            continuation->next = old_head;
        } while (!continuations_.compare_exchange_weak(
            old_head, continuation, std::memory_order_release,
            std::memory_order_relaxed));

        // Double check after adding to list, if state changed to Completed, run
        // continuations This handles the race condition where state becomes
        // Completed between the initial check and the CAS loop.
        if (state_.load(std::memory_order_acquire) == State::Completed) {
            runContinuations();
        }
    }

    [[nodiscard]] bool cancel() noexcept {
        State expected = State::Pending;
        if (state_.compare_exchange_strong(expected, State::Cancelled,
                                           std::memory_order_acq_rel)) {
            promise_.set_exception(
                std::make_exception_ptr(InvalidPackagedTaskException(
                    ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME,
                    "Task has been cancelled")));
            runContinuations();  // Notify continuations about cancellation via
                                 // exception
            return true;
        }
        return false;
    }

    [[nodiscard]] bool isCancelled() const noexcept {
        return state_.load(std::memory_order_acquire) == State::Cancelled;
    }

#ifdef ATOM_USE_ASIO
    void setAsioContext(asio::io_context* context) { asioContext_ = context; }
    [[nodiscard]] asio::io_context* getAsioContext() const {
        return asioContext_;
    }
#endif

    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(task_);
    }

private:
    enum class State : uint8_t { Pending, Executing, Completed, Cancelled };

    void runContinuations() {
        internal::ContinuationBase<ResultType>* head =
            continuations_.exchange(nullptr, std::memory_order_acq_rel);

        if (!head)
            return;

        // Reverse the list to execute in registration order
        internal::ContinuationBase<ResultType>* prev = nullptr;
        while (head) {
            auto* next = head->next;
            head->next = prev;
            prev = head;
            head = next;
        }
        head = prev;

        // Capture the shared_future once for all continuations
        auto future = promise_.get_future().share();
        while (head) {
            auto* next = head->next;
            try {
                head->run(future);
            } catch (...) {
                // Log exceptions from continuations
            }
            delete head;
            head = next;
        }
    }

    alignas(hardware_destructive_interference_size) TaskType task_;
    std::promise<ResultType> promise_;
    std::atomic<State> state_{State::Pending};
    std::atomic<internal::ContinuationBase<ResultType>*> continuations_{
        nullptr};

#ifdef ATOM_USE_ASIO
    asio::io_context* asioContext_ = nullptr;
#endif
};

template <typename... Args>
class alignas(hardware_constructive_interference_size)
    PackagedTask<void, Args...> {
public:
    using TaskType = std::function<void(Args...)>;

    explicit PackagedTask(TaskType task) : task_(std::move(task)) {
        if (!task_) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Provided task is invalid");
        }
    }

#ifdef ATOM_USE_ASIO
    PackagedTask(TaskType task, asio::io_context* context)
        : task_(std::move(task)), asioContext_(context) {
        if (!task_) {
            THROW_INVALID_PACKAGED_TASK_EXCEPTION("Provided task is invalid");
        }
    }
#endif

    PackagedTask(const PackagedTask&) = delete;
    PackagedTask& operator=(const PackagedTask&) = delete;

    PackagedTask(PackagedTask&& other) noexcept = default;
    PackagedTask& operator=(PackagedTask&& other) noexcept = default;

    [[nodiscard]] EnhancedFuture<void> getEnhancedFuture() {
        return EnhancedFuture<void>(promise_.get_future().share());
    }

    void operator()(Args... args) {
        State expected = State::Pending;
        if (!state_.compare_exchange_strong(expected, State::Executing,
                                            std::memory_order_acq_rel)) {
            return;  // Already executed or cancelled
        }

        auto execute = [this, ... largs = std::forward<Args>(args)]() mutable {
            try {
                std::invoke(task_, std::forward<Args>(largs)...);
                promise_.set_value();
            } catch (...) {
                promise_.set_exception(std::current_exception());
            }
            state_.store(State::Completed, std::memory_order_release);
            runContinuations();
        };

#ifdef ATOM_USE_ASIO
        if (asioContext_) {
            asio::post(*asioContext_, std::move(execute));
        } else {
            execute();
        }
#else
        execute();
#endif
    }

    template <typename F>
        requires std::invocable<F>
    void onComplete(F&& func) {
        auto* continuation = new internal::Continuation<void, std::decay_t<F>>(
            std::forward<F>(func));

        // Capture the shared_future here
        auto shared_fut = promise_.get_future().share();

        if (state_.load(std::memory_order_acquire) == State::Completed) {
            continuation->run(shared_fut);
            delete continuation;
            return;
        }

        internal::ContinuationBase<void>* old_head =
            continuations_.load(std::memory_order_relaxed);
        do {
            continuation->next = old_head;
        } while (!continuations_.compare_exchange_weak(
            old_head, continuation, std::memory_order_release,
            std::memory_order_relaxed));

        if (state_.load(std::memory_order_acquire) == State::Completed) {
            runContinuations();
        }
    }

    [[nodiscard]] bool cancel() noexcept {
        State expected = State::Pending;
        if (state_.compare_exchange_strong(expected, State::Cancelled,
                                           std::memory_order_acq_rel)) {
            promise_.set_exception(
                std::make_exception_ptr(InvalidPackagedTaskException(
                    ATOM_FILE_NAME, ATOM_FILE_LINE, ATOM_FUNC_NAME,
                    "Task has been cancelled")));
            runContinuations();
            return true;
        }
        return false;
    }

    [[nodiscard]] bool isCancelled() const noexcept {
        return state_.load(std::memory_order_acquire) == State::Cancelled;
    }

#ifdef ATOM_USE_ASIO
    void setAsioContext(asio::io_context* context) { asioContext_ = context; }
    [[nodiscard]] asio::io_context* getAsioContext() const {
        return asioContext_;
    }
#endif

    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(task_);
    }

private:
    enum class State : uint8_t { Pending, Executing, Completed, Cancelled };

    void runContinuations() {
        internal::ContinuationBase<void>* head =
            continuations_.exchange(nullptr, std::memory_order_acq_rel);

        if (!head)
            return;

        // Reverse list
        internal::ContinuationBase<void>* prev = nullptr;
        while (head) {
            auto* next = head->next;
            head->next = prev;
            prev = head;
            head = next;
        }
        head = prev;

        // Capture the shared_future once for all continuations
        auto future = promise_.get_future().share();
        while (head) {
            auto* next = head->next;
            try {
                head->run(future);
            } catch (...) {
                // Log
            }
            delete head;
            head = next;
        }
    }

    alignas(hardware_destructive_interference_size) TaskType task_;
    std::promise<void> promise_;
    std::atomic<State> state_{State::Pending};
    std::atomic<internal::ContinuationBase<void>*> continuations_{nullptr};

#ifdef ATOM_USE_ASIO
    asio::io_context* asioContext_ = nullptr;
#endif
};

template <typename Signature, typename F>
[[nodiscard]] auto make_enhanced_task(F&& f) {
    return PackagedTask<Signature>(std::forward<F>(f));
}

template <typename F>
[[nodiscard]] auto make_enhanced_task(F&& f) {
    return make_enhanced_task_impl(std::forward<F>(f),
                                   &std::decay_t<F>::operator());
}

template <typename F, typename Ret, typename C, typename... Args>
[[nodiscard]] auto make_enhanced_task_impl(F&& f, Ret (C::*)(Args...) const) {
    return PackagedTask<Ret, Args...>(
        std::function<Ret(Args...)>(std::forward<F>(f)));
}

template <typename F, typename Ret, typename C, typename... Args>
[[nodiscard]] auto make_enhanced_task_impl(F&& f, Ret (C::*)(Args...)) {
    return PackagedTask<Ret, Args...>(
        std::function<Ret(Args...)>(std::forward<F>(f)));
}

#ifdef ATOM_USE_ASIO
template <typename Signature, typename F>
[[nodiscard]] auto make_enhanced_task_with_asio(F&& f,
                                                asio::io_context* context) {
    return PackagedTask<Signature>(std::forward<F>(f), context);
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
    return PackagedTask<Ret, Args...>(
        std::function<Ret(Args...)>(std::forward<F>(f)), context);
}

template <typename F, typename Ret, typename C, typename... Args>
[[nodiscard]] auto make_enhanced_task_with_asio_impl(
    F&& f, Ret (C::*)(Args...), asio::io_context* context) {
    return PackagedTask<Ret, Args...>(
        std::function<Ret(Args...)>(std::forward<F>(f)), context);
}
#endif

}  // namespace atom::async

#endif  // ATOM_ASYNC_PACKAGED_TASK_HPP
