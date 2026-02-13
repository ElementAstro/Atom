#include "cron_validation.hpp"

#include <algorithm>
#include <cctype>

const std::unordered_map<std::string, std::string>
    CronValidation::specialExpressions_ = {
        {"@yearly", "0 0 1 1 *"},  {"@annually", "0 0 1 1 *"},
        {"@monthly", "0 0 1 * *"}, {"@weekly", "0 0 * * 0"},
        {"@daily", "0 0 * * *"},   {"@midnight", "0 0 * * *"},
        {"@hourly", "0 * * * *"},  {"@reboot", "@reboot"}};

const std::unordered_map<std::string, int> CronValidation::monthNames_ = {
    {"JAN", 1}, {"FEB", 2}, {"MAR", 3}, {"APR", 4}, {"MAY", 5}, {"JUN", 6},
    {"JUL", 7}, {"AUG", 8}, {"SEP", 9}, {"OCT", 10}, {"NOV", 11}, {"DEC", 12}
};

const std::unordered_map<std::string, int> CronValidation::dayNames_ = {
    {"SUN", 0}, {"MON", 1}, {"TUE", 2}, {"WED", 3},
    {"THU", 4}, {"FRI", 5}, {"SAT", 6}
};

auto CronValidation::validateCronExpression(const std::string& cronExpr,
                                           const std::optional<TimezoneInfo>& timezone)
    -> CronValidationResult {
    CronValidationResult result;

    if (cronExpr.empty()) {
        result.valid = false;
        result.message = "Empty cron expression";
        return result;
    }

    // Handle special expressions
    if (cronExpr[0] == '@') {
        const std::string converted = convertSpecialExpression(cronExpr);
        if (converted.empty()) {
            result.valid = false;
            result.message = "Unknown special expression: " + cronExpr;
            return result;
        }
        if (converted == "@reboot") {
            result.valid = true;
            result.message = "Valid special expression: reboot";
            if (timezone.has_value()) {
                result.warnings.push_back("Timezone ignored for @reboot expression");
            }
            return result;
        }
        return validateCronExpression(converted, timezone);
    }

    // Parse fields
    auto fields = splitString(cronExpr, ' ');
    if (fields.size() != 5) {
        result.valid = false;
        result.message = "Invalid cron expression format. Expected 5 fields, got " +
                        std::to_string(fields.size());
        return result;
    }

    // Validate each field
    std::vector<CronField> fieldTypes = {
        CronField::MINUTE, CronField::HOUR, CronField::DAY_OF_MONTH,
        CronField::MONTH, CronField::DAY_OF_WEEK
    };

    for (size_t i = 0; i < fields.size(); ++i) {
        auto fieldResult = validateCronField(fieldTypes[i], fields[i]);
        if (!fieldResult.valid) {
            result.valid = false;
            result.message = fieldResult.message;
            return result;
        }
        // Collect warnings and suggestions
        result.warnings.insert(result.warnings.end(),
                              fieldResult.warnings.begin(), fieldResult.warnings.end());
        result.suggestions.insert(result.suggestions.end(),
                                 fieldResult.suggestions.begin(), fieldResult.suggestions.end());
    }

    // Additional validations
    if (!checkExecutionFrequency(cronExpr)) {
        result.warnings.push_back("Expression may execute very frequently (>60 times per hour)");
    }

    // Add optimization suggestions
    auto optimizations = suggestOptimizations(cronExpr);
    result.suggestions.insert(result.suggestions.end(),
                             optimizations.begin(), optimizations.end());

    result.valid = true;
    result.message = "Valid cron expression";
    return result;
}

