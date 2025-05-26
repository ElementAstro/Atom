/*!
 * \file type_caster.hpp
 * \brief Enhanced type caster with advanced features: type inference, aliasing,
 * multi-stage conversion, and logging.
 * \author Max Qian <lightapt.com>
 * \date 2023-04-05
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_TYPE_CASTER_HPP
#define ATOM_META_TYPE_CASTER_HPP

#include <any>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "atom/error/exception.hpp"
#include "type_info.hpp"

namespace atom::meta {

namespace detail {
/*!
 * \brief Gets the global type registry.
 * \return Reference to the global type registry.
 */
inline auto getTypeRegistry() -> std::unordered_map<std::string, TypeInfo>& {
    static std::unordered_map<std::string, TypeInfo> registry;
    return registry;
}
}  // namespace detail

/*!
 * \class TypeCaster
 * \brief A class that provides type casting functionality with support for type
 * inference, aliasing, multi-stage conversion, and logging.
 */
class TypeCaster {
public:
    using ConvertFunc = std::function<std::any(const std::any&)>;
    using ConvertMap = std::unordered_map<TypeInfo, ConvertFunc>;

    /*!
     * \brief Constructor that registers built-in types.
     */
    TypeCaster() { registerBuiltinTypes(); }

    /*!
     * \brief Creates a shared pointer to a new TypeCaster instance.
     * \return A shared pointer to a TypeCaster instance.
     */
    static auto createShared() -> std::shared_ptr<TypeCaster> {
        return std::make_shared<TypeCaster>();
    }

    /*!
     * \brief Converts an input of any type to the specified destination type.
     * \tparam DestinationType The type to convert to.
     * \param input The input value to be converted.
     * \return The converted value.
     * \throws std::invalid_argument if the source type is not found.
     */
    template <typename DestinationType>
    auto convert(const std::any& input) const -> std::any {
        TypeInfo destInfo;
        std::optional<TypeInfo> srcInfo;
        {
            std::shared_lock type_lock(type_mutex_);
            srcInfo = getTypeInfo(input.type().name());
            destInfo = userType<DestinationType>();
        }

        if (!srcInfo.has_value()) {
            THROW_INVALID_ARGUMENT("Source type not found: " +
                                   std::string(input.type().name()));
        }

        if (srcInfo.value() == destInfo) {
            return input;
        }

        std::vector<ConvertFunc> conversions;
        {
            std::shared_lock convLock(conversion_mutex_);
            auto path = findShortestConversionPath(srcInfo.value(), destInfo);
            conversions.reserve(path.size() - 1);
            for (size_t i = 0; i < path.size() - 1; ++i) {
                auto srcIt = conversions_.find(path[i]);
                if (srcIt != conversions_.end()) {
                    auto destIt = srcIt->second.find(path[i + 1]);
                    if (destIt != srcIt->second.end()) {
                        conversions.push_back(destIt->second);
                    }
                }
            }
        }

        std::any result = input;
        for (const auto& converter : conversions) {
            result = converter(result);
        }
        return result;
    }

    /*!
     * \brief Registers a conversion function between two types.
     * \tparam SourceType The source type.
     * \tparam DestinationType The destination type.
     * \param func The conversion function.
     * \throws std::invalid_argument if the source and destination types are the
     * same.
     */
    template <typename SourceType, typename DestinationType>
    void registerConversion(ConvertFunc func) {
        auto srcInfo = userType<SourceType>();
        auto destInfo = userType<DestinationType>();

        if (srcInfo == destInfo) {
            THROW_INVALID_ARGUMENT(
                "Source and destination types must be different.");
        }

        {
            std::unique_lock typeLock(type_mutex_);
            registerTypeInternal<SourceType>(srcInfo.bareName());
            registerTypeInternal<DestinationType>(destInfo.bareName());
        }

        {
            std::unique_lock convLock(conversion_mutex_);
            conversions_[srcInfo][destInfo] = std::move(func);
            clearCache();
        }
    }

    /*!
     * \brief Registers an alias for a type.
     * \tparam T The type to alias.
     * \param alias The alias name.
     */
    template <typename T>
    void registerAlias(const std::string& alias) {
        std::unique_lock typeLock(type_mutex_);
        auto typeInfo = userType<T>();
        type_alias_map_[alias] = typeInfo;
        type_name_map_[alias] = typeInfo;
    }

    /*!
     * \brief Registers a group of types under a common group name.
     * \param groupName The name of the group.
     * \param types The list of type names to group.
     */
    void registerTypeGroup(const std::string& groupName,
                           const std::vector<std::string>& types) {
        std::unique_lock typeLock(type_mutex_);
        for (const auto& typeName : types) {
            type_group_map_[typeName] = groupName;
        }
    }

