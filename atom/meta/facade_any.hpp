/*!
 * \file facade_any.hpp
 * \brief Defines EnhancedBoxedValue, an enhanced version of BoxedValue
 * utilizing the facade pattern
 * \author Max Qian <lightapt.com>
 * \date 2025-04-21
 * \copyright Copyright (C) 2023-2025 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_FACADE_ANY_HPP
#define ATOM_META_FACADE_ANY_HPP

#include <any>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include "atom/meta/any.hpp"
#include "atom/meta/facade.hpp"

namespace atom::meta {

namespace enhanced_any_skills {

/**
 * @brief Printable skill: Enables objects to be printed to an output stream
 */
struct printable_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = printable_dispatch;
    using print_func_t = void (*)(const void*, std::ostream&);

    template <class T>
    static void print_impl(const void* obj, std::ostream& os) {
        const T& concrete_obj = *static_cast<const T*>(obj);
        if constexpr (requires { os << concrete_obj; }) {
            os << concrete_obj;
        } else if constexpr (requires { concrete_obj.toString(); }) {
            os << concrete_obj.toString();
        } else if constexpr (requires { concrete_obj.to_string(); }) {
            os << concrete_obj.to_string();
        } else {
            os << "[unprintable " << typeid(T).name() << "]";
        }
    }
};

/**
 * @brief String conversion skill: Enables objects to be converted to
 * std::string
 */
struct stringable_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = stringable_dispatch;
    using to_string_func_t = std::string (*)(const void*);

    template <class T>
    static std::string to_string_impl(const void* obj) {
        const T& concrete_obj = *static_cast<const T*>(obj);
        if constexpr (requires { std::to_string(concrete_obj); }) {
            return std::to_string(concrete_obj);
        } else if constexpr (requires { std::string(concrete_obj); }) {
            return std::string(concrete_obj);
        } else if constexpr (requires { concrete_obj.toString(); }) {
            return concrete_obj.toString();
        } else if constexpr (requires { concrete_obj.to_string(); }) {
            return concrete_obj.to_string();
        } else {
            return "[no string conversion for type: " +
                   std::string(typeid(T).name()) + "]";
        }
    }
};

/**
 * @brief Comparison skill: Enables objects to be compared for equality and
 * ordering
 */
struct comparable_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = comparable_dispatch;
    using equals_func_t = bool (*)(const void*, const void*,
                                   const std::type_info&);
    using less_than_func_t = bool (*)(const void*, const void*,
                                      const std::type_info&);

    template <class T>
    static bool equals_impl(const void* obj1, const void* obj2,
                            const std::type_info& type2_info) {
        if (typeid(T) != type2_info) {
            return false;
        }

        const T& concrete_obj1 = *static_cast<const T*>(obj1);
        const T& concrete_obj2 = *static_cast<const T*>(obj2);

        if constexpr (requires { concrete_obj1 == concrete_obj2; }) {
            return concrete_obj1 == concrete_obj2;
        } else {
            return false;
        }
    }

    template <class T>
    static bool less_than_impl(const void* obj1, const void* obj2,
                               const std::type_info& type2_info) {
        if (typeid(T) != type2_info) {
            return typeid(T).before(type2_info);
        }

        const T& concrete_obj1 = *static_cast<const T*>(obj1);
        const T& concrete_obj2 = *static_cast<const T*>(obj2);

        if constexpr (requires { concrete_obj1 < concrete_obj2; }) {
            return concrete_obj1 < concrete_obj2;
        } else {
            return false;
        }
    }
};

/**
 * @brief Serialization skill: Enables objects to be serialized to/from strings
 */
