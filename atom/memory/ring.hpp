#ifndef ATOM_ALGORITHM_RING_HPP
#define ATOM_ALGORITHM_RING_HPP

#include <algorithm>
#include <concepts>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <vector>

#ifdef ATOM_USE_BOOST
#include <boost/circular_buffer.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#endif

namespace atom::memory {
/**
 * @brief A thread-safe circular buffer implementation.
 *
 * @tparam T The type of elements stored in the buffer.
 */
template <typename T>
class RingBuffer {
public:
    /**
     * @brief Construct a new RingBuffer object.
     *
     * @param size The maximum size of the buffer.
     * @throw std::invalid_argument if size is zero.
     */
    explicit RingBuffer(size_t size) {
        if (size == 0) {
            throw std::invalid_argument(
                "RingBuffer size must be greater than zero.");
        }
#ifdef ATOM_USE_BOOST
        buffer_ = boost::circular_buffer<T>(size);
#else
        buffer_.reserve(size);
        buffer_.resize(size);
#endif
        max_size_ = size;
    }

    /**
     * @brief Push an item to the buffer.
     *
     * @param item The item to push.
     * @return true if the item was successfully pushed, false if the buffer was
     * full.
     * @throw std::runtime_error if pushing fails due to internal reasons.
     */
    auto push(const T& item) -> bool {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        if (buffer_.full()) {
            return false;
        }
        buffer_.push_back(item);
#else
        if (full()) {
            return false;
        }
        buffer_[head_] = item;
        head_ = (head_ + 1) % max_size_;
        ++count_;
#endif
        return true;
    }

    /**
     * @brief Push an item to the buffer, overwriting the oldest item if full.
     *
     * @param item The item to push.
     */
    void pushOverwrite(const T& item) {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        buffer_.push_back(item);
#else
        buffer_[head_] = item;
        if (full()) {
            tail_ = (tail_ + 1) % max_size_;
        } else {
            ++count_;
        }
        head_ = (head_ + 1) % max_size_;
#endif
    }

    /**
     * @brief Pop an item from the buffer.
     *
     * @return std::optional<T> The popped item, or std::nullopt if the buffer
     * was empty.
     */
    auto pop() -> std::optional<T> {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        if (buffer_.empty()) {
            return std::nullopt;
        }
        T item = buffer_.front();
        buffer_.pop_front();
        return item;
#else
        if (empty()) {
            return std::nullopt;
        }
        T item = buffer_[tail_];
        tail_ = (tail_ + 1) % max_size_;
        --count_;
        return item;
#endif
    }

    /**
     * @brief Check if the buffer is full.
     *
     * @return true if the buffer is full, false otherwise.
     */
    auto full() const -> bool {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        return buffer_.full();
#else
        return count_ == max_size_;
#endif
    }

    /**
     * @brief Check if the buffer is empty.
     *
     * @return true if the buffer is empty, false otherwise.
     */
    auto empty() const -> bool {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        return buffer_.empty();
#else
        return count_ == 0;
#endif
    }

    /**
     * @brief Get the current number of items in the buffer.
     *
     * @return size_t The number of items in the buffer.
     */
    auto size() const -> size_t {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        return buffer_.size();
#else
        return count_;
#endif
    }

    /**
     * @brief Get the maximum size of the buffer.
     *
     * @return size_t The maximum size of the buffer.
     */
    auto capacity() const -> size_t { return max_size_; }

    /**
     * @brief Clear all items from the buffer.
     */
    void clear() {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        buffer_.clear();
#else
        head_ = 0;
        tail_ = 0;
        count_ = 0;
#endif
    }

    /**
     * @brief Get the front item of the buffer without removing it.
     *
     * @return std::optional<T> The front item, or std::nullopt if the buffer is
     * empty.
     */
    auto front() const -> std::optional<T> {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        if (buffer_.empty()) {
            return std::nullopt;
        }
        return buffer_.front();
#else
        if (empty()) {
            return std::nullopt;
        }
        return buffer_[tail_];
#endif
    }

    /**
     * @brief Get the back item of the buffer without removing it.
     *
     * @return std::optional<T> The back item, or std::nullopt if the buffer is
     * empty.
     */
    auto back() const -> std::optional<T> {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        if (buffer_.empty()) {
            return std::nullopt;
        }
        return buffer_.back();
#else
        if (empty()) {
            return std::nullopt;
        }
        size_t backIndex = (head_ + max_size_ - 1) % max_size_;
        return buffer_[backIndex];
#endif
    }

    /**
     * @brief Check if the buffer contains a specific item.
     *
     * @param item The item to search for.
     * @return true if the item is in the buffer, false otherwise.
     */
    auto contains(const T& item) const -> bool {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        return std::find(buffer_.begin(), buffer_.end(), item) != buffer_.end();
#else
        for (size_t i = 0; i < count_; ++i) {
            size_t index = (tail_ + i) % max_size_;
            if (buffer_[index] == item) {
                return true;
            }
        }
        return false;
#endif
    }

    /**
     * @brief Get a view of the buffer's contents as a vector.
     *
     * @return std::vector<T> A vector containing the buffer's contents in
     * order.
     */
    std::vector<T> view() const {
        std::lock_guard lock(mutex_);
        std::vector<T> combined;
        combined.reserve(size());
#ifdef ATOM_USE_BOOST
        std::copy(buffer_.begin(), buffer_.end(), std::back_inserter(combined));
#else
        for (size_t i = 0; i < count_; ++i) {
            size_t index = (tail_ + i) % max_size_;
            combined.emplace_back(buffer_[index]);
        }
#endif
        return combined;
    }

