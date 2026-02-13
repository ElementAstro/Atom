/*
 * env_utils.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: Environment variable utility functions

**************************************************/

#ifndef ATOM_SYSTEM_ENV_UTILS_HPP
#define ATOM_SYSTEM_ENV_UTILS_HPP

#include <tuple>
#include <regex>
#include <functional>
#include <optional>

#include "atom/containers/high_performance.hpp"
#include "atom/macro.hpp"
#include "env_core.hpp"

namespace atom::utils {

using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;
template <typename T>
using Vector = atom::containers::Vector<T>;

/**
 * @brief Variable expansion options
 */
enum class ExpansionOptions {
    NONE = 0,
    RECURSIVE = 1,          // Allow recursive expansion
    STRICT = 2,             // Fail on undefined variables
    ESCAPE_SEQUENCES = 4,   // Process escape sequences
    CASE_INSENSITIVE = 8    // Case insensitive variable names
};

/**
 * @brief Variable validation result
 */
struct ValidationResult {
    bool isValid;
    String errorMessage;
    Vector<String> suggestions;

    ValidationResult(bool valid = true, const String& error = "", const Vector<String>& suggest = {})
        : isValid(valid), errorMessage(error), suggestions(suggest) {}
};

/**
 * @brief Environment variable template
 */
struct EnvTemplate {
    String name;
    String description;
    HashMap<String, String> variables;
    Vector<String> requiredVariables;
    HashMap<String, String> defaultValues;

    EnvTemplate(const String& n = "", const String& desc = "")
        : name(n), description(desc) {}
};

/**
 * @brief Variable substitution callback
 */
using VariableSubstitutionCallback = std::function<std::optional<String>(const String& varName)>;

/**
 * @brief Environment variable utility functions
 */
class EnvUtils {
public:
    /**
     * @brief Expands environment variable references in a string
     * @param str String containing environment variable references (e.g.,
     * "$HOME/file" or "%PATH%;newpath")
     * @param format Environment variable format, can be Unix style (${VAR}) or
     * Windows style (%VAR%)
     * @return Expanded string
     */
    ATOM_NODISCARD static auto expandVariables(
        const String& str, VariableFormat format = VariableFormat::AUTO)
        -> String;

    /**
     * @brief Compares differences between two environment variable sets
     * @param env1 First environment variable set
     * @param env2 Second environment variable set
     * @return Difference content, including added, removed, and modified
     * variables
     */
    ATOM_NODISCARD static auto diffEnvironments(
        const HashMap<String, String>& env1,
        const HashMap<String, String>& env2)
        -> std::tuple<HashMap<String, String>,   // Added variables
                      HashMap<String, String>,   // Removed variables
                      HashMap<String, String>>;  // Modified variables

    /**
     * @brief Merges two environment variable sets
     * @param baseEnv Base environment variable set
     * @param overlayEnv Overlay environment variable set
     * @param override Whether to override base environment variables in case of
     * conflict
     * @return Merged environment variable set
     */
    ATOM_NODISCARD static auto mergeEnvironments(
        const HashMap<String, String>& baseEnv,
        const HashMap<String, String>& overlayEnv, bool override = true)
        -> HashMap<String, String>;

    // ========== NEW ENHANCED FEATURES ==========

    /**
     * @brief Expands variables with advanced options
     * @param str String containing variable references
     * @param format Variable format
     * @param options Expansion options (bitwise OR of ExpansionOptions)
     * @param customResolver Custom variable resolver function
     * @return Expanded string
     */
    ATOM_NODISCARD static auto expandVariablesAdvanced(
        const String& str,
        VariableFormat format = VariableFormat::AUTO,
        int options = static_cast<int>(ExpansionOptions::NONE),
        VariableSubstitutionCallback customResolver = nullptr) -> String;

    /**
     * @brief Validates environment variable names and values
     * @param vars Map of variables to validate
     * @param strict Whether to use strict validation rules
     * @return Map of validation results
     */
    ATOM_NODISCARD static auto validateEnvironmentVariables(
        const HashMap<String, String>& vars, bool strict = false)
        -> HashMap<String, ValidationResult>;

    /**
     * @brief Validates a single environment variable
     * @param name Variable name
     * @param value Variable value
     * @param strict Whether to use strict validation rules
     * @return Validation result
     */
    ATOM_NODISCARD static auto validateVariable(
        const String& name, const String& value, bool strict = false)
        -> ValidationResult;

    /**
     * @brief Sanitizes environment variable values
     * @param vars Map of variables to sanitize
     * @param removeNullBytes Whether to remove null bytes
     * @param trimWhitespace Whether to trim leading/trailing whitespace
     * @return Sanitized variables map
     */
    ATOM_NODISCARD static auto sanitizeEnvironmentVariables(
        const HashMap<String, String>& vars,
        bool removeNullBytes = true,
        bool trimWhitespace = true) -> HashMap<String, String>;

    /**
     * @brief Filters environment variables by pattern
     * @param vars Variables to filter
     * @param pattern Regex pattern to match against variable names
     * @param includeValues Whether to also match against values
     * @return Filtered variables map
     */
    ATOM_NODISCARD static auto filterVariablesByPattern(
        const HashMap<String, String>& vars,
        const String& pattern,
        bool includeValues = false) -> HashMap<String, String>;

