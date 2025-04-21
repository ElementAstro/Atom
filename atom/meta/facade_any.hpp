/*!
 * \file enhanced_any.hpp
 * \brief Defines EnhancedBoxedValue, an enhanced version of BoxedValue
 * utilizing the facade pattern.
 * \author Max Qian <lightapt.com>
 * \date 2025-04-21
 * \copyright Copyright (C) 2023-2025 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_ENHANCED_ANY_HPP
#define ATOM_META_ENHANCED_ANY_HPP

#include <any>
#include <iostream>  // For std::ostream
#include <optional>
#include <stdexcept>  // For std::runtime_error
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include "atom/meta/any.hpp"
#include "atom/meta/facade.hpp"

namespace atom::meta {

// Define basic skill interfaces for EnhancedBoxedValue
namespace enhanced_any_skills {

/**
 * @brief Printable skill: Enables objects to be printed to an output stream.
 *
 * Attempts to use `operator<<`, `toString()`, or `to_string()` for printing.
 * Falls back to a default representation if none are available.
 */
struct printable_dispatch {
    static constexpr bool is_direct = false;  // Indirect dispatch via vtable
    using dispatch_type = printable_dispatch;
    // Function pointer type for the print implementation
    using print_func_t = void (*)(const void*, std::ostream&);

    template <class T>
    static void print_impl(const void* obj, std::ostream& os) {
        const T& concrete_obj = *static_cast<const T*>(obj);
        if constexpr (requires { os << concrete_obj; }) {
            os << concrete_obj;  // Use operator<< if available
        } else if constexpr (requires { concrete_obj.toString(); }) {
            os << concrete_obj
                      .toString();  // Use toString() method if available
        } else if constexpr (requires { concrete_obj.to_string(); }) {
            os << concrete_obj
                      .to_string();  // Use to_string() method if available
        } else {
            // Fallback if no printing method is found
            os << "[unprintable " << typeid(T).name() << "]";
        }
    }
};

/**
 * @brief String conversion skill: Enables objects to be converted to a
 * std::string.
 *
 * Attempts to use `std::to_string`, `std::string` conversion, `toString()`, or
 * `to_string()`. Returns a default message if no conversion is possible.
 */
struct stringable_dispatch {
    static constexpr bool is_direct = false;  // Indirect dispatch
    using dispatch_type = stringable_dispatch;
    // Function pointer type for the string conversion implementation
    using to_string_func_t = std::string (*)(const void*);

    template <class T>
    static std::string to_string_impl(const void* obj) {
        const T& concrete_obj = *static_cast<const T*>(obj);
        if constexpr (requires { std::to_string(concrete_obj); }) {
            return std::to_string(
                concrete_obj);  // Use std::to_string if applicable
        } else if constexpr (requires { std::string(concrete_obj); }) {
            return std::string(concrete_obj);  // Use explicit conversion to
                                               // std::string if available
        } else if constexpr (requires { concrete_obj.toString(); }) {
            return concrete_obj
                .toString();  // Use toString() method if available
        } else if constexpr (requires { concrete_obj.to_string(); }) {
            return concrete_obj
                .to_string();  // Use to_string() method if available
        } else {
            // Fallback if no string conversion method is found
            return "[no string conversion for type: " +
                   std::string(typeid(T).name()) + "]";
        }
    }
};

/**
 * @brief Comparison skill: Enables objects to be compared for equality and
 * ordering.
 *
 * Requires `operator==` for equality and `operator<` for less-than comparison.
 * If types mismatch, equality returns false, and less-than compares type_info.
 */
struct comparable_dispatch {
    static constexpr bool is_direct = false;  // Indirect dispatch
    using dispatch_type = comparable_dispatch;
    // Function pointer types for comparison implementations
    using equals_func_t = bool (*)(const void*, const void*,
                                   const std::type_info&);
    using less_than_func_t = bool (*)(const void*, const void*,
                                      const std::type_info&);

    template <class T>
    static bool equals_impl(const void* obj1, const void* obj2,
                            const std::type_info& type2_info) {
        if (typeid(T) != type2_info) {
            return false;  // Type mismatch
        }

        const T& concrete_obj1 = *static_cast<const T*>(obj1);
        const T& concrete_obj2 = *static_cast<const T*>(obj2);

        if constexpr (requires { concrete_obj1 == concrete_obj2; }) {
            return concrete_obj1 ==
                   concrete_obj2;  // Use operator== if available
        } else {
            return false;  // Comparison not supported
        }
    }

