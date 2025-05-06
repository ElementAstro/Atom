#include "stopwatcher.hpp"

#include <algorithm>  // std::ranges
#include <atomic>     // std::atomic for thread-safe operations
#include <chrono>
#include <cmath>  // std::sqrt for standard deviation
#include <execution>
#include <format>
#include <iomanip>
#include <mutex>
#include <numeric>
#include <ranges>  // C++20 ranges
#include <shared_mutex>
#include <sstream>
#include <vector>

#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"

namespace atom::utils {

using namespace std::literals::chrono_literals;

// Implementation of ScopedStopWatch
ScopedStopWatch::ScopedStopWatch(std::string_view name) : stopwatch_(name) {
    auto result = stopwatch_.start();
    if (!result) {
        LOG_F(WARNING, "Failed to start ScopedStopWatch: {}",
              static_cast<int>(result.error()));
    }
}

ScopedStopWatch::~ScopedStopWatch() {
    auto result = stopwatch_.stop();
    if (result) {
        LOG_F(INFO, "ScopedStopWatch '{}' completed in {} ms",
              stopwatch_.getName(), stopwatch_.elapsedMilliseconds());
    } else {
        LOG_F(WARNING, "Failed to stop ScopedStopWatch: {}",
              static_cast<int>(result.error()));
    }
}

auto ScopedStopWatch::getStopWatcher() const -> const StopWatcher& {
    return stopwatch_;
}

// Implementation of StopWatcher
class StopWatcher::Impl {
public:
    Impl(std::string_view name) : name_(name) {
        try {
            LOG_F(INFO, "StopWatcher '{}' initialized", name_);
        } catch (...) {
            // Ensure constructor doesn't throw, but log failure is non-fatal
        }
    }

    // Helper method for deserialization
    void addLapTime(double lapTime) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        lapTimes_.push_back(lapTime);
    }

    ~Impl() { stopAutoLapThread(); }

