/*!
 * \file type_info.hpp
 * \brief Enhanced TypeInfo for better type handling with C++20/23 support
 * \author Max Qian <lightapt.com> with enhancements
 * \date 2025-03-13
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_TYPE_INFO_HPP
#define ATOM_META_TYPE_INFO_HPP

#include <bitset>           // Include for flags
#include <concepts>         // Include for C++20 concepts
#include <cstdlib>          // Include for std::size_t
#include <functional>       // Include for std::hash
#include <memory>           // Include for smart pointers
#include <mutex>            // Include for thread safety
#include <optional>         // Include for std::optional
#include <ostream>          // Include for std::ostream
#include <shared_mutex>     // Include for reader-writer lock
#include <source_location>  // Include for diagnostics
#include <span>             // Include for C++20 span
#include <string>           // Include for std::string
#include <string_view>      // Include for string_view
#include <type_traits>      // Include for type traits
#include <typeinfo>         // Include for type_info
#include <unordered_map>    // Include for registry
#include <vector>           // Include for containers

#include "abi.hpp"  // Include for ATOM_INLINE, ATOM_CONSTEXPR, ATOM_NOEXCEPT
#include "concept.hpp"  // Include for Pointer, SmartPointer

namespace atom::meta {

// Constants for bitset size, increased for additional flags
constexpr std::size_t K_FLAG_BITSET_SIZE = 32;  // Expanded for future use

// Helper to remove cv-qualifiers, references, and pointers
template <typename T>
using BareType =
    std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>;

// C++20 concept for types that can be used with TypeInfo
template <typename T>
concept TypeInfoCompatible = requires {
    { typeid(T) } -> std::convertible_to<std::type_info>;
};

template <typename T>
struct PointerType {};

template <typename T>
struct PointerType<T*> {
    using type = T;
};

template <typename T>
struct PointerType<std::shared_ptr<T>> {
    using type = T;
};

template <typename T>
struct PointerType<std::unique_ptr<T>> {
    using type = T;
};

template <typename T>
struct PointerType<std::weak_ptr<T>> {
    using type = T;
};

// Added support for std::span (C++20)
template <typename T, std::size_t Extent>
struct PointerType<std::span<T, Extent>> {
    using type = T;
};

template <typename T>
constexpr bool K_IS_ARITHMETIC_POINTER_V =
    std::is_arithmetic_v<typename PointerType<T>::type>;

/**
 * @brief Exception class for TypeInfo operations
 */
class TypeInfoException : public std::runtime_error {
public:
    explicit TypeInfoException(
        const std::string& message,
        const std::source_location& location = std::source_location::current())
        : std::runtime_error(std::string(message) + " [at " +
                             location.file_name() + ":" +
                             std::to_string(location.line()) + "]") {}
};

/// \brief Compile time deduced information about a type
class TypeInfo {
public:
    using Flags = std::bitset<K_FLAG_BITSET_SIZE>;  // Using bitset for flags

    /// \brief Construct a new Type Info object
    constexpr TypeInfo(Flags flags, const std::type_info* typeInfo,
                       const std::type_info* bareTypeInfo) noexcept
        : mTypeInfo_(typeInfo), mBareTypeInfo_(bareTypeInfo), mFlags_(flags) {}

    constexpr TypeInfo() noexcept = default;

    // Added move semantics
    constexpr TypeInfo(TypeInfo&& other) noexcept = default;
    constexpr TypeInfo& operator=(TypeInfo&& other) noexcept = default;

    // Ensure copyable
    constexpr TypeInfo(const TypeInfo& other) noexcept = default;
    constexpr TypeInfo& operator=(const TypeInfo& other) noexcept = default;

