#include "string_utils.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <regex>

namespace atom::system::utils {

auto trim(const std::string& str) -> std::string {
    return ltrim(rtrim(str));
}

auto ltrim(const std::string& str) -> std::string {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
        start++;
    }
    return std::string(start, str.end());
}

auto rtrim(const std::string& str) -> std::string {
    auto end = str.end();
    while (end != str.begin() && std::isspace(*(end - 1))) {
        end--;
    }
    return std::string(str.begin(), end);
}

auto toLower(const std::string& str) -> std::string {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

auto toUpper(const std::string& str) -> std::string {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

auto split(const std::string& str, char delimiter) -> std::vector<std::string> {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }

    return result;
}

auto split(const std::string& str, const std::string& delimiter) -> std::vector<std::string> {
    std::vector<std::string> result;
    size_t start = 0;
    size_t end = str.find(delimiter);

    while (end != std::string::npos) {
        result.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }

    result.push_back(str.substr(start));
    return result;
}

auto join(const std::vector<std::string>& strings, const std::string& delimiter) -> std::string {
    if (strings.empty()) {
        return "";
    }

    std::stringstream ss;
    ss << strings[0];

    for (size_t i = 1; i < strings.size(); ++i) {
        ss << delimiter << strings[i];
    }

    return ss.str();
}

auto replace(const std::string& str, const std::string& from, const std::string& to) -> std::string {
    std::string result = str;
    size_t pos = 0;

    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }

    return result;
}

auto startsWith(const std::string& str, const std::string& prefix) -> bool {
    return str.length() >= prefix.length() &&
           str.compare(0, prefix.length(), prefix) == 0;
}

auto endsWith(const std::string& str, const std::string& suffix) -> bool {
    return str.length() >= suffix.length() &&
           str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

auto contains(const std::string& str, const std::string& substring) -> bool {
    return str.find(substring) != std::string::npos;
}

auto pad(const std::string& str, size_t width, char padChar, bool leftAlign) -> std::string {
    if (str.length() >= width) {
        return str;
    }

    size_t padSize = width - str.length();

    if (leftAlign) {
        return str + std::string(padSize, padChar);
    } else {
        return std::string(padSize, padChar) + str;
    }
}

auto center(const std::string& str, size_t width, char padChar) -> std::string {
    if (str.length() >= width) {
        return str;
    }

    size_t totalPad = width - str.length();
    size_t leftPad = totalPad / 2;
    size_t rightPad = totalPad - leftPad;

    return std::string(leftPad, padChar) + str + std::string(rightPad, padChar);
}

auto wordWrap(const std::string& text, size_t width) -> std::vector<std::string> {
    std::vector<std::string> result;
    std::istringstream words(text);
    std::string word;
    std::string currentLine;

    while (words >> word) {
        if (currentLine.empty()) {
            currentLine = word;
        } else if (currentLine.length() + 1 + word.length() <= width) {
            currentLine += " " + word;
        } else {
            result.push_back(currentLine);
            currentLine = word;
        }
    }

    if (!currentLine.empty()) {
        result.push_back(currentLine);
    }

    return result;
}

auto escape(const std::string& str, const std::string& format) -> std::string {
    std::string result = str;

    if (format == "html" || format == "xml") {
        result = replace(result, "&", "&amp;");
        result = replace(result, "<", "&lt;");
        result = replace(result, ">", "&gt;");
        result = replace(result, "\"", "&quot;");
        if (format == "xml") {
            result = replace(result, "'", "&apos;");
        }
    } else if (format == "json") {
        result = replace(result, "\\", "\\\\");
        result = replace(result, "\"", "\\\"");
        result = replace(result, "\n", "\\n");
        result = replace(result, "\r", "\\r");
        result = replace(result, "\t", "\\t");
    } else if (format == "csv") {
        if (result.find(',') != std::string::npos ||
            result.find('"') != std::string::npos ||
            result.find('\n') != std::string::npos) {
            result = replace(result, "\"", "\"\"");
            result = "\"" + result + "\"";
        }
    }

    return result;
}

auto removeAnsiCodes(const std::string& str) -> std::string {
    std::regex ansiRegex("\033\\[[0-9;]*m");
    return std::regex_replace(str, ansiRegex, "");
}

auto displayWidth(const std::string& str) -> size_t {
    return removeAnsiCodes(str).length();
}

} // namespace atom::system::utils