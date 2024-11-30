/*!
 * \file proxy.hpp
 * \brief Proxy Function Implementation
 * \author Max Qian <lightapt.com>
 * \date 2024-03-01
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_PROXY_HPP
#define ATOM_META_PROXY_HPP

#include <any>
#include <functional>
#include <typeinfo>
#include <utility>
#include <vector>

#if ATOM_ENABLE_DEBUG
#include <iostream>
#endif

#include "atom/algorithm/hash.hpp"
#include "atom/error/exception.hpp"
#include "atom/function/abi.hpp"
#include "atom/function/func_traits.hpp"
#include "atom/function/proxy_params.hpp"
#include "atom/macro.hpp"

namespace atom::meta {

struct ATOM_ALIGNAS(128) FunctionInfo {
private:
    std::string returnType_;
    std::vector<std::string> argumentTypes_;
    std::string hash_;

public:
    FunctionInfo() = default;

    void logFunctionInfo() const {
#if ATOM_ENABLE_DEBUG
        std::cout << "Function return type: " << returnType_ << "\n";
        for (size_t i = 0; i < argumentTypes_.size(); ++i) {
            std::cout << "Argument " << i + 1
                      << ": Type = " << argumentTypes_[i] << std::endl;
        }
#endif
    }

    [[nodiscard]] auto getReturnType() const -> const std::string & {
        return returnType_;
    }
    [[nodiscard]] auto getArgumentTypes() const
        -> const std::vector<std::string> & {
        return argumentTypes_;
    }
    [[nodiscard]] auto getHash() const -> const std::string & { return hash_; }

    void setReturnType(const std::string &returnType) {
        returnType_ = returnType;
    }
    void addArgumentType(const std::string &argumentType) {
        argumentTypes_.push_back(argumentType);
    }
    void setHash(const std::string &hash) { hash_ = hash; }
};

template <typename T>
auto anyCastRef(std::any &operand) -> T && {
    return *std::any_cast<std::decay_t<T> *>(operand);
}

template <typename T>
auto anyCastRef(const std::any &operand) -> T & {
#if ATOM_ENABLE_DEBUG
    std::cout << "type: " << TypeInfo::fromType<T>().name() << "\n";
#endif
    return *std::any_cast<std::decay_t<T> *>(operand);
}

template <typename T>
auto anyCastVal(std::any &operand) -> T {
    return std::any_cast<T>(operand);
}

template <typename T>
auto anyCastVal(const std::any &operand) -> T {
    return std::any_cast<T>(operand);
}

template <typename T>
auto anyCastConstRef(const std::any &operand) -> const T & {
    return std::any_cast<T>(operand);
}

template <typename T>
auto anyCastHelper(std::any &operand) -> decltype(auto) {
    if constexpr (std::is_reference_v<T> &&
                  std::is_const_v<std::remove_reference_t<T>>) {
        return anyCastConstRef<T>(operand);
    } else if constexpr (std::is_reference_v<T>) {
        return anyCastRef<T>(operand);
    } else {
        return anyCastVal<T>(operand);
    }
}

template <typename T>
auto anyCastHelper(const std::any &operand) -> decltype(auto) {
    if constexpr (std::is_reference_v<T> &&
                  std::is_const_v<std::remove_reference_t<T>>) {
        return anyCastConstRef<T>(operand);
    } else if constexpr (std::is_reference_v<T>) {
        return anyCastRef<T>(operand);
    } else {
        return anyCastVal<T>(operand);
    }
}

template <typename Func>
class BaseProxyFunction {
protected:
    std::decay_t<Func> func_;
    using Traits = FunctionTraits<Func>;
    static constexpr std::size_t ARITY = Traits::arity;
    FunctionInfo info_;

public:
    explicit BaseProxyFunction(Func &&func, FunctionInfo &info)
        : func_(std::move(func)) {
        collectFunctionInfo();
        calcFuncInfoHash();
        info = info_;
    }

    [[nodiscard]] auto getFunctionInfo() const -> FunctionInfo { return info_; }

protected:
    void collectFunctionInfo() {
        info_.setReturnType(
            DemangleHelper::demangleType<typename Traits::return_type>());
        collectArgumentTypes(std::make_index_sequence<ARITY>{});
    }

    template <std::size_t... Is>
    void collectArgumentTypes(std::index_sequence<Is...> /*unused*/) {
        (info_.addArgumentType(DemangleHelper::demangleType<
                               typename Traits::template argument_t<Is>>()),
         ...);
    }

    void calcFuncInfoHash() {
        // Max: Here we need to check the difference between different compilers
        if (!info_.getArgumentTypes().empty()) {
            std::string combinedTypes = info_.getReturnType();
            for (const auto &argType : info_.getArgumentTypes()) {
                combinedTypes += argType;
            }
            info_.setHash(
                std::to_string(atom::algorithm::computeHash(combinedTypes)));
        }
    }

    void logArgumentTypes() const {
#if ATOM_ENABLE_DEBUG
        std::cout << "Function Arity: " << ARITY << "\n";
        info_.logFunctionInfo();
#endif
    }

    template <std::size_t... Is>
    auto callFunction(const std::vector<std::any> &args,
                      std::index_sequence<Is...> /*unused*/) -> std::any {
        if constexpr (std::is_void_v<typename Traits::return_type>) {
            std::invoke(func_,
                        anyCastHelper<typename Traits::template argument_t<Is>>(
                            args[Is])...);
            return {};
        } else {
            return std::make_any<typename Traits::return_type>(std::invoke(
                func_, anyCastHelper<typename Traits::template argument_t<Is>>(
                           args[Is])...));
        }
    }

    auto callFunction(const FunctionParams &params) -> std::any {
        if constexpr (std::is_void_v<typename Traits::return_type>) {
            std::invoke(func_, params.toVector());
            return {};
        } else {
            return std::make_any<typename Traits::return_type>(
                std::invoke(func_, params.toVector()));
        }
    }

    template <std::size_t... Is>
    auto callMemberFunction(const std::vector<std::any> &args,
                            std::index_sequence<Is...> /*unused*/) -> std::any {
        auto invokeFunc = [this](auto &obj, auto &&...args) {
            if constexpr (std::is_void_v<typename Traits::return_type>) {
                (obj.*func_)(std::forward<decltype(args)>(args)...);
                return std::any{};
            } else {
                return std::make_any<typename Traits::return_type>(
                    (obj.*func_)(std::forward<decltype(args)>(args)...));
            }
        };

        if (args[0].type() ==
            typeid(std::reference_wrapper<typename Traits::class_type>)) {
            auto &obj =
                std::any_cast<
                    std::reference_wrapper<typename Traits::class_type>>(
                    args[0])
                    .get();
            return invokeFunc(
                obj, anyCastHelper<typename Traits::template argument_t<Is>>(
                         args[Is + 1])...);
        }
        auto &obj = const_cast<typename Traits::class_type &>(
            std::any_cast<const typename Traits::class_type &>(args[0]));
        return invokeFunc(
            obj, anyCastHelper<typename Traits::template argument_t<Is>>(
                     args[Is + 1])...);
    }
};

