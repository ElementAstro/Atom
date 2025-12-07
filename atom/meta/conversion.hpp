#ifndef ATOM_META_CONVERSION_HPP
#define ATOM_META_CONVERSION_HPP

#include <any>
#include <memory>
#include <type_traits>
#include <typeinfo>
#include <vector>
#include "atom/macro.hpp"

#if ENABLE_FASTHASH
#include "emhash/hash_table8.hpp"
#else
#include <unordered_map>
#endif

#include "atom/error/exception.hpp"
#include "type_info.hpp"

namespace atom::meta {

/**
 * @brief Exception thrown when type conversion fails
 */
class BadConversionException : public error::RuntimeError {
    using atom::error::RuntimeError::RuntimeError;
};

#define THROW_CONVERSION_ERROR(...)                              \
    throw BadConversionException(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                 ATOM_FUNC_NAME, __VA_ARGS__)

/**
 * @brief Base class for all type conversions
 */
class TypeConversionBase {
public:
    /**
     * @brief Convert from source type to target type
     * @param from The source value to convert
     * @return The converted value
     */
    ATOM_NODISCARD virtual auto convert(const std::any& from) const
        -> std::any = 0;

    /**
     * @brief Convert from target type back to source type
     * @param toAny The target value to convert back
     * @return The converted value
     */
    ATOM_NODISCARD virtual auto convertDown(const std::any& toAny) const
        -> std::any = 0;

    /**
     * @brief Get the target type information
     * @return Target type information
     */
    ATOM_NODISCARD virtual auto to() const ATOM_NOEXCEPT -> const TypeInfo& {
        return toType;
    }

    /**
     * @brief Get the source type information
     * @return Source type information
     */
    ATOM_NODISCARD virtual auto from() const ATOM_NOEXCEPT -> const TypeInfo& {
        return fromType;
    }

    ATOM_NODISCARD auto getFromType() const ATOM_NOEXCEPT -> const TypeInfo& {
        return fromType;
    }

    ATOM_NODISCARD auto getToType() const ATOM_NOEXCEPT -> const TypeInfo& {
        return toType;
    }

    /**
     * @brief Check if this conversion is bidirectional
     * @return true if bidirectional, false otherwise
     */
    ATOM_NODISCARD virtual auto bidir() const ATOM_NOEXCEPT -> bool {
        return true;
    }

    virtual ~TypeConversionBase() = default;

    TypeConversionBase(const TypeConversionBase&) = default;
    TypeConversionBase& operator=(const TypeConversionBase&) = default;
    TypeConversionBase(TypeConversionBase&&) = default;
    TypeConversionBase& operator=(TypeConversionBase&&) = default;

protected:
    TypeConversionBase(const TypeInfo& toTypeInfo, const TypeInfo& fromTypeInfo)
        : toType(toTypeInfo), fromType(fromTypeInfo) {}

    TypeInfo toType;
    TypeInfo fromType;
};

/**
 * @brief Static conversion implementation for compile-time type casting
 */
template <typename From, typename To>
class StaticConversion : public TypeConversionBase {
public:
    StaticConversion() : TypeConversionBase(userType<To>(), userType<From>()) {}

    ATOM_NODISCARD auto convert(const std::any& from) const
        -> std::any override {
        try {
            if constexpr (std::is_pointer_v<From> && std::is_pointer_v<To>) {
                auto fromPtr = std::any_cast<From>(from);
                return std::any(static_cast<To>(fromPtr));
            } else if constexpr (std::is_reference_v<From> &&
                                 std::is_reference_v<To>) {
                using FromBare = std::remove_reference_t<From>;
                auto* fromPtr =
                    std::any_cast<FromBare>(&const_cast<std::any&>(from));
                if (!fromPtr) {
                    THROW_CONVERSION_ERROR("Failed to convert reference types");
                }
                return std::any(std::ref(
                    static_cast<std::remove_reference_t<To>&>(*fromPtr)));
            } else {
                THROW_CONVERSION_ERROR("Failed to convert ", fromType.name(),
                                       " to ", toType.name());
            }
        } catch (const std::bad_cast&) {
            THROW_CONVERSION_ERROR("Failed to convert ", fromType.name(),
                                   " to ", toType.name());
        }
    }

