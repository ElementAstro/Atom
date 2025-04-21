/*!
 * \file enhanced_proxy.hpp
 * \brief Defines enhanced proxy functions utilizing the facade pattern for
 * extended capabilities.
 * \author Max Qian <lightapt.com>
 * \date 2025-04-21
 * \copyright Copyright (C) 2023-2025 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_ENHANCED_PROXY_HPP
#define ATOM_META_ENHANCED_PROXY_HPP

#include <any>
#include <functional>
#include <future>
#include <iostream>  // Required for std::ostream in printable_dispatch
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "atom/meta/facade.hpp"  // Include facade system for skill dispatch
#include "atom/meta/proxy.hpp"  // Include base proxy functionality (ProxyFunction, FunctionInfo, etc.)

namespace atom::meta {

/*!
 * \namespace enhanced_proxy_skills
 * \brief Contains skill dispatch structures used by the enhanced proxy facade.
 *
 * These structures define the implementation details for various capabilities
 * (skills) that the EnhancedProxyFunction can possess, such as synchronous
 * and asynchronous invocation, metadata retrieval, serialization, etc.
 */
namespace enhanced_proxy_skills {

/*!
 * \struct callable_dispatch
 * \brief Skill dispatch for synchronous function invocation.
 *
 * Implements the logic to call the underlying function synchronously
 * using the existing `ProxyFunction` mechanism.
 */
struct callable_dispatch {
    static constexpr bool is_direct = false;  ///< Indicates indirect dispatch.
    using dispatch_type =
        callable_dispatch;  ///< Type alias for the dispatch structure.
    /*!
     * \brief Function pointer type for the invocation implementation.
     * \param const void* Pointer to the function object.
     * \param const std::vector<std::any>& Vector of arguments.
     * \return std::any The result of the function call.
     */
    using invoke_func_t = std::any (*)(const void*,
                                       const std::vector<std::any>&);

    /*!
     * \brief Implementation of the synchronous invocation skill.
     * \tparam Func The type of the underlying function object.
     * \param func_ptr A type-erased pointer to the function object.
     * \param args A vector of `std::any` containing the arguments for the call.
     * \return `std::any` containing the return value of the function call.
     */
    template <typename Func>
    static std::any invoke_impl(const void* func_ptr,
                                const std::vector<std::any>& args) {
        const Func& func = *static_cast<const Func*>(func_ptr);

        // Use the existing ProxyFunction invocation mechanism
        ProxyFunction<Func> proxy_func(func);
        return proxy_func(args);
    }
};

/*!
 * \struct async_callable_dispatch
 * \brief Skill dispatch for asynchronous function invocation.
 *
 * Implements the logic to call the underlying function asynchronously
 * using the existing `AsyncProxyFunction` mechanism, returning a `std::future`.
 */
struct async_callable_dispatch {
    static constexpr bool is_direct = false;  ///< Indicates indirect dispatch.
    using dispatch_type =
        async_callable_dispatch;  ///< Type alias for the dispatch structure.
    /*!
     * \brief Function pointer type for the asynchronous invocation
     * implementation.
     * \param const void* Pointer to the function object.
     * \param const std::vector<std::any>& Vector of arguments.
     * \return std::future<std::any> A future holding the result of the
     * asynchronous call.
     */
    using invoke_async_func_t =
        std::future<std::any> (*)(const void*, const std::vector<std::any>&);