    /**
     * @brief Iterator class for RingBuffer.
     */
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        Iterator(const RingBuffer<T>* buffer, size_t pos, size_t traversed)
            : buffer_(buffer), pos_(pos), traversed_(traversed) {}

        auto operator*() const -> reference { return buffer_->buffer_[pos_]; }

        auto operator->() const -> pointer { return &buffer_->buffer_[pos_]; }

        auto operator++() -> Iterator& {
            pos_ = (pos_ + 1) % buffer_->max_size_;
            ++traversed_;
            return *this;
        }

        auto operator++(int) const -> Iterator {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        friend auto operator==(const Iterator& a, const Iterator& b) -> bool {
            return a.traversed_ == b.traversed_;
        }

        friend auto operator!=(const Iterator& a, const Iterator& b) -> bool {
            return !(a == b);
        }

    private:
        const RingBuffer<T>* buffer_;
        size_t pos_;
        size_t traversed_;
    };

    /**
     * @brief Get an iterator to the beginning of the buffer.
     *
     * @return Iterator
     */
    auto begin() const -> Iterator {
        std::lock_guard lock(mutex_);
        return Iterator(this, tail_, 0);
    }

    /**
     * @brief Get an iterator to the end of the buffer.
     *
     * @return Iterator
     */
    auto end() const -> Iterator {
        std::lock_guard lock(mutex_);
        return Iterator(this, head_, count_);
    }

    /**
     * @brief Resize the buffer.
     *
     * @param new_size The new size of the buffer.
     * @throw std::runtime_error if new_size is less than the current number of
     * elements.
     */
    void resize(size_t new_size) {
        std::lock_guard lock(mutex_);
        if (new_size < size()) {
            throw std::runtime_error(
                "New size cannot be smaller than current number of elements.");
        }
#ifdef ATOM_USE_BOOST
        buffer_.set_capacity(new_size);
#else
        std::vector<T> newBuffer(new_size);
        for (size_t i = 0; i < count_; ++i) {
            size_t oldIndex = (tail_ + i) % max_size_;
            newBuffer[i] = std::move(buffer_[oldIndex]);
        }
        buffer_ = std::move(newBuffer);
        max_size_ = new_size;
        head_ = count_ % max_size_;
        tail_ = 0;
#endif
    }

    /**
     * @brief Get an element at a specific index in the buffer.
     *
     * @param index The index of the element to retrieve.
     * @return std::optional<T> The element at the specified index, or
     * std::nullopt if the index is out of bounds.
     */
    auto at(size_t index) const -> std::optional<T> {
        std::lock_guard lock(mutex_);
        if (index >= size()) {
            return std::nullopt;
        }
#ifdef ATOM_USE_BOOST
        return buffer_[index];
#else
        size_t actualIndex = (tail_ + index) % max_size_;
        return buffer_[actualIndex];
#endif
    }

    /**
     * @brief Apply a function to each element in the buffer.
     *
     * @param func The function to apply to each element.
     */
    template <std::invocable<T&> F>
    void forEach(F&& func) {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        for (auto& item : buffer_) {
            func(item);
        }
#else
        for (size_t i = 0; i < count_; ++i) {
            size_t index = (tail_ + i) % max_size_;
            func(buffer_[index]);
        }
#endif
    }

    /**
     * @brief Remove elements from the buffer that satisfy a predicate.
     *
     * @param pred The predicate function.
     */
    template <std::predicate<T> P>
    void removeIf(P&& pred) {
        std::lock_guard lock(mutex_);
#ifdef ATOM_USE_BOOST
        buffer_.erase(std::remove_if(buffer_.begin(), buffer_.end(), pred),
                      buffer_.end());
#else
        size_t write = tail_;
        size_t newCount = 0;

        for (size_t i = 0; i < count_; ++i) {
            size_t read = (tail_ + i) % max_size_;
            if (!pred(buffer_[read])) {
                if (write != read) {
                    buffer_[write] = std::move(buffer_[read]);
                }
                write = (write + 1) % max_size_;
                ++newCount;
            }
        }

        count_ = newCount;
        head_ = write;
#endif
    }

    /**
     * @brief Rotate the buffer by a specified number of positions.
     *
     * @param n The number of positions to rotate. Positive values rotate left,
     * negative values rotate right.
     */
    void rotate(int n) {
        std::lock_guard lock(mutex_);
        if (empty() || n == 0) {
            return;
        }

#ifdef ATOM_USE_BOOST
        buffer_.rotate(n);
#else
        size_t effectiveN = static_cast<size_t>(n) % count_;
        if (n < 0) {
            effectiveN = count_ - effectiveN;
        }

        tail_ = (tail_ + effectiveN) % max_size_;
        head_ = (head_ + effectiveN) % max_size_;
#endif
    }

private:
#ifdef ATOM_USE_BOOST
    using CircularBuffer = boost::circular_buffer<T>;
    CircularBuffer buffer_;
    using MutexType = boost::mutex;
#else
    using MutexType = std::mutex;
    std::vector<T> buffer_;
    size_t max_size_;
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
#endif

    mutable MutexType mutex_;
};

}  // namespace atom::memory

#endif  // ATOM_ALGORITHM_RING_HPP