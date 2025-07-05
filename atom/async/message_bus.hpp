/*
 * message_bus.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-7-23

Description: Main Message Bus with Asio support and additional features

**************************************************/

#ifndef ATOM_ASYNC_MESSAGE_BUS_HPP
#define ATOM_ASYNC_MESSAGE_BUS_HPP

#include <algorithm>
#include <any>     // For std::any, std::any_cast, std::bad_any_cast
#include <chrono>  // For std::chrono
#include <concepts>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>  // For std::optional
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>  // For std::thread (used if ATOM_USE_ASIO is off)
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "spdlog/spdlog.h"  // Added for logging

#ifdef ATOM_USE_ASIO
#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/steady_timer.hpp>
#endif

#if __cpp_impl_coroutine >= 201902L
#include <coroutine>
#define ATOM_COROUTINE_SUPPORT
#endif

#include "atom/macro.hpp"

#ifdef ATOM_USE_LOCKFREE_QUEUE
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
// Assuming atom/async/queue.hpp is not strictly needed if using boost::lockfree
// directly #include "atom/async/queue.hpp"
#endif

namespace atom::async {

// C++20 concept for messages
template <typename T>
concept MessageConcept =
    std::copyable<T> && !std::is_pointer_v<T> && !std::is_reference_v<T>;

/**
 * @brief Exception class for MessageBus errors
 */
class MessageBusException : public std::runtime_error {
public:
    explicit MessageBusException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief The MessageBus class provides a message bus system with Asio support.
 */
class MessageBus : public std::enable_shared_from_this<MessageBus> {
public:
    using Token = std::size_t;
    static constexpr std::size_t K_MAX_HISTORY_SIZE =
        100;  ///< Maximum number of messages to keep in history.
    static constexpr std::size_t K_MAX_SUBSCRIBERS_PER_MESSAGE =
        1000;  ///< Maximum subscribers per message type to prevent DoS

#ifdef ATOM_USE_LOCKFREE_QUEUE
    // Use lockfree message queue for pending messages
    struct PendingMessage {
        std::string name;
        std::any message;
        std::type_index type;

        template <MessageConcept MessageType>
        PendingMessage(std::string n, const MessageType& msg)
            : name(std::move(n)),
              message(msg),
              type(std::type_index(typeid(MessageType))) {}

        // Required for lockfree queue
        PendingMessage() = default;
        PendingMessage(const PendingMessage&) = default;
        PendingMessage& operator=(const PendingMessage&) = default;
        PendingMessage(PendingMessage&&) noexcept = default;
        PendingMessage& operator=(PendingMessage&&) noexcept = default;
    };

    // Different message queue types based on configuration
    using MessageQueue =
        std::conditional_t<defined(ATOM_USE_SPSC_QUEUE),
                           boost::lockfree::spsc_queue<PendingMessage>,
                           boost::lockfree::queue<PendingMessage>>;
#endif

// 平台特定优化
#if defined(ATOM_PLATFORM_WINDOWS)
    // Windows特定优化
    static constexpr bool USE_SLIM_RW_LOCKS = true;
    static constexpr bool USE_WAITABLE_TIMERS = true;
#elif defined(ATOM_PLATFORM_APPLE)
    // macOS特定优化
    static constexpr bool USE_DISPATCH_QUEUES = true;
    static constexpr bool USE_SLIM_RW_LOCKS = false;
    static constexpr bool USE_WAITABLE_TIMERS = false;
#else
    // Linux/其他平台优化
    static constexpr bool USE_SLIM_RW_LOCKS = false;
    static constexpr bool USE_WAITABLE_TIMERS = false;
#endif

    /**
     * @brief Constructs a MessageBus.
     * @param io_context The Asio io_context to use (if ATOM_USE_ASIO is
     * defined).
     */
#ifdef ATOM_USE_ASIO
    explicit MessageBus(asio::io_context& io_context)
        : nextToken_(0),
          io_context_(io_context)
#else
    explicit MessageBus()
        : nextToken_(0)
#endif
#ifdef ATOM_USE_LOCKFREE_QUEUE
          ,
          pendingMessages_(1024)  // Initial capacity
          ,
          processingActive_(false)
#endif
    {
#ifdef ATOM_USE_LOCKFREE_QUEUE
        // Message processing might be started on first publish or explicitly
#endif
    }

    /**
     * @brief Destructor to clean up resources
     */
    ~MessageBus() {
#ifdef ATOM_USE_LOCKFREE_QUEUE
        stopMessageProcessing();
#endif
    }

    /**
     * @brief Non-copyable
     */
    MessageBus(const MessageBus&) = delete;
    MessageBus& operator=(const MessageBus&) = delete;

    /**
     * @brief Movable (deleted for simplicity with enable_shared_from_this and
     * potential threads)
     */
    MessageBus(MessageBus&&) noexcept = delete;
    MessageBus& operator=(MessageBus&&) noexcept = delete;

