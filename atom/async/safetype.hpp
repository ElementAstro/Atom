#ifndef ATOM_ASYNC_SAFETYPE_HPP
#define ATOM_ASYNC_SAFETYPE_HPP

#include <atomic>
#include <concepts>  // C++20 concepts
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>  // C++20 ranges
#include <shared_mutex>
#include <span>
#include <vector>

#include "atom/error/exception.hpp"

namespace atom::async {

// Concept for types that can be used in lock-free data structures
template <typename T>
concept LockFreeSafe = std::is_nothrow_destructible_v<T>;

/**
 * @brief A lock-free stack implementation suitable for concurrent use.
 *
 * @tparam T Type of elements stored in the stack.
 */
template <LockFreeSafe T>
class LockFreeStack {
private:
    struct Node {
        T value;  ///< The stored value of type T.
        std::atomic<std::shared_ptr<Node>> next{
            nullptr};  ///< Pointer to the next node in the stack.

        /**
         * @brief Construct a new Node object.
         *
         * @param value_ The value to store in the node.
         */
        explicit Node(T value_) noexcept(
            std::is_nothrow_move_constructible_v<T>)
            : value(std::move(value_)) {}
    };

    std::atomic<std::shared_ptr<Node>> head_{
        nullptr};  ///< Atomic pointer to the top of the stack.
    std::atomic<int> approximateSize_{
        0};  ///< An approximate count of the stack's elements.

public:
    /**
     * @brief Construct a new Lock Free Stack object.
     */
    LockFreeStack() noexcept = default;

    /**
     * @brief Destroy the Lock Free Stack object.
     */
    ~LockFreeStack() noexcept {
        // Smart pointers handle cleanup automatically
    }

    // Non-copyable
    LockFreeStack(const LockFreeStack&) = delete;
    LockFreeStack& operator=(const LockFreeStack&) = delete;

    // Movable
    LockFreeStack(LockFreeStack&& other) noexcept
        : head_(other.head_.exchange(nullptr)),
          approximateSize_(other.approximateSize_.exchange(0)) {}

    LockFreeStack& operator=(LockFreeStack&& other) noexcept {
        if (this != &other) {
            // Clear current stack
            while (pop()) {
            }

            // Move from other
            head_ = other.head_.exchange(nullptr);
            approximateSize_ = other.approximateSize_.exchange(0);
        }
        return *this;
    }

    /**
     * @brief Pushes a value onto the stack. Thread-safe.
     *
     * @param value The value to push onto the stack.
     */
    void push(const T& value) noexcept(
        std::is_nothrow_copy_constructible_v<T>) {
        try {
            auto newNode = std::make_shared<Node>(value);
            push_node(std::move(newNode));
        } catch (const std::bad_alloc&) {
            // Log memory allocation failure
        }
    }

    /**
     * @brief Pushes a value onto the stack using move semantics. Thread-safe.
     *
     * @param value The value to move onto the stack.
     */
    void push(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>) {
        try {
            auto newNode = std::make_shared<Node>(std::move(value));
            push_node(std::move(newNode));
        } catch (const std::bad_alloc&) {
            // Log memory allocation failure
        }
    }

    /**
     * @brief Attempts to pop the top value off the stack. Thread-safe.
     *
     * @return std::optional<T> The popped value if stack is not empty,
     * otherwise nullopt.
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
     * @brief Get the top value of the stack without removing it. Thread-safe.
     *
     * @return std::optional<T> The top value if stack is not empty, otherwise
     * nullopt.
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
     *
     * @return true If the stack is empty.
     * @return false If the stack has one or more elements.
     */
    [[nodiscard]] auto empty() const noexcept -> bool {
        return head_.load(std::memory_order_acquire) == nullptr;
    }

