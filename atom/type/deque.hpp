/*
 * atom/type/deque.hpp
 *
 * Copyright (C) 2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-11

Description: Optimized deque and circular buffer implementations

**************************************************/

#pragma once

#include <algorithm>
#include <cassert>
#include <memory>
#include <stdexcept>
#include <vector>

namespace atom {
namespace containers {

/**
 * @brief Optimized circular buffer with configurable growth policy
 *
 * Provides efficient circular buffer operations with optional automatic
 * resizing when the buffer becomes full.
 *
 * @tparam T Element type
 * @tparam Allocator Allocator type
 */
template <typename T, typename Allocator = std::allocator<T>>
class circular_buffer {
public:
    using value_type = T;
    using allocator_type = Allocator;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = typename std::allocator_traits<Allocator>::pointer;
    using const_pointer =
        typename std::allocator_traits<Allocator>::const_pointer;

private:
    allocator_type alloc_;
    pointer buffer_;
    size_type capacity_;
    size_type size_;
    size_type head_;  // Index of first element
    size_type tail_;  // Index of next insertion point
    bool auto_resize_;

public:
    /**
     * @brief Constructor
     *
     * @param capacity Initial capacity
     * @param auto_resize Whether to automatically resize when full
     * @param alloc Allocator instance
     */
    explicit circular_buffer(size_type capacity = 16, bool auto_resize = false,
                             const allocator_type& alloc = allocator_type())
        : alloc_(alloc),
          buffer_(std::allocator_traits<Allocator>::allocate(alloc_, capacity)),
          capacity_(capacity),
          size_(0),
          head_(0),
          tail_(0),
          auto_resize_(auto_resize) {
        assert(capacity > 0);
    }

    /**
     * @brief Destructor
     */
    ~circular_buffer() {
        clear();
        std::allocator_traits<Allocator>::deallocate(alloc_, buffer_,
                                                     capacity_);
    }

    /**
     * @brief Copy constructor
     */
    circular_buffer(const circular_buffer& other)
        : alloc_(std::allocator_traits<Allocator>::
                     select_on_container_copy_construction(other.alloc_)),
          buffer_(std::allocator_traits<Allocator>::allocate(alloc_,
                                                             other.capacity_)),
          capacity_(other.capacity_),
          size_(0),
          head_(0),
          tail_(0),
          auto_resize_(other.auto_resize_) {
        for (size_type i = 0; i < other.size_; ++i) {
            push_back(other[i]);
        }
    }

    /**
     * @brief Move constructor
     */
    circular_buffer(circular_buffer&& other) noexcept
        : alloc_(std::move(other.alloc_)),
          buffer_(other.buffer_),
          capacity_(other.capacity_),
          size_(other.size_),
          head_(other.head_),
          tail_(other.tail_),
          auto_resize_(other.auto_resize_) {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
        other.size_ = 0;
        other.head_ = 0;
        other.tail_ = 0;
    }

    /**
     * @brief Copy assignment
     */
    circular_buffer& operator=(const circular_buffer& other) {
        if (this != &other) {
            circular_buffer temp(other);
            swap(temp);
        }
        return *this;
    }

    /**
     * @brief Move assignment
     */
    circular_buffer& operator=(circular_buffer&& other) noexcept {
        if (this != &other) {
            clear();
            std::allocator_traits<Allocator>::deallocate(alloc_, buffer_,
                                                         capacity_);

            alloc_ = std::move(other.alloc_);
            buffer_ = other.buffer_;
            capacity_ = other.capacity_;
            size_ = other.size_;
            head_ = other.head_;
            tail_ = other.tail_;
            auto_resize_ = other.auto_resize_;

            other.buffer_ = nullptr;
            other.capacity_ = 0;
            other.size_ = 0;
            other.head_ = 0;
            other.tail_ = 0;
        }
        return *this;
    }

    /**
     * @brief Add element to the back
     *
     * @param value Element to add
     */
    void push_back(const T& value) {
        if (size_ == capacity_) {
            if (auto_resize_) {
                resize_internal(capacity_ * 2);
            } else {
                // Overwrite oldest element
                pop_front();
            }
        }

        std::allocator_traits<Allocator>::construct(alloc_, &buffer_[tail_],
                                                    value);
        tail_ = (tail_ + 1) % capacity_;
        ++size_;
    }

