/*!
 * \file facade_proxy.hpp
 * \brief Enhanced proxy functions utilizing the facade pattern for extended
 * capabilities
 * \author Max Qian <lightapt.com>
 * \date 2025-04-21
 * \copyright Copyright (C) 2023-2025 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_FACADE_PROXY_HPP
#define ATOM_META_FACADE_PROXY_HPP

#include <any>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "atom/meta/facade.hpp"
#include "atom/meta/proxy.hpp"

namespace atom::meta {

/*!
 * \namespace enhanced_proxy_skills
 * \brief Contains skill dispatch structures used by the enhanced proxy facade
 */
namespace enhanced_proxy_skills {

/*!
 * \brief Compile-time check whether Func can be invoked through the
 * type-erased call machinery.
 *
 * Either the callable natively accepts `const std::vector<std::any>&`
 * (bound/composed functions) or its signature can be analyzed via
 * FunctionTraits so ProxyFunction can unpack the arguments.
 */
template <typename Func>
concept erased_invocable =
    std::is_invocable_r_v<std::any, const Func&,
                          const std::vector<std::any>&> ||
    requires { typename FunctionTraits<Func>::return_type; };

/*!
 * \brief Invoke a wrapped callable with type-erased arguments.
 *
 * Callables that natively accept `const std::vector<std::any>&` are invoked
 * directly; everything else goes through ProxyFunction, which validates,
 * converts and unpacks the arguments.
 */
template <typename Func>
std::any invoke_erased(const Func& func, const std::vector<std::any>& args) {
    if constexpr (std::is_invocable_r_v<std::any, const Func&,
                                        const std::vector<std::any>&>) {
        return func(args);
    } else {
        ProxyFunction<Func> proxy_func{Func(func)};
        return proxy_func(args);
    }
}

/*!
 * \struct callable_dispatch
 * \brief Skill dispatch for synchronous function invocation
 */
struct callable_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = callable_dispatch;
    using uses_generic_skill_protocol = void;
    using invoke_func_t = std::any (*)(const void*,
                                       const std::vector<std::any>&);

    template <typename Func>
    static constexpr bool applicable = erased_invocable<Func>;

    template <typename Func>
    static const void* skill_entry() {
        return reinterpret_cast<const void*>(&invoke_impl<Func>);
    }

    template <class R>
        requires std::same_as<R, std::any>
    static R invoke_skill(const void* entry, const void* obj,
                          const std::vector<std::any>& args) {
        return reinterpret_cast<invoke_func_t>(entry)(obj, args);
    }

    template <typename Func>
    static std::any invoke_impl(const void* func_ptr,
                                const std::vector<std::any>& args) {
        return invoke_erased(*static_cast<const Func*>(func_ptr), args);
    }
};

/*!
 * \struct async_callable_dispatch
 * \brief Skill dispatch for asynchronous function invocation
 */
struct async_callable_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = async_callable_dispatch;
    using uses_generic_skill_protocol = void;
    using invoke_async_func_t =
        std::future<std::any> (*)(const void*, const std::vector<std::any>&);

    template <typename Func>
    static constexpr bool applicable = erased_invocable<Func>;

    template <typename Func>
    static const void* skill_entry() {
        return reinterpret_cast<const void*>(&invoke_async_impl<Func>);
    }

    template <class R>
        requires std::same_as<R, std::future<std::any>>
    static R invoke_skill(const void* entry, const void* obj,
                          const std::vector<std::any>& args) {
        return reinterpret_cast<invoke_async_func_t>(entry)(obj, args);
    }

    template <typename Func>
    static std::future<std::any> invoke_async_impl(
        const void* func_ptr, const std::vector<std::any>& args) {
        // Copy the callable into the task so it stays alive for the whole
        // asynchronous execution (the proxy storage could be destroyed
        // before the future runs).
        const Func& func = *static_cast<const Func*>(func_ptr);
        return std::async(std::launch::async,
                          [func = Func(func), args]() -> std::any {
                              return invoke_erased(func, args);
                          });
    }
};

