#ifndef CRON_VALIDATION_HPP
#define CRON_VALIDATION_HPP

#include <string>
#include <unordered_map>

/**
 * @brief Result of cron validation
 */
struct CronValidationResult {
    bool valid;
    std::string message;
};

/**
 * @brief Provides cron expression validation functionality
 */
class CronValidation {
public:
    /**
     * @brief Validates a cron expression.
     * @param cronExpr The cron expression to validate.
     * @return Validation result with validity and message.
     */
    static auto validateCronExpression(const std::string& cronExpr)
        -> CronValidationResult;

    /**
     * @brief Converts a special cron expression to standard format.
     * @param specialExpr The special expression to convert (e.g., @daily).
     * @return The standard cron expression or empty string if not recognized.
     */
    static auto convertSpecialExpression(const std::string& specialExpr)
        -> std::string;

private:
    static const std::unordered_map<std::string, std::string>
        specialExpressions_;
};

#endif // CRON_VALIDATION_HPP