    /**
     * @brief Get the approximate size of the stack. Thread-safe.
     *
     * @return int The approximate number of elements in the stack.
     */
    [[nodiscard]] auto size() const noexcept -> int {
        return approximateSize_.load(std::memory_order_acquire);
    }

private:
    void push_node(std::shared_ptr<Node> newNode) noexcept {
        // 修复：创建一个临时变量存储当前head
        std::shared_ptr<Node> expected = head_.load(std::memory_order_relaxed);

        // 初始化newNode->next
        newNode->next.store(expected, std::memory_order_relaxed);

        // 尝试更新head_
        while (!head_.compare_exchange_weak(expected, newNode,
                                            std::memory_order_acq_rel,
                                            std::memory_order_relaxed)) {
            // 如果失败，更新newNode->next为新的expected值
            newNode->next.store(expected, std::memory_order_relaxed);
        }

        approximateSize_.fetch_add(1, std::memory_order_relaxed);
    }
};

template <typename T, typename U>
concept HashTableKeyValue = requires(T t, U u) {
    { std::hash<T>{}(t) } -> std::convertible_to<size_t>;
    { t == t } -> std::convertible_to<bool>;
    requires std::default_initializable<U>;
};

template <typename Key, typename Value>
    requires HashTableKeyValue<Key, Value>
class LockFreeHashTable {
private:
    struct Node {
        Key key;
        Value value;
        std::atomic<std::shared_ptr<Node>> next;

        Node(Key k,
             Value v) noexcept(std::is_nothrow_move_constructible_v<Key> &&
                               std::is_nothrow_move_constructible_v<Value>)
            : key(std::move(k)), value(std::move(v)), next(nullptr) {}
    };

    struct Bucket {
        std::atomic<std::shared_ptr<Node>> head;

        Bucket() noexcept : head(nullptr) {}

        auto find(const Key& key) const noexcept
            -> std::optional<std::reference_wrapper<Value>> {
            auto node = head.load(std::memory_order_acquire);
            while (node) {
                if (node->key == key) {
                    return std::ref(node->value);
                }
                node = node->next.load(std::memory_order_acquire);
            }
            return std::nullopt;
        }

        void insert(const Key& key, const Value& value) {
            try {
                auto newNode = std::make_shared<Node>(key, value);
                // 修复：创建一个临时变量存储当前head
                std::shared_ptr<Node> expected =
                    head.load(std::memory_order_acquire);

                // 初始化newNode->next
                newNode->next.store(expected, std::memory_order_relaxed);

                // 尝试更新head
                while (!head.compare_exchange_weak(expected, newNode,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_relaxed)) {
                    // 如果失败，更新newNode->next为新的expected值
                    newNode->next.store(expected, std::memory_order_relaxed);
                }
            } catch (const std::exception& e) {
                // Handle allocation failure
            }
        }

        bool erase(const Key& key) noexcept {
            auto currentNode = head.load(std::memory_order_acquire);
            std::shared_ptr<Node> prevNode = nullptr;

            while (currentNode) {
                auto nextNode =
                    currentNode->next.load(std::memory_order_acquire);

                if (currentNode->key == key) {
                    if (!prevNode) {
                        // Removing head node
                        if (head.compare_exchange_strong(
                                currentNode, nextNode,
                                std::memory_order_acq_rel,
                                std::memory_order_relaxed)) {
                            return true;
                        }
                    } else {
                        // Removing non-head node
                        if (prevNode->next.compare_exchange_strong(
                                currentNode, nextNode,
                                std::memory_order_acq_rel,
                                std::memory_order_relaxed)) {
                            return true;
                        }
                    }
                    // If compare_exchange failed, reload and try again
                    currentNode = head.load(std::memory_order_acquire);
                    prevNode = nullptr;
                    continue;
                }

                prevNode = currentNode;
                currentNode = nextNode;
            }
            return false;
        }
    };

    std::vector<std::unique_ptr<Bucket>> buckets_;
    std::hash<Key> hasher_;
    std::atomic<size_t> size_{0};

    auto getBucket(const Key& key) const noexcept -> Bucket& {
        auto bucketIndex = hasher_(key) % buckets_.size();
        return *buckets_[bucketIndex];
    }

public:
    explicit LockFreeHashTable(size_t num_buckets = 16)
        : buckets_(std::max(num_buckets, size_t(1))) {
        for (size_t i = 0; i < buckets_.size(); ++i) {
            buckets_[i] = std::make_unique<Bucket>();
        }
    }

    // Support for range constructors (C++20)
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>,
                                     std::pair<Key, Value>>
    explicit LockFreeHashTable(R&& range, size_t num_buckets = 16)
        : LockFreeHashTable(num_buckets) {
        for (auto&& [key, value] : range) {
            insert(key, value);
        }
    }

    auto find(const Key& key) const noexcept
        -> std::optional<std::reference_wrapper<Value>> {
        return getBucket(key).find(key);
    }

