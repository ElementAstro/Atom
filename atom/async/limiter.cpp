#include "limiter.hpp"

#include <algorithm>
#include <execution>
#include <format>
#include <stdexcept>
#include <string_view>

#include "atom/log/loguru.hpp"

namespace atom::async {

RateLimiter::Settings::Settings(size_t max_requests,
                                std::chrono::seconds time_window)
    : maxRequests(max_requests), timeWindow(time_window) {
    if (max_requests == 0) {
        throw std::invalid_argument("max_requests must be greater than 0");
    }
    if (time_window.count() <= 0) {
        throw std::invalid_argument("time_window must be greater than 0");
    }
    LOG_F(INFO, "Settings created: max_requests=%zu, time_window=%lld seconds",
          max_requests, time_window.count());
}

// Implementation of RateLimiter constructor
RateLimiter::RateLimiter() noexcept { LOG_F(INFO, "RateLimiter created"); }

// Destructor
RateLimiter::~RateLimiter() noexcept {
    try {
        std::unique_lock lock(mutex_);
        // 确保所有等待的协程都被唤醒以避免悬挂协程
        for (auto& [_, waiters_queue] : waiters_) {
            while (!waiters_queue.empty()) {
                auto handle = waiters_queue.front();
                waiters_queue.pop_front();
                lock.unlock();
                handle.resume();
                lock.lock();
            }
        }
    } catch (...) {
        // 确保析构函数不抛出异常
    }
}

// 移动构造
RateLimiter::RateLimiter(RateLimiter&& other) noexcept
    : paused_(other.paused_.load()) {
    std::unique_lock lock(other.mutex_);
    settings_ = std::move(other.settings_);
    requests_ = std::move(other.requests_);
    waiters_ = std::move(other.waiters_);
    log_ = std::move(other.log_);

    // 移动原子对象
    for (const auto& [name, count] : other.rejected_requests_) {
        rejected_requests_[name].store(count.load());
    }
    other.rejected_requests_.clear();
}

// 移动赋值
RateLimiter& RateLimiter::operator=(RateLimiter&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(mutex_, other.mutex_);
        settings_ = std::move(other.settings_);
        requests_ = std::move(other.requests_);
        waiters_ = std::move(other.waiters_);
        log_ = std::move(other.log_);
        paused_.store(other.paused_.load());

        // 移动原子对象
        rejected_requests_.clear();
        for (const auto& [name, count] : other.rejected_requests_) {
            rejected_requests_[name].store(count.load());
        }
        other.rejected_requests_.clear();
    }
    return *this;
}

// Implementation of Awaiter constructor
RateLimiter::Awaiter::Awaiter(RateLimiter& limiter,
                              std::string function_name) noexcept
    : limiter_(limiter), function_name_(std::move(function_name)) {
    LOG_F(INFO, "Awaiter created for function: %s", function_name_.c_str());
}

// Implementation of Awaiter::await_ready
auto RateLimiter::Awaiter::await_ready() const noexcept -> bool {
    LOG_F(INFO, "Awaiter::await_ready called for function: %s",
          function_name_.c_str());
    return false;
}