    template <class T>
    static bool less_than_impl(const void* obj1, const void* obj2,
                               const std::type_info& type2_info) {
        if (typeid(T) != type2_info) {
            // If types differ, sort based on type_info::before
            return typeid(T).before(type2_info);
        }

        const T& concrete_obj1 = *static_cast<const T*>(obj1);
        const T& concrete_obj2 = *static_cast<const T*>(obj2);

        if constexpr (requires { concrete_obj1 < concrete_obj2; }) {
            return concrete_obj1 < concrete_obj2;  // Use operator< if available
        } else {
            return false;  // Comparison not supported
        }
    }
};

/**
 * @brief Serialization skill: Enables objects to be serialized to a string and
 * deserialized from a string.
 *
 * Attempts to use `serialize()`, `toJson()`, or `to_json()` for serialization.
 * For basic types, provides a simple JSON-like string representation.
 * Attempts to use `deserialize()`, `fromJson()`, or `from_json()` for
 * deserialization.
 */
struct serializable_dispatch {
    static constexpr bool is_direct = false;  // Indirect dispatch
    using dispatch_type = serializable_dispatch;
    // Function pointer types for serialization/deserialization implementations
    using serialize_func_t = std::string (*)(const void*);
    using deserialize_func_t = bool (*)(void*, const std::string&);

    template <class T>
    static std::string serialize_impl(const void* obj) {
        const T& concrete_obj = *static_cast<const T*>(obj);
        // Check for common serialization methods
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
            // Fallback: Convert basic types to JSON-like strings
            if constexpr (std::is_same_v<T, std::string>) {
                return "\"" + concrete_obj + "\"";  // Quote strings
            } else if constexpr (std::is_arithmetic_v<T>) {
                // Use stringable skill for numbers
                return stringable_dispatch::to_string_impl<T>(obj);
            } else if constexpr (std::is_same_v<T, bool>) {
                return concrete_obj ? "true"
                                    : "false";  // Booleans as true/false
            } else {
                return "null";  // Default to null for unsupported types
            }
        }
    }

    template <class T>
    static bool deserialize_impl(void* obj, const std::string& data) {
        T& concrete_obj = *static_cast<T*>(obj);
        // Check for common deserialization methods
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
            return false;  // Deserialization not supported
        }
    }
};

/**
 * @brief Cloneable skill: Enables objects to be cloned (deep copied).
 *
 * Attempts to use a `clone()` method if available.
 * Falls back to copy construction if the type is copy-constructible.
 * Returns nullptr if cloning is not supported.
 */
struct cloneable_dispatch {
    static constexpr bool is_direct = false;  // Indirect dispatch
    using dispatch_type = cloneable_dispatch;
    // Function pointer type for the clone implementation
    using clone_func_t = void* (*)(const void*);

    template <class T>
    static void* clone_impl(const void* obj) {
        const T& concrete_obj = *static_cast<const T*>(obj);
        if constexpr (requires { concrete_obj.clone(); }) {
            // Use clone() method if available
            auto cloned = concrete_obj.clone();
            // Assume clone() returns by value or rvalue-ref, create new T
            return new T(std::move(cloned));
        } else if constexpr (std::is_copy_constructible_v<T>) {
            // Fallback to copy constructor
            return new T(concrete_obj);
        } else {
            return nullptr;  // Cloning not supported
        }
    }
};

/**
 * @brief JSON conversion skill: Enables objects to be converted to/from JSON
 * strings.
 *
 * Primarily looks for `toJson()`, `to_json()`, `fromJson()`, `from_json()`.
 * Falls back to the `serializable_dispatch` skill if specific JSON methods
 * aren't found.
 */
struct json_convertible_dispatch {
    static constexpr bool is_direct = false;  // Indirect dispatch
    using dispatch_type = json_convertible_dispatch;
    // Function pointer types for JSON conversion implementations
    using to_json_func_t = std::string (*)(const void*);
    using from_json_func_t = bool (*)(void*, const std::string&);

