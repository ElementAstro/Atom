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
#include <future>
#include <shared_mutex>
#include <source_location>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

#if ATOM_ENABLE_DEBUG
#include <iostream>
#endif

#include "atom/algorithm/hash.hpp"
#include "atom/macro.hpp"
#include "atom/meta/abi.hpp"
#include "atom/meta/func_traits.hpp"
#include "atom/meta/proxy_params.hpp"

namespace atom::meta {

/**
 * @brief Function information structure containing function signature metadata
 */
struct ATOM_ALIGNAS(128) FunctionInfo {
private:
    std::string name_;
    std::string returnType_;
    std::vector<std::string> argumentTypes_;
    std::vector<std::string> parameterNames_;
    std::string hash_;
    bool isNoexcept_{false};
    std::source_location location_;

public:
    FunctionInfo() = default;

    FunctionInfo(std::string_view name, std::string_view returnType)
        : name_(name), returnType_(returnType) {}

    /**
     * @brief Log function information for debugging purposes
     */
    void logFunctionInfo() const {
#if ATOM_ENABLE_DEBUG
        std::cout << "Function name: " << name_ << "\n";
        std::cout << "Function return type: " << returnType_ << "\n";
        std::cout << "Function location: " << location_.file_name() << ":"
                  << location_.line() << "\n";

        for (size_t i = 0; i < argumentTypes_.size(); ++i) {
            std::cout << "Argument " << i + 1
                      << ": Type = " << argumentTypes_[i];
            if (i < parameterNames_.size() && !parameterNames_[i].empty()) {
                std::cout << ", Name = " << parameterNames_[i];
            }
            std::cout << std::endl;
        }

        std::cout << "Function hash: " << hash_ << "\n";
        std::cout << "Is noexcept: " << (isNoexcept_ ? "true" : "false")
                  << "\n";
#endif
    }

    [[nodiscard]] auto getReturnType() const -> const std::string& {
        return returnType_;
    }
    [[nodiscard]] auto getArgumentTypes() const
        -> const std::vector<std::string>& {
        return argumentTypes_;
    }
    [[nodiscard]] auto getHash() const -> const std::string& { return hash_; }
    [[nodiscard]] auto getName() const -> const std::string& { return name_; }
    [[nodiscard]] auto getParameterNames() const
        -> const std::vector<std::string>& {
        return parameterNames_;
    }
    [[nodiscard]] auto getLocation() const -> const std::source_location& {
        return location_;
    }
    [[nodiscard]] bool isNoexcept() const { return isNoexcept_; }

    void setName(std::string_view name) { name_ = name; }
    void setReturnType(const std::string& returnType) {
        returnType_ = returnType;
    }
    void addArgumentType(const std::string& argumentType) {
        argumentTypes_.push_back(argumentType);
    }
    void setHash(const std::string& hash) { hash_ = hash; }

    void setParameterName(size_t index, std::string_view name) {
        if (index >= parameterNames_.size()) {
            parameterNames_.resize(index + 1);
        }
        parameterNames_[index] = name;
    }

    void setNoexcept(bool isNoexcept) { isNoexcept_ = isNoexcept; }
    void setLocation(const std::source_location& location) {
        location_ = location;
    }

    /**
     * @brief Serialize function information to JSON
     * @return JSON representation of function information
     */
    [[nodiscard]] nlohmann::json toJson() const {
        nlohmann::json result;
        result["name"] = name_;
        result["return_type"] = returnType_;
        result["argument_types"] = argumentTypes_;
        result["parameter_names"] = parameterNames_;
        result["hash"] = hash_;
        result["noexcept"] = isNoexcept_;
        result["file"] = location_.file_name();
        result["line"] = location_.line();
        result["column"] = location_.column();
        return result;
    }

