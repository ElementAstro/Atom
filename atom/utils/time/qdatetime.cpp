#include "qdatetime.hpp"
#include "qtimezone.hpp"

#include <ctime>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

#ifdef __AVX2__
#include <immintrin.h>
#endif

#include <spdlog/spdlog.h>

namespace atom::utils {

struct PairHash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        return std::hash<T1>{}(p.first) ^ (std::hash<T2>{}(p.second) << 1);
    }
};

class DateTimeCache {
public:
    static DateTimeCache& instance() {
        static DateTimeCache cache;
        return cache;
    }

    std::time_t getCachedTimeOffset(const QTimeZone& tz, const QDateTime& dt) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::pair<std::string, std::time_t> key(tz.identifier(),
                                                      dt.toTimeT());

        auto it = timeOffsetCache_.find(key);
        if (it != timeOffsetCache_.end()) {
            return it->second;
        }

        const auto offset = tz.offsetFromUtc(dt);
        timeOffsetCache_.emplace(key, offset.count());
        return offset.count();
    }

private:
    DateTimeCache() = default;
    std::mutex mutex_;
    std::unordered_map<std::pair<std::string, std::time_t>, std::time_t,
                       PairHash>
        timeOffsetCache_;
};

QDateTime::QDateTime() : dateTime_(std::nullopt), timeZone_(std::nullopt) {
    spdlog::debug("QDateTime default constructor called");
}

QDateTime::QDateTime(int year, int month, int day, int hour, int minute,
                     int second, int ms)
    : timeZone_(std::nullopt) {
    spdlog::debug("QDateTime constructor with components called");

    try {
        validateDate(year, month, day);
        validateTime(hour, minute, second, ms);

        std::tm t = {};
        t.tm_year = year - 1900;
        t.tm_mon = month - 1;
        t.tm_mday = day;
        t.tm_hour = hour;
        t.tm_min = minute;
        t.tm_sec = second;
        t.tm_isdst = -1;

        auto timeT = std::mktime(&t);
        if (timeT == -1) {
            throw std::invalid_argument(
                "Invalid date/time components or mktime failure");
        }

        dateTime_ = Clock::from_time_t(timeT) + std::chrono::milliseconds(ms);

        spdlog::debug(
            "QDateTime created with components: {}-{:02d}-{:02d} "
            "{:02d}:{:02d}:{:02d}.{:03d} (local)",
            year, month, day, hour, minute, second, ms);
    } catch (const std::exception& e) {
        spdlog::error("Exception in QDateTime component constructor: {}",
                      e.what());
        dateTime_ = std::nullopt;
        throw;
    }
}

