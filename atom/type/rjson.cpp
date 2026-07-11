#include "rjson.hpp"
#include <cmath>
#include <sstream>
#include <stdexcept>
#include "atom/error/exception.hpp"

namespace atom::type {

// JsonValue Constructors
JsonValue::JsonValue() : type_(Type::Null), value_(nullptr) {}
JsonValue::JsonValue(const std::string& value)
    : type_(Type::String), value_(value) {}
JsonValue::JsonValue(const char* value)
    : type_(Type::String), value_(std::string(value)) {}
JsonValue::JsonValue(double value) : type_(Type::Number), value_(value) {}
JsonValue::JsonValue(int value)
    : type_(Type::Number), value_(static_cast<double>(value)) {}
JsonValue::JsonValue(bool value) : type_(Type::Bool), value_(value) {}
JsonValue::JsonValue(const JsonObject& value)
    : type_(Type::Object), value_(value) {}
JsonValue::JsonValue(const JsonArray& value)
    : type_(Type::Array), value_(value) {}

// Accessors for JsonValue types
auto JsonValue::type() const -> Type { return type_; }

auto JsonValue::as_string() const -> const std::string& {
    if (type_ != Type::String) {
        THROW_INVALID_ARGUMENT("Not a string");
    }
    return std::get<std::string>(value_);
}

auto JsonValue::as_number() const -> double {
    if (type_ != Type::Number) {
        THROW_INVALID_ARGUMENT("Not a number");
    }
    return std::get<double>(value_);
}

auto JsonValue::as_bool() const -> bool {
    if (type_ != Type::Bool) {
        THROW_INVALID_ARGUMENT("Not a bool");
    }
    return std::get<bool>(value_);
}

auto JsonValue::as_object() const -> const JsonObject& {
    if (type_ != Type::Object) {
        THROW_INVALID_ARGUMENT("Not an object");
    }
    return std::get<JsonObject>(value_);
}

auto JsonValue::as_array() const -> const JsonArray& {
    if (type_ != Type::Array) {
        THROW_INVALID_ARGUMENT("Not an array");
    }
    return std::get<JsonArray>(value_);
}

auto JsonValue::to_string() const -> std::string {
    switch (type_) {
        case Type::Null:
            return "null";
        case Type::String: {
            std::string escaped = as_string();
            std::string result = "\"";
            for (char c : escaped) {
                switch (c) {
                    case '"':
                        result += "\\\"";
                        break;
                    case '\\':
                        result += "\\\\";
                        break;
                    case '\b':
                        result += "\\b";
                        break;
                    case '\f':
                        result += "\\f";
                        break;
                    case '\n':
                        result += "\\n";
                        break;
                    case '\r':
                        result += "\\r";
                        break;
                    case '\t':
                        result += "\\t";
                        break;
                    default:
                        result += c;
                        break;
                }
            }
            result += "\"";
            return result;
        }
        case Type::Number: {
            double num = as_number();
            // Check if the number is an integer
            if (num == std::floor(num)) {
                return std::to_string(static_cast<long long>(num));
            } else {
                std::string str = std::to_string(num);
                // Remove trailing zeros after decimal point
                str.erase(str.find_last_not_of('0') + 1, std::string::npos);
                str.erase(str.find_last_not_of('.') + 1, std::string::npos);
                return str;
            }
        }
        case Type::Bool:
            return as_bool() ? "true" : "false";
        case Type::Object: {
            std::string result = "{";
            const auto& obj = as_object();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (it != obj.begin()) {
                    result += ",";
                }
                result += "\"" + it->first + "\":" + it->second.to_string();
            }
            result += "}";
            return result;
        }
        case Type::Array: {
            std::string result = "[";
            const auto& arr = as_array();
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0)
                    result += ",";
                result += arr[i].to_string();
            }
            result += "]";
            return result;
        }
    }
    THROW_INVALID_ARGUMENT("Unknown type");
}

// Overloaded operators for object and array access
auto JsonValue::operator[](const std::string& key) const -> const JsonValue& {
    if (type_ != Type::Object) {
        THROW_INVALID_ARGUMENT("Not an object");
    }
    return as_object().at(key);
}

auto JsonValue::operator[](size_t index) const -> const JsonValue& {
    if (type_ != Type::Array) {
        THROW_INVALID_ARGUMENT("Not an array");
    }
    return as_array().at(index);
}

// JsonParser Implementation
auto JsonParser::parse(const std::string& str) -> JsonValue {
    size_t index = 0;
    return parseValue(str, index);
}

auto JsonParser::parseValue(const std::string& str,
                            size_t& index) -> JsonValue {
    skipWhitespace(str, index);
    if (index >= str.size()) {
        THROW_INVALID_ARGUMENT("Unexpected end of JSON input");
    }
    if (str[index] == '"') {
        return JsonValue(parseString(str, index));
    }
    if (str[index] == 't' || str[index] == 'f') {
        return JsonValue(parseBool(str, index));
    }
    if (str[index] == 'n') {
        parseNull(str, index);
        return {};
    }
    if (str[index] == '{') {
        return JsonValue(parseObject(str, index));
    }
    if (str[index] == '[') {
        return JsonValue(parseArray(str, index));
    }
    if ((std::isdigit(str[index]) != 0) || str[index] == '-') {
        return JsonValue(parseNumber(str, index));
    }
    THROW_INVALID_ARGUMENT("Invalid JSON value");
}

auto JsonParser::parseString(const std::string& str,
                             size_t& index) -> std::string {
    ++index;  // Skip opening quote
    std::string result;
    while (index < str.size() && str[index] != '"') {
        if (str[index] == '\\') {
            result += parseEscapedChar(str, index);
        } else {
            result += str[index++];
        }
    }
    if (index >= str.size()) {
        THROW_INVALID_ARGUMENT("Unterminated string in JSON");
    }
    ++index;  // Skip closing quote
    return result;
}

