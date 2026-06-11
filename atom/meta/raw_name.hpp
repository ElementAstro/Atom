/*!
 * \file raw_name.hpp
 * \brief Get raw name of a type
 * \author Max Qian <lightapt.com>
 * \date 2024-5-25
 * \copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_META_RAW_NAME_HPP
#define ATOM_META_RAW_NAME_HPP

#include <string_view>

#include "atom/macro.hpp"
#include "template_traits.hpp"

namespace atom::meta {

namespace detail {
/**
 * @brief Extract type name from compiler-specific function signature
 * @param name Function signature string
 * @return Extracted type name
 */
constexpr std::string_view extract_type_name(std::string_view name) noexcept {
#if defined(__GNUC__) && !defined(__clang__)
    // GCC formats __PRETTY_FUNCTION__ as
    //   "... raw_name_of() [with T = <name>; std::string_view =
    //   std::basic_string_view<char>]"
    // Locate the value after the first "= " inside "[with ...]" instead of
    // relying on a hard-coded prefix length (the prefix depends on the
    // enclosing function signature), and strip the alias bindings that GCC
    // appends after ';' as well as the trailing ']'.
    if (auto pos = name.find("[with "); pos != std::string_view::npos) {
        auto start = name.find("= ", pos);
        if (start != std::string_view::npos) {
            start += 2;
            auto end = name.find(';', start);
            if (end == std::string_view::npos) {
                end = name.rfind(']');
            }
            if (end != std::string_view::npos && end > start) {
                return name.substr(start, end - start);
            }
        }
    }
    return name;
#elif defined(__clang__)
    constexpr auto prefix = std::string_view("T = ");
    if (auto pos = name.find(prefix); pos != std::string_view::npos) {
        auto start = pos + prefix.size();
        auto end = name.find_last_of(']');
        if (end != std::string_view::npos && end > start) {
            return name.substr(start, end - start);
        }
    }
    return name;
#elif defined(_MSC_VER)
    constexpr auto start_marker = std::string_view("<");
    constexpr auto end_marker = std::string_view(">(");
    if (auto start_pos = name.find(start_marker);
        start_pos != std::string_view::npos) {
        start_pos += start_marker.size();
        if (auto end_pos = name.rfind(end_marker);
            end_pos != std::string_view::npos && end_pos > start_pos) {
            auto extracted = name.substr(start_pos, end_pos - start_pos);
            if (auto space_pos = extracted.find(' ');
                space_pos != std::string_view::npos) {
                return extracted.substr(space_pos + 1);
            }
            return extracted;
        }
    }
    return name;
#else
    static_assert(false, "Unsupported compiler for type name extraction");
#endif
}

/**
 * @brief Extract enum value name from compiler-specific function signature
 * @param name Function signature string
 * @return Extracted enum value name
 */
constexpr std::string_view extract_enum_name(std::string_view name) noexcept {
#if defined(__GNUC__) && !defined(__clang__)
    // First isolate the "[with auto Value = Enum::Name; ...]" value, then
    // strip the qualification. Searching the full signature for "::" would
    // hit the "std::basic_string_view<char>" alias binding GCC appends.
    auto value = extract_type_name(name);
    if (auto pos = value.rfind("::"); pos != std::string_view::npos) {
        return value.substr(pos + 2);
    }
    return value;
#elif defined(__clang__)
    if (auto pos = name.rfind("::"); pos != std::string_view::npos) {
        return name.substr(pos + 2);
    }
    return name;
#elif defined(_MSC_VER)
    constexpr auto start_marker = std::string_view("<");
    constexpr auto end_marker = std::string_view(">(");
    if (auto start_pos = name.find(start_marker);
        start_pos != std::string_view::npos) {
        start_pos += start_marker.size();
        if (auto end_pos = name.rfind(end_marker);
            end_pos != std::string_view::npos && end_pos > start_pos) {
            auto extracted = name.substr(start_pos, end_pos - start_pos);
            if (auto pos = extracted.rfind("::");
                pos != std::string_view::npos) {
                return extracted.substr(pos + 2);
            }
            return extracted;
        }
    }
    return name;
#else
    static_assert(false, "Unsupported compiler for enum name extraction");
#endif
}

