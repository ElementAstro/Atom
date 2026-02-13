#include "message_bus.hpp"

#include <spdlog/spdlog.h>
#include <any>
#include <functional>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <typeindex>
#include <unordered_map>

namespace msgbus {

// Implementation struct for PIMPL
struct MessageBus::Impl {
    using HandlerMap =
        std::unordered_map<uint64_t, std::function<void(const std::any&)>>;
    using TopicHandlers = std::unordered_map<std::string, HandlerMap>;
    using TypeHandlers = std::unordered_map<std::type_index, TopicHandlers>;

    mutable std::shared_mutex handlers_mutex;
    TypeHandlers handlers;

    mutable std::mutex message_queue_mutex;
    std::queue<std::function<void()>> message_queue;

    std::atomic<std::chrono::milliseconds> avg_delivery_time{
        std::chrono::milliseconds(0)};
};

// MessageBus constructor
MessageBus::MessageBus(const BackPressureConfig& config)
    : config_(config),
      shutdown_(false),
      handler_id_counter_(0),
      pimpl_(std::make_unique<Impl>()) {
    spdlog::info("MessageBus initialized with max queue size: {}",
                 config_.max_queue_size);
}
MessageBus::~MessageBus() { shutdown(); }

void MessageBus::shutdown() {
    bool expected = false;
    if (!shutdown_.compare_exchange_strong(expected, true)) {
        return;  // Already shutting down
    }
    spdlog::info("MessageBus shutdown complete");
}

void MessageBus::process_messages() {
    std::unique_lock<std::mutex> queue_lock(pimpl_->message_queue_mutex);
    auto start_time = std::chrono::steady_clock::now();

    while (!pimpl_->message_queue.empty()) {
        auto message_func = std::move(pimpl_->message_queue.front());
        pimpl_->message_queue.pop();

        // Release lock while processing to allow other threads to publish
        queue_lock.unlock();

        try {
            message_func();
        } catch (const std::exception& e) {
            spdlog::error("Error processing message: {}", e.what());
        }

        queue_lock.lock();
    }

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    // Update average delivery time (simple exponential moving average)
    auto current_avg = pimpl_->avg_delivery_time.load();
    auto new_avg =
        std::chrono::milliseconds((current_avg.count() * 9 + duration.count()) /
                                  10  // 0.9 weight on old average
        );
    pimpl_->avg_delivery_time.store(new_avg);
}

MessageBus* MessageBus::get_instance() {
    static MessageBus instance;
    return &instance;
}

MessageBus::QueueStats MessageBus::get_stats() const {
    std::shared_lock<std::shared_mutex> handlers_lock(pimpl_->handlers_mutex);
    std::unique_lock<std::mutex> queue_lock(pimpl_->message_queue_mutex);

    size_t total_handlers = 0;
    for (const auto& [type, topics] : pimpl_->handlers) {
        for (const auto& [topic, handlers_map] : topics) {
            total_handlers += handlers_map.size();
        }
    }

    return QueueStats{.pending_messages = pimpl_->message_queue.size(),
                      .max_queue_size = config_.max_queue_size,
                      .total_handlers = total_handlers,
                      .avg_delivery_time = pimpl_->avg_delivery_time.load()};
}

}  // namespace msgbus
