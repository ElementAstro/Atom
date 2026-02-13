#ifndef ATOM_ASYNC_SYNC_THREAD_SAFE_VECTOR_HPP
#define ATOM_ASYNC_SYNC_THREAD_SAFE_VECTOR_HPP

#include <atomic>
#include <memory>
#include <optional>
#include <ranges>
#include <shared_mutex>
#include <vector>

#include "atom/error/exception.hpp"

namespace atom::async {

/**
 * @brief Concept for thread-safe vector elements.
 */
template <typename T>
concept ThreadSafeVectorElem = std::is_nothrow_move_constructible_v<T> &&
                               std::is_nothrow_destructible_v<T>;

/**
 * @brief A thread-safe dynamic array implementation.
 *
 * Provides atomic operations for concurrent access with automatic resizing.
 *
 * @tparam T Type of elements stored in the vector.
 */
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

            for (size_t i = 0; i < size_.load(std::memory_order_relaxed); ++i) {
                newData[i].store(data_[i].load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
            }

            data_.swap(newData);
            capacity_.store(newCapacity, std::memory_order_release);
        } catch (const std::exception& e) {
            THROW_RUNTIME_ERROR("Failed to resize vector: " +
                                std::string(e.what()));
        }
    }

public:
    /**
     * @brief Construct with initial capacity.
     * @param initial_capacity Initial capacity (default: 16).
     */
    explicit ThreadSafeVector(size_t initial_capacity = 16)
        : capacity_(std::max(initial_capacity, size_t(1))), size_(0) {
        try {
            data_ = std::make_unique<std::atomic<T>[]>(capacity_.load());
        } catch (const std::bad_alloc& e) {
            THROW_RUNTIME_ERROR(
                "Failed to allocate memory for ThreadSafeVector");
        }
    }

    /**
     * @brief Construct from a range.
     */
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_value_t<R>, T>
    explicit ThreadSafeVector(R&& range, size_t initial_capacity = 16)
        : ThreadSafeVector(initial_capacity) {
        for (auto&& item : range) {
            pushBack(item);
        }
    }

    /**
     * @brief Push a value to the back.
     */
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

    /**
     * @brief Push a value to the back using move semantics.
     */
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
                } catch (const std::exception&) {
                    return;
                }
            }
            currentSize = size_.load(std::memory_order_relaxed);
        }
    }

    /**
     * @brief Pop a value from the back.
     */
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

    /**
     * @brief Access element at index with bounds checking.
     */
    auto at(size_t index) const -> T {
        if (index >= size_.load(std::memory_order_acquire)) {
            THROW_OUT_OF_RANGE("Index out of range in ThreadSafeVector::at()");
        }
        return data_[index].load(std::memory_order_acquire);
    }

    /**
     * @brief Try to access element at index.
     */
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

    /**
     * @brief Shrink capacity to fit current size.
     */
    void shrinkToFit() {
        std::unique_lock lock(resize_mutex_);

        size_t currentSize = size_.load(std::memory_order_relaxed);
        size_t currentCapacity = capacity_.load(std::memory_order_relaxed);

        if (currentSize == currentCapacity) {
            return;
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
        } catch (const std::exception&) {
            // Ignore errors during shrink
        }
    }

    /**
     * @brief Get the front element.
     */
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

    /**
     * @brief Get the back element.
     */
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

    /**
     * @brief Get a snapshot copy of all elements.
     * @return std::vector<T> A copy of all elements.
     */
    [[nodiscard]] auto snapshot() const -> std::vector<T> {
        std::shared_lock lock(resize_mutex_);
        size_t currentSize = size_.load(std::memory_order_acquire);
        std::vector<T> result;
        result.reserve(currentSize);

        for (size_t i = 0; i < currentSize; ++i) {
            result.push_back(data_[i].load(std::memory_order_acquire));
        }

        return result;
    }

    /**
     * @brief Apply a function to a read-only view of all elements.
     * @tparam Func Function type that accepts std::vector<T>
     * @param func Function to apply to the data
     * @return The result of func, if any
     */
    template <typename Func>
    auto withData(Func&& func) const -> decltype(func(std::declval<std::vector<T>>())) {
        std::shared_lock lock(resize_mutex_);
        size_t currentSize = size_.load(std::memory_order_acquire);
        std::vector<T> temp;
        temp.reserve(currentSize);

        for (size_t i = 0; i < currentSize; ++i) {
            temp.push_back(data_[i].load(std::memory_order_acquire));
        }

        return func(temp);
    }
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_SYNC_THREAD_SAFE_VECTOR_HPP