    /*!
     * \brief Implementation of the asynchronous invocation skill.
     * \tparam Func The type of the underlying function object.
     * \param func_ptr A type-erased pointer to the function object.
     * \param args A vector of `std::any` containing the arguments for the call.
     * \return `std::future<std::any>` representing the asynchronous result.
     */
    template <typename Func>
    static std::future<std::any> invoke_async_impl(
        const void* func_ptr, const std::vector<std::any>& args) {
        const Func& func = *static_cast<const Func*>(func_ptr);

        // Use the existing AsyncProxyFunction invocation mechanism
        AsyncProxyFunction<Func> async_proxy_func(func);
        return async_proxy_func(args);
    }
};

/*!
 * \struct function_info_dispatch
 * \brief Skill dispatch for retrieving function metadata.
 *
 * Implements the logic to extract function metadata (name, return type,
 * parameter types) using the `ProxyFunction` and `FunctionInfo` structures.
 */
struct function_info_dispatch {
    static constexpr bool is_direct = false;  ///< Indicates indirect dispatch.
    using dispatch_type =
        function_info_dispatch;  ///< Type alias for the dispatch structure.
    /// Function pointer type for getting the full FunctionInfo object.
    using get_info_func_t = FunctionInfo (*)(const void*);
    /// Function pointer type for getting the function name.
    using get_name_func_t = std::string (*)(const void*);
    /// Function pointer type for getting the return type name.
    using get_return_type_func_t = std::string (*)(const void*);
    /// Function pointer type for getting the parameter type names.
    using get_param_types_func_t = std::vector<std::string> (*)(const void*);

    /*!
     * \brief Implementation to get the complete `FunctionInfo` object.
     * \tparam Func The type of the underlying function object.
     * \param func_ptr A type-erased pointer to the function object.
     * \return `FunctionInfo` containing the metadata.
     */
    template <typename Func>
    static FunctionInfo get_info_impl(const void* func_ptr) {
        const Func& func = *static_cast<const Func*>(func_ptr);
        FunctionInfo info;
        ProxyFunction<Func> proxy_func(func,
                                       info);  // Populates info by reference
        return info;
    }

    /*!
     * \brief Implementation to get the function name.
     * \tparam Func The type of the underlying function object.
     * \param func_ptr A type-erased pointer to the function object.
     * \return `std::string` containing the function name.
     */
    template <typename Func>
    static std::string get_name_impl(const void* func_ptr) {
        FunctionInfo info = get_info_impl<Func>(func_ptr);
        return info.getName();
    }

    /*!
     * \brief Implementation to get the function return type name.
     * \tparam Func The type of the underlying function object.
     * \param func_ptr A type-erased pointer to the function object.
     * \return `std::string` containing the return type name.
     */
    template <typename Func>
    static std::string get_return_type_impl(const void* func_ptr) {
        FunctionInfo info = get_info_impl<Func>(func_ptr);
        return info.getReturnType();
    }

    /*!
     * \brief Implementation to get the function parameter type names.
     * \tparam Func The type of the underlying function object.
     * \param func_ptr A type-erased pointer to the function object.
     * \return `std::vector<std::string>` containing the parameter type names.
     */
    template <typename Func>
    static std::vector<std::string> get_param_types_impl(const void* func_ptr) {
        FunctionInfo info = get_info_impl<Func>(func_ptr);
        return info.getArgumentTypes();
    }
};

/*!
 * \struct serializable_dispatch
 * \brief Skill dispatch for serializing function metadata to JSON.
 *
 * Implements the logic to serialize the function metadata to a JSON string
 * using the `FunctionInfo` structure.
 */
struct serializable_dispatch {
    static constexpr bool is_direct = false;  ///< Indicates indirect dispatch.
    using dispatch_type =
        serializable_dispatch;  ///< Type alias for the dispatch structure.
    /*!
     * \brief Function pointer type for the serialization implementation.
     * \param const void* Pointer to the function object.
     * \return std::string The JSON string representing the function metadata.
     */
    using serialize_func_t = std::string (*)(const void*);

