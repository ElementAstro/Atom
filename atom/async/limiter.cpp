#include "limiter.hpp"

#include <algorithm>
#include <execution>
#include <format>
#include <ranges>
#include <string_view>

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

namespace atom::async {

RateLimiter::Settings::Settings(size_t max_requests,
                                std::chrono::seconds time_window)
    : maxRequests(max_requests), timeWindow(time_window) {
    if (max_requests == 0) {
        THROW_INVALID_ARGUMENT("max_requests must be greater than 0");
    }
    if (time_window.count() <= 0) {
        THROW_INVALID_ARGUMENT("time_window must be greater than 0");
    }
    LOG_F(INFO, "Settings created: max_requests={}, time_window={} seconds",
          max_requests, time_window.count());
}

// Implementation of RateLimiter constructor
RateLimiter::RateLimiter() noexcept {
    LOG_F(INFO, "RateLimiter created");

    // Initialize platform-specific synchronization primitives
#ifdef ATOM_PLATFORM_WINDOWS
    InitializeCriticalSection(&resumeLock_);
    InitializeConditionVariable(&resumeCondition_);
#elif defined(ATOM_PLATFORM_LINUX)
    sem_init(&resumeSemaphore_, 0, 0);
#endif
}

// Destructor
RateLimiter::~RateLimiter() noexcept {
    try {
#ifdef ATOM_USE_BOOST_LOCKFREE
        std::unique_lock lock(mutex_);
        // Resume all waiting coroutines to avoid hanging
        for (auto& [name, queue] : waiters_) {
            std::coroutine_handle<> handle;
            while (!queue.empty() && queue.pop(handle)) {
                lock.unlock();
                handle.resume();
                lock.lock();
            }
        }
#else
        std::unique_lock lock(mutex_);
        // Ensure all waiting coroutines are resumed to avoid hanging coroutines
        for (auto& [_, waiters_queue] : waiters_) {
            while (!waiters_queue.empty()) {
                auto handle = waiters_queue.front();
                waiters_queue.pop_front();
                lock.unlock();
                handle.resume();
                lock.lock();
            }
        }
#endif

        // Clean up platform-specific resources
#ifdef ATOM_PLATFORM_WINDOWS
        DeleteCriticalSection(&resumeLock_);
#elif defined(ATOM_PLATFORM_LINUX)
        sem_destroy(&resumeSemaphore_);
#endif
    } catch (...) {
        // Ensure destructor does not throw exceptions
    }
}

// Move constructor
RateLimiter::RateLimiter(RateLimiter&& other) noexcept
    : paused_(other.paused_.load()) {
    std::unique_lock lock(other.mutex_);
    settings_ = std::move(other.settings_);
#ifdef ATOM_USE_BOOST_LOCKFREE
    requests_ = std::move(other.requests_);
    waiters_ = std::move(other.waiters_);
#else
    requests_ = std::move(other.requests_);
    waiters_ = std::move(other.waiters_);
#endif
    log_ = std::move(other.log_);

    // Move atomic objects
    for (const auto& [name, count] : other.rejected_requests_) {
        rejected_requests_[name].store(count.load());
    }
    other.rejected_requests_.clear();

    // Move platform-specific resources
#ifdef ATOM_PLATFORM_WINDOWS
    std::swap(resumeCondition_, other.resumeCondition_);
    std::swap(resumeLock_, other.resumeLock_);
#elif defined(ATOM_PLATFORM_LINUX)
    waitersReady_.store(other.waitersReady_.load());
    other.waitersReady_.store(0);
    // Semaphore needs reinitialization
    sem_destroy(&other.resumeSemaphore_);
    sem_init(&resumeSemaphore_, 0, 0);
#endif
}

// Move assignment operator
RateLimiter& RateLimiter::operator=(RateLimiter&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(mutex_, other.mutex_);
        settings_ = std::move(other.settings_);
#ifdef ATOM_USE_BOOST_LOCKFREE
        requests_ = std::move(other.requests_);
        waiters_ = std::move(other.waiters_);
#else
        requests_ = std::move(other.requests_);
        waiters_ = std::move(other.waiters_);
#endif
        log_ = std::move(other.log_);
        paused_.store(other.paused_.load());

        // Move atomic objects
        rejected_requests_.clear();
        for (const auto& [name, count] : other.rejected_requests_) {
            rejected_requests_[name].store(count.load());
        }
        other.rejected_requests_.clear();

        // Move platform-specific resources
#ifdef ATOM_PLATFORM_WINDOWS
        std::swap(resumeCondition_, other.resumeCondition_);
        std::swap(resumeLock_, other.resumeLock_);
#elif defined(ATOM_PLATFORM_LINUX)
        waitersReady_.store(other.waitersReady_.load());
        other.waitersReady_.store(0);
        // Semaphore needs reinitialization
        sem_destroy(&resumeSemaphore_);
        sem_init(&resumeSemaphore_, 0, 0);
        sem_destroy(&other.resumeSemaphore_);
        sem_init(&other.resumeSemaphore_, 0, 0);
#endif
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

        // Ensure settings exist
        if (!limiter_.settings_.contains(function_name_)) {
            // Use default settings if none exist
            limiter_.settings_[function_name_] = Settings();
        }

        auto& settings = limiter_.settings_[function_name_];
        limiter_.cleanup(function_name_, settings.timeWindow);

#ifdef ATOM_USE_BOOST_LOCKFREE
        // Use lockfree implementation
        if (limiter_.paused_.load(std::memory_order_acquire) ||
            limiter_.requests_[function_name_].size_approx() >=
                settings.maxRequests) {
            limiter_.waiters_[function_name_].push(handle);
            limiter_.rejected_requests_[function_name_].fetch_add(
                1, std::memory_order_relaxed);
            was_rejected_ = true;

            // Platform-specific optimization
#ifdef ATOM_PLATFORM_LINUX
            limiter_.waitersReady_.fetch_add(1, std::memory_order_relaxed);
#endif

            LOG_F(WARNING,
                  "Request for function %s rejected. Total rejected: {}",
                  function_name_.c_str(),
                  limiter_.rejected_requests_[function_name_].load());
        } else {
            limiter_.requests_[function_name_].push(
                std::chrono::steady_clock::now());
            was_rejected_ = false;
            lock.unlock();
            LOG_F(INFO, "Request for function %s accepted",
                  function_name_.c_str());
            handle.resume();
        }
#else
        if (limiter_.paused_.load(std::memory_order_acquire) ||
            limiter_.requests_[function_name_].size() >= settings.maxRequests) {
            limiter_.waiters_[function_name_].emplace_back(handle);
            limiter_.rejected_requests_[function_name_].fetch_add(
                1, std::memory_order_relaxed);
            was_rejected_ = true;

            // Platform-specific optimization
#ifdef ATOM_PLATFORM_LINUX
            limiter_.waitersReady_.fetch_add(1, std::memory_order_relaxed);
#endif

            LOG_F(WARNING,
                  "Request for function %s rejected. Total rejected: {}",
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
#endif
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Exception in await_suspend: %s", e.what());
        handle.resume();  // Ensure coroutine continues execution
    } catch (...) {
        LOG_F(ERROR, "Unknown exception in await_suspend");
        handle.resume();  // Ensure coroutine continues execution
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
        THROW_INVALID_ARGUMENT("max_requests must be greater than 0");
    }
    if (time_window.count() <= 0) {
        THROW_INVALID_ARGUMENT("time_window must be greater than 0");
    }

    LOG_F(INFO,
          "RateLimiter::setFunctionLimit called for function: %s, "
          "max_requests={}, time_window={} seconds",
          function_name.data(), max_requests, time_window.count());

    std::unique_lock<std::shared_mutex> lock(mutex_);
    settings_[std::string(function_name)] = Settings(max_requests, time_window);
}

// Implementation of RateLimiter::setFunctionLimits - New
void RateLimiter::setFunctionLimits(
    std::span<const std::pair<std::string_view, Settings>> settings) {
    LOG_F(INFO, "RateLimiter::setFunctionLimits called with {} settings",
          settings.size());

    std::unique_lock<std::shared_mutex> lock(mutex_);

    for (const auto& [function_name, setting] : settings) {
        if (setting.maxRequests == 0) {
            THROW_INVALID_ARGUMENT(std::format(
                "max_requests must be greater than 0 for function: {}",
                function_name));
        }
        if (setting.timeWindow.count() <= 0) {
            THROW_INVALID_ARGUMENT(std::format(
                "time_window must be greater than 0 for function: {}",
                function_name));
        }

        settings_[std::string(function_name)] = setting;

        LOG_F(INFO,
              "Set limit for function: %s, max_requests={}, time_window={} "
              "seconds",
              std::string(function_name).c_str(), setting.maxRequests,
              setting.timeWindow.count());
    }
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

        // Selectively use platform-specific optimizations
#if defined(ATOM_PLATFORM_WINDOWS)
        lock.unlock();
        optimizedProcessWaiters();
#elif defined(ATOM_PLATFORM_MACOS)
        lock.unlock();
        optimizedProcessWaiters();
#elif defined(ATOM_PLATFORM_LINUX)
        lock.unlock();
        optimizedProcessWaiters();
#else
        processWaiters();
#endif
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

// Implementation of RateLimiter::resetFunction - New
void RateLimiter::resetFunction(std::string_view function_name) {
    LOG_F(INFO, "RateLimiter::resetFunction called for function: %s",
          function_name.data());

    std::unique_lock<std::shared_mutex> lock(mutex_);

    std::string func_name(function_name);

    // Clear request records
#ifdef ATOM_USE_BOOST_LOCKFREE
    if (auto it = requests_.find(func_name); it != requests_.end()) {
        it->second.clear();
    }
#else
    if (auto it = requests_.find(func_name); it != requests_.end()) {
        it->second.clear();
    }
#endif

    // Reset rejection count
    if (auto it = rejected_requests_.find(func_name);
        it != rejected_requests_.end()) {
        it->second.store(0, std::memory_order_relaxed);
    }

    LOG_F(INFO, "Reset completed for function: %s", function_name.data());
}

// Implementation of RateLimiter::resetAll - New
void RateLimiter::resetAll() noexcept {
    LOG_F(INFO, "RateLimiter::resetAll called");

    std::unique_lock<std::shared_mutex> lock(mutex_);

    // Clear all request records
#ifdef ATOM_USE_BOOST_LOCKFREE
    for (auto& [_, queue] : requests_) {
        queue.clear();
    }
#else
    for (auto& [_, queue] : requests_) {
        queue.clear();
    }
#endif

    // Reset all rejection counts
    for (auto& [_, count] : rejected_requests_) {
        count.store(0, std::memory_order_relaxed);
    }

    LOG_F(INFO, "All rate limits have been reset");
}

// Implementation of RateLimiter::cleanup
void RateLimiter::cleanup(std::string_view function_name,
                          const std::chrono::seconds& time_window) {
    LOG_F(INFO,
          "RateLimiter::cleanup called for function: %s, time_window={} "
          "seconds",
          function_name.data(), time_window.count());

    auto now = std::chrono::steady_clock::now();
    auto cutoff_time = now - time_window;

#ifdef ATOM_USE_BOOST_LOCKFREE
    // For lockfree implementation, we need to create a new queue and transfer
    // valid items
    LockfreeRequestQueue new_queue;
    auto& current_queue = requests_[std::string(function_name)];

    // Move valid timestamps to the new queue
    std::chrono::steady_clock::time_point timestamp;
    while (!current_queue.empty() && current_queue.pop(timestamp)) {
        if (timestamp >= cutoff_time) {
            new_queue.push(timestamp);
        }
    }

    // Replace the old queue with the new one
    current_queue = std::move(new_queue);
#else
    auto& reqs = requests_[std::string(function_name)];

    // C++20 ranges and views for more concise filtering
    auto is_expired = [&cutoff_time](const auto& time) {
        return time < cutoff_time;
    };

    reqs.erase(std::ranges::remove_if(reqs, is_expired).begin(), reqs.end());
#endif
}

// Implementation of RateLimiter::processWaiters
void RateLimiter::processWaiters() {
    LOG_F(INFO, "RateLimiter::processWaiters called");

#ifdef ATOM_USE_BOOST_LOCKFREE
    // Create temporary storage for waiters to process
    std::vector<std::pair<std::string, std::coroutine_handle<>>>
        waiters_to_process;

    // Identify waiters that can be processed
    for (auto& [function_name, wait_queue] : waiters_) {
        if (wait_queue.empty())
            continue;

        auto& settings = settings_[function_name];
        auto& req_queue = requests_[function_name];

        // Process as many waiters as possible according to rate limits
        while (!wait_queue.empty() &&
               req_queue.size_approx() < settings.maxRequests) {
            std::coroutine_handle<> handle;
            if (wait_queue.pop(handle)) {
                req_queue.push(std::chrono::steady_clock::now());
                waiters_to_process.emplace_back(function_name, handle);
            }
        }
    }

    // Release lock before resuming waiters
    if (!waiters_to_process.empty()) {
        mutex_.unlock();
        // Use C++20 parallel algorithms for better performance
        std::for_each(std::execution::par_unseq, waiters_to_process.begin(),
                      waiters_to_process.end(), [](const auto& pair) {
                          LOG_F(INFO, "Resuming waiter for function: %s",
                                pair.first.c_str());
                          pair.second.resume();
                      });
        mutex_.lock();
    }
#else
    // Create temporary storage for waiters to process
    std::vector<std::pair<std::string, std::coroutine_handle<>>>
        waiters_to_process;

    // Get waiters that need processing
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

    // Release lock before processing waiters to avoid lock contention
    if (!waiters_to_process.empty()) {
        mutex_.unlock();
        // Use C++20 parallel algorithms for better performance
        std::for_each(std::execution::par_unseq, waiters_to_process.begin(),
                      waiters_to_process.end(), [](const auto& pair) {
                          LOG_F(INFO, "Resuming waiter for function: %s",
                                pair.first.c_str());
                          pair.second.resume();
                      });
        mutex_.lock();
    }
#endif
}

// Windows platform-specific optimized implementation
#ifdef ATOM_PLATFORM_WINDOWS
void RateLimiter::optimizedProcessWaiters() {
    LOG_F(INFO, "Using Windows-optimized processWaiters");

    EnterCriticalSection(&resumeLock_);

    // Create temporary storage for waiters to process
    std::vector<std::pair<std::string, std::coroutine_handle<>>>
        waiters_to_process;

    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        // Standard processing logic, same as processWaiters
#ifdef ATOM_USE_BOOST_LOCKFREE
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty())
                continue;

            auto& settings = settings_[function_name];
            auto& req_queue = requests_[function_name];

            while (!wait_queue.empty() &&
                   req_queue.size_approx() < settings.maxRequests) {
                std::coroutine_handle<> handle;
                if (wait_queue.pop(handle)) {
                    req_queue.push(std::chrono::steady_clock::now());
                    waiters_to_process.emplace_back(function_name, handle);
                }
            }
        }
#else
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
#endif
    }

