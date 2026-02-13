#ifndef ATOM_ASYNC_SYNC_LOCKFREE_LIST_HPP
#define ATOM_ASYNC_SYNC_LOCKFREE_LIST_HPP

#include <atomic>
#include <memory>
#include <optional>

namespace atom::async {

/**
 * @brief Concept for lock-free list elements.
 */
template <typename T>
concept LockFreeListElem = std::is_nothrow_move_constructible_v<T> &&
                           std::is_nothrow_destructible_v<T>;

/**
 * @brief A lock-free singly-linked list implementation.
 *
 * Provides thread-safe push/pop operations at the front of the list.
 *
 * @tparam T Type of elements stored in the list.
 */
template <LockFreeListElem T>
class LockFreeList {
private:
    struct Node {
        T value;
        std::atomic<std::shared_ptr<Node>> next;

        explicit Node(const T& val) noexcept(
            std::is_nothrow_copy_constructible_v<T>)
            : value(val), next(nullptr) {}

        explicit Node(T&& val) noexcept(std::is_nothrow_move_constructible_v<T>)
            : value(std::move(val)), next(nullptr) {}
    };

    std::atomic<std::shared_ptr<Node>> head_{nullptr};
    std::atomic<size_t> size_{0};

public:
    LockFreeList() noexcept = default;

    ~LockFreeList() noexcept = default;

    // Non-copyable
    LockFreeList(const LockFreeList&) = delete;
    LockFreeList& operator=(const LockFreeList&) = delete;

    // Movable
    LockFreeList(LockFreeList&& other) noexcept
        : head_(other.head_.exchange(nullptr)),
          size_(other.size_.exchange(0)) {}

    LockFreeList& operator=(LockFreeList&& other) noexcept {
        if (this != &other) {
            head_ = other.head_.exchange(nullptr);
            size_ = other.size_.exchange(0);
        }
        return *this;
    }

    /**
     * @brief Push a value to the front of the list.
     * @param value The value to push.
     */
    void pushFront(const T& value) {
        try {
            auto newNode = std::make_shared<Node>(value);
            pushNodeFront(std::move(newNode));
        } catch (const std::bad_alloc&) {
            // Memory allocation failure
        }
    }

    /**
     * @brief Push a value to the front using move semantics.
     * @param value The value to push.
     */
    void pushFront(T&& value) noexcept(
        std::is_nothrow_move_constructible_v<T>) {
        try {
            auto newNode = std::make_shared<Node>(std::move(value));
            pushNodeFront(std::move(newNode));
        } catch (const std::bad_alloc&) {
            // Memory allocation failure
        }
    }

    /**
     * @brief Pop a value from the front of the list.
     * @return std::optional<T> The popped value if list is not empty.
     */
    auto popFront() noexcept -> std::optional<T> {
        auto oldHead = head_.load(std::memory_order_acquire);
        std::shared_ptr<Node> newHead;

        while (oldHead) {
            newHead = oldHead->next.load(std::memory_order_relaxed);
            if (head_.compare_exchange_weak(oldHead, newHead,
                                            std::memory_order_acq_rel,
                                            std::memory_order_relaxed)) {
                size_.fetch_sub(1, std::memory_order_relaxed);
                return std::optional<T>{std::move(oldHead->value)};
            }
        }
        return std::nullopt;
    }

    /**
     * @brief Get the front value without removing it.
     * @return std::optional<T> The front value if list is not empty.
     */
    auto front() const noexcept -> std::optional<T> {
        auto currentHead = head_.load(std::memory_order_acquire);
        if (currentHead) {
            return std::optional<T>(currentHead->value);
        }
        return std::nullopt;
    }

    /**
     * @brief Check if the list is empty.
     */
    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == nullptr;
    }

    /**
     * @brief Get the size of the list.
     */
    [[nodiscard]] auto size() const noexcept -> size_t {
        return size_.load(std::memory_order_acquire);
    }

    /**
     * @brief Clear the list.
     */
    void clear() noexcept {
        auto currentHead = head_.exchange(nullptr, std::memory_order_acq_rel);
        size_.store(0, std::memory_order_release);
    }

    /**
     * @brief Forward iterator for LockFreeList.
     */
    class Iterator {
    public:
        using iterator_concept = std::forward_iterator_tag;
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        explicit Iterator(std::shared_ptr<Node> node) noexcept
            : current_(std::move(node)) {}

        reference operator*() const noexcept { return current_->value; }

        pointer operator->() const noexcept { return &(current_->value); }

        Iterator& operator++() noexcept {
            current_ = current_->next.load(std::memory_order_acquire);
            return *this;
        }

        Iterator operator++(int) noexcept {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }

        bool operator==(const Iterator& other) const noexcept {
            return current_ == other.current_;
        }

        bool operator!=(const Iterator& other) const noexcept {
            return !(*this == other);
        }

    private:
        std::shared_ptr<Node> current_;
    };

    auto begin() const noexcept -> Iterator {
        return Iterator(head_.load(std::memory_order_acquire));
    }

    auto end() const noexcept -> Iterator { return Iterator(nullptr); }

private:
    void pushNodeFront(std::shared_ptr<Node> newNode) noexcept {
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

        size_.fetch_add(1, std::memory_order_relaxed);
    }
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_SYNC_LOCKFREE_LIST_HPP
