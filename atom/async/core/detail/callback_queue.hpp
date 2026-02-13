/*
 * callback_queue.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-01-01

Description: Unified callback queue for async operations

**************************************************/

#ifndef ATOM_ASYNC_CORE_DETAIL_CALLBACK_QUEUE_HPP
#define ATOM_ASYNC_CORE_DETAIL_CALLBACK_QUEUE_HPP

#include <functional>
#include <memory>
#include <vector>

#ifdef ATOM_USE_BOOST_LOCKFREE
#include <thread>  // For std::this_thread::yield()
#endif

#ifdef ATOM_USE_BOOST_LOCKFREE
#include <boost/lockfree/queue.hpp>
#endif

namespace atom::async::detail {

/**
 * @brief Wrapper for callbacks in lock-free queue
 * @tparam Signature The callback signature (e.g., void(T) or void())
 */
template <typename Signature>
struct CallbackWrapper {
    std::function<Signature> callback;

    CallbackWrapper() = default;
    explicit CallbackWrapper(std::function<Signature> cb)
        : callback(std::move(cb)) {}
};

#ifdef ATOM_USE_BOOST_LOCKFREE

/**
 * @brief Lock-free callback container using Boost.Lockfree
 * @tparam Signature The callback signature
 */
template <typename Signature>
class LockfreeCallbackQueue {
public:
    using CallbackType = std::function<Signature>;
    using WrapperType = CallbackWrapper<Signature>;

    explicit LockfreeCallbackQueue(size_t capacity = 128) : queue_(capacity) {}

    ~LockfreeCallbackQueue() { clear(); }

    // Non-copyable, non-movable due to boost::lockfree::queue
    LockfreeCallbackQueue(const LockfreeCallbackQueue&) = delete;
    LockfreeCallbackQueue& operator=(const LockfreeCallbackQueue&) = delete;
    LockfreeCallbackQueue(LockfreeCallbackQueue&&) = delete;
    LockfreeCallbackQueue& operator=(LockfreeCallbackQueue&&) = delete;

    /**
     * @brief Add a callback to the queue
     * @param callback The callback to add
     */
    void add(CallbackType callback) {
        auto* wrapper = new WrapperType(std::move(callback));
        while (!queue_.push(wrapper)) {
            std::this_thread::yield();
        }
    }

    /**
     * @brief Execute all callbacks with the given value
     * @tparam Args The argument types to pass to callbacks
     * @param args The arguments to pass to each callback
     */
    template <typename... Args>
    void executeAll(Args&&... args) {
        WrapperType* wrapper = nullptr;
        while (queue_.pop(wrapper)) {
            if (wrapper && wrapper->callback) {
                try {
                    wrapper->callback(std::forward<Args>(args)...);
                } catch (...) {
                    // Log error but continue with other callbacks
                }
                delete wrapper;
            }
        }
    }

    /**
     * @brief Execute all callbacks (void version)
     */
    void executeAll() {
        WrapperType* wrapper = nullptr;
        while (queue_.pop(wrapper)) {
            if (wrapper && wrapper->callback) {
                try {
                    wrapper->callback();
                } catch (...) {
                    // Log error but continue with other callbacks
                }
                delete wrapper;
            }
        }
    }

    /**
     * @brief Check if the queue is empty
     * @return True if empty
     */
    [[nodiscard]] bool empty() const { return queue_.empty(); }

    /**
     * @brief Clear all pending callbacks
     */
    void clear() {
        WrapperType* wrapper = nullptr;
        while (queue_.pop(wrapper)) {
            delete wrapper;
        }
    }

private:
    boost::lockfree::queue<WrapperType*> queue_;
};

#endif  // ATOM_USE_BOOST_LOCKFREE

/**
 * @brief Standard callback container using std::vector
 * @tparam Signature The callback signature
 *
 * Note: This container is NOT thread-safe. External synchronization
 * is required if accessed from multiple threads.
 */
template <typename Signature>
class StandardCallbackQueue {
public:
    using CallbackType = std::function<Signature>;

    StandardCallbackQueue() = default;
    ~StandardCallbackQueue() = default;

    // Movable but not copyable
    StandardCallbackQueue(const StandardCallbackQueue&) = delete;
    StandardCallbackQueue& operator=(const StandardCallbackQueue&) = delete;
    StandardCallbackQueue(StandardCallbackQueue&&) noexcept = default;
    StandardCallbackQueue& operator=(StandardCallbackQueue&&) noexcept =
        default;

    /**
     * @brief Add a callback to the queue
     * @param callback The callback to add
     */
    void add(CallbackType callback) {
        callbacks_.emplace_back(std::move(callback));
    }