    /**
     * @brief Create TypeInfo from a type
     * @tparam T The type to create information for
     * @return TypeInfo object containing information about T
     */
    template <TypeInfoCompatible T>
    static consteval auto fromType() noexcept -> TypeInfo {
        using BareT = BareType<T>;
        Flags flags;

        // Basic type traits
        flags.set(IS_CONST_FLAG, std::is_const_v<std::remove_reference_t<T>>);
        flags.set(IS_REFERENCE_FLAG, std::is_reference_v<T>);
        flags.set(IS_POINTER_FLAG, Pointer<T> || Pointer<BareT> ||
                                       SmartPointer<T> || SmartPointer<BareT>);
        flags.set(IS_VOID_FLAG, std::is_void_v<T>);

        // Determine if arithmetic
        if constexpr (Pointer<T> || Pointer<BareT> || SmartPointer<T> ||
                      SmartPointer<BareT>) {
            flags.set(IS_ARITHMETIC_FLAG, K_IS_ARITHMETIC_POINTER_V<T>);
        } else {
            flags.set(IS_ARITHMETIC_FLAG, std::is_arithmetic_v<T>);
        }

        // Type categories
        flags.set(IS_ARRAY_FLAG, std::is_array_v<T>);
        flags.set(IS_ENUM_FLAG, std::is_enum_v<T>);
        flags.set(IS_CLASS_FLAG, std::is_class_v<T>);
        flags.set(IS_FUNCTION_FLAG, std::is_function_v<T>);

        // Type properties
        flags.set(IS_TRIVIAL_FLAG, std::is_trivial_v<T>);
        flags.set(IS_STANDARD_LAYOUT_FLAG, std::is_standard_layout_v<T>);
        flags.set(IS_POD_FLAG,
                  std::is_trivial_v<T> && std::is_standard_layout_v<T>);

        // Constructibility traits
        flags.set(IS_DEFAULT_CONSTRUCTIBLE_FLAG,
                  std::is_default_constructible_v<T>);
        flags.set(IS_MOVEABLE_FLAG, std::is_move_constructible_v<T>);
        flags.set(IS_COPYABLE_FLAG, std::is_copy_constructible_v<T>);

        // C++20 new traits
        flags.set(IS_AGGREGATE_FLAG, std::is_aggregate_v<T>);
        flags.set(IS_BOUNDED_ARRAY_FLAG, std::is_bounded_array_v<T>);
        flags.set(IS_UNBOUNDED_ARRAY_FLAG, std::is_unbounded_array_v<T>);
        flags.set(IS_SCOPED_ENUM_FLAG, std::is_scoped_enum_v<T>);
        flags.set(IS_FINAL_FLAG, std::is_final_v<T>);
        flags.set(IS_ABSTRACT_FLAG, std::is_abstract_v<T>);
        flags.set(IS_POLYMORPHIC_FLAG, std::is_polymorphic_v<T>);
        flags.set(IS_EMPTY_FLAG, std::is_empty_v<T>);

        return {flags, &typeid(T), &typeid(BareT)};
    }

    /**
     * @brief Create TypeInfo from an instance
     * @tparam T The type of the instance
     * @param instance The instance to create information for
     * @return TypeInfo object containing information about T
     */
    template <typename T>
    static auto fromInstance(const T& instance
                             [[maybe_unused]]) noexcept -> TypeInfo {
        return fromType<T>();
    }

    template <typename T>
    static constexpr auto fromType() noexcept -> TypeInfo {
        // Implementation will depend on how TypeInfo is constructed
        // This is a placeholder implementation
        TypeInfo info;
        // Set up the TypeInfo based on T
        return info;
    }

    /**
     * @brief Less than comparison operator
     * @param otherTypeInfo The TypeInfo to compare against
     * @return true if this TypeInfo is less than otherTypeInfo
     */
    auto operator<(const TypeInfo& otherTypeInfo) const noexcept -> bool {
        return mTypeInfo_->before(*otherTypeInfo.mTypeInfo_);
    }

