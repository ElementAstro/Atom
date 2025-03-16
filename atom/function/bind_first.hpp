/*!
 * \file bind_first.hpp
 * \brief An enhanced utility for binding functions to objects
 * \author Max Qian <lightapt.com>
 * \date 2024-03-12
 * \copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef ATOM_META_BIND_FIRST_HPP
#define ATOM_META_BIND_FIRST_HPP

#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <source_location>
#include <string_view>
#include <utility>

#include "atom/function/concept.hpp"

namespace atom::meta {

// Core pointer and reference manipulation utilities
//--------------------------------------------------

/**
 * @brief Get a pointer from a raw pointer
 * @tparam T The pointee type
 * @param ptr The input pointer
 * @return The same pointer
 */
template <typename T>
[[nodiscard]] constexpr auto getPointer(T* ptr) noexcept -> T* {
    return ptr;
}

/**
 * @brief Get a pointer from a reference_wrapper
 * @tparam T The reference type
 * @param ref The reference wrapper
 * @return Pointer to the referenced object
 */
template <typename T>
[[nodiscard]] constexpr auto getPointer(
    const std::reference_wrapper<T>& ref) noexcept -> T* {
    return &ref.get();
}

/**
 * @brief Get a pointer from an object
 * @tparam T The object type
 * @param ref The object
 * @return Pointer to the object
 */
template <typename T>
[[nodiscard]] constexpr auto getPointer(const T& ref) noexcept -> const T* {
    return &ref;
}

/**
 * @brief Remove const from a pointer
 * @tparam T The pointee type
 * @param ptr Const pointer
 * @return Non-const pointer
 */
template <typename T>
[[nodiscard]] constexpr auto removeConstPointer(const T* ptr) noexcept -> T* {
    return const_cast<T*>(ptr);
}

// Primary bind_first implementation
//----------------------------------

/**
 * @brief Bind an object to a function pointer as first argument
 * @tparam O Object type
 * @tparam Ret Return type
 * @tparam P1 First parameter type
 * @tparam Param Remaining parameter types
 * @param func Function to bind
 * @param object Object to bind as first argument
 * @return Bound function
 */
template <typename O, typename Ret, typename P1, typename... Param>
    requires Invocable<Ret (*)(P1, Param...), O, Param...>
[[nodiscard]] constexpr auto bindFirst(Ret (*func)(P1, Param...), O&& object) {
    return [func, object = std::forward<O>(object)](Param... param) -> Ret {
        return func(object, std::forward<Param>(param)...);
    };
}

/**
 * @brief Bind an object to a member function
 * @tparam O Object type
 * @tparam Ret Return type
 * @tparam Class Class type
 * @tparam Param Parameter types
 * @param func Member function to bind
 * @param object Object to bind the function to
 * @return Bound function
 */
template <typename O, typename Ret, typename Class, typename... Param>
    requires Invocable<Ret (Class::*)(Param...), O, Param...>
[[nodiscard]] constexpr auto bindFirst(Ret (Class::*func)(Param...),
                                       O&& object) {
    return [func, object = std::forward<O>(object)](Param... param) -> Ret {
        return (removeConstPointer(getPointer(object))->*func)(
            std::forward<Param>(param)...);
    };
}

/**
 * @brief Bind an object to a const member function
 * @tparam O Object type
 * @tparam Ret Return type
 * @tparam Class Class type
 * @tparam Param Parameter types
 * @param func Const member function to bind
 * @param object Object to bind the function to
 * @return Bound function
 */
template <typename O, typename Ret, typename Class, typename... Param>
    requires Invocable<Ret (Class::*)(Param...) const, O, Param...>
[[nodiscard]] constexpr auto bindFirst(Ret (Class::*func)(Param...) const,
                                       O&& object) {
    return [func, object = std::forward<O>(object)](Param... param) -> Ret {
        return (getPointer(object)->*func)(std::forward<Param>(param)...);
    };
}

/**
 * @brief Bind an object to a std::function
 * @tparam O Object type
 * @tparam Ret Return type
 * @tparam P1 First parameter type
 * @tparam Param Remaining parameter types
 * @param func Function to bind
 * @param object Object to bind as first argument
 * @return Bound function
 */
template <typename O, typename Ret, typename P1, typename... Param>
    requires Invocable<std::function<Ret(P1, Param...)>, O, Param...>
[[nodiscard]] auto bindFirst(const std::function<Ret(P1, Param...)>& func,
                             O&& object) {
    return [func, object = std::forward<O>(object)](Param... param) -> Ret {
        return func(object, std::forward<Param>(param)...);
    };
}

/**
 * @brief Bind a function object and an object to a member operator()
 */
template <typename F, typename O, typename Ret, typename Class, typename P1,
          typename... Param>
    requires Invocable<F, O, P1, Param...>
