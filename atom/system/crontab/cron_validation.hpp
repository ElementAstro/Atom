#ifndef CRON_VALIDATION_HPP
#define CRON_VALIDATION_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <optional>

/**
 * @brief Detailed result of cron validation with enhanced information
 */
struct CronValidationResult {
    bool valid;
    std::string message;
    std::vector<std::string> warnings;  // Non-fatal issues
    std::vector<std::string> suggestions;  // Optimization suggestions

    CronValidationResult(bool v = false, std::string msg = "")
        : valid(v), message(std::move(msg)) {}
};

/**
 * @brief Cron field types for specific validation
 */
enum class CronField {
    MINUTE = 0,
    HOUR = 1,
    DAY_OF_MONTH = 2,
    MONTH = 3,
    DAY_OF_WEEK = 4
};

/**
 * @brief Timezone information for cron expressions
 */
struct TimezoneInfo {
    std::string timezone_id;
    int utc_offset_minutes;
    bool is_dst_aware;

    TimezoneInfo(std::string tz = "UTC", int offset = 0, bool dst = false)
        : timezone_id(std::move(tz)), utc_offset_minutes(offset), is_dst_aware(dst) {}
};

/**
 * @brief Enhanced cron expression validation with comprehensive features
 */
class CronValidation {
public:
    /**
     * @brief Validates a cron expression with comprehensive checks.
     * @param cronExpr The cron expression to validate.
     * @param timezone Optional timezone information.
     * @return Detailed validation result.
     */
    static auto validateCronExpression(const std::string& cronExpr,
                                     const std::optional<TimezoneInfo>& timezone = std::nullopt)
        -> CronValidationResult;

    /**
     * @brief Validates a specific cron field.
     * @param field The field type to validate.
     * @param value The field value to validate.
     * @return Validation result for the specific field.
     */
    static auto validateCronField(CronField field, const std::string& value)
        -> CronValidationResult;

    /**
     * @brief Validates step values (e.g., star/5, 10-20/2).
     * @param field The field type.
     * @param value The step expression.
     * @return Validation result.
     */
    static auto validateStepExpression(CronField field, const std::string& value)
        -> CronValidationResult;

    /**
     * @brief Validates range expressions (e.g., 10-20, MON-FRI).
     * @param field The field type.
     * @param value The range expression.
     * @return Validation result.
     */
    static auto validateRangeExpression(CronField field, const std::string& value)
        -> CronValidationResult;

    /**
     * @brief Validates list expressions (e.g., 1,3,5,7).
     * @param field The field type.
     * @param value The list expression.
     * @return Validation result.
     */
    static auto validateListExpression(CronField field, const std::string& value)
        -> CronValidationResult;

    /**
     * @brief Converts a special cron expression to standard format.
     * @param specialExpr The special expression to convert (e.g., @daily).
     * @return The standard cron expression or empty string if not recognized.
     */
    static auto convertSpecialExpression(const std::string& specialExpr)
        -> std::string;

    /**
     * @brief Converts named values to numeric (e.g., MON -> 1, JAN -> 1).
     * @param field The field type.
     * @param value The named value.
     * @return Numeric equivalent or -1 if invalid.
     */
    static auto convertNamedValue(CronField field, const std::string& value) -> int;

    /**
     * @brief Gets the valid range for a cron field.
     * @param field The field type.
     * @return Pair of (min, max) values.
     */
    static auto getFieldRange(CronField field) -> std::pair<int, int>;

    /**
     * @brief Calculates next execution times for a cron expression.
     * @param cronExpr The cron expression.
     * @param count Number of next executions to calculate.
     * @param from Starting time point.
     * @return Vector of next execution times.
     */
    static auto calculateNextExecutions(const std::string& cronExpr,
                                      size_t count = 5,
                                      std::chrono::system_clock::time_point from = std::chrono::system_clock::now())
        -> std::vector<std::chrono::system_clock::time_point>;

    /**
     * @brief Checks if a cron expression would execute too frequently.
     * @param cronExpr The cron expression.
     * @param max_frequency_per_hour Maximum allowed executions per hour.
     * @return True if frequency is acceptable.
     */
    static auto checkExecutionFrequency(const std::string& cronExpr,
                                       size_t max_frequency_per_hour = 60) -> bool;

    /**
     * @brief Suggests optimizations for a cron expression.
     * @param cronExpr The cron expression.
     * @return Vector of optimization suggestions.
     */
    static auto suggestOptimizations(const std::string& cronExpr)
        -> std::vector<std::string>;

private:
    static const std::unordered_map<std::string, std::string> specialExpressions_;
    static const std::unordered_map<std::string, int> monthNames_;
    static const std::unordered_map<std::string, int> dayNames_;

    static auto parseFieldValue(const std::string& value, int min, int max) -> std::vector<int>;
    static auto isValidNumber(const std::string& str, int min, int max) -> bool;
    static auto splitString(const std::string& str, char delimiter) -> std::vector<std::string>;
};

#endif // CRON_VALIDATION_HPP