auto JsonParser::parseEscapedChar(const std::string& str,
                                  size_t& index) -> char {
    ++index;  // Skip backslash
    if (index >= str.size()) {
        THROW_INVALID_ARGUMENT(
            "Unexpected end of JSON input in escape sequence");
    }
    switch (str[index++]) {
        case '"':
            return '"';
        case '\\':
            return '\\';
        case '/':
            return '/';
        case 'b':
            return '\b';
        case 'f':
            return '\f';
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        default:
            THROW_INVALID_ARGUMENT("Invalid escape sequence");
    }
}

auto JsonParser::parseNumber(const std::string& str, size_t& index) -> double {
    size_t startIndex = index;
    bool hasDecimal = false;

    if (index >= str.size()) {
        THROW_INVALID_ARGUMENT("Unexpected end of JSON input in number");
    }

    if (str[index] == '-') {
        ++index;
        if (index >= str.size()) {
            THROW_INVALID_ARGUMENT("Unexpected end of JSON input after '-'");
        }
    }

    while (index < str.size() &&
           ((std::isdigit(str[index]) != 0) || str[index] == '.')) {
        if (str[index] == '.') {
            if (hasDecimal) {
                THROW_INVALID_ARGUMENT(
                    "Invalid number format: multiple decimal points");
            }
            hasDecimal = true;
            ++index;
            // After decimal point, there must be at least one digit
            if (index >= str.size() || (std::isdigit(str[index]) == 0)) {
                THROW_INVALID_ARGUMENT(
                    "Invalid number format: decimal point must be followed by "
                    "digits");
            }
        } else {
            ++index;
        }
    }

    // Check for scientific notation (e or E)
    if (index < str.size() && (str[index] == 'e' || str[index] == 'E')) {
        ++index;
        if (index < str.size() && (str[index] == '+' || str[index] == '-')) {
            ++index;
        }
        while (index < str.size() && (std::isdigit(str[index]) != 0)) {
            ++index;
        }
    }

    try {
        return std::stod(str.substr(startIndex, index - startIndex));
    } catch (const std::invalid_argument&) {
        THROW_INVALID_ARGUMENT("Invalid number format");
    } catch (const std::out_of_range&) {
        THROW_INVALID_ARGUMENT("Number out of range");
    }
}

auto JsonParser::parseBool(const std::string& str, size_t& index) -> bool {
    if (index + 4 <= str.size() && str.substr(index, 4) == "true") {
        index += 4;
        return true;
    }
    if (index + 5 <= str.size() && str.substr(index, 5) == "false") {
        index += 5;
        return false;
    }
    THROW_INVALID_ARGUMENT("Invalid boolean value");
}

void JsonParser::parseNull(const std::string& str, size_t& index) {
    if (index + 4 <= str.size() && str.substr(index, 4) == "null") {
        index += 4;
    } else {
        THROW_INVALID_ARGUMENT("Invalid null value");
    }
}

auto JsonParser::parseObject(const std::string& str,
                             size_t& index) -> JsonObject {
    ++index;  // Skip opening '{'
    JsonObject obj;
    skipWhitespace(str, index);

    if (index >= str.size()) {
        THROW_INVALID_ARGUMENT("Unexpected end of JSON input in object");
    }

    while (str[index] != '}') {
        if (str[index] != '"') {
            THROW_INVALID_ARGUMENT("Expected string key in JSON object");
        }
        std::string key = parseString(str, index);
        skipWhitespace(str, index);

        if (index >= str.size()) {
            THROW_INVALID_ARGUMENT("Unexpected end of JSON input in object");
        }

        if (str[index] != ':') {
            THROW_INVALID_ARGUMENT("Expected ':' after key in JSON object");
        }
        ++index;  // Skip ':'
        skipWhitespace(str, index);

        JsonValue value = parseValue(str, index);
        obj[key] = value;
        skipWhitespace(str, index);

        if (index >= str.size()) {
            THROW_INVALID_ARGUMENT("Unexpected end of JSON input in object");
        }

        if (str[index] == ',') {
            ++index;  // Skip comma and continue
        } else if (str[index] != '}') {
            THROW_INVALID_ARGUMENT("Expected ',' or '}' in JSON object");
        }
        skipWhitespace(str, index);

        if (index >= str.size()) {
            THROW_INVALID_ARGUMENT("Unexpected end of JSON input in object");
        }
    }
    ++index;  // Skip closing '}'
    return obj;
}

auto JsonParser::parseArray(const std::string& str,
                            size_t& index) -> JsonArray {
    ++index;  // Skip opening '['
    JsonArray arr;
    skipWhitespace(str, index);

    if (index >= str.size()) {
        THROW_INVALID_ARGUMENT("Unexpected end of JSON input in array");
    }

    while (str[index] != ']') {
        arr.push_back(parseValue(str, index));
        skipWhitespace(str, index);

        if (index >= str.size()) {
            THROW_INVALID_ARGUMENT("Unexpected end of JSON input in array");
        }

        if (str[index] == ',') {
            ++index;  // Skip comma and continue
        } else if (str[index] != ']') {
            THROW_INVALID_ARGUMENT("Expected ',' or ']' in JSON array");
        }
        skipWhitespace(str, index);

        if (index >= str.size()) {
            THROW_INVALID_ARGUMENT("Unexpected end of JSON input in array");
        }
    }
    ++index;  // Skip closing ']'
    return arr;
}

void JsonParser::skipWhitespace(const std::string& str, size_t& index) {
    while (index < str.size() && (std::isspace(str[index]) != 0)) {
        ++index;
    }
}

}  // namespace atom::type
