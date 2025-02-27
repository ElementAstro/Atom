/*
 * eventstack.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-26

Description: A thread-safe stack data structure for managing events.

**************************************************/

#ifndef ATOM_ASYNC_EVENTSTACK_HPP
#define ATOM_ASYNC_EVENTSTACK_HPP

#include <algorithm>
#include <atomic>
#include <concepts>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if __has_include(<execution>)
#include <execution>
#define HAS_EXECUTION_HEADER 1
#else
#define HAS_EXECUTION_HEADER 0
#endif

namespace atom::async {

// Custom exceptions for EventStack
class EventStackException : public std::runtime_error {
public:
    explicit EventStackException(const std::string& message)
        : std::runtime_error(message) {}
};

class EventStackEmptyException : public EventStackException {
public:
    EventStackEmptyException()
        : EventStackException("Attempted operation on empty EventStack") {}
};

class EventStackSerializationException : public EventStackException {
public:
    explicit EventStackSerializationException(const std::string& message)
        : EventStackException("Serialization error: " + message) {}
};

// Concept for serializable types
template <typename T>
concept Serializable = requires(T a, std::string& s) {
    { std::to_string(a) } -> std::convertible_to<std::string>;
    // Or alternatively check for serialization methods if they exist
};

// Concept for comparable types
template <typename T>
concept Comparable = requires(T a, T b) {
    { a == b } -> std::convertible_to<bool>;
    { a < b } -> std::convertible_to<bool>;
};

/**
 * @brief A thread-safe stack data structure for managing events.
 *
 * @tparam T The type of events to store.
 */
template <typename T>
    requires std::copyable<T> && std::movable<T>
class EventStack {
public:
    EventStack() = default;
    ~EventStack() = default;

    // Rule of five: explicitly define copy constructor, copy assignment
    // operator, move constructor, and move assignment operator.
    EventStack(const EventStack& other) noexcept(
        std::is_nothrow_copy_constructible_v<T>);
    EventStack& operator=(const EventStack& other) noexcept(
        std::is_nothrow_copy_assignable_v<T>);
    EventStack(EventStack&& other) noexcept;
    EventStack& operator=(EventStack&& other) noexcept;

    // C++20 three-way comparison operator
    auto operator<=>(const EventStack& other) const =
        delete;  // Custom implementation needed if required

    /**
     * @brief Pushes an event onto the stack.
     *
     * @param event The event to push.
     * @throws std::bad_alloc If memory allocation fails.
     */
    void pushEvent(T event);

    /**
     * @brief Pops an event from the stack.
     *
     * @return The popped event, or std::nullopt if the stack is empty.
     */
    [[nodiscard]] auto popEvent() noexcept -> std::optional<T>;

#if ENABLE_DEBUG
    /**
     * @brief Prints all events in the stack.
     */
    void printEvents() const;
#endif

    /**
     * @brief Checks if the stack is empty.
     *
     * @return true if the stack is empty, false otherwise.
     */
    [[nodiscard]] auto isEmpty() const noexcept -> bool;

    /**
     * @brief Returns the number of events in the stack.
     *
     * @return The number of events.
     */
    [[nodiscard]] auto size() const noexcept -> size_t;

    /**
     * @brief Clears all events from the stack.
     */
    void clearEvents() noexcept;

    /**
     * @brief Returns the top event in the stack without removing it.
     *
     * @return The top event, or std::nullopt if the stack is empty.
     * @throws EventStackEmptyException if the stack is empty and exceptions are
     * enabled.
     */
    [[nodiscard]] auto peekTopEvent() const -> std::optional<T>;

    /**
     * @brief Copies the current stack.
     *
     * @return A copy of the stack.
     */
    [[nodiscard]] auto copyStack() const
        noexcept(std::is_nothrow_copy_constructible_v<T>) -> EventStack<T>;

    /**
     * @brief Filters events based on a custom filter function.
     *
     * @param filterFunc The filter function.
     * @throws std::bad_function_call If filterFunc is invalid.
     */
    template <typename Func>
        requires std::invocable<Func&, const T&> &&
                 std::same_as<std::invoke_result_t<Func&, const T&>, bool>
    void filterEvents(Func&& filterFunc);

