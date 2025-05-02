#include "promise.hpp"

namespace atom::async {
// Implementation for void specialization
Promise<void>::Promise() noexcept : future_(promise_.get_future().share()) {}

// Implement move constructor for void specialization
Promise<void>::Promise(Promise&& other) noexcept
    : promise_(std::move(other.promise_)), future_(std::move(other.future_)) {
#ifdef ATOM_USE_BOOST_LOCKFREE
    // Special handling for lock-free queue
    CallbackWrapper* wrapper = nullptr;
    while (other.callbacks_.pop(wrapper)) {
        if (wrapper) {
            callbacks_.push(wrapper);
        }
    }
#else
    std::unique_lock lock(other.mutex_);
    callbacks_ = std::move(other.callbacks_);
#endif
    cancelled_.store(other.cancelled_.load());
    completed_.store(other.completed_.load());

    // Handle cancellation thread
    if (other.cancellationThread_.has_value()) {
        cancellationThread_ = std::move(other.cancellationThread_);
        other.cancellationThread_.reset();
    }

#ifndef ATOM_USE_BOOST_LOCKFREE
    other.callbacks_.clear();
#endif
    other.cancelled_.store(false);
    other.completed_.store(false);
}

// Implement move assignment operator for void specialization
Promise<void>& Promise<void>::operator=(Promise&& other) noexcept {
    if (this != &other) {
        promise_ = std::move(other.promise_);
        future_ = std::move(other.future_);

#ifdef ATOM_USE_BOOST_LOCKFREE
        // Clean up current queue
        CallbackWrapper* wrapper = nullptr;
        while (callbacks_.pop(wrapper)) {
            delete wrapper;
        }

        // Transfer elements
        while (other.callbacks_.pop(wrapper)) {
            if (wrapper) {
                callbacks_.push(wrapper);
            }
        }
#else
        std::scoped_lock lock(mutex_, other.mutex_);
        callbacks_ = std::move(other.callbacks_);
#endif
        cancelled_.store(other.cancelled_.load());
        completed_.store(other.completed_.load());

        // Handle cancellation thread
        if (cancellationThread_.has_value()) {
            cancellationThread_->request_stop();
        }
        if (other.cancellationThread_.has_value()) {
            cancellationThread_ = std::move(other.cancellationThread_);
            other.cancellationThread_.reset();
        }

#ifndef ATOM_USE_BOOST_LOCKFREE
        other.callbacks_.clear();
#endif
        other.cancelled_.store(false);
        other.completed_.store(false);
    }
    return *this;
}

[[nodiscard]] auto Promise<void>::getEnhancedFuture() noexcept
    -> EnhancedFuture<void> {
    return EnhancedFuture<void>(future_);
}

void Promise<void>::setValue() {
    if (isCancelled()) {
        THROW_PROMISE_CANCELLED_EXCEPTION(
            "Cannot set value, promise was cancelled.");
    }

    if (completed_.exchange(true)) {
        THROW_PROMISE_CANCELLED_EXCEPTION(
            "Cannot set value, promise was already completed.");
    }

    try {
        promise_.set_value();
        runCallbacks();  // Execute callbacks
    } catch (const std::exception& e) {
        // If we can't set the value due to a system exception, capture it
        try {
            promise_.set_exception(std::current_exception());
        } catch (...) {
            // Promise might already be satisfied or broken, ignore this
        }
        throw;  // Rethrow the original exception
    }
}

void Promise<void>::setException(std::exception_ptr exception) noexcept(false) {
    if (isCancelled()) {
        THROW_PROMISE_CANCELLED_EXCEPTION(
            "Cannot set exception, promise was cancelled.");
    }

    if (completed_.exchange(true)) {
        THROW_PROMISE_CANCELLED_EXCEPTION(
            "Cannot set exception, promise was already completed.");
    }

    if (!exception) {
        exception = std::make_exception_ptr(std::invalid_argument(
            "Null exception pointer passed to setException"));
    }

    try {
        promise_.set_exception(exception);
        runCallbacks();  // Execute callbacks
    } catch (const std::exception&) {
        // Promise might already be satisfied or broken
        throw;  // Propagate the exception
    }
}

template <typename F>
    requires VoidCallbackInvocable<F>
void Promise<void>::onComplete(F&& func) {
    // First check if cancelled without acquiring the lock for better
    // performance
    if (isCancelled()) {
        return;  // No callbacks should be added if the promise is cancelled
    }

    bool shouldRunCallback = false;
    {
#ifdef ATOM_USE_BOOST_LOCKFREE
        // Lock-free queue implementation
        auto* wrapper = new CallbackWrapper(std::forward<F>(func));
        callbacks_.push(wrapper);

        shouldRunCallback =
            future_.valid() && future_.wait_for(std::chrono::seconds(0)) ==
                                   std::future_status::ready;
#else
        std::unique_lock lock(mutex_);
        if (isCancelled()) {
            return;  // Double-check after acquiring the lock
        }

        // Store callback
        callbacks_.emplace_back(std::forward<F>(func));

        // Check if we should run the callback immediately
        shouldRunCallback =
            future_.valid() && future_.wait_for(std::chrono::seconds(0)) ==
                                   std::future_status::ready;
#endif
    }

    // Run callback outside the lock if needed
    if (shouldRunCallback) {
        try {
            future_.get();
#ifdef ATOM_USE_BOOST_LOCKFREE
            // For lock-free queue, we need to handle callback execution
            // manually
            CallbackWrapper* wrapper = nullptr;
            while (callbacks_.pop(wrapper)) {
                if (wrapper && wrapper->callback) {
                    try {
                        wrapper->callback();
                    } catch (...) {
                        // Ignore exceptions in callbacks
                    }
                    delete wrapper;
                }
            }
#else
            func();
#endif
        } catch (...) {
            // Ignore exceptions from callback execution after the fact
        }
    }
}

void Promise<void>::setCancellable(std::stop_token stopToken) {
    if (stopToken.stop_possible()) {
        setupCancellationHandler(stopToken);
    }
}

void Promise<void>::setupCancellationHandler(std::stop_token token) {
    // Use jthread to automatically manage the cancellation handler
    cancellationThread_.emplace([this, token](std::stop_token localToken) {
        std::stop_callback callback(token, [this]() { cancel(); });

        // Wait until the local token is stopped or the promise is completed
        while (!localToken.stop_requested() && !completed_.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}

[[nodiscard]] bool Promise<void>::cancel() noexcept {
    bool expectedValue = false;
    const bool wasCancelled =
        cancelled_.compare_exchange_strong(expectedValue, true);

    if (wasCancelled) {
        // Only try to set exception if we were the ones who cancelled it
        try {
            // Fix: Use string to construct PromiseCancelledException
            promise_.set_exception(std::make_exception_ptr(
                PromiseCancelledException("Promise was explicitly cancelled")));
        } catch (...) {
            // Promise might already have a value or exception, ignore this
        }

        // Clear any pending callbacks
#ifdef ATOM_USE_BOOST_LOCKFREE
        // Clean up lock-free queue
        CallbackWrapper* wrapper = nullptr;
        while (callbacks_.pop(wrapper)) {
            delete wrapper;
        }
#else
        std::unique_lock lock(mutex_);
        callbacks_.clear();
#endif
    }

    return wasCancelled;
}

[[nodiscard]] auto Promise<void>::isCancelled() const noexcept -> bool {
    return cancelled_.load(std::memory_order_acquire);
}

[[nodiscard]] auto Promise<void>::getFuture() const noexcept
    -> std::shared_future<void> {
    return future_;
}

[[nodiscard]] auto Promise<void>::operator co_await() const noexcept {
    return PromiseAwaiter<void>(future_);
}

[[nodiscard]] auto Promise<void>::getAwaiter() noexcept
    -> PromiseAwaiter<void> {
    return PromiseAwaiter<void>(future_);
}

void Promise<void>::runCallbacks() noexcept {
    if (isCancelled()) {
        return;
    }

#ifdef ATOM_USE_BOOST_LOCKFREE
    // Lock-free queue version
    if (callbacks_.empty())
        return;

    if (future_.valid() && future_.wait_for(std::chrono::seconds(0)) ==
                               std::future_status::ready) {
        try {
            future_.get();  // Check for exceptions
            CallbackWrapper* wrapper = nullptr;
            while (callbacks_.pop(wrapper)) {
                if (wrapper && wrapper->callback) {
                    try {
                        wrapper->callback();
                    } catch (...) {
                        // Ignore exceptions in callbacks
                    }
                    delete wrapper;
                }
            }
        } catch (...) {
            // Handle the case where the future contains an exception
            // Clean up callbacks but do not execute
            CallbackWrapper* wrapper = nullptr;
            while (callbacks_.pop(wrapper)) {
                delete wrapper;
            }
        }
    }
#else
    // Make a local copy of callbacks to avoid holding the lock while executing
    // them
    std::vector<std::function<void()>> localCallbacks;
    {
        std::shared_lock lock(mutex_);
        if (callbacks_.empty())
            return;
        localCallbacks = std::move(callbacks_);
        callbacks_.clear();
    }

    if (future_.valid() && future_.wait_for(std::chrono::seconds(0)) ==
                               std::future_status::ready) {
        try {
            future_.get();  // Check for exceptions
            for (auto& callback : localCallbacks) {
                try {
                    callback();
                } catch (...) {
                    // Ignore exceptions from callbacks
                    // In a production system, you might want to log these
                }
            }
        } catch (...) {
            // Handle the case where the future contains an exception.
            // We don't invoke callbacks in this case.
        }
    }
#endif
}

}  // namespace atom::async
