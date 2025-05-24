#include "limiter.hpp"

#include <algorithm>
#include <chrono> // Required for time types
#include <coroutine> // Required for std::coroutine_handle
#include <execution>
#include <format>
#include <ranges>
#include <string_view>
#include <vector> // Required for std::vector

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

#ifdef ATOM_USE_ASIO
#include <asio/post.hpp>
#include <asio/thread_pool.hpp>
#include <thread> // For std::thread::hardware_concurrency
#endif

#ifdef ATOM_PLATFORM_WINDOWS
#include <windows.h>
#endif

#ifdef ATOM_PLATFORM_MACOS
#include <dispatch/dispatch.h>
#endif

#ifdef ATOM_PLATFORM_LINUX
#include <pthread.h>
#include <semaphore.h>
#endif

namespace atom::async {
// Implementation of RateLimiter constructor
RateLimiter::RateLimiter() noexcept
#ifdef ATOM_USE_ASIO
    // Initialize Asio thread pool with a number of threads equal to hardware concurrency, or 1 if not determinable.
    : asio_pool_( (std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 1) )
#endif
{
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

#ifdef ATOM_USE_ASIO
        asio_pool_.join(); // Wait for all Asio tasks to complete
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
    : paused_(other.paused_.load())
#ifdef ATOM_USE_ASIO
    // The asio_pool_ member will be default-initialized for the new object,
    // or initialized as per its member initializer in this class's constructor.
    // other.asio_pool_ will be joined upon other's destruction.
    , asio_pool_( (std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 1) )
#endif
{
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
        // Note: this->asio_pool_ is not affected by the move of other members.
        // It continues to exist and operate. other.asio_pool_ will be joined on other's destruction.
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
    return false; // Always suspend to check rate limit
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

            // Platform-specific optimization for Linux native path
#if defined(ATOM_PLATFORM_LINUX) && !defined(ATOM_USE_ASIO)
            limiter_.waitersReady_.fetch_add(1, std::memory_order_relaxed);
#endif

            LOG_F(WARNING,
                  "Request for function %s rejected. Total rejected: %zu",
                  function_name_.c_str(),
                  limiter_.rejected_requests_[function_name_].load(std::memory_order_relaxed));
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

            // Platform-specific optimization for Linux native path
#if defined(ATOM_PLATFORM_LINUX) && !defined(ATOM_USE_ASIO)
            limiter_.waitersReady_.fetch_add(1, std::memory_order_relaxed);
#endif

            LOG_F(WARNING,
                  "Request for function %s rejected. Total rejected: %zu",
                  function_name_.c_str(),
                  limiter_.rejected_requests_[function_name_].load(std::memory_order_relaxed));
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
          std::string(function_name).c_str()); // Ensure null-terminated for log
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
        THROW_INVALID_ARGUMENT("time_window must be greater than 0 seconds");
    }

    LOG_F(INFO,
          "RateLimiter::setFunctionLimit called for function: %s, "
          "max_requests=%zu, time_window=%lld seconds",
          std::string(function_name).c_str(), max_requests, (long long)time_window.count());

    std::unique_lock<std::shared_mutex> lock(mutex_);
    settings_[std::string(function_name)] = Settings(max_requests, time_window);
}

// Implementation of RateLimiter::setFunctionLimits
void RateLimiter::setFunctionLimits(
    std::span<const std::pair<std::string_view, Settings>> settings_list) {
    LOG_F(INFO, "RateLimiter::setFunctionLimits called with %zu settings",
          settings_list.size());

    std::unique_lock<std::shared_mutex> lock(mutex_);

    for (const auto& [function_name_sv, setting] : settings_list) {
        std::string function_name_str(function_name_sv);
        if (setting.maxRequests == 0) {
            THROW_INVALID_ARGUMENT(std::format(
                "max_requests must be greater than 0 for function: {}",
                function_name_str));
        }
        if (setting.timeWindow.count() <= 0) {
            THROW_INVALID_ARGUMENT(std::format(
                "time_window must be greater than 0 seconds for function: {}",
                function_name_str));
        }

        settings_[function_name_str] = setting;

        LOG_F(INFO,
              "Set limit for function: %s, max_requests=%zu, time_window=%lld "
              "seconds",
              function_name_str.c_str(), setting.maxRequests,
              (long long)setting.timeWindow.count());
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
        lock.unlock(); // Release mutex before calling processing functions

        // Selectively use platform-specific or Asio optimizations
#if defined(ATOM_USE_ASIO)
        asioProcessWaiters();
#elif defined(ATOM_PLATFORM_WINDOWS)
        optimizedProcessWaiters();
#elif defined(ATOM_PLATFORM_MACOS)
        optimizedProcessWaiters();
#elif defined(ATOM_PLATFORM_LINUX)
        optimizedProcessWaiters();
#else
        processWaiters(); // Generic version
#endif
    }
}

