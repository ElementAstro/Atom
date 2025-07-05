/*
 * message_queue.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_ASYNC_MESSAGE_QUEUE_HPP
#define ATOM_ASYNC_MESSAGE_QUEUE_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <coroutine>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// Add spdlog include
#include "spdlog/spdlog.h"

// Conditional Asio include
#ifdef ATOM_USE_ASIO
#include <asio.hpp>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#define ATOM_PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#define ATOM_PLATFORM_MACOS 1
#elif defined(__linux__)
#define ATOM_PLATFORM_LINUX 1
#endif

#if defined(__GNUC__) || defined(__clang__)
#define ATOM_LIKELY(x) __builtin_expect(!!(x), 1)
#define ATOM_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define ATOM_FORCE_INLINE __attribute__((always_inline)) inline
#define ATOM_NO_INLINE __attribute__((noinline))
#define ATOM_RESTRICT __restrict__
#elif defined(_MSC_VER)
#define ATOM_LIKELY(x) (x)
#define ATOM_UNLIKELY(x) (x)
#define ATOM_FORCE_INLINE __forceinline
#define ATOM_NO_INLINE __declspec(noinline)
#define ATOM_RESTRICT __restrict
#else
#define ATOM_LIKELY(x) (x)
#define ATOM_UNLIKELY(x) (x)
#define ATOM_FORCE_INLINE inline
#define ATOM_NO_INLINE
#define ATOM_RESTRICT
#endif

#ifndef ATOM_CACHE_LINE_SIZE
#if defined(ATOM_PLATFORM_WINDOWS)
#define ATOM_CACHE_LINE_SIZE 64
#elif defined(ATOM_PLATFORM_MACOS)
#define ATOM_CACHE_LINE_SIZE 128
#else
#define ATOM_CACHE_LINE_SIZE 64
#endif
#endif

#define ATOM_CACHELINE_ALIGN alignas(ATOM_CACHE_LINE_SIZE)

// Add boost lockfree support
#ifdef ATOM_USE_LOCKFREE_QUEUE
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#endif

namespace atom::async {

// Custom exception classes for message queue operations (messages in English)
class MessageQueueException : public std::runtime_error {
public:
    explicit MessageQueueException(
        const std::string& message,
        const std::source_location& location = std::source_location::current())
        : std::runtime_error(message + " at " + location.file_name() + ":" +
                             std::to_string(location.line()) + " in " +
                             location.function_name()) {
        // Example: spdlog::error("MessageQueueException: {} (at {}:{} in {})",
        // message, location.file_name(), location.line(),
        // location.function_name());
    }
};

class SubscriberException : public MessageQueueException {
public:
    explicit SubscriberException(
        const std::string& message,
        const std::source_location& location = std::source_location::current())
        : MessageQueueException(message, location) {}
};

class TimeoutException : public MessageQueueException {
public:
    explicit TimeoutException(
        const std::string& message,
        const std::source_location& location = std::source_location::current())
        : MessageQueueException(message, location) {}
};

// Concept to ensure message type has basic requirements - 增强版本
template <typename T>
concept MessageType =
    std::copy_constructible<T> && std::move_constructible<T> &&
    std::is_copy_assignable_v<T> && requires(T a) {
        {
            std::hash<std::remove_cvref_t<T>>{}(a)
        } -> std::convertible_to<std::size_t>;
    };

// 前向声明
template <MessageType T>
class MessageQueue;

// C++20 协程特性: 为消息队列提供协程接口
template <MessageType T>
class MessageAwaiter {
public:
    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) {
        m_handle = h;
        // 订阅消息，收到后恢复协程
        m_queue.subscribe(
            [this](const T& msg) {
                if (!m_cancelled) {
                    m_message = msg;
                    m_handle.resume();
                }
            },
            "coroutine_awaiter", m_priority, m_filter, m_timeout);
    }

    T await_resume() {
        m_cancelled = true;
        if (!m_message) {
            throw MessageQueueException(
                "No message received in coroutine awaiter");
        }
        return std::move(*m_message);
    }

    ~MessageAwaiter() { m_cancelled = true; }

private:
    MessageQueue<T>& m_queue;
    std::coroutine_handle<> m_handle;
    std::function<bool(const T&)> m_filter;
    std::optional<T> m_message;
    std::atomic<bool> m_cancelled{false};
    int m_priority{0};
    std::chrono::milliseconds m_timeout{std::chrono::milliseconds::zero()};

    friend class MessageQueue<T>;

    explicit MessageAwaiter(
        MessageQueue<T>& queue, std::function<bool(const T&)> filter = nullptr,
        int priority = 0,
        std::chrono::milliseconds timeout = std::chrono::milliseconds::zero())
        : m_queue(queue),
          m_filter(std::move(filter)),
          m_priority(priority),
          m_timeout(timeout) {}
};

/**
 * @brief A message queue that allows subscribers to receive messages of type T.
 *
 * @tparam T The type of messages that can be published and subscribed to.
 */
