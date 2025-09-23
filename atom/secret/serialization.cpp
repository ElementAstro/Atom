#include "serialization.hpp"

#include <iomanip>
#include <sstream>
#include <ctime>

namespace atom::secret {

// ============================================================================
// JsonSerializer Implementation
// ============================================================================

Result<std::string> JsonSerializer::serializeEntry(const PasswordEntry& entry) {
    std::ostringstream json;
    
    json << "{\n";
    json << "  \"password\": \"" << escapeJsonString(entry.password) << "\",\n";
    json << "  \"username\": \"" << escapeJsonString(entry.username) << "\",\n";
    json << "  \"url\": \"" << escapeJsonString(entry.url) << "\",\n";
    json << "  \"notes\": \"" << escapeJsonString(entry.notes) << "\",\n";
    json << "  \"title\": \"" << escapeJsonString(entry.title) << "\",\n";
    json << "  \"category\": \"" << categoryToString(entry.category) << "\",\n";
    json << "  \"tags\": " << serializeStringArray(entry.tags) << ",\n";
    json << "  \"created\": \"" << timePointToString(entry.created) << "\",\n";
    json << "  \"modified\": \"" << timePointToString(entry.modified) << "\",\n";
    json << "  \"expires\": \"" << timePointToString(entry.expires) << "\",\n";
    json << "  \"previousPasswords\": " << serializeStringArray(entry.previousPasswords) << "\n";
    json << "}";
    
    return Result<std::string>(json.str());
}

Result<PasswordEntry> JsonSerializer::deserializeEntry(const std::string& json) {
    if (!SimpleJsonParser::isValidJson(json)) {
        return Result<PasswordEntry>::error("Invalid JSON format");
    }
    
    PasswordEntry entry;
    
    entry.password = SimpleJsonParser::extractString(json, "password");
    entry.username = SimpleJsonParser::extractString(json, "username");
    entry.url = SimpleJsonParser::extractString(json, "url");
    entry.notes = SimpleJsonParser::extractString(json, "notes");
    entry.title = SimpleJsonParser::extractString(json, "title");
    
    std::string categoryStr = SimpleJsonParser::extractString(json, "category");
    entry.category = stringToCategory(categoryStr);
    
    std::string tagsArray = SimpleJsonParser::extractArray(json, "tags");
    entry.tags = deserializeStringArray(tagsArray);
    
    std::string createdStr = SimpleJsonParser::extractString(json, "created");
    entry.created = stringToTimePoint(createdStr);
    
    std::string modifiedStr = SimpleJsonParser::extractString(json, "modified");
    entry.modified = stringToTimePoint(modifiedStr);
    
    std::string expiresStr = SimpleJsonParser::extractString(json, "expires");
    entry.expires = stringToTimePoint(expiresStr);
    
    std::string previousArray = SimpleJsonParser::extractArray(json, "previousPasswords");
    entry.previousPasswords = deserializeStringArray(previousArray);
    
    return Result<PasswordEntry>(std::move(entry));
}

Result<std::string> JsonSerializer::serializeEntries(const std::vector<PasswordEntry>& entries) {
    std::ostringstream json;
    
    json << "[\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        auto entryResult = serializeEntry(entries[i]);
        if (entryResult.isError()) {
            return Result<std::string>("Failed to serialize entry " + std::to_string(i) + 
                                      ": " + entryResult.error());
        }
        
        json << entryResult.value();
        if (i < entries.size() - 1) {
            json << ",";
        }
        json << "\n";
    }
    json << "]";
    
    return Result<std::string>(json.str());
}