    /**
     * @brief Creates a shared instance of MessageBus.
     * @param io_context The Asio io_context (if ATOM_USE_ASIO is defined).
     * @return A shared pointer to the created MessageBus instance.
     */
#ifdef ATOM_USE_ASIO
    [[nodiscard]] static auto createShared(asio::io_context& io_context)
        -> std::shared_ptr<MessageBus> {
        return std::make_shared<MessageBus>(io_context);
    }
#else
    [[nodiscard]] static auto createShared() -> std::shared_ptr<MessageBus> {
        return std::make_shared<MessageBus>();
    }
#endif

#ifdef ATOM_USE_LOCKFREE_QUEUE
    /**
     * @brief Starts the message processing loop
     */
    void startMessageProcessing() {
        bool expected = false;
        if (processingActive_.compare_exchange_strong(
                expected, true)) {  // Start only if not already active
#ifdef ATOM_USE_ASIO
            asio::post(io_context_, [self = shared_from_this()]() {
                self->processMessagesContinuously();
            });
            spdlog::info(
                "[MessageBus] Asio-driven lock-free message processing "
                "started.");
#else
            if (processingThread_.joinable()) {
                processingThread_.join();  // Join previous thread if any
            }
            processingThread_ =
                std::thread([self_capture = shared_from_this()]() {
                    spdlog::info(
                        "[MessageBus] Non-Asio lock-free processing thread "
                        "started.");
                    while (self_capture->processingActive_.load(
                        std::memory_order_relaxed)) {
                        self_capture->processLockFreeQueueBatch();
                        std::this_thread::sleep_for(std::chrono::milliseconds(
                            5));  // Prevent busy waiting
                    }
                    spdlog::info(
                        "[MessageBus] Non-Asio lock-free processing thread "
                        "stopped.");
                });
#endif
        }
    }

    /**
     * @brief Stops the message processing loop
     */
    void stopMessageProcessing() {
        bool expected = true;
        if (processingActive_.compare_exchange_strong(
                expected, false)) {  // Stop only if active
            spdlog::info("[MessageBus] Lock-free message processing stopping.");
#if !defined(ATOM_USE_ASIO)
            if (processingThread_.joinable()) {
                processingThread_.join();
                spdlog::info("[MessageBus] Non-Asio processing thread joined.");
            }
#else
            // For Asio, stopping is done by not re-posting.
            // The current tasks in io_context will finish.
            spdlog::info(
                "[MessageBus] Asio-driven processing will stop after current "
                "tasks.");
#endif
        }
    }

#ifdef ATOM_USE_ASIO
    /**
     * @brief Process pending messages from the queue continuously
     * (Asio-driven).
     */
    void processMessagesContinuously() {
        if (!processingActive_.load(std::memory_order_relaxed)) {
            spdlog::debug(
                "[MessageBus] Asio processing loop terminating as "
                "processingActive_ is false.");
            return;
        }

        processLockFreeQueueBatch();  // Process one batch

        // Reschedule message processing
        asio::post(io_context_, [self = shared_from_this()]() {
            self->processMessagesContinuously();
        });
    }
#endif  // ATOM_USE_ASIO

    /**
     * @brief Processes a batch of messages from the lock-free queue.
     */
    void processLockFreeQueueBatch() {
        const size_t MAX_MESSAGES_PER_BATCH = 20;
        size_t processed = 0;
        PendingMessage msg_item;  // Renamed to avoid conflict

        while (processed < MAX_MESSAGES_PER_BATCH &&
               pendingMessages_.pop(msg_item)) {
            processOneMessage(msg_item);
            processed++;
        }
        if (processed > 0) {
            spdlog::trace(
                "[MessageBus] Processed {} messages from lock-free queue.",
                processed);
        }
    }

    /**
     * @brief Process a single message from the queue
     */
    void processOneMessage(const PendingMessage& pendingMsg) {
        try {
            std::shared_lock lock(
                mutex_);  // Lock for accessing subscribers_ and namespaces_
            std::unordered_set<Token> calledSubscribers;

            // Find subscribers for this message type
            auto typeIter = subscribers_.find(pendingMsg.type);
            if (typeIter != subscribers_.end()) {
                // Publish to directly matching subscribers
                auto& nameMap = typeIter->second;
                auto nameIter = nameMap.find(pendingMsg.name);
                if (nameIter != nameMap.end()) {
                    publishToSubscribersLockFree(nameIter->second,
                                                 pendingMsg.message,
                                                 calledSubscribers);
                }

                // Publish to namespace matching subscribers
                for (const auto& namespaceName : namespaces_) {
                    if (pendingMsg.name.rfind(namespaceName + ".", 0) ==
                        0) {  // name starts with namespaceName + "."
                        auto nsIter = nameMap.find(namespaceName);
                        if (nsIter != nameMap.end()) {
                            // Ensure we don't call for the exact same name if
                            // pendingMsg.name itself is a registered_ns_key, as
                            // it's already handled by the direct match above.
                            // The calledSubscribers set will prevent actual
                            // duplicate delivery.
                            if (pendingMsg.name != namespaceName) {
                                publishToSubscribersLockFree(nsIter->second,
                                                             pendingMsg.message,
                                                             calledSubscribers);
                            }
                        }
                    }
                }
            }
        } catch (const std::exception& ex) {
            spdlog::error(
                "[MessageBus] Error processing message from queue ('{}'): {}",
                pendingMsg.name, ex.what());
        }
    }