    template <class T>
    static std::string to_json_impl(const void* obj) {
        const T& concrete_obj = *static_cast<const T*>(obj);
        // Check for specific JSON serialization methods
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
            // Fallback: Try using the general serialization interface
            return serializable_dispatch::serialize_impl<T>(obj);
        }
    }

    template <class T>
    static bool from_json_impl(void* obj, const std::string& json) {
        T& concrete_obj = *static_cast<T*>(obj);
        // Check for specific JSON deserialization methods
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
            // Fallback: Try using the general deserialization interface
            return serializable_dispatch::deserialize_impl<T>(obj, json);
        }
    }
};

/**
 * @brief Callable skill: Enables objects (like lambdas or function objects) to
 * be called.
 *
 * Supports calling with zero or one `std::any` argument currently.
 * Returns the result as `std::any`. Returns empty `std::any` for void functions
 * or if call fails.
 * @todo Extend to support more argument counts and types.
 */
struct callable_dispatch {
    static constexpr bool is_direct = false;  // Indirect dispatch
    using dispatch_type = callable_dispatch;
    // Function pointer type for the call implementation
    using call_func_t = std::any (*)(const void*, const std::vector<std::any>&);

    template <class T>
    static std::any call_impl(const void* obj,
                              const std::vector<std::any>& args) {
        const T& concrete_obj = *static_cast<const T*>(obj);

        // Check if callable with zero arguments
        if constexpr (requires(const T& t) { t(); }) {
            if (args.empty()) {
                if constexpr (std::is_void_v<decltype(concrete_obj())>) {
                    concrete_obj();  // Call void function
                    return {};       // Return empty std::any for void
                } else {
                    return concrete_obj();  // Call non-void function and return
                                            // result
                }
            }
        }

        // Check if callable with one std::any argument
        // Note: This requires the object to accept std::any directly.
        // More sophisticated argument handling would involve type
        // checking/casting.
        if constexpr (requires(const T& t, const std::any& a) { t(a); }) {
            if (args.size() == 1) {
                if constexpr (std::is_void_v<decltype(concrete_obj(
                                  std::declval<std::any>()))>) {
                    concrete_obj(args[0]);  // Call void function
                    return {};              // Return empty std::any for void
                } else {
                    return concrete_obj(args[0]);  // Call non-void function
                }
            }
        }

        // Support for different numbers/types of arguments can be added here...

        // Call not supported or arguments mismatch
        return {};
    }
};

}  // namespace enhanced_any_skills

// Define the facade for EnhancedBoxedValue
// Use the default_builder and add multiple skills and constraints.
using enhanced_boxed_value_facade = default_builder::
    // Add support for various skills defined above
    add_convention<enhanced_any_skills::printable_dispatch,
                   void(std::ostream&) const>::  // print(ostream)
    add_convention<enhanced_any_skills::stringable_dispatch,
                   std::string() const>::  // toString()
    add_convention<
        enhanced_any_skills::comparable_dispatch,
        bool(const proxy<typename default_builder::build>&)
            const,  // equals(other_proxy)
        bool(const proxy<typename default_builder::build>&)
            const  // lessThan(other_proxy) - Note: Facade needs distinct
                   // signatures if methods have same params
        >::add_convention<enhanced_any_skills::serializable_dispatch,
                          std::string() const,      // serialize()
                          bool(const std::string&)  // deserialize(string)
                          >::
        add_convention<enhanced_any_skills::cloneable_dispatch,
                       void*() const>::  // clone() -> void* (raw pointer)
    add_convention<enhanced_any_skills::json_convertible_dispatch,
                   std::string() const,      // toJson()
                   bool(const std::string&)  // fromJson(string)
                   >::
        add_convention<
            enhanced_any_skills::callable_dispatch,
            std::any(const std::vector<std::any>&)>::  // call(vector<any>)
    // Add constraints
    restrict_layout<256>::  // Limit size to 256 bytes (adjust as needed)
    support_copy<constraint_level::nothrow>::        // Require nothrow copyable
    support_relocation<constraint_level::nothrow>::  // Require nothrow
                                                     // relocatable (move
                                                     // construct + destroy old)
    support_destruction<constraint_level::nothrow>::  // Require nothrow
                                                      // destructible
    build;  // Finalize the facade definition

