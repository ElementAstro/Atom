/*
 * type_conversion.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Advanced Type Conversion System
Provides automatic type conversion between C++ and scripting languages,
including STL containers, custom types, and complex data structures.

**************************************************/

#ifndef ATOM_COMPONENT_TYPE_CONVERSION_HPP
#define ATOM_COMPONENT_TYPE_CONVERSION_HPP

#include <any>
#include <array>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "scripting_api.hpp"

namespace atom::components::scripting {

/**
 * @brief Type traits for automatic type conversion
 */
namespace type_traits {

// Check if type is a container
template <typename T>
struct is_container : std::false_type {};

template <typename T, typename A>
struct is_container<std::vector<T, A>> : std::true_type {};

template <typename T, typename A>
struct is_container<std::deque<T, A>> : std::true_type {};

template <typename T, typename A>
struct is_container<std::list<T, A>> : std::true_type {};

template <typename T, typename C, typename A>
struct is_container<std::set<T, C, A>> : std::true_type {};

template <typename T, typename H, typename E, typename A>
struct is_container<std::unordered_set<T, H, E, A>> : std::true_type {};

// Check if type is an associative container
template <typename T>
struct is_associative : std::false_type {};

template <typename K, typename V, typename C, typename A>
struct is_associative<std::map<K, V, C, A>> : std::true_type {};

template <typename K, typename V, typename H, typename E, typename A>
struct is_associative<std::unordered_map<K, V, H, E, A>> : std::true_type {};

// Check if type is optional
template <typename T>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};

// Check if type is a smart pointer
template <typename T>
struct is_smart_pointer : std::false_type {};

template <typename T>
struct is_smart_pointer<std::unique_ptr<T>> : std::true_type {};

template <typename T>
struct is_smart_pointer<std::shared_ptr<T>> : std::true_type {};

template <typename T>
struct is_smart_pointer<std::weak_ptr<T>> : std::true_type {};

// Check if type is a tuple
template <typename T>
struct is_tuple : std::false_type {};

template <typename... Args>
struct is_tuple<std::tuple<Args...>> : std::true_type {};

// Check if type is an array
template <typename T>
struct is_array : std::false_type {};

template <typename T, size_t N>
struct is_array<std::array<T, N>> : std::true_type {};

// Get container value type
template <typename T>
struct container_value_type {};

template <typename T, typename A>
struct container_value_type<std::vector<T, A>> {
    using type = T;
};

template <typename T, typename A>
struct container_value_type<std::deque<T, A>> {
    using type = T;
};

template <typename T, typename A>
struct container_value_type<std::list<T, A>> {
    using type = T;
};

template <typename T, typename C, typename A>
struct container_value_type<std::set<T, C, A>> {
    using type = T;
};

template <typename T, typename H, typename E, typename A>
struct container_value_type<std::unordered_set<T, H, E, A>> {
    using type = T;
};

// Get associative container key/value types
template <typename T>
struct associative_key_type {};

template <typename T>
struct associative_value_type {};

template <typename K, typename V, typename C, typename A>
struct associative_key_type<std::map<K, V, C, A>> {
    using type = K;
};

template <typename K, typename V, typename C, typename A>
struct associative_value_type<std::map<K, V, C, A>> {
    using type = V;
};

template <typename K, typename V, typename H, typename E, typename A>
struct associative_key_type<std::unordered_map<K, V, H, E, A>> {
    using type = K;
};

template <typename K, typename V, typename H, typename E, typename A>
struct associative_value_type<std::unordered_map<K, V, H, E, A>> {
    using type = V;
};

}  // namespace type_traits

/**
 * @brief Generic type converter interface
 */
template <typename ScriptEngine>
class TypeConverter {
public:
    explicit TypeConverter(ScriptEngine& engine) : engine_(engine) {}

    /**
     * @brief Converts C++ value to script value
     * @tparam T C++ type
     * @param value C++ value
     * @return True if conversion successful
     */
    template <typename T>
    bool toScript(const T& value);

    /**
     * @brief Converts script value to C++ value
     * @tparam T Target C++ type
     * @param index Script stack/object index
     * @return Converted value or nullopt if conversion failed
     */
    template <typename T>
    std::optional<T> fromScript(int index = -1);

    /**
     * @brief Converts STL container to script array/table
     * @tparam Container STL container type
     * @param container C++ container
     * @return True if conversion successful
     */
    template <typename Container>
    bool containerToScript(const Container& container);

    /**
     * @brief Converts script array/table to STL container
     * @tparam Container STL container type
     * @param index Script stack/object index
     * @return Converted container or nullopt if conversion failed
     */
    template <typename Container>
    std::optional<Container> containerFromScript(int index = -1);

    /**
     * @brief Converts std::optional to script value
     * @tparam T Optional value type
     * @param opt Optional value
     * @return True if conversion successful
     */
    template <typename T>
    bool optionalToScript(const std::optional<T>& opt);