void QDateTime::validateDate(int year, int month, int day) {
    if (year < 1900 || year > 2099) {
        throw std::out_of_range("Year out of range (1900-2099)");
    }
    if (month < 1 || month > 12) {
        throw std::out_of_range("Month out of range (1-12)");
    }

    int daysInMonth;
    if (month == 2) {
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
    if (second < 0 || second > 59) {
        throw std::out_of_range("Second out of range (0-59)");
    }
    if (ms < 0 || ms > 999) {
        throw std::out_of_range("Millisecond out of range (0-999)");
    }
}

auto QDateTime::currentDateTime() -> QDateTime {
    spdlog::debug("QDateTime::currentDateTime called");
    QDateTime dt;
    dt.dateTime_ = Clock::now();
    spdlog::debug("QDateTime::currentDateTime returning current local time");
    return dt;
}

auto QDateTime::currentDateTime(const QTimeZone& timeZone) -> QDateTime {
    spdlog::debug("QDateTime::currentDateTime called with timeZone {}",
                  timeZone.identifier());
    try {
        auto nowUtc = Clock::now();

        QDateTime utcDt;
        utcDt.dateTime_ = nowUtc;
        utcDt.timeZone_ = QTimeZone("UTC");

        auto offset = timeZone.offsetFromUtc(utcDt);

        QDateTime dt;
        dt.dateTime_ = nowUtc + offset;
        dt.timeZone_ = timeZone;

        spdlog::debug(
            "QDateTime::currentDateTime returning current time in specified "
            "timezone");
        return dt;
    } catch (const std::exception& e) {
        spdlog::error("Exception in currentDateTime with timezone: {}",
                      e.what());
        return currentDateTime();
    }
}

auto QDateTime::toTimeT() const -> std::time_t {
    spdlog::trace("QDateTime::toTimeT called");
    if (!dateTime_) {
        spdlog::warn("QDateTime::toTimeT called on invalid QDateTime");
        return 0;
    }
    std::time_t result = Clock::to_time_t(dateTime_.value());
    spdlog::trace("QDateTime::toTimeT returning UTC epoch seconds: {}",
                  static_cast<long long>(result));
    return result;
}

auto QDateTime::isValid() const -> bool { return dateTime_.has_value(); }

void QDateTime::ensureValidOrThrow() const {
    if (!isValid()) {
        spdlog::error("Operation called on invalid QDateTime");
        throw std::logic_error("Operation called on invalid QDateTime");
    }
}

auto QDateTime::addDays(int days) const -> QDateTime {
    spdlog::debug("QDateTime::addDays called with days: {}", days);
    ensureValidOrThrow();

    try {
        QDateTime dt = *this;
        dt.dateTime_ = dateTime_.value() + std::chrono::hours(days * 24);
        spdlog::debug("QDateTime::addDays returning new QDateTime");
        return dt;
    } catch (const std::exception& e) {
        spdlog::error("Exception in addDays: {}", e.what());
        return {};
    }
}

auto QDateTime::addSecs(int seconds) const -> QDateTime {
    spdlog::debug("QDateTime::addSecs called with seconds: {}", seconds);
    ensureValidOrThrow();

    try {
        QDateTime dt = *this;
        dt.dateTime_ = dateTime_.value() + std::chrono::seconds(seconds);
        spdlog::debug("QDateTime::addSecs returning new QDateTime");
        return dt;
    } catch (const std::exception& e) {
        spdlog::error("Exception in addSecs: {}", e.what());
        return {};
    }
}

auto QDateTime::daysTo(const QDateTime& other) const -> int {
    spdlog::debug("QDateTime::daysTo called");
    ensureValidOrThrow();
    other.ensureValidOrThrow();

    try {
        auto duration = other.dateTime_.value() - dateTime_.value();
        auto days =
            std::chrono::duration_cast<std::chrono::hours>(duration).count() /
            24;
        int result = static_cast<int>(days);
        spdlog::debug("QDateTime::daysTo returning: {}", result);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Exception in daysTo: {}", e.what());
        return 0;
    }
}

auto QDateTime::secsTo(const QDateTime& other) const -> int {
    spdlog::debug("QDateTime::secsTo called");
    ensureValidOrThrow();
    other.ensureValidOrThrow();

    try {
        auto duration = other.dateTime_.value() - dateTime_.value();
        auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        int result = static_cast<int>(seconds);
        spdlog::debug("QDateTime::secsTo returning: {}", result);
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Exception in secsTo: {}", e.what());
        return 0;
    }
}

auto QDateTime::addMSecs(int msecs) const -> QDateTime {
    spdlog::debug("QDateTime::addMSecs called with msecs: {}", msecs);
    ensureValidOrThrow();

    try {
        QDateTime dt = *this;
        dt.dateTime_ = dateTime_.value() + std::chrono::milliseconds(msecs);
        spdlog::debug("QDateTime::addMSecs returning new QDateTime");
        return dt;
    } catch (const std::exception& e) {
        spdlog::error("Exception in addMSecs: {}", e.what());
        return {};
    }
}

auto QDateTime::addMonths(int months) const -> QDateTime {
    spdlog::debug("QDateTime::addMonths called with months: {}", months);
    ensureValidOrThrow();

    try {
        std::time_t currentTimeT = Clock::to_time_t(dateTime_.value());
        std::tm tm = *std::localtime(&currentTimeT);

        auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                dateTime_.value().time_since_epoch() % std::chrono::seconds(1));

        int currentMonth = tm.tm_mon;
        int currentYear = tm.tm_year;

        currentMonth += months;
        currentYear += currentMonth / 12;
        currentMonth %= 12;
        if (currentMonth < 0) {
            currentMonth += 12;
            currentYear -= 1;
        }

        tm.tm_mon = currentMonth;
        tm.tm_year = currentYear;

        int daysInNewMonth;
        int year_ad = tm.tm_year + 1900;
        int month_ad = tm.tm_mon + 1;
        if (month_ad == 2) {
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
            tm.tm_mday = daysInNewMonth;
        }

        tm.tm_isdst = -1;
        std::time_t newTimeT = std::mktime(&tm);
        if (newTimeT == -1) {
            spdlog::error(
                "Failed to calculate adjusted date via mktime when adding "
                "months");
            throw std::runtime_error(
                "Failed to calculate adjusted date when adding months");
        }

        QDateTime dt = *this;
        dt.dateTime_ = Clock::from_time_t(newTimeT) + milliseconds;

        spdlog::debug("QDateTime::addMonths returning new QDateTime");
        return dt;
    } catch (const std::exception& e) {
        spdlog::error("Exception in addMonths: {}", e.what());
        return {};
    }
}

