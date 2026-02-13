#include "image_metadata.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace atom::image::core {

using Clock = std::chrono::system_clock;

// Helpers
namespace {
std::string trim_copy(std::string_view sv) {
    size_t b = 0, e = sv.size();
    while (b < e && std::isspace(static_cast<unsigned char>(sv[b])))
        ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(sv[e - 1])))
        --e;
    return std::string(sv.substr(b, e - b));
}

std::vector<std::string> split(std::string_view sv, char delim) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= sv.size()) {
        size_t pos = sv.find(delim, start);
        if (pos == std::string_view::npos)
            pos = sv.size();
        out.emplace_back(std::string(sv.substr(start, pos - start)));
        if (pos == sv.size())
            break;
        start = pos + 1;
    }
    return out;
}

// Convert time_point to ISO-8601 (UTC)
std::string tp_to_iso(const Clock::time_point& tp) {
    std::time_t t = Clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// Convert ISO-8601 to time_point (best-effort, UTC)
Clock::time_point iso_to_tp(const std::string& s) {
    std::tm tm{};
    std::istringstream iss(s);
    iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    if (!iss.fail()) {
#if defined(_WIN32)
        // Windows lacks timegm; emulate by _mkgmtime
        std::time_t t = _mkgmtime(&tm);
#else
        std::time_t t = timegm(&tm);
#endif
        if (t != static_cast<std::time_t>(-1)) {
            return Clock::from_time_t(t);
        }
    }
    return Clock::time_point{};
}
}  // namespace

std::string ImageMetadata::getString(const std::string& key,
                                     const std::string& defaultValue) const {
    auto it = metadata_.find(key);
    if (it == metadata_.end())
        return defaultValue;
    const auto& v = it->second;
    if (auto p = std::get_if<std::string>(&v))
        return *p;
    if (auto p = std::get_if<int>(&v))
        return std::to_string(*p);
    if (auto p = std::get_if<double>(&v)) {
        std::ostringstream oss;
        oss << *p;
        return oss.str();
    }
    if (auto p = std::get_if<bool>(&v))
        return *p ? "true" : "false";
    if (auto p = std::get_if<Clock::time_point>(&v))
        return tp_to_iso(*p);
    if (auto p = std::get_if<std::vector<std::string>>(&v)) {
        std::ostringstream oss;
        bool first = true;
        for (auto& s : *p) {
            if (!first)
                oss << ",";
            first = false;
            oss << s;
        }
        return oss.str();
    }
    if (auto p = std::get_if<std::vector<int>>(&v)) {
        std::ostringstream oss;
        bool first = true;
        for (auto x : *p) {
            if (!first)
                oss << ",";
            first = false;
            oss << x;
        }
        return oss.str();
    }
    if (auto p = std::get_if<std::vector<double>>(&v)) {
        std::ostringstream oss;
        bool first = true;
        for (auto x : *p) {
            if (!first)
                oss << ",";
            first = false;
            oss << x;
        }
        return oss.str();
    }
    return defaultValue;
}

int ImageMetadata::getInt(const std::string& key, int defaultValue) const {
    auto it = metadata_.find(key);
    if (it == metadata_.end())
        return defaultValue;
    const auto& v = it->second;
    if (auto p = std::get_if<int>(&v))
        return *p;
    if (auto p = std::get_if<double>(&v))
        return static_cast<int>(*p);
    if (auto p = std::get_if<std::string>(&v)) {
        int out = defaultValue;
        std::from_chars(p->data(), p->data() + p->size(), out);
        return out;
    }
    if (auto p = std::get_if<bool>(&v))
        return *p ? 1 : 0;
    return defaultValue;
}

double ImageMetadata::getDouble(const std::string& key,
                                double defaultValue) const {
    auto it = metadata_.find(key);
    if (it == metadata_.end())
        return defaultValue;
    const auto& v = it->second;
    if (auto p = std::get_if<double>(&v))
        return *p;
    if (auto p = std::get_if<int>(&v))
        return static_cast<double>(*p);
    if (auto p = std::get_if<std::string>(&v)) {
        try {
            return std::stod(*p);
        } catch (...) {
            return defaultValue;
        }
    }
    return defaultValue;
}

bool ImageMetadata::getBool(const std::string& key, bool defaultValue) const {
    auto it = metadata_.find(key);
    if (it == metadata_.end())
        return defaultValue;
    const auto& v = it->second;
    if (auto p = std::get_if<bool>(&v))
        return *p;
    if (auto p = std::get_if<int>(&v))
        return *p != 0;
    if (auto p = std::get_if<std::string>(&v)) {
        auto s = *p;
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (s == "true" || s == "1" || s == "yes" || s == "on")
            return true;
        if (s == "false" || s == "0" || s == "no" || s == "off")
            return false;
    }
    return defaultValue;
}