    /**
     * @brief Inequality operator
     * @param otherTypeInfo The TypeInfo to compare against
     * @return true if this TypeInfo is not equal to otherTypeInfo
     */
    constexpr auto operator!=(const TypeInfo& otherTypeInfo) const noexcept
        -> bool {
        return !(*this == otherTypeInfo);
    }

    /**
     * @brief Equality operator
     * @param otherTypeInfo The TypeInfo to compare against
     * @return true if this TypeInfo is equal to otherTypeInfo
     */
    constexpr auto operator==(const TypeInfo& otherTypeInfo) const noexcept
        -> bool {
        return otherTypeInfo.mTypeInfo_ == mTypeInfo_ &&
               *otherTypeInfo.mTypeInfo_ == *mTypeInfo_ &&
               otherTypeInfo.mBareTypeInfo_ == mBareTypeInfo_ &&
               *otherTypeInfo.mBareTypeInfo_ == *mBareTypeInfo_ &&
               otherTypeInfo.mFlags_ == mFlags_;
    }

    /**
     * @brief Check if the bare types are equal
     * @param otherTypeInfo The TypeInfo to compare against
     * @return true if the bare types are equal
     */
    [[nodiscard]] constexpr auto bareEqual(
        const TypeInfo& otherTypeInfo) const noexcept -> bool {
        return otherTypeInfo.mBareTypeInfo_ == mBareTypeInfo_ ||
               *otherTypeInfo.mBareTypeInfo_ == *mBareTypeInfo_;
    }

    /**
     * @brief Check if the bare type equals a specific type_info
     * @param otherTypeInfo The type_info to compare against
     * @return true if the bare type equals otherTypeInfo
     */
    [[nodiscard]] auto bareEqualTypeInfo(
        const std::type_info& otherTypeInfo) const noexcept -> bool {
        return !isUndef() && (*mBareTypeInfo_) == otherTypeInfo;
    }

    /**
     * @brief Get the demangled name of the type
     * @return The demangled name as a string
     */
    [[nodiscard]] auto name() const noexcept -> std::string {
        return !isUndef() ? DemangleHelper::demangle(mTypeInfo_->name())
                          : "undefined";
    }

    /**
     * @brief Get the demangled name of the bare type
     * @return The demangled name of the bare type as a string
     */
    [[nodiscard]] auto bareName() const noexcept -> std::string {
        return !isUndef() ? DemangleHelper::demangle(mBareTypeInfo_->name())
                          : "undefined";
    }

    // Type property query methods
    [[nodiscard]] auto isDefaultConstructible() const noexcept -> bool {
        return mFlags_.test(IS_DEFAULT_CONSTRUCTIBLE_FLAG);
    }
    [[nodiscard]] auto isMoveable() const noexcept -> bool {
        return mFlags_.test(IS_MOVEABLE_FLAG);
    }
    [[nodiscard]] auto isCopyable() const noexcept -> bool {
        return mFlags_.test(IS_COPYABLE_FLAG);
    }
    [[nodiscard]] auto isConst() const noexcept -> bool {
        return mFlags_.test(IS_CONST_FLAG);
    }
    [[nodiscard]] auto isReference() const noexcept -> bool {
        return mFlags_.test(IS_REFERENCE_FLAG);
    }
    [[nodiscard]] auto isVoid() const noexcept -> bool {
        return mFlags_.test(IS_VOID_FLAG);
    }
    [[nodiscard]] auto isArithmetic() const noexcept -> bool {
        return mFlags_.test(IS_ARITHMETIC_FLAG);
    }
    [[nodiscard]] auto isArray() const noexcept -> bool {
        return mFlags_.test(IS_ARRAY_FLAG);
    }
    [[nodiscard]] auto isEnum() const noexcept -> bool {
        return mFlags_.test(IS_ENUM_FLAG);
    }
    [[nodiscard]] auto isClass() const noexcept -> bool {
        return mFlags_.test(IS_CLASS_FLAG);
    }
    [[nodiscard]] auto isFunction() const noexcept -> bool {
        return mFlags_.test(IS_FUNCTION_FLAG);
    }
    [[nodiscard]] auto isTrivial() const noexcept -> bool {
        return mFlags_.test(IS_TRIVIAL_FLAG);
    }
    [[nodiscard]] auto isStandardLayout() const noexcept -> bool {
        return mFlags_.test(IS_STANDARD_LAYOUT_FLAG);
    }
    [[nodiscard]] auto isPod() const noexcept -> bool {
        return mFlags_.test(IS_POD_FLAG);
    }
    [[nodiscard]] auto isPointer() const noexcept -> bool {
        return mFlags_.test(IS_POINTER_FLAG);
    }
    [[nodiscard]] auto isUndef() const noexcept -> bool {
        return mFlags_.test(IS_UNDEF_FLAG);
    }

