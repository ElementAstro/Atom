#include "message_bus.hpp"

#include <spdlog/spdlog.h>
#include <any>
#include <queue>
#include <regex>
#include <shared_mutex>
#include <typeindex>

namespace msgbus {

class MessageBus {
public:
    explicit MessageBus(const BackPressureConfig& config = {})
        : config_(config), shutdown_(false), handler_id_counter_(0) {
        // **Initialize libuv loop**
        loop_ = std::make_unique<uv_loop_t>();
        uv_loop_init(loop_.get());

        // **Start event loop thread**
        event_thread_ = std::thread([this]() { run_event_loop(); });

        spdlog::info("MessageBus initialized with max queue size: {}",
                     config_.max_queue_size);
    }

    ~MessageBus() { shutdown(); }

    // **Template-based subscription**
    template <MessageType T, MessageHandler<T> Handler>
    SubscriptionHandle subscribe(const std::string& topic_pattern,
                                 Handler&& handler,
                                 MessageFilter<T> filter = nullptr) {
        auto registration_id = ++handler_id_counter_;

        {
            std::lock_guard<std::shared_mutex> lock(handlers_mutex_);

            auto& topic_handlers =
                handlers_[std::type_index(typeid(T))][topic_pattern];

            auto wrapped_handler = [handler = std::forward<Handler>(handler),
                                    filter](const std::any& envelope_any) {
                try {
                    const auto& envelope =
                        std::any_cast<const MessageEnvelope<T>&>(envelope_any);

                    if (filter && !filter(envelope)) {
                        return;
                    }

                    if constexpr (AsyncMessageHandler<Handler, T>) {
                        auto future = handler(envelope.payload);
                        // Handle async result if needed
                    } else {
                        handler(envelope.payload);
                    }
                } catch (const std::bad_any_cast& e) {
                    spdlog::error("Handler type mismatch: {}", e.what());
                } catch (const std::exception& e) {
                    spdlog::error("Handler execution error: {}", e.what());
                }
            };

            topic_handlers[registration_id] = std::move(wrapped_handler);
        }

        auto cleanup = [this, registration_id, topic_pattern,
                        type_index = std::type_index(typeid(T))]() {
            unsubscribe_internal(type_index, topic_pattern, registration_id);
        };

        spdlog::debug("Subscribed to topic pattern '{}' with handler ID {}",
                      topic_pattern, registration_id);

        return std::make_unique<HandlerRegistration>(
            registration_id, topic_pattern, std::move(cleanup));
    }

    // **Publish message**
    template <MessageType T>
    Result<void> publish(const std::string& topic, T&& message,
                         const std::string& sender_id = "") {
        if (shutdown_.load()) {
            return std::unexpected(MessageBusError::ShutdownInProgress);
        }

        auto envelope = std::make_shared<MessageEnvelope<T>>(
            topic, std::forward<T>(message), sender_id);

        // **Queue message for async processing**
        {
            std::unique_lock<std::mutex> lock(message_queue_mutex_);

            if (message_queue_.size() >= config_.max_queue_size) {
                if (config_.drop_oldest && !message_queue_.empty()) {
                    message_queue_.pop();
                    spdlog::warn(
                        "Dropped oldest message due to queue overflow");
                } else {
                    spdlog::warn("Message queue full, dropping message");
                    return std::unexpected(MessageBusError::QueueFull);
                }
            }

            message_queue_.emplace([this, envelope, topic,
                                    type_index = std::type_index(typeid(T))]() {
                deliver_message(type_index, topic, *envelope);
            });
        }

        // **Signal event loop**
        uv_async_send(&async_handle_);

        spdlog::debug("Published message to topic '{}' with ID {}", topic,
                      envelope->message_id);

        return {};
    }

    // **Coroutine-based message waiting**
    template <MessageType T>
    MessageAwaiter<T> wait_for_message(
        const std::string& topic, MessageFilter<T> filter = nullptr,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
        return MessageAwaiter<T>{topic, std::move(filter), timeout};
    }

    // **Get queue statistics**
    struct QueueStats {
        size_t pending_messages;
        size_t max_queue_size;
        size_t total_handlers;
        std::chrono::milliseconds avg_delivery_time;
    };

    QueueStats get_stats() const {
        std::shared_lock<std::shared_mutex> handlers_lock(handlers_mutex_);
        std::unique_lock<std::mutex> queue_lock(message_queue_mutex_);

        size_t total_handlers = 0;
        for (const auto& [type, topics] : handlers_) {
            for (const auto& [topic, handlers_map] : topics) {
                total_handlers += handlers_map.size();
            }
        }

        return QueueStats{.pending_messages = message_queue_.size(),
                          .max_queue_size = config_.max_queue_size,
                          .total_handlers = total_handlers,
                          .avg_delivery_time = avg_delivery_time_.load()};
    }

    void shutdown() {
        bool expected = false;
        if (!shutdown_.compare_exchange_strong(expected, true)) {
            return;  // Already shutting down
        }

        spdlog::info("Shutting down MessageBus...");

        // **Stop event loop**
        uv_close(reinterpret_cast<uv_handle_t*>(&async_handle_), nullptr);

        if (event_thread_.joinable()) {
            event_thread_.join();
        }

        // **Cleanup libuv**
        uv_loop_close(loop_.get());

        spdlog::info("MessageBus shutdown complete");
    }

