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

namespace atom::async {

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
        : std::runtime_error(message) {}
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
    Trigger() noexcept = default;

    /**
     * @brief Destructor that ensures all pending operations are completed.
     */
    ~Trigger() noexcept { cancelAllTriggers(); }

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
    [[nodiscard]] std::shared_ptr<std::atomic<bool>> scheduleTrigger(
        std::string event, ParamType param, std::chrono::milliseconds delay);

    /**
     * @brief Schedules an asynchronous trigger for a specified event.
     *
     * @param event The name of the event to trigger.
     * @param param The parameter to be passed to the callbacks.
     * @return A future representing the ongoing operation to trigger the event.
     */
    [[nodiscard]] std::future<std::size_t> scheduleAsyncTrigger(
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

private:
    struct CallbackInfo {
        CallbackPriority priority;
        std::size_t id;
        CallbackPtr callback;
    };

    mutable std::shared_mutex
        m_mutex_;  ///< Read-write mutex for thread-safe access
    std::unordered_map<std::string, std::vector<CallbackInfo>>
        m_callbacks_;  ///< Map of events to their callbacks
    std::atomic<std::size_t> m_next_id_{
        0};  ///< Counter for generating unique callback IDs
    std::unordered_map<std::string,
                       std::vector<std::shared_ptr<std::atomic<bool>>>>
        m_pending_triggers_;  ///< Map of events to their pending triggers
};

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
[[nodiscard]] std::size_t Trigger<ParamType>::registerCallback(
    std::string_view event, Callback callback, CallbackPriority priority) {
    if (event.empty()) {
        throw TriggerException("Event name cannot be empty");
    }
    if (!callback) {
        throw TriggerException("Callback cannot be null");
    }

    std::unique_lock lock(m_mutex_);
    auto id = m_next_id_++;
    auto callbackPtr = std::make_shared<Callback>(std::move(callback));
    m_callbacks_[std::string(event)].push_back(
        {priority, id, std::move(callbackPtr)});
    return id;
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
bool Trigger<ParamType>::unregisterCallback(std::string_view event,
                                            std::size_t callbackId) noexcept {
    if (event.empty()) {
        return false;
    }

    std::unique_lock lock(m_mutex_);
    auto it = m_callbacks_.find(std::string(event));
    if (it == m_callbacks_.end()) {
        return false;
    }

    auto& callbacks = it->second;
    auto callbackIt = std::find_if(
        callbacks.begin(), callbacks.end(),
        [callbackId](const auto& info) { return info.id == callbackId; });

    if (callbackIt == callbacks.end()) {
        return false;
    }

    callbacks.erase(callbackIt);
    return true;
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
std::size_t Trigger<ParamType>::unregisterAllCallbacks(
    std::string_view event) noexcept {
    if (event.empty()) {
        return 0;
    }

    std::unique_lock lock(m_mutex_);
    auto it = m_callbacks_.find(std::string(event));
    if (it == m_callbacks_.end()) {
        return 0;
    }

    std::size_t count = it->second.size();
    m_callbacks_.erase(it);
    return count;
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
std::size_t Trigger<ParamType>::trigger(std::string_view event,
                                        const ParamType& param) noexcept {
    if (event.empty()) {
        return 0;
    }

    // Create a local copy of callbacks to avoid holding the lock while calling
    // them
    std::vector<CallbackPtr> callbacksToExecute;
    {
        std::shared_lock lock(m_mutex_);
        auto it = m_callbacks_.find(std::string(event));
        if (it == m_callbacks_.end()) {
            return 0;
        }

        // Sort callbacks by priority (High -> Normal -> Low)
        auto sortedCallbacks = it->second;
        std::ranges::sort(sortedCallbacks,
                          [](const auto& cb1, const auto& cb2) {
                              return static_cast<int>(cb1.priority) <
                                     static_cast<int>(cb2.priority);
                          });

        // Extract the callback pointers
        callbacksToExecute.reserve(sortedCallbacks.size());
        for (const auto& info : sortedCallbacks) {
            callbacksToExecute.push_back(info.callback);
        }
    }

    // Execute callbacks outside the lock
    std::size_t executedCount = 0;
    for (const auto& callback : callbacksToExecute) {
        try {
            if (callback) {
                (*callback)(param);
                ++executedCount;
            }
        } catch (const std::exception& e) {
            // Log exception but continue with other callbacks
            // In real implementation, use proper logging
        } catch (...) {
            // Catch all other exceptions
        }
    }

    return executedCount;
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
[[nodiscard]] std::shared_ptr<std::atomic<bool>>
Trigger<ParamType>::scheduleTrigger(std::string event, ParamType param,
                                    std::chrono::milliseconds delay) {
    if (event.empty()) {
        throw TriggerException("Event name cannot be empty");
    }
    if (delay < std::chrono::milliseconds(0)) {
        throw TriggerException("Delay cannot be negative");
    }

    auto cancelFlag = std::make_shared<std::atomic<bool>>(false);

    {
        std::unique_lock lock(m_mutex_);
        m_pending_triggers_[event].push_back(cancelFlag);
    }

    std::jthread([this, event = std::move(event), param = std::move(param),
                  delay, cancelFlag]() mutable {
        // Early check before sleep
        if (*cancelFlag) {
            return;
        }

        std::this_thread::sleep_for(delay);

        if (!(*cancelFlag)) {
            trigger(event, param);

            // Remove the cancel flag from pending triggers
            std::unique_lock lock(m_mutex_);
            auto it = m_pending_triggers_.find(event);
            if (it != m_pending_triggers_.end()) {
                auto& flags = it->second;
                flags.erase(std::remove(flags.begin(), flags.end(), cancelFlag),
                            flags.end());
                if (flags.empty()) {
                    m_pending_triggers_.erase(it);
                }
            }
        }
    }).detach();

    return cancelFlag;
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
[[nodiscard]] std::future<std::size_t> Trigger<ParamType>::scheduleAsyncTrigger(
    std::string event, ParamType param) {
    if (event.empty()) {
        throw TriggerException("Event name cannot be empty");
    }

    auto promise = std::make_shared<std::promise<std::size_t>>();
    std::future<std::size_t> future = promise->get_future();

    std::jthread([this, event = std::move(event), param = std::move(param),
                  promise]() mutable {
        try {
            std::size_t count = trigger(event, param);
            promise->set_value(count);
        } catch (...) {
            try {
                promise->set_exception(std::current_exception());
            } catch (...) {
                // Handle any exception from set_exception
            }
        }
    }).detach();

    return future;
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
std::size_t Trigger<ParamType>::cancelTrigger(std::string_view event) noexcept {
    if (event.empty()) {
        return 0;
    }

    std::unique_lock lock(m_mutex_);
    auto it = m_pending_triggers_.find(std::string(event));
    if (it == m_pending_triggers_.end()) {
        return 0;
    }

    std::size_t canceledCount = 0;
    for (auto& flag : it->second) {
        flag->store(true, std::memory_order_release);
        ++canceledCount;
    }

    m_pending_triggers_.erase(it);
    return canceledCount;
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
std::size_t Trigger<ParamType>::cancelAllTriggers() noexcept {
    std::unique_lock lock(m_mutex_);
    std::size_t canceledCount = 0;

    for (auto& [event, flags] : m_pending_triggers_) {
        for (auto& flag : flags) {
            flag->store(true, std::memory_order_release);
            ++canceledCount;
        }
    }

    m_pending_triggers_.clear();
    return canceledCount;
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
[[nodiscard]] bool Trigger<ParamType>::hasCallbacks(
    std::string_view event) const noexcept {
    if (event.empty()) {
        return false;
    }

    std::shared_lock lock(m_mutex_);
    auto it = m_callbacks_.find(std::string(event));
    return it != m_callbacks_.end() && !it->second.empty();
}

template <typename ParamType>
    requires CallableWithParam<ParamType> && CopyableType<ParamType>
[[nodiscard]] std::size_t Trigger<ParamType>::callbackCount(
    std::string_view event) const noexcept {
    if (event.empty()) {
        return 0;
    }

    std::shared_lock lock(m_mutex_);
    auto it = m_callbacks_.find(std::string(event));
    return it != m_callbacks_.end() ? it->second.size() : 0;
}

}  // namespace atom::async

#endif  // ATOM_ASYNC_TRIGGER_HPP
