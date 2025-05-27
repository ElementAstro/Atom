/*
 * no_offset_ptr.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_TYPE_NO_OFFSET_PTR_HPP
#define ATOM_TYPE_NO_OFFSET_PTR_HPP

#include <atomic>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

#ifdef ATOM_USE_BOOST
#include <boost/type_traits/is_constructible.hpp>
#include <boost/type_traits/is_nothrow_move_assignable.hpp>
#include <boost/type_traits/is_nothrow_move_constructible.hpp>
#include <boost/type_traits/is_object.hpp>
#endif

namespace atom {

/**
 * @brief Exception thrown when attempting to access an invalid UnshiftedPtr.
 */
class unshifted_ptr_error : public std::runtime_error {
public:
    explicit unshifted_ptr_error(const char* message)
        : std::runtime_error(message) {}
};

/**
 * @brief Thread safety policy for UnshiftedPtr.
 */
enum class ThreadSafetyPolicy {
    None,   ///< No thread safety (fastest)
    Mutex,  ///< Use mutex for thread safety
    Atomic  ///< Use atomic operations (lock-free)
};

/**
 * @brief A lightweight pointer-like class that manages an object of type T
 * without dynamic memory allocation.
 *
 * @tparam T The type of the object to manage.
 * @tparam Safety The thread safety policy to use.
 */
template <typename T, ThreadSafetyPolicy Safety = ThreadSafetyPolicy::None>
#ifdef ATOM_USE_BOOST
    requires boost::is_object<T>::value
#else
    requires std::is_object_v<T>
#endif
class UnshiftedPtr {
public:
    using mutex_type = typename std::conditional<
        Safety == ThreadSafetyPolicy::None, std::monostate,
        typename std::conditional<Safety == ThreadSafetyPolicy::Mutex,
                                  std::mutex, std::monostate>::type>::type;

    /**
     * @brief Default constructor. Constructs the managed object using T's
     * default constructor.
     */
    constexpr UnshiftedPtr() noexcept(
#ifdef ATOM_USE_BOOST
        boost::is_nothrow_default_constructible<T>::value
#else
        std::is_nothrow_default_constructible_v<T>
#endif
    ) {
        try {
            new (&storage_) T();
            set_ownership(true);
        } catch (...) {
            set_ownership(false);
            throw;
        }
    }

    /**
     * @brief Constructor that initializes the managed object with given
     * arguments.
     *
     * @tparam Args Parameter pack for constructor arguments.
     * @param args Arguments to forward to the constructor of T.
     */
    template <typename... Args>
#ifdef ATOM_USE_BOOST
        requires boost::is_constructible<T, Args&&...>::value
#else
        requires std::constructible_from<T, Args...>
#endif
    constexpr explicit UnshiftedPtr(Args&&... args) noexcept(
#ifdef ATOM_USE_BOOST
        boost::is_nothrow_constructible<T, Args&&...>::value
#else
        std::is_nothrow_constructible_v<T, Args...>
#endif
    ) {
        try {
            new (&storage_) T(std::forward<Args>(args)...);
            set_ownership(true);
        } catch (...) {
            set_ownership(false);
            throw;
        }
    }

    /**
     * @brief Destructor. Destroys the managed object.
     */
    constexpr ~UnshiftedPtr() noexcept {
        try {
            destroy();
        } catch (...) {
            assert(false && "Exception thrown in UnshiftedPtr destructor");
        }
    }

    UnshiftedPtr(const UnshiftedPtr&) = delete;
    auto operator=(const UnshiftedPtr&) -> UnshiftedPtr& = delete;

    /**
     * @brief Move constructor.
     *
     * @param other The UnshiftedPtr to move from.
     */
    constexpr UnshiftedPtr(UnshiftedPtr&& other)
#ifdef ATOM_USE_BOOST
        noexcept(boost::is_nothrow_move_constructible<T>::value)
        requires boost::is_move_constructible<T>::value
#else
        noexcept(std::is_nothrow_move_constructible_v<T>)
        requires std::move_constructible<T>
#endif
    {
        if (other.has_value()) {
            try {
                new (&storage_) T(std::move(*other));
                set_ownership(true);
                other.relinquish_ownership();
            } catch (...) {
                set_ownership(false);
                throw;
            }
        } else {
            set_ownership(false);
        }
    }

    /**
     * @brief Move assignment operator.
     *
     * @param other The UnshiftedPtr to move from.
     * @return Reference to *this.
     */
    constexpr auto operator=(UnshiftedPtr&& other) -> UnshiftedPtr& {
        if (this != &other) {
            if (has_value()) {
                destroy();
            }

            if (other.has_value()) {
                try {
                    new (&storage_) T(std::move(*other));
                    set_ownership(true);
                    other.relinquish_ownership();
                } catch (...) {
                    set_ownership(false);
                    throw;
                }
            }
        }
        return *this;
    }