    // Use Windows thread pool instead of std::for_each to process waiters
    if (!waiters_to_process.empty()) {
        struct ResumeInfo {
            std::string function_name;
            std::coroutine_handle<> handle;
        };

        for (const auto& [function_name, handle] : waiters_to_process) {
            auto* info = new ResumeInfo{function_name, handle};

            // Use Windows thread pool API
            if (!QueueUserWorkItem(
                    [](PVOID context) -> DWORD {
                        auto* info = static_cast<ResumeInfo*>(context);
                        LOG_F(INFO, "Resuming waiter for function: %s",
                              info->function_name.c_str());
                        info->handle.resume();
                        delete info;
                        return 0;
                    },
                    info, WT_EXECUTEDEFAULT)) {
                // If queuing fails, fall back to synchronous execution
                LOG_F(WARNING,
                      "Failed to queue work item, executing synchronously");
                LOG_F(INFO, "Resuming waiter for function: %s",
                      info->function_name.c_str());
                info->handle.resume();
                delete info;
            }
        }
    }

    LeaveCriticalSection(&resumeLock_);
}
#endif

// macOS platform-specific optimized implementation
#ifdef ATOM_PLATFORM_MACOS
void RateLimiter::optimizedProcessWaiters() {
    LOG_F(INFO, "Using macOS-optimized processWaiters");

    // Create temporary storage for waiters to process
    std::vector<std::pair<std::string, std::coroutine_handle<>>>
        waiters_to_process;

    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        // Standard processing logic, same as processWaiters
#ifdef ATOM_USE_BOOST_LOCKFREE
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty())
                continue;

            auto& settings = settings_[function_name];
            auto& req_queue = requests_[function_name];

            while (!wait_queue.empty() &&
                   req_queue.size_approx() < settings.maxRequests) {
                std::coroutine_handle<> handle;
                if (wait_queue.pop(handle)) {
                    req_queue.push(std::chrono::steady_clock::now());
                    waiters_to_process.emplace_back(function_name, handle);
                }
            }
        }
