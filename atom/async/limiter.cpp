#include "limiter.hpp"

#include <algorithm>
#include <chrono>
#include <coroutine>
#include <execution>
#include <format>
#include <ranges>
#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>
#include "atom/error/exception.hpp"

#ifdef ATOM_USE_ASIO
#include <asio/post.hpp>
#include <asio/thread_pool.hpp>
#include <thread>
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

RateLimiter::RateLimiter() noexcept
#ifdef ATOM_USE_ASIO
    : asio_pool_(std::thread::hardware_concurrency() > 0
                     ? std::thread::hardware_concurrency()
                     : 1)
#endif
{
    spdlog::info("RateLimiter created");

#ifdef ATOM_PLATFORM_WINDOWS
    InitializeCriticalSection(&resumeLock_);
    InitializeConditionVariable(&resumeCondition_);
#elif defined(ATOM_PLATFORM_LINUX)
    sem_init(&resumeSemaphore_, 0, 0);
#endif
}

RateLimiter::~RateLimiter() noexcept {
    try {
#ifdef ATOM_USE_BOOST_LOCKFREE
        std::unique_lock lock(mutex_);
        for (auto& [name, queue] : waiters_) {
            std::coroutine_handle<> handle;
            while (queue.pop(handle)) {
                lock.unlock();
                handle.resume();
                lock.lock();
            }
        }
#else
        std::unique_lock lock(mutex_);
        for (auto& [_, waiters_queue] : waiters_) {
            while (!waiters_queue.empty()) {
                auto waiter_info = waiters_queue.front();
                waiters_queue.pop_front();
                lock.unlock();
                waiter_info.handle.resume();
                lock.lock();
            }
        }
#endif

#ifdef ATOM_USE_ASIO
        asio_pool_.join();
#endif

#ifdef ATOM_PLATFORM_WINDOWS
        DeleteCriticalSection(&resumeLock_);
#elif defined(ATOM_PLATFORM_LINUX)
        sem_destroy(&resumeSemaphore_);
#endif
    } catch (...) {
        spdlog::error("Exception in RateLimiter destructor");
    }
}

RateLimiter::RateLimiter(RateLimiter&& other) noexcept
    : paused_(other.paused_.load())
#ifdef ATOM_USE_ASIO
      ,
      asio_pool_(std::thread::hardware_concurrency() > 0
                     ? std::thread::hardware_concurrency()
                     : 1)
#endif
{
    std::unique_lock lock(other.mutex_);
    settings_ = std::move(other.settings_);
    requests_ = std::move(other.requests_);
    waiters_ = std::move(other.waiters_);

    for (const auto& [name, count] : other.rejected_requests_) {
        rejected_requests_[name].store(count.load());
    }
    other.rejected_requests_.clear();

#ifdef ATOM_PLATFORM_WINDOWS
    std::swap(resumeCondition_, other.resumeCondition_);
    std::swap(resumeLock_, other.resumeLock_);
#elif defined(ATOM_PLATFORM_LINUX)
    waitersReady_.store(other.waitersReady_.load());
    other.waitersReady_.store(0);
    sem_destroy(&other.resumeSemaphore_);
    sem_init(&resumeSemaphore_, 0, 0);
#endif
}

RateLimiter& RateLimiter::operator=(RateLimiter&& other) noexcept {
    if (this != &other) {
        std::scoped_lock lock(mutex_, other.mutex_);
        settings_ = std::move(other.settings_);
        requests_ = std::move(other.requests_);
        waiters_ = std::move(other.waiters_);
        paused_.store(other.paused_.load());

        rejected_requests_.clear();
        for (const auto& [name, count] : other.rejected_requests_) {
            rejected_requests_[name].store(count.load());
        }
        other.rejected_requests_.clear();

#ifdef ATOM_PLATFORM_WINDOWS
        std::swap(resumeCondition_, other.resumeCondition_);
        std::swap(resumeLock_, other.resumeLock_);
#elif defined(ATOM_PLATFORM_LINUX)
        waitersReady_.store(other.waitersReady_.load());
        other.waitersReady_.store(0);
        sem_destroy(&resumeSemaphore_);
        sem_init(&resumeSemaphore_, 0, 0);
        sem_destroy(&other.resumeSemaphore_);
        sem_init(&other.resumeSemaphore_, 0, 0);
#endif
    }
    return *this;
}

RateLimiter::Awaiter::Awaiter(RateLimiter& limiter,
                              std::string function_name) noexcept
    : limiter_(limiter), function_name_(std::move(function_name)) {
    spdlog::debug("Awaiter created for function: {}", function_name_);
}

