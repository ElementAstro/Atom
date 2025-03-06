/*
 * small_list.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-17

Description: A Small List Implementation

**************************************************/

#ifndef ATOM_TYPE_SMALL_LIST_HPP
#define ATOM_TYPE_SMALL_LIST_HPP

#include <algorithm>
#include <cstddef>
#include <execution>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace atom::type {

/**
 * @brief A small doubly linked list implementation with modern C++ features.
 *
 * @tparam T The type of elements stored in the list.
 */
template <typename T>
class SmallList {
    // Ensure that T is swappable and comparable
    static_assert(std::is_swappable_v<T>, "T must be swappable");
    static_assert(std::is_copy_constructible_v<T>,
                  "T must be copy constructible");

private:
    /**
     * @brief A node in the doubly linked list.
     */
    struct Node {
        T data;                      ///< The data stored in the node.
        std::unique_ptr<Node> next;  ///< Pointer to the next node.
        Node* prev;                  ///< Pointer to the previous node.

        /**
         * @brief Constructs a node with the given value.
         *
         * @param value The value to store in the node.
         * @throws std::bad_alloc If memory allocation fails.
         */
        explicit Node(const T& value)
            : data(value), next(nullptr), prev(nullptr) {}

        /**
         * @brief Constructs a node with a moved value.
         *
         * @param value The value to move into the node.
         * @throws std::bad_alloc If memory allocation fails.
         */
        explicit Node(T&& value) noexcept(
            std::is_nothrow_move_constructible_v<T>)
            : data(std::move(value)), next(nullptr), prev(nullptr) {}

        /**
         * @brief Constructs a node in-place.
         *
         * @tparam Args The types of the arguments.
         * @param args The arguments to forward to the constructor of T.
         */
        template <typename... Args>
        explicit Node(Args&&... args)
            : data(std::forward<Args>(args)...), next(nullptr), prev(nullptr) {}
    };

    std::unique_ptr<Node> head_;  ///< Pointer to the head of the list.
    Node* tail_;                  ///< Pointer to the tail of the list.
    size_t list_size_;            ///< The number of elements in the list.

    /**
     * @brief Validates that the list is not empty before accessing elements.
     *
     * @throws std::out_of_range If the list is empty.
     */
    void validateNotEmpty() const {
        if (empty()) {
            throw std::out_of_range("Cannot access elements of an empty list");
        }
    }

public:
    /**
     * @brief Default constructor. Constructs an empty list.
     */
    constexpr SmallList() noexcept
        : head_(nullptr), tail_(nullptr), list_size_(0) {}

    /**
     * @brief Constructs a list with elements from an initializer list.
     *
     * @param init_list The initializer list containing elements to add to the
     * list.
     * @throws std::bad_alloc If memory allocation fails.
     */
    constexpr SmallList(std::initializer_list<T> init_list) : SmallList() {
        try {
            reserve(init_list.size());
            for (const auto& value : init_list) {
                pushBack(value);
            }
        } catch (const std::exception& e) {
            clear();
            throw;
        }
    }

    /**
     * @brief Destructor. Clears the list.
     */
    ~SmallList() noexcept { clear(); }

    /**
     * @brief Copy constructor. Constructs a list by copying elements from
     * another list.
     *
     * @param other The list to copy elements from.
     * @throws std::bad_alloc If memory allocation fails.
     */
    SmallList(const SmallList& other) : SmallList() {
        try {
            reserve(other.size());
            for (const auto& value : other) {
                pushBack(value);
            }
        } catch (const std::exception& e) {
            clear();
            throw;
        }
    }

    /**
     * @brief Move constructor. Constructs a list by moving elements from
     * another list.
     *
     * @param other The list to move elements from.
     */
    SmallList(SmallList&& other) noexcept : SmallList() { swap(other); }

    /**
     * @brief Copy assignment operator. Copies elements from another list.
     *
     * @param other The list to copy elements from.
     * @return A reference to the assigned list.
     */
    SmallList& operator=(const SmallList& other) {
        if (this != &other) {
            SmallList temp(other);
            swap(temp);
        }
        return *this;
    }

    /**
     * @brief Move assignment operator. Moves elements from another list.
     *
     * @param other The list to move elements from.
     * @return A reference to the assigned list.
     */
    SmallList& operator=(SmallList&& other) noexcept {
        if (this != &other) {
            clear();
            swap(other);
        }
        return *this;
    }