Result<std::vector<PasswordEntry>> JsonSerializer::deserializeEntries(const std::string& json) {
    if (!SimpleJsonParser::isValidJson(json)) {
        return Result<std::vector<PasswordEntry>>::error("Invalid JSON format");
    }
    
    std::vector<PasswordEntry> entries;
    
    // Simple array parsing - find individual objects
    size_t pos = json.find('[');
    if (pos == std::string::npos) {
        return Result<std::vector<PasswordEntry>>::error("JSON is not an array");
    }
    
    pos++; // Skip opening bracket
    int braceCount = 0;
    size_t objectStart = std::string::npos;
    
    for (size_t i = pos; i < json.length(); ++i) {
        char c = json[i];
        
        if (c == '{') {
            if (braceCount == 0) {
                objectStart = i;
            }
            braceCount++;
        } else if (c == '}') {
            braceCount--;
            if (braceCount == 0 && objectStart != std::string::npos) {
                // Extract object
                std::string objectJson = json.substr(objectStart, i - objectStart + 1);
                auto entryResult = deserializeEntry(objectJson);
                if (entryResult.isError()) {
                    return Result<std::vector<PasswordEntry>>::error(
                        "Failed to deserialize entry: " + entryResult.error());
                }
                entries.push_back(std::move(entryResult.value()));
                objectStart = std::string::npos;
            }
        }
    }
    
    return Result<std::vector<PasswordEntry>>(std::move(entries));
}

Result<std::string> JsonSerializer::serializeSettings(const PasswordManagerSettings& settings) {
    std::ostringstream json;
    
    json << "{\n";
    json << "  \"autoLockTimeoutSeconds\": " << settings.autoLockTimeoutSeconds << ",\n";
    json << "  \"notifyOnPasswordExpiry\": " << (settings.notifyOnPasswordExpiry ? "true" : "false") << ",\n";
    json << "  \"passwordExpiryDays\": " << settings.passwordExpiryDays << ",\n";
    json << "  \"minPasswordLength\": " << settings.minPasswordLength << ",\n";
    json << "  \"requireSpecialChars\": " << (settings.requireSpecialChars ? "true" : "false") << ",\n";
    json << "  \"requireNumbers\": " << (settings.requireNumbers ? "true" : "false") << ",\n";
    json << "  \"requireMixedCase\": " << (settings.requireMixedCase ? "true" : "false") << ",\n";
    json << "  \"encryptionOptions\": {\n";
    json << "    \"useHardwareAcceleration\": " << (settings.encryptionOptions.useHardwareAcceleration ? "true" : "false") << ",\n";
    json << "    \"keyIterations\": " << settings.encryptionOptions.keyIterations << ",\n";
    json << "    \"encryptionMethod\": \"" << methodToString(settings.encryptionOptions.encryptionMethod) << "\"\n";
    json << "  }\n";
    json << "}";
    
    return Result<std::string>(json.str());
}

Result<PasswordManagerSettings> JsonSerializer::deserializeSettings(const std::string& json) {
    if (!SimpleJsonParser::isValidJson(json)) {
        return Result<PasswordManagerSettings>::error("Invalid JSON format");
    }
    
    PasswordManagerSettings settings;
    
    settings.autoLockTimeoutSeconds = SimpleJsonParser::extractInt(json, "autoLockTimeoutSeconds");
    settings.notifyOnPasswordExpiry = SimpleJsonParser::extractBool(json, "notifyOnPasswordExpiry");
    settings.passwordExpiryDays = SimpleJsonParser::extractInt(json, "passwordExpiryDays");
    settings.minPasswordLength = SimpleJsonParser::extractInt(json, "minPasswordLength");
    settings.requireSpecialChars = SimpleJsonParser::extractBool(json, "requireSpecialChars");
    settings.requireNumbers = SimpleJsonParser::extractBool(json, "requireNumbers");
    settings.requireMixedCase = SimpleJsonParser::extractBool(json, "requireMixedCase");
    
    std::string encryptionOptionsJson = SimpleJsonParser::extractObject(json, "encryptionOptions");
    if (!encryptionOptionsJson.empty()) {
        settings.encryptionOptions.useHardwareAcceleration = 
            SimpleJsonParser::extractBool(encryptionOptionsJson, "useHardwareAcceleration");
        settings.encryptionOptions.keyIterations = 
            SimpleJsonParser::extractInt(encryptionOptionsJson, "keyIterations");
        
        std::string methodStr = SimpleJsonParser::extractString(encryptionOptionsJson, "encryptionMethod");
        settings.encryptionOptions.encryptionMethod = stringToMethod(methodStr);
    }
    
    return Result<PasswordManagerSettings>(std::move(settings));
}

