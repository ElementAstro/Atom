#include "stopwatcher.hpp"

#include <chrono>
#include <execution>
#include <format>
#include <iomanip>
#include <mutex>
#include <numeric>
#include <shared_mutex>
#include <sstream>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

namespace atom::utils {

using namespace std::literals::chrono_literals;

class StopWatcher::Impl {
public:
    Impl() {
        try {
            LOG_F(INFO, "StopWatcher initialized");
        } catch (...) {
            // Ensure constructor doesn't throw, but log failure is non-fatal
        }
    }

    void start() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        try {
            if (state_ == StopWatcherState::Running) {
                LOG_F(WARNING, "StopWatcher already running");
                THROW_RUNTIME_ERROR("Stopwatch already running");
            }

            if (state_ == StopWatcherState::Idle ||
                state_ == StopWatcherState::Stopped) {
                startTime_ = Clock::now();
                state_ = StopWatcherState::Running;
                intervals_.clear();
                lapTimes_.clear();
                intervals_.push_back(startTime_);
                totalPausedTime_ = Duration::zero();
                LOG_F(INFO, "StopWatcher started");
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error starting StopWatcher: {}", e.what());
            throw;
        }
    }

    bool stop() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        try {
            if (state_ != StopWatcherState::Running) {
                LOG_F(WARNING, "Attempted to stop non-running StopWatcher");
                return false;
            }

            auto stopTime = Clock::now();
            endTime_ = stopTime;
            state_ = StopWatcherState::Stopped;
            intervals_.push_back(stopTime);
            processCallbacks(stopTime);
            LOG_F(INFO, "StopWatcher stopped. Total time: {:.3f} ms",
                  elapsedMillisecondsInternal());
            return true;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error stopping StopWatcher: {}", e.what());
            throw;
        }
    }

