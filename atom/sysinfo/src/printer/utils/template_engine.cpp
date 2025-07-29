#include "template_engine.hpp"
#include "string_utils.hpp"
#include "format_utils.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <stdexcept>

namespace atom::system::utils {

TemplateEngine::TemplateEngine(const TemplateVariables& variables)
    : variables_(variables) {
    registerDefaultHelpers();
}

void TemplateEngine::setVariables(const TemplateVariables& variables) {
    variables_ = variables;
}

void TemplateEngine::setVariable(const std::string& name, const TemplateValue& value) {
    variables_[name] = value;
}

void TemplateEngine::registerHelper(const std::string& name, const TemplateHelper& helper) {
    helpers_[name] = helper;
}

auto TemplateEngine::render(const std::string& templateStr) const -> std::string {
    std::string result = templateStr;

    // Process in order: helpers, conditionals, loops, then variables
    result = processHelpers(result);
    result = processConditionals(result);
    result = processLoops(result);
    result = processVariables(result);

    return result;
}

auto TemplateEngine::renderFile(const std::string& templateFile) const -> std::string {
    std::ifstream file(templateFile);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open template file: " + templateFile);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return render(buffer.str());
}

auto TemplateEngine::hasVariable(const std::string& name) const -> bool {
    return variables_.find(name) != variables_.end();
}

auto TemplateEngine::getVariableAsString(const std::string& name) const -> std::string {
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        return valueToString(it->second);
    }
    return "";
}

auto TemplateEngine::processVariables(const std::string& templateStr) const -> std::string {
    std::string result = templateStr;
    std::regex varRegex(R"(\{\{([^#/][^}]*)\}\})");
    std::smatch match;

    while (std::regex_search(result, match, varRegex)) {
        std::string varName = trim(match[1].str());
        std::string replacement;

        if (hasVariable(varName)) {
            replacement = getVariableAsString(varName);
        }

        result = result.substr(0, match.position()) + replacement +
                result.substr(match.position() + match.length());
    }

    return result;
}

auto TemplateEngine::processConditionals(const std::string& templateStr) const -> std::string {
    std::string result = templateStr;
    std::regex ifRegex(R"(\{\{#if\s+([^}]+)\}\}(.*?)\{\{/if\}\})");
    std::smatch match;

    while (std::regex_search(result, match, ifRegex)) {
        std::string condition = trim(match[1].str());
        std::string content = match[2].str();
        std::string replacement;

        if (hasVariable(condition)) {
            auto value = variables_.at(condition);
            if (isTruthy(value)) {
                replacement = content;
            }
        }

        result = result.substr(0, match.position()) + replacement +
                result.substr(match.position() + match.length());
    }

    return result;
}

auto TemplateEngine::processLoops(const std::string& templateStr) const -> std::string {
    // Simple implementation - would need more complex logic for real arrays
    return templateStr;
}

auto TemplateEngine::processHelpers(const std::string& templateStr) const -> std::string {
    std::string result = templateStr;
    std::regex helperRegex(R"(\{\{([a-zA-Z_][a-zA-Z0-9_]*)\s+([^}]*)\}\})");
    std::smatch match;

    while (std::regex_search(result, match, helperRegex)) {
        std::string helperName = match[1].str();
        std::string argsStr = trim(match[2].str());
        std::string replacement;

        auto helperIt = helpers_.find(helperName);
        if (helperIt != helpers_.end()) {
            auto args = split(argsStr, ' ');
            // Process arguments to resolve variables
            for (auto& arg : args) {
                arg = trim(arg);
                if (hasVariable(arg)) {
                    arg = getVariableAsString(arg);
                }
            }

            try {
                replacement = helperIt->second(args);
            } catch (const std::exception&) {
                replacement = ""; // Helper failed, use empty string
            }
        }

        result = result.substr(0, match.position()) + replacement +
                result.substr(match.position() + match.length());
    }

    return result;
}

auto TemplateEngine::valueToString(const TemplateValue& value) const -> std::string {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return v;
        } else if constexpr (std::is_same_v<T, bool>) {
            return v ? "true" : "false";
        } else {
            return std::to_string(v);
        }
    }, value);
}

auto TemplateEngine::isTruthy(const TemplateValue& value) const -> bool {
    return std::visit([](const auto& v) -> bool {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return !v.empty();
        } else if constexpr (std::is_same_v<T, bool>) {
            return v;
        } else {
            return v != 0;
        }
    }, value);
}

auto TemplateEngine::extractTagContent(const std::string& templateStr,
                                      const std::string& startTag,
                                      const std::string& endTag) const
    -> std::pair<std::string, std::string> {

    size_t startPos = templateStr.find(startTag);
    if (startPos == std::string::npos) {
        return {"", templateStr};
    }

    size_t contentStart = templateStr.find("}}", startPos) + 2;
    size_t endPos = templateStr.find(endTag, contentStart);

    if (endPos == std::string::npos) {
        return {"", templateStr};
    }

    std::string content = templateStr.substr(contentStart, endPos - contentStart);
    std::string remaining = templateStr.substr(0, startPos) +
                           templateStr.substr(templateStr.find("}}", endPos) + 2);

    return {content, remaining};
}

void TemplateEngine::registerDefaultHelpers() {
    // Format bytes helper
    registerHelper("formatBytes", [](const std::vector<std::string>& args) -> std::string {
        if (args.empty()) return "";
        try {
            uint64_t bytes = std::stoull(args[0]);
            return formatBytes(bytes);
        } catch (...) {
            return args[0];
        }
    });

    // Format percentage helper
    registerHelper("formatPercentage", [](const std::vector<std::string>& args) -> std::string {
        if (args.empty()) return "";
        try {
            double percentage = std::stod(args[0]);
            int precision = args.size() > 1 ? std::stoi(args[1]) : 1;
            return formatPercentage(percentage, precision);
        } catch (...) {
            return args[0];
        }
    });

    // Uppercase helper
    registerHelper("upper", [](const std::vector<std::string>& args) -> std::string {
        if (args.empty()) return "";
        return toUpper(args[0]);
    });

    // Lowercase helper
    registerHelper("lower", [](const std::vector<std::string>& args) -> std::string {
        if (args.empty()) return "";
        return toLower(args[0]);
    });

    // Default value helper
    registerHelper("default", [](const std::vector<std::string>& args) -> std::string {
        if (args.size() < 2) return "";
        return args[0].empty() ? args[1] : args[0];
    });
}

auto createSystemTemplateEngine() -> TemplateEngine {
    TemplateEngine engine;

    // Add common system variables
    engine.setVariable("generator", std::string("Atom System Information Printer"));
    engine.setVariable("timestamp", formatTimestamp());

    return engine;
}

} // namespace atom::system::utils
