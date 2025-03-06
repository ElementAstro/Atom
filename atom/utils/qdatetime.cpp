#include "qdatetime.hpp"
#include "qtimezone.hpp"

#include <mutex>
#include <unordered_map>

#ifdef __AVX2__
#include <immintrin.h>
#endif

#include "atom/log/loguru.hpp"

namespace atom::utils {

// 保留自定义的 PairHash 函数对象
struct PairHash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
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
        const auto key = std::make_pair(tz.identifier(), dt.toTimeT());
        // 使用我们自定义的哈希函数查找 key
        // auto it = timeOffsetCache_.find(key);
        // if (it != timeOffsetCache_.end()) {
        //    return it->second;
        //}
        const auto offset = tz.offsetFromUtc(dt).count();
        timeOffsetCache_.emplace(key, offset);
        return offset;
    }

private:
    DateTimeCache() = default;
    std::mutex mutex_;
    // 将 ::std:: 修改为 std::，确保使用 PairHash 作为哈希函数
    std::unordered_map<std::pair<std::string, std::time_t>, std::time_t,
                       PairHash>
        timeOffsetCache_;
};

QDateTime::QDateTime() : dateTime_(std::nullopt) {
    LOG_F(INFO, "QDateTime default constructor called");
}

auto QDateTime::currentDateTime() -> QDateTime {
    LOG_F(INFO, "QDateTime::currentDateTime called");
    QDateTime dt;
    dt.dateTime_ = Clock::now();
    LOG_F(INFO, "QDateTime::currentDateTime returning current time");
    return dt;
}

auto QDateTime::currentDateTime(const QTimeZone& timeZone) -> QDateTime {
    LOG_F(INFO, "QDateTime::currentDateTime called with timeZone");
    try {
        QDateTime dt;
        dt.dateTime_ = Clock::now();
        auto offset =
            DateTimeCache::instance().getCachedTimeOffset(timeZone, dt);
        dt.dateTime_ = dt.dateTime_.value() + std::chrono::seconds(offset);
        LOG_F(
            INFO,
            "QDateTime::currentDateTime returning current time with timezone");
        return dt;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in currentDateTime with timezone: {}",
              e.what());
        return currentDateTime();
    }
}

auto QDateTime::toTimeT() const -> std::time_t {
    LOG_F(INFO, "QDateTime::toTimeT called");
    if (!dateTime_) {
        LOG_F(WARNING, "QDateTime::toTimeT called on invalid QDateTime");
        return 0;
    }
    std::time_t result = Clock::to_time_t(dateTime_.value());
    LOG_F(INFO, "QDateTime::toTimeT returning: {}", result);
    return result;
}

auto QDateTime::isValid() const -> bool {
    LOG_F(INFO, "QDateTime::isValid called");
    bool result = dateTime_.has_value();
    LOG_F(INFO, "QDateTime::isValid returning: {}", result ? "true" : "false");
    return result;
}

auto QDateTime::ensureValid() const -> bool {
    if (!isValid()) {
        throw std::logic_error("Operation called on invalid QDateTime");
    }
    return true;
}

auto QDateTime::addDays(int days) const -> QDateTime {
    LOG_F(INFO, "QDateTime::addDays called with days: {}", days);
    try {
        if (!dateTime_) {
            LOG_F(WARNING, "QDateTime::addDays called on invalid QDateTime");
            return {};
        }
        QDateTime dt;
        dt.dateTime_ = dateTime_.value() + std::chrono::hours(days * 24);
        LOG_F(INFO, "QDateTime::addDays returning new QDateTime");
        return dt;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in addDays: {}", e.what());
        return {};
    }
}

auto QDateTime::addSecs(int seconds) const -> QDateTime {
    LOG_F(INFO, "QDateTime::addSecs called with seconds: {}", seconds);
    try {
        if (!dateTime_) {
            LOG_F(WARNING, "QDateTime::addSecs called on invalid QDateTime");
            return {};
        }
        QDateTime dt;
        dt.dateTime_ = dateTime_.value() + std::chrono::seconds(seconds);
        LOG_F(INFO, "QDateTime::addSecs returning new QDateTime");
        return dt;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in addSecs: {}", e.what());
        return {};
    }
}

auto QDateTime::daysTo(const QDateTime& other) const -> int {
    LOG_F(INFO, "QDateTime::daysTo called");
    try {
        if (!dateTime_ || !other.dateTime_) {
            LOG_F(WARNING, "QDateTime::daysTo called on invalid QDateTime");
            return 0;
        }
        int result = std::chrono::duration_cast<std::chrono::hours>(
                         other.dateTime_.value() - dateTime_.value())
                         .count() /
                     24;
        LOG_F(INFO, "QDateTime::daysTo returning: {}", result);
        return result;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in daysTo: {}", e.what());
        return 0;
    }
}

auto QDateTime::secsTo(const QDateTime& other) const -> int {
    LOG_F(INFO, "QDateTime::secsTo called");
    try {
        if (!dateTime_ || !other.dateTime_) {
            LOG_F(WARNING, "QDateTime::secsTo called on invalid QDateTime");
            return 0;
        }

        int result = std::chrono::duration_cast<std::chrono::seconds>(
                         other.dateTime_.value() - dateTime_.value())
                         .count();
        LOG_F(INFO, "QDateTime::secsTo returning: {}", result);
        return result;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in secsTo: {}", e.what());
        return 0;
    }
}

}  // namespace atom::utils
