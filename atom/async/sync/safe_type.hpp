#ifndef ATOM_ASYNC_SYNC_SAFE_TYPE_HPP
#define ATOM_ASYNC_SYNC_SAFE_TYPE_HPP

#include <mutex>
#include <shared_mutex>
#include <utility>

namespace atom::async::sync {

/**
 * @brief Thread-safe wrapper for any type T.
 *
 * SafeType provides thread-safe access to a value of type T using a
 * shared_mutex for reader-writer synchronization. Multiple readers can access
 * the value concurrently, but writers have exclusive access.
 *
 * @tparam T The type to wrap.
 */
template <typename T>
class SafeType {
public:
    SafeType() = default;

    explicit SafeType(const T& value) : value_(value) {}

    explicit SafeType(T&& value) : value_(std::move(value)) {}

    SafeType(const SafeType& other) {
        std::shared_lock lock(other.mutex_);
        value_ = other.value_;
    }

    SafeType(SafeType&& other) noexcept {
        std::unique_lock lock(other.mutex_);
        value_ = std::move(other.value_);
    }

    SafeType& operator=(const SafeType& other) {
        if (this != &other) {
            std::unique_lock lock1(mutex_, std::defer_lock);
            std::shared_lock lock2(other.mutex_, std::defer_lock);
            std::lock(lock1, lock2);
            value_ = other.value_;
        }
        return *this;
    }

    SafeType& operator=(SafeType&& other) noexcept {
        if (this != &other) {
            std::unique_lock lock1(mutex_, std::defer_lock);
            std::unique_lock lock2(other.mutex_, std::defer_lock);
            std::lock(lock1, lock2);
            value_ = std::move(other.value_);
        }
        return *this;
    }

    /**
     * @brief Get a copy of the current value.
     */
    T get() const {
        std::shared_lock lock(mutex_);
        return value_;
    }

    /**
     * @brief Set the value.
     */
    void set(const T& value) {
        std::unique_lock lock(mutex_);
        value_ = value;
    }

    /**
     * @brief Set the value (move).
     */
    void set(T&& value) {
        std::unique_lock lock(mutex_);
        value_ = std::move(value);
    }

    /**
     * @brief Modify the value using a function.
     * @tparam Func Function type
     * @param func Function to apply to the value
     * @return Result of the function (if any)
     */
    template <typename Func>
    auto modify(Func&& func) -> decltype(func(std::declval<T&>())) {
        std::unique_lock lock(mutex_);
        if constexpr (std::is_void_v<decltype(func(value_))>) {
            func(value_);
        } else {
            return func(value_);
        }
    }

    /**
     * @brief Read the value using a function (read-only access).
     * @tparam Func Function type
     * @param func Function to apply to the value
     * @return Result of the function (if any)
     */
    template <typename Func>
    auto read(Func&& func) const -> decltype(func(std::declval<const T&>())) {
        std::shared_lock lock(mutex_);
        if constexpr (std::is_void_v<decltype(func(value_))>) {
            func(value_);
        } else {
            return func(value_);
        }
    }

    /**
     * @brief Swap values with another SafeType.
     */
    void swap(SafeType& other) {
        if (this != &other) {
            std::unique_lock lock1(mutex_, std::defer_lock);
            std::unique_lock lock2(other.mutex_, std::defer_lock);
            std::lock(lock1, lock2);
            std::swap(value_, other.value_);
        }
    }

    /**
     * @brief Compare and swap operation.
     * @return true if swap occurred, false otherwise.
     */
    bool compareAndSwap(const T& expected, const T& desired) {
        std::unique_lock lock(mutex_);
        if (value_ == expected) {
            value_ = desired;
            return true;
        }
        return false;
    }

private:
    mutable std::shared_mutex mutex_;
    T value_{};
};

}  // namespace atom::async::sync

#endif  // ATOM_ASYNC_SYNC_SAFE_TYPE_HPP