struct serializable_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = serializable_dispatch;
    using serialize_func_t = std::string (*)(const void*);
    using deserialize_func_t = bool (*)(void*, const std::string&);

    template <class T>
    static std::string serialize_impl(const void* obj) {
        const T& concrete_obj = *static_cast<const T*>(obj);
        if constexpr (requires {
                          {
                              concrete_obj.serialize()
                          } -> std::convertible_to<std::string>;
                      }) {
            return concrete_obj.serialize();
        } else if constexpr (requires {
                                 {
                                     concrete_obj.toJson()
                                 } -> std::convertible_to<std::string>;
                             }) {
            return concrete_obj.toJson();
        } else if constexpr (requires {
                                 {
                                     concrete_obj.to_json()
                                 } -> std::convertible_to<std::string>;
                             }) {
            return concrete_obj.to_json();
        } else {
            if constexpr (std::is_same_v<T, std::string>) {
                return "\"" + concrete_obj + "\"";
            } else if constexpr (std::is_arithmetic_v<T>) {
                return stringable_dispatch::to_string_impl<T>(obj);
            } else if constexpr (std::is_same_v<T, bool>) {
                return concrete_obj ? "true" : "false";
            } else {
                return "null";
            }
        }
    }

    template <class T>
    static bool deserialize_impl(void* obj, const std::string& data) {
        T& concrete_obj = *static_cast<T*>(obj);
        if constexpr (requires {
                          {
                              concrete_obj.deserialize(data)
                          } -> std::convertible_to<bool>;
                      }) {
            return concrete_obj.deserialize(data);
        } else if constexpr (requires {
                                 {
                                     concrete_obj.fromJson(data)
                                 } -> std::convertible_to<bool>;
                             }) {
            return concrete_obj.fromJson(data);
        } else if constexpr (requires {
                                 {
                                     concrete_obj.from_json(data)
                                 } -> std::convertible_to<bool>;
                             }) {
            return concrete_obj.from_json(data);
        } else {
            return false;
        }
    }
};

/**
 * @brief Cloneable skill: Enables objects to be cloned (deep copied)
 */
struct cloneable_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = cloneable_dispatch;
    using clone_func_t =
        std::unique_ptr<void, void (*)(void*)> (*)(const void*);

    template <class T>
    static std::unique_ptr<void, void (*)(void*)> clone_impl(const void* obj) {
        const T& concrete_obj = *static_cast<const T*>(obj);
        if constexpr (requires { concrete_obj.clone(); }) {
            auto cloned = concrete_obj.clone();
            auto* ptr = new T(std::move(cloned));
            return std::unique_ptr<void, void (*)(void*)>(
                ptr, [](void* p) { delete static_cast<T*>(p); });
        } else if constexpr (std::is_copy_constructible_v<T>) {
            auto* ptr = new T(concrete_obj);
            return std::unique_ptr<void, void (*)(void*)>(
                ptr, [](void* p) { delete static_cast<T*>(p); });
        } else {
            return std::unique_ptr<void, void (*)(void*)>(nullptr,
                                                          [](void*) {});
        }
    }
};

/**
 * @brief JSON conversion skill: Enables objects to be converted to/from JSON
 * strings
 */
struct json_convertible_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = json_convertible_dispatch;
    using to_json_func_t = std::string (*)(const void*);
    using from_json_func_t = bool (*)(void*, const std::string&);

    template <class T>
    static std::string to_json_impl(const void* obj) {
        const T& concrete_obj = *static_cast<const T*>(obj);
        if constexpr (requires {
                          {
                              concrete_obj.toJson()
                          } -> std::convertible_to<std::string>;
                      }) {
            return concrete_obj.toJson();
        } else if constexpr (requires {
                                 {
                                     concrete_obj.to_json()
                                 } -> std::convertible_to<std::string>;
                             }) {
            return concrete_obj.to_json();
        } else {
            return serializable_dispatch::serialize_impl<T>(obj);
        }
    }

    template <class T>
    static bool from_json_impl(void* obj, const std::string& json) {
        T& concrete_obj = *static_cast<T*>(obj);
        if constexpr (requires {
                          {
                              concrete_obj.fromJson(json)
                          } -> std::convertible_to<bool>;
                      }) {
            return concrete_obj.fromJson(json);
        } else if constexpr (requires {
                                 {
                                     concrete_obj.from_json(json)
                                 } -> std::convertible_to<bool>;
                             }) {
            return concrete_obj.from_json(json);
        } else {
            return serializable_dispatch::deserialize_impl<T>(obj, json);
        }
    }
};

