/*
 * eventstack.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file eventstack.hpp
 * @brief A high-performance thread-safe stack data structure for managing
 * events
 * @details Utilizes lock-free data structures, advanced concurrency primitives,
 *          and modern C++ standards for optimal performance and scalability
 */

#ifndef ATOM_ASYNC_EVENTSTACK_HPP
#define ATOM_ASYNC_EVENTSTACK_HPP

#include <algorithm>
#include <atomic>
#include <concepts>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#if __has_include(<execution>)
#include <execution>
#define HAS_EXECUTION_HEADER 1
#else
#define HAS_EXECUTION_HEADER 0
#endif

namespace atom::async {

/**
 * @brief Custom exception for EventStack operations
 */
class EventStackException : public std::runtime_error {
public:
    explicit EventStackException(const std::string& message)
        : std::runtime_error(message) {
        spdlog::error("EventStackException: {}", message);
    }
};

/**
 * @brief Exception thrown when attempting operations on empty EventStack
 */
class EventStackEmptyException : public EventStackException {
public:
    EventStackEmptyException()
        : EventStackException("Attempted operation on empty EventStack") {}
};

/**
 * @brief Exception thrown during serialization/deserialization errors
 */
class EventStackSerializationException : public EventStackException {
public:
    explicit EventStackSerializationException(const std::string& message)
        : EventStackException("Serialization error: " + message) {}
};

/**
 * @brief Concept for serializable types
 */
template <typename T>
concept Serializable = requires(T a) {
    { std::to_string(a) } -> std::convertible_to<std::string>;
} || std::same_as<T, std::string>;

/**
 * @brief Concept for comparable types
 */
template <typename T>
concept Comparable = requires(T a, T b) {
    { a == b } -> std::convertible_to<bool>;
    { a < b } -> std::convertible_to<bool>;
};

/**
 * @brief Lock-free node for stack implementation
 */
template <typename T>
struct alignas(std::hardware_destructive_interference_size) LockFreeNode {
    std::atomic<LockFreeNode*> next{nullptr};
    T data;

    template <typename... Args>
    explicit LockFreeNode(Args&&... args) : data(std::forward<Args>(args)...) {}
};

/**
 * @brief High-performance thread-safe stack with lock-free operations
 * @tparam T The type of events to store
 * @details Uses Treiber stack algorithm for lock-free operations with
 *          hazard pointers for memory safety
 */
template <typename T>
    requires std::copyable<T> && std::movable<T>
class EventStack {
private:
    using Node = LockFreeNode<T>;

    alignas(std::hardware_destructive_interference_size)
        std::atomic<Node*> head_{nullptr};
    alignas(std::hardware_destructive_interference_size)
        std::atomic<std::size_t> size_{0};

    /**
     * @brief Hazard pointer for memory reclamation
     */
    class HazardPointer {
    public:
        static constexpr std::size_t MAX_HAZARD_POINTERS = 100;

        static auto acquire() -> Node* {
            thread_local static std::size_t hazard_index = 0;
            auto& hazard_ptr =
                hazard_pointers_[hazard_index % MAX_HAZARD_POINTERS];
            hazard_index++;
            return hazard_ptr.load();
        }

        static void release(Node* ptr) {
            for (auto& hazard_ptr : hazard_pointers_) {
                Node* expected = ptr;
                if (hazard_ptr.compare_exchange_weak(expected, nullptr)) {
                    break;
                }
            }
        }

        static void protect(Node* ptr) {
            thread_local static std::size_t protect_index = 0;
            hazard_pointers_[protect_index % MAX_HAZARD_POINTERS].store(ptr);
            protect_index++;
        }

    private:
        static inline std::array<std::atomic<Node*>, MAX_HAZARD_POINTERS>
            hazard_pointers_;
    };

    /**
     * @brief Memory pool for efficient node allocation
     */
    class alignas(std::hardware_destructive_interference_size) MemoryPool {
    public:
        static auto allocate() -> Node* {
            if (auto node = free_list_.load()) {
                while (!free_list_.compare_exchange_weak(node,
                                                         node->next.load())) {
                    if (!node)
                        break;
                }
                if (node) {
                    node->next.store(nullptr);
                    return node;
                }
            }
            return new Node{};
        }