    /*!
     * \brief Implementation of the serialization skill.
     * \tparam Func The type of the underlying function object.
     * \param func_ptr A type-erased pointer to the function object.
     * \return `std::string` containing the JSON representation of the function
     * metadata.
     */
    template <typename Func>
    static std::string serialize_impl(const void* func_ptr) {
        const Func& func = *static_cast<const Func*>(func_ptr);
        FunctionInfo info;
        ProxyFunction<Func> proxy_func(func, info);
        return info.toJson().dump();
    }
};

/*!
 * \struct printable_dispatch
 * \brief Skill dispatch for printing function metadata to an output stream.
 *
 * Implements the logic to print the function metadata to an output stream
 * using the `FunctionInfo` structure.
 */
struct printable_dispatch {
    static constexpr bool is_direct = false;  ///< Indicates indirect dispatch.
    using dispatch_type =
        printable_dispatch;  ///< Type alias for the dispatch structure.
    /*!
     * \brief Function pointer type for the print implementation.
     * \param const void* Pointer to the function object.
     * \param std::ostream& Reference to the output stream.
     */
    using print_func_t = void (*)(const void*, std::ostream&);

    /*!
     * \brief Implementation of the print skill.
     * \tparam Func The type of the underlying function object.
     * \param func_ptr A type-erased pointer to the function object.
     * \param os Reference to the output stream.
     */
    template <typename Func>
    static void print_impl(const void* func_ptr, std::ostream& os) {
        const Func& func = *static_cast<const Func*>(func_ptr);
        FunctionInfo info;
        ProxyFunction<Func> proxy_func(func, info);

        os << "Function: " << info.getName() << "\n";
        os << "Return type: " << info.getReturnType() << "\n";
        os << "Parameters: ";

        const auto& arg_types = info.getArgumentTypes();
        const auto& param_names = info.getParameterNames();

        for (size_t i = 0; i < arg_types.size(); ++i) {
            if (i > 0)
                os << ", ";

            os << arg_types[i];

            if (i < param_names.size() && !param_names[i].empty()) {
                os << " " << param_names[i];
            }
        }

        os << "\n";
        if (info.isNoexcept()) {
            os << "noexcept\n";
        }
    }
};

/*!
 * \struct bindable_dispatch
 * \brief Skill dispatch for binding arguments to a function.
 *
 * Implements the logic to bind arguments to the underlying function,
 * creating a new function object with the bound arguments.
 */
struct bindable_dispatch {
    static constexpr bool is_direct = false;  ///< Indicates indirect dispatch.
    using dispatch_type =
        bindable_dispatch;  ///< Type alias for the dispatch structure.
    /*!
     * \brief Function pointer type for the bind implementation.
     * \param const void* Pointer to the function object.
     * \param const std::vector<std::any>& Vector of arguments to bind.
     * \return void* Pointer to the new function object with bound arguments.
     */
    using bind_func_t = void* (*)(const void*, const std::vector<std::any>&);

    /*!
     * \brief Implementation of the bind skill.
     * \tparam Func The type of the underlying function object.
     * \param func_ptr A type-erased pointer to the function object.
     * \param bound_args A vector of `std::any` containing the arguments to
     * bind.
     * \return `void*` pointing to the new function object with bound arguments.
     */
    template <typename Func>
    static void* bind_impl(const void* func_ptr,
                           const std::vector<std::any>& bound_args) {
        const Func& func = *static_cast<const Func*>(func_ptr);

        // Create a function object that merges bound arguments with call-time
        // arguments
        auto bound_func =
            [func,
             bound_args](const std::vector<std::any>& call_args) -> std::any {
            std::vector<std::any> merged_args;
            merged_args.reserve(bound_args.size() + call_args.size());

            // Merge bound arguments and call-time arguments
            merged_args.insert(merged_args.end(), bound_args.begin(),
                               bound_args.end());
            merged_args.insert(merged_args.end(), call_args.begin(),
                               call_args.end());

            ProxyFunction<Func> proxy_func(func);
            return proxy_func(merged_args);
        };

        // Create a heap-allocated copy and return
        return new auto(bound_func);
    }
};

/*!
 * \struct composable_dispatch
 * \brief Skill dispatch for composing two functions.
 *
 * Implements the logic to compose two functions, creating a new function
 * object that represents the composition of the two.
 */
struct composable_dispatch {
    static constexpr bool is_direct = false;  ///< Indicates indirect dispatch.
    using dispatch_type =
        composable_dispatch;  ///< Type alias for the dispatch structure.
    /*!
     * \brief Function pointer type for the compose implementation.
     * \param const void* Pointer to the first function object.
     * \param const void* Pointer to the second function object.
     * \return void* Pointer to the new composed function object.
     */
    using compose_func_t = void* (*)(const void*, const void*);

