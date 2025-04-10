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
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <asio.hpp>

// Add boost lockfree support
#ifdef ATOM_USE_LOCKFREE_QUEUE
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#endif

namespace atom::async {

// Custom exception classes for message queue operations
class MessageQueueException : public std::runtime_error {
public:
    explicit MessageQueueException(const std::string& message)
        : std::runtime_error(message) {}
};

class SubscriberException : public MessageQueueException {
public:
    explicit SubscriberException(const std::string& message)
        : MessageQueueException(message) {}
};

class TimeoutException : public MessageQueueException {
public:
    explicit TimeoutException(const std::string& message)
        : MessageQueueException(message) {}
};

// Concept to ensure message type has basic requirements
template <typename T>
concept MessageType = requires(T a) {
    { a } -> std::copy_constructible;
    { std::is_move_constructible_v<T> } -> std::convertible_to<bool>;
    { std::is_copy_assignable_v<T> } -> std::convertible_to<bool>;
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
     * @brief Constructs a MessageQueue with the given io_context.
     * @param ioContext The Asio io_context to use for asynchronous operations.
     * @param capacity Initial capacity for lockfree queue (used only if
     * ATOM_USE_LOCKFREE_QUEUE is defined)
     */
    explicit MessageQueue(asio::io_context& ioContext,
                          [[maybe_unused]] size_t capacity = 1024) noexcept
        : ioContext_(ioContext)
#ifdef ATOM_USE_LOCKFREE_QUEUE
#ifdef ATOM_USE_SPSC_QUEUE
          ,
          m_lockfreeQueue_(capacity)
#else
          ,
          m_lockfreeQueue_(capacity)
#endif
#endif
    {
    }

    // Rule of five implementation
    ~MessageQueue() noexcept { stopProcessing(); }

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
     * @throws SubscriberException if the callback is empty
     */
    void subscribe(
        CallbackType callback, std::string_view subscriberName,
        int priority = 0, FilterType filter = nullptr,
        std::chrono::milliseconds timeout = std::chrono::milliseconds::zero());

    /**
     * @brief Unsubscribe from messages using the given callback.
     *
     * @param callback The callback function used during subscription.
     * @return true if subscriber was found and removed, false otherwise
     */
    [[nodiscard]] bool unsubscribe(const CallbackType& callback) noexcept;

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
        // Try pushing to the queue with a retry mechanism
        for (int retry = 0; retry < 3 && !pushed; ++retry) {
            pushed = m_lockfreeQueue_.push(msg);
            if (!pushed) {
                std::this_thread::yield();
            }
        }

        // If queue is full, fall back to mutex-based approach
        if (!pushed) {
            std::lock_guard lock(m_mutex_);
            m_messages_.emplace_back(std::move(msg));
        }

        // Wake up processing thread if needed
        m_condition_.notify_one();
        ioContext_.post([this]() { processMessages(); });
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
        // Try pushing to the queue with a retry mechanism
        for (int retry = 0; retry < 3 && !pushed; ++retry) {
            pushed = m_lockfreeQueue_.push(msg);
            if (!pushed) {
                std::this_thread::yield();
            }
        }

        // If queue is full, fall back to mutex-based approach
        if (!pushed) {
            std::lock_guard lock(m_mutex_);
            m_messages_.emplace_back(std::move(msg));
        }

        // Wake up processing thread if needed
        m_condition_.notify_one();
        ioContext_.post([this]() { processMessages(); });
    }

#else
    /**
     * @brief Publish a message to the queue, with an optional priority.
     *
     * @param message The message to publish.
     * @param priority The priority of the message, higher priority messages are
     * handled first.
     */
    void publish(const T& message, int priority = 0);

    /**
     * @brief Publish a message to the queue using move semantics.
     *
     * @param message The message to publish (will be moved).
     * @param priority The priority of the message.
     */
    void publish(T&& message, int priority = 0);
#endif

    /**
     * @brief Start processing messages in the queue.
     */
    void startProcessing();

    /**
     * @brief Stop processing messages in the queue.
     */
    void stopProcessing() noexcept;