    /**
     * @brief Helper method to publish to subscribers in lockfree mode's
     * processing path
     */
    void publishToSubscribersLockFree(
        const std::vector<Subscriber>& subscribersList, const std::any& message,
        std::unordered_set<Token>& calledSubscribers) {
        for (const auto& subscriber : subscribersList) {
            try {
                if (subscriber.filter(message) &&
                    calledSubscribers.insert(subscriber.token).second) {
                    auto handler_task =
                        [handlerFunc =
                             subscriber.handler,  // Renamed to avoid conflict
                         message_copy = message,
                         token =
                             subscriber.token]() {  // Capture message by value
                                                    // & token for logging
                            try {
                                handlerFunc(message_copy);
                            } catch (const std::exception& e) {
                                spdlog::error(
                                    "[MessageBus] Handler exception (token "
                                    "{}): {}",
                                    token, e.what());
                            }
                        };

#ifdef ATOM_USE_ASIO
                    if (subscriber.async) {
                        asio::post(io_context_, handler_task);
                    } else {
                        handler_task();
                    }
#else
                    // If Asio is not used, async handlers become synchronous
                    handler_task();
                    if (subscriber.async) {
                        spdlog::trace(
                            "[MessageBus] ATOM_USE_ASIO is not defined. Async "
                            "handler for token {} executed synchronously.",
                            subscriber.token);
                    }
#endif
                }
            } catch (const std::exception& e) {
                spdlog::error("[MessageBus] Filter exception (token {}): {}",
                              subscriber.token, e.what());
            }
        }
    }

    /**
     * @brief Modified publish method that uses lockfree queue
     */
    template <MessageConcept MessageType>
    void publish(
        std::string_view name_sv,
        const MessageType& message,  // Renamed name to name_sv
        std::optional<std::chrono::milliseconds> delay = std::nullopt) {
        try {
            if (name_sv.empty()) {
                throw MessageBusException("Message name cannot be empty");
            }
            std::string name_str(name_sv);  // Convert for capture

            // Capture shared_from_this() for the task
            auto sft_ptr = shared_from_this();  // Moved shared_from_this() call
            auto publishTask = [self = sft_ptr, name_s = name_str,
                                message_copy =
                                    message]() {  // Capture the ptr as self
                if (!self->processingActive_.load(std::memory_order_relaxed)) {
                    self->startMessageProcessing();  // Ensure processing is
                                                     // active
                }

                PendingMessage pendingMsg(name_s, message_copy);

                bool pushed = false;
                for (int retry = 0; retry < 3 && !pushed; ++retry) {
                    pushed = self->pendingMessages_.push(pendingMsg);
                    if (!pushed &&
                        retry <
                            2) {  // Don't yield on last attempt before fallback
                        std::this_thread::yield();
                    }
                }

                if (!pushed) {
                    spdlog::warn(
                        "[MessageBus] Message queue full for '{}', processing "
                        "synchronously as fallback.",
                        name_s);
                    self->processOneMessage(pendingMsg);  // Fallback
                } else {
                    spdlog::trace(
                        "[MessageBus] Message '{}' pushed to lock-free queue.",
                        name_s);
                }

                {  // Scope for history lock
                    std::unique_lock lock(self->mutex_);
                    self->recordMessageHistory<MessageType>(name_s,
                                                            message_copy);
                }
            };

            if (delay && delay.value().count() > 0) {
#ifdef ATOM_USE_ASIO
                auto timer =
                    std::make_shared<asio::steady_timer>(io_context_, *delay);
                timer->async_wait([timer, publishTask_copy = publishTask,
                                   name_copy = name_str](
                                      const asio::error_code&
                                          errorCode) {  // Capture task by value
                    if (!errorCode) {
                        publishTask_copy();
                    } else {
                        spdlog::error(
                            "[MessageBus] Asio timer error for message '{}': "
                            "{}",
                            name_copy, errorCode.message());
                    }
                });
#else
                spdlog::debug(
                    "[MessageBus] ATOM_USE_ASIO not defined. Using std::thread "
                    "for delayed publish of '{}'.",
                    name_str);
                auto delayedPublishWrapper =
                    [delay_val = *delay, task_to_run = publishTask,
                     name_copy = name_str]() {  // Removed self capture
                        std::this_thread::sleep_for(delay_val);
                        try {
                            task_to_run();
                        } catch (const std::exception& e) {
                            spdlog::error(
                                "[MessageBus] Exception in non-Asio delayed "
                                "task for message '{}': {}",
                                name_copy, e.what());
                        } catch (...) {
                            spdlog::error(
                                "[MessageBus] Unknown exception in non-Asio "
                                "delayed task for message '{}'",
                                name_copy);
                        }
                    };
                std::thread(delayedPublishWrapper).detach();
#endif
            } else {
                publishTask();
            }
        } catch (const std::exception& ex) {
            spdlog::error(
                "[MessageBus] Error in lock-free publish for message '{}': {}",
                name_sv, ex.what());
            throw MessageBusException(
                std::string("Failed to publish message (lock-free): ") +
                ex.what());
        }
    }
#else  // ATOM_USE_LOCKFREE_QUEUE is not defined (Synchronous publish)
    /**
     * @brief Publishes a message to all relevant subscribers.
     * Synchronous version when lockfree queue is not used.
     * @tparam MessageType The type of the message.
     * @param name_sv The name of the message.
     * @param message The message to publish.
     * @param delay Optional delay before publishing.
     */
    template <MessageConcept MessageType>
    void publish(
        std::string_view name_sv, const MessageType& message,
        std::optional<std::chrono::milliseconds> delay = std::nullopt) {
        try {
            if (name_sv.empty()) {
                throw MessageBusException("Message name cannot be empty");
            }
            std::string name_str(name_sv);

            auto sft_ptr = shared_from_this();  // Moved shared_from_this() call
            auto publishTask = [self = sft_ptr, name_s = name_str,
                                message_copy =
                                    message]() {  // Capture the ptr as self
                std::unique_lock lock(self->mutex_);
                std::unordered_set<Token> calledSubscribers;
                spdlog::trace(
                    "[MessageBus] Publishing message '{}' synchronously.",
                    name_s);

                self->publishToSubscribersInternal<MessageType>(
                    name_s, message_copy, calledSubscribers);

                for (const auto& registered_ns_key : self->namespaces_) {
                    if (name_s.rfind(registered_ns_key + ".", 0) == 0) {
                        if (name_s !=
                            registered_ns_key) {  // Avoid re-processing exact
                                                  // match if it's a namespace
                            self->publishToSubscribersInternal<MessageType>(
                                registered_ns_key, message_copy,
                                calledSubscribers);
                        }
                    }
                }
                self->recordMessageHistory<MessageType>(name_s, message_copy);
            };

            if (delay && delay.value().count() > 0) {
#ifdef ATOM_USE_ASIO
                auto timer =
                    std::make_shared<asio::steady_timer>(io_context_, *delay);
                timer->async_wait(
                    [timer, task_to_run = publishTask,
                     name_copy = name_str](const asio::error_code& errorCode) {
                        if (!errorCode) {
                            task_to_run();
                        } else {
                            spdlog::error(
                                "[MessageBus] Asio timer error for message "
                                "'{}': {}",
                                name_copy, errorCode.message());
                        }
                    });
#else
                spdlog::debug(
                    "[MessageBus] ATOM_USE_ASIO not defined. Using std::thread "
                    "for delayed publish of '{}'.",
                    name_str);
                auto delayedPublishWrapper =
                    [delay_val = *delay, task_to_run = publishTask,
                     name_copy = name_str]() {  // Removed self capture
                        std::this_thread::sleep_for(delay_val);
                        try {
                            task_to_run();
                        } catch (const std::exception& e) {
                            spdlog::error(
                                "[MessageBus] Exception in non-Asio delayed "
                                "task for message '{}': {}",
                                name_copy, e.what());
                        } catch (...) {
                            spdlog::error(
                                "[MessageBus] Unknown exception in non-Asio "
                                "delayed task for message '{}'",
                                name_copy);
                        }
                    };
                std::thread(delayedPublishWrapper).detach();
#endif
            } else {
                publishTask();
            }
        } catch (const std::exception& ex) {
            spdlog::error(
                "[MessageBus] Error in synchronous publish for message '{}': "
                "{}",
                name_sv, ex.what());
            throw MessageBusException(
                std::string("Failed to publish message synchronously: ") +
                ex.what());
        }
    }
#endif  // ATOM_USE_LOCKFREE_QUEUE

