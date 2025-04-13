#include "qdatetime.hpp"
#include "qtimezone.hpp"

#include <ctime>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
// #include <sstream> // Included header sstream is not used directly
// #include <iomanip> // Included header iomanip is not used directly

#ifdef __AVX2__
#include <immintrin.h>
#endif

#include "atom/log/loguru.hpp"

namespace atom::utils {

// 保留自定义的 PairHash 函数对象
struct PairHash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        // Combine hashes using XOR and bit shift
        // Consider using a more robust hash combination method if needed
        return std::hash<T1>{}(p.first) ^ (std::hash<T2>{}(p.second) << 1);
    }
};

// Thread-safe cache for expensive operations
class DateTimeCache {
public:
    static DateTimeCache& instance() {
        static DateTimeCache cache;
        return cache;
    }

    std::time_t getCachedTimeOffset(const QTimeZone& tz, const QDateTime& dt) {
        std::lock_guard<std::mutex> lock(mutex_);
        // 使用与 unordered_map 键类型完全匹配的键类型
        const std::pair<std::string, std::time_t> key(tz.identifier(),
                                                      dt.toTimeT());

        auto it = timeOffsetCache_.find(key);
        if (it != timeOffsetCache_.end()) {
            return it->second;
        }
        // Assuming offsetFromUtc returns std::chrono::seconds or similar
        // duration
        const auto offset = tz.offsetFromUtc(dt);       // Get duration directly
        timeOffsetCache_.emplace(key, offset.count());  // Store seconds count
        return offset.count();
    }

private:
    DateTimeCache() = default;
    std::mutex mutex_;
    std::unordered_map<std::pair<std::string, std::time_t>, std::time_t,
                       PairHash>
        timeOffsetCache_;
};

QDateTime::QDateTime()
    : dateTime_(std::nullopt),
      timeZone_(std::nullopt) {  // Initialize timeZone_
    LOG_F(INFO, "QDateTime default constructor called");
}

QDateTime::QDateTime(int year, int month, int day, int hour, int minute,
                     int second, int ms)
    : timeZone_(std::nullopt) {  // Initialize timeZone_
    LOG_F(INFO, "QDateTime constructor with components called");

    try {
        // Validate components
        validateDate(year, month, day);
        validateTime(hour, minute, second, ms);

        // Create tm struct (assuming local time interpretation for components)
        std::tm t = {};
        t.tm_year = year - 1900;  // Years since 1900
        t.tm_mon = month - 1;     // Months are 0-11
        t.tm_mday = day;
        t.tm_hour = hour;
        t.tm_min = minute;
        t.tm_sec = second;
        t.tm_isdst = -1;  // Let mktime determine DST

        // Create timepoint from time_t (mktime assumes local time)
        auto timeT = std::mktime(&t);
        if (timeT == -1) {
            throw std::invalid_argument(
                "Invalid date/time components or mktime failure");
        }

        // Set dateTime_ using the time_t and add milliseconds
        dateTime_ = Clock::from_time_t(timeT) + std::chrono::milliseconds(ms);
        // By default, constructed QDateTime represents local time unless
        // specified otherwise timeZone_ remains nullopt, implying local time
        // context from components

        LOG_F(INFO,
              "QDateTime created with components: {}-%02d-%02d "
              "%02d:%02d:%02d.%03d (local)",
              year, month, day, hour, minute, second, ms);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in QDateTime component constructor: {}",
              e.what());
        // Consider re-throwing or setting an invalid state
        dateTime_ = std::nullopt;
        throw;  // Re-throw original exception
    }
}

void QDateTime::validateDate(int year, int month, int day) {
    // Consider a wider range or using std::chrono::year_month_day for
    // validation
    if (year < 1900 || year > 2099) {  // Adjusted range slightly, still limited
        throw std::out_of_range("Year out of range (1900-2099)");
    }
    if (month < 1 || month > 12) {
        throw std::out_of_range("Month out of range (1-12)");
    }

    // Determine days in month, accounting for leap years
    int daysInMonth;
    if (month == 2) {  // February
        bool isLeap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        daysInMonth = isLeap ? 29 : 28;
    } else if (month == 4 || month == 6 || month == 9 || month == 11) {
        daysInMonth = 30;
    } else {
        daysInMonth = 31;
    }

    if (day < 1 || day > daysInMonth) {
        throw std::out_of_range(
            "Day out of range for specified month and year");
    }
}