    /**
     * @brief Get the number of messages currently in the queue.
     * @return The number of messages in the queue.
     */
#ifdef ATOM_USE_LOCKFREE_QUEUE
    [[nodiscard]] size_t getMessageCount() const noexcept {
        size_t lockfreeCount = 0;
        if (!m_lockfreeQueue_.empty()) {
            lockfreeCount =
                1;  // Can only detect empty/non-empty with lockfree queue
        }

        std::lock_guard lock(m_mutex_);
        return lockfreeCount + m_messages_.size();
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
            m_lockfreeQueue_.reserve(newCapacity);
            return true;
        } catch (...) {
            return false;
        }
#else
        return false;  // Not supported with SPSC queue
#endif
    }

    /**
     * @brief Get the capacity of the lockfree queue
     * @return Current capacity of the lockfree queue
     */
    [[nodiscard]] size_t getQueueCapacity() const noexcept {
#ifdef ATOM_USE_LOCKFREE_QUEUE
        return m_lockfreeQueue_.capacity();
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

        // Clear lockfree queue
        Message msg;
        while (m_lockfreeQueue_.pop(msg)) {
            count++;
        }

        // Clear standard queue
        {
            std::lock_guard lock(m_mutex_);
            count += m_messages_.size();
            m_messages_.clear();
        }

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
            *cancelled = true;
            if (!result.has_value()) {
                throw MessageQueueException("No message received");
            }
            return std::move(*result);
        }

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
            return priority > other.priority;
        }
    };

    struct Message {
        T data;
        int priority;
        std::chrono::steady_clock::time_point timestamp;

        Message() {}  // Default constructor required for boost::lockfree

        Message(T data, int priority)
            : data(std::move(data)),
              priority(priority),
              timestamp(std::chrono::steady_clock::now()) {}

        bool operator<(const Message& other) const noexcept {
            // First compare by priority, then by timestamp (FIFO for equal
            // priorities)
            return priority != other.priority ? priority > other.priority
                                              : timestamp < other.timestamp;
        }
    };

    std::deque<Message> m_messages_;
    std::vector<Subscriber> m_subscribers_;
    mutable std::mutex m_mutex_;
    std::condition_variable m_condition_;
    std::atomic<bool> m_isRunning_{false};
    std::atomic<bool> m_isProcessing_{false};
    asio::io_context& ioContext_;
    std::unique_ptr<std::jthread> m_processingThread_;

#ifdef ATOM_USE_LOCKFREE_QUEUE
#ifdef ATOM_USE_SPSC_QUEUE
    // Single-producer, single-consumer queue
    boost::lockfree::spsc_queue<Message> m_lockfreeQueue_;
#else
    // Multi-producer, multi-consumer queue
    boost::lockfree::queue<Message> m_lockfreeQueue_;
#endif
#endif

#ifdef ATOM_USE_LOCKFREE_QUEUE
    /**
     * @brief Process messages in the queue.
     * Lockfree version.
     */
    void processMessages() {
        if (!m_isRunning_.load() || m_isProcessing_.load()) {
            return;
        }

        // First check lockfree queue
        Message message;
        bool messageProcessed = false;

        // Try to get a message from the lockfree queue
        if (m_lockfreeQueue_.pop(message)) {
            messageProcessed = true;

            // Make a copy of subscribers to avoid issues if list changes during
            // processing
            std::vector<Subscriber> subscribersCopy;
            {
                std::lock_guard subscribersLock(m_mutex_);
                subscribersCopy = m_subscribers_;
            }

            // Process message with all subscribers in priority order
            for (const auto& subscriber : subscribersCopy) {
                try {
                    if (applyFilter(subscriber, message.data)) {
                        handleTimeout(subscriber, message.data);
                    }
                } catch (const std::exception&) {
                    // Handle exceptions but continue processing for other
                    // subscribers
                }
            }
        }

        // If no message in lockfree queue, check standard queue
        if (!messageProcessed) {
            std::unique_lock lock(m_mutex_);
            if (m_messages_.empty()) {
                return;
            }

            // Sort messages by priority
            std::ranges::sort(m_messages_);

            // Process the highest priority message
            message = std::move(m_messages_.front());
            m_messages_.pop_front();
            messageProcessed = true;

            // Release the lock before processing
            lock.unlock();

            // Make a copy of subscribers to avoid issues if list changes during
            // processing
            std::vector<Subscriber> subscribersCopy;
            {
                std::lock_guard subscribersLock(m_mutex_);
                subscribersCopy = m_subscribers_;
            }

            // Process message with all subscribers in priority order
            for (const auto& subscriber : subscribersCopy) {
                try {
                    if (applyFilter(subscriber, message.data)) {
                        handleTimeout(subscriber, message.data);
                    }
                } catch (const std::exception&) {
                    // Handle exceptions but continue processing for other
                    // subscribers
                }
            }
        }

        // Schedule next message processing if there are more messages
        if (messageProcessed) {
            ioContext_.post([this]() { processMessages(); });
        }
    }
