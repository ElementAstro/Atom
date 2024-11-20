/*
 * indestructible.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_TYPE_INDESTRUCTIBLE_HPP
#define ATOM_TYPE_INDESTRUCTIBLE_HPP

#include <cstring>
#include <memory>
#include <type_traits>
#include <utility>

#ifdef ATOM_USE_BOOST
#include <boost/core/noncopyable.hpp>
#include <boost/type_traits.hpp>
#endif

/**
 * @brief A class template for creating an object that cannot be destructed.
 *
 * This class template provides a way to create an object that cannot be
 * destructed. It uses a union to store the object and provides various
 * member functions to manage the object's lifetime.
 *
 * @tparam T The type of the object to create.
 */
template <typename T>
struct Indestructible {
    union {
        T object;    ///< The object being stored.
        char dummy;  ///< Dummy character used for alignment purposes.
    };

    /**
     * @brief Constructs an Indestructible object with the provided arguments.
     *
     * This constructor constructs the stored object using the provided
     * arguments. It uses perfect forwarding to pass the arguments to the
     * constructor of the stored object.
     *
     * @tparam Args The types of the arguments to construct the object with.
     * @param args The arguments to construct the object with.
     */
    template <typename... Args>
#ifdef ATOM_USE_BOOST
        requires boost::is_constructible<T, Args&&...>::value
#else
        requires std::is_constructible_v<T, Args&&...>
#endif
    explicit constexpr Indestructible(std::in_place_t, Args&&... args)
        : object(std::forward<Args>(args)...) {
    }