// Implementation of Awaiter::await_suspend
void RateLimiter::Awaiter::await_suspend(std::coroutine_handle<> handle) {
    LOG_F(INFO, "Awaiter::await_suspend called for function: %s",
          function_name_.c_str());

    try {
        std::unique_lock<std::shared_mutex> lock(limiter_.mutex_);

        // 确保设置已存在
        if (!limiter_.settings_.contains(function_name_)) {
            // 如果没有设置，使用默认值
            limiter_.settings_[function_name_] = Settings();
        }

        auto& settings = limiter_.settings_[function_name_];
        limiter_.cleanup(function_name_, settings.timeWindow);

        if (limiter_.paused_.load(std::memory_order_acquire) ||
            limiter_.requests_[function_name_].size() >= settings.maxRequests) {
            limiter_.waiters_[function_name_].emplace_back(handle);
            limiter_.rejected_requests_[function_name_].fetch_add(
                1, std::memory_order_relaxed);
            was_rejected_ = true;
            LOG_F(WARNING,
                  "Request for function %s rejected. Total rejected: %zu",
                  function_name_.c_str(),
                  limiter_.rejected_requests_[function_name_].load());
        } else {
            limiter_.requests_[function_name_].emplace_back(
                std::chrono::steady_clock::now());
            was_rejected_ = false;
            lock.unlock();
            LOG_F(INFO, "Request for function %s accepted",
                  function_name_.c_str());
            handle.resume();
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in await_suspend: %s", e.what());
        handle.resume();  // 确保协程继续执行
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in await_suspend");
        handle.resume();  // 确保协程继续执行
    }
}

// Implementation of Awaiter::await_resume
void RateLimiter::Awaiter::await_resume() {
    LOG_F(INFO, "Awaiter::await_resume called for function: %s",
          function_name_.c_str());
    if (was_rejected_) {
        throw RateLimitExceededException(std::format(
            "Rate limit exceeded for function: {}", function_name_));
    }
}

// Implementation of RateLimiter::acquire
RateLimiter::Awaiter RateLimiter::acquire(std::string_view function_name) {
    LOG_F(INFO, "RateLimiter::acquire called for function: %s",
          function_name.data());
    return Awaiter(*this, std::string(function_name));
}

// Implementation of RateLimiter::setFunctionLimit
void RateLimiter::setFunctionLimit(std::string_view function_name,
                                   size_t max_requests,
                                   std::chrono::seconds time_window) {
    if (max_requests == 0) {
        throw std::invalid_argument("max_requests must be greater than 0");
    }
    if (time_window.count() <= 0) {
        throw std::invalid_argument("time_window must be greater than 0");
    }

    LOG_F(INFO,
          "RateLimiter::setFunctionLimit called for function: %s, "
          "max_requests=%zu, time_window=%lld seconds",
          function_name.data(), max_requests, time_window.count());

    std::unique_lock<std::shared_mutex> lock(mutex_);
    settings_[std::string(function_name)] = Settings(max_requests, time_window);
}

// Implementation of RateLimiter::pause
void RateLimiter::pause() noexcept {
    LOG_F(INFO, "RateLimiter::pause called");
    paused_.store(true, std::memory_order_release);
}

// Implementation of RateLimiter::resume
void RateLimiter::resume() {
    LOG_F(INFO, "RateLimiter::resume called");
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        paused_.store(false, std::memory_order_release);
        processWaiters();
    }
}

// Implementation of RateLimiter::printLog
void RateLimiter::printLog() const noexcept {
#if ENABLE_DEBUG
    LOG_F(INFO, "RateLimiter::printLog called");
    std::shared_lock<std::shared_mutex> lock(mutex_);
    for (const auto& [function_name, timestamps] : log_) {
        std::cout << "Request log for " << function_name << ":\n";
        for (const auto& timestamp : timestamps) {
            std::cout << "Request at " << timestamp.time_since_epoch().count()
                      << std::endl;
        }
    }
#endif
}

// Implementation of RateLimiter::getRejectedRequests
auto RateLimiter::getRejectedRequests(
    std::string_view function_name) const noexcept -> size_t {
    LOG_F(INFO, "RateLimiter::getRejectedRequests called for function: %s",
          function_name.data());

    std::shared_lock<std::shared_mutex> lock(mutex_);
    if (auto it = rejected_requests_.find(std::string(function_name));
        it != rejected_requests_.end()) {
        return it->second.load(std::memory_order_relaxed);
    }
    return 0;
}

// Implementation of RateLimiter::cleanup
void RateLimiter::cleanup(std::string_view function_name,
                          const std::chrono::seconds& time_window) {
    LOG_F(INFO,
          "RateLimiter::cleanup called for function: %s, time_window=%lld "
          "seconds",
          function_name.data(), time_window.count());

    auto now = std::chrono::steady_clock::now();
    auto& reqs = requests_[std::string(function_name)];

    // 使用移除-擦除习惯用法更高效地清理
    auto cutoff_time = now - time_window;
    reqs.erase(std::ranges::remove_if(reqs,
                                      [&cutoff_time](const auto& time) {
                                          return time < cutoff_time;
                                      })
                   .begin(),
               reqs.end());
}

// Implementation of RateLimiter::processWaiters
void RateLimiter::processWaiters() {
    LOG_F(INFO, "RateLimiter::processWaiters called");

    // 创建临时存储处理的waiter
    std::vector<std::pair<std::string, std::coroutine_handle<>>>
        waiters_to_process;

    // 获取需要处理的waiters
    for (auto& [function_name, wait_queue] : waiters_) {
        if (wait_queue.empty())
            continue;

        auto& settings = settings_[function_name];
        while (!wait_queue.empty() &&
               requests_[function_name].size() < settings.maxRequests) {
            auto waiter = wait_queue.front();
            wait_queue.pop_front();
            requests_[function_name].emplace_back(
                std::chrono::steady_clock::now());
            waiters_to_process.emplace_back(function_name, waiter);
        }
    }

    // 释放锁后再处理waiters，避免锁争用
    if (!waiters_to_process.empty()) {
        mutex_.unlock();
        // 可以使用并行处理提升性能
        std::for_each(std::execution::par_unseq, waiters_to_process.begin(),
                      waiters_to_process.end(), [](const auto& pair) {
                          LOG_F(INFO, "Resuming waiter for function: %s",
                                pair.first.c_str());
                          pair.second.resume();
                      });
        mutex_.lock();
    }
}

}  // namespace atom::async