        static void deallocate(Node* node) {
            if (!node)
                return;

            auto current_head = free_list_.load();
            do {
                node->next.store(current_head);
            } while (!free_list_.compare_exchange_weak(current_head, node));
        }

    private:
        static inline std::atomic<Node*> free_list_{nullptr};
    };

public:
    /**
     * @brief Default constructor
     */
    EventStack() {
        spdlog::debug("EventStack created with lock-free implementation");
    }

    /**
     * @brief Destructor
     */
    ~EventStack() {
        clearEvents();
        spdlog::debug("EventStack destroyed");
    }

    EventStack(const EventStack&) = delete;
    EventStack& operator=(const EventStack&) = delete;

    /**
     * @brief Move constructor
     */
    EventStack(EventStack&& other) noexcept
        : head_(other.head_.exchange(nullptr)), size_(other.size_.exchange(0)) {
        spdlog::debug("EventStack moved");
    }

    /**
     * @brief Move assignment operator
     */
    EventStack& operator=(EventStack&& other) noexcept {
        if (this != &other) {
            clearEvents();
            head_.store(other.head_.exchange(nullptr));
            size_.store(other.size_.exchange(0));
            spdlog::debug("EventStack move assigned");
        }
        return *this;
    }

    /**
     * @brief Pushes an event onto the stack using lock-free algorithm
     * @param event The event to push
     * @throws EventStackException If memory allocation fails
     */
    void pushEvent(T event) {
        auto node = MemoryPool::allocate();
        if (!node) {
            throw EventStackException("Memory allocation failed");
        }

        try {
            new (&node->data) T(std::move(event));
        } catch (...) {
            MemoryPool::deallocate(node);
            throw;
        }

        auto current_head = head_.load();
        do {
            node->next.store(current_head);
        } while (!head_.compare_exchange_weak(current_head, node));

        size_.fetch_add(1, std::memory_order_relaxed);
        spdlog::trace("Event pushed to stack, size: {}", size_.load());
    }