    /**
     * @brief Reserves memory for a specified number of elements.
     *
     * Note: This is a hint and doesn't guarantee memory allocation as linked
     * lists allocate nodes individually, but it can be used for optimization
     * purposes.
     *
     * @param capacity The number of elements to reserve memory for.
     */
    void reserve(size_t capacity) {
        // This is just a hint for future improvements
        // Linked lists don't benefit from pre-allocation the same way vectors
        // do
        (void)capacity;  // Avoid unused parameter warning
    }

    /**
     * @brief Adds an element to the end of the list.
     *
     * @param value The value to add.
     * @throws std::bad_alloc If memory allocation fails.
     */
    void pushBack(const T& value) {
        try {
            auto newNode = std::make_unique<Node>(value);
            if (empty()) {
                head_ = std::move(newNode);
                tail_ = head_.get();
            } else {
                newNode->prev = tail_;
                tail_->next = std::move(newNode);
                tail_ = tail_->next.get();
            }
            ++list_size_;
        } catch (const std::exception& e) {
            throw;
        }
    }

    /**
     * @brief Adds an element to the end of the list using move semantics.
     *
     * @param value The value to move into the list.
     * @throws std::bad_alloc If memory allocation fails.
     */
    void pushBack(T&& value) {
        try {
            auto newNode = std::make_unique<Node>(std::move(value));
            if (empty()) {
                head_ = std::move(newNode);
                tail_ = head_.get();
            } else {
                newNode->prev = tail_;
                tail_->next = std::move(newNode);
                tail_ = tail_->next.get();
            }
            ++list_size_;
        } catch (const std::exception& e) {
            throw;
        }
    }

    /**
     * @brief Adds an element to the front of the list.
     *
     * @param value The value to add.
     * @throws std::bad_alloc If memory allocation fails.
     */
    void pushFront(const T& value) {
        try {
            auto newNode = std::make_unique<Node>(value);
            if (empty()) {
                head_ = std::move(newNode);
                tail_ = head_.get();
            } else {
                newNode->next = std::move(head_);
                head_->prev = newNode.get();
                head_ = std::move(newNode);
            }
            ++list_size_;
        } catch (const std::exception& e) {
            throw;
        }
    }

    /**
     * @brief Adds an element to the front of the list using move semantics.
     *
     * @param value The value to move into the list.
     * @throws std::bad_alloc If memory allocation fails.
     */
    void pushFront(T&& value) {
        try {
            auto newNode = std::make_unique<Node>(std::move(value));
            if (empty()) {
                head_ = std::move(newNode);
                tail_ = head_.get();
            } else {
                newNode->next = std::move(head_);
                head_->prev = newNode.get();
                head_ = std::move(newNode);
            }
            ++list_size_;
        } catch (const std::exception& e) {
            throw;
        }
    }

    /**
     * @brief Removes the last element from the list.
     *
     * @throws std::out_of_range If the list is empty.
     */
    void popBack() {
        validateNotEmpty();

        if (tail_ == head_.get()) {
            head_.reset();
            tail_ = nullptr;
        } else {
            tail_ = tail_->prev;
            tail_->next.reset();
        }
        --list_size_;
    }

    /**
     * @brief Removes the first element from the list.
     *
     * @throws std::out_of_range If the list is empty.
     */
    void popFront() {
        validateNotEmpty();

        if (head_.get() == tail_) {
            head_.reset();
            tail_ = nullptr;
        } else {
            Node* nextNode = head_->next.get();
            head_ = std::move(head_->next);
            head_->prev = nullptr;
        }
        --list_size_;
    }

    /**
     * @brief Returns a reference to the first element in the list.
     *
     * @return A reference to the first element.
     * @throws std::out_of_range If the list is empty.
     */
    [[nodiscard]] T& front() {
        validateNotEmpty();
        return head_->data;
    }

    /**
     * @brief Returns a const reference to the first element in the list.
     *
     * @return A const reference to the first element.
     * @throws std::out_of_range If the list is empty.
     */
    [[nodiscard]] const T& front() const {
        validateNotEmpty();
        return head_->data;
    }

    /**
     * @brief Returns a reference to the last element in the list.
     *
     * @return A reference to the last element.
     * @throws std::out_of_range If the list is empty.
     */
    [[nodiscard]] T& back() {
        validateNotEmpty();
        return tail_->data;
    }

    /**
     * @brief Returns a const reference to the last element in the list.
     *
     * @return A const reference to the last element.
     * @throws std::out_of_range If the list is empty.
     */
    [[nodiscard]] const T& back() const {
        validateNotEmpty();
        return tail_->data;
    }

