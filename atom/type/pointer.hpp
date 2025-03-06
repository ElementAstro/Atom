/*
 * pointer.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-3

Description: Pointer Sentinel for Atom

**************************************************/

#ifndef ATOM_TYPE_POINTER_HPP
#define ATOM_TYPE_POINTER_HPP

#include <atomic>
#include <concepts>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <variant>

#ifdef ATOM_USE_BOOST
#include <boost/type_traits.hpp>
#endif

#include "atom/error/exception.hpp"
#include "atom/macro.hpp"

// Forward declaration for thread pool
namespace atom {
class ThreadPool;
}

/**
 * @brief Extended concept to check if a type is a pointer type, including raw
 * pointers, std::shared_ptr, std::unique_ptr, and std::weak_ptr.
 *
 * @tparam T The type to check.
 */
template <typename T>
concept PointerType =
#ifdef ATOM_USE_BOOST
    boost::is_pointer<T>::value ||
    boost::is_same<
        T, std::shared_ptr<typename std::remove_pointer<T>::type>>::value ||
    boost::is_same<
        T, std::unique_ptr<typename std::remove_pointer<T>::type>>::value ||
    boost::is_same<T,
                   std::weak_ptr<typename std::remove_pointer<T>::type>>::value;
#else
    std::is_pointer_v<T> ||
    std::is_same_v<T, std::shared_ptr<typename std::remove_pointer<T>::type>> ||
    std::is_same_v<T, std::unique_ptr<typename std::remove_pointer<T>::type>> ||
    std::is_same_v<T, std::weak_ptr<typename std::remove_pointer<T>::type>>;
#endif

/**
 * @brief Exception class for pointer-related errors
 */
class PointerException : public std::runtime_error {
public:
    explicit PointerException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief A thread-safe class template to hold different types of pointers using
 * std::variant with enhanced error handling and concurrency support.
 *
 * @tparam T The type of the pointed-to object.
 */
template <typename T>
class PointerSentinel {
private:
    // Using std::variant to store different types of pointers
    std::variant<std::shared_ptr<T>, std::unique_ptr<T>, std::weak_ptr<T>, T*>
        ptr_;

    // Thread safety
    mutable std::shared_mutex mutex_;

    // Validity tracking
    std::atomic<bool> is_valid_{false};

    // Helper method to validate pointer before operations
    void validate() const {
        if (!is_valid_.load(std::memory_order_acquire)) {
            throw PointerException("Invalid pointer state");
        }
    }

public:
    /**
     * @brief Default constructor creates an invalid pointer sentinel
     */
    PointerSentinel() = default;

    /**
     * @brief Construct a new Pointer Sentinel object from a shared pointer.
     *
     * @param p The shared pointer.
     * @throws PointerException if the pointer is null
     */
    explicit PointerSentinel(std::shared_ptr<T> p) {
        if (!p) {
            throw PointerException(
                "Null shared_ptr provided to PointerSentinel");
        }
        ptr_ = std::move(p);
        is_valid_.store(true, std::memory_order_release);
    }

    /**
     * @brief Construct a new Pointer Sentinel object from a unique pointer.
     *
     * @param p The unique pointer.
     * @throws PointerException if the pointer is null
     */
    explicit PointerSentinel(std::unique_ptr<T>&& p) {
        if (!p) {
            throw PointerException(
                "Null unique_ptr provided to PointerSentinel");
        }
        ptr_ = std::move(p);
        is_valid_.store(true, std::memory_order_release);
    }

    /**
     * @brief Construct a new Pointer Sentinel object from a weak pointer.
     *
     * @param p The weak pointer.
     * @throws PointerException if the weak pointer is expired
     */
    explicit PointerSentinel(std::weak_ptr<T> p) {
        if (p.expired()) {
            throw PointerException(
                "Expired weak_ptr provided to PointerSentinel");
        }
        ptr_ = std::move(p);
        is_valid_.store(true, std::memory_order_release);
    }

    /**
     * @brief Construct a new Pointer Sentinel object from a raw pointer.
     *
     * @param p The raw pointer.
     * @throws PointerException if the pointer is null
     */
    explicit PointerSentinel(T* p) {
        if (!p) {
            throw PointerException(
                "Null raw pointer provided to PointerSentinel");
        }
        ptr_ = p;
        is_valid_.store(true, std::memory_order_release);
    }

