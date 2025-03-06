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
        // In case of any unexpected error, return 0
        return 0;
    }
}

auto ElapsedTimer::operator<=>(const ElapsedTimer& other) const noexcept
    -> std::strong_ordering {
    // If both timers are invalid, they are considered equal
    if (!isValid() && !other.isValid()) {
        return std::strong_ordering::equal;
    }

    // An invalid timer is considered less than a valid one
    if (!isValid()) {
        return std::strong_ordering::less;
    }

    if (!other.isValid()) {
        return std::strong_ordering::greater;
    }

    // Compare the actual start times
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
    // Two timers are equal if they are both invalid or have the same start time
    if (!isValid() && !other.isValid()) {
        return true;
    }

    if (isValid() != other.isValid()) {
        return false;
    }

    return start_time_.value() == other.start_time_.value();
}

}  // namespace atom::utils