    /**
     * @brief Safely gets the first element if it exists.
     *
     * @return An optional containing the first element, or empty if the list is
     * empty.
     */
    [[nodiscard]] std::optional<std::reference_wrapper<T>> tryFront() noexcept {
        if (empty()) {
            return std::nullopt;
        }
        return std::optional<std::reference_wrapper<T>>(head_->data);
    }

    /**
     * @brief Safely gets the last element if it exists.
     *
     * @return An optional containing the last element, or empty if the list is
     * empty.
     */
    [[nodiscard]] std::optional<std::reference_wrapper<T>> tryBack() noexcept {
        if (empty()) {
            return std::nullopt;
        }
        return std::optional<std::reference_wrapper<T>>(tail_->data);
    }

    /**
     * @brief Checks if the list is empty.
     *
     * @return True if the list is empty, false otherwise.
     */
    [[nodiscard]] constexpr bool empty() const noexcept {
        return list_size_ == 0;
    }

    /**
     * @brief Returns the number of elements in the list.
     *
     * @return The number of elements in the list.
     */
    [[nodiscard]] constexpr size_t size() const noexcept { return list_size_; }

    /**
     * @brief Clears the list, removing all elements.
     */
    void clear() noexcept {
        // Start from the head and destroy each node
        head_.reset();
        tail_ = nullptr;
        list_size_ = 0;
    }

    /**
     * @brief A bidirectional iterator for the list.
     */
    class Iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        /**
         * @brief Constructs an iterator pointing to the given node.
         *
         * @param ptr The node to point to.
         */
        explicit Iterator(Node* ptr = nullptr) noexcept : nodePtr(ptr) {}

        /**
         * @brief Advances the iterator to the next element.
         *
         * @return A reference to the iterator.
         */
        Iterator& operator++() noexcept {
            if (nodePtr) {
                nodePtr = nodePtr->next.get();
            }
            return *this;
        }

        /**
         * @brief Advances the iterator to the next element.
         *
         * @return A copy of the iterator before the increment.
         */
        Iterator operator++(int) noexcept {
            Iterator temp = *this;
            ++(*this);
            return temp;
        }

        /**
         * @brief Moves the iterator to the previous element.
         *
         * @return A reference to the iterator.
         * @throws std::out_of_range If the iterator is already at the
         * beginning.
         */
        Iterator& operator--() {
            if (!nodePtr && !hasPrevious()) {
                throw std::out_of_range(
                    "Cannot decrement iterator at the beginning of the list");
            }
            nodePtr = nodePtr->prev;
            return *this;
        }

        /**
         * @brief Moves the iterator to the previous element.
         *
         * @return A copy of the iterator before the decrement.
         * @throws std::out_of_range If the iterator is already at the
         * beginning.
         */
        Iterator operator--(int) {
            Iterator temp = *this;
            --(*this);
            return temp;
        }

        /**
         * @brief Checks if two iterators are equal.
         *
         * @param other The iterator to compare with.
         * @return True if the iterators are equal, false otherwise.
         */
        bool operator==(const Iterator& other) const noexcept {
            return nodePtr == other.nodePtr;
        }

        /**
         * @brief Checks if two iterators are not equal.
         *
         * @param other The iterator to compare with.
         * @return True if the iterators are not equal, false otherwise.
         */
        bool operator!=(const Iterator& other) const noexcept {
            return !(*this == other);
        }

        /**
         * @brief Dereferences the iterator to access the element.
         *
         * @return A reference to the element.
         * @throws std::out_of_range If the iterator is at the end.
         */
        reference operator*() const {
            if (!nodePtr) {
                throw std::out_of_range("Cannot dereference end iterator");
            }
            return nodePtr->data;
        }

        /**
         * @brief Dereferences the iterator to access the element.
         *
         * @return A pointer to the element.
         * @throws std::out_of_range If the iterator is at the end.
         */
        pointer operator->() const {
            if (!nodePtr) {
                throw std::out_of_range("Cannot dereference end iterator");
            }
            return &(nodePtr->data);
        }

        /**
         * @brief Checks if the iterator has a previous element.
         *
         * @return True if the iterator has a previous element, false otherwise.
         */
        [[nodiscard]] bool hasPrevious() const noexcept {
            return nodePtr && nodePtr->prev;
        }

        /**
         * @brief Checks if the iterator has a next element.
         *
         * @return True if the iterator has a next element, false otherwise.
         */
        [[nodiscard]] bool hasNext() const noexcept {
            return nodePtr && nodePtr->next;
        }