template <MessageType T>
class MessageQueue {
public:
    using CallbackType = std::function<void(const T&)>;
    using FilterType = std::function<bool(const T&)>;

    /**
     * @brief Constructs a MessageQueue.
     * @param ioContext The Asio io_context to use for asynchronous operations
     * (if ATOM_USE_ASIO is defined).
     * @param capacity Initial capacity for lockfree queue (used only if
     * ATOM_USE_LOCKFREE_QUEUE is defined)
     */
#ifdef ATOM_USE_ASIO
    explicit MessageQueue(asio::io_context& ioContext,
                          [[maybe_unused]] size_t capacity = 1024) noexcept
        : ioContext_(ioContext)
#else
    explicit MessageQueue([[maybe_unused]] size_t capacity = 1024) noexcept
#endif
#ifdef ATOM_USE_LOCKFREE_QUEUE
#ifdef ATOM_USE_SPSC_QUEUE
          ,
          m_lockfreeQueue_(capacity)
#else
          ,
          m_lockfreeQueue_(capacity)
#endif
#endif  // ATOM_USE_LOCKFREE_QUEUE
    {
        // Pre-allocate memory to reduce runtime allocations
        m_subscribers_.reserve(16);
        spdlog::debug("MessageQueue initialized.");
    }

    // Rule of five implementation
    ~MessageQueue() noexcept {
        spdlog::debug("MessageQueue destructor called.");
        stopProcessing();
    }

    MessageQueue(const MessageQueue&) = delete;
    MessageQueue& operator=(const MessageQueue&) = delete;
    MessageQueue(MessageQueue&&) noexcept = default;
    MessageQueue& operator=(MessageQueue&&) noexcept = default;

    /**
     * @brief Subscribe to messages with a callback and optional filter and
     * timeout.
     *
     * @param callback The callback function to be called when a new message is
     * received.
     * @param subscriberName The name of the subscriber.
     * @param priority The priority of the subscriber. Higher priority receives
     * messages first.
     * @param filter An optional filter to only receive messages that match the
     * criteria.
     * @param timeout The maximum time allowed for the subscriber to process a
     * message.
     * @throws SubscriberException if the callback is empty or name is empty
     */
    void subscribe(
        CallbackType callback, std::string_view subscriberName,
        int priority = 0, FilterType filter = nullptr,
        std::chrono::milliseconds timeout = std::chrono::milliseconds::zero()) {
        if (!callback) {
            throw SubscriberException("Callback function cannot be empty");
        }
        if (subscriberName.empty()) {
            throw SubscriberException("Subscriber name cannot be empty");
        }

        std::lock_guard lock(m_mutex_);
        m_subscribers_.emplace_back(std::string(subscriberName),
                                    std::move(callback), priority,
                                    std::move(filter), timeout);
        sortSubscribers();
        spdlog::debug("Subscriber '{}' added with priority {}.",
                      std::string(subscriberName), priority);
    }

    /**
     * @brief Unsubscribe from messages using the given callback.
     *
     * @param callback The callback function used during subscription.
     * @return true if subscriber was found and removed, false otherwise
     */
    [[nodiscard]] bool unsubscribe(const CallbackType& callback) noexcept {
        std::lock_guard lock(m_mutex_);
        const auto initialSize = m_subscribers_.size();
        auto it = std::remove_if(m_subscribers_.begin(), m_subscribers_.end(),
                                 [&callback](const auto& subscriber) {
                                     return subscriber.callback.target_type() ==
                                            callback.target_type();
                                 });
        bool removed = it != m_subscribers_.end();
        m_subscribers_.erase(it, m_subscribers_.end());
        if (removed) {
            spdlog::debug("Subscriber unsubscribed.");
        } else {
            spdlog::warn("Attempted to unsubscribe a non-existent subscriber.");
        }
        return removed;
    }

#ifdef ATOM_USE_LOCKFREE_QUEUE
    /**
     * @brief Publish a message to the queue, with an optional priority.
     * Lockfree version.
     *
     * @param message The message to publish.
     * @param priority The priority of the message, higher priority messages are
     * handled first.
     */
    void publish(const T& message, int priority = 0) {
        Message msg(message, priority);
        bool pushed = false;
        for (int retry = 0; retry < 3 && !pushed; ++retry) {
            pushed = m_lockfreeQueue_.push(msg);
            if (!pushed) {
                std::this_thread::yield();
            }
        }

        if (!pushed) {
            spdlog::warn(
                "Lockfree queue push failed after retries, falling back to "
                "standard deque.");
            std::lock_guard lock(m_mutex_);
            m_messages_.emplace_back(std::move(msg));
        }

        m_condition_.notify_one();
#ifdef ATOM_USE_ASIO
        ioContext_.post([this]() { processMessages(); });
#endif
    }