#else
    /**
     * @brief Process messages in the queue.
     */
    void processMessages();
#endif

    /**
     * @brief Apply the filter to a message for a given subscriber.
     * @param subscriber The subscriber to apply the filter for.
     * @param message The message to filter.
     * @return True if the message passes the filter, false otherwise.
     */
    [[nodiscard]] bool applyFilter(const Subscriber& subscriber,
                                   const T& message) const noexcept;

    /**
     * @brief Handle the timeout for a given subscriber and message.
     * @param subscriber The subscriber to handle the timeout for.
     * @param message The message to process.
     * @return True if the message was processed within the timeout, false
     * otherwise.
     */
    [[nodiscard]] bool handleTimeout(const Subscriber& subscriber,
                                     const T& message) const;

    /**
     * @brief Sort subscribers by priority
     */
    void sortSubscribers() noexcept;
};

template <MessageType T>
void MessageQueue<T>::subscribe(CallbackType callback,
                                std::string_view subscriberName, int priority,
                                FilterType filter,
                                std::chrono::milliseconds timeout) {
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
}

template <MessageType T>
bool MessageQueue<T>::unsubscribe(const CallbackType& callback) noexcept {
    std::lock_guard lock(m_mutex_);
    const auto initialSize = m_subscribers_.size();

    auto it = std::remove_if(m_subscribers_.begin(), m_subscribers_.end(),
                             [&callback](const auto& subscriber) {
                                 return subscriber.callback.target_type() ==
                                        callback.target_type();
                             });

    m_subscribers_.erase(it, m_subscribers_.end());
    return m_subscribers_.size() < initialSize;
}

#ifndef ATOM_USE_LOCKFREE_QUEUE
template <MessageType T>
void MessageQueue<T>::publish(const T& message, int priority) {
    {
        std::lock_guard lock(m_mutex_);
        m_messages_.emplace_back(message, priority);
    }
    // Wake up processing thread if needed
    m_condition_.notify_one();
    ioContext_.post([this]() { processMessages(); });
}

template <MessageType T>
void MessageQueue<T>::publish(T&& message, int priority) {
    {
        std::lock_guard lock(m_mutex_);
        m_messages_.emplace_back(std::move(message), priority);
    }
    // Wake up processing thread if needed
    m_condition_.notify_one();
    ioContext_.post([this]() { processMessages(); });
}
#endif

template <MessageType T>
void MessageQueue<T>::startProcessing() {
    if (m_isRunning_.exchange(true)) {
        return;  // Already running
    }

    m_processingThread_ =
        std::make_unique<std::jthread>([this](std::stop_token stoken) {
            m_isProcessing_.store(true);
            while (!stoken.stop_requested()) {
                std::unique_lock lock(m_mutex_);

                // Wait for messages or stop request
                m_condition_.wait(lock, [this, &stoken]() {
                    return !m_messages_.empty() || stoken.stop_requested();
                });

                if (stoken.stop_requested()) {
                    break;
                }

                if (!m_messages_.empty()) {
                    // Sort messages by priority
                    std::ranges::sort(m_messages_);

                    // Process all available messages
                    while (!m_messages_.empty()) {
                        auto message = std::move(m_messages_.front());
                        m_messages_.pop_front();

                        // Release lock while processing to allow new messages
                        lock.unlock();

                        // Make a copy of subscribers to avoid issues if list
                        // changes during processing
                        std::vector<Subscriber> subscribersCopy;
                        {
                            std::lock_guard subscribersLock(m_mutex_);
                            subscribersCopy = m_subscribers_;
                        }

                        for (const auto& subscriber : subscribersCopy) {
                            try {
                                if (applyFilter(subscriber, message.data)) {
                                    handleTimeout(subscriber, message.data);
                                }
                            } catch (const TimeoutException& e) {
                                // Log timeout but continue with other
                                // subscribers In a real application, you might
                                // want to log this
                            } catch (const std::exception& e) {
                                // Handle exceptions from subscriber callbacks
                                // In a real application, you might want to log
                                // this
                            }
                        }

                        lock.lock();
                    }
                }
            }
            m_isProcessing_.store(false);
        });

    // Execute any pending tasks
    if (!ioContext_.stopped()) {
        ioContext_.restart();
        ioContext_.poll();
    }
}

