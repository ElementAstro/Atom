#ifndef ATOM_META_REFL_JSON_HPP
#define ATOM_META_REFL_JSON_HPP

// Enhanced JSON reflection with performance optimizations and new features

#include <functional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "atom/error/exception.hpp"
#include "atom/meta/refl_field.hpp"
#include "atom/type/json.hpp"
using json = nlohmann::json;

namespace atom::meta {
// JSON field descriptor: shared base plus JSON-specific key mapping and
// value transformers
template <typename T, typename MemberType>
struct Field : FieldBase<T, MemberType> {
    using Base = FieldBase<T, MemberType>;
    using typename Base::Validator;
    using Transformer = std::function<MemberType(const MemberType&)>;

    Transformer serializer;    // Transform value before serialization
    Transformer deserializer;  // Transform value after deserialization
    const char* json_key = nullptr;  // Custom JSON key (if different from name)

    Field(const char* n, MemberType T::* m, bool r = true, MemberType def = {},
          Validator v = nullptr, Transformer ser = nullptr,
          Transformer deser = nullptr)
        : Base(n, m, r, std::move(def), std::move(v)),
          serializer(std::move(ser)),
          deserializer(std::move(deser)) {}

    Field& withJsonKey(const char* key) {
        json_key = key;
        return *this;
    }

    // Get effective JSON key
    [[nodiscard]] const char* getJsonKey() const noexcept {
        return json_key ? json_key : this->name;
    }
};

// Reflectable class template
template <typename T, typename... Fields>
struct Reflectable {
    using ReflectedType = T;
    std::tuple<Fields...> fields;

    explicit Reflectable(Fields... flds) : fields(flds...) {}

    [[nodiscard]] auto from_json(const json& j, int target_version = 1) const -> T {
        T obj;
        std::apply(
            [&](const auto&... field) {
                (([&] {
                     const char* json_key = field.getJsonKey();

                     // Enhanced: Version-aware deserialization
                     if (field.version > target_version && field.deprecated) {
                         return;  // Skip deprecated fields for older versions
                     }

                     if (j.contains(json_key)) {
                         auto value = j.at(json_key)
                                          .template get<std::remove_cvref_t<
                                              decltype(obj.*(field.member))>>();

                         // Enhanced: Apply deserializer transformation
                         if (field.deserializer) {
                             value = field.deserializer(value);
                         }

                         obj.*(field.member) = std::move(value);

                         // Enhanced: Validation with better error messages
                         if (field.validator && !field.validator(obj.*(field.member))) {
                             THROW_INVALID_ARGUMENT(
                                 std::string("Validation failed for field '") + field.name +
                                 "': " + (field.description ? field.description : "no description"));
                         }
                     } else if (!field.required) {
                         obj.*(field.member) = field.default_value;
                     } else {
                         THROW_MISSING_ARGUMENT(
                             std::string("Missing required field '") + field.name +
                             "' (JSON key: '" + json_key + "')");
                     }
                 }()),
                 ...);
            },
            fields);
        return obj;
    }

    [[nodiscard]] auto to_json(const T& obj, bool include_deprecated = false,
                               bool include_metadata = false) const -> json {
        json j;
        std::apply(
            [&](const auto&... field) {
                (([&] {
                     // Enhanced: Skip deprecated fields unless explicitly requested
                     if (field.deprecated && !include_deprecated) {
                         return;
                     }

                     const char* json_key = field.getJsonKey();
                     auto value = obj.*(field.member);

                     // Enhanced: Apply serializer transformation
                     if (field.serializer) {
                         value = field.serializer(value);
                     }

                     j[json_key] = value;

                     // Enhanced: Include metadata if requested
                     if (include_metadata) {
                         json metadata;
                         if (field.description) {
                             metadata["description"] = field.description;
                         }
                         metadata["required"] = field.required;
                         metadata["deprecated"] = field.deprecated;
                         metadata["version"] = field.version;

                         j["__metadata__"][field.name] = metadata;
                     }
                 }()),
                 ...);
            },
            fields);
        return j;
    }

    // Enhanced: Validation method
    [[nodiscard]] auto validate(const T& obj) const -> std::vector<std::string> {
        std::vector<std::string> errors;
        std::apply(
            [&](const auto&... field) {
                (([&] {
                     if (field.validator && !field.validator(obj.*(field.member))) {
                         errors.emplace_back(std::string("Validation failed for field '") +
                                           field.name + "': " +
                                           (field.description ? field.description : "no description"));
                     }
                 }()),
                 ...);
            },
            fields);
        return errors;
    }

    // Enhanced: Get schema information
    [[nodiscard]] auto get_schema() const -> json {
        json schema;
        schema["type"] = "object";
        schema["properties"] = json::object();
        schema["required"] = json::array();

        std::apply(
            [&](const auto&... field) {
                (([&] {
                     const char* json_key = field.getJsonKey();
                     json field_schema;

                     if (field.description) {
                         field_schema["description"] = field.description;
                     }
                     field_schema["deprecated"] = field.deprecated;
                     field_schema["version"] = field.version;

                     schema["properties"][json_key] = field_schema;

                     if (field.required) {
                         schema["required"].push_back(json_key);
                     }
                 }()),
                 ...);
            },
            fields);
        return schema;
    }
};

// Enhanced field creation functions
template <typename T, typename MemberType>
auto make_field(const char* name, MemberType T::* member, bool required = true,
                MemberType default_value = {},
                typename Field<T, MemberType>::Validator validator = nullptr,
                typename Field<T, MemberType>::Transformer serializer = nullptr,
                typename Field<T, MemberType>::Transformer deserializer = nullptr)
    -> Field<T, MemberType> {
    return Field<T, MemberType>(name, member, required, default_value,
                                validator, serializer, deserializer);
}

// Enhanced: Simplified field creation with builder pattern
template <typename T, typename MemberType>
auto field(const char* name, MemberType T::* member) -> Field<T, MemberType> {
    return Field<T, MemberType>(name, member);
}

// Enhanced: Required field shorthand
template <typename T, typename MemberType>
auto required_field(const char* name, MemberType T::* member) -> Field<T, MemberType> {
    return Field<T, MemberType>(name, member, true);
}

// Enhanced: Optional field shorthand
template <typename T, typename MemberType>
auto optional_field(const char* name, MemberType T::* member,
                   MemberType default_value = {}) -> Field<T, MemberType> {
    return Field<T, MemberType>(name, member, false, default_value);
}

// Enhanced: Deprecated field shorthand
template <typename T, typename MemberType>
auto deprecated_field(const char* name, MemberType T::* member,
                     MemberType default_value = {}) -> Field<T, MemberType> {
    return Field<T, MemberType>(name, member, false, default_value)
        .withDeprecated(true);
}
}  // namespace atom::meta

#endif
