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

#include <algorithm>
#include <regex>
#include <unordered_set>

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

// ========== NEW ENHANCED FUNCTIONALITY ==========

auto EnvUtils::expandVariablesAdvanced(const String& str, VariableFormat format,
                                        int options, VariableSubstitutionCallback customResolver) -> String {
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

    String result = str;
    bool recursive = (options & static_cast<int>(ExpansionOptions::RECURSIVE)) != 0;
    bool strict = (options & static_cast<int>(ExpansionOptions::STRICT)) != 0;
    bool escapeSeq = (options & static_cast<int>(ExpansionOptions::ESCAPE_SEQUENCES)) != 0;

    // Process escape sequences first if enabled
    if (escapeSeq) {
        result = processEscapeSequences(result);
    }

    // Expand variables
    int maxIterations = recursive ? 10 : 1;  // Prevent infinite recursion
    for (int iteration = 0; iteration < maxIterations; ++iteration) {
        String previousResult = result;

        auto [found, start, end, varName] = findNextVariable(result, 0, format);
        while (found) {
            String replacement;

            // Try custom resolver first
            if (customResolver) {
                auto customValue = customResolver(varName);
                if (customValue.has_value()) {
                    replacement = customValue.value();
                } else if (strict) {
                    spdlog::error("Undefined variable in strict mode: {}", varName);
                    return "";
                }
            } else {
                replacement = EnvCore::getEnv(varName, "");
                if (replacement.empty() && strict) {
                    spdlog::error("Undefined variable in strict mode: {}", varName);
                    return "";
                }
            }

            result.replace(start, end - start, replacement);

            // Find next variable
            auto next = findNextVariable(result, start + replacement.length(), format);
            found = std::get<0>(next);
            start = std::get<1>(next);
            end = std::get<2>(next);
            varName = std::get<3>(next);
        }

        // If no changes were made or not recursive, break
        if (!recursive || result == previousResult) {
            break;
        }
    }

    return result;
}

auto EnvUtils::validateEnvironmentVariables(const HashMap<String, String>& vars, bool strict)
    -> HashMap<String, ValidationResult> {
    HashMap<String, ValidationResult> results;

    for (const auto& [name, value] : vars) {
        results[name] = validateVariable(name, value, strict);
    }

    return results;
}

auto EnvUtils::validateVariable(const String& name, const String& value, bool strict)
    -> ValidationResult {
    Vector<String> suggestions;

    // Validate name
    if (name.empty()) {
        return ValidationResult(false, "Variable name cannot be empty");
    }

    if (!isValidVariableName(name)) {
        return ValidationResult(false, "Invalid variable name format", suggestions);
    }

    if (strict) {
        // Check for reserved names
        if (isReservedVariableName(name)) {
            return ValidationResult(false, "Variable name is reserved", suggestions);
        }

        // Check name conventions
        if (std::islower(name[0])) {
            suggestions.push_back("Consider using uppercase: " + String(1, std::toupper(name[0])) + name.substr(1));
        }
    }

    // Validate value
    if (value.find('\0') != String::npos) {
        return ValidationResult(false, "Variable value contains null bytes");
    }

    if (strict && value.length() > 32768) {  // 32KB limit
        return ValidationResult(false, "Variable value exceeds maximum length");
    }

    return ValidationResult(true);
}

auto EnvUtils::sanitizeEnvironmentVariables(const HashMap<String, String>& vars,
                                             bool removeNullBytes, bool trimWhitespace)
    -> HashMap<String, String> {
    HashMap<String, String> sanitized;

    for (const auto& [name, value] : vars) {
        String sanitizedValue = value;

        if (removeNullBytes) {
            sanitizedValue.erase(std::remove(sanitizedValue.begin(), sanitizedValue.end(), '\0'),
                                sanitizedValue.end());
        }

        if (trimWhitespace) {
            // Trim leading whitespace
            size_t start = sanitizedValue.find_first_not_of(" \t\r\n");
            if (start == String::npos) {
                sanitizedValue = "";
            } else {
                // Trim trailing whitespace
                size_t end = sanitizedValue.find_last_not_of(" \t\r\n");
                sanitizedValue = sanitizedValue.substr(start, end - start + 1);
            }
        }

        sanitized[name] = sanitizedValue;
    }

    return sanitized;
}

