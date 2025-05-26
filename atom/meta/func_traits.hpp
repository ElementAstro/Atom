/*!
 * \file func_traits.hpp
 * \brief Function traits for C++20 with comprehensive function type analysis
 * \author Max Qian <lightapt.com>
 * \date 2024-04-02
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_FUNC_TRAITS_HPP
#define ATOM_META_FUNC_TRAITS_HPP

#include "atom/meta/abi.hpp"
#include "atom/meta/concept.hpp"

namespace atom::meta {

/**
 * \brief Primary template for function traits
 * \tparam Func Function type to analyze
 */
template <typename Func>
struct FunctionTraits;

/**
 * \brief Base traits for function types
 * \tparam Return Return type
 * \tparam Args Argument types
 */
template <typename Return, typename... Args>
struct FunctionTraitsBase {
    using return_type = Return;
    using argument_types = std::tuple<Args...>;
    static constexpr std::size_t arity = sizeof...(Args);

    template <std::size_t N>
        requires(N < arity)
    using argument_t = std::tuple_element_t<N, argument_types>;

    static constexpr bool is_member_function = false;
    static constexpr bool is_const_member_function = false;
    static constexpr bool is_volatile_member_function = false;
    static constexpr bool is_lvalue_reference_member_function = false;
    static constexpr bool is_rvalue_reference_member_function = false;
    static constexpr bool is_noexcept = false;
    static constexpr bool is_variadic = false;

    static const inline std::string full_name =
        DemangleHelper::demangle(typeid(Return(Args...)).name());
};

/**
 * \brief Traits for regular function types
 */
template <typename Return, typename... Args>
struct FunctionTraits<Return(Args...)> : FunctionTraitsBase<Return, Args...> {};

/**
 * \brief Traits for const function types
 */
template <typename Return, typename... Args>
struct FunctionTraits<Return(Args...) const>
    : FunctionTraitsBase<Return, Args...> {
    static constexpr bool is_const_member_function = true;
};

/**
 * \brief Traits for noexcept function types
 */
template <typename Return, typename... Args>
struct FunctionTraits<Return(Args...) noexcept>
    : FunctionTraitsBase<Return, Args...> {
    static constexpr bool is_noexcept = true;
};

/**
 * \brief Traits for const noexcept function types
 */
template <typename Return, typename... Args>
struct FunctionTraits<Return(Args...) const noexcept>
    : FunctionTraitsBase<Return, Args...> {
    static constexpr bool is_const_member_function = true;
    static constexpr bool is_noexcept = true;
};

/**
 * \brief Traits for variadic function types
 */
template <typename Return, typename... Args>
struct FunctionTraits<Return(Args..., ...)>
    : FunctionTraitsBase<Return, Args...> {
    static constexpr bool is_variadic = true;
};

/**
 * \brief Traits for variadic noexcept function types
 */
template <typename Return, typename... Args>
struct FunctionTraits<Return(Args..., ...) noexcept>
    : FunctionTraitsBase<Return, Args...> {
    static constexpr bool is_variadic = true;
    static constexpr bool is_noexcept = true;
};

/**
 * \brief Traits for std::function types
 */
template <typename Return, typename... Args>
struct FunctionTraits<std::function<Return(Args...)>>
    : FunctionTraitsBase<Return, Args...> {};

/**
 * \brief Traits for function pointer types
 */
template <typename Return, typename... Args>
struct FunctionTraits<Return (*)(Args...)>
    : FunctionTraitsBase<Return, Args...> {};

/**
 * \brief Traits for noexcept function pointer types
 */
template <typename Return, typename... Args>
struct FunctionTraits<Return (*)(Args...) noexcept>
    : FunctionTraitsBase<Return, Args...> {
    static constexpr bool is_noexcept = true;
};

/**
 * \brief Base traits for member function pointers
 * \tparam Return Return type
 * \tparam Class Class type
 * \tparam Args Argument types
 */
template <typename Return, typename Class, typename... Args>
struct MemberFunctionTraitsBase : FunctionTraitsBase<Return, Args...> {
    using class_type = Class;
    static constexpr bool is_member_function = true;
};

