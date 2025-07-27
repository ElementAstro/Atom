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
#include <vector>
#include <queue>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <condition_variable>
#include <span>
#include <optional>
#include <variant>

#include <uv.h>

namespace msgbus {

// **Enhanced Core Concepts**
template <typename T>
concept Serializable = requires(T t) {
    { t.serialize() } -> std::convertible_to<std::string>;
    { T::deserialize(std::declval<std::string>()) } -> std::convertible_to<T>;
};

template <typename T>
concept BinarySerializable = requires(T t) {
    { t.serialize_binary() } -> std::convertible_to<std::vector<uint8_t>>;
    { T::deserialize_binary(std::declval<std::span<const uint8_t>>()) } -> std::convertible_to<T>;
};

template <typename T>
concept MessageType = std::copyable<T> && std::default_initializable<T>;

template <typename F, typename T>
concept MessageHandler = std::invocable<F, T>;

template <typename F, typename T>
concept AsyncMessageHandler = MessageHandler<F, T> && requires(F f, T t) {
    { f(t) } -> std::convertible_to<std::future<void>>;
};

template <typename F, typename T>
concept CoroMessageHandler = MessageHandler<F, T> && requires(F f, T t) {
    { f(t) } -> std::convertible_to<std::coroutine_handle<>>;
};

// **Message Priority Levels**
enum class MessagePriority : uint8_t {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

// **Message Delivery Guarantees**
enum class DeliveryGuarantee {
    AT_MOST_ONCE,   // Fire and forget
    AT_LEAST_ONCE,  // Retry until acknowledged
    EXACTLY_ONCE    // Deduplication + retry
};

// **Compression Types**
enum class CompressionType {
    NONE,
    LZ4,
    ZSTD,
    GZIP
};

// **Enhanced Error Types**
enum class MessageBusError {
    InvalidTopic,
    HandlerNotFound,
    QueueFull,
    SerializationError,
    DeserializationError,
    NetworkError,
    CompressionError,
    DecompressionError,
    AuthenticationError,
    AuthorizationError,
    RateLimitExceeded,
    MessageTooLarge,
    DuplicateMessage,
    MessageExpired,
    ShutdownInProgress,
    InternalError
};

template <typename T>
using Result = std::expected<T, MessageBusError>;

// **Message Statistics**
struct MessageStats {
    std::atomic<uint64_t> messages_sent{0};
    std::atomic<uint64_t> messages_received{0};
    std::atomic<uint64_t> messages_dropped{0};
    std::atomic<uint64_t> serialization_errors{0};
    std::atomic<uint64_t> delivery_failures{0};
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> bytes_received{0};
    std::chrono::steady_clock::time_point start_time{std::chrono::steady_clock::now()};

    void reset() {
        messages_sent = 0;
        messages_received = 0;
        messages_dropped = 0;
        serialization_errors = 0;
        delivery_failures = 0;
        bytes_sent = 0;
        bytes_received = 0;
        start_time = std::chrono::steady_clock::now();
    }
};

// **Enhanced Message Envelope**
template <MessageType T>
struct MessageEnvelope {
    std::string topic;
    T payload;
    std::chrono::system_clock::time_point timestamp;
    std::chrono::system_clock::time_point expiry_time;
    std::string sender_id;
    std::string correlation_id;
    std::string reply_to;
    uint64_t message_id;
    MessagePriority priority;
    DeliveryGuarantee delivery_guarantee;
    CompressionType compression;
    std::unordered_map<std::string, std::string> metadata;
    std::vector<std::string> routing_path;
    uint32_t retry_count;
    size_t payload_size;
    std::string checksum;

    MessageEnvelope(std::string t, T p, std::string s = "",
                   MessagePriority prio = MessagePriority::NORMAL,
                   DeliveryGuarantee guarantee = DeliveryGuarantee::AT_MOST_ONCE)
        : topic(std::move(t)),
          payload(std::move(p)),
          timestamp(std::chrono::system_clock::now()),
          expiry_time(timestamp + std::chrono::hours(24)), // Default 24h expiry
          sender_id(std::move(s)),
          message_id(generate_id()),
          priority(prio),
          delivery_guarantee(guarantee),
          compression(CompressionType::NONE),
          retry_count(0),
          payload_size(0) {
        calculate_checksum();
    }