std::string JsonSerializer::unescapeString(const std::string& str) {
    return unescapeJsonString(str);
}

std::string JsonSerializer::escapeJsonString(const std::string& str) {
    std::ostringstream escaped;
    
    for (char c : str) {
        switch (c) {
            case '"':  escaped << "\\\""; break;
            case '\\': escaped << "\\\\"; break;
            case '\b': escaped << "\\b"; break;
            case '\f': escaped << "\\f"; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default:
                if (c < 0x20) {
                    escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    escaped << c;
                }
                break;
        }
    }
    
    return escaped.str();
}

std::string JsonSerializer::unescapeJsonString(const std::string& str) {
    std::ostringstream unescaped;
    
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            switch (str[i + 1]) {
                case '"':  unescaped << '"'; i++; break;
                case '\\': unescaped << '\\'; i++; break;
                case 'b':  unescaped << '\b'; i++; break;
                case 'f':  unescaped << '\f'; i++; break;
                case 'n':  unescaped << '\n'; i++; break;
                case 'r':  unescaped << '\r'; i++; break;
                case 't':  unescaped << '\t'; i++; break;
                case 'u':
                    if (i + 5 < str.length()) {
                        // Parse Unicode escape sequence
                        std::string hexStr = str.substr(i + 2, 4);
                        int codePoint = std::stoi(hexStr, nullptr, 16);
                        unescaped << static_cast<char>(codePoint);
                        i += 5;
                    } else {
                        unescaped << str[i];
                    }
                    break;
                default:
                    unescaped << str[i];
                    break;
            }
        } else {
            unescaped << str[i];
        }
    }
    
    return unescaped.str();
}

std::string JsonSerializer::timePointToString(const std::chrono::system_clock::time_point& timePoint) {
    auto time_t = std::chrono::system_clock::to_time_t(timePoint);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timePoint.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';

    return oss.str();
}

std::chrono::system_clock::time_point JsonSerializer::stringToTimePoint(const std::string& timeString) {
    if (timeString.empty()) {
        return std::chrono::system_clock::time_point{};
    }

    std::tm tm = {};
    std::istringstream ss(timeString);

    // Parse ISO 8601 format: YYYY-MM-DDTHH:MM:SS.sssZ
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail()) {
        return std::chrono::system_clock::time_point{};
    }

    auto time_t = std::mktime(&tm);
    auto timePoint = std::chrono::system_clock::from_time_t(time_t);

    // Parse milliseconds if present
    if (ss.peek() == '.') {
        ss.ignore(); // Skip the dot
        int ms = 0;
        ss >> ms;
        timePoint += std::chrono::milliseconds(ms);
    }

    return timePoint;
}

std::string JsonSerializer::categoryToString(PasswordCategory category) {
    switch (category) {
        case PasswordCategory::General: return "General";
        case PasswordCategory::Finance: return "Finance";
        case PasswordCategory::Work: return "Work";
        case PasswordCategory::Personal: return "Personal";
        case PasswordCategory::Social: return "Social";
        case PasswordCategory::Entertainment: return "Entertainment";
        case PasswordCategory::Other: return "Other";
        default: return "General";
    }
}

PasswordCategory JsonSerializer::stringToCategory(const std::string& str) {
    if (str == "Finance") return PasswordCategory::Finance;
    if (str == "Work") return PasswordCategory::Work;
    if (str == "Personal") return PasswordCategory::Personal;
    if (str == "Social") return PasswordCategory::Social;
    if (str == "Entertainment") return PasswordCategory::Entertainment;
    if (str == "Other") return PasswordCategory::Other;
    return PasswordCategory::General;
}

