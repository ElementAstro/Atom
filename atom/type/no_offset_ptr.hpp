/*
 * no_offset_ptr.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_TYPE_NO_OFFSET_PTR_HPP
#define ATOM_TYPE_NO_OFFSET_PTR_HPP

#include <cassert>
#include <type_traits>
#include <utility>

#ifdef ATOM_USE_BOOST
#include <boost/type_traits/aligned_storage.hpp>
#include <boost/type_traits/is_constructible.hpp>
#include <boost/type_traits/is_nothrow_move_assignable.hpp>
#include <boost/type_traits/is_nothrow_move_constructible.hpp>
#include <boost/type_traits/is_object.hpp>
#endif

/**
 * @brief A lightweight pointer-like class that manages an object of type T
 * without dynamic memory allocation.
 *
 * @tparam T The type of the object to manage.
 */
template <typename T>
#ifdef ATOM_USE_BOOST
    requires boost::is_object<T>::value
#else
    requires std::is_object_v<T>
#endif
class UnshiftedPtr {
public:
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
        new (&storage_) T();
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
        new (&storage_) T(std::forward<Args>(args)...);
    }

    /**
     * @brief Destructor. Destroys the managed object.
     */
    constexpr ~UnshiftedPtr() noexcept(
#ifdef ATOM_USE_BOOST
        boost::is_nothrow_destructible<T>::value
#else
        std::is_nothrow_destructible_v<T>
#endif
    ) {
        destroy();
    }

    // Disable copying and moving
    UnshiftedPtr(const UnshiftedPtr&) = delete;
    UnshiftedPtr(UnshiftedPtr&&) = delete;
    auto operator=(const UnshiftedPtr&) -> UnshiftedPtr& = delete;
    auto operator=(UnshiftedPtr&&) -> UnshiftedPtr& = delete;

    /**
     * @brief Provides pointer-like access to the managed object.
     *
     * @return A pointer to the managed object.
     */
    constexpr auto operator->() noexcept -> T* { return get_ptr(); }

    /**
     * @brief Provides const pointer-like access to the managed object.
     *
     * @return A const pointer to the managed object.
     */
    constexpr auto operator->() const noexcept -> const T* { return get_ptr(); }

    /**
     * @brief Dereferences the managed object.
     *
     * @return A reference to the managed object.
     */
    constexpr auto operator*() noexcept -> T& { return get(); }

    /**
     * @brief Dereferences the managed object.
     *
     * @return A const reference to the managed object.
     */
    constexpr auto operator*() const noexcept -> const T& { return get(); }

    /**
     * @brief Resets the managed object by calling its destructor and
     * reconstructing it in-place.
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
    constexpr void reset(Args&&... args) noexcept(
#ifdef ATOM_USE_BOOST
        boost::is_nothrow_destructible<T>::value &&
        boost::is_nothrow_constructible<T, Args&&...>::value
#else
        std::is_nothrow_destructible_v<T> &&
        std::is_nothrow_constructible_v<T, Args...>
#endif
    ) {
        destroy();
        new (&storage_) T(std::forward<Args>(args)...);
    }

    /**
     * @brief Emplaces a new object in place with the provided arguments.
     *
     * @tparam Args The types of the arguments to construct the object with.
     * @param args The arguments to construct the object with.
     */
    template <typename... Args>
#ifdef ATOM_USE_BOOST
        requires boost::is_constructible<T, Args&&...>::value
#else
        requires std::constructible_from<T, Args...>
#endif
    constexpr void emplace(Args&&... args) noexcept(
#ifdef ATOM_USE_BOOST
        boost::is_nothrow_constructible<T, Args&&...>::value
#else
        std::is_nothrow_constructible_v<T, Args...>
#endif
    ) {
        reset(std::forward<Args>(args)...);
    }

    /**
     * @brief Releases ownership of the managed object without destroying it.
     *
     * @return A pointer to the managed object.
     */
    [[nodiscard]] constexpr auto release() noexcept -> T* {
        T* ptr = get_ptr();
        relinquish_ownership();
        return ptr;
    }

    /**
     * @brief Checks if the managed object has a value.
     *
     * @return True if the managed object has a value, false otherwise.
     */
    [[nodiscard]] constexpr auto hasValue() const noexcept -> bool {
        return owns_;
    }

private:
    /**
     * @brief Retrieves a pointer to the managed object.
     *
     * @return A pointer to the managed object.
     */
    constexpr auto get_ptr() noexcept -> T* {
        return reinterpret_cast<T*>(&storage_);
    }

    /**
     * @brief Retrieves a const pointer to the managed object.
     *
     * @return A const pointer to the managed object.
     */
    constexpr auto get_ptr() const noexcept -> const T* {
        return reinterpret_cast<const T*>(&storage_);
    }

    /**
     * @brief Retrieves a reference to the managed object.
     *
     * @return A reference to the managed object.
     */
    constexpr auto get() noexcept -> T& { return *get_ptr(); }

    /**
     * @brief Retrieves a const reference to the managed object.
     *
     * @return A const reference to the managed object.
     */
    constexpr auto get() const noexcept -> const T& { return *get_ptr(); }

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
        if (owns_) {
            get().~T();
            owns_ = false;
        }
    }

    /**
     * @brief Relinquishes ownership without destroying the object.
     */
    constexpr void relinquish_ownership() noexcept { owns_ = false; }

#ifdef ATOM_USE_BOOST
    using StorageType =
        typename boost::aligned_storage<sizeof(T), alignof(T)>::type;
#else
    using StorageType = std::aligned_storage_t<sizeof(T), alignof(T)>;
#endif

    StorageType storage_;
    bool owns_{true};
};

#endif  // ATOM_TYPE_NO_OFFSET_PTR_HPP