    /**
     * @brief Add element to the back (move version)
     *
     * @param value Element to add
     */
    void push_back(T&& value) {
        if (size_ == capacity_) {
            if (auto_resize_) {
                resize_internal(capacity_ * 2);
            } else {
                // Overwrite oldest element
                pop_front();
            }
        }

        std::allocator_traits<Allocator>::construct(alloc_, &buffer_[tail_],
                                                    std::move(value));
        tail_ = (tail_ + 1) % capacity_;
        ++size_;
    }

    /**
     * @brief Add element to the front
     *
     * @param value Element to add
     */
    void push_front(const T& value) {
        if (size_ == capacity_) {
            if (auto_resize_) {
                resize_internal(capacity_ * 2);
            } else {
                // Overwrite newest element
                pop_back();
            }
        }

        head_ = (head_ + capacity_ - 1) % capacity_;
        std::allocator_traits<Allocator>::construct(alloc_, &buffer_[head_],
                                                    value);
        ++size_;
    }

    /**
     * @brief Add element to the front (move version)
     *
     * @param value Element to add
     */
    void push_front(T&& value) {
        if (size_ == capacity_) {
            if (auto_resize_) {
                resize_internal(capacity_ * 2);
            } else {
                // Overwrite newest element
                pop_back();
            }
        }

        head_ = (head_ + capacity_ - 1) % capacity_;
        std::allocator_traits<Allocator>::construct(alloc_, &buffer_[head_],
                                                    std::move(value));
        ++size_;
    }

    /**
     * @brief Remove element from the front
     */
    void pop_front() {
        if (empty()) {
            throw std::runtime_error(
                "pop_front() called on empty circular_buffer");
        }

        std::allocator_traits<Allocator>::destroy(alloc_, &buffer_[head_]);
        head_ = (head_ + 1) % capacity_;
        --size_;
    }

    /**
     * @brief Remove element from the back
     */
    void pop_back() {
        if (empty()) {
            throw std::runtime_error(
                "pop_back() called on empty circular_buffer");
        }

        tail_ = (tail_ + capacity_ - 1) % capacity_;
        std::allocator_traits<Allocator>::destroy(alloc_, &buffer_[tail_]);
        --size_;
    }

    /**
     * @brief Access front element
     */
    reference front() {
        if (empty()) {
            throw std::runtime_error("front() called on empty circular_buffer");
        }
        return buffer_[head_];
    }

    /**
     * @brief Access front element (const)
     */
    const_reference front() const {
        if (empty()) {
            throw std::runtime_error("front() called on empty circular_buffer");
        }
        return buffer_[head_];
    }

    /**
     * @brief Access back element
     */
    reference back() {
        if (empty()) {
            throw std::runtime_error("back() called on empty circular_buffer");
        }
        return buffer_[(tail_ + capacity_ - 1) % capacity_];
    }

    /**
     * @brief Access back element (const)
     */
    const_reference back() const {
        if (empty()) {
            throw std::runtime_error("back() called on empty circular_buffer");
        }
        return buffer_[(tail_ + capacity_ - 1) % capacity_];
    }

    /**
     * @brief Access element by index
     *
     * @param index Index from front (0-based)
     */
    reference operator[](size_type index) {
        assert(index < size_);
        return buffer_[(head_ + index) % capacity_];
    }

    /**
     * @brief Access element by index (const)
     *
     * @param index Index from front (0-based)
     */
    const_reference operator[](size_type index) const {
        assert(index < size_);
        return buffer_[(head_ + index) % capacity_];
    }

    /**
     * @brief Access element by index with bounds checking
     *
     * @param index Index from front (0-based)
     */
    reference at(size_type index) {
        if (index >= size_) {
            throw std::out_of_range("circular_buffer::at");
        }
        return buffer_[(head_ + index) % capacity_];
    }

    /**
     * @brief Access element by index with bounds checking (const)
     *
     * @param index Index from front (0-based)
     */
    const_reference at(size_type index) const {
        if (index >= size_) {
            throw std::out_of_range("circular_buffer::at");
        }
        return buffer_[(head_ + index) % capacity_];
    }

    /**
     * @brief Get current size
     */
    size_type size() const noexcept { return size_; }

    /**
     * @brief Get capacity
     */
    size_type capacity() const noexcept { return capacity_; }

    /**
     * @brief Check if buffer is empty
     */
    bool empty() const noexcept { return size_ == 0; }

    /**
     * @brief Check if buffer is full
     */
    bool full() const noexcept { return size_ == capacity_; }

    /**
     * @brief Clear all elements
     */
    void clear() {
        while (!empty()) {
            pop_front();
        }
    }

    /**
     * @brief Reserve capacity
     *
     * @param new_capacity New minimum capacity
     */
    void reserve(size_type new_capacity) {
        if (new_capacity > capacity_) {
            resize_internal(new_capacity);
        }
    }

