#pragma once

#include <vector>
#include <mutex>
#include <cstdint>

namespace serial {

/**
 * @brief Cross-platform buffer pool for efficient memory management in serial operations
 *
 * This buffer pool is designed to work consistently across all platforms,
 * reducing memory allocations and improving performance for serial I/O operations.
 */
class SerialBufferPool {
public:
    /**
     * @brief Acquire a buffer from the pool or create a new one
     * @param size Minimum size required for the buffer
     * @return A vector with at least the requested capacity
     */
    static std::vector<uint8_t> acquire(size_t size);

    /**
     * @brief Return a buffer to the pool for reuse
     * @param buffer Buffer to return to the pool
     */
    static void release(std::vector<uint8_t> buffer);

    /**
     * @brief Clear all buffers from the pool (useful for cleanup)
     */
    static void clear();

    /**
     * @brief Get current pool statistics
     * @return Pair of (pool_size, total_capacity)
     */
    static std::pair<size_t, size_t> getStats();

private:
    static std::vector<std::vector<uint8_t>> pool_;
    static std::mutex mutex_;
    static constexpr size_t MAX_POOL_SIZE = 16; // Increased for better performance
    static constexpr size_t MIN_BUFFER_SIZE = 256;
};

}  // namespace serial
