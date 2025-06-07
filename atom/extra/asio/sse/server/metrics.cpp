#include "metrics.hpp"

namespace atom::extra::asio::sse {

void ServerMetrics::increment_connection_count() {
    ++total_connections_;
    ++current_connections_;
    update_max_concurrent();
}

void ServerMetrics::decrement_connection_count() {
    if (current_connections_ > 0) {
        --current_connections_;
    }
}

void ServerMetrics::increment_event_count() { ++total_events_; }

void ServerMetrics::record_event_size(size_t size_bytes) {
    total_bytes_sent_ += size_bytes;
}

void ServerMetrics::record_auth_failure() { ++auth_failures_; }

void ServerMetrics::record_auth_success() { ++auth_successes_; }

nlohmann::json ServerMetrics::get_metrics() const {
    nlohmann::json metrics;
    metrics["total_connections"] = total_connections_.load();
    metrics["current_connections"] = current_connections_.load();
    metrics["max_concurrent_connections"] = max_concurrent_connections_.load();
    metrics["total_events_sent"] = total_events_.load();
    metrics["total_bytes_sent"] = total_bytes_sent_.load();
    metrics["auth_successes"] = auth_successes_.load();
    metrics["auth_failures"] = auth_failures_.load();
    metrics["uptime_seconds"] =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time_)
            .count();
    return metrics;
}

void ServerMetrics::update_max_concurrent() {
    uint64_t current = current_connections_.load();
    uint64_t max = max_concurrent_connections_.load();
    while (current > max) {
        if (max_concurrent_connections_.compare_exchange_weak(max, current)) {
            break;
        }
    }
}

}  // namespace atom::extra::asio::sse