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
#include <functional>  // Required for std::function
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if __has_include(<execution>)
#define HAS_EXECUTION_HEADER 1
#else
#define HAS_EXECUTION_HEADER 0
#endif

#if defined(USE_BOOST_LOCKFREE)
#include <boost/lockfree/stack.hpp>
#define ATOM_ASYNC_USE_LOCKFREE 1
#else
#define ATOM_ASYNC_USE_LOCKFREE 0
#endif

// 引入并行处理组件
#include "parallel.hpp"

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
concept Serializable = requires(T a) {
    { std::to_string(a) } -> std::convertible_to<std::string>;
} || std::same_as<T, std::string>;  // Special case for strings

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
    EventStack()
#if ATOM_ASYNC_USE_LOCKFREE
#if ATOM_ASYNC_LOCKFREE_BOUNDED
        : events_(ATOM_ASYNC_LOCKFREE_CAPACITY)
#else
        : events_(ATOM_ASYNC_LOCKFREE_CAPACITY)
#endif
#endif
    {
    }
    ~EventStack() = default;

    // Rule of five: explicitly define copy constructor, copy assignment
    // operator, move constructor, and move assignment operator.
#if !ATOM_ASYNC_USE_LOCKFREE
    EventStack(const EventStack& other) noexcept(false);  // Changed for rethrow
    EventStack& operator=(const EventStack& other) noexcept(
        false);                               // Changed for rethrow
    EventStack(EventStack&& other) noexcept;  // Assumes vector move is noexcept
    EventStack& operator=(
        EventStack&& other) noexcept;  // Assumes vector move is noexcept
#else
    // Lock-free stack is typically non-copyable. Movable is fine.
    EventStack(const EventStack& other) = delete;
    EventStack& operator=(const EventStack& other) = delete;
    EventStack(EventStack&&
                   other) noexcept {  // Based on boost::lockfree::stack's move
        // This requires careful implementation if eventCount_ is to be
        // consistent For simplicity, assuming boost::lockfree::stack handles
        // its internal state on move. The user would need to manage eventCount_
        // consistency if it's critical after move. A full implementation would
        // involve draining other.events_ and pushing to this->events_ and
        // managing eventCount_ carefully. boost::lockfree::stack itself is
        // movable.
        if (this != &other) {
            // events_ = std::move(other.events_); // boost::lockfree::stack is
            // movable For now, to make it compile, let's clear and copy (not
            // ideal for lock-free) This is a placeholder for a proper lock-free
            // move or making it non-movable too.
            T elem;
            while (events_.pop(elem)) {
            }  // Clear current
            std::vector<T> temp_elements;
            // Draining 'other' in a move constructor is unusual.
            // This section needs a proper lock-free move strategy.
            // For now, let's make it simple and potentially inefficient or
            // incorrect for true lock-free semantics.
            while (other.events_.pop(elem)) {
                temp_elements.push_back(elem);
            }
            std::reverse(temp_elements.begin(), temp_elements.end());
            for (const auto& item : temp_elements) {
                events_.push(item);
            }
            eventCount_.store(other.eventCount_.load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
            other.eventCount_.store(0, std::memory_order_relaxed);
        }
    }
    EventStack& operator=(EventStack&& other) noexcept {
        if (this != &other) {
            T elem;
            while (events_.pop(elem)) {
            }  // Clear current
            std::vector<T> temp_elements;
            // Draining 'other' in a move assignment is unusual.
            while (other.events_.pop(elem)) {
                temp_elements.push_back(elem);
            }
            std::reverse(temp_elements.begin(), temp_elements.end());
            for (const auto& item : temp_elements) {
                events_.push(item);
            }
            eventCount_.store(other.eventCount_.load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
            other.eventCount_.store(0, std::memory_order_relaxed);
        }
        return *this;
    }
#endif

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
#if ATOM_ASYNC_USE_LOCKFREE
    boost::lockfree::stack<T> events_{128};  // Initial capacity hint
    std::atomic<size_t> eventCount_{0};

    // Helper method for operations that need access to all elements
    std::vector<T> drainStack() {
        std::vector<T> result;
        result.reserve(eventCount_.load(std::memory_order_relaxed));
        T elem;
        while (events_.pop(elem)) {
            result.push_back(std::move(elem));
        }
        // Order is reversed compared to original stack
        std::reverse(result.begin(), result.end());
        return result;
    }

    // Refill stack from vector (preserves order)
    void refillStack(const std::vector<T>& elements) {
        // Clear current stack first
        T dummy;
        while (events_.pop(dummy)) {
        }

        // Push elements in reverse to maintain original order
        for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
            events_.push(*it);
        }
        eventCount_.store(elements.size(), std::memory_order_relaxed);
    }
#else
    std::vector<T> events_;              // Vector to store events
    mutable std::shared_mutex mtx_;      // Mutex for thread safety
    std::atomic<size_t> eventCount_{0};  // Atomic counter for event count
#endif
};

#if !ATOM_ASYNC_USE_LOCKFREE
// Copy constructor
template <typename T>
    requires std::copyable<T> && std::movable<T>
