/*
 * @file threadlocal_optimized.hpp
 *
 * @brief Enhanced ThreadLocal with C++20 features
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * @date 2025-5-21
 *
 * @details A high-performance thread-local storage class that provides
 * thread-specific storage for objects. This class allows each thread to
 * maintain its own independent instance of type T, supporting optional
 * initialization, automatic cleanup, and various access and operation methods.
 * Performance optimized and feature-enhanced based on C++20 features.
 */

#ifndef ATOM_ASYNC_THREADLOCAL_OPTIMIZED_HPP
#define ATOM_ASYNC_THREADLOCAL_OPTIMIZED_HPP

#include <algorithm>  // For algorithm support (e.g., std::find if needed, though not currently used in map approach)
#include <concepts>
#include <functional>
#include <mutex>   // Required for std::unique_lock
#include <optional>
#include <shared_mutex>     // Required for std::shared_mutex, std::shared_lock
#include <source_location>  // For enhanced exception information
#include <stdexcept>
#include <string_view>  // For more efficient string handling
#include <thread>
#include <type_traits>  // For enhanced type traits checking
#include <unordered_map>
#include <utility>

#include "atom/type/noncopyable.hpp"

// Platform-specific includes for advanced features
#if defined(_WIN32)
#include <processthreadsapi.h>
#include <windows.h>
#elif defined(__linux__)
#include <linux/futex.h>
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach/thread_act.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#include <sched.h>
#endif

namespace atom::async {

/**
 * @brief Cache line size for false sharing prevention
 */
inline constexpr std::size_t CACHE_LINE_SIZE = 64;

/**
 * @brief Alignas for cache line optimization
 * @tparam T The type to align.
 */
template <typename T>
struct alignas(CACHE_LINE_SIZE) CacheAligned {
    T value;

    /**
     * @brief Constructs a CacheAligned object.
     * @tparam Args Argument types for the contained value's constructor.
     * @param args Arguments to forward to the contained value's constructor.
     */
    template <typename... Args>
    explicit CacheAligned(Args&&... args)
        : value(std::forward<Args>(args)...) {}

    /**
     * @brief Implicit conversion to a reference to the contained value.
     * @return Reference to the contained value.
     */
    operator T&() noexcept { return value; }

    /**
     * @brief Implicit conversion to a const reference to the contained value.
     * @return Const reference to the contained value.
     */
    operator const T&() const noexcept { return value; }
};

/**
 * @brief Enhanced concept constraint for types storable in EnhancedThreadLocal.
 *
 * Stricter than a basic storable concept, requiring default constructibility,
 * move constructibility, nothrow move constructibility, and nothrow
 * destructibility.
 * @tparam T The type to check.
 */
template <typename T>
concept EnhancedThreadLocalStorable =
    std::default_initializable<T> && std::move_constructible<T> &&
    std::is_nothrow_move_constructible_v<T> &&  // Ensures move constructor does
                                                // not throw exceptions
    std::is_nothrow_destructible_v<T>;  // Ensures destructor does not throw
                                        // exceptions

/**
 * @brief Enhanced error handling enumeration for ThreadLocal operations.
 */
enum class ThreadLocalError {
    NoInitializer,         ///< No initializer provided
    InitializationFailed,  ///< Initialization failed
    ValueNotFound,         ///< Value not found
    OperationFailed        ///< Operation failed
};

/**
 * @brief Error information wrapper class for ThreadLocal exceptions.
 */
class ThreadLocalException : public std::runtime_error {
public:
    /**
     * @brief Constructs a ThreadLocalException.
     * @param error The specific error code.
     * @param message A descriptive error message.
     * @param location The source location where the exception occurred.
     */
    ThreadLocalException(
        ThreadLocalError error, std::string_view message,
        const std::source_location& location = std::source_location::current())
        : std::runtime_error(std::string(message)),
          error_(error),
          function_(location.function_name()),
          file_(location.file_name()),
          line_(location.line()) {}

    /**
     * @brief Gets the error code.
     * @return The ThreadLocalError code.
     */
    [[nodiscard]] ThreadLocalError error() const noexcept { return error_; }

