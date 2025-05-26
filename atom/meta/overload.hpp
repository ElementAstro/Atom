/*!
 * \file overload.hpp
 * \brief Simplified Function Overload Helper with Better Type Deduction
 * \author Max Qian <lightapt.com>
 * \date 2024-04-01
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_OVERLOAD_HPP
#define ATOM_META_OVERLOAD_HPP

#include <type_traits>
#include <utility>

namespace atom::meta {

/**
 * @brief A utility to simplify the casting of overloaded member functions and
 * free functions
 * @tparam Args The argument types of the function to be cast
 */
template <typename... Args>
struct OverloadCast {
    /**
     * @brief Casts a non-const member function
     * @tparam ReturnType The return type of the member function
     * @tparam ClassType The class type of the member function
     * @param func The member function pointer
     * @return The casted member function pointer
     */
    template <typename ReturnType, typename ClassType>
    constexpr auto operator()(
        ReturnType (ClassType::*func)(Args...)) const noexcept {
        return func;
    }

    /**
     * @brief Casts a const member function
     * @tparam ReturnType The return type of the member function
     * @tparam ClassType The class type of the member function
     * @param func The const member function pointer
     * @return The casted const member function pointer
     */
    template <typename ReturnType, typename ClassType>
    constexpr auto operator()(ReturnType (ClassType::*func)(Args...)
                                  const) const noexcept {
        return func;
    }

    /**
     * @brief Casts a volatile member function
     * @tparam ReturnType The return type of the member function
     * @tparam ClassType The class type of the member function
     * @param func The volatile member function pointer
     * @return The casted volatile member function pointer
     */
    template <typename ReturnType, typename ClassType>
    constexpr auto operator()(
        ReturnType (ClassType::*func)(Args...) volatile) const noexcept {
        return func;
    }

    /**
     * @brief Casts a const volatile member function
     * @tparam ReturnType The return type of the member function
     * @tparam ClassType The class type of the member function
     * @param func The const volatile member function pointer
     * @return The casted const volatile member function pointer
     */
    template <typename ReturnType, typename ClassType>
    constexpr auto operator()(ReturnType (ClassType::*func)(Args...)
                                  const volatile) const noexcept {
        return func;
    }

    /**
     * @brief Casts a non-const noexcept member function
     * @tparam ReturnType The return type of the member function
     * @tparam ClassType The class type of the member function
     * @param func The noexcept member function pointer
     * @return The casted noexcept member function pointer
     */
    template <typename ReturnType, typename ClassType>
    constexpr auto operator()(
        ReturnType (ClassType::*func)(Args...) noexcept) const noexcept {
        return func;
    }

    /**
     * @brief Casts a const noexcept member function
     * @tparam ReturnType The return type of the member function
     * @tparam ClassType The class type of the member function
     * @param func The const noexcept member function pointer
     * @return The casted const noexcept member function pointer
     */
    template <typename ReturnType, typename ClassType>
    constexpr auto operator()(ReturnType (ClassType::*func)(Args...)
                                  const noexcept) const noexcept {
        return func;
    }

    /**
     * @brief Casts a volatile noexcept member function
     * @tparam ReturnType The return type of the member function
     * @tparam ClassType The class type of the member function
     * @param func The volatile noexcept member function pointer
     * @return The casted volatile noexcept member function pointer
     */
    template <typename ReturnType, typename ClassType>
    constexpr auto operator()(ReturnType (ClassType::*func)(
        Args...) volatile noexcept) const noexcept {
        return func;
    }

    /**
     * @brief Casts a const volatile noexcept member function
     * @tparam ReturnType The return type of the member function
     * @tparam ClassType The class type of the member function
     * @param func The const volatile noexcept member function pointer
     * @return The casted const volatile noexcept member function pointer
     */
    template <typename ReturnType, typename ClassType>
    constexpr auto operator()(ReturnType (ClassType::*func)(Args...)
                                  const volatile noexcept) const noexcept {
        return func;
    }

    /**
     * @brief Casts a free function
     * @tparam ReturnType The return type of the free function
     * @param func The free function pointer
     * @return The casted free function pointer
     */
    template <typename ReturnType>
    constexpr auto operator()(ReturnType (*func)(Args...)) const noexcept {
        return func;
    }

    /**
     * @brief Casts a noexcept free function
     * @tparam ReturnType The return type of the free function
     * @param func The noexcept free function pointer
     * @return The casted noexcept free function pointer
     */
    template <typename ReturnType>
    constexpr auto operator()(
        ReturnType (*func)(Args...) noexcept) const noexcept {
        return func;
    }