void QDateTime::validateTime(int hour, int minute, int second, int ms) {
    if (hour < 0 || hour > 23) {
        throw std::out_of_range("Hour out of range (0-23)");
    }
    if (minute < 0 || minute > 59) {
        throw std::out_of_range("Minute out of range (0-59)");
    }
    if (second < 0 || second > 59) {  // Standard allows 60 for leap seconds,
                                      // but std::tm might not handle it well
        throw std::out_of_range("Second out of range (0-59)");
    }
    if (ms < 0 || ms > 999) {
        throw std::out_of_range("Millisecond out of range (0-999)");
    }
}

auto QDateTime::currentDateTime() -> QDateTime {
    LOG_F(INFO, "QDateTime::currentDateTime called");
    QDateTime dt;
    dt.dateTime_ = Clock::now();
    // Default constructor implies local timezone context
    // dt.timeZone_ = QTimeZone(); // 使用默认构造函数
    LOG_F(INFO, "QDateTime::currentDateTime returning current local time");
    return dt;
}

// This function seems redundant if currentDateTime() already returns local
// time. If the intention was to get current time in a *specific* zone, it needs
// adjustment.
auto QDateTime::currentDateTime(const QTimeZone& timeZone) -> QDateTime {
    LOG_F(INFO, "QDateTime::currentDateTime called with timeZone {}",
          timeZone.identifier().data());
    try {
        // Get current time point (implicitly UTC)
        auto nowUtc = Clock::now();

        // Create a temporary QDateTime representing this UTC time point
        QDateTime utcDt;
        utcDt.dateTime_ = nowUtc;
        // Assume QTimeZone needs a constructor for UTC, e.g., QTimeZone("UTC")
        // If QTimeZone has a static utc() method, use that.
        utcDt.timeZone_ = QTimeZone("UTC");  // Or QTimeZone::utc() if available

        // Calculate the offset for the target timezone *at the current UTC
        // time*
        auto offset = timeZone.offsetFromUtc(
            utcDt);  // Pass UTC time to get correct offset

        // Create the result QDateTime
        QDateTime dt;
        dt.dateTime_ = nowUtc + offset;  // Adjust the time point by the offset
        dt.timeZone_ = timeZone;         // Set the target timezone

        LOG_F(INFO,
              "QDateTime::currentDateTime returning current time in specified "
              "timezone");
        return dt;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in currentDateTime with timezone: {}",
              e.what());
        // Fallback to local time might be confusing, consider returning invalid
        // or rethrowing
        return currentDateTime();  // Fallback to local time
    }
}

auto QDateTime::toTimeT() const -> std::time_t {
    LOG_F(INFO, "QDateTime::toTimeT called");
    if (!dateTime_) {
        LOG_F(WARNING, "QDateTime::toTimeT called on invalid QDateTime");
        // Returning 0 might be misleading, consider throwing or returning
        // optional<time_t>
        return 0;
    }
    // Clock::to_time_t converts the time_point to time_t.
    // This value represents seconds since epoch UTC, regardless of the
    // QDateTime's timeZone_.
    std::time_t result = Clock::to_time_t(dateTime_.value());
    LOG_F(INFO, "QDateTime::toTimeT returning UTC epoch seconds: %lld",
          static_cast<long long>(result));
    return result;
}

auto QDateTime::isValid() const -> bool {
    // LOG_F(INFO, "QDateTime::isValid called"); // Reduce log verbosity
    bool result = dateTime_.has_value();
    // LOG_F(INFO, "QDateTime::isValid returning: {}", result ? "true" :
    // "false");
    return result;
}

// Renamed for clarity, throws if invalid
void QDateTime::ensureValidOrThrow() const {
    if (!isValid()) {
        LOG_F(ERROR, "Operation called on invalid QDateTime");
        throw std::logic_error("Operation called on invalid QDateTime");
    }
}

