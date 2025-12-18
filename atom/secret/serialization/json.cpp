#include "json.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>

namespace atom::secret {

json JsonSerializer::entryToJson(const PasswordEntry& entry) {
    json j;

    j["id"] = entry.id;
    j["title"] = entry.title;
    j["username"] = entry.username;
    j["password"] = entry.password;
    j["url"] = entry.url;
    j["email"] = entry.email;
    j["notes"] = entry.notes;
    j["totp"] = entry.totp;
    j["category"] = categoryToString(entry.category);
    j["tags"] = entry.tags;

    // Custom fields
    json fieldsArray = json::array();
    for (const auto& field : entry.customFields) {
        json fieldObj;
        fieldObj["name"] = field.name;
        fieldObj["value"] = field.value;
        fieldObj["isProtected"] = field.isProtected;
        fieldObj["isMultiline"] = field.isMultiline;
        fieldsArray.push_back(fieldObj);
    }
    j["customFields"] = fieldsArray;

    // Timestamps
    j["created"] = timePointToString(entry.created);
    j["modified"] = timePointToString(entry.modified);
    j["expires"] = timePointToString(entry.expires);
    j["lastAccessed"] = timePointToString(entry.lastAccessed);

    // Other fields
    j["accessCount"] = entry.accessCount;
    j["isFavorite"] = entry.isFavorite;
    j["isArchived"] = entry.isArchived;
    j["iconUrl"] = entry.iconUrl;

    // Password history
    json historyArray = json::array();
    for (const auto& prev : entry.passwordHistory) {
        json histObj;
        histObj["password"] = prev.password;
        histObj["changed"] = timePointToString(prev.changed);
        historyArray.push_back(histObj);
    }
    j["passwordHistory"] = historyArray;

    return j;
}

PasswordEntry JsonSerializer::jsonToEntry(const json& j) {
    PasswordEntry entry;

    entry.id = j.value("id", "");
    entry.title = j.value("title", "");
    entry.username = j.value("username", "");
    entry.password = j.value("password", "");
    entry.url = j.value("url", "");
    entry.email = j.value("email", "");
    entry.notes = j.value("notes", "");
    entry.totp = j.value("totp", "");

    std::string categoryStr = j.value("category", "General");
    entry.category = stringToCategory(categoryStr);

    if (j.contains("tags") && j["tags"].is_array()) {
        entry.tags = j["tags"].get<std::vector<std::string>>();
    }

    if (j.contains("customFields") && j["customFields"].is_array()) {
        for (const auto& fieldObj : j["customFields"]) {
            CustomField field;
            field.name = fieldObj.value("name", "");
            field.value = fieldObj.value("value", "");
            field.isProtected = fieldObj.value("isProtected", false);
            field.isMultiline = fieldObj.value("isMultiline", false);
            entry.customFields.push_back(field);
        }
    }

    entry.created = stringToTimePoint(j.value("created", ""));
    entry.modified = stringToTimePoint(j.value("modified", ""));
    entry.expires = stringToTimePoint(j.value("expires", ""));
    entry.lastAccessed = stringToTimePoint(j.value("lastAccessed", ""));

    entry.accessCount = j.value("accessCount", 0);
    entry.isFavorite = j.value("isFavorite", false);
    entry.isArchived = j.value("isArchived", false);
    entry.iconUrl = j.value("iconUrl", "");

    if (j.contains("passwordHistory") && j["passwordHistory"].is_array()) {
        for (const auto& histObj : j["passwordHistory"]) {
            PreviousPassword prev;
            prev.password = histObj.value("password", "");
            prev.changed = stringToTimePoint(histObj.value("changed", ""));
            entry.passwordHistory.push_back(prev);
        }
    }

    return entry;
}

Result<std::string> JsonSerializer::serializeEntry(const PasswordEntry& entry,
                                                   bool pretty) {
    try {
        json j = entryToJson(entry);
        if (pretty) {
            return Result<std::string>::success(j.dump(2));
        }
        return Result<std::string>::success(j.dump());
    } catch (const std::exception& e) {
        return Result<std::string>::error(
            ErrorCode::SerializationFailed,
            std::string("Failed to serialize entry: ") + e.what());
    }
}

Result<PasswordEntry> JsonSerializer::deserializeEntry(
    const std::string& jsonStr) {
    try {
        json j = json::parse(jsonStr);
        return Result<PasswordEntry>::success(jsonToEntry(j));
    } catch (const json::parse_error& e) {
        return Result<PasswordEntry>::error(
            ErrorCode::DeserializationFailed,
            std::string("JSON parse error: ") + e.what());
    } catch (const std::exception& e) {
        return Result<PasswordEntry>::error(
            ErrorCode::DeserializationFailed,
            std::string("Failed to deserialize entry: ") + e.what());
    }
}

Result<std::string> JsonSerializer::serializeEntries(
    const std::vector<PasswordEntry>& entries, bool pretty) {
    try {
        json arr = json::array();
        for (const auto& entry : entries) {
            arr.push_back(entryToJson(entry));
        }
        if (pretty) {
            return Result<std::string>::success(arr.dump(2));
        }
        return Result<std::string>::success(arr.dump());
    } catch (const std::exception& e) {
        return Result<std::string>::error(
            ErrorCode::SerializationFailed,
            std::string("Failed to serialize entries: ") + e.what());
    }
}

Result<std::vector<PasswordEntry>> JsonSerializer::deserializeEntries(
    const std::string& jsonStr) {
    try {
        json arr = json::parse(jsonStr);
        if (!arr.is_array()) {
            return Result<std::vector<PasswordEntry>>::error(
                ErrorCode::InvalidFormat, "Expected JSON array");
        }

        std::vector<PasswordEntry> entries;
        entries.reserve(arr.size());
        for (const auto& j : arr) {
            entries.push_back(jsonToEntry(j));
        }
        return Result<std::vector<PasswordEntry>>::success(std::move(entries));
    } catch (const json::parse_error& e) {
        return Result<std::vector<PasswordEntry>>::error(
            ErrorCode::DeserializationFailed,
            std::string("JSON parse error: ") + e.what());
    } catch (const std::exception& e) {
        return Result<std::vector<PasswordEntry>>::error(
            ErrorCode::DeserializationFailed,
            std::string("Failed to deserialize entries: ") + e.what());
    }
}

Result<std::string> JsonSerializer::serializeEntriesWithKeys(
    const std::vector<std::pair<std::string, PasswordEntry>>& entries,
    bool pretty) {
    try {
        json obj = json::object();
        for (const auto& [key, entry] : entries) {
            obj[key] = entryToJson(entry);
        }
        if (pretty) {
            return Result<std::string>::success(obj.dump(2));
        }
        return Result<std::string>::success(obj.dump());
    } catch (const std::exception& e) {
        return Result<std::string>::error(
            ErrorCode::SerializationFailed,
            std::string("Failed to serialize entries: ") + e.what());
    }
}

Result<std::vector<std::pair<std::string, PasswordEntry>>>
JsonSerializer::deserializeEntriesWithKeys(const std::string& jsonStr) {
    try {
        json obj = json::parse(jsonStr);
        if (!obj.is_object()) {
            return Result<std::vector<std::pair<std::string, PasswordEntry>>>::
                error(ErrorCode::InvalidFormat, "Expected JSON object");
        }

        std::vector<std::pair<std::string, PasswordEntry>> entries;
        for (auto& [key, value] : obj.items()) {
            entries.emplace_back(key, jsonToEntry(value));
        }
        return Result<std::vector<std::pair<std::string, PasswordEntry>>>::
            success(std::move(entries));
    } catch (const json::parse_error& e) {
        return Result<std::vector<std::pair<std::string, PasswordEntry>>>::
            error(ErrorCode::DeserializationFailed,
                  std::string("JSON parse error: ") + e.what());
    } catch (const std::exception& e) {
        return Result<std::vector<std::pair<std::string, PasswordEntry>>>::
            error(ErrorCode::DeserializationFailed,
                  std::string("Failed to deserialize entries: ") + e.what());
    }
}

std::string JsonSerializer::timePointToString(
    const std::chrono::system_clock::time_point& timePoint) {
    if (timePoint == std::chrono::system_clock::time_point{}) {
        return "";
    }

    auto time = std::chrono::system_clock::to_time_t(timePoint);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::chrono::system_clock::time_point JsonSerializer::stringToTimePoint(
    const std::string& timeString) {
    if (timeString.empty()) {
        return std::chrono::system_clock::time_point{};
    }

    std::tm tm = {};
    std::istringstream ss(timeString);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail()) {
        return std::chrono::system_clock::time_point{};
    }

#if defined(_WIN32)
    time_t time = _mkgmtime(&tm);
#else
    time_t time = timegm(&tm);
#endif

    return std::chrono::system_clock::from_time_t(time);
}

}  // namespace atom::secret
