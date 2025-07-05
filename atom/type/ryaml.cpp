#include "ryaml.hpp"

#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace atom::type {

// YamlValue implementation

YamlValue::YamlValue() : type_(Type::Null), value_(nullptr) {}

YamlValue::YamlValue(const std::string& value)
    : type_(Type::String), value_(value) {}

YamlValue::YamlValue(const char* value)
    : type_(Type::String), value_(std::string(value)) {}

YamlValue::YamlValue(double value) : type_(Type::Number), value_(value) {}

YamlValue::YamlValue(int value)
    : type_(Type::Number), value_(static_cast<double>(value)) {}

YamlValue::YamlValue(long value)
    : type_(Type::Number), value_(static_cast<double>(value)) {}

YamlValue::YamlValue(bool value) : type_(Type::Bool), value_(value) {}

YamlValue::YamlValue(const YamlObject& value)
    : type_(Type::Object), value_(value) {}

YamlValue::YamlValue(const YamlArray& value)
    : type_(Type::Array), value_(value) {}

YamlValue YamlValue::create_alias(const std::string& alias_name) {
    YamlValue value;
    value.type_ = Type::Alias;
    value.value_ = AliasName(alias_name);
    return value;
}

auto YamlValue::type() const -> Type { return type_; }

bool YamlValue::is_null() const { return type_ == Type::Null; }

bool YamlValue::is_string() const { return type_ == Type::String; }

bool YamlValue::is_number() const { return type_ == Type::Number; }

bool YamlValue::is_bool() const { return type_ == Type::Bool; }

bool YamlValue::is_object() const { return type_ == Type::Object; }

bool YamlValue::is_array() const { return type_ == Type::Array; }

bool YamlValue::is_alias() const { return type_ == Type::Alias; }

auto YamlValue::as_string() const -> const std::string& {
    if (type_ != Type::String) {
        throw YamlException("Not a string", {});
    }
    return std::get<std::string>(value_);
}

auto YamlValue::as_number() const -> double {
    if (type_ != Type::Number) {
        throw YamlException("Not a number", {});
    }
    return std::get<double>(value_);
}

auto YamlValue::as_int() const -> int {
    if (type_ != Type::Number) {
        throw YamlException("Not a number", {});
    }

    double value = std::get<double>(value_);
    if (value != std::floor(value) ||
        value < static_cast<double>(std::numeric_limits<int>::min()) ||
        value > static_cast<double>(std::numeric_limits<int>::max())) {
        throw YamlException("Number cannot be represented as an integer", {});
    }

    return static_cast<int>(value);
}

auto YamlValue::as_long() const -> long {
    if (type_ != Type::Number) {
        throw YamlException("Not a number", {});
    }

    double value = std::get<double>(value_);
    if (value != std::floor(value) ||
        value < static_cast<double>(std::numeric_limits<long>::min()) ||
        value > static_cast<double>(std::numeric_limits<long>::max())) {
        throw YamlException("Number cannot be represented as a long integer",
                            {});
    }

    return static_cast<long>(value);
}

auto YamlValue::as_bool() const -> bool {
    if (type_ != Type::Bool) {
        throw YamlException("Not a boolean", {});
    }
    return std::get<bool>(value_);
}

auto YamlValue::as_object() const -> const YamlObject& {
    if (type_ != Type::Object) {
        throw YamlException("Not an object", {});
    }
    return std::get<YamlObject>(value_);
}

auto YamlValue::as_object() -> YamlObject& {
    if (type_ != Type::Object) {
        throw YamlException("Not an object", {});
    }
    return std::get<YamlObject>(value_);
}

auto YamlValue::as_array() const -> const YamlArray& {
    if (type_ != Type::Array) {
        throw YamlException("Not an array", {});
    }
    return std::get<YamlArray>(value_);
}

auto YamlValue::as_array() -> YamlArray& {
    if (type_ != Type::Array) {
        throw YamlException("Not an array", {});
    }
    return std::get<YamlArray>(value_);
}

auto YamlValue::alias_name() const -> const std::string& {
    if (type_ != Type::Alias) {
        throw YamlException("Not an alias", {});
    }
    return std::get<AliasName>(value_).name;
}

auto YamlValue::tag() const -> const YamlTag& { return tag_; }

void YamlValue::set_tag(const YamlTag& tag) { tag_ = tag; }

auto YamlValue::anchor() const -> const YamlAnchor& { return anchor_; }

void YamlValue::set_anchor(const YamlAnchor& anchor) { anchor_ = anchor; }