// Implementation of RateLimiter::printLog
void RateLimiter::printLog() const noexcept {
#if ENABLE_DEBUG // Assuming ENABLE_DEBUG is a macro controlling this
    LOG_F(INFO, "RateLimiter::printLog called");
    std::shared_lock<std::shared_mutex> lock(mutex_); // Read lock
    for (const auto& [function_name, timestamps] : log_) {
        // Using LOG_F for consistency, though std::cout is also fine for debug
        LOG_F(INFO, "Request log for %s:", function_name.c_str());
        for (const auto& timestamp : timestamps) {
            LOG_F(INFO, "  Request at %lld", (long long)timestamp.time_since_epoch().count());
        }
    }
#endif
}

// Implementation of RateLimiter::getRejectedRequests
auto RateLimiter::getRejectedRequests(
    std::string_view function_name) const noexcept -> size_t {
    LOG_F(INFO, "RateLimiter::getRejectedRequests called for function: %s",
          std::string(function_name).c_str());

    std::shared_lock<std::shared_mutex> lock(mutex_); // Read lock
    auto it = rejected_requests_.find(std::string(function_name));
    if (it != rejected_requests_.end()) {
        return it->second.load(std::memory_order_relaxed);
    }
    return 0;
}

// Implementation of RateLimiter::resetFunction
void RateLimiter::resetFunction(std::string_view function_name_sv) {
    std::string func_name(function_name_sv);
    LOG_F(INFO, "RateLimiter::resetFunction called for function: %s",
          func_name.c_str());

    std::unique_lock<std::shared_mutex> lock(mutex_);

    // Clear request records
#ifdef ATOM_USE_BOOST_LOCKFREE
    if (auto it = requests_.find(func_name); it != requests_.end()) {
        // Assuming 'clear' is not directly available on boost::lockfree::queue
        // Reassign to an empty queue or pop all elements
        LockfreeRequestQueue empty_queue;
        it->second.swap(empty_queue); // Efficiently clears by swapping with empty
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
    } else {
        // Ensure the entry exists if we want to reset it to 0,
        // or rely on getRejectedRequests returning 0 for non-existent keys.
        // For consistency, we can insert it if not present.
        rejected_requests_[func_name].store(0, std::memory_order_relaxed);
    }

    LOG_F(INFO, "Reset completed for function: %s", func_name.c_str());
}

// Implementation of RateLimiter::resetAll
void RateLimiter::resetAll() noexcept {
    LOG_F(INFO, "RateLimiter::resetAll called");

    std::unique_lock<std::shared_mutex> lock(mutex_);

    // Clear all request records
#ifdef ATOM_USE_BOOST_LOCKFREE
    for (auto& pair : requests_) {
        LockfreeRequestQueue empty_queue;
        pair.second.swap(empty_queue);
    }
#else
    for (auto& pair : requests_) {
        pair.second.clear();
    }
#endif

    // Reset all rejection counts
    for (auto& pair : rejected_requests_) {
        pair.second.store(0, std::memory_order_relaxed);
    }

    LOG_F(INFO, "All rate limits have been reset");
}

