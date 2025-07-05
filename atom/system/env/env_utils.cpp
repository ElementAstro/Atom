/*
 * env_utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Environment variable utility functions implementation

**************************************************/

#include "env_utils.hpp"

#include <spdlog/spdlog.h>

namespace atom::utils {

auto EnvUtils::expandVariables(const String& str, VariableFormat format) -> String {
    spdlog::debug("Expanding variables in string with format: {}",
                  static_cast<int>(format));

    if (str.empty()) {
        return str;
    }

    if (format == VariableFormat::AUTO) {
#ifdef _WIN32
        format = VariableFormat::WINDOWS;
#else
        format = VariableFormat::UNIX;
#endif
    }

    switch (format) {
        case VariableFormat::UNIX:
            return expandUnixVariables(str);
        case VariableFormat::WINDOWS:
            return expandWindowsVariables(str);
        default:
            return str;
    }
}

auto EnvUtils::expandUnixVariables(const String& str) -> String {
    String result;
    result.reserve(str.length() * 2);

    size_t pos = 0;
    while (pos < str.length()) {
        if (str[pos] == '$' && pos + 1 < str.length()) {
            size_t start = pos + 1;
            size_t end = start;
            String varName;

            if (str[start] == '{') {
                // ${VAR} format
                start++;
                end = str.find('}', start);
                if (end != String::npos) {
                    varName = str.substr(start, end - start);
                    pos = end + 1;
                } else {
                    result += str[pos++];
                    continue;
                }
            } else {
                // $VAR format
                while (end < str.length() &&
                       (std::isalnum(str[end]) || str[end] == '_')) {
                    end++;
                }
                if (end > start) {
                    varName = str.substr(start, end - start);
                    pos = end;
                } else {
                    result += str[pos++];
                    continue;
                }
            }

            if (isValidVariableName(varName)) {
                String value = EnvCore::getEnv(varName, "");
                result += value;
            } else {
                result += "$" + varName;
            }
        } else {
            result += str[pos++];
        }
    }

    return result;
}

auto EnvUtils::expandWindowsVariables(const String& str) -> String {
    String result;
    result.reserve(str.length() * 2);

    size_t pos = 0;
    while (pos < str.length()) {
        if (str[pos] == '%') {
            size_t start = pos + 1;
            size_t end = str.find('%', start);

            if (end != String::npos && end > start) {
                String varName = str.substr(start, end - start);

                if (isValidVariableName(varName)) {
                    String value = EnvCore::getEnv(varName, "");
                    result += value;
                } else {
                    result += "%" + varName + "%";
                }

                pos = end + 1;
            } else {
                result += str[pos++];
            }
        } else {
            result += str[pos++];
        }
    }

    return result;
}

auto EnvUtils::findNextVariable(const String& str, size_t start,
                                VariableFormat format)
    -> std::tuple<bool, size_t, size_t, String> {

    size_t pos = start;

    if (format == VariableFormat::UNIX) {
        pos = str.find('$', start);
        if (pos != String::npos && pos + 1 < str.length()) {
            size_t varStart = pos + 1;
            size_t varEnd = varStart;

            if (str[varStart] == '{') {
                varStart++;
                varEnd = str.find('}', varStart);
                if (varEnd != String::npos) {
                    String varName = str.substr(varStart, varEnd - varStart);
                    return {true, pos, varEnd + 1, varName};
                }
            } else {
                while (varEnd < str.length() &&
                       (std::isalnum(str[varEnd]) || str[varEnd] == '_')) {
                    varEnd++;
                }
                if (varEnd > varStart) {
                    String varName = str.substr(varStart, varEnd - varStart);
                    return {true, pos, varEnd, varName};
                }
            }
        }
    } else if (format == VariableFormat::WINDOWS) {
        pos = str.find('%', start);
        if (pos != String::npos) {
            size_t varStart = pos + 1;
            size_t varEnd = str.find('%', varStart);

            if (varEnd != String::npos && varEnd > varStart) {
                String varName = str.substr(varStart, varEnd - varStart);
                return {true, pos, varEnd + 1, varName};
            }
        }
    }

    return {false, 0, 0, ""};
}

auto EnvUtils::isValidVariableName(const String& name) -> bool {
    if (name.empty()) {
        return false;
    }

    // First character must be letter or underscore
    if (!std::isalpha(name[0]) && name[0] != '_') {
        return false;
    }

    // Remaining characters must be alphanumeric or underscore
    for (size_t i = 1; i < name.length(); ++i) {
        if (!std::isalnum(name[i]) && name[i] != '_') {
            return false;
        }
    }

    return true;
}

auto EnvUtils::diffEnvironments(const HashMap<String, String>& env1,
                                const HashMap<String, String>& env2)
    -> std::tuple<HashMap<String, String>, HashMap<String, String>,
                  HashMap<String, String>> {
    HashMap<String, String> added;
    HashMap<String, String> removed;
    HashMap<String, String> modified;

    // Find added and modified variables
    for (const auto& [key, val2] : env2) {
        auto it = env1.find(key);
        if (it == env1.end()) {
            added[key] = val2;
        } else if (it->second != val2) {
            modified[key] = val2;
        }
    }

    // Find removed variables
    for (const auto& [key, val1] : env1) {
        if (env2.find(key) == env2.end()) {
            removed[key] = val1;
        }
    }

    spdlog::debug("Environment diff: {} added, {} removed, {} modified",
                  added.size(), removed.size(), modified.size());
    return std::make_tuple(added, removed, modified);
}

auto EnvUtils::mergeEnvironments(const HashMap<String, String>& baseEnv,
                                 const HashMap<String, String>& overlayEnv,
                                 bool override) -> HashMap<String, String> {
    HashMap<String, String> result = baseEnv;

    for (const auto& [key, val] : overlayEnv) {
        auto it = result.find(key);
        if (it == result.end() || override) {
            result[key] = val;
        }
    }

    spdlog::debug("Merged environments: {} total variables", result.size());
    return result;
}

}  // namespace atom::utils