    ATOM_NODISCARD auto convertDown(const std::any& toAny) const
        -> std::any override {
        try {
            if constexpr (std::is_pointer_v<From> && std::is_pointer_v<To>) {
                auto toPtr = std::any_cast<To>(toAny);
                return std::any(static_cast<From>(toPtr));
            } else if constexpr (std::is_reference_v<From> &&
                                 std::is_reference_v<To>) {
                using ToBare = std::remove_reference_t<To>;
                auto* toPtr =
                    std::any_cast<ToBare>(&const_cast<std::any&>(toAny));
                if (!toPtr) {
                    throw std::bad_cast();
                }
                return std::any(std::ref(
                    static_cast<std::remove_reference_t<From>&>(*toPtr)));
            } else {
                THROW_CONVERSION_ERROR("Failed to convert ", toType.name(),
                                       " to ", fromType.name());
            }
        } catch (const std::bad_cast&) {
            THROW_CONVERSION_ERROR("Failed to convert ", toType.name(), " to ",
                                   fromType.name());
        }
    }
};

/**
 * @brief Dynamic conversion implementation for runtime type casting
 */
template <typename From, typename To>
class DynamicConversion : public TypeConversionBase {
public:
    DynamicConversion()
        : TypeConversionBase(userType<To>(), userType<From>()) {}

    ATOM_NODISCARD auto convert(const std::any& from) const
        -> std::any override {
        if constexpr (std::is_pointer_v<From> && std::is_pointer_v<To>) {
            auto fromPtr = std::any_cast<From>(from);
            auto convertedPtr = dynamic_cast<To>(fromPtr);
            if (!convertedPtr && fromPtr != nullptr) {
                throw std::bad_cast();
            }
            return std::any(convertedPtr);
        } else if constexpr (std::is_reference_v<From> &&
                             std::is_reference_v<To>) {
            try {
                auto& fromRef = std::any_cast<From&>(from);
                return std::any(dynamic_cast<To&>(fromRef));
            } catch (const std::bad_cast&) {
                THROW_CONVERSION_ERROR("Failed to convert ", fromType.name(),
                                       " to ", toType.name());
            }
        } else {
            THROW_CONVERSION_ERROR("Failed to convert ", fromType.name(),
                                   " to ", toType.name());
        }
    }

    ATOM_NODISCARD auto convertDown(const std::any& toAny) const
        -> std::any override {
        if constexpr (std::is_pointer_v<From> && std::is_pointer_v<To>) {
            auto toPtr = std::any_cast<To>(toAny);
            auto convertedPtr = dynamic_cast<From>(toPtr);
            if (!convertedPtr && toPtr != nullptr) {
                throw std::bad_cast();
            }
            return std::any(convertedPtr);
        } else if constexpr (std::is_reference_v<From> &&
                             std::is_reference_v<To>) {
            try {
                using ToBare = std::remove_reference_t<To>;
                auto* toPtr =
                    std::any_cast<ToBare>(&const_cast<std::any&>(toAny));
                if (!toPtr) {
                    throw std::bad_cast();
                }
                return std::any(std::ref(
                    dynamic_cast<std::remove_reference_t<From>&>(*toPtr)));
            } catch (const std::bad_cast&) {
                THROW_CONVERSION_ERROR("Failed to convert ", toType.name(),
                                       " to ", fromType.name());
            }
        } else {
            THROW_CONVERSION_ERROR("Failed to convert ", toType.name(), " to ",
                                   fromType.name());
        }
    }
};

/**
 * @brief Helper function to create base class conversion
 */
template <typename Base, typename Derived>
auto baseClass() -> std::shared_ptr<TypeConversionBase> {
    if constexpr (std::is_polymorphic_v<Base> &&
                  std::is_polymorphic_v<Derived>) {
        return std::make_shared<DynamicConversion<Derived*, Base*>>();
    } else {
        return std::make_shared<StaticConversion<Derived, Base>>();
    }
}

/**
 * @brief Specialized conversion for std::vector types
 */
template <typename From, typename To>
class VectorConversion : public TypeConversionBase {
public:
    VectorConversion()
        : TypeConversionBase(userType<std::vector<To>>(),
                             userType<std::vector<From>>()) {}

