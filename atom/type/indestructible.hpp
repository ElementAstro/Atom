/*!
 * \file indestructible.hpp
 * \brief A class template for creating objects that cannot be destructed
 * automatically
 * \author Max Qian <lightapt.com>
 * \date 2023-2024
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
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
 * @brief A class template for creating an object that cannot be destructed
 * automatically
 *
 * This class template provides a way to create an object that bypasses
 * automatic destruction. It uses a union to store the object and provides
 * manual lifetime management through various member functions.
 *
 * @tparam T The type of the object to store
 */
template <typename T>
struct Indestructible {
private:
    union {
        T object;
        char dummy;
    };

    template <typename U>
    static constexpr bool is_constructible_v =
#ifdef ATOM_USE_BOOST
        boost::is_constructible<T, U>::value;
#else
        std::is_constructible_v<T, U>;
#endif

    template <typename U>
    static constexpr bool is_copy_constructible_v =
#ifdef ATOM_USE_BOOST
        boost::is_copy_constructible<T>::value;
#else
        std::is_copy_constructible_v<T>;
#endif

    template <typename U>
    static constexpr bool is_move_constructible_v =
#ifdef ATOM_USE_BOOST
        boost::is_move_constructible<T>::value;
#else
        std::is_move_constructible_v<T>;
#endif

    static constexpr bool is_trivially_destructible_v =
#ifdef ATOM_USE_BOOST
        boost::is_trivially_destructible<T>::value;
#else
        std::is_trivially_destructible_v<T>;
#endif

    static constexpr bool is_trivially_copy_constructible_v =
#ifdef ATOM_USE_BOOST
        boost::is_trivially_copy_constructible<T>::value;
#else
        std::is_trivially_copy_constructible_v<T>;
#endif

    static constexpr bool is_trivially_move_constructible_v =
#ifdef ATOM_USE_BOOST
        boost::is_trivially_move_constructible<T>::value;
#else
        std::is_trivially_move_constructible_v<T>;
#endif

    static constexpr bool is_copy_assignable_v =
#ifdef ATOM_USE_BOOST
        boost::is_copy_assignable<T>::value;
#else
        std::is_copy_assignable_v<T>;
#endif

    static constexpr bool is_move_assignable_v =
#ifdef ATOM_USE_BOOST
        boost::is_move_assignable<T>::value;
#else
        std::is_move_assignable_v<T>;
#endif

public:
    /**
     * @brief Constructs an Indestructible object with the provided arguments
     *
     * This constructor constructs the stored object using the provided
     * arguments with perfect forwarding.
     *
     * @tparam Args The types of the arguments to construct the object with
     * @param args The arguments to construct the object with
     */
    template <typename... Args>
        requires is_constructible_v<Args&&...>
    explicit constexpr Indestructible(std::in_place_t, Args&&... args)
        : object(std::forward<Args>(args)...) {}

    /**
     * @brief Destructor
     *
     * Explicitly calls the destructor of the stored object if it is not
     * trivially destructible.
     */
    ~Indestructible() {
        if constexpr (!is_trivially_destructible_v) {
            std::destroy_at(&object);
        }
    }

    /**
     * @brief Copy constructor
     *
     * Copies the stored object from another Indestructible object.
     *
     * @param other The other Indestructible object to copy from
     */
    Indestructible(const Indestructible& other)
        requires is_copy_constructible_v<T>
    {
        construct_from(other);
    }

    /**
     * @brief Move constructor
     *
     * Moves the stored object from another Indestructible object.
     *
     * @param other The other Indestructible object to move from
     */
    Indestructible(Indestructible&& other) noexcept
        requires is_move_constructible_v<T>
    {
        construct_from(std::move(other));
    }

    /**
     * @brief Copy assignment operator
     *
     * Copies the stored object from another Indestructible object.
     *
     * @param other The other Indestructible object to copy from
     * @return Reference to this Indestructible object
     */
    auto operator=(const Indestructible& other) -> Indestructible&
        requires std::is_copy_assignable_v<T>
    {
        if (this != &other) {
            assign_from(other);
        }
        return *this;
    }

    /**
     * @brief Move assignment operator
     *
     * Moves the stored object from another Indestructible object.
     *
     * @param other The other Indestructible object to move from
     * @return Reference to this Indestructible object
     */
    auto operator=(Indestructible&& other) noexcept -> Indestructible&
        requires std::is_move_assignable_v<T>
    {
        if (this != &other) {
            assign_from(std::move(other));
        }
        return *this;
    }

    /**
     * @brief Returns a reference to the stored object
     * @return Reference to the stored object
     */
    [[nodiscard]] constexpr T& get() & noexcept { return object; }

    /**
     * @brief Returns a const reference to the stored object
     * @return Const reference to the stored object
     */
    [[nodiscard]] constexpr const T& get() const& noexcept { return object; }

    /**
     * @brief Returns an rvalue reference to the stored object
     * @return Rvalue reference to the stored object
     */
    [[nodiscard]] constexpr T&& get() && noexcept { return std::move(object); }

