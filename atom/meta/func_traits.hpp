/*!
 * \file func_traits.hpp
 * \brief Function traits for C++20 with comprehensive function type analysis
 * \author Max Qian <lightapt.com>
 * \date 2024-04-02
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_FUNC_TRAITS_HPP
#define ATOM_META_FUNC_TRAITS_HPP

#include <format>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <version>

#include "atom/meta/abi.hpp"
#include "atom/meta/concept.hpp"

// C++23 feature detection
#if __cpp_lib_move_only_function >= 202110L
#include <functional>
#define ATOM_FUNC_TRAITS_HAS_MOVE_ONLY_FUNCTION 1
#else
#define ATOM_FUNC_TRAITS_HAS_MOVE_ONLY_FUNCTION 0
#endif

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

// Variadic function pointer types
template <typename Return, typename... Args>
struct FunctionTraits<Return (*)(Args..., ...)>
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
struct FunctionTraits<Return (Class::*)(Args...)&>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_lvalue_reference_member_function = true;
};

/**
 * \brief Traits for const lvalue reference qualified member function pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) const&>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_const_member_function = true;
    static constexpr bool is_lvalue_reference_member_function = true;
};

/**
 * \brief Traits for volatile lvalue reference qualified member function
 * pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) volatile&>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_volatile_member_function = true;
    static constexpr bool is_lvalue_reference_member_function = true;
};

/**
 * \brief Traits for const volatile lvalue reference qualified member function
 * pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) const volatile&>
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
struct FunctionTraits<Return (Class::*)(Args...) const&&>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_const_member_function = true;
    static constexpr bool is_rvalue_reference_member_function = true;
};

/**
 * \brief Traits for volatile rvalue reference qualified member function
 * pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) volatile&&>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_volatile_member_function = true;
    static constexpr bool is_rvalue_reference_member_function = true;
};

/**
 * \brief Traits for const volatile rvalue reference qualified member function
 * pointers
 */
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) const volatile&&>
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

