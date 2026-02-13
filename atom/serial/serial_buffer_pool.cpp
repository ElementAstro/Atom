#include "serial_buffer_pool.hpp"
#include <algorithm>

namespace serial {

std::vector<uint8_t> SerialBufferPool::acquire(size_t size) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Look for a suitable buffer in the pool
    for (auto it = pool_.begin(); it != pool_.end(); ++it) {
        if (it->capacity() >= size) {
            auto buffer = std::move(*it);
            pool_.erase(it);
            buffer.clear();
            buffer.reserve(size);
            return buffer;
        }
    }

    // Create new buffer if none suitable found
    std::vector<uint8_t> buffer;
    buffer.reserve(std::max(size, MIN_BUFFER_SIZE));
    return buffer;
}

void SerialBufferPool::release(std::vector<uint8_t> buffer) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (pool_.size() < MAX_POOL_SIZE && buffer.capacity() >= MIN_BUFFER_SIZE) {
        buffer.clear();
        buffer.shrink_to_fit(); // Ensure consistent memory usage
        pool_.push_back(std::move(buffer));
    }
}

void SerialBufferPool::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    pool_.clear();
}

std::pair<size_t, size_t> SerialBufferPool::getStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t totalCapacity = 0;
    for (const auto& buffer : pool_) {
        totalCapacity += buffer.capacity();
    }
    return {pool_.size(), totalCapacity};
}

// Static member definitions
std::vector<std::vector<uint8_t>> SerialBufferPool::pool_;
std::mutex SerialBufferPool::mutex_;

}  // namespace serial