        Node* nodePtr;  ///< Pointer to the current node.
    };

    /**
     * @brief A const bidirectional iterator for the list.
     */
    class ConstIterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = const T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        /**
         * @brief Constructs a const iterator pointing to the given node.
         *
         * @param ptr The node to point to.
         */
        explicit ConstIterator(const Node* ptr = nullptr) noexcept
            : nodePtr(ptr) {}

        /**
         * @brief Constructs a const iterator from a non-const iterator.
         *
         * @param it The non-const iterator to convert.
         */
        ConstIterator(const Iterator& it) noexcept : nodePtr(it.nodePtr) {}

        /**
         * @brief Advances the iterator to the next element.
         *
         * @return A reference to the iterator.
         */
        ConstIterator& operator++() noexcept {
            if (nodePtr) {
                nodePtr = nodePtr->next.get();
            }
            return *this;
        }

        /**
         * @brief Advances the iterator to the next element.
         *
         * @return A copy of the iterator before the increment.
         */
        ConstIterator operator++(int) noexcept {
            ConstIterator temp = *this;
            ++(*this);
            return temp;
        }

        /**
         * @brief Moves the iterator to the previous element.
         *
         * @return A reference to the iterator.
         * @throws std::out_of_range If the iterator is already at the
         * beginning.
         */
        ConstIterator& operator--() {
            if (!nodePtr || !nodePtr->prev) {
                throw std::out_of_range(
                    "Cannot decrement iterator at the beginning of the list");
            }
            nodePtr = nodePtr->prev;
            return *this;
        }

        /**
         * @brief Moves the iterator to the previous element.
         *
         * @return A copy of the iterator before the decrement.
         * @throws std::out_of_range If the iterator is already at the
         * beginning.
         */
        ConstIterator operator--(int) {
            ConstIterator temp = *this;
            --(*this);
            return temp;
        }

        /**
         * @brief Checks if two iterators are equal.
         *
         * @param other The iterator to compare with.
         * @return True if the iterators are equal, false otherwise.
         */
        bool operator==(const ConstIterator& other) const noexcept {
            return nodePtr == other.nodePtr;
        }

        /**
         * @brief Checks if two iterators are not equal.
         *
         * @param other The iterator to compare with.
         * @return True if the iterators are not equal, false otherwise.
         */
        bool operator!=(const ConstIterator& other) const noexcept {
            return !(*this == other);
        }

        /**
         * @brief Dereferences the iterator to access the element.
         *
         * @return A const reference to the element.
         * @throws std::out_of_range If the iterator is at the end.
         */
        reference operator*() const {
            if (!nodePtr) {
                throw std::out_of_range("Cannot dereference end iterator");
            }
            return nodePtr->data;
        }

        /**
         * @brief Dereferences the iterator to access the element.
         *
         * @return A const pointer to the element.
         * @throws std::out_of_range If the iterator is at the end.
         */
        pointer operator->() const {
            if (!nodePtr) {
                throw std::out_of_range("Cannot dereference end iterator");
            }
            return &(nodePtr->data);
        }

        const Node* nodePtr;  ///< Pointer to the current node.
    };

    /**
     * @brief A reverse iterator for the list.
     */
    using ReverseIterator = std::reverse_iterator<Iterator>;

    /**
     * @brief A const reverse iterator for the list.
     */
    using ConstReverseIterator = std::reverse_iterator<ConstIterator>;

    /**
     * @brief Returns an iterator to the beginning of the list.
     *
     * @return An iterator to the beginning of the list.
     */
    [[nodiscard]] Iterator begin() noexcept { return Iterator(head_.get()); }

    /**
     * @brief Returns an iterator to the end of the list.
     *
     * @return An iterator to the end of the list.
     */
    [[nodiscard]] Iterator end() noexcept { return Iterator(nullptr); }

    /**
     * @brief Returns a const iterator to the beginning of the list.
     *
     * @return A const iterator to the beginning of the list.
     */
    [[nodiscard]] ConstIterator begin() const noexcept {
        return ConstIterator(head_.get());
    }

    /**
     * @brief Returns a const iterator to the end of the list.
     *
     * @return A const iterator to the end of the list.
     */
    [[nodiscard]] ConstIterator end() const noexcept {
        return ConstIterator(nullptr);
    }

    /**
     * @brief Returns a const iterator to the beginning of the list.
     *
     * @return A const iterator to the beginning of the list.
     */
    [[nodiscard]] ConstIterator cbegin() const noexcept {
        return ConstIterator(head_.get());
    }

    /**
     * @brief Returns a const iterator to the end of the list.
     *
     * @return A const iterator to the end of the list.
     */
    [[nodiscard]] ConstIterator cend() const noexcept {
        return ConstIterator(nullptr);
    }

    /**
     * @brief Returns a reverse iterator to the beginning of the reversed list.
     *
     * @return A reverse iterator to the beginning of the reversed list.
     */
    [[nodiscard]] ReverseIterator rbegin() noexcept {
        return ReverseIterator(end());
    }

    /**
     * @brief Returns a reverse iterator to the end of the reversed list.
     *
     * @return A reverse iterator to the end of the reversed list.
     */
    [[nodiscard]] ReverseIterator rend() noexcept {
        return ReverseIterator(begin());
    }

    /**
     * @brief Returns a const reverse iterator to the beginning of the reversed
     * list.
     *
     * @return A const reverse iterator to the beginning of the reversed list.
     */
    [[nodiscard]] ConstReverseIterator crbegin() const noexcept {
        return ConstReverseIterator(cend());
    }

    /**
     * @brief Returns a const reverse iterator to the end of the reversed list.
     *
     * @return A const reverse iterator to the end of the reversed list.
     */
    [[nodiscard]] ConstReverseIterator crend() const noexcept {
        return ConstReverseIterator(cbegin());
    }

    /**
     * @brief Inserts an element at the specified position.
     *
     * @param pos The position to insert the element at.
     * @param value The value to insert.
     * @throws std::bad_alloc If memory allocation fails.
     * @return An iterator pointing to the inserted element.
     */
    Iterator insert(Iterator pos, const T& value) {
        try {
            if (pos == begin()) {
                pushFront(value);
                return begin();
            } else if (pos == end()) {
                pushBack(value);
                return Iterator(tail_);
            } else {
                auto newNode = std::make_unique<Node>(value);
                Node* prevNode = pos.nodePtr->prev;
                Node* currentNode = pos.nodePtr;

                newNode->prev = prevNode;
                newNode->next = std::unique_ptr<Node>(nullptr);

                prevNode->next.release();
                newNode->next = std::unique_ptr<Node>(currentNode);

                prevNode->next = std::move(newNode);
                currentNode->prev = prevNode->next.get();

                ++list_size_;
                return Iterator(currentNode->prev);
            }
        } catch (const std::exception& e) {
            throw;
        }
    }

    /**
     * @brief Inserts an element at the specified position using move semantics.
     *
     * @param pos The position to insert the element at.
     * @param value The value to move into the list.
     * @throws std::bad_alloc If memory allocation fails.
     * @return An iterator pointing to the inserted element.
     */
    Iterator insert(Iterator pos, T&& value) {
        try {
            if (pos == begin()) {
                pushFront(std::move(value));
                return begin();
            } else if (pos == end()) {
                pushBack(std::move(value));
                return Iterator(tail_);
            } else {
                auto newNode = std::make_unique<Node>(std::move(value));
                Node* prevNode = pos.nodePtr->prev;
                Node* currentNode = pos.nodePtr;

                newNode->prev = prevNode;
                newNode->next = std::unique_ptr<Node>(nullptr);

                prevNode->next.release();
                newNode->next = std::unique_ptr<Node>(currentNode);

                prevNode->next = std::move(newNode);
                currentNode->prev = prevNode->next.get();

                ++list_size_;
                return Iterator(currentNode->prev);
            }
        } catch (const std::exception& e) {
            throw;
        }
    }

    /**
     * @brief Erases the element at the specified position.
     *
     * @param pos The position of the element to erase.
     * @return An iterator to the element following the erased element.
     * @throws std::invalid_argument If the position is invalid.
     */
    Iterator erase(Iterator pos) {
        if (empty() || pos == end()) {
            throw std::invalid_argument(
                "Cannot erase from an empty list or at end position");
        }

        if (pos == begin()) {
            popFront();
            return begin();
        }

        if (pos.nodePtr == tail_) {
            popBack();
            return end();
        }

        Node* prevNode = pos.nodePtr->prev;
        Node* nextNode = pos.nodePtr->next.get();

        // Save the next node pointer for return value
        std::unique_ptr<Node> nextNodeOwnership = std::move(pos.nodePtr->next);

        // Connect previous node to the next node
        prevNode->next = std::move(nextNodeOwnership);
        nextNode->prev = prevNode;

        --list_size_;
        return Iterator(nextNode);
    }

    /**
     * @brief Removes all elements with the specified value.
     *
     * @param value The value to remove.
     * @return The number of elements removed.
     */
    size_t remove(const T& value) {
        size_t removedCount = 0;

        for (auto it = begin(); it != end();) {
            if (*it == value) {
                it = erase(it);
                ++removedCount;
            } else {
                ++it;
            }
        }

        return removedCount;
    }

    /**
     * @brief Removes elements that satisfy a predicate.
     *
     * @tparam Predicate The type of the predicate function.
     * @param pred The predicate function.
     * @return The number of elements removed.
     */
    template <typename Predicate>
    size_t removeIf(Predicate pred) {
        size_t removedCount = 0;

        for (auto it = begin(); it != end();) {
            if (pred(*it)) {
                it = erase(it);
                ++removedCount;
            } else {
                ++it;
            }
        }

        return removedCount;
    }

    /**
     * @brief Removes consecutive duplicate elements from the list.
     *
     * @return The number of elements removed.
     */
    size_t unique() {
        if (size() <= 1) {
            return 0;
        }

        size_t removedCount = 0;
        for (auto it = begin(); it != end();) {
            auto nextIt = it;
            ++nextIt;

            if (nextIt != end() && *it == *nextIt) {
                it = erase(nextIt);
                ++removedCount;
            } else {
                ++it;
            }
        }

        return removedCount;
    }

    /**
     * @brief Sorts the elements in the list.
     *
     * Uses a hybrid approach: converts to vector for sorting, then rebuilds the
     * list. This is much faster than in-place linked list sorting for large
     * lists.
     */
    void sort() {
        if (size() <= 1) {
            return;
        }

        try {
            // Convert list to vector for faster sorting
            std::vector<T> tempVector;
            tempVector.reserve(size());

            for (const auto& item : *this) {
                tempVector.push_back(item);
            }

            // Sort the vector (potentially using parallel algorithms for large
            // datasets)
            if (size() > 10000) {
                std::sort(std::execution::par_unseq, tempVector.begin(),
                          tempVector.end());
            } else {
                std::sort(tempVector.begin(), tempVector.end());
            }

            // Rebuild the list
            clear();
            for (const auto& item : tempVector) {
                pushBack(item);
            }
        } catch (const std::exception& e) {
            // If something goes wrong, fall back to the original implementation
            std::unique_ptr<SmallList<T>> temp =
                std::make_unique<SmallList<T>>();

            while (!empty()) {
                auto it = begin();
                T minVal = *it;
                for (auto curr = ++begin(); curr != end(); ++curr) {
                    if (*curr < minVal) {
                        minVal = *curr;
                        it = curr;
                    }
                }
                temp->pushBack(minVal);
                erase(it);
            }

            swap(*temp);
        }
    }

    /**
     * @brief Sorts the elements in the list using a custom comparison function.
     *
     * @tparam Compare The type of the comparison function.
     * @param comp The comparison function.
     */
    template <typename Compare>
    void sort(Compare comp) {
        if (size() <= 1) {
            return;
        }

        try {
            // Convert list to vector for faster sorting
            std::vector<T> tempVector;
            tempVector.reserve(size());

            for (const auto& item : *this) {
                tempVector.push_back(item);
            }

            // Sort the vector (potentially using parallel algorithms for large
            // datasets)
            if (size() > 10000) {
                std::sort(std::execution::par_unseq, tempVector.begin(),
                          tempVector.end(), comp);
            } else {
                std::sort(tempVector.begin(), tempVector.end(), comp);
            }

            // Rebuild the list
            clear();
            for (const auto& item : tempVector) {
                pushBack(item);
            }
        } catch (const std::exception& e) {
            // Fall back to manual selection sort
            std::unique_ptr<SmallList<T>> temp =
                std::make_unique<SmallList<T>>();

            while (!empty()) {
                auto it = begin();
                T minVal = *it;
                for (auto curr = ++begin(); curr != end(); ++curr) {
                    if (comp(*curr, minVal)) {
                        minVal = *curr;
                        it = curr;
                    }
                }
                temp->pushBack(minVal);
                erase(it);
            }

            swap(*temp);
        }
    }

    /**
     * @brief Merges two sorted lists.
     *
     * @param other The other sorted list to merge with.
     * @throws std::invalid_argument If either list is not sorted.
     */
    void merge(SmallList<T>& other) {
        // Verify both lists are sorted
        if (!isSorted() || !other.isSorted()) {
            throw std::invalid_argument(
                "Both lists must be sorted before merging");
        }

        if (other.empty()) {
            return;
        }

        if (this == &other) {
            return;  // Self-merge does nothing
        }

        auto it = begin();
        auto otherIt = other.begin();

        while (it != end() && otherIt != other.end()) {
            if (*otherIt < *it) {
                // Insert other's element before current position
                auto nextOtherIt = otherIt;
                ++nextOtherIt;
                insert(it, std::move(*otherIt));
                otherIt = nextOtherIt;
            } else {
                ++it;
            }
        }

        // Add any remaining elements from other
        while (otherIt != other.end()) {
            auto nextOtherIt = otherIt;
            ++nextOtherIt;
            pushBack(std::move(*otherIt));
            otherIt = nextOtherIt;
        }

        // Clear the other list since we've moved all its elements
        other.clear();
    }

    /**
     * @brief Checks if the list is sorted.
     *
     * @return True if the list is sorted in non-decreasing order, false
     * otherwise.
     */
    [[nodiscard]] bool isSorted() const {
        if (size() <= 1) {
            return true;
        }

        auto it = begin();
        auto prev = it++;

        while (it != end()) {
            if (*it < *prev) {
                return false;
            }
            prev = it++;
        }

        return true;
    }

    /**
     * @brief Swaps the contents of this list with another list.
     *
     * @param other The list to swap contents with.
     */
    void swap(SmallList<T>& other) noexcept {
        std::swap(head_, other.head_);
        std::swap(tail_, other.tail_);
        std::swap(list_size_, other.list_size_);
    }

    /**
     * @brief Constructs an element in place at the end of the list.
     *
     * @tparam Args The types of the arguments to construct the element with.
     * @param args The arguments to construct the element with.
     * @throws std::bad_alloc If memory allocation fails.
     */
    template <typename... Args>
    void emplaceBack(Args&&... args) {
        try {
            auto newNode = std::make_unique<Node>(std::forward<Args>(args)...);
            if (empty()) {
                head_ = std::move(newNode);
                tail_ = head_.get();
            } else {
                newNode->prev = tail_;
                tail_->next = std::move(newNode);
                tail_ = tail_->next.get();
            }
            ++list_size_;
        } catch (const std::exception& e) {
            throw;
        }
    }

    /**
     * @brief Constructs an element in place at the front of the list.
     *
     * @tparam Args The types of the arguments to construct the element with.
     * @param args The arguments to construct the element with.
     * @throws std::bad_alloc If memory allocation fails.
     */
    template <typename... Args>
    void emplaceFront(Args&&... args) {
        try {
            auto newNode = std::make_unique<Node>(std::forward<Args>(args)...);
            if (empty()) {
                head_ = std::move(newNode);
                tail_ = head_.get();
            } else {
                newNode->next = std::move(head_);
                head_->prev = newNode.get();
                head_ = std::move(newNode);
            }
            ++list_size_;
        } catch (const std::exception& e) {
            throw;
        }
    }

    /**
     * @brief Constructs an element in place at the specified position.
     *
     * @tparam Args The types of the arguments to construct the element with.
     * @param pos The position to construct the element at.
     * @param args The arguments to construct the element with.
     * @return An iterator to the constructed element.
     * @throws std::bad_alloc If memory allocation fails.
     */
    template <typename... Args>
    Iterator emplace(Iterator pos, Args&&... args) {
        try {
            if (pos == begin()) {
                emplaceFront(std::forward<Args>(args)...);
                return begin();
            } else if (pos == end()) {
                emplaceBack(std::forward<Args>(args)...);
                return Iterator(tail_);
            } else {
                auto newNode =
                    std::make_unique<Node>(std::forward<Args>(args)...);
                Node* prevNode = pos.nodePtr->prev;
                Node* currentNode = pos.nodePtr;

                newNode->prev = prevNode;
                newNode->next = std::unique_ptr<Node>(nullptr);

                prevNode->next.release();
                newNode->next = std::unique_ptr<Node>(currentNode);

                prevNode->next = std::move(newNode);
                currentNode->prev = prevNode->next.get();

                ++list_size_;
                return Iterator(currentNode->prev);
            }
        } catch (const std::exception& e) {
            throw;
        }
    }

    /**
     * @brief Resizes the list to contain the specified number of elements.
     *
     * If the current size is greater than count, the list is reduced to its
     * first count elements. If the current size is less than count, additional
     * default-inserted elements are appended.
     *
     * @param count The new size of the list.
     * @throws std::bad_alloc If memory allocation fails.
     */
    void resize(size_t count) {
        if (count < size()) {
            while (size() > count) {
                popBack();
            }
        } else if (count > size()) {
            while (size() < count) {
                try {
                    emplaceBack();
                } catch (const std::exception& e) {
                    throw;
                }
            }
        }
    }

    /**
     * @brief Resizes the list to contain the specified number of elements.
     *
     * If the current size is greater than count, the list is reduced to its
     * first count elements. If the current size is less than count, additional
     * copies of value are appended.
     *
     * @param count The new size of the list.
     * @param value The value to use for the new elements.
     * @throws std::bad_alloc If memory allocation fails.
     */
    void resize(size_t count, const T& value) {
        if (count < size()) {
            while (size() > count) {
                popBack();
            }
        } else if (count > size()) {
            while (size() < count) {
                try {
                    pushBack(value);
                } catch (const std::exception& e) {
                    throw;
                }
            }
        }
    }

    /**
     * @brief Reverses the order of elements in the list.
     */
    void reverse() noexcept {
        if (size() <= 1) {
            return;
        }

        Node* current = head_.get();
        Node* temp = nullptr;

        // Swap next and prev pointers for all nodes
        while (current) {
            // Store next node
            temp = current->next.get();

            // Swap next and prev pointers
            std::swap(current->next,
                      reinterpret_cast<std::unique_ptr<Node>&>(current->prev));

            // Update prev to point to the correct node (as a raw pointer)
            if (current->next) {
                current->next->prev = current;
            }

            // Move to the next node
            current = temp;
        }

        // Swap head and tail
        std::swap(head_, reinterpret_cast<std::unique_ptr<Node>&>(tail_));
    }

    /**
     * @brief Splices elements from another list into this list.
     *
     * @param pos Position in this list where elements will be inserted.
     * @param other The list from which elements will be taken.
     * @throws std::invalid_argument If trying to splice a list into itself.
     * @throws std::bad_alloc If memory allocation fails.
     */
    void splice(Iterator pos, SmallList<T>& other) {
        if (this == &other) {
            throw std::invalid_argument("Cannot splice a list into itself");
        }

        if (other.empty()) {
            return;
        }

        try {
            // Create a copy of the other list's elements
            SmallList<T> tempList;
            for (const auto& item : other) {
                tempList.pushBack(item);
            }

            // Insert the elements at the specified position
            for (auto& item : tempList) {
                pos = insert(pos, std::move(item));
                ++pos;
            }

            // Clear the other list
            other.clear();
        } catch (const std::exception& e) {
            throw;
        }
    }

    /**
     * @brief Compares two lists for equality.
     *
     * @param lhs The first list.
     * @param rhs The second list.
     * @return True if the lists are equal, false otherwise.
     */
    friend bool operator==(const SmallList<T>& lhs, const SmallList<T>& rhs) {
        if (lhs.size() != rhs.size()) {
            return false;
        }

        return std::equal(lhs.begin(), lhs.end(), rhs.begin());
    }

    /**
     * @brief Compares two lists for inequality.
     *
     * @param lhs The first list.
     * @param rhs The second list.
     * @return True if the lists are not equal, false otherwise.
     */
    friend bool operator!=(const SmallList<T>& lhs, const SmallList<T>& rhs) {
        return !(lhs == rhs);
    }

    /**
     * @brief Lexicographically compares two lists.
     *
     * @param lhs The first list.
     * @param rhs The second list.
     * @return True if lhs is lexicographically less than rhs, false otherwise.
     */
    friend bool operator<(const SmallList<T>& lhs, const SmallList<T>& rhs) {
        return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(),
                                            rhs.end());
    }

    /**
     * @brief Lexicographically compares two lists.
     *
     * @param lhs The first list.
     * @param rhs The second list.
     * @return True if lhs is lexicographically less than or equal to rhs, false
     * otherwise.
     */
    friend bool operator<=(const SmallList<T>& lhs, const SmallList<T>& rhs) {
        return !(rhs < lhs);
    }

    /**
     * @brief Lexicographically compares two lists.
     *
     * @param lhs The first list.
     * @param rhs The second list.
     * @return True if lhs is lexicographically greater than rhs, false
     * otherwise.
     */
    friend bool operator>(const SmallList<T>& lhs, const SmallList<T>& rhs) {
        return rhs < lhs;
    }

    /**
     * @brief Lexicographically compares two lists.
     *
     * @param lhs The first list.
     * @param rhs The second list.
     * @return True if lhs is lexicographically greater than or equal to rhs,
     * false otherwise.
     */
    friend bool operator>=(const SmallList<T>& lhs, const SmallList<T>& rhs) {
        return !(lhs < rhs);
    }

    /**
     * @brief Non-member swap function.
     *
     * @param lhs The first list to swap.
     * @param rhs The second list to swap.
     */
    friend void swap(SmallList<T>& lhs, SmallList<T>& rhs) noexcept {
        lhs.swap(rhs);
    }
};

}  // namespace atom::type

#endif  // ATOM_TYPE_SMALL_LIST_HPP