    /**
     * @brief Publishes a message to all subscribers globally.
     * @tparam MessageType The type of the message.
     * @param message The message to publish.
     */
    template <MessageConcept MessageType>
    void publishGlobal(const MessageType& message) noexcept {
        try {
            spdlog::trace("[MessageBus] Publishing global message of type {}.",
                          typeid(MessageType).name());
            std::vector<std::string> names_to_publish;
            {
                std::shared_lock lock(mutex_);
                auto typeIter =
                    subscribers_.find(std::type_index(typeid(MessageType)));
                if (typeIter != subscribers_.end()) {
                    names_to_publish.reserve(typeIter->second.size());
                    for (const auto& [name, _] : typeIter->second) {
                        names_to_publish.push_back(name);
                    }
                }
            }

            for (const auto& name : names_to_publish) {
                this->publish<MessageType>(
                    name, message);  // Uses the appropriate publish overload
            }
        } catch (const std::exception& ex) {
            spdlog::error("[MessageBus] Error in publishGlobal: {}", ex.what());
        }
    }

    /**
     * @brief Subscribes to a message.
     * @tparam MessageType The type of the message.
     * @param name_sv The name of the message or namespace.
     * @param handler The handler function.
     * @param async Whether to call the handler asynchronously (requires
     * ATOM_USE_ASIO for true async).
     * @param once Whether to unsubscribe after the first message.
     * @param filter Optional filter function.
     * @return A token representing the subscription.
     */
    template <MessageConcept MessageType>
    [[nodiscard]] auto subscribe(
        std::string_view name_sv,
        std::function<void(const MessageType&)> handler_fn,  // Renamed params
        bool async = true, bool once = false,
        std::function<bool(const MessageType&)> filter_fn =
            [](const MessageType&) { return true; }) -> Token {
        if (name_sv.empty()) {
            throw MessageBusException("Subscription name cannot be empty");
        }
        if (!handler_fn) {
            throw MessageBusException("Handler function cannot be null");
        }

        std::unique_lock lock(mutex_);
        std::string nameStr(name_sv);

        auto& subscribersList =
            subscribers_[std::type_index(typeid(MessageType))][nameStr];

        if (subscribersList.size() >= K_MAX_SUBSCRIBERS_PER_MESSAGE) {
            spdlog::error(
                "[MessageBus] Maximum subscribers ({}) reached for message "
                "name '{}', type '{}'.",
                K_MAX_SUBSCRIBERS_PER_MESSAGE, nameStr,
                typeid(MessageType).name());
            throw MessageBusException(
                "Maximum number of subscribers reached for this message type "
                "and name");
        }

        Token token = nextToken_++;
        subscribersList.emplace_back(Subscriber{
            [handler_capture = std::move(handler_fn)](
                const std::any& msg) {  // Capture handler
                try {
                    handler_capture(std::any_cast<const MessageType&>(msg));
                } catch (const std::bad_any_cast& e) {
                    spdlog::error(
                        "[MessageBus] Handler bad_any_cast (token unknown, "
                        "type {}): {}",
                        typeid(MessageType).name(), e.what());
                }
            },
            async, once,
            [filter_capture =
                 std::move(filter_fn)](const std::any& msg) {  // Capture filter
                try {
                    return filter_capture(
                        std::any_cast<const MessageType&>(msg));
                } catch (const std::bad_any_cast& e) {
                    spdlog::error(
                        "[MessageBus] Filter bad_any_cast (token unknown, type "
                        "{}): {}",
                        typeid(MessageType).name(), e.what());
                    return false;  // Default behavior on cast error
                }
            },
            token});

        namespaces_.insert(extractNamespace(nameStr));
        spdlog::info(
            "[MessageBus] Subscribed to: '{}' (type: {}) with token: {}. "
            "Async: {}, Once: {}",
            nameStr, typeid(MessageType).name(), token, async, once);
        return token;
    }

#if defined(ATOM_COROUTINE_SUPPORT) && defined(ATOM_USE_ASIO)
    /**
     * @brief Awaitable version of subscribe for use with C++20 coroutines
     * @tparam MessageType The type of the message
     */
    template <MessageConcept MessageType>
    struct [[nodiscard]] MessageAwaitable {
        MessageBus& bus_;
        std::string_view name_sv_;  // Renamed
        Token token_{0};
        std::optional<MessageType> message_opt_;  // Renamed
        // bool done_{false}; // Not strictly needed if resume is handled
        // carefully