std::string YamlValue::to_string(const YamlSerializeOptions& options) const {
    switch (type_) {
        case Type::Null:
            return "null";
        case Type::String:
            return "\"" + as_string() + "\"";
        case Type::Number: {
            std::stringstream ss;
            double value = as_number();
            if (std::floor(value) == value && !std::isnan(value) &&
                !std::isinf(value)) {
                ss << std::fixed << std::setprecision(0) << value;
            } else {
                ss << std::fixed
                   << std::setprecision(
                          std::numeric_limits<double>::max_digits10)
                   << value;
                // Remove trailing zeros
                std::string str = ss.str();
                size_t pos = str.find_last_not_of('0');
                if (pos != std::string::npos && str[pos] == '.')
                    pos--;
                str.erase(pos + 1);
                return str;
            }
            return ss.str();
        }
        case Type::Bool:
            return as_bool() ? "true" : "false";
        case Type::Object: {
            if (options.use_flow_style) {
                std::stringstream ss;
                ss << "{";
                const auto& obj = as_object();
                bool first = true;
                for (const auto& [key, value] : obj) {
                    if (!first)
                        ss << ", ";
                    first = false;
                    ss << key << ": " << value.to_string(options);
                }
                ss << "}";
                return ss.str();
            } else {
                std::stringstream ss;
                const auto& obj = as_object();
                for (const auto& [key, value] : obj) {
                    ss << key << ": " << value.to_string(options) << "\n";
                }
                return ss.str();
            }
        }
        case Type::Array: {
            if (options.use_flow_style) {
                std::stringstream ss;
                ss << "[";
                const auto& arr = as_array();
                bool first = true;
                for (const auto& item : arr) {
                    if (!first)
                        ss << ", ";
                    first = false;
                    ss << item.to_string(options);
                }
                ss << "]";
                return ss.str();
            } else {
                std::stringstream ss;
                const auto& arr = as_array();
                for (const auto& item : arr) {
                    ss << "- " << item.to_string(options) << "\n";
                }
                return ss.str();
            }
        }
        case Type::Alias:
            return "*" + alias_name();
    }
    throw YamlException("Unknown type", {});
}

std::string YamlValue::to_yaml(const YamlSerializeOptions& options) const {
    std::stringstream ss;
    to_yaml_impl(ss, options, 0);
    return ss.str();
}

void YamlValue::to_yaml_impl(std::stringstream& ss,
                             const YamlSerializeOptions& options,
                             int indent_level) const {
    // Output anchor if present
    if (anchor_.has_name()) {
        ss << "&" << anchor_.name() << " ";
    }

    // Output tag if not default and not explicitly handled by the type
    if (!tag_.is_default()) {
        ss << tag_.tag() << " ";
    }

    switch (type_) {
        case Type::Null:
            ss << "null";
            break;
        case Type::String: {
            const std::string& value = as_string();
            bool needs_quotes =
                value.empty() ||
                (value.find_first_of("#:,[]{}%&*!|>'\"\n\r") !=
                 std::string::npos) ||
                (value.find("---") == 0) || (value.find("...") == 0) ||
                (std::isdigit(value[0]) || value[0] == '-' ||
                 value[0] == '.') ||
                (value == "true" || value == "false" || value == "null");

            if (needs_quotes) {
                ss << "\"";
                for (char c : value) {
                    if (c == '\"') {
                        ss << "\\\"";
                    } else if (c == '\\') {
                        ss << "\\\\";
                    } else if (c == '\n') {
                        ss << "\\n";
                    } else if (c == '\r') {
                        ss << "\\r";
                    } else if (c == '\t') {
                        ss << "\\t";
                    } else {
                        ss << c;
                    }
                }
                ss << "\"";
            } else {
                ss << value;
            }
            break;
        }
        case Type::Number: {
            double value = as_number();
            if (std::floor(value) == value && !std::isnan(value) &&
                !std::isinf(value)) {
                ss << std::fixed << std::setprecision(0) << value;
            } else {
                ss << std::fixed
                   << std::setprecision(
                          std::numeric_limits<double>::max_digits10)
                   << value;
                // Remove trailing zeros
                std::string str = ss.str();
                size_t pos = str.find_last_not_of('0');
                if (pos != std::string::npos && str[pos] == '.')
                    pos--;
                str = str.substr(0, pos + 1);
                ss.str("");
                ss << str;
            }
            break;
        }
        case Type::Bool:
            ss << (as_bool() ? "true" : "false");
            break;
        case Type::Object:
            if (options.use_flow_style) {
                ss << "{";
                const auto& obj = as_object();
                bool first = true;
                for (const auto& [key, value] : obj) {
                    if (!first)
                        ss << ", ";
                    first = false;

                    // Check if key needs quotes
                    bool needs_quotes =
                        key.empty() ||
                        (key.find_first_of("#:,[]{}%&*!|>'\"\n\r") !=
                         std::string::npos) ||
                        (key.find("---") == 0) || (key.find("...") == 0) ||
                        (std::isdigit(key[0]) || key[0] == '-' ||
                         key[0] == '.') ||
                        (key == "true" || key == "false" || key == "null");

                    if (needs_quotes) {
                        ss << "\"" << key << "\"";
                    } else {
                        ss << key;
                    }

                    ss << ": ";
                    value.to_yaml_impl(ss, options, indent_level + 1);
                }
                ss << "}";
            } else {
                serialize_object(ss, options, indent_level);
            }
            break;
        case Type::Array:
            if (options.use_flow_style) {
                ss << "[";
                const auto& arr = as_array();
                bool first = true;
                for (const auto& item : arr) {
                    if (!first)
                        ss << ", ";
                    first = false;
                    item.to_yaml_impl(ss, options, indent_level + 1);
                }
                ss << "]";
            } else {
                serialize_array(ss, options, indent_level);
            }
            break;
        case Type::Alias:
            ss << "*" << alias_name();
            break;
    }
}