    /**
     * @brief Swap with another circular buffer
     */
    void swap(circular_buffer& other) noexcept {
        using std::swap;
        swap(alloc_, other.alloc_);
        swap(buffer_, other.buffer_);
        swap(capacity_, other.capacity_);
        swap(size_, other.size_);
        swap(head_, other.head_);
        swap(tail_, other.tail_);
        swap(auto_resize_, other.auto_resize_);
    }

private:
    void resize_internal(size_type new_capacity) {
        pointer new_buffer =
            std::allocator_traits<Allocator>::allocate(alloc_, new_capacity);

        // Copy elements to new buffer in linear order
        for (size_type i = 0; i < size_; ++i) {
            std::allocator_traits<Allocator>::construct(
                alloc_, &new_buffer[i],
                std::move(buffer_[(head_ + i) % capacity_]));
            std::allocator_traits<Allocator>::destroy(
                alloc_, &buffer_[(head_ + i) % capacity_]);
        }

        std::allocator_traits<Allocator>::deallocate(alloc_, buffer_,
                                                     capacity_);

        buffer_ = new_buffer;
        capacity_ = new_capacity;
        head_ = 0;
        tail_ = size_;
    }
};

/**
 * @brief High-performance deque with chunked storage
 *
 * Implements a deque using chunked storage for better cache performance
 * and reduced memory fragmentation compared to standard deque.
 *
 * @tparam T Element type
 * @tparam ChunkSize Elements per chunk
 * @tparam Allocator Allocator type
 */
template <typename T, std::size_t ChunkSize = 512,
          typename Allocator = std::allocator<T>>
class chunked_deque {
public:
    using value_type = T;
    using allocator_type = Allocator;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = typename std::allocator_traits<Allocator>::pointer;
    using const_pointer =
        typename std::allocator_traits<Allocator>::const_pointer;

private:
    static constexpr size_type chunk_size = ChunkSize;
    static_assert(chunk_size > 0, "Chunk size must be greater than 0");

    struct Chunk {
        alignas(T) char data[sizeof(T) * chunk_size];

        T* get_element(size_type index) {
            return reinterpret_cast<T*>(data + index * sizeof(T));
        }

        const T* get_element(size_type index) const {
            return reinterpret_cast<const T*>(data + index * sizeof(T));
        }
    };

    using chunk_allocator =
        typename std::allocator_traits<Allocator>::template rebind_alloc<Chunk>;
    using chunk_pointer =
        typename std::allocator_traits<chunk_allocator>::pointer;

    allocator_type alloc_;
    chunk_allocator chunk_alloc_;
    std::vector<chunk_pointer> chunks_;
    size_type first_chunk_;    // Index of first chunk
    size_type last_chunk_;     // Index of last chunk
    size_type first_element_;  // Index within first chunk
    size_type last_element_;   // Index within last chunk
    size_type size_;

public:
    /**
     * @brief Constructor
     */
    explicit chunked_deque(const allocator_type& alloc = allocator_type())
        : alloc_(alloc),
          chunk_alloc_(alloc),
          first_chunk_(0),
          last_chunk_(0),
          first_element_(chunk_size / 2),
          last_element_(chunk_size / 2),
          size_(0) {
        // Allocate initial chunk
        chunks_.push_back(
            std::allocator_traits<chunk_allocator>::allocate(chunk_alloc_, 1));
    }

    /**
     * @brief Destructor
     */
    ~chunked_deque() {
        clear();
        for (auto chunk : chunks_) {
            std::allocator_traits<chunk_allocator>::deallocate(chunk_alloc_,
                                                               chunk, 1);
        }
    }

    /**
     * @brief Add element to the back
     */
    void push_back(const T& value) {
        if (last_element_ == chunk_size) {
            add_chunk_back();
        }

        std::allocator_traits<Allocator>::construct(
            alloc_, chunks_[last_chunk_]->get_element(last_element_), value);
        ++last_element_;
        ++size_;
    }

    /**
     * @brief Add element to the back (move version)
     */
    void push_back(T&& value) {
        if (last_element_ == chunk_size) {
            add_chunk_back();
        }

        std::allocator_traits<Allocator>::construct(
            alloc_, chunks_[last_chunk_]->get_element(last_element_),
            std::move(value));
        ++last_element_;
        ++size_;
    }

    /**
     * @brief Add element to the front
     */
    void push_front(const T& value) {
        if (first_element_ == 0) {
            add_chunk_front();
        }

        --first_element_;
        std::allocator_traits<Allocator>::construct(
            alloc_, chunks_[first_chunk_]->get_element(first_element_), value);
        ++size_;
    }