/**
 * @brief Callable skill: Enables objects (like lambdas or function objects) to
 * be called
 */
struct callable_dispatch {
    static constexpr bool is_direct = false;
    using dispatch_type = callable_dispatch;
    using call_func_t = std::any (*)(const void*, const std::vector<std::any>&);

    template <class T>
    static std::any call_impl(const void* obj,
                              const std::vector<std::any>& args) {
        const T& concrete_obj = *static_cast<const T*>(obj);

        if constexpr (requires(const T& t) { t(); }) {
            if (args.empty()) {
                if constexpr (std::is_void_v<decltype(concrete_obj())>) {
                    concrete_obj();
                    return {};
                } else {
                    return concrete_obj();
                }
            }
        }

        if constexpr (requires(const T& t, const std::any& a) { t(a); }) {
            if (args.size() == 1) {
                if constexpr (std::is_void_v<decltype(concrete_obj(
                                  std::declval<std::any>()))>) {
                    concrete_obj(args[0]);
                    return {};
                } else {
                    return concrete_obj(args[0]);
                }
            }
        }

        return {};
    }
};

}  // namespace enhanced_any_skills

using enhanced_boxed_value_facade = default_builder::add_convention<
    enhanced_any_skills::printable_dispatch,
    void(std::ostream&)
        const>::add_convention<enhanced_any_skills::stringable_dispatch,
                               std::string() const>::
    add_convention<enhanced_any_skills::comparable_dispatch,
                   bool(const proxy<typename default_builder::build>&) const,
                   bool(const proxy<typename default_builder::build>&) const>::
        add_convention<enhanced_any_skills::serializable_dispatch,
                       std::string() const, bool(const std::string&)>::
            add_convention<enhanced_any_skills::cloneable_dispatch,
                           std::unique_ptr<void, void (*)(void*)>() const>::
                add_convention<enhanced_any_skills::json_convertible_dispatch,
                               std::string() const, bool(const std::string&)>::
                    add_convention<enhanced_any_skills::callable_dispatch,
                                   std::any(const std::vector<std::any>&)>::
                        restrict_layout<256>::support_copy<
                            constraint_level::nothrow>::
                            support_relocation<constraint_level::nothrow>::
                                support_destruction<
                                    constraint_level::nothrow>::build;

struct ProxyVisitor {
    bool success = false;
    proxy<enhanced_boxed_value_facade> result;

    template <typename T>
    bool operator()(T& value) {
        if constexpr (std::is_copy_constructible_v<T> &&
                      !std::is_pointer_v<T>) {
            try {
                result = proxy<enhanced_boxed_value_facade>(value);
                success = true;
                return true;
            } catch (const std::exception&) {
                return false;
            }
        }
        return false;
    }

    bool fallback() { return false; }
};

/**
 * \class EnhancedBoxedValue
 * \brief An enhanced version of BoxedValue that uses the facade pattern to
 * provide powerful type erasure and dynamic dispatch capabilities
 */
class EnhancedBoxedValue {
private:
    BoxedValue boxed_value_;
    proxy<enhanced_boxed_value_facade> proxy_;
    bool has_proxy_ = false;

public:
    /**
     * \brief Default constructor: Creates an empty EnhancedBoxedValue
     */
    EnhancedBoxedValue() : boxed_value_() {}

    /**
     * \brief Construct from an existing BoxedValue
     * \param value The BoxedValue to wrap
     */
    explicit EnhancedBoxedValue(const BoxedValue& value) : boxed_value_(value) {
        initProxy();
    }