[[nodiscard]] constexpr auto bindFirst(const F& funcObj, O&& object,
                                       Ret (Class::*func)(P1, Param...) const) {
    return [funcObj, object = std::forward<O>(object),
            func](Param... param) -> Ret {
        return (funcObj.*func)(object, std::forward<Param>(param)...);
    };
}

/**
 * @brief Generic bind for function objects
 */
template <typename F, typename O>
    requires Invocable<F, O>
[[nodiscard]] constexpr auto bindFirst(const F& func, O&& object) {
    return bindFirst(func, std::forward<O>(object), &F::operator());
}

template <typename F, typename O>
[[nodiscard]] constexpr auto bindFirst(F&& func, O&& object) {
    return [func = std::forward<F>(func),
            object = std::forward<O>(object)](auto&&... params) mutable {
        return func(object, std::forward<decltype(params)>(params)...);
    };
}

/**
 * @brief Universal reference version of bindFirst
 */
template <typename F, typename O>
    requires Invocable<F, O>
[[nodiscard]] constexpr auto bindFirst(F&& func, O&& object) {
    return [func = std::forward<F>(func), object = std::forward<O>(object)](
               auto&&... param) -> decltype(auto) {
        return std::invoke(func, object,
                           std::forward<decltype(param)>(param)...);
    };
}

/**
 * @brief Bind a class member variable
 */
template <typename O, typename T, typename Class>
[[nodiscard]] constexpr auto bindMember(T Class::*member, O&& object) noexcept {
    return [member, object = std::forward<O>(object)]() -> T& {
        return removeConstPointer(getPointer(object))->*member;
    };
}

/**
 * @brief Bind a static function
 */
template <typename Ret, typename... Param>
[[nodiscard]] constexpr auto bindStatic(Ret (*func)(Param...)) noexcept {
    return [func](Param... param) -> Ret {
        return func(std::forward<Param>(param)...);
    };
}

// Advanced binding features
//-------------------------

/**
 * @brief Asynchronously call a bound function
 */
template <typename F, typename... Args>
[[nodiscard]] auto asyncBindFirst(F&& func, Args&&... args) {
    return std::async(std::launch::async, std::forward<F>(func),
                      std::forward<Args>(args)...);
}

// Exception handling utilities
//----------------------------

// Primary template declaration
template <typename F>
struct BindingFunctor;

// Specialization for function pointers
template <typename ReturnType, typename... Args>
struct BindingFunctor<ReturnType (*)(Args...)> {
    using FunctionType = ReturnType (*)(Args...);
    using ResultType = ReturnType;

    FunctionType func;

    template <typename... RemainingArgs>
    ReturnType operator()(RemainingArgs&&... args) const {
        return func(std::forward<RemainingArgs>(args)...);
    }
};

// Exception class for binding errors
class BindingException : public std::exception {
private:
    std::string message;

public:
    BindingException(const std::string& context, const std::exception& e,
                     const std::string& location = "")
        : message(context + ": " + e.what() +
                  (location.empty() ? "" : " at " + location)) {}

    const char* what() const noexcept override { return message.c_str(); }
};

// Exception handling wrapper for bind_first
template <typename Callable, typename FirstArg, typename... Args>
auto bindFirstWithExceptionHandling(Callable&& callable, FirstArg&& first_arg,
                                    const std::string& context,
                                    Args&&... args) {
    auto bound = bindFirst(std::forward<Callable>(callable),
                           std::forward<FirstArg>(first_arg),
                           std::forward<Args>(args)...);

    return [bound, context]<typename... CallArgs>(CallArgs&&... call_args) {
        try {
            return bound(std::forward<CallArgs>(call_args)...);
        } catch (const std::exception& e) {
            throw BindingException(context, e, "function call");
        }
    };
}

// Thread-safe binding
//-------------------

/**
 * @brief Thread-safe bindFirst using shared_ptr and mutex
 */
template <typename O, typename Ret, typename... Param>
[[nodiscard]] auto bindFirstThreadSafe(Ret (O::*func)(Param...),
                                       std::shared_ptr<O> object) {
    return [func, object](Param... param) -> Ret {
        // Object lifetime is managed by shared_ptr
        return (object.get()->*func)(std::forward<Param>(param)...);
    };
}

// C++20 coroutine support
//-----------------------

#if __cpp_lib_coroutine
#include <coroutine>

/**
 * @brief Simple awaitable wrapper for bound functions
 */
template <typename Ret>
struct BoundAwaitable {
    std::function<Ret()> func;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) const {}
    Ret await_resume() { return func(); }
};

/**
 * @brief Create awaitable from bound function
 */
template <typename F, typename O>
    requires Invocable<F, O>
[[nodiscard]] auto makeAwaitable(F&& func, O&& obj) {
    auto bound = bindFirst(std::forward<F>(func), std::forward<O>(obj));
    return BoundAwaitable<std::invoke_result_t<decltype(bound)>>{
        std::move(bound)};
}
#endif

}  // namespace atom::meta

#endif  // ATOM_META_BIND_FIRST_HPP