/*!
 * \struct function_info_dispatch
 * \brief Skill dispatch for retrieving function metadata
 */
struct function_info_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = function_info_dispatch;
    using get_info_func_t = FunctionInfo (*)(const void*);
    using get_name_func_t = std::string (*)(const void*);
    using get_return_type_func_t = std::string (*)(const void*);
    using get_param_types_func_t = std::vector<std::string> (*)(const void*);

    template <typename Func>
    static FunctionInfo get_info_impl(const void* func_ptr) {
        const Func& func = *static_cast<const Func*>(func_ptr);
        FunctionInfo info;
        ProxyFunction<Func> proxy_func(func, info);
        return info;
    }

    template <typename Func>
    static std::string get_name_impl(const void* func_ptr) {
        FunctionInfo info = get_info_impl<Func>(func_ptr);
        return info.getName();
    }

    template <typename Func>
    static std::string get_return_type_impl(const void* func_ptr) {
        FunctionInfo info = get_info_impl<Func>(func_ptr);
        return info.getReturnType();
    }

    template <typename Func>
    static std::vector<std::string> get_param_types_impl(const void* func_ptr) {
        FunctionInfo info = get_info_impl<Func>(func_ptr);
        return info.getArgumentTypes();
    }
};

/*!
 * \struct serializable_dispatch
 * \brief Skill dispatch for serializing function metadata to JSON
 */
struct serializable_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = serializable_dispatch;
    using serialize_func_t = std::string (*)(const void*);

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
 * \brief Skill dispatch for printing function metadata to an output stream
 */
struct printable_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = printable_dispatch;
    using print_func_t = void (*)(const void*, std::ostream&);

    template <typename Func>
    static void print_impl(const void* func_ptr, std::ostream& os) {
        const Func& func = *static_cast<const Func*>(func_ptr);
        FunctionInfo info;
        ProxyFunction<Func> proxy_func(func, info);

        os << "Function: " << info.getName() << "\n"
           << "Return type: " << info.getReturnType() << "\n"
           << "Parameters: ";

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
 * \brief Skill dispatch for binding arguments to a function
 */
struct bindable_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = bindable_dispatch;
    using uses_generic_skill_protocol = void;
    using bind_func_t = std::shared_ptr<void> (*)(const void*,
                                                  const std::vector<std::any>&);

    template <typename Func>
    static constexpr bool applicable = erased_invocable<Func>;

    template <typename Func>
    static const void* skill_entry() {
        return reinterpret_cast<const void*>(&bind_impl<Func>);
    }

    template <class R>
        requires std::same_as<R, std::shared_ptr<void>>
    static R invoke_skill(const void* entry, const void* obj,
                          const std::vector<std::any>& bound_args) {
        return reinterpret_cast<bind_func_t>(entry)(obj, bound_args);
    }

    template <typename Func>
    static std::shared_ptr<void> bind_impl(
        const void* func_ptr, const std::vector<std::any>& bound_args) {
        const Func& func = *static_cast<const Func*>(func_ptr);

        std::function<std::any(const std::vector<std::any>&)> bound_func =
            [func = Func(func), bound_args](
                const std::vector<std::any>& call_args) -> std::any {
            std::vector<std::any> merged_args;
            merged_args.reserve(bound_args.size() + call_args.size());
            merged_args.insert(merged_args.end(), bound_args.begin(),
                               bound_args.end());
            merged_args.insert(merged_args.end(), call_args.begin(),
                               call_args.end());

            return invoke_erased(func, merged_args);
        };

        return std::make_shared<
            std::function<std::any(const std::vector<std::any>&)>>(
            std::move(bound_func));
    }
};

/*!
 * \struct composable_dispatch
 * \brief Skill dispatch for composing two functions
 */
