/*!
 * \file refl_field.hpp
 * \brief Shared field descriptor base for reflection-based serialization
 *
 * Common base for the JSON (refl_json.hpp) and YAML (refl_yaml.hpp) field
 * descriptors: name/member binding, required/default handling, validation
 * and introspection metadata live here exactly once.
 */

#ifndef ATOM_META_REFL_FIELD_HPP
#define ATOM_META_REFL_FIELD_HPP

#include <functional>
#include <utility>

namespace atom::meta {

/*!
 * \brief Field descriptor shared by all reflection serializers
 * \tparam T Reflected class type
 * \tparam MemberType Type of the bound member
 *
 * Builder methods use C++23 deducing this so chaining from a derived field
 * type (e.g. the JSON field with withJsonKey) keeps the derived type.
 */
template <typename T, typename MemberType>
struct FieldBase {
    using ReflectedType = T;
    using member_type = MemberType;
    using Validator = std::function<bool(const MemberType&)>;

    const char* name;
    MemberType T::* member;
    bool required = true;
    MemberType default_value{};
    Validator validator;

    // Introspection metadata
    const char* description = nullptr;
    bool deprecated = false;
    int version = 1;  ///< Field version for migration support

    FieldBase(const char* n, MemberType T::* m, bool r = true,
              MemberType def = {}, Validator v = nullptr)
        : name(n),
          member(m),
          required(r),
          default_value(std::move(def)),
          validator(std::move(v)) {}

    template <typename Self>
    auto&& withDescription(this Self&& self, const char* desc) {
        self.description = desc;
        return std::forward<Self>(self);
    }

    template <typename Self>
    auto&& withDeprecated(this Self&& self, bool dep = true) {
        self.deprecated = dep;
        return std::forward<Self>(self);
    }

    template <typename Self>
    auto&& withVersion(this Self&& self, int ver) {
        self.version = ver;
        return std::forward<Self>(self);
    }

    /*!
     * \brief Run the validator, if any
     * \return True when no validator is set or the validator accepts
     */
    [[nodiscard]] bool validate(const MemberType& value) const {
        return !validator || validator(value);
    }
};

}  // namespace atom::meta

#endif  // ATOM_META_REFL_FIELD_HPP
