#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <spdlog/spdlog.h>

#include "atom/web/time.hpp"

using namespace atom::web;
using namespace std::chrono_literals;

class MockTimeManagerImpl : public TimeManagerImpl {
public:
    std::time_t getSystemTime() override { return mock_time_; }

    std::chrono::system_clock::time_point getSystemTimePoint() override {
        return std::chrono::system_clock::from_time_t(mock_time_);
    }

    std::error_code setSystemTime(int year, int month, int day, int hour,
                                  int minute, int second) override {
        if (!validateTime(year, month, day, hour, minute, second)) {
            return std::error_code(
                static_cast<int>(TimeError::InvalidParameter),
                std::system_category());
        }
        last_set_time_ = {year, month, day, hour, minute, second};
        return std::error_code(static_cast<int>(TimeError::None),
                               std::system_category());
    }

    std::error_code setSystemTimezone(std::string_view timezone) override {
        if (timezone.empty() || timezone.length() > 64) {
            return std::error_code(
                static_cast<int>(TimeError::InvalidParameter),
                std::system_category());
        }
        last_timezone_ = std::string(timezone);
        return std::error_code(static_cast<int>(TimeError::None),
                               std::system_category());
    }

    std::error_code syncTimeFromRTC() override {
        rtc_sync_called_ = true;
        return std::error_code(static_cast<int>(TimeError::None),
                               std::system_category());
    }

    std::optional<std::time_t> getNtpTime(std::string_view hostname,
                                          std::chrono::milliseconds timeout) override {
        if (hostname.empty()) {
            return std::nullopt;
        }
        last_ntp_host_ = std::string(hostname);
        last_ntp_timeout_ = timeout;
        if (mock_ntp_failure_) {
            return std::nullopt;
        }
        return mock_ntp_time_;
    }

    void setMockTime(std::time_t time) { mock_time_ = time; }
    void setMockNtpTime(std::time_t time) { mock_ntp_time_ = time; }
    void setMockNtpFailure(bool fail) { mock_ntp_failure_ = fail; }

    struct TimeParams {
        int year, month, day, hour, minute, second;
    };

    std::optional<TimeParams> getLastSetTime() const { return last_set_time_; }
    std::optional<std::string> getLastTimezone() const { return last_timezone_; }
    std::optional<std::string> getLastNtpHost() const { return last_ntp_host_; }
    std::optional<std::chrono::milliseconds> getLastNtpTimeout() const { return last_ntp_timeout_; }
    bool wasRtcSyncCalled() const { return rtc_sync_called_; }

private:
    std::time_t mock_time_ = 1609459200;
    std::time_t mock_ntp_time_ = 1609459200;
    bool mock_ntp_failure_ = false;
    bool rtc_sync_called_ = false;

    std::optional<TimeParams> last_set_time_;
    std::optional<std::string> last_timezone_;
    std::optional<std::string> last_ntp_host_;
    std::optional<std::chrono::milliseconds> last_ntp_timeout_;

    bool validateTime(int year, int month, int day, int hour, int minute,
                      int second) {
        if (year < 1970 || year > 2038) return false;
        if (month < 1 || month > 12) return false;
        if (day < 1 || day > 31) return false;
        if (hour < 0 || hour > 23) return false;
        if (minute < 0 || minute > 59) return false;
        if (second < 0 || second > 59) return false;
        int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
            daysInMonth[2] = 29;
        return day <= daysInMonth[month];
    }
};

class TimeManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
        auto mockImpl = std::make_unique<MockTimeManagerImpl>();
        mockImpl_ = mockImpl.get();
        timeManager_.setImpl(std::move(mockImpl));
    }

    TimeManager timeManager_;
    MockTimeManagerImpl* mockImpl_;
};

TEST_F(TimeManagerTest, GetSystemTime) {
    std::time_t expectedTime = 1620000000;
    mockImpl_->setMockTime(expectedTime);
    std::time_t actualTime = timeManager_.getSystemTime();
    EXPECT_EQ(actualTime, expectedTime);
}

TEST_F(TimeManagerTest, GetSystemTimePoint) {
    std::time_t expectedTime = 1620000000;
    mockImpl_->setMockTime(expectedTime);
    auto expectedTimePoint = std::chrono::system_clock::from_time_t(expectedTime);
    auto actualTimePoint = timeManager_.getSystemTimePoint();
    EXPECT_EQ(std::chrono::system_clock::to_time_t(actualTimePoint),
              std::chrono::system_clock::to_time_t(expectedTimePoint));
}

TEST_F(TimeManagerTest, SetSystemTime_Valid) {
    auto result = timeManager_.setSystemTime(2022, 3, 15, 14, 30, 45);
    EXPECT_FALSE(result);
    auto lastSetTime = mockImpl_->getLastSetTime();
    ASSERT_TRUE(lastSetTime.has_value());
    EXPECT_EQ(lastSetTime->year, 2022);
    EXPECT_EQ(lastSetTime->month, 3);
    EXPECT_EQ(lastSetTime->day, 15);
    EXPECT_EQ(lastSetTime->hour, 14);
    EXPECT_EQ(lastSetTime->minute, 30);
    EXPECT_EQ(lastSetTime->second, 45);
}

