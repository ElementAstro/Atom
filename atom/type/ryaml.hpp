#ifndef ATOM_TYPE_RYAML_HPP
#define ATOM_TYPE_RYAML_HPP

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "atom/error/exception.hpp"

namespace atom::type {

/**
 * @brief Forward declaration of YamlValue class.
 */
class YamlValue;

/**
 * @brief Alias for a map of string to YamlValue representing a YAML object.
 */
using YamlObject = std::unordered_map<std::string, YamlValue>;

/**
 * @brief Alias for a vector of YamlValue representing a YAML array.
 */
using YamlArray = std::vector<YamlValue>;

/**
 * @brief Represents the position in a YAML document.
 */
struct YamlPosition {
    size_t line{1};
    size_t column{1};

    std::string to_string() const {
        return "line " + std::to_string(line) + ", column " +
               std::to_string(column);
    }
};

/**
 * @brief Represents an error encountered during YAML parsing.
 */
class YamlException : public atom::error::Exception {
public:
    YamlException(const std::string& message, const YamlPosition& position)
        : atom::error::Exception(__FILE__, __LINE__, __func__, message, " at ",
                                 position.to_string()),
          position_(position) {}

    [[nodiscard]] const YamlPosition& position() const { return position_; }

private:
    YamlPosition position_;
};

/**
 * @brief Represents a YAML tag.
 */
class YamlTag {
public:
    YamlTag() : tag_("!!str") {}
    explicit YamlTag(const std::string& tag) : tag_(tag) {}

    [[nodiscard]] const std::string& tag() const { return tag_; }
    [[nodiscard]] bool is_default() const { return tag_ == "!!str"; }

    static YamlTag Str() { return YamlTag("!!str"); }
    static YamlTag Int() { return YamlTag("!!int"); }
    static YamlTag Float() { return YamlTag("!!float"); }
    static YamlTag Bool() { return YamlTag("!!bool"); }
    static YamlTag Null() { return YamlTag("!!null"); }
    static YamlTag Map() { return YamlTag("!!map"); }
    static YamlTag Seq() { return YamlTag("!!seq"); }

private:
    std::string tag_;
};

/**
 * @brief Represents an anchor in a YAML document.
 */
class YamlAnchor {
public:
    YamlAnchor() = default;
    explicit YamlAnchor(const std::string& name) : name_(name) {}

    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] bool has_name() const { return !name_.empty(); }

private:
    std::string name_;
};

/**
 * @brief Options for YAML serialization.
 */
struct YamlSerializeOptions {
    bool use_flow_style{false};
    bool canonical{false};
    int indent{2};
    bool explicit_start{false};
    bool explicit_end{false};
};

struct AliasName {
    std::string name;
    explicit AliasName(const std::string& n) : name(n) {}
};

/**
 * @brief Represents a value in a YAML document.
 *
 * This class can represent various YAML value types including null, string,
 * number, boolean, object (map), and array. It provides methods to access the
 * value in its appropriate type and convert it to a string representation.
 */
class YamlValue {
public:
    /**
     * @brief Enumeration of YAML value types.
     */
    enum class Type { Null, String, Number, Bool, Object, Array, Alias };

    /**
     * @brief Default constructor initializing a null value.
     */
    YamlValue();

    /**
     * @brief Constructs a YamlValue from a string.
     * @param value The string value.
     */
    explicit YamlValue(const std::string& value);

    /**
     * @brief Constructs a YamlValue from a C-style string.
     * @param value The string value.
     */
    explicit YamlValue(const char* value);

    /**
     * @brief Constructs a YamlValue from a number.
     * @param value The numeric value.
     */
    explicit YamlValue(double value);

    /**
     * @brief Constructs a YamlValue from an integer.
     * @param value The integer value.
     */
    explicit YamlValue(int value);

    /**
     * @brief Constructs a YamlValue from a long integer.
     * @param value The long integer value.
     */
    explicit YamlValue(long value);

    /**
     * @brief Constructs a YamlValue from a boolean.
     * @param value The boolean value.
     */
    explicit YamlValue(bool value);

    /**
     * @brief Constructs a YamlValue from a YAML object.
     * @param value The YAML object (map).
     */
    explicit YamlValue(const YamlObject& value);

    /**
     * @brief Constructs a YamlValue from a YAML array.
     * @param value The YAML array (vector).
     */
    explicit YamlValue(const YamlArray& value);