template <MessageType T>
void MessageQueue<T>::stopProcessing() noexcept {
    if (!m_isRunning_.exchange(false)) {
        return;  // Already stopped
    }

    // Stop the processing thread
    if (m_processingThread_) {
        m_processingThread_->request_stop();
        m_condition_.notify_all();
        m_processingThread_.reset();
    }

    if (!ioContext_.stopped()) {
        try {
            ioContext_.stop();
        } catch (...) {
            // Ignore any exceptions when stopping
        }
    }
}

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

    std::lock_guard lock(m_mutex_);
    const auto initialSize = m_messages_.size();

    auto it = std::remove_if(m_messages_.begin(), m_messages_.end(),
                             [&cancelCondition](const auto& msg) {
                                 return cancelCondition(msg.data);
                             });

    m_messages_.erase(it, m_messages_.end());
    return initialSize - m_messages_.size();
}

#ifndef ATOM_USE_LOCKFREE_QUEUE
template <MessageType T>
size_t MessageQueue<T>::clearAllMessages() noexcept {
    std::lock_guard lock(m_mutex_);
    const size_t count = m_messages_.size();
    m_messages_.clear();
    return count;
}
#endif

#ifndef ATOM_USE_LOCKFREE_QUEUE
template <MessageType T>
void MessageQueue<T>::processMessages() {
    if (!m_isRunning_.load() || m_isProcessing_.load()) {
        return;
    }

    std::unique_lock lock(m_mutex_);
    if (m_messages_.empty()) {
        return;
    }

    // Sort messages by priority
    std::ranges::sort(m_messages_);

    // Process the highest priority message
    auto message = std::move(m_messages_.front());
    m_messages_.pop_front();

    // Release the lock before processing
    lock.unlock();

    // Make a copy of subscribers to avoid issues if list changes during
    // processing
    std::vector<Subscriber> subscribersCopy;
    {
        std::lock_guard subscribersLock(m_mutex_);
        subscribersCopy = m_subscribers_;
    }

    // Process message with all subscribers in priority order
    for (const auto& subscriber : subscribersCopy) {
        try {
            if (applyFilter(subscriber, message.data)) {
                handleTimeout(subscriber, message.data);
            }
        } catch (const std::exception&) {
            // Handle exceptions but continue processing for other subscribers
        }
    }

    // Schedule next message processing if there are more
    lock.lock();
    if (!m_messages_.empty()) {
        ioContext_.post([this]() { processMessages(); });
    }
}
#endif

template <MessageType T>
bool MessageQueue<T>::applyFilter(const Subscriber& subscriber,
                                  const T& message) const noexcept {
    if (!subscriber.filter) {
        return true;
    }

    try {
        return subscriber.filter(message);
    } catch (...) {
        // If filter throws an exception, we'll skip this subscriber
        return false;
    }
}

template <MessageType T>
bool MessageQueue<T>::handleTimeout(const Subscriber& subscriber,
                                    const T& message) const {
    if (subscriber.timeout == std::chrono::milliseconds::zero()) {
        try {
            subscriber.callback(message);
            return true;
        } catch (const std::exception& e) {
            // Propagate exception upward for caller to handle
            throw;
        }
    }

    // Use a future to handle timeouts
    std::promise<void> promise;
    auto future = promise.get_future();

    auto task = [callback = subscriber.callback, &message,
                 promise = std::move(promise)]() mutable {
        try {
            callback(message);
            promise.set_value();
        } catch (...) {
            promise.set_exception(std::current_exception());
        }
    };

    asio::post(ioContext_, std::move(task));

    auto status = future.wait_for(subscriber.timeout);
    if (status == std::future_status::timeout) {
        throw TimeoutException("Subscriber " + subscriber.name +
                               " timed out processing message");
    }

    // Re-throw any exceptions that occurred in the callback
    try {
        future.get();
        return true;
    } catch (...) {
        throw;
    }
}

template <MessageType T>
void MessageQueue<T>::sortSubscribers() noexcept {
    std::ranges::sort(m_subscribers_);
}

}  // namespace atom::async

#endif  // ATOM_ASYNC_MESSAGE_QUEUE_HPP