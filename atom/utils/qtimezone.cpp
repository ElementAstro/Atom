#include "qtimezone.hpp"
#include "qdatetime.hpp"

#include <chrono>
#include <ctime>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#ifdef __AVX2__
#include <immintrin.h>
#endif

#include <spdlog/spdlog.h>

namespace atom::utils {

class TimeZoneCache {
public:
    static TimeZoneCache& instance() {
        static TimeZoneCache cache;
        return cache;
    }

    bool isDSTForDateTime(const std::string& tzId, std::time_t timestamp) {
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto tzIt = dstCache_.find(tzId);
            if (tzIt != dstCache_.end()) {
                auto timeIt = tzIt->second.find(timestamp);
                if (timeIt != tzIt->second.end()) {
                    return timeIt->second;
                }
            }
        }

        bool isDST = calculateDST(tzId, timestamp);

        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            auto& tzCache = dstCache_[tzId];
            if (tzCache.size() > MAX_CACHE_SIZE) {
                tzCache.clear();
            }
            tzCache[timestamp] = isDST;
        }

        return isDST;
    }

    bool calculateDST(const std::string& tzId, std::time_t timestamp) {
        if (tzId == "UTC")
            return false;

        std::tm localTime{};
#ifdef _WIN32
        if (localtime_s(&localTime, &timestamp) != 0) {
            return false;
        }
#else
        if (localtime_r(&timestamp, &localTime) == nullptr) {
            return false;
        }
#endif
        return localTime.tm_isdst > 0;
    }

    const std::unordered_map<std::string, std::string>& getDisplayNames() {
        std::call_once(initFlag_, [this]() { initializeDisplayNames(); });
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return displayNameCache_;
    }

private:
    static constexpr size_t MAX_CACHE_SIZE = 1000;

    TimeZoneCache() = default;

    void initializeDisplayNames() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        displayNameCache_ = {{"UTC", "Coordinated Universal Time"},
                             {"PST", "Pacific Standard Time"},
                             {"EST", "Eastern Standard Time"},
                             {"CST", "Central Standard Time"},
                             {"MST", "Mountain Standard Time"}};
    }

    std::shared_mutex mutex_;
    std::once_flag initFlag_;
    std::unordered_map<std::string, std::unordered_map<std::time_t, bool>>
        dstCache_;
    std::unordered_map<std::string, std::string> displayNameCache_;
};

QTimeZone::QTimeZone() noexcept
    : timeZoneId_("UTC"),
      displayName_("Coordinated Universal Time"),
      offset_(std::chrono::seconds(0)) {
    spdlog::debug("QTimeZone default constructor called, set to UTC");
}

void QTimeZone::initialize() {
    try {
        auto& cache = TimeZoneCache::instance();
        auto names = cache.getDisplayNames();
        auto it = names.find(timeZoneId_);
        if (it != names.end()) {
            displayName_ = it->second;
        } else {
            displayName_ = timeZoneId_;
        }

        std::tm localTime{};
        std::time_t currentTime = std::time(nullptr);

#ifdef _WIN32
        if (localtime_s(&localTime, &currentTime) != 0) {
            spdlog::error("Failed to get local time");
            THROW_GET_TIME_ERROR("Failed to get local time");
        }
#else
        if (localtime_r(&currentTime, &localTime) == nullptr) {
            spdlog::error("Failed to get local time");
            THROW_GET_TIME_ERROR("Failed to get local time");
        }
#endif

        std::tm utcTime{};
#ifdef _WIN32
        if (gmtime_s(&utcTime, &currentTime) != 0) {
            spdlog::error("Failed to get UTC time");
            THROW_GET_TIME_ERROR("Failed to get UTC time");
        }
#else
        if (gmtime_r(&currentTime, &utcTime) == nullptr) {
            spdlog::error("Failed to get UTC time");
            THROW_GET_TIME_ERROR("Failed to get UTC time");
        }
#endif

        std::time_t localTimeT = std::mktime(&localTime);
        std::time_t utcTimeT = std::mktime(&utcTime);

        if (localTimeT == -1 || utcTimeT == -1) {
            spdlog::error("Failed to convert time");
            THROW_GET_TIME_ERROR("Failed to convert time");
        }

        offset_ = std::chrono::seconds(localTimeT - utcTimeT);
        spdlog::debug("QTimeZone initialized with offset: {} seconds",
                      offset_->count());
    } catch (const std::exception& e) {
        spdlog::error("Exception during QTimeZone initialization: {}",
                      e.what());
        throw;
    }
}

auto QTimeZone::availableTimeZoneIds() noexcept -> std::vector<std::string> {
    spdlog::debug("QTimeZone::availableTimeZoneIds called");
    static const std::vector<std::string> timeZoneIds = {"UTC", "PST", "EST",
                                                         "CST", "MST"};
    return timeZoneIds;
}

auto QTimeZone::identifier() const noexcept -> std::string_view {
    return timeZoneId_;
}

auto QTimeZone::displayName() const noexcept -> std::string_view {
    return displayName_;
}

