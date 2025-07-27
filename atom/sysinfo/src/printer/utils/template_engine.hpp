/**
 * @file template_engine.hpp
 * @brief Simple template engine for system information reports
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_UTILS_TEMPLATE_ENGINE_HPP
#define ATOM_SYSINFO_PRINTER_UTILS_TEMPLATE_ENGINE_HPP

#include <string>
#include <unordered_map>
#include <functional>
#include <variant>

namespace atom::system::utils {

/**
 * @brief Template variable value type
 */
using TemplateValue = std::variant<std::string, int, double, bool>;

/**
 * @brief Template variables map
 */
using TemplateVariables = std::unordered_map<std::string, TemplateValue>;

/**
 * @brief Template helper function type
 */
using TemplateHelper = std::function<std::string(const std::vector<std::string>&)>;

/**
 * @brief Template helpers map
 */
using TemplateHelpers = std::unordered_map<std::string, TemplateHelper>;

/**
 * @class TemplateEngine
 * @brief Simple template engine with variable substitution and basic logic
 */
class TemplateEngine {
public:
    /**
     * @brief Default constructor
     */
    TemplateEngine() = default;

    /**
     * @brief Constructor with variables
     * @param variables Initial template variables
     */
    explicit TemplateEngine(const TemplateVariables& variables);

    /**
     * @brief Set template variables
     * @param variables The variables to set
     */
    void setVariables(const TemplateVariables& variables);

    /**
     * @brief Set a single template variable
     * @param name Variable name
     * @param value Variable value
     */
    void setVariable(const std::string& name, const TemplateValue& value);

    /**
     * @brief Register a template helper function
     * @param name Helper name
     * @param helper Helper function
     */
    void registerHelper(const std::string& name, const TemplateHelper& helper);

    /**
     * @brief Render template from string
     * @param templateStr The template string
     * @return Rendered output
     */
    [[nodiscard]] auto render(const std::string& templateStr) const -> std::string;

    /**
     * @brief Render template from file
     * @param templateFile Path to template file
     * @return Rendered output
     */
    [[nodiscard]] auto renderFile(const std::string& templateFile) const -> std::string;

    /**
     * @brief Check if variable exists
     * @param name Variable name
     * @return true if variable exists
     */
    [[nodiscard]] auto hasVariable(const std::string& name) const -> bool;

    /**
     * @brief Get variable value as string
     * @param name Variable name
     * @return Variable value as string, or empty string if not found
     */
    [[nodiscard]] auto getVariableAsString(const std::string& name) const -> std::string;

private:
    TemplateVariables variables_;
    TemplateHelpers helpers_;

    /**
     * @brief Process template variables ({{variable}})
     * @param templateStr The template string
     * @return String with variables substituted
     */
    [[nodiscard]] auto processVariables(const std::string& templateStr) const -> std::string;

    /**
     * @brief Process template conditionals ({{#if condition}})
     * @param templateStr The template string
     * @return String with conditionals processed
     */
    [[nodiscard]] auto processConditionals(const std::string& templateStr) const -> std::string;

    /**
     * @brief Process template loops ({{#each array}})
     * @param templateStr The template string
     * @return String with loops processed
     */
    [[nodiscard]] auto processLoops(const std::string& templateStr) const -> std::string;

    /**
     * @brief Process template helpers ({{helper arg1 arg2}})
     * @param templateStr The template string
     * @return String with helpers processed
     */
    [[nodiscard]] auto processHelpers(const std::string& templateStr) const -> std::string;

    /**
     * @brief Convert template value to string
     * @param value The template value
     * @return String representation
     */
    [[nodiscard]] auto valueToString(const TemplateValue& value) const -> std::string;

    /**
     * @brief Check if template value is truthy
     * @param value The template value
     * @return true if value is truthy
     */
    [[nodiscard]] auto isTruthy(const TemplateValue& value) const -> bool;

    /**
     * @brief Extract content between template tags
     * @param templateStr The template string
     * @param startTag Start tag (e.g., "{{#if")
     * @param endTag End tag (e.g., "{{/if}}")
     * @return Extracted content and remaining string
     */
    [[nodiscard]] auto extractTagContent(const std::string& templateStr,
                                        const std::string& startTag,
                                        const std::string& endTag) const
        -> std::pair<std::string, std::string>;

    /**
     * @brief Register default helper functions
     */
    void registerDefaultHelpers();
};

/**
 * @brief Create a template engine with common system information variables
 * @return Configured template engine
 */
[[nodiscard]] auto createSystemTemplateEngine() -> TemplateEngine;

} // namespace atom::system::utils

#endif // ATOM_SYSINFO_PRINTER_UTILS_TEMPLATE_ENGINE_HPP