#pragma once

/**
 * @file memory_manager.hpp
 * @brief Advanced memory management with NUMA awareness and cache optimization
 */

#include <atomic>
#include <memory>
#include <vector>
#include <unordered_map>
#include <thread>
#include <spdlog/spdlog.h>
#include "adaptive_spinlock.hpp"
#include "../asio_compatibility.hpp"

#ifdef ATOM_HAS_NUMA
#include <numa.h>
#include <numaif.h>
#endif

namespace atom::extra::asio::concurrency {

/**
 * @brief NUMA-aware memory allocator for optimal cache locality
 */
template<typename T>
class numa_allocator {
private:
    int numa_node_;
    
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    /**
     * @brief Construct NUMA allocator for specific node
     */
    explicit numa_allocator(int numa_node = -1) : numa_node_(numa_node) {
#ifdef ATOM_HAS_NUMA
        if (numa_node_ == -1) {
            numa_node_ = numa_node_of_cpu(sched_getcpu());
        }
#endif
    }

    /**
     * @brief Copy constructor
     */
    template<typename U>
    numa_allocator(const numa_allocator<U>& other) : numa_node_(other.numa_node_) {}

    /**
     * @brief Allocate memory on specific NUMA node
     */
    pointer allocate(size_type n) {
#ifdef ATOM_HAS_NUMA
        void* ptr = numa_alloc_onnode(n * sizeof(T), numa_node_);
        if (!ptr) {
            throw std::bad_alloc();
        }
        spdlog::trace("NUMA allocated {} bytes on node {}", n * sizeof(T), numa_node_);
        return static_cast<pointer>(ptr);
#else
        auto ptr = std::aligned_alloc(cache_line_size, n * sizeof(T));
        if (!ptr) {
            throw std::bad_alloc();
        }
        return static_cast<pointer>(ptr);
#endif
    }

    /**
     * @brief Deallocate NUMA memory
     */
    void deallocate(pointer ptr, size_type n) {
#ifdef ATOM_HAS_NUMA
        numa_free(ptr, n * sizeof(T));
        spdlog::trace("NUMA deallocated {} bytes", n * sizeof(T));
#else
        std::free(ptr);
#endif
    }

    /**
     * @brief Get NUMA node
     */
    int get_numa_node() const noexcept { return numa_node_; }

    /**
     * @brief Equality comparison
     */
    template<typename U>
    bool operator==(const numa_allocator<U>& other) const noexcept {
        return numa_node_ == other.numa_node_;
    }

    template<typename U>
    bool operator!=(const numa_allocator<U>& other) const noexcept {
        return !(*this == other);
    }
};

/**
 * @brief Cache-aligned memory block for optimal performance
 */
template<typename T, std::size_t Alignment = cache_line_size>
class aligned_memory_block {
private:
    alignas(Alignment) T data_;

public:
    template<typename... Args>
    explicit aligned_memory_block(Args&&... args) : data_(std::forward<Args>(args)...) {}

    T& get() noexcept { return data_; }
    const T& get() const noexcept { return data_; }

    T* operator->() noexcept { return &data_; }
    const T* operator->() const noexcept { return &data_; }

    T& operator*() noexcept { return data_; }
    const T& operator*() const noexcept { return data_; }
};

/**
 * @brief High-performance memory pool with NUMA awareness
 */
template<typename T, std::size_t ChunkSize = 1024>
class numa_memory_pool {
private:
    struct chunk {
        alignas(cache_line_size) std::array<T, ChunkSize> data;
        std::atomic<std::size_t> next_free{0};
        std::unique_ptr<chunk> next;
        int numa_node;
        
        explicit chunk(int node) : numa_node(node) {}
    };

    cache_aligned<std::atomic<chunk*>> current_chunk_;
    adaptive_spinlock allocation_lock_;
    int preferred_numa_node_;
    cache_aligned<std::atomic<std::size_t>> total_allocated_{0};
    cache_aligned<std::atomic<std::size_t>> total_chunks_{0};

    /**
     * @brief Allocate new chunk on preferred NUMA node
     */
    std::unique_ptr<chunk> allocate_chunk() {
#ifdef ATOM_HAS_NUMA
        auto chunk_ptr = std::make_unique<chunk>(preferred_numa_node_);
        
        // Bind chunk memory to NUMA node
        if (numa_available() >= 0) {
            unsigned long nodemask = 1UL << preferred_numa_node_;
            mbind(chunk_ptr.get(), sizeof(chunk), MPOL_BIND, &nodemask, 
                  sizeof(nodemask) * 8, MPOL_MF_STRICT);
        }
        
        spdlog::debug("Allocated new memory chunk on NUMA node {}", preferred_numa_node_);
#else
        auto chunk_ptr = std::make_unique<chunk>(-1);
        spdlog::debug("Allocated new memory chunk (no NUMA support)");
#endif
        
        total_chunks_.get().fetch_add(1, std::memory_order_relaxed);
        return chunk_ptr;
    }

public:
    /**
     * @brief Construct NUMA memory pool
     */
    explicit numa_memory_pool(int numa_node = -1) : preferred_numa_node_(numa_node) {
#ifdef ATOM_HAS_NUMA
        if (preferred_numa_node_ == -1) {
            preferred_numa_node_ = numa_node_of_cpu(sched_getcpu());
        }
#endif
        
        auto initial_chunk = allocate_chunk();
        current_chunk_.get().store(initial_chunk.release(), std::memory_order_release);
        
        spdlog::info("NUMA memory pool initialized for type: {}, node: {}", 
                    typeid(T).name(), preferred_numa_node_);
    }