    /**
     * @brief Publish a message to the queue using move semantics.
     * Lockfree version.
     *
     * @param message The message to publish (will be moved).
     * @param priority The priority of the message.
     */
    void publish(T&& message, int priority = 0) {
        Message msg(std::move(message), priority);
        bool pushed = false;
        for (int retry = 0; retry < 3 && !pushed; ++retry) {
            pushed =
                m_lockfreeQueue_.push(std::move(msg));  // Assuming push(T&&)
            if (!pushed) {
                std::this_thread::yield();
            }
        }

        if (!pushed) {
            spdlog::warn(
                "Lockfree queue move-push failed after retries, falling back "
                "to standard deque.");
            std::lock_guard lock(m_mutex_);
            m_messages_.emplace_back(
                std::move(msg));  // msg was already constructed with move,
                                  // re-move if needed
        }

        m_condition_.notify_one();
#ifdef ATOM_USE_ASIO
        ioContext_.post([this]() { processMessages(); });
#endif
    }

#else  // NOT ATOM_USE_LOCKFREE_QUEUE
    /**
     * @brief Publish a message to the queue, with an optional priority.
     *
     * @param message The message to publish.
     * @param priority The priority of the message, higher priority messages are
     * handled first.
     */
    void publish(const T& message, int priority = 0) {
        {
            std::lock_guard lock(m_mutex_);
            m_messages_.emplace_back(message, priority);
        }
        m_condition_.notify_one();
#ifdef ATOM_USE_ASIO
        ioContext_.post([this]() { processMessages(); });
#endif
    }

    /**
     * @brief Publish a message to the queue using move semantics.
     *
     * @param message The message to publish (will be moved).
     * @param priority The priority of the message.
     */
    void publish(T&& message, int priority = 0) {
        {
            std::lock_guard lock(m_mutex_);
            m_messages_.emplace_back(std::move(message), priority);
        }
        m_condition_.notify_one();
#ifdef ATOM_USE_ASIO
        ioContext_.post([this]() { processMessages(); });
#endif
    }
#endif  // ATOM_USE_LOCKFREE_QUEUE