    void insert(const Key& key, const Value& value) {
        getBucket(key).insert(key, value);
        size_.fetch_add(1, std::memory_order_relaxed);
    }

    bool erase(const Key& key) noexcept {
        bool result = getBucket(key).erase(key);
        if (result) {
            size_.fetch_sub(1, std::memory_order_relaxed);
        }
        return result;
    }

    [[nodiscard]] auto empty() const noexcept -> bool { return size() == 0; }

    [[nodiscard]] auto size() const noexcept -> size_t {
        return size_.load(std::memory_order_acquire);
    }

    void clear() noexcept {
        for (const auto& bucket : buckets_) {
            auto node =
                bucket->head.exchange(nullptr, std::memory_order_acq_rel);
        }
        size_.store(0, std::memory_order_release);
    }

    auto operator[](const Key& key) -> Value& {
        auto found = find(key);
        if (found) {
            return found->get();
        }

        // Insert default value if not found
        insert(key, Value{});

        // The value must exist now
        auto result = find(key);
        if (!result) {
            THROW_RUNTIME_ERROR("Failed to insert value into hash table");
        }
        return result->get();
    }

    // 迭代器类 - C++20 improvements with concepts
    class Iterator {
    public:
        using iterator_concept = std::forward_iterator_tag;
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const Key&, Value&>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type;

        Iterator(typename std::vector<std::unique_ptr<Bucket>>::const_iterator
                     bucket_iter,
                 typename std::vector<std::unique_ptr<Bucket>>::const_iterator
                     bucket_end,
                 std::shared_ptr<Node> node) noexcept
            : bucket_iter_(bucket_iter),
              bucket_end_(bucket_end),
              node_(std::move(node)) {
            advancePastEmptyBuckets();
        }

        auto operator++() noexcept -> Iterator& {
            if (node_) {
                node_ = node_->next.load(std::memory_order_acquire);
                if (!node_) {
                    ++bucket_iter_;
                    advancePastEmptyBuckets();
                }
            }
            return *this;
        }

        auto operator++(int) noexcept -> Iterator {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        auto operator==(const Iterator& other) const noexcept -> bool {
            return bucket_iter_ == other.bucket_iter_ && node_ == other.node_;
        }

        auto operator!=(const Iterator& other) const noexcept -> bool {
            return !(*this == other);
        }

        auto operator*() const noexcept -> reference {
            return {node_->key, node_->value};
        }

    private:
        void advancePastEmptyBuckets() noexcept {
            while (bucket_iter_ != bucket_end_ && !node_) {
                node_ = (*bucket_iter_)->head.load(std::memory_order_acquire);
                if (!node_) {
                    ++bucket_iter_;
                }
            }
        }

        typename std::vector<std::unique_ptr<Bucket>>::const_iterator
            bucket_iter_;
        typename std::vector<std::unique_ptr<Bucket>>::const_iterator
            bucket_end_;
        std::shared_ptr<Node> node_;
    };

    auto begin() const noexcept -> Iterator {
        auto bucketIter = buckets_.begin();
        auto bucketEnd = buckets_.end();
        std::shared_ptr<Node> node;
        if (bucketIter != bucketEnd) {
            node = (*bucketIter)->head.load(std::memory_order_acquire);
        }
        return Iterator(bucketIter, bucketEnd, node);
    }

    auto end() const noexcept -> Iterator {
        return Iterator(buckets_.end(), buckets_.end(), nullptr);
    }
};

// C++20 concept for thread-safe vector elements
template <typename T>
concept ThreadSafeVectorElem = std::is_nothrow_move_constructible_v<T> &&
                               std::is_nothrow_destructible_v<T>;

template <ThreadSafeVectorElem T>
class ThreadSafeVector {
    std::unique_ptr<std::atomic<T>[]> data_;
    std::atomic<size_t> capacity_;
    std::atomic<size_t> size_;
    mutable std::shared_mutex resize_mutex_;