    [[nodiscard]] auto convert(const std::any& from) const
        -> std::any override {
        try {
            const auto& fromVec = std::any_cast<const std::vector<From>&>(from);
            std::vector<To> toVec;
            toVec.reserve(fromVec.size());

            for (const auto& elem : fromVec) {
                auto convertedElem =
                    std::dynamic_pointer_cast<typename To::element_type>(elem);
                if (!convertedElem) {
                    throw std::bad_cast();
                }
                toVec.push_back(convertedElem);
            }

            return std::any(toVec);
        } catch (const std::bad_any_cast&) {
            THROW_CONVERSION_ERROR("Failed to convert ", fromType.name(),
                                   " to ", toType.name());
        }
    }

    ATOM_NODISCARD auto convertDown(const std::any& toAny) const
        -> std::any override {
        try {
            const auto& toVec = std::any_cast<const std::vector<To>&>(toAny);
            std::vector<From> fromVec;
            fromVec.reserve(toVec.size());

            for (const auto& elem : toVec) {
                auto convertedElem =
                    std::dynamic_pointer_cast<typename From::element_type>(
                        elem);
                if (!convertedElem) {
                    throw std::bad_cast();
                }
                fromVec.push_back(convertedElem);
            }

            return std::any(fromVec);
        } catch (const std::bad_any_cast&) {
            THROW_CONVERSION_ERROR("Failed to convert ", toType.name(), " to ",
                                   fromType.name());
        }
    }
};

/**
 * @brief Specialized conversion for map types
 */
template <template <typename...> class MapType, typename K1, typename V1,
          typename K2, typename V2>
class MapConversion : public TypeConversionBase {
public:
    MapConversion()
        : TypeConversionBase(userType<MapType<K2, V2>>(),
                             userType<MapType<K1, V1>>()) {}

    [[nodiscard]] auto convert(const std::any& from) const
        -> std::any override {
        try {
            const auto& fromMap = std::any_cast<const MapType<K1, V1>&>(from);
            MapType<K2, V2> toMap;

            for (const auto& [key, value] : fromMap) {
                K2 convertedKey = static_cast<K2>(key);
                V2 convertedValue =
                    std::dynamic_pointer_cast<typename V2::element_type>(value);
                if (!convertedValue) {
                    THROW_CONVERSION_ERROR("Failed to convert value in map");
                }
                toMap.emplace(convertedKey, convertedValue);
            }

            return std::any(toMap);
        } catch (const std::bad_any_cast&) {
            THROW_CONVERSION_ERROR("Failed to convert ", fromType.name(),
                                   " to ", toType.name());
        }
    }

    ATOM_NODISCARD auto convertDown(const std::any& toAny) const
        -> std::any override {
        try {
            const auto& toMap = std::any_cast<const MapType<K2, V2>&>(toAny);
            MapType<K1, V1> fromMap;

            for (const auto& [key, value] : toMap) {
                K1 convertedKey = static_cast<K1>(key);
                V1 convertedValue =
                    std::dynamic_pointer_cast<typename V1::element_type>(value);
                if (!convertedValue) {
                    THROW_CONVERSION_ERROR("Failed to convert value in map");
                }
                fromMap.emplace(convertedKey, convertedValue);
            }

            return std::any(fromMap);
        } catch (const std::bad_any_cast&) {
            THROW_CONVERSION_ERROR("Failed to convert ", toType.name(), " to ",
                                   fromType.name());
        }
    }
};

/**
 * @brief Specialized conversion for sequence types
 */
template <template <typename...> class SeqType, typename From, typename To>
class SequenceConversion : public TypeConversionBase {
public:
    SequenceConversion()
        : TypeConversionBase(userType<SeqType<To>>(),
                             userType<SeqType<From>>()) {}