    /**
     * @brief Constructs a YamlValue as an alias.
     * @param alias_name The name of the alias.
     */
    static YamlValue create_alias(const std::string& alias_name);

    /**
     * @brief Gets the type of the YAML value.
     * @return The type of the YAML value.
     */
    [[nodiscard]] auto type() const -> Type;

    /**
     * @brief Checks if the value is null.
     * @return True if the value is null, false otherwise.
     */
    [[nodiscard]] bool is_null() const;

    /**
     * @brief Checks if the value is a string.
     * @return True if the value is a string, false otherwise.
     */
    [[nodiscard]] bool is_string() const;

    /**
     * @brief Checks if the value is a number.
     * @return True if the value is a number, false otherwise.
     */
    [[nodiscard]] bool is_number() const;

    /**
     * @brief Checks if the value is a boolean.
     * @return True if the value is a boolean, false otherwise.
     */
    [[nodiscard]] bool is_bool() const;

    /**
     * @brief Checks if the value is an object.
     * @return True if the value is an object, false otherwise.
     */
    [[nodiscard]] bool is_object() const;

    /**
     * @brief Checks if the value is an array.
     * @return True if the value is an array, false otherwise.
     */
    [[nodiscard]] bool is_array() const;

    /**
     * @brief Checks if the value is an alias.
     * @return True if the value is an alias, false otherwise.
     */
    [[nodiscard]] bool is_alias() const;

    /**
     * @brief Gets the value as a string.
     * @return The string value.
     * @throws YamlException if the value is not a string.
     */
    [[nodiscard]] auto as_string() const -> const std::string&;

    /**
     * @brief Gets the value as a number.
     * @return The numeric value.
     * @throws YamlException if the value is not a number.
     */
    [[nodiscard]] auto as_number() const -> double;

    /**
     * @brief Gets the value as an integer.
     * @return The integer value.
     * @throws YamlException if the value is not a number or cannot be
     * represented as an integer.
     */
    [[nodiscard]] auto as_int() const -> int;

    /**
     * @brief Gets the value as a long integer.
     * @return The long integer value.
     * @throws YamlException if the value is not a number or cannot be
     * represented as a long integer.
     */
    [[nodiscard]] auto as_long() const -> long;

    /**
     * @brief Gets the value as a boolean.
     * @return The boolean value.
     * @throws YamlException if the value is not a boolean.
     */
    [[nodiscard]] auto as_bool() const -> bool;

    /**
     * @brief Gets the value as a YAML object.
     * @return The YAML object (map).
     * @throws YamlException if the value is not an object.
     */
    [[nodiscard]] auto as_object() const -> const YamlObject&;

    /**
     * @brief Gets a mutable reference to the value as a YAML object.
     * @return The mutable YAML object (map).
     * @throws YamlException if the value is not an object.
     */
    auto as_object() -> YamlObject&;

    /**
     * @brief Gets the value as a YAML array.
     * @return The YAML array (vector).
     * @throws YamlException if the value is not an array.
     */
    [[nodiscard]] auto as_array() const -> const YamlArray&;

    /**
     * @brief Gets a mutable reference to the value as a YAML array.
     * @return The mutable YAML array (vector).
     * @throws YamlException if the value is not an array.
     */
    auto as_array() -> YamlArray&;

    /**
     * @brief Gets the alias name if this value is an alias.
     * @return The alias name.
     * @throws YamlException if the value is not an alias.
     */
    [[nodiscard]] auto alias_name() const -> const std::string&;

    /**
     * @brief Gets the tag of the value.
     * @return The YAML tag.
     */
    [[nodiscard]] auto tag() const -> const YamlTag&;

    /**
     * @brief Sets the tag of the value.
     * @param tag The YAML tag.
     */
    void set_tag(const YamlTag& tag);

    /**
     * @brief Gets the anchor of the value.
     * @return The YAML anchor.
     */
    [[nodiscard]] auto anchor() const -> const YamlAnchor&;

    /**
     * @brief Sets the anchor of the value.
     * @param anchor The YAML anchor.
     */
    void set_anchor(const YamlAnchor& anchor);

    /**
     * @brief Converts the YAML value to its string representation.
     * @param options Serialization options.
     * @return The string representation of the YAML value.
     */
    std::string to_string(const YamlSerializeOptions& options = {}) const;