auto QDateTime::addDays(int days) const -> QDateTime {
    LOG_F(INFO, "QDateTime::addDays called with days: {}", days);
    // Use ensureValidOrThrow for consistency
    ensureValidOrThrow();  // Throws if invalid

    try {
        QDateTime dt = *this;  // Copy current state
        dt.dateTime_ = dateTime_.value() + std::chrono::hours(days * 24);
        // timeZone_ is copied from *this
        LOG_F(INFO, "QDateTime::addDays returning new QDateTime");
        return dt;
    } catch (const std::exception& e) {
        // Catch potential exceptions from chrono operations (though unlikely
        // here)
        LOG_F(ERROR, "Exception in addDays: {}", e.what());
        return {};  // Return invalid QDateTime on error
    }
}

auto QDateTime::addSecs(int seconds) const -> QDateTime {
    LOG_F(INFO, "QDateTime::addSecs called with seconds: {}", seconds);
    ensureValidOrThrow();

    try {
        QDateTime dt = *this;
        dt.dateTime_ = dateTime_.value() + std::chrono::seconds(seconds);
        LOG_F(INFO, "QDateTime::addSecs returning new QDateTime");
        return dt;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in addSecs: {}", e.what());
        return {};
    }
}

// Consider returning int64_t for larger differences
auto QDateTime::daysTo(const QDateTime& other) const -> int {
    LOG_F(INFO, "QDateTime::daysTo called");
    ensureValidOrThrow();
    other.ensureValidOrThrow();

    try {
        // Calculate difference in a high-resolution duration first
        auto duration = other.dateTime_.value() - dateTime_.value();
        // Convert to days (integer division truncates)
        auto days =
            std::chrono::duration_cast<std::chrono::hours>(duration).count() /
            24;
        // Cast to int, potentially losing data if difference is huge
        int result = static_cast<int>(days);
        LOG_F(INFO, "QDateTime::daysTo returning: {}", result);
        return result;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in daysTo: {}", e.what());
        return 0;  // Return 0 on error, might be ambiguous
    }
}

// Consider returning int64_t
auto QDateTime::secsTo(const QDateTime& other) const -> int {
    LOG_F(INFO, "QDateTime::secsTo called");
    ensureValidOrThrow();
    other.ensureValidOrThrow();

    try {
        auto duration = other.dateTime_.value() - dateTime_.value();
        auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        // Cast to int, potentially losing data
        int result = static_cast<int>(seconds);
        LOG_F(INFO, "QDateTime::secsTo returning: {}", result);
        return result;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in secsTo: {}", e.what());
        return 0;
    }
}

auto QDateTime::addMSecs(int msecs) const -> QDateTime {
    LOG_F(INFO, "QDateTime::addMSecs called with msecs: {}", msecs);
    ensureValidOrThrow();

    try {
        QDateTime dt = *this;
        dt.dateTime_ = dateTime_.value() + std::chrono::milliseconds(msecs);
        LOG_F(INFO, "QDateTime::addMSecs returning new QDateTime");
        return dt;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in addMSecs: {}", e.what());
        return {};
    }
}