std::vector<std::string> ImageMetadata::getKeys() const {
    std::vector<std::string> keys;
    keys.reserve(metadata_.size());
    for (const auto& kv : metadata_)
        keys.emplace_back(kv.first);
    return keys;
}

void ImageMetadata::merge(const ImageMetadata& other, bool overwrite) {
    for (const auto& [k, v] : other.metadata_) {
        if (overwrite || !has(k))
            metadata_[k] = v;
    }
}

std::string ImageMetadata::valueToString(const MetadataValue& value) const {
    return std::visit(
        [&](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string>) {
                return arg;
            } else if constexpr (std::is_same_v<T, int>) {
                return std::to_string(arg);
            } else if constexpr (std::is_same_v<T, double>) {
                std::ostringstream oss;
                oss << arg;
                return oss.str();
            } else if constexpr (std::is_same_v<T, bool>) {
                return arg ? "true" : "false";
            } else if constexpr (std::is_same_v<T, Clock::time_point>) {
                return tp_to_iso(arg);
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                std::ostringstream oss;
                bool first = true;
                for (auto& s : arg) {
                    if (!first)
                        oss << ",";
                    first = false;
                    oss << s;
                }
                return oss.str();
            } else if constexpr (std::is_same_v<T, std::vector<int>>) {
                std::ostringstream oss;
                bool first = true;
                for (auto x : arg) {
                    if (!first)
                        oss << ",";
                    first = false;
                    oss << x;
                }
                return oss.str();
            } else if constexpr (std::is_same_v<T, std::vector<double>>) {
                std::ostringstream oss;
                bool first = true;
                for (auto x : arg) {
                    if (!first)
                        oss << ",";
                    first = false;
                    oss << x;
                }
                return oss.str();
            } else {
                return std::string{};
            }
        },
        value);
}

MetadataValue ImageMetadata::stringToValue(const std::string& str,
                                           const std::string& typeHint) const {
    std::string t = typeHint;
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (t == "string")
        return str;
    if (t == "int") {
        int v = 0;
        std::from_chars(str.data(), str.data() + str.size(), v);
        return v;
    }
    if (t == "double") {
        try {
            return std::stod(str);
        } catch (...) {
            return 0.0;
        }
    }
    if (t == "bool") {
        std::string s = str;
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return (s == "true" || s == "1" || s == "yes" || s == "on");
    }
    if (t == "time" || t == "time_point" || t == "datetime") {
        return iso_to_tp(str);
    }
    if (t == "strings") {
        auto parts = split(str, ',');
        for (auto& p : parts)
            p = trim_copy(p);
        return parts;
    }
    if (t == "ints") {
        std::vector<int> out;
        for (auto& s : split(str, ',')) {
            int v = 0;
            std::string sv = trim_copy(s);
            std::from_chars(sv.data(), sv.data() + sv.size(), v);
            out.push_back(v);
        }
        return out;
    }
    if (t == "doubles") {
        std::vector<double> out;
        for (auto& s : split(str, ',')) {
            try {
                out.push_back(std::stod(trim_copy(s)));
            } catch (...) {
                out.push_back(0.0);
            }
        }
        return out;
    }
    // Fallback
    return str;
}

std::string ImageMetadata::toString() const {
    // Simple line-based format: key|type=value\n
    auto type_name = [](const MetadataValue& v) -> const char* {
        switch (v.index()) {
            case 0:
                return "string";  // std::string
            case 1:
                return "int";
            case 2:
                return "double";
            case 3:
                return "bool";
            case 4:
                return "time";  // time_point
            case 5:
                return "strings";
            case 6:
                return "ints";
            case 7:
                return "doubles";
            default:
                return "string";
        }
    };

    std::ostringstream oss;
    for (const auto& [k, v] : metadata_) {
        oss << k << '|' << type_name(v) << '=' << valueToString(v) << '\n';
    }
    return oss.str();
}

bool ImageMetadata::fromString(const std::string& str) {
    metadata_.clear();
    std::istringstream iss(str);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty())
            continue;
        auto bar = line.find('|');
        auto eq = line.find('=');
        if (bar == std::string::npos || eq == std::string::npos || bar > eq)
            continue;
        std::string key = trim_copy(std::string_view(line).substr(0, bar));
        std::string typeHint =
            trim_copy(std::string_view(line).substr(bar + 1, eq - bar - 1));
        std::string value = std::string(line.substr(eq + 1));
        metadata_[key] = stringToValue(value, typeHint);
    }
    return true;
}

}  // namespace atom::image::core