    /**
     * \brief Construct from any type T
     * \tparam T The type of the value
     * \param value The value to store
     */
    template <typename T>
        requires(!std::same_as<EnhancedBoxedValue, std::decay_t<T>> &&
                 !std::same_as<BoxedValue, std::decay_t<T>>)
    explicit EnhancedBoxedValue(T&& value)
        : boxed_value_(std::forward<T>(value)) {
        initProxy();
    }

    /**
     * \brief Construct from any type T with an associated description
     * \tparam T The type of the value
     * \param value The value to store
     * \param description Description of the value
     */
    template <typename T>
        requires(!std::same_as<EnhancedBoxedValue, std::decay_t<T>> &&
                 !std::same_as<BoxedValue, std::decay_t<T>>)
    EnhancedBoxedValue(T&& value, std::string_view description)
        : boxed_value_(varWithDesc(std::forward<T>(value), description)) {
        initProxy();
    }

    /**
     * \brief Copy constructor
     */
    EnhancedBoxedValue(const EnhancedBoxedValue& other)
        : boxed_value_(other.boxed_value_), has_proxy_(other.has_proxy_) {
        if (has_proxy_) {
            proxy_ = other.proxy_;
        }
    }

    /**
     * \brief Move constructor
     */
    EnhancedBoxedValue(EnhancedBoxedValue&& other) noexcept
        : boxed_value_(std::move(other.boxed_value_)),
          proxy_(std::move(other.proxy_)),
          has_proxy_(other.has_proxy_) {
        other.has_proxy_ = false;
    }

    /**
     * \brief Copy assignment operator
     */
    EnhancedBoxedValue& operator=(const EnhancedBoxedValue& other) {
        if (this != &other) {
            boxed_value_ = other.boxed_value_;
            has_proxy_ = other.has_proxy_;
            if (has_proxy_) {
                proxy_ = other.proxy_;
            } else {
                proxy_.reset();
            }
        }
        return *this;
    }

    /**
     * \brief Move assignment operator
     */
    EnhancedBoxedValue& operator=(EnhancedBoxedValue&& other) noexcept {
        if (this != &other) {
            boxed_value_ = std::move(other.boxed_value_);
            proxy_ = std::move(other.proxy_);
            has_proxy_ = other.has_proxy_;
            other.has_proxy_ = false;
        }
        return *this;
    }

    /**
     * \brief Assign from any type T
     * \tparam T The type of the value
     * \param value The value to assign
     */
    template <typename T>
        requires(!std::same_as<EnhancedBoxedValue, std::decay_t<T>> &&
                 !std::same_as<BoxedValue, std::decay_t<T>>)
    EnhancedBoxedValue& operator=(T&& value) {
        boxed_value_ = std::forward<T>(value);
        initProxy();
        return *this;
    }

    /**
     * \brief Get the internal BoxedValue (const access)
     * \return Reference to the internal BoxedValue
     */
    [[nodiscard]] const BoxedValue& getBoxedValue() const {
        return boxed_value_;
    }

    /**
     * \brief Get the internal proxy object (const access)
     * \return Reference to the proxy object
     * \throws std::runtime_error if no valid proxy exists
     */
    [[nodiscard]] const proxy<enhanced_boxed_value_facade>& getProxy() const {
        if (!has_proxy_) {
            throw std::runtime_error(
                "No proxy available for the contained value.");
        }
        return proxy_;
    }

    /**
     * \brief Check if the EnhancedBoxedValue holds a valid, non-null,
     * non-undefined value
     * \return true if value is valid, false otherwise
     */
    [[nodiscard]] bool hasValue() const {
        return !boxed_value_.isUndef() && !boxed_value_.isNull();
    }

    /**
     * \brief Check if a valid proxy object was successfully created for the
     * contained value
     * \return true if proxy exists, false otherwise
     */
    [[nodiscard]] bool hasProxy() const { return has_proxy_; }