    void resize() {
        std::unique_lock lock(resize_mutex_);

        size_t oldCapacity = capacity_.load(std::memory_order_relaxed);
        size_t newCapacity = std::max(oldCapacity * 2, size_t(1));

        try {
            auto newData = std::make_unique<std::atomic<T>[]>(newCapacity);

            // Use memory alignment for SIMD
            constexpr size_t CACHE_LINE_SIZE = 64;
            if constexpr (sizeof(T) <= CACHE_LINE_SIZE &&
                          std::is_trivially_copyable_v<T>) {
// Use SIMD-friendly copying for small trivial types
#pragma omp parallel for if (oldCapacity > 1000)
                for (size_t i = 0; i < size_.load(std::memory_order_relaxed);
                     ++i) {
                    newData[i].store(data_[i].load(std::memory_order_relaxed),
                                     std::memory_order_relaxed);
                }
            } else {
                // Standard copying for other types
                for (size_t i = 0; i < size_.load(std::memory_order_relaxed);
                     ++i) {
                    newData[i].store(data_[i].load(std::memory_order_relaxed),
                                     std::memory_order_relaxed);
                }
            }

            // Atomic exchange of data
            data_.swap(newData);
            capacity_.store(newCapacity, std::memory_order_release);
        } catch (const std::exception& e) {
            // Handle allocation failure
            THROW_RUNTIME_ERROR("Failed to resize vector: " +
                                std::string(e.what()));
        }
    }

public:
    explicit ThreadSafeVector(size_t initial_capacity = 16)
        : capacity_(std::max(initial_capacity, size_t(1))), size_(0) {
        try {
            data_ = std::make_unique<std::atomic<T>[]>(capacity_.load());
        } catch (const std::bad_alloc& e) {
            THROW_RUNTIME_ERROR(
                "Failed to allocate memory for ThreadSafeVector");
        }
    }

    // Support for range constructors (C++20)
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, T>
    explicit ThreadSafeVector(R&& range, size_t initial_capacity = 16)
        : ThreadSafeVector(initial_capacity) {
        for (auto&& item : range) {
            pushBack(item);
        }
    }

    void pushBack(const T& value) {
        size_t currentSize = size_.load(std::memory_order_relaxed);
        while (true) {
            if (currentSize < capacity_.load(std::memory_order_relaxed)) {
                if (size_.compare_exchange_weak(currentSize, currentSize + 1,
                                                std::memory_order_acq_rel)) {
                    data_[currentSize].store(value, std::memory_order_release);
                    return;
                }
            } else {
                try {
                    resize();
                } catch (const std::exception& e) {
                    THROW_RUNTIME_ERROR("Push failed: " +
                                        std::string(e.what()));
                }
            }
            currentSize = size_.load(std::memory_order_relaxed);
        }
    }

    void pushBack(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>) {
        size_t currentSize = size_.load(std::memory_order_relaxed);
        while (true) {
            if (currentSize < capacity_.load(std::memory_order_relaxed)) {
                if (size_.compare_exchange_weak(currentSize, currentSize + 1,
                                                std::memory_order_acq_rel)) {
                    data_[currentSize].store(std::move(value),
                                             std::memory_order_release);
                    return;
                }
            } else {
                try {
                    resize();
                } catch (const std::exception& e) {
                    // If resize fails, just return without adding the element
                    return;
                }
            }
            currentSize = size_.load(std::memory_order_relaxed);
        }
    }

    auto popBack() noexcept -> std::optional<T> {
        size_t currentSize = size_.load(std::memory_order_relaxed);
        while (currentSize > 0) {
            if (size_.compare_exchange_weak(currentSize, currentSize - 1,
                                            std::memory_order_acq_rel)) {
                return data_[currentSize - 1].load(std::memory_order_acquire);
            }
            currentSize = size_.load(std::memory_order_relaxed);
        }
        return std::nullopt;
    }

    auto at(size_t index) const -> T {
        if (index >= size_.load(std::memory_order_acquire)) {
            THROW_OUT_OF_RANGE("Index out of range in ThreadSafeVector::at()");
        }
        return data_[index].load(std::memory_order_acquire);
    }