auto CronValidation::validateCronField(CronField field, const std::string& value)
    -> CronValidationResult {
    CronValidationResult result;

    if (value.empty()) {
        result.valid = false;
        result.message = "Empty field value";
        return result;
    }

    auto [min, max] = getFieldRange(field);

    // Handle wildcard
    if (value == "*") {
        result.valid = true;
        result.message = "Valid wildcard";
        return result;
    }

    // Handle step expressions
    if (value.find('/') != std::string::npos) {
        return validateStepExpression(field, value);
    }

    // Handle range expressions
    if (value.find('-') != std::string::npos) {
        return validateRangeExpression(field, value);
    }

    // Handle list expressions
    if (value.find(',') != std::string::npos) {
        return validateListExpression(field, value);
    }

    // Handle single value
    int numValue = convertNamedValue(field, value);
    if (numValue == -1) {
        // Try parsing as number
        try {
            numValue = std::stoi(value);
        } catch (const std::exception&) {
            result.valid = false;
            result.message = "Invalid field value: " + value;
            return result;
        }
    }

    if (numValue < min || numValue > max) {
        result.valid = false;
        result.message = "Value " + std::to_string(numValue) +
                        " out of range [" + std::to_string(min) +
                        "-" + std::to_string(max) + "]";
        return result;
    }

    result.valid = true;
    result.message = "Valid field value";
    return result;
}

auto CronValidation::validateStepExpression(CronField field, const std::string& value)
    -> CronValidationResult {
    CronValidationResult result;

    auto parts = splitString(value, '/');
    if (parts.size() != 2) {
        result.valid = false;
        result.message = "Invalid step expression format";
        return result;
    }

    // Validate base part
    auto baseResult = validateCronField(field, parts[0]);
    if (!baseResult.valid && parts[0] != "*") {
        return baseResult;
    }

    // Validate step value
    try {
        int step = std::stoi(parts[1]);
        if (step <= 0) {
            result.valid = false;
            result.message = "Step value must be positive";
            return result;
        }

        auto [min, max] = getFieldRange(field);
        if (step > (max - min + 1)) {
            result.warnings.push_back("Step value larger than field range");
        }

        result.valid = true;
        result.message = "Valid step expression";
        return result;
    } catch (const std::exception&) {
        result.valid = false;
        result.message = "Invalid step value: " + parts[1];
        return result;
    }
}

auto CronValidation::validateRangeExpression(CronField field, const std::string& value)
    -> CronValidationResult {
    CronValidationResult result;

    auto parts = splitString(value, '-');
    if (parts.size() != 2) {
        result.valid = false;
        result.message = "Invalid range expression format";
        return result;
    }

    int start = convertNamedValue(field, parts[0]);
    int end = convertNamedValue(field, parts[1]);

    if (start == -1) {
        try {
            start = std::stoi(parts[0]);
        } catch (const std::exception&) {
            result.valid = false;
            result.message = "Invalid range start: " + parts[0];
            return result;
        }
    }

    if (end == -1) {
        try {
            end = std::stoi(parts[1]);
        } catch (const std::exception&) {
            result.valid = false;
            result.message = "Invalid range end: " + parts[1];
            return result;
        }
    }

    auto [min, max] = getFieldRange(field);
    if (start < min || start > max || end < min || end > max) {
        result.valid = false;
        result.message = "Range values out of valid range [" +
                        std::to_string(min) + "-" + std::to_string(max) + "]";
        return result;
    }

    if (start > end) {
        result.valid = false;
        result.message = "Range start cannot be greater than end";
        return result;
    }

    result.valid = true;
    result.message = "Valid range expression";
    return result;
}

auto CronValidation::convertSpecialExpression(const std::string& specialExpr)
    -> std::string {
    if (specialExpr.empty() || specialExpr[0] != '@') {
        return specialExpr;
    }

    auto it = specialExpressions_.find(specialExpr);
    return it != specialExpressions_.end() ? it->second : "";
}

auto CronValidation::validateListExpression(CronField field, const std::string& value)
    -> CronValidationResult {
    CronValidationResult result;

    auto parts = splitString(value, ',');
    if (parts.empty()) {
        result.valid = false;
        result.message = "Empty list expression";
        return result;
    }

    for (const auto& part : parts) {
        auto partResult = validateCronField(field, part);
        if (!partResult.valid) {
            result.valid = false;
            result.message = "Invalid list item: " + part + " - " + partResult.message;
            return result;
        }
    }

    if (parts.size() > 10) {
        result.warnings.push_back("Large list expression may impact performance");
    }

    result.valid = true;
    result.message = "Valid list expression";
    return result;
}