auto RateLimiter::Awaiter::await_ready() const noexcept -> bool {
    return false;
}

void RateLimiter::Awaiter::await_suspend(std::coroutine_handle<> handle) {
    spdlog::debug("Awaiter suspending for function: {}", function_name_);

    try {
        std::unique_lock<std::shared_mutex> lock(limiter_.mutex_);

        if (!limiter_.settings_.contains(function_name_)) {
            limiter_.settings_[function_name_] = Settings();
        }

        auto& settings = limiter_.settings_[function_name_];
        limiter_.cleanup(function_name_, settings.timeWindow);

#ifdef ATOM_USE_BOOST_LOCKFREE
        auto& req_queue = limiter_.requests_[function_name_];
        if (limiter_.paused_.load(std::memory_order_acquire) ||
            req_queue.size_approx() >= settings.maxRequests) {
            limiter_.waiters_[function_name_].push(handle);
            limiter_.rejected_requests_[function_name_].fetch_add(
                1, std::memory_order_relaxed);
            was_rejected_ = true;

#if defined(ATOM_PLATFORM_LINUX) && !defined(ATOM_USE_ASIO)
            limiter_.waitersReady_.fetch_add(1, std::memory_order_relaxed);
#endif
            spdlog::warn("Request for function {} rejected. Total rejected: {}",
                         function_name_,
                         limiter_.rejected_requests_[function_name_].load(
                             std::memory_order_relaxed));
        } else {
            req_queue.push(std::chrono::steady_clock::now());
            was_rejected_ = false;
            lock.unlock();
            spdlog::debug("Request for function {} accepted", function_name_);
            handle.resume();
        }
#else
        auto& req_list = limiter_.requests_[function_name_];
        if (limiter_.paused_.load(std::memory_order_acquire) ||
            req_list.size() >= settings.maxRequests) {
            limiter_.waiters_[function_name_].emplace_back(handle, this);
            limiter_.rejected_requests_[function_name_].fetch_add(
                1, std::memory_order_relaxed);
            was_rejected_ = true;

#if defined(ATOM_PLATFORM_LINUX) && !defined(ATOM_USE_ASIO)
            limiter_.waitersReady_.fetch_add(1, std::memory_order_relaxed);
#endif
            spdlog::warn("Request for function {} rejected. Total rejected: {}",
                         function_name_,
                         limiter_.rejected_requests_[function_name_].load(
                             std::memory_order_relaxed));
        } else {
            req_list.emplace_back(std::chrono::steady_clock::now());
            was_rejected_ = false;
            lock.unlock();
            spdlog::debug("Request for function {} accepted", function_name_);
            handle.resume();
        }
#endif
    } catch (const std::exception& e) {
        spdlog::error("Exception in await_suspend: {}", e.what());
        handle.resume();
    } catch (...) {
        spdlog::error("Unknown exception in await_suspend");
        handle.resume();
    }
}

void RateLimiter::Awaiter::await_resume() {
    spdlog::debug("Awaiter resuming for function: {}", function_name_);
    if (was_rejected_) {
        throw RateLimitExceededException(std::format(
            "Rate limit exceeded for function: {}", function_name_));
    }
}

RateLimiter::Awaiter RateLimiter::acquire(std::string_view function_name) {
    spdlog::debug("Acquiring rate limiter for function: {}", function_name);
    return Awaiter(*this, std::string(function_name));
}

void RateLimiter::setFunctionLimit(std::string_view function_name,
                                   size_t max_requests,
                                   std::chrono::seconds time_window) {
    if (max_requests == 0) {
        THROW_INVALID_ARGUMENT("max_requests must be greater than 0");
    }
    if (time_window.count() <= 0) {
        THROW_INVALID_ARGUMENT("time_window must be greater than 0 seconds");
    }

    spdlog::info(
        "Setting limit for function: {}, max_requests={}, time_window={}s",
        function_name, max_requests, time_window.count());

    std::unique_lock<std::shared_mutex> lock(mutex_);
    settings_[std::string(function_name)] = Settings(max_requests, time_window);
}

void RateLimiter::setFunctionLimits(
    std::span<const std::pair<std::string_view, Settings>> settings_list) {
    spdlog::info("Setting {} function limits", settings_list.size());

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
        spdlog::debug(
            "Set limit for function: {}, max_requests={}, time_window={}s",
            function_name_str, setting.maxRequests, setting.timeWindow.count());
    }
}