auto QDateTime::addYears(int years) const -> QDateTime {
    spdlog::debug("QDateTime::addYears called with years: {}", years);
    return addMonths(years * 12);
}

std::tm QDateTime::toTm() const {
    ensureValidOrThrow();
    std::time_t timeT = Clock::to_time_t(dateTime_.value());

    if (timeZone_ && timeZone_->identifier() == "UTC") {
#ifdef _WIN32
        std::tm result;
        gmtime_s(&result, &timeT);
        return result;
#else
        return *std::gmtime(&timeT);
#endif
    } else {
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
    spdlog::debug("QDateTime::getDate called");
    ensureValidOrThrow();

    try {
        std::tm tm = toTm();

        Date date;
        date.year = tm.tm_year + 1900;
        date.month = tm.tm_mon + 1;
        date.day = tm.tm_mday;

        spdlog::debug("QDateTime::getDate returning date: {}-{:02d}-{:02d}",
                      date.year, date.month, date.day);
        return date;
    } catch (const std::exception& e) {
        spdlog::error("Exception in getDate: {}", e.what());
        throw;
    }
}

auto QDateTime::getTime() const -> Time {
    spdlog::debug("QDateTime::getTime called");
    ensureValidOrThrow();

    try {
        std::tm tm = toTm();

        auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                dateTime_.value().time_since_epoch() % std::chrono::seconds(1))
                .count();

        Time time;
        time.hour = tm.tm_hour;
        time.minute = tm.tm_min;
        time.second = tm.tm_sec;
        time.millisecond = static_cast<int>(milliseconds);

        spdlog::debug(
            "QDateTime::getTime returning time: {:02d}:{:02d}:{:02d}.{:03d}",
            time.hour, time.minute, time.second, time.millisecond);
        return time;
    } catch (const std::exception& e) {
        spdlog::error("Exception in getTime: {}", e.what());
        throw;
    }
}

auto QDateTime::setDate(int year, int month, int day) const -> QDateTime {
    spdlog::debug("QDateTime::setDate called with year={}, month={}, day={}",
                  year, month, day);
    ensureValidOrThrow();

    try {
        validateDate(year, month, day);

        std::tm t = toTm();
        t.tm_year = year - 1900;
        t.tm_mon = month - 1;
        t.tm_mday = day;
        t.tm_isdst = -1;

        std::time_t newTimeT = std::mktime(&t);
        if (newTimeT == -1) {
            spdlog::error("mktime failed in setDate");
            throw std::runtime_error(
                "Failed to construct date/time in setDate");
        }

        auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                dateTime_.value().time_since_epoch() % std::chrono::seconds(1));

        QDateTime dt = *this;
        dt.dateTime_ = Clock::from_time_t(newTimeT) + milliseconds;

        spdlog::debug("QDateTime::setDate returning new QDateTime");
        return dt;
    } catch (const std::exception& e) {
        spdlog::error("Exception in setDate: {}", e.what());
        return {};
    }
}

