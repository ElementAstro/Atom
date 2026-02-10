#ifndef ATOM_ASYNC_SYNC_LOCKFREE_STACK_HPP
#define ATOM_ASYNC_SYNC_LOCKFREE_STACK_HPP

#include <atomic>
#include <memory>
#include <optional>

namespace atom::async {

/**
 * @brief Concept for types that can be used in lock-free data structures.
 */
template <typename T>
concept LockFreeSafe = std::is_nothrow_destructible_v<T>;

/**
 * @brief A lock-free stack implementation suitable for concurrent use.
 *
 * Uses atomic shared_ptr for safe memory management in concurrent scenarios.
 *
 * @tparam T Type of elements stored in the stack.
 */
template <LockFreeSafe T>
class LockFreeStack {
private:
    struct Node {
        T value;
        std::atomic<std::shared_ptr<Node>> next{nullptr};

        explicit Node(T value_) noexcept(
            std::is_nothrow_move_constructible_v<T>)
            : value(std::move(value_)) {}
    };

    std::atomic<std::shared_ptr<Node>> head_{nullptr};
    std::atomic<int> approximateSize_{0};

public:
    LockFreeStack() noexcept = default;

    ~LockFreeStack() noexcept = default;

    // Non-copyable
    LockFreeStack(const LockFreeStack&) = delete;
    LockFreeStack& operator=(const LockFreeStack&) = delete;

    // Movable
    LockFreeStack(LockFreeStack&& other) noexcept
        : head_(other.head_.exchange(nullptr)),
          approximateSize_(other.approximateSize_.exchange(0)) {}

    LockFreeStack& operator=(LockFreeStack&& other) noexcept {
        if (this != &other) {
            while (pop()) {
            }
            head_ = other.head_.exchange(nullptr);
            approximateSize_ = other.approximateSize_.exchange(0);
        }
        return *this;
    }

    /**
     * @brief Pushes a value onto the stack. Thread-safe.
     * @param value The value to push onto the stack.
     */
    void push(const T& value) noexcept(
        std::is_nothrow_copy_constructible_v<T>) {
        try {
            auto newNode = std::make_shared<Node>(value);
            push_node(std::move(newNode));
        } catch (const std::bad_alloc&) {
            // Memory allocation failure
        }
    }

    /**
     * @brief Pushes a value onto the stack using move semantics. Thread-safe.
     * @param value The value to move onto the stack.
     */
    void push(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>) {
        try {
            auto newNode = std::make_shared<Node>(std::move(value));
            push_node(std::move(newNode));
        } catch (const std::bad_alloc&) {
            // Memory allocation failure
        }
    }

    /**
     * @brief Attempts to pop the top value off the stack. Thread-safe.
     * @return std::optional<T> The popped value if stack is not empty.
     */
    auto pop() noexcept -> std::optional<T> {
        auto oldHead = head_.load(std::memory_order_acquire);
        std::shared_ptr<Node> newHead;

        while (oldHead) {
            newHead = oldHead->next.load(std::memory_order_relaxed);
            if (head_.compare_exchange_weak(oldHead, newHead,
                                            std::memory_order_acq_rel,
                                            std::memory_order_relaxed)) {
                approximateSize_.fetch_sub(1, std::memory_order_relaxed);
                return std::optional<T>{std::move(oldHead->value)};
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Get the top value without removing it. Thread-safe.
     * @return std::optional<T> The top value if stack is not empty.
     */
    auto top() const noexcept -> std::optional<T> {
        auto currentHead = head_.load(std::memory_order_acquire);
        if (currentHead) {
            return std::optional<T>(currentHead->value);
        }
        return std::nullopt;
    }

    /**
     * @brief Check if the stack is empty. Thread-safe.
     */
    [[nodiscard]] auto empty() const noexcept -> bool {
        return head_.load(std::memory_order_acquire) == nullptr;
    }

    /**
     * @brief Get the approximate size of the stack. Thread-safe.
     */
    [[nodiscard]] auto size() const noexcept -> int {
        return approximateSize_.load(std::memory_order_acquire);
    }

private:
    void push_node(std::shared_ptr<Node> newNode) noexcept {
        // Load current head into expected
        std::shared_ptr<Node> expected = head_.load(std::memory_order_relaxed);

        // Initialize newNode->next to current head
        newNode->next.store(expected, std::memory_order_relaxed);

        // Try to update head_ atomically
        while (!head_.compare_exchange_weak(expected, newNode,
                                            std::memory_order_acq_rel,
                                            std::memory_order_relaxed)) {
            // On failure, expected is updated; update newNode->next accordingly
            newNode->next.store(expected, std::memory_order_relaxed);
        }

        approximateSize_.fetch_add(1, std::memory_order_relaxed);
    }
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_SYNC_LOCKFREE_STACK_HPP
