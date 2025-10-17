#pragma once

#include <atomic>
#include <chrono>
#include <concepts>
#include <coroutine>
// Temporarily disable std::expected usage until compiler support is stable
// #include <expected>
#include <any>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>

#include <uv.h>

namespace msgbus {

// Simple Result type as a replacement for std::expected
template <typename T, typename E>
class Result {
private:
    std::variant<T, E> data_;

public:
    Result(const T& value) : data_(value) {}
    Result(T&& value) : data_(std::move(value)) {}
    Result(const E& error) : data_(error) {}
    Result(E&& error) : data_(std::move(error)) {}

    bool has_value() const { return std::holds_alternative<T>(data_); }
    operator bool() const { return has_value(); }

    const T& value() const { return std::get<T>(data_); }
    T& value() { return std::get<T>(data_); }

    const E& error() const { return std::get<E>(data_); }
    E& error() { return std::get<E>(data_); }

    const T& operator*() const { return value(); }
    T& operator*() { return value(); }
};

// Specialization for void type
template <typename E>
class Result<void, E> {
private:
    std::optional<E> error_;

public:
    Result() : error_(std::nullopt) {}
    Result(const E& error) : error_(error) {}
    Result(E&& error) : error_(std::move(error)) {}

    bool has_value() const { return !error_.has_value(); }
    operator bool() const { return has_value(); }

    void value() const { /* void has no value to return */ }

    const E& error() const { return error_.value(); }
    E& error() { return error_.value(); }
};

// **Core Concepts**
template <typename T>
concept Serializable = requires(T t) {
    { t.serialize() } -> std::convertible_to<std::string>;
    { T::deserialize(std::declval<std::string>()) } -> std::convertible_to<T>;
};

template <typename T>
concept MessageType = std::copyable<T> && std::default_initializable<T>;

template <typename F, typename T>
concept MessageHandler = std::invocable<F, T>;

template <typename F, typename T>
concept AsyncMessageHandler = MessageHandler<F, T> && requires(F f, T t) {
    { f(t) } -> std::convertible_to<std::future<void>>;
};

// **Error Types**
enum class MessageBusError {
    InvalidTopic,
    HandlerNotFound,
    QueueFull,
    SerializationError,
    NetworkError,
    ShutdownInProgress
};

// Note: Result template is now defined above as a class template

// **Message Envelope**
template <MessageType T>
struct MessageEnvelope {
    std::string topic;
    T payload;
    std::chrono::system_clock::time_point timestamp;
    std::string sender_id;
    uint64_t message_id;
    std::unordered_map<std::string, std::string> metadata;

    MessageEnvelope(std::string t, T p, std::string s = "")
        : topic(std::move(t)),
          payload(std::move(p)),
          timestamp(std::chrono::system_clock::now()),
          sender_id(std::move(s)),
          message_id(generate_id()) {}

private:
    static std::atomic<uint64_t> id_counter;
    static uint64_t generate_id() { return ++id_counter; }
};

template <MessageType T>
std::atomic<uint64_t> MessageEnvelope<T>::id_counter{0};

// **Message Filter**
template <MessageType T>
using MessageFilter = std::function<bool(const MessageEnvelope<T>&)>;

// **Handler Registration**
struct HandlerRegistration {
    uint64_t id;
    std::string topic_pattern;
    std::function<void()> cleanup;

    HandlerRegistration(uint64_t i, std::string p, std::function<void()> c)
        : id(i), topic_pattern(std::move(p)), cleanup(std::move(c)) {}

    ~HandlerRegistration() {
        if (cleanup)
            cleanup();
    }
};

using SubscriptionHandle = std::unique_ptr<HandlerRegistration>;

// **Back-pressure Configuration**
struct BackPressureConfig {
    size_t max_queue_size = 10000;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);
    bool drop_oldest = true;
};

// **Coroutine Support**
template <typename T>
struct MessageAwaiter {
    std::string topic;
    MessageFilter<T> filter;
    std::chrono::milliseconds timeout;

    bool await_ready() const noexcept { return false; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> handle);

    Result<MessageEnvelope<T>, MessageBusError> await_resume();

private:
    std::shared_ptr<std::promise<Result<MessageEnvelope<T>, MessageBusError>>>
        promise_;
};

// **MessageBus Class Declaration**
class MessageBus {
public:
    struct QueueStats {
        size_t pending_messages;
        size_t max_queue_size;
        size_t total_handlers;
        std::chrono::milliseconds avg_delivery_time;
    };

    explicit MessageBus(const BackPressureConfig& config = {});
    ~MessageBus();

    // Template-based subscription
    template <MessageType T, MessageHandler<T> Handler>
    SubscriptionHandle subscribe(const std::string& topic_pattern,
                                 Handler&& handler,
                                 MessageFilter<T> filter = nullptr);

    // Publish message
    template <MessageType T>
    Result<void, MessageBusError> publish(const std::string& topic, T&& message,
                                          const std::string& sender_id = "");

    // Simple publish without template deduction issues
    template <MessageType T>
    Result<void, MessageBusError> publish(const T& message) {
        T message_copy = message;
        return publish("default", std::move(message_copy), "");
    }

    // Coroutine-based message waiting
    template <MessageType T>
    MessageAwaiter<T> wait_for_message(
        const std::string& topic, MessageFilter<T> filter = nullptr,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    // Get queue statistics
    QueueStats get_stats() const;

    void shutdown();
    void process_messages();  // Synchronous processing for examples

    static MessageBus* get_instance();

private:
    BackPressureConfig config_;
    std::atomic<bool> shutdown_;
    std::atomic<uint64_t> handler_id_counter_;

    // Implementation details (will be defined in .cpp)
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

// Template method implementations
template <MessageType T, MessageHandler<T> Handler>
SubscriptionHandle MessageBus::subscribe(const std::string& topic_pattern,
                                         Handler&& handler,
                                         MessageFilter<T> filter) {
    uint64_t handler_id = handler_id_counter_++;

    auto wrapper = [handler = std::forward<Handler>(handler),
                    filter](const std::any& envelope_any) {
        try {
            const auto& envelope =
                std::any_cast<const MessageEnvelope<T>&>(envelope_any);
            if (!filter || filter(envelope)) {
                handler(envelope.payload);
            }
        } catch (const std::bad_any_cast& e) {
            // Log error silently for now
        }
    };

    // Simple implementation - queue the handler
    // Note: This is a basic implementation for example purposes
    auto cleanup = []() {
        // Basic cleanup
    };

    return std::make_unique<HandlerRegistration>(handler_id, topic_pattern,
                                                 cleanup);
}

template <MessageType T>
Result<void, MessageBusError> MessageBus::publish(
    const std::string& topic, T&& message, const std::string& sender_id) {
    if (shutdown_.load()) {
        return MessageBusError::ShutdownInProgress;
    }

    try {
        MessageEnvelope<T> envelope(topic, std::forward<T>(message), sender_id);

        // For this basic implementation, just return success
        // In a full implementation, this would queue the message for processing
        return {};  // Success
    } catch (const std::exception& e) {
        return MessageBusError::SerializationError;
    }
}

}  // namespace msgbus