    // C++20 additional type traits
    [[nodiscard]] auto isAggregate() const noexcept -> bool {
        return mFlags_.test(IS_AGGREGATE_FLAG);
    }
    [[nodiscard]] auto isBoundedArray() const noexcept -> bool {
        return mFlags_.test(IS_BOUNDED_ARRAY_FLAG);
    }
    [[nodiscard]] auto isUnboundedArray() const noexcept -> bool {
        return mFlags_.test(IS_UNBOUNDED_ARRAY_FLAG);
    }
    [[nodiscard]] auto isScopedEnum() const noexcept -> bool {
        return mFlags_.test(IS_SCOPED_ENUM_FLAG);
    }
    [[nodiscard]] auto isFinal() const noexcept -> bool {
        return mFlags_.test(IS_FINAL_FLAG);
    }
    [[nodiscard]] auto isAbstract() const noexcept -> bool {
        return mFlags_.test(IS_ABSTRACT_FLAG);
    }
    [[nodiscard]] auto isPolymorphic() const noexcept -> bool {
        return mFlags_.test(IS_POLYMORPHIC_FLAG);
    }
    [[nodiscard]] auto isEmpty() const noexcept -> bool {
        return mFlags_.test(IS_EMPTY_FLAG);
    }

    /**
     * @brief Get access to the bare type_info
     * @return Pointer to the bare type_info
     */
    [[nodiscard]] constexpr auto bareTypeInfo() const noexcept
        -> const std::type_info* {
        return mBareTypeInfo_;
    }

    /**
     * @brief Serialize TypeInfo to JSON format
     * @return JSON string representation
     */
    [[nodiscard]] auto toJson() const -> std::string {
        std::string result = "{\n";
        result += "  \"typeName\": \"" + name() + "\",\n";
        result += "  \"bareTypeName\": \"" + bareName() + "\",\n";
        result += "  \"traits\": {\n";

        // Build a vector of property pairs
        std::vector<std::pair<std::string, bool>> properties = {
            {"isDefaultConstructible", isDefaultConstructible()},
            {"isMoveable", isMoveable()},
            {"isCopyable", isCopyable()},
            {"isConst", isConst()},
            {"isReference", isReference()},
            {"isVoid", isVoid()},
            {"isArithmetic", isArithmetic()},
            {"isArray", isArray()},
            {"isEnum", isEnum()},
            {"isClass", isClass()},
            {"isFunction", isFunction()},
            {"isTrivial", isTrivial()},
            {"isStandardLayout", isStandardLayout()},
            {"isPod", isPod()},
            {"isPointer", isPointer()},
            {"isAggregate", isAggregate()},
            {"isBoundedArray", isBoundedArray()},
            {"isUnboundedArray", isUnboundedArray()},
            {"isScopedEnum", isScopedEnum()},
            {"isFinal", isFinal()},
            {"isAbstract", isAbstract()},
            {"isPolymorphic", isPolymorphic()},
            {"isEmpty", isEmpty()}};

        // Add properties to JSON
        for (size_t i = 0; i < properties.size(); ++i) {
            result += "    \"" + properties[i].first +
                      "\": " + (properties[i].second ? "true" : "false");
            if (i < properties.size() - 1) {
                result += ",";
            }
            result += "\n";
        }

        result += "  }\n}";
        return result;
    }

private:
    const std::type_info* mTypeInfo_ = &typeid(void);
    const std::type_info* mBareTypeInfo_ = &typeid(void);
    Flags mFlags_ = Flags().set(IS_UNDEF_FLAG);