void RateLimiter::pause() noexcept {
    spdlog::info("Rate limiter paused");
    paused_.store(true, std::memory_order_release);
}

void RateLimiter::resume() {
    spdlog::info("Rate limiter resumed");
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        paused_.store(false, std::memory_order_release);
        lock.unlock();

#if defined(ATOM_USE_ASIO)
        asioProcessWaiters();
#elif defined(ATOM_PLATFORM_WINDOWS) || defined(ATOM_PLATFORM_MACOS) || \
    defined(ATOM_PLATFORM_LINUX)
        optimizedProcessWaiters();
#else
        processWaiters();
#endif
    }
}

auto RateLimiter::getRejectedRequests(
    std::string_view function_name) const noexcept -> size_t {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = rejected_requests_.find(std::string(function_name));
    return it != rejected_requests_.end()
               ? it->second.load(std::memory_order_relaxed)
               : 0;
}

void RateLimiter::resetFunction(std::string_view function_name_sv) {
    std::string func_name(function_name_sv);
    spdlog::info("Resetting function: {}", func_name);

    std::unique_lock<std::shared_mutex> lock(mutex_);

#ifdef ATOM_USE_BOOST_LOCKFREE
    if (auto it = requests_.find(func_name); it != requests_.end()) {
        std::coroutine_handle<> dummy;
        while (it->second.pop(dummy)) {
        }
    }
#else
    if (auto it = requests_.find(func_name); it != requests_.end()) {
        it->second.clear();
    }
#endif

    if (auto it = rejected_requests_.find(func_name);
        it != rejected_requests_.end()) {
        it->second.store(0, std::memory_order_relaxed);
    } else {
        rejected_requests_[func_name].store(0, std::memory_order_relaxed);
    }
}

void RateLimiter::resetAll() noexcept {
    spdlog::info("Resetting all rate limits");

    std::unique_lock<std::shared_mutex> lock(mutex_);

#ifdef ATOM_USE_BOOST_LOCKFREE
    for (auto& [name, queue] : requests_) {
        std::coroutine_handle<> dummy;
        while (queue.pop(dummy)) {
        }
    }
#else
    for (auto& [name, deque] : requests_) {
        deque.clear();
    }
#endif

    for (auto& [name, counter] : rejected_requests_) {
        counter.store(0, std::memory_order_relaxed);
    }
}

void RateLimiter::cleanup(std::string_view function_name_sv,
                          const std::chrono::seconds& time_window) {
    std::string func_name(function_name_sv);
    auto now = std::chrono::steady_clock::now();
    auto cutoff_time = now - time_window;

#ifdef ATOM_USE_BOOST_LOCKFREE
    auto it = requests_.find(func_name);
    if (it == requests_.end())
        return;

    LockfreeRequestQueue new_queue;
    std::chrono::steady_clock::time_point timestamp;
    while (it->second.pop(timestamp)) {
        if (timestamp >= cutoff_time) {
            new_queue.push(timestamp);
        }
    }
    it->second = std::move(new_queue);
#else
    auto it = requests_.find(func_name);
    if (it == requests_.end())
        return;

    auto& reqs = it->second;
    std::erase_if(reqs, [&cutoff_time](const auto& time_point) {
        return time_point < cutoff_time;
    });
#endif
}

void RateLimiter::processWaiters() {
    spdlog::debug("Processing waiters (generic)");

    std::vector<std::pair<std::string, std::coroutine_handle<>>>
        waiters_to_process;

    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

#ifdef ATOM_USE_BOOST_LOCKFREE
        for (auto& [function_name, wait_queue] : waiters_) {
            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end())
                continue;

            auto& current_settings = settings_it->second;
            auto& req_queue = requests_[function_name];

            std::coroutine_handle<> handle;
            while (wait_queue.pop(handle) &&
                   req_queue.size_approx() < current_settings.maxRequests) {
                req_queue.push(std::chrono::steady_clock::now());
                waiters_to_process.emplace_back(function_name, handle);
            }
        }
#else
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty())
                continue;

            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end())
                continue;

            auto& current_settings = settings_it->second;
            auto& req_list = requests_[function_name];

            while (!wait_queue.empty() &&
                   req_list.size() < current_settings.maxRequests) {
                auto waiter = wait_queue.front();
                wait_queue.pop_front();
                req_list.emplace_back(std::chrono::steady_clock::now());
                waiters_to_process.emplace_back(function_name, waiter.handle);
            }
        }
