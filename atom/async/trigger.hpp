/*
 * trigger.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-14

Description: Trigger class for C++

**************************************************/

#ifndef ATOM_ASYNC_TRIGGER_HPP
#define ATOM_ASYNC_TRIGGER_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// Add spdlog include
#include "spdlog/spdlog.h"

#ifdef ATOM_USE_BOOST_LOCKS
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_lock_guard.hpp>
#include <boost/thread/shared_mutex.hpp>
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
#include <boost/atomic.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#endif

namespace atom::async {

// Conditionally select threading primitives based on availability of Boost
namespace internal {
#ifdef ATOM_USE_BOOST_LOCKS
using mutex_type = boost::mutex;
using shared_mutex_type = boost::shared_mutex;
using unique_lock = boost::unique_lock<mutex_type>;
using shared_lock = boost::shared_lock<shared_mutex_type>;
using lock_guard = boost::lock_guard<mutex_type>;

template <typename T>
using future = boost::future<T>;

template <typename T>
using promise = boost::promise<T>;

using thread = boost::thread;

template <typename Func, typename... Args>
auto make_thread(Func&& func, Args&&... args) {
    return boost::thread(std::forward<Func>(func), std::forward<Args>(args)...);
}

// Equivalent of std::jthread using Boost threads
class joining_thread {
private:
    boost::thread thread_;

public:
    template <typename Func, typename... Args>
    explicit joining_thread(Func&& func, Args&&... args)
        : thread_(std::forward<Func>(func), std::forward<Args>(args)...) {}

    ~joining_thread() {
        if (thread_.joinable()) {
            try {
                thread_.join();
            } catch (const std::exception& e) {
                spdlog::error(
                    "Exception during boost::thread join in ~joining_thread: "
                    "{}",
                    e.what());
            } catch (...) {
                spdlog::error(
                    "Unknown exception during boost::thread join in "
                    "~joining_thread");
            }
        }
    }

    void detach() { thread_.detach(); }

    joining_thread(joining_thread&&) = default;
    joining_thread& operator=(joining_thread&&) = default;
    joining_thread(const joining_thread&) = delete;
    joining_thread& operator=(const joining_thread&) = delete;
};
#else
using mutex_type = std::mutex;
using shared_mutex_type = std::shared_mutex;
using unique_lock = std::unique_lock<mutex_type>;
using shared_lock = std::shared_lock<shared_mutex_type>;
using lock_guard = std::lock_guard<mutex_type>;

template <typename T>
using future = std::future<T>;

template <typename T>
using promise = std::promise<T>;

using thread = std::thread;

template <typename Func, typename... Args>
auto make_thread(Func&& func, Args&&... args) {
    return std::thread(std::forward<Func>(func), std::forward<Args>(args)...);
}

using joining_thread = std::jthread;
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
template <typename T>
using atomic = boost::atomic<T>;

// Helper for lock-free operations
template <typename T>
class lockfree_queue {
private:
    boost::lockfree::queue<T> queue_;

public:
    explicit lockfree_queue(size_t size) : queue_(size) {}

    bool push(const T& value) { return queue_.push(value); }

    bool pop(T& value) { return queue_.pop(value); }

    bool empty() const { return queue_.empty(); }
};
#else
template <typename T>
using atomic = std::atomic<T>;

// Simple mutex-based queue as a fallback
template <typename T>
class lockfree_queue {
private:
    std::vector<T> queue_;
    mutable mutex_type mutex_;

public:
    explicit lockfree_queue(size_t) {}

    bool push(const T& value) {
        lock_guard lock(mutex_);
        queue_.push_back(value);
        return true;
    }

    bool pop(T& value) {
        lock_guard lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        value = queue_.front();
        queue_.erase(queue_.begin());
        return true;
    }