        explicit MessageAwaitable(MessageBus& bus, std::string_view name)
            : bus_(bus), name_sv_(name) {}

        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> handle) {
            spdlog::trace(
                "[MessageBus] Coroutine awaiting message '{}' of type {}",
                name_sv_, typeid(MessageType).name());
            token_ = bus_.subscribe<MessageType>(
                name_sv_,
                [this, handle](
                    const MessageType&
                        msg) mutable {  // Removed mutable as done_ is removed
                    message_opt_.emplace(msg);
                    // done_ = true;
                    if (handle) {  // Ensure handle is valid before resuming
                        handle.resume();
                    }
                },
                true, true);  // Async true, Once true for typical awaitable
        }

        MessageType await_resume() {
            if (!message_opt_.has_value()) {
                spdlog::error(
                    "[MessageBus] Coroutine resumed for '{}' but no message "
                    "was received.",
                    name_sv_);
                throw MessageBusException("No message received in coroutine");
            }
            spdlog::trace("[MessageBus] Coroutine received message for '{}'",
                          name_sv_);
            return std::move(message_opt_.value());
        }

        ~MessageAwaitable() {
            if (token_ != 0 &&
                bus_.isActive()) {  // Check if bus is still active
                try {
                    // Check if the subscription might still exist before
                    // unsubscribing This is tricky without querying subscriber
                    // state directly here. Unsubscribing a non-existent token
                    // is handled gracefully by unsubscribe.
                    spdlog::trace(
                        "[MessageBus] Cleaning up coroutine subscription token "
                        "{} for '{}'",
                        token_, name_sv_);
                    bus_.unsubscribe<MessageType>(token_);
                } catch (const std::exception& e) {
                    spdlog::warn(
                        "[MessageBus] Exception during coroutine awaitable "
                        "cleanup for token {}: {}",
                        token_, e.what());
                } catch (...) {
                    spdlog::warn(
                        "[MessageBus] Unknown exception during coroutine "
                        "awaitable cleanup for token {}",
                        token_);
                }
            }
        }
    };

    /**
     * @brief Creates an awaitable for receiving a message in a coroutine
     * @tparam MessageType The type of the message
     * @param name The message name to wait for
     * @return An awaitable object for use with co_await
     */
    template <MessageConcept MessageType>
    [[nodiscard]] auto receiveAsync(std::string_view name)
        -> MessageAwaitable<MessageType> {
        return MessageAwaitable<MessageType>(*this, name);
    }
#elif defined(ATOM_COROUTINE_SUPPORT) && !defined(ATOM_USE_ASIO)
    template <MessageConcept MessageType>
    [[nodiscard]] auto receiveAsync(std::string_view name) {
        spdlog::warn(
            "[MessageBus] receiveAsync (coroutines) called but ATOM_USE_ASIO "
            "is not defined. True async behavior is not guaranteed.");
        // Potentially provide a synchronous-emulation or throw an error.
        // For now, let's disallow or make it clear it's not fully async.
        // This requires a placeholder or a compile-time error if not supported.
        // To make it compile, we can return a dummy or throw.
        throw MessageBusException(
            "receiveAsync with coroutines requires ATOM_USE_ASIO to be defined "
            "for proper asynchronous operation.");
        // Or, provide a simplified awaitable that might behave more
        // synchronously: struct DummyAwaitable { bool await_ready() { return
        // true; } void await_suspend(std::coroutine_handle<>) {} MessageType
        // await_resume() { throw MessageBusException("Not implemented"); } };
        // return DummyAwaitable{};
    }
#endif  // ATOM_COROUTINE_SUPPORT