auto EnvUtils::filterVariablesByPattern(const HashMap<String, String>& vars,
                                         const String& pattern, bool includeValues)
    -> HashMap<String, String> {
    HashMap<String, String> filtered;

    try {
        std::regex regex_pattern(std::string(pattern.c_str()));

        for (const auto& [name, value] : vars) {
            bool matches = std::regex_search(std::string(name.c_str()), regex_pattern);

            if (!matches && includeValues) {
                matches = std::regex_search(std::string(value.c_str()), regex_pattern);
            }

            if (matches) {
                filtered[name] = value;
            }
        }
    } catch (const std::regex_error& e) {
        spdlog::error("Invalid regex pattern: {}", e.what());
        return vars;  // Return original if pattern is invalid
    }

    return filtered;
}

auto EnvUtils::findCircularDependencies(const HashMap<String, String>& vars) -> Vector<String> {
    auto graph = buildDependencyGraph(vars);
    return detectCycles(graph);
}

auto EnvUtils::resolveDependencyOrder(const HashMap<String, String>& vars) -> Vector<String> {
    auto graph = buildDependencyGraph(vars);
    return topologicalSort(graph);
}

auto EnvUtils::createTemplate(const String& name, const String& description,
                              const HashMap<String, String>& vars,
                              const Vector<String>& required) -> EnvTemplate {
    EnvTemplate tmpl(name, description);
    tmpl.variables = vars;
    tmpl.requiredVariables = required;

    // Extract default values for required variables
    for (const auto& reqVar : required) {
        auto it = vars.find(reqVar);
        if (it != vars.end()) {
            tmpl.defaultValues[reqVar] = it->second;
        }
    }

    return tmpl;
}

auto EnvUtils::applyTemplate(const EnvTemplate& tmpl,
                             const HashMap<String, String>& overrides,
                             bool validateRequired) -> HashMap<String, String> {
    HashMap<String, String> result = tmpl.variables;

    // Apply overrides
    for (const auto& [key, value] : overrides) {
        result[key] = value;
    }

    // Validate required variables if requested
    if (validateRequired) {
        for (const auto& reqVar : tmpl.requiredVariables) {
            if (result.find(reqVar) == result.end() || result[reqVar].empty()) {
                // Use default value if available
                auto defaultIt = tmpl.defaultValues.find(reqVar);
                if (defaultIt != tmpl.defaultValues.end()) {
                    result[reqVar] = defaultIt->second;
                } else {
                    spdlog::error("Required variable '{}' not provided in template '{}'", reqVar, tmpl.name);
                }
            }
        }
    }

    return result;
}

auto EnvUtils::interpolateTemplate(const String& template_str,
                                   const HashMap<String, String>& vars,
                                   VariableFormat format) -> String {
    return expandVariablesAdvanced(template_str, format, 0,
        [&vars](const String& varName) -> std::optional<String> {
            auto it = vars.find(varName);
            return (it != vars.end()) ? std::optional<String>(it->second) : std::nullopt;
        });
}

auto EnvUtils::escapeVariableValue(const String& value, VariableFormat format) -> String {
    (void)format;  // Suppress unused parameter warning for now
    String escaped;
    escaped.reserve(value.length() * 2);  // Reserve extra space for escapes

    for (char c : value) {
        switch (c) {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += c;
                break;
        }
    }

    return escaped;
}

auto EnvUtils::unescapeVariableValue(const String& value, VariableFormat format) -> String {
    (void)format;  // Suppress unused parameter warning for now
    String unescaped;
    unescaped.reserve(value.length());

    for (size_t i = 0; i < value.length(); ++i) {
        if (value[i] == '\\' && i + 1 < value.length()) {
            switch (value[i + 1]) {
                case '"':
                    unescaped += '"';
                    ++i;
                    break;
                case '\\':
                    unescaped += '\\';
                    ++i;
                    break;
                case 'n':
                    unescaped += '\n';
                    ++i;
                    break;
                case 'r':
                    unescaped += '\r';
                    ++i;
                    break;
                case 't':
                    unescaped += '\t';
                    ++i;
                    break;
                default:
                    unescaped += value[i];
                    break;
            }
        } else {
            unescaped += value[i];
        }
    }

    return unescaped;
}