    /**
     * @brief Deserialize function information from JSON
     * @param j JSON object to deserialize from
     * @return FunctionInfo instance
     */
    static FunctionInfo fromJson(const nlohmann::json& j) {
        FunctionInfo info;
        info.name_ = j.value("name", "");
        info.returnType_ = j.at("return_type").get<std::string>();
        info.argumentTypes_ =
            j.at("argument_types").get<std::vector<std::string>>();
        info.parameterNames_ =
            j.value("parameter_names", std::vector<std::string>{});
        info.hash_ = j.at("hash").get<std::string>();
        info.isNoexcept_ = j.value("noexcept", false);
        return info;
    }
};

template <typename T>
auto anyCastRef(std::any& operand) -> T&& {
    using DecayedT = std::decay_t<T>;
    try {
        return *std::any_cast<DecayedT*>(operand);
    } catch (const std::bad_any_cast& e) {
        throw ProxyTypeError(std::string("Failed to cast to reference type ") +
                             typeid(T).name() + ": " + e.what());
    }
}

template <typename T>
auto anyCastRef(const std::any& operand) -> T& {
#if ATOM_ENABLE_DEBUG
    std::cout << "type: " << DemangleHelper::demangleType<T>() << "\n";
#endif
    using DecayedT = std::decay_t<T>;
    try {
        return *std::any_cast<DecayedT*>(operand);
    } catch (const std::bad_any_cast& e) {
        throw ProxyTypeError(std::string("Failed to cast to reference type ") +
                             typeid(T).name() + ": " + e.what());
    }
}

template <typename T>
auto anyCastVal(std::any& operand) -> T {
    try {
        return std::any_cast<T>(operand);
    } catch (const std::bad_any_cast& e) {
        throw ProxyTypeError(std::string("Failed to cast to value type ") +
                             typeid(T).name() + ": " + e.what());
    }
}

template <typename T>
auto anyCastVal(const std::any& operand) -> T {
    try {
        return std::any_cast<T>(operand);
    } catch (const std::bad_any_cast& e) {
        throw ProxyTypeError(std::string("Failed to cast to value type ") +
                             typeid(T).name() + ": " + e.what());
    }
}

template <typename T>
auto anyCastConstRef(const std::any& operand) -> const T& {
    try {
        return std::any_cast<T>(operand);
    } catch (const std::bad_any_cast& e) {
        throw ProxyTypeError(
            std::string("Failed to cast to const reference type ") +
            typeid(T).name() + ": " + e.what());
    }
}

template <typename T>
bool tryConvertType(std::any& src);

template <typename T>
auto anyCastHelper(std::any& operand) -> decltype(auto) {
    try {
        if constexpr (std::is_reference_v<T> &&
                      std::is_const_v<std::remove_reference_t<T>>) {
            return anyCastConstRef<
                std::remove_const_t<std::remove_reference_t<T>>>(operand);
        } else if constexpr (std::is_reference_v<T>) {
            return anyCastRef<T>(operand);
        } else {
            return anyCastVal<T>(operand);
        }
    } catch (const ProxyTypeError& e) {
        if (tryConvertType<T>(operand)) {
            return anyCastVal<T>(operand);
        }
        throw;
    }
}

template <typename T>
auto anyCastHelper(const std::any& operand) -> decltype(auto) {
    try {
        if constexpr (std::is_reference_v<T> &&
                      std::is_const_v<std::remove_reference_t<T>>) {
            return anyCastConstRef<
                std::remove_const_t<std::remove_reference_t<T>>>(operand);
        } else if constexpr (std::is_reference_v<T>) {
            return anyCastRef<T>(operand);
        } else {
            return anyCastVal<T>(operand);
        }
    } catch (const ProxyTypeError& e) {
        throw;
    }
}

template <typename T, typename = void>
struct CanConvert : std::false_type {};

template <typename T>
struct CanConvert<
    T, std::void_t<decltype(std::declval<T&>() = std::declval<int>())>>
    : std::true_type {};

/**
 * @brief Attempt to convert between compatible types
 * @tparam T Target type for conversion
 * @param src Source any object to convert
 * @return true if conversion succeeded, false otherwise
 */
template <typename T>
bool tryConvertType(std::any& src) {
    const auto& typeInfo = src.type();

    if constexpr (std::is_integral_v<std::decay_t<T>>) {
        if (typeInfo == typeid(int)) {
            src = static_cast<T>(std::any_cast<int>(src));
            return true;
        }
        if (typeInfo == typeid(long)) {
            src = static_cast<T>(std::any_cast<long>(src));
            return true;
        }
        if (typeInfo == typeid(short)) {
            src = static_cast<T>(std::any_cast<short>(src));
            return true;
        }
        if (typeInfo == typeid(double)) {
            src = static_cast<T>(std::any_cast<double>(src));
            return true;
        }
        if (typeInfo == typeid(float)) {
            src = static_cast<T>(std::any_cast<float>(src));
            return true;
        }
    } else if constexpr (std::is_floating_point_v<std::decay_t<T>>) {
        if (typeInfo == typeid(float)) {
            src = static_cast<T>(std::any_cast<float>(src));
            return true;
        }
        if (typeInfo == typeid(double)) {
            src = static_cast<T>(std::any_cast<double>(src));
            return true;
        }
        if (typeInfo == typeid(int)) {
            src = static_cast<T>(std::any_cast<int>(src));
            return true;
        }
        if (typeInfo == typeid(long)) {
            src = static_cast<T>(std::any_cast<long>(src));
            return true;
        }
    } else if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
        if (typeInfo == typeid(const char*)) {
            src = std::string(std::any_cast<const char*>(src));
            return true;
        }
        if (typeInfo == typeid(std::string_view)) {
            src = std::string(std::any_cast<std::string_view>(src));
            return true;
        }
    }