    /**
     * @brief Converts the YAML value to its YAML representation.
     * @param options Serialization options.
     * @return The YAML representation of the value.
     */
    std::string to_yaml(const YamlSerializeOptions& options = {}) const;

    /**
     * @brief Accesses a value in a YAML object by key.
     * @param key The key in the YAML object.
     * @return The value associated with the key.
     * @throws YamlException if the value is not an object or the key is
     * not present.
     */
    auto operator[](const std::string& key) const -> const YamlValue&;

    /**
     * @brief Accesses or creates a value in a YAML object by key.
     * @param key The key in the YAML object.
     * @return A reference to the value associated with the key.
     * @throws YamlException if the value is not an object.
     */
    auto operator[](const std::string& key) -> YamlValue&;

    /**
     * @brief Accesses a value in a YAML array by index.
     * @param index The index in the YAML array.
     * @return The value at the specified index.
     * @throws YamlException if the value is not an array or the index
     * is out of range.
     */
    auto operator[](size_t index) const -> const YamlValue&;

    /**
     * @brief Accesses a value in a YAML array by index.
     * @param index The index in the YAML array.
     * @return A reference to the value at the specified index.
     * @throws YamlException if the value is not an array or the index
     * is out of range.
     */
    auto operator[](size_t index) -> YamlValue&;

    /**
     * @brief Equality operator.
     * @param other The other YamlValue to compare with.
     * @return True if the values are equal, false otherwise.
     */
    bool operator==(const YamlValue& other) const;

    /**
     * @brief Inequality operator.
     * @param other The other YamlValue to compare with.
     * @return True if the values are not equal, false otherwise.
     */
    bool operator!=(const YamlValue& other) const;

    /**
     * @brief Checks if the object contains a key.
     * @param key The key to check.
     * @return True if the object contains the key, false otherwise.
     * @throws YamlException if the value is not an object.
     */
    [[nodiscard]] bool contains(const std::string& key) const;

    /**
     * @brief Gets a value from an object by key, or a default value if the key
     * does not exist.
     * @param key The key in the YAML object.
     * @param default_value The default value to return if the key does not
     * exist.
     * @return The value associated with the key, or the default value.
     * @throws YamlException if the value is not an object.
     */
    auto get(const std::string& key, const YamlValue& default_value) const
        -> const YamlValue&;

    /**
     * @brief Attempts to get a value from an object by key.
     * @param key The key in the YAML object.
     * @return An optional containing the value if found, or an empty optional
     * if not.
     * @throws YamlException if the value is not an object.
     */
    [[nodiscard]] std::optional<std::reference_wrapper<const YamlValue>>
    try_get(const std::string& key) const;

    /**
     * @brief Gets the size of the object or array.
     * @return The size of the object or array.
     * @throws YamlException if the value is not an object or array.
     */
    [[nodiscard]] size_t size() const;

    /**
     * @brief Checks if the object or array is empty.
     * @return True if the object or array is empty, false otherwise.
     * @throws YamlException if the value is not an object or array.
     */
    [[nodiscard]] bool empty() const;

    /**
     * @brief Clears the object or array.
     * @throws YamlException if the value is not an object or array.
     */
    void clear();

    /**
     * @brief Erases a key from an object.
     * @param key The key to erase.
     * @return The number of elements erased (0 or 1).
     * @throws YamlException if the value is not an object.
     */
    size_t erase(const std::string& key);

    /**
     * @brief Erases an index from an array.
     * @param index The index to erase.
     * @throws YamlException if the value is not an array or the index is out of
     * range.
     */
    void erase(size_t index);

    /**
     * @brief 模板方法，将 YAML 值转换为指定类型
     * @tparam T 要转换到的目标类型
     * @return 转换后的值
     */
    template <typename T>
    [[nodiscard]] T as() const;

private:
    Type type_;  ///< The type of the YAML value.
    std::variant<std::nullptr_t, std::string, double, bool, YamlObject,
                 YamlArray, AliasName>
        value_;          ///< The actual value stored.
    YamlTag tag_;        ///< The YAML tag.
    YamlAnchor anchor_;  ///< The YAML anchor.

    // Helper methods for serialization
    void to_yaml_impl(std::stringstream& ss,
                      const YamlSerializeOptions& options,
                      int indent_level) const;
    void serialize_object(std::stringstream& ss,
                          const YamlSerializeOptions& options,
                          int indent_level) const;
    void serialize_array(std::stringstream& ss,
                         const YamlSerializeOptions& options,
                         int indent_level) const;
    std::string get_indent(int level,
                           const YamlSerializeOptions& options) const;
};