TEST_F(TimeManagerTest, SetSystemTime_Invalid) {
    auto result1 = timeManager_.setSystemTime(1969, 1, 1, 0, 0, 0);
    EXPECT_TRUE(result1);
    EXPECT_EQ(static_cast<int>(result1.value()),
              static_cast<int>(TimeError::InvalidParameter));
    auto result2 = timeManager_.setSystemTime(2022, 13, 1, 0, 0, 0);
    EXPECT_TRUE(result2);
    EXPECT_EQ(static_cast<int>(result2.value()),
              static_cast<int>(TimeError::InvalidParameter));
    auto result3 = timeManager_.setSystemTime(2022, 2, 30, 0, 0, 0);
    EXPECT_TRUE(result3);
    EXPECT_EQ(static_cast<int>(result3.value()),
              static_cast<int>(TimeError::InvalidParameter));
    auto result4 = timeManager_.setSystemTime(2022, 1, 1, 24, 0, 0);
    EXPECT_TRUE(result4);
    EXPECT_EQ(static_cast<int>(result4.value()),
              static_cast<int>(TimeError::InvalidParameter));
}

TEST_F(TimeManagerTest, SetSystemTimezone_Valid) {
    auto result = timeManager_.setSystemTimezone("America/New_York");
    EXPECT_FALSE(result);
    auto lastTimezone = mockImpl_->getLastTimezone();
    ASSERT_TRUE(lastTimezone.has_value());
    EXPECT_EQ(*lastTimezone, "America/New_York");
}

TEST_F(TimeManagerTest, SetSystemTimezone_Invalid) {
    auto result1 = timeManager_.setSystemTimezone("");
    EXPECT_TRUE(result1);
    EXPECT_EQ(static_cast<int>(result1.value()),
              static_cast<int>(TimeError::InvalidParameter));
    std::string longTimezone(65, 'x');
    auto result2 = timeManager_.setSystemTimezone(longTimezone);
    EXPECT_TRUE(result2);
    EXPECT_EQ(static_cast<int>(result2.value()),
              static_cast<int>(TimeError::InvalidParameter));
}

TEST_F(TimeManagerTest, SyncTimeFromRTC) {
    auto result = timeManager_.syncTimeFromRTC();
    EXPECT_FALSE(result);
    EXPECT_TRUE(mockImpl_->wasRtcSyncCalled());
}

TEST_F(TimeManagerTest, GetNtpTime_Success) {
    std::time_t expectedTime = 1620000000;
    mockImpl_->setMockNtpTime(expectedTime);
    auto result = timeManager_.getNtpTime("pool.ntp.org", 3000ms);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, expectedTime);
    auto lastHost = mockImpl_->getLastNtpHost();
    ASSERT_TRUE(lastHost.has_value());
    EXPECT_EQ(*lastHost, "pool.ntp.org");
    auto lastTimeout = mockImpl_->getLastNtpTimeout();
    ASSERT_TRUE(lastTimeout.has_value());
    EXPECT_EQ(*lastTimeout, 3000ms);
}

TEST_F(TimeManagerTest, GetNtpTime_Failure) {
    mockImpl_->setMockNtpFailure(true);
    auto result = timeManager_.getNtpTime("pool.ntp.org");
    EXPECT_FALSE(result.has_value());
}

TEST_F(TimeManagerTest, GetNtpTime_InvalidHostname) {
    auto result = timeManager_.getNtpTime("");
    EXPECT_FALSE(result.has_value());
}

TEST_F(TimeManagerTest, GetNtpTime_DefaultTimeout) {
    mockImpl_->setMockNtpTime(1620000000);
    auto result = timeManager_.getNtpTime("pool.ntp.org");
    ASSERT_TRUE(result.has_value());
    auto lastTimeout = mockImpl_->getLastNtpTimeout();
    ASSERT_TRUE(lastTimeout.has_value());
    EXPECT_EQ(*lastTimeout, 5000ms);
}

TEST_F(TimeManagerTest, MoveConstructor) {
    std::time_t expectedTime = 1620000000;
    mockImpl_->setMockTime(expectedTime);
    TimeManager movedManager = std::move(timeManager_);
    EXPECT_EQ(movedManager.getSystemTime(), expectedTime);
}

TEST_F(TimeManagerTest, MoveAssignment) {
    std::time_t expectedTime = 1620000000;
    mockImpl_->setMockTime(expectedTime);
    TimeManager secondManager;
    secondManager = std::move(timeManager_);
    EXPECT_EQ(secondManager.getSystemTime(), expectedTime);
}

TEST_F(TimeManagerTest, EdgeCases) {
    auto leapYearResult = timeManager_.setSystemTime(2024, 2, 29, 12, 0, 0);
    EXPECT_FALSE(leapYearResult);
    auto nonLeapYearResult = timeManager_.setSystemTime(2023, 2, 29, 12, 0, 0);
    EXPECT_TRUE(nonLeapYearResult);
    auto validMonthEndResult = timeManager_.setSystemTime(2023, 4, 30, 12, 0, 0);
    EXPECT_FALSE(validMonthEndResult);
    auto invalidMonthEndResult = timeManager_.setSystemTime(2023, 4, 31, 12, 0, 0);
    EXPECT_TRUE(invalidMonthEndResult);
    auto unusualTimezoneResult = timeManager_.setSystemTimezone("Etc/GMT+12");
    EXPECT_FALSE(unusualTimezoneResult);
}

TEST_F(TimeManagerTest, ConcurrentOperations) {
    mockImpl_->setMockTime(1620000000);
    mockImpl_->setMockNtpTime(1620000100);
    std::thread t1([&]() { (void)timeManager_.getSystemTime(); });
    std::thread t2([&]() { (void)timeManager_.getSystemTimePoint(); });
    std::thread t3([&]() { (void)timeManager_.setSystemTime(2022, 3, 15, 14, 30, 45); });
    std::thread t4([&]() { (void)timeManager_.getNtpTime("pool.ntp.org"); });
    t1.join();
    t2.join();
    t3.join();
    t4.join();
    EXPECT_TRUE(true);
}