    /**
     * @brief Returns a const rvalue reference to the stored object
     * @return Const rvalue reference to the stored object
     */
    [[nodiscard]] constexpr const T&& get() const&& noexcept {
        return std::move(object);
    }

    /**
     * @brief Accesses the stored object
     * @return Pointer to the stored object
     */
    [[nodiscard]] constexpr T* operator->() noexcept { return &object; }
    [[nodiscard]] constexpr const T* operator->() const noexcept {
        return &object;
    }

    /**
     * @brief Converts to reference of stored object
     * @return Reference to the stored object
     */
    [[nodiscard]] constexpr operator T&() & noexcept { return object; }

    /**
     * @brief Converts to const reference of stored object
     * @return Const reference to the stored object
     */
    [[nodiscard]] constexpr operator const T&() const& noexcept {
        return object;
    }

    /**
     * @brief Converts to rvalue reference of stored object
     * @return Rvalue reference to the stored object
     */
    [[nodiscard]] constexpr operator T&&() && noexcept {
        return std::move(object);
    }

    /**
     * @brief Converts to const rvalue reference of stored object
     * @return Const rvalue reference to the stored object
     */
    [[nodiscard]] constexpr operator const T&&() const&& noexcept {
        return std::move(object);
    }

    /**
     * @brief Resets the stored object with new arguments
     *
     * Destroys the current stored object and constructs a new object
     * with the provided arguments using perfect forwarding.
     *
     * @tparam Args The types of the arguments to construct the object with
     * @param args The arguments to construct the object with
     */
    template <typename... Args>
        requires is_constructible_v<Args&&...>
    void reset(Args&&... args) {
        destroy();
        std::construct_at(&object, std::forward<Args>(args)...);
    }

    /**
     * @brief Emplaces a new object in place with the provided arguments
     *
     * Destroys the current stored object and constructs a new object
     * with the provided arguments using perfect forwarding.
     *
     * @tparam Args The types of the arguments to construct the object with
     * @param args The arguments to construct the object with
     */
    template <typename... Args>
        requires is_constructible_v<Args&&...>
    void emplace(Args&&... args) {
        reset(std::forward<Args>(args)...);
    }

private:
    /**
     * @brief Constructs the stored object from another object
     *
     * Uses optimal construction strategy based on type traits - either
     * trivial copy via memcpy or placement new construction.
     *
     * @tparam U The type of the other object
     * @param other The other object to construct from
     */
    template <typename U>
    void construct_from(U&& other) {
        if constexpr (is_trivially_copy_constructible_v ||
                      is_trivially_move_constructible_v) {
            std::memcpy(&object, &other.object, sizeof(T));
        } else {
            std::construct_at(&object, std::forward<U>(other).object);
        }
    }

    /**
     * @brief Assigns the stored object from another object
     *
     * Uses perfect forwarding to determine whether to copy or move assign.
     *
     * @tparam U The type of the other object
     * @param other The other object to assign from
     */
    template <typename U>
    void assign_from(U&& other) {
        if constexpr (is_copy_assignable_v || is_move_assignable_v) {
            if constexpr (std::is_rvalue_reference_v<U&&>) {
                object = std::move(other.object);
            } else {
                object = other.object;
            }
        }
    }

    /**
     * @brief Destroys the stored object
     *
     * Explicitly calls the destructor of the stored object if it is not
     * trivially destructible.
     */
    void destroy() noexcept {
        if constexpr (!is_trivially_destructible_v) {
            std::destroy_at(&object);
        }
    }
};

/**
 * @brief A RAII guard class for ensuring destruction of an object
 *
 * This class provides a guard that ensures the destruction of an object when
 * the guard goes out of scope. It uses `std::destroy_at` to destroy the object.
 *
 * @tparam T The type of the object to guard
 */
template <class T>
class destruction_guard {
public:
    /**
     * @brief Constructs a destruction guard for the given object
     * @param p Pointer to the object to guard
     */
    explicit destruction_guard(T* p) noexcept : p_(p) {}

    destruction_guard(const destruction_guard&) = delete;
    destruction_guard& operator=(const destruction_guard&) = delete;
    destruction_guard(destruction_guard&&) = delete;
    destruction_guard& operator=(destruction_guard&&) = delete;

    /**
     * @brief Destructor
     *
     * Destroys the guarded object using `std::destroy_at`.
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
    T* p_;
};

/**
 * @brief Convenience function to create an Indestructible object
 * @tparam T The type of object to create
 * @tparam Args The types of constructor arguments
 * @param args Constructor arguments
 * @return Indestructible object containing the constructed object
 */
template <typename T, typename... Args>
    requires std::is_constructible_v<T, Args&&...>
[[nodiscard]] constexpr auto make_indestructible(Args&&... args)
    -> Indestructible<T> {
    return Indestructible<T>(std::in_place, std::forward<Args>(args)...);
}

#endif  // ATOM_TYPE_INDESTRUCTIBLE_HPP