struct composable_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = composable_dispatch;
    using compose_func_t = std::shared_ptr<void> (*)(const void*, const void*);

    template <typename Func1, typename Func2>
    static std::shared_ptr<void> compose_impl(const void* func1_ptr,
                                              const void* func2_ptr) {
        const Func1& func1 = *static_cast<const Func1*>(func1_ptr);
        const Func2& func2 = *static_cast<const Func2*>(func2_ptr);

        auto composed_func = composeProxy(func1, func2);
        return std::make_shared<decltype(composed_func)>(
            std::move(composed_func));
    }
};

}  // namespace enhanced_proxy_skills

/*!
 * \brief Builder for the enhanced proxy facade.
 *
 * The storage must be large enough (and sufficiently aligned) to hold
 * composed proxy functors such as ComposedProxy, and copyability is
 * nontrivial because std::function copies may throw.
 */
using enhanced_proxy_builder =
    facade_builder<std::tuple<>, std::tuple<>,
                   proxiable_constraints{
                       .max_size = 2048,
                       .max_align = 64,
                       .copyability = constraint_level::nontrivial,
                       .relocatability = constraint_level::nothrow,
                       .destructibility = constraint_level::nothrow,
                       .concurrency = thread_safety::none}>;

using enhanced_proxy_facade = enhanced_proxy_builder::add_convention<
    enhanced_proxy_skills::callable_dispatch,
    std::any(const std::vector<std::any>&)>::
    add_convention<enhanced_proxy_skills::async_callable_dispatch,
                   std::future<std::any>(const std::vector<std::any>&)>::
        add_convention<enhanced_proxy_skills::function_info_dispatch,
                       FunctionInfo(), std::string(), std::string(),
                       std::vector<std::string>()>::
            add_convention<
                enhanced_proxy_skills::serializable_dispatch,
                std::string()>::add_convention<enhanced_proxy_skills::
                                                   printable_dispatch,
                                               void(std::ostream&)>::
                add_convention<enhanced_proxy_skills::bindable_dispatch,
                               std::shared_ptr<void>(
                                   const std::vector<std::any>&)>::
                    add_convention<
                        enhanced_proxy_skills::composable_dispatch,
                        std::shared_ptr<void>(
                            const proxy<typename default_builder::build>&)>::
                        build;

/*!
 * \class EnhancedProxyFunction
 * \brief Enhanced proxy function using the facade pattern, providing extended
 * dynamic behavior and type erasure
 * \tparam Func The function type to wrap
 */
template <typename Func>
class EnhancedProxyFunction {
private:
    std::decay_t<Func> func_;
    proxy<enhanced_proxy_facade> proxy_;
    mutable FunctionInfo info_;

public:
    /*!
     * \brief Constructor
     * \param func The function to be proxied
     */
    explicit EnhancedProxyFunction(Func&& func)
        : func_(std::forward<Func>(func)) {
        initProxy();
        collectFunctionInfo();
    }

    /*!
     * \brief Constructor with name
     * \param func The function to be proxied
     * \param name The name of the function
     */
    EnhancedProxyFunction(Func&& func, std::string_view name)
        : func_(std::forward<Func>(func)) {
        initProxy();
        collectFunctionInfo();
        setName(name);
    }

    /*!
     * \brief Copy constructor
     */
    EnhancedProxyFunction(const EnhancedProxyFunction& other) = default;

    /*!
     * \brief Move constructor
     */
    EnhancedProxyFunction(EnhancedProxyFunction&& other) noexcept = default;

    /*!
     * \brief Copy assignment operator
     */
    EnhancedProxyFunction& operator=(const EnhancedProxyFunction& other) =
        default;

    /*!
     * \brief Move assignment operator
     */
    EnhancedProxyFunction& operator=(EnhancedProxyFunction&& other) noexcept =
        default;

    /*!
     * \brief Set the function name
     * \param name The name to set
     */
    void setName(std::string_view name) { info_.setName(std::string(name)); }

    /*!
     * \brief Set the parameter name
     * \param index The index of the parameter
     * \param name The name to set
     */
    void setParameterName(size_t index, std::string_view name) {
        info_.setParameterName(index, name);
    }

    /*!
     * \brief Get the function info
     * \return The function info
     *
     * Runtime metadata (name, parameter names) only exists in this wrapper,
     * so metadata queries answer from the locally collected info instead of
     * reconstructing it through the type-erased proxy.
     */
    [[nodiscard]] FunctionInfo getFunctionInfo() const { return info_; }

