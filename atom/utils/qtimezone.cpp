#include "qtimezone.hpp"
#include "qdatetime.hpp"

#include <chrono>
#include <ctime>
#include <string>
#include <vector>
#include <mutex>
#include <shared_mutex>

#ifdef __AVX2__
#include <immintrin.h>
#endif

#include "atom/log/loguru.hpp"

namespace atom::utils {

// Thread-safe time zone data cache
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
        
        // Calculate DST status (implementation depends on TimeZone::isDaylightTime logic)
        bool isDST = calculateDST(tzId, timestamp);
        
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            dstCache_[tzId][timestamp] = isDST;
        }
        
        return isDST;
    }
    
    // Helper to calculate DST status for a timezone + timestamp
    bool calculateDST(const std::string& tzId, std::time_t timestamp) {
        // This is a simplified implementation - full implementation would depend 
        // on the actual DST rules for each timezone
        if (tzId == "UTC") return false;
        
        std::tm localTime{};
#ifdef _WIN32
        localtime_s(&localTime, &timestamp);
#else
        localtime_r(&timestamp, &localTime);
#endif
        return localTime.tm_isdst > 0;
    }
    
    const std::unordered_map<std::string, std::string>& getDisplayNames() {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return displayNameCache_;
    }
    
    void initializeDisplayNames() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (displayNameCache_.empty()) {
            displayNameCache_ = {
                {"UTC", "Coordinated Universal Time"},
                {"PST", "Pacific Standard Time"},
                {"EST", "Eastern Standard Time"},
                {"CST", "Central Standard Time"},
                {"MST", "Mountain Standard Time"}
            };
        }
    }

private:
    TimeZoneCache() {
        initializeDisplayNames();
    }
    
    std::shared_mutex mutex_;
    std::unordered_map<std::string, std::unordered_map<std::time_t, bool>> dstCache_;
    std::unordered_map<std::string, std::string> displayNameCache_;
};

QTimeZone::QTimeZone() noexcept 
    : timeZoneId_("UTC"), 
      displayName_("Coordinated Universal Time"),
      offset_(std::chrono::seconds(0)) {
    LOG_F(INFO, "QTimeZone default constructor called, set to UTC");
}

void QTimeZone::initialize() {
    try {
        auto& cache = TimeZoneCache::instance();
        auto names = cache.getDisplayNames();
        auto it = names.find(timeZoneId_);
        if (it != names.end()) {
            displayName_ = it->second;
        } else {
            displayName_ = timeZoneId_; // Fallback to using ID as display name
        }

        std::tm localTime{};
        std::time_t currentTime = std::time(nullptr);
#ifdef _WIN32
        if (localtime_s(&localTime, &currentTime) != 0) {
            LOG_F(ERROR, "Failed to get local time");
            THROW_GET_TIME_ERROR("Failed to get local time");
        }
#else
        if (localtime_r(&currentTime, &localTime) == nullptr) {
            LOG_F(ERROR, "Failed to get local time");
            THROW_GET_TIME_ERROR("Failed to get local time");
        }
#endif

        std::tm utcTime{};
#ifdef _WIN32
        if (gmtime_s(&utcTime, &currentTime) != 0) {
            LOG_F(ERROR, "Failed to get UTC time");
            THROW_GET_TIME_ERROR("Failed to get UTC time");
        }
#else
        if (gmtime_r(&currentTime, &utcTime) == nullptr) {
            LOG_F(ERROR, "Failed to get UTC time");
            THROW_GET_TIME_ERROR("Failed to get UTC time");
        }
#endif

        std::time_t localTimeT = std::mktime(&localTime);
        std::time_t utcTimeT = std::mktime(&utcTime);
        
        if (localTimeT == -1 || utcTimeT == -1) {
            LOG_F(ERROR, "Failed to convert time");
            THROW_GET_TIME_ERROR("Failed to convert time");
        }
        
        offset_ = std::chrono::seconds(localTimeT - utcTimeT);
        LOG_F(INFO, "QTimeZone initialized with offset: {}", offset_->count());
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception during QTimeZone initialization: {}", e.what());
        throw;
    }
}