auto EnvUtils::suggestVariableNames(const String& name,
                                    const Vector<String>& availableVars,
                                    size_t maxSuggestions) -> Vector<String> {
    Vector<std::pair<String, size_t>> candidates;

    for (const auto& available : availableVars) {
        size_t distance = calculateLevenshteinDistance(name, available);
        if (distance <= 3) {  // Only suggest if reasonably close
            candidates.emplace_back(available, distance);
        }
    }

    // Sort by distance (closest first)
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });

    Vector<String> suggestions;
    size_t count = std::min(maxSuggestions, candidates.size());
    for (size_t i = 0; i < count; ++i) {
        suggestions.push_back(candidates[i].first);
    }

    return suggestions;
}

auto EnvUtils::convertVariableFormat(const HashMap<String, String>& vars,
                                     VariableFormat targetFormat) -> HashMap<String, String> {
    HashMap<String, String> converted;

    for (const auto& [name, value] : vars) {
        String convertedValue = value;

        // Convert variable references in the value
        if (targetFormat == VariableFormat::WINDOWS) {
            // Convert ${VAR} and $VAR to %VAR%
            convertedValue = std::regex_replace(std::string(convertedValue.c_str()),
                                               std::regex(R"(\$\{([^}]+)\})"), "%$1%");
            convertedValue = std::regex_replace(std::string(convertedValue.c_str()),
                                               std::regex(R"(\$([A-Za-z_][A-Za-z0-9_]*))"), "%$1%");
        } else if (targetFormat == VariableFormat::UNIX) {
            // Convert %VAR% to ${VAR}
            convertedValue = std::regex_replace(std::string(convertedValue.c_str()),
                                               std::regex(R"(%([A-Za-z_][A-Za-z0-9_]*)%)"), "${$1}");
        }

        converted[name] = String(convertedValue);
    }

    return converted;
}

auto EnvUtils::analyzeVariableUsage(const HashMap<String, String>& vars) -> HashMap<String, String> {
    HashMap<String, String> analysis;

    size_t totalVars = vars.size();
    size_t emptyVars = 0;
    size_t longVars = 0;
    size_t withReferences = 0;

    for (const auto& [name, value] : vars) {
        if (value.empty()) {
            emptyVars++;
        }
        if (value.length() > 1000) {
            longVars++;
        }
        if (value.find('$') != String::npos || value.find('%') != String::npos) {
            withReferences++;
        }
    }

    analysis["total_variables"] = std::to_string(totalVars);
    analysis["empty_variables"] = std::to_string(emptyVars);
    analysis["long_variables"] = std::to_string(longVars);
    analysis["variables_with_references"] = std::to_string(withReferences);
    analysis["empty_percentage"] = std::to_string((emptyVars * 100) / std::max(totalVars, size_t(1)));
    analysis["reference_percentage"] = std::to_string((withReferences * 100) / std::max(totalVars, size_t(1)));

    return analysis;
}

// Helper method implementations
auto EnvUtils::extractVariableReferences(const String& str, VariableFormat format) -> Vector<String> {
    Vector<String> references;
    String pattern;

    switch (format) {
        case VariableFormat::UNIX:
            pattern = R"(\$\{([^}]+)\}|\$([A-Za-z_][A-Za-z0-9_]*))";
            break;
        case VariableFormat::WINDOWS:
            pattern = R"(%([A-Za-z_][A-Za-z0-9_]*)%)";
            break;
        default:
            // Auto-detect or use both patterns
            pattern = R"(\$\{([^}]+)\}|\$([A-Za-z_][A-Za-z0-9_]*)|%([A-Za-z_][A-Za-z0-9_]*)%)";
            break;
    }

    try {
        std::regex regex_pattern(pattern);
        std::sregex_iterator iter(str.begin(), str.end(), regex_pattern);
        std::sregex_iterator end;

        for (; iter != end; ++iter) {
            const std::smatch& match = *iter;
            for (size_t i = 1; i < match.size(); ++i) {
                if (match[i].matched) {
                    references.push_back(String(match[i].str()));
                    break;
                }
            }
        }
    } catch (const std::regex_error& e) {
        spdlog::error("Regex error in extractVariableReferences: {}", e.what());
    }

    return references;
}

auto EnvUtils::buildDependencyGraph(const HashMap<String, String>& vars)
    -> HashMap<String, Vector<String>> {
    HashMap<String, Vector<String>> graph;

    for (const auto& [name, value] : vars) {
        auto references = extractVariableReferences(value, VariableFormat::AUTO);
        graph[name] = references;
    }

    return graph;
}

