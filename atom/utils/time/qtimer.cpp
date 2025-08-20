#include "qtimer.hpp"

#include <algorithm>
#include <compare>

#include "atom/error/exception.hpp"

namespace atom::utils {

ElapsedTimer::ElapsedTimer(bool start_now) {
    if (start_now) {
        start();
    }
}

void ElapsedTimer::start() {
    try {
        start_time_ = Clock::now();
    } catch (const std::exception& e) {
        THROW_RUNTIME_ERROR(std::string("Failed to start timer: ") + e.what());
    }
}

void ElapsedTimer::invalidate() { start_time_.reset(); }

auto ElapsedTimer::isValid() const noexcept -> bool {
    return start_time_.has_value();
}

auto ElapsedTimer::elapsedNs() const -> int64_t {
    return elapsed<Nanoseconds>();
}

auto ElapsedTimer::elapsedUs() const -> int64_t {
    return elapsed<Microseconds>();
}

auto ElapsedTimer::elapsedMs() const -> int64_t {
    return elapsed<Milliseconds>();
}

auto ElapsedTimer::elapsedSec() const -> int64_t { return elapsed<Seconds>(); }

auto ElapsedTimer::elapsedMin() const -> int64_t { return elapsed<Minutes>(); }

auto ElapsedTimer::elapsedHrs() const -> int64_t { return elapsed<Hours>(); }

auto ElapsedTimer::elapsed() const -> int64_t { return elapsedMs(); }

auto ElapsedTimer::hasExpired(int64_t ms) const -> bool {
    if (ms < 0) {
        THROW_INVALID_ARGUMENT("Duration cannot be negative");
    }

    return elapsedMs() >= ms;
}

auto ElapsedTimer::remainingTimeMs(int64_t ms) const -> int64_t {
    if (ms < 0) {
        THROW_INVALID_ARGUMENT("Duration cannot be negative");
    }

    if (!isValid()) {
        return 0;
    }

    int64_t elapsed = elapsedMs();
    return std::max<int64_t>(0, ms - elapsed);
}

auto ElapsedTimer::currentTimeMs() noexcept -> int64_t {
    try {
        return std::chrono::duration_cast<Milliseconds>(
                   Clock::now().time_since_epoch())
            .count();
    } catch (...) {
        return 0;
    }
}

auto ElapsedTimer::operator<=>(const ElapsedTimer& other) const noexcept
    -> std::strong_ordering {
    if (!isValid() && !other.isValid()) {
        return std::strong_ordering::equal;
    }

    if (!isValid()) {
        return std::strong_ordering::less;
    }

    if (!other.isValid()) {
        return std::strong_ordering::greater;
    }

    if (start_time_.value() < other.start_time_.value()) {
        return std::strong_ordering::less;
    } else if (start_time_.value() > other.start_time_.value()) {
        return std::strong_ordering::greater;
    } else {
        return std::strong_ordering::equal;
    }
}

auto ElapsedTimer::operator==(const ElapsedTimer& other) const noexcept
    -> bool {
    if (!isValid() && !other.isValid()) {
        return true;
    }

    if (isValid() != other.isValid()) {
        return false;
    }

    return start_time_.value() == other.start_time_.value();
}

Timer::Timer(Callback callback) : callback_(std::move(callback)) {}

Timer::~Timer() {
    try {
        stop();
    } catch (...) {
    }
}

Timer::Timer(Timer&& other) noexcept {
    std::lock_guard<std::mutex> lock(other.timer_mutex_);
    callback_ = std::move(other.callback_);
    interval_ = other.interval_.load();
    is_active_ = other.is_active_.load();
    is_single_shot_ = other.is_single_shot_.load();
    precision_mode_ = other.precision_mode_.load();

    other.is_active_ = false;
    other.should_stop_ = true;

    if (other.timer_thread_.joinable()) {
        other.timer_thread_.join();
    }

    next_timeout_ = other.next_timeout_;
    other.next_timeout_.reset();
}

Timer& Timer::operator=(Timer&& other) noexcept {
    if (this != &other) {
        try {
            stop();
        } catch (...) {
        }

        std::lock_guard<std::mutex> lock(other.timer_mutex_);
        callback_ = std::move(other.callback_);
        interval_ = other.interval_.load();
        is_active_ = other.is_active_.load();
        is_single_shot_ = other.is_single_shot_.load();
        precision_mode_ = other.precision_mode_.load();

        other.is_active_ = false;
        other.should_stop_ = true;

        if (other.timer_thread_.joinable()) {
            other.timer_thread_.join();
        }

        next_timeout_ = other.next_timeout_;
        other.next_timeout_.reset();
    }
    return *this;
}

void Timer::setCallback(Callback callback) {
    std::lock_guard<std::mutex> lock(timer_mutex_);
    callback_ = std::move(callback);
}

void Timer::setInterval(int64_t milliseconds) {
    if (milliseconds <= 0) {
        throw TimerException(TimerException::ErrorCode::INVALID_INTERVAL,
                             "Timer interval must be positive");
    }

    interval_ = milliseconds;
}

auto Timer::interval() const noexcept -> int64_t { return interval_.load(); }

void Timer::setPrecisionMode(PrecisionMode mode) noexcept {
    precision_mode_ = mode;
}

auto Timer::precisionMode() const noexcept -> PrecisionMode {
    return precision_mode_.load();
}

void Timer::setSingleShot(bool singleShot) noexcept {
    is_single_shot_ = singleShot;
}

auto Timer::isSingleShot() const noexcept -> bool {
    return is_single_shot_.load();
}

auto Timer::isActive() const noexcept -> bool { return is_active_.load(); }

void Timer::start() {
    std::lock_guard<std::mutex> lock(timer_mutex_);

    if (interval_.load() <= 0) {
        throw TimerException(TimerException::ErrorCode::INVALID_INTERVAL,
                             "Cannot start timer with non-positive interval");
    }

    if (!callback_) {
        throw TimerException(
            TimerException::ErrorCode::CALLBACK_EXECUTION_ERROR,
            "Cannot start timer without callback function");
    }

    if (is_active_) {
        should_stop_ = true;
        if (timer_thread_.joinable()) {
            timer_thread_.join();
        }
    }

    should_stop_ = false;
    is_active_ = true;

    next_timeout_ = Clock::now() + std::chrono::milliseconds(interval_);

    try {
        timer_thread_ = std::thread(&Timer::timerLoop, this);
    } catch (const std::system_error& e) {
        is_active_ = false;
        next_timeout_.reset();
        throw TimerException(
            TimerException::ErrorCode::THREAD_CREATION_ERROR,
            std::string("Failed to create timer thread: ") + e.what());
    }
}

void Timer::start(int64_t milliseconds) {
    setInterval(milliseconds);
    start();
}

void Timer::stop() {
    std::lock_guard<std::mutex> lock(timer_mutex_);

    if (!is_active_) {
        return;
    }

    should_stop_ = true;
    is_active_ = false;

    if (timer_thread_.joinable()) {
        try {
            timer_thread_.join();
        } catch (const std::system_error& e) {
            THROW_RUNTIME_ERROR(std::string("Failed to join timer thread: ") +
                                e.what());
        }
    }

    next_timeout_.reset();
}

auto Timer::singleShot(int64_t milliseconds, Callback callback,
                       PrecisionMode mode) -> std::shared_ptr<Timer> {
    if (milliseconds <= 0) {
        throw TimerException(TimerException::ErrorCode::INVALID_INTERVAL,
                             "Timer interval must be positive");
    }

    auto timer = std::make_shared<Timer>(std::move(callback));
    timer->setPrecisionMode(mode);
    timer->setSingleShot(true);
    timer->setInterval(milliseconds);
    timer->start();

    return timer;
}

auto Timer::remainingTime() const -> int64_t {
    std::lock_guard<std::mutex> lock(timer_mutex_);

    if (!is_active_ || !next_timeout_.has_value()) {
        return 0;
    }

    auto now = Clock::now();
    if (now >= next_timeout_.value()) {
        return 0;
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(
               next_timeout_.value() - now)
        .count();
}

void Timer::timerLoop() {
    constexpr auto MIN_SLEEP = std::chrono::milliseconds(1);
    constexpr auto COARSE_SLEEP = std::chrono::milliseconds(15);

    while (!should_stop_.load()) {
        auto now = Clock::now();

        if (next_timeout_.has_value() && now >= next_timeout_.value()) {
            try {
                callback_();
            } catch (...) {
            }

            if (is_single_shot_.load()) {
                is_active_ = false;
                break;
            }

            next_timeout_ = now + std::chrono::milliseconds(interval_.load());
        }

        if (precision_mode_ == PrecisionMode::PRECISE) {
            std::this_thread::sleep_for(MIN_SLEEP);
        } else {
            auto sleep_time =
                std::min(COARSE_SLEEP,
                         std::chrono::duration_cast<std::chrono::milliseconds>(
                             next_timeout_.value_or(now + COARSE_SLEEP) - now));

            if (sleep_time.count() > 0) {
                std::this_thread::sleep_for(sleep_time);
            } else {
                std::this_thread::sleep_for(MIN_SLEEP);
            }
        }
    }
}

}  // namespace atom::utils