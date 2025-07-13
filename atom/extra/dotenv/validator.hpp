#pragma once

#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

namespace dotenv {

/**
 * @brief Validation rule for environment variables
 */
class ValidationRule {
public:
    using Validator = std::function<bool(const std::string&)>;

    ValidationRule(std::string name, Validator validator,
                   std::string error_message)
        : name_(std::move(name)),
          validator_(std::move(validator)),
          error_message_(std::move(error_message)) {}

    bool validate(const std::string& value) const { return validator_(value); }

    const std::string& getName() const { return name_; }
    const std::string& getErrorMessage() const { return error_message_; }

private:
    std::string name_;
    Validator validator_;
    std::string error_message_;
};

/**
 * @brief Schema for validating environment variables
 */
class ValidationSchema {
public:
    /**
     * @brief Add a required variable
     */
    ValidationSchema& required(const std::string& key);

    /**
     * @brief Add an optional variable with default value
     */
    ValidationSchema& optional(const std::string& key,
                               const std::string& default_value = "");

    /**
     * @brief Add validation rule for a variable
     */
    ValidationSchema& rule(const std::string& key,
                           std::shared_ptr<ValidationRule> rule);

    /**
     * @brief Add multiple rules for a variable
     */
    ValidationSchema& rules(const std::string& key,
                            std::vector<std::shared_ptr<ValidationRule>> rules);

    /**
     * @brief Check if variable is required
     */
    bool isRequired(const std::string& key) const;

    /**
     * @brief Get default value for variable
     */
    std::string getDefault(const std::string& key) const;

    /**
     * @brief Get validation rules for variable
     */
    std::vector<std::shared_ptr<ValidationRule>> getRules(
        const std::string& key) const;

    /**
     * @brief Get all required variables
     */
    std::vector<std::string> getRequiredVariables() const;

private:
    std::vector<std::string> required_vars_;
    std::unordered_map<std::string, std::string> defaults_;
    std::unordered_map<std::string,
                       std::vector<std::shared_ptr<ValidationRule>>>
        rules_;
};

/**
 * @brief Built-in validation rules
 */
namespace rules {

std::shared_ptr<ValidationRule> notEmpty();
std::shared_ptr<ValidationRule> minLength(size_t min_len);
std::shared_ptr<ValidationRule> maxLength(size_t max_len);
std::shared_ptr<ValidationRule> pattern(const std::regex& regex,
                                        const std::string& description = "");
std::shared_ptr<ValidationRule> numeric();
std::shared_ptr<ValidationRule> integer();
std::shared_ptr<ValidationRule> boolean();
std::shared_ptr<ValidationRule> url();
std::shared_ptr<ValidationRule> email();
std::shared_ptr<ValidationRule> oneOf(
    const std::vector<std::string>& allowed_values);
std::shared_ptr<ValidationRule> custom(ValidationRule::Validator validator,
                                       const std::string& error_message);

}  // namespace rules

/**
 * @brief Validation result
 */
struct ValidationResult {
    bool is_valid = true;
    std::vector<std::string> errors;
    std::unordered_map<std::string, std::string> processed_vars;

    void addError(const std::string& error) {
        errors.push_back(error);
        is_valid = false;
    }
};

/**
 * @brief Environment variable validator
 */
class Validator {
public:
    /**
     * @brief Validate environment variables against schema
     */
    ValidationResult validate(
        const std::unordered_map<std::string, std::string>& env_vars,
        const ValidationSchema& schema);

    /**
     * @brief Validate and apply defaults
     */
    ValidationResult validateWithDefaults(
        std::unordered_map<std::string, std::string>& env_vars,
        const ValidationSchema& schema);

private:
    bool validateVariable(
        const std::string& key, const std::string& value,
        const std::vector<std::shared_ptr<ValidationRule>>& rules,
        ValidationResult& result);
};

}  // namespace dotenv