    [[nodiscard]] auto start() -> std::expected<void, StopWatcherError> {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        try {
            if (state_ == StopWatcherState::Running) {
                LOG_F(WARNING, "StopWatcher '{}' already running", name_);
                return std::unexpected(StopWatcherError::AlreadyRunning);
            }

            if (state_ == StopWatcherState::Idle ||
                state_ == StopWatcherState::Stopped) {
                startTime_ = Clock::now();
                state_ = StopWatcherState::Running;
                intervals_.clear();
                lapTimes_.clear();
                intervals_.push_back(startTime_);
                totalPausedTime_ = Duration::zero();
                LOG_F(INFO, "StopWatcher '{}' started", name_);

                // Restart auto-lap thread if it was previously enabled
                if (autoLapInterval_ > 0) {
                    startAutoLapThread();
                }
            }
            return {};
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error starting StopWatcher '{}': {}", name_,
                  e.what());
            THROW_STOPWATCHER_EXCEPTION(
                std::format("Failed to start StopWatcher: {}", e.what()));
        }
    }

    [[nodiscard]] auto stop() -> std::expected<void, StopWatcherError> {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        try {
            if (state_ != StopWatcherState::Running) {
                LOG_F(WARNING, "Attempted to stop non-running StopWatcher '{}'",
                      name_);
                return std::unexpected(StopWatcherError::NotRunning);
            }

            auto stopTime = Clock::now();
            endTime_ = stopTime;
            state_ = StopWatcherState::Stopped;
            intervals_.push_back(stopTime);

            // Stop auto-lap thread when timer stops
            stopAutoLapThread();

            // Process callbacks outside the lock to avoid deadlocks
            auto callbacksCopy = callbacks_;
            lock.unlock();

            for (const auto& [callback, interval] : callbacksCopy) {
                try {
                    auto targetTime =
                        startTime_ + std::chrono::milliseconds(interval);
                    if (stopTime >= targetTime) {
                        callback();
                        LOG_F(INFO, "Callback executed at {} ms", interval);
                    }
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Callback execution failed: {}", e.what());
                }
            }

            LOG_F(INFO, "StopWatcher '{}' stopped. Total time: {:.3f} ms",
                  name_, elapsedMillisecondsInternal());
            return {};
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error stopping StopWatcher '{}': {}", name_,
                  e.what());
            THROW_STOPWATCHER_EXCEPTION("Failed to stop StopWatcher: {}",
                                        e.what());
        }
    }

    [[nodiscard]] auto pause() -> std::expected<void, StopWatcherError> {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        try {
            if (state_ != StopWatcherState::Running) {
                LOG_F(WARNING,
                      "Attempted to pause non-running StopWatcher '{}'", name_);
                return std::unexpected(StopWatcherError::NotRunning);
            }

            pauseTime_ = Clock::now();
            state_ = StopWatcherState::Paused;
            intervals_.push_back(pauseTime_);

            // Pause auto-lap thread
            isAutoLapActive_.store(false, std::memory_order_relaxed);

            LOG_F(INFO, "StopWatcher '{}' paused at {:.3f} ms", name_,
                  elapsedMillisecondsInternal());
            return {};
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error pausing StopWatcher '{}': {}", name_, e.what());
            THROW_STOPWATCHER_EXCEPTION(
                std::format("Failed to pause StopWatcher: {}", e.what()));
        }
    }

    [[nodiscard]] auto resume() -> std::expected<void, StopWatcherError> {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        try {
            if (state_ != StopWatcherState::Paused) {
                LOG_F(WARNING,
                      "Attempted to resume non-paused StopWatcher '{}'", name_);
                return std::unexpected(StopWatcherError::NotPaused);
            }

            auto resumeTime = Clock::now();
            totalPausedTime_ += resumeTime - pauseTime_;
            state_ = StopWatcherState::Running;
            intervals_.push_back(resumeTime);

            // Resume auto-lap thread if it was enabled
            if (autoLapInterval_ > 0) {
                isAutoLapActive_.store(true, std::memory_order_relaxed);
            }

            LOG_F(INFO, "StopWatcher '{}' resumed", name_);
            return {};
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error resuming StopWatcher '{}': {}", name_,
                  e.what());
            THROW_STOPWATCHER_EXCEPTION(
                std::format("Failed to resume StopWatcher: {}", e.what()));
        }
    }

    void reset() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        try {
            // Stop auto-lap thread
            stopAutoLapThread();

            state_ = StopWatcherState::Idle;
            intervals_.clear();
            lapTimes_.clear();
            callbacks_.clear();
            totalPausedTime_ = Duration::zero();

            // Clear child stopwatches
            childStopWatches_.clear();

            LOG_F(INFO, "StopWatcher '{}' reset", name_);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error resetting StopWatcher '{}': {}", name_,
                  e.what());
            THROW_STOPWATCHER_EXCEPTION(
                std::format("Failed to reset StopWatcher: {}", e.what()));
        }
    }

    [[nodiscard]] auto lap() -> std::expected<double, StopWatcherError> {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        try {
            if (state_ != StopWatcherState::Running) {
                LOG_F(WARNING,
                      "Cannot record lap: StopWatcher '{}' not running", name_);
                return std::unexpected(StopWatcherError::NotRunning);
            }

            auto lapTime = Clock::now();
            auto elapsed = std::chrono::duration<double, std::milli>(
                               lapTime - startTime_ - totalPausedTime_)
                               .count();
            lapTimes_.push_back(elapsed);
            LOG_F(INFO, "Lap recorded for '{}': {:.3f} ms", name_, elapsed);
            return elapsed;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error recording lap for '{}': {}", name_, e.what());
            THROW_STOPWATCHER_EXCEPTION(
                std::format("Failed to record lap: {}", e.what()));
        }
    }

    [[nodiscard]] constexpr double elapsedMillisecondsInternal() const {
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

        try {
            // Use C++20's std::format with chrono formatting
            auto duration =
                std::chrono::duration<double, std::milli>(totalMilliseconds);
            auto hours =
                std::chrono::duration_cast<std::chrono::hours>(duration);
            auto minutes =
                std::chrono::duration_cast<std::chrono::minutes>(duration) %
                60min;
            auto seconds =
                std::chrono::duration_cast<std::chrono::seconds>(duration) %
                60s;
            auto milliseconds =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    duration) %
                1000ms;

            return std::format("{:02}:{:02}:{:02}.{:03}", hours.count(),
                               minutes.count(), seconds.count(),
                               milliseconds.count());
        } catch (...) {
            // Fallback if std::format throws or is not available
            auto totalSeconds =
                static_cast<int>(totalMilliseconds / K_MILLISECONDS_PER_SECOND);
            int hours = totalSeconds / K_SECONDS_PER_HOUR;
            int minutes =
                (totalSeconds % K_SECONDS_PER_HOUR) / K_SECONDS_PER_MINUTE;
            int seconds = totalSeconds % K_SECONDS_PER_MINUTE;
            int milliseconds =
                static_cast<int>(totalMilliseconds) % K_MILLISECONDS_PER_SECOND;

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

    [[nodiscard]] LapStatistics getLapStatistics() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        if (lapTimes_.empty()) {
            return LapStatistics{};
        }

        try {
            // Use parallel algorithm for large datasets
            const bool useParallel = lapTimes_.size() > 1000;

            // Calculate min and max
            auto [minIt, maxIt] =
                useParallel
                    ? std::minmax_element(std::execution::par_unseq,
                                          lapTimes_.begin(), lapTimes_.end())
                    : std::minmax_element(lapTimes_.begin(), lapTimes_.end());

            double min = *minIt;
            double max = *maxIt;

            // Calculate average
            double sum =
                useParallel
                    ? std::reduce(std::execution::par_unseq, lapTimes_.begin(),
                                  lapTimes_.end(), 0.0)
                    : std::reduce(lapTimes_.begin(), lapTimes_.end(), 0.0);
            double avg = sum / static_cast<double>(lapTimes_.size());

            // Calculate standard deviation
            double varianceSum = 0.0;
            if (useParallel) {
                varianceSum = std::transform_reduce(
                    std::execution::par_unseq, lapTimes_.begin(),
                    lapTimes_.end(), 0.0, std::plus<>(),
                    [avg](double x) { return (x - avg) * (x - avg); });
            } else {
                for (double time : lapTimes_) {
                    varianceSum += (time - avg) * (time - avg);
                }
            }

            double stdDev =
                std::sqrt(varianceSum / static_cast<double>(lapTimes_.size()));

            return LapStatistics{min, max, avg, stdDev, lapTimes_.size()};
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error calculating lap statistics: {}", e.what());
            return LapStatistics{};
        }
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

    [[nodiscard]] auto registerCallback(std::function<void()> callback,
                                        int milliseconds)
        -> std::expected<void, StopWatcherError> {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        try {
            if (milliseconds < 0) {
                LOG_F(ERROR, "Invalid callback interval: {} ms", milliseconds);
                return std::unexpected(StopWatcherError::InvalidInterval);
            }
            callbacks_.emplace_back(std::move(callback), milliseconds);
            LOG_F(INFO, "Callback registered for {} ms", milliseconds);
            return {};
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error registering callback: {}", e.what());
            return std::unexpected(StopWatcherError::CallbackFailed);
        }
    }

    [[nodiscard]] auto enableAutoLap(int intervalMs)
        -> std::expected<void, StopWatcherError> {
        if (intervalMs <= 0) {
            LOG_F(ERROR, "Invalid auto-lap interval: {} ms", intervalMs);
            return std::unexpected(StopWatcherError::InvalidInterval);
        }

        std::unique_lock<std::shared_mutex> lock(mutex_);
        autoLapInterval_ = intervalMs;

        if (state_ == StopWatcherState::Running) {
            startAutoLapThread();
        }

        LOG_F(INFO, "Auto-lap enabled for StopWatcher '{}' with interval {} ms",
              name_, intervalMs);
        return {};
    }

    void disableAutoLap() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        stopAutoLapThread();
        autoLapInterval_ = 0;
        LOG_F(INFO, "Auto-lap disabled for StopWatcher '{}'", name_);
    }

    [[nodiscard]] bool isRunning() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return state_ == StopWatcherState::Running;
    }

    [[nodiscard]] std::string_view getName() const {
        // Name is immutable after construction, no lock needed
        return name_;
    }

    [[nodiscard]] auto createChildStopWatch(std::string_view name)
        -> std::unique_ptr<StopWatcher> {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        std::string childName = std::format("{}:{}", name_, name);
        auto child = std::make_unique<StopWatcher>(childName);

        // Store a weak_ptr to avoid ownership cycles
        childStopWatches_.push_back(std::weak_ptr<StopWatcher>(
            std::shared_ptr<StopWatcher>(child.release())));

        LOG_F(INFO, "Created child StopWatcher '{}' for parent '{}'", childName,
              name_);
        return child;
    }

    [[nodiscard]] auto toJson() const -> std::string {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        nlohmann::json j;
        j["name"] = name_;
        j["state"] = static_cast<int>(state_);
        j["elapsed_ms"] = elapsedMillisecondsInternal();

        // Store lap times
        j["lap_times"] = nlohmann::json::array();
        for (const auto& lap : lapTimes_) {
            j["lap_times"].push_back(lap);
        }

        // Include statistics
        auto stats = getLapStatistics();
        j["statistics"] = {{"min", stats.min},
                           {"max", stats.max},
                           {"avg", stats.average},
                           {"std_dev", stats.standardDev},
                           {"count", stats.count}};

        // Include child stopwatches
        j["children"] = nlohmann::json::array();
        for (const auto& weakChild : childStopWatches_) {
            if (auto child = weakChild.lock()) {
                j["children"].push_back(nlohmann::json::parse(child->toJson()));
            }
        }

        return j.dump(2);  // Pretty-print with 2-space indentation
    }

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::high_resolution_clock::duration;

    std::string name_;  // Stopwatch name
    TimePoint startTime_{}, endTime_{}, pauseTime_{};
    Duration totalPausedTime_{Duration::zero()};
    StopWatcherState state_{StopWatcherState::Idle};
    std::vector<TimePoint> intervals_;
    std::vector<double> lapTimes_;
    std::vector<std::pair<std::function<void()>, int>> callbacks_;
    std::vector<std::weak_ptr<StopWatcher>> childStopWatches_;

    // Auto-lap functionality
    int autoLapInterval_{0};
    std::atomic<bool> isAutoLapActive_{false};
    std::jthread autoLapThread_;

    mutable std::shared_mutex
        mutex_;  // Shared mutex for reader-writer lock pattern

    static constexpr int K_MILLISECONDS_PER_SECOND = 1000;
    static constexpr int K_SECONDS_PER_MINUTE = 60;
    static constexpr int K_SECONDS_PER_HOUR = 3600;

    // Start the auto-lap thread
    void startAutoLapThread() {
        stopAutoLapThread();  // Ensure any existing thread is stopped

        if (autoLapInterval_ <= 0) {
            return;
        }

        isAutoLapActive_.store(true, std::memory_order_relaxed);

        // Create a new jthread for auto-lap functionality
        autoLapThread_ = std::jthread([this](std::stop_token stoken) {
            LOG_F(INFO, "Auto-lap thread started for StopWatcher '{}'", name_);

            while (!stoken.stop_requested()) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(autoLapInterval_));

                if (stoken.stop_requested() ||
                    !isAutoLapActive_.load(std::memory_order_relaxed)) {
                    break;
                }

                try {
                    // Create a temporary unique_lock only when recording a lap
                    std::unique_lock<std::shared_mutex> lock(mutex_);

                    if (state_ == StopWatcherState::Running) {
                        auto lapTime = Clock::now();
                        auto elapsed =
                            std::chrono::duration<double, std::milli>(
                                lapTime - startTime_ - totalPausedTime_)
                                .count();
                        lapTimes_.push_back(elapsed);
                        lock.unlock();

                        LOG_F(INFO, "Auto-lap recorded for '{}': {:.3f} ms",
                              name_, elapsed);
                    }
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Error in auto-lap for '{}': {}", name_,
                          e.what());
                }
            }

            LOG_F(INFO, "Auto-lap thread stopped for StopWatcher '{}'", name_);
        });
    }

    // Stop the auto-lap thread
    void stopAutoLapThread() {
        isAutoLapActive_.store(false, std::memory_order_relaxed);

        if (autoLapThread_.joinable()) {
            autoLapThread_.request_stop();
            // jthread automatically joins on destruction
        }
    }
};

