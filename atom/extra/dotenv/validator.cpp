#include "validator.hpp"

#include <algorithm>

namespace dotenv {

// ValidationSchema Implementation
ValidationSchema& ValidationSchema::required(const std::string& key) {
    required_vars_.push_back(key);
    return *this;
}

ValidationSchema& ValidationSchema::optional(const std::string& key,
                                             const std::string& default_value) {
    defaults_[key] = default_value;
    return *this;
}

ValidationSchema& ValidationSchema::rule(const std::string& key,
                                         std::shared_ptr<ValidationRule> rule) {
    rules_[key].push_back(std::move(rule));
    return *this;
}

ValidationSchema& ValidationSchema::rules(
    const std::string& key,
    std::vector<std::shared_ptr<ValidationRule>> rules) {
    rules_[key] = std::move(rules);
    return *this;
}

bool ValidationSchema::isRequired(const std::string& key) const {
    return std::find(required_vars_.begin(), required_vars_.end(), key) !=
           required_vars_.end();
}

std::string ValidationSchema::getDefault(const std::string& key) const {
    auto it = defaults_.find(key);
    return it != defaults_.end() ? it->second : std::string{};
}

std::vector<std::shared_ptr<ValidationRule>> ValidationSchema::getRules(
    const std::string& key) const {
    auto it = rules_.find(key);
    return it != rules_.end() ? it->second
                              : std::vector<std::shared_ptr<ValidationRule>>{};
}

std::vector<std::string> ValidationSchema::getRequiredVariables() const {
    return required_vars_;
}

// Validator Implementation
ValidationResult Validator::validate(
    const std::unordered_map<std::string, std::string>& env_vars,
    const ValidationSchema& schema) {
    ValidationResult result;
    result.processed_vars = env_vars;

    // Check required variables
    for (const auto& required_var : schema.getRequiredVariables()) {
        if (env_vars.find(required_var) == env_vars.end()) {
            result.addError("Required variable '" + required_var +
                            "' is missing");
            continue;
        }

        const std::string& value = env_vars.at(required_var);
        auto rules = schema.getRules(required_var);
        validateVariable(required_var, value, rules, result);
    }

    // Validate existing variables that have rules
    for (const auto& [key, value] : env_vars) {
        auto rules = schema.getRules(key);
        if (!rules.empty() && !schema.isRequired(key)) {
            validateVariable(key, value, rules, result);
        }
    }

    return result;
}

ValidationResult Validator::validateWithDefaults(
    std::unordered_map<std::string, std::string>& env_vars,
    const ValidationSchema& schema) {
    // Apply defaults first
    for (const auto& required_var : schema.getRequiredVariables()) {
        if (env_vars.find(required_var) == env_vars.end()) {
            std::string default_val = schema.getDefault(required_var);
            if (!default_val.empty()) {
                env_vars[required_var] = default_val;
            }
        }
    }

    return validate(env_vars, schema);
}

bool Validator::validateVariable(
    const std::string& key, const std::string& value,
    const std::vector<std::shared_ptr<ValidationRule>>& rules,
    ValidationResult& result) {
    bool valid = true;

    for (const auto& rule : rules) {
        if (!rule->validate(value)) {
            result.addError("Variable '" + key +
                            "' failed validation: " + rule->getErrorMessage());
            valid = false;
        }
    }

    return valid;
}

// Built-in validation rules
namespace rules {

std::shared_ptr<ValidationRule> notEmpty() {
    return std::make_shared<ValidationRule>(
        "notEmpty", [](const std::string& value) { return !value.empty(); },
        "Value cannot be empty");
}

std::shared_ptr<ValidationRule> minLength(size_t min_len) {
    return std::make_shared<ValidationRule>(
        "minLength",
        [min_len](const std::string& value) {
            return value.length() >= min_len;
        },
        "Value must be at least " + std::to_string(min_len) +
            " characters long");
}

std::shared_ptr<ValidationRule> maxLength(size_t max_len) {
    return std::make_shared<ValidationRule>(
        "maxLength",
        [max_len](const std::string& value) {
            return value.length() <= max_len;
        },
        "Value must be at most " + std::to_string(max_len) +
            " characters long");
}

std::shared_ptr<ValidationRule> pattern(const std::regex& regex,
                                        const std::string& description) {
    return std::make_shared<ValidationRule>(
        "pattern",
        [regex](const std::string& value) {
            return std::regex_match(value, regex);
        },
        description.empty() ? "Value does not match required pattern"
                            : description);
}

std::shared_ptr<ValidationRule> numeric() {
    return std::make_shared<ValidationRule>(
        "numeric",
        [](const std::string& value) {
            if (value.empty())
                return false;
            char* end;
            std::strtod(value.c_str(), &end);
            return *end == '\0';
        },
        "Value must be numeric");
}

std::shared_ptr<ValidationRule> integer() {
    return std::make_shared<ValidationRule>(
        "integer",
        [](const std::string& value) {
            if (value.empty())
                return false;
            char* end;
            std::strtol(value.c_str(), &end, 10);
            return *end == '\0';
        },
        "Value must be an integer");
}

std::shared_ptr<ValidationRule> boolean() {
    return std::make_shared<ValidationRule>(
        "boolean",
        [](const std::string& value) {
            std::string lower = value;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           ::tolower);
            return lower == "true" || lower == "false" || lower == "1" ||
                   lower == "0" || lower == "yes" || lower == "no" ||
                   lower == "on" || lower == "off";
        },
        "Value must be a boolean (true/false, 1/0, yes/no, on/off)");
}

std::shared_ptr<ValidationRule> url() {
    static const std::regex url_regex(R"(^https?://[^\s/$.?#].[^\s]*$)",
                                      std::regex_constants::icase);
    return pattern(url_regex, "Value must be a valid URL");
}

std::shared_ptr<ValidationRule> email() {
    static const std::regex email_regex(
        R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
    return pattern(email_regex, "Value must be a valid email address");
}

std::shared_ptr<ValidationRule> oneOf(
    const std::vector<std::string>& allowed_values) {
    return std::make_shared<ValidationRule>(
        "oneOf",
        [allowed_values](const std::string& value) {
            return std::find(allowed_values.begin(), allowed_values.end(),
                             value) != allowed_values.end();
        },
        "Value must be one of the allowed values");
}

std::shared_ptr<ValidationRule> custom(ValidationRule::Validator validator,
                                       const std::string& error_message) {
    return std::make_shared<ValidationRule>("custom", std::move(validator),
                                            error_message);
}

}  // namespace rules

}  // namespace dotenv
