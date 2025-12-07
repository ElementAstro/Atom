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
#include <string>
#include <utility>

#include "atom/meta/concept.hpp"

namespace atom::meta {

//==============================================================================
// Core pointer and reference manipulation utilities
//==============================================================================

/*!
 * \brief Get a pointer from a raw pointer
 * \tparam T The pointee type
 * \param ptr The input pointer
 * \return The same pointer
 */
template <typename T>
[[nodiscard]] constexpr auto getPointer(T* ptr) noexcept -> T* {
    return ptr;
}

/*!
 * \brief Get a pointer from a reference_wrapper
 * \tparam T The reference type
 * \param ref The reference wrapper
 * \return Pointer to the referenced object
 */
template <typename T>
[[nodiscard]] constexpr auto getPointer(
    const std::reference_wrapper<T>& ref) noexcept -> T* {
    return &ref.get();
}

/*!
 * \brief Get a pointer from an object
 * \tparam T The object type
 * \param ref The object
 * \return Pointer to the object
 */
template <typename T>
[[nodiscard]] constexpr auto getPointer(const T& ref) noexcept -> const T* {
    return &ref;
}

/*!
 * \brief Remove const from a pointer
 * \tparam T The pointee type
 * \param ptr Const pointer
 * \return Non-const pointer
 */
template <typename T>
[[nodiscard]] constexpr auto removeConstPointer(const T* ptr) noexcept -> T* {
    return const_cast<T*>(ptr);
}

//==============================================================================
// Primary bind_first implementation
//==============================================================================

/*!
 * \brief Bind an object to a function pointer as first argument
 * \tparam O Object type
 * \tparam Ret Return type
 * \tparam P1 First parameter type
 * \tparam Param Remaining parameter types
 * \param func Function to bind
 * \param object Object to bind as first argument
 * \return Bound function
 */
template <typename O, typename Ret, typename P1, typename... Param>
    requires Invocable<Ret (*)(P1, Param...), O, Param...>
[[nodiscard]] constexpr auto bindFirst(Ret (*func)(P1, Param...), O&& object) {
    return [func, object = std::forward<O>(object)](Param... param) -> Ret {
        return func(object, std::forward<Param>(param)...);
    };
}

/*!
 * \brief Bind an object to a member function
 * \tparam O Object type
 * \tparam Ret Return type
 * \tparam Class Class type
 * \tparam Param Parameter types
 * \param func Member function to bind
 * \param object Object to bind the function to
 * \return Bound function
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

/*!
 * \brief Bind an object to a const member function
 * \tparam O Object type
 * \tparam Ret Return type
 * \tparam Class Class type
 * \tparam Param Parameter types
 * \param func Const member function to bind
 * \param object Object to bind the function to
 * \return Bound function
 */
template <typename O, typename Ret, typename Class, typename... Param>
    requires Invocable<Ret (Class::*)(Param...) const, O, Param...>
[[nodiscard]] constexpr auto bindFirst(Ret (Class::*func)(Param...) const,
                                       O&& object) {
    return [func, object = std::forward<O>(object)](Param... param) -> Ret {
        return (getPointer(object)->*func)(std::forward<Param>(param)...);
    };
}

/*!
 * \brief Bind an object to a std::function
 * \tparam O Object type
 * \tparam Ret Return type
 * \tparam P1 First parameter type
 * \tparam Param Remaining parameter types
 * \param func Function to bind
 * \param object Object to bind as first argument
 * \return Bound function
 */
template <typename O, typename Ret, typename P1, typename... Param>
    requires Invocable<std::function<Ret(P1, Param...)>, O, Param...>
[[nodiscard]] auto bindFirst(const std::function<Ret(P1, Param...)>& func,
                             O&& object) {
    return [func, object = std::forward<O>(object)](Param... param) -> Ret {
        return func(object, std::forward<Param>(param)...);
    };
}

/*!
 * \brief Universal reference version of bindFirst for function objects
 * \tparam F Function object type
 * \tparam O Object type
 * \param func Function object to bind
 * \param object Object to bind as first argument
 * \return Bound function
 */
template <typename F, typename O>
[[nodiscard]] constexpr auto bindFirst(F&& func, O&& object) {
    return [func = std::forward<F>(func), object = std::forward<O>(object)](
               auto&&... param) -> decltype(auto) {
        return std::invoke(func, object,
                           std::forward<decltype(param)>(param)...);
    };
}

/*!
 * \brief Bind a class member variable
 * \tparam O Object type
 * \tparam T Member variable type
 * \tparam Class Class type
 * \param member Member variable pointer
 * \param object Object to bind
 * \return Function that returns reference to the member variable
 */
template <typename O, typename T, typename Class>
[[nodiscard]] constexpr auto bindMember(T Class::*member, O&& object) noexcept {
    return [member, object = std::forward<O>(object)]() -> T& {
        return removeConstPointer(getPointer(object))->*member;
    };
}

/*!
 * \brief Bind a static function
 * \tparam Ret Return type
 * \tparam Param Parameter types
 * \param func Static function to bind
 * \return Bound function
 */
template <typename Ret, typename... Param>
[[nodiscard]] constexpr auto bindStatic(Ret (*func)(Param...)) noexcept {
    return [func](Param... param) -> Ret {
        return func(std::forward<Param>(param)...);
    };
}

//==============================================================================
// Advanced binding features
//==============================================================================

/*!
 * \brief Asynchronously call a bound function
 * \tparam F Function type
 * \tparam Args Argument types
 * \param func Function to call asynchronously
 * \param args Arguments to pass to the function
 * \return Future object containing the result
 */
template <typename F, typename... Args>
[[nodiscard]] auto asyncBindFirst(F&& func, Args&&... args) {
    return std::async(std::launch::async, std::forward<F>(func),
                      std::forward<Args>(args)...);
}

//==============================================================================
// Exception handling utilities
//==============================================================================

/*!
 * \brief Primary template declaration for binding functor
 * \tparam F Function type
 */
template <typename F>
struct BindingFunctor;

/*!
 * \brief Specialization for function pointers
 * \tparam ReturnType Return type of the function
 * \tparam Args Argument types of the function
 */
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

/*!
 * \brief Exception class for binding errors
 */
class BindingException : public std::exception {
private:
    std::string message;

public:
    /*!
     * \brief Construct binding exception with context and location
     * \param context Context where the error occurred
     * \param e Original exception
     * \param location Location where the error occurred
     */
    BindingException(const std::string& context, const std::exception& e,
                     const std::string& location = "")
        : message(context + ": " + e.what() +
                  (location.empty() ? "" : " at " + location)) {}