    /**
     * @brief Unsubscribes from a message using the given token.
     * @tparam MessageType The type of the message.
     * @param token The token representing the subscription.
     */
    template <MessageConcept MessageType>
    void unsubscribe(Token token) noexcept {
        try {
            std::unique_lock lock(mutex_);
            auto typeIter = subscribers_.find(
                std::type_index(typeid(MessageType)));  // Renamed iterator
            if (typeIter != subscribers_.end()) {
                bool found = false;
                std::vector<std::string> names_to_cleanup_if_empty;
                for (auto& [name, subscribersList] : typeIter->second) {
                    size_t old_size = subscribersList.size();
                    removeSubscription(subscribersList, token);
                    if (subscribersList.size() < old_size) {
                        found = true;
                        if (subscribersList.empty()) {
                            names_to_cleanup_if_empty.push_back(name);
                        }
                        // Optimization: if 'once' subscribers are common,
                        // breaking here might be too early if a token could
                        // somehow be associated with multiple names (not
                        // current design). For now, assume a token is unique
                        // across all names for a given type. break;
                    }
                }

                for (const auto& name_to_remove : names_to_cleanup_if_empty) {
                    typeIter->second.erase(name_to_remove);
                }
                if (typeIter->second.empty()) {
                    subscribers_.erase(typeIter);
                }

                if (found) {
                    spdlog::info(
                        "[MessageBus] Unsubscribed token: {} for type {}",
                        token, typeid(MessageType).name());
                } else {
                    spdlog::trace(
                        "[MessageBus] Token {} not found for unsubscribe (type "
                        "{}).",
                        token, typeid(MessageType).name());
                }
            } else {
                spdlog::trace(
                    "[MessageBus] Type {} not found for unsubscribe token {}.",
                    typeid(MessageType).name(), token);
            }
        } catch (const std::exception& ex) {
            spdlog::error("[MessageBus] Error in unsubscribe for token {}: {}",
                          token, ex.what());
        }
    }

    /**
     * @brief Unsubscribes all handlers for a given message name or namespace.
     * @tparam MessageType The type of the message.
     * @param name_sv The name of the message or namespace.
     */
    template <MessageConcept MessageType>
    void unsubscribeAll(std::string_view name_sv) noexcept {
        try {
            std::unique_lock lock(mutex_);
            auto typeIter =
                subscribers_.find(std::type_index(typeid(MessageType)));
            if (typeIter != subscribers_.end()) {
                std::string nameStr(name_sv);
                auto nameIterator = typeIter->second.find(nameStr);
                if (nameIterator != typeIter->second.end()) {
                    size_t count = nameIterator->second.size();
                    typeIter->second.erase(
                        nameIterator);  // Erase the entry for this name
                    if (typeIter->second.empty()) {
                        subscribers_.erase(typeIter);
                    }
                    spdlog::info(
                        "[MessageBus] Unsubscribed all {} handlers for: '{}' "
                        "(type {})",
                        count, nameStr, typeid(MessageType).name());
                } else {
                    spdlog::trace(
                        "[MessageBus] No subscribers found for name '{}' (type "
                        "{}) to unsubscribeAll.",
                        nameStr, typeid(MessageType).name());
                }
            }
        } catch (const std::exception& ex) {
            spdlog::error(
                "[MessageBus] Error in unsubscribeAll for name '{}': {}",
                name_sv, ex.what());
        }
    }

    /**
     * @brief Gets the number of subscribers for a given message name or
     * namespace.
     * @tparam MessageType The type of the message.
     * @param name_sv The name of the message or namespace.
     * @return The number of subscribers.
     */
    template <MessageConcept MessageType>
    [[nodiscard]] auto getSubscriberCount(
        std::string_view name_sv) const noexcept -> std::size_t {
        try {
            std::shared_lock lock(mutex_);
            auto typeIter =
                subscribers_.find(std::type_index(typeid(MessageType)));
            if (typeIter != subscribers_.end()) {
                std::string nameStr(name_sv);
                auto nameIterator = typeIter->second.find(nameStr);
                if (nameIterator != typeIter->second.end()) {
                    return nameIterator->second.size();
                }
            }
            return 0;
        } catch (const std::exception& ex) {
            spdlog::error(
                "[MessageBus] Error in getSubscriberCount for name '{}': {}",
                name_sv, ex.what());
            return 0;
        }
    }

    /**
     * @brief Checks if there are any subscribers for a given message name or
     * namespace.
     * @tparam MessageType The type of the message.
     * @param name_sv The name of the message or namespace.
     * @return True if there are subscribers, false otherwise.
     */
    template <MessageConcept MessageType>
    [[nodiscard]] auto hasSubscriber(std::string_view name_sv) const noexcept
        -> bool {
        try {
            std::shared_lock lock(mutex_);
            auto typeIter =
                subscribers_.find(std::type_index(typeid(MessageType)));
            if (typeIter != subscribers_.end()) {
                std::string nameStr(name_sv);
                auto nameIterator = typeIter->second.find(nameStr);
                return nameIterator != typeIter->second.end() &&
                       !nameIterator->second.empty();
            }
            return false;
        } catch (const std::exception& ex) {
            spdlog::error(
                "[MessageBus] Error in hasSubscriber for name '{}': {}",
                name_sv, ex.what());
            return false;
        }
    }

    /**
     * @brief Clears all subscribers.
     */
    void clearAllSubscribers() noexcept {
        try {
            std::unique_lock lock(mutex_);
            subscribers_.clear();
            namespaces_.clear();
            messageHistory_.clear();  // Also clear history
            nextToken_ = 0;           // Reset token counter
            spdlog::info(
                "[MessageBus] Cleared all subscribers, namespaces, and "
                "history.");
        } catch (const std::exception& ex) {
            spdlog::error("[MessageBus] Error in clearAllSubscribers: {}",
                          ex.what());
        }
    }