    /**
     * @brief Start processing messages in the queue.
     */
    void startProcessing() {
        if (m_isRunning_.exchange(true)) {
            spdlog::info("Message processing is already running.");
            return;
        }
        spdlog::info("Starting message processing...");

        m_processingThread_ =
            std::make_unique<std::jthread>([this](std::stop_token stoken) {
                m_isProcessing_.store(true);

#ifndef ATOM_USE_ASIO  // This whole loop is for non-Asio path
                spdlog::debug("MessageQueue jthread started (non-Asio mode).");
                auto process_message_content =
                    [&](const T& data, const std::string& source_q_name) {
                        spdlog::trace(
                            "jthread: Processing message from {} queue.",
                            source_q_name);
                        std::vector<Subscriber> subscribersCopy;
                        {
                            std::lock_guard<std::mutex> slock(m_mutex_);
                            subscribersCopy = m_subscribers_;
                        }

                        for (const auto& subscriber : subscribersCopy) {
                            try {
                                if (applyFilter(subscriber, data)) {
                                    (void)handleTimeout(subscriber, data);
                                }
                            } catch (const TimeoutException& e) {
                                spdlog::warn(
                                    "jthread: Timeout in subscriber '{}': {}",
                                    subscriber.name, e.what());
                            } catch (const std::exception& e) {
                                spdlog::error(
                                    "jthread: Exception in subscriber '{}': {}",
                                    subscriber.name, e.what());
                            }
                        }
                    };

                while (!stoken.stop_requested()) {
                    bool processedThisCycle = false;
                    Message currentMessage;

#ifdef ATOM_USE_LOCKFREE_QUEUE
                    // 1. Try to get from lockfree queue (non-blocking)
                    if (m_lockfreeQueue_.pop(currentMessage)) {
                        process_message_content(currentMessage.data,
                                                "lockfree_q_direct");
                        processedThisCycle = true;
                    }
#endif  // ATOM_USE_LOCKFREE_QUEUE

                    // 2. If nothing from lockfree (or lockfree not used), check
                    // m_messages_
                    if (!processedThisCycle) {
                        std::unique_lock lock(m_mutex_);
                        m_condition_.wait(lock, [&]() {
                            if (stoken.stop_requested())
                                return true;
                            bool has_deque_msg = !m_messages_.empty();
#ifdef ATOM_USE_LOCKFREE_QUEUE
                            return has_deque_msg || !m_lockfreeQueue_.empty();
#else
                        return has_deque_msg;
#endif
                        });

                        if (stoken.stop_requested())
                            break;

                    // After wait, re-check queues. Lock is held.
#ifdef ATOM_USE_LOCKFREE_QUEUE
                        if (m_lockfreeQueue_.pop(
                                currentMessage)) {  // Pop while lock is held
                                                    // (pop is thread-safe)
                            lock.unlock();          // Unlock BEFORE processing
                            process_message_content(currentMessage.data,
                                                    "lockfree_q_after_wait");
                            processedThisCycle = true;
                        } else if (!m_messages_
                                        .empty()) {  // Check deque if lockfree
                                                     // was empty
                            std::sort(m_messages_.begin(), m_messages_.end());
                            currentMessage = std::move(m_messages_.front());
                            m_messages_.pop_front();
                            lock.unlock();  // Unlock BEFORE processing
                            process_message_content(currentMessage.data,
                                                    "deque_q_after_wait");
                            processedThisCycle = true;
                        } else {
                            lock.unlock();  // Nothing found after wait
                        }
#else   // NOT ATOM_USE_LOCKFREE_QUEUE (Only m_messages_ queue)
                        if (!m_messages_.empty()) {  // Lock is held
                            std::sort(m_messages_.begin(), m_messages_.end());
                            currentMessage = std::move(m_messages_.front());
                            m_messages_.pop_front();
                            lock.unlock();  // Unlock BEFORE processing
                            process_message_content(currentMessage.data,
                                                    "deque_q_after_wait");
                            processedThisCycle = true;
                        } else {
                            lock.unlock();  // Nothing found after wait
                        }
#endif  // ATOM_USE_LOCKFREE_QUEUE (inside wait block)
                    }  // end if !processedThisCycle (from initial direct
                       // lockfree check)

                    if (!processedThisCycle && !stoken.stop_requested()) {
                        std::this_thread::yield();  // Avoid busy spin on
                                                    // spurious wakeup
                    }
                }  // end while (!stoken.stop_requested())
                spdlog::debug("MessageQueue jthread stopping (non-Asio mode).");
#else   // ATOM_USE_ASIO is defined
        // If Asio is used, this jthread is idle and just waits for stop.
        // Asio's processMessages will handle message processing.
                spdlog::debug(
                    "MessageQueue jthread started (Asio mode - idle).");
                std::unique_lock lock(m_mutex_);
                m_condition_.wait(
                    lock, [&stoken]() { return stoken.stop_requested(); });
                spdlog::debug(
                    "MessageQueue jthread stopping (Asio mode - idle).");
#endif  // ATOM_USE_ASIO (for jthread loop)
                m_isProcessing_.store(false);
            });

#ifdef ATOM_USE_ASIO
        if (!ioContext_.stopped()) {
            ioContext_.restart();  // Ensure io_context is running
            ioContext_.poll();     // Process any initial handlers
        }
#endif
    }

    /**
     * @brief Stop processing messages in the queue.
     */
    void stopProcessing() noexcept {
        if (!m_isRunning_.exchange(false)) {
            // spdlog::info("Message processing is already stopped or was not
            // running.");
            return;
        }
        spdlog::info("Stopping message processing...");

        if (m_processingThread_) {
            m_processingThread_->request_stop();
            m_condition_.notify_all();  // Wake up jthread if it's waiting
            try {
                if (m_processingThread_->joinable()) {
                    m_processingThread_->join();
                }
            } catch (const std::system_error& e) {
                spdlog::error("Exception joining processing thread: {}",
                              e.what());
            }
            m_processingThread_.reset();
        }
        spdlog::debug("Processing thread stopped.");

#ifdef ATOM_USE_ASIO
        if (!ioContext_.stopped()) {
            try {
                ioContext_.stop();
                spdlog::debug("Asio io_context stopped.");
            } catch (const std::exception& e) {
                spdlog::error("Exception while stopping io_context: {}",
                              e.what());
            } catch (...) {
                spdlog::error("Unknown exception while stopping io_context.");
            }
        }
#endif
    }