    return false;
}

/**
 * @brief Base proxy function class handling function calls and information
 * collection
 * @tparam Func Function type to wrap
 */
template <typename Func>
class BaseProxyFunction {
protected:
    std::decay_t<Func> func_;
    using Traits = FunctionTraits<Func>;
    static constexpr std::size_t ARITY = Traits::arity;
    FunctionInfo info_;
    mutable std::shared_mutex mutex_;

public:
    explicit BaseProxyFunction(Func&& func, FunctionInfo& info)
        : func_(std::move(func)) {
        collectFunctionInfo();
        calcFuncInfoHash();
        info = info_;
    }

    [[nodiscard]] auto getFunctionInfo() const -> FunctionInfo {
        std::shared_lock lock(mutex_);
        return info_;
    }

    /**
     * @brief Validate function arguments against expected types
     * @tparam Is Index sequence for parameter pack expansion
     * @param args Arguments to validate
     */
    template <std::size_t... Is>
    void validateArguments(std::vector<std::any>& args,
                           std::index_sequence<Is...>) {
        const bool typesMatch =
            (... &&
             (args[Is].type() ==
                  typeid(
                      std::decay_t<typename Traits::template argument_t<Is>>) ||
              tryConvertType<typename Traits::template argument_t<Is>>(
                  args[Is])));

        if (!typesMatch) {
            std::string errorMsg = "Argument type mismatch: expected (";
            std::string sep = "";

            ((errorMsg += sep + DemangleHelper::demangleType<
                                    typename Traits::template argument_t<Is>>(),
              sep = ", "),
             ...);

            errorMsg += ") but got (";
            sep = "";

            for (const auto& arg : args) {
                errorMsg += sep + arg.type().name();
                sep = ", ";
            }

            errorMsg += ")";
            throw ProxyTypeError(errorMsg);
        }
    }

protected:
    void collectFunctionInfo() {
        info_.setReturnType(
            DemangleHelper::demangleType<typename Traits::return_type>());
        collectArgumentTypes(std::make_index_sequence<ARITY>{});
        info_.setName("anonymous_function");

        if constexpr (Traits::is_noexcept) {
            info_.setNoexcept(true);
        }

        info_.setLocation(std::source_location::current());
    }

    template <std::size_t... Is>
    void collectArgumentTypes(std::index_sequence<Is...>) {
        (info_.addArgumentType(DemangleHelper::demangleType<
                               typename Traits::template argument_t<Is>>()),
         ...);
    }

