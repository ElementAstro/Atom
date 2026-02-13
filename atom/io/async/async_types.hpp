#ifndef ATOM_IO_ASYNC_ASYNC_TYPES_HPP
#define ATOM_IO_ASYNC_ASYNC_TYPES_HPP

#include <atomic>
#include <chrono>
#include <concepts>
#include <coroutine>
#include <filesystem>
#include <future>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>

#include "atom/async/core/async.hpp"
#include "atom/async/pool.hpp"

#ifdef ATOM_USE_ASIO
#include <asio.hpp>
#endif

namespace atom::io::async {

/**
 * @brief Concept for valid path string types
 */
template <typename T>
concept PathString = std::convertible_to<T, std::string> ||
                     std::convertible_to<T, std::filesystem::path> ||
                     std::convertible_to<T, std::string_view>;

/**
 * @brief Concept for types that can be used as file content
 */
template <typename T>
concept FileContent =
    std::ranges::contiguous_range<T> &&
    (std::same_as<std::ranges::range_value_t<T>, char> ||
     std::same_as<std::ranges::range_value_t<T>, unsigned char> ||
     std::same_as<std::ranges::range_value_t<T>, std::byte>);

/**
 * @brief Concept for types that can be used as file permissions
 */
template <typename T>
concept FilePermissions = std::same_as<T, std::filesystem::perms> ||
                          std::convertible_to<T, std::filesystem::perms>;

/**
 * @brief Concept for types that represent file sizes
 */
template <typename T>
concept FileSizeType = std::integral<T> && std::unsigned_integral<T>;

/**
 * @brief Concept for types that can be used as buffer data
 */
template <typename T>
concept BufferData =
    std::ranges::contiguous_range<T> &&
    std::is_trivially_copyable_v<std::ranges::range_value_t<T>>;

/**
 * @brief Concept for types that can be used as compression options
 */
template <typename T>
concept CompressionOptionsType = requires(T t) {
    { t.compression_level } -> std::convertible_to<int>;
    { t.enable_checksum } -> std::convertible_to<bool>;
};

/**
 * @brief Context for managing async operations with cancellation support
 */
class AsyncContext {
public:
    AsyncContext() = default;

    /**
     * @brief Checks if the context has been cancelled
     * @return True if cancelled, false otherwise
     */
    [[nodiscard]] bool is_cancelled() const noexcept {
        return cancelled_.load();
    }

    /**
     * @brief Cancels all operations using this context
     */
    void cancel() noexcept { cancelled_.store(true); }

    /**
     * @brief Resets the cancellation state
     */
    void reset() noexcept { cancelled_.store(false); }

private:
    std::atomic<bool> cancelled_{false};
};

/**
 * @brief Result type for async operations with enhanced error handling
 */
template <typename T>
struct AsyncResult {
    bool success{false};
    std::string error_message;
    T value{};

    /**
     * @brief Creates a successful result
     */
    static AsyncResult<T> success_result(T&& val) {
        AsyncResult<T> result;
        result.success = true;
        result.value = std::move(val);
        return result;
    }

    /**
     * @brief Creates a failed result
     */
    static AsyncResult<T> error_result(std::string_view error) {
        AsyncResult<T> result;
        result.success = false;
        result.error_message = error;
        return result;
    }
};

template <>
struct AsyncResult<void> {
    bool success{false};
    std::string error_message;

    /**
     * @brief Creates a successful result
     */
    static AsyncResult<void> success_result() {
        AsyncResult<void> result;
        result.success = true;
        return result;
    }

    /**
     * @brief Creates a failed result
     */
    static AsyncResult<void> error_result(std::string_view error) {
        AsyncResult<void> result;
        result.success = false;
        result.error_message = error;
        return result;
    }
};

/**
 * @brief Concept for callback functions that can handle AsyncResult
 */
template <typename F, typename T>
concept AsyncResultCallback = std::invocable<F, AsyncResult<T>>;

// Use the existing high-performance thread pool from atom::async namespace
using ThreadPool = atom::async::ThreadPool;

/**
 * @brief High-performance coroutine Task implementation with cancellation
 * support
 */
template <typename T>
class [[nodiscard]] Task {
public:
    struct promise_type {
        std::promise<T> promise;
        std::weak_ptr<AsyncContext> context;

        Task get_return_object() noexcept {
            return Task(promise.get_future(), context.lock());
        }

        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }

        void return_value(T value) noexcept {
            if (auto ctx = context.lock(); !ctx || !ctx->is_cancelled()) {
                promise.set_value(std::move(value));
            }
        }

        void unhandled_exception() noexcept {
            try {
                std::rethrow_exception(std::current_exception());
            } catch (const std::exception& e) {
                T failed_result;
                if constexpr (std::is_same_v<T, AsyncResult<void>>) {
                    failed_result = AsyncResult<void>::error_result(e.what());
                } else {
                    failed_result =
                        AsyncResult<typename T::value_type>::error_result(
                            e.what());
                }
                promise.set_value(std::move(failed_result));
            }
        }

        void set_context(std::shared_ptr<AsyncContext> ctx) { context = ctx; }
    };

    explicit Task(std::future<T> future,
                  std::shared_ptr<AsyncContext> ctx = nullptr) noexcept
        : future_(std::move(future)), context_(std::move(ctx)) {}

    /**
     * @brief Gets the result, blocking if necessary
     * @return The task result
     */
    T get() { return future_.get(); }

    /**
     * @brief Checks if the task is ready without blocking
     * @return True if ready, false otherwise
     */
    [[nodiscard]] bool is_ready() const noexcept {
        return future_.wait_for(std::chrono::seconds(0)) ==
               std::future_status::ready;
    }

    /**
     * @brief Waits for the task to complete with timeout
     * @param timeout Maximum time to wait
     * @return Future status
     */
    template <typename Rep, typename Period>
    [[nodiscard]] std::future_status wait_for(
        const std::chrono::duration<Rep, Period>& timeout) const {
        return future_.wait_for(timeout);
    }

    /**
     * @brief Cancels the task if context is available
     */
    void cancel() {
        if (context_) {
            context_->cancel();
        }
    }

    /**
     * @brief Checks if the task is cancelled
     * @return True if cancelled, false otherwise
     */
    [[nodiscard]] bool is_cancelled() const noexcept {
        return context_ && context_->is_cancelled();
    }

private:
    std::future<T> future_;
    std::shared_ptr<AsyncContext> context_;
};

}  // namespace atom::io::async

#endif  // ATOM_IO_ASYNC_ASYNC_TYPES_HPP