    // Flag indices for type traits
    static constexpr unsigned int IS_CONST_FLAG = 0;
    static constexpr unsigned int IS_REFERENCE_FLAG = 1;
    static constexpr unsigned int IS_POINTER_FLAG = 2;
    static constexpr unsigned int IS_VOID_FLAG = 3;
    static constexpr unsigned int IS_ARITHMETIC_FLAG = 4;
    static constexpr unsigned int IS_UNDEF_FLAG = 5;
    static constexpr unsigned int IS_ARRAY_FLAG = 6;
    static constexpr unsigned int IS_ENUM_FLAG = 7;
    static constexpr unsigned int IS_CLASS_FLAG = 8;
    static constexpr unsigned int IS_FUNCTION_FLAG = 9;
    static constexpr unsigned int IS_TRIVIAL_FLAG = 10;
    static constexpr unsigned int IS_STANDARD_LAYOUT_FLAG = 11;
    static constexpr unsigned int IS_POD_FLAG = 12;
    static constexpr unsigned int IS_DEFAULT_CONSTRUCTIBLE_FLAG = 13;
    static constexpr unsigned int IS_MOVEABLE_FLAG = 14;
    static constexpr unsigned int IS_COPYABLE_FLAG = 15;

    // New C++20 flags
    static constexpr unsigned int IS_AGGREGATE_FLAG = 16;
    static constexpr unsigned int IS_BOUNDED_ARRAY_FLAG = 17;
    static constexpr unsigned int IS_UNBOUNDED_ARRAY_FLAG = 18;
    static constexpr unsigned int IS_SCOPED_ENUM_FLAG = 19;
    static constexpr unsigned int IS_FINAL_FLAG = 20;
    static constexpr unsigned int IS_ABSTRACT_FLAG = 21;
    static constexpr unsigned int IS_POLYMORPHIC_FLAG = 22;
    static constexpr unsigned int IS_EMPTY_FLAG = 23;
    // Flags 24-31 reserved for future expansion
};

// GetTypeInfo specializations
template <typename T>
struct GetTypeInfo {
    constexpr static auto get() noexcept -> TypeInfo {
        return TypeInfo::fromType<T>();
    }
};

// Specializations for smart pointers and references
template <typename T>
struct GetTypeInfo<std::shared_ptr<T>> {
    constexpr static auto get() noexcept -> TypeInfo {
        return TypeInfo::fromType<std::shared_ptr<T>>();
    }
};

template <typename T>
struct GetTypeInfo<std::unique_ptr<T>> {
    constexpr static auto get() noexcept -> TypeInfo {
        return TypeInfo::fromType<std::unique_ptr<T>>();
    }
};

template <typename T>
struct GetTypeInfo<std::weak_ptr<T>> {
    constexpr static auto get() noexcept -> TypeInfo {
        return TypeInfo::fromType<std::weak_ptr<T>>();
    }
};

// Added support for C++20 span
template <typename T, std::size_t Extent>
struct GetTypeInfo<std::span<T, Extent>> {
    constexpr static auto get() noexcept -> TypeInfo {
        return TypeInfo::fromType<std::span<T, Extent>>();
    }
};

// Reference specializations
template <typename T>
struct GetTypeInfo<const std::shared_ptr<T>&>
    : GetTypeInfo<std::shared_ptr<T>> {};