// Move the visitor outside the class to avoid issues with template nesting in
// local classes Visitor for handling the BoxedValue's value and creating a
// proxy object
struct ProxyVisitor {
    bool success = false;
    proxy<enhanced_boxed_value_facade> result;

    // Handle common basic types
    template <typename T>
    bool operator()(T& value) {  // Return bool instead of void
        if constexpr (
            // Check if the type satisfies the conditions required by
            // enhanced_boxed_value_facade
            std::is_copy_constructible_v<T> && !std::is_pointer_v<T>) {
            try {
                result = proxy<enhanced_boxed_value_facade>(value);
                success = true;
                return true;  // Return true on success
            } catch (const std::exception& e) {
                std::cerr << "Failed to create proxy for type "
                          << typeid(T).name() << ": " << e.what() << std::endl;
                return false;  // Return false on failure
            }
        }
        return false;  // Return false when the type does not meet the
                       // conditions
    }

    // Fallback function to handle cases that cannot be matched
    bool fallback() {  // Return bool instead of void
        // Keep success as false
        return false;  // Return false to indicate fallback
    }
};

/**
 * \class EnhancedBoxedValue
 * \brief An enhanced version of BoxedValue that uses the facade pattern
 *        to provide powerful type erasure and dynamic dispatch capabilities.
 *
 * This class wraps a BoxedValue and uses a `proxy` object (based on the
 * `enhanced_boxed_value_facade`) to dynamically invoke skills (like printing,
 * serialization, comparison) on the contained value, regardless of its
 * underlying type, as long as the type supports the required operations
 * or the skill provides a fallback.
 */
class EnhancedBoxedValue {
private:
    // Internally uses the original BoxedValue to store the value and its
    // metadata.
    BoxedValue boxed_value_;

    // Uses a proxy object based on the defined facade for skill dispatch.
    proxy<enhanced_boxed_value_facade> proxy_;

    // Flag indicating whether a valid proxy object could be created for the
    // stored type.
    bool has_proxy_ = false;

public:
    // Default constructor: Creates an empty EnhancedBoxedValue.
    EnhancedBoxedValue() : boxed_value_() {}

    // Construct from an existing BoxedValue.
    explicit EnhancedBoxedValue(const BoxedValue& value) : boxed_value_(value) {
        initProxy();  // Attempt to initialize the proxy for the contained
                      // value.
    }

    // Construct from any type T (excluding EnhancedBoxedValue and BoxedValue
    // itself).
    template <typename T>
        requires(!std::same_as<EnhancedBoxedValue, std::decay_t<T>> &&
                 !std::same_as<BoxedValue, std::decay_t<T>>)
    explicit EnhancedBoxedValue(T&& value)
        : boxed_value_(std::forward<T>(value)) {
        initProxy();  // Attempt to initialize the proxy.
    }

    // Construct from any type T with an associated description.
    template <typename T>
        requires(!std::same_as<EnhancedBoxedValue, std::decay_t<T>> &&
                 !std::same_as<BoxedValue, std::decay_t<T>>)
    EnhancedBoxedValue(T&& value, std::string_view description)
        : boxed_value_(varWithDesc(std::forward<T>(value), description)) {
        initProxy();  // Attempt to initialize the proxy.
    }

    // Copy constructor.
    EnhancedBoxedValue(const EnhancedBoxedValue& other)
        : boxed_value_(other.boxed_value_), has_proxy_(other.has_proxy_) {
        if (has_proxy_) {
            // If the source had a proxy, copy it.
            // Assumes the proxy itself is copyable as defined by the facade
            // constraints.
            proxy_ = other.proxy_;
        }
    }

    // Move constructor.
    EnhancedBoxedValue(EnhancedBoxedValue&& other) noexcept
        : boxed_value_(std::move(other.boxed_value_)),
          proxy_(std::move(other.proxy_)),  // Move the proxy
          has_proxy_(other.has_proxy_) {
        other.has_proxy_ = false;  // Source no longer has a valid proxy.
    }

    // Copy assignment operator.
    EnhancedBoxedValue& operator=(const EnhancedBoxedValue& other) {
        if (this != &other) {
            boxed_value_ = other.boxed_value_;
            has_proxy_ = other.has_proxy_;
            if (has_proxy_) {
                proxy_ = other.proxy_;  // Copy the proxy
            } else {
                proxy_.reset();  // Reset proxy if source didn't have one
            }
        }
        return *this;
    }

