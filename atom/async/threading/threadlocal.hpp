/*
 * threadlocal_optimized.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-5-21

Description: Enhanced ThreadLocal with C++20 features

**************************************************/

#ifndef ATOM_ASYNC_THREADLOCAL_OPTIMIZED_HPP
#define ATOM_ASYNC_THREADLOCAL_OPTIMIZED_HPP

#include <algorithm>  // For algorithm support
#include <concepts>
#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <source_location>  // For enhanced exception information
#include <stdexcept>
#include <string_view>  // For more efficient string handling
#include <thread>
#include <type_traits>  // For enhanced type traits checking
#include <unordered_map>
#include <utility>

#include "atom/type/noncopyable.hpp"

namespace atom::async {

// Enhanced concept constraint, stricter than the original ThreadLocalStorable
template <typename T>
concept EnhancedThreadLocalStorable =
    std::default_initializable<T> && std::move_constructible<T> &&
    std::is_nothrow_move_constructible_v<T> &&  // Ensures move constructor does
                                                // not throw exceptions
    std::is_nothrow_destructible_v<T>;  // Ensures destructor does not throw
                                        // exceptions

// Enhanced error handling
enum class ThreadLocalError {
    NoInitializer,         // No initializer provided
    InitializationFailed,  // Initialization failed
    ValueNotFound,         // Value not found
    OperationFailed        // Operation failed
};

// Error information wrapper class
class ThreadLocalException : public std::runtime_error {
public:
    ThreadLocalException(
        ThreadLocalError error, std::string_view message,
        const std::source_location& location = std::source_location::current())
        : std::runtime_error(std::string(message)),
          error_(error),
          function_(location.function_name()),
          file_(location.file_name()),
          line_(location.line()) {}

    [[nodiscard]] ThreadLocalError error() const noexcept { return error_; }
    [[nodiscard]] const char* function() const noexcept { return function_; }
    [[nodiscard]] const char* file() const noexcept { return file_; }
    [[nodiscard]] int line() const noexcept { return line_; }

private:
    ThreadLocalError error_;
    const char* function_;
    const char* file_;
    int line_;
};

/**
 * @brief An enhanced thread-local storage class that provides thread-specific
 * storage for objects.
 *
 * This class allows each thread to maintain its own independent instance of
 * type T, supporting optional initialization, automatic cleanup, and various
 * access and operation methods. Performance optimized and feature-enhanced
 * based on C++20 features.
 *
 * @tparam T The type of the value to be stored in thread-local storage
 */
template <EnhancedThreadLocalStorable T>
class EnhancedThreadLocal : public NonCopyable {
public:
    // Type definitions, adding support for multiple initialization functions
    using InitializerFn = std::function<T()>;
    using ConditionalInitializerFn =
        std::function<std::optional<T>()>;  // Initializer that may return an
                                            // empty value
    using ThreadIdInitializerFn =
        std::function<T(std::thread::id)>;  // Initializer based on thread ID
    using CleanupFn = std::function<void(T&)>;  // Cleanup function

    /**
     * @brief Thread-local value wrapper, supporting multiple access and
     * operation methods
     *
     * This class provides a rich interface for thread-local values, including
     * conditional access, transformation operations, etc. Compared to directly
     * accessing the raw value, it offers safer and more convenient operations.
     */
    class ValueWrapper {
    public:
        explicit ValueWrapper(T& value) : value_(value) {}

        // Get reference
        [[nodiscard]] T& get() noexcept { return value_; }
        [[nodiscard]] const T& get() const noexcept { return value_; }

        // Apply a function to the value and return the result
        template <typename Func>
            requires std::invocable<Func, T&>
        auto apply(Func&& func) -> std::invoke_result_t<Func, T&> {
            return std::forward<Func>(func)(value_);
        }

        // Apply a function to the value (const version)
        template <typename Func>
            requires std::invocable<Func, const T&>
        auto apply(Func&& func) const -> std::invoke_result_t<Func, const T&> {
            return std::forward<Func>(func)(value_);
        }

        // Transform the value and return a new value
        template <typename Func>
            requires std::invocable<Func, T&> &&
                     std::convertible_to<std::invoke_result_t<Func, T&>, T>
        T transform(Func&& func) {
            return std::forward<Func>(func)(value_);
        }

        // Operator -> for member access
        T* operator->() noexcept { return &value_; }
        const T* operator->() const noexcept { return &value_; }