    /**
     * @brief Provides pointer-like access to the managed object.
     *
     * @return A pointer to the managed object.
     * @throws unshifted_ptr_error if the pointer doesn't own an object.
     */
    [[nodiscard]] constexpr auto operator->() -> T* {
        validate_ownership();
        return get_ptr();
    }

    /**
     * @brief Provides const pointer-like access to the managed object.
     *
     * @return A const pointer to the managed object.
     * @throws unshifted_ptr_error if the pointer doesn't own an object.
     */
    [[nodiscard]] constexpr auto operator->() const -> const T* {
        validate_ownership();
        return get_ptr();
    }

    /**
     * @brief Dereferences the managed object.
     *
     * @return A reference to the managed object.
     * @throws unshifted_ptr_error if the pointer doesn't own an object.
     */
    [[nodiscard]] constexpr auto operator*() -> T& {
        validate_ownership();
        return get();
    }

    /**
     * @brief Dereferences the managed object.
     *
     * @return A const reference to the managed object.
     * @throws unshifted_ptr_error if the pointer doesn't own an object.
     */
    [[nodiscard]] constexpr auto operator*() const -> const T& {
        validate_ownership();
        return get();
    }

    /**
     * @brief Resets the managed object by calling its destructor and
     * reconstructing it in-place.
     *
     * @tparam Args Parameter pack for constructor arguments.
     * @param args Arguments to forward to the constructor of T.
     * @throws Any exception thrown by T's constructor.
     */
    template <typename... Args>
#ifdef ATOM_USE_BOOST
        requires boost::is_constructible<T, Args&&...>::value
#else
        requires std::constructible_from<T, Args...>
#endif
    constexpr void reset(Args&&... args) {
        if constexpr (Safety == ThreadSafetyPolicy::None) {
            destroy();
            try {
                new (&storage_) T(std::forward<Args>(args)...);
                set_ownership_unsafe(true);
            } catch (...) {
                set_ownership_unsafe(false);
                throw;
            }
        } else {
            std::lock_guard<mutex_type> lock(mutex_);
            destroy();
            try {
                new (&storage_) T(std::forward<Args>(args)...);
                set_ownership_unsafe(true);
            } catch (...) {
                set_ownership_unsafe(false);
                throw;
            }
        }
    }

    /**
     * @brief Emplaces a new object in place with the provided arguments.
     *
     * @tparam Args The types of the arguments to construct the object with.
     * @param args The arguments to construct the object with.
     * @throws Any exception thrown by T's constructor.
     */
    template <typename... Args>
#ifdef ATOM_USE_BOOST
        requires boost::is_constructible<T, Args&&...>::value
#else
        requires std::constructible_from<T, Args...>
#endif
    constexpr void emplace(Args&&... args) {
        reset(std::forward<Args>(args)...);
    }

    /**
     * @brief Releases ownership of the managed object without destroying it.
     *
     * @return A pointer to the managed object.
     * @throws unshifted_ptr_error if the pointer doesn't own an object.
     */
    [[nodiscard]] constexpr auto release() -> T* {
        if constexpr (Safety == ThreadSafetyPolicy::None) {
            validate_ownership_unsafe();
            T* ptr = get_ptr();
            relinquish_ownership_unsafe();
            return ptr;
        } else {
            std::lock_guard<mutex_type> lock(mutex_);
            validate_ownership_unsafe();
            T* ptr = get_ptr();
            relinquish_ownership_unsafe();
            return ptr;
        }
    }

    /**
     * @brief Checks if the managed object has a value.
     *
     * @return True if the managed object has a value, false otherwise.
     */
    [[nodiscard]] constexpr auto has_value() const noexcept -> bool {
        if constexpr (Safety == ThreadSafetyPolicy::Atomic) {
            return owns_.load(std::memory_order_acquire);
        } else if constexpr (Safety == ThreadSafetyPolicy::None) {
            return owns_;
        } else {
            std::lock_guard<mutex_type> lock(mutex_);
            return owns_;
        }
    }

    /**
     * @brief Alias for has_value() for backward compatibility.
     */
    [[nodiscard]] constexpr auto hasValue() const noexcept -> bool {
        return has_value();
    }

    /**
     * @brief Provides direct access to the managed object when it exists.
     *
     * @return A pointer to the managed object, or nullptr if no object is
     * owned.
     */
    [[nodiscard]] constexpr auto get_safe() noexcept -> T* {
        return has_value() ? get_ptr() : nullptr;
    }

    /**
     * @brief Provides direct const access to the managed object when it exists.
     *
     * @return A const pointer to the managed object, or nullptr if no object is
     * owned.
     */
    [[nodiscard]] constexpr auto get_safe() const noexcept -> const T* {
        return has_value() ? get_ptr() : nullptr;
    }