#ifdef ATOM_CPP_20_SUPPORT
/**
 * @brief Extract member name from compiler-specific function signature
 * @param name Function signature string
 * @return Extracted member name
 */
constexpr std::string_view extract_member_name(std::string_view name) noexcept {
#if defined(__GNUC__) && !defined(__clang__)
    // Isolate the "[with ... = Wrapper<...>{&Class::member}; ...]" value
    // first so the search is not confused by the alias bindings GCC appends,
    // then take the identifier after the last "::" up to the closing
    // ")"/"}" of the brace initializer.
    auto value = extract_type_name(name);
    if (auto start = value.rfind("::"); start != std::string_view::npos) {
        start += 2;
        auto end = value.find_first_of(")}", start);
        if (end == std::string_view::npos) {
            end = value.size();
        }
        if (end > start) {
            return value.substr(start, end - start);
        }
    }
    return value;
#elif defined(__clang__)
    if (auto start = name.rfind('{'); start != std::string_view::npos) {
        start++;
        if (auto end = name.rfind('}');
            end != std::string_view::npos && end > start) {
            auto temp = name.substr(start, end - start);
            if (auto member_start = temp.rfind("::");
                member_start != std::string_view::npos) {
                return temp.substr(member_start + 2);
            }
            return temp;
        }
    }
    return name;
#elif defined(_MSC_VER)
    if (auto start = name.rfind("->"); start != std::string_view::npos) {
        start += 2;
        if (auto end = name.rfind(')');
            end != std::string_view::npos && end > start) {
            return name.substr(start, end - start);
        }
    }
    return name;
#else
    static_assert(false, "Unsupported compiler for member name extraction");
#endif
}
#endif
}  // namespace detail

/**
 * @brief Get raw name of a type at compile time
 * @tparam T Type to get the name of
 * @return String view containing the type name
 */
template <typename T>
constexpr std::string_view raw_name_of() noexcept {
    return detail::extract_type_name(ATOM_META_FUNCTION_NAME);
}

/**
 * @brief Get raw name of a template type at compile time
 * @tparam T Template type to get the name of
 * @return String view containing the template type name
 */
template <typename T>
constexpr std::string_view raw_name_of_template() noexcept {
    static_assert(is_template_v<T>,
                  "raw_name_of_template: Type must be a template "
                  "instantiation");
    return detail::extract_type_name(ATOM_META_FUNCTION_NAME);
}

/**
 * @brief Get raw name of a compile-time value
 * @tparam Value Compile-time value to get the name of
 * @return String view containing the value representation
 */
template <auto Value>
constexpr std::string_view raw_name_of() noexcept {
    return detail::extract_type_name(ATOM_META_FUNCTION_NAME);
}

/**
 * @brief Get raw name of an enum value at compile time
 * @tparam Value Enum value to get the name of
 * @return String view containing the enum value name
 */
template <auto Value>
constexpr std::string_view raw_name_of_enum() noexcept {
    return detail::extract_enum_name(ATOM_META_FUNCTION_NAME);
}

#ifdef ATOM_CPP_20_SUPPORT
/**
 * @brief Wrapper for member pointer values (C++20 only)
 * @tparam T Type of the wrapped value
 */
template <typename T>
struct Wrapper {
    T value;
    constexpr explicit Wrapper(T val) noexcept : value(val) {}
};

/**
 * @brief Get raw name of a member at compile time (C++20 only)
 * @tparam T Wrapped member value
 * @return String view containing the member name
 */
template <Wrapper T>
constexpr std::string_view raw_name_of_member() noexcept {
    return detail::extract_member_name(ATOM_META_FUNCTION_NAME);
}
#endif

/**
 * @brief Type alias for argument type extraction
 * @tparam T Type to extract arguments from
 */
template <typename T>
using args_type_of = typename template_traits<T>::args_type;

}  // namespace atom::meta

#endif