// StopWatcher public interface implementations
StopWatcher::StopWatcher(std::string_view name)
    : impl_(std::make_unique<Impl>(name)) {}
StopWatcher::~StopWatcher() = default;
StopWatcher::StopWatcher(StopWatcher&&) noexcept = default;
auto StopWatcher::operator=(StopWatcher&&) noexcept -> StopWatcher& = default;

auto StopWatcher::start() -> std::expected<void, StopWatcherError> {
    return impl_->start();
}

auto StopWatcher::stop() -> std::expected<void, StopWatcherError> {
    return impl_->stop();
}

auto StopWatcher::pause() -> std::expected<void, StopWatcherError> {
    return impl_->pause();
}

auto StopWatcher::resume() -> std::expected<void, StopWatcherError> {
    return impl_->resume();
}

void StopWatcher::reset() { impl_->reset(); }

auto StopWatcher::lap() -> std::expected<double, StopWatcherError> {
    return impl_->lap();
}

void StopWatcher::addLapTimeForDeserialization(double lapTime) {
    if (impl_) {
        impl_->addLapTime(lapTime);
    }
}

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

auto StopWatcher::getLapStatistics() const -> LapStatistics {
    return impl_->getLapStatistics();
}

auto StopWatcher::getLapCount() const -> size_t { return impl_->getLapCount(); }