#endif
    }

    if (!waiters_to_process.empty()) {
        std::for_each(std::execution::par_unseq, waiters_to_process.begin(),
                      waiters_to_process.end(), [](const auto& pair) {
                          spdlog::debug("Resuming waiter for function: {}",
                                        pair.first);
                          pair.second.resume();
                      });
    }
}

#ifdef ATOM_USE_ASIO
void RateLimiter::asioProcessWaiters() {
    spdlog::debug("Processing waiters using Asio");

    std::vector<std::pair<std::string, std::coroutine_handle<>>>
        waiters_to_process;

    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

#ifdef ATOM_USE_BOOST_LOCKFREE
        for (auto& [function_name, wait_queue] : waiters_) {
            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end())
                continue;

            auto& current_settings = settings_it->second;
            auto& req_queue = requests_[function_name];

            std::coroutine_handle<> handle;
            while (wait_queue.pop(handle) &&
                   req_queue.size_approx() < current_settings.maxRequests) {
                req_queue.push(std::chrono::steady_clock::now());
                waiters_to_process.emplace_back(function_name, handle);
            }
        }
#else
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty())
                continue;

            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end())
                continue;

            auto& current_settings = settings_it->second;
            auto& req_list = requests_[function_name];

            while (!wait_queue.empty() &&
                   req_list.size() < current_settings.maxRequests) {
                auto waiter = wait_queue.front();
                wait_queue.pop_front();
                req_list.emplace_back(std::chrono::steady_clock::now());
                waiters_to_process.emplace_back(function_name, waiter.handle);
            }
        }
#endif
    }

    if (!waiters_to_process.empty()) {
        for (const auto& [fn_name, handle] : waiters_to_process) {
            asio::post(asio_pool_, [fn_name, handle]() {
                spdlog::debug("Resuming waiter for function: {} (Asio)",
                              fn_name);
                handle.resume();
            });
        }
    }
}
#endif

#ifdef ATOM_PLATFORM_WINDOWS
void RateLimiter::optimizedProcessWaiters() {
    spdlog::debug("Processing waiters using Windows optimization");

    EnterCriticalSection(&resumeLock_);

    std::vector<std::pair<std::string, std::coroutine_handle<>>>
        waiters_to_process;

    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

#ifdef ATOM_USE_BOOST_LOCKFREE
        for (auto& [function_name, wait_queue] : waiters_) {
            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end())
                continue;

            auto& current_settings = settings_it->second;
            auto& req_queue = requests_[function_name];

            std::coroutine_handle<> handle;
            while (wait_queue.pop(handle) &&
                   req_queue.size_approx() < current_settings.maxRequests) {
                req_queue.push(std::chrono::steady_clock::now());
                waiters_to_process.emplace_back(function_name, handle);
            }
        }
#else
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty())
                continue;

            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end())
                continue;

            auto& current_settings = settings_it->second;
            auto& req_list = requests_[function_name];

            while (!wait_queue.empty() &&
                   req_list.size() < current_settings.maxRequests) {
                auto waiter = wait_queue.front();
                wait_queue.pop_front();
                req_list.emplace_back(std::chrono::steady_clock::now());
                waiters_to_process.emplace_back(function_name, waiter.handle);
            }
        }
#endif
    }

    if (!waiters_to_process.empty()) {
        struct ResumeInfo {
            std::string function_name;
            std::coroutine_handle<> handle;
        };

        for (const auto& [fn_name, handle] : waiters_to_process) {
            auto* info = new ResumeInfo{fn_name, handle};

            if (!QueueUserWorkItem(
                    [](PVOID context) -> DWORD {
                        auto* current_info = static_cast<ResumeInfo*>(context);
                        spdlog::debug(
                            "Resuming waiter for function: {} (Windows)",
                            current_info->function_name);
                        current_info->handle.resume();
                        delete current_info;
                        return 0;
                    },
                    info, WT_EXECUTEDEFAULT)) {
                spdlog::warn(
                    "Failed to queue work item for {}, executing synchronously",
                    info->function_name);
                info->handle.resume();
                delete info;
            }
        }
    }

    LeaveCriticalSection(&resumeLock_);
}
#endif