    /**
     * @brief Finds circular dependencies in variable references
     * @param vars Variables to check
     * @return Vector of variable names involved in circular dependencies
     */
    ATOM_NODISCARD static auto findCircularDependencies(
        const HashMap<String, String>& vars) -> Vector<String>;

    /**
     * @brief Resolves variable dependencies in correct order
     * @param vars Variables to resolve
     * @return Vector of variable names in dependency order
     */
    ATOM_NODISCARD static auto resolveDependencyOrder(
        const HashMap<String, String>& vars) -> Vector<String>;

    /**
     * @brief Creates an environment template
     * @param name Template name
     * @param description Template description
     * @param vars Variables to include in template
     * @param required Required variable names
     * @return Environment template
     */
    ATOM_NODISCARD static auto createTemplate(
        const String& name,
        const String& description,
        const HashMap<String, String>& vars,
        const Vector<String>& required = {}) -> EnvTemplate;

    /**
     * @brief Applies an environment template
     * @param tmpl Template to apply
     * @param overrides Variable overrides
     * @param validateRequired Whether to validate required variables
     * @return Applied variables map
     */
    ATOM_NODISCARD static auto applyTemplate(
        const EnvTemplate& tmpl,
        const HashMap<String, String>& overrides = {},
        bool validateRequired = true) -> HashMap<String, String>;

    /**
     * @brief Interpolates string templates with environment variables
     * @param template_str Template string with placeholders
     * @param vars Variables for interpolation
     * @param format Placeholder format
     * @return Interpolated string
     */
    ATOM_NODISCARD static auto interpolateTemplate(
        const String& template_str,
        const HashMap<String, String>& vars,
        VariableFormat format = VariableFormat::AUTO) -> String;

    /**
     * @brief Escapes special characters in variable values
     * @param value Value to escape
     * @param format Target format
     * @return Escaped value
     */
    ATOM_NODISCARD static auto escapeVariableValue(
        const String& value, VariableFormat format = VariableFormat::AUTO) -> String;

    /**
     * @brief Unescapes special characters in variable values
     * @param value Value to unescape
     * @param format Source format
     * @return Unescaped value
     */
    ATOM_NODISCARD static auto unescapeVariableValue(
        const String& value, VariableFormat format = VariableFormat::AUTO) -> String;

    /**
     * @brief Generates variable name suggestions for typos
     * @param name Potentially misspelled variable name
     * @param availableVars Available variable names
     * @param maxSuggestions Maximum number of suggestions
     * @return Vector of suggested variable names
     */
    ATOM_NODISCARD static auto suggestVariableNames(
        const String& name,
        const Vector<String>& availableVars,
        size_t maxSuggestions = 5) -> Vector<String>;

    /**
     * @brief Converts environment variables to different formats
     * @param vars Variables to convert
     * @param targetFormat Target format
     * @return Converted variables in target format
     */
    ATOM_NODISCARD static auto convertVariableFormat(
        const HashMap<String, String>& vars,
        VariableFormat targetFormat) -> HashMap<String, String>;

    /**
     * @brief Analyzes environment variable usage patterns
     * @param vars Variables to analyze
     * @return Analysis results map
     */
    ATOM_NODISCARD static auto analyzeVariableUsage(
        const HashMap<String, String>& vars) -> HashMap<String, String>;

private:
    /**
     * @brief Expands Unix-style variable references (${VAR} or $VAR)
     * @param str The string to expand
     * @return Expanded string
     */
    static auto expandUnixVariables(const String& str) -> String;

    /**
     * @brief Expands Windows-style variable references (%VAR%)
     * @param str The string to expand
     * @return Expanded string
     */
    static auto expandWindowsVariables(const String& str) -> String;

    /**
     * @brief Finds the next variable reference in a string
     * @param str The string to search
     * @param start Starting position
     * @param format Variable format to look for
     * @return Tuple of (found, start_pos, end_pos, var_name)
     */
    static auto findNextVariable(const String& str, size_t start,
                                 VariableFormat format)
        -> std::tuple<bool, size_t, size_t, String>;

    /**
     * @brief Validates a variable name
     * @param name The variable name to validate
     * @return True if valid, otherwise false
     */
    static auto isValidVariableName(const String& name) -> bool;

    // Enhanced helper methods
    static auto extractVariableReferences(const String& str, VariableFormat format)
        -> Vector<String>;

    static auto buildDependencyGraph(const HashMap<String, String>& vars)
        -> HashMap<String, Vector<String>>;

    static auto topologicalSort(const HashMap<String, Vector<String>>& graph)
        -> Vector<String>;

    static auto detectCycles(const HashMap<String, Vector<String>>& graph)
        -> Vector<String>;

    static auto calculateLevenshteinDistance(const String& s1, const String& s2)
        -> size_t;

    static auto normalizeVariableName(const String& name, bool caseSensitive = true)
        -> String;

    static auto processEscapeSequences(const String& str) -> String;

    static auto isReservedVariableName(const String& name) -> bool;

    static auto getVariableNamePattern(VariableFormat format) -> String;
};

}  // namespace atom::utils

#endif  // ATOM_SYSTEM_ENV_UTILS_HPP
