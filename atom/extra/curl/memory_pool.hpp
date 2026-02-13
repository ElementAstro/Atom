#ifndef ATOM_EXTRA_CURL_MEMORY_POOL_HPP
#define ATOM_EXTRA_CURL_MEMORY_POOL_HPP

#include "atom/memory/memory_pool.hpp"
#include <memory>

namespace atom::extra::curl {

// Use atom::memory::ObjectPool directly for object management
template<typename T, size_t BlocksPerChunk = 1024>
using MemoryPool = atom::memory::ObjectPool<T, BlocksPerChunk>;

// Provide compatibility aliases for configuration
namespace MemoryPoolConfig {
    template<typename T>
    inline std::unique_ptr<MemoryPool<T>> createDefault() {
        return std::make_unique<MemoryPool<T>>();
    }

    template<typename T>
    inline std::unique_ptr<MemoryPool<T, 2048>> createHighThroughput() {
        return std::make_unique<MemoryPool<T, 2048>>();  // More objects per chunk
    }

    template<typename T>
    inline std::unique_ptr<MemoryPool<T, 512>> createLowMemory() {
        return std::make_unique<MemoryPool<T, 512>>();  // Fewer objects per chunk
    }
}

// Global memory pools for common curl types
namespace pools {
    extern MemoryPool<std::vector<char>, 2048> response_buffer_pool;
    extern MemoryPool<std::string> string_pool;
}

}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_MEMORY_POOL_HPP