auto QTimeZone::isValid() const noexcept -> bool { return offset_.has_value(); }

auto QTimeZone::offsetFromUtc(const QDateTime& dateTime) const
    -> std::chrono::seconds {
    try {
        if (!dateTime.isValid()) {
            spdlog::warn(
                "QTimeZone::offsetFromUtc called with invalid QDateTime");
            return std::chrono::seconds(0);
        }

        std::time_t currentTime = dateTime.toTimeT();
        std::chrono::seconds baseOffset = standardTimeOffset();
        std::chrono::seconds result = baseOffset;

        if (hasDaylightTime() && TimeZoneCache::instance().isDSTForDateTime(
                                     timeZoneId_, currentTime)) {
            result += daylightTimeOffset();
            spdlog::trace("Adding DST offset: {} seconds",
                          daylightTimeOffset().count());
        }

        spdlog::trace("QTimeZone::offsetFromUtc returning: {} seconds",
                      result.count());
        return result;
    } catch (const std::exception& e) {
        spdlog::error("Exception in offsetFromUtc: {}", e.what());
        THROW_GET_TIME_ERROR("Failed to calculate time offset: " +
                             std::string(e.what()));
    }
}

auto QTimeZone::standardTimeOffset() const noexcept -> std::chrono::seconds {
    return offset_.value_or(std::chrono::seconds(0));
}

auto QTimeZone::daylightTimeOffset() const noexcept -> std::chrono::seconds {
    static constexpr auto ONE_HOUR = std::chrono::seconds(3600);

    if (timeZoneId_ == "PST" || timeZoneId_ == "EST" || timeZoneId_ == "CST" ||
        timeZoneId_ == "MST") {
        return ONE_HOUR;
    }
    return std::chrono::seconds(0);
}

auto QTimeZone::hasDaylightTime() const noexcept -> bool {
    return timeZoneId_ != "UTC";
}

auto QTimeZone::isDaylightTime(const QDateTime& dateTime) const -> bool {
    if (!dateTime.isValid()) {
        spdlog::warn("QTimeZone::isDaylightTime called with invalid QDateTime");
        return false;
    }

    if (!hasDaylightTime()) {
        return false;
    }

    std::time_t currentTime = dateTime.toTimeT();

    auto cacheIt = dstCache_.find(currentTime);
    if (cacheIt != dstCache_.end()) {
        return cacheIt->second;
    }

    try {
        std::tm localTime{};
#ifdef _WIN32
        if (localtime_s(&localTime, &currentTime) != 0) {
            spdlog::error("Failed to get local time");
            THROW_GET_TIME_ERROR("Failed to get local time");
        }
#else
        if (localtime_r(&currentTime, &localTime) == nullptr) {
            spdlog::error("Failed to get local time");
            THROW_GET_TIME_ERROR("Failed to get local time");
        }
#endif

        static constexpr int MARCH = 2;
        static constexpr int NOVEMBER = 10;

        std::tm startDST{};
        startDST.tm_year = localTime.tm_year;
        startDST.tm_mon = MARCH;
        startDST.tm_mday = 1;
        startDST.tm_hour = 2;
        startDST.tm_min = 0;
        startDST.tm_sec = 0;

        std::time_t startTime = std::mktime(&startDST);
        if (startTime == -1) {
            spdlog::error("Failed to convert time for startDST");
            THROW_GET_TIME_ERROR("Failed to convert time for startDST");
        }

        int sundayCount = 0;
        while (sundayCount < 2) {
#ifdef _WIN32
            localtime_s(&startDST, &startTime);
#else
            localtime_r(&startTime, &startDST);
#endif
            if (startDST.tm_wday == 0) {
                sundayCount++;
            }
            if (sundayCount < 2) {
                startTime += 24 * 60 * 60;
            }
        }

        std::tm endDST{};
        endDST.tm_year = localTime.tm_year;
        endDST.tm_mon = NOVEMBER;
        endDST.tm_mday = 1;
        endDST.tm_hour = 2;
        endDST.tm_min = 0;
        endDST.tm_sec = 0;

        std::time_t endTime = std::mktime(&endDST);
        if (endTime == -1) {
            spdlog::error("Failed to convert time for endDST");
            THROW_GET_TIME_ERROR("Failed to convert time for endDST");
        }

        while (true) {
#ifdef _WIN32
            localtime_s(&endDST, &endTime);
#else
            localtime_r(&endTime, &endDST);
#endif
            if (endDST.tm_wday == 0) {
                break;
            }
            endTime += 24 * 60 * 60;
        }

        bool isDST = (currentTime >= startTime && currentTime < endTime);

        if (dstCache_.size() < 1000) {
            dstCache_[currentTime] = isDST;
        }

        spdlog::trace("QTimeZone::isDaylightTime returning: {}", isDST);
        return isDST;
    } catch (const std::exception& e) {
        spdlog::error("Exception in isDaylightTime: {}", e.what());
        THROW_GET_TIME_ERROR("Failed to determine DST status: " +
                             std::string(e.what()));
    }
}

}  // namespace atom::utils