    /*!
     * \brief Implementation of the compose skill.
     * \tparam Func1 The type of the first function object.
     * \tparam Func2 The type of the second function object.
     * \param func1_ptr A type-erased pointer to the first function object.
     * \param func2_ptr A type-erased pointer to the second function object.
     * \return `void*` pointing to the new composed function object.
     */
    template <typename Func1, typename Func2>
    static void* compose_impl(const void* func1_ptr, const void* func2_ptr) {
        const Func1& func1 = *static_cast<const Func1*>(func1_ptr);
        const Func2& func2 = *static_cast<const Func2*>(func2_ptr);

        // Create a composed function object
        auto composed_func = composeProxy(func1, func2);

        // Create a heap-allocated copy and return
        return new auto(composed_func);
    }
};

}  // namespace enhanced_proxy_skills

// Define the facade for EnhancedProxyFunction
using enhanced_proxy_facade = default_builder::
    // Add support for various skills
    add_convention<enhanced_proxy_skills::callable_dispatch,
                   std::any(const std::vector<std::any>&)>::
        add_convention<enhanced_proxy_skills::async_callable_dispatch,
                       std::future<std::any>(const std::vector<std::any>&)>::
            add_convention<enhanced_proxy_skills::function_info_dispatch,
                           FunctionInfo(), std::string(), std::string(),
                           std::vector<std::string>()>::
                add_convention<enhanced_proxy_skills::serializable_dispatch,
                               std::string()>::
                    add_convention<enhanced_proxy_skills::printable_dispatch,
                                   void(std::ostream&)>::
                        add_convention<enhanced_proxy_skills::bindable_dispatch,
                                       void*(const std::vector<std::any>&)>::
                            add_convention<
                                enhanced_proxy_skills::composable_dispatch,
                                void*(const proxy<
                                      typename default_builder::build>&)>::
                                // Add constraints
    restrict_layout<128>::support_copy<constraint_level::nothrow>::
        support_relocation<constraint_level::nothrow>::support_destruction<
            constraint_level::nothrow>::build;

/*!
 * \class EnhancedProxyFunction
 * \brief Enhanced proxy function using the facade pattern, providing extended
 * dynamic behavior and type erasure.
 */
template <typename Func>
class EnhancedProxyFunction {
private:
    // Store the original function
    std::decay_t<Func> func_;

    // Use proxy<F> for skill dispatch
    proxy<enhanced_proxy_facade> proxy_;

    // Function info cache
    mutable FunctionInfo info_;

public:
    /*!
     * \brief Constructor
     * \param func The function to be proxied.
     */
    explicit EnhancedProxyFunction(Func&& func)
        : func_(std::forward<Func>(func)) {
        initProxy();
        collectFunctionInfo();
    }

    /*!
     * \brief Constructor with name
     * \param func The function to be proxied.
     * \param name The name of the function.
     */
    EnhancedProxyFunction(Func&& func, std::string_view name)
        : func_(std::forward<Func>(func)) {
        initProxy();
        collectFunctionInfo();
        setName(name);
    }

    /*!
     * \brief Copy constructor
     * \param other The other EnhancedProxyFunction to copy from.
     */
    EnhancedProxyFunction(const EnhancedProxyFunction& other)
        : func_(other.func_), proxy_(other.proxy_), info_(other.info_) {}

    /*!
     * \brief Move constructor
     * \param other The other EnhancedProxyFunction to move from.
     */
    EnhancedProxyFunction(EnhancedProxyFunction&& other) noexcept
        : func_(std::move(other.func_)),
          proxy_(std::move(other.proxy_)),
          info_(std::move(other.info_)) {}