    bool empty() const {
        lock_guard lock(mutex_);
        return queue_.empty();
    }
};
#endif

// 添加针对共享互斥锁的锁类型
template <typename Mutex>
using unique_lock_t = std::unique_lock<Mutex>;

template <typename Mutex>
using shared_lock_t = std::shared_lock<Mutex>;
}  // namespace internal

/**
 * @brief Concept to check if a type can be invoked with a given parameter type.
 *
 * This concept checks if a std::function taking a parameter of type ParamType
 * is invocable with an instance of ParamType.
 *
 * @tparam ParamType The parameter type to check for.
 */
template <typename ParamType>
concept CallableWithParam = requires(ParamType p) {
    std::invoke(std::declval<std::function<void(ParamType)>>(), p);
};

/**
 * @brief Concept to check if a type is copyable.
 *
 * This concept ensures that the parameter type can be safely copied,
 * which is necessary for asynchronous operations.
 *
 * @tparam T The type to check.
 */
template <typename T>
concept CopyableType =
    std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>;

/**
 * @brief Exception class for Trigger-related errors.
 */
class TriggerException : public std::runtime_error {
public:
    explicit TriggerException(const std::string& message)
        : std::runtime_error(message) {
        // spdlog::debug("TriggerException created: {}", message); // Optional:
        // log creation
    }
};

/**
 * @brief A class for handling event-driven callbacks with parameter support.
 *
 * This class allows users to register, unregister, and trigger callbacks for
 * different events, providing a mechanism to manage callbacks with priorities
 * and delays.
 *
 * @tparam ParamType The type of parameter to be passed to the callbacks.
 */
template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
class Trigger {
public:
    using Callback = std::function<void(ParamType)>;  ///< Type alias for the
                                                      ///< callback function.
    using CallbackPtr =
        std::shared_ptr<Callback>;  ///< Smart pointer for callback management

    /// Enumeration for callback priority levels.
    enum class CallbackPriority { High, Normal, Low };

    /**
     * @brief Constructor.
     */
    Trigger() noexcept { spdlog::debug("Trigger instance created."); };

    /**
     * @brief Destructor that ensures all pending operations are completed.
     */
    ~Trigger() noexcept {
        spdlog::debug("Trigger instance destroyed, cancelling all triggers.");
        cancelAllTriggers();
    }

    // Rule of five
    Trigger(const Trigger&) = delete;
    Trigger& operator=(const Trigger&) = delete;
    Trigger(Trigger&&) noexcept = default;
    Trigger& operator=(Trigger&&) noexcept = default;

    /**
     * @brief Registers a callback for a specified event.
     *
     * @param event The name of the event for which the callback is registered.
     * @param callback The callback function to be executed when the event is
     * triggered.
     * @param priority The priority level of the callback (default is Normal).
     * @return A unique identifier for the registered callback.
     * @throws TriggerException if the event name is empty or callback is
     * invalid.
     */
    [[nodiscard]] std::size_t registerCallback(
        std::string_view event, Callback callback,
        CallbackPriority priority = CallbackPriority::Normal);

    /**
     * @brief Unregisters a callback for a specified event using its ID.
     *
     * @param event The name of the event from which the callback is
     * unregistered.
     * @param callbackId The unique identifier of the callback to be removed.
     * @return true if the callback was found and removed, false otherwise.
     */
    bool unregisterCallback(std::string_view event,
                            std::size_t callbackId) noexcept;

    /**
     * @brief Unregisters all callbacks for a specified event.
     *
     * @param event The name of the event from which all callbacks are
     * unregistered.
     * @return The number of callbacks that were unregistered.
     */
    std::size_t unregisterAllCallbacks(std::string_view event) noexcept;

    /**
     * @brief Triggers the callbacks associated with a specified event.
     *
     * @param event The name of the event to trigger.
     * @param param The parameter to be passed to the callbacks.
     * @return The number of callbacks that were executed.
     *
     * All callbacks registered for the event are executed with the provided
     * parameter.
     */
    std::size_t trigger(std::string_view event,
                        const ParamType& param) noexcept;

