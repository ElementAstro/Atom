#pragma once

#include "common.hpp"
#include "resolver.hpp"

namespace atom::extra {

/**
 * @class BindingScope
 * @brief A class template for managing the lifecycle of bindings.
 * @tparam T The type of the binding.
 * @tparam SymbolTypes The symbol types associated with the binding.
 */
template <typename T, typename... SymbolTypes>
class BindingScope {
public:
    /**
     * @brief Sets the binding to transient scope.
     */
    void inTransientScope() { lifecycle_ = Lifecycle::Transient; }

    /**
     * @brief Sets the binding to singleton scope.
     */
    void inSingletonScope() {
        lifecycle_ = Lifecycle::Singleton;
        resolver_ =
            std::make_shared<CachedResolver<T, SymbolTypes...>>(resolver_);
    }

    /**
     * @brief Sets the binding to request scope.
     */
    void inRequestScope() { lifecycle_ = Lifecycle::Request; }

protected:
    ResolverPtr<T, SymbolTypes...>
        resolver_;  ///< The resolver for the binding.
    Lifecycle lifecycle_ =
        Lifecycle::Transient;  ///< The lifecycle of the binding.
};

/**
 * @class BindingTo
 * @brief A class template for binding to specific values or factories.
 * @tparam T The type of the binding.
 * @tparam SymbolTypes The symbol types associated with the binding.
 */
template <typename T, typename... SymbolTypes>
class BindingTo : public BindingScope<T, SymbolTypes...> {
public:
    /**
     * @brief Binds to a constant value.
     * @param value The constant value to bind.
     */
    void toConstantValue(T&& value) {
        this->resolver_ = std::make_shared<ConstantResolver<T, SymbolTypes...>>(
            std::forward<T>(value));
    }

    /**
     * @brief Binds to a dynamic value generated by a factory.
     * @param factory The factory to generate the dynamic value.
     * @return A reference to the BindingScope.
     */
    BindingScope<T, SymbolTypes...>& toDynamicValue(
        Factory<T, SymbolTypes...>&& factory) {
        this->resolver_ = std::make_shared<DynamicResolver<T, SymbolTypes...>>(
            std::move(factory));
        return *this;
    }

    /**
     * @brief Binds to another type.
     * @tparam U The type to bind to.
     * @return A reference to the BindingScope.
     */
    template <typename U>
    BindingScope<T, SymbolTypes...>& to() {
        this->resolver_ =
            std::make_shared<AutoResolver<T, U, SymbolTypes...>>();
        return *this;
    }
};

/**
 * @class Binding
 * @brief A class template for managing bindings and resolving values.
 * @tparam T The type of the binding.
 * @tparam SymbolTypes The symbol types associated with the binding.
 */
template <typename T, typename... SymbolTypes>
class Binding : public BindingTo<typename T::value, SymbolTypes...> {
public:
    /**
     * @brief Resolves the value of the binding.
     * @param context The context for resolving the value.
     * @return The resolved value.
     * @throws exceptions::ResolutionException if the resolver is not found.
     */
    typename T::value resolve(const Context<SymbolTypes...>& context) {
        if (!this->resolver_) {
            throw exceptions::ResolutionException(
                "atom::extra::Resolver not found. Malformed binding.");
        }
        return this->resolver_->resolve(context);
    }

    /**
     * @brief Adds a tag to the binding.
     * @param tag The tag to add.
     */
    void when(const Tag& tag) { tags_.push_back(tag); }

    /**
     * @brief Sets the target name for the binding.
     * @param name The target name.
     */
    void whenTargetNamed(const std::string& name) { targetName_ = name; }

    /**
     * @brief Checks if the binding matches a given tag.
     * @param tag The tag to check.
     * @return True if the binding matches the tag, false otherwise.
     */
    bool matchesTag(const Tag& tag) const {
        return std::find_if(tags_.begin(), tags_.end(), [&](const Tag& t) {
                   return t.name == tag.name;
               }) != tags_.end();
    }

    /**
     * @brief Checks if the binding matches a given target name.
     * @param name The target name to check.
     * @return True if the binding matches the target name, false otherwise.
     */
    bool matchesTargetName(const std::string& name) const {
        return targetName_ == name;
    }

private:
    std::vector<Tag> tags_;   ///< The tags associated with the binding.
    std::string targetName_;  ///< The target name for the binding.
};

}  // namespace atom::extra