// Implementation of RateLimiter::cleanup
void RateLimiter::cleanup(std::string_view function_name_sv,
                          const std::chrono::seconds& time_window) {
    std::string func_name(function_name_sv);
    LOG_F(INFO,
          "RateLimiter::cleanup called for function: %s, time_window=%lld "
          "seconds",
          func_name.c_str(), (long long)time_window.count());

    auto now = std::chrono::steady_clock::now();
    auto cutoff_time = now - time_window;

#ifdef ATOM_USE_BOOST_LOCKFREE
    // For lockfree implementation, we need to create a new queue and transfer valid items
    // This needs to be done carefully if requests_ might not contain func_name yet.
    auto it = requests_.find(func_name);
    if (it == requests_.end()) return; // No requests to clean for this function

    LockfreeRequestQueue& current_queue = it->second;
    LockfreeRequestQueue new_queue; // Assuming default construction is fine

    // Move valid timestamps to the new queue
    std::chrono::steady_clock::time_point timestamp;
    // Consume all items from current_queue
    while (current_queue.pop(timestamp)) {
        if (timestamp >= cutoff_time) {
            new_queue.push(timestamp); // Push valid items to new_queue
        }
    }
    // Replace the old queue with the new one by swapping
    current_queue.swap(new_queue);
#else
    auto it = requests_.find(func_name);
    if (it == requests_.end()) return; // No requests to clean

    auto& reqs = it->second;

    // C++20 ranges and views for more concise filtering
    auto is_expired = [&cutoff_time](const auto& time_point) {
        return time_point < cutoff_time;
    };
    // std::erase_if is C++20 for deques
    std::erase_if(reqs, is_expired);
#endif
}

// Implementation of RateLimiter::processWaiters (Generic)
void RateLimiter::processWaiters() {
    LOG_F(INFO, "RateLimiter::processWaiters called (generic)");

    std::vector<std::pair<std::string, std::coroutine_handle<>>> waiters_to_process;

    { // Scope for the unique_lock
        std::unique_lock<std::shared_mutex> lock(mutex_); // Re-lock mutex for this operation section

#ifdef ATOM_USE_BOOST_LOCKFREE
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty()) continue;

            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end()) continue; // Should have settings
            auto& current_settings = settings_it->second;
            
            // Ensure requests_ map has an entry for function_name before accessing
            auto& req_queue = requests_[function_name]; // Creates if not exists

            while (!wait_queue.empty() &&
                   req_queue.size_approx() < current_settings.maxRequests) {
                std::coroutine_handle<> handle;
                if (wait_queue.pop(handle)) {
                    req_queue.push(std::chrono::steady_clock::now());
                    waiters_to_process.emplace_back(function_name, handle);
                }
            }
        }
#else
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty()) continue;

            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end()) continue;
            auto& current_settings = settings_it->second;

            // Ensure requests_ map has an entry
            auto& req_list = requests_[function_name]; // Creates if not exists

            while (!wait_queue.empty() &&
                   req_list.size() < current_settings.maxRequests) {
                auto waiter = wait_queue.front();
                wait_queue.pop_front();
                req_list.emplace_back(std::chrono::steady_clock::now());
                waiters_to_process.emplace_back(function_name, waiter);
            }
        }
#endif
    } // unique_lock released here

    // Resume waiters outside the lock
    if (!waiters_to_process.empty()) {
        // Use C++20 parallel algorithms for better performance if appropriate,
        // but ensure safety with coroutine resumption. Sequential might be safer
        // if coroutines interact heavily or are not thread-safe.
        // For now, using parallel as in original.
        std::for_each(std::execution::par_unseq, waiters_to_process.begin(),
                      waiters_to_process.end(), [](const auto& pair) {
                          LOG_F(INFO, "Resuming waiter for function: %s (generic)",
                                pair.first.c_str());
                          pair.second.resume();
                      });
    }
}

#ifdef ATOM_USE_ASIO
void RateLimiter::asioProcessWaiters() {
    LOG_F(INFO, "Using Asio-optimized processWaiters");

    std::vector<std::pair<std::string, std::coroutine_handle<>>> waiters_to_process;

    { // Scope for the unique_lock
        std::unique_lock<std::shared_mutex> lock(mutex_); // Lock to safely access shared data

#ifdef ATOM_USE_BOOST_LOCKFREE
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty()) continue;
            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end()) continue;
            auto& current_settings = settings_it->second;
            auto& req_queue = requests_[function_name];

            while (!wait_queue.empty() && req_queue.size_approx() < current_settings.maxRequests) {
                std::coroutine_handle<> handle;
                if (wait_queue.pop(handle)) {
                    req_queue.push(std::chrono::steady_clock::now());
                    waiters_to_process.emplace_back(function_name, handle);
                }
            }
        }
#else // Not ATOM_USE_BOOST_LOCKFREE
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty()) continue;
            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end()) continue;
            auto& current_settings = settings_it->second;
            auto& req_list = requests_[function_name];

            while (!wait_queue.empty() && req_list.size() < current_settings.maxRequests) {
                auto waiter = wait_queue.front();
                wait_queue.pop_front();
                req_list.emplace_back(std::chrono::steady_clock::now());
                waiters_to_process.emplace_back(function_name, waiter);
            }
        }
