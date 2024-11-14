#include "stopwatcher.hpp"

#include <chrono>
#include <iomanip>
#include <mutex>
#include <numeric>
#include <sstream>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

namespace atom::utils {

class StopWatcher::Impl {
public:
    Impl() { LOG_F(INFO, "StopWatcher initialized"); }

    void start() {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (state_ == StopWatcherState::Idle ||
                state_ == StopWatcherState::Stopped) {
                startTime_ = Clock::now();
                state_ = StopWatcherState::Running;
                intervals_.clear();
                lapTimes_.clear();
                intervals_.push_back(startTime_);
                LOG_F(INFO, "StopWatcher started");
            } else {
                LOG_F(WARNING, "StopWatcher already running");
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error starting StopWatcher: {}", e.what());
            throw;
        }
    }

    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (state_ == StopWatcherState::Running) {
                auto stopTime = Clock::now();
                endTime_ = stopTime;
                state_ = StopWatcherState::Stopped;
                intervals_.push_back(stopTime);
                checkCallbacks(stopTime);
                LOG_F(INFO, "StopWatcher stopped. Total time: {} ms",
                      elapsedMilliseconds());
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error stopping StopWatcher: {}", e.what());
            throw;
        }
    }

    void pause() {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (state_ == StopWatcherState::Running) {
                pauseTime_ = Clock::now();
                state_ = StopWatcherState::Paused;
                intervals_.push_back(pauseTime_);
                LOG_F(INFO, "StopWatcher paused at {} ms",
                      elapsedMilliseconds());
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error pausing StopWatcher: {}", e.what());
            throw;
        }
    }

    void resume() {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (state_ == StopWatcherState::Paused) {
                auto resumeTime = Clock::now();
                totalPausedTime_ += resumeTime - pauseTime_;
                state_ = StopWatcherState::Running;
                intervals_.push_back(resumeTime);
                LOG_F(INFO, "StopWatcher resumed");
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error resuming StopWatcher: {}", e.what());
            throw;
        }
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            state_ = StopWatcherState::Idle;
            intervals_.clear();
            lapTimes_.clear();
            callbacks_.clear();
            totalPausedTime_ = Duration::zero();
            LOG_F(INFO, "StopWatcher reset");
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error resetting StopWatcher: {}", e.what());
            throw;
        }
    }

    void lap() {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (state_ == StopWatcherState::Running) {
                auto lapTime = Clock::now();
                auto elapsed = std::chrono::duration<double, std::milli>(
                                   lapTime - startTime_ - totalPausedTime_)
                                   .count();
                lapTimes_.push_back(elapsed);
                LOG_F(INFO, "Lap recorded: {} ms", elapsed);
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error recording lap: {}", e.what());
            throw;
        }
    }

    [[nodiscard]] auto elapsedMilliseconds() const -> double {
        std::lock_guard<std::mutex> lock(mutex_);
        auto endTimePoint =
            (state_ == StopWatcherState::Running)
                ? Clock::now()
                : (state_ == StopWatcherState::Paused ? pauseTime_ : endTime_);
        return std::chrono::duration<double, std::milli>(
                   endTimePoint - startTime_ - totalPausedTime_)
            .count();
    }

    [[nodiscard]] auto elapsedSeconds() const -> double {
        return elapsedMilliseconds() / K_MILLISECONDS_PER_SECOND;
    }

    [[nodiscard]] auto elapsedFormatted() const -> std::string {
        auto totalSeconds = static_cast<int>(elapsedSeconds());
        int hours = totalSeconds / K_SECONDS_PER_HOUR;
        int minutes =
            (totalSeconds % K_SECONDS_PER_HOUR) / K_SECONDS_PER_MINUTE;
        int seconds = totalSeconds % K_SECONDS_PER_MINUTE;
        int milliseconds = static_cast<int>(elapsedMilliseconds()) % 1000;

        std::ostringstream stream;
        stream << std::setw(2) << std::setfill('0') << hours << ":"
               << std::setw(2) << std::setfill('0') << minutes << ":"
               << std::setw(2) << std::setfill('0') << seconds << "."
               << std::setw(3) << std::setfill('0') << milliseconds;
        return stream.str();
    }

    [[nodiscard]] auto getState() const -> StopWatcherState {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

    [[nodiscard]] auto getLapTimes() const -> std::vector<double> {
        std::lock_guard<std::mutex> lock(mutex_);
        return lapTimes_;
    }

    [[nodiscard]] auto getAverageLapTime() const -> double {
        std::lock_guard<std::mutex> lock(mutex_);
        if (lapTimes_.empty()) {
            return 0.0;
        }
        return std::accumulate(lapTimes_.begin(), lapTimes_.end(), 0.0) /
               static_cast<double>(lapTimes_.size());
    }

    void registerCallback(std::function<void()> callback, int milliseconds) {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (milliseconds < 0) {
                THROW_INVALID_ARGUMENT("Callback interval must be positive");
            }
            callbacks_.emplace_back(std::move(callback), milliseconds);
            LOG_F(INFO, "Callback registered for {} ms", milliseconds);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error registering callback: {}", e.what());
            throw;
        }
    }

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::high_resolution_clock::duration;

    TimePoint startTime_, endTime_, pauseTime_;
    Duration totalPausedTime_{Duration::zero()};
    StopWatcherState state_{StopWatcherState::Idle};
    std::vector<TimePoint> intervals_;
    std::vector<double> lapTimes_;
    std::vector<std::pair<std::function<void()>, int>> callbacks_;
    mutable std::mutex mutex_;

    static constexpr int K_MILLISECONDS_PER_SECOND = 1000;
    static constexpr int K_SECONDS_PER_MINUTE = 60;
    static constexpr int K_SECONDS_PER_HOUR = 3600;

    void checkCallbacks(const TimePoint& currentTime) {
        for (const auto& [callback, interval] : callbacks_) {
            auto targetTime = startTime_ + std::chrono::milliseconds(interval);
            if (currentTime >= targetTime) {
                try {
                    callback();
                    LOG_F(INFO, "Callback executed at {} ms", interval);
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Callback execution failed: {}", e.what());
                }
            }
        }
    }
};

// StopWatcher public interface implementations
StopWatcher::StopWatcher() : impl_(std::make_unique<Impl>()) {}
StopWatcher::~StopWatcher() = default;
StopWatcher::StopWatcher(StopWatcher&&) noexcept = default;
auto StopWatcher::operator=(StopWatcher&&) noexcept -> StopWatcher& = default;

void StopWatcher::start() { impl_->start(); }
void StopWatcher::stop() { impl_->stop(); }
void StopWatcher::pause() { impl_->pause(); }
void StopWatcher::resume() { impl_->resume(); }
void StopWatcher::reset() { impl_->reset(); }
void StopWatcher::lap() { impl_->lap(); }

auto StopWatcher::elapsedMilliseconds() const -> double {
    return impl_->elapsedMilliseconds();
}
auto StopWatcher::elapsedSeconds() const -> double {
    return impl_->elapsedSeconds();
}
auto StopWatcher::elapsedFormatted() const -> std::string {
    return impl_->elapsedFormatted();
}
auto StopWatcher::getState() const -> StopWatcherState {
    return impl_->getState();
}
auto StopWatcher::getLapTimes() const -> std::vector<double> {
    return impl_->getLapTimes();
}
auto StopWatcher::getAverageLapTime() const -> double {
    return impl_->getAverageLapTime();
}

void StopWatcher::registerCallback(std::function<void()> callback,
                                   int milliseconds) {
    impl_->registerCallback(std::move(callback), milliseconds);
}

}  // namespace atom::utils