EventStack<T>::EventStack(const EventStack& other) noexcept(false) {
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
    false) {
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
#endif  // !ATOM_ASYNC_USE_LOCKFREE

template <typename T>
    requires std::copyable<T> && std::movable<T>
void EventStack<T>::pushEvent(T event) {
    try {
#if ATOM_ASYNC_USE_LOCKFREE
        if (events_.push(std::move(event))) {
            ++eventCount_;
        } else {
            throw EventStackException(
                "Failed to push event: lockfree stack operation failed");
        }
#else
        std::unique_lock lock(mtx_);
        events_.push_back(std::move(event));
        ++eventCount_;
#endif
    } catch (const std::exception& e) {
        throw EventStackException(std::string("Failed to push event: ") +
                                  e.what());
    }
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
auto EventStack<T>::popEvent() noexcept -> std::optional<T> {
#if ATOM_ASYNC_USE_LOCKFREE
    T event;
    if (events_.pop(event)) {
        size_t current = eventCount_.load(std::memory_order_relaxed);
        if (current > 0) {
            eventCount_.compare_exchange_strong(current, current - 1);
        }
        return event;
    }
    return std::nullopt;
#else
    std::unique_lock lock(mtx_);
    if (!events_.empty()) {
        T event = std::move(events_.back());
        events_.pop_back();
        --eventCount_;
        return event;
    }
    return std::nullopt;
#endif
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
#if ATOM_ASYNC_USE_LOCKFREE
    return eventCount_.load(std::memory_order_relaxed) == 0;
#else
    std::shared_lock lock(mtx_);
    return events_.empty();
#endif
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
auto EventStack<T>::size() const noexcept -> size_t {
    return eventCount_.load(std::memory_order_relaxed);
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
void EventStack<T>::clearEvents() noexcept {
#if ATOM_ASYNC_USE_LOCKFREE
    // Drain the stack
    T dummy;
    while (events_.pop(dummy)) {
    }
    eventCount_.store(0, std::memory_order_relaxed);
#else
    std::unique_lock lock(mtx_);
    events_.clear();
    eventCount_.store(0, std::memory_order_relaxed);
#endif
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
auto EventStack<T>::peekTopEvent() const -> std::optional<T> {
#if ATOM_ASYNC_USE_LOCKFREE
    if (eventCount_.load(std::memory_order_relaxed) == 0) {
        return std::nullopt;
    }

    // This operation requires creating a temporary copy of the stack
    boost::lockfree::stack<T> tempStack(128);
    tempStack.push(T{});  // Ensure we have at least one element
    if (!const_cast<boost::lockfree::stack<T>&>(events_).pop_unsafe(
            [&tempStack](T& item) {
                tempStack.push(item);
                return false;
            })) {
        return std::nullopt;
    }

    T result;
    tempStack.pop(result);
    return result;
#else
    std::shared_lock lock(mtx_);
    if (!events_.empty()) {
        return events_.back();
    }
    return std::nullopt;
#endif
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
#if ATOM_ASYNC_USE_LOCKFREE
        std::vector<T> elements = drainStack();
        elements = Parallel::filter(elements.begin(), elements.end(),
                                    std::forward<Func>(filterFunc));
        refillStack(elements);
#else
        std::unique_lock lock(mtx_);
        auto filtered = Parallel::filter(events_.begin(), events_.end(),
                                         std::forward<Func>(filterFunc));
        events_ = std::move(filtered);
        eventCount_.store(events_.size(), std::memory_order_relaxed);
#endif
    } catch (const std::exception& e) {
        throw EventStackException(std::string("Failed to filter events: ") +
                                  e.what());
    }
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
                             auto EventStack<T>::serializeStack() const
             -> std::string
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
            if constexpr (std::same_as<T, std::string>) {
                serializedStack += event + ";";
            } else {
                serializedStack += std::to_string(event) + ";";
            }
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
                // Handle string type differently from other types
                T event;
                if constexpr (std::same_as<T, std::string>) {
                    event = token;
                } else {
                    event =
                        T{std::stoll(token)};  // Convert string to number type
                }
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

        Parallel::sort(events_.begin(), events_.end());

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

        Parallel::sort(events_.begin(), events_.end(),
                       std::forward<Func>(compareFunc));

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
                          std::same_as<std::invoke_result_t<Func&, const T&>,
                                       bool>
auto EventStack<T>::countEvents(Func&& predicate) const -> size_t {
    try {
        std::shared_lock lock(mtx_);

        size_t count = 0;
        auto countPredicate = [&predicate, &count](const T& item) {
            if (predicate(item)) {
                ++count;
            }
        };

        Parallel::for_each(events_.begin(), events_.end(), countPredicate);
        return count;

    } catch (const std::exception& e) {
        throw EventStackException(std::string("Failed to count events: ") +
                                  e.what());
    }
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
                             template <typename Func>
                 requires std::invocable<Func&, const T&> &&
                          std::same_as<std::invoke_result_t<Func&, const T&>,
                                       bool>
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
                          std::same_as<std::invoke_result_t<Func&, const T&>,
                                       bool>
auto EventStack<T>::anyEvent(Func&& predicate) const -> bool {
    try {
        std::shared_lock lock(mtx_);

        std::atomic<bool> result{false};
        auto checkPredicate = [&result, &predicate](const T& item) {
            if (predicate(item) && !result.load(std::memory_order_relaxed)) {
                result.store(true, std::memory_order_relaxed);
            }
        };

        Parallel::for_each(events_.begin(), events_.end(), checkPredicate);
        return result.load(std::memory_order_relaxed);

    } catch (const std::exception& e) {
        throw EventStackException(std::string("Failed to check any event: ") +
                                  e.what());
    }
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
                             template <typename Func>
                 requires std::invocable<Func&, const T&> &&
                          std::same_as<std::invoke_result_t<Func&, const T&>,
                                       bool>
auto EventStack<T>::allEvents(Func&& predicate) const -> bool {
    try {
        std::shared_lock lock(mtx_);

        std::atomic<bool> allMatch{true};
        auto checkPredicate = [&allMatch, &predicate](const T& item) {
            if (!predicate(item) && allMatch.load(std::memory_order_relaxed)) {
                allMatch.store(false, std::memory_order_relaxed);
            }
        };

        Parallel::for_each(events_.begin(), events_.end(), checkPredicate);
        return allMatch.load(std::memory_order_relaxed);

    } catch (const std::exception& e) {
        throw EventStackException(std::string("Failed to check all events: ") +
                                  e.what());
    }
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
auto EventStack<T>::getEventsView() const noexcept -> std::span<const T> {
#if ATOM_ASYNC_USE_LOCKFREE
    // A true const view of a lock-free stack is complex.
    // This would require copying to a temporary buffer if a span is needed.
    // For now, returning an empty span or throwing might be options.
    // The drainStack() method is non-const.
    // To satisfy the interface, one might copy, but it's not a "view".
    // Returning empty span to avoid compilation error, but this needs a proper
    // design for lock-free.
    return std::span<const T>();
#else
    if constexpr (std::is_same_v<T, bool>) {
        // std::vector<bool>::iterator is not a contiguous_iterator in the C++20
        // sense, and std::to_address cannot be used to get a bool* for it.
        // Thus, std::span cannot be directly constructed from its iterators
        // in the typical way that guarantees a view over contiguous bools.
        // Returning an empty span to avoid compilation errors and indicate this
        // limitation.
        return std::span<const T>();
    } else {
        std::shared_lock lock(mtx_);
        return std::span<const T>(events_.begin(), events_.end());
    }
#endif
}

template <typename T>
    requires std::copyable<T> && std::movable<T>
                             template <typename Func>
                 requires std::invocable<Func&, const T&>
void EventStack<T>::forEach(Func&& func) const {
    try {
#if ATOM_ASYNC_USE_LOCKFREE
        // This is problematic for const-correctness with
        // drainStack/refillStack. A const forEach on a lock-free stack
        // typically involves temporary copying.
        std::vector<T> elements = const_cast<EventStack<T>*>(this)
                                      ->drainStack();  // Unsafe const_cast
        try {
            Parallel::for_each(elements.begin(), elements.end(),
                               func);  // Pass func as lvalue
        } catch (...) {
            const_cast<EventStack<T>*>(this)->refillStack(
                elements);  // Refill on error
            throw;
        }
        const_cast<EventStack<T>*>(this)->refillStack(
            elements);  // Refill after processing
#else
        std::shared_lock lock(mtx_);
        Parallel::for_each(events_.begin(), events_.end(),
                           func);  // Pass func as lvalue
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
#if ATOM_ASYNC_USE_LOCKFREE
        std::vector<T> elements = drainStack();
        try {
            // 直接使用原始函数，而不是包装成std::function
            if constexpr (std::is_same_v<T, bool>) {
                for (auto& event : elements) {
                    transformFunc(event);
                }
            } else {
                // 直接传递原始的transformFunc
                Parallel::for_each(elements.begin(), elements.end(),
                                   std::forward<Func>(transformFunc));
            }
        } catch (...) {
            refillStack(elements);  // Refill on error
            throw;
        }
        refillStack(elements);  // Refill after processing
#else
        std::unique_lock lock(mtx_);
        if constexpr (std::is_same_v<T, bool>) {
            // 对于bool类型进行特殊处理
            for (typename std::vector<T>::reference event_ref : events_) {
                bool val = event_ref;  // 将proxy转换为bool
                transformFunc(val);    // 调用用户函数
                event_ref = val;       // 将修改后的值赋回去
            }
        } else {
            // TODO: Fix this
            /*
            Parallel::for_each(events_.begin(), events_.end(),
                               std::forward<Func>(transformFunc));
            */
            
        }
#endif
    } catch (const std::exception& e) {
        throw EventStackException(std::string("Failed to transform events: ") +
                                  e.what());
    }
}

}  // namespace atom::async

#endif  // ATOM_ASYNC_EVENTSTACK_HPP
