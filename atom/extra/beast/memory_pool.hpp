#ifndef ATOM_EXTRA_BEAST_MEMORY_POOL_HPP
#define ATOM_EXTRA_BEAST_MEMORY_POOL_HPP

#include "concurrency_primitives.hpp"
#include <atomic>
#include <memory>
#include <vector>
#include <cstdlib>
#include <new>
#include <spdlog/spdlog.h>

namespace atom::beast::memory {

/**
 * @brief NUMA-aware memory allocator with thread-local pools
 */
template<typename T>
class NUMAAwareAllocator {
private:
    static constexpr std::size_t POOL_SIZE = 1024;
    static constexpr std::size_t ALIGNMENT = alignof(std::max_align_t);
    
    struct MemoryBlock {
        alignas(ALIGNMENT) char data[sizeof(T)];
        std::atomic<MemoryBlock*> next{nullptr};
    };
    
    struct ThreadLocalPool {
        std::atomic<MemoryBlock*> free_list{nullptr};
        std::vector<std::unique_ptr<MemoryBlock[]>> chunks;
        std::size_t allocated_count{0};
        
        ThreadLocalPool() {
            allocate_new_chunk();
        }
        
        void allocate_new_chunk() {
            auto chunk = std::make_unique<MemoryBlock[]>(POOL_SIZE);
            
            // Link all blocks in the chunk
            for (std::size_t i = 0; i < POOL_SIZE - 1; ++i) {
                chunk[i].next.store(&chunk[i + 1], std::memory_order_relaxed);
            }
            chunk[POOL_SIZE - 1].next.store(nullptr, std::memory_order_relaxed);
            
            // Add to free list
            auto* old_head = free_list.exchange(&chunk[0], std::memory_order_acq_rel);
            if (old_head) {
                chunk[POOL_SIZE - 1].next.store(old_head, std::memory_order_relaxed);
            }
            
            chunks.push_back(std::move(chunk));
            spdlog::debug("Allocated new memory chunk for thread {}", 
                         std::hash<std::thread::id>{}(std::this_thread::get_id()));
        }
    };
    
    static thread_local ThreadLocalPool pool_;
    
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    
    template<typename U>
    struct rebind {
        using other = NUMAAwareAllocator<U>;
    };
    
    NUMAAwareAllocator() = default;
    
    template<typename U>
    NUMAAwareAllocator(const NUMAAwareAllocator<U>&) noexcept {}
    
    /**
     * @brief Allocates memory for n objects of type T
     */
    [[nodiscard]] T* allocate(std::size_t n) {
        if (n != 1) {
            // Fall back to standard allocation for non-single objects
            return static_cast<T*>(std::aligned_alloc(ALIGNMENT, n * sizeof(T)));
        }
        
        auto* block = pool_.free_list.load(std::memory_order_acquire);
        while (block) {
            auto* next = block->next.load(std::memory_order_relaxed);
            if (pool_.free_list.compare_exchange_weak(block, next,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_acquire)) {
                ++pool_.allocated_count;
                return reinterpret_cast<T*>(block->data);
            }
        }
        
        // No free blocks available, allocate new chunk
        pool_.allocate_new_chunk();
        return allocate(1);
    }
    
    /**
     * @brief Deallocates memory for n objects
     */
    void deallocate(T* ptr, std::size_t n) noexcept {
        if (n != 1 || !ptr) {
            std::free(ptr);
            return;
        }
        
        auto* block = reinterpret_cast<MemoryBlock*>(ptr);
        auto* old_head = pool_.free_list.load(std::memory_order_relaxed);
        
        do {
            block->next.store(old_head, std::memory_order_relaxed);
        } while (!pool_.free_list.compare_exchange_weak(old_head, block,
                                                       std::memory_order_release,
                                                       std::memory_order_relaxed));
        
        --pool_.allocated_count;
    }
    
    /**
     * @brief Constructs an object at the given location
     */
    template<typename... Args>
    void construct(T* ptr, Args&&... args) {
        new(ptr) T(std::forward<Args>(args)...);
    }
    
    /**
     * @brief Destroys an object at the given location
     */
    void destroy(T* ptr) noexcept {
        ptr->~T();
    }
    
    /**
     * @brief Returns the maximum number of objects that can be allocated
     */
    [[nodiscard]] std::size_t max_size() const noexcept {
        return std::numeric_limits<std::size_t>::max() / sizeof(T);
    }
    
    /**
     * @brief Returns allocation statistics for the current thread
     */
    [[nodiscard]] std::size_t allocated_count() const noexcept {
        return pool_.allocated_count;
    }
    