auto EnvUtils::topologicalSort(const HashMap<String, Vector<String>>& graph) -> Vector<String> {
    Vector<String> result;
    std::unordered_set<String> visited;
    std::unordered_set<String> visiting;

    std::function<bool(const String&)> visit = [&](const String& node) -> bool {
        if (visiting.find(node) != visiting.end()) {
            return false;  // Cycle detected
        }
        if (visited.find(node) != visited.end()) {
            return true;   // Already processed
        }

        visiting.insert(node);

        auto it = graph.find(node);
        if (it != graph.end()) {
            for (const auto& dependency : it->second) {
                if (!visit(dependency)) {
                    return false;
                }
            }
        }

        visiting.erase(node);
        visited.insert(node);
        result.push_back(node);
        return true;
    };

    for (const auto& [node, deps] : graph) {
        if (visited.find(node) == visited.end()) {
            if (!visit(node)) {
                spdlog::warn("Cycle detected in dependency graph");
                break;
            }
        }
    }

    std::reverse(result.begin(), result.end());
    return result;
}

auto EnvUtils::detectCycles(const HashMap<String, Vector<String>>& graph) -> Vector<String> {
    Vector<String> cycleNodes;
    std::unordered_set<String> visited;
    std::unordered_set<String> visiting;

    std::function<bool(const String&)> hasCycle = [&](const String& node) -> bool {
        if (visiting.find(node) != visiting.end()) {
            cycleNodes.push_back(node);
            return true;
        }
        if (visited.find(node) != visited.end()) {
            return false;
        }

        visiting.insert(node);

        auto it = graph.find(node);
        if (it != graph.end()) {
            for (const auto& dependency : it->second) {
                if (hasCycle(dependency)) {
                    cycleNodes.push_back(node);
                    return true;
                }
            }
        }

        visiting.erase(node);
        visited.insert(node);
        return false;
    };

    for (const auto& [node, deps] : graph) {
        if (visited.find(node) == visited.end()) {
            if (hasCycle(node)) {
                break;
            }
        }
    }

    return cycleNodes;
}

auto EnvUtils::calculateLevenshteinDistance(const String& s1, const String& s2) -> size_t {
    const size_t len1 = s1.length();
    const size_t len2 = s2.length();

    std::vector<std::vector<size_t>> dp(len1 + 1, std::vector<size_t>(len2 + 1));

    for (size_t i = 0; i <= len1; ++i) {
        dp[i][0] = i;
    }
    for (size_t j = 0; j <= len2; ++j) {
        dp[0][j] = j;
    }

    for (size_t i = 1; i <= len1; ++i) {
        for (size_t j = 1; j <= len2; ++j) {
            if (s1[i - 1] == s2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                dp[i][j] = 1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
            }
        }
    }

    return dp[len1][len2];
}

auto EnvUtils::normalizeVariableName(const String& name, bool caseSensitive) -> String {
    String normalized = name;
    if (!caseSensitive) {
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::toupper);
    }
    return normalized;
}

auto EnvUtils::processEscapeSequences(const String& str) -> String {
    String result;
    result.reserve(str.length());

    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            switch (str[i + 1]) {
                case 'n':
                    result += '\n';
                    ++i;
                    break;
                case 't':
                    result += '\t';
                    ++i;
                    break;
                case 'r':
                    result += '\r';
                    ++i;
                    break;
                case '\\':
                    result += '\\';
                    ++i;
                    break;
                default:
                    result += str[i];
                    break;
            }
        } else {
            result += str[i];
        }
    }

    return result;
}

auto EnvUtils::isReservedVariableName(const String& name) -> bool {
    static const std::unordered_set<String> reserved = {
        "PATH", "HOME", "USER", "USERNAME", "SHELL", "TERM", "PWD",
        "OLDPWD", "LANG", "LC_ALL", "TZ", "TMPDIR", "TEMP", "TMP"
    };

    return reserved.find(name) != reserved.end();
}

auto EnvUtils::getVariableNamePattern(VariableFormat format) -> String {
    switch (format) {
        case VariableFormat::UNIX:
            return R"(\$\{([^}]+)\}|\$([A-Za-z_][A-Za-z0-9_]*))";
        case VariableFormat::WINDOWS:
            return R"(%([A-Za-z_][A-Za-z0-9_]*)%)";
        default:
            return R"(\$\{([^}]+)\}|\$([A-Za-z_][A-Za-z0-9_]*)|%([A-Za-z_][A-Za-z0-9_]*)%)";
    }
}

}  // namespace atom::utils