    /*!
     * \brief Copy assignment
     * \param other The other EnhancedProxyFunction to copy from.
     * \return Reference to this EnhancedProxyFunction.
     */
    EnhancedProxyFunction& operator=(const EnhancedProxyFunction& other) {
        if (this != &other) {
            func_ = other.func_;
            proxy_ = other.proxy_;
            info_ = other.info_;
        }
        return *this;
    }

    /*!
     * \brief Move assignment
     * \param other The other EnhancedProxyFunction to move from.
     * \return Reference to this EnhancedProxyFunction.
     */
    EnhancedProxyFunction& operator=(EnhancedProxyFunction&& other) noexcept {
        if (this != &other) {
            func_ = std::move(other.func_);
            proxy_ = std::move(other.proxy_);
            info_ = std::move(other.info_);
        }
        return *this;
    }

    /*!
     * \brief Set the function name
     * \param name The name to set.
     */
    void setName(std::string_view name) {
        info_.setName(std::string(name));
        // Recompute the hash value
        ProxyFunction<Func> proxy_func(func_, info_);
    }

    /*!
     * \brief Set the parameter name
     * \param index The index of the parameter.
     * \param name The name to set.
     */
    void setParameterName(size_t index, std::string_view name) {
        info_.setParameterName(index, name);
    }

    /*!
     * \brief Get the function info
     * \return The function info.
     */
    [[nodiscard]] FunctionInfo getFunctionInfo() const {
        return proxy_.call<enhanced_proxy_skills::function_info_dispatch,
                           FunctionInfo>();
    }

    /*!
     * \brief Get the function name
     * \return The function name.
     */
    [[nodiscard]] std::string getName() const {
        return proxy_
            .call<enhanced_proxy_skills::function_info_dispatch, std::string>();
    }

    /*!
     * \brief Get the return type
     * \return The return type.
     */
    [[nodiscard]] std::string getReturnType() const {
        return proxy_
            .call<enhanced_proxy_skills::function_info_dispatch, std::string>();
    }

    /*!
     * \brief Get the parameter types
     * \return The parameter types.
     */
    [[nodiscard]] std::vector<std::string> getParameterTypes() const {
        return proxy_.call<enhanced_proxy_skills::function_info_dispatch,
                           std::vector<std::string>>();
    }

    /*!
     * \brief Invoke the function
     * \param args The arguments for the function call.
     * \return The result of the function call.
     */
    std::any operator()(const std::vector<std::any>& args) {
        return proxy_.call<enhanced_proxy_skills::callable_dispatch, std::any>(
            args);
    }

    /*!
     * \brief Invoke the function using FunctionParams
     * \param params The FunctionParams object containing the arguments.
     * \return The result of the function call.
     */
    std::any operator()(const FunctionParams& params) {
        return proxy_.call<enhanced_proxy_skills::callable_dispatch, std::any>(
            params.toAnyVector());
    }

    /*!
     * \brief Asynchronously invoke the function
     * \param args The arguments for the function call.
     * \return A future holding the result of the asynchronous call.
     */
    std::future<std::any> asyncCall(const std::vector<std::any>& args) {
        return proxy_.call<enhanced_proxy_skills::async_callable_dispatch,
                           std::future<std::any>>(args);
    }

    /*!
     * \brief Asynchronously invoke the function using FunctionParams
     * \param params The FunctionParams object containing the arguments.
     * \return A future holding the result of the asynchronous call.
     */
    std::future<std::any> asyncCall(const FunctionParams& params) {
        return proxy_.call<enhanced_proxy_skills::async_callable_dispatch,
                           std::future<std::any>>(params.toAnyVector());
    }

    /*!
     * \brief Serialize the function info to JSON
     * \return The JSON string representing the function info.
     */
    [[nodiscard]] std::string serialize() const {
        return proxy_
            .call<enhanced_proxy_skills::serializable_dispatch, std::string>();
    }

    /*!
     * \brief Print the function info to an output stream
     * \param os The output stream.
     */
    void print(std::ostream& os = std::cout) const {
        proxy_.call<enhanced_proxy_skills::printable_dispatch>(os);
    }