template <typename Func>
class ProxyFunction : protected BaseProxyFunction<Func> {
    using Base = BaseProxyFunction<Func>;
    using Traits = typename Base::Traits;
    static constexpr std::size_t ARITY = Base::ARITY;

public:
    explicit ProxyFunction(Func &&func, FunctionInfo &info)
        : Base(std::move(func), info) {}

    auto operator()(const std::vector<std::any> &args) -> std::any {
        this->logArgumentTypes();
        if constexpr (Traits::is_member_function) {
            if (args.size() != ARITY + 1) {
                THROW_EXCEPTION("Incorrect number of arguments");
            }
            return this->callMemberFunction(args,
                                            std::make_index_sequence<ARITY>());
        } else {
#if ATOM_ENABLE_DEBUG
            std::cout << "Function Arity: " << arity << "\n";
            std::cout << "Arguments size: " << args.size() << "\n";
#endif
            if (args.size() != ARITY) {
                THROW_EXCEPTION("Incorrect number of arguments");
            }
            return this->callFunction(args, std::make_index_sequence<ARITY>());
        }
    }

    auto operator()(const FunctionParams &params) -> std::any {
        this->logArgumentTypes();
        if constexpr (Traits::is_member_function) {
            if (params.size() != ARITY + 1) {
                THROW_EXCEPTION("Incorrect number of arguments");
            }
            return this->callMemberFunction(params.toVector());
        } else {
            if (params.size() != ARITY) {
                THROW_EXCEPTION("Incorrect number of arguments");
            }
            return this->callFunction(params.toVector());
        }
    }
};

}  // namespace atom::meta

#endif