    /*!
     * \brief Get the function name
     * \return The function name
     */
    [[nodiscard]] std::string getName() const { return info_.getName(); }

    /*!
     * \brief Get the return type
     * \return The return type
     */
    [[nodiscard]] std::string getReturnType() const {
        return info_.getReturnType();
    }

    /*!
     * \brief Get the parameter types
     * \return The parameter types
     */
    [[nodiscard]] std::vector<std::string> getParameterTypes() const {
        return info_.getArgumentTypes();
    }

    /*!
     * \brief Invoke the function
     * \param args The arguments for the function call
     * \return The result of the function call
     */
    std::any operator()(const std::vector<std::any>& args) {
        return proxy_.call<enhanced_proxy_skills::callable_dispatch, std::any>(
            args);
    }

    /*!
     * \brief Invoke the function using FunctionParams
     * \param params The FunctionParams object containing the arguments
     * \return The result of the function call
     */
    std::any operator()(const FunctionParams& params) {
        return proxy_.call<enhanced_proxy_skills::callable_dispatch, std::any>(
            params.toAnyVector());
    }

    /*!
     * \brief Asynchronously invoke the function
     * \param args The arguments for the function call
     * \return A future holding the result of the asynchronous call
     */
    std::future<std::any> asyncCall(const std::vector<std::any>& args) {
        return proxy_.call<enhanced_proxy_skills::async_callable_dispatch,
                           std::future<std::any>>(args);
    }

    /*!
     * \brief Asynchronously invoke the function using FunctionParams
     * \param params The FunctionParams object containing the arguments
     * \return A future holding the result of the asynchronous call
     */
    std::future<std::any> asyncCall(const FunctionParams& params) {
        return proxy_.call<enhanced_proxy_skills::async_callable_dispatch,
                           std::future<std::any>>(params.toAnyVector());
    }

    /*!
     * \brief Serialize the function info to JSON
     * \return The JSON string representing the function info
     */
    [[nodiscard]] std::string serialize() const {
        return info_.toJson().dump();
    }

    /*!
     * \brief Print the function info to an output stream
     * \param os The output stream
     */
    void print(std::ostream& os = std::cout) const {
        os << "Function: " << info_.getName() << "\n"
           << "Return type: " << info_.getReturnType() << "\n"
           << "Parameters: ";

        const auto& arg_types = info_.getArgumentTypes();
        const auto& param_names = info_.getParameterNames();

        for (size_t i = 0; i < arg_types.size(); ++i) {
            if (i > 0)
                os << ", ";
            os << arg_types[i];
            if (i < param_names.size() && !param_names[i].empty()) {
                os << " " << param_names[i];
            }
        }

        os << "\n";
        if (info_.isNoexcept()) {
            os << "noexcept\n";
        }
    }

    /*!
     * \brief Bind arguments to the function
     * \tparam Args The types of the arguments to bind
     * \param args The arguments to bind
     * \return A new EnhancedProxyFunction with the bound arguments
     */
    template <typename... Args>
    EnhancedProxyFunction<std::function<std::any(const std::vector<std::any>&)>>
    bind(Args&&... args) const {
        std::vector<std::any> bound_args;
        (bound_args.push_back(std::forward<Args>(args)), ...);

        auto bound_func_ptr =
            proxy_.call<enhanced_proxy_skills::bindable_dispatch,
                        std::shared_ptr<void>>(bound_args);

        auto bound_func = *std::static_pointer_cast<
            std::function<std::any(const std::vector<std::any>&)>>(
            bound_func_ptr);

        return EnhancedProxyFunction<
            std::function<std::any(const std::vector<std::any>&)>>(
            std::move(bound_func), "bound_" + info_.getName());
    }

    /*!
     * \brief Get the proxy object
     * \return Reference to the internal proxy
     */
    [[nodiscard]] const proxy<enhanced_proxy_facade>& getProxy() const {
        return proxy_;
    }

