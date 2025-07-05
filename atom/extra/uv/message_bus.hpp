#pragma once

#include <atomic>
#include <chrono>
#include <concepts>
#include <coroutine>
#include <expected>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>

#include <uv.h>

namespace msgbus {

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

template <typename T>
using Result = std::expected<T, MessageBusError>;

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

    Result<MessageEnvelope<T>> await_resume();

private:
    std::shared_ptr<std::promise<Result<MessageEnvelope<T>>>> promise_;
};

}  // namespace msgbus