    /**
     * @brief Pops an event from the stack using lock-free algorithm
     * @return The popped event, or std::nullopt if empty
     */
    [[nodiscard]] auto popEvent() noexcept -> std::optional<T> {
        auto current_head = head_.load();

        while (current_head) {
            HazardPointer::protect(current_head);

            if (current_head != head_.load()) {
                current_head = head_.load();
                continue;
            }

            auto next = current_head->next.load();
            if (head_.compare_exchange_weak(current_head, next)) {
                T result = std::move(current_head->data);
                HazardPointer::release(current_head);
                MemoryPool::deallocate(current_head);
                size_.fetch_sub(1, std::memory_order_relaxed);

                spdlog::trace("Event popped from stack, size: {}",
                              size_.load());
                return result;
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Checks if the stack is empty
     * @return true if empty, false otherwise
     */
    [[nodiscard]] auto isEmpty() const noexcept -> bool {
        return size_.load(std::memory_order_relaxed) == 0;
    }

    /**
     * @brief Returns the number of events in the stack
     * @return The number of events
     */
    [[nodiscard]] auto size() const noexcept -> std::size_t {
        return size_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Clears all events from the stack
     */
    void clearEvents() noexcept {
        while (popEvent().has_value()) {
        }
        spdlog::debug("All events cleared from stack");
    }

    /**
     * @brief Returns the top event without removing it
     * @return The top event, or std::nullopt if empty
     */
    [[nodiscard]] auto peekTopEvent() const -> std::optional<T> {
        auto current_head = head_.load();
        if (!current_head)
            return std::nullopt;

        HazardPointer::protect(current_head);

        if (current_head != head_.load()) {
            return std::nullopt;
        }

        T result = current_head->data;
        HazardPointer::release(current_head);

        return result;
    }

    /**
     * @brief Filters events based on a predicate using parallel execution
     * @tparam Func Predicate function type
     * @param filterFunc The filter function
     */
    template <typename Func>
        requires std::invocable<Func&, const T&> &&
                 std::same_as<std::invoke_result_t<Func&, const T&>, bool>
    void filterEvents(Func&& filterFunc) {
        std::vector<T> events = drainToVector();

        std::vector<T> filtered;
        filtered.reserve(events.size());

#if HAS_EXECUTION_HEADER
        std::copy_if(std::execution::par_unseq, events.begin(), events.end(),
                     std::back_inserter(filtered),
                     std::forward<Func>(filterFunc));
#else
        std::copy_if(events.begin(), events.end(), std::back_inserter(filtered),
                     std::forward<Func>(filterFunc));
#endif

        refillFromVector(std::move(filtered));
        spdlog::debug("Events filtered, new size: {}", size());
    }

    /**
     * @brief Serializes the stack to a string
     * @return Serialized string representation
     */
    [[nodiscard]] auto serializeStack() const -> std::string
        requires Serializable<T>
    {
        std::vector<T> events = drainToVector();
        std::string result;

        std::size_t estimated_size =
            events.size() * (std::is_same_v<T, std::string> ? 32 : 16);
        result.reserve(estimated_size);

        for (const auto& event : events) {
            if constexpr (std::same_as<T, std::string>) {
                result += event + ";";
            } else {
                result += std::to_string(event) + ";";
            }
        }

        const_cast<EventStack*>(this)->refillFromVector(std::move(events));
        spdlog::debug("Stack serialized, length: {}", result.size());
        return result;
    }

    /**
     * @brief Deserializes a string into the stack
     * @param serializedData The serialized data
     */
    void deserializeStack(std::string_view serializedData)
        requires Serializable<T>
    {
        clearEvents();

        std::vector<T> events;
        std::size_t pos = 0;

        while (pos < serializedData.size()) {
            auto next_pos = serializedData.find(';', pos);
            if (next_pos == std::string_view::npos)
                break;

            if (next_pos > pos) {
                std::string token(serializedData.substr(pos, next_pos - pos));

                if constexpr (std::same_as<T, std::string>) {
                    events.emplace_back(std::move(token));
                } else {
                    events.emplace_back(static_cast<T>(std::stoll(token)));
                }
            }
            pos = next_pos + 1;
        }

        refillFromVector(std::move(events));
        spdlog::debug("Stack deserialized, size: {}", size());
    }

    /**
     * @brief Removes duplicate events
     */
    void removeDuplicates()
        requires Comparable<T>
    {
        std::vector<T> events = drainToVector();

#if HAS_EXECUTION_HEADER
        std::sort(std::execution::par_unseq, events.begin(), events.end());
#else
        std::sort(events.begin(), events.end());
#endif

        auto new_end = std::unique(events.begin(), events.end());
        events.erase(new_end, events.end());

        refillFromVector(std::move(events));
        spdlog::debug("Duplicates removed, new size: {}", size());
    }

    /**
     * @brief Sorts events using parallel execution
     * @tparam Func Comparison function type
     * @param compareFunc The comparison function
     */
    template <typename Func>
        requires std::invocable<Func&, const T&, const T&> &&
                 std::same_as<std::invoke_result_t<Func&, const T&, const T&>,
                              bool>
    void sortEvents(Func&& compareFunc) {
        std::vector<T> events = drainToVector();

#if HAS_EXECUTION_HEADER
        std::sort(std::execution::par_unseq, events.begin(), events.end(),
                  std::forward<Func>(compareFunc));
#else
        std::sort(events.begin(), events.end(),
                  std::forward<Func>(compareFunc));
#endif

        refillFromVector(std::move(events));
        spdlog::debug("Events sorted, size: {}", size());
    }

    /**
     * @brief Reverses the order of events
     */
    void reverseEvents() noexcept {
        std::vector<T> events = drainToVector();
        std::reverse(events.begin(), events.end());
        refillFromVector(std::move(events));
        spdlog::debug("Events reversed, size: {}", size());
    }

    /**
     * @brief Counts events matching a predicate using parallel execution
     * @tparam Func Predicate function type
     * @param predicate The predicate function
     * @return Count of matching events
     */
    template <typename Func>
        requires std::invocable<Func&, const T&> &&
                 std::same_as<std::invoke_result_t<Func&, const T&>, bool>
    [[nodiscard]] auto countEvents(Func&& predicate) const -> std::size_t {
        std::vector<T> events = drainToVector();

#if HAS_EXECUTION_HEADER
        auto count = std::count_if(std::execution::par_unseq, events.begin(),
                                   events.end(), std::forward<Func>(predicate));
#else
        auto count = std::count_if(events.begin(), events.end(),
                                   std::forward<Func>(predicate));
#endif

        const_cast<EventStack*>(this)->refillFromVector(std::move(events));
        spdlog::trace("Events counted: {}", count);
        return static_cast<std::size_t>(count);
    }

    /**
     * @brief Finds first event matching a predicate
     * @tparam Func Predicate function type
     * @param predicate The predicate function
     * @return First matching event or std::nullopt
     */
    template <typename Func>
        requires std::invocable<Func&, const T&> &&
                 std::same_as<std::invoke_result_t<Func&, const T&>, bool>
    [[nodiscard]] auto findEvent(Func&& predicate) const -> std::optional<T> {
        std::vector<T> events = drainToVector();

        auto it = std::find_if(events.begin(), events.end(),
                               std::forward<Func>(predicate));

        std::optional<T> result =
            (it != events.end()) ? std::make_optional(*it) : std::nullopt;

        const_cast<EventStack*>(this)->refillFromVector(std::move(events));
        return result;
    }

    /**
     * @brief Checks if any event matches a predicate
     * @tparam Func Predicate function type
     * @param predicate The predicate function
     * @return true if any event matches
     */
    template <typename Func>
        requires std::invocable<Func&, const T&> &&
                 std::same_as<std::invoke_result_t<Func&, const T&>, bool>
    [[nodiscard]] auto anyEvent(Func&& predicate) const -> bool {
        std::vector<T> events = drainToVector();

        bool result = std::any_of(events.begin(), events.end(),
                                  std::forward<Func>(predicate));

        const_cast<EventStack*>(this)->refillFromVector(std::move(events));
        return result;
    }

    /**
     * @brief Checks if all events match a predicate
     * @tparam Func Predicate function type
     * @param predicate The predicate function
     * @return true if all events match
     */
    template <typename Func>
        requires std::invocable<Func&, const T&> &&
                 std::same_as<std::invoke_result_t<Func&, const T&>, bool>
    [[nodiscard]] auto allEvents(Func&& predicate) const -> bool {
        std::vector<T> events = drainToVector();

        bool result = std::all_of(events.begin(), events.end(),
                                  std::forward<Func>(predicate));

        const_cast<EventStack*>(this)->refillFromVector(std::move(events));
        return result;
    }

    /**
     * @brief Applies a function to each event using parallel execution
     * @tparam Func Function type
     * @param func The function to apply
     */
    template <typename Func>
        requires std::invocable<Func&, const T&>
    void forEach(Func&& func) const {
        std::vector<T> events = drainToVector();

#if HAS_EXECUTION_HEADER
        std::for_each(std::execution::par_unseq, events.begin(), events.end(),
                      std::forward<Func>(func));
#else
        std::for_each(events.begin(), events.end(), std::forward<Func>(func));
#endif

        const_cast<EventStack*>(this)->refillFromVector(std::move(events));
        spdlog::trace("ForEach applied to {} events", events.size());
    }

    /**
     * @brief Transforms events using parallel execution
     * @tparam Func Transform function type
     * @param transformFunc The transform function
     */
    template <typename Func>
        requires std::invocable<Func&, T&>
    void transformEvents(Func&& transformFunc) {
        std::vector<T> events = drainToVector();

#if HAS_EXECUTION_HEADER
        std::for_each(std::execution::par_unseq, events.begin(), events.end(),
                      std::forward<Func>(transformFunc));
#else
        std::for_each(events.begin(), events.end(),
                      std::forward<Func>(transformFunc));
#endif

        refillFromVector(std::move(events));
        spdlog::debug("Events transformed, size: {}", size());
    }

private:
    /**
     * @brief Drains the stack into a vector for batch operations
     * @return Vector containing all events
     */
    std::vector<T> drainToVector() const {
        std::vector<T> result;
        result.reserve(size_.load(std::memory_order_relaxed));

        auto* current = head_.load();
        while (current) {
            HazardPointer::protect(current);

            if (current != head_.load()) {
                current = head_.load();
                continue;
            }

            result.push_back(current->data);
            current = current->next.load();
        }

        std::reverse(result.begin(), result.end());
        return result;
    }

    /**
     * @brief Refills the stack from a vector
     * @param events Vector of events to add
     */
    void refillFromVector(std::vector<T>&& events) {
        clearEvents();

        for (auto& event : events) {
            pushEvent(std::move(event));
        }
    }
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_EVENTSTACK_HPP