    bool pause() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        try {
            if (state_ != StopWatcherState::Running) {
                LOG_F(WARNING, "Attempted to pause non-running StopWatcher");
                return false;
            }

            pauseTime_ = Clock::now();
            state_ = StopWatcherState::Paused;
            intervals_.push_back(pauseTime_);
            LOG_F(INFO, "StopWatcher paused at {:.3f} ms",
                  elapsedMillisecondsInternal());
            return true;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error pausing StopWatcher: {}", e.what());
            throw;
        }
    }

    bool resume() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        try {
            if (state_ != StopWatcherState::Paused) {
                LOG_F(WARNING, "Attempted to resume non-paused StopWatcher");
                return false;
            }

            auto resumeTime = Clock::now();
            totalPausedTime_ += resumeTime - pauseTime_;
            state_ = StopWatcherState::Running;
            intervals_.push_back(resumeTime);
            LOG_F(INFO, "StopWatcher resumed");
            return true;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error resuming StopWatcher: {}", e.what());
            throw;
        }
    }

    void reset() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
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

    double lap() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        try {
            if (state_ != StopWatcherState::Running) {
                LOG_F(WARNING, "Cannot record lap: StopWatcher not running");
                THROW_RUNTIME_ERROR(
                    "Cannot record lap: StopWatcher not running");
            }

            auto lapTime = Clock::now();
            auto elapsed = std::chrono::duration<double, std::milli>(
                               lapTime - startTime_ - totalPausedTime_)
                               .count();
            lapTimes_.push_back(elapsed);
            LOG_F(INFO, "Lap recorded: {:.3f} ms", elapsed);
            return elapsed;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error recording lap: {}", e.what());
            throw;
        }
    }

    [[nodiscard]] double elapsedMillisecondsInternal() const {
        auto endTimePoint =
            (state_ == StopWatcherState::Running)
                ? Clock::now()
                : (state_ == StopWatcherState::Paused ? pauseTime_ : endTime_);
        return std::chrono::duration<double, std::milli>(
                   endTimePoint - startTime_ - totalPausedTime_)
            .count();
    }

    [[nodiscard]] double elapsedMilliseconds() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return elapsedMillisecondsInternal();
    }

    [[nodiscard]] double elapsedSeconds() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return elapsedMillisecondsInternal() / K_MILLISECONDS_PER_SECOND;
    }

    [[nodiscard]] std::string elapsedFormatted() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto totalMilliseconds = elapsedMillisecondsInternal();
        auto totalSeconds =
            static_cast<int>(totalMilliseconds / K_MILLISECONDS_PER_SECOND);
        int hours = totalSeconds / K_SECONDS_PER_HOUR;
        int minutes =
            (totalSeconds % K_SECONDS_PER_HOUR) / K_SECONDS_PER_MINUTE;
        int seconds = totalSeconds % K_SECONDS_PER_MINUTE;
        int milliseconds =
            static_cast<int>(totalMilliseconds) % K_MILLISECONDS_PER_SECOND;

        try {
            // Use C++20 std::format if available
            return std::format("{:02}:{:02}:{:02}.{:03}", hours, minutes,
                               seconds, milliseconds);
        } catch (...) {
            // Fallback if std::format throws or is not available on the
            // platform
            std::ostringstream stream;
            stream << std::setw(2) << std::setfill('0') << hours << ":"
                   << std::setw(2) << std::setfill('0') << minutes << ":"
                   << std::setw(2) << std::setfill('0') << seconds << "."
                   << std::setw(3) << std::setfill('0') << milliseconds;
            return stream.str();
        }
    }

    [[nodiscard]] StopWatcherState getState() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return state_;
    }

    [[nodiscard]] std::span<const double> getLapTimes() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return std::span<const double>(lapTimes_);
    }

    [[nodiscard]] double getAverageLapTime() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        if (lapTimes_.empty()) {
            return 0.0;
        }

        try {
            // Use parallel execution policy for larger datasets
            if (lapTimes_.size() > 1000) {
                return std::reduce(std::execution::par_unseq, lapTimes_.begin(),
                                   lapTimes_.end(), 0.0) /
                       static_cast<double>(lapTimes_.size());
            } else {
                return std::reduce(lapTimes_.begin(), lapTimes_.end(), 0.0) /
                       static_cast<double>(lapTimes_.size());
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error calculating average lap time: {}", e.what());
            return 0.0;
        }
    }

    [[nodiscard]] size_t getLapCount() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return lapTimes_.size();
    }

    void registerCallback(std::function<void()> callback, int milliseconds) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        try {
            if (milliseconds < 0) {
                LOG_F(ERROR, "Invalid callback interval: {} ms", milliseconds);
                THROW_INVALID_ARGUMENT(
                    "Callback interval must be non-negative");
            }
            callbacks_.emplace_back(std::move(callback), milliseconds);
            LOG_F(INFO, "Callback registered for {} ms", milliseconds);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error registering callback: {}", e.what());
            throw;
        }
    }

    [[nodiscard]] bool isRunning() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return state_ == StopWatcherState::Running;
    }

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::high_resolution_clock::duration;

    TimePoint startTime_{}, endTime_{}, pauseTime_{};
    Duration totalPausedTime_{Duration::zero()};
    StopWatcherState state_{StopWatcherState::Idle};
    std::vector<TimePoint> intervals_;
    std::vector<double> lapTimes_;
    std::vector<std::pair<std::function<void()>, int>> callbacks_;
    mutable std::shared_mutex
        mutex_;  // Shared mutex for reader-writer lock pattern

    static constexpr int K_MILLISECONDS_PER_SECOND = 1000;
    static constexpr int K_SECONDS_PER_MINUTE = 60;
    static constexpr int K_SECONDS_PER_HOUR = 3600;

    void processCallbacks(const TimePoint& currentTime) {
        // Make a copy of callbacks to process to avoid deadlocks
        auto callbacksCopy = callbacks_;

        // Release the lock before potentially lengthy callback processing
        mutex_.unlock();

        for (const auto& [callback, interval] : callbacksCopy) {
            try {
                auto targetTime =
                    startTime_ + std::chrono::milliseconds(interval);
                if (currentTime >= targetTime) {
                    callback();
                    LOG_F(INFO, "Callback executed at {} ms", interval);
                }
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Callback execution failed: {}", e.what());
                // Continue with other callbacks even if one fails
            }
        }

        // Reacquire the lock
        std::unique_lock<std::shared_mutex> lock(mutex_);
    }
};

// StopWatcher public interface implementations
StopWatcher::StopWatcher() : impl_(std::make_unique<Impl>()) {}
StopWatcher::~StopWatcher() = default;
StopWatcher::StopWatcher(StopWatcher&&) noexcept = default;
auto StopWatcher::operator=(StopWatcher&&) noexcept -> StopWatcher& = default;

void StopWatcher::start() { impl_->start(); }
bool StopWatcher::stop() { return impl_->stop(); }
bool StopWatcher::pause() { return impl_->pause(); }
bool StopWatcher::resume() { return impl_->resume(); }
void StopWatcher::reset() { impl_->reset(); }
auto StopWatcher::lap() -> double { return impl_->lap(); }

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
auto StopWatcher::getLapTimes() const -> std::span<const double> {
    return impl_->getLapTimes();
}
auto StopWatcher::getAverageLapTime() const -> double {
    return impl_->getAverageLapTime();
}
auto StopWatcher::getLapCount() const -> size_t { return impl_->getLapCount(); }
void StopWatcher::registerCallbackImpl(std::function<void()> callback,
                                       int milliseconds) {
    impl_->registerCallback(std::move(callback), milliseconds);
}
bool StopWatcher::isRunning() const { return impl_->isRunning(); }

}  // namespace atom::utils