    /**
     * @brief Returns the number of chunks allocated for the current thread
     */
    [[nodiscard]] std::size_t chunk_count() const noexcept {
        return pool_.chunks.size();
    }
};

template<typename T>
thread_local typename NUMAAwareAllocator<T>::ThreadLocalPool NUMAAwareAllocator<T>::pool_;

template<typename T, typename U>
bool operator==(const NUMAAwareAllocator<T>&, const NUMAAwareAllocator<U>&) noexcept {
    return true;
}

template<typename T, typename U>
bool operator!=(const NUMAAwareAllocator<T>&, const NUMAAwareAllocator<U>&) noexcept {
    return false;
}

/**
 * @brief Lock-free object pool for high-frequency allocations
 */
template<typename T, std::size_t PoolSize = 1024>
class LockFreeObjectPool {
private:
    struct PoolNode {
        alignas(T) char storage[sizeof(T)];
        std::atomic<PoolNode*> next{nullptr};
        
        T* get_object() noexcept {
            return reinterpret_cast<T*>(storage);
        }
    };
    
    alignas(concurrency::CACHE_LINE_SIZE) std::atomic<PoolNode*> free_list_{nullptr};
    std::unique_ptr<PoolNode[]> pool_storage_;
    std::atomic<std::size_t> allocated_count_{0};
    std::atomic<std::size_t> total_allocations_{0};
    std::atomic<std::size_t> total_deallocations_{0};
    
public:
    LockFreeObjectPool() : pool_storage_(std::make_unique<PoolNode[]>(PoolSize)) {
        // Initialize free list
        for (std::size_t i = 0; i < PoolSize - 1; ++i) {
            pool_storage_[i].next.store(&pool_storage_[i + 1], std::memory_order_relaxed);
        }
        pool_storage_[PoolSize - 1].next.store(nullptr, std::memory_order_relaxed);
        free_list_.store(&pool_storage_[0], std::memory_order_relaxed);
        
        spdlog::info("Initialized lock-free object pool with {} objects of size {}", 
                    PoolSize, sizeof(T));
    }
    
    /**
     * @brief Acquires an object from the pool
     */
    template<typename... Args>
    [[nodiscard]] T* acquire(Args&&... args) {
        auto* node = free_list_.load(std::memory_order_acquire);
        
        while (node) {
            auto* next = node->next.load(std::memory_order_relaxed);
            if (free_list_.compare_exchange_weak(node, next,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire)) {
                allocated_count_.fetch_add(1, std::memory_order_relaxed);
                total_allocations_.fetch_add(1, std::memory_order_relaxed);
                
                // Construct object in-place
                T* obj = node->get_object();
                new(obj) T(std::forward<Args>(args)...);
                return obj;
            }
        }
        
        // Pool exhausted, fall back to regular allocation
        spdlog::warn("Object pool exhausted, falling back to heap allocation");
        total_allocations_.fetch_add(1, std::memory_order_relaxed);
        return new T(std::forward<Args>(args)...);
    }
    
    /**
     * @brief Returns an object to the pool
     */
    void release(T* obj) noexcept {
        if (!obj) return;
        
        // Check if object belongs to our pool
        auto* pool_start = reinterpret_cast<char*>(pool_storage_.get());
        auto* pool_end = pool_start + PoolSize * sizeof(PoolNode);
        auto* obj_ptr = reinterpret_cast<char*>(obj);
        
        if (obj_ptr >= pool_start && obj_ptr < pool_end) {
            // Object belongs to pool
            obj->~T();
            
            auto* node = reinterpret_cast<PoolNode*>(obj);
            auto* old_head = free_list_.load(std::memory_order_relaxed);
            
            do {
                node->next.store(old_head, std::memory_order_relaxed);
            } while (!free_list_.compare_exchange_weak(old_head, node,
                                                      std::memory_order_release,
                                                      std::memory_order_relaxed));
            
            allocated_count_.fetch_sub(1, std::memory_order_relaxed);
        } else {
            // Object was heap-allocated
            delete obj;
        }
        
        total_deallocations_.fetch_add(1, std::memory_order_relaxed);
    }
    
    /**
     * @brief Returns current allocation statistics
     */
    struct Statistics {
        std::size_t allocated_count;
        std::size_t total_allocations;
        std::size_t total_deallocations;
        double pool_utilization;
    };
    
    [[nodiscard]] Statistics get_statistics() const noexcept {
        auto allocated = allocated_count_.load(std::memory_order_relaxed);
        auto total_alloc = total_allocations_.load(std::memory_order_relaxed);
        auto total_dealloc = total_deallocations_.load(std::memory_order_relaxed);
        
        return Statistics{
            allocated,
            total_alloc,
            total_dealloc,
            static_cast<double>(allocated) / PoolSize * 100.0
        };
    }
    
    /**
     * @brief Checks if the pool is empty
     */
    [[nodiscard]] bool empty() const noexcept {
        return free_list_.load(std::memory_order_acquire) == nullptr;
    }
    
    /**
     * @brief Returns the maximum pool capacity
     */
    [[nodiscard]] constexpr std::size_t capacity() const noexcept {
        return PoolSize;
    }
};

} // namespace atom::beast::memory

#endif // ATOM_EXTRA_BEAST_MEMORY_POOL_HPP