// Adding calendar months/years is complex due to varying lengths and DST.
// Using std::tm is problematic as it relies on local time interpretation.
// A robust solution often requires a dedicated calendar library or careful
// handling of the time point based on the associated timezone rules. This
// implementation using std::tm might produce unexpected results across DST
// transitions or if the QDateTime doesn't represent local time.
auto QDateTime::addMonths(int months) const -> QDateTime {
    LOG_F(INFO, "QDateTime::addMonths called with months: {}", months);
    ensureValidOrThrow();

    try {
        // --- This approach using std::tm has limitations ---
        // 1. It assumes the current dateTime_ represents local time for tm
        // conversion.
        // 2. std::mktime behavior can be system-dependent, especially around
        // DST.
        // 3. It loses sub-second precision temporarily.

        // Convert current time_point to time_t (UTC seconds)
        std::time_t currentTimeT = Clock::to_time_t(dateTime_.value());

        // Convert time_t to broken-down struct *in the context of the current
        // timezone* If timeZone_ is set, we should ideally use its rules.
        // std::localtime uses system local. This is a major
        // simplification/potential source of error if timeZone_ is not local.
        std::tm tm = *std::localtime(
            &currentTimeT);  // Potential issue: uses system local time

        // Store original milliseconds
        auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                dateTime_.value().time_since_epoch() % std::chrono::seconds(1));

        // Calculate new month and year
        int currentMonth = tm.tm_mon;  // 0-11
        int currentYear = tm.tm_year;  // years since 1900

        currentMonth += months;
        currentYear += currentMonth / 12;
        currentMonth %= 12;
        if (currentMonth < 0) {
            currentMonth += 12;
            currentYear -= 1;  // Adjust year correctly for negative modulo
        }

        tm.tm_mon = currentMonth;
        tm.tm_year = currentYear;

        // Adjust day if it exceeds the new month's maximum
        // 移除未使用的变量
        // int originalDay = tm.tm_mday;

        // Use validateDate logic to find max days (or a helper)
        int daysInNewMonth;
        int year_ad = tm.tm_year + 1900;
        int month_ad = tm.tm_mon + 1;
        if (month_ad == 2) {  // February
            bool isLeap = (year_ad % 4 == 0 &&
                           (year_ad % 100 != 0 || year_ad % 400 == 0));
            daysInNewMonth = isLeap ? 29 : 28;
        } else if (month_ad == 4 || month_ad == 6 || month_ad == 9 ||
                   month_ad == 11) {
            daysInNewMonth = 30;
        } else {
            daysInNewMonth = 31;
        }

        if (tm.tm_mday > daysInNewMonth) {
            tm.tm_mday =
                daysInNewMonth;  // Clamp day to the max of the new month
        }

        // Convert back to time_t (still assumes local interpretation by mktime)
        tm.tm_isdst = -1;  // Let mktime figure out DST again
        std::time_t newTimeT = std::mktime(&tm);
        if (newTimeT == -1) {
            LOG_F(ERROR,
                  "Failed to calculate adjusted date via mktime when adding "
                  "months");
            throw std::runtime_error(
                "Failed to calculate adjusted date when adding months");
        }

        // Create new QDateTime from the new time_t and add back milliseconds
        QDateTime dt = *this;  // Copy timezone
        dt.dateTime_ = Clock::from_time_t(newTimeT) + milliseconds;

        LOG_F(INFO,
              "QDateTime::addMonths returning new QDateTime (potential issues "
              "with timezones/DST)");
        return dt;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in addMonths: {}", e.what());
        return {};
    }
}

auto QDateTime::addYears(int years) const -> QDateTime {
    LOG_F(INFO, "QDateTime::addYears called with years: {}", years);
    // This relies on addMonths, inheriting its limitations.
    return addMonths(years * 12);
}

// Helper to convert time_point to tm struct based on timezone context
std::tm QDateTime::toTm() const {
    ensureValidOrThrow();
    std::time_t timeT = Clock::to_time_t(dateTime_.value());
    // TODO: This is the critical part. If timeZone_ is set and not local,
    // we need a way to convert timeT to a tm struct according to that zone's
    // rules. Standard C++ doesn't provide this directly. Libraries like Howard
    // Hinnant's date/tz or ICU are needed for robust timezone handling. As a
    // fallback, we use std::localtime, which assumes system local time.
    if (timeZone_ && timeZone_->identifier() == "UTC") {
// If timezone is explicitly UTC, use gmtime
#ifdef _WIN32
        std::tm result;
        gmtime_s(&result, &timeT);
        return result;
#else
        return *std::gmtime(&timeT);
#endif
    } else {
// Otherwise (no timezone or non-UTC timezone), fall back to localtime
// This is an approximation if timeZone_ is set but not local.
#ifdef _WIN32
        std::tm result;
        localtime_s(&result, &timeT);
        return result;
#else
        return *std::localtime(&timeT);
#endif
    }
}

auto QDateTime::getDate() const -> Date {
    LOG_F(INFO, "QDateTime::getDate called");
    ensureValidOrThrow();

    try {
        std::tm tm = toTm();  // Use helper to get tm struct respecting timezone
                              // context (approximately)

        Date date;
        date.year = tm.tm_year + 1900;
        date.month = tm.tm_mon + 1;
        date.day = tm.tm_mday;

        LOG_F(INFO,
              "QDateTime::getDate returning date: {}-%02d-%02d (in object's "
              "timezone context)",
              date.year, date.month, date.day);
        return date;
    } catch (const std::exception& e) {
        // Catch exceptions from ensureValidOrThrow or potentially toTm
        LOG_F(ERROR, "Exception in getDate: {}", e.what());
        throw;  // Re-throw
    }
}

