/*!
 * \file anymeta.hpp
 * \brief Enhanced Type Metadata with Dynamic Reflection, Method Overloads, and
 * Event System - OPTIMIZED VERSION \author Max Qian <lightapt.com> \date
 * 2023-12-28 \optimized 2025-01-22 - Performance optimizations by AI Assistant
 * \copyright Copyright (C) 2023-2024 Max Qian
 *
 * OPTIMIZATIONS APPLIED:
 * - Enhanced metadata storage with better cache performance
 * - Optimized method lookup with fast-path optimizations
 * - Improved event system with reduced overhead
 * - Better memory layout for frequently accessed data
 * - Added caching for expensive operations
 */

#ifndef ATOM_META_ANYMETA_HPP
#define ATOM_META_ANYMETA_HPP

#include "any.hpp"
#include "type_info.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/macro.hpp"

namespace atom::meta {

/**
 * \brief Optimized type metadata container with enhanced performance and
 * caching
 */
class alignas(64) TypeMetadata {  // Cache line alignment for better performance
public:
    using MethodFunction = std::function<BoxedValue(std::vector<BoxedValue>)>;
    using GetterFunction = std::function<BoxedValue(const BoxedValue&)>;
    using SetterFunction = std::function<void(BoxedValue&, const BoxedValue&)>;
    using ConstructorFunction =
        std::function<BoxedValue(std::vector<BoxedValue>)>;
    using EventCallback =
        std::function<void(BoxedValue&, const std::vector<BoxedValue>&)>;

    /**
     * \brief Optimized property metadata structure with better layout
     */
    struct ATOM_ALIGNAS(64) Property {
        GetterFunction getter;
        SetterFunction setter;
        BoxedValue default_value;
        std::string description;

        // Optimized: Additional metadata for performance
        bool is_cached = false;
        mutable std::optional<BoxedValue> cached_value = std::nullopt;
        mutable std::chrono::steady_clock::time_point cache_time =
            std::chrono::steady_clock::now();
        static constexpr std::chrono::milliseconds CACHE_TTL{100};
    };

    /**
     * \brief Event metadata with priority-ordered, unsubscribable listeners
     */
    struct ATOM_ALIGNAS(32) Event {
        struct Listener {
            int priority;
            std::uint64_t id;
            EventCallback callback;
        };

        std::vector<Listener> listeners;
        std::string description;
        std::uint64_t next_listener_id = 1;

        // Event statistics for monitoring
        mutable std::atomic<uint64_t> fire_count{0};

        Event() = default;
        Event(const Event& other)
            : listeners(other.listeners),
              description(other.description),
              next_listener_id(other.next_listener_id),
              fire_count(other.fire_count.load()) {}

        Event& operator=(const Event& other) {
            if (this != &other) {
                listeners = other.listeners;
                description = other.description;
                next_listener_id = other.next_listener_id;
                fire_count.store(other.fire_count.load());
            }
            return *this;
        }
    };

private:
    // Optimized: Group frequently accessed data together
    std::unordered_map<std::string, std::vector<MethodFunction>> m_methods_;
    std::unordered_map<std::string, Property> m_properties_;
    std::unordered_map<std::string, std::vector<ConstructorFunction>>
        m_constructors_;
    std::unordered_map<std::string, Event> m_events_;

    // Guards the per-property value caches
    mutable std::shared_mutex cache_mutex_;

public:
    // Make TypeMetadata copyable and movable
    TypeMetadata() = default;
    TypeMetadata(const TypeMetadata& other)
        : m_methods_(other.m_methods_),
          m_properties_(other.m_properties_),
          m_constructors_(other.m_constructors_),
          m_events_(other.m_events_) {}

    TypeMetadata(TypeMetadata&& other) noexcept
        : m_methods_(std::move(other.m_methods_)),
          m_properties_(std::move(other.m_properties_)),
          m_constructors_(std::move(other.m_constructors_)),
          m_events_(std::move(other.m_events_)) {}

    TypeMetadata& operator=(const TypeMetadata& other) {
        if (this != &other) {
            m_methods_ = other.m_methods_;
            m_properties_ = other.m_properties_;
            m_constructors_ = other.m_constructors_;
            m_events_ = other.m_events_;
        }
        return *this;
    }