    /**
     * @brief Gets the function name where the exception occurred.
     * @return The function name.
     */
    [[nodiscard]] const char* function() const noexcept { return function_; }

    /**
     * @brief Gets the file name where the exception occurred.
     * @return The file name.
     */
    [[nodiscard]] const char* file() const noexcept { return file_; }

    /**
     * @brief Gets the line number where the exception occurred.
     * @return The line number.
     */
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
    /** @name Type Definitions */
    ///@{
    /**
     * @brief Function type for standard initialization.
     */
    using InitializerFn = std::function<T()>;
    /**
     * @brief Function type for conditional initialization (may return empty).
     */
    using ConditionalInitializerFn = std::function<std::optional<T>()>;
    /**
     * @brief Function type for thread ID-based initialization.
     */
    using ThreadIdInitializerFn = std::function<T(std::thread::id)>;
    /**
     * @brief Function type for cleanup when a value is removed.
     */
    using CleanupFn = std::function<void(T&)>;
    ///@}

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
        /**
         * @brief Constructs a ValueWrapper.
         * @param value The thread-local value to wrap.
         */
        explicit ValueWrapper(T& value) : value_(value) {}

        /**
         * @brief Gets a reference to the contained value.
         * @return Reference to the contained value.
         */
        [[nodiscard]] T& get() noexcept { return value_; }

        /**
         * @brief Gets a const reference to the contained value.
         * @return Const reference to the contained value.
         */
        [[nodiscard]] const T& get() const noexcept { return value_; }

        /**
         * @brief Applies a function to the value and returns the result.
         * @tparam Func The type of the function to apply.
         * @param func The function to apply.
         * @return The result of applying the function.
         */
        template <typename Func>
            requires std::invocable<Func, T&>
        auto apply(Func&& func) -> std::invoke_result_t<Func, T&> {
            return std::forward<Func>(func)(value_);
        }

        /**
         * @brief Applies a function to the value (const version).
         * @tparam Func The type of the function to apply.
         * @param func The function to apply.
         * @return The result of applying the function.
         */
        template <typename Func>
            requires std::invocable<Func, const T&>
        auto apply(Func&& func) const -> std::invoke_result_t<Func, const T&> {
            return std::forward<Func>(func)(value_);
        }

        /**
         * @brief Transforms the value and returns a new value.
         * @tparam Func The type of the transformation function.
         * @param func The transformation function.
         * @return The transformed value.
         */
        template <typename Func>
            requires std::invocable<Func, T&> &&
                     std::convertible_to<std::invoke_result_t<Func, T&>, T>
        T transform(Func&& func) {
            return std::forward<Func>(func)(value_);
        }

        /**
         * @brief Provides pointer-like access to the contained value.
         * @return Pointer to the contained value.
         */
        T* operator->() noexcept { return &value_; }

        /**
         * @brief Provides const pointer-like access to the contained value.
         * @return Const pointer to the contained value.
         */
        const T* operator->() const noexcept { return &value_; }

        /**
         * @brief Dereferences the wrapper to get a reference to the contained
         * value.
         * @return Reference to the contained value.
         */
        T& operator*() noexcept { return value_; }

        /**
         * @brief Dereferences the wrapper to get a const reference to the
         * contained value.
         * @return Const reference to the contained value.
         */
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
        : initializer_([value = std::move(defaultValue)]() { return value; }),
          cleanup_(nullptr) {}

    /**
     * @brief Move constructor.
     */
    EnhancedThreadLocal(EnhancedThreadLocal&&) noexcept = default;