auto QDateTime::getTime() const -> Time {
    LOG_F(INFO, "QDateTime::getTime called");
    ensureValidOrThrow();

    try {
        std::tm tm = toTm();  // Use helper

        // Get milliseconds directly from the time_point
        auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                dateTime_.value().time_since_epoch() % std::chrono::seconds(1))
                .count();

        Time time;
        time.hour = tm.tm_hour;
        time.minute = tm.tm_min;
        time.second = tm.tm_sec;
        time.millisecond = static_cast<int>(milliseconds);  // Ensure positive

        LOG_F(INFO,
              "QDateTime::getTime returning time: %02d:%02d:%02d.%03d (in "
              "object's timezone context)",
              time.hour, time.minute, time.second, time.millisecond);
        return time;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in getTime: {}", e.what());
        throw;  // Re-throw
    }
}

auto QDateTime::setDate(int year, int month, int day) const -> QDateTime {
    LOG_F(INFO, "QDateTime::setDate called with year={}, month={}, day={}",
          year, month, day);
    ensureValidOrThrow();

    try {
        // Validate the new date components
        validateDate(year, month, day);

        // 移除未使用的变量
        // Time currentTime = getTime();

        // Create a tm struct with the new date and current time
        std::tm t = toTm();  // Get current tm struct
        t.tm_year = year - 1900;
        t.tm_mon = month - 1;
        t.tm_mday = day;
        // Keep existing time components (hour, min, sec) from t
        t.tm_isdst = -1;  // Let mktime determine DST

        // Convert back to time_t. CRITICAL: mktime assumes local time.
        // This will be incorrect if the QDateTime's timeZone_ is not local.
        // A timezone-aware conversion is needed here for correctness.
        std::time_t newTimeT = std::mktime(&t);
        if (newTimeT == -1) {
            LOG_F(ERROR, "mktime failed in setDate");
            throw std::runtime_error(
                "Failed to construct date/time in setDate");
        }

        // Get original milliseconds
        auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                dateTime_.value().time_since_epoch() % std::chrono::seconds(1));

        // Create new QDateTime, preserving original timezone
        QDateTime dt = *this;  // Copy timezone
        dt.dateTime_ = Clock::from_time_t(newTimeT) + milliseconds;

        LOG_F(INFO,
              "QDateTime::setDate returning new QDateTime (potential issues "
              "with timezones)");
        return dt;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in setDate: {}", e.what());
        // Return invalid or rethrow based on desired behavior
        return {};  // Return invalid
        // throw; // Or re-throw
    }
}

auto QDateTime::setTime(int hour, int minute, int second, int ms) const
    -> QDateTime {
    LOG_F(INFO,
          "QDateTime::setTime called with hour={}, minute={}, second={}, ms={}",
          hour, minute, second, ms);
    ensureValidOrThrow();

    try {
        // Validate the time components
        validateTime(hour, minute, second, ms);

        // 移除未使用的变量
        // Date currentDate = getDate(); // This uses toTm()

        // Create a tm struct with the current date and new time
        std::tm t = toTm();  // Get current tm struct
        // Keep existing date components (year, mon, mday) from t
        t.tm_hour = hour;
        t.tm_min = minute;
        t.tm_sec = second;
        t.tm_isdst = -1;  // Let mktime determine DST

        // Convert back to time_t (again, assumes local time via mktime)
        std::time_t newTimeT = std::mktime(&t);
        if (newTimeT == -1) {
            LOG_F(ERROR, "mktime failed in setTime");
            throw std::runtime_error(
                "Failed to construct date/time in setTime");
        }

        // Create new QDateTime, preserving original timezone
        QDateTime dt = *this;  // Copy timezone
        dt.dateTime_ =
            Clock::from_time_t(newTimeT) + std::chrono::milliseconds(ms);

        LOG_F(INFO,
              "QDateTime::setTime returning new QDateTime (potential issues "
              "with timezones)");
        return dt;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in setTime: {}", e.what());
        return {};  // Return invalid
        // throw; // Or re-throw
    }
}

auto QDateTime::setTimeZone(const QTimeZone& timeZone) const -> QDateTime {
    LOG_F(INFO, "QDateTime::setTimeZone called with zone {}",
          timeZone.identifier().data());
    ensureValidOrThrow();

    // This function should ideally *not* change the underlying time_point (UTC
    // instant). It should only change the timezone interpretation associated
    // with that instant.
    QDateTime dt = *this;     // Copy the time_point
    dt.timeZone_ = timeZone;  // Set the new timezone context

    LOG_F(INFO,
          "QDateTime::setTimeZone returning QDateTime with same time instant, "
          "new timezone context");
    return dt;
}