auto QTimeZone::availableTimeZoneIds() noexcept -> std::vector<std::string> {
    LOG_F(INFO, "QTimeZone::availableTimeZoneIds called");
    static const std::vector<std::string> timeZoneIds = {"UTC", "PST", "EST", "CST", "MST"};
    return timeZoneIds;
}

auto QTimeZone::identifier() const noexcept -> std::string_view {
    LOG_F(INFO, "QTimeZone::identifier called, returning: {}", timeZoneId_);
    return timeZoneId_;
}

auto QTimeZone::displayName() const noexcept -> std::string_view {
    LOG_F(INFO, "QTimeZone::displayName called for timeZoneId: {}", timeZoneId_);
    return displayName_;
}

auto QTimeZone::isValid() const noexcept -> bool {
    LOG_F(INFO, "QTimeZone::isValid called, returning: {}", offset_.has_value());
    return offset_.has_value();
}

auto QTimeZone::offsetFromUtc(const QDateTime& dateTime) const -> std::chrono::seconds {
    LOG_F(INFO, "QTimeZone::offsetFromUtc called");
    
    try {
        if (!dateTime.isValid()) {
            LOG_F(WARNING, "QTimeZone::offsetFromUtc called with invalid QDateTime");
            return std::chrono::seconds(0);
        }
        
        std::time_t currentTime = dateTime.toTimeT();
        std::tm utcTime{};
        
#ifdef _WIN32
        if (gmtime_s(&utcTime, &currentTime) != 0) {
            LOG_F(ERROR, "Failed to get UTC time");
            THROW_GET_TIME_ERROR("Failed to get UTC time");
        }
#else
        if (gmtime_r(&currentTime, &utcTime) == nullptr) {
            LOG_F(ERROR, "Failed to get UTC time");
            THROW_GET_TIME_ERROR("Failed to get UTC time");
        }
#endif

        std::chrono::seconds baseOffset = standardTimeOffset();
        std::chrono::seconds result = baseOffset;
        
        if (hasDaylightTime() && 
            TimeZoneCache::instance().isDSTForDateTime(timeZoneId_, currentTime)) {
            result += daylightTimeOffset();
            LOG_F(INFO, "Adding DST offset: {}", daylightTimeOffset().count());
        }
        
        LOG_F(INFO, "QTimeZone::offsetFromUtc returning: {}", result.count());
        return result;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in offsetFromUtc: {}", e.what());
        THROW_GET_TIME_ERROR("Failed to calculate time offset: " + std::string(e.what()));
    }
}

auto QTimeZone::standardTimeOffset() const noexcept -> std::chrono::seconds {
    LOG_F(INFO, "QTimeZone::standardTimeOffset called, returning: {}",
          offset_.value_or(std::chrono::seconds(0)).count());
    return offset_.value_or(std::chrono::seconds(0));
}

auto QTimeZone::daylightTimeOffset() const noexcept -> std::chrono::seconds {
    LOG_F(INFO, "QTimeZone::daylightTimeOffset called for timeZoneId: {}",
          timeZoneId_);
    static constexpr int K_ONE_HOUR_IN_SECONDS = 3600;
    if (timeZoneId_ == "PST" || timeZoneId_ == "EST" || timeZoneId_ == "CST" ||
        timeZoneId_ == "MST") {
        return std::chrono::seconds(K_ONE_HOUR_IN_SECONDS);
    }
    return std::chrono::seconds(0);
}

auto QTimeZone::hasDaylightTime() const noexcept -> bool {
    LOG_F(INFO, "QTimeZone::hasDaylightTime called for timeZoneId: {}",
          timeZoneId_);
    return timeZoneId_ != "UTC";
}