    /**
     * @brief Serializes the stack into a string.
     *
     * @return The serialized stack.
     * @throws EventStackSerializationException If serialization fails.
     */
    [[nodiscard]] auto serializeStack() const -> std::string
        requires Serializable<T>;

    /**
     * @brief Deserializes a string into the stack.
     *
     * @param serializedData The serialized stack data.
     * @throws EventStackSerializationException If deserialization fails.
     */
    void deserializeStack(std::string_view serializedData)
        requires Serializable<T>;

    /**
     * @brief Removes duplicate events from the stack.
     */
    void removeDuplicates()
        requires Comparable<T>;

    /**
     * @brief Sorts the events in the stack based on a custom comparison
     * function.
     *
     * @param compareFunc The comparison function.
     * @throws std::bad_function_call If compareFunc is invalid.
     */
    template <typename Func>
        requires std::invocable<Func&, const T&, const T&> &&
                 std::same_as<std::invoke_result_t<Func&, const T&, const T&>,
                              bool>
    void sortEvents(Func&& compareFunc);

    /**
     * @brief Reverses the order of events in the stack.
     */
    void reverseEvents() noexcept;

    /**
     * @brief Counts the number of events that satisfy a predicate.
     *
     * @param predicate The predicate function.
     * @return The count of events satisfying the predicate.
     * @throws std::bad_function_call If predicate is invalid.
     */
    template <typename Func>
        requires std::invocable<Func&, const T&> &&
                     std::same_as<std::invoke_result_t<Func&, const T&>, bool>
    [[nodiscard]] auto countEvents(Func&& predicate) const -> size_t;

    /**
     * @brief Finds the first event that satisfies a predicate.
     *
     * @param predicate The predicate function.
     * @return The first event satisfying the predicate, or std::nullopt if not
     * found.
     * @throws std::bad_function_call If predicate is invalid.
     */
    template <typename Func>
        requires std::invocable<Func&, const T&> &&
                     std::same_as<std::invoke_result_t<Func&, const T&>, bool>
    [[nodiscard]] auto findEvent(Func&& predicate) const -> std::optional<T>;

    /**
     * @brief Checks if any event in the stack satisfies a predicate.
     *
     * @param predicate The predicate function.
     * @return true if any event satisfies the predicate, false otherwise.
     * @throws std::bad_function_call If predicate is invalid.
     */
    template <typename Func>
        requires std::invocable<Func&, const T&> &&
                     std::same_as<std::invoke_result_t<Func&, const T&>, bool>
    [[nodiscard]] auto anyEvent(Func&& predicate) const -> bool;

    /**
     * @brief Checks if all events in the stack satisfy a predicate.
     *
     * @param predicate The predicate function.
     * @return true if all events satisfy the predicate, false otherwise.
     * @throws std::bad_function_call If predicate is invalid.
     */
    template <typename Func>
        requires std::invocable<Func&, const T&> &&
                     std::same_as<std::invoke_result_t<Func&, const T&>, bool>
    [[nodiscard]] auto allEvents(Func&& predicate) const -> bool;

    /**
     * @brief Returns a span view of the events.
     *
     * @return A span view of the events.
     */
    [[nodiscard]] auto getEventsView() const noexcept -> std::span<const T>;

    /**
     * @brief Applies a function to each event in the stack.
     *
     * @param func The function to apply.
     * @throws std::bad_function_call If func is invalid.
     */
    template <typename Func>
        requires std::invocable<Func&, const T&>
    void forEach(Func&& func) const;

