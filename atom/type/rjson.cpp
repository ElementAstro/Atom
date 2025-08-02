#include "rjson.hpp"
#include <cmath>
#include <stdexcept>
#include <variant>

namespace atom::type {

// JsonValue Constructors
JsonValue::JsonValue() : type_(Type::Null), value_(nullptr) {}
JsonValue::JsonValue(const std::string& value)
    : type_(Type::String), value_(value) {}
JsonValue::JsonValue(double value) : type_(Type::Number), value_(value) {}
JsonValue::JsonValue(bool value) : type_(Type::Bool), value_(value) {}
JsonValue::JsonValue(const JsonObject& value)
    : type_(Type::Object), value_(value) {}
JsonValue::JsonValue(const JsonArray& value)
    : type_(Type::Array), value_(value) {}

// Accessors for JsonValue types
auto JsonValue::type() const -> Type { return type_; }

auto JsonValue::asString() const -> const std::string& {
    if (type_ != Type::String) {
        throw std::bad_variant_access{};
    }
    return std::get<std::string>(value_);
}

auto JsonValue::asNumber() const -> double {
    if (type_ != Type::Number) {
        throw std::bad_variant_access{};
    }
    return std::get<double>(value_);
}

auto JsonValue::asBool() const -> bool {
    if (type_ != Type::Bool) {
        throw std::bad_variant_access{};
    }
    return std::get<bool>(value_);
}

auto JsonValue::asObject() const -> const JsonObject& {
    if (type_ != Type::Object) {
        throw std::bad_variant_access{};
    }
    return std::get<JsonObject>(value_);
}

auto JsonValue::asArray() const -> const JsonArray& {
    if (type_ != Type::Array) {
        throw std::bad_variant_access{};
    }
    return std::get<JsonArray>(value_);
}

auto JsonValue::toString() const -> std::string {
    switch (type_) {
        case Type::Null:
            return "null";
        case Type::String: {
            std::string escaped = asString();
            // Escape special characters
            std::string result = "\"";
            for (char c : escaped) {
                switch (c) {
                    case '"': result += "\\\""; break;
                    case '\\': result += "\\\\"; break;
                    case '\n': result += "\\n"; break;
                    case '\r': result += "\\r"; break;
                    case '\t': result += "\\t"; break;
                    default: result += c; break;
                }
            }
            result += "\"";
            return result;
        }
        case Type::Number: {
            double num = asNumber();
            // Check if it's an integer
            if (num == std::floor(num) && std::isfinite(num)) {
                return std::to_string(static_cast<long long>(num));
            } else {
                // Use a more precise formatting for floating point
                std::string result = std::to_string(num);
                // Remove trailing zeros after decimal point
                result.erase(result.find_last_not_of('0') + 1, std::string::npos);
                result.erase(result.find_last_not_of('.') + 1, std::string::npos);
                return result;
            }
        }
        case Type::Bool:
            return asBool() ? "true" : "false";
        case Type::Object: {
            std::string result = "{";
            const auto& obj = asObject();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (it != obj.begin()) {
                    result += ",";
                }
                result += "\"" + it->first + "\":" + it->second.toString();
            }
            result += "}";
            return result;
        }
        case Type::Array: {
            std::string result = "[";
            const auto& arr = asArray();
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0)
                    result += ",";
                result += arr[i].toString();
            }
            result += "]";
            return result;
        }
    }
    throw std::runtime_error("Unknown type");
}

// Overloaded operators for object and array access
auto JsonValue::operator[](const std::string& key) const -> const JsonValue& {
    if (type_ != Type::Object) {
        throw std::bad_variant_access{};
    }
    return asObject().at(key);
}

auto JsonValue::operator[](size_t index) const -> const JsonValue& {
    if (type_ != Type::Array) {
        throw std::bad_variant_access{};
    }
    return asArray().at(index);
}

// JsonParser Implementation
auto JsonParser::parse(const std::string& str) -> JsonValue {
    size_t index = 0;
    return parseValue(str, index);
}

auto JsonParser::parseValue(const std::string& str,
                            size_t& index) -> JsonValue {
    skipWhitespace(str, index);
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
    throw std::runtime_error("Invalid JSON value");
}

auto JsonParser::parseString(const std::string& str,
                             size_t& index) -> std::string {
    ++index;  // Skip opening quote
    std::string result;
    while (index < str.length() && str[index] != '"') {
        if (str[index] == '\\') {
            result += parseEscapedChar(str, index);
        } else {
            result += str[index++];
        }
    }
    if (index >= str.length()) {
        throw std::runtime_error("Unterminated string");
    }
    ++index;  // Skip closing quote
    return result;
}