    /*!
     * \brief Get the wrapped callable
     * \return Reference to the wrapped callable
     */
    [[nodiscard]] const std::decay_t<Func>& getFunction() const {
        return func_;
    }

    /*!
     * \brief Compose with another function: `other` consumes the leading
     * arguments, its result becomes this function's first argument and the
     * remaining arguments are passed through.
     *
     * For `outer.compose(inner)` with outer arity N, a call with arguments
     * (a..., b...) evaluates outer(inner(a...), b...) where b... are the
     * trailing N-1 arguments.
     *
     * \tparam OtherFunc The type of the inner function
     * \param other The inner function to compose with
     * \return A new composed function
     */
    template <typename OtherFunc>
    auto compose(const EnhancedProxyFunction<OtherFunc>& other) const {
        auto composed = [outer = func_, inner = other.getFunction()](
                            const std::vector<std::any>& args) -> std::any {
            constexpr std::size_t outer_arity = []() -> std::size_t {
                if constexpr (requires {
                                  FunctionTraits<std::decay_t<Func>>::arity;
                              }) {
                    return FunctionTraits<std::decay_t<Func>>::arity;
                } else {
                    return 1;
                }
            }();
            constexpr std::size_t rest = outer_arity > 0 ? outer_arity - 1 : 0;

            if (args.size() < rest) {
                throw ProxyArgumentError(
                    "Too few arguments for composed function: expected at "
                    "least " +
                    std::to_string(rest) + ", got " +
                    std::to_string(args.size()));
            }

            const auto split =
                args.end() - static_cast<std::ptrdiff_t>(rest);
            std::vector<std::any> inner_args(args.begin(), split);
            std::any intermediate =
                enhanced_proxy_skills::invoke_erased(inner, inner_args);

            std::vector<std::any> outer_args;
            outer_args.reserve(rest + 1);
            outer_args.push_back(std::move(intermediate));
            outer_args.insert(outer_args.end(), split, args.end());
            return enhanced_proxy_skills::invoke_erased(outer, outer_args);
        };

        return EnhancedProxyFunction<decltype(composed)>(
            std::move(composed),
            "composed_" + info_.getName() + "_" + other.getName());
    }

    /*!
     * \brief Output stream operator
     * \param os The output stream
     * \param func The EnhancedProxyFunction to print
     * \return The output stream
     */
    friend std::ostream& operator<<(std::ostream& os,
                                    const EnhancedProxyFunction& func) {
        func.print(os);
        return os;
    }

private:
    void initProxy() { proxy_ = proxy<enhanced_proxy_facade>(func_); }

    void collectFunctionInfo() {
        if constexpr (requires { sizeof(FunctionTraits<std::decay_t<Func>>); }) {
            std::decay_t<Func> func_copy(func_);
            ProxyFunction<std::decay_t<Func>> proxy_func(std::move(func_copy),
                                                         info_);
        }
    }
};

template <typename Func>
EnhancedProxyFunction(Func) -> EnhancedProxyFunction<Func>;

template <typename Func>
EnhancedProxyFunction(Func&&, std::string_view)
    -> EnhancedProxyFunction<std::decay_t<Func>>;

/*!
 * \brief Factory function to create an enhanced proxy
 * \tparam Func The type of the function
 * \param func The function to be proxied
 * \return An EnhancedProxyFunction
 */
template <typename Func>
auto makeEnhancedProxy(Func&& func) {
    return EnhancedProxyFunction<std::decay_t<Func>>(std::forward<Func>(func));
}

/*!
 * \brief Factory function to create a named enhanced proxy
 * \tparam Func The type of the function
 * \param func The function to be proxied
 * \param name The name of the function
 * \return An EnhancedProxyFunction
 */
template <typename Func>
auto makeEnhancedProxy(Func&& func, std::string_view name) {
    return EnhancedProxyFunction<std::decay_t<Func>>(std::forward<Func>(func),
                                                     name);
}

}  // namespace atom::meta

#endif  // ATOM_META_FACADE_PROXY_HPP
