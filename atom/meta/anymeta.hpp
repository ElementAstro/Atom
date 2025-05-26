/*!
 * \file anymeta.hpp
 * \brief Enhanced Type Metadata with Dynamic Reflection, Method Overloads, and
 * Event System
 * \author Max Qian <lightapt.com>
 * \date 2023-12-28
 * \copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef ATOM_META_ANYMETA_HPP
#define ATOM_META_ANYMETA_HPP

#include "any.hpp"
#include "type_info.hpp"

#include <functional>
#include <iostream>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/macro.hpp"

namespace atom::meta {

/**
 * \brief Type metadata container with support for methods, properties,
 * constructors, and events
 */
class TypeMetadata {
public:
    using MethodFunction = std::function<BoxedValue(std::vector<BoxedValue>)>;
    using GetterFunction = std::function<BoxedValue(const BoxedValue&)>;
    using SetterFunction = std::function<void(BoxedValue&, const BoxedValue&)>;
    using ConstructorFunction =
        std::function<BoxedValue(std::vector<BoxedValue>)>;
    using EventCallback =
        std::function<void(BoxedValue&, const std::vector<BoxedValue>&)>;

    /**
     * \brief Property metadata structure
     */
    struct ATOM_ALIGNAS(64) Property {
        GetterFunction getter;
        SetterFunction setter;
        BoxedValue default_value;
        std::string description;
    };

    /**
     * \brief Event metadata structure with prioritized listeners
     */
    struct ATOM_ALIGNAS(32) Event {
        std::vector<std::pair<int, EventCallback>> listeners;
        std::string description;
    };

private:
    std::unordered_map<std::string, std::vector<MethodFunction>> m_methods_;
    std::unordered_map<std::string, Property> m_properties_;
    std::unordered_map<std::string, std::vector<ConstructorFunction>>
        m_constructors_;
    std::unordered_map<std::string, Event> m_events_;

public:
    /**
     * \brief Add method to type metadata (supports overloads)
     * \param name Method name
     * \param method Method function
     */
    void addMethod(const std::string& name, MethodFunction method) {
        m_methods_[name].emplace_back(std::move(method));
    }

    /**
     * \brief Remove method by name
     * \param name Method name
     */
    void removeMethod(const std::string& name) { m_methods_.erase(name); }

    /**
     * \brief Add property to type metadata
     * \param name Property name
     * \param getter Property getter function
     * \param setter Property setter function
     * \param default_value Default property value
     * \param description Property description
     */
    void addProperty(const std::string& name, GetterFunction getter,
                     SetterFunction setter, BoxedValue default_value = {},
                     const std::string& description = "") {
        m_properties_.emplace(name,
                              Property{std::move(getter), std::move(setter),
                                       std::move(default_value), description});
    }

    /**
     * \brief Remove property by name
     * \param name Property name
     */
    void removeProperty(const std::string& name) { m_properties_.erase(name); }

    /**
     * \brief Add constructor to type metadata
     * \param type_name Type name
     * \param constructor Constructor function
     */
    void addConstructor(const std::string& type_name,
                        ConstructorFunction constructor) {
        m_constructors_[type_name].emplace_back(std::move(constructor));
    }

    /**
     * \brief Add event to type metadata
     * \param event_name Event name
     * \param description Event description
     */
    void addEvent(const std::string& event_name,
                  const std::string& description = "") {
        m_events_[event_name].description = description;
    }

    /**
     * \brief Remove event by name
     * \param event_name Event name
     */
    void removeEvent(const std::string& event_name) {
        m_events_.erase(event_name);
    }