    const char* what() const noexcept override { return message.c_str(); }
};

/*!
 * \brief Exception handling wrapper for bind_first
 * \tparam Callable Callable type
 * \tparam FirstArg First argument type
 * \tparam Args Additional argument types
 * \param callable Function to bind
 * \param first_arg First argument to bind
 * \param context Context for error reporting
 * \param args Additional arguments
 * \return Exception-safe bound function
 */
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

//==============================================================================
// Thread-safe binding
//==============================================================================

/*!
 * \brief Thread-safe bindFirst using shared_ptr
 * \tparam O Object type
 * \tparam Ret Return type
 * \tparam Param Parameter types
 * \param func Member function to bind
 * \param object Shared pointer to object
 * \return Thread-safe bound function
 */
template <typename O, typename Ret, typename... Param>
[[nodiscard]] auto bindFirstThreadSafe(Ret (O::*func)(Param...),
                                       std::shared_ptr<O> object) {
    return [func, object](Param... param) -> Ret {
        return (object.get()->*func)(std::forward<Param>(param)...);
    };
}

//==============================================================================
// C++23 Enhanced Binding Utilities
//==============================================================================

/**
 * @brief Concept for bindable callables
 */
template <typename F>
concept Bindable =
    std::is_invocable_v<F> || std::is_member_function_pointer_v<F>;

/**
 * @brief Concept for member functions
 */
template <typename F, typename Class>
concept MemberFunctionOf = std::is_member_function_pointer_v<F> &&
                           requires { typename std::invoke_result<F, Class>; };

/**
 * @brief Bind with automatic type deduction
 */
template <typename Callable, typename FirstArg>
    requires Bindable<Callable>
[[nodiscard]] auto autoBind(Callable&& callable, FirstArg&& first_arg) {
    return bindFirst(std::forward<Callable>(callable),
                     std::forward<FirstArg>(first_arg));
}

/**
 * @brief Bind with weak_ptr for safe lifetime management
 */
template <typename O, typename Ret, typename... Param>
[[nodiscard]] auto bindFirstWeak(Ret (O::*func)(Param...),
                                 std::weak_ptr<O> object) {
    return [func, object](Param... param) -> std::optional<Ret> {
        if (auto shared = object.lock()) {
            return (shared.get()->*func)(std::forward<Param>(param)...);
        }
        return std::nullopt;
    };
}

/**
 * @brief Bind returning optional for nullable first arg
 */
template <typename Callable, typename FirstArg>
[[nodiscard]] auto bindFirstOptional(Callable&& callable,
                                     std::optional<FirstArg> first_arg) {
    return
        [callable = std::forward<Callable>(callable),
         first_arg = std::move(first_arg)](auto&&... args)
            -> std::optional<
                std::invoke_result_t<Callable, FirstArg, decltype(args)...>> {
            if (first_arg) {
                return std::invoke(callable, *first_arg,
                                   std::forward<decltype(args)>(args)...);
            }
            return std::nullopt;
        };
}

/**
 * @brief Bind with lazy evaluation of first argument
 */
template <typename Callable, typename FirstArgFactory>
    requires std::invocable<FirstArgFactory>
[[nodiscard]] auto bindFirstLazy(Callable&& callable,
                                 FirstArgFactory&& factory) {
    return [callable = std::forward<Callable>(callable),
            factory = std::forward<FirstArgFactory>(factory)](auto&&... args) {
        return std::invoke(callable, factory(),
                           std::forward<decltype(args)>(args)...);
    };
}

/**
 * @brief Bind multiple arguments at once
 */
template <typename Callable, typename... BoundArgs>
[[nodiscard]] auto bindMultiple(Callable&& callable,
                                BoundArgs&&... bound_args) {
    return [callable = std::forward<Callable>(callable),
            ... args = std::forward<BoundArgs>(bound_args)](auto&&... rest) {
        return std::invoke(callable, args...,
                           std::forward<decltype(rest)>(rest)...);
    };
}

/**
 * @brief Bind registry for named bindings
 */
class BindingRegistry {
    std::unordered_map<std::string, std::function<void()>> bindings_;
    mutable std::shared_mutex mutex_;

public:
    template <typename Callable, typename... Args>
    void registerBinding(std::string_view name, Callable&& callable,
                         Args&&... args) {
        auto bound = bindFirst(std::forward<Callable>(callable),
                               std::forward<Args>(args)...);
        std::unique_lock lock(mutex_);
        bindings_[std::string(name)] = std::move(bound);
    }

    bool invoke(std::string_view name) const {
        std::shared_lock lock(mutex_);
        auto it = bindings_.find(std::string(name));
        if (it != bindings_.end()) {
            it->second();
            return true;
        }
        return false;
    }

    [[nodiscard]] std::vector<std::string> getBindingNames() const {
        std::shared_lock lock(mutex_);
        std::vector<std::string> result;
        result.reserve(bindings_.size());
        for (const auto& [name, _] : bindings_) {
            result.push_back(name);
        }
        return result;
    }

    static BindingRegistry& getInstance() {
        static BindingRegistry instance;
        return instance;
    }
};

}  // namespace atom::meta

#endif  // ATOM_META_BIND_FIRST_HPP