auto QTimeZone::isDaylightTime(const QDateTime& dateTime) const -> bool {
    LOG_F(INFO, "QTimeZone::isDaylightTime called");
    
    if (!dateTime.isValid()) {
        LOG_F(WARNING, "QTimeZone::isDaylightTime called with invalid QDateTime");
        return false;
    }
    
    if (!hasDaylightTime()) {
        LOG_F(INFO, "QTimeZone::isDaylightTime returning false (no daylight saving time)");
        return false;
    }

    std::time_t currentTime = dateTime.toTimeT();
    
    // Check the cache first
    auto cacheIt = dstCache_.find(currentTime);
    if (cacheIt != dstCache_.end()) {
        return cacheIt->second;
    }
    
    try {
        // Determine DST rules for Northern Hemisphere
        std::tm localTime{};
#ifdef _WIN32
        if (localtime_s(&localTime, &currentTime) != 0) {
            LOG_F(ERROR, "Failed to get local time");
            THROW_GET_TIME_ERROR("Failed to get local time");
        }
#else
        if (localtime_r(&currentTime, &localTime) == nullptr) {
            LOG_F(ERROR, "Failed to get local time");
            THROW_GET_TIME_ERROR("Failed to get local time");
        }
#endif

        static constexpr int K_MARCH = 2;     // 0-based month (March)
        static constexpr int K_NOVEMBER = 10; // 0-based month (November)
        
        // Create current year's DST start date (2nd Sunday in March at 2AM)
        std::tm startDST{};
        startDST.tm_year = localTime.tm_year;
        startDST.tm_mon = K_MARCH;
        startDST.tm_mday = 1; // Start with the 1st
        startDST.tm_hour = 2;
        startDST.tm_min = 0;
        startDST.tm_sec = 0;
        
        // Find second Sunday in March
        std::time_t startTime = std::mktime(&startDST);
        if (startTime == -1) {
            LOG_F(ERROR, "Failed to convert time for startDST");
            THROW_GET_TIME_ERROR("Failed to convert time for startDST");
        }
        
        int sundayCount = 0;
        while (sundayCount < 2) {
            // Re-get the tm structure
#ifdef _WIN32
            localtime_s(&startDST, &startTime);
#else
            localtime_r(&startTime, &startDST);
#endif
            if (startDST.tm_wday == 0) { // Sunday
                sundayCount++;
            }
            if (sundayCount < 2) {
                startTime += 24 * 60 * 60; // Add one day
            }
        }
        
        // Create current year's DST end date (1st Sunday in November at 2AM)
        std::tm endDST{};
        endDST.tm_year = localTime.tm_year;
        endDST.tm_mon = K_NOVEMBER;
        endDST.tm_mday = 1; // Start with the 1st
        endDST.tm_hour = 2;
        endDST.tm_min = 0;
        endDST.tm_sec = 0;
        
        // Find first Sunday in November
        std::time_t endTime = std::mktime(&endDST);
        if (endTime == -1) {
            LOG_F(ERROR, "Failed to convert time for endDST");
            THROW_GET_TIME_ERROR("Failed to convert time for endDST");
        }
        
        while (true) {
            // Re-get the tm structure
#ifdef _WIN32
            localtime_s(&endDST, &endTime);
#else
            localtime_r(&endTime, &endDST);
#endif
            if (endDST.tm_wday == 0) { // Sunday
                break;
            }
            endTime += 24 * 60 * 60; // Add one day
        }
        
        bool isDST = (currentTime >= startTime && currentTime < endTime);
        
        // Cache the result
        dstCache_[currentTime] = isDST;
        
        LOG_F(INFO, "QTimeZone::isDaylightTime returning: {}", isDST);
        return isDST;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in isDaylightTime: {}", e.what());
        THROW_GET_TIME_ERROR("Failed to determine DST status: " + std::string(e.what()));
    }
}

}  // namespace atom::utils