    /**
     * @brief Move assignment operator.
     * @return Reference to the moved-to object.
     */
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
                        // Call cleanup function before destroying the value
                        cleanup_(value_opt.value());
                    }
                }
            }
            // The values_ map will be cleared automatically when the destructor
            // finishes
        } catch (...) {
            // Ignore exceptions during cleanup
        }
    }

    /**
     * @brief Gets or creates the value for the current thread using a factory
     * function.
     *
     * If the value does not exist, it is created using the provided factory
     * function. This method uses a shared_lock for the fast path (value already
     * exists) and upgrades to a unique_lock only when initialization is needed,
     * reducing contention.
     *
     * @tparam Factory The type of the factory function.
     * @param factory Function to create the value.
     * @return Reference to the thread-local value.
     * @throws ThreadLocalException If the factory function throws or returns an
     * invalid value.
     */
    template <typename Factory>
        requires std::invocable<Factory> &&
                 std::convertible_to<std::invoke_result_t<Factory>, T>
    auto getOrCreate(Factory&& factory) -> T& {
        auto tid = std::this_thread::get_id();

        // First, try with a shared lock (read access)
        {
            std::shared_lock lock(mutex_);
            auto it = values_.find(tid);
            if (it != values_.end() && it->second.has_value()) {
                return it->second
                    .value();  // Fast path: value exists and is initialized
            }
        }  // Release shared lock

        // Slow path: Value not found or not initialized. Need unique lock
        // (write access).
        std::unique_lock lock(mutex_);

        // Double-check under unique lock in case another thread initialized it
        auto [it, inserted] = values_.try_emplace(tid);
        if (!inserted && it->second.has_value()) {
            return it->second
                .value();  // Another thread initialized it concurrently
        }

        // Create the value using the factory
        std::exception_ptr ex_ptr = nullptr;
        try {
            it->second = std::make_optional(std::forward<Factory>(factory)());
        } catch (...) {
            ex_ptr = std::current_exception();
            values_.erase(it);  // Ensure entry is removed on exception
        }

        if (ex_ptr) {
            std::rethrow_exception(ex_ptr);
        }

        // Value should now be initialized and present
        return it->second.value();
    }

    /**
     * @brief Gets the value for the current thread.
     *
     * If the value is not yet initialized, the initializer function is called.
     * This method leverages getOrCreate for optimized access.
     *
     * @return Reference to the thread-local value.
     * @throws ThreadLocalException If no initializer is available and the value
     * has not been set, or if initialization fails.
     */
    auto get() -> T& {
        auto tid = std::this_thread::get_id();
        // Use getOrCreate with a factory that calls the appropriate initializer
        return getOrCreate([this, tid]() -> T {
            if (initializer_) {
                return initializer_();
            } else if (conditionalInitializer_) {
                auto opt_value = conditionalInitializer_();
                if (opt_value.has_value()) {
                    return std::move(opt_value.value());
                } else {
                    // Conditional initializer returned empty, throw here
                    throw ThreadLocalException(
                        ThreadLocalError::InitializationFailed,
                        "Conditional initializer returned no value");
                }
            } else if (threadIdInitializer_) {
                return threadIdInitializer_(tid);
            } else {
                // No initializer set, throw here
                throw ThreadLocalException(ThreadLocalError::NoInitializer,
                                           "No initializer available for "
                                           "uninitialized thread-local value");
            }
        });
    }

    /**
     * @brief Tries to get the value for the current thread
     *
     * Unlike get(), this method does not throw an exception but returns an
     * std::optional
     *
     * @return std::optional containing a reference to the thread-local value,
     * or empty if it doesn't exist.
     */
    [[nodiscard]] auto tryGet() noexcept
        -> std::optional<std::reference_wrapper<T>> {
        try {
            auto tid = std::this_thread::get_id();
            std::shared_lock lock(mutex_);  // Use shared_lock for read access
            auto it = values_.find(tid);
            if (it != values_.end() && it->second.has_value()) {
                return std::ref(it->second.value());
            }
            return std::nullopt;
        } catch (...) {
            // Catch potential exceptions from thread::get_id or map operations
            return std::nullopt;
        }
    }

    /**
     * @brief Gets a wrapper for the current thread's value
     *
     * Returns a value wrapper that provides additional functionality
     *
     * @return ValueWrapper wrapping the current thread's value.
     * @throws ThreadLocalException If the underlying get() operation throws.
     */
    auto getWrapper() -> ValueWrapper { return ValueWrapper(get()); }

    /**
     * @brief Accesses the thread-local value using the arrow operator
     *
     * @return Pointer to the thread-local value, or nullptr if get() throws.
     */
    auto operator->() -> T* {
        try {
            return &get();
        } catch (...) {
            return nullptr;  // Return nullptr on exception
        }
    }

    /**
     * @brief Accesses the thread-local value using the arrow operator (const
     * version)
     *
     * @return Constant pointer to the thread-local value, or nullptr if the
     * value is not initialized or an exception occurs.
     */
    auto operator->() const -> const T* {
        try {
            auto tid = std::this_thread::get_id();
            std::shared_lock lock(mutex_);
            auto it = values_.find(tid);
            return it != values_.end() && it->second.has_value()
                       ? &it->second.value()
                       : nullptr;
        } catch (...) {
            return nullptr;  // Return nullptr on exception
        }
    }

    /**
     * @brief Dereferences the thread-local value
     *
     * @return Reference to the thread-local value.
     * @throws ThreadLocalException If the underlying get() operation throws.
     */
    auto operator*() -> T& { return get(); }

    /**
     * @brief Dereferences the thread-local value (const version)
     *
     * @return Constant reference to the thread-local value.
     * @throws ThreadLocalException If the value is not initialized or an
     * exception occurs.
     */
    auto operator*() const -> const T& {
        auto tid = std::this_thread::get_id();
        std::shared_lock lock(mutex_);
        auto it = values_.find(tid);
        if (it != values_.end() && it->second.has_value()) {
            return it->second.value();
        }
        throw ThreadLocalException(
            ThreadLocalError::ValueNotFound,
            "Thread-local value not initialized for const access");
    }

    /**
     * @brief Resets the value in thread-local storage for the current thread.
     *
     * If a value is provided, it is set as the thread-local value, otherwise it
     * is reset to the default constructed value. Calls the cleanup function if
     * an old value exists.
     *
     * @param value The value to set, defaults to T().
     */
    void reset(T value = T()) noexcept {
        try {
            auto tid = std::this_thread::get_id();
            std::unique_lock lock(mutex_);  // Use unique_lock for write access

            // If a cleanup function is configured and there is an old value,
            // call the cleanup function
            auto it = values_.find(tid);
            if (it != values_.end()) {
                if (cleanup_ && it->second.has_value()) {
                    cleanup_(it->second.value());
                }
                // Update the existing entry
                it->second = std::make_optional(std::move(value));
            } else {
                // Insert a new entry
                values_[tid] = std::make_optional(std::move(value));
            }
        } catch (...) {
            // Ignore exceptions during reset to maintain noexcept guarantee
        }
    }

    /**
     * @brief Checks if the current thread has a value
     *
     * @return true if the current thread has an initialized value, false
     * otherwise.
     */
    [[nodiscard]] auto hasValue() const noexcept -> bool {
        try {
            auto tid = std::this_thread::get_id();
            std::shared_lock lock(mutex_);  // Use shared_lock for read access
            auto it = values_.find(tid);
            return it != values_.end() && it->second.has_value();
        } catch (...) {
            // Catch potential exceptions from thread::get_id or map operations
            return false;
        }
    }

    /**
     * @brief Gets a pointer to the thread-local value
     *
     * Returns nullptr if the value has not been initialized.
     *
     * @return Pointer to the thread-local value, or nullptr if not initialized
     * or an exception occurs.
     */
    [[nodiscard]] auto getPointer() noexcept -> T* {
        try {
            auto tid = std::this_thread::get_id();
            std::shared_lock lock(mutex_);  // Use shared_lock for read access
            auto it = values_.find(tid);
            return it != values_.end() && it->second.has_value()
                       ? &it->second.value()
                       : nullptr;
        } catch (...) {
            // Catch potential exceptions from thread::get_id or map operations
            return nullptr;
        }
    }

    /**
     * @brief Gets a pointer to the thread-local value (const version)
     *
     * @return Constant pointer to the thread-local value, or nullptr if not
     * initialized or an exception occurs.
     */
    [[nodiscard]] auto getPointer() const noexcept -> const T* {
        try {
            auto tid = std::this_thread::get_id();
            std::shared_lock lock(mutex_);  // Use shared_lock for read access
            auto it = values_.find(tid);
            return it != values_.end() && it->second.has_value()
                       ? &it->second.value()
                       : nullptr;
        } catch (...) {
            // Catch potential exceptions from thread::get_id or map operations
            return nullptr;
        }
    }

    /**
     * @brief Atomically compares and updates the thread-local value for the
     * current thread.
     *
     * Updates to desired only if the current value equals expected.
     * This operation is atomic with respect to other operations on *this*
     * EnhancedThreadLocal object, but not necessarily atomic with respect to
     * other operations on the value itself if T is not atomic.
     *
     * @tparam U The type to compare with T.
     * @param expected The expected current value.
     * @param desired The new value to set.
     * @return true if the update was successful, false otherwise.
     */
    template <typename U = T>
        requires std::equality_comparable_with<T, U>
    bool compareAndUpdate(const U& expected, T desired) noexcept {
        try {
            auto tid = std::this_thread::get_id();
            std::unique_lock lock(mutex_);  // Use unique_lock for write access

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
            // Ignore exceptions to maintain noexcept guarantee
            return false;
        }
    }

    /**
     * @brief Updates the thread-local value for the current thread using the
     * provided transformation function.
     *
     * @tparam Func Transformation function type.
     * @param func Function that accepts the current value by reference and
     * modifies it in place.
     * @return true if successfully updated, false otherwise (e.g., value not
     * found).
     */
    template <typename Func>
        requires std::invocable<Func, T&>
    bool update(Func&& func) noexcept {
        try {
            auto tid = std::this_thread::get_id();
            std::unique_lock lock(mutex_);  // Use unique_lock for write access

            auto it = values_.find(tid);
            if (it != values_.end() && it->second.has_value()) {
                T& currentValue = it->second.value();
                std::forward<Func>(func)(currentValue);  // Modify in place
                return true;
            }
            return false;
        } catch (...) {
            // Ignore exceptions to maintain noexcept guarantee
            return false;
        }
    }

    /**
     * @brief Executes a function for each thread-local value
     *
     * Provides a function that will be called to process the initialized value
     * for each thread. Iteration happens under a shared lock.
     *
     * @tparam Func A callable type (e.g., lambda or function pointer) that
     * accepts T&.
     * @param func Function to execute for each thread-local value.
     */
    template <std::invocable<T&> Func>
    void forEach(Func&& func) {
        try {
            std::shared_lock lock(
                mutex_);  // Use shared_lock for read access during iteration
            for (auto& [tid, value_opt] : values_) {
                if (value_opt.has_value()) {
                    std::forward<Func>(func)(value_opt.value());
                }
            }
        } catch (const std::exception& e) {
            // Ignore exceptions during iteration
        }
    }

    /**
     * @brief Executes a function for each thread-local value, providing the
     * thread ID.
     *
     * Provides a function that will be called to process the initialized value
     * for each thread. Iteration happens under a shared lock.
     *
     * @tparam Func A callable type (e.g., lambda or function pointer) that
     * accepts T& and std::thread::id.
     * @param func Function to execute for each thread-local value and its ID.
     */
    template <typename Func>
        requires std::invocable<Func, T&, std::thread::id>
    void forEachWithId(Func&& func) {
        try {
            std::shared_lock lock(
                mutex_);  // Use shared_lock for read access during iteration
            for (auto& [tid, value_opt] : values_) {
                if (value_opt.has_value()) {
                    std::forward<Func>(func)(value_opt.value(), tid);
                }
            }
        } catch (const std::exception& e) {
            // Ignore exceptions during iteration
        }
    }

    /**
     * @brief Finds the first thread value that satisfies the given condition
     *
     * @tparam Predicate Predicate function type that accepts T&.
     * @param pred Predicate used to test values.
     * @return An optional reference containing the found value, or empty if not
     * found or an exception occurs.
     */
    template <typename Predicate>
        requires std::predicate<Predicate, T&>
    [[nodiscard]] auto findIf(Predicate&& pred) noexcept
        -> std::optional<std::reference_wrapper<T>> {
        try {
            std::shared_lock lock(mutex_);  // Use shared_lock for read access
            for (auto& [tid, value_opt] : values_) {
                if (value_opt.has_value() &&
                    std::forward<Predicate>(pred)(value_opt.value())) {
                    return std::ref(value_opt.value());
                }
            }
            return std::nullopt;
        } catch (...) {
            // Catch potential exceptions from thread::get_id, map operations,
            // or predicate
            return std::nullopt;
        }
    }

    /**
     * @brief Clears thread-local storage for all threads.
     *
     * Calls the cleanup function for each value before removing it.
     */
    void clear() noexcept {
        try {
            std::unique_lock lock(mutex_);  // Use unique_lock for write access

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
     * @brief Clears thread-local storage for the current thread.
     *
     * Calls the cleanup function for the current thread's value if it exists.
     */
    void clearCurrentThread() noexcept {
        try {
            auto tid = std::this_thread::get_id();
            std::unique_lock lock(mutex_);  // Use unique_lock for write access

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
     * @brief Removes all thread values that satisfy the given condition.
     *
     * Calls the cleanup function for each removed value.
     *
     * @tparam Predicate Predicate function type that accepts T&.
     * @param pred Predicate used to test values.
     * @return The number of values removed.
     */
    template <typename Predicate>
        requires std::predicate<Predicate, T&>
    std::size_t removeIf(Predicate&& pred) noexcept {
        try {
            std::unique_lock lock(mutex_);  // Use unique_lock for write access
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
            // Ignore exceptions to maintain noexcept guarantee
            return 0;
        }
    }

    /**
     * @brief Gets the number of stored thread values
     *
     * @return The number of currently stored thread values.
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t {
        try {
            std::shared_lock lock(mutex_);  // Use shared_lock for read access
            return values_.size();
        } catch (...) {
            // Catch potential exceptions from map operations
            return 0;
        }
    }

    /**
     * @brief Checks if the storage is empty
     *
     * @return true if there are no stored thread values, false otherwise.
     */
    [[nodiscard]] auto empty() const noexcept -> bool {
        try {
            std::shared_lock lock(mutex_);  // Use shared_lock for read access
            return values_.empty();
        } catch (...) {
            // Catch potential exceptions from map operations
            return true;
        }
    }

    /**
     * @brief Sets or updates the cleanup function.
     *
     * Note: Changing the cleanup function does not affect values already
     * initialized. The new function will be used for values initialized or
     * reset *after* this call, and for cleanup during the destructor.
     *
     * @param cleanup New cleanup function to be called when a value is removed.
     */
    void setCleanupFunction(CleanupFn cleanup) noexcept {
        // No lock needed for std::function assignment itself, but a lock
        // might be considered if multiple threads could call this concurrently
        // and consistency of the cleanup function across threads is critical
        // during a brief transition. For simplicity and typical use cases,
        // direct assignment is often sufficient.
        cleanup_ = std::move(cleanup);
    }

    /**
     * @brief Checks if the specified thread has a value
     *
     * @param tid Thread ID to check.
     * @return true if the specified thread has an initialized value, false
     * otherwise.
     */
    [[nodiscard]] auto hasValueForThread(std::thread::id tid) const noexcept
        -> bool {
        try {
            std::shared_lock lock(mutex_);  // Use shared_lock for read access
            auto it = values_.find(tid);
            return it != values_.end() && it->second.has_value();
        } catch (...) {
            // Catch potential exceptions from map operations
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
    mutable std::shared_mutex
        mutex_;  ///< Mutex for thread-safe access to the map
    std::unordered_map<std::thread::id, std::optional<T>>
        values_;  ///< Stores values by thread ID
};

/**
 * @brief Alias using EnhancedThreadLocal as the default implementation.
 * @tparam T The type of the value to be stored.
 */
template <typename T>
using ThreadLocal = EnhancedThreadLocal<T>;

}  // namespace atom::async

#endif  // ATOM_ASYNC_THREADLOCAL_OPTIMIZED_HPP