    /**
     * @brief Execute all callbacks with the given value
     * @tparam Args The argument types to pass to callbacks
     * @param args The arguments to pass to each callback
     */
    template <typename... Args>
    void executeAll(Args&&... args) {
        for (auto& callback : callbacks_) {
            if (callback) {
                try {
                    callback(std::forward<Args>(args)...);
                } catch (...) {
                    // Log error but continue with other callbacks
                }
            }
        }
        callbacks_.clear();
    }

    /**
     * @brief Execute all callbacks (void version)
     */
    void executeAll() {
        for (auto& callback : callbacks_) {
            if (callback) {
                try {
                    callback();
                } catch (...) {
                    // Log error but continue with other callbacks
                }
            }
        }
        callbacks_.clear();
    }

    /**
     * @brief Check if the queue is empty
     * @return True if empty
     */
    [[nodiscard]] bool empty() const { return callbacks_.empty(); }

    /**
     * @brief Clear all pending callbacks
     */
    void clear() { callbacks_.clear(); }

    /**
     * @brief Get the underlying vector (for iteration)
     * @return Reference to the callbacks vector
     */
    [[nodiscard]] std::vector<CallbackType>& callbacks() { return callbacks_; }

    /**
     * @brief Get the underlying vector (const)
     * @return Const reference to the callbacks vector
     */
    [[nodiscard]] const std::vector<CallbackType>& callbacks() const {
        return callbacks_;
    }

private:
    std::vector<CallbackType> callbacks_;
};

/**
 * @brief Type alias for the appropriate callback queue based on configuration
 * @tparam Signature The callback signature
 */
template <typename Signature>
using CallbackQueue =
#ifdef ATOM_USE_BOOST_LOCKFREE
    LockfreeCallbackQueue<Signature>;
#else
    StandardCallbackQueue<Signature>;
#endif

/**
 * @brief Shared callback container for use with shared_ptr
 * @tparam Signature The callback signature
 */
template <typename Signature>
class SharedCallbackQueue {
public:
    using CallbackType = std::function<Signature>;

    SharedCallbackQueue()
        :
#ifdef ATOM_USE_BOOST_LOCKFREE
          queue_(std::make_shared<LockfreeCallbackQueue<Signature>>())
#else
          callbacks_(std::make_shared<std::vector<CallbackType>>())
#endif
    {
    }

    /**
     * @brief Add a callback
     */
    void add(CallbackType callback) {
#ifdef ATOM_USE_BOOST_LOCKFREE
        queue_->add(std::move(callback));
#else
        callbacks_->emplace_back(std::move(callback));
#endif
    }

    /**
     * @brief Execute all callbacks with arguments
     */
    template <typename... Args>
    void executeAll(Args&&... args) {
#ifdef ATOM_USE_BOOST_LOCKFREE
        queue_->executeAll(std::forward<Args>(args)...);
#else
        for (auto& callback : *callbacks_) {
            if (callback) {
                try {
                    callback(std::forward<Args>(args)...);
                } catch (...) {
                    // Continue with other callbacks
                }
            }
        }
        callbacks_->clear();
#endif
    }

    /**
     * @brief Execute all callbacks (void version)
     */
    void executeAll() {
#ifdef ATOM_USE_BOOST_LOCKFREE
        queue_->executeAll();
#else
        for (auto& callback : *callbacks_) {
            if (callback) {
                try {
                    callback();
                } catch (...) {
                    // Continue with other callbacks
                }
            }
        }
        callbacks_->clear();
#endif
    }

    /**
     * @brief Check if empty
     */
    [[nodiscard]] bool empty() const {
#ifdef ATOM_USE_BOOST_LOCKFREE
        return queue_->empty();
#else
        return callbacks_->empty();
#endif
    }

    /**
     * @brief Clear all callbacks
     */
    void clear() {
#ifdef ATOM_USE_BOOST_LOCKFREE
        queue_->clear();
#else
        callbacks_->clear();
#endif
    }

#ifndef ATOM_USE_BOOST_LOCKFREE
    /**
     * @brief Get the shared callbacks vector
     */
    [[nodiscard]] std::shared_ptr<std::vector<CallbackType>> getCallbacks() {
        return callbacks_;
    }
#endif

private:
#ifdef ATOM_USE_BOOST_LOCKFREE
    std::shared_ptr<LockfreeCallbackQueue<Signature>> queue_;
#else
    std::shared_ptr<std::vector<CallbackType>> callbacks_;
#endif
};

}  // namespace atom::async::detail

#endif  // ATOM_ASYNC_CORE_DETAIL_CALLBACK_QUEUE_HPP