auto QDateTime::timeZone() const -> std::optional<QTimeZone> {
    // LOG_F(INFO, "QDateTime::timeZone called"); // Reduce log verbosity
    return timeZone_;
}

auto QDateTime::isDST() const -> bool {
    LOG_F(INFO, "QDateTime::isDST called");
    ensureValidOrThrow();

    try {
        // Use the tm struct obtained respecting the timezone context
        // (approximately)
        std::tm tm = toTm();

        // tm_isdst > 0 means DST is in effect
        // tm_isdst == 0 means DST is not in effect
        // tm_isdst < 0 means information is not available
        bool isDst = (tm.tm_isdst > 0);
        LOG_F(INFO,
              "QDateTime::isDST returning: {} (based on {} time conversion)",
              isDst ? "true" : "false",
              (timeZone_ && timeZone_->identifier() == "UTC")
                  ? "UTC (gmtime)"
                  : "local (localtime)");
        return isDst;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in isDST: {}", e.what());
        return false;  // Return false on error
    }
}

auto QDateTime::toUTC() const -> QDateTime {
    LOG_F(INFO, "QDateTime::toUTC called");
    ensureValidOrThrow();

    try {
        QDateTime dt;
        dt.dateTime_ =
            dateTime_;  // The underlying time_point is already UTC conceptually
        // Assume QTimeZone has a way to represent UTC, e.g. QTimeZone("UTC") or
        // QTimeZone::utc()
        dt.timeZone_ = QTimeZone("UTC");  // Set timezone to UTC

        LOG_F(INFO,
              "QDateTime::toUTC returning QDateTime with UTC timezone context");
        return dt;
    } catch (const std::exception& e) {
        // Catch potential exceptions from QTimeZone constructor/factory
        LOG_F(ERROR, "Exception in toUTC: {}", e.what());
        return {};  // Return invalid
    }
}

auto QDateTime::toLocalTime() const -> QDateTime {
    LOG_F(INFO, "QDateTime::toLocalTime called");
    ensureValidOrThrow();

    try {
        QDateTime dt;
        dt.dateTime_ = dateTime_;  // Keep the same underlying time_point
        // 使用默认构造函数代替 systemTimeZone() 方法
        dt.timeZone_ = QTimeZone();  // 假设默认构造函数创建本地时区

        LOG_F(INFO,
              "QDateTime::toLocalTime returning QDateTime with local timezone "
              "context");
        return dt;
    } catch (const std::exception& e) {
        // Catch potential exceptions from QTimeZone constructor/factory
        LOG_F(ERROR, "Exception in toLocalTime: {}", e.what());
        return {};  // Return invalid
    }
}

// --- Comparison Operators ---

bool QDateTime::operator==(const QDateTime& other) const {
    // Compares the underlying time points (UTC instants)
    return dateTime_ == other.dateTime_;
}

bool QDateTime::operator!=(const QDateTime& other) const {
    return !(*this == other);
}

bool QDateTime::operator<(const QDateTime& other) const {
    // Compares the underlying time points
    if (!dateTime_.has_value() || !other.dateTime_.has_value()) {
        // Define behavior for comparing invalid QDateTimes if necessary
        // For example, treat invalid as less than valid, or throw.
        // Here, we return false if either is invalid, which might not be ideal.
        return false;
    }
    return dateTime_.value() < other.dateTime_.value();
}

bool QDateTime::operator<=(const QDateTime& other) const {
    if (!dateTime_.has_value() || !other.dateTime_.has_value())
        return false;
    return dateTime_.value() <= other.dateTime_.value();
}

bool QDateTime::operator>(const QDateTime& other) const {
    if (!dateTime_.has_value() || !other.dateTime_.has_value())
        return false;
    return dateTime_.value() > other.dateTime_.value();
}

bool QDateTime::operator>=(const QDateTime& other) const {
    if (!dateTime_.has_value() || !other.dateTime_.has_value())
        return false;
    return dateTime_.value() >= other.dateTime_.value();
}

}  // namespace atom::utils
