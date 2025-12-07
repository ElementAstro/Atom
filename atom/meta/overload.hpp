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

//==============================================================================
// C++23 Enhanced Overload Utilities
//==============================================================================

/**
 * @brief Concept for overloadable callables
 */
template <typename T>
concept Overloadable =
    std::is_invocable_v<T> || std::is_member_function_pointer_v<T> ||
    std::is_function_v<std::remove_pointer_t<T>>;

/**
 * @brief Overload set for multiple callable types
 */
template <typename... Fs>
struct overload_set : Fs... {
    using Fs::operator()...;

    constexpr overload_set(Fs... fs) : Fs(std::move(fs))... {}
};

// Deduction guide
template <typename... Fs>
overload_set(Fs...) -> overload_set<Fs...>;

/**
 * @brief Create an overload set
 */
template <typename... Fs>
constexpr auto make_overload(Fs &&...fs) {
    return overload_set<std::decay_t<Fs>...>(std::forward<Fs>(fs)...);
}

/**
 * @brief Select overload by return type
 */
template <typename Return>
struct return_type_selector {
    template <typename... Args>
    constexpr auto operator()(Return (*func)(Args...)) const noexcept {
        return func;
    }

    template <typename Class, typename... Args>
    constexpr auto operator()(Return (Class::*func)(Args...)) const noexcept {
        return func;
    }

    template <typename Class, typename... Args>
    constexpr auto operator()(Return (Class::*func)(Args...)
                                  const) const noexcept {
        return func;
    }
};

/**
 * @brief Select overload by return type helper
 */
template <typename Return>
inline constexpr return_type_selector<Return> select_return{};

/**
 * @brief Select overload by argument count
 */
template <std::size_t N>
struct arity_selector {
    template <typename Return, typename... Args>
        requires(sizeof...(Args) == N)
    constexpr auto operator()(Return (*func)(Args...)) const noexcept {
        return func;
    }

    template <typename Return, typename Class, typename... Args>
        requires(sizeof...(Args) == N)
    constexpr auto operator()(Return (Class::*func)(Args...)) const noexcept {
        return func;
    }
};

/**
 * @brief Select overload by arity helper
 */
template <std::size_t N>
inline constexpr arity_selector<N> select_arity{};

/**
 * @brief Combined argument and return type selector
 */
template <typename Return, typename... Args>
struct exact_signature_selector {
    constexpr auto operator()(Return (*func)(Args...)) const noexcept {
        return func;
    }

    template <typename Class>
    constexpr auto operator()(Return (Class::*func)(Args...)) const noexcept {
        return func;
    }

    template <typename Class>
    constexpr auto operator()(Return (Class::*func)(Args...)
                                  const) const noexcept {
        return func;
    }
};

/**
 * @brief Select exact signature helper
 */
template <typename Return, typename... Args>
inline constexpr exact_signature_selector<Return, Args...> select_exact{};

/**
 * @brief Overload resolution helper using type list
 */
template <typename Signature>
struct signature_selector;

template <typename Return, typename... Args>
struct signature_selector<Return(Args...)> {
    constexpr auto operator()(Return (*func)(Args...)) const noexcept {
        return func;
    }
};

/**
 * @brief Select by signature type
 */
template <typename Signature>
inline constexpr signature_selector<Signature> select_signature{};

}  // namespace atom::meta

#endif  // ATOM_META_OVERLOAD_HPP