    /**
     * @brief Casts a member function with lvalue reference qualifier
     * @tparam ReturnType The return type of the member function
     * @tparam ClassType The class type of the member function
     * @param func The lvalue reference qualified member function pointer
     * @return The casted member function pointer
     */
    template <typename ReturnType, typename ClassType>
    constexpr auto operator()(
        ReturnType (ClassType::*func)(Args...) &) const noexcept {
        return func;
    }

    /**
     * @brief Casts a member function with rvalue reference qualifier
     * @tparam ReturnType The return type of the member function
     * @tparam ClassType The class type of the member function
     * @param func The rvalue reference qualified member function pointer
     * @return The casted member function pointer
     */
    template <typename ReturnType, typename ClassType>
    constexpr auto operator()(
        ReturnType (ClassType::*func)(Args...) &&) const noexcept {
        return func;
    }

    /**
     * @brief Casts a const member function with lvalue reference qualifier
     * @tparam ReturnType The return type of the member function
     * @tparam ClassType The class type of the member function
     * @param func The const lvalue reference qualified member function pointer
     * @return The casted member function pointer
     */
    template <typename ReturnType, typename ClassType>
    constexpr auto operator()(ReturnType (ClassType::*func)(Args...)
                                  const &) const noexcept {
        return func;
    }

    /**
     * @brief Casts a const member function with rvalue reference qualifier
     * @tparam ReturnType The return type of the member function
     * @tparam ClassType The class type of the member function
     * @param func The const rvalue reference qualified member function pointer
     * @return The casted member function pointer
     */
    template <typename ReturnType, typename ClassType>
    constexpr auto operator()(ReturnType (ClassType::*func)(Args...)
                                  const &&) const noexcept {
        return func;
    }
};

/**
 * @brief Helper variable template to instantiate OverloadCast with improved
 * usability
 * @tparam Args The argument types of the function to be cast
 * @return An instance of OverloadCast with the specified argument types
 */
template <typename... Args>
inline constexpr auto overload_cast = OverloadCast<Args...>{};

/**
 * @brief Creates a decay copy of the given value
 * @tparam T The type of the value to copy
 * @param value The value to copy
 * @return A decay copy of the input value
 */
template <typename T>
constexpr auto decayCopy(T &&value) noexcept(
    std::is_nothrow_convertible_v<T, std::decay_t<T>>) -> std::decay_t<T> {
    return std::forward<T>(value);
}

/**
 * @brief Type trait to check if a type is a function pointer
 * @tparam T The type to check
 */
template <typename T>
struct is_function_pointer : std::false_type {};

template <typename R, typename... Args>
struct is_function_pointer<R (*)(Args...)> : std::true_type {};

template <typename R, typename... Args>
struct is_function_pointer<R (*)(Args...) noexcept> : std::true_type {};

/**
 * @brief Helper variable template for is_function_pointer
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_function_pointer_v = is_function_pointer<T>::value;

/**
 * @brief Type trait to check if a type is a member function pointer
 * @tparam T The type to check
 */
template <typename T>
struct is_member_function_pointer : std::false_type {};

template <typename R, typename C, typename... Args>
struct is_member_function_pointer<R (C::*)(Args...)> : std::true_type {};

template <typename R, typename C, typename... Args>
struct is_member_function_pointer<R (C::*)(Args...) const> : std::true_type {};

template <typename R, typename C, typename... Args>
struct is_member_function_pointer<R (C::*)(Args...) volatile> : std::true_type {
};

template <typename R, typename C, typename... Args>
struct is_member_function_pointer<R (C::*)(Args...) const volatile>
    : std::true_type {};

template <typename R, typename C, typename... Args>
struct is_member_function_pointer<R (C::*)(Args...) noexcept> : std::true_type {
};

template <typename R, typename C, typename... Args>
struct is_member_function_pointer<R (C::*)(Args...) const noexcept>
    : std::true_type {};

template <typename R, typename C, typename... Args>
struct is_member_function_pointer<R (C::*)(Args...) volatile noexcept>
    : std::true_type {};

template <typename R, typename C, typename... Args>
struct is_member_function_pointer<R (C::*)(Args...) const volatile noexcept>
    : std::true_type {};

/**
 * @brief Helper variable template for is_member_function_pointer
 * @tparam T The type to check
 */
template <typename T>
inline constexpr bool is_member_function_pointer_v =
    is_member_function_pointer<T>::value;

}  // namespace atom::meta

#endif  // ATOM_META_OVERLOAD_HPP