    /**
     * @brief Transforms events using the provided function.
     *
     * @param transformFunc The function to transform events.
     * @throws std::bad_function_call If transformFunc is invalid.
     */
    template <typename Func>
        requires std::invocable<Func&, T&>
    void transformEvents(Func&& transformFunc);

private:
    std::vector<T> events_;             /**< Vector to store events. */
    mutable std::shared_mutex mtx_;     /**< Mutex for thread safety. */
    std::atomic<size_t> eventCount_{0}; /**< Atomic counter for event count. */
};

// Copy constructor
template <typename T>
    requires std::copyable<T> && std::movable<T>
EventStack<T>::EventStack(const EventStack& other) noexcept(
    std::is_nothrow_copy_constructible_v<T>) {
    try {
        std::shared_lock lock(other.mtx_);
        events_ = other.events_;
        eventCount_.store(other.eventCount_.load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
    } catch (...) {
        // In case of exception, ensure count is 0
        eventCount_.store(0, std::memory_order_relaxed);
        throw;  // Re-throw the exception
    }
}

// Copy assignment operator
template <typename T>
    requires std::copyable<T> && std::movable<T>
EventStack<T>& EventStack<T>::operator=(const EventStack& other) noexcept(
    std::is_nothrow_copy_assignable_v<T>) {
    if (this != &other) {
        try {
            std::unique_lock lock1(mtx_, std::defer_lock);
            std::shared_lock lock2(other.mtx_, std::defer_lock);
            std::lock(lock1, lock2);
            events_ = other.events_;
            eventCount_.store(other.eventCount_.load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
        } catch (...) {
            // In case of exception, we keep the original state
            // No need to adjust eventCount_ as we didn't modify events_
            throw;  // Re-throw the exception
        }
    }
    return *this;
}

// Move constructor
template <typename T>
    requires std::copyable<T> && std::movable<T>
EventStack<T>::EventStack(EventStack&& other) noexcept {
    std::unique_lock lock(other.mtx_);
    events_ = std::move(other.events_);
    eventCount_.store(other.eventCount_.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
    other.eventCount_.store(0, std::memory_order_relaxed);
}

// Move assignment operator
template <typename T>
    requires std::copyable<T> && std::movable<T>
EventStack<T>& EventStack<T>::operator=(EventStack&& other) noexcept {
    if (this != &other) {
        std::unique_lock lock1(mtx_, std::defer_lock);
        std::unique_lock lock2(other.mtx_, std::defer_lock);
        std::lock(lock1, lock2);
        events_ = std::move(other.events_);
        eventCount_.store(other.eventCount_.load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
        other.eventCount_.store(0, std::memory_order_relaxed);
    }
    return *this;
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
void EventStack<T>::pushEvent(T event) {
    try {
        std::unique_lock lock(mtx_);
        events_.push_back(std::move(event));
        ++eventCount_;
    } catch (const std::exception& e) {
        throw EventStackException(std::string("Failed to push event: ") +
                                  e.what());
    }
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
auto EventStack<T>::popEvent() noexcept -> std::optional<T> {
    std::unique_lock lock(mtx_);
    if (!events_.empty()) {
        T event = std::move(events_.back());
        events_.pop_back();
        --eventCount_;
        return event;
    }
    return std::nullopt;
}

#if ENABLE_DEBUG
template <typename T>
    requires std::copyable<T> && std::movable<T>
void EventStack<T>::printEvents() const {
    std::shared_lock lock(mtx_);
    std::cout << "Events in stack:" << std::endl;
    for (const T& event : events_) {
        std::cout << event << std::endl;
    }
}
#endif

template <typename T>
    requires std::copyable<T> && std::movable<T>
auto EventStack<T>::isEmpty() const noexcept -> bool {
    std::shared_lock lock(mtx_);
    return events_.empty();
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
auto EventStack<T>::size() const noexcept -> size_t {
    return eventCount_.load(std::memory_order_relaxed);
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
void EventStack<T>::clearEvents() noexcept {
    std::unique_lock lock(mtx_);
    events_.clear();
    eventCount_.store(0, std::memory_order_relaxed);
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
auto EventStack<T>::peekTopEvent() const -> std::optional<T> {
    std::shared_lock lock(mtx_);
    if (!events_.empty()) {
        return events_.back();
    }
    return std::nullopt;
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
auto EventStack<T>::copyStack() const
    noexcept(std::is_nothrow_copy_constructible_v<T>) -> EventStack<T> {
    std::shared_lock lock(mtx_);
    EventStack<T> newStack;
    newStack.events_ = events_;
    newStack.eventCount_.store(eventCount_.load(std::memory_order_relaxed),
                               std::memory_order_relaxed);
    return newStack;
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
                             template <typename Func>
                 requires std::invocable<Func&, const T&> &&
                          std::same_as<std::invoke_result_t<Func&, const T&>,
                                       bool>
void EventStack<T>::filterEvents(Func&& filterFunc) {
    try {
        std::unique_lock lock(mtx_);
        auto newEnd = std::remove_if(
            events_.begin(), events_.end(),
            [&](const T& event) { return !std::invoke(filterFunc, event); });
        events_.erase(newEnd, events_.end());
        eventCount_.store(events_.size(), std::memory_order_relaxed);
    } catch (const std::exception& e) {
        throw EventStackException(std::string("Failed to filter events: ") +
                                  e.what());
    }
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
             auto EventStack<T>::serializeStack() const -> std::string
                 requires Serializable<T>
{
    try {
        std::shared_lock lock(mtx_);
        std::string serializedStack;
        const size_t estimatedSize =
            events_.size() *
            (sizeof(T) > 8 ? sizeof(T) : 8);  // Reasonable estimate
        serializedStack.reserve(estimatedSize);

        for (const T& event : events_) {
            serializedStack += std::to_string(event) + ";";
        }
        return serializedStack;
    } catch (const std::exception& e) {
        throw EventStackSerializationException(e.what());
    }
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
                             void EventStack<T>::deserializeStack(
                                 std::string_view serializedData)
                 requires Serializable<T>
{
    try {
        std::unique_lock lock(mtx_);
        events_.clear();

        // Estimate the number of items to avoid frequent reallocations
        const size_t estimatedCount =
            std::count(serializedData.begin(), serializedData.end(), ';');
        events_.reserve(estimatedCount);

        size_t pos = 0;
        size_t nextPos = 0;
        while ((nextPos = serializedData.find(';', pos)) !=
               std::string_view::npos) {
            if (nextPos > pos) {  // Skip empty entries
                std::string token(serializedData.substr(pos, nextPos - pos));
                // Conversion from string to T requires custom implementation
                // This is a simplistic approach and might need customization
                T event = token;  // Assuming T can be constructed from string
                events_.push_back(std::move(event));
            }
            pos = nextPos + 1;
        }
        eventCount_.store(events_.size(), std::memory_order_relaxed);
    } catch (const std::exception& e) {
        throw EventStackSerializationException(e.what());
    }
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
                             void EventStack<T>::removeDuplicates()
                 requires Comparable<T>
{
    try {
        std::unique_lock lock(mtx_);

#if HAS_EXECUTION_HEADER
        std::sort(std::execution::par_unseq, events_.begin(), events_.end());
#else
        std::sort(events_.begin(), events_.end());
#endif

        auto newEnd = std::unique(events_.begin(), events_.end());
        events_.erase(newEnd, events_.end());
        eventCount_.store(events_.size(), std::memory_order_relaxed);
    } catch (const std::exception& e) {
        throw EventStackException(std::string("Failed to remove duplicates: ") +
                                  e.what());
    }
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
                             template <typename Func>
                 requires std::invocable<Func&, const T&, const T&> &&
                          std::same_as<
                              std::invoke_result_t<Func&, const T&, const T&>,
                              bool>
void EventStack<T>::sortEvents(Func&& compareFunc) {
    try {
        std::unique_lock lock(mtx_);

#if HAS_EXECUTION_HEADER
        std::sort(std::execution::par_unseq, events_.begin(), events_.end(),
                  std::forward<Func>(compareFunc));
#else
        std::sort(events_.begin(), events_.end(),
                  std::forward<Func>(compareFunc));
#endif

    } catch (const std::exception& e) {
        throw EventStackException(std::string("Failed to sort events: ") +
                                  e.what());
    }
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
void EventStack<T>::reverseEvents() noexcept {
    std::unique_lock lock(mtx_);
    std::reverse(events_.begin(), events_.end());
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
                             template <typename Func>
                 requires std::invocable<Func&, const T&> &&
                              std::same_as<
                                  std::invoke_result_t<Func&, const T&>, bool>
auto EventStack<T>::countEvents(Func&& predicate) const -> size_t {
    try {
        std::shared_lock lock(mtx_);

#if HAS_EXECUTION_HEADER
        return std::count_if(std::execution::par_unseq, events_.begin(),
                             events_.end(), std::forward<Func>(predicate));
#else
        return std::count_if(events_.begin(), events_.end(),
                             std::forward<Func>(predicate));
#endif

    } catch (const std::exception& e) {
        throw EventStackException(std::string("Failed to count events: ") +
                                  e.what());
    }
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
                             template <typename Func>
                 requires std::invocable<Func&, const T&> &&
                              std::same_as<
                                  std::invoke_result_t<Func&, const T&>, bool>
auto EventStack<T>::findEvent(Func&& predicate) const -> std::optional<T> {
    try {
        std::shared_lock lock(mtx_);
        auto iterator = std::find_if(events_.begin(), events_.end(),
                                     std::forward<Func>(predicate));
        if (iterator != events_.end()) {
            return *iterator;
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        throw EventStackException(std::string("Failed to find event: ") +
                                  e.what());
    }
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
                             template <typename Func>
                 requires std::invocable<Func&, const T&> &&
                              std::same_as<
                                  std::invoke_result_t<Func&, const T&>, bool>
auto EventStack<T>::anyEvent(Func&& predicate) const -> bool {
    try {
        std::shared_lock lock(mtx_);

#if HAS_EXECUTION_HEADER
        return std::any_of(std::execution::par_unseq, events_.begin(),
                           events_.end(), std::forward<Func>(predicate));
#else
        return std::any_of(events_.begin(), events_.end(),
                           std::forward<Func>(predicate));
#endif

    } catch (const std::exception& e) {
        throw EventStackException(std::string("Failed to check any event: ") +
                                  e.what());
    }
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
                             template <typename Func>
                 requires std::invocable<Func&, const T&> &&
                              std::same_as<
                                  std::invoke_result_t<Func&, const T&>, bool>
auto EventStack<T>::allEvents(Func&& predicate) const -> bool {
    try {
        std::shared_lock lock(mtx_);

#if HAS_EXECUTION_HEADER
        return std::all_of(std::execution::par_unseq, events_.begin(),
                           events_.end(), std::forward<Func>(predicate));
#else
        return std::all_of(events_.begin(), events_.end(),
                           std::forward<Func>(predicate));
#endif

    } catch (const std::exception& e) {
        throw EventStackException(std::string("Failed to check all events: ") +
                                  e.what());
    }
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
auto EventStack<T>::getEventsView() const noexcept -> std::span<const T> {
    std::shared_lock lock(mtx_);
    return std::span<const T>(events_.data(), events_.size());
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
                             template <typename Func>
                 requires std::invocable<Func&, const T&>
void EventStack<T>::forEach(Func&& func) const {
    try {
        std::shared_lock lock(mtx_);

#if HAS_EXECUTION_HEADER
        std::for_each(std::execution::par_unseq, events_.begin(), events_.end(),
                      std::forward<Func>(func));
#else
        std::for_each(events_.begin(), events_.end(), std::forward<Func>(func));
#endif

    } catch (const std::exception& e) {
        throw EventStackException(
            std::string("Failed to apply function to each event: ") + e.what());
    }
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
                             template <typename Func>
                 requires std::invocable<Func&, T&>
void EventStack<T>::transformEvents(Func&& transformFunc) {
    try {
        std::unique_lock lock(mtx_);

#if HAS_EXECUTION_HEADER
        std::for_each(std::execution::par_unseq, events_.begin(), events_.end(),
                      std::forward<Func>(transformFunc));
#else
        std::for_each(events_.begin(), events_.end(),
                      std::forward<Func>(transformFunc));
#endif

    } catch (const std::exception& e) {
        throw EventStackException(std::string("Failed to transform events: ") +
                                  e.what());
    }
}

}  // namespace atom::async

#endif  // ATOM_ASYNC_EVENTSTACK_HPP