        // Dereference operator
        T& operator*() noexcept { return value_; }
        const T& operator*() const noexcept { return value_; }

    private:
        T& value_;
    };

    /**
     * @brief Default constructor
     *
     * Creates a ThreadLocal instance without an initializer.
     */
    EnhancedThreadLocal() noexcept = default;

    /**
     * @brief Constructs a ThreadLocal instance with an initializer function
     *
     * @param initializer Function called to initialize the value on first
     * access
     * @param cleanup Optional cleanup function, called when the value is
     * removed or the thread terminates
     */
    explicit EnhancedThreadLocal(InitializerFn initializer,
                                 CleanupFn cleanup = nullptr) noexcept
        : initializer_(std::move(initializer)), cleanup_(std::move(cleanup)) {}

    /**
     * @brief Constructs a ThreadLocal instance with a conditional initializer
     * function
     *
     * Provides an initializer that may return an empty value. If it returns
     * empty, no value is created for that thread.
     *
     * @param conditionalInitializer Initialization function that returns
     * std::optional<T>
     * @param cleanup Optional cleanup function, called when the value is
     * removed or the thread terminates
     */
    explicit EnhancedThreadLocal(
        ConditionalInitializerFn conditionalInitializer,
        CleanupFn cleanup = nullptr) noexcept
        : conditionalInitializer_(std::move(conditionalInitializer)),
          cleanup_(std::move(cleanup)) {}

    /**
     * @brief Constructs a ThreadLocal instance with a thread ID-based
     * initializer function
     *
     * @param threadIdInitializer Initialization function that accepts a thread
     * ID and returns the corresponding value
     * @param cleanup Optional cleanup function, called when the value is
     * removed or the thread terminates
     */
    explicit EnhancedThreadLocal(ThreadIdInitializerFn threadIdInitializer,
                                 CleanupFn cleanup = nullptr) noexcept
        : threadIdInitializer_(std::move(threadIdInitializer)),
          cleanup_(std::move(cleanup)) {}

    /**
     * @brief Constructs a ThreadLocal instance with a default value
     *
     * @param defaultValue Default value for all threads
     */
    explicit EnhancedThreadLocal(T defaultValue)
        : initializer_([value = std::move(defaultValue)]() { return value; }) {}

    // Move constructor
    EnhancedThreadLocal(EnhancedThreadLocal&&) noexcept = default;

    // Move assignment operator
    auto operator=(EnhancedThreadLocal&&) noexcept
        -> EnhancedThreadLocal& = default;

    /**
     * @brief Destructor, responsible for cleaning up all thread values
     */
    ~EnhancedThreadLocal() noexcept {
        try {
            std::unique_lock lock(mutex_);
            if (cleanup_) {
                for (auto& [tid, value_opt] : values_) {
                    if (value_opt.has_value()) {
                        cleanup_(value_opt.value());
                    }
                }
            }
            values_.clear();
        } catch (...) {
            // Ignore exceptions during cleanup
        }
    }

    /**
     * @brief Gets the value for the current thread
     *
     * If the value is not yet initialized, the initializer function is called.
     *
     * @return Reference to the thread-local value
     * @throws ThreadLocalException If no initializer is available and the value
     * has not been set
     */
    auto get() -> T& {
        auto tid = std::this_thread::get_id();
        std::unique_lock lock(mutex_);

        // Try to get or create the value
        auto [it, inserted] = values_.try_emplace(tid);
        if (inserted || !it->second.has_value()) {
            if (initializer_) {
                try {
                    it->second = std::make_optional(initializer_());
                } catch (const std::exception& e) {
                    values_.erase(tid);
                    throw ThreadLocalException(
                        ThreadLocalError::InitializationFailed,
                        std::string(
                            "Failed to initialize thread-local value: ") +
                            e.what());
                }
            } else if (conditionalInitializer_) {
                try {
                    it->second = conditionalInitializer_();
                    if (!it->second.has_value()) {
                        values_.erase(tid);
                        throw ThreadLocalException(
                            ThreadLocalError::InitializationFailed,
                            "Conditional initializer returned no value");
                    }
                } catch (const std::exception& e) {
                    values_.erase(tid);
                    throw ThreadLocalException(
                        ThreadLocalError::InitializationFailed,
                        std::string("Conditional initializer failed: ") +
                            e.what());
                }
            } else if (threadIdInitializer_) {
                try {
                    it->second = std::make_optional(threadIdInitializer_(tid));
                } catch (const std::exception& e) {
                    values_.erase(tid);
                    throw ThreadLocalException(
                        ThreadLocalError::InitializationFailed,
                        std::string("Thread ID initializer failed: ") +
                            e.what());
                }
            } else {
                values_.erase(tid);
                throw ThreadLocalException(ThreadLocalError::NoInitializer,
                                           "No initializer available for "
                                           "uninitialized thread-local value");
            }
        }

        return it->second.value();
    }