    [[nodiscard]] auto convert(const std::any& from) const
        -> std::any override {
        try {
            const auto& fromSeq = std::any_cast<const SeqType<From>&>(from);
            SeqType<To> toSeq;

            for (const auto& elem : fromSeq) {
                auto convertedElem =
                    std::dynamic_pointer_cast<typename To::element_type>(elem);
                if (!convertedElem) {
                    throw std::bad_cast();
                }
                toSeq.push_back(convertedElem);
            }

            return std::any(toSeq);
        } catch (const std::bad_any_cast&) {
            THROW_CONVERSION_ERROR("Failed to convert ", fromType.name(),
                                   " to ", toType.name());
        }
    }

    ATOM_NODISCARD auto convertDown(const std::any& toAny) const
        -> std::any override {
        try {
            const auto& toSeq = std::any_cast<const SeqType<To>&>(toAny);
            SeqType<From> fromSeq;

            for (const auto& elem : toSeq) {
                auto convertedElem =
                    std::dynamic_pointer_cast<typename From::element_type>(
                        elem);
                if (!convertedElem) {
                    throw std::bad_cast();
                }
                fromSeq.push_back(convertedElem);
            }

            return std::any(fromSeq);
        } catch (const std::bad_any_cast&) {
            THROW_CONVERSION_ERROR("Failed to convert ", toType.name(), " to ",
                                   fromType.name());
        }
    }
};

/**
 * @brief Specialized conversion for set types
 */
template <template <typename...> class SetType, typename From, typename To>
class SetConversion : public TypeConversionBase {
public:
    SetConversion()
        : TypeConversionBase(userType<SetType<To>>(),
                             userType<SetType<From>>()) {}

    [[nodiscard]] auto convert(const std::any& from) const
        -> std::any override {
        try {
            const auto& fromSet = std::any_cast<const SetType<From>&>(from);
            SetType<To> toSet;

            for (const auto& elem : fromSet) {
                auto convertedElem =
                    std::dynamic_pointer_cast<typename To::element_type>(elem);
                if (!convertedElem) {
                    throw std::bad_cast();
                }
                toSet.insert(convertedElem);
            }

            return std::any(toSet);
        } catch (const std::bad_any_cast&) {
            THROW_CONVERSION_ERROR("Failed to convert ", fromType.name(),
                                   " to ", toType.name());
        }
    }

    ATOM_NODISCARD auto convertDown(const std::any& toAny) const
        -> std::any override {
        try {
            const auto& toSet = std::any_cast<const SetType<To>&>(toAny);
            SetType<From> fromSet;

            for (const auto& elem : toSet) {
                auto convertedElem =
                    std::dynamic_pointer_cast<typename From::element_type>(
                        elem);
                if (!convertedElem) {
                    throw std::bad_cast();
                }
                fromSet.insert(convertedElem);
            }

            return std::any(fromSet);
        } catch (const std::bad_any_cast&) {
            THROW_CONVERSION_ERROR("Failed to convert ", toType.name(), " to ",
                                   fromType.name());
        }
    }
};

/**
 * @brief Type conversion registry and manager
 */
class TypeConversions {
public:
    TypeConversions() = default;

    /**
     * @brief Create a shared instance of TypeConversions
     * @return Shared pointer to TypeConversions instance
     */
    static auto createShared() -> std::shared_ptr<TypeConversions> {
        return std::make_shared<TypeConversions>();
    }

    /**
     * @brief Add a type conversion to the registry
     * @param conversion The conversion to add
     */
    void addConversion(const std::shared_ptr<TypeConversionBase>& conversion) {
        auto key = conversion->getFromType();
        conversions_[key].push_back(conversion);
    }