    /**
     * @brief Converts script value to std::optional
     * @tparam T Optional value type
     * @param index Script stack/object index
     * @return Converted optional
     */
    template <typename T>
    std::optional<T> optionalFromScript(int index = -1);

    /**
     * @brief Converts std::tuple to script array
     * @tparam Args Tuple argument types
     * @param tuple Tuple value
     * @return True if conversion successful
     */
    template <typename... Args>
    bool tupleToScript(const std::tuple<Args...>& tuple);

    /**
     * @brief Converts script array to std::tuple
     * @tparam Args Tuple argument types
     * @param index Script stack/object index
     * @return Converted tuple or nullopt if conversion failed
     */
    template <typename... Args>
    std::optional<std::tuple<Args...>> tupleFromScript(int index = -1);

    /**
     * @brief Converts std::variant to script value
     * @tparam Args Variant argument types
     * @param variant Variant value
     * @return True if conversion successful
     */
    template <typename... Args>
    bool variantToScript(const std::variant<Args...>& variant);

    /**
     * @brief Converts script value to std::variant
     * @tparam Args Variant argument types
     * @param index Script stack/object index
     * @return Converted variant or nullopt if conversion failed
     */
    template <typename... Args>
    std::optional<std::variant<Args...>> variantFromScript(int index = -1);

private:
    ScriptEngine& engine_;

    // Helper methods for recursive conversion
    template <typename T>
    bool convertPrimitive(const T& value);

    template <typename T>
    std::optional<T> convertPrimitiveFrom(int index);

    template <typename Container>
    bool convertSequenceContainer(const Container& container);

    template <typename Container>
    std::optional<Container> convertSequenceContainerFrom(int index);

    template <typename Container>
    bool convertAssociativeContainer(const Container& container);

    template <typename Container>
    std::optional<Container> convertAssociativeContainerFrom(int index);

    // Tuple conversion helpers
    template <size_t I = 0, typename... Args>
    bool tupleToScriptImpl(const std::tuple<Args...>& tuple);

    template <size_t I = 0, typename... Args>
    bool tupleFromScriptImpl(std::tuple<Args...>& tuple, int index);

    // Variant conversion helpers
    template <size_t I = 0, typename... Args>
    bool variantToScriptImpl(const std::variant<Args...>& variant);

    template <size_t I = 0, typename... Args>
    std::optional<std::variant<Args...>> variantFromScriptImpl(int index);
};

/**
 * @brief Automatic type registration system
 */
template <typename ScriptEngine>
class TypeRegistry {
public:
    explicit TypeRegistry(ScriptEngine& engine)
        : engine_(engine), converter_(engine) {}

    /**
     * @brief Registers a C++ type for automatic conversion
     * @tparam T Type to register
     * @param typeName Type name in script
     */
    template <typename T>
    void registerType(const std::string& typeName);

    /**
     * @brief Registers a C++ enum for script access
     * @tparam E Enum type
     * @param enumName Enum name in script
     * @param values Enum values and names
     */
    template <typename E>
    void registerEnum(const std::string& enumName,
                      const std::vector<std::pair<E, std::string>>& values);

    /**
     * @brief Registers STL container types
     */
    void registerStandardTypes();

    /**
     * @brief Gets the type converter
     * @return Type converter reference
     */
    TypeConverter<ScriptEngine>& getConverter() { return converter_; }

private:
    ScriptEngine& engine_;
    TypeConverter<ScriptEngine> converter_;
    std::unordered_map<std::string, std::any> registeredTypes_;
};

/**
 * @brief Conversion result with error information
 */
struct ConversionResult {
    bool success = false;
    std::string errorMessage;
    std::any convertedValue;

    template <typename T>
    std::optional<T> get() const {
        if (!success)
            return std::nullopt;
        try {
            return std::any_cast<T>(convertedValue);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }
};

/**
 * @brief Universal type conversion function
 * @tparam ScriptEngine Script engine type
 * @tparam T Target type
 * @param engine Script engine
 * @param value Input value
 * @return Conversion result
 */
template <typename ScriptEngine, typename T>
ConversionResult convertType(ScriptEngine& engine, const std::any& value);

/**
 * @brief Type conversion utilities
 */
namespace conversion_utils {

/**
 * @brief Checks if a type can be converted
 * @tparam From Source type
 * @tparam To Target type
 * @return True if conversion is possible
 */
template <typename From, typename To>
constexpr bool is_convertible_v = std::is_convertible_v<From, To>;

/**
 * @brief Gets type name as string
 * @tparam T Type
 * @return Type name
 */
template <typename T>
std::string getTypeName();

/**
 * @brief Validates type conversion safety
 * @tparam From Source type
 * @tparam To Target type
 * @param value Source value
 * @return True if conversion is safe
 */
template <typename From, typename To>
bool isConversionSafe(const From& value);

}  // namespace conversion_utils

}  // namespace atom::components::scripting

#endif  // ATOM_COMPONENT_TYPE_CONVERSION_HPP