#ifdef ATOM_PLATFORM_MACOS
void RateLimiter::optimizedProcessWaiters() {
    spdlog::debug("Processing waiters using macOS optimization");

    std::vector<std::pair<std::string, std::coroutine_handle<>>>
        waiters_to_process;

    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

#ifdef ATOM_USE_BOOST_LOCKFREE
        for (auto& [function_name, wait_queue] : waiters_) {
            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end())
                continue;

            auto& current_settings = settings_it->second;
            auto& req_queue = requests_[function_name];

            std::coroutine_handle<> handle;
            while (wait_queue.pop(handle) &&
                   req_queue.size_approx() < current_settings.maxRequests) {
                req_queue.push(std::chrono::steady_clock::now());
                waiters_to_process.emplace_back(function_name, handle);
            }
        }
#else
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty())
                continue;

            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end())
                continue;

            auto& current_settings = settings_it->second;
            auto& req_list = requests_[function_name];

            while (!wait_queue.empty() &&
                   req_list.size() < current_settings.maxRequests) {
                auto waiter = wait_queue.front();
                wait_queue.pop_front();
                req_list.emplace_back(std::chrono::steady_clock::now());
                waiters_to_process.emplace_back(function_name, waiter.handle);
            }
        }
#endif
    }

    if (!waiters_to_process.empty()) {
        dispatch_queue_t queue =
            dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
        dispatch_group_t group = dispatch_group_create();

        for (const auto& [fname, handle] : waiters_to_process) {
            dispatch_group_async(group, queue, ^{
                spdlog::debug("Resuming waiter for function: {} (macOS GCD)",
                              fname);
                handle.resume();
            });
        }
        dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
    }
}
#endif

#ifdef ATOM_PLATFORM_LINUX
void RateLimiter::optimizedProcessWaiters() {
    spdlog::debug("Processing waiters using Linux optimization");

#if !defined(ATOM_USE_ASIO)
    int expected_waiters = waitersReady_.load(std::memory_order_relaxed);
    if (expected_waiters <= 0) {
        return;
    }
#endif

    std::vector<std::pair<std::string, std::coroutine_handle<>>>
        waiters_to_process;

    {
        std::unique_lock<std::shared_mutex> lock(mutex_);

#ifdef ATOM_USE_BOOST_LOCKFREE
        for (auto& [function_name, wait_queue] : waiters_) {
            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end())
                continue;

            auto& current_settings = settings_it->second;
            auto& req_queue = requests_[function_name];

            std::coroutine_handle<> handle;
            while (wait_queue.pop(handle) &&
                   req_queue.size_approx() < current_settings.maxRequests) {
                req_queue.push(std::chrono::steady_clock::now());
                waiters_to_process.emplace_back(function_name, handle);
#if !defined(ATOM_USE_ASIO)
                waitersReady_.fetch_sub(1, std::memory_order_relaxed);
#endif
            }
        }
#else
        for (auto& [function_name, wait_queue] : waiters_) {
            if (wait_queue.empty())
                continue;

            auto settings_it = settings_.find(function_name);
            if (settings_it == settings_.end())
                continue;

            auto& current_settings = settings_it->second;
            auto& req_list = requests_[function_name];

            while (!wait_queue.empty() &&
                   req_list.size() < current_settings.maxRequests) {
                auto waiter = wait_queue.front();
                wait_queue.pop_front();
                req_list.emplace_back(std::chrono::steady_clock::now());
                waiters_to_process.emplace_back(function_name, waiter.handle);
#if !defined(ATOM_USE_ASIO)
                waitersReady_.fetch_sub(1, std::memory_order_relaxed);
#endif
            }
        }
#endif
    }

    if (!waiters_to_process.empty()) {
        struct ResumeThreadArg {
            std::string function_name;
            std::coroutine_handle<> handle;
        };

        std::vector<pthread_t> threads;
        threads.reserve(waiters_to_process.size());

        for (const auto& [fn_name, handle] : waiters_to_process) {
            auto* arg = new ResumeThreadArg{fn_name, handle};
            pthread_t thread;
            if (pthread_create(
                    &thread, nullptr,
                    [](void* thread_arg) -> void* {
                        auto* data = static_cast<ResumeThreadArg*>(thread_arg);
                        spdlog::debug(
                            "Resuming waiter for function: {} (Linux pthread)",
                            data->function_name);
                        data->handle.resume();
                        delete data;
                        return nullptr;
                    },
                    arg) == 0) {
                threads.push_back(thread);
            } else {
                spdlog::warn(
                    "Failed to create thread for {}, executing synchronously",
                    arg->function_name);
                arg->handle.resume();
                delete arg;
            }
        }

        for (auto thread_id : threads) {
            pthread_detach(thread_id);
        }
    }
}
#endif

}  // namespace atom::async
