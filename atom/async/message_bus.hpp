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
#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/steady_timer.hpp>
#include <concepts>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if __cpp_impl_coroutine >= 201902L
#include <coroutine>
#define ATOM_COROUTINE_SUPPORT
#endif

#include "atom/macro.hpp"

#ifdef ATOM_USE_LOCKFREE_QUEUE
#include <boost/lockfree/queue.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include "atom/async/queue.hpp"
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
     * @brief Constructs a MessageBus with the given io_context.
     * @param io_context The Asio io_context to use for asynchronous operations.
     */
    explicit MessageBus(asio::io_context& io_context)
        : nextToken_(0),
          io_context_(io_context)
#ifdef ATOM_USE_LOCKFREE_QUEUE
          ,
          pendingMessages_(1024)  // Initial capacity
          ,
          processingActive_(false)
#endif
    {
#ifdef ATOM_USE_LOCKFREE_QUEUE
        // Start the message processing thread
        startMessageProcessing();
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
     * @brief Movable
     */
    MessageBus(MessageBus&&) noexcept = delete;
    MessageBus& operator=(MessageBus&&) noexcept = delete;

    /**
     * @brief Creates a shared instance of MessageBus.
     * @param io_context The Asio io_context to use for asynchronous operations.
     * @return A shared pointer to the created MessageBus instance.
     */
    [[nodiscard]] static auto createShared(asio::io_context& io_context)
        -> std::shared_ptr<MessageBus> {
        return std::make_shared<MessageBus>(io_context);
    }

#ifdef ATOM_USE_LOCKFREE_QUEUE
    /**
     * @brief Starts the message processing loop
     */
    void startMessageProcessing() {
        processingActive_ = true;
        asio::post(io_context_,
                   [self = shared_from_this()]() { self->processMessages(); });
    }

    /**
     * @brief Stops the message processing loop
     */
    void stopMessageProcessing() { processingActive_ = false; }

    /**
     * @brief Process pending messages from the queue
     */
    void processMessages() {
        if (!processingActive_)
            return;

        const size_t MAX_MESSAGES_PER_BATCH = 20;
        size_t processed = 0;

        while (processingActive_ && processed < MAX_MESSAGES_PER_BATCH) {
            PendingMessage msg;
            if (pendingMessages_.pop(msg)) {
                processOneMessage(msg);
                processed++;
            } else {
                break;  // No more messages
            }
        }

        // Reschedule message processing
        if (processingActive_) {
            asio::post(io_context_, [self = shared_from_this()]() {
                self->processMessages();
            });
        }
    }

    /**
     * @brief Process a single message from the queue
     */
    void processOneMessage(const PendingMessage& pendingMsg) {
        try {
            std::shared_lock lock(mutex_);
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
                    if (pendingMsg.name.find(namespaceName + ".") == 0) {
                        auto nsIter = nameMap.find(namespaceName);
                        if (nsIter != nameMap.end()) {
                            publishToSubscribersLockFree(nsIter->second,
                                                         pendingMsg.message,
                                                         calledSubscribers);
                        }
                    }
                }
            }
        } catch (const std::exception& ex) {
            std::cerr << "[MessageBus] Error processing message: " << ex.what()
                      << std::endl;
        }
    }

    /**
     * @brief Helper method to publish to subscribers in lockfree mode
     */
    void publishToSubscribersLockFree(
        const std::vector<Subscriber>& subscribersList, const std::any& message,
        std::unordered_set<Token>& calledSubscribers) {
        for (const auto& subscriber : subscribersList) {
            try {
                if (subscriber.filter(message) &&
                    calledSubscribers.insert(subscriber.token).second) {
                    auto handler = [handlerFunc = subscriber.handler,
                                    message]() {
                        try {
                            handlerFunc(message);
                        } catch (const std::exception& e) {
                            std::cerr << "[MessageBus] Handler exception: "
                                      << e.what() << std::endl;
                        }
                    };

                    if (subscriber.async) {
                        asio::post(io_context_, handler);
                    } else {
                        handler();
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[MessageBus] Filter exception: " << e.what()
                          << std::endl;
            }
        }
    }

    /**
     * @brief Modified publish method that uses lockfree queue
     */
    template <MessageConcept MessageType>
    void publish(
        std::string_view name, const MessageType& message,
        std::optional<std::chrono::milliseconds> delay = std::nullopt) {
        try {
            if (name.empty()) {
                throw MessageBusException("Message name cannot be empty");
            }

            if (!processingActive_) {
                startMessageProcessing();
            }

            auto publishTask = [this, name = std::string(name), message]() {
                // Create the pending message and push to the lockfree queue
                PendingMessage pendingMsg(name, message);

                // Try to push to queue with a retry mechanism
                bool pushed = false;
                for (int retry = 0; retry < 3 && !pushed; ++retry) {
                    pushed = pendingMessages_.push(pendingMsg);
                    if (!pushed) {
                        std::this_thread::yield();  // Let other threads run
                    }
                }

                if (!pushed) {
                    // Fall back to synchronous processing if queue is full
                    std::cerr << "[MessageBus] Warning: Message queue full, "
                                 "processing synchronously"
                              << std::endl;
                    processOneMessage(pendingMsg);
                }

                // Record the message in history (with locking)
                {
                    std::unique_lock lock(mutex_);
                    recordMessageHistory<MessageType>(
                        std::string(name),  // Ensure name is a std::string here
                        message);
                }
            };

            if (delay) {
                // Use Asio's steady_timer for delayed publishing
                auto timer =
                    std::make_shared<asio::steady_timer>(io_context_, *delay);
                timer->async_wait(
                    [timer, publishTask](const asio::error_code& errorCode) {
                        if (!errorCode) {
                            publishTask();
                        } else {
                            std::cerr << "[MessageBus] Timer error: "
                                      << errorCode.message() << std::endl;
                        }
                    });
            } else {
                // Immediately execute the publish task
                publishTask();
            }
        } catch (const std::exception& ex) {
            std::cerr << "[MessageBus] Error in publish: " << ex.what()
                      << std::endl;
            throw MessageBusException(
                std::string("Failed to publish message: ") + ex.what());
        }
    }
#else   // ATOM_USE_LOCKFREE_QUEUE is not defined
    /**
     * @brief Publishes a message to all relevant subscribers.
     * Synchronous version when lockfree queue is not used.
     * @tparam MessageType The type of the message.
     * @param name The name of the message.
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

            std::string name_str(name_sv);  // Convert to string for capture

            auto publishTask = [this, name_str, message]() {
                std::unique_lock lock(
                    mutex_);  // Acquire unique lock for all operations
                std::unordered_set<Token> calledSubscribers;

                // 1. Publish to directly matching subscribers for the specific
                // name `name_str`
                publishToSubscribers<MessageType>(name_str, message,
                                                  calledSubscribers);

                // 2. Publish to namespace matching subscribers
                // Iterate all registered namespaces. If `name_str`
                // ("foo.bar.baz") starts with `a_namespace + "."` ("foo."),
                // then deliver to subscribers of `a_namespace` ("foo").
                for (const auto& registered_ns_key : namespaces_) {
                    if (name_str.rfind(registered_ns_key + ".", 0) == 0) {
                        // Ensure we don't call for the exact same name if
                        // name_str itself is a registered_ns_key, as it's
                        // already handled by the direct match above. The
                        // calledSubscribers set will prevent actual duplicate
                        // delivery.
                        if (name_str != registered_ns_key) {
                            publishToSubscribers<MessageType>(
                                registered_ns_key, message, calledSubscribers);
                        }
                    }
                }
                recordMessageHistory<MessageType>(name_str, message);
            };

            if (delay) {
                auto timer =
                    std::make_shared<asio::steady_timer>(io_context_, *delay);
                timer->async_wait([timer, publishTask, name_str](
                                      const asio::error_code& errorCode) {
                    if (!errorCode) {
                        publishTask();
                    } else {
                        std::cerr << "[MessageBus] Timer error for message '"
                                  << name_str << "': " << errorCode.message()
                                  << std::endl;
                    }
                });
            } else {
                publishTask();
            }
        } catch (const std::exception& ex) {
            std::cerr << "[MessageBus] Error in synchronous publish: "
                      << ex.what() << std::endl;
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
            std::vector<std::string> names_to_publish;
            {  // Scope for shared_lock
                std::shared_lock lock(mutex_);
                auto typeIter =
                    subscribers_.find(std::type_index(typeid(MessageType)));
                if (typeIter != subscribers_.end()) {
                    names_to_publish.reserve(typeIter->second.size());
                    for (const auto& [name, _] : typeIter->second) {
                        names_to_publish.push_back(name);
                    }
                }
            }  // shared_lock released

            for (const auto& name : names_to_publish) {
                // Use this-> to ensure correct member template lookup
                this->publish<MessageType>(name, message);
            }
        } catch (const std::exception& ex) {
            std::cerr << "[MessageBus] Error in publishGlobal: " << ex.what()
                      << std::endl;
        }
    }

    /**
     * @brief Subscribes to a message.
     * @tparam MessageType The type of the message.
     * @param name The name of the message or namespace (supports wildcard).
     * @param handler The handler function to call when the message is received.
     * @param async Whether to call the handler asynchronously.
     * @param once Whether to unsubscribe after the first message is received.
     * @param filter Optional filter function to determine whether to call the
     * handler.
     * @return A token representing the subscription.
     * @throws MessageBusException if subscription limit is reached or name is
     * invalid
     */
    template <MessageConcept MessageType>
    [[nodiscard]] auto subscribe(
        std::string_view name, std::function<void(const MessageType&)> handler,
        bool async = true, bool once = false,
        std::function<bool(const MessageType&)> filter =
            [](const MessageType&) { return true; }) -> Token {
        if (name.empty()) {
            throw MessageBusException("Subscription name cannot be empty");
        }
        if (!handler) {
            throw MessageBusException("Handler function cannot be null");
        }

        std::unique_lock lock(mutex_);
        std::string nameStr(name);

        auto& subscribersList =
            subscribers_[std::type_index(typeid(MessageType))][nameStr];

        // Check for subscriber limit to prevent DoS attacks
        if (subscribersList.size() >= K_MAX_SUBSCRIBERS_PER_MESSAGE) {
            throw MessageBusException(
                "Maximum number of subscribers reached for this message type");
        }

        Token token = nextToken_++;
        subscribersList.emplace_back(Subscriber{
            [handler = std::move(handler)](const std::any& msg) {
                handler(std::any_cast<const MessageType&>(msg));
            },
            async, once,
            [filter = std::move(filter)](const std::any& msg) {
                return filter(std::any_cast<const MessageType&>(msg));
            },
            token});

        namespaces_.insert(extractNamespace(nameStr));  // Record namespace
        std::cout << "[MessageBus] Subscribed to: " << nameStr
                  << " with token: " << token << std::endl;
        return token;
    }

#ifdef ATOM_COROUTINE_SUPPORT
    /**
     * @brief Awaitable version of subscribe for use with C++20 coroutines
     * @tparam MessageType The type of the message
     */
    template <MessageConcept MessageType>
    struct [[nodiscard]] MessageAwaitable {
        MessageBus& bus_;
        std::string_view name_;
        Token token_{0};
        std::optional<MessageType> message_;
        bool done_{false};

        explicit MessageAwaitable(MessageBus& bus, std::string_view name)
            : bus_(bus), name_(name) {}

        bool await_ready() const noexcept { return false; }

        void await_suspend(std::coroutine_handle<> handle) {
            token_ = bus_.subscribe<MessageType>(
                name_,
                [this, handle](const MessageType& msg) mutable {
                    message_.emplace(msg);
                    done_ = true;
                    handle.resume();
                },
                true, true);
        }

        MessageType await_resume() {
            if (!message_.has_value()) {
                throw MessageBusException("No message received in coroutine");
            }
            return std::move(message_.value());
        }

        ~MessageAwaitable() {
            if (token_ != 0) {
                try {
                    bus_.unsubscribe<MessageType>(token_);
                } catch (...) {
                    // Ignore exceptions in destructor
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
            auto iterator =
                subscribers_.find(std::type_index(typeid(MessageType)));
            if (iterator != subscribers_.end()) {
                for (auto& [name, subscribersList] : iterator->second) {
                    removeSubscription(subscribersList, token);
                }
            }
            std::cout << "[MessageBus] Unsubscribed token: " << token
                      << std::endl;
        } catch (const std::exception& ex) {
            std::cerr << "[MessageBus] Error in unsubscribe: " << ex.what()
                      << std::endl;
        }
    }

    /**
     * @brief Unsubscribes all handlers for a given message name or namespace.
     * @tparam MessageType The type of the message.
     * @param name The name of the message or namespace.
     */
    template <MessageConcept MessageType>
    void unsubscribeAll(std::string_view name) noexcept {
        try {
            std::unique_lock lock(mutex_);
            auto iterator =
                subscribers_.find(std::type_index(typeid(MessageType)));
            if (iterator != subscribers_.end()) {
                std::string nameStr(name);
                auto nameIterator = iterator->second.find(nameStr);
                if (nameIterator != iterator->second.end()) {
                    size_t count = nameIterator->second.size();
                    iterator->second.erase(nameIterator);
                    std::cout << "[MessageBus] Unsubscribed all handlers for: "
                              << nameStr << " (" << count << " subscribers)"
                              << std::endl;
                }
            }
        } catch (const std::exception& ex) {
            std::cerr << "[MessageBus] Error in unsubscribeAll: " << ex.what()
                      << std::endl;
        }
    }

    /**
     * @brief Gets the number of subscribers for a given message name or
     * namespace.
     * @tparam MessageType The type of the message.
     * @param name The name of the message or namespace.
     * @return The number of subscribers.
     */
    template <MessageConcept MessageType>
    [[nodiscard]] auto getSubscriberCount(std::string_view name) const noexcept
        -> std::size_t {
        try {
            std::shared_lock lock(mutex_);
            auto iterator =
                subscribers_.find(std::type_index(typeid(MessageType)));
            if (iterator != subscribers_.end()) {
                std::string nameStr(name);
                auto nameIterator = iterator->second.find(nameStr);
                if (nameIterator != iterator->second.end()) {
                    return nameIterator->second.size();
                }
            }
            return 0;
        } catch (const std::exception& ex) {
            std::cerr << "[MessageBus] Error in getSubscriberCount: "
                      << ex.what() << std::endl;
            return 0;
        }
    }

    /**
     * @brief Checks if there are any subscribers for a given message name or
     * namespace.
     * @tparam MessageType The type of the message.
     * @param name The name of the message or namespace.
     * @return True if there are subscribers, false otherwise.
     */
    template <MessageConcept MessageType>
    [[nodiscard]] auto hasSubscriber(std::string_view name) const noexcept
        -> bool {
        try {
            std::shared_lock lock(mutex_);
            auto iterator =
                subscribers_.find(std::type_index(typeid(MessageType)));
            if (iterator != subscribers_.end()) {
                std::string nameStr(name);
                auto nameIterator = iterator->second.find(nameStr);
                return nameIterator != iterator->second.end() &&
                       !nameIterator->second.empty();
            }
            return false;
        } catch (const std::exception& ex) {
            std::cerr << "[MessageBus] Error in hasSubscriber: " << ex.what()
                      << std::endl;
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
            std::cout << "[MessageBus] Cleared all subscribers." << std::endl;
        } catch (const std::exception& ex) {
            std::cerr << "[MessageBus] Error in clearAllSubscribers: "
                      << ex.what() << std::endl;
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
            std::cerr << "[MessageBus] Error in getActiveNamespaces: "
                      << ex.what() << std::endl;
            return {};
        }
    }

    /**
     * @brief Gets the message history for a given message name.
     * @tparam MessageType The type of the message.
     * @param name The name of the message.
     * @param count Maximum number of messages to return.
     * @return A vector of messages.
     */
    template <MessageConcept MessageType>
    [[nodiscard]] auto getMessageHistory(
        std::string_view name, std::size_t count = K_MAX_HISTORY_SIZE) const
        -> std::vector<MessageType> {
        try {
            if (count == 0) {
                return {};
            }

            count = std::min(count, K_MAX_HISTORY_SIZE);
            std::shared_lock lock(mutex_);
            auto iterator =
                messageHistory_.find(std::type_index(typeid(MessageType)));
            if (iterator != messageHistory_.end()) {
                std::string nameStr(name);
                auto nameIterator = iterator->second.find(nameStr);
                if (nameIterator != iterator->second.end()) {
                    const auto& historyData = nameIterator->second;
                    std::vector<MessageType> history;
                    history.reserve(std::min(count, historyData.size()));

                    std::size_t start = (historyData.size() > count)
                                            ? historyData.size() - count
                                            : 0;
                    for (std::size_t i = start; i < historyData.size(); ++i) {
                        history.emplace_back(
                            std::any_cast<MessageType>(historyData[i]));
                    }
                    return history;
                }
            }
            return {};
        } catch (const std::exception& ex) {
            std::cerr << "[MessageBus] Error in getMessageHistory: "
                      << ex.what() << std::endl;
            return {};
        }
    }

    /**
     * @brief Checks if the message bus is currently processing messages
     * @return True if active, false otherwise
     */
    [[nodiscard]] bool isActive() const noexcept {
#ifdef ATOM_USE_LOCKFREE_QUEUE
        return processingActive_;
#else
        return true;  // 同步模式下总是活跃
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
            size_t historySize{0};
        } stats;

        stats.namespaceCount = namespaces_.size();
        stats.typeCount = subscribers_.size();

        for (const auto& [_, typeMap] : subscribers_) {
            for (const auto& [__, subscribers] : typeMap) {
                stats.subscriberCount += subscribers.size();
            }
        }

        for (const auto& [_, nameMap] : messageHistory_) {
            for (const auto& [__, history] : nameMap) {
                stats.historySize += history.size();
            }
        }

        return stats;
    }

private:
    struct Subscriber {
        std::function<void(const std::any&)>
            handler;  ///< The handler function.
        bool async;   ///< Whether to call the handler asynchronously.
        bool once;    ///< Whether to unsubscribe after the first message.
        std::function<bool(const std::any&)> filter;  ///< The filter function.
        Token token;     ///< The subscription token.
    } ATOM_ALIGNAS(64);  // 缓存行对齐提升性能

    /**
     * @brief Publishes a message to the subscribers.
     * @tparam MessageType The type of the message.
     * @param name The name of the message.
     * @param message The message to publish.
     * @param calledSubscribers The set of already called subscribers.
     */
    template <MessageConcept MessageType>
    void publishToSubscribers(const std::string& name,
                              const MessageType& message,
                              std::unordered_set<Token>& calledSubscribers) {
        auto iterator = subscribers_.find(std::type_index(typeid(MessageType)));
        if (iterator == subscribers_.end())
            return;

        auto nameIterator = iterator->second.find(name);
        if (nameIterator == iterator->second.end())
            return;

        auto& subscribersList = nameIterator->second;

        // Use C++20 erase-remove idiom with ranges
        auto processSubscribers = [&]() {
            // Create a temporary vector of subscribers to remove after
            // processing
            std::vector<Token> tokensToRemove;

            for (auto& subscriber : subscribersList) {
                try {
                    if (subscriber.filter(message) &&
                        calledSubscribers.insert(subscriber.token).second) {
                        auto handler = [handlerFunc = subscriber.handler,
                                        message]() {
                            try {
                                std::any msg = message;
                                handlerFunc(msg);
                            } catch (const std::exception& e) {
                                std::cerr << "[MessageBus] Handler exception: "
                                          << e.what() << std::endl;
                            }
                        };

                        if (subscriber.async) {
                            asio::post(io_context_, handler);
                        } else {
                            handler();
                        }

                        if (subscriber.once) {
                            tokensToRemove.push_back(subscriber.token);
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[MessageBus] Filter exception: " << e.what()
                              << std::endl;
                }
            }

            // Remove the one-time subscribers
            if (!tokensToRemove.empty()) {
                subscribersList.erase(
                    std::remove_if(subscribersList.begin(),
                                   subscribersList.end(),
                                   [&](const Subscriber& sub) {
                                       return std::ranges::find(tokensToRemove,
                                                                sub.token) !=
                                              tokensToRemove.end();
                                   }),
                    subscribersList.end());
            }
        };

        processSubscribers();
    }

    /**
     * @brief Removes a subscription from the list.
     * @param subscribersList The list of subscribers.
     * @param token The token representing the subscription.
     */
    static void removeSubscription(std::vector<Subscriber>& subscribersList,
                                   Token token) noexcept {
        // 使用 C++20 std::erase_if
        std::erase_if(subscribersList, [token](const Subscriber& sub) {
            return sub.token == token;
        });
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
        auto& history =
            messageHistory_[std::type_index(typeid(MessageType))][name];
        history.emplace_back(message);
        if (history.size() > K_MAX_HISTORY_SIZE) {
            history.erase(history.begin());
        }
    }

    /**
     * @brief Extracts the namespace from the message name.
     * @param name The message name.
     * @return The namespace part of the name.
     */
    [[nodiscard]] std::string extractNamespace(
        std::string_view name) const noexcept {
        auto pos = name.find('.');
        if (pos != std::string_view::npos) {
            return std::string(name.substr(0, pos));
        }
        return std::string(name);
    }

#ifdef ATOM_USE_LOCKFREE_QUEUE
    MessageQueue pendingMessages_;  ///< Lockfree queue of pending messages
    std::atomic<bool>
        processingActive_;  ///< Flag to control message processing
#endif

    std::unordered_map<std::type_index,
                       std::unordered_map<std::string, std::vector<Subscriber>>>
        subscribers_;  ///< Map of subscribers.
    std::unordered_map<std::type_index,
                       std::unordered_map<std::string, std::vector<std::any>>>
        messageHistory_;                          ///< Map of message history.
    std::unordered_set<std::string> namespaces_;  ///< Set of namespaces.
    mutable std::shared_mutex mutex_;             ///< Mutex for thread safety.
    Token nextToken_;                             ///< Next token value.

    asio::io_context&
        io_context_;  ///< Asio io_context for asynchronous operations.
};

}  // namespace atom::async

#endif  // ATOM_ASYNC_MESSAGE_BUS_HPP
