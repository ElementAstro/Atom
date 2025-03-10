#include "rate_limiter.hpp"

#include <thread>

namespace atom::extra::curl {
RateLimiter::RateLimiter(double requests_per_second)
    : requests_per_second_(requests_per_second),
      min_delay_(std::chrono::microseconds(
          static_cast<int64_t>(1000000 / requests_per_second))),
      last_request_time_(std::chrono::steady_clock::now()) {}

void RateLimiter::wait() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - last_request_time_;

    if (elapsed < min_delay_) {
        auto delay = min_delay_ - elapsed;
        std::this_thread::sleep_for(delay);
    }

    last_request_time_ = std::chrono::steady_clock::now();
}

void RateLimiter::set_rate(double requests_per_second) {
    std::lock_guard<std::mutex> lock(mutex_);
    requests_per_second_ = requests_per_second;
    min_delay_ = std::chrono::microseconds(
        static_cast<int64_t>(1000000 / requests_per_second));
}
}  // namespace atom::extra::curl