    /**
     * @brief Tries to get the value for the current thread
     *
     * Unlike get(), this method does not throw an exception but returns an
     * std::optional
     *
     * @return std::optional containing the thread-local value, or empty if it
     * doesn't exist
     */
    [[nodiscard]] auto tryGet() noexcept
        -> std::optional<std::reference_wrapper<T>> {
        try {
            auto tid = std::this_thread::get_id();
            std::shared_lock lock(mutex_);
            auto it = values_.find(tid);
            if (it != values_.end() && it->second.has_value()) {
                return std::ref(it->second.value());
            }
            return std::nullopt;
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Gets or creates the value for the current thread
     *
     * If the value does not exist, it is created using the provided factory
     * function
     *
     * @param factory Function to create the value
     * @return Reference to the thread-local value
     */
    template <typename Factory>
        requires std::invocable<Factory> &&
                 std::convertible_to<std::invoke_result_t<Factory>, T>
    auto getOrCreate(Factory&& factory) -> T& {
        auto tid = std::this_thread::get_id();
        std::unique_lock lock(mutex_);

        auto [it, inserted] = values_.try_emplace(tid);
        if (inserted || !it->second.has_value()) {
            try {
                it->second =
                    std::make_optional(std::forward<Factory>(factory)());
            } catch (const std::exception& e) {
                values_.erase(tid);
                throw ThreadLocalException(
                    ThreadLocalError::InitializationFailed,
                    std::string("Factory function failed: ") + e.what());
            }
        }

        return it->second.value();
    }

    /**
     * @brief Gets a wrapper for the current thread's value
     *
     * Returns a value wrapper that provides additional functionality
     *
     * @return ValueWrapper wrapping the current thread's value
     */
    auto getWrapper() -> ValueWrapper { return ValueWrapper(get()); }

    /**
     * @brief Accesses the thread-local value using the arrow operator
     *
     * @return Pointer to the thread-local value
     */
    auto operator->() -> T* {
        try {
            return &get();
        } catch (...) {
            return nullptr;
        }
    }

    /**
     * @brief Accesses the thread-local value using the arrow operator (const
     * version)
     *
     * @return Constant pointer to the thread-local value
     */
    auto operator->() const -> const T* {
        try {
            return &get();
        } catch (...) {
            return nullptr;
        }
    }

    /**
     * @brief Dereferences the thread-local value
     *
     * @return Reference to the thread-local value
     */
    auto operator*() -> T& { return get(); }

    /**
     * @brief Dereferences the thread-local value (const version)
     *
     * @return Constant reference to the thread-local value
     */
    auto operator*() const -> const T& { return get(); }

    /**
     * @brief Resets the value in thread-local storage
     *
     * If a value is provided, it is set as the thread-local value, otherwise it
     * is reset to the default constructed value.
     *
     * @param value The value to set, defaults to T()
     */
    void reset(T value = T()) noexcept {
        try {
            auto tid = std::this_thread::get_id();
            std::unique_lock lock(mutex_);

            // If a cleanup function is configured and there is an old value,
            // call the cleanup function
            auto it = values_.find(tid);
            if (cleanup_ && it != values_.end() && it->second.has_value()) {
                cleanup_(it->second.value());
            }

            values_[tid] = std::make_optional(std::move(value));
        } catch (...) {
            // Maintain strong exception safety guarantee
        }
    }

    /**
     * @brief Checks if the current thread has a value
     *
     * @return true if the current thread has an initialized value, false
     * otherwise
     */
    [[nodiscard]] auto hasValue() const noexcept -> bool {
        try {
            auto tid = std::this_thread::get_id();
            std::shared_lock lock(mutex_);
            auto it = values_.find(tid);
            return it != values_.end() && it->second.has_value();
        } catch (...) {
            return false;
        }
    }

    /**
     * @brief Gets a pointer to the thread-local value
     *
     * Returns nullptr if the value has not been initialized.
     *
     * @return Pointer to the thread-local value
     */
    [[nodiscard]] auto getPointer() noexcept -> T* {
        try {
            auto tid = std::this_thread::get_id();
            std::shared_lock lock(mutex_);
            auto it = values_.find(tid);
            return it != values_.end() && it->second.has_value()
                       ? &it->second.value()
                       : nullptr;
        } catch (...) {
            return nullptr;
        }
    }

    /**
     * @brief Gets a pointer to the thread-local value (const version)
     *
     * @return Constant pointer to the thread-local value
     */
    [[nodiscard]] auto getPointer() const noexcept -> const T* {
        try {
            auto tid = std::this_thread::get_id();
            std::shared_lock lock(mutex_);
            auto it = values_.find(tid);
            return it != values_.end() && it->second.has_value()
                       ? &it->second.value()
                       : nullptr;
        } catch (...) {
            return nullptr;
        }
    }

    /**
     * @brief Atomically compares and updates the thread-local value
     *
     * Updates to desired only if the current value equals expected.
     * This operation is atomic and suitable for scenarios requiring
     * coordination of multi-threaded operations.
     *
     * @param expected The expected current value
     * @param desired The new value to set
     * @return true if the update was successful, false otherwise
     */
    template <typename U = T>
        requires std::equality_comparable_with<T, U>
    bool compareAndUpdate(const U& expected, T desired) noexcept {
        try {
            auto tid = std::this_thread::get_id();
            std::unique_lock lock(mutex_);

            auto it = values_.find(tid);
            if (it != values_.end() && it->second.has_value() &&
                it->second.value() == expected) {
                if (cleanup_) {
                    cleanup_(it->second.value());
                }
                it->second = std::make_optional(std::move(desired));
                return true;
            }
            return false;
        } catch (...) {
            return false;
        }
    }

    /**
     * @brief Updates the thread-local value using the provided transformation
     * function
     *
     * @tparam Func Transformation function type
     * @param func Function that accepts the current value and returns a new
     * value
     * @return true if successfully updated, false otherwise
     */
    template <typename Func>
        requires std::invocable<Func, T&> &&
                 std::convertible_to<std::invoke_result_t<Func, T&>, T>
    bool update(Func&& func) noexcept {
        try {
            auto tid = std::this_thread::get_id();
            std::unique_lock lock(mutex_);

            auto it = values_.find(tid);
            if (it != values_.end() && it->second.has_value()) {
                T oldValue = std::move(it->second.value());
                if (cleanup_) {
                    cleanup_(oldValue);
                }

                it->second =
                    std::make_optional(std::forward<Func>(func)(oldValue));
                return true;
            }
            return false;
        } catch (...) {
            return false;
        }
    }

    /**
     * @brief Executes a function for each thread-local value
     *
     * Provides a function that will be called to process the initialized value
     * for each thread.
     *
     * @tparam Func A callable type (e.g., lambda or function pointer)
     * @param func Function to execute for each thread-local value
     */
    template <typename Func>
        requires std::invocable<Func, T&, std::thread::id>
    void forEachWithId(Func&& func) {
        try {
            std::shared_lock lock(mutex_);
            for (auto& [tid, value_opt] : values_) {
                if (value_opt.has_value()) {
                    std::forward<Func>(func)(value_opt.value(), tid);
                }
            }
        } catch (const std::exception& e) {
            // Log error but do not throw from forEach
        }
    }

    /**
     * @brief Executes a function for each thread-local value
     *
     * Provides a function that will be called to process the initialized value
     * for each thread.
     *
     * @tparam Func A callable type (e.g., lambda or function pointer)
     * @param func Function to execute for each thread-local value
     */
    template <std::invocable<T&> Func>
    void forEach(Func&& func) {
        try {
            std::shared_lock lock(mutex_);
            for (auto& [tid, value_opt] : values_) {
                if (value_opt.has_value()) {
                    std::forward<Func>(func)(value_opt.value());
                }
            }
        } catch (const std::exception& e) {
            // Log error but do not throw from forEach
        }
    }

    /**
     * @brief Finds the first thread value that satisfies the given condition
     *
     * @tparam Predicate Predicate function type
     * @param pred Predicate used to test values
     * @return An optional reference containing the found value, or empty if not
     * found
     */
    template <typename Predicate>
        requires std::predicate<Predicate, T&>
    [[nodiscard]] auto findIf(Predicate&& pred) noexcept
        -> std::optional<std::reference_wrapper<T>> {
        try {
            std::shared_lock lock(mutex_);
            for (auto& [tid, value_opt] : values_) {
                if (value_opt.has_value() &&
                    std::forward<Predicate>(pred)(value_opt.value())) {
                    return std::ref(value_opt.value());
                }
            }
            return std::nullopt;
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * @brief Clears thread-local storage for all threads
     */
    void clear() noexcept {
        try {
            std::unique_lock lock(mutex_);

            // If a cleanup function is configured, call it for each value
            if (cleanup_) {
                for (auto& [tid, value_opt] : values_) {
                    if (value_opt.has_value()) {
                        cleanup_(value_opt.value());
                    }
                }
            }

            values_.clear();
        } catch (...) {
            // Ignore exceptions during cleanup
        }
    }

    /**
     * @brief Clears thread-local storage for the current thread
     */
    void clearCurrentThread() noexcept {
        try {
            auto tid = std::this_thread::get_id();
            std::unique_lock lock(mutex_);

            auto it = values_.find(tid);
            if (it != values_.end()) {
                if (cleanup_ && it->second.has_value()) {
                    cleanup_(it->second.value());
                }
                values_.erase(it);
            }
        } catch (...) {
            // Ignore exceptions during cleanup
        }
    }

    /**
     * @brief Removes all thread values that satisfy the given condition
     *
     * @tparam Predicate Predicate function type
     * @param pred Predicate used to test values
     * @return The number of values removed
     */
    template <typename Predicate>
        requires std::predicate<Predicate, T&>
    std::size_t removeIf(Predicate&& pred) noexcept {
        try {
            std::unique_lock lock(mutex_);
            std::size_t removedCount = 0;

            // Use stable iteration to remove matching elements
            for (auto it = values_.begin(); it != values_.end();) {
                if (it->second.has_value() &&
                    std::forward<Predicate>(pred)(it->second.value())) {
                    if (cleanup_) {
                        cleanup_(it->second.value());
                    }
                    it = values_.erase(it);
                    ++removedCount;
                } else {
                    ++it;
                }
            }

            return removedCount;
        } catch (...) {
            return 0;
        }
    }

    /**
     * @brief Gets the number of stored thread values
     *
     * @return The number of currently stored thread values
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t {
        try {
            std::shared_lock lock(mutex_);
            return values_.size();
        } catch (...) {
            return 0;
        }
    }

    /**
     * @brief Checks if the storage is empty
     *
     * @return true if there are no stored thread values, false otherwise
     */
    [[nodiscard]] auto empty() const noexcept -> bool {
        try {
            std::shared_lock lock(mutex_);
            return values_.empty();
        } catch (...) {
            return true;
        }
    }

    /**
     * @brief Sets or updates the cleanup function
     *
     * @param cleanup New cleanup function to be called when a value is removed
     */
    void setCleanupFunction(CleanupFn cleanup) noexcept {
        cleanup_ = std::move(cleanup);
    }

    /**
     * @brief Checks if the specified thread has a value
     *
     * @param tid Thread ID to check
     * @return true if the specified thread has an initialized value, false
     * otherwise
     */
    [[nodiscard]] auto hasValueForThread(std::thread::id tid) const noexcept
        -> bool {
        try {
            std::shared_lock lock(mutex_);
            auto it = values_.find(tid);
            return it != values_.end() && it->second.has_value();
        } catch (...) {
            return false;
        }
    }

private:
    InitializerFn initializer_;  ///< Function to initialize T
    ConditionalInitializerFn
        conditionalInitializer_;  ///< Conditional initialization function
    ThreadIdInitializerFn
        threadIdInitializer_;  ///< Thread ID-based initialization function
    CleanupFn cleanup_;        ///< Cleanup function when value is removed
    mutable std::shared_mutex mutex_;  ///< Mutex for thread-safe access
    std::unordered_map<std::thread::id, std::optional<T>>
        values_;  ///< Stores values by thread ID
};

// Alias using EnhancedThreadLocal as the default implementation
template <typename T>
using ThreadLocal = EnhancedThreadLocal<T>;

}  // namespace atom::async

#endif  // ATOM_ASYNC_THREADLOCAL_OPTIMIZED_HPP
