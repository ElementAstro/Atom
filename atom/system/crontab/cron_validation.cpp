#include "cron_validation.hpp"

#include <regex>
#include <sstream>

#include "spdlog/spdlog.h"

const std::unordered_map<std::string, std::string>
    CronValidation::specialExpressions_ = {
        {"@yearly", "0 0 1 1 *"},  {"@annually", "0 0 1 1 *"},
        {"@monthly", "0 0 1 * *"}, {"@weekly", "0 0 * * 0"},
        {"@daily", "0 0 * * *"},   {"@midnight", "0 0 * * *"},
        {"@hourly", "0 * * * *"},  {"@reboot", "@reboot"}};

auto CronValidation::validateCronExpression(const std::string& cronExpr)
    -> CronValidationResult {
    if (!cronExpr.empty() && cronExpr[0] == '@') {
        const std::string converted = convertSpecialExpression(cronExpr);
        if (converted == cronExpr) {
            return {false, "Unknown special expression"};
        }
        if (converted == "@reboot") {
            return {true, "Valid special expression: reboot"};
        }
        return validateCronExpression(converted);
    }

    static const std::regex cronRegex(R"(^(\S+\s+){4}\S+$)");
    if (!std::regex_match(cronExpr, cronRegex)) {
        return {false, "Invalid cron expression format. Expected 5 fields."};
    }

    std::stringstream ss(cronExpr);
    std::string minute, hour, dayOfMonth, month, dayOfWeek;
    ss >> minute >> hour >> dayOfMonth >> month >> dayOfWeek;

    static const std::regex minuteRegex(
        R"(^(\*|[0-5]?[0-9](-[0-5]?[0-9])?)(,(\*|[0-5]?[0-9](-[0-5]?[0-9])?))*$)");
    if (!std::regex_match(minute, minuteRegex)) {
        return {false, "Invalid minute field"};
    }

    static const std::regex hourRegex(
        R"(^(\*|[01]?[0-9]|2[0-3](-([01]?[0-9]|2[0-3]))?)(,(\*|[01]?[0-9]|2[0-3](-([01]?[0-9]|2[0-3]))?))*$)");
    if (!std::regex_match(hour, hourRegex)) {
        return {false, "Invalid hour field"};
    }

    return {true, "Valid cron expression"};
}

auto CronValidation::convertSpecialExpression(const std::string& specialExpr)
    -> std::string {
    if (specialExpr.empty() || specialExpr[0] != '@') {
        return specialExpr;
    }

    auto it = specialExpressions_.find(specialExpr);
    return it != specialExpressions_.end() ? it->second : "";
}