#endif
    } // unique_lock released

    if (!waiters_to_process.empty()) {
        for (const auto& pair_to_process : waiters_to_process) {
            // Capture necessary data by value for the lambda
            std::string fn_copy = pair_to_process.first;
            std::coroutine_handle<> h_copy = pair_to_process.second;
            asio::post(asio_pool_, [fn_copy, h_copy]() {
                LOG_F(INFO, "Resuming waiter for function: %s (Asio)", fn_copy.c_str());
                h_copy.resume();
            });
        }
    }
}
#endif


// Windows platform-specific optimized implementation
#ifdef ATOM_PLATFORM_WINDOWS
void RateLimiter::optimizedProcessWaiters() {
    LOG_F(INFO, "Using Windows-optimized processWaiters");

    EnterCriticalSection(&resumeLock_); // Outer lock for the operation

    std::vector<std::pair<std::string, std::coroutine_handle<>>> waiters_to_process;

    { // Scope for unique_lock on mutex_
        std::unique_lock<std::shared_mutex> lock(mutex_); // Lock for shared data access

#ifdef ATOM_USE_BOOST_LOCKFREE
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty()) continue;
            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end()) continue;
            auto& current_settings = settings_it->second;
            auto& req_queue = requests_[function_name];

            while (!wait_queue.empty() &&
                   req_queue.size_approx() < current_settings.maxRequests) {
                std::coroutine_handle<> handle;
                if (wait_queue.pop(handle)) {
                    req_queue.push(std::chrono::steady_clock::now());
                    waiters_to_process.emplace_back(function_name, handle);
                }
            }
        }
#else
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty()) continue;
            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end()) continue;
            auto& current_settings = settings_it->second;
            auto& req_list = requests_[function_name];
            
            while (!wait_queue.empty() &&
                   req_list.size() < current_settings.maxRequests) {
                auto waiter = wait_queue.front();
                wait_queue.pop_front();
                req_list.emplace_back(std::chrono::steady_clock::now());
                waiters_to_process.emplace_back(function_name, waiter);
            }
        }
#endif
    } // unique_lock on mutex_ released

    // Use Windows thread pool to process waiters
    if (!waiters_to_process.empty()) {
        struct ResumeInfo {
            std::string function_name;
            std::coroutine_handle<> handle;
        };

        for (const auto& pair_to_process : waiters_to_process) {
            auto* info = new ResumeInfo{pair_to_process.first, pair_to_process.second};

            if (!QueueUserWorkItem(
                    [](PVOID context) -> DWORD {
                        auto* current_info = static_cast<ResumeInfo*>(context);
                        LOG_F(INFO, "Resuming waiter for function: %s (Windows)",
                              current_info->function_name.c_str());
                        current_info->handle.resume();
                        delete current_info;
                        return 0;
                    },
                    info, WT_EXECUTEDEFAULT)) {
                LOG_F(WARNING,
                      "Failed to queue work item for %s, executing synchronously", info->function_name.c_str());
                // Fallback to synchronous execution if queuing fails
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

    std::vector<std::pair<std::string, std::coroutine_handle<>>> waiters_to_process;

    { // Scope for unique_lock
        std::unique_lock<std::shared_mutex> lock(mutex_);

#ifdef ATOM_USE_BOOST_LOCKFREE
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty()) continue;
            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end()) continue;
            auto& current_settings = settings_it->second;
            auto& req_queue = requests_[function_name];

            while (!wait_queue.empty() &&
                   req_queue.size_approx() < current_settings.maxRequests) {
                std::coroutine_handle<> handle;
                if (wait_queue.pop(handle)) {
                    req_queue.push(std::chrono::steady_clock::now());
                    waiters_to_process.emplace_back(function_name, handle);
                }
            }
        }
#else
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty()) continue;
            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end()) continue;
            auto& current_settings = settings_it->second;
            auto& req_list = requests_[function_name];

            while (!wait_queue.empty() &&
                   req_list.size() < current_settings.maxRequests) {
                auto waiter = wait_queue.front();
                wait_queue.pop_front();
                req_list.emplace_back(std::chrono::steady_clock::now());
                waiters_to_process.emplace_back(function_name, waiter);
            }
        }