    /**
     * \brief Add event listener with priority
     * \param event_name Event name
     * \param callback Event callback function
     * \param priority Listener priority (higher values execute first)
     */
    void addEventListener(const std::string& event_name, EventCallback callback,
                          int priority = 0) {
        auto& listeners = m_events_[event_name].listeners;
        listeners.emplace_back(priority, std::move(callback));

        std::sort(
            listeners.begin(), listeners.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });
    }

    /**
     * \brief Fire event and notify all listeners
     * \param obj Target object
     * \param event_name Event name
     * \param args Event arguments
     */
    void fireEvent(BoxedValue& obj, const std::string& event_name,
                   const std::vector<BoxedValue>& args) const {
        if (auto it = m_events_.find(event_name); it != m_events_.end()) {
            for (const auto& [priority, listener] : it->second.listeners) {
                listener(obj, args);
            }
        }
    }

    /**
     * \brief Get all overloaded methods by name
     * \param name Method name
     * \return Pointer to method vector or nullptr if not found
     */
    [[nodiscard]] auto getMethods(const std::string& name) const noexcept
        -> const std::vector<MethodFunction>* {
        if (auto it = m_methods_.find(name); it != m_methods_.end()) {
            return &it->second;
        }
        return nullptr;
    }

    /**
     * \brief Get property by name
     * \param name Property name
     * \return Property if found, nullopt otherwise
     */
    [[nodiscard]] auto getProperty(const std::string& name) const noexcept
        -> std::optional<Property> {
        if (auto it = m_properties_.find(name); it != m_properties_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /**
     * \brief Get constructor by type name and index
     * \param type_name Type name
     * \param index Constructor index (default: 0)
     * \return Constructor function if found, nullopt otherwise
     */
    [[nodiscard]] auto getConstructor(const std::string& type_name,
                                      size_t index = 0) const noexcept
        -> std::optional<ConstructorFunction> {
        if (auto it = m_constructors_.find(type_name);
            it != m_constructors_.end() && index < it->second.size()) {
            return it->second[index];
        }
        return std::nullopt;
    }

    /**
     * \brief Get event by name
     * \param name Event name
     * \return Pointer to event or nullptr if not found
     */
    [[nodiscard]] auto getEvent(const std::string& name) const noexcept
        -> const Event* {
        if (auto it = m_events_.find(name); it != m_events_.end()) {
            return &it->second;
        }
        return nullptr;
    }
};

/**
 * \brief Thread-safe singleton registry for type metadata
 */
class TypeRegistry {
private:
    std::unordered_map<std::string, TypeMetadata> m_registry_;
    mutable std::shared_mutex m_mutex_;

public:
    /**
     * \brief Get singleton instance
     * \return Reference to the global type registry
     */
    static auto instance() -> TypeRegistry& {
        static TypeRegistry registry;
        return registry;
    }

    /**
     * \brief Register a type with its metadata
     * \param name Type name
     * \param metadata Type metadata
     */
    void registerType(const std::string& name, TypeMetadata metadata) {
        std::unique_lock lock(m_mutex_);
        m_registry_.emplace(name, std::move(metadata));
    }

    /**
     * \brief Get metadata for a registered type
     * \param name Type name
     * \return Type metadata if found, nullopt otherwise
     */
    [[nodiscard]] auto getMetadata(const std::string& name) const noexcept
        -> std::optional<TypeMetadata> {
        std::shared_lock lock(m_mutex_);
        if (auto it = m_registry_.find(name); it != m_registry_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

/**
 * \brief Call method on BoxedValue object dynamically
 * \param obj Target object
 * \param method_name Method name
 * \param args Method arguments
 * \return Method result
 * \throws atom::error::NotFound if method not found
 */
inline auto callMethod(BoxedValue& obj, const std::string& method_name,
                       std::vector<BoxedValue> args) -> BoxedValue {
    if (auto metadata =
            TypeRegistry::instance().getMetadata(obj.getTypeInfo().name())) {
        if (auto methods = metadata->getMethods(method_name);
            methods && !methods->empty()) {
            return methods->front()(std::move(args));
        }
    }
    THROW_NOT_FOUND("Method not found: " + method_name);
}

/**
 * \brief Get property value from BoxedValue object
 * \param obj Target object
 * \param property_name Property name
 * \return Property value
 * \throws atom::error::NotFound if property not found
 */
inline auto getProperty(const BoxedValue& obj, const std::string& property_name)
    -> BoxedValue {
    if (auto metadata =
            TypeRegistry::instance().getMetadata(obj.getTypeInfo().name())) {
        if (auto property = metadata->getProperty(property_name)) {
            return property->getter(obj);
        }
    }
    THROW_NOT_FOUND("Property not found: " + property_name);
}

/**
 * \brief Set property value on BoxedValue object
 * \param obj Target object
 * \param property_name Property name
 * \param value New property value
 * \throws atom::error::NotFound if property not found
 */
inline void setProperty(BoxedValue& obj, const std::string& property_name,
                        const BoxedValue& value) {
    if (auto metadata =
            TypeRegistry::instance().getMetadata(obj.getTypeInfo().name())) {
        if (auto property = metadata->getProperty(property_name)) {
            property->setter(obj, value);
            return;
        }
    }
    THROW_NOT_FOUND("Property not found: " + property_name);
}

/**
 * \brief Fire event on BoxedValue object
 * \param obj Target object
 * \param event_name Event name
 * \param args Event arguments
 */
inline void fireEvent(BoxedValue& obj, const std::string& event_name,
                      const std::vector<BoxedValue>& args) {
    if (auto metadata =
            TypeRegistry::instance().getMetadata(obj.getTypeInfo().name())) {
        metadata->fireEvent(obj, event_name, args);
    }
}

/**
 * \brief Create instance of registered type dynamically
 * \param type_name Type name
 * \param args Constructor arguments
 * \return Created instance
 * \throws atom::error::NotFound if constructor not found
 */
inline auto createInstance(const std::string& type_name,
                           std::vector<BoxedValue> args) -> BoxedValue {
    if (auto metadata = TypeRegistry::instance().getMetadata(type_name)) {
        if (auto constructor = metadata->getConstructor(type_name)) {
            return (*constructor)(std::move(args));
        }
    }
    THROW_NOT_FOUND("Constructor not found for type: " + type_name);
}

/**
 * \brief Template class for registering types with metadata
 * \tparam T Type to register
 */
template <typename T>
class TypeRegistrar {
public:
    /**
     * \brief Register type with default metadata
     * \param type_name Type name for registration
     */
    static void registerType(const std::string& type_name) {
        TypeMetadata metadata;

        metadata.addConstructor(
            type_name, [](std::vector<BoxedValue> args) -> BoxedValue {
                return args.empty() ? BoxedValue(T{}) : BoxedValue{};
            });

        metadata.addEvent("onCreate", "Triggered when an object is created");
        metadata.addEvent("onDestroy", "Triggered when an object is destroyed");

        metadata.addMethod(
            "print", [](std::vector<BoxedValue> args) -> BoxedValue {
                if (!args.empty()) {
                    std::cout << "Method print called with value: "
                              << args[0].debugString() << std::endl;
                }
                return BoxedValue{};
            });

        TypeRegistry::instance().registerType(type_name, std::move(metadata));
    }
};

}  // namespace atom::meta

#endif  // ATOM_META_ANYMETA_HPP