    /**
     * @brief Copy constructor with deep copy for unique_ptr and raw pointers.
     *
     * @param other The other Pointer Sentinel object to copy from.
     */
    PointerSentinel(const PointerSentinel& other) {
        std::shared_lock lock(other.mutex_);
        try {
            other.validate();
            ptr_ = std::visit(
                [](const auto& p)
                    -> std::variant<std::shared_ptr<T>, std::unique_ptr<T>,
                                    std::weak_ptr<T>, T*> {
                    using U = std::decay_t<decltype(p)>;

                    if constexpr (std::is_same_v<U, std::shared_ptr<T>>) {
                        return p;
                    } else if constexpr (std::is_same_v<U,
                                                        std::unique_ptr<T>>) {
                        if (!p)
                            return std::unique_ptr<T>{};
                        return std::make_unique<T>(*p);
                    } else if constexpr (std::is_same_v<U, std::weak_ptr<T>>) {
                        return p;
                    } else {  // Raw pointer
                        if (!p)
                            return static_cast<T*>(nullptr);
                        return new T(*p);
                    }
                },
                other.ptr_);
            is_valid_.store(other.is_valid_.load(std::memory_order_acquire),
                            std::memory_order_release);
        } catch (const std::exception& e) {
            throw PointerException(std::string("Copy construction failed: ") +
                                   e.what());
        }
    }

    /**
     * @brief Move constructor.
     *
     * @param other The other Pointer Sentinel object to move from.
     */
    PointerSentinel(PointerSentinel&& other) noexcept {
        std::unique_lock lock(other.mutex_);
        ptr_ = std::move(other.ptr_);
        is_valid_.store(other.is_valid_.load(std::memory_order_acquire),
                        std::memory_order_release);
        other.is_valid_.store(false, std::memory_order_release);
    }

    /**
     * @brief Destructor handles cleanup of raw pointers
     */
    ~PointerSentinel() {
        try {
            std::unique_lock lock(mutex_);
            if (std::holds_alternative<T*>(ptr_)) {
                delete std::get<T*>(ptr_);
            }
        } catch (...) {
            // Ensure no exceptions escape the destructor
        }
    }

    /**
     * @brief Copy assignment operator.
     *
     * @param other The other Pointer Sentinel object to copy from.
     * @return A reference to this Pointer Sentinel object.
     */
    auto operator=(const PointerSentinel& other) -> PointerSentinel& {
        if (this != &other) {
            PointerSentinel temp(other);
            std::unique_lock lock1(mutex_, std::defer_lock);
            std::shared_lock lock2(other.mutex_, std::defer_lock);
            std::lock(lock1, lock2);

            std::swap(ptr_, temp.ptr_);
            is_valid_.store(temp.is_valid_.load(std::memory_order_acquire),
                            std::memory_order_release);
        }
        return *this;
    }

    /**
     * @brief Move assignment operator.
     *
     * @param other The other Pointer Sentinel object to move from.
     * @return A reference to this Pointer Sentinel object.
     */
    auto operator=(PointerSentinel&& other) noexcept -> PointerSentinel& {
        if (this != &other) {
            std::unique_lock lock1(mutex_, std::defer_lock);
            std::unique_lock lock2(other.mutex_, std::defer_lock);
            std::lock(lock1, lock2);

            // Clean up current resources if needed
            if (std::holds_alternative<T*>(ptr_)) {
                delete std::get<T*>(ptr_);
            }

            ptr_ = std::move(other.ptr_);
            is_valid_.store(other.is_valid_.load(std::memory_order_acquire),
                            std::memory_order_release);
            other.is_valid_.store(false, std::memory_order_release);
        }
        return *this;
    }

    /**
     * @brief Check if the pointer is valid
     *
     * @return bool True if the pointer is valid and usable
     */
    [[nodiscard]] bool is_valid() const noexcept {
        return is_valid_.load(std::memory_order_acquire);
    }

    /**
     * @brief Get the raw pointer stored in the variant with boundary checks
     *
     * @return T* The raw pointer, or nullptr if invalid
     * @throws PointerException if the weak_ptr is expired or the pointer is
     * invalid
     */
    [[nodiscard]] auto get() const -> T* {
        std::shared_lock lock(mutex_);
        try {
            validate();
            return std::visit(
                [this](auto&& arg) -> T* {
                    using U = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_pointer_v<U>) {
                        if (!arg) {
                            throw PointerException("Null raw pointer");
                        }
                        return arg;
                    } else if constexpr (std::is_same_v<U, std::weak_ptr<T>>) {
                        auto spt = arg.lock();  // Try to lock the weak_ptr
                        if (!spt) {
                            throw PointerException("Expired weak_ptr");
                        }
                        return spt.get();
                    } else {
                        if (!arg) {
                            throw PointerException("Null smart pointer");
                        }
                        return arg.get();
                    }
                },
                ptr_);
        } catch (const PointerException& e) {
            throw;
        } catch (const std::exception& e) {
            throw PointerException(std::string("Error getting pointer: ") +
                                   e.what());
        }
    }