    /**
     * @brief Gets the list of active namespaces.
     * @return A vector of active namespace names.
     */
    [[nodiscard]] auto getActiveNamespaces() const noexcept
        -> std::vector<std::string> {
        try {
            std::shared_lock lock(mutex_);
            return {namespaces_.begin(), namespaces_.end()};
        } catch (const std::exception& ex) {
            spdlog::error("[MessageBus] Error in getActiveNamespaces: {}",
                          ex.what());
            return {};
        }
    }

    /**
     * @brief Gets the message history for a given message name.
     * @tparam MessageType The type of the message.
     * @param name_sv The name of the message.
     * @param count Maximum number of messages to return.
     * @return A vector of messages.
     */
    template <MessageConcept MessageType>
    [[nodiscard]] auto getMessageHistory(
        std::string_view name_sv, std::size_t count = K_MAX_HISTORY_SIZE) const
        -> std::vector<MessageType> {
        try {
            if (count == 0) {
                return {};
            }

            count = std::min(count, K_MAX_HISTORY_SIZE);
            std::shared_lock lock(mutex_);
            auto typeIter =
                messageHistory_.find(std::type_index(typeid(MessageType)));
            if (typeIter != messageHistory_.end()) {
                std::string nameStr(name_sv);
                auto nameIterator = typeIter->second.find(nameStr);
                if (nameIterator != typeIter->second.end()) {
                    const auto& historyData = nameIterator->second;
                    std::vector<MessageType> history;
                    history.reserve(std::min(count, historyData.size()));

                    std::size_t start = (historyData.size() > count)
                                            ? historyData.size() - count
                                            : 0;
                    for (std::size_t i = start; i < historyData.size(); ++i) {
                        try {
                            history.emplace_back(
                                std::any_cast<const MessageType&>(
                                    historyData[i]));
                        } catch (const std::bad_any_cast& e) {
                            spdlog::warn(
                                "[MessageBus] Bad any_cast in "
                                "getMessageHistory for '{}', type {}: {}",
                                nameStr, typeid(MessageType).name(), e.what());
                        }
                    }
                    return history;
                }
            }
            return {};
        } catch (const std::exception& ex) {
            spdlog::error(
                "[MessageBus] Error in getMessageHistory for name '{}': {}",
                name_sv, ex.what());
            return {};
        }
    }

    /**
     * @brief Checks if the message bus is currently processing messages (for
     * lock-free queue) or generally operational.
     * @return True if active, false otherwise
     */
    [[nodiscard]] bool isActive() const noexcept {
#ifdef ATOM_USE_LOCKFREE_QUEUE
        return processingActive_.load(std::memory_order_relaxed);
#else
        return true;  // Synchronous mode is always considered active for
                      // publishing
#endif
    }

    /**
     * @brief Gets the current statistics for the message bus
     * @return A structure containing statistics
     */
    [[nodiscard]] auto getStatistics() const noexcept {
        std::shared_lock lock(mutex_);
        struct Statistics {
            size_t subscriberCount{0};
            size_t typeCount{0};
            size_t namespaceCount{0};
            size_t historyTotalMessages{0};
#ifdef ATOM_USE_LOCKFREE_QUEUE
            size_t pendingQueueSizeApprox{0};  // Approximate for lock-free
#endif
        } stats;

        stats.namespaceCount = namespaces_.size();
        stats.typeCount = subscribers_.size();

        for (const auto& [_, typeMap] : subscribers_) {
            for (const auto& [__, subscribersList] : typeMap) {  // Renamed
                stats.subscriberCount += subscribersList.size();
            }
        }

        for (const auto& [_, nameMap] : messageHistory_) {
            for (const auto& [__, historyList] : nameMap) {  // Renamed
                stats.historyTotalMessages += historyList.size();
            }
        }
#ifdef ATOM_USE_LOCKFREE_QUEUE
        // pendingMessages_.empty() is usually available, but size might not be
        // cheap/exact. For boost::lockfree::queue, there's no direct size(). We
        // can't get an exact size easily. We can only check if it's empty or
        // try to count by popping, which is not suitable here. So, we'll omit
        // pendingQueueSizeApprox or set to 0 if not available.
        // stats.pendingQueueSizeApprox = pendingMessages_.read_available(); //
        // If spsc_queue or similar with read_available
#endif
        return stats;
    }

private:
    struct Subscriber {
        std::function<void(const std::any&)> handler;
        bool async;
        bool once;
        std::function<bool(const std::any&)> filter;
        Token token;
    } ATOM_ALIGNAS(64);

#ifndef ATOM_USE_LOCKFREE_QUEUE  // Only needed for synchronous publish
    /**
     * @brief Internal method to publish to subscribers (called under lock).
     * @tparam MessageType The type of the message.
     * @param name The name of the message.
     * @param message The message to publish.
     * @param calledSubscribers The set of already called subscribers.
     */
    template <MessageConcept MessageType>
    void publishToSubscribersInternal(
        const std::string& name, const MessageType& message,
        std::unordered_set<Token>& calledSubscribers) {
        auto typeIter = subscribers_.find(std::type_index(typeid(MessageType)));
        if (typeIter == subscribers_.end())
            return;

        auto nameIterator = typeIter->second.find(name);
        if (nameIterator == typeIter->second.end())
            return;

        auto& subscribersList = nameIterator->second;
        std::vector<Token> tokensToRemove;  // For one-time subscribers

        for (auto& subscriber :
             subscribersList) {  // Iterate by reference to allow modification
                                 // if needed (though not directly here)
            try {
                // Ensure message is converted to std::any for filter and
                // handler
                std::any msg_any = message;
                if (subscriber.filter(msg_any) &&
                    calledSubscribers.insert(subscriber.token).second) {
                    auto handler_task =
                        [handlerFunc = subscriber.handler,
                         message_for_handler = msg_any,
                         token =
                             subscriber
                                 .token]() {  // Capture message_any by value
                            try {
                                handlerFunc(message_for_handler);
                            } catch (const std::exception& e) {
                                spdlog::error(
                                    "[MessageBus] Handler exception (sync "
                                    "publish, token {}): {}",
                                    token, e.what());
                            }
                        };

#ifdef ATOM_USE_ASIO
                    if (subscriber.async) {
                        asio::post(io_context_, handler_task);
                    } else {
                        handler_task();
                    }
#else
                    handler_task();  // Synchronous if no Asio
                    if (subscriber.async) {
                        spdlog::trace(
                            "[MessageBus] ATOM_USE_ASIO not defined. Async "
                            "handler for token {} (sync publish) executed "
                            "synchronously.",
                            subscriber.token);
                    }
#endif
                    if (subscriber.once) {
                        tokensToRemove.push_back(subscriber.token);
                    }
                }
            } catch (const std::bad_any_cast& e) {
                spdlog::error(
                    "[MessageBus] Filter bad_any_cast (sync publish, token "
                    "{}): {}",
                    subscriber.token, e.what());
            } catch (const std::exception& e) {
                spdlog::error(
                    "[MessageBus] Filter/Handler exception (sync publish, "
                    "token {}): {}",
                    subscriber.token, e.what());
            }
        }

        if (!tokensToRemove.empty()) {
            subscribersList.erase(
                std::remove_if(subscribersList.begin(), subscribersList.end(),
                               [&](const Subscriber& sub) {
                                   return std::find(tokensToRemove.begin(),
                                                    tokensToRemove.end(),
                                                    sub.token) !=
                                          tokensToRemove.end();
                               }),
                subscribersList.end());
            if (subscribersList.empty()) {
                // If list becomes empty, remove 'name' entry from
                // typeIter->second
                typeIter->second.erase(nameIterator);
                if (typeIter->second.empty()) {
                    // If type map becomes empty, remove type_index entry from
                    // subscribers_
                    subscribers_.erase(typeIter);
                }
            }
        }
    }
#endif  // !ATOM_USE_LOCKFREE_QUEUE

