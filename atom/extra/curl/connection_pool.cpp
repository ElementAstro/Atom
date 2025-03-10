#include "connection_pool.hpp"

namespace atom::extra::curl {
ConnectionPool::ConnectionPool(size_t max_connections)
    : max_connections_(max_connections) {}

ConnectionPool::~ConnectionPool() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto handle : pool_) {
        curl_easy_cleanup(handle);
    }
}

CURL* ConnectionPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);

    if (!pool_.empty()) {
        CURL* handle = pool_.back();
        pool_.pop_back();
        return handle;
    }

    return curl_easy_init();
}

void ConnectionPool::release(CURL* handle) {
    if (!handle)
        return;

    std::unique_lock<std::mutex> lock(mutex_);

    curl_easy_reset(handle);

    if (pool_.size() < max_connections_) {
        pool_.push_back(handle);
    } else {
        curl_easy_cleanup(handle);
    }
}
}  // namespace atom::extra::curl