    /**
     * @brief Get the raw pointer without throwing exceptions
     *
     * @return T* The raw pointer, or nullptr if invalid
     */
    [[nodiscard]] auto get_noexcept() const noexcept -> T* {
        try {
            std::shared_lock lock(mutex_);
            if (!is_valid()) {
                return nullptr;
            }

            return std::visit(
                [](auto&& arg) -> T* {
                    using U = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_pointer_v<U>) {
                        return arg;
                    } else if constexpr (std::is_same_v<U, std::weak_ptr<T>>) {
                        auto spt = arg.lock();
                        return spt ? spt.get() : nullptr;
                    } else {
                        return arg ? arg.get() : nullptr;
                    }
                },
                ptr_);
        } catch (...) {
            return nullptr;
        }
    }

    /**
     * @brief Helper method to invoke member functions on the pointed-to object
     * with error handling.
     *
     * @tparam Func The type of the member function pointer.
     * @tparam Args The types of the arguments to the member function.
     * @param func The member function pointer.
     * @param args The arguments to the member function.
     * @return auto The return type of the member function.
     * @throws PointerException if the pointer is invalid or the operation fails
     */
    template <typename Func, typename... Args>
    [[nodiscard]] auto invoke(Func func, Args&&... args) {
        std::shared_lock lock(mutex_);
        try {
            validate();
            return std::visit(
                [func, &args...](auto&& arg) -> decltype(auto) {
                    using U = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_pointer_v<U>) {
                        if (!arg) {
                            throw PointerException(
                                "Null raw pointer during invoke");
                        }
                        return ((*arg).*func)(std::forward<Args>(args)...);
                    } else if constexpr (std::is_same_v<U, std::weak_ptr<T>>) {
                        auto spt = arg.lock();
                        if (!spt) {
                            throw PointerException(
                                "Expired weak_ptr during invoke");
                        }
                        return ((*spt.get()).*
                                func)(std::forward<Args>(args)...);
                    } else {
                        if (!arg) {
                            throw PointerException(
                                "Null smart pointer during invoke");
                        }
                        return ((*arg.get()).*
                                func)(std::forward<Args>(args)...);
                    }
                },
                ptr_);
        } catch (const PointerException& e) {
            throw;
        } catch (const std::exception& e) {
            throw PointerException(std::string("Invoke operation failed: ") +
                                   e.what());
        }
    }

    /**
     * @brief Helper method to invoke a callable object on the pointed-to object
     * with error handling.
     *
     * @tparam Callable The type of the callable object.
     * @param callable The callable object.
     * @return auto The return type of the callable object.
     * @throws PointerException if the pointer is invalid or the operation fails
     */
    template <typename Callable>
    [[nodiscard]] auto apply(Callable&& callable) {
        std::shared_lock lock(mutex_);
        try {
            validate();
            return std::visit(
                [&callable](auto&& arg) -> decltype(auto) {
                    using U = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_pointer_v<U>) {
                        if (!arg) {
                            throw PointerException(
                                "Null raw pointer during apply");
                        }
                        return std::invoke(std::forward<Callable>(callable),
                                           arg);
                    } else if constexpr (std::is_same_v<U, std::weak_ptr<T>>) {
                        auto spt = arg.lock();
                        if (!spt) {
                            throw PointerException(
                                "Expired weak_ptr during apply");
                        }
                        return std::invoke(std::forward<Callable>(callable),
                                           spt.get());
                    } else {
                        if (!arg) {
                            throw PointerException(
                                "Null smart pointer during apply");
                        }
                        return std::invoke(std::forward<Callable>(callable),
                                           arg.get());
                    }
                },
                ptr_);
        } catch (const PointerException& e) {
            throw;
        } catch (const std::exception& e) {
            throw PointerException(std::string("Apply operation failed: ") +
                                   e.what());
        }
    }

    /**
     * @brief Helper function to apply a function to the pointed-to object,
     * with the function returning a void.
     *
     * @tparam Func The type of the function to apply.
     * @tparam Args The types of the arguments to the function.
     * @param func The function to apply.
     * @param args The arguments to the function.
     * @throws PointerException if the pointer is invalid or the operation fails
     */
    template <typename Func, typename... Args>
    void applyVoid(Func func, Args&&... args) {
        std::shared_lock lock(mutex_);
        try {
            validate();
            std::visit(
                [&func, &args...](auto&& arg) {
                    using U = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_pointer_v<U>) {
                        if (!arg) {
                            throw PointerException(
                                "Null raw pointer during applyVoid");
                        }
                        std::invoke(std::forward<Func>(func), arg,
                                    std::forward<Args>(args)...);
                    } else if constexpr (std::is_same_v<U, std::weak_ptr<T>>) {
                        auto spt = arg.lock();
                        if (!spt) {
                            throw PointerException(
                                "Expired weak_ptr during applyVoid");
                        }
                        std::invoke(std::forward<Func>(func), spt.get(),
                                    std::forward<Args>(args)...);
                    } else {
                        if (!arg) {
                            throw PointerException(
                                "Null smart pointer during applyVoid");
                        }
                        std::invoke(std::forward<Func>(func), arg.get(),
                                    std::forward<Args>(args)...);
                    }
                },
                ptr_);
        } catch (const PointerException& e) {
            throw;
        } catch (const std::exception& e) {
            throw PointerException(std::string("ApplyVoid operation failed: ") +
                                   e.what());
        }
    }

    /**
     * @brief Convert to another pointer type if compatible
     *
     * @tparam U The target type to convert to
     * @return PointerSentinel<U> A new pointer sentinel of type U
     * @throws PointerException if the conversion is not possible
     */
    template <typename U>
    [[nodiscard]] PointerSentinel<U> convert_to() const {
        static_assert(std::is_convertible_v<T*, U*> ||
                          std::is_base_of_v<T, U> || std::is_base_of_v<U, T>,
                      "Types must be convertible for pointer conversion");

        std::shared_lock lock(mutex_);
        try {
            validate();

            return std::visit(
                [](auto&& arg) -> PointerSentinel<U> {
                    using V = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_pointer_v<V>) {
                        return PointerSentinel<U>(static_cast<U*>(arg));
                    } else if constexpr (std::is_same_v<V, std::weak_ptr<T>>) {
                        auto spt = arg.lock();
                        if (!spt) {
                            throw PointerException(
                                "Expired weak_ptr during conversion");
                        }
                        return PointerSentinel<U>(
                            std::static_pointer_cast<U>(spt));
                    } else if constexpr (std::is_same_v<V,
                                                        std::shared_ptr<T>>) {
                        return PointerSentinel<U>(
                            std::static_pointer_cast<U>(arg));
                    } else {
                        // Unique ptr requires special handling
                        U* raw_ptr = static_cast<U*>(arg.get());
                        return PointerSentinel<U>(std::unique_ptr<U>(raw_ptr));
                    }
                },
                ptr_);
        } catch (const PointerException& e) {
            throw;
        } catch (const std::exception& e) {
            throw PointerException(std::string("Type conversion failed: ") +
                                   e.what());
        }
    }

    /**
     * @brief Apply operation asynchronously using std::async
     *
     * @tparam Callable The type of callable object
     * @param callable The function to execute
     * @return std::future<decltype(callable(T*))> Future containing the result
     */
    template <typename Callable>
    [[nodiscard]] auto apply_async(Callable&& callable) {
        std::shared_lock lock(mutex_);
        try {
            validate();

            using result_type = decltype(callable(std::declval<T*>()));

            // Create a copy of the pointer to ensure it remains valid
            auto copied_ptr = std::visit(
                [](auto&& arg) -> std::shared_ptr<T> {
                    using U = std::decay_t<decltype(arg)>;

                    if constexpr (std::is_pointer_v<U>) {
                        return std::shared_ptr<T>(
                            arg, [](T*) {});  // Non-owning shared_ptr
                    } else if constexpr (std::is_same_v<U, std::weak_ptr<T>>) {
                        return arg.lock();
                    } else if constexpr (std::is_same_v<U,
                                                        std::shared_ptr<T>>) {
                        return arg;
                    } else {
                        // Make a shared copy from the unique_ptr without
                        // transferring ownership
                        return std::shared_ptr<T>(arg.get(), [](T*) {});
                    }
                },
                ptr_);

            if (!copied_ptr) {
                throw PointerException(
                    "Could not obtain shared_ptr for async operation");
            }

            return std::async(std::launch::async,
                              [copied_ptr, callable = std::forward<Callable>(
                                               callable)]() -> result_type {
                                  return callable(copied_ptr.get());
                              });

        } catch (const PointerException& e) {
            throw;
        } catch (const std::exception& e) {
            throw PointerException(std::string("Async operation failed: ") +
                                   e.what());
        }
    }

    /**
     * @brief Apply SIMD-friendly operation by passing both the object and a
     * size parameter
     *
     * @tparam Func Function type that accepts pointer and size
     * @param func Function that can utilize SIMD instructions
     * @param size Size parameter for vectorized operations
     */
    template <typename Func>
    void apply_simd(Func&& func, size_t size) {
        std::shared_lock lock(mutex_);
        try {
            validate();

            auto* ptr = this->get();
            if (!ptr) {
                throw PointerException("Null pointer during SIMD operation");
            }

            // The func should be implemented to use SIMD instructions
            // internally
            std::forward<Func>(func)(ptr, size);

        } catch (const PointerException& e) {
            throw;
        } catch (const std::exception& e) {
            throw PointerException(std::string("SIMD operation failed: ") +
                                   e.what());
        }
    }
};

#endif  // ATOM_TYPE_POINTER_HPP