/**
 * @brief A class representing a YAML document.
 */
class YamlDocument {
public:
    /**
     * @brief Default constructor.
     */
    YamlDocument() = default;

    /**
     * @brief Constructs a YamlDocument from a root YamlValue.
     * @param root The root value.
     */
    explicit YamlDocument(const YamlValue& root);

    /**
     * @brief Gets the root value of the document.
     * @return The root value.
     */
    [[nodiscard]] const YamlValue& root() const;

    /**
     * @brief Gets a mutable reference to the root value of the document.
     * @return The mutable root value.
     */
    YamlValue& root();

    /**
     * @brief Sets the root value of the document.
     * @param root The root value.
     */
    void set_root(const YamlValue& root);

    /**
     * @brief Converts the document to a YAML string.
     * @param options Serialization options.
     * @return The YAML string.
     */
    std::string to_yaml(const YamlSerializeOptions& options = {}) const;

private:
    YamlValue root_;  ///< The root value of the document.
};

/**
 * @brief Represents options for YAML parsing.
 */
struct YamlParseOptions {
    bool allow_duplicate_keys{false};
    bool support_comments{true};
    bool support_anchors{true};
    bool support_tags{true};
    bool allow_unquoted_strings{true};
};

/**
 * @brief Parses a YAML document from a string.
 *
 * This class provides methods to parse a YAML document represented as a string
 * and convert it into a `YamlValue` object that reflects the structure and
 * values of the YAML document.
 */
class YamlParser {
public:
    /**
     * @brief Parses a YAML string into a YamlValue.
     * @param str The YAML string.
     * @param options Parsing options.
     * @return The parsed YamlValue object.
     * @throws YamlException on parsing errors.
     */
    static auto parse(const std::string& str,
                      const YamlParseOptions& options = {}) -> YamlValue;

    /**
     * @brief Parses a YAML string into a YamlDocument.
     * @param str The YAML string.
     * @param options Parsing options.
     * @return The parsed YamlDocument.
     * @throws YamlException on parsing errors.
     */
    static auto parse_document(const std::string& str,
                               const YamlParseOptions& options = {})
        -> YamlDocument;

    /**
     * @brief Parses multiple YAML documents from a string.
     * @param str The YAML string.
     * @param options Parsing options.
     * @return A vector of parsed YamlDocuments.
     * @throws YamlException on parsing errors.
     */
    static auto parse_multi_documents(const std::string& str,
                                      const YamlParseOptions& options = {})
        -> std::vector<YamlDocument>;

private:
    /**
     * @brief Context for parsing.
     */
    struct ParseContext {
        const std::string& str;
        size_t index = 0;
        YamlPosition position{1, 1};
        YamlParseOptions options;
        std::unordered_map<std::string, std::shared_ptr<YamlValue>> anchors;
    };

    /**
     * @brief Parses a YAML value from a string.
     * @param ctx The parsing context.
     * @return The parsed YamlValue.
     * @throws YamlException on parsing errors.
     */
    static auto parse_value(ParseContext& ctx) -> YamlValue;

    /**
     * @brief Parses a YAML string from a string.
     * @param ctx The parsing context.
     * @return The parsed string.
     * @throws YamlException on parsing errors.
     */
    static auto parse_string(ParseContext& ctx) -> std::string;

    /**
     * @brief Parses a YAML number from a string.
     * @param ctx The parsing context.
     * @return The parsed number.
     * @throws YamlException on parsing errors.
     */
    static auto parse_number(ParseContext& ctx) -> double;

    /**
     * @brief Parses a YAML boolean from a string.
     * @param ctx The parsing context.
     * @return The parsed boolean.
     * @throws YamlException on parsing errors.
     */
    static auto parse_bool(ParseContext& ctx) -> bool;

    /**
     * @brief Parses a YAML null value from a string.
     * @param ctx The parsing context.
     * @throws YamlException on parsing errors.
     */
    static void parse_null(ParseContext& ctx);

    /**
     * @brief Parses a YAML object from a string.
     * @param ctx The parsing context.
     * @return The parsed YamlObject.
     * @throws YamlException on parsing errors.
     */
    static auto parse_object(ParseContext& ctx) -> YamlObject;