    /**
     * @brief Destructor.
     *
     * If T is not trivially destructible, the destructor explicitly calls the
     * destructor of the stored object.
     */
    ~Indestructible() {
#ifdef ATOM_USE_BOOST
        if constexpr (!boost::is_trivially_destructible<T>::value) {
#else
        if constexpr (!std::is_trivially_destructible_v<T>) {
#endif
            object.~T();
        }
    }

    /**
     * @brief Copy constructor.
     *
     * This constructor copies the stored object from another Indestructible
     * object. It uses perfect forwarding to pass the other object to the
     * constructor of the stored object.
     *
     * @param other The other Indestructible object to copy from.
     */
    Indestructible(const Indestructible& other)
#ifdef ATOM_USE_BOOST
        requires boost::is_copy_constructible<T>::value
#else
        requires std::is_copy_constructible_v<T>
#endif
    {
        construct_from(other);
    }

    /**
     * @brief Move constructor.
     *
     * This constructor moves the stored object from another Indestructible
     * object. It uses perfect forwarding to pass the other object to the
     * constructor of the stored object.
     *
     * @param other The other Indestructible object to move from.
     */
    Indestructible(Indestructible&& other) noexcept
#ifdef ATOM_USE_BOOST
        requires boost::is_move_constructible<T>::value
#else
        requires std::is_move_constructible_v<T>
#endif
    {
        construct_from(std::move(other));
    }

    /**
     * @brief Copy assignment operator.
     *
     * This operator copies the stored object from another Indestructible
     * object. It uses perfect forwarding to pass the other object to the
     * assignment operator of the stored object.
     *
     * @param other The other Indestructible object to copy from.
     * @return A reference to this Indestructible object.
     */
    auto operator=(const Indestructible& other) -> Indestructible& {
        if (this != &other) {
            assign_from(other);
        }
        return *this;
    }

    /**
     * @brief Move assignment operator.
     *
     * This operator moves the stored object from another Indestructible
     * object. It uses perfect forwarding to pass the other object to the
     * assignment operator of the stored object.
     *
     * @param other The other Indestructible object to move from.
     * @return A reference to this Indestructible object.
     */
    auto operator=(Indestructible&& other) noexcept -> Indestructible& {
        if (this != &other) {
            assign_from(std::move(other));
        }
        return *this;
    }

private:
    /**
     * @brief Constructs the stored object from another object.
     *
     * This function constructs the stored object from another object. It uses
     * perfect forwarding to pass the other object to the constructor of the
     * stored object.
     *
     * @tparam U The type of the other object.
     * @param other The other object to construct from.
     */
    template <typename U>
    void construct_from(U&& other) {
#ifdef ATOM_USE_BOOST
        if constexpr (boost::is_trivially_copy_constructible<T>::value ||
                      boost::is_trivially_move_constructible<T>::value) {
#else
        if constexpr (std::is_trivially_copy_constructible_v<T> ||
                      std::is_trivially_move_constructible_v<T>) {
#endif
            std::memcpy(&object, &other.object, sizeof(T));
        } else {
            new (&object) T(std::forward<U>(other.object));
        }
    }

    /**
     * @brief Assigns the stored object from another object.
     *
     * This function assigns the stored object from another object. It uses
     * perfect forwarding to pass the other object to the assignment operator
     * of the stored object.
     *
     * @tparam U The type of the other object.
     * @param other The other object to assign from.
     */
    template <typename U>
    void assign_from(U&& other) {
#ifdef ATOM_USE_BOOST
        if constexpr (boost::is_copy_assignable<T>::value ||
                      boost::is_move_assignable<T>::value) {
#else
        if constexpr (std::is_copy_assignable_v<T> ||
                      std::is_move_assignable_v<T>) {
#endif
            object = std::forward<U>(other.object);
        }
    }

public:
    /**
     * @brief Returns a reference to the stored object.
     *
     * @return Reference to the stored object.
     */
    constexpr T& get() & noexcept { return object; }

    /**
     * @brief Returns a const reference to the stored object.
     *
     * @return Const reference to the stored object.
     */
    constexpr const T& get() const& noexcept { return object; }

    /**
     * @brief Returns an rvalue reference to the stored object.
     *
     * @return Rvalue reference to the stored object.
     */
    constexpr T&& get() && noexcept { return std::move(object); }

    /**
     * @brief Returns a const rvalue reference to the stored object.
     *
     * @return Const rvalue reference to the stored object.
     */
    constexpr const T&& get() const&& noexcept { return std::move(object); }

    /**
     * @brief Accesses the stored object.
     *
     * @return Pointer to the stored object.
     */
    constexpr T* operator->() noexcept { return &object; }
    constexpr const T* operator->() const noexcept { return &object; }

    /**
     * @brief Converts to reference of stored object.
     *
     * This operator converts the Indestructible object to a reference to the
     * stored object.
     *
     * @return Reference to the stored object.
     */
    constexpr operator T&() & noexcept { return object; }

    /**
     * @brief Converts to const reference of stored object.
     *
     * This operator converts the Indestructible object to a const reference to
     * the stored object.
     *
     * @return Const reference to the stored object.
     */
    constexpr operator const T&() const& noexcept { return object; }

    /**
     * @brief Converts to rvalue reference of stored object.
     *
     * This operator converts the Indestructible object to an rvalue reference
     * to the stored object.
     *
     * @return Rvalue reference to the stored object.
     */
    constexpr operator T&&() && noexcept { return std::move(object); }

    /**
     * @brief Converts to const rvalue reference of stored object.
     *
     * This operator converts the Indestructible object to a const rvalue
     * reference to the stored object.
     *
     * @return Const rvalue reference to the stored object.
     */
    constexpr operator const T&&() const&& noexcept {
        return std::move(object);
    }

    /**
     * @brief Resets the stored object with new arguments.
     *
     * This function destroys the current stored object and constructs a new
     * object with the provided arguments. It uses perfect forwarding to pass
     * the arguments to the constructor of the new object.
     *
     * @tparam Args The types of the arguments to construct the object with.
     * @param args The arguments to construct the object with.
     */
    template <typename... Args>
#ifdef ATOM_USE_BOOST
        requires boost::is_constructible<T, Args&&...>::value
#else
        requires std::is_constructible_v<T, Args&&...>
#endif
    void reset(Args&&... args) {
        destroy();
        new (&object) T(std::forward<Args>(args)...);
    }

    /**
     * @brief Emplaces a new object in place with the provided arguments.
     *
     * This function destroys the current stored object and constructs a new
     * object with the provided arguments. It uses perfect forwarding to pass
     * the arguments to the constructor of the new object.
     *
     * @tparam Args The types of the arguments to construct the object with.
     * @param args The arguments to construct the object with.
     */
    template <typename... Args>
#ifdef ATOM_USE_BOOST
        requires boost::is_constructible<T, Args&&...>::value
#else
        requires std::is_constructible_v<T, Args&&...>
#endif
    void emplace(Args&&... args) {
        reset(std::forward<Args>(args)...);
    }

private:
    /**
     * @brief Destroys the stored object.
     *
     * This function explicitly calls the destructor of the stored object if
     * it is not trivially destructible.
     */
    void destroy() {
#ifdef ATOM_USE_BOOST
        if constexpr (!boost::is_trivially_destructible<T>::value) {
#else
        if constexpr (!std::is_trivially_destructible_v<T>) {
#endif
            object.~T();
        }
    }
};

/**
 * @brief A guard class for ensuring destruction of an object.
 *
 * This class provides a guard that ensures the destruction of an object when
 * the guard goes out of scope. It uses the `std::destroy_at` function to
 * destroy the object.
 *
 * @tparam T The type of the object to guard.
 */
template <class T>
class destruction_guard {
public:
    /**
     * @brief Constructs a destruction guard for the given object.
     *
     * @param p Pointer to the object to guard.
     */
    explicit destruction_guard(T* p) noexcept : p_(p) {}

    destruction_guard(const destruction_guard&) = delete;
    destruction_guard& operator=(const destruction_guard&) = delete;

    /**
     * @brief Destructor.
     *
     * This destructor destroys the guarded object using the `std::destroy_at`
     * function.
     */
    ~destruction_guard() noexcept(
#ifdef ATOM_USE_BOOST
        boost::is_nothrow_destructible<T>::value
#else
        std::is_nothrow_destructible_v<T>
#endif
    ) {
        std::destroy_at(p_);
    }

private:
    T* p_;  ///< Pointer to the guarded object.
};

#endif  // ATOM_TYPE_INDESTRUCTIBLE_HPP