template <typename T>
struct GetTypeInfo<std::shared_ptr<T>&> : GetTypeInfo<std::shared_ptr<T>> {};
template <typename T>
struct GetTypeInfo<const std::unique_ptr<T>&>
    : GetTypeInfo<std::unique_ptr<T>> {};
template <typename T>
struct GetTypeInfo<std::unique_ptr<T>&> : GetTypeInfo<std::unique_ptr<T>> {};
template <typename T>
struct GetTypeInfo<const std::weak_ptr<T>&> : GetTypeInfo<std::weak_ptr<T>> {};
template <typename T>
struct GetTypeInfo<std::weak_ptr<T>&> : GetTypeInfo<std::weak_ptr<T>> {};

// Reference wrapper specialization
template <typename T>
struct GetTypeInfo<const std::reference_wrapper<T>&> {
    constexpr static auto get() noexcept -> TypeInfo {
        using BareT = BareType<T>;
        return TypeInfo::fromType<BareT>();
    }
};

/**
 * @brief Get TypeInfo for a type instance
 * @tparam T Type of the instance
 * @param t Instance to get type info for
 * @return TypeInfo for the instance
 */
template <typename T>
constexpr auto userType(const T&) noexcept -> TypeInfo {
    return GetTypeInfo<T>::get();
}

/**
 * @brief Get TypeInfo for a type
 * @tparam T Type to get information for
 * @return TypeInfo for the specified type
 */
template <typename T>
constexpr auto userType() noexcept -> TypeInfo {
    return GetTypeInfo<T>::get();
}

// Thread-safe type registry implementation
namespace detail {
class TypeRegistry {
public:
    using RegistryMap = std::unordered_map<std::string, TypeInfo>;

    // Get singleton instance
    static TypeRegistry& getInstance() {
        static TypeRegistry instance;
        return instance;
    }

    // Register a type with a name
    void registerType(std::string_view type_name, const TypeInfo& typeInfo) {
        std::unique_lock lock(mMutex);
        mRegistry[std::string(type_name)] = typeInfo;
    }