    bool is_expired() const {
        return std::chrono::system_clock::now() > expiry_time;
    }

    void set_expiry(std::chrono::milliseconds ttl) {
        expiry_time = timestamp + ttl;
    }

    bool verify_checksum() const {
        return checksum == calculate_checksum_internal();
    }

private:
    static std::atomic<uint64_t> id_counter;
    static uint64_t generate_id() { return ++id_counter; }

    void calculate_checksum() {
        checksum = calculate_checksum_internal();
    }

    std::string calculate_checksum_internal() const {
        // Simple checksum implementation (in real code, use proper hash)
        std::hash<std::string> hasher;
        return std::to_string(hasher(topic + sender_id + std::to_string(message_id)));
    }
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

// **Enhanced Configuration**
struct MessageBusConfig {
    // Queue configuration
    size_t max_queue_size = 10000;
    size_t max_priority_queue_size = 1000;
    std::chrono::milliseconds timeout = std::chrono::milliseconds(1000);
    bool drop_oldest = true;
    bool enable_priority_queues = true;

    // Threading configuration
    size_t worker_thread_count = std::thread::hardware_concurrency();
    size_t io_thread_count = 2;
    bool enable_thread_affinity = false;

    // Performance configuration
    size_t batch_size = 100;
    std::chrono::milliseconds batch_timeout = std::chrono::milliseconds(10);
    bool enable_message_batching = true;
    bool enable_compression = false;
    CompressionType default_compression = CompressionType::LZ4;
    size_t compression_threshold = 1024; // Compress messages larger than 1KB

    // Reliability configuration
    bool enable_persistence = false;
    std::string persistence_path = "./msgbus_data";
    std::chrono::seconds message_retention = std::chrono::hours(24);
    uint32_t max_retry_attempts = 3;
    std::chrono::milliseconds retry_delay = std::chrono::milliseconds(100);

    // Network configuration
    bool enable_clustering = false;
    std::vector<std::string> cluster_nodes;
    uint16_t cluster_port = 8080;
    std::chrono::seconds heartbeat_interval = std::chrono::seconds(30);

    // Security configuration
    bool enable_authentication = false;
    bool enable_encryption = false;
    std::string auth_token;

    // Monitoring configuration
    bool enable_metrics = true;
    std::chrono::seconds metrics_interval = std::chrono::seconds(60);
    bool enable_tracing = false;
};

// **Enhanced Coroutine Support**
template <typename T>
struct MessageAwaiter {
    std::string topic;
    MessageFilter<T> filter;
    std::chrono::milliseconds timeout;
    MessagePriority min_priority;

    bool await_ready() const noexcept { return false; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> handle);

    Result<MessageEnvelope<T>> await_resume();

private:
    std::shared_ptr<std::promise<Result<MessageEnvelope<T>>>> promise_;
};

template <typename T>
struct BatchMessageAwaiter {
    std::string topic_pattern;
    size_t batch_size;
    std::chrono::milliseconds timeout;
    MessageFilter<T> filter;

    bool await_ready() const noexcept { return false; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> handle);

    Result<std::vector<MessageEnvelope<T>>> await_resume();

private:
    std::shared_ptr<std::promise<Result<std::vector<MessageEnvelope<T>>>>> promise_;
};

template <typename T>
struct PublishAwaiter {
    MessageEnvelope<T> envelope;
    DeliveryGuarantee guarantee;

    bool await_ready() const noexcept { return guarantee == DeliveryGuarantee::AT_MOST_ONCE; }

    template <typename Promise>
    bool await_suspend(std::coroutine_handle<Promise> handle);

    Result<void> await_resume();

private:
    std::shared_ptr<std::promise<Result<void>>> promise_;
};

}  // namespace msgbus