    /**
     * @brief Get the number of messages currently in the queue.
     * @return The number of messages in the queue.
     */
#ifdef ATOM_USE_LOCKFREE_QUEUE
    [[nodiscard]] size_t getMessageCount() const noexcept {
        size_t lockfreeCount = 0;
        // boost::lockfree::queue doesn't have a reliable size().
        // It has `empty()`. We can't get an exact count easily without
        // consuming. The original code returned 1 if not empty, which is
        // misleading. For now, let's report 0 or 1 for lockfree part as an
        // estimate.
        if (!m_lockfreeQueue_.empty()) {
            lockfreeCount = 1;  // Approximate: at least one
        }
        std::lock_guard lock(m_mutex_);
        return lockfreeCount +
               m_messages_.size();  // This is still an approximation
    }
#else
    [[nodiscard]] size_t getMessageCount() const noexcept;
#endif

    /**
     * @brief Get the number of subscribers currently subscribed to the queue.
     * @return The number of subscribers.
     */
    [[nodiscard]] size_t getSubscriberCount() const noexcept;

#ifdef ATOM_USE_LOCKFREE_QUEUE
    /**
     * @brief Resize the lockfree queue capacity
     * @param newCapacity New capacity for the queue
     * @return True if the operation was successful
     *
     * Note: This operation may temporarily block the queue
     */
    bool resizeQueue(size_t newCapacity) noexcept {
#if defined(ATOM_USE_LOCKFREE_QUEUE) && !defined(ATOM_USE_SPSC_QUEUE)
        try {
            // boost::lockfree::queue does not have a reserve or resize method
            // after construction. The capacity is fixed at construction or uses
            // node-based allocation. The original
            // `m_lockfreeQueue_.reserve(newCapacity)` is incorrect for
            // boost::lockfree::queue. For spsc_queue, capacity is also fixed.
            spdlog::warn(
                "Resizing boost::lockfree::queue capacity at runtime is not "
                "supported.");
            return false;
        } catch (const std::exception& e) {
            spdlog::error("Exception during (unsupported) queue resize: {}",
                          e.what());
            return false;
        }
#else
        spdlog::warn(
            "Queue resize not supported for SPSC queue or if lockfree queue is "
            "not used.");
        return false;
#endif
    }

    /**
     * @brief Get the capacity of the lockfree queue
     * @return Current capacity of the lockfree queue
     */
    [[nodiscard]] size_t getQueueCapacity() const noexcept {
// boost::lockfree::queue (node-based) doesn't have a fixed capacity to query
// easily. spsc_queue has fixed capacity.
#if defined(ATOM_USE_LOCKFREE_QUEUE) && defined(ATOM_USE_SPSC_QUEUE)
        // For spsc_queue, if it stores capacity, return it. Otherwise, this is
        // hard. The constructor takes capacity, but it's not directly queryable
        // from the object. Let's assume it's not easily available.
        return 0;  // Placeholder, as boost::lockfree queues don't typically
                   // expose this easily.
#elif defined(ATOM_USE_LOCKFREE_QUEUE)
        return 0;  // Placeholder for boost::lockfree::queue (MPMC)
#else
        return 0;
#endif
    }
#endif

    /**
     * @brief Cancel specific messages that meet a given condition.
     *
     * @param cancelCondition The condition to cancel certain messages.
     * @return The number of messages that were canceled.
     */
    [[nodiscard]] size_t cancelMessages(
        std::function<bool(const T&)> cancelCondition) noexcept;

    /**
     * @brief Clear all pending messages in the queue.
     *
     * @return The number of messages that were cleared.
     */
#ifdef ATOM_USE_LOCKFREE_QUEUE
    [[nodiscard]] size_t clearAllMessages() noexcept {
        size_t count = 0;
        Message msg;
        while (m_lockfreeQueue_.pop(msg)) {
            count++;
        }
        {
            std::lock_guard lock(m_mutex_);
            count += m_messages_.size();
            m_messages_.clear();
        }
        spdlog::info("Cleared {} messages from the queue.", count);
        return count;
    }
#else
    [[nodiscard]] size_t clearAllMessages() noexcept;
#endif

    /**
     * @brief Coroutine support for async message subscription
     */
    struct MessageAwaitable {
        MessageQueue<T>& queue;
        FilterType filter;
        std::optional<T> result;
        std::shared_ptr<bool> cancelled = std::make_shared<bool>(false);