#else
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
#endif
    }

    // Use GCD instead of std::for_each to process waiters
    if (!waiters_to_process.empty()) {
        // Create a concurrent dispatch queue
        dispatch_queue_t queue =
            dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);

        // Create a group to track all tasks
        dispatch_group_t group = dispatch_group_create();

        for (const auto& [function_name, handle] : waiters_to_process) {
            // Copy data for each waiter to use safely in the block
            auto fname_copy = function_name;
            auto handle_copy = handle;

            dispatch_group_async(group, queue, ^{
                LOG_F(INFO, "Resuming waiter for function: %s",
                      fname_copy.c_str());
                handle_copy.resume();
            });
        }

        // Wait for all tasks to complete
        dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
        dispatch_release(group);
    }
}
#endif

// Linux platform-specific optimized implementation
#ifdef ATOM_PLATFORM_LINUX
void RateLimiter::optimizedProcessWaiters() {
    LOG_F(INFO, "Using Linux-optimized processWaiters");

    int expected_waiters = waitersReady_.load(std::memory_order_relaxed);
    if (expected_waiters <= 0) {
        return;  // No waiters, return directly
    }

    // Create temporary storage for waiters to process
    std::vector<std::pair<std::string, std::coroutine_handle<>>>
        waiters_to_process;

    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

        // Standard processing logic, same as processWaiters
#ifdef ATOM_USE_BOOST_LOCKFREE
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty())
                continue;

            auto& settings = settings_[function_name];
            auto& req_queue = requests_[function_name];

            while (!wait_queue.empty() &&
                   req_queue.size_approx() < settings.maxRequests) {
                std::coroutine_handle<> handle;
                if (wait_queue.pop(handle)) {
                    req_queue.push(std::chrono::steady_clock::now());
                    waiters_to_process.emplace_back(function_name, handle);
                    waitersReady_.fetch_sub(1, std::memory_order_relaxed);
                }
            }
        }