    /**
     * @brief Schedules a trigger for a specified event after a delay.
     *
     * @param event The name of the event to trigger.
     * @param param The parameter to be passed to the callbacks.
     * @param delay The delay after which to trigger the event, specified in
     * milliseconds.
     * @return A future that can be used to wait for or cancel the scheduled
     * trigger.
     */
    [[nodiscard]] std::shared_ptr<internal::atomic<bool>> scheduleTrigger(
        std::string event, ParamType param, std::chrono::milliseconds delay);

    /**
     * @brief Schedules an asynchronous trigger for a specified event.
     *
     * @param event The name of the event to trigger.
     * @param param The parameter to be passed to the callbacks.
     * @return A future representing the ongoing operation to trigger the event.
     */
    [[nodiscard]] internal::future<std::size_t> scheduleAsyncTrigger(
        std::string event, ParamType param);

    /**
     * @brief Cancels the scheduled trigger for a specified event.
     *
     * @param event The name of the event for which to cancel the trigger.
     * @return The number of pending triggers that were canceled.
     *
     * This will prevent the execution of any scheduled callbacks for the event.
     */
    std::size_t cancelTrigger(std::string_view event) noexcept;

    /**
     * @brief Cancels all scheduled triggers.
     *
     * @return The number of pending triggers that were canceled.
     *
     * This method clears all scheduled callbacks for any events.
     */
    std::size_t cancelAllTriggers() noexcept;

    /**
     * @brief Checks if the trigger has any registered callbacks for an event.
     *
     * @param event The name of the event to check.
     * @return true if there are callbacks registered for the event, false
     * otherwise.
     */
    [[nodiscard]] bool hasCallbacks(std::string_view event) const noexcept;

    /**
     * @brief Gets the number of registered callbacks for an event.
     *
     * @param event The name of the event to check.
     * @return The number of callbacks registered for the event.
     */
    [[nodiscard]] std::size_t callbackCount(
        std::string_view event) const noexcept;

#ifdef ATOM_USE_BOOST_LOCKFREE
    /**
     * @brief Creates a lock-free trigger queue for high-throughput event
     * handling
     *
     * This creates an optimized version of the trigger system for scenarios
     * requiring high-throughput event processing with minimal contention.
     *
     * @param queueSize The size of the internal lock-free queue
     * @return A unique pointer to the created trigger queue
     */
    [[nodiscard]] static std::unique_ptr<
        internal::lockfree_queue<std::pair<std::string, ParamType>>>
    createLockFreeTriggerQueue(std::size_t queueSize = 1024);

    /**
     * @brief Process events from a lock-free trigger queue
     *
     * @param queue The lock-free trigger queue to process
     * @param maxEvents Maximum number of events to process in one call (0 for
     * all available)
     * @return Number of events processed
     */
    std::size_t processLockFreeTriggers(
        internal::lockfree_queue<std::pair<std::string, ParamType>>& queue,
        std::size_t maxEvents = 0) noexcept;
#endif

private:
    struct CallbackInfo {
        CallbackPriority priority;
        std::size_t id;
        CallbackPtr callback;
    };