    TypeMetadata& operator=(TypeMetadata&& other) noexcept {
        if (this != &other) {
            m_methods_ = std::move(other.m_methods_);
            m_properties_ = std::move(other.m_properties_);
            m_constructors_ = std::move(other.m_constructors_);
            m_events_ = std::move(other.m_events_);
        }
        return *this;
    }
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
                     const std::string& description = "",
                     bool cached = false) {
        m_properties_.emplace(
            name, Property{std::move(getter), std::move(setter),
                           std::move(default_value), description, cached});
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
     * \return Listener id usable with removeEventListener
     */
    std::uint64_t addEventListener(const std::string& event_name,
                                   EventCallback callback, int priority = 0) {
        auto& event = m_events_[event_name];
        const std::uint64_t id = event.next_listener_id++;
        event.listeners.push_back({priority, id, std::move(callback)});

        std::stable_sort(event.listeners.begin(), event.listeners.end(),
                         [](const auto& a, const auto& b) {
                             return a.priority > b.priority;
                         });
        return id;
    }

    /**
     * \brief Remove a previously registered event listener
     * \param event_name Event name
     * \param listener_id Id returned by addEventListener
     * \return True if a listener was removed
     */
    bool removeEventListener(const std::string& event_name,
                             std::uint64_t listener_id) {
        if (auto it = m_events_.find(event_name); it != m_events_.end()) {
            auto& listeners = it->second.listeners;
            auto removed = std::erase_if(listeners, [&](const auto& l) {
                return l.id == listener_id;
            });
            return removed > 0;
        }
        return false;
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
            it->second.fire_count.fetch_add(1, std::memory_order_relaxed);
            for (const auto& listener : it->second.listeners) {
                listener.callback(obj, args);
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
     * \brief Read a property value, honoring the property's cache TTL
     * \param obj Object to read from
     * \param name Property name
     * \return Property value, or nullopt if the property is unknown
     */
    [[nodiscard]] auto getPropertyValue(const BoxedValue& obj,
                                        const std::string& name) const
        -> std::optional<BoxedValue> {
        auto it = m_properties_.find(name);
        if (it == m_properties_.end() || !it->second.getter) {
            return std::nullopt;
        }
        const Property& prop = it->second;

        if (prop.is_cached) {
            const auto now = std::chrono::steady_clock::now();
            {
                std::shared_lock lock(cache_mutex_);
                if (prop.cached_value &&
                    now - prop.cache_time < Property::CACHE_TTL) {
                    return *prop.cached_value;
                }
            }
            auto value = prop.getter(obj);
            std::unique_lock lock(cache_mutex_);
            prop.cached_value = value;
            prop.cache_time = now;
            return value;
        }
        return prop.getter(obj);
    }

    /**
     * \brief Write a property value and invalidate its cache
     * \param obj Object to write to
     * \param name Property name
     * \param value New value
     * \return True if the property exists and has a setter
     */
    bool setPropertyValue(BoxedValue& obj, const std::string& name,
                          const BoxedValue& value) {
        auto it = m_properties_.find(name);
        if (it == m_properties_.end() || !it->second.setter) {
            return false;
        }
        it->second.setter(obj, value);
        if (it->second.is_cached) {
            std::unique_lock lock(cache_mutex_);
            it->second.cached_value.reset();
        }
        return true;
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
 *
 * Metadata is stored behind shared_ptr so lookups share the live object:
 * event statistics, property caches and late method registration all act on
 * the registered metadata rather than on a copy. Mutating metadata after it
 * is in concurrent use is not synchronized; register methods/properties
 * before publishing the type to other threads.
 */
class TypeRegistry {
private:
    std::unordered_map<std::string, std::shared_ptr<TypeMetadata>> m_registry_;
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
        m_registry_[name] =
            std::make_shared<TypeMetadata>(std::move(metadata));
    }

    /**
     * \brief Get metadata for a registered type
     * \param name Type name
     * \return Shared pointer to the live metadata, or nullptr if unknown
     */
    [[nodiscard]] auto getMetadata(const std::string& name) const noexcept
        -> std::shared_ptr<TypeMetadata> {
        std::shared_lock lock(m_mutex_);
        if (auto it = m_registry_.find(name); it != m_registry_.end()) {
            return it->second;
        }
        return nullptr;
    }

    /**
     * \brief Check whether a type is registered
     */
    [[nodiscard]] bool isRegistered(const std::string& name) const noexcept {
        std::shared_lock lock(m_mutex_);
        return m_registry_.contains(name);
    }

    /**
     * \brief Get the names of all registered types
     */
    [[nodiscard]] auto getRegisteredTypes() const -> std::vector<std::string> {
        std::shared_lock lock(m_mutex_);
        std::vector<std::string> names;
        names.reserve(m_registry_.size());
        for (const auto& [name, metadata] : m_registry_) {
            names.push_back(name);
        }
        return names;
    }

    /**
     * \brief Remove all registered types
     */
    void clear() {
        std::unique_lock lock(m_mutex_);
        m_registry_.clear();
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
inline auto getProperty(const BoxedValue& obj,
                        const std::string& property_name) -> BoxedValue {
    if (auto metadata =
            TypeRegistry::instance().getMetadata(obj.getTypeInfo().name())) {
        if (auto value = metadata->getPropertyValue(obj, property_name)) {
            return *value;
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
        if (metadata->setPropertyValue(obj, property_name, value)) {
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

//==============================================================================
// C++23 Enhanced Metadata Utilities
//==============================================================================

/**
 * @brief Concept for types with metadata support
 */
template <typename T>
concept MetadataSupported = requires {
    { TypeInfo::fromType<T>() };
};

/**
 * @brief Fluent metadata builder
 */
class MetadataBuilder {
    TypeMetadata metadata_;
    std::string type_name_;

public:
    explicit MetadataBuilder(std::string_view name) : type_name_(name) {}

    MetadataBuilder& withMethod(std::string_view name,
                                TypeMetadata::MethodFunction func) {
        metadata_.addMethod(std::string(name), std::move(func));
        return *this;
    }

    MetadataBuilder& withProperty(std::string_view name,
                                  TypeMetadata::GetterFunction getter,
                                  TypeMetadata::SetterFunction setter = nullptr,
                                  std::string_view desc = "",
                                  bool cached = false) {
        metadata_.addProperty(std::string(name), std::move(getter),
                              std::move(setter), {}, std::string(desc),
                              cached);
        return *this;
    }

    MetadataBuilder& withEvent(std::string_view name,
                               std::string_view desc = "") {
        metadata_.addEvent(std::string(name), std::string(desc));
        return *this;
    }

    MetadataBuilder& withConstructor(std::string_view name,
                                     TypeMetadata::ConstructorFunction func) {
        metadata_.addConstructor(std::string(name), std::move(func));
        return *this;
    }

    void build() {
        TypeRegistry::instance().registerType(type_name_, std::move(metadata_));
    }

    [[nodiscard]] const TypeMetadata& getMetadata() const { return metadata_; }
};

/**
 * @brief Create a metadata builder
 */
inline auto buildMetadata(std::string_view type_name) -> MetadataBuilder {
    return MetadataBuilder(type_name);
}

/**
 * @brief Enhanced type registrar with automatic method binding
 */
template <MetadataSupported T>
class AutoTypeRegistrar {
public:
    static void registerWithDefaults(std::string_view name) {
        buildMetadata(name)
            .withConstructor(
                std::string(name),
                [](std::vector<BoxedValue>) { return BoxedValue(T{}); })
            .withEvent("onCreate", "Fired when instance is created")
            .withEvent("onDestroy", "Fired when instance is destroyed")
            .build();
    }

    /**
     * @brief Register an additional method on an already-registered type
     *
     * The callback receives the raw BoxedValue argument list; unpack and
     * type-check the arguments inside the callback.
     */
    static bool registerMethod(std::string_view type_name,
                               std::string_view method_name,
                               TypeMetadata::MethodFunction func) {
        if (auto metadata =
                TypeRegistry::instance().getMetadata(std::string(type_name))) {
            metadata->addMethod(std::string(method_name), std::move(func));
            return true;
        }
        return false;
    }
};

/**
 * @brief Query metadata for a type
 * @return Shared pointer to the live metadata, or nullptr if not registered
 */
template <MetadataSupported T>
auto queryMetadata() -> std::shared_ptr<TypeMetadata> {
    auto name = TypeInfo::fromType<T>().name();
    return TypeRegistry::instance().getMetadata(std::string(name));
}

/**
 * @brief Invoke method on object by name with type checking
 */
template <typename Result = BoxedValue>
auto invokeMethod(BoxedValue& obj, std::string_view method_name,
                  std::vector<BoxedValue> args = {}) -> std::optional<Result> {
    auto result = callMethod(obj, std::string(method_name), std::move(args));
    if constexpr (std::is_same_v<Result, BoxedValue>) {
        return result;
    } else {
        if (result.template canCast<Result>()) {
            return result.template cast<Result>();
        }
        return std::nullopt;
    }
}

/**
 * @brief Metadata visitor for introspection
 */
template <typename Visitor>
void visitMetadata(std::string_view type_name, Visitor&& visitor) {
    if (auto metadata =
            TypeRegistry::instance().getMetadata(std::string(type_name))) {
        std::forward<Visitor>(visitor)(*metadata);
    }
}

/**
 * @brief Get all registered type names
 */
inline auto getAllRegisteredTypes() -> std::vector<std::string> {
    return TypeRegistry::instance().getRegisteredTypes();
}

/**
 * @brief Check if a type has a specific method
 */
inline bool hasMethod(std::string_view type_name,
                      std::string_view method_name) {
    if (auto metadata =
            TypeRegistry::instance().getMetadata(std::string(type_name))) {
        return metadata->getMethods(std::string(method_name)) != nullptr;
    }
    return false;
}

/**
 * @brief Check if a type has a specific property
 */
inline bool hasProperty(std::string_view type_name,
                        std::string_view property_name) {
    if (auto metadata =
            TypeRegistry::instance().getMetadata(std::string(type_name))) {
        return metadata->getProperty(std::string(property_name)).has_value();
    }
    return false;
}

#define ATOM_REGISTER_METADATA(Type) \
    atom::meta::AutoTypeRegistrar<Type>::registerWithDefaults(#Type)

}  // namespace atom::meta

#endif  // ATOM_META_ANYMETA_HPP