    /**
     * @brief Destructor
     */
    ~numa_memory_pool() {
        auto* chunk_ptr = current_chunk_.get().load(std::memory_order_acquire);
        while (chunk_ptr) {
            auto* next = chunk_ptr->next.release();
            delete chunk_ptr;
            chunk_ptr = next;
        }
        
        auto chunks = total_chunks_.get().load(std::memory_order_relaxed);
        auto allocated = total_allocated_.get().load(std::memory_order_relaxed);
        
        spdlog::info("NUMA memory pool destroyed: {} chunks, {} objects allocated", 
                    chunks, allocated);
    }

    // Non-copyable, non-movable
    numa_memory_pool(const numa_memory_pool&) = delete;
    numa_memory_pool& operator=(const numa_memory_pool&) = delete;
    numa_memory_pool(numa_memory_pool&&) = delete;
    numa_memory_pool& operator=(numa_memory_pool&&) = delete;

    /**
     * @brief Allocate object from pool
     */
    template<typename... Args>
    T* allocate(Args&&... args) {
        auto* chunk_ptr = current_chunk_.get().load(std::memory_order_acquire);
        
        while (chunk_ptr) {
            auto index = chunk_ptr->next_free.fetch_add(1, std::memory_order_acq_rel);
            
            if (index < ChunkSize) {
                // Successfully allocated from this chunk
                auto* obj = new (&chunk_ptr->data[index]) T(std::forward<Args>(args)...);
                total_allocated_.get().fetch_add(1, std::memory_order_relaxed);
                return obj;
            }
            
            // Chunk is full, try to allocate a new one
            adaptive_lock_guard lock(allocation_lock_);
            
            // Check if another thread already allocated a new chunk
            auto* current = current_chunk_.get().load(std::memory_order_acquire);
            if (current != chunk_ptr) {
                chunk_ptr = current;
                continue;
            }
            
            // Allocate new chunk
            auto new_chunk = allocate_chunk();
            auto* new_chunk_ptr = new_chunk.get();
            
            chunk_ptr->next = std::move(new_chunk);
            current_chunk_.get().store(new_chunk_ptr, std::memory_order_release);
            
            chunk_ptr = new_chunk_ptr;
        }
        
        // Should never reach here
        throw std::bad_alloc();
    }

    /**
     * @brief Get pool statistics
     */
    struct pool_stats {
        std::size_t total_allocated;
        std::size_t total_chunks;
        int numa_node;
    };

    pool_stats get_stats() const noexcept {
        return {
            total_allocated_.get().load(std::memory_order_relaxed),
            total_chunks_.get().load(std::memory_order_relaxed),
            preferred_numa_node_
        };
    }
};

/**
 * @brief Global memory manager for optimal allocation strategies
 */
class memory_manager {
private:
    std::unordered_map<std::thread::id, int> thread_numa_mapping_;
    reader_writer_spinlock mapping_lock_;
    
    // Singleton instance
    static std::unique_ptr<memory_manager> instance_;
    static std::once_flag init_flag_;

    memory_manager() {
#ifdef ATOM_HAS_NUMA
        if (numa_available() >= 0) {
            spdlog::info("NUMA support available with {} nodes", numa_max_node() + 1);
        } else {
            spdlog::warn("NUMA support not available");
        }
#else
        spdlog::info("Memory manager initialized without NUMA support");
#endif
    }

public:
    /**
     * @brief Get singleton instance
     */
    static memory_manager& instance() {
        std::call_once(init_flag_, []() {
            instance_ = std::unique_ptr<memory_manager>(new memory_manager());
        });
        return *instance_;
    }

    /**
     * @brief Get optimal NUMA node for current thread
     */
    int get_optimal_numa_node() {
        auto thread_id = std::this_thread::get_id();
        
        // Try read lock first
        {
            shared_lock_guard read_lock(mapping_lock_);
            auto it = thread_numa_mapping_.find(thread_id);
            if (it != thread_numa_mapping_.end()) {
                return it->second;
            }
        }
        
        // Need write lock to create mapping
        adaptive_lock_guard write_lock(mapping_lock_);
        
        // Double-check
        auto it = thread_numa_mapping_.find(thread_id);
        if (it != thread_numa_mapping_.end()) {
            return it->second;
        }
        
#ifdef ATOM_HAS_NUMA
        int numa_node = numa_node_of_cpu(sched_getcpu());
#else
        int numa_node = 0;
#endif
        
        thread_numa_mapping_[thread_id] = numa_node;
        spdlog::debug("Mapped thread to NUMA node {}", numa_node);
        
        return numa_node;
    }

    /**
     * @brief Create NUMA-aware allocator for type T
     */
    template<typename T>
    numa_allocator<T> create_allocator() {
        return numa_allocator<T>(get_optimal_numa_node());
    }

    /**
     * @brief Create NUMA memory pool for type T
     */
    template<typename T, std::size_t ChunkSize = 1024>
    std::unique_ptr<numa_memory_pool<T, ChunkSize>> create_pool() {
        return std::make_unique<numa_memory_pool<T, ChunkSize>>(get_optimal_numa_node());
    }
};



/**
 * @brief Convenience function to get global memory manager
 */
inline memory_manager& get_memory_manager() {
    return memory_manager::instance();
}

} // namespace atom::extra::asio::concurrency