/**
 * \brief Traits for member function pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...)>
    : MemberFunctionTraitsBase<Return, Class, Args...> {};

/**
 * \brief Traits for const member function pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) const>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_const_member_function = true;
};

/**
 * \brief Traits for volatile member function pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) volatile>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_volatile_member_function = true;
};

/**
 * \brief Traits for const volatile member function pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) const volatile>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_const_member_function = true;
    static constexpr bool is_volatile_member_function = true;
};

/**
 * \brief Traits for lvalue reference qualified member function pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) &>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_lvalue_reference_member_function = true;
};

/**
 * \brief Traits for const lvalue reference qualified member function pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) const &>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_const_member_function = true;
    static constexpr bool is_lvalue_reference_member_function = true;
};

/**
 * \brief Traits for volatile lvalue reference qualified member function
 * pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) volatile &>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_volatile_member_function = true;
    static constexpr bool is_lvalue_reference_member_function = true;
};

/**
 * \brief Traits for const volatile lvalue reference qualified member function
 * pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) const volatile &>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_const_member_function = true;
    static constexpr bool is_volatile_member_function = true;
    static constexpr bool is_lvalue_reference_member_function = true;
};

/**
 * \brief Traits for rvalue reference qualified member function pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) &&>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_rvalue_reference_member_function = true;
};

/**
 * \brief Traits for const rvalue reference qualified member function pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) const &&>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_const_member_function = true;
    static constexpr bool is_rvalue_reference_member_function = true;
};

/**
 * \brief Traits for volatile rvalue reference qualified member function
 * pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) volatile &&>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_volatile_member_function = true;
    static constexpr bool is_rvalue_reference_member_function = true;
};

/**
 * \brief Traits for const volatile rvalue reference qualified member function
 * pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) const volatile &&>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_const_member_function = true;
    static constexpr bool is_volatile_member_function = true;
    static constexpr bool is_rvalue_reference_member_function = true;
};

/**
 * \brief Traits for noexcept member function pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) noexcept>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_noexcept = true;
};

/**
 * \brief Traits for const noexcept member function pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) const noexcept>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_const_member_function = true;
    static constexpr bool is_noexcept = true;
};

/**
 * \brief Traits for volatile noexcept member function pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) volatile noexcept>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_volatile_member_function = true;
    static constexpr bool is_noexcept = true;
};

/**
 * \brief Traits for const volatile noexcept member function pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) const volatile noexcept>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_const_member_function = true;
    static constexpr bool is_volatile_member_function = true;
    static constexpr bool is_noexcept = true;
};

/**
 * \brief Traits for lvalue reference qualified noexcept member function
 * pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) & noexcept>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_lvalue_reference_member_function = true;
    static constexpr bool is_noexcept = true;
};

/**
 * \brief Traits for const lvalue reference qualified noexcept member function
 * pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) const & noexcept>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_const_member_function = true;
    static constexpr bool is_lvalue_reference_member_function = true;
    static constexpr bool is_noexcept = true;
};

/**
 * \brief Traits for rvalue reference qualified noexcept member function
 * pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) && noexcept>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_rvalue_reference_member_function = true;
    static constexpr bool is_noexcept = true;
};

/**
 * \brief Traits for const rvalue reference qualified noexcept member function
 * pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) const && noexcept>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_const_member_function = true;
    static constexpr bool is_rvalue_reference_member_function = true;
    static constexpr bool is_noexcept = true;
};

/**
 * \brief Traits for callable objects with operator()
 */
template <typename Func>
    requires requires { &std::remove_cvref_t<Func>::operator(); }
struct FunctionTraits<Func>
    : FunctionTraits<decltype(&std::remove_cvref_t<Func>::operator())> {};

/**
 * \brief Traits for function references
 */
template <typename Func>
struct FunctionTraits<Func &> : FunctionTraits<Func> {};

/**
 * \brief Traits for function rvalue references
 */
template <typename Func>
struct FunctionTraits<Func &&> : FunctionTraits<Func> {};

/**
 * \brief Variable template for member function check
 */
template <typename Func>
inline constexpr bool is_member_function_v =
    FunctionTraits<Func>::is_member_function;

/**
 * \brief Variable template for const member function check
 */
template <typename Func>
inline constexpr bool is_const_member_function_v =
    FunctionTraits<Func>::is_const_member_function;

/**
 * \brief Variable template for volatile member function check
 */
template <typename Func>
inline constexpr bool is_volatile_member_function_v =
    FunctionTraits<Func>::is_volatile_member_function;

/**
 * \brief Variable template for lvalue reference qualified member function check
 */
template <typename Func>
inline constexpr bool is_lvalue_reference_member_function_v =
    FunctionTraits<Func>::is_lvalue_reference_member_function;

/**
 * \brief Variable template for rvalue reference qualified member function check
 */
template <typename Func>
inline constexpr bool is_rvalue_reference_member_function_v =
    FunctionTraits<Func>::is_rvalue_reference_member_function;

/**
 * \brief Variable template for noexcept function check
 */
template <typename Func>
inline constexpr bool is_noexcept_v = FunctionTraits<Func>::is_noexcept;

/**
 * \brief Variable template for variadic function check
 */
template <typename Func>
inline constexpr bool is_variadic_v = FunctionTraits<Func>::is_variadic;

/**
 * \brief Check if a tuple contains any reference types
 * \tparam Tuple Tuple type to check
 * \return true if any element is a reference type
 */
template <typename Tuple>
constexpr auto tuple_has_reference() -> bool {
    return []<typename... Types>(std::tuple<Types...> *) {
        return (std::is_reference_v<Types> || ...);
    }(static_cast<Tuple *>(nullptr));
}

/**
 * \brief Check if a function has any reference arguments
 * \tparam Func Function type to check
 * \return true if any argument is a reference type
 */
template <typename Func>
constexpr auto has_reference_argument() -> bool {
    using args_tuple = typename FunctionTraits<Func>::argument_types;
    return tuple_has_reference<args_tuple>();
}

/**
 * \brief Function pipe class for functional composition
 * \tparam Func Function type
 */