    /**
     * \brief Convert the contained value to a string using the stringable skill
     * \return String representation of the value
     */
    [[nodiscard]] std::string toString() const {
        if (has_proxy_) {
            try {
                return proxy_.call<enhanced_any_skills::stringable_dispatch,
                                   std::string>();
            } catch (const std::exception& e) {
                return boxed_value_.debugString() +
                       " (proxy call failed: " + e.what() + ")";
            }
        } else {
            return boxed_value_.debugString();
        }
    }

    /**
     * \brief Convert the contained value to a JSON string using the JSON
     * conversion skill
     * \return JSON string representation of the value
     */
    [[nodiscard]] std::string toJson() const {
        if (has_proxy_) {
            try {
                return proxy_
                    .call<enhanced_any_skills::json_convertible_dispatch,
                          std::string>();
            } catch (const std::exception& e) {
                return toString() + " (proxy call failed: " + e.what() + ")";
            }
        } else {
            return toString();
        }
    }

    /**
     * \brief Load the state of the contained value from a JSON string
     * \param json The JSON string to deserialize
     * \return true on success, false otherwise
     */
    bool fromJson(const std::string& json) {
        if (has_proxy_) {
            try {
                return proxy_
                    .call<enhanced_any_skills::json_convertible_dispatch, bool>(
                        json);
            } catch (const std::exception&) {
                return false;
            }
        }
        return false;
    }

    /**
     * \brief Print the contained value to an output stream using the printable
     * skill
     * \param os The output stream to print to
     */
    void print(std::ostream& os = std::cout) const {
        if (has_proxy_) {
            try {
                proxy_.call<enhanced_any_skills::printable_dispatch>(os);
                return;
            } catch (const std::exception& e) {
                os << boxed_value_.debugString()
                   << " (proxy call failed: " << e.what() << ")";
                return;
            }
        }
        os << boxed_value_.debugString();
    }

    /**
     * \brief Compare this EnhancedBoxedValue with another for equality
     * \param other The other EnhancedBoxedValue to compare with
     * \return true if equal, false otherwise
     */
    bool equals(const EnhancedBoxedValue& other) const {
        if (has_proxy_ && other.has_proxy_) {
            try {
                return proxy_
                    .call<enhanced_any_skills::comparable_dispatch, bool>(
                        other.proxy_);
            } catch (const std::exception&) {
                return boxed_value_.getTypeInfo() ==
                       other.boxed_value_.getTypeInfo();
            }
        } else {
            return boxed_value_.getTypeInfo() ==
                   other.boxed_value_.getTypeInfo();
        }
    }

    /**
     * \brief Attempt to call the contained value if it's a function object
     * \param args Arguments for the function call
     * \return Result of the call as std::any, or empty std::any on failure
     */
    std::any call(const std::vector<std::any>& args = {}) {
        if (has_proxy_) {
            try {
                return proxy_
                    .call<enhanced_any_skills::callable_dispatch, std::any>(
                        args);
            } catch (const std::exception&) {
                return {};
            }
        }
        return {};
    }

    /**
     * \brief Clone the EnhancedBoxedValue using the cloneable skill
     * \return A new EnhancedBoxedValue containing the cloned object
     */
    EnhancedBoxedValue clone() const {
        if (has_proxy_) {
            try {
                auto cloned_ptr =
                    proxy_.call<enhanced_any_skills::cloneable_dispatch,
                                std::unique_ptr<void, void (*)(void*)>>();
                if (cloned_ptr) {
                    return EnhancedBoxedValue(*this);
                }
            } catch (const std::exception&) {
            }
        }
        return EnhancedBoxedValue(*this);
    }

    /**
     * \brief Get the TypeInfo of the contained value
     * \return Reference to the TypeInfo
     */
    [[nodiscard]] const TypeInfo& getTypeInfo() const {
        return boxed_value_.getTypeInfo();
    }