    // Move assignment operator.
    EnhancedBoxedValue& operator=(EnhancedBoxedValue&& other) noexcept {
        if (this != &other) {
            boxed_value_ = std::move(other.boxed_value_);
            proxy_ = std::move(other.proxy_);  // Move the proxy
            has_proxy_ = other.has_proxy_;
            other.has_proxy_ = false;  // Source no longer has a valid proxy.
        }
        return *this;
    }

    // Assign from any type T.
    template <typename T>
        requires(!std::same_as<EnhancedBoxedValue, std::decay_t<T>> &&
                 !std::same_as<BoxedValue, std::decay_t<T>>)
    EnhancedBoxedValue& operator=(T&& value) {
        boxed_value_ =
            std::forward<T>(value);  // Assign to the internal BoxedValue
        initProxy();  // Re-initialize the proxy for the new value.
        return *this;
    }

    // Get the internal BoxedValue (const access).
    [[nodiscard]] const BoxedValue& getBoxedValue() const {
        return boxed_value_;
    }

    // Get the internal proxy object (const access).
    // Throws if no valid proxy exists.
    [[nodiscard]] const proxy<enhanced_boxed_value_facade>& getProxy() const {
        if (!has_proxy_) {
            throw std::runtime_error(
                "No proxy available for the contained value.");
        }
        return proxy_;
    }

    // Check if the EnhancedBoxedValue holds a valid, non-null, non-undefined
    // value.
    [[nodiscard]] bool hasValue() const {
        return !boxed_value_.isUndef() && !boxed_value_.isNull();
    }

    // Check if a valid proxy object was successfully created for the contained
    // value.
    [[nodiscard]] bool hasProxy() const { return has_proxy_; }

    // Convert the contained value to a string using the stringable skill.
    // Falls back to BoxedValue::debugString if the skill fails or no proxy
    // exists.
    [[nodiscard]] std::string toString() const {
        if (has_proxy_) {
            try {
                // Call the stringable skill via the proxy
                return proxy_.call<enhanced_any_skills::stringable_dispatch,
                                   std::string>();
            } catch (const std::exception& e) {
                // Log error?
                // Fallback to the original BoxedValue's debug string
                // representation
                return boxed_value_.debugString() +
                       " (proxy call failed: " + e.what() + ")";
            }
        } else {
            // Fallback if no proxy was created
            return boxed_value_.debugString();
        }
    }

    // Convert the contained value to a JSON string using the JSON conversion
    // skill. Falls back to toString() if the skill fails or no proxy exists.
    [[nodiscard]] std::string toJson() const {
        if (has_proxy_) {
            try {
                // Call the JSON conversion skill via the proxy
                return proxy_
                    .call<enhanced_any_skills::json_convertible_dispatch,
                          std::string>();
            } catch (const std::exception& e) {
                // Log error?
                // Fallback to the general toString() method
                return toString() + " (proxy call failed: " + e.what() + ")";
            }
        } else {
            // Fallback if no proxy was created
            return toString();
        }
    }

    // Load the state of the contained value from a JSON string using the JSON
    // conversion skill. Returns true on success, false otherwise or if no proxy
    // exists.
    bool fromJson(const std::string& json) {
        if (has_proxy_) {
            try {
                // Call the JSON deserialization skill via the proxy
                // Note: This modifies the object held by the proxy.
                return proxy_
                    .call<enhanced_any_skills::json_convertible_dispatch, bool>(
                        json);
            } catch (const std::exception&) {
                // Log error?
                return false;  // Skill call failed
            }
        }
        return false;  // No proxy available
    }

    // Print the contained value to an output stream using the printable skill.
    // Falls back to printing the BoxedValue::debugString if the skill fails or
    // no proxy exists.
    void print(std::ostream& os = std::cout) const {
        if (has_proxy_) {
            try {
                // Call the printable skill via the proxy
                proxy_.call<enhanced_any_skills::printable_dispatch>(os);
                return;  // Success
            } catch (const std::exception& e) {
                // Log error?
                // Fallback to printing the original BoxedValue's debug string
                os << boxed_value_.debugString()
                   << " (proxy call failed: " << e.what() << ")";
                return;
            }
        }
        // Fallback if no proxy was created
        os << boxed_value_.debugString();
    }