void YamlValue::serialize_object(std::stringstream& ss,
                                 const YamlSerializeOptions& options,
                                 int indent_level) const {
    const auto& obj = as_object();
    if (obj.empty()) {
        ss << "{}";
        return;
    }

    bool first = true;
    for (const auto& [key, value] : obj) {
        if (!first) {
            ss << "\n" << get_indent(indent_level, options);
        }
        first = false;

        // Check if key needs quotes
        bool needs_quotes =
            key.empty() ||
            (key.find_first_of("#:,[]{}%&*!|>'\"\n\r") != std::string::npos) ||
            (key.find("---") == 0) || (key.find("...") == 0) ||
            (std::isdigit(key[0]) || key[0] == '-' || key[0] == '.') ||
            (key == "true" || key == "false" || key == "null");

        if (needs_quotes) {
            ss << "\"" << key << "\"";
        } else {
            ss << key;
        }

        ss << ":";

        if (value.is_object() && !value.as_object().empty() &&
            !options.use_flow_style) {
            ss << "\n" << get_indent(indent_level + 1, options);
            value.to_yaml_impl(ss, options, indent_level + 1);
        } else if (value.is_array() && !value.as_array().empty() &&
                   !options.use_flow_style) {
            ss << "\n" << get_indent(indent_level + 1, options);
            value.to_yaml_impl(ss, options, indent_level + 1);
        } else {
            ss << " ";
            value.to_yaml_impl(ss, options, indent_level + 1);
        }
    }
}

void YamlValue::serialize_array(std::stringstream& ss,
                                const YamlSerializeOptions& options,
                                int indent_level) const {
    const auto& arr = as_array();
    if (arr.empty()) {
        ss << "[]";
        return;
    }

    for (size_t i = 0; i < arr.size(); ++i) {
        if (i > 0) {
            ss << "\n" << get_indent(indent_level, options);
        }

        ss << "-";

        const auto& item = arr[i];
        if (item.is_object() && !item.as_object().empty() &&
            !options.use_flow_style) {
            ss << "\n" << get_indent(indent_level + 1, options);
            item.to_yaml_impl(ss, options, indent_level + 1);
        } else if (item.is_array() && !item.as_array().empty() &&
                   !options.use_flow_style) {
            ss << "\n" << get_indent(indent_level + 1, options);
            item.to_yaml_impl(ss, options, indent_level + 1);
        } else {
            ss << " ";
            item.to_yaml_impl(ss, options, indent_level + 1);
        }
    }
}

std::string YamlValue::get_indent(int level,
                                  const YamlSerializeOptions& options) const {
    return std::string(level * options.indent, ' ');
}

auto YamlValue::operator[](const std::string& key) const -> const YamlValue& {
    if (type_ != Type::Object) {
        throw YamlException("Not an object", {});
    }

    const auto& obj = as_object();
    auto it = obj.find(key);
    if (it == obj.end()) {
        throw YamlException("Key not found: " + key, {});
    }

    return it->second;
}

auto YamlValue::operator[](const std::string& key) -> YamlValue& {
    if (type_ != Type::Object) {
        throw YamlException("Not an object", {});
    }

    return std::get<YamlObject>(value_)[key];
}

auto YamlValue::operator[](size_t index) const -> const YamlValue& {
    if (type_ != Type::Array) {
        throw YamlException("Not an array", {});
    }

    const auto& arr = as_array();
    if (index >= arr.size()) {
        throw YamlException("Index out of range: " + std::to_string(index), {});
    }

    return arr[index];
}

auto YamlValue::operator[](size_t index) -> YamlValue& {
    if (type_ != Type::Array) {
        throw YamlException("Not an array", {});
    }

    auto& arr = std::get<YamlArray>(value_);
    if (index >= arr.size()) {
        throw YamlException("Index out of range: " + std::to_string(index), {});
    }

    return arr[index];
}

bool YamlValue::operator==(const YamlValue& other) const {
    if (type_ != other.type_) {
        return false;
    }

    switch (type_) {
        case Type::Null:
            return true;
        case Type::String:
            return as_string() == other.as_string();
        case Type::Number:
            return as_number() == other.as_number();
        case Type::Bool:
            return as_bool() == other.as_bool();
        case Type::Object:
            return as_object() == other.as_object();
        case Type::Array:
            return as_array() == other.as_array();
        case Type::Alias:
            return alias_name() == other.alias_name();
    }

    return false;
}

bool YamlValue::operator!=(const YamlValue& other) const {
    return !(*this == other);
}

bool YamlValue::contains(const std::string& key) const {
    if (type_ != Type::Object) {
        throw YamlException("Not an object", {});
    }

    return as_object().find(key) != as_object().end();
}

auto YamlValue::get(const std::string& key,
                    const YamlValue& default_value) const -> const YamlValue& {
    if (type_ != Type::Object) {
        throw YamlException("Not an object", {});
    }

    const auto& obj = as_object();
    auto it = obj.find(key);
    return it != obj.end() ? it->second : default_value;
}

std::optional<std::reference_wrapper<const YamlValue>> YamlValue::try_get(
    const std::string& key) const {
    if (type_ != Type::Object) {
        throw YamlException("Not an object", {});
    }

    const auto& obj = as_object();
    auto it = obj.find(key);
    if (it != obj.end()) {
        return std::cref(it->second);
    }

    return std::nullopt;
}

size_t YamlValue::size() const {
    if (type_ == Type::Object) {
        return as_object().size();
    } else if (type_ == Type::Array) {
        return as_array().size();
    }

    throw YamlException("Not an object or array", {});
}

bool YamlValue::empty() const {
    if (type_ == Type::Object) {
        return as_object().empty();
    } else if (type_ == Type::Array) {
        return as_array().empty();
    }

    throw YamlException("Not an object or array", {});
}

void YamlValue::clear() {
    if (type_ == Type::Object) {
        std::get<YamlObject>(value_).clear();
    } else if (type_ == Type::Array) {
        std::get<YamlArray>(value_).clear();
    } else {
        throw YamlException("Not an object or array", {});
    }
}