    mutable internal::shared_mutex_type
        m_mutex_;  ///< Read-write mutex for thread-safe access
    std::unordered_map<std::string, std::vector<CallbackInfo>>
        m_callbacks_;  ///< Map of events to their callbacks
    internal::atomic<std::size_t> m_next_id_{
        0};  ///< Counter for generating unique callback IDs
    std::unordered_map<std::string,
                       std::vector<std::shared_ptr<internal::atomic<bool>>>>
        m_pending_triggers_;  ///< Map of events to their pending triggers
};

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
[[nodiscard]] std::size_t Trigger<ParamType>::registerCallback(
    std::string_view event, Callback callback, CallbackPriority priority) {
    std::string event_str(event);
    if (event_str.empty()) {
        spdlog::error("Attempted to register callback with empty event name.");
        throw TriggerException("Event name cannot be empty");
    }
    if (!callback) {
        spdlog::error("Attempted to register null callback for event '{}'.",
                      event_str);
        throw TriggerException("Callback cannot be null");
    }

    spdlog::debug("Registering callback for event '{}' with priority {}.",
                  event_str, static_cast<int>(priority));
    internal::unique_lock_t<internal::shared_mutex_type> lock(m_mutex_);
    auto id = m_next_id_++;
    auto callbackPtr = std::make_shared<Callback>(std::move(callback));
    m_callbacks_[event_str].push_back({priority, id, std::move(callbackPtr)});
    spdlog::info("Registered callback ID {} for event '{}'.", id, event_str);
    return id;
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
bool Trigger<ParamType>::unregisterCallback(std::string_view event,
                                            std::size_t callbackId) noexcept {
    std::string event_str(event);
    if (event_str.empty()) {
        spdlog::warn("Attempted to unregister callback with empty event name.");
        return false;
    }
    spdlog::debug("Attempting to unregister callback ID {} for event '{}'.",
                  callbackId, event_str);

    internal::unique_lock_t<internal::shared_mutex_type> lock(m_mutex_);
    auto it = m_callbacks_.find(event_str);
    if (it == m_callbacks_.end()) {
        spdlog::warn(
            "Failed to unregister callback ID {}: event '{}' not found.",
            callbackId, event_str);
        return false;
    }

    auto& callbacks = it->second;
    auto callbackIt = std::find_if(
        callbacks.begin(), callbacks.end(),
        [callbackId](const auto& info) { return info.id == callbackId; });

    if (callbackIt == callbacks.end()) {
        spdlog::warn(
            "Failed to unregister callback: ID {} not found for event '{}'.",
            callbackId, event_str);
        return false;
    }

    callbacks.erase(callbackIt);
    spdlog::info("Unregistered callback ID {} for event '{}'.", callbackId,
                 event_str);
    return true;
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
std::size_t Trigger<ParamType>::unregisterAllCallbacks(
    std::string_view event) noexcept {
    std::string event_str(event);
    if (event_str.empty()) {
        spdlog::warn(
            "Attempted to unregister all callbacks with empty event name.");
        return 0;
    }
    spdlog::debug("Unregistering all callbacks for event '{}'.", event_str);

    internal::unique_lock_t<internal::shared_mutex_type> lock(m_mutex_);
    auto it = m_callbacks_.find(event_str);
    if (it == m_callbacks_.end()) {
        spdlog::debug("No callbacks found to unregister for event '{}'.",
                      event_str);
        return 0;
    }

    std::size_t count = it->second.size();
    m_callbacks_.erase(it);
    spdlog::info("Unregistered {} callbacks for event '{}'.", count, event_str);
    return count;
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
std::size_t Trigger<ParamType>::trigger(std::string_view event,
                                        const ParamType& param) noexcept {
    std::string event_str(event);
    if (event_str.empty()) {
        spdlog::warn("Attempted to trigger an empty event name.");
        return 0;
    }
    spdlog::trace("Triggering event '{}'.", event_str);

    std::vector<CallbackPtr> callbacksToExecute;
    {
        internal::shared_lock_t<internal::shared_mutex_type> lock(m_mutex_);
        auto it = m_callbacks_.find(event_str);
        if (it == m_callbacks_.end()) {
            spdlog::trace("No callbacks registered for event '{}'.", event_str);
            return 0;
        }

        auto sortedCallbacks = it->second;
        std::ranges::sort(sortedCallbacks,
                          [](const auto& cb1, const auto& cb2) {
                              return static_cast<int>(cb1.priority) <
                                     static_cast<int>(cb2.priority);
                          });

        callbacksToExecute.reserve(sortedCallbacks.size());
        for (const auto& info : sortedCallbacks) {
            callbacksToExecute.push_back(info.callback);
        }
    }
    spdlog::trace("Found {} callbacks for event '{}' to execute.",
                  callbacksToExecute.size(), event_str);

    std::size_t executedCount = 0;
    for (const auto& callback_ptr : callbacksToExecute) {
        try {
            if (callback_ptr && *callback_ptr) {
                (*callback_ptr)(param);
                ++executedCount;
            } else {
                spdlog::warn(
                    "Encountered null or empty callback pointer for event "
                    "'{}'.",
                    event_str);
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception in callback for event '{}': {}", event_str,
                          e.what());
        } catch (...) {
            spdlog::error("Unknown exception in callback for event '{}'.",
                          event_str);
        }
    }
    spdlog::debug("Executed {} callbacks for event '{}'.", executedCount,
                  event_str);
    return executedCount;
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
[[nodiscard]] std::shared_ptr<internal::atomic<bool>>
Trigger<ParamType>::scheduleTrigger(std::string event, ParamType param,
                                    std::chrono::milliseconds delay) {
    if (event.empty()) {
        spdlog::error("Attempted to schedule trigger with empty event name.");
        throw TriggerException("Event name cannot be empty");
    }
    if (delay < std::chrono::milliseconds(0)) {
        spdlog::error(
            "Attempted to schedule trigger for event '{}' with negative delay "
            "({}ms).",
            event, delay.count());
        throw TriggerException("Delay cannot be negative");
    }

    spdlog::debug("Scheduling trigger for event '{}' with delay {}ms.", event,
                  delay.count());
    auto cancelFlag = std::make_shared<internal::atomic<bool>>(false);

    {
        internal::unique_lock_t<internal::shared_mutex_type> lock(m_mutex_);
        m_pending_triggers_[event].push_back(cancelFlag);
    }

    // Capture event and param by value (they are already std::string and
    // ParamType)
    internal::joining_thread([this,
                              event_copy =
                                  event,  // Explicit copy for clarity if
                                          // needed, or rely on lambda capture
                              param_copy = param, delay, cancelFlag]() mutable {
        spdlog::trace("Scheduled trigger thread started for event '{}'.",
                      event_copy);
        if (cancelFlag->load(std::memory_order_acquire)) {
            spdlog::debug(
                "Scheduled trigger for event '{}' cancelled before sleep.",
                event_copy);
            // Clean up the cancel flag from m_pending_triggers_ if it was
            // cancelled early
            internal::unique_lock_t<internal::shared_mutex_type> lock(m_mutex_);
            auto it = m_pending_triggers_.find(event_copy);
            if (it != m_pending_triggers_.end()) {
                auto& flags = it->second;
                flags.erase(std::remove(flags.begin(), flags.end(), cancelFlag),
                            flags.end());
                if (flags.empty()) {
                    m_pending_triggers_.erase(it);
                }
            }
            return;
        }

        std::this_thread::sleep_for(delay);

        if (!cancelFlag->load(std::memory_order_acquire)) {
            spdlog::info(
                "Executing scheduled trigger for event '{}' after delay.",
                event_copy);
            trigger(event_copy,
                    param_copy);  // param_copy is moved if ParamType is movable
                                  // and trigger takes by value or copied if
                                  // trigger takes by const ref. Current trigger
                                  // takes by const ParamType& param

            internal::unique_lock_t<internal::shared_mutex_type> lock(m_mutex_);
            auto it = m_pending_triggers_.find(event_copy);
            if (it != m_pending_triggers_.end()) {
                auto& flags = it->second;
                flags.erase(std::remove(flags.begin(), flags.end(), cancelFlag),
                            flags.end());
                if (flags.empty()) {
                    m_pending_triggers_.erase(it);
                }
                spdlog::trace(
                    "Removed cancel flag for completed scheduled trigger of "
                    "event '{}'.",
                    event_copy);
            }
        } else {
            spdlog::debug(
                "Scheduled trigger for event '{}' cancelled after sleep, "
                "before execution.",
                event_copy);
            // Clean up the cancel flag if it was cancelled during/after sleep
            // but before execution
            internal::unique_lock_t<internal::shared_mutex_type> lock(m_mutex_);
            auto it = m_pending_triggers_.find(event_copy);
            if (it != m_pending_triggers_.end()) {
                auto& flags = it->second;
                flags.erase(std::remove(flags.begin(), flags.end(), cancelFlag),
                            flags.end());
                if (flags.empty()) {
                    m_pending_triggers_.erase(it);
                }
            }
        }
        spdlog::trace("Scheduled trigger thread finished for event '{}'.",
                      event_copy);
    }).detach();

    return cancelFlag;
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
[[nodiscard]] internal::future<std::size_t>
Trigger<ParamType>::scheduleAsyncTrigger(std::string event, ParamType param) {
    if (event.empty()) {
        spdlog::error(
            "Attempted to schedule async trigger with empty event name.");
        throw TriggerException("Event name cannot be empty");
    }
    spdlog::debug("Scheduling async trigger for event '{}'.", event);

    auto promise_ptr = std::make_shared<internal::promise<std::size_t>>();
    internal::future<std::size_t> future = promise_ptr->get_future();

    internal::joining_thread([this, event_copy = event, param_copy = param,
                              promise_ptr]() mutable {
        spdlog::trace("Async trigger thread started for event '{}'.",
                      event_copy);
        try {
            std::size_t count = trigger(event_copy, param_copy);
            promise_ptr->set_value(count);
            spdlog::trace(
                "Async trigger for event '{}' completed, {} callbacks "
                "executed.",
                event_copy, count);
        } catch (const std::exception& e) {
            spdlog::error(
                "Exception in async trigger execution for event '{}': {}",
                event_copy, e.what());
            try {
                promise_ptr->set_exception(std::current_exception());
            } catch (const std::future_error& fe) {
                spdlog::error(
                    "std::future_error setting exception for async trigger "
                    "promise (event '{}'): {}",
                    event_copy, fe.what());
            } catch (const std::exception& se) {
                spdlog::error(
                    "std::exception setting exception for async trigger "
                    "promise (event '{}'): {}",
                    event_copy, se.what());
            } catch (...) {
                spdlog::error(
                    "Unknown exception setting exception for async trigger "
                    "promise for event '{}'.",
                    event_copy);
            }
        } catch (...) {
            spdlog::error(
                "Unknown exception in async trigger execution for event '{}'.",
                event_copy);
            try {
                promise_ptr->set_exception(std::make_exception_ptr(
                    TriggerException("Unknown error in async trigger")));
            } catch (const std::future_error& fe) {
                spdlog::error(
                    "std::future_error setting exception for async trigger "
                    "promise (event '{}'): {}",
                    event_copy, fe.what());
            } catch (const std::exception& se) {
                spdlog::error(
                    "std::exception setting exception for async trigger "
                    "promise (event '{}'): {}",
                    event_copy, se.what());
            } catch (...) {
                spdlog::error(
                    "Unknown exception setting exception for async trigger "
                    "promise for event '{}' (after unknown trigger error).",
                    event_copy);
            }
        }
        spdlog::trace("Async trigger thread finished for event '{}'.",
                      event_copy);
    }).detach();

    return future;
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
std::size_t Trigger<ParamType>::cancelTrigger(std::string_view event) noexcept {
    std::string event_str(event);
    if (event_str.empty()) {
        spdlog::warn("Attempted to cancel trigger with empty event name.");
        return 0;
    }
    spdlog::debug("Cancelling scheduled triggers for event '{}'.", event_str);

    internal::unique_lock_t<internal::shared_mutex_type> lock(m_mutex_);
    auto it = m_pending_triggers_.find(event_str);
    if (it == m_pending_triggers_.end()) {
        spdlog::debug("No pending triggers found to cancel for event '{}'.",
                      event_str);
        return 0;
    }

    std::size_t canceledCount = 0;
    for (auto& flag_ptr : it->second) {
        if (flag_ptr) {
#ifdef ATOM_USE_BOOST_LOCKFREE
            flag_ptr->store(true, boost::memory_order_release);
#else
            flag_ptr->store(true, std::memory_order_release);
#endif
            ++canceledCount;
        }
    }

    m_pending_triggers_.erase(it);
    if (canceledCount > 0) {
        spdlog::info("Cancelled {} pending triggers for event '{}'.",
                     canceledCount, event_str);
    } else {
        spdlog::debug(
            "No active pending triggers were cancelled for event '{}' (flags "
            "might have been null or already processed).",
            event_str);
    }
    return canceledCount;
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
std::size_t Trigger<ParamType>::cancelAllTriggers() noexcept {
    spdlog::debug("Cancelling all scheduled triggers.");
    internal::unique_lock_t<internal::shared_mutex_type> lock(m_mutex_);
    std::size_t canceledCount = 0;

    for (auto& pair_event_flags : m_pending_triggers_) {
        for (auto& flag_ptr : pair_event_flags.second) {
            if (flag_ptr) {
#ifdef ATOM_USE_BOOST_LOCKFREE
                flag_ptr->store(true, boost::memory_order_release);
#else
                flag_ptr->store(true, std::memory_order_release);
#endif
                ++canceledCount;
            }
        }
    }

    m_pending_triggers_.clear();
    spdlog::info("Cancelled {} total pending triggers.", canceledCount);
    return canceledCount;
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
[[nodiscard]] bool Trigger<ParamType>::hasCallbacks(
    std::string_view event) const noexcept {
    std::string event_str(event);
    if (event_str.empty()) {
        // spdlog::trace("hasCallbacks check for empty event name."); // Too
        // verbose
        return false;
    }

    internal::shared_lock_t<internal::shared_mutex_type> lock(m_mutex_);
    auto it = m_callbacks_.find(event_str);
    bool found = it != m_callbacks_.end() && !it->second.empty();
    // spdlog::trace("hasCallbacks for event '{}': {}", event_str, found); //
    // Too verbose
    return found;
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
[[nodiscard]] std::size_t Trigger<ParamType>::callbackCount(
    std::string_view event) const noexcept {
    std::string event_str(event);
    if (event_str.empty()) {
        // spdlog::trace("callbackCount check for empty event name."); // Too
        // verbose
        return 0;
    }

    internal::shared_lock_t<internal::shared_mutex_type> lock(m_mutex_);
    auto it = m_callbacks_.find(event_str);
    size_t count = it != m_callbacks_.end() ? it->second.size() : 0;
    // spdlog::trace("callbackCount for event '{}': {}", event_str, count); //
    // Too verbose
    return count;
}

#ifdef ATOM_USE_BOOST_LOCKFREE
template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
[[nodiscard]] std::unique_ptr<
    internal::lockfree_queue<std::pair<std::string, ParamType>>>
Trigger<ParamType>::createLockFreeTriggerQueue(std::size_t queueSize) {
    spdlog::debug("Creating lock-free trigger queue with size {}.", queueSize);
    return std::make_unique<
        internal::lockfree_queue<std::pair<std::string, ParamType>>>(queueSize);
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
std::size_t Trigger<ParamType>::processLockFreeTriggers(
    internal::lockfree_queue<std::pair<std::string, ParamType>>& queue,
    std::size_t maxEvents) noexcept {
    spdlog::trace("Processing lock-free triggers, maxEvents: {}.", maxEvents);
    std::size_t processedCount = 0;
    std::pair<std::string, ParamType> eventData;

    while ((maxEvents == 0 || processedCount < maxEvents) &&
           queue.pop(eventData)) {
        spdlog::trace("Popped event '{}' from lock-free queue.",
                      eventData.first);
        processedCount += trigger(eventData.first, eventData.second);
    }
    if (processedCount > 0) {
        spdlog::debug("Processed {} events from lock-free queue.",
                      processedCount);
    } else {
        spdlog::trace("No events processed from lock-free queue in this call.");
    }
    return processedCount;
}
#endif

}  // namespace atom::async

#endif  // ATOM_ASYNC_TRIGGER_HPP