auto JsonParser::parseEscapedChar(const std::string& str,
                                  size_t& index) -> char {
    ++index;  // Skip backslash
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
            throw std::runtime_error("Invalid escape sequence");
    }
}

auto JsonParser::parseNumber(const std::string& str, size_t& index) -> double {
    size_t startIndex = index;
    bool hasDecimal = false;
    bool hasDigitsAfterDecimal = false;

    if (str[index] == '-') {
        ++index;
    }

    // Must have at least one digit before decimal point or after minus sign
    if (index >= str.length() || !std::isdigit(str[index])) {
        throw std::runtime_error("Invalid number format: no digits");
    }

    while (index < str.length() && ((std::isdigit(str[index]) != 0) || str[index] == '.')) {
        if (str[index] == '.') {
            if (hasDecimal) {
                throw std::runtime_error(
                    "Invalid number format: multiple decimal points");
            }
            hasDecimal = true;
            ++index;
            // Check if there are digits after decimal point
            if (index < str.length() && std::isdigit(str[index])) {
                hasDigitsAfterDecimal = true;
            }
        } else {
            if (hasDecimal) {
                hasDigitsAfterDecimal = true;
            }
            ++index;
        }
    }

    // If we have a decimal point, we must have digits after it
    if (hasDecimal && !hasDigitsAfterDecimal) {
        throw std::runtime_error("Invalid number format: no digits after decimal point");
    }

    // Check for scientific notation (e or E)
    if (index < str.length() && (str[index] == 'e' || str[index] == 'E')) {
        ++index;
        if (index < str.length() && (str[index] == '+' || str[index] == '-')) {
            ++index;
        }
        // Must have at least one digit in exponent
        if (index >= str.length() || !std::isdigit(str[index])) {
            throw std::runtime_error("Invalid number format: no digits in exponent");
        }
        while (index < str.length() && std::isdigit(str[index]) != 0) {
            ++index;
        }
    }

    try {
        return std::stod(str.substr(startIndex, index - startIndex));
    } catch (const std::invalid_argument&) {
        throw std::runtime_error("Invalid number format");
    } catch (const std::out_of_range&) {
        throw std::runtime_error("Number out of range");
    }
}

auto JsonParser::parseBool(const std::string& str, size_t& index) -> bool {
    if (str.substr(index, 4) == "true") {
        index += 4;
        return true;
    }
    if (str.substr(index, 5) == "false") {
        index += 5;
        return false;
    }
    throw std::runtime_error("Invalid boolean value");
}

void JsonParser::parseNull(const std::string& str, size_t& index) {
    if (str.substr(index, 4) == "null") {
        index += 4;
    } else {
        throw std::runtime_error("Invalid null value");
    }
}

auto JsonParser::parseObject(const std::string& str,
                             size_t& index) -> JsonObject {
    ++index;  // Skip opening '{'
    JsonObject obj;
    skipWhitespace(str, index);

    while (str[index] != '}') {
        if (str[index] != '"') {
            throw std::runtime_error("Expected string key in JSON object");
        }
        std::string key = parseString(str, index);
        skipWhitespace(str, index);

        if (str[index] != ':') {
            throw std::runtime_error("Expected ':' after key in JSON object");
        }
        ++index;  // Skip ':'
        skipWhitespace(str, index);

        JsonValue value = parseValue(str, index);
        obj[key] = value;
        skipWhitespace(str, index);

        if (str[index] == ',') {
            ++index;  // Skip comma and continue
        } else if (str[index] != '}') {
            throw std::runtime_error("Expected ',' or '}' in JSON object");
        }
        skipWhitespace(str, index);
    }
    ++index;  // Skip closing '}'
    return obj;
}

auto JsonParser::parseArray(const std::string& str,
                            size_t& index) -> JsonArray {
    ++index;  // Skip opening '['
    JsonArray arr;
    skipWhitespace(str, index);

    while (str[index] != ']') {
        arr.push_back(parseValue(str, index));
        skipWhitespace(str, index);

        if (str[index] == ',') {
            ++index;  // Skip comma and continue
        } else if (str[index] != ']') {
            throw std::runtime_error("Expected ',' or ']' in JSON array");
        }
        skipWhitespace(str, index);
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