    /**
     * \brief Check if the contained value is of a specific type T
     * \tparam T The type to check against
     * \return true if the value is of type T, false otherwise
     */
    template <typename T>
    [[nodiscard]] bool isType() const {
        return boxed_value_.isType<T>();
    }

    /**
     * \brief Attempt to cast the contained value to a specific type T
     * \tparam T The target type
     * \return std::optional containing the casted value if successful
     */
    template <typename T>
    [[nodiscard]] std::optional<T> tryCast() const {
        return boxed_value_.tryCast<T>();
    }

    /**
     * \brief Set an attribute associated with this value
     * \param name The attribute name
     * \param value The attribute value
     * \return Reference to this object for chaining
     */
    EnhancedBoxedValue& setAttr(const std::string& name,
                                const EnhancedBoxedValue& value) {
        boxed_value_.setAttr(name, value.boxed_value_);
        return *this;
    }

    /**
     * \brief Get an attribute by name
     * \param name The attribute name
     * \return EnhancedBoxedValue wrapping the attribute value
     */
    [[nodiscard]] EnhancedBoxedValue getAttr(const std::string& name) const {
        BoxedValue attr = boxed_value_.getAttr(name);
        return EnhancedBoxedValue(attr);
    }

    /**
     * \brief List the names of all attributes associated with this value
     * \return Vector of attribute names
     */
    [[nodiscard]] std::vector<std::string> listAttrs() const {
        return boxed_value_.listAttrs();
    }

    /**
     * \brief Check if an attribute with the given name exists
     * \param name The attribute name
     * \return true if attribute exists, false otherwise
     */
    [[nodiscard]] bool hasAttr(const std::string& name) const {
        return boxed_value_.hasAttr(name);
    }

    /**
     * \brief Remove an attribute by name
     * \param name The attribute name
     */
    void removeAttr(const std::string& name) { boxed_value_.removeAttr(name); }

    /**
     * \brief Reset the EnhancedBoxedValue to an empty/undefined state
     */
    void reset() {
        boxed_value_ = BoxedValue();
        if (has_proxy_) {
            proxy_.reset();
            has_proxy_ = false;
        }
    }

    /**
     * \brief Equality operator
     */
    friend bool operator==(const EnhancedBoxedValue& lhs,
                           const EnhancedBoxedValue& rhs) {
        return lhs.equals(rhs);
    }

    /**
     * \brief Output stream operator
     */
    friend std::ostream& operator<<(std::ostream& os,
                                    const EnhancedBoxedValue& value) {
        value.print(os);
        return os;
    }

private:
    void initProxy() {
        if (boxed_value_.isUndef() || boxed_value_.isNull() ||
            boxed_value_.isVoid()) {
            has_proxy_ = false;
            proxy_.reset();
            return;
        }

        try {
            ProxyVisitor visitor;
            boxed_value_.visit(visitor);
            has_proxy_ = visitor.success;
            if (has_proxy_) {
                proxy_ = std::move(visitor.result);
            } else {
                proxy_.reset();
            }
        } catch (const std::exception&) {
            has_proxy_ = false;
            proxy_.reset();
        }
    }
};

/**
 * \brief Convenience function to create an EnhancedBoxedValue from a value
 * \tparam T The type of the value
 * \param value The value to wrap
 * \return EnhancedBoxedValue containing the value
 */
template <typename T>
auto enhancedVar(T&& value) -> EnhancedBoxedValue {
    return EnhancedBoxedValue(std::forward<T>(value));
}

/**
 * \brief Convenience function to create an EnhancedBoxedValue from a value with
 * a description
 * \tparam T The type of the value
 * \param value The value to wrap
 * \param description Description of the value
 * \return EnhancedBoxedValue containing the value and description
 */
template <typename T>
auto enhancedVarWithDesc(T&& value, std::string_view description)
    -> EnhancedBoxedValue {
    return EnhancedBoxedValue(std::forward<T>(value), description);
}

}  // namespace atom::meta

#endif  // ATOM_META_FACADE_ANY_HPP