#endif
    } // unique_lock released

    // Use GCD to process waiters
    if (!waiters_to_process.empty()) {
        dispatch_queue_t queue =
            dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
        dispatch_group_t group = dispatch_group_create();

        for (const auto& pair_to_process : waiters_to_process) {
            std::string fname_copy = pair_to_process.first; // Capture by value
            std::coroutine_handle<> handle_copy = pair_to_process.second; // Capture by value

            dispatch_group_async(group, queue, ^{
              LOG_F(INFO, "Resuming waiter for function: %s (macOS GCD)",
                    fname_copy.c_str());
              handle_copy.resume();
            });
        }
        dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
        // dispatch_release(group); // Not needed with ARC, but good practice if not ARC or for older GCD.
                                 // Modern GCD with ARC manages object lifetimes.
                                 // If this project doesn't use ARC for C++ objects interacting with Obj-C runtime,
                                 // then manual release is important. Assuming modern setup.
    }
}
#endif

// Linux platform-specific optimized implementation
#ifdef ATOM_PLATFORM_LINUX
void RateLimiter::optimizedProcessWaiters() {
    LOG_F(INFO, "Using Linux-optimized processWaiters");

#if !defined(ATOM_USE_ASIO) // This optimization is for the native Linux path
    int expected_waiters = waitersReady_.load(std::memory_order_relaxed);
    if (expected_waiters <= 0) {
        LOG_F(INFO, "No waiters ready for Linux optimized path, returning.");
        return; 
    }
#endif

    std::vector<std::pair<std::string, std::coroutine_handle<>>> waiters_to_process;

    { // Scope for unique_lock
        std::unique_lock<std::shared_mutex> lock(mutex_);

#ifdef ATOM_USE_BOOST_LOCKFREE
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty()) continue;
            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end()) continue;
            auto& current_settings = settings_it->second;
            auto& req_queue = requests_[function_name];

            while (!wait_queue.empty() &&
                   req_queue.size_approx() < current_settings.maxRequests) {
                std::coroutine_handle<> handle;
                if (wait_queue.pop(handle)) {
                    req_queue.push(std::chrono::steady_clock::now());
                    waiters_to_process.emplace_back(function_name, handle);
#if !defined(ATOM_USE_ASIO)
                    waitersReady_.fetch_sub(1, std::memory_order_relaxed);
#endif
                }
            }
        }
#else
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty()) continue;
            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end()) continue;
            auto& current_settings = settings_it->second;
            auto& req_list = requests_[function_name];

            while (!wait_queue.empty() &&
                   req_list.size() < current_settings.maxRequests) {
                auto waiter = wait_queue.front();
                wait_queue.pop_front();
                req_list.emplace_back(std::chrono::steady_clock::now());
                waiters_to_process.emplace_back(function_name, waiter);
#if !defined(ATOM_USE_ASIO)
                waitersReady_.fetch_sub(1, std::memory_order_relaxed);
#endif
            }
        }
#endif
    } // unique_lock released

    // Use POSIX threads to process waiters
    if (!waiters_to_process.empty()) {
        struct ResumeThreadArg {
            std::string function_name;
            std::coroutine_handle<> handle;
        };

        std::vector<pthread_t> threads;
        threads.reserve(waiters_to_process.size());

        for (const auto& pair_to_process : waiters_to_process) {
            auto* arg = new ResumeThreadArg{pair_to_process.first, pair_to_process.second};
            pthread_t thread;
            if (pthread_create(
                    &thread, nullptr,
                    [](void* thread_arg) -> void* {
                        auto* data = static_cast<ResumeThreadArg*>(thread_arg);
                        LOG_F(INFO, "Resuming waiter for function: %s (Linux pthread)",
                              data->function_name.c_str());
                        data->handle.resume();
                        delete data;
                        return nullptr;
                    },
                    arg) == 0) {
                threads.push_back(thread);
            } else {
                LOG_F(WARNING,
                      "Failed to create thread for %s, executing synchronously", arg->function_name.c_str());
                // Fallback to synchronous execution
                arg->handle.resume();
                delete arg;
            }
        }

        // Detach all successfully created threads
        for (auto thread_id : threads) {
            pthread_detach(thread_id);
        }
    }
}
#endif

}  // namespace atom::async