    static auto get_instance() {
        static MessageBus instance;
        return &instance;
    }

private:
    // **Internal message delivery**
    template <MessageType T>
    void deliver_message(std::type_index type_index, const std::string& topic,
                         const MessageEnvelope<T>& envelope) {
        auto start_time = std::chrono::steady_clock::now();

        std::shared_lock<std::shared_mutex> lock(handlers_mutex_);

        auto type_it = handlers_.find(type_index);
        if (type_it == handlers_.end()) {
            return;
        }

        size_t delivered_count = 0;

        for (const auto& [pattern, handlers_map] : type_it->second) {
            if (topic_matches_pattern(topic, pattern)) {
                for (const auto& [handler_id, handler] : handlers_map) {
                    try {
                        handler(std::any(envelope));
                        ++delivered_count;
                    } catch (const std::exception& e) {
                        spdlog::error("Handler {} failed: {}", handler_id,
                                      e.what());
                    }
                }
            }
        }

        auto end_time = std::chrono::steady_clock::now();
        auto delivery_time =
            std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                  start_time);

        // **Update average delivery time**
        auto prev_avg = avg_delivery_time_.load();
        while (!avg_delivery_time_.compare_exchange_weak(
            prev_avg, std::chrono::milliseconds(
                          (prev_avg.count() + delivery_time.count()) / 2))) {
            // Retry until successful
        }

        spdlog::debug("Delivered message {} to {} handlers in {}ms",
                      envelope.message_id, delivered_count,
                      delivery_time.count());
    }

    void unsubscribe_internal(std::type_index type_index,
                              const std::string& topic_pattern,
                              uint64_t handler_id) {
        std::lock_guard<std::shared_mutex> lock(handlers_mutex_);

        auto type_it = handlers_.find(type_index);
        if (type_it != handlers_.end()) {
            auto topic_it = type_it->second.find(topic_pattern);
            if (topic_it != type_it->second.end()) {
                topic_it->second.erase(handler_id);

                if (topic_it->second.empty()) {
                    type_it->second.erase(topic_it);
                }

                if (type_it->second.empty()) {
                    handlers_.erase(type_it);
                }
            }
        }

        spdlog::debug("Unsubscribed handler {} from topic pattern '{}'",
                      handler_id, topic_pattern);
    }

    // **Topic pattern matching (supports wildcards)**
    bool topic_matches_pattern(const std::string& topic,
                               const std::string& pattern) const {
        if (pattern == "*" || pattern == topic) {
            return true;
        }

        // **Convert glob pattern to regex**
        std::string regex_pattern = pattern;
        std::ranges::replace(regex_pattern, '*', '.');
        regex_pattern =
            std::regex_replace(regex_pattern, std::regex("\\."), ".*");

        try {
            std::regex re(regex_pattern);
            return std::regex_match(topic, re);
        } catch (const std::regex_error&) {
            return false;
        }
    }

    // **Event loop management**
    void run_event_loop() {
        // **Initialize async handle**
        uv_async_init(loop_.get(), &async_handle_, [](uv_async_t* handle) {
            auto* bus = static_cast<MessageBus*>(handle->data);
            bus->process_message_queue();
        });

        async_handle_.data = this;

        spdlog::debug("Starting event loop");

        while (!shutdown_.load()) {
            uv_run(loop_.get(), UV_RUN_DEFAULT);

            if (shutdown_.load()) {
                break;
            }

            // **Brief pause to prevent busy waiting**
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        spdlog::debug("Event loop stopped");
    }

    void process_message_queue() {
        std::queue<std::function<void()>> local_queue;

        {
            std::unique_lock<std::mutex> lock(message_queue_mutex_);
            local_queue.swap(message_queue_);
        }

        while (!local_queue.empty() && !shutdown_.load()) {
            try {
                local_queue.front()();
                local_queue.pop();
            } catch (const std::exception& e) {
                spdlog::error("Message processing error: {}", e.what());
                local_queue.pop();
            }
        }
    }

    using HandlerMap =
        std::unordered_map<uint64_t, std::function<void(const std::any&)>>;
    using TopicHandlers = std::unordered_map<std::string, HandlerMap>;
    using TypeHandlers = std::unordered_map<std::type_index, TopicHandlers>;

    BackPressureConfig config_;
    std::atomic<bool> shutdown_;
    std::atomic<uint64_t> handler_id_counter_;
    std::atomic<std::chrono::milliseconds> avg_delivery_time_{
        std::chrono::milliseconds(0)};

    mutable std::shared_mutex handlers_mutex_;
    TypeHandlers handlers_;

    mutable std::mutex message_queue_mutex_;
    std::queue<std::function<void()>> message_queue_;

    std::unique_ptr<uv_loop_t> loop_;
    uv_async_t async_handle_;
    std::thread event_thread_;
};

// **Coroutine implementation**
template <typename T>
template <typename Promise>
bool MessageAwaiter<T>::await_suspend(std::coroutine_handle<Promise> handle) {
    promise_ = std::make_shared<std::promise<Result<MessageEnvelope<T>>>>();

    // **Set up temporary subscription**
    auto bus = MessageBus::get_instance();
    auto subscription = bus->subscribe<T>(
        topic,
        [promise = promise_, this](const T& msg) {
            // **Handle message reception**
            MessageEnvelope<T> envelope(topic, msg);
            if (!filter || filter(envelope)) {
                promise->set_value(std::move(envelope));
            }
        },
        filter);

    // **Set up timeout**
    std::thread([promise = promise_, timeout = timeout, handle]() {
        std::this_thread::sleep_for(timeout);
        promise->set_value(std::unexpected(MessageBusError::NetworkError));
        handle.resume();
    }).detach();

    return true;
}

template <typename T>
Result<MessageEnvelope<T>> MessageAwaiter<T>::await_resume() {
    auto future = promise_->get_future();
    return future.get();
}

}  // namespace msgbus