auto StopWatcher::registerCallbackImpl(std::function<void()> callback,
                                       int milliseconds)
    -> std::expected<void, StopWatcherError> {
    return impl_->registerCallback(std::move(callback), milliseconds);
}

bool StopWatcher::isRunning() const { return impl_->isRunning(); }

auto StopWatcher::getName() const -> std::string_view {
    return impl_->getName();
}

auto StopWatcher::createChildStopWatch(std::string_view name)
    -> std::unique_ptr<StopWatcher> {
    return impl_->createChildStopWatch(name);
}

auto StopWatcher::enableAutoLap(int intervalMs)
    -> std::expected<void, StopWatcherError> {
    return impl_->enableAutoLap(intervalMs);
}

void StopWatcher::disableAutoLap() { impl_->disableAutoLap(); }

auto StopWatcher::toJson() const -> std::string { return impl_->toJson(); }

auto StopWatcher::fromJson(std::string_view json)
    -> std::unique_ptr<StopWatcher> {
    try {
        auto j = nlohmann::json::parse(json);

        // Extract basic info
        std::string name = j["name"].get<std::string>();
        auto sw = std::make_unique<StopWatcher>(name);

        // If we have lap times, initialize them
        if (j.contains("lap_times") && !j["lap_times"].empty()) {
            sw->reset();
            [[maybe_unused]] auto temp =
                sw->start();  // Start to set up the initial state

            // Just add lap times without actually measuring
            for (const auto& lapTime : j["lap_times"]) {
                sw->addLapTimeForDeserialization(lapTime.get<double>());
            }

            [[maybe_unused]] auto temp_s = sw->stop();  // Put in stopped state
        }

        LOG_F(INFO, "Created StopWatcher '{}' from JSON data", name);
        return sw;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error deserializing StopWatcher from JSON: {}", e.what());
        THROW_STOPWATCHER_EXCEPTION(
            std::format("Failed to deserialize StopWatcher: {}", e.what()));
    }
}

}  // namespace atom::utils