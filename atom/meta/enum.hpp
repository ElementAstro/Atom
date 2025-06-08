/*!
 * \file enum.hpp
 * \brief Enhanced Enum Utilities with Comprehensive Features
 * \author Max Qian <lightapt.com>
 * \date 2023-03-29
 * \copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef ATOM_META_ENUM_HPP
#define ATOM_META_ENUM_HPP

#include <algorithm>
#include <array>
#include <cassert>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace atom::meta {

// **C++20 concept support**
#if __cplusplus >= 202002L
template <typename T>
concept EnumerationType = std::is_enum_v<T>;
#endif

// **Enhanced EnumTraits base structure**
template <typename T>
struct EnumTraits {
    static_assert(std::is_enum_v<T>, "T must be an enum type");

    using enum_type = T;
    using underlying_type = std::underlying_type_t<T>;

    // **Core data arrays - need specialization**
    static constexpr std::array<T, 0> values{};
    static constexpr std::array<std::string_view, 0> names{};
    static constexpr std::array<std::string_view, 0> descriptions{};
    static constexpr std::array<std::string_view, 0> aliases{};

    // **Metadata properties**
    static constexpr bool is_flags = false;
    static constexpr bool is_sequential = false;
    static constexpr bool is_continuous = false;
    static constexpr T default_value{};
    static constexpr std::string_view type_name = "Unknown";
    static constexpr std::string_view type_description = "";

    // **Value range information**
    static constexpr underlying_type min_value() noexcept {
        if constexpr (values.size() > 0) {
            underlying_type min_val = static_cast<underlying_type>(values[0]);
            for (const auto& val : values) {
                auto int_val = static_cast<underlying_type>(val);
                if (int_val < min_val)
                    min_val = int_val;
            }
            return min_val;
        }
        return 0;
    }

    static constexpr underlying_type max_value() noexcept {
        if constexpr (values.size() > 0) {
            underlying_type max_val = static_cast<underlying_type>(values[0]);
            for (const auto& val : values) {
                auto int_val = static_cast<underlying_type>(val);
                if (int_val > max_val)
                    max_val = int_val;
            }
            return max_val;
        }
        return 0;
    }

    static constexpr size_t size() noexcept { return values.size(); }
    static constexpr bool empty() noexcept { return values.size() == 0; }

    // **Check if value is a valid enum value**
    static constexpr bool contains(T value) noexcept {
        for (const auto& val : values) {
            if (val == value)
                return true;
        }
        return false;
    }
};

// **Automatic enum name extraction**
namespace detail {
template <typename T, T Value>
constexpr std::string_view extract_enum_name() noexcept {
#if defined(__clang__) || defined(__GNUC__)
    constexpr std::string_view name = __PRETTY_FUNCTION__;
    constexpr std::string_view prefix = "Value = ";
    constexpr char suffix = ']';
#elif defined(_MSC_VER)
    constexpr std::string_view name = __FUNCSIG__;
    constexpr std::string_view prefix = "extract_enum_name<";
    constexpr char suffix = '>';
#else
    return "UNKNOWN";
#endif

    const auto prefix_pos = name.find(prefix);
    if (prefix_pos == std::string_view::npos) {
        return "UNKNOWN";
    }

    const auto start = prefix_pos + prefix.size();
    const auto end = name.find(suffix, start);

    if (start >= end) {
        return "UNKNOWN";
    }

    auto result = name.substr(start, end - start);

    // **Remove namespace prefix**
    const auto last_colon = result.find_last_of(':');
    if (last_colon != std::string_view::npos &&
        last_colon + 1 < result.size()) {
        result = result.substr(last_colon + 1);
    }

    // **Check if it is a valid enum name**
    if (result.empty() || result.find("UNKNOWN") != std::string_view::npos ||
        result.find('(') != std::string_view::npos ||
        (result[0] >= '0' && result[0] <= '9')) {
        return "";
    }

    return result;
}

template <typename T, T Value>
constexpr bool is_valid_enum_value() noexcept {
    constexpr auto name = extract_enum_name<T, Value>();
    return !name.empty();
}

// **Fast lookup optimization - constexpr hash table**
template <typename T>
class EnumLookupTable {
private:
    static constexpr size_t calculate_table_size() noexcept {
        constexpr size_t count = EnumTraits<T>::size();
        return count > 0 ? count * 2 + 1 : 1;  // Load factor about 0.5
    }

    static constexpr size_t table_size = calculate_table_size();

    struct Entry {
        std::string_view key{};
        T value{};
        bool occupied = false;
    };

    std::array<Entry, table_size> table_{};

    constexpr size_t hash(std::string_view str) const noexcept {
        size_t result = 5381;
        for (char c : str) {
            result = ((result << 5) + result) + static_cast<size_t>(c);
        }
        return result % table_size;
    }

public:
    constexpr EnumLookupTable() noexcept {
        if constexpr (EnumTraits<T>::size() > 0) {
            const auto& names = EnumTraits<T>::names;
            const auto& values = EnumTraits<T>::values;

            for (size_t i = 0; i < names.size(); ++i) {
                size_t index = hash(names[i]);
                while (table_[index].occupied) {
                    index = (index + 1) % table_size;
                }
                table_[index] = {names[i], values[i], true};
            }
        }
    }

    constexpr std::optional<T> find(std::string_view name) const noexcept {
        if constexpr (EnumTraits<T>::size() == 0) {
            return std::nullopt;
        }

        size_t index = hash(name);
        size_t start_index = index;

        do {
            if (!table_[index].occupied) {
                return std::nullopt;
            }
            if (table_[index].key == name) {
                return table_[index].value;
            }
            index = (index + 1) % table_size;
        } while (index != start_index);

        return std::nullopt;
    }
};

template <typename T>
static constexpr auto lookup_table = EnumLookupTable<T>{};

// **String comparison helper functions**
constexpr bool iequals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); ++i) {
        char ca = a[i];
        char cb = b[i];

        // Convert to lowercase
        if (ca >= 'A' && ca <= 'Z')
            ca += 32;
        if (cb >= 'A' && cb <= 'Z')
            cb += 32;

        if (ca != cb)
            return false;
    }
    return true;
}

constexpr bool starts_with(std::string_view str,
                           std::string_view prefix) noexcept {
    return str.size() >= prefix.size() &&
           str.substr(0, prefix.size()) == prefix;
}

constexpr bool contains_substring(std::string_view str,
                                  std::string_view substr) noexcept {
    if (substr.empty())
        return true;
    if (str.size() < substr.size())
        return false;

    for (size_t i = 0; i <= str.size() - substr.size(); ++i) {
        if (str.substr(i, substr.size()) == substr) {
            return true;
        }
    }
    return false;
}
}  // namespace detail

// **Core enum operation functions**

/**
 * \brief Get the string name of an enum value
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr std::string_view enum_name(T value) noexcept {
    if constexpr (EnumTraits<T>::size() == 0) {
        // **Runtime reflection fallback**
        return "";
    } else {
        constexpr auto& VALUES = EnumTraits<T>::values;
        constexpr auto& NAMES = EnumTraits<T>::names;

        for (size_t i = 0; i < VALUES.size(); ++i) {
            if (VALUES[i] == value) {
                return NAMES[i];
            }
        }
    }
    return {};
}

/**
 * \brief Convert string to enum value
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr std::optional<T> enum_cast(std::string_view name) noexcept {
    if constexpr (EnumTraits<T>::size() == 0) {
        return std::nullopt;
    }

    // **Optimization: use hash table for large enums, linear search for small
    // enums**
    if constexpr (EnumTraits<T>::size() > 10) {
        return detail::lookup_table<T>.find(name);
    } else {
        constexpr auto& VALUES = EnumTraits<T>::values;
        constexpr auto& NAMES = EnumTraits<T>::names;

        for (size_t i = 0; i < NAMES.size(); ++i) {
            if (NAMES[i] == name) {
                return VALUES[i];
            }
        }
    }
    return std::nullopt;
}

/**
 * \brief Case-insensitive enum conversion
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr std::optional<T> enum_cast_icase(std::string_view name) noexcept {
    if constexpr (EnumTraits<T>::size() == 0) {
        return std::nullopt;
    }

    constexpr auto& NAMES = EnumTraits<T>::names;
    constexpr auto& VALUES = EnumTraits<T>::values;

    for (size_t i = 0; i < NAMES.size(); ++i) {
        if (detail::iequals(NAMES[i], name)) {
            return VALUES[i];
        }
    }

    // **Check aliases**
    if constexpr (EnumTraits<T>::aliases.size() > 0) {
        constexpr auto& ALIASES = EnumTraits<T>::aliases;
        for (size_t i = 0; i < ALIASES.size() && i < VALUES.size(); ++i) {
            if (!ALIASES[i].empty() && detail::iequals(ALIASES[i], name)) {
                return VALUES[i];
            }
        }
    }

    return std::nullopt;
}

/**
 * \brief Prefix match query
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
std::vector<T> enum_cast_prefix(std::string_view prefix) noexcept {
    std::vector<T> results;

    if constexpr (EnumTraits<T>::size() > 0) {
        constexpr auto& NAMES = EnumTraits<T>::names;
        constexpr auto& VALUES = EnumTraits<T>::values;

        for (size_t i = 0; i < NAMES.size(); ++i) {
            if (detail::starts_with(NAMES[i], prefix)) {
                results.push_back(VALUES[i]);
            }
        }
    }

    return results;
}

/**
 * \brief Fuzzy match query
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
std::vector<T> enum_cast_fuzzy(std::string_view pattern) noexcept {
    std::vector<T> results;

    if constexpr (EnumTraits<T>::size() > 0) {
        constexpr auto& NAMES = EnumTraits<T>::names;
        constexpr auto& VALUES = EnumTraits<T>::values;

        for (size_t i = 0; i < NAMES.size(); ++i) {
            if (detail::contains_substring(NAMES[i], pattern)) {
                results.push_back(VALUES[i]);
            }
        }
    }

    return results;
}

/**
 * \brief Convert enum value to integer
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr auto enum_to_integer(T value) noexcept {
    return static_cast<std::underlying_type_t<T>>(value);
}

/**
 * \brief Convert integer to enum value
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr std::optional<T> integer_to_enum(
    std::underlying_type_t<T> value) noexcept {
    if constexpr (EnumTraits<T>::size() > 0) {
        constexpr auto& VALUES = EnumTraits<T>::values;

        for (const auto& val : VALUES) {
            if (enum_to_integer(val) == value) {
                return val;
            }
        }
    }
    return std::nullopt;
}

/**
 * \brief Check if enum value is valid
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr bool enum_contains(T value) noexcept {
    if constexpr (EnumTraits<T>::size() > 0) {
        return EnumTraits<T>::contains(value);
    }
    return false;
}

/**
 * \brief Get all enum value and name pairs
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr auto enum_entries() noexcept {
    if constexpr (EnumTraits<T>::size() > 0) {
        constexpr auto& VALUES = EnumTraits<T>::values;
        constexpr auto& NAMES = EnumTraits<T>::names;
        std::array<std::pair<T, std::string_view>, VALUES.size()> entries{};

        for (size_t i = 0; i < VALUES.size(); ++i) {
            entries[i] = {VALUES[i], NAMES[i]};
        }
        return entries;
    } else {
        return std::array<std::pair<T, std::string_view>, 0>{};
    }
}

/**
 * \brief Get the description of an enum value
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr std::string_view enum_description(T value) noexcept {
    if constexpr (EnumTraits<T>::size() > 0 &&
                  EnumTraits<T>::descriptions.size() > 0) {
        constexpr auto& VALUES = EnumTraits<T>::values;
        constexpr auto& DESCRIPTIONS = EnumTraits<T>::descriptions;

        for (size_t i = 0; i < VALUES.size() && i < DESCRIPTIONS.size(); ++i) {
            if (VALUES[i] == value) {
                return DESCRIPTIONS[i];
            }
        }
    }
    return {};
}

/**
 * \brief Get the default value of an enum
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr T enum_default() noexcept {
    if constexpr (EnumTraits<T>::size() > 0) {
        return EnumTraits<T>::default_value;
    } else {
        return static_cast<T>(0);
    }
}

/**
 * \brief Check if integer value is in enum range
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr bool integer_in_enum_range(std::underlying_type_t<T> value) noexcept {
    if constexpr (EnumTraits<T>::size() > 0) {
        constexpr auto& VALUES = EnumTraits<T>::values;
        return std::any_of(VALUES.begin(), VALUES.end(), [value](T e) {
            return enum_to_integer(e) == value;
        });
    }
    return false;
}

/**
 * \brief Check if enum value is in the specified range
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr bool enum_in_range(T value, T min_val, T max_val) noexcept {
    auto int_value = enum_to_integer(value);
    auto int_min = enum_to_integer(min_val);
    auto int_max = enum_to_integer(max_val);
    return int_value >= int_min && int_value <= int_max;
}

// **Bitwise operation support (for flag enums)**

/**
 * \brief Enum entries sorted by name
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
auto enum_sorted_by_name() noexcept {
    auto entries = enum_entries<T>();
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });
    return entries;
}

/**
 * \brief Enum entries sorted by value
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
auto enum_sorted_by_value() noexcept {
    auto entries = enum_entries<T>();
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return enum_to_integer(a.first) < enum_to_integer(b.first);
    });
    return entries;
}

// **Bitwise operators (for flag enums)**
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr T operator|(T lhs, T rhs) noexcept {
    using UT = std::underlying_type_t<T>;
    return static_cast<T>(static_cast<UT>(lhs) | static_cast<UT>(rhs));
}

template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr T& operator|=(T& lhs, T rhs) noexcept {
    return lhs = lhs | rhs;
}

template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr T operator&(T lhs, T rhs) noexcept {
    using UT = std::underlying_type_t<T>;
    return static_cast<T>(static_cast<UT>(lhs) & static_cast<UT>(rhs));
}

template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr T& operator&=(T& lhs, T rhs) noexcept {
    return lhs = lhs & rhs;
}

template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr T operator^(T lhs, T rhs) noexcept {
    using UT = std::underlying_type_t<T>;
    return static_cast<T>(static_cast<UT>(lhs) ^ static_cast<UT>(rhs));
}

template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr T& operator^=(T& lhs, T rhs) noexcept {
    return lhs = lhs ^ rhs;
}

template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr T operator~(T rhs) noexcept {
    using UT = std::underlying_type_t<T>;
    return static_cast<T>(~static_cast<UT>(rhs));
}

// **Flag enum specific functions**

/**
 * \brief Check if flag is set
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr bool has_flag(T flags, T flag) noexcept {
    static_assert(EnumTraits<T>::is_flags, "T must be a flag enum");
    return (flags & flag) == flag;
}

/**
 * \brief Set flag
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr T set_flag(T flags, T flag) noexcept {
    static_assert(EnumTraits<T>::is_flags, "T must be a flag enum");
    return flags | flag;
}

/**
 * \brief Clear flag
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr T clear_flag(T flags, T flag) noexcept {
    static_assert(EnumTraits<T>::is_flags, "T must be a flag enum");
    return flags & ~flag;
}

/**
 * \brief Toggle flag
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr T toggle_flag(T flags, T flag) noexcept {
    static_assert(EnumTraits<T>::is_flags, "T must be a flag enum");
    return flags ^ flag;
}

/**
 * \brief Get all set flags
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
std::vector<T> get_set_flags(T flags) noexcept {
    static_assert(EnumTraits<T>::is_flags, "T must be a flag enum");
    std::vector<T> result;

    if constexpr (EnumTraits<T>::size() > 0) {
        constexpr auto& VALUES = EnumTraits<T>::values;
        for (const auto& flag : VALUES) {
            if (has_flag(flags, flag)) {
                result.push_back(flag);
            }
        }
    }

    return result;
}

// **Serialization support**

/**
 * \brief Serialize enum value to string
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
std::string serialize_enum(T value) {
    auto name = enum_name(value);
    return std::string(name);
}

/**
 * \brief Deserialize enum value from string
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
std::optional<T> deserialize_enum(const std::string& str) {
    return enum_cast<T>(str);
}

/**
 * \brief Serialize flag enum to string list
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
std::string serialize_flags(T flags, const std::string& separator = "|") {
    static_assert(EnumTraits<T>::is_flags, "T must be a flag enum");

    auto set_flags = get_set_flags(flags);
    if (set_flags.empty()) {
        return "";
    }

    std::string result;
    for (size_t i = 0; i < set_flags.size(); ++i) {
        if (i > 0) {
            result += separator;
        }
        result += serialize_enum(set_flags[i]);
    }

    return result;
}

/**
 * \brief Deserialize flag enum from string list
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
std::optional<T> deserialize_flags(const std::string& str,
                                   const std::string& separator = "|") {
    static_assert(EnumTraits<T>::is_flags, "T must be a flag enum");

    if (str.empty()) {
        return static_cast<T>(0);
    }

    T result = static_cast<T>(0);
    size_t start = 0;
    size_t end = 0;

    while (end != std::string::npos) {
        end = str.find(separator, start);
        std::string flag_name = str.substr(
            start, end == std::string::npos ? std::string::npos : end - start);

        // Remove whitespace
        flag_name.erase(0, flag_name.find_first_not_of(" \t\n\r"));
        flag_name.erase(flag_name.find_last_not_of(" \t\n\r") + 1);

        auto flag = deserialize_enum<T>(flag_name);
        if (!flag) {
            return std::nullopt;  // Invalid flag name
        }

        result = set_flag(result, *flag);

        start = (end == std::string::npos) ? end : end + separator.length();
    }

    return result;
}

// **Enum validator**
template <typename T>
class EnumValidator {
    static_assert(std::is_enum_v<T>, "T must be an enum type");

private:
    std::function<bool(T)> validator_;
    std::string error_message_;

public:
    explicit EnumValidator(std::function<bool(T)> validator,
                           std::string error_msg = "Invalid enum value")
        : validator_(std::move(validator)),
          error_message_(std::move(error_msg)) {}

    bool validate(T value) const { return validator_(value); }

    const std::string& error_message() const noexcept { return error_message_; }

    std::optional<T> validated_cast(std::string_view name) const {
        auto result = enum_cast<T>(name);
        if (result && validate(*result)) {
            return result;
        }
        return std::nullopt;
    }
};

// **Enum iterator**
template <typename T>
class EnumIterator {
    static_assert(std::is_enum_v<T>, "T must be an enum type");

private:
    size_t index_;

public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = const T*;
    using reference = const T&;

    explicit constexpr EnumIterator(size_t index = 0) noexcept
        : index_(index) {}

    constexpr reference operator*() const noexcept {
        return EnumTraits<T>::values[index_];
    }

    constexpr pointer operator->() const noexcept {
        return &EnumTraits<T>::values[index_];
    }

    constexpr EnumIterator& operator++() noexcept {
        ++index_;
        return *this;
    }

    constexpr EnumIterator operator++(int) noexcept {
        auto temp = *this;
        ++index_;
        return temp;
    }

    constexpr bool operator==(const EnumIterator& other) const noexcept {
        return index_ == other.index_;
    }

    constexpr bool operator!=(const EnumIterator& other) const noexcept {
        return index_ != other.index_;
    }
};

/**
 * \brief Get the iterator range of enum values
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr auto enum_range() noexcept {
    struct Range {
        constexpr EnumIterator<T> begin() const noexcept {
            return EnumIterator<T>(0);
        }
        constexpr EnumIterator<T> end() const noexcept {
            return EnumIterator<T>(EnumTraits<T>::size());
        }
    };
    return Range{};
}

// **Macro definitions to simplify EnumTraits specialization**
#define ATOM_META_ENUM_TRAITS(EnumType, ...)                         \
    template <>                                                      \
    struct atom::meta::EnumTraits<EnumType> {                        \
        using enum_type = EnumType;                                  \
        using underlying_type = std::underlying_type_t<EnumType>;    \
                                                                     \
        static constexpr std::array values = {__VA_ARGS__};          \
                                                                     \
        static constexpr std::array names = []() {                   \
            std::array<std::string_view, values.size()> result{};    \
            for (size_t i = 0; i < values.size(); ++i) {             \
                result[i] = #__VA_ARGS__[i];                         \
            }                                                        \
            return result;                                           \
        }();                                                         \
                                                                     \
        static constexpr std::array<std::string_view, values.size()> \
            descriptions{};                                          \
        static constexpr std::array<std::string_view, values.size()> \
            aliases{};                                               \
                                                                     \
        static constexpr bool is_flags = false;                      \
        static constexpr bool is_sequential = true;                  \
        static constexpr bool is_continuous = true;                  \
        static constexpr EnumType default_value = values[0];         \
        static constexpr std::string_view type_name = #EnumType;     \
        static constexpr std::string_view type_description = "";     \
                                                                     \
        static constexpr bool contains(EnumType value) noexcept {    \
            for (const auto& val : values) {                         \
                if (val == value)                                    \
                    return true;                                     \
            }                                                        \
            return false;                                            \
        }                                                            \
    }

// **Enum specialization macro with description**
#define ATOM_META_ENUM_TRAITS_WITH_DESC(EnumType, TypeDesc, ...)       \
    template <>                                                        \
    struct atom::meta::EnumTraits<EnumType> {                          \
        using enum_type = EnumType;                                    \
        using underlying_type = std::underlying_type_t<EnumType>;      \
                                                                       \
        struct EnumEntry {                                             \
            EnumType value;                                            \
            std::string_view name;                                     \
            std::string_view description;                              \
            std::string_view alias;                                    \
        };                                                             \
                                                                       \
        static constexpr std::array entries = {__VA_ARGS__};           \
                                                                       \
        static constexpr auto values = []() {                          \
            std::array<EnumType, entries.size()> result{};             \
            for (size_t i = 0; i < entries.size(); ++i) {              \
                result[i] = entries[i].value;                          \
            }                                                          \
            return result;                                             \
        }();                                                           \
                                                                       \
        static constexpr auto names = []() {                           \
            std::array<std::string_view, entries.size()> result{};     \
            for (size_t i = 0; i < entries.size(); ++i) {              \
                result[i] = entries[i].name;                           \
            }                                                          \
            return result;                                             \
        }();                                                           \
                                                                       \
        static constexpr auto descriptions = []() {                    \
            std::array<std::string_view, entries.size()> result{};     \
            for (size_t i = 0; i < entries.size(); ++i) {              \
                result[i] = entries[i].description;                    \
            }                                                          \
            return result;                                             \
        }();                                                           \
                                                                       \
        static constexpr auto aliases = []() {                         \
            std::array<std::string_view, entries.size()> result{};     \
            for (size_t i = 0; i < entries.size(); ++i) {              \
                result[i] = entries[i].alias;                          \
            }                                                          \
            return result;                                             \
        }();                                                           \
                                                                       \
        static constexpr bool is_flags = false;                        \
        static constexpr bool is_sequential = false;                   \
        static constexpr bool is_continuous = false;                   \
        static constexpr EnumType default_value = values[0];           \
        static constexpr std::string_view type_name = #EnumType;       \
        static constexpr std::string_view type_description = TypeDesc; \
                                                                       \
        static constexpr bool contains(EnumType value) noexcept {      \
            for (const auto& val : values) {                           \
                if (val == value)                                      \
                    return true;                                       \
            }                                                          \
            return false;                                              \
        }                                                              \
    }

// **Flag enum specialization macro**
#define ATOM_META_FLAG_ENUM_TRAITS(EnumType, TypeDesc, ...)                 \
    template <>                                                             \
    struct atom::meta::EnumTraits<EnumType> {                               \
        using enum_type = EnumType;                                         \
        using underlying_type = std::underlying_type_t<EnumType>;           \
                                                                            \
        static constexpr std::array values = {__VA_ARGS__};                 \
                                                                            \
        static constexpr std::array names = []() {                          \
            std::array<std::string_view, values.size()> result{};           \
            for (size_t i = 0; i < values.size(); ++i) {                    \
                result[i] = #__VA_ARGS__[i];                                \
            }                                                               \
            return result;                                                  \
        }();                                                                \
                                                                            \
        static constexpr std::array<std::string_view, values.size()>        \
            descriptions{};                                                 \
        static constexpr std::array<std::string_view, values.size()>        \
            aliases{};                                                      \
                                                                            \
        static constexpr bool is_flags = true;                              \
        static constexpr bool is_sequential = false;                        \
        static constexpr bool is_continuous = false;                        \
        static constexpr EnumType default_value = static_cast<EnumType>(0); \
        static constexpr std::string_view type_name = #EnumType;            \
        static constexpr std::string_view type_description = TypeDesc;      \
                                                                            \
        static constexpr bool contains(EnumType value) noexcept {           \
            for (const auto& val : values) {                                \
                if (val == value)                                           \
                    return true;                                            \
            }                                                               \
            return false;                                                   \
        }                                                                   \
    }

// **Enum helper function - create enum entry with description**
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
constexpr auto make_enum_entry(T value, std::string_view name,
                               std::string_view description = "",
                               std::string_view alias = "") noexcept {
    return typename EnumTraits<T>::EnumEntry{value, name, description, alias};
}

/**
 * \brief Enum reflection class - provides static and dynamic reflection
 * capabilities
 */