    void calcFuncInfoHash() {
        if (!info_.getArgumentTypes().empty()) {
            std::string combinedTypes = info_.getReturnType();
            combinedTypes += info_.getName();
            for (const auto& argType : info_.getArgumentTypes()) {
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
    auto callFunction(std::vector<std::any>& args, std::index_sequence<Is...>)
        -> std::any {
        try {
            if constexpr (std::is_void_v<typename Traits::return_type>) {
                std::invoke(
                    func_,
                    anyCastHelper<typename Traits::template argument_t<Is>>(
                        args[Is])...);
                return {};
            } else {
                return std::make_any<typename Traits::return_type>(std::invoke(
                    func_,
                    anyCastHelper<typename Traits::template argument_t<Is>>(
                        args[Is])...));
            }
        } catch (const ProxyTypeError& e) {
            throw ProxyTypeError(std::string("Function call failed: ") +
                                 e.what());
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Function call threw exception: ") + e.what());
        } catch (...) {
            throw std::runtime_error("Function call threw unknown exception");
        }
    }

    auto callFunction(const FunctionParams& params) -> std::any {
        try {
            auto args = params.toAnyVector();
            return callFunction(args, std::make_index_sequence<ARITY>{});
        } catch (const ProxyTypeError& e) {
            throw ProxyTypeError(
                std::string("Function call with params failed: ") + e.what());
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Function call with params threw exception: ") +
                e.what());
        } catch (...) {
            throw std::runtime_error(
                "Function call with params threw unknown exception");
        }
    }

    template <std::size_t... Is>
    auto callMemberFunction(std::vector<std::any>& args,
                            std::index_sequence<Is...>) -> std::any {
        try {
            auto invokeFunc = [this](auto& obj, auto&&... args) {
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
                auto& obj =
                    std::any_cast<
                        std::reference_wrapper<typename Traits::class_type>>(
                        args[0])
                        .get();
                return invokeFunc(
                    obj,
                    anyCastHelper<typename Traits::template argument_t<Is>>(
                        args[Is + 1])...);
            }

            auto& obj = const_cast<typename Traits::class_type&>(
                std::any_cast<const typename Traits::class_type&>(args[0]));
            return invokeFunc(
                obj, anyCastHelper<typename Traits::template argument_t<Is>>(
                         args[Is + 1])...);
        } catch (const ProxyTypeError& e) {
            throw ProxyTypeError(std::string("Member function call failed: ") +
                                 e.what());
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Member function call threw exception: ") +
                e.what());
        } catch (...) {
            throw std::runtime_error(
                "Member function call threw unknown exception");
        }
    }
};

/**
 * @brief Proxy function class wrapping function calls and handling parameters
 * @tparam Func Function type to wrap
 */
template <typename Func>
class ProxyFunction : protected BaseProxyFunction<Func> {
    using Base = BaseProxyFunction<Func>;
    using Traits = typename Base::Traits;
    static constexpr std::size_t ARITY = Base::ARITY;

public:
    explicit ProxyFunction(Func&& func)
        : Base(std::forward<Func>(func), Base::info_) {}
    explicit ProxyFunction(Func&& func, FunctionInfo& info)
        : Base(std::forward<Func>(func), info) {}

    void setName(std::string_view name) {
        std::unique_lock lock(this->mutex_);
        this->info_.setName(name);
        this->calcFuncInfoHash();
    }

    void setParameterName(size_t index, std::string_view name) {
        std::unique_lock lock(this->mutex_);
        this->info_.setParameterName(index, name);
    }

    void setLocation(const std::source_location& location) {
        std::unique_lock lock(this->mutex_);
        this->info_.setLocation(location);
    }

    auto operator()(const std::vector<std::any>& args) -> std::any {
        std::shared_lock lock(this->mutex_);
        this->logArgumentTypes();

        try {
            auto mutableArgs = args;
            if constexpr (Traits::is_member_function) {
                if (args.size() != ARITY + 1) {
                    throw ProxyArgumentError(
                        "Incorrect number of arguments for member function: "
                        "expected " +
                        std::to_string(ARITY + 1) + ", got " +
                        std::to_string(args.size()));
                }
                this->validateArguments(mutableArgs,
                                        std::make_index_sequence<ARITY>());
                return this->callMemberFunction(
                    mutableArgs, std::make_index_sequence<ARITY>());
            } else {
                if (args.size() != ARITY) {
                    throw ProxyArgumentError(
                        "Incorrect number of arguments: expected " +
                        std::to_string(ARITY) + ", got " +
                        std::to_string(args.size()));
                }
                this->validateArguments(mutableArgs,
                                        std::make_index_sequence<ARITY>());
                return this->callFunction(mutableArgs,
                                          std::make_index_sequence<ARITY>());
            }
        } catch (const ProxyTypeError& e) {
            throw ProxyTypeError(std::string("Function call error: ") +
                                 e.what());
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Function threw exception: ") +
                                     e.what());
        } catch (...) {
            throw std::runtime_error("Function threw unknown exception");
        }
    }