        explicit MessageAwaitable(MessageQueue<T>& q, FilterType f = nullptr)
            : queue(q), filter(std::move(f)) {}

        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) {
            queue.subscribe(
                [this, h](const T& message) {
                    if (!*cancelled) {
                        result = message;
                        h.resume();
                    }
                },
                "coroutine_subscriber", 0,
                [this, f = filter](const T& msg) { return !f || f(msg); });
        }

        T await_resume() {
            *cancelled =
                true;  // Mark as done to prevent callback from resuming again
            if (!result.has_value()) {
                throw MessageQueueException("No message received by awaitable");
            }
            return std::move(*result);
        }
        // Ensure cancellation on destruction if coroutine is destroyed early
        ~MessageAwaitable() { *cancelled = true; }
    };

    /**
     * @brief Create an awaitable for use in coroutines
     *
     * @param filter Optional filter to apply
     * @return MessageAwaitable An awaitable object for coroutines
     */
    [[nodiscard]] MessageAwaitable nextMessage(FilterType filter = nullptr) {
        return MessageAwaitable(*this, std::move(filter));
    }

private:
    struct Subscriber {
        std::string name;
        CallbackType callback;
        int priority;
        FilterType filter;
        std::chrono::milliseconds timeout;

        Subscriber(std::string name, CallbackType callback, int priority,
                   FilterType filter, std::chrono::milliseconds timeout)
            : name(std::move(name)),
              callback(std::move(callback)),
              priority(priority),
              filter(std::move(filter)),
              timeout(timeout) {}

        bool operator<(const Subscriber& other) const noexcept {
            return priority > other.priority;  // Higher priority comes first
        }
    };

    struct Message {
        T data;
        int priority;
        std::chrono::steady_clock::time_point timestamp;

        Message() = default;

        Message(T data_val, int prio)
            : data(std::move(data_val)),
              priority(prio),
              timestamp(std::chrono::steady_clock::now()) {}

        // Ensure Message is copyable and movable if T is, for queue operations
        Message(const Message&) = default;
        Message(Message&&) noexcept = default;
        Message& operator=(const Message&) = default;
        Message& operator=(Message&&) noexcept = default;

        bool operator<(const Message& other) const noexcept {
            return priority != other.priority ? priority > other.priority
                                              : timestamp < other.timestamp;
        }
    };

    std::deque<Message> m_messages_;
    std::vector<Subscriber> m_subscribers_;
    mutable std::mutex m_mutex_;  // Protects m_messages_ and m_subscribers_
    std::condition_variable m_condition_;
    std::atomic<bool> m_isRunning_{false};
    std::atomic<bool> m_isProcessing_{
        false};  // Guard for Asio-driven processMessages

#ifdef ATOM_USE_ASIO
    asio::io_context& ioContext_;
#endif
    std::unique_ptr<std::jthread> m_processingThread_;

#ifdef ATOM_USE_LOCKFREE_QUEUE
#ifdef ATOM_USE_SPSC_QUEUE
    boost::lockfree::spsc_queue<Message> m_lockfreeQueue_;
#else
    boost::lockfree::queue<Message> m_lockfreeQueue_;
#endif
#endif  // ATOM_USE_LOCKFREE_QUEUE