template <typename T>
#if __cplusplus >= 202002L
    requires EnumerationType<T>
#endif
class EnumReflection {
public:
    static constexpr size_t count() noexcept { return EnumTraits<T>::size(); }

    static constexpr bool is_flags() noexcept {
        return EnumTraits<T>::is_flags;
    }

    static constexpr bool is_sequential() noexcept {
        return EnumTraits<T>::is_sequential;
    }

    static constexpr bool is_continuous() noexcept {
        return EnumTraits<T>::is_continuous;
    }

    static constexpr std::string_view type_name() noexcept {
        return EnumTraits<T>::type_name;
    }

    static constexpr std::string_view type_description() noexcept {
        return EnumTraits<T>::type_description;
    }

    static constexpr auto values() noexcept { return EnumTraits<T>::values; }

    static constexpr auto names() noexcept { return EnumTraits<T>::names; }

    static constexpr auto descriptions() noexcept {
        return EnumTraits<T>::descriptions;
    }

    static constexpr auto aliases() noexcept { return EnumTraits<T>::aliases; }

    static constexpr auto entries() noexcept { return enum_entries<T>(); }

    static constexpr auto default_value() noexcept {
        return EnumTraits<T>::default_value;
    }

    static constexpr auto min_value() noexcept {
        return EnumTraits<T>::min_value();
    }

    static constexpr auto max_value() noexcept {
        return EnumTraits<T>::max_value();
    }

    // Get the name of an enum value
    static std::string_view get_name(T value) noexcept {
        return enum_name(value);
    }

    // Get the description of an enum value
    static std::string_view get_description(T value) noexcept {
        return enum_description(value);
    }

    // Get the iterator range of all enum values
    static constexpr auto get_range() noexcept { return enum_range<T>(); }

    // Get enum value from name
    static std::optional<T> from_name(std::string_view name) noexcept {
        return enum_cast<T>(name);
    }

    // Get enum value from integer
    static std::optional<T> from_integer(
        std::underlying_type_t<T> value) noexcept {
        return integer_to_enum<T>(value);
    }
};
}  // namespace atom::meta

#endif  // ATOM_META_ENUM_HPP