    auto operator()(const FunctionParams& params) -> std::any {
        std::shared_lock lock(this->mutex_);
        this->logArgumentTypes();

        try {
            if constexpr (Traits::is_member_function) {
                if (params.size() != ARITY + 1) {
                    throw ProxyArgumentError(
                        "Incorrect number of parameters for member function: "
                        "expected " +
                        std::to_string(ARITY + 1) + ", got " +
                        std::to_string(params.size()));
                }
                auto args = params.toAnyVector();
                this->validateArguments(args,
                                        std::make_index_sequence<ARITY>());
                return this->callMemberFunction(
                    args, std::make_index_sequence<ARITY>());
            } else {
                if (params.size() != ARITY) {
                    throw ProxyArgumentError(
                        "Incorrect number of parameters: expected " +
                        std::to_string(ARITY) + ", got " +
                        std::to_string(params.size()));
                }
                return this->callFunction(params);
            }
        } catch (const ProxyTypeError& e) {
            throw ProxyTypeError(
                std::string("Function call with params error: ") + e.what());
        } catch (const std::exception& e) {
            throw std::runtime_error(
                std::string("Function with params threw exception: ") +
                e.what());
        } catch (...) {
            throw std::runtime_error(
                "Function with params threw unknown exception");
        }
    }
};

/**
 * @brief Async proxy function class supporting asynchronous calls
 * @tparam Func Function type to wrap
 */
template <typename Func>
class AsyncProxyFunction : protected BaseProxyFunction<Func> {
    using Base = BaseProxyFunction<Func>;
    using Traits = typename Base::Traits;
    static constexpr std::size_t ARITY = Base::ARITY;

public:
    explicit AsyncProxyFunction(Func&& func)
        : Base(std::forward<Func>(func), Base::info_) {}
    explicit AsyncProxyFunction(Func&& func, FunctionInfo& info)
        : Base(std::forward<Func>(func), info) {}

    void setName(std::string_view name) {
        std::unique_lock lock(this->mutex_);
        this->info_.setName(name);
        this->calcFuncInfoHash();
    }

    auto operator()(const std::vector<std::any>& args)
        -> std::future<std::any> {
        std::shared_lock lock(this->mutex_);
        this->logArgumentTypes();

        return std::async(std::launch::async, [this, args = args]() mutable {
            try {
                if constexpr (Traits::is_member_function) {
                    if (args.size() != ARITY + 1) {
                        throw ProxyArgumentError(
                            "Incorrect number of arguments for async member "
                            "function: expected " +
                            std::to_string(ARITY + 1) + ", got " +
                            std::to_string(args.size()));
                    }
                    this->validateArguments(args,
                                            std::make_index_sequence<ARITY>());
                    return this->callMemberFunction(
                        args, std::make_index_sequence<ARITY>());
                } else {
                    if (args.size() != ARITY) {
                        throw ProxyArgumentError(
                            "Incorrect number of arguments for async function: "
                            "expected " +
                            std::to_string(ARITY) + ", got " +
                            std::to_string(args.size()));
                    }
                    this->validateArguments(args,
                                            std::make_index_sequence<ARITY>());
                    return this->callFunction(
                        args, std::make_index_sequence<ARITY>());
                }
            } catch (const ProxyTypeError& e) {
                throw ProxyTypeError(
                    std::string("Async function call error: ") + e.what());
            } catch (const std::exception& e) {
                throw std::runtime_error(
                    std::string("Async function threw exception: ") + e.what());
            } catch (...) {
                throw std::runtime_error(
                    "Async function threw unknown exception");
            }
        });
    }

    auto operator()(const FunctionParams& params) -> std::future<std::any> {
        std::shared_lock lock(this->mutex_);
        this->logArgumentTypes();

        return std::async(
            std::launch::async, [this, params = params]() mutable {
                try {
                    if constexpr (Traits::is_member_function) {
                        if (params.size() != ARITY + 1) {
                            throw ProxyArgumentError(
                                "Incorrect number of parameters for async "
                                "member function: expected " +
                                std::to_string(ARITY + 1) + ", got " +
                                std::to_string(params.size()));
                        }
                        auto args = params.toAnyVector();
                        this->validateArguments(
                            args, std::make_index_sequence<ARITY>());
                        return this->callMemberFunction(
                            args, std::make_index_sequence<ARITY>());
                    } else {
                        if (params.size() != ARITY) {
                            throw ProxyArgumentError(
                                "Incorrect number of parameters for async "
                                "function: expected " +
                                std::to_string(ARITY) + ", got " +
                                std::to_string(params.size()));
                        }
                        return this->callFunction(params);
                    }
                } catch (const ProxyTypeError& e) {
                    throw ProxyTypeError(
                        std::string("Async function call with params error: ") +
                        e.what());
                } catch (const std::exception& e) {
                    throw std::runtime_error(
                        std::string(
                            "Async function with params threw exception: ") +
                        e.what());
                } catch (...) {
                    throw std::runtime_error(
                        "Async function with params threw unknown exception");
                }
            });
    }
};