#if defined(ATOM_USE_ASIO)  // processMessages methods are only for Asio path
#ifdef ATOM_USE_LOCKFREE_QUEUE
    /**
     * @brief Process messages in the queue. Asio, Lockfree version.
     */
    void processMessages() {
        if (!m_isRunning_.load(std::memory_order_relaxed))
            return;

        bool expected_processing = false;
        if (!m_isProcessing_.compare_exchange_strong(
                expected_processing, true, std::memory_order_acq_rel)) {
            return;
        }

        struct ProcessingGuard {
            std::atomic<bool>& flag;
            ProcessingGuard(std::atomic<bool>& f) : flag(f) {}
            ~ProcessingGuard() { flag.store(false, std::memory_order_release); }
        } guard(m_isProcessing_);

        spdlog::trace("Asio: processMessages (lockfree) started.");
        Message message;
        bool messageProcessedThisCall = false;

        if (m_lockfreeQueue_.pop(message)) {
            spdlog::trace("Asio: Popped message from lockfree queue.");
            messageProcessedThisCall = true;
            std::vector<Subscriber> subscribersCopy;
            {
                std::lock_guard lock(m_mutex_);
                subscribersCopy = m_subscribers_;
            }
            for (const auto& subscriber : subscribersCopy) {
                try {
                    if (applyFilter(subscriber, message.data)) {
                        (void)handleTimeout(subscriber, message.data);
                    }
                } catch (const TimeoutException& e) {
                    spdlog::warn("Asio: Timeout in subscriber '{}': {}",
                                 subscriber.name, e.what());
                } catch (const std::exception& e) {
                    spdlog::error("Asio: Exception in subscriber '{}': {}",
                                  subscriber.name, e.what());
                }
            }
        }

        if (!messageProcessedThisCall) {
            std::unique_lock lock(m_mutex_);
            if (!m_messages_.empty()) {
                std::sort(m_messages_.begin(), m_messages_.end());
                message = std::move(m_messages_.front());
                m_messages_.pop_front();
                spdlog::trace("Asio: Popped message from deque.");
                messageProcessedThisCall = true;

                std::vector<Subscriber> subscribersCopy = m_subscribers_;
                lock.unlock();

                for (const auto& subscriber : subscribersCopy) {
                    try {
                        if (applyFilter(subscriber, message.data)) {
                            (void)handleTimeout(subscriber, message.data);
                        }
                    } catch (const TimeoutException& e) {
                        spdlog::warn("Asio: Timeout in subscriber '{}': {}",
                                     subscriber.name, e.what());
                    } catch (const std::exception& e) {
                        spdlog::error("Asio: Exception in subscriber '{}': {}",
                                      subscriber.name, e.what());
                    }
                }
            } else {
                // lock.unlock(); // Not needed, unique_lock destructor handles
                // it
            }
        }

        if (messageProcessedThisCall) {
            spdlog::trace(
                "Asio: Message processed, re-posting processMessages.");
            ioContext_.post([this]() { processMessages(); });
        } else {
            spdlog::trace("Asio: No message processed in this call.");
        }
    }
#else   // NOT ATOM_USE_LOCKFREE_QUEUE (Asio, non-lockfree path)
    /**
     * @brief Process messages in the queue. Asio, Non-lockfree version.
     */
    void processMessages() {
        if (!m_isRunning_.load(std::memory_order_relaxed))
            return;
        spdlog::trace("Asio: processMessages (non-lockfree) started.");

        std::unique_lock lock(m_mutex_);
        if (m_messages_.empty()) {
            spdlog::trace("Asio: No messages in deque.");
            return;
        }

        std::sort(m_messages_.begin(), m_messages_.end());
        auto message = std::move(m_messages_.front());
        m_messages_.pop_front();
        spdlog::trace("Asio: Popped message from deque.");

        std::vector<Subscriber> subscribersCopy = m_subscribers_;
        lock.unlock();

        for (const auto& subscriber : subscribersCopy) {
            try {
                if (applyFilter(subscriber, message.data)) {
                    (void)handleTimeout(subscriber, message.data);
                }
            } catch (const TimeoutException& e) {
                spdlog::warn("Asio: Timeout in subscriber '{}': {}",
                             subscriber.name, e.what());
            } catch (const std::exception& e) {
                spdlog::error("Asio: Exception in subscriber '{}': {}",
                              subscriber.name, e.what());
            }
        }

        std::unique_lock check_lock(m_mutex_);
        bool more_messages = !m_messages_.empty();
        check_lock.unlock();

        if (more_messages) {
            spdlog::trace(
                "Asio: More messages in deque, re-posting processMessages.");
            ioContext_.post([this]() { processMessages(); });
        } else {
            spdlog::trace("Asio: No more messages in deque for now.");
        }
    }
