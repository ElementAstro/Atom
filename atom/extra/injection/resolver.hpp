#pragma once

#include <memory>
#include <optional>
#include "common.hpp"
#include "inject.hpp"

namespace atom::extra {

/**
 * @class Resolver
 * @brief An abstract base class for resolving dependencies.
 * @tparam T The type of the dependency.
 * @tparam SymbolTypes The symbol types associated with the resolver.
 */
template <typename T, typename... SymbolTypes>
class Resolver {
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~Resolver() = default;

    /**
     * @brief Resolves the dependency.
     * @param context The context for resolving the dependency.
     * @return The resolved dependency.
     */
    virtual T resolve(const Context<SymbolTypes...>& context) = 0;
};

/**
 * @brief A type alias for a shared pointer to a Resolver.
 * @tparam T The type of the dependency.
 * @tparam SymbolTypes The symbol types associated with the resolver.
 */
template <typename T, typename... SymbolTypes>
using ResolverPtr = std::shared_ptr<Resolver<T, SymbolTypes...>>;

/**
 * @class ConstantResolver
 * @brief A resolver that returns a constant value.
 * @tparam T The type of the dependency.
 * @tparam SymbolTypes The symbol types associated with the resolver.
 */
template <typename T, typename... SymbolTypes>
class ConstantResolver : public Resolver<T, SymbolTypes...> {
public:
    /**
     * @brief Constructs a ConstantResolver with a constant value.
     * @param value The constant value to return.
     */
    explicit ConstantResolver(T value) : value_(std::move(value)) {}

    /**
     * @brief Resolves the dependency by returning the constant value.
     * @param context The context for resolving the dependency.
     * @return The constant value.
     */
    T resolve(const Context<SymbolTypes...>&) override { return value_; }

private:
    T value_;  ///< The constant value.
};

/**
 * @class DynamicResolver
 * @brief A resolver that returns a dynamic value generated by a factory.
 * @tparam T The type of the dependency.
 * @tparam SymbolTypes The symbol types associated with the resolver.
 */
template <typename T, typename... SymbolTypes>
class DynamicResolver : public Resolver<T, SymbolTypes...> {
public:
    /**
     * @brief Constructs a DynamicResolver with a factory function.
     * @param factory The factory function to generate the dynamic value.
     */
    explicit DynamicResolver(Factory<T, SymbolTypes...> factory)
        : factory_(std::move(factory)) {}

    /**
     * @brief Resolves the dependency by calling the factory function.
     * @param context The context for resolving the dependency.
     * @return The dynamic value generated by the factory.
     */
    T resolve(const Context<SymbolTypes...>& context) override {
        return factory_(context);
    }

private:
    Factory<T, SymbolTypes...> factory_;  ///< The factory function.
};

/**
 * @class AutoResolver
 * @brief A resolver that automatically resolves dependencies for a type.
 * @tparam T The type of the dependency.
 * @tparam U The type to instantiate.
 * @tparam SymbolTypes The symbol types associated with the resolver.
 */
template <typename T, typename U, typename... SymbolTypes>
class AutoResolver : public Resolver<T, SymbolTypes...> {
public:
    /**
     * @brief Resolves the dependency by automatically instantiating the type.
     * @param context The context for resolving the dependency.
     * @return The instantiated type.
     */
    T resolve(const Context<SymbolTypes...>& context) override {
        return std::make_from_tuple<U>(InjectableA<U>::resolve(context));
    }
};

/**
 * @class AutoResolver specialization for std::unique_ptr
 * @brief A resolver that automatically resolves dependencies for a unique
 * pointer type.
 * @tparam T The type of the dependency.
 * @tparam U The type to instantiate.
 * @tparam SymbolTypes The symbol types associated with the resolver.
 */
template <typename T, typename U, typename... SymbolTypes>
class AutoResolver<std::unique_ptr<T>, U, SymbolTypes...>
    : public Resolver<std::unique_ptr<T>, SymbolTypes...> {
public:
    /**
     * @brief Resolves the dependency by automatically instantiating the type as
     * a unique pointer.
     * @param context The context for resolving the dependency.
     * @return The instantiated type as a unique pointer.
     */
    std::unique_ptr<T> resolve(
        const Context<SymbolTypes...>& context) override {
        return std::apply(
            [](auto&&... deps) {
                return std::make_unique<U>(
                    std::forward<decltype(deps)>(deps)...);
            },
            InjectableA<U>::resolve(context));
    }
};

/**
 * @class AutoResolver specialization for std::shared_ptr
 * @brief A resolver that automatically resolves dependencies for a shared
 * pointer type.
 * @tparam T The type of the dependency.
 * @tparam U The type to instantiate.
 * @tparam SymbolTypes The symbol types associated with the resolver.
 */
template <typename T, typename U, typename... SymbolTypes>
class AutoResolver<std::shared_ptr<T>, U, SymbolTypes...>
    : public Resolver<std::shared_ptr<T>, SymbolTypes...> {
public:
    /**
     * @brief Resolves the dependency by automatically instantiating the type as
     * a shared pointer.
     * @param context The context for resolving the dependency.
     * @return The instantiated type as a shared pointer.
     */
    std::shared_ptr<T> resolve(
        const Context<SymbolTypes...>& context) override {
        return std::apply(
            [](auto&&... deps) {
                return std::make_shared<U>(
                    std::forward<decltype(deps)>(deps)...);
            },
            InjectableA<U>::resolve(context));
    }
};

/**
 * @class CachedResolver
 * @brief A resolver that caches the resolved value.
 * @tparam T The type of the dependency.
 * @tparam SymbolTypes The symbol types associated with the resolver.
 */
template <typename T, typename... SymbolTypes>
class CachedResolver : public Resolver<T, SymbolTypes...> {
    static_assert(
        std::is_copy_constructible_v<T>,
        "atom::extra::CachedResolver requires a copy constructor. Are "
        "you caching a unique_ptr?");

public:
    /**
     * @brief Constructs a CachedResolver with a parent resolver.
     * @param parent The parent resolver to cache the value from.
     */
    explicit CachedResolver(ResolverPtr<T, SymbolTypes...> parent)
        : parent_(std::move(parent)) {}

    /**
     * @brief Resolves the dependency by returning the cached value or resolving
     * it from the parent.
     * @param context The context for resolving the dependency.
     * @return The cached value or the resolved value from the parent.
     */
    T resolve(const Context<SymbolTypes...>& context) override {
        if (!cached_.has_value()) {
            cached_ = parent_->resolve(context);
        }
        return cached_.value();
    }

private:
    std::optional<T> cached_;                ///< The cached value.
    ResolverPtr<T, SymbolTypes...> parent_;  ///< The parent resolver.
};

}  // namespace atom::extra