/**
 * @brief Composed proxy class supporting function composition
 * @tparam Func1 First function type
 * @tparam Func2 Second function type
 */
template <typename Func1, typename Func2>
class ComposedProxy {
    ProxyFunction<Func1> first_;
    ProxyFunction<Func2> second_;
    mutable std::shared_mutex mutex_;
    FunctionInfo info_;

public:
    ComposedProxy(Func1&& f1, Func2&& f2)
        : first_(std::forward<Func1>(f1)), second_(std::forward<Func2>(f2)) {
        const auto& info1 = first_.getFunctionInfo();
        const auto& info2 = second_.getFunctionInfo();

        info_.setName("composed_" + info1.getName() + "_" + info2.getName());
        info_.setReturnType(info2.getReturnType());

        for (const auto& argType : info1.getArgumentTypes()) {
            info_.addArgumentType(argType);
        }

        std::string hash = info1.getHash() + "_" + info2.getHash();
        info_.setHash(hash);
    }

    ComposedProxy(const ComposedProxy& other)
        : first_(other.first_), second_(other.second_) {}
    ComposedProxy(ComposedProxy&& other) noexcept
        : first_(std::move(other.first_)), second_(std::move(other.second_)) {}

    ComposedProxy& operator=(const ComposedProxy& other) {
        if (this != &other) {
            first_ = other.first_;
            second_ = other.second_;
        }
        return *this;
    }

    ComposedProxy& operator=(ComposedProxy&& other) noexcept {
        if (this != &other) {
            first_ = std::move(other.first_);
            second_ = std::move(other.second_);
        }
        return *this;
    }

    [[nodiscard]] auto getFunctionInfo() const -> FunctionInfo {
        std::shared_lock lock(mutex_);
        return info_;
    }

    auto operator()(const std::vector<std::any>& args) -> std::any {
        std::shared_lock lock(mutex_);
        auto intermediateResult = first_(args);
        std::vector<std::any> secondArgs{std::move(intermediateResult)};
        return second_(secondArgs);
    }

    auto operator()(const FunctionParams& params) -> std::any {
        std::shared_lock lock(mutex_);
        auto intermediateResult = first_(params);
        FunctionParams secondParams;
        secondParams.push_back(Arg("result", intermediateResult));
        return second_(secondParams);
    }
};

template <typename Func>
ProxyFunction(Func) -> ProxyFunction<Func>;

template <typename Func>
ProxyFunction(Func&&, FunctionInfo&) -> ProxyFunction<std::decay_t<Func>>;

template <typename Func>
AsyncProxyFunction(Func) -> AsyncProxyFunction<Func>;

template <typename Func>
AsyncProxyFunction(Func&&, FunctionInfo&)
    -> AsyncProxyFunction<std::decay_t<Func>>;

/**
 * @brief Factory function to create a proxy
 * @tparam Func Function type
 * @param func Function to wrap
 * @return ProxyFunction instance
 */
template <typename Func>
auto makeProxy(Func&& func) {
    return ProxyFunction<std::decay_t<Func>>(std::forward<Func>(func));
}

/**
 * @brief Factory function to create an async proxy
 * @tparam Func Function type
 * @param func Function to wrap
 * @return AsyncProxyFunction instance
 */
template <typename Func>
auto makeAsyncProxy(Func&& func) {
    return AsyncProxyFunction<std::decay_t<Func>>(std::forward<Func>(func));
}

/**
 * @brief Factory function to create a composed proxy
 * @tparam Func1 First function type
 * @tparam Func2 Second function type
 * @param f1 First function
 * @param f2 Second function
 * @return ComposedProxy instance
 */
template <typename Func1, typename Func2>
auto composeProxy(Func1&& f1, Func2&& f2) {
    return ComposedProxy<std::decay_t<Func1>, std::decay_t<Func2>>(
        std::forward<Func1>(f1), std::forward<Func2>(f2));
}

}  // namespace atom::meta

#endif