    auto try_at(size_t index) const noexcept -> std::optional<T> {
        if (index >= size_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        return data_[index].load(std::memory_order_acquire);
    }

    [[nodiscard]] auto empty() const noexcept -> bool {
        return size_.load(std::memory_order_acquire) == 0;
    }

    [[nodiscard]] auto getSize() const noexcept -> size_t {
        return size_.load(std::memory_order_acquire);
    }

    [[nodiscard]] auto getCapacity() const noexcept -> size_t {
        return capacity_.load(std::memory_order_acquire);
    }

    void clear() noexcept { size_.store(0, std::memory_order_release); }

    void shrinkToFit() {
        std::unique_lock lock(resize_mutex_);

        size_t currentSize = size_.load(std::memory_order_relaxed);
        size_t currentCapacity = capacity_.load(std::memory_order_relaxed);

        if (currentSize == currentCapacity) {
            return;  // Already at optimal size
        }

        try {
            auto newData = std::make_unique<std::atomic<T>[]>(
                currentSize > 0 ? currentSize : 1);

            for (size_t i = 0; i < currentSize; ++i) {
                newData[i].store(data_[i].load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
            }

            data_.swap(newData);
            capacity_.store(currentSize > 0 ? currentSize : 1,
                            std::memory_order_release);
        } catch (const std::exception& e) {
            // Ignore errors during shrink - it's just an optimization
        }
    }

    auto front() const -> T {
        if (empty()) {
            THROW_OUT_OF_RANGE("Vector is empty in ThreadSafeVector::front()");
        }
        return data_[0].load(std::memory_order_acquire);
    }

    auto try_front() const noexcept -> std::optional<T> {
        if (empty()) {
            return std::nullopt;
        }
        return data_[0].load(std::memory_order_acquire);
    }

    auto back() const -> T {
        size_t currentSize = size_.load(std::memory_order_acquire);
        if (currentSize == 0) {
            THROW_OUT_OF_RANGE("Vector is empty in ThreadSafeVector::back()");
        }
        return data_[currentSize - 1].load(std::memory_order_acquire);
    }

    auto try_back() const noexcept -> std::optional<T> {
        size_t currentSize = size_.load(std::memory_order_acquire);
        if (currentSize == 0) {
            return std::nullopt;
        }
        return data_[currentSize - 1].load(std::memory_order_acquire);
    }

    auto operator[](size_t index) const -> T { return at(index); }

    // C++20: Support for std::span view of the data
    auto get_span() const -> std::span<const T> {
        std::shared_lock lock(resize_mutex_);

        // Create a temporary vector for the span
        std::vector<T> temp(size_.load(std::memory_order_acquire));

        for (size_t i = 0; i < temp.size(); ++i) {
            temp[i] = data_[i].load(std::memory_order_acquire);
        }

        // Return a span of the temporary vector
        // Note: This isn't ideal as it copies data, but we can't return a span
        // of atomic<T>
        return std::span<const T>(temp);
    }
};

// C++20 concept for lock-free list elements
template <typename T>
concept LockFreeListElem = std::is_nothrow_move_constructible_v<T> &&
                           std::is_nothrow_destructible_v<T>;

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

    ~LockFreeList() noexcept = default;  // Smart pointers handle cleanup

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

    void pushFront(const T& value) {
        try {
            auto newNode = std::make_shared<Node>(value);
            pushNodeFront(std::move(newNode));
        } catch (const std::bad_alloc&) {
            // Log memory allocation failure
        }
    }

    void pushFront(T&& value) noexcept(
        std::is_nothrow_move_constructible_v<T>) {
        try {
            auto newNode = std::make_shared<Node>(std::move(value));
            pushNodeFront(std::move(newNode));
        } catch (const std::bad_alloc&) {
            // Log memory allocation failure
        }
    }

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

    auto front() const noexcept -> std::optional<T> {
        auto currentHead = head_.load(std::memory_order_acquire);
        if (currentHead) {
            return std::optional<T>(currentHead->value);
        }
        return std::nullopt;
    }

    [[nodiscard]] bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) == nullptr;
    }

    [[nodiscard]] auto size() const noexcept -> size_t {
        return size_.load(std::memory_order_acquire);
    }

    void clear() noexcept {
        auto currentHead = head_.exchange(nullptr, std::memory_order_acq_rel);
        size_.store(0, std::memory_order_release);
        // Smart pointers handle cleanup automatically
    }

    // Iterator for LockFreeList - C++20 style
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
        // 修复：创建一个临时变量存储当前head
        std::shared_ptr<Node> expected = head_.load(std::memory_order_relaxed);

        // 初始化newNode->next
        newNode->next.store(expected, std::memory_order_relaxed);

        // 尝试更新head_
        while (!head_.compare_exchange_weak(expected, newNode,
                                            std::memory_order_acq_rel,
                                            std::memory_order_relaxed)) {
            // 如果失败，更新newNode->next为新的expected值
            newNode->next.store(expected, std::memory_order_relaxed);
        }

        size_.fetch_add(1, std::memory_order_relaxed);
    }
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_SAFETYPE_HPP