auto CronValidation::convertNamedValue(CronField field, const std::string& value) -> int {
    std::string upperValue = value;
    std::transform(upperValue.begin(), upperValue.end(), upperValue.begin(), ::toupper);

    switch (field) {
        case CronField::MONTH: {
            auto it = monthNames_.find(upperValue);
            return it != monthNames_.end() ? it->second : -1;
        }
        case CronField::DAY_OF_WEEK: {
            auto it = dayNames_.find(upperValue);
            return it != dayNames_.end() ? it->second : -1;
        }
        default:
            return -1;
    }
}

auto CronValidation::getFieldRange(CronField field) -> std::pair<int, int> {
    switch (field) {
        case CronField::MINUTE:      return {0, 59};
        case CronField::HOUR:        return {0, 23};
        case CronField::DAY_OF_MONTH: return {1, 31};
        case CronField::MONTH:       return {1, 12};
        case CronField::DAY_OF_WEEK: return {0, 7};  // 0 and 7 both represent Sunday
        default:                     return {0, 0};
    }
}

auto CronValidation::splitString(const std::string& str, char delimiter) -> std::vector<std::string> {
    std::vector<std::string> result;
    std::string current;

    for (char c : str) {
        if (c == delimiter) {
            if (!current.empty()) {
                result.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }

    if (!current.empty()) {
        result.push_back(current);
    }

    return result;
}

auto CronValidation::checkExecutionFrequency(const std::string& cronExpr,
                                           size_t max_frequency_per_hour) -> bool {
    // Simple heuristic: check if minute field allows more than max_frequency_per_hour executions
    auto fields = splitString(cronExpr, ' ');
    if (fields.empty()) return false;

    const std::string& minuteField = fields[0];

    // If it's a wildcard or step with small value, it might be too frequent
    if (minuteField == "*") {
        return false; // Every minute = 60 times per hour
    }

    if (minuteField.find('/') != std::string::npos) {
        auto parts = splitString(minuteField, '/');
        if (parts.size() == 2) {
            try {
                int step = std::stoi(parts[1]);
                return (60 / step) <= static_cast<int>(max_frequency_per_hour);
            } catch (const std::exception&) {
                return true; // If we can't parse, assume it's okay
            }
        }
    }

    return true; // For other cases, assume it's okay
}

auto CronValidation::suggestOptimizations(const std::string& cronExpr)
    -> std::vector<std::string> {
    std::vector<std::string> suggestions;

    auto fields = splitString(cronExpr, ' ');
    if (fields.size() != 5) return suggestions;

    // Check for redundant wildcards
    if (fields[2] == "*" && fields[4] != "*") {
        suggestions.push_back("Consider using specific day of month instead of wildcard when day of week is specified");
    }

    // Check for very frequent execution
    if (fields[0] == "*") {
        suggestions.push_back("Consider using step values (e.g., */5) instead of every minute");
    }

    // Check for midnight execution
    if (fields[0] == "0" && fields[1] == "0") {
        suggestions.push_back("Consider staggering midnight jobs to avoid system load spikes");
    }

    return suggestions;
}

auto CronValidation::calculateNextExecutions([[maybe_unused]] const std::string& cronExpr,
                                            [[maybe_unused]] size_t count,
                                            [[maybe_unused]] std::chrono::system_clock::time_point from)
    -> std::vector<std::chrono::system_clock::time_point> {
    std::vector<std::chrono::system_clock::time_point> result;

    // This is a simplified implementation
    // A full implementation would require complex date/time calculations
    // For now, we'll return empty vector with a note that this needs full implementation

    // TODO: Implement full cron expression parsing and next execution calculation
    // This would involve:
    // 1. Parsing each field into allowed values
    // 2. Finding next valid combination of minute/hour/day/month/weekday
    // 3. Handling edge cases like leap years, month boundaries, etc.

    return result;
}