size_t YamlValue::erase(const std::string& key) {
    if (type_ != Type::Object) {
        throw YamlException("Not an object", {});
    }

    return std::get<YamlObject>(value_).erase(key);
}

void YamlValue::erase(size_t index) {
    if (type_ != Type::Array) {
        throw YamlException("Not an array", {});
    }

    auto& arr = std::get<YamlArray>(value_);
    if (index >= arr.size()) {
        throw YamlException("Index out of range: " + std::to_string(index), {});
    }

    arr.erase(arr.begin() + index);
}

// YamlDocument implementation

YamlDocument::YamlDocument(const YamlValue& root) : root_(root) {}

const YamlValue& YamlDocument::root() const { return root_; }

YamlValue& YamlDocument::root() { return root_; }

void YamlDocument::set_root(const YamlValue& root) { root_ = root; }

std::string YamlDocument::to_yaml(const YamlSerializeOptions& options) const {
    std::stringstream ss;

    if (options.explicit_start) {
        ss << "---\n";
    }

    ss << root_.to_yaml(options);

    if (options.explicit_end) {
        ss << "...\n";
    }

    return ss.str();
}

// YamlParser implementation

auto YamlParser::parse(const std::string& str, const YamlParseOptions& options)
    -> YamlValue {
    ParseContext ctx{str, 0, {1, 1}, options};
    skip_whitespace(ctx);

    // Check for document start marker
    if (ctx.index + 3 <= ctx.str.size() &&
        ctx.str.substr(ctx.index, 3) == "---") {
        ctx.index += 3;
        ctx.position.column += 3;
        skip_whitespace(ctx);
    }

    auto result = parse_value(ctx);

    // Check for document end marker
    skip_whitespace(ctx);
    if (ctx.index + 3 <= ctx.str.size() &&
        ctx.str.substr(ctx.index, 3) == "...") {
        ctx.index += 3;
        ctx.position.column += 3;
    }

    return result;
}

auto YamlParser::parse_document(const std::string& str,
                                const YamlParseOptions& options)
    -> YamlDocument {
    return YamlDocument(parse(str, options));
}

auto YamlParser::parse_multi_documents(const std::string& str,
                                       const YamlParseOptions& options)
    -> std::vector<YamlDocument> {
    std::vector<YamlDocument> documents;

    ParseContext ctx{str, 0, {1, 1}, options};
    while (ctx.index < ctx.str.size()) {
        skip_whitespace(ctx);

        if (ctx.index >= ctx.str.size()) {
            break;
        }

        // Check for document start marker
        bool has_start_marker = false;
        if (ctx.index + 3 <= ctx.str.size() &&
            ctx.str.substr(ctx.index, 3) == "---") {
            has_start_marker = true;
            ctx.index += 3;
            ctx.position.column += 3;
            skip_whitespace(ctx);
        }

        // Skip empty documents
        if (has_start_marker && ctx.index + 3 <= ctx.str.size() &&
            (ctx.str.substr(ctx.index, 3) == "---" ||
             ctx.str.substr(ctx.index, 3) == "...")) {
            continue;
        }

        auto value = parse_value(ctx);
        documents.emplace_back(value);

        // Check for document end marker
        skip_whitespace(ctx);
        if (ctx.index + 3 <= ctx.str.size() &&
            ctx.str.substr(ctx.index, 3) == "...") {
            ctx.index += 3;
            ctx.position.column += 3;
            skip_whitespace(ctx);
        }
    }

    return documents;
}