template <typename Func>
class function_pipe;

/**
 * \brief Specialization for function pipe with specific signature
 * \tparam R Return type
 * \tparam Arg0 First argument type
 * \tparam Args Remaining argument types
 */
template <typename R, typename Arg0, typename... Args>
class function_pipe<R(Arg0, Args...)> {
    std::function<R(Arg0, Args...)> func_;
    std::tuple<Args...> args_;

public:
    /**
     * \brief Constructor accepting any callable type
     * \tparam T Callable type
     * \param f Callable object
     */
    template <Callable T>
    explicit function_pipe(T &&f) : func_(std::forward<T>(f)) {}

    /**
     * \brief Capture arguments for later invocation
     * \param args Arguments to store
     * \return Reference to this pipe
     */
    auto operator()(Args... args) -> auto & {
        args_ = std::make_tuple(args...);
        return *this;
    }

    /**
     * \brief Pipe operator for function invocation
     * \param arg0 First argument
     * \param pf Function pipe
     * \return Function result
     */
    friend auto operator|(Arg0 arg0, const function_pipe &pf) -> R {
        return std::apply(pf.func_,
                          std::tuple_cat(std::make_tuple(arg0), pf.args_));
    }
};

/**
 * \brief Deduction guide for function pipe
 */
template <Callable T>
function_pipe(T) -> function_pipe<typename FunctionTraits<T>::return_type(
    typename std::tuple_element<
        0, typename FunctionTraits<T>::argument_types>::type,
    typename std::tuple_element<
        1, typename FunctionTraits<T>::argument_types>::type)>;

/**
 * \brief Primary template to detect non-static member function
 * \tparam T Class type
 * \tparam Signature Function signature
 * \tparam Enable SFINAE helper
 */
template <typename T, typename Signature, typename = void>
struct has_method : std::false_type {};

/**
 * \brief Specialization to detect non-static member function
 */
template <typename T, typename Ret, typename... Args>
struct has_method<
    T, Ret(Args...),
    std::void_t<decltype(std::declval<T>().method(std::declval<Args>()...))>>
    : std::true_type {};

/**
 * \brief Primary template to detect static member function
 * \tparam T Class type
 * \tparam Signature Function signature
 * \tparam Enable SFINAE helper
 */
template <typename T, typename Signature, typename = void>
struct has_static_method : std::false_type {};

/**
 * \brief Specialization to detect static member function
 */
template <typename T, typename Ret, typename... Args>
struct has_static_method<
    T, Ret(Args...),
    std::void_t<decltype(T::static_method(std::declval<Args>()...))>>
    : std::true_type {};

/**
 * \brief Primary template to detect const member function
 * \tparam T Class type
 * \tparam Signature Function signature
 * \tparam Enable SFINAE helper
 */
template <typename T, typename Signature, typename = void>
struct has_const_method : std::false_type {};

/**
 * \brief Specialization to detect const member function
 */
template <typename T, typename Ret, typename... Args>
struct has_const_method<T, Ret(Args...) const,
                        std::void_t<decltype(std::declval<const T>().method(
                            std::declval<Args>()...))>> : std::true_type {};

/**
 * \brief Macro to define a check for a specific method name
 * \param MethodName Name of the method to check for
 */
#define DEFINE_HAS_METHOD(MethodName)                                          \
    template <typename T, typename Ret, typename... Args>                      \
    struct has_##MethodName {                                                  \
        template <typename U>                                                  \
        static auto test(int)                                                  \
            -> decltype(std::declval<U>().MethodName(std::declval<Args>()...), \
                        std::true_type());                                     \
                                                                               \
        template <typename>                                                    \
        static std::false_type test(...);                                      \
                                                                               \
        static constexpr bool value = decltype(test<T>(0))::value;             \
    }

/**
 * \brief Macro to define a check for a specific static method name
 * \param MethodName Name of the static method to check for
 */
#define DEFINE_HAS_STATIC_METHOD(MethodName)                       \
    template <typename T, typename Ret, typename... Args>          \
    struct has_static_##MethodName {                               \
        template <typename U>                                      \
        static auto test(int)                                      \
            -> decltype(U::MethodName(std::declval<Args>()...),    \
                        std::true_type());                         \
                                                                   \
        template <typename>                                        \
        static std::false_type test(...);                          \
                                                                   \
        static constexpr bool value = decltype(test<T>(0))::value; \
    }

/**
 * \brief Macro to define a check for a specific const method name
 * \param MethodName Name of the const method to check for
 */
#define DEFINE_HAS_CONST_METHOD(MethodName)                                   \
    template <typename T, typename Ret, typename... Args>                     \
    struct has_const_##MethodName {                                           \
        template <typename U>                                                 \
        static auto test(int) -> decltype(std::declval<const U>().MethodName( \
                                              std::declval<Args>()...),       \
                                          std::true_type());                  \
                                                                              \
        template <typename>                                                   \
        static std::false_type test(...);                                     \
                                                                              \
        static constexpr bool value = decltype(test<T>(0))::value;            \
    }

}  // namespace atom::meta

#endif  // ATOM_META_FUNC_TRAITS_HPP