    // Compare this EnhancedBoxedValue with another for equality using the
    // comparable skill. Falls back to comparing TypeInfo if the skill fails or
    // proxies are missing.
    bool equals(const EnhancedBoxedValue& other) const {
        if (has_proxy_ && other.has_proxy_) {
            // Both have proxies, attempt comparison via skill
            try {
                // Call the comparison skill (equals part) via the proxy
                // Note: The facade definition needs adjustment if
                // equals/less_than have identical signatures. Assuming facade
                // maps the first bool(...) const to equals_impl.
                return proxy_
                    .call<enhanced_any_skills::comparable_dispatch, bool>(
                        other.proxy_);
            } catch (const std::exception&) {
                // Log error?
                // Fallback to comparing the underlying TypeInfo objects
                return boxed_value_.getTypeInfo() ==
                       other.boxed_value_.getTypeInfo();
            }
        } else {
            // If one or both lack a proxy, compare only the TypeInfo
            return boxed_value_.getTypeInfo() ==
                   other.boxed_value_.getTypeInfo();
        }
    }

    // Attempt to call the contained value if it's a function object, using the
    // callable skill. Returns the result as std::any, or an empty std::any on
    // failure or if not callable.
    std::any call(const std::vector<std::any>& args = {}) {
        if (has_proxy_) {
            try {
                // Call the callable skill via the proxy
                return proxy_
                    .call<enhanced_any_skills::callable_dispatch, std::any>(
                        args);
            } catch (const std::exception&) {
                // Log error?
                return {};  // Return empty std::any on failure
            }
        }
        return {};  // No proxy available
    }

    // Clone the EnhancedBoxedValue using the cloneable skill.
    // Returns a new EnhancedBoxedValue containing the cloned object.
    // Falls back to copy construction if cloning fails or isn't supported.
    // @warning The current clone_impl returns void*, which is problematic for
    //          ownership
    //          and constructing the new EnhancedBoxedValue safely without
    //          knowing the type. This implementation simplifies by falling back
    //          to copy construction, which might not be a true deep clone if
    //          the copy constructor is shallow. A more robust solution would
    //          involve the facade managing object lifetime or returning a proxy
    //          directly.
    EnhancedBoxedValue clone() const {
        if (has_proxy_) {
            try {
                // Call the cloneable skill via the proxy
                void* cloned_ptr =
                    proxy_
                        .call<enhanced_any_skills::cloneable_dispatch, void*>();
                if (cloned_ptr) {
                    // Problem: We have a void* but need the actual type T to
                    // manage the memory and construct a new EnhancedBoxedValue
                    // correctly. The facade doesn't easily expose the concrete
                    // type here.
                    //
                    // Simplification: Assume the clone skill failed or wasn't
                    // fully implemented, and fall back to copy construction.
                    // This avoids memory leaks with the raw pointer. delete
                    // static_cast<???*>(cloned_ptr); // How to delete? return
                    // EnhancedBoxedValue( *static_cast<T*>(cloned_ptr) ); //
                    // How to construct?

                    // Safer fallback: Use copy constructor.
                    // If a true deep clone via `clone()` is needed, the
                    // facade/skill design needs refinement (e.g., returning a
                    // proxy or using shared_ptr).
                    std::cerr << "Warning: clone() skill returned pointer, but "
                                 "falling back to copy construction due to "
                                 "type erasure limitations."
                              << std::endl;
                    // We should ideally delete cloned_ptr here if we knew the
                    // type. Since we don't, this might leak if clone_impl
                    // allocated memory. To avoid the leak risk, let's just rely
                    // on copy construction for now. delete cloned_ptr; // This
                    // is unsafe without type info.
                }
                // Fall through to copy construction if clone_impl returned
                // nullptr or we aborted due to void*
            } catch (const std::exception& e) {
                // Log error?
                // Fallback to copy construction if skill call failed
                std::cerr << "Warning: clone() skill failed (" << e.what()
                          << "), falling back to copy construction."
                          << std::endl;
            }
        }
        // Fallback to copy construction if no proxy or clone failed
        return EnhancedBoxedValue(*this);
    }

    // Get the TypeInfo of the contained value from the internal BoxedValue.
    [[nodiscard]] const TypeInfo& getTypeInfo() const {
        return boxed_value_.getTypeInfo();
    }