#else
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
                waitersReady_.fetch_sub(1, std::memory_order_relaxed);
            }
        }
#endif
    }

    // Use POSIX threads to process waiters
    if (!waiters_to_process.empty()) {
        struct ResumeThreadArg {
            std::string function_name;
            std::coroutine_handle<> handle;
        };

        std::vector<pthread_t> threads;
        threads.reserve(waiters_to_process.size());

        for (const auto& [function_name, handle] : waiters_to_process) {
            auto* arg = new ResumeThreadArg{function_name, handle};

            pthread_t thread;
            if (pthread_create(
                    &thread, nullptr,
                    [](void* arg) -> void* {
                        auto* data = static_cast<ResumeThreadArg*>(arg);
                        LOG_F(INFO, "Resuming waiter for function: %s",
                              data->function_name.c_str());
                        data->handle.resume();
                        delete data;
                        return nullptr;
                    },
                    arg) == 0) {
                threads.push_back(thread);
            } else {
                // If thread creation fails, fall back to synchronous execution
                LOG_F(WARNING,
                      "Failed to create thread, executing synchronously");
                LOG_F(INFO, "Resuming waiter for function: %s",
                      arg->function_name.c_str());
                arg->handle.resume();
                delete arg;
            }
        }

        // Detach all threads
        for (auto thread : threads) {
            pthread_detach(thread);
        }
    }
}
#endif

}  // namespace atom::async