    /**
     * @brief Removes a subscription from the list.
     * @param subscribersList The list of subscribers.
     * @param token The token representing the subscription.
     */
    static void removeSubscription(std::vector<Subscriber>& subscribersList,
                                   Token token) noexcept {
        // auto old_size = subscribersList.size(); // Not strictly needed here
        std::erase_if(subscribersList, [token](const Subscriber& sub) {
            return sub.token == token;
        });
        // if (subscribersList.size() < old_size) {
        // Logged by caller if needed
        // }
    }

    /**
     * @brief Records a message in the history.
     * @tparam MessageType The type of the message.
     * @param name The name of the message.
     * @param message The message to record.
     */
    template <MessageConcept MessageType>
    void recordMessageHistory(const std::string& name,
                              const MessageType& message) {
        // Assumes mutex_ is already locked by caller
        auto& historyList =
            messageHistory_[std::type_index(typeid(MessageType))]
                           [name];  // Renamed
        historyList.emplace_back(
            std::any(message));  // Store as std::any explicitly
        if (historyList.size() > K_MAX_HISTORY_SIZE) {
            historyList.erase(historyList.begin());
        }
        spdlog::trace(
            "[MessageBus] Recorded message for '{}' in history. History size: "
            "{}",
            name, historyList.size());
    }

    /**
     * @brief Extracts the namespace from the message name.
     * @param name_sv The message name.
     * @return The namespace part of the name.
     */
    [[nodiscard]] std::string extractNamespace(
        std::string_view name_sv) const noexcept {
        auto pos = name_sv.find('.');
        if (pos != std::string_view::npos) {
            return std::string(name_sv.substr(0, pos));
        }
        // If no '.', the name itself can be considered a "namespace" or root
        // level. For consistency, if we always want a distinct namespace part,
        // this might return empty or the name itself. Current logic: "foo.bar"
        // -> "foo"; "foo" -> "foo". If "foo" should not be a namespace for
        // itself, then: return (pos != std::string_view::npos) ?
        // std::string(name_sv.substr(0, pos)) : "";
        return std::string(
            name_sv);  // Treat full name as namespace if no dot, or just the
                       // part before first dot. The original code returns
                       // std::string(name) if no dot. Let's keep it.
    }

#ifdef ATOM_USE_LOCKFREE_QUEUE
    MessageQueue pendingMessages_;
    std::atomic<bool> processingActive_;
#if !defined(ATOM_USE_ASIO)
    std::thread processingThread_;
#endif
#endif

    std::unordered_map<std::type_index,
                       std::unordered_map<std::string, std::vector<Subscriber>>>
        subscribers_;
    std::unordered_map<std::type_index,
                       std::unordered_map<std::string, std::vector<std::any>>>
        messageHistory_;
    std::unordered_set<std::string> namespaces_;
    mutable std::shared_mutex
        mutex_;  // For subscribers_, messageHistory_, namespaces_, nextToken_
    Token nextToken_;

#ifdef ATOM_USE_ASIO
    asio::io_context& io_context_;
#endif
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_MESSAGE_BUS_HPP