    // Check if the contained value is of a specific type T.
    template <typename T>
    [[nodiscard]] bool isType() const {
        return boxed_value_.isType<T>();
    }

    // Attempt to cast the contained value to a specific type T.
    // Returns std::optional<T>.
    template <typename T>
    [[nodiscard]] std::optional<T> tryCast() const {
        return boxed_value_.tryCast<T>();
    }

    // --- Attribute Management (delegated to internal BoxedValue) ---

    // Set an attribute (key-value pair) associated with this value.
    EnhancedBoxedValue& setAttr(const std::string& name,
                                const EnhancedBoxedValue& value) {
        // Store the attribute using the internal BoxedValue's attribute system.
        // We store the internal BoxedValue from the passed EnhancedBoxedValue.
        boxed_value_.setAttr(name, value.boxed_value_);
        return *this;
    }

    // Get an attribute by name. Returns an EnhancedBoxedValue wrapping the
    // attribute value. Returns an undefined EnhancedBoxedValue if the attribute
    // doesn't exist.
    [[nodiscard]] EnhancedBoxedValue getAttr(const std::string& name) const {
        BoxedValue attr = boxed_value_.getAttr(name);
        // Wrap the retrieved BoxedValue attribute in a new EnhancedBoxedValue.
        return EnhancedBoxedValue(attr);
    }

    // List the names of all attributes associated with this value.
    [[nodiscard]] std::vector<std::string> listAttrs() const {
        return boxed_value_.listAttrs();
    }

    // Check if an attribute with the given name exists.
    [[nodiscard]] bool hasAttr(const std::string& name) const {
        return boxed_value_.hasAttr(name);
    }

    // Remove an attribute by name.
    void removeAttr(const std::string& name) { boxed_value_.removeAttr(name); }

    // Reset the EnhancedBoxedValue to an empty/undefined state.
    void reset() {
        boxed_value_ = BoxedValue();  // Reset internal BoxedValue
        if (has_proxy_) {
            proxy_.reset();  // Reset the proxy
            has_proxy_ = false;
        }
    }

    // Equality operator (uses the equals method).
    friend bool operator==(const EnhancedBoxedValue& lhs,
                           const EnhancedBoxedValue& rhs) {
        return lhs.equals(rhs);
    }

    // Output stream operator (uses the print method).
    friend std::ostream& operator<<(std::ostream& os,
                                    const EnhancedBoxedValue& value) {
        value.print(os);
        return os;
    }

private:
    // Initialize the proxy object based on the type currently held by
    // boxed_value_.
    void initProxy() {
        // Don't create a proxy for undefined, null, or void types.
        if (boxed_value_.isUndef() || boxed_value_.isNull() ||
            boxed_value_.isVoid()) {
            has_proxy_ = false;
            proxy_.reset();  // Ensure proxy is reset
            return;
        }

        // Use BoxedValue's visit method to handle any type
        try {
            // Create a visitor instance
            ProxyVisitor visitor;

            // Visit the value stored in BoxedValue, attempting to create a
            // proxy
            boxed_value_.visit(
                visitor);  // Call visit, result is handled by visitor.success

            // Set proxy status based on the visit result
            has_proxy_ = visitor.success;
            if (has_proxy_) {
                proxy_ = std::move(visitor.result);
            } else {
                proxy_.reset();
                // Optional: Log the reason why the proxy could not be created
                // std::cerr << "Warning: No suitable proxy could be created for
                // the contained type." << std::endl;
            }
        } catch (const std::exception& e) {
            // Handle potential exceptions
            has_proxy_ = false;
            proxy_.reset();
            std::cerr << "Error initializing proxy: " << e.what() << std::endl;
        }
    }
};

// Convenience function to create an EnhancedBoxedValue from a value.
template <typename T>
auto enhancedVar(T&& value) -> EnhancedBoxedValue {
    return EnhancedBoxedValue(std::forward<T>(value));
}

// Convenience function to create an EnhancedBoxedValue from a value with a
// description.
template <typename T>
auto enhancedVarWithDesc(T&& value, std::string_view description)
    -> EnhancedBoxedValue {
    return EnhancedBoxedValue(std::forward<T>(value), description);
}

}  // namespace atom::meta

#endif  // ATOM_META_ENHANCED_ANY_HPP