    /*!
     * \brief Registers a multi-stage conversion function.
     * \tparam IntermediateType The intermediate type.
     * \tparam SourceType The source type.
     * \tparam DestinationType The destination type.
     * \param func1 The first stage conversion function.
     * \param func2 The second stage conversion function.
     */
    template <typename IntermediateType, typename SourceType,
              typename DestinationType>
    void registerMultiStageConversion(ConvertFunc func1, ConvertFunc func2) {
        registerConversion<SourceType, IntermediateType>(std::move(func1));
        registerConversion<IntermediateType, DestinationType>(std::move(func2));
    }

    /*!
     * \brief Checks if a conversion exists between two types.
     * \param src The source type.
     * \param dst The destination type.
     * \return True if a conversion exists, false otherwise.
     */
    auto hasConversion(TypeInfo src, TypeInfo dst) const -> bool {
        std::shared_lock convLock(conversion_mutex_);
        auto srcIt = conversions_.find(src);
        return srcIt != conversions_.end() &&
               srcIt->second.find(dst) != srcIt->second.end();
    }

    /*!
     * \brief Gets a list of registered types.
     * \return A vector of registered type names.
     */
    auto getRegisteredTypes() const -> std::vector<std::string> {
        std::shared_lock typeLock(type_mutex_);
        std::vector<std::string> typeNames;
        typeNames.reserve(type_name_map_.size());
        for (const auto& [name, info] : type_name_map_) {
            typeNames.push_back(name);
        }
        return typeNames;
    }

    /*!
     * \brief Registers a type with a given name.
     * \tparam T The type to register.
     * \param name The name to register the type with.
     */
    template <typename T>
    void registerType(const std::string& name) {
        std::unique_lock typeLock(type_mutex_);
        registerTypeInternal<T>(name);
    }

    /*!
     * \brief Registers an enum value with a string mapping.
     * \tparam EnumType The enum type.
     * \param enum_name The name of the enum.
     * \param string_value The string value of the enum.
     * \param enum_value The corresponding enum value.
     */
    template <typename EnumType>
    void registerEnumValue(const std::string& enum_name,
                           const std::string& string_value,
                           EnumType enum_value) {
        std::unique_lock enumLock(enum_mutex_);
        if (!m_enumMaps_.contains(enum_name)) {
            m_enumMaps_[enum_name] =
                std::unordered_map<std::string, EnumType>();
        }

        auto& enumMap =
            std::any_cast<std::unordered_map<std::string, EnumType>&>(
                m_enumMaps_[enum_name]);
        enumMap[string_value] = enum_value;
    }

    /*!
     * \brief Converts an enum value to its string representation.
     * \tparam EnumType The enum type.
     * \param value The enum value to convert.
     * \param enum_name The name of the enum.
     * \return The string representation of the enum value.
     * \throws std::invalid_argument if the enum value is invalid.
     */
    template <typename EnumType>
    auto enumToString(EnumType value, const std::string& enum_name)
        -> std::string {
        std::shared_lock enumLock(enum_mutex_);
        const auto& enumMap = getEnumMap<EnumType>(enum_name);
        for (const auto& [key, enumValue] : enumMap) {
            if (enumValue == value) {
                return key;
            }
        }
        THROW_INVALID_ARGUMENT("Invalid enum value for enum: " + enum_name);
    }

    /*!
     * \brief Converts a string to its corresponding enum value.
     * \tparam EnumType The enum type.
     * \param string_value The string value to convert.
     * \param enum_name The name of the enum.
     * \return The corresponding enum value.
     * \throws std::invalid_argument if the string value is invalid.
     */
    template <typename EnumType>
    auto stringToEnum(const std::string& string_value,
                      const std::string& enum_name) -> EnumType {
        std::shared_lock enumLock(enum_mutex_);
        const auto& enumMap = getEnumMap<EnumType>(enum_name);
        auto iterator = enumMap.find(string_value);
        if (iterator != enumMap.end()) {
            return iterator->second;
        }
        THROW_INVALID_ARGUMENT("Invalid enum string '" + string_value +
                               "' for enum: " + enum_name);
    }

private:
    std::unordered_map<TypeInfo, ConvertMap> conversions_;
    mutable std::unordered_map<std::string, std::vector<TypeInfo>>
        conversion_paths_cache_;
    std::unordered_map<std::string, TypeInfo> type_name_map_;
    std::unordered_map<std::string, TypeInfo> type_alias_map_;
    std::unordered_map<std::string, std::string> type_group_map_;
    std::unordered_map<std::string, std::any> m_enumMaps_;