    /**
     * @brief Add element to the front (move version)
     */
    void push_front(T&& value) {
        if (first_element_ == 0) {
            add_chunk_front();
        }

        --first_element_;
        std::allocator_traits<Allocator>::construct(
            alloc_, chunks_[first_chunk_]->get_element(first_element_),
            std::move(value));
        ++size_;
    }

    /**
     * @brief Remove element from the back
     */
    void pop_back() {
        if (empty()) {
            throw std::runtime_error(
                "pop_back() called on empty chunked_deque");
        }

        --last_element_;
        std::allocator_traits<Allocator>::destroy(
            alloc_, chunks_[last_chunk_]->get_element(last_element_));
        --size_;

        if (last_element_ == 0 && last_chunk_ > first_chunk_) {
            remove_chunk_back();
        }
    }

    /**
     * @brief Remove element from the front
     */
    void pop_front() {
        if (empty()) {
            throw std::runtime_error(
                "pop_front() called on empty chunked_deque");
        }

        std::allocator_traits<Allocator>::destroy(
            alloc_, chunks_[first_chunk_]->get_element(first_element_));
        ++first_element_;
        --size_;

        if (first_element_ == chunk_size && first_chunk_ < last_chunk_) {
            remove_chunk_front();
        }
    }

    /**
     * @brief Access element by index
     */
    reference operator[](size_type index) {
        auto [chunk_idx, element_idx] = get_position(index);
        return *chunks_[chunk_idx]->get_element(element_idx);
    }

    /**
     * @brief Access element by index (const)
     */
    const_reference operator[](size_type index) const {
        auto [chunk_idx, element_idx] = get_position(index);
        return *chunks_[chunk_idx]->get_element(element_idx);
    }

    /**
     * @brief Access front element
     */
    reference front() {
        if (empty()) {
            throw std::runtime_error("front() called on empty chunked_deque");
        }
        return *chunks_[first_chunk_]->get_element(first_element_);
    }

    /**
     * @brief Access front element (const)
     */
    const_reference front() const {
        if (empty()) {
            throw std::runtime_error("front() called on empty chunked_deque");
        }
        return *chunks_[first_chunk_]->get_element(first_element_);
    }

    /**
     * @brief Access back element
     */
    reference back() {
        if (empty()) {
            throw std::runtime_error("back() called on empty chunked_deque");
        }
        return *chunks_[last_chunk_]->get_element(last_element_ - 1);
    }

    /**
     * @brief Access back element (const)
     */
    const_reference back() const {
        if (empty()) {
            throw std::runtime_error("back() called on empty chunked_deque");
        }
        return *chunks_[last_chunk_]->get_element(last_element_ - 1);
    }

    /**
     * @brief Get current size
     */
    size_type size() const noexcept { return size_; }

    /**
     * @brief Check if deque is empty
     */
    bool empty() const noexcept { return size_ == 0; }

    /**
     * @brief Clear all elements
     */
    void clear() {
        while (!empty()) {
            pop_back();
        }
    }

private:
    std::pair<size_type, size_type> get_position(size_type index) const {
        assert(index < size_);

        size_type total_index = first_element_ + index;
        size_type chunk_offset = total_index / chunk_size;
        size_type element_offset = total_index % chunk_size;

        return {first_chunk_ + chunk_offset, element_offset};
    }

    void add_chunk_back() {
        if (last_chunk_ + 1 >= chunks_.size()) {
            chunks_.push_back(std::allocator_traits<chunk_allocator>::allocate(
                chunk_alloc_, 1));
        }
        ++last_chunk_;
        last_element_ = 0;
    }

    void add_chunk_front() {
        if (first_chunk_ == 0) {
            chunks_.insert(chunks_.begin(),
                           std::allocator_traits<chunk_allocator>::allocate(
                               chunk_alloc_, 1));
            ++last_chunk_;
        } else {
            --first_chunk_;
        }
        first_element_ = chunk_size;
    }

    void remove_chunk_back() {
        --last_chunk_;
        last_element_ = chunk_size;
    }

    void remove_chunk_front() {
        ++first_chunk_;
        first_element_ = 0;
    }
};

// Convenience aliases
template <typename T>
using CircularBuffer = circular_buffer<T>;

template <typename T>
using AutoResizeCircularBuffer = circular_buffer<T>;

template <typename T>
using ChunkedDeque = chunked_deque<T>;

}  // namespace containers
}  // namespace atom