    /**
     * @brief Enables using UnshiftedPtr in boolean contexts.
     *
     * @return True if the UnshiftedPtr contains a value, false otherwise.
     */
    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return has_value();
    }

    /**
     * @brief SIMD-optimized batch operation if applicable for type T.
     *
     * This is a placeholder for SIMD operations. The implementation
     * depends on the actual type T and its operations.
     */
    template <typename Func>
        requires std::invocable<Func, T&>
    constexpr void apply_if(Func&& func) {
        if (has_value()) {
            if constexpr (Safety == ThreadSafetyPolicy::None) {
                if (owns_) {
                    func(get());
                }
            } else {
                std::lock_guard<mutex_type> lock(mutex_);
                if (owns_) {
                    func(get());
                }
            }
        }
    }

private:
    /**
     * @brief Validates that the pointer owns an object, throwing if not.
     */
    constexpr void validate_ownership() const {
        if constexpr (Safety == ThreadSafetyPolicy::None) {
            if (!owns_) {
                throw unshifted_ptr_error(
                    "Attempting to access invalid UnshiftedPtr");
            }
        } else if constexpr (Safety == ThreadSafetyPolicy::Atomic) {
            if (!owns_.load(std::memory_order_acquire)) {
                throw unshifted_ptr_error(
                    "Attempting to access invalid UnshiftedPtr");
            }
        } else {
            std::lock_guard<mutex_type> lock(mutex_);
            validate_ownership_unsafe();
        }
    }

    /**
     * @brief Validates ownership without locking (must be called under lock or
     * in non-threaded context).
     */
    constexpr void validate_ownership_unsafe() const {
        if (!owns_) {
            throw unshifted_ptr_error(
                "Attempting to access invalid UnshiftedPtr");
        }
    }

    /**
     * @brief Sets the ownership flag in a thread-safe way.
     */
    constexpr void set_ownership(bool value) noexcept {
        if constexpr (Safety == ThreadSafetyPolicy::None) {
            owns_ = value;
        } else if constexpr (Safety == ThreadSafetyPolicy::Atomic) {
            owns_.store(value, std::memory_order_release);
        } else {
            std::lock_guard<mutex_type> lock(mutex_);
            owns_ = value;
        }
    }

    /**
     * @brief Sets the ownership flag without locking (must be called under lock
     * or in non-threaded context).
     */
    constexpr void set_ownership_unsafe(bool value) noexcept {
        if constexpr (Safety == ThreadSafetyPolicy::Atomic) {
            owns_.store(value, std::memory_order_relaxed);
        } else {
            owns_ = value;
        }
    }

    /**
     * @brief Retrieves a pointer to the managed object.
     *
     * @return A pointer to the managed object.
     */
    [[nodiscard]] constexpr auto get_ptr() noexcept -> T* {
        return std::launder(reinterpret_cast<T*>(&storage_));
    }

    /**
     * @brief Retrieves a const pointer to the managed object.
     *
     * @return A const pointer to the managed object.
     */
    [[nodiscard]] constexpr auto get_ptr() const noexcept -> const T* {
        return std::launder(reinterpret_cast<const T*>(&storage_));
    }

    /**
     * @brief Retrieves a reference to the managed object.
     *
     * @return A reference to the managed object.
     */
    [[nodiscard]] constexpr auto get() noexcept -> T& { return *get_ptr(); }

    /**
     * @brief Retrieves a const reference to the managed object.
     *
     * @return A const reference to the managed object.
     */
    [[nodiscard]] constexpr auto get() const noexcept -> const T& {
        return *get_ptr();
    }

    /**
     * @brief Destroys the managed object if ownership is held.
     */
    constexpr void destroy() noexcept(
#ifdef ATOM_USE_BOOST
        boost::is_nothrow_destructible<T>::value
#else
        std::is_nothrow_destructible_v<T>
#endif
    ) {
        if constexpr (Safety == ThreadSafetyPolicy::None) {
            if (owns_) {
                get().~T();
                owns_ = false;
            }
        } else if constexpr (Safety == ThreadSafetyPolicy::Atomic) {
            bool expected = true;
            if (owns_.compare_exchange_strong(expected, false,
                                              std::memory_order_acq_rel)) {
                get().~T();
            }
        } else {
            std::lock_guard<mutex_type> lock(mutex_);
            if (owns_) {
                get().~T();
                owns_ = false;
            }
        }
    }

    /**
     * @brief Relinquishes ownership without destroying the object.
     */
    constexpr void relinquish_ownership() noexcept { set_ownership(false); }

    /**
     * @brief Relinquishes ownership without locking (must be called under lock
     * or in non-threaded context).
     */
    constexpr void relinquish_ownership_unsafe() noexcept {
        set_ownership_unsafe(false);
    }

    alignas(T) std::byte storage_[sizeof(T)];

    std::conditional_t<Safety == ThreadSafetyPolicy::Atomic, std::atomic<bool>,
                       bool>
        owns_{false};

    mutable mutex_type mutex_;
};

template <typename T>
using ThreadSafeUnshiftedPtr = UnshiftedPtr<T, ThreadSafetyPolicy::Mutex>;

template <typename T>
using LockFreeUnshiftedPtr = UnshiftedPtr<T, ThreadSafetyPolicy::Atomic>;

}  // namespace atom

#endif  // ATOM_TYPE_NO_OFFSET_PTR_HPP