// noexcept qualified rvalue reference and const volatile variants
template <typename Return, typename Class, typename... Args>
struct FunctionTraits<Return (Class::*)(Args...) const volatile && noexcept>
    : MemberFunctionTraitsBase<Return, Class, Args...> {
    static constexpr bool is_const_member_function = true;
    static constexpr bool is_volatile_member_function = true;
    static constexpr bool is_rvalue_reference_member_function = true;
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
struct FunctionTraits<Func&> : FunctionTraits<Func> {};

/**
 * \brief Traits for function rvalue references
 */
template <typename Func>
struct FunctionTraits<Func&&> : FunctionTraits<Func> {};

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
    return []<typename... Types>(std::tuple<Types...>*) {
        return (std::is_reference_v<Types> || ...);
    }(static_cast<Tuple*>(nullptr));
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
    explicit function_pipe(T&& f) : func_(std::forward<T>(f)) {}

    /**
     * \brief Capture arguments for later invocation
     * \param args Arguments to store
     * \return Reference to this pipe
     */
    auto operator()(Args... args) -> auto& {
        args_ = std::make_tuple(args...);
        return *this;
    }

    /**
     * \brief Pipe operator for function invocation
     * \param arg0 First argument
     * \param pf Function pipe
     * \return Function result
     */
    friend auto operator|(Arg0 arg0, const function_pipe& pf) -> R {
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

//==============================================================================
// C++23 Enhanced Function Traits
//==============================================================================

/**
 * @brief Get function return type as string
 */
template <typename Func>
auto getReturnTypeName() -> std::string {
    return DemangleHelper::demangle(
        typeid(typename FunctionTraits<Func>::return_type).name());
}

/**
 * @brief Get all argument type names as vector of strings
 */
template <typename Func>
auto getArgumentTypeNames() -> std::vector<std::string> {
    using args_tuple = typename FunctionTraits<Func>::argument_types;
    std::vector<std::string> names;
    names.reserve(FunctionTraits<Func>::arity);

    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((names.push_back(DemangleHelper::demangle(
             typeid(std::tuple_element_t<Is, args_tuple>).name()))),
         ...);
    }(std::make_index_sequence<FunctionTraits<Func>::arity>{});

    return names;
}

/**
 * @brief Function signature information for runtime introspection
 */
struct FunctionSignatureInfo {
    std::string return_type;
    std::vector<std::string> argument_types;
    std::size_t arity;
    bool is_member_function;
    bool is_const;
    bool is_noexcept;
    bool is_variadic;

    /**
     * @brief Format as human-readable string
     */
    [[nodiscard]] auto toString() const -> std::string {
        std::string args;
        for (std::size_t i = 0; i < argument_types.size(); ++i) {
            if (i > 0)
                args += ", ";
            args += argument_types[i];
        }
        if (is_variadic) {
            if (!args.empty())
                args += ", ";
            args += "...";
        }

        std::string qualifiers;
        if (is_const)
            qualifiers += " const";
        if (is_noexcept)
            qualifiers += " noexcept";

        return std::format("{}({}){}", return_type, args, qualifiers);
    }
};

/**
 * @brief Get complete function signature information
 */
template <typename Func>
auto getFunctionSignatureInfo() -> FunctionSignatureInfo {
    using Traits = FunctionTraits<Func>;
    return FunctionSignatureInfo{
        .return_type = getReturnTypeName<Func>(),
        .argument_types = getArgumentTypeNames<Func>(),
        .arity = Traits::arity,
        .is_member_function = Traits::is_member_function,
        .is_const = Traits::is_const_member_function,
        .is_noexcept = Traits::is_noexcept,
        .is_variadic = Traits::is_variadic};
}

/**
 * @brief Concept for functions returning void
 */
template <typename Func>
concept VoidReturning =
    std::is_void_v<typename FunctionTraits<Func>::return_type>;

/**
 * @brief Concept for functions with no arguments
 */
template <typename Func>
concept Nullary = (FunctionTraits<Func>::arity == 0);

/**
 * @brief Concept for functions with one argument
 */
template <typename Func>
concept Unary = (FunctionTraits<Func>::arity == 1);

/**
 * @brief Concept for functions with two arguments
 */
template <typename Func>
concept Binary = (FunctionTraits<Func>::arity == 2);

/**
 * @brief Concept for pure functions (const, noexcept, no side effects)
 */
template <typename Func>
concept PureFunction = FunctionTraits<Func>::is_const_member_function &&
                       FunctionTraits<Func>::is_noexcept;

/**
 * @brief Check if function can be invoked with given argument types
 */
template <typename Func, typename... Args>
concept InvocableWith = requires(Func f, Args... args) {
    { std::invoke(f, args...) };
};

/**
 * @brief Transform function arguments through a transformation
 * @tparam Func Original function type
 * @tparam Transform Transformation to apply to arguments
 */
template <typename Func, template <typename> class Transform>
struct TransformArguments {
    using original_args = typename FunctionTraits<Func>::argument_types;

    template <typename Tuple>
    struct TransformTuple;

    template <typename... Args>
    struct TransformTuple<std::tuple<Args...>> {
        using type = std::tuple<Transform<Args>...>;
    };

    using type = typename TransformTuple<original_args>::type;
};

template <typename Func, template <typename> class Transform>
using TransformArguments_t = typename TransformArguments<Func, Transform>::type;

/**
 * @brief Add const to all argument types
 */
template <typename T>
using AddConstArg = std::add_const_t<T>;

/**
 * @brief Add lvalue reference to all argument types
 */
template <typename T>
using AddLvalueRefArg = std::add_lvalue_reference_t<T>;

/**
 * @brief Remove reference from all argument types
 */
template <typename T>
using RemoveRefArg = std::remove_reference_t<T>;

#if ATOM_FUNC_TRAITS_HAS_MOVE_ONLY_FUNCTION
/**
 * @brief Traits for std::move_only_function (C++23)
 */
template <typename Return, typename... Args>
struct FunctionTraits<std::move_only_function<Return(Args...)>>
    : FunctionTraitsBase<Return, Args...> {
    static constexpr bool is_move_only = true;
};

template <typename Return, typename... Args>
struct FunctionTraits<std::move_only_function<Return(Args...) noexcept>>
    : FunctionTraitsBase<Return, Args...> {
    static constexpr bool is_move_only = true;
    static constexpr bool is_noexcept = true;
};
#endif

/**
 * @brief Compile-time function composition type
 */
template <typename F, typename G>
struct ComposedFunction {
    F f;
    G g;

    template <typename... Args>
    constexpr auto operator()(Args&&... args) const
        -> decltype(f(g(std::forward<Args>(args)...))) {
        return f(g(std::forward<Args>(args)...));
    }
};

/**
 * @brief Compose two functions
 */
template <typename F, typename G>
constexpr auto compose(F&& f, G&& g)
    -> ComposedFunction<std::decay_t<F>, std::decay_t<G>> {
    return {std::forward<F>(f), std::forward<G>(g)};
}

/**
 * @brief Partial application helper
 */
template <typename Func, typename... BoundArgs>
class PartialApplication {
    Func func_;
    std::tuple<BoundArgs...> bound_args_;

public:
    constexpr PartialApplication(Func func, BoundArgs... args)
        : func_(std::move(func)), bound_args_(std::move(args)...) {}

    template <typename... FreeArgs>
    constexpr auto operator()(FreeArgs&&... free_args) const
        -> decltype(std::apply(
            func_, std::tuple_cat(bound_args_,
                                  std::forward_as_tuple(
                                      std::forward<FreeArgs>(free_args)...)))) {
        return std::apply(
            func_, std::tuple_cat(bound_args_,
                                  std::forward_as_tuple(
                                      std::forward<FreeArgs>(free_args)...)));
    }
};

/**
 * @brief Create a partial application
 */
template <typename Func, typename... Args>
constexpr auto partial(Func&& func, Args&&... args)
    -> PartialApplication<std::decay_t<Func>, std::decay_t<Args>...> {
    return {std::forward<Func>(func), std::forward<Args>(args)...};
}

/**
 * @brief Curry a binary function
 */
template <Binary Func>
class CurriedBinary {
    Func func_;

public:
    constexpr explicit CurriedBinary(Func func) : func_(std::move(func)) {}

    template <typename Arg1>
    constexpr auto operator()(Arg1&& arg1) const {
        return [func = func_, a1 = std::forward<Arg1>(arg1)](auto&& arg2) {
            return func(a1, std::forward<decltype(arg2)>(arg2));
        };
    }
};

/**
 * @brief Curry a binary function
 */
template <Binary Func>
constexpr auto curry(Func&& func) -> CurriedBinary<std::decay_t<Func>> {
    return CurriedBinary<std::decay_t<Func>>{std::forward<Func>(func)};
}

//==============================================================================
// Advanced Function Utilities
//==============================================================================

/**
 * @brief Memoize a function with cache
 */
template <typename Func>
class Memoizer {
    Func func_;
    mutable std::unordered_map<std::size_t, std::any> cache_;
    mutable std::shared_mutex mutex_;

    template <typename... Args>
    static std::size_t computeHash(Args&&... args) {
        std::size_t seed = 0;
        ((seed ^= std::hash<std::decay_t<Args>>{}(args) + 0x9e3779b9 +
                  (seed << 6) + (seed >> 2)),
         ...);
        return seed;
    }

public:
    explicit Memoizer(Func func) : func_(std::move(func)) {}

    template <typename... Args>
    auto operator()(Args&&... args) const {
        using ReturnType = std::invoke_result_t<Func, Args...>;
        auto hash = computeHash(std::forward<Args>(args)...);

        {
            std::shared_lock lock(mutex_);
            if (auto it = cache_.find(hash); it != cache_.end()) {
                return std::any_cast<ReturnType>(it->second);
            }
        }

        auto result = func_(std::forward<Args>(args)...);
        {
            std::unique_lock lock(mutex_);
            cache_[hash] = result;
        }
        return result;
    }

    void clearCache() {
        std::unique_lock lock(mutex_);
        cache_.clear();
    }

    [[nodiscard]] std::size_t cacheSize() const {
        std::shared_lock lock(mutex_);
        return cache_.size();
    }
};

/**
 * @brief Create a memoized function
 */
template <typename Func>
auto memoize(Func&& func) {
    return Memoizer<std::decay_t<Func>>(std::forward<Func>(func));
}

/**
 * @brief Function with timeout
 */
template <typename Func>
class TimedFunction {
    Func func_;
    std::chrono::milliseconds timeout_;

public:
    TimedFunction(Func func, std::chrono::milliseconds timeout)
        : func_(std::move(func)), timeout_(timeout) {}

    template <typename... Args>
    auto operator()(Args&&... args) const
        -> std::optional<std::invoke_result_t<Func, Args...>> {
        using ReturnType = std::invoke_result_t<Func, Args...>;

        std::packaged_task<ReturnType()> task(
            [&]() { return func_(std::forward<Args>(args)...); });

        auto future = task.get_future();
        std::thread t(std::move(task));

        if (future.wait_for(timeout_) == std::future_status::timeout) {
            t.detach();
            return std::nullopt;
        }

        t.join();
        return future.get();
    }
};

/**
 * @brief Create a timed function
 */
template <typename Func>
auto withTimeout(Func&& func, std::chrono::milliseconds timeout) {
    return TimedFunction<std::decay_t<Func>>(std::forward<Func>(func), timeout);
}

/**
 * @brief Rate limiter for function calls
 */
template <typename Func>
class RateLimiter {
    Func func_;
    std::chrono::milliseconds interval_;
    mutable std::chrono::steady_clock::time_point last_call_;
    mutable std::mutex mutex_;

public:
    RateLimiter(Func func, std::chrono::milliseconds interval)
        : func_(std::move(func)),
          interval_(interval),
          last_call_(std::chrono::steady_clock::time_point::min()) {}

    template <typename... Args>
    auto operator()(Args&&... args) const
        -> std::optional<std::invoke_result_t<Func, Args...>> {
        std::lock_guard lock(mutex_);
        auto now = std::chrono::steady_clock::now();

        if (now - last_call_ < interval_) {
            return std::nullopt;  // Rate limited
        }

        last_call_ = now;
        return func_(std::forward<Args>(args)...);
    }

    void reset() {
        std::lock_guard lock(mutex_);
        last_call_ = std::chrono::steady_clock::time_point::min();
    }
};

/**
 * @brief Create a rate-limited function
 */
template <typename Func>
auto rateLimit(Func&& func, std::chrono::milliseconds interval) {
    return RateLimiter<std::decay_t<Func>>(std::forward<Func>(func), interval);
}

/**
 * @brief Function that retries on failure
 */
template <typename Func>
class RetryFunction {
    Func func_;
    std::size_t max_retries_;
    std::chrono::milliseconds delay_;

public:
    RetryFunction(Func func, std::size_t retries,
                  std::chrono::milliseconds delay)
        : func_(std::move(func)), max_retries_(retries), delay_(delay) {}

    template <typename... Args>
    auto operator()(Args&&... args) const
        -> std::optional<std::invoke_result_t<Func, Args...>> {
        for (std::size_t attempt = 0; attempt < max_retries_; ++attempt) {
            try {
                return func_(std::forward<Args>(args)...);
            } catch (...) {
                if (attempt + 1 < max_retries_) {
                    std::this_thread::sleep_for(delay_);
                }
            }
        }
        return std::nullopt;
    }
};

/**
 * @brief Create a retry function
 */
template <typename Func>
auto withRetry(Func&& func, std::size_t retries = 3,
               std::chrono::milliseconds delay = std::chrono::milliseconds{
                   100}) {
    return RetryFunction<std::decay_t<Func>>(std::forward<Func>(func), retries,
                                             delay);
}

/**
 * @brief Function pipeline builder
 */
template <typename... Funcs>
class Pipeline {
    std::tuple<Funcs...> funcs_;

    template <std::size_t I, typename Arg>
    auto callAt(Arg&& arg) const {
        if constexpr (I == sizeof...(Funcs) - 1) {
            return std::get<I>(funcs_)(std::forward<Arg>(arg));
        } else {
            return callAt<I + 1>(std::get<I>(funcs_)(std::forward<Arg>(arg)));
        }
    }

public:
    constexpr Pipeline(Funcs... funcs) : funcs_(std::move(funcs)...) {}

    template <typename Arg>
    auto operator()(Arg&& arg) const {
        return callAt<0>(std::forward<Arg>(arg));
    }

    template <typename NextFunc>
    auto then(NextFunc&& next) const {
        return std::apply(
            [&next](auto&&... fs) {
                return Pipeline<Funcs..., std::decay_t<NextFunc>>(
                    std::forward<decltype(fs)>(fs)...,
                    std::forward<NextFunc>(next));
            },
            funcs_);
    }
};

/**
 * @brief Create a function pipeline
 */
template <typename... Funcs>
auto pipeline(Funcs&&... funcs) {
    return Pipeline<std::decay_t<Funcs>...>(std::forward<Funcs>(funcs)...);
}

/**
 * @brief Y-combinator for recursive lambdas
 */
template <typename F>
class YCombinator {
    F func_;

public:
    explicit YCombinator(F func) : func_(std::move(func)) {}

    template <typename... Args>
    decltype(auto) operator()(Args&&... args) const {
        return func_(*this, std::forward<Args>(args)...);
    }
};

/**
 * @brief Create a Y-combinator
 */
template <typename F>
auto fix(F&& func) {
    return YCombinator<std::decay_t<F>>(std::forward<F>(func));
}

/**
 * @brief Get function signature info
 */
template <typename Func>
auto getSignatureInfo() -> FunctionSignatureInfo {
    using Traits = FunctionTraits<Func>;

    FunctionSignatureInfo info;
    info.return_type = typeid(typename Traits::return_type).name();
    info.arity = Traits::arity;
    info.is_noexcept = Traits::is_noexcept;
    info.is_const = Traits::is_const;
    info.is_variadic = Traits::is_variadic;

    // Get parameter types - use argument_types member from earlier definition
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ((info.argument_types.push_back(
             typeid(typename Traits::template argument_t<Is>).name())),
         ...);
    }(std::make_index_sequence<Traits::arity>{});

    return info;
}

/**
 * @brief Function call counter
 */
template <typename Func>
class CountedFunction {
    Func func_;
    mutable std::atomic<std::size_t> call_count_{0};

public:
    explicit CountedFunction(Func func) : func_(std::move(func)) {}

    template <typename... Args>
    auto operator()(Args&&... args) const {
        ++call_count_;
        return func_(std::forward<Args>(args)...);
    }

    [[nodiscard]] std::size_t callCount() const { return call_count_.load(); }

    void resetCount() { call_count_.store(0); }
};

/**
 * @brief Create a counted function
 */
template <typename Func>
auto counted(Func&& func) {
    return CountedFunction<std::decay_t<Func>>(std::forward<Func>(func));
}

}  // namespace atom::meta

#endif  // ATOM_META_FUNC_TRAITS_HPP