auto YamlParser::parse_value(ParseContext& ctx) -> YamlValue {
    skip_whitespace(ctx);

    if (is_end(ctx)) {
        throw YamlException("Unexpected end of input", ctx.position);
    }

    // Parse tag if present
    YamlTag tag;
    if (ctx.str[ctx.index] == '!') {
        tag = parse_tag(ctx);
        skip_whitespace(ctx);
    }

    // Parse anchor if present
    YamlAnchor anchor;
    if (ctx.str[ctx.index] == '&') {
        anchor = parse_anchor(ctx);
        skip_whitespace(ctx);
    }

    YamlValue result;

    // Parse the value
    if (ctx.str[ctx.index] == '*') {
        // Alias
        result = parse_alias(ctx);
    } else if (ctx.str[ctx.index] == '\'') {
        // Single-quoted string
        ++ctx.index;
        ++ctx.position.column;

        std::string value;
        bool escaped = false;

        while (!is_end(ctx) && (ctx.str[ctx.index] != '\'' || escaped)) {
            if (ctx.str[ctx.index] == '\'' && !escaped) {
                escaped = true;
            } else {
                if (escaped && ctx.str[ctx.index] == '\'') {
                    value += '\'';
                    escaped = false;
                } else {
                    value += ctx.str[ctx.index];
                }
            }

            advance(ctx);
        }

        if (is_end(ctx)) {
            throw YamlException("Unterminated string", ctx.position);
        }

        advance(ctx);  // Skip the closing quote
        result = YamlValue(value);
    } else if (ctx.str[ctx.index] == '"') {
        // Double-quoted string
        result = YamlValue(parse_string(ctx));
    } else if (ctx.str[ctx.index] == '{') {
        // Flow-style object
        result = YamlValue(parse_flow_object(ctx));
    } else if (ctx.str[ctx.index] == '[') {
        // Flow-style array
        result = YamlValue(parse_flow_array(ctx));
    } else if (ctx.str[ctx.index] == '-' && ctx.index + 1 < ctx.str.size() &&
               std::isspace(ctx.str[ctx.index + 1])) {
        // Block sequence
        result = YamlValue(parse_array(ctx));
    } else if (ctx.str[ctx.index] == '|' || ctx.str[ctx.index] == '>') {
        // Block scalar
        char style = ctx.str[ctx.index];
        result = YamlValue(parse_block_scalar(ctx, style));
    } else if (std::isdigit(ctx.str[ctx.index]) ||
               (ctx.str[ctx.index] == '-' && ctx.index + 1 < ctx.str.size() &&
                std::isdigit(ctx.str[ctx.index + 1]))) {
        // Number
        result = YamlValue(parse_number(ctx));
    } else if (ctx.str.substr(ctx.index, 4) == "true") {
        // Boolean true
        result = YamlValue(parse_bool(ctx));
    } else if (ctx.str.substr(ctx.index, 5) == "false") {
        // Boolean false
        result = YamlValue(parse_bool(ctx));
    } else if (ctx.str.substr(ctx.index, 4) == "null") {
        // Null
        parse_null(ctx);
        result = YamlValue();
    } else if (is_first_identifier_char(ctx.str[ctx.index])) {
        // Try to parse as an object or unquoted string
        size_t i = ctx.index;
        while (i < ctx.str.size() && is_identifier_char(ctx.str[i])) {
            ++i;
        }

        if (i < ctx.str.size() && ctx.str[i] == ':') {
            // Object
            result = YamlValue(parse_object(ctx));
        } else {
            // Unquoted string
            std::string value;
            while (ctx.index < ctx.str.size() &&
                   !std::isspace(ctx.str[ctx.index]) &&
                   ctx.str[ctx.index] != ',' && ctx.str[ctx.index] != ']' &&
                   ctx.str[ctx.index] != '}' && ctx.str[ctx.index] != ':') {
                value += ctx.str[ctx.index];
                advance(ctx);
            }

            result = YamlValue(value);
        }
    } else {
        throw YamlException(
            "Unexpected character: " + std::string(1, ctx.str[ctx.index]),
            ctx.position);
    }

    result.set_tag(tag);
    result.set_anchor(anchor);

    // Register anchor
    if (anchor.has_name() && ctx.options.support_anchors) {
        ctx.anchors[anchor.name()] = std::make_shared<YamlValue>(result);
    }

    return result;
}

