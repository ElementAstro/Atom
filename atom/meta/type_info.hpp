/*!
 * \file type_info.hpp
 * \brief Enhanced TypeInfo for better type handling with C++20/23 support
 * \author Max Qian <lightapt.com> with enhancements
 * \date 2025-03-13
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_TYPE_INFO_HPP
#define ATOM_META_TYPE_INFO_HPP

#include <bitset>
#include <concepts>
#include <cstdlib>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <shared_mutex>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <vector>
#include <version>

#include "abi.hpp"
#include "concept.hpp"

// C++23 feature detection
#if __cpp_lib_expected >= 202202L
#include <expected>
#define ATOM_TYPEINFO_HAS_EXPECTED 1
#else
#define ATOM_TYPEINFO_HAS_EXPECTED 0
#endif

namespace atom::meta {

constexpr std::size_t K_FLAG_BITSET_SIZE = 32;

template <typename T>
using BareType =
    std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>;

template <typename T>
concept TypeInfoCompatible = requires {
    { typeid(T) } -> std::convertible_to<const std::type_info&>;
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

/**
 * @brief Compile time deduced information about a type
 */
class TypeInfo {
public:
    using Flags = std::bitset<K_FLAG_BITSET_SIZE>;

    /**
     * @brief Construct a new Type Info object
     */
    constexpr TypeInfo(Flags flags, const std::type_info* typeInfo,
                       const std::type_info* bareTypeInfo) noexcept
        : mTypeInfo_(typeInfo), mBareTypeInfo_(bareTypeInfo), mFlags_(flags) {}

    constexpr TypeInfo() noexcept = default;
    constexpr TypeInfo(TypeInfo&& other) noexcept = default;
    constexpr TypeInfo& operator=(TypeInfo&& other) noexcept = default;
    constexpr TypeInfo(const TypeInfo& other) noexcept = default;
    constexpr TypeInfo& operator=(const TypeInfo& other) noexcept = default;