    // Get TypeInfo by name
    std::optional<TypeInfo> getTypeInfo(std::string_view type_name) const {
        std::shared_lock lock(mMutex);
        auto it = mRegistry.find(std::string(type_name));
        if (it != mRegistry.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    // Check if a type is registered
    bool isTypeRegistered(std::string_view type_name) const {
        std::shared_lock lock(mMutex);
        return mRegistry.find(std::string(type_name)) != mRegistry.end();
    }

    // Get all registered type names
    std::vector<std::string> getRegisteredTypeNames() const {
        std::shared_lock lock(mMutex);
        std::vector<std::string> names;
        names.reserve(mRegistry.size());
        for (const auto& [name, _] : mRegistry) {
            names.push_back(name);
        }
        return names;
    }

    // Clear registry (mainly for testing)
    void clear() {
        std::unique_lock lock(mMutex);
        mRegistry.clear();
    }

private:
    TypeRegistry() = default;

    mutable std::shared_mutex mMutex;
    RegistryMap mRegistry;
};

template <typename T>
struct TypeRegistrar {
    explicit TypeRegistrar(std::string_view type_name) {
        detail::TypeRegistry::getInstance().registerType(type_name,
                                                         userType<T>());
    }
};
}  // namespace detail

/**
 * @brief Register a type in the registry with its TypeInfo
 * @param type_name Name to register the type under
 * @param typeInfo TypeInfo object for the type
 * @throws TypeInfoException if type registration fails
 */
inline void registerType(std::string_view type_name, const TypeInfo& typeInfo) {
    try {
        detail::TypeRegistry::getInstance().registerType(type_name, typeInfo);
    } catch (const std::exception& e) {
        throw TypeInfoException(std::string("Failed to register type: ") +
                                e.what());
    }
}

/**
 * @brief Register a type in the registry by its template type
 * @tparam T Type to register
 * @param type_name Name to register the type under
 * @throws TypeInfoException if type registration fails
 */
template <typename T>
inline void registerType(std::string_view type_name) {
    try {
        detail::TypeRegistry::getInstance().registerType(type_name,
                                                         userType<T>());
    } catch (const std::exception& e) {
        throw TypeInfoException(std::string("Failed to register type: ") +
                                e.what());
    }
}

/**
 * @brief Get TypeInfo for a registered type
 * @param type_name Name of the type to retrieve
 * @return Optional TypeInfo that contains the type info if found
 */
inline auto getTypeInfo(std::string_view type_name) -> std::optional<TypeInfo> {
    return detail::TypeRegistry::getInstance().getTypeInfo(type_name);
}

/**
 * @brief Check if a type is registered
 * @param type_name Name of the type to check
 * @return true if the type is registered
 */
inline auto isTypeRegistered(std::string_view type_name) -> bool {
    return detail::TypeRegistry::getInstance().isTypeRegistered(type_name);
}

/**
 * @brief Get all registered type names
 * @return Vector of registered type names
 */
inline auto getRegisteredTypeNames() -> std::vector<std::string> {
    return detail::TypeRegistry::getInstance().getRegisteredTypeNames();
}

/**
 * @brief Compare two types for compatibility
 * @tparam T First type
 * @tparam U Second type
 * @return true if types are compatible (convertible)
 */
template <typename T, typename U>
constexpr bool areTypesCompatible() {
    if constexpr (std::is_same_v<BareType<T>, BareType<U>>) {
        return true;
    } else {
        return std::is_convertible_v<T, U> || std::is_convertible_v<U, T>;
    }
}

/**
 * @brief Type factory to create instances from type names
 * Creates objects of registered types using default construction
 */
class TypeFactory {
public:
    /**
     * @brief Create an instance of a registered type
     * @param type_name Name of the registered type
     * @return Shared pointer to the created instance or nullptr on failure
     */
    template <typename BaseType = void>
    static std::shared_ptr<BaseType> createInstance(
        std::string_view type_name) {
        // Implementation requires registration of factory functions
        // This is a simplified version
        static std::unordered_map<std::string,
                                  std::function<std::shared_ptr<BaseType>()>>
            factories;

        auto it = factories.find(std::string(type_name));
        if (it != factories.end()) {
            return it->second();
        }
        return nullptr;
    }

    /**
     * @brief Register a factory function for a type
     * @tparam T Type to register factory for
     * @tparam BaseType Base type for the factory
     * @param type_name Name to register under
     */
    template <typename T, typename BaseType = void>
    static void registerFactory(std::string_view type_name) {
        if constexpr (std::is_default_constructible_v<T>) {
            static std::unordered_map<
                std::string, std::function<std::shared_ptr<BaseType>()>>
                factories;
            factories[std::string(type_name)] =
                []() -> std::shared_ptr<BaseType> {
                if constexpr (std::is_convertible_v<T*, BaseType*> ||
                              std::is_void_v<BaseType>) {
                    return std::make_shared<T>();
                } else {
                    return nullptr;
                }
            };
            registerType<T>(type_name);
        }
    }
};

}  // namespace atom::meta

// Stream operator for TypeInfo
inline auto operator<<(std::ostream& oss,
                       const atom::meta::TypeInfo& typeInfo) -> std::ostream& {
    return oss << typeInfo.name();
}

// Hash specialization for TypeInfo
namespace std {
template <>
struct hash<atom::meta::TypeInfo> {
    auto operator()(const atom::meta::TypeInfo& typeInfo) const noexcept
        -> std::size_t {
        if (typeInfo.isUndef()) {
            return 0;
        }
        return std::hash<const std::type_info*>{}(typeInfo.bareTypeInfo()) ^
               (std::hash<std::string>{}(typeInfo.name()) << 2U) ^
               (std::hash<std::string>{}(typeInfo.bareName()) << 3U);
    }
};
}  // namespace std

#endif  // ATOM_META_TYPE_INFO_HPP