    mutable std::shared_mutex type_mutex_;
    mutable std::shared_mutex conversion_mutex_;
    mutable std::shared_mutex enum_mutex_;

    /*!
     * \brief Registers built-in types.
     */
    void registerBuiltinTypes() {
        registerTypeInternal<std::size_t>("size_t");
        registerTypeInternal<int>("int");
        registerTypeInternal<long>("long");
        registerTypeInternal<long long>("long long");
        registerTypeInternal<float>("float");
        registerTypeInternal<double>("double");
        registerTypeInternal<char>("char");
        registerTypeInternal<unsigned char>("unsigned char");
        registerTypeInternal<char*>("char *");
        registerTypeInternal<const char*>("const char*");
        registerTypeInternal<std::string>("std::string");
        registerTypeInternal<std::string_view>("std::string_view");
        registerTypeInternal<bool>("bool");
    }

    /*!
     * \brief Internal type registration without locking.
     * \tparam T The type to register.
     * \param name The name to register the type with.
     */
    template <typename T>
    void registerTypeInternal(const std::string& name) {
        auto type_info = userType<T>();
        type_name_map_[name] = type_info;
        type_name_map_[typeid(T).name()] = type_info;
        detail::getTypeRegistry()[typeid(T).name()] = type_info;
    }

    /*!
     * \brief Finds the shortest conversion path between two types.
     * \param src The source type.
     * \param dst The destination type.
     * \return A vector of TypeInfo representing the conversion path.
     * \throws std::runtime_error if no conversion path is found.
     */
    auto findShortestConversionPath(TypeInfo src, TypeInfo dst) const
        -> std::vector<TypeInfo> {
        std::string cacheKey = makeCacheKey(src, dst);

        if (auto it = conversion_paths_cache_.find(cacheKey);
            it != conversion_paths_cache_.end()) {
            return it->second;
        }

        auto path = findPath(src, dst);
        conversion_paths_cache_[cacheKey] = path;
        return path;
    }

    /*!
     * \brief Finds a conversion path between two types using BFS.
     * \param src The source type information.
     * \param dst The destination type information.
     * \return A vector of TypeInfo objects representing the conversion path.
     * \throws std::runtime_error If no conversion path is found.
     */
    auto findPath(TypeInfo src, TypeInfo dst) const -> std::vector<TypeInfo> {
        std::queue<std::vector<TypeInfo>> paths;
        std::unordered_set<TypeInfo> visited;

        paths.push({src});
        visited.insert(src);

        while (!paths.empty()) {
            auto currentPath = paths.front();
            paths.pop();
            auto last = currentPath.back();

            if (last == dst) {
                return currentPath;
            }

            if (auto it = conversions_.find(last); it != conversions_.end()) {
                for (const auto& [next_type, _] : it->second) {
                    if (visited.insert(next_type).second) {
                        auto newPath = currentPath;
                        newPath.push_back(next_type);
                        paths.push(std::move(newPath));
                    }
                }
            }
        }

        THROW_RUNTIME_ERROR("No conversion path found from " + src.bareName() +
                            " to " + dst.bareName());
    }

    /*!
     * \brief Creates a cache key for conversion paths.
     * \param src The source type.
     * \param dst The destination type.
     * \return A string representing the cache key.
     */
    static auto makeCacheKey(TypeInfo src, TypeInfo dst) -> std::string {
        return src.bareName() + "->" + dst.bareName();
    }

    /*!
     * \brief Clears the conversion paths cache.
     */
    void clearCache() { conversion_paths_cache_.clear(); }

    /*!
     * \brief Gets the TypeInfo for a given type name.
     * \param name The name of the type.
     * \return An optional TypeInfo object.
     */
    static auto getTypeInfo(const std::string& name)
        -> std::optional<TypeInfo> {
        auto& registry = detail::getTypeRegistry();
        if (auto iterator = registry.find(name); iterator != registry.end()) {
            return iterator->second;
        }
        return std::nullopt;
    }

    /*!
     * \brief Gets the enum map for a specific enum type.
     * \tparam EnumType The enum type.
     * \param enum_name The name of the enum.
     * \return Reference to the enum map.
     */
    template <typename EnumType>
    auto getEnumMap(const std::string& enum_name) const
        -> const std::unordered_map<std::string, EnumType>& {
        return std::any_cast<const std::unordered_map<std::string, EnumType>&>(
            m_enumMaps_.at(enum_name));
    }
};

}  // namespace atom::meta

#endif  // ATOM_META_TYPE_CASTER_HPP