    /**
     * @brief Create TypeInfo from a type
     * @tparam T The type to create information for
     * @return TypeInfo object containing information about T
     */
    template <TypeInfoCompatible T>
    static constexpr auto fromType() noexcept -> TypeInfo {
        using BareT = BareType<T>;
        using NoCvRefT = std::remove_cvref_t<T>;
        // Pointer-like covers raw pointers, smart pointers and std::span,
        // including when accessed through (const) references.
        constexpr bool kPointerLike =
            requires { typename PointerType<NoCvRefT>::type; };
        Flags flags;

        flags.set(IS_CONST_FLAG, std::is_const_v<std::remove_reference_t<T>>);
        flags.set(IS_REFERENCE_FLAG, std::is_reference_v<T>);
        flags.set(IS_POINTER_FLAG, Pointer<T> || Pointer<BareT> ||
                                       SmartPointer<T> || SmartPointer<BareT> ||
                                       kPointerLike);
        flags.set(IS_VOID_FLAG, std::is_void_v<T>);

        if constexpr (kPointerLike) {
            flags.set(IS_ARITHMETIC_FLAG,
                      std::is_arithmetic_v<typename PointerType<NoCvRefT>::type>);
        } else {
            flags.set(IS_ARITHMETIC_FLAG, std::is_arithmetic_v<T>);
        }

        flags.set(IS_ARRAY_FLAG, std::is_array_v<T>);
        flags.set(IS_ENUM_FLAG, std::is_enum_v<T>);
        flags.set(IS_CLASS_FLAG, std::is_class_v<NoCvRefT>);
        flags.set(IS_FUNCTION_FLAG, std::is_function_v<T>);
        flags.set(IS_TRIVIAL_FLAG, std::is_trivial_v<T>);
        flags.set(IS_STANDARD_LAYOUT_FLAG, std::is_standard_layout_v<T>);
        flags.set(IS_POD_FLAG,
                  std::is_trivial_v<T> && std::is_standard_layout_v<T>);
        flags.set(IS_DEFAULT_CONSTRUCTIBLE_FLAG,
                  std::is_default_constructible_v<T>);
        flags.set(IS_MOVEABLE_FLAG, std::is_move_constructible_v<T>);
        flags.set(IS_COPYABLE_FLAG, std::is_copy_constructible_v<T>);
        flags.set(IS_AGGREGATE_FLAG, std::is_aggregate_v<T>);
        flags.set(IS_BOUNDED_ARRAY_FLAG, std::is_bounded_array_v<T>);
        flags.set(IS_UNBOUNDED_ARRAY_FLAG, std::is_unbounded_array_v<T>);
        // C++20 compatible scoped enum detection
        if constexpr (std::is_enum_v<T>) {
            flags.set(IS_SCOPED_ENUM_FLAG,
                      !std::is_convertible_v<T, std::underlying_type_t<T>>);
        } else {
            flags.set(IS_SCOPED_ENUM_FLAG, false);
        }
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
    static auto fromInstance(T&& instance
                             [[maybe_unused]]) noexcept -> TypeInfo {
        // Forwarding reference keeps const/reference qualifiers of the
        // argument (e.g. `const Foo&` yields isConst() && isReference()).
        return fromType<T>();
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
     * @brief Serialize TypeInfo to JSON format (optimized version)
     * @return JSON string representation
     */
    [[nodiscard]] auto toJson() const -> std::string {
        // Removed unused template_str placeholder to silence -Wunused warnings.

        std::string traits;
        traits.reserve(512);

        constexpr std::array<std::pair<std::string_view, unsigned int>, 23>
            properties = {
                {{"isDefaultConstructible", IS_DEFAULT_CONSTRUCTIBLE_FLAG},
                 {"isMoveable", IS_MOVEABLE_FLAG},
                 {"isCopyable", IS_COPYABLE_FLAG},
                 {"isConst", IS_CONST_FLAG},
                 {"isReference", IS_REFERENCE_FLAG},
                 {"isVoid", IS_VOID_FLAG},
                 {"isArithmetic", IS_ARITHMETIC_FLAG},
                 {"isArray", IS_ARRAY_FLAG},
                 {"isEnum", IS_ENUM_FLAG},
                 {"isClass", IS_CLASS_FLAG},
                 {"isFunction", IS_FUNCTION_FLAG},
                 {"isTrivial", IS_TRIVIAL_FLAG},
                 {"isStandardLayout", IS_STANDARD_LAYOUT_FLAG},
                 {"isPod", IS_POD_FLAG},
                 {"isPointer", IS_POINTER_FLAG},
                 {"isAggregate", IS_AGGREGATE_FLAG},
                 {"isBoundedArray", IS_BOUNDED_ARRAY_FLAG},
                 {"isUnboundedArray", IS_UNBOUNDED_ARRAY_FLAG},
                 {"isScopedEnum", IS_SCOPED_ENUM_FLAG},
                 {"isFinal", IS_FINAL_FLAG},
                 {"isAbstract", IS_ABSTRACT_FLAG},
                 {"isPolymorphic", IS_POLYMORPHIC_FLAG},
                 {"isEmpty", IS_EMPTY_FLAG}}};

        for (size_t i = 0; i < properties.size(); ++i) {
            traits += "\"";
            traits += properties[i].first;
            traits += "\":";
            traits += mFlags_.test(properties[i].second) ? "true" : "false";
            if (i < properties.size() - 1) {
                traits += ",";
            }
        }

        std::string result;
        result.reserve(name().size() + bareName().size() + traits.size() + 64);
        result = "{\"typeName\":\"" + name() + "\",\"bareTypeName\":\"" +
                 bareName() + "\",\"traits\":{" + traits + "}}";

        return result;
    }

    template <typename T>
    static constexpr auto create() noexcept -> TypeInfo {
        return fromType<T>();
    }

private:
    const std::type_info* mTypeInfo_ = &typeid(void);
    const std::type_info* mBareTypeInfo_ = &typeid(void);
    Flags mFlags_ = Flags().set(IS_UNDEF_FLAG);

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
    static constexpr unsigned int IS_AGGREGATE_FLAG = 16;
    static constexpr unsigned int IS_BOUNDED_ARRAY_FLAG = 17;
    static constexpr unsigned int IS_UNBOUNDED_ARRAY_FLAG = 18;
    static constexpr unsigned int IS_SCOPED_ENUM_FLAG = 19;
    static constexpr unsigned int IS_FINAL_FLAG = 20;
    static constexpr unsigned int IS_ABSTRACT_FLAG = 21;
    static constexpr unsigned int IS_POLYMORPHIC_FLAG = 22;
    static constexpr unsigned int IS_EMPTY_FLAG = 23;

    static_assert(IS_EMPTY_FLAG < K_FLAG_BITSET_SIZE,
                  "Flag index exceeds the bitset capacity; grow "
                  "K_FLAG_BITSET_SIZE before adding more flags");
};

template <typename T>
struct GetTypeInfo {
    static constexpr auto get() noexcept -> TypeInfo {
        return TypeInfo::fromType<T>();
    }
};

template <typename T>
struct GetTypeInfo<std::shared_ptr<T>> {
    static constexpr auto get() noexcept -> TypeInfo {
        return TypeInfo::fromType<std::shared_ptr<T>>();
    }
};

template <typename T>
struct GetTypeInfo<std::unique_ptr<T>> {
    static constexpr auto get() noexcept -> TypeInfo {
        return TypeInfo::fromType<std::unique_ptr<T>>();
    }
};

template <typename T>
struct GetTypeInfo<std::weak_ptr<T>> {
    static constexpr auto get() noexcept -> TypeInfo {
        return TypeInfo::fromType<std::weak_ptr<T>>();
    }
};

template <typename T, std::size_t Extent>
struct GetTypeInfo<std::span<T, Extent>> {
    static constexpr auto get() noexcept -> TypeInfo {
        return TypeInfo::fromType<std::span<T, Extent>>();
    }
};

// Note: (const) references to smart pointers are intentionally handled by the
// primary template so that const/reference qualifiers are reflected in the
// flags while the pointer-like nature is still detected via PointerType.

template <typename T>
struct GetTypeInfo<const std::reference_wrapper<T>&> {
    static constexpr auto get() noexcept -> TypeInfo {
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

namespace detail {
/**
 * @brief Thread-safe type registry implementation
 */
class TypeRegistry {
public:
    using RegistryMap = std::unordered_map<std::string, TypeInfo>;

    static TypeRegistry& getInstance() {
        static TypeRegistry instance;
        return instance;
    }

    void registerType(std::string_view type_name, const TypeInfo& typeInfo) {
        std::unique_lock lock(mMutex);
        mRegistry.emplace(type_name, typeInfo);
    }

    std::optional<TypeInfo> getTypeInfo(std::string_view type_name) const {
        std::shared_lock lock(mMutex);
        if (auto it = mRegistry.find(std::string(type_name));
            it != mRegistry.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    bool isTypeRegistered(std::string_view type_name) const {
        std::shared_lock lock(mMutex);
        return mRegistry.contains(std::string(type_name));
    }

    std::vector<std::string> getRegisteredTypeNames() const {
        std::shared_lock lock(mMutex);
        std::vector<std::string> names;
        names.reserve(mRegistry.size());
        for (const auto& [name, _] : mRegistry) {
            names.push_back(name);
        }
        return names;
    }

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
 * @brief Check if a type is derived from another
 * @tparam Derived The potential derived type
 * @tparam Base The potential base type
 * @return true if Derived is derived from Base
 */
template <typename Derived, typename Base>
constexpr bool isDerivedFrom() {
    return std::is_base_of_v<Base, Derived>;
}

/**
 * @brief Check if a type has a specific member function (compile-time)
 * @tparam T The type to check
 * @tparam Signature The expected function signature
 */
template <typename T, typename Signature>
constexpr bool hasMethod() {
    return std::is_member_function_pointer_v<Signature T::*>;
}

/**
 * @brief Type relationship information
 */
enum class TypeRelationship {
    Same,         ///< Types are exactly the same
    Convertible,  ///< First type is convertible to second
    BaseOf,       ///< First type is base of second
    DerivedFrom,  ///< First type is derived from second
    Unrelated     ///< Types have no direct relationship
};

/**
 * @brief Get the relationship between two types
 * @tparam T First type
 * @tparam U Second type
 * @return TypeRelationship indicating how the types are related
 */
template <typename T, typename U>
constexpr TypeRelationship getTypeRelationship() {
    if constexpr (std::is_same_v<T, U>) {
        return TypeRelationship::Same;
    } else if constexpr (std::is_base_of_v<T, U>) {
        return TypeRelationship::BaseOf;
    } else if constexpr (std::is_base_of_v<U, T>) {
        return TypeRelationship::DerivedFrom;
    } else if constexpr (std::is_convertible_v<T, U>) {
        return TypeRelationship::Convertible;
    } else {
        return TypeRelationship::Unrelated;
    }
}

/**
 * @brief Get type relationship as string
 * @param rel The type relationship
 * @return String representation
 */
inline constexpr std::string_view typeRelationshipToString(
    TypeRelationship rel) noexcept {
    switch (rel) {
        case TypeRelationship::Same:
            return "Same";
        case TypeRelationship::Convertible:
            return "Convertible";
        case TypeRelationship::BaseOf:
            return "BaseOf";
        case TypeRelationship::DerivedFrom:
            return "DerivedFrom";
        case TypeRelationship::Unrelated:
            return "Unrelated";
        default:
            return "Unknown";
    }
}

/**
 * @brief Extended type information with additional C++23 features
 */
template <typename T>
struct ExtendedTypeInfo {
    static constexpr TypeInfo info = TypeInfo::fromType<T>();
    static constexpr bool is_trivially_relocatable =
        std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>;
    static constexpr bool is_nothrow_swappable = std::is_nothrow_swappable_v<T>;
    static constexpr bool is_nothrow_hashable = requires(const T& t) {
        { std::hash<T>{}(t) } noexcept;
    };
    static constexpr bool has_virtual_destructor =
        std::has_virtual_destructor_v<T>;
    static constexpr std::size_t type_size = sizeof(T);
    static constexpr std::size_t type_alignment = alignof(T);

    // Type category detection
    static constexpr bool is_scalar = std::is_scalar_v<T>;
    static constexpr bool is_compound = std::is_compound_v<T>;
    static constexpr bool is_fundamental = std::is_fundamental_v<T>;
    static constexpr bool is_object = std::is_object_v<T>;

    /**
     * @brief Get a formatted string with all type information
     */
    static auto toString() -> std::string {
        return std::format(
            "Type: {}\n"
            "  Size: {} bytes\n"
            "  Alignment: {} bytes\n"
            "  Trivially Relocatable: {}\n"
            "  Nothrow Swappable: {}\n"
            "  Has Virtual Destructor: {}\n"
            "  Is Scalar: {}\n"
            "  Is Fundamental: {}\n",
            info.name(), type_size, type_alignment, is_trivially_relocatable,
            is_nothrow_swappable, has_virtual_destructor, is_scalar,
            is_fundamental);
    }
};

#if ATOM_TYPEINFO_HAS_EXPECTED
/**
 * @brief Try to get TypeInfo with error handling using std::expected (C++23)
 * @param type_name Name of the type to retrieve
 * @return Expected containing TypeInfo or error string
 */
inline auto tryGetTypeInfo(std::string_view type_name)
    -> std::expected<TypeInfo, std::string> {
    if (auto info =
            detail::TypeRegistry::getInstance().getTypeInfo(type_name)) {
        return *info;
    }
    return std::unexpected(
        std::format("Type '{}' not found in registry", type_name));
}
#endif

/**
 * @brief Type factory to create instances from type names
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
        auto& factories = getFactories<BaseType>();
        if (auto it = factories.find(std::string(type_name));
            it != factories.end()) {
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
            getFactories<BaseType>().emplace(
                std::string(type_name), []() -> std::shared_ptr<BaseType> {
                    if constexpr (std::is_convertible_v<T*, BaseType*> ||
                                  std::is_void_v<BaseType>) {
                        return std::make_shared<T>();
                    } else {
                        return nullptr;
                    }
                });
            registerType<T>(type_name);
        }
    }

private:
    /**
     * @brief Shared factory map per BaseType so that registerFactory and
     * createInstance operate on the same storage.
     */
    template <typename BaseType>
    static auto getFactories() -> std::unordered_map<
        std::string, std::function<std::shared_ptr<BaseType>()>>& {
        static std::unordered_map<std::string,
                                  std::function<std::shared_ptr<BaseType>()>>
            factories;
        return factories;
    }
};

/**
 * @brief Macro to register a type with automatic name deduction
 */
#define ATOM_REGISTER_TYPE(Type) atom::meta::registerType<Type>(#Type)

/**
 * @brief Macro to register a type with custom name
 */
#define ATOM_REGISTER_TYPE_AS(Type, Name) atom::meta::registerType<Type>(Name)

/**
 * @brief Concept for types that can be registered in the type registry
 */
template <typename T>
concept Registrable = requires {
    { typeid(T) } -> std::convertible_to<const std::type_info&>;
    requires !std::is_void_v<T>;
};

/**
 * @brief Register multiple types at once
 */
template <Registrable... Types>
inline void registerTypes(
    const std::array<std::string_view, sizeof...(Types)>& names) {
    std::size_t i = 0;
    (registerType<Types>(names[i++]), ...);
}

//==============================================================================
// Type Info Integration with Other Meta Components
//==============================================================================

/**
 * @brief Get demangled type name using abi.hpp integration
 */
template <Demanglable T>
auto getDemangledTypeName() -> std::string {
    return std::string(DemangleHelper::demangle(typeid(T).name()));
}

/**
 * @brief Get bare type name without qualifiers using abi.hpp
 */
template <Demanglable T>
auto getBareTypeName() -> std::string {
    return std::string(DemangleHelper::getBareTypeName(typeid(T).name()));
}

/**
 * @brief Extract namespace from type using abi.hpp
 */
template <Demanglable T>
auto getTypeNamespace() -> std::string_view {
    return DemangleHelper::extractNamespace(getDemangledTypeName<T>());
}

/**
 * @brief Get type category using abi.hpp
 */
template <Demanglable T>
auto getTypeCategory() -> std::string {
    return std::string(DemangleHelper::getTypeCategory<T>());
}

/**
 * @brief Type info builder for fluent API
 */
class TypeInfoBuilder {
    TypeInfo info_;
    std::unordered_map<std::string, std::string> metadata_;

public:
    template <TypeInfoCompatible T>
    static TypeInfoBuilder create() {
        TypeInfoBuilder builder;
        builder.info_ = TypeInfo::fromType<T>();
        return builder;
    }

    TypeInfoBuilder& withMetadata(std::string key, std::string value) {
        metadata_[std::move(key)] = std::move(value);
        return *this;
    }

    TypeInfoBuilder& markAsRegistered() {
        // Register the type if not already registered
        return *this;
    }

    [[nodiscard]] TypeInfo build() const { return info_; }

    [[nodiscard]] const auto& getMetadata() const { return metadata_; }
};

/**
 * @brief Type comparison utilities
 */
struct TypeComparator {
    /**
     * @brief Compare two TypeInfo objects
     */
    static constexpr int compare(const TypeInfo& a, const TypeInfo& b) {
        if (a == b)
            return 0;
        return a.name() < b.name() ? -1 : 1;
    }

    /**
     * @brief Check if types are related
     */
    template <typename T, typename U>
    static constexpr bool areRelated() {
        return getTypeRelationship<T, U>() != TypeRelationship::Unrelated;
    }

    /**
     * @brief Get common base type if exists
     */
    template <typename T, typename U>
    static constexpr bool haveCommonBase() {
        return std::is_base_of_v<T, U> || std::is_base_of_v<U, T>;
    }
};

/**
 * @brief Type trait collection for a type
 */
template <typename T>
struct TypeTraitCollection {
    // Basic traits
    static constexpr bool is_void = std::is_void_v<T>;
    static constexpr bool is_null_pointer = std::is_null_pointer_v<T>;
    static constexpr bool is_integral = std::is_integral_v<T>;
    static constexpr bool is_floating_point = std::is_floating_point_v<T>;
    static constexpr bool is_array = std::is_array_v<T>;
    static constexpr bool is_enum = std::is_enum_v<T>;
    static constexpr bool is_union = std::is_union_v<T>;
    static constexpr bool is_class = std::is_class_v<T>;
    static constexpr bool is_function = std::is_function_v<T>;
    static constexpr bool is_pointer = std::is_pointer_v<T>;
    static constexpr bool is_lvalue_reference = std::is_lvalue_reference_v<T>;
    static constexpr bool is_rvalue_reference = std::is_rvalue_reference_v<T>;
    static constexpr bool is_member_pointer = std::is_member_pointer_v<T>;

    // Composite traits
    static constexpr bool is_arithmetic = std::is_arithmetic_v<T>;
    static constexpr bool is_fundamental = std::is_fundamental_v<T>;
    static constexpr bool is_scalar = std::is_scalar_v<T>;
    static constexpr bool is_object = std::is_object_v<T>;
    static constexpr bool is_compound = std::is_compound_v<T>;
    static constexpr bool is_reference = std::is_reference_v<T>;
    static constexpr bool is_member_function_pointer =
        std::is_member_function_pointer_v<T>;

    // Type properties
    static constexpr bool is_const = std::is_const_v<T>;
    static constexpr bool is_volatile = std::is_volatile_v<T>;
    static constexpr bool is_trivial = std::is_trivial_v<T>;
    static constexpr bool is_trivially_copyable =
        std::is_trivially_copyable_v<T>;
    static constexpr bool is_standard_layout = std::is_standard_layout_v<T>;
    static constexpr bool is_empty = std::is_empty_v<T>;
    static constexpr bool is_polymorphic = std::is_polymorphic_v<T>;
    static constexpr bool is_abstract = std::is_abstract_v<T>;
    static constexpr bool is_final = std::is_final_v<T>;
    static constexpr bool is_aggregate = std::is_aggregate_v<T>;

    // Constructibility
    static constexpr bool is_default_constructible =
        std::is_default_constructible_v<T>;
    static constexpr bool is_copy_constructible =
        std::is_copy_constructible_v<T>;
    static constexpr bool is_move_constructible =
        std::is_move_constructible_v<T>;
    static constexpr bool is_copy_assignable = std::is_copy_assignable_v<T>;
    static constexpr bool is_move_assignable = std::is_move_assignable_v<T>;
    static constexpr bool is_destructible = std::is_destructible_v<T>;

    /**
     * @brief Get a summary string of all traits
     */
    static auto summary() -> std::string {
        std::string result =
            std::format("Type Traits for {}:\n", getDemangledTypeName<T>());
        result += std::format("  Fundamental: {}, Scalar: {}, Object: {}\n",
                              is_fundamental, is_scalar, is_object);
        result +=
            std::format("  Trivial: {}, Standard Layout: {}, Aggregate: {}\n",
                        is_trivial, is_standard_layout, is_aggregate);
        result += std::format(
            "  Default Constructible: {}, Copy Constructible: {}, Move "
            "Constructible: {}\n",
            is_default_constructible, is_copy_constructible,
            is_move_constructible);
        return result;
    }
};

/**
 * @brief Visitor pattern for TypeInfo
 */
template <typename Visitor>
auto visitTypeInfo(const TypeInfo& info, Visitor&& visitor) {
    return std::forward<Visitor>(visitor)(info);
}

/**
 * @brief Transform TypeInfo with a function
 */
template <typename Transform>
auto transformTypeName(const TypeInfo& info,
                       Transform&& transform) -> std::string {
    return std::forward<Transform>(transform)(info.name());
}

}  // namespace atom::meta

inline auto operator<<(std::ostream& oss,
                       const atom::meta::TypeInfo& typeInfo) -> std::ostream& {
    return oss << typeInfo.name();
}

/**
 * @brief std::format support for TypeInfo (C++20)
 */
template <>
struct std::formatter<atom::meta::TypeInfo> : std::formatter<std::string> {
    auto format(const atom::meta::TypeInfo& typeInfo,
                std::format_context& ctx) const {
        return std::formatter<std::string>::format(typeInfo.name(), ctx);
    }
};

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

#endif