#endif  // ATOM_USE_LOCKFREE_QUEUE (for Asio processMessages)
#endif  // ATOM_USE_ASIO (for processMessages methods)

    /**
     * @brief Apply the filter to a message for a given subscriber.
     * @param subscriber The subscriber to apply the filter for.
     * @param message The message to filter.
     * @return True if the message passes the filter, false otherwise.
     */
    [[nodiscard]] bool applyFilter(const Subscriber& subscriber,
                                   const T& message) const noexcept {
        if (!subscriber.filter) {
            return true;
        }
        try {
            return subscriber.filter(message);
        } catch (const std::exception& e) {
            spdlog::error("Exception in filter for subscriber '{}': {}",
                          subscriber.name, e.what());
            return false;  // Skip subscriber if filter throws
        } catch (...) {
            spdlog::error("Unknown exception in filter for subscriber '{}'",
                          subscriber.name);
            return false;
        }
    }

    /**
     * @brief Handle the timeout for a given subscriber and message.
     * @param subscriber The subscriber to handle the timeout for.
     * @param message The message to process.
     * @return True if the message was processed within the timeout, false
     * otherwise.
     */
    [[nodiscard]] bool handleTimeout(const Subscriber& subscriber,
                                     const T& message) const {
        if (subscriber.timeout == std::chrono::milliseconds::zero()) {
            try {
                subscriber.callback(message);
                return true;
            } catch (const std::exception& e) {
                // Logged by caller (processMessages or jthread loop)
                throw;  // Propagate to be caught and logged by caller
            }
        }

#ifdef ATOM_USE_ASIO
        std::promise<void> promise;
        auto future = promise.get_future();
        // Capture necessary parts by value for the task
        auto task = [cb = subscriber.callback, &message, p = std::move(promise),
                     sub_name = subscriber.name]() mutable {
            try {
                cb(message);
                p.set_value();
            } catch (...) {
                try {
                    // Log inside task for immediate context, or let caller log
                    // TimeoutException spdlog::warn("Asio task: Exception in
                    // callback for subscriber '{}'", sub_name);
                    p.set_exception(std::current_exception());
                } catch (...) { /* std::promise::set_exception can throw */
                    spdlog::error(
                        "Asio task: Failed to set exception for subscriber "
                        "'{}'",
                        sub_name);
                }
            }
        };
        asio::post(ioContext_, std::move(task));

        auto status = future.wait_for(subscriber.timeout);
        if (status == std::future_status::timeout) {
            throw TimeoutException("Subscriber " + subscriber.name +
                                   " timed out (Asio path)");
        }
        future.get();  // Re-throw exceptions from callback
        return true;
#else  // NOT ATOM_USE_ASIO
        std::future<void> future = std::async(
            std::launch::async,
            [cb = subscriber.callback, &message, name = subscriber.name]() {
                try {
                    cb(message);
                } catch (const std::exception& e_async) {
                    // Logged by caller (processMessages or jthread loop)
                    throw;
                } catch (...) {
                    // Logged by caller
                    throw;
                }
            });
        auto status = future.wait_for(subscriber.timeout);
        if (status == std::future_status::timeout) {
            throw TimeoutException("Subscriber " + subscriber.name +
                                   " timed out (non-Asio path)");
        }
        future.get();  // Propagate exceptions from callback
        return true;
#endif
    }

    /**
     * @brief Sort subscribers by priority
     */
    void sortSubscribers() noexcept {
        // Assumes m_mutex_ is held by caller if modification occurs
        std::sort(m_subscribers_.begin(), m_subscribers_.end());
    }
};

#ifndef ATOM_USE_LOCKFREE_QUEUE
template <MessageType T>
size_t MessageQueue<T>::getMessageCount() const noexcept {
    std::lock_guard lock(m_mutex_);
    return m_messages_.size();
}
#endif

template <MessageType T>
size_t MessageQueue<T>::getSubscriberCount() const noexcept {
    std::lock_guard lock(m_mutex_);
    return m_subscribers_.size();
}

template <MessageType T>
size_t MessageQueue<T>::cancelMessages(
    std::function<bool(const T&)> cancelCondition) noexcept {
    if (!cancelCondition) {
        return 0;
    }
    size_t cancelledCount = 0;
#ifdef ATOM_USE_LOCKFREE_QUEUE
    // Cancelling from lockfree queue is complex; typically, you'd filter on
    // dequeue. For simplicity, we only cancel from the m_messages_ deque. Users
    // should be aware of this limitation if lockfree queue is active.
    spdlog::warn(
        "cancelMessages currently only operates on the standard deque, not the "
        "lockfree queue portion.");
#endif
    std::lock_guard lock(m_mutex_);
    const auto initialSize = m_messages_.size();
    auto it = std::remove_if(m_messages_.begin(), m_messages_.end(),
                             [&cancelCondition](const auto& msg) {
                                 return cancelCondition(msg.data);
                             });
    cancelledCount = std::distance(it, m_messages_.end());
    m_messages_.erase(it, m_messages_.end());
    if (cancelledCount > 0) {
        spdlog::info("Cancelled {} messages from the deque.", cancelledCount);
    }
    return cancelledCount;
}

#ifndef ATOM_USE_LOCKFREE_QUEUE
template <MessageType T>
size_t MessageQueue<T>::clearAllMessages() noexcept {
    std::lock_guard lock(m_mutex_);
    const size_t count = m_messages_.size();
    m_messages_.clear();
    if (count > 0) {
        spdlog::info("Cleared {} messages from the deque.", count);
    }
    return count;
}
#endif

}  // namespace atom::async

#endif  // ATOM_ASYNC_MESSAGE_QUEUE_HPP