    /*!
     * \brief Bind arguments to the function
     * \tparam Args The types of the arguments to bind.
     * \param args The arguments to bind.
     * \return A new EnhancedProxyFunction with the bound arguments.
     */
    template <typename... Args>
    EnhancedProxyFunction<std::function<std::any(const std::vector<std::any>&)>>
    bind(Args&&... args) const {
        std::vector<std::any> bound_args;
        (bound_args.push_back(std::forward<Args>(args)), ...);

        void* bound_func_ptr =
            proxy_.call<enhanced_proxy_skills::bindable_dispatch, void*>(
                bound_args);
        
        // 使用移动语义
        auto bound_func = std::move(*static_cast<
            std::function<std::any(const std::vector<std::any>&)>*>(
            bound_func_ptr));

        // 清理临时对象
        delete static_cast<
            std::function<std::any(const std::vector<std::any>&)>*>(
            bound_func_ptr);

        return EnhancedProxyFunction<
            std::function<std::any(const std::vector<std::any>&)>>(
            std::move(bound_func), "bound_" + info_.getName());
    }

    // 添加此公共方法以安全地访问proxy_
    [[nodiscard]] const proxy<enhanced_proxy_facade>& getProxy() const {
        return proxy_;
    }

    // 修改compose方法
    template <typename OtherFunc>
    auto compose(const EnhancedProxyFunction<OtherFunc>& other) const {
        using ComposedFuncType = decltype(composeProxy(
            std::declval<Func>(), std::declval<OtherFunc>()));

        // 使用公共方法获取proxy而不是直接访问私有成员
        void* composed_func_ptr =
            proxy_.call<enhanced_proxy_skills::composable_dispatch, void*>(
                other.getProxy());
        
        // 使用移动语义而不是复制
        auto composed_func = std::move(*static_cast<ComposedFuncType*>(composed_func_ptr));

        // 清理临时对象
        delete static_cast<ComposedFuncType*>(composed_func_ptr);

        return EnhancedProxyFunction<ComposedFuncType>(
            std::move(composed_func),
            "composed_" + info_.getName() + "_" + other.getName());
    }

    /*!
     * \brief Output stream operator
     * \param os The output stream.
     * \param func The EnhancedProxyFunction to print.
     * \return The output stream.
     */
    friend std::ostream& operator<<(std::ostream& os,
                                    const EnhancedProxyFunction& func) {
        func.print(os);
        return os;
    }

private:
    /*!
     * \brief Initialize the proxy object
     */
    void initProxy() { proxy_ = proxy<enhanced_proxy_facade>(func_); }

    /*!
     * \brief Collect function info
     */
    void collectFunctionInfo() {
        ProxyFunction<std::decay_t<Func>> proxy_func(func_, info_);
        // info_ is populated by reference
    }
};

// C++17 deduction guides
template <typename Func>
EnhancedProxyFunction(Func) -> EnhancedProxyFunction<Func>;

template <typename Func>
EnhancedProxyFunction(Func&&, std::string_view)
    -> EnhancedProxyFunction<std::decay_t<Func>>;

/*!
 * \brief Factory function to create an enhanced proxy
 * \tparam Func The type of the function.
 * \param func The function to be proxied.
 * \return An EnhancedProxyFunction.
 */
template <typename Func>
auto makeEnhancedProxy(Func&& func) {
    return EnhancedProxyFunction<std::decay_t<Func>>(std::forward<Func>(func));
}

/*!
 * \brief Factory function to create a named enhanced proxy
 * \tparam Func The type of the function.
 * \param func The function to be proxied.
 * \param name The name of the function.
 * \return An EnhancedProxyFunction.
 */
template <typename Func>
auto makeEnhancedProxy(Func&& func, std::string_view name) {
    return EnhancedProxyFunction<std::decay_t<Func>>(std::forward<Func>(func),
                                                     name);
}

}  // namespace atom::meta

#endif  // ATOM_META_ENHANCED_PROXY_HPP