    /**
     * @brief Parses a YAML array from a string.
     * @param ctx The parsing context.
     * @return The parsed YamlArray.
     * @throws YamlException on parsing errors.
     */
    static auto parse_array(ParseContext& ctx) -> YamlArray;

    /**
     * @brief Parses a YAML flow-style object from a string.
     * @param ctx The parsing context.
     * @return The parsed YamlObject.
     * @throws YamlException on parsing errors.
     */
    static auto parse_flow_object(ParseContext& ctx) -> YamlObject;

    /**
     * @brief Parses a YAML flow-style array from a string.
     * @param ctx The parsing context.
     * @return The parsed YamlArray.
     * @throws YamlException on parsing errors.
     */
    static auto parse_flow_array(ParseContext& ctx) -> YamlArray;

    /**
     * @brief Parses a YAML block scalar from a string.
     * @param ctx The parsing context.
     * @param style The block scalar style ('|' or '>').
     * @return The parsed string.
     * @throws YamlException on parsing errors.
     */
    static auto parse_block_scalar(ParseContext& ctx, char style)
        -> std::string;

    /**
     * @brief Parses a YAML tag from a string.
     * @param ctx The parsing context.
     * @return The parsed YamlTag.
     * @throws YamlException on parsing errors.
     */
    static auto parse_tag(ParseContext& ctx) -> YamlTag;

    /**
     * @brief Parses a YAML anchor from a string.
     * @param ctx The parsing context.
     * @return The parsed YamlAnchor.
     * @throws YamlException on parsing errors.
     */
    static auto parse_anchor(ParseContext& ctx) -> YamlAnchor;

    /**
     * @brief Parses a YAML alias from a string.
     * @param ctx The parsing context.
     * @return The parsed YamlValue.
     * @throws YamlException on parsing errors.
     */
    static auto parse_alias(ParseContext& ctx) -> YamlValue;

    /**
     * @brief Skips whitespace characters in a string.
     * @param ctx The parsing context.
     */
    static void skip_whitespace(ParseContext& ctx);

    /**
     * @brief Skips a YAML comment.
     * @param ctx The parsing context.
     */
    static void skip_comment(ParseContext& ctx);

    /**
     * @brief Advances the current position by one character.
     * @param ctx The parsing context.
     */
    static void advance(ParseContext& ctx);

    /**
     * @brief Checks if the current character matches the expected one.
     * @param ctx The parsing context.
     * @param expected The expected character.
     * @return True if the current character matches, false otherwise.
     */
    static bool match(ParseContext& ctx, char expected);

    /**
     * @brief Expects a specific character at the current position.
     * @param ctx The parsing context.
     * @param expected The expected character.
     * @throws YamlException if the current character does not match the
     * expected one.
     */
    static void expect(ParseContext& ctx, char expected);

    /**
     * @brief Checks if we are at the end of the string.
     * @param ctx The parsing context.
     * @return True if we are at the end, false otherwise.
     */
    static bool is_end(const ParseContext& ctx);

    /**
     * @brief Checks if a character is a valid identifier character.
     * @param c The character to check.
     * @return True if the character is a valid identifier character, false
     * otherwise.
     */
    static bool is_identifier_char(char c);

    /**
     * @brief Checks if a character is a valid first character for an
     * identifier.
     * @param c The character to check.
     * @return True if the character is a valid first character for an
     * identifier, false otherwise.
     */
    static bool is_first_identifier_char(char c);
};

// 模板专门化 - 修复点 2: 改为使用正确的模板特化语法
template <>
[[nodiscard]] inline bool YamlValue::as<bool>() const {
    return as_bool();
}

template <>
[[nodiscard]] inline int YamlValue::as<int>() const {
    return as_int();
}

template <>
[[nodiscard]] inline long YamlValue::as<long>() const {
    return as_long();
}

template <>
[[nodiscard]] inline double YamlValue::as<double>() const {
    return as_number();
}

template <>
[[nodiscard]] inline std::string YamlValue::as<std::string>() const {
    return as_string();
}

template <>
[[nodiscard]] inline YamlObject YamlValue::as<YamlObject>() const {
    return as_object();
}

template <>
[[nodiscard]] inline YamlArray YamlValue::as<YamlArray>() const {
    return as_array();
}

}  // namespace atom::type

#endif  // ATOM_TYPE_RYAML_HPP
