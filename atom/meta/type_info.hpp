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

#include "abi.hpp"
#include "concept.hpp"

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
        Flags flags;

        flags.set(IS_CONST_FLAG, std::is_const_v<std::remove_reference_t<T>>);
        flags.set(IS_REFERENCE_FLAG, std::is_reference_v<T>);
        flags.set(IS_POINTER_FLAG, Pointer<T> || Pointer<BareT> ||
                                       SmartPointer<T> || SmartPointer<BareT>);
        flags.set(IS_VOID_FLAG, std::is_void_v<T>);

        if constexpr (Pointer<T> || Pointer<BareT> || SmartPointer<T> ||
                      SmartPointer<BareT>) {
            flags.set(IS_ARITHMETIC_FLAG, K_IS_ARITHMETIC_POINTER_V<T>);
        } else {
            flags.set(IS_ARITHMETIC_FLAG, std::is_arithmetic_v<T>);
        }

        flags.set(IS_ARRAY_FLAG, std::is_array_v<T>);
        flags.set(IS_ENUM_FLAG, std::is_enum_v<T>);
        flags.set(IS_CLASS_FLAG, std::is_class_v<T>);
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
    static auto fromInstance(const T& instance [[maybe_unused]]) noexcept
        -> TypeInfo {
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
        static constexpr std::string_view template_str =
            R"({"typeName":"{}","bareTypeName":"{}","traits":{})";

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
        static std::unordered_map<std::string,
                                  std::function<std::shared_ptr<BaseType>()>>
            factories;

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
            static std::unordered_map<
                std::string, std::function<std::shared_ptr<BaseType>()>>
                factories;
            factories.emplace(type_name, []() -> std::shared_ptr<BaseType> {
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
};

}  // namespace atom::meta

inline auto operator<<(std::ostream& oss, const atom::meta::TypeInfo& typeInfo)
    -> std::ostream& {
    return oss << typeInfo.name();
}

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