std::string JsonSerializer::methodToString(EncryptionOptions::Method method) {
    switch (method) {
        case EncryptionOptions::Method::AES_GCM: return "AES_GCM";
        case EncryptionOptions::Method::AES_CBC: return "AES_CBC";
        case EncryptionOptions::Method::CHACHA20_POLY1305: return "CHACHA20_POLY1305";
        default: return "AES_GCM";
    }
}

EncryptionOptions::Method JsonSerializer::stringToMethod(const std::string& str) {
    if (str == "AES_CBC") return EncryptionOptions::Method::AES_CBC;
    if (str == "CHACHA20_POLY1305") return EncryptionOptions::Method::CHACHA20_POLY1305;
    return EncryptionOptions::Method::AES_GCM;
}

std::string JsonSerializer::serializeStringArray(const std::vector<std::string>& strings) {
    std::ostringstream json;
    json << "[";

    for (size_t i = 0; i < strings.size(); ++i) {
        json << "\"" << escapeJsonString(strings[i]) << "\"";
        if (i < strings.size() - 1) {
            json << ", ";
        }
    }

    json << "]";
    return json.str();
}

std::vector<std::string> JsonSerializer::deserializeStringArray(const std::string& json) {
    std::vector<std::string> strings;

    if (json.empty() || json == "[]") {
        return strings;
    }

    // Simple array parsing
    size_t pos = json.find('[');
    if (pos == std::string::npos) {
        return strings;
    }

    pos++; // Skip opening bracket
    bool inString = false;
    bool escaped = false;
    std::string currentString;

    for (size_t i = pos; i < json.length(); ++i) {
        char c = json[i];

        if (escaped) {
            currentString += c;
            escaped = false;
        } else if (c == '\\' && inString) {
            escaped = true;
            currentString += c;
        } else if (c == '"') {
            if (inString) {
                strings.push_back(unescapeJsonString(currentString));
                currentString.clear();
                inString = false;
            } else {
                inString = true;
            }
        } else if (inString) {
            currentString += c;
        } else if (c == ']') {
            break;
        }
    }

    return strings;
}

// ============================================================================
// SimpleJsonParser Implementation
// ============================================================================

std::string SimpleJsonParser::extractString(const std::string& json, const std::string& key) {
    size_t endPos;
    size_t startPos = findValue(json, key, 0, endPos);

    if (startPos == std::string::npos) {
        return "";
    }

    // Skip opening quote
    if (json[startPos] == '"') {
        startPos++;
        endPos--; // Skip closing quote
    }

    std::string value = json.substr(startPos, endPos - startPos);
    return JsonSerializer::unescapeString(value);
}

int SimpleJsonParser::extractInt(const std::string& json, const std::string& key) {
    size_t endPos;
    size_t startPos = findValue(json, key, 0, endPos);

    if (startPos == std::string::npos) {
        return 0;
    }

    std::string value = json.substr(startPos, endPos - startPos);
    try {
        return std::stoi(value);
    } catch (...) {
        return 0;
    }
}

bool SimpleJsonParser::extractBool(const std::string& json, const std::string& key) {
    size_t endPos;
    size_t startPos = findValue(json, key, 0, endPos);

    if (startPos == std::string::npos) {
        return false;
    }

    std::string value = json.substr(startPos, endPos - startPos);
    return value == "true";
}

std::string SimpleJsonParser::extractArray(const std::string& json, const std::string& key) {
    size_t endPos;
    size_t startPos = findValue(json, key, 0, endPos);

    if (startPos == std::string::npos) {
        return "";
    }

    return json.substr(startPos, endPos - startPos);
}

std::string SimpleJsonParser::extractObject(const std::string& json, const std::string& key) {
    size_t endPos;
    size_t startPos = findValue(json, key, 0, endPos);

    if (startPos == std::string::npos) {
        return "";
    }

    return json.substr(startPos, endPos - startPos);
}

bool SimpleJsonParser::isValidJson(const std::string& json) {
    if (json.empty()) {
        return false;
    }

    // Simple validation - check for balanced braces/brackets
    int braceCount = 0;
    int bracketCount = 0;
    bool inString = false;
    bool escaped = false;

    for (char c : json) {
        if (escaped) {
            escaped = false;
            continue;
        }

        if (c == '\\' && inString) {
            escaped = true;
            continue;
        }

        if (c == '"') {
            inString = !inString;
            continue;
        }

        if (!inString) {
            if (c == '{') braceCount++;
            else if (c == '}') braceCount--;
            else if (c == '[') bracketCount++;
            else if (c == ']') bracketCount--;
        }
    }

    return braceCount == 0 && bracketCount == 0 && !inString;
}

size_t SimpleJsonParser::findValue(const std::string& json, const std::string& key,
                                  size_t startPos, size_t& endPos) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey, startPos);

    if (keyPos == std::string::npos) {
        return std::string::npos;
    }

    // Find the colon after the key
    size_t colonPos = json.find(':', keyPos + searchKey.length());
    if (colonPos == std::string::npos) {
        return std::string::npos;
    }

    // Skip whitespace after colon
    size_t valueStart = skipWhitespace(json, colonPos + 1);
    if (valueStart >= json.length()) {
        return std::string::npos;
    }

    // Find the end of the value
    endPos = findValueEnd(json, valueStart);

    return valueStart;
}

size_t SimpleJsonParser::skipWhitespace(const std::string& json, size_t pos) {
    while (pos < json.length() && std::isspace(json[pos])) {
        pos++;
    }
    return pos;
}

size_t SimpleJsonParser::findValueEnd(const std::string& json, size_t startPos) {
    if (startPos >= json.length()) {
        return startPos;
    }

    char startChar = json[startPos];

    if (startChar == '"') {
        // String value
        bool escaped = false;
        for (size_t i = startPos + 1; i < json.length(); ++i) {
            if (escaped) {
                escaped = false;
                continue;
            }
            if (json[i] == '\\') {
                escaped = true;
                continue;
            }
            if (json[i] == '"') {
                return i + 1;
            }
        }
        return json.length();
    } else if (startChar == '{') {
        // Object value
        int braceCount = 1;
        bool inString = false;
        bool escaped = false;

        for (size_t i = startPos + 1; i < json.length(); ++i) {
            if (escaped) {
                escaped = false;
                continue;
            }
            if (json[i] == '\\' && inString) {
                escaped = true;
                continue;
            }
            if (json[i] == '"') {
                inString = !inString;
                continue;
            }
            if (!inString) {
                if (json[i] == '{') braceCount++;
                else if (json[i] == '}') {
                    braceCount--;
                    if (braceCount == 0) {
                        return i + 1;
                    }
                }
            }
        }
        return json.length();
    } else if (startChar == '[') {
        // Array value
        int bracketCount = 1;
        bool inString = false;
        bool escaped = false;

        for (size_t i = startPos + 1; i < json.length(); ++i) {
            if (escaped) {
                escaped = false;
                continue;
            }
            if (json[i] == '\\' && inString) {
                escaped = true;
                continue;
            }
            if (json[i] == '"') {
                inString = !inString;
                continue;
            }
            if (!inString) {
                if (json[i] == '[') bracketCount++;
                else if (json[i] == ']') {
                    bracketCount--;
                    if (bracketCount == 0) {
                        return i + 1;
                    }
                }
            }
        }
        return json.length();
    } else {
        // Primitive value (number, boolean, null)
        for (size_t i = startPos; i < json.length(); ++i) {
            char c = json[i];
            if (c == ',' || c == '}' || c == ']' || std::isspace(c)) {
                return i;
            }
        }
        return json.length();
    }
}

}  // namespace atom::secret