auto YamlParser::parse_string(ParseContext& ctx) -> std::string {
    if (ctx.str[ctx.index] != '"') {
        throw YamlException("Expected '\"'", ctx.position);
    }

    advance(ctx);

    std::string result;
    bool escape = false;

    while (!is_end(ctx) && (ctx.str[ctx.index] != '"' || escape)) {
        if (escape) {
            switch (ctx.str[ctx.index]) {
                case '"':
                    result += '"';
                    break;
                case '\\':
                    result += '\\';
                    break;
                case '/':
                    result += '/';
                    break;
                case 'b':
                    result += '\b';
                    break;
                case 'f':
                    result += '\f';
                    break;
                case 'n':
                    result += '\n';
                    break;
                case 'r':
                    result += '\r';
                    break;
                case 't':
                    result += '\t';
                    break;
                case 'u': {
                    if (ctx.index + 4 >= ctx.str.size()) {
                        throw YamlException(
                            "Incomplete Unicode escape sequence", ctx.position);
                    }

                    std::string hex = ctx.str.substr(ctx.index + 1, 4);
                    for (char c : hex) {
                        if (!((c >= '0' && c <= '9') ||
                              (c >= 'a' && c <= 'f') ||
                              (c >= 'A' && c <= 'F'))) {
                            throw YamlException(
                                "Invalid Unicode escape sequence",
                                ctx.position);
                        }
                    }

                    uint32_t code_point = std::stoul(hex, nullptr, 16);

                    // Convert UTF-16 surrogate pair to UTF-8
                    if (code_point >= 0xD800 && code_point <= 0xDBFF) {
                        // High surrogate, need to get the low surrogate
                        if (ctx.index + 10 >= ctx.str.size() ||
                            ctx.str[ctx.index + 5] != '\\' ||
                            ctx.str[ctx.index + 6] != 'u') {
                            throw YamlException("Incomplete surrogate pair",
                                                ctx.position);
                        }

                        std::string low_hex = ctx.str.substr(ctx.index + 7, 4);
                        for (char c : low_hex) {
                            if (!((c >= '0' && c <= '9') ||
                                  (c >= 'a' && c <= 'f') ||
                                  (c >= 'A' && c <= 'F'))) {
                                throw YamlException(
                                    "Invalid Unicode escape sequence",
                                    ctx.position);
                            }
                        }

                        uint32_t low_code_point =
                            std::stoul(low_hex, nullptr, 16);
                        if (low_code_point < 0xDC00 ||
                            low_code_point > 0xDFFF) {
                            throw YamlException("Invalid surrogate pair",
                                                ctx.position);
                        }

                        code_point = 0x10000 + ((code_point - 0xD800) << 10) +
                                     (low_code_point - 0xDC00);
                        ctx.index += 5;  // Skip the low surrogate
                    }

                    // Convert to UTF-8
                    if (code_point < 0x80) {
                        result += static_cast<char>(code_point);
                    } else if (code_point < 0x800) {
                        result += static_cast<char>(0xC0 | (code_point >> 6));
                        result += static_cast<char>(0x80 | (code_point & 0x3F));
                    } else if (code_point < 0x10000) {
                        result += static_cast<char>(0xE0 | (code_point >> 12));
                        result += static_cast<char>(0x80 |
                                                    ((code_point >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (code_point & 0x3F));
                    } else {
                        result += static_cast<char>(0xF0 | (code_point >> 18));
                        result += static_cast<char>(
                            0x80 | ((code_point >> 12) & 0x3F));
                        result += static_cast<char>(0x80 |
                                                    ((code_point >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (code_point & 0x3F));
                    }

                    ctx.index += 4;  // Skip the hex digits
                    ctx.position.column += 4;
                    break;
                }
                default:
                    result += ctx.str[ctx.index];
                    break;
            }
            escape = false;
        } else if (ctx.str[ctx.index] == '\\') {
            escape = true;
        } else {
            result += ctx.str[ctx.index];
        }

        advance(ctx);
    }

    if (is_end(ctx)) {
        throw YamlException("Unterminated string", ctx.position);
    }

    advance(ctx);  // Skip the closing quote

    return result;
}

auto YamlParser::parse_number(ParseContext& ctx) -> double {
    size_t start_index = ctx.index;
    YamlPosition start_position = ctx.position;

    // Parse sign
    if (ctx.str[ctx.index] == '-') {
        advance(ctx);
    }

    // Parse integer part
    bool has_digits = false;
    while (!is_end(ctx) && std::isdigit(ctx.str[ctx.index])) {
        has_digits = true;
        advance(ctx);
    }

    // Parse decimal part
    if (!is_end(ctx) && ctx.str[ctx.index] == '.') {
        advance(ctx);

        while (!is_end(ctx) && std::isdigit(ctx.str[ctx.index])) {
            has_digits = true;
            advance(ctx);
        }
    }

    // Parse exponent
    if (!is_end(ctx) &&
        (ctx.str[ctx.index] == 'e' || ctx.str[ctx.index] == 'E')) {
        advance(ctx);

        if (!is_end(ctx) &&
            (ctx.str[ctx.index] == '+' || ctx.str[ctx.index] == '-')) {
            advance(ctx);
        }

        bool has_exponent_digits = false;
        while (!is_end(ctx) && std::isdigit(ctx.str[ctx.index])) {
            has_exponent_digits = true;
            advance(ctx);
        }

        if (!has_exponent_digits) {
            throw YamlException("Invalid number: missing exponent digits",
                                start_position);
        }
    }

    if (!has_digits) {
        throw YamlException("Invalid number: no digits", start_position);
    }

    // Special keywords
    std::string num_str = ctx.str.substr(start_index, ctx.index - start_index);
    if (num_str == ".inf" || num_str == ".Inf" || num_str == ".INF") {
        return std::numeric_limits<double>::infinity();
    } else if (num_str == "-.inf" || num_str == "-.Inf" || num_str == "-.INF") {
        return -std::numeric_limits<double>::infinity();
    } else if (num_str == ".nan" || num_str == ".NaN" || num_str == ".NAN") {
        return std::numeric_limits<double>::quiet_NaN();
    }

    try {
        return std::stod(num_str);
    } catch (const std::exception& e) {
        throw YamlException("Invalid number: " + num_str, start_position);
    }
}

auto YamlParser::parse_bool(ParseContext& ctx) -> bool {
    if (ctx.str.substr(ctx.index, 4) == "true") {
        ctx.index += 4;
        ctx.position.column += 4;
        return true;
    } else if (ctx.str.substr(ctx.index, 5) == "false") {
        ctx.index += 5;
        ctx.position.column += 5;
        return false;
    }

    throw YamlException("Invalid boolean", ctx.position);
}

void YamlParser::parse_null(ParseContext& ctx) {
    if (ctx.str.substr(ctx.index, 4) == "null") {
        ctx.index += 4;
        ctx.position.column += 4;
    } else {
        throw YamlException("Invalid null", ctx.position);
    }
}

auto YamlParser::parse_object(ParseContext& ctx) -> YamlObject {
    YamlObject obj;
    bool first = true;

    while (!is_end(ctx)) {
        skip_whitespace(ctx);

        if (!first && (is_end(ctx) || ctx.str[ctx.index] != '-')) {
            break;
        }

        // Parse key
        std::string key;
        if (ctx.str[ctx.index] == '"') {
            key = parse_string(ctx);
        } else if (ctx.str[ctx.index] == '\'') {
            // Single-quoted string
            ++ctx.index;
            ++ctx.position.column;

            bool escaped = false;
            while (!is_end(ctx) && (ctx.str[ctx.index] != '\'' || escaped)) {
                if (ctx.str[ctx.index] == '\'' && !escaped) {
                    escaped = true;
                } else {
                    if (escaped && ctx.str[ctx.index] == '\'') {
                        key += '\'';
                        escaped = false;
                    } else {
                        key += ctx.str[ctx.index];
                    }
                }

                advance(ctx);
            }

            if (is_end(ctx)) {
                throw YamlException("Unterminated string", ctx.position);
            }

            advance(ctx);  // Skip the closing quote
        } else {
            // Unquoted string
            while (!is_end(ctx) && is_identifier_char(ctx.str[ctx.index])) {
                key += ctx.str[ctx.index];
                advance(ctx);
            }
        }

        skip_whitespace(ctx);

        if (is_end(ctx) || ctx.str[ctx.index] != ':') {
            throw YamlException("Expected ':'", ctx.position);
        }

        advance(ctx);  // Skip the colon

        // Parse value
        skip_whitespace(ctx);
        YamlValue value = parse_value(ctx);

        if (!ctx.options.allow_duplicate_keys && obj.find(key) != obj.end()) {
            throw YamlException("Duplicate key: " + key, ctx.position);
        }

        obj[key] = value;
        first = false;

        // Skip to next key
        skip_whitespace(ctx);
        if (!is_end(ctx) && ctx.str[ctx.index] == ',') {
            advance(ctx);  // Skip the comma
        }
    }

    return obj;
}

auto YamlParser::parse_array(ParseContext& ctx) -> YamlArray {
    YamlArray arr;

    while (!is_end(ctx)) {
        skip_whitespace(ctx);

        if (is_end(ctx) || ctx.str[ctx.index] != '-') {
            break;
        }

        advance(ctx);  // Skip the dash

        if (is_end(ctx) || !std::isspace(ctx.str[ctx.index])) {
            throw YamlException("Expected space after '-'", ctx.position);
        }

        skip_whitespace(ctx);
        arr.push_back(parse_value(ctx));
    }

    return arr;
}

auto YamlParser::parse_flow_object(ParseContext& ctx) -> YamlObject {
    if (ctx.str[ctx.index] != '{') {
        throw YamlException("Expected '{'", ctx.position);
    }

    advance(ctx);  // Skip the opening brace
    skip_whitespace(ctx);

    YamlObject obj;

    if (!is_end(ctx) && ctx.str[ctx.index] == '}') {
        advance(ctx);  // Skip the closing brace for empty object
        return obj;
    }

    while (!is_end(ctx)) {
        skip_whitespace(ctx);

        // Parse key
        std::string key;
        if (ctx.str[ctx.index] == '"') {
            key = parse_string(ctx);
        } else if (ctx.str[ctx.index] == '\'') {
            // Single-quoted string
            ++ctx.index;
            ++ctx.position.column;

            bool escaped = false;
            while (!is_end(ctx) && (ctx.str[ctx.index] != '\'' || escaped)) {
                if (ctx.str[ctx.index] == '\'' && !escaped) {
                    escaped = true;
                } else {
                    if (escaped && ctx.str[ctx.index] == '\'') {
                        key += '\'';
                        escaped = false;
                    } else {
                        key += ctx.str[ctx.index];
                    }
                }

                advance(ctx);
            }

            if (is_end(ctx)) {
                throw YamlException("Unterminated string", ctx.position);
            }

            advance(ctx);  // Skip the closing quote
        } else {
            // Unquoted string
            while (!is_end(ctx) && is_identifier_char(ctx.str[ctx.index])) {
                key += ctx.str[ctx.index];
                advance(ctx);
            }
        }

        skip_whitespace(ctx);

        if (is_end(ctx) || ctx.str[ctx.index] != ':') {
            throw YamlException("Expected ':'", ctx.position);
        }

        advance(ctx);  // Skip the colon
        skip_whitespace(ctx);

        // Parse value
        YamlValue value = parse_value(ctx);

        if (!ctx.options.allow_duplicate_keys && obj.find(key) != obj.end()) {
            throw YamlException("Duplicate key: " + key, ctx.position);
        }

        obj[key] = value;

        skip_whitespace(ctx);

        if (is_end(ctx)) {
            throw YamlException("Unterminated object", ctx.position);
        }

        if (ctx.str[ctx.index] == '}') {
            advance(ctx);  // Skip the closing brace
            break;
        }

        if (ctx.str[ctx.index] != ',') {
            throw YamlException("Expected ',' or '}'", ctx.position);
        }

        advance(ctx);  // Skip the comma
    }

    return obj;
}

auto YamlParser::parse_flow_array(ParseContext& ctx) -> YamlArray {
    if (ctx.str[ctx.index] != '[') {
        throw YamlException("Expected '['", ctx.position);
    }

    advance(ctx);  // Skip the opening bracket
    skip_whitespace(ctx);

    YamlArray arr;

    if (!is_end(ctx) && ctx.str[ctx.index] == ']') {
        advance(ctx);  // Skip the closing bracket for empty array
        return arr;
    }

    while (!is_end(ctx)) {
        skip_whitespace(ctx);

        // Parse value
        arr.push_back(parse_value(ctx));

        skip_whitespace(ctx);

        if (is_end(ctx)) {
            throw YamlException("Unterminated array", ctx.position);
        }

        if (ctx.str[ctx.index] == ']') {
            advance(ctx);  // Skip the closing bracket
            break;
        }

        if (ctx.str[ctx.index] != ',') {
            throw YamlException("Expected ',' or ']'", ctx.position);
        }

        advance(ctx);  // Skip the comma
    }

    return arr;
}

auto YamlParser::parse_block_scalar(ParseContext& ctx, char style)
    -> std::string {
    if (ctx.str[ctx.index] != '|' && ctx.str[ctx.index] != '>') {
        throw YamlException("Expected '|' or '>'", ctx.position);
    }

    char block_style = ctx.str[ctx.index];
    advance(ctx);  // Skip the style indicator

    // Parse header (optional)
    int indent = -1;
    bool keep_newlines = true;

    if (!is_end(ctx) &&
        (ctx.str[ctx.index] == '+' || ctx.str[ctx.index] == '-')) {
        if (ctx.str[ctx.index] == '-') {
            keep_newlines = false;
        }
        advance(ctx);
    }

    if (!is_end(ctx) && std::isdigit(ctx.str[ctx.index])) {
        indent = ctx.str[ctx.index] - '0';
        advance(ctx);
    }

    // Skip to the end of the line
    while (!is_end(ctx) && ctx.str[ctx.index] != '\n') {
        advance(ctx);
    }

    if (is_end(ctx)) {
        return "";
    }

    advance(ctx);  // Skip the newline
    ++ctx.position.line;
    ctx.position.column = 1;

    // Determine the indentation level
    if (indent < 0) {
        indent = 0;
        size_t i = ctx.index;
        while (i < ctx.str.size() && ctx.str[i] == ' ') {
            ++indent;
            ++i;
        }
    }

    // Parse the block scalar
    std::string result;
    int current_indent = 0;
    std::string line;

    while (!is_end(ctx)) {
        // Count indentation
        current_indent = 0;
        while (!is_end(ctx) && ctx.str[ctx.index] == ' ') {
            ++current_indent;
            advance(ctx);
        }

        // Check if we're outside the block
        if (current_indent < indent && !is_end(ctx) &&
            ctx.str[ctx.index] != '\n') {
            break;
        }

        // Parse the line
        line.clear();
        while (!is_end(ctx) && ctx.str[ctx.index] != '\n') {
            line += ctx.str[ctx.index];
            advance(ctx);
        }

        if (is_end(ctx)) {
            result += line;
            break;
        }

        advance(ctx);  // Skip the newline
        ++ctx.position.line;
        ctx.position.column = 1;

        // Add the line to the result
        if (block_style == '|') {
            result += line;
            if (keep_newlines || !is_end(ctx)) {
                result += '\n';
            }
        } else if (block_style == '>') {
            if (!line.empty()) {
                if (!result.empty() && result.back() != '\n') {
                    result += ' ';
                }
                result += line;
            }
            if (keep_newlines || (current_indent < indent && !is_end(ctx) &&
                                  ctx.str[ctx.index] != '\n')) {
                result += '\n';
            }
        }
    }

    return result;
}

auto YamlParser::parse_tag(ParseContext& ctx) -> YamlTag {
    if (ctx.str[ctx.index] != '!') {
        throw YamlException("Expected '!'", ctx.position);
    }

    advance(ctx);  // Skip the '!'

    std::string tag = "!";

    if (!is_end(ctx) && ctx.str[ctx.index] == '!') {
        advance(ctx);  // Skip the second '!'
        tag += "!";
    }

    while (!is_end(ctx) && !std::isspace(ctx.str[ctx.index]) &&
           ctx.str[ctx.index] != ':' && ctx.str[ctx.index] != ',') {
        tag += ctx.str[ctx.index];
        advance(ctx);
    }

    return YamlTag(tag);
}

auto YamlParser::parse_anchor(ParseContext& ctx) -> YamlAnchor {
    if (ctx.str[ctx.index] != '&') {
        throw YamlException("Expected '&'", ctx.position);
    }

    advance(ctx);  // Skip the '&'

    std::string name;
    while (!is_end(ctx) && !std::isspace(ctx.str[ctx.index]) &&
           ctx.str[ctx.index] != ':' && ctx.str[ctx.index] != ',') {
        name += ctx.str[ctx.index];
        advance(ctx);
    }

    return YamlAnchor(name);
}

auto YamlParser::parse_alias(ParseContext& ctx) -> YamlValue {
    if (ctx.str[ctx.index] != '*') {
        throw YamlException("Expected '*'", ctx.position);
    }

    advance(ctx);  // Skip the '*'

    std::string name;
    while (!is_end(ctx) && !std::isspace(ctx.str[ctx.index]) &&
           ctx.str[ctx.index] != ':' && ctx.str[ctx.index] != ',') {
        name += ctx.str[ctx.index];
        advance(ctx);
    }

    if (!ctx.options.support_anchors) {
        throw YamlException("Anchor support is disabled", ctx.position);
    }

    auto it = ctx.anchors.find(name);
    if (it == ctx.anchors.end()) {
        throw YamlException("Alias not found: " + name, ctx.position);
    }

    return *it->second;
}

void YamlParser::skip_whitespace(ParseContext& ctx) {
    while (!is_end(ctx)) {
        if (std::isspace(ctx.str[ctx.index])) {
            if (ctx.str[ctx.index] == '\n') {
                ++ctx.position.line;
                ctx.position.column = 0;
            }
            advance(ctx);
        } else if (ctx.options.support_comments && ctx.str[ctx.index] == '#') {
            skip_comment(ctx);
        } else {
            break;
        }
    }
}

void YamlParser::skip_comment(ParseContext& ctx) {
    if (ctx.str[ctx.index] != '#') {
        return;
    }

    while (!is_end(ctx) && ctx.str[ctx.index] != '\n') {
        advance(ctx);
    }
}

void YamlParser::advance(ParseContext& ctx) {
    ++ctx.index;
    ++ctx.position.column;
}

bool YamlParser::match(ParseContext& ctx, char expected) {
    if (is_end(ctx) || ctx.str[ctx.index] != expected) {
        return false;
    }

    advance(ctx);
    return true;
}

void YamlParser::expect(ParseContext& ctx, char expected) {
    if (is_end(ctx)) {
        throw YamlException("Unexpected end of input", ctx.position);
    }

    if (ctx.str[ctx.index] != expected) {
        throw YamlException(
            "Expected '" + std::string(1, expected) + "', got '" +
                (is_end(ctx) ? "EOF" : std::string(1, ctx.str[ctx.index])) +
                "'",
            ctx.position);
    }

    advance(ctx);
}

bool YamlParser::is_end(const ParseContext& ctx) {
    return ctx.index >= ctx.str.size();
}

bool YamlParser::is_identifier_char(char c) {
    return std::isalnum(c) || c == '_' || c == '-';
}

bool YamlParser::is_first_identifier_char(char c) {
    return std::isalpha(c) || c == '_';
}

}  // namespace atom::type
