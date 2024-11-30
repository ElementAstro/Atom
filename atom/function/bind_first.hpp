/*!
 * \file bind_first.hpp
 * \brief An easy way to bind a function to an object
 * \author Max Qian <lightapt.com>
 * \date 2024-03-01
 * \copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef ATOM_META_BIND_FIRST_HPP
#define ATOM_META_BIND_FIRST_HPP

#include <functional>
#include <future>

#include "atom/function/concept.hpp"

namespace atom::meta {
template <typename T>
constexpr auto getPointer(T *ptr) noexcept -> T * {
    return ptr;
}

template <typename T>
auto getPointer(const std::reference_wrapper<T> &ref) noexcept -> T * {
    return &ref.get();
}

template <typename T>
constexpr auto getPointer(const T &ref) noexcept -> const T * {
    return &ref;
}

template <typename T>
constexpr auto removeConstPointer(const T *ptr) noexcept -> T * {
    return const_cast<T *>(ptr);
}

template <typename O, typename Ret, typename P1, typename... Param>
constexpr auto bindFirst(Ret (*func)(P1, Param...), O &&object)
    requires Invocable<Ret (*)(P1, Param...), O, Param...>
{
    return [func, object = std::forward<O>(object)](Param... param) -> Ret {
        return func(object, std::forward<Param>(param)...);
    };
}

template <typename O, typename Ret, typename Class, typename... Param>
constexpr auto bindFirst(Ret (Class::*func)(Param...), O &&object)
    requires Invocable<Ret (Class::*)(Param...), O, Param...>
{
    return [func, object = std::forward<O>(object)](Param... param) -> Ret {
        return (removeConstPointer(getPointer(object))->*func)(
            std::forward<Param>(param)...);
    };
}

template <typename O, typename Ret, typename Class, typename... Param>
constexpr auto bindFirst(Ret (Class::*func)(Param...) const, O &&object)
    requires Invocable<Ret (Class::*)(Param...) const, O, Param...>
{
    return [func, object = std::forward<O>(object)](Param... param) -> Ret {
        return (getPointer(object)->*func)(std::forward<Param>(param)...);
    };
}

template <typename O, typename Ret, typename P1, typename... Param>
auto bindFirst(const std::function<Ret(P1, Param...)> &func, O &&object)
    requires Invocable<std::function<Ret(P1, Param...)>, O, Param...>
{
    return [func, object = std::forward<O>(object)](Param... param) -> Ret {
        return func(object, std::forward<Param>(param)...);
    };
}

template <typename F, typename O, typename Ret, typename Class, typename P1,
          typename... Param>
constexpr auto bindFirst(const F &funcObj, O &&object,
                         Ret (Class::*func)(P1, Param...) const)
    requires Invocable<F, O, P1, Param...>
{
    return [funcObj, object = std::forward<O>(object),
            func](Param... param) -> Ret {
        return (funcObj.*func)(object, std::forward<Param>(param)...);
    };
}

template <typename F, typename O>
constexpr auto bindFirst(const F &func, O &&object)
    requires Invocable<F, O>
{
    return bindFirst(func, std::forward<O>(object), &F::operator());
}

template <typename F, typename O>
constexpr auto bindFirst(F &&func, O &&object)
    requires Invocable<F, O>
{
    return [func = std::forward<F>(func), object = std::forward<O>(object)](
               auto &&...param) -> decltype(auto) {
        return std::invoke(func, object,
                           std::forward<decltype(param)>(param)...);
    };
}

template <typename O, typename T, typename Class>
constexpr auto bindMember(T Class::*member, O &&object) noexcept {
    return [member, object = std::forward<O>(object)]() -> T & {
        return removeConstPointer(getPointer(object))->*member;
    };
}

template <typename Ret, typename Class, typename... Param>
constexpr auto bindStatic(Ret (*func)(Param...)) noexcept {
    return [func](Param... param) -> Ret {
        return func(std::forward<Param>(param)...);
    };
}

template <typename F, typename... Args>
auto asyncBindFirst(F &&func, Args &&...args) {
    return std::async(std::launch::async, std::forward<F>(func),
                      std::forward<Args>(args)...);
}

template <typename O, typename Ret, typename P1, typename... Param>
constexpr auto bindFirstWithExceptionHandling(Ret (*func)(P1, Param...),
                                              O &&object)
    requires Invocable<Ret (*)(P1, Param...), O, Param...>
{
    return [func, object = std::forward<O>(object)](Param... param) -> Ret {
        try {
            return func(object, std::forward<Param>(param)...);
        } catch (const std::exception &e) {
            throw;
        }
    };
}

}  // namespace atom::meta

#endif  // ATOM_META_BIND_FIRST_HPP