    /**
     * @brief Convert from one type to another with explicit type parameters
     * @tparam To Target type
     * @tparam From Source type
     * @param from The value to convert
     * @return The converted value
     */
    template <typename To, typename From>
    [[nodiscard]] auto convert(const std::any& from) const -> std::any {
        auto fromType = userType<From>();
        auto toType = userType<To>();

        auto it = conversions_.find(fromType);
        if (it != conversions_.end()) {
            for (const auto& conv : it->second) {
                if (conv->getToType() == toType) {
                    try {
                        return conv->convert(from);
                    } catch (const std::bad_any_cast& e) {
                        THROW_CONVERSION_ERROR("Failed to convert from ",
                                               fromType.name(), " to ",
                                               toType.name(), ": ", e.what());
                    }
                }
            }
        }
        THROW_CONVERSION_ERROR("No conversion found from ", fromType.name(),
                               " to ", toType.name());
    }

    /**
     * @brief Convert to a specific type with automatic source type deduction
     * @tparam To Target type
     * @param from The value to convert
     * @return The converted value
     */
    template <typename To>
    [[nodiscard]] auto convertTo(const std::any& from) const -> std::any {
        TypeInfo toType = userType<To>();

        for (const auto& [fromType, convList] : conversions_) {
            for (const auto& conv : convList) {
                if (conv->getToType() == toType) {
                    try {
                        return conv->convert(from);
                    } catch (const std::bad_any_cast&) {
                        continue;
                    } catch (const BadConversionException&) {
                        continue;
                    }
                }
            }
        }

        THROW_CONVERSION_ERROR("No conversion found from any type to ",
                               toType.name());
    }