auto QDateTime::setTime(int hour, int minute, int second, int ms) const
    -> QDateTime {
    spdlog::debug(
        "QDateTime::setTime called with hour={}, minute={}, second={}, ms={}",
        hour, minute, second, ms);
    ensureValidOrThrow();

    try {
        validateTime(hour, minute, second, ms);

        std::tm t = toTm();
        t.tm_hour = hour;
        t.tm_min = minute;
        t.tm_sec = second;
        t.tm_isdst = -1;

        std::time_t newTimeT = std::mktime(&t);
        if (newTimeT == -1) {
            spdlog::error("mktime failed in setTime");
            throw std::runtime_error(
                "Failed to construct date/time in setTime");
        }

        QDateTime dt = *this;
        dt.dateTime_ =
            Clock::from_time_t(newTimeT) + std::chrono::milliseconds(ms);

        spdlog::debug("QDateTime::setTime returning new QDateTime");
        return dt;
    } catch (const std::exception& e) {
        spdlog::error("Exception in setTime: {}", e.what());
        return {};
    }
}

auto QDateTime::setTimeZone(const QTimeZone& timeZone) const -> QDateTime {
    spdlog::debug("QDateTime::setTimeZone called with zone {}",
                  timeZone.identifier());
    ensureValidOrThrow();

    QDateTime dt = *this;
    dt.timeZone_ = timeZone;

    spdlog::debug(
        "QDateTime::setTimeZone returning QDateTime with same time instant, "
        "new timezone context");
    return dt;
}

auto QDateTime::timeZone() const -> std::optional<QTimeZone> {
    return timeZone_;
}

auto QDateTime::isDST() const -> bool {
    spdlog::debug("QDateTime::isDST called");
    ensureValidOrThrow();

    try {
        std::tm tm = toTm();

        bool isDst = (tm.tm_isdst > 0);
        spdlog::debug(
            "QDateTime::isDST returning: {} (based on {} time conversion)",
            isDst ? "true" : "false",
            (timeZone_ && timeZone_->identifier() == "UTC")
                ? "UTC (gmtime)"
                : "local (localtime)");
        return isDst;
    } catch (const std::exception& e) {
        spdlog::error("Exception in isDST: {}", e.what());
        return false;
    }
}

auto QDateTime::toUTC() const -> QDateTime {
    spdlog::debug("QDateTime::toUTC called");
    ensureValidOrThrow();

    try {
        QDateTime dt;
        dt.dateTime_ = dateTime_;
        dt.timeZone_ = QTimeZone("UTC");

        spdlog::debug(
            "QDateTime::toUTC returning QDateTime with UTC timezone context");
        return dt;
    } catch (const std::exception& e) {
        spdlog::error("Exception in toUTC: {}", e.what());
        return {};
    }
}

auto QDateTime::toLocalTime() const -> QDateTime {
    spdlog::debug("QDateTime::toLocalTime called");
    ensureValidOrThrow();

    try {
        QDateTime dt;
        dt.dateTime_ = dateTime_;
        dt.timeZone_ = QTimeZone();

        spdlog::debug(
            "QDateTime::toLocalTime returning QDateTime with local timezone "
            "context");
        return dt;
    } catch (const std::exception& e) {
        spdlog::error("Exception in toLocalTime: {}", e.what());
        return {};
    }
}

bool QDateTime::operator==(const QDateTime& other) const {
    return dateTime_ == other.dateTime_;
}

bool QDateTime::operator!=(const QDateTime& other) const {
    return !(*this == other);
}

bool QDateTime::operator<(const QDateTime& other) const {
    if (!dateTime_.has_value() || !other.dateTime_.has_value()) {
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