    /**
     * @brief Check if conversion is possible between two types
     * @param fromTypeInfo Source type information
     * @param toTypeInfo Target type information
     * @return true if conversion is possible, false otherwise
     */
    [[nodiscard]] auto canConvert(const TypeInfo& fromTypeInfo,
                                  const TypeInfo& toTypeInfo) const -> bool {
        auto it = conversions_.find(fromTypeInfo);
        if (it != conversions_.end()) {
            for (const auto& conv : it->second) {
                if (conv->to() == toTypeInfo) {
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * @brief Add base class conversion for inheritance hierarchies
     * @tparam Base Base class type
     * @tparam Derived Derived class type
     */
    template <typename Base, typename Derived>
    void addBaseClass() {
        addConversion(std::make_shared<DynamicConversion<Derived*, Base*>>());

        if constexpr (!std::is_same_v<Base, Derived>) {
            addConversion(std::make_shared<StaticConversion<Derived, Base>>());
        }
    }

    /**
     * @brief Add map type conversion
     */
    template <template <typename...> class MapType, typename K1, typename V1,
              typename K2, typename V2>
    void addMapConversion() {
        addConversion(
            std::make_shared<MapConversion<MapType, K1, V1, K2, V2>>());
    }

    /**
     * @brief Add vector type conversion
     */
    template <typename From, typename To>
    void addVectorConversion() {
        addConversion(
            std::make_shared<VectorConversion<std::shared_ptr<From>,
                                              std::shared_ptr<To>>>());
    }

    /**
     * @brief Add sequence type conversion
     */
    template <template <typename...> class SeqType, typename From, typename To>
    void addSequenceConversion() {
        addConversion(
            std::make_shared<SequenceConversion<SeqType, std::shared_ptr<From>,
                                                std::shared_ptr<To>>>());
    }

    /**
     * @brief Add set type conversion
     */
    template <template <typename...> class SetType, typename From, typename To>
    void addSetConversion() {
        addConversion(
            std::make_shared<SetConversion<SetType, std::shared_ptr<From>,
                                           std::shared_ptr<To>>>());
    }

private:
#if ENABLE_FASTHASH
    emhash8::HashMap<TypeInfo, std::vector<std::shared_ptr<TypeConversionBase>>,
                     std::hash<TypeInfo>>
        conversions_;
#else
    std::unordered_map<TypeInfo,
                       std::vector<std::shared_ptr<TypeConversionBase>>,
                       std::hash<TypeInfo>>
        conversions_;
#endif
};

//==============================================================================
// C++23 Enhanced Conversion Utilities
//==============================================================================

/**
 * @brief Concept for convertible types
 */
template <typename From, typename To>
concept Convertible =
    std::is_convertible_v<From, To> || requires(From f) { static_cast<To>(f); };

/**
 * @brief Concept for implicitly convertible types
 */
template <typename From, typename To>
concept ImplicitlyConvertible = std::is_convertible_v<From, To>;

/**
 * @brief Concept for types with custom conversion
 */
template <typename T>
concept HasCustomConversion = requires(const T& t) {
    { t.convert() };
};

/**
 * @brief Safe conversion with optional result
 */
template <typename To, typename From>
auto safeConvert(const From& value) -> std::optional<To> {
    if constexpr (std::is_convertible_v<From, To>) {
        return static_cast<To>(value);
    } else {
        return std::nullopt;
    }
}

/**
 * @brief Conversion chain for multi-step conversions
 */
template <typename... Types>
class ConversionChain;

template <typename First, typename Second>
class ConversionChain<First, Second> {
public:
    static auto convert(const First& value) -> std::optional<Second> {
        return safeConvert<Second>(value);
    }
};

template <typename First, typename Second, typename... Rest>
class ConversionChain<First, Second, Rest...> {
public:
    static auto convert(const First& value)
        -> std::optional<
            typename ConversionChain<Second, Rest...>::result_type> {
        if (auto mid = safeConvert<Second>(value)) {
            return ConversionChain<Second, Rest...>::convert(*mid);
        }
        return std::nullopt;
    }

    using result_type = typename ConversionChain<Second, Rest...>::result_type;
};

/**
 * @brief Type-safe conversion registry entry
 */
template <typename From, typename To>
struct ConversionEntry {
    using source_type = From;
    using target_type = To;
    std::function<To(const From&)> converter;
    bool is_bidirectional = false;
    std::function<From(const To&)> reverse_converter;
};

/**
 * @brief Fluent conversion builder
 */
template <typename From>
class ConversionBuilder {
    From value_;

public:
    explicit ConversionBuilder(From value) : value_(std::move(value)) {}

    template <typename To>
    auto to() const -> std::optional<To> {
        return safeConvert<To>(value_);
    }

    template <typename To>
    auto toOrDefault(To default_value) const -> To {
        if (auto result = safeConvert<To>(value_)) {
            return *result;
        }
        return default_value;
    }

    template <typename To>
    auto toOrThrow() const -> To {
        if (auto result = safeConvert<To>(value_)) {
            return *result;
        }
        THROW_CONVERSION_ERROR("Conversion failed");
    }
};

/**
 * @brief Create a conversion builder
 */
template <typename T>
auto convert(T&& value) {
    return ConversionBuilder<std::decay_t<T>>(std::forward<T>(value));
}

/**
 * @brief Check if conversion is registered
 */
template <typename From, typename To>
bool isConversionRegistered(const TypeConversions& conversions) {
    auto from_info = userType<From>();
    auto to_info = userType<To>();
    return conversions.canConvert(from_info, to_info);
}

/**
 * @brief Automatic conversion detector
 */
template <typename From, typename To>
struct ConversionDetector {
    static constexpr bool is_implicit = std::is_convertible_v<From, To>;
    static constexpr bool is_explicit =
        std::is_constructible_v<To, From> && !is_implicit;
    static constexpr bool is_static_castable =
        requires(From f) { static_cast<To>(f); };
    static constexpr bool is_reinterpret_castable =
        std::is_pointer_v<From> && std::is_pointer_v<To>;
    static constexpr bool is_any_convertible =
        is_implicit || is_explicit || is_static_castable;
};

}  // namespace atom::meta

#endif  // ATOM_META_CONVERSION_HPP
