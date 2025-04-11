/*
 * atom/memory/memory_pool.hpp
 *
 * Copyright (C) 2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-4-11

Description: 高性能内存池实现

**************************************************/

#pragma once

#include "../macro.hpp"
#include <memory>
#include <vector>
#include <mutex>
#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>

namespace atom {
namespace memory {

/**
 * @brief 高性能固定大小块内存池
 * 
 * 这个内存池专门用于高效分配和释放固定大小的内存块。
 * 适用于频繁创建和销毁大量小对象的场景，减少内存碎片和系统调用开销。
 * 
 * @tparam BlockSize 每个内存块的大小（字节）
 * @tparam BlocksPerChunk 每个内存块集合包含的块数量
 */
template <std::size_t BlockSize = 64, std::size_t BlocksPerChunk = 1024>
class MemoryPool {
private:
    // 内存块的链表节点结构
    struct Block {
        // 指向下一个可用块的指针
        Block* next;
    };
    
    // 内存块集合（chunk）
    struct Chunk {
        // 一整块连续的内存，用于分配为多个小块
        std::array<std::byte, BlockSize * BlocksPerChunk> memory;
        
        // 构造函数，初始化内存块链表
        Chunk() {
            static_assert(BlockSize >= sizeof(Block), "Block size too small");
        }
    };
    
    // 指向可用块链表的头指针
    Block* free_list_ = nullptr;
    
    // 存储所有分配的内存块集合
    std::vector<std::unique_ptr<Chunk>> chunks_;
    
    // 用于线程安全操作的互斥锁
    std::mutex mutex_;
    
    // 已分配但未释放的块数量
    std::size_t allocated_blocks_ = 0;
    
    // 总块数
    std::size_t total_blocks_ = 0;

    // 在现有块全部用完时分配新的内存块集合
    void allocate_new_chunk() {
        auto chunk = std::make_unique<Chunk>();
        
        // 将新块集合中的所有块连接到空闲链表
        for (std::size_t i = 0; i < BlocksPerChunk; ++i) {
            auto* block = reinterpret_cast<Block*>(
                &chunk->memory[i * BlockSize]
            );
            block->next = free_list_;
            free_list_ = block;
        }
        
        // 添加到块集合列表并更新统计信息
        chunks_.push_back(std::move(chunk));
        total_blocks_ += BlocksPerChunk;
    }

public:
    // 静态断言以确保块大小足够容纳Block结构和满足对齐要求
    static_assert(BlockSize >= sizeof(Block), 
        "Block size must be at least sizeof(Block)");
    static_assert(BlockSize % alignof(std::max_align_t) == 0, 
        "Block size must be a multiple of std::max_align_t alignment");

    /**
     * @brief 默认构造函数
     */
    MemoryPool() = default;
    
    /**
     * @brief 析构函数，释放所有内存
     */
    ~MemoryPool() = default;
    
    // 禁止复制
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    
    // 允许移动
    MemoryPool(MemoryPool&&) noexcept = default;
    MemoryPool& operator=(MemoryPool&&) noexcept = default;
    
    /**
     * @brief 分配一个内存块
     * 
     * @return void* 指向新分配内存块的指针
     */
    ATOM_NODISCARD void* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (free_list_ == nullptr) {
            // 如果没有可用块，分配新的块集合
            allocate_new_chunk();
        }
        
        // 从空闲链表中获取一个块
        Block* block = free_list_;
        free_list_ = block->next;
        ++allocated_blocks_;
        
        return static_cast<void*>(block);
    }
    
    /**
     * @brief 释放一个之前分配的内存块
     * 
     * @param ptr 要释放的内存块指针
     */
    void deallocate(void* ptr) ATOM_NOEXCEPT {
        if (!ptr) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 将块放回空闲链表
        Block* block = static_cast<Block*>(ptr);
        block->next = free_list_;
        free_list_ = block;
        
        --allocated_blocks_;
    }
    
    /**
     * @brief 获取内存池的统计信息
     * 
     * @return std::pair<std::size_t, std::size_t> 包含已分配块数和总块数的配对
     */
    std::pair<std::size_t, std::size_t> get_stats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return {allocated_blocks_, total_blocks_};
    }
    
    /**
     * @brief 检查内存池是否为空（没有已分配块）
     * 
     * @return bool 如果没有已分配块则返回true
     */
    bool is_empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return allocated_blocks_ == 0;
    }
    
    /**
     * @brief 重置内存池，将所有块标记为可用
     * 
     * 注意：这不会释放已分配的内存，只是重置内部状态
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 重建自由块链表
        free_list_ = nullptr;
        for (auto& chunk : chunks_) {
            for (std::size_t i = 0; i < BlocksPerChunk; ++i) {
                auto* block = reinterpret_cast<Block*>(
                    &chunk->memory[i * BlockSize]
                );
                block->next = free_list_;
                free_list_ = block;
            }
        }
        
        // 更新统计信息
        allocated_blocks_ = 0;
    }
};

/**
 * @brief 通用对象池，基于内存池实现
 * 
 * 能够高效地分配和回收特定类型的对象，避免了频繁的内存分配和释放操作。
 * 
 * @tparam T 池中对象的类型
 * @tparam BlocksPerChunk 每个内存块集合包含的对象数量
 */
template <typename T, std::size_t BlocksPerChunk = 1024>
class ObjectPool {
private:
    // 计算每个对象需要的内存块大小（考虑对齐）
    static constexpr std::size_t block_size = 
        ((sizeof(T) + alignof(std::max_align_t) - 1) / alignof(std::max_align_t)) 
        * alignof(std::max_align_t);
    
    // 内部使用的内存池
    MemoryPool<block_size, BlocksPerChunk> memory_pool_;
    
public:
    /**
     * @brief 默认构造函数
     */
    ObjectPool() = default;
    
    /**
     * @brief 析构函数
     */
    ~ObjectPool() = default;
    
    // 禁止复制
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    
    // 允许移动
    ObjectPool(ObjectPool&&) noexcept = default;
    ObjectPool& operator=(ObjectPool&&) noexcept = default;
    
    /**
     * @brief 分配并构造一个对象
     * 
     * @tparam Args 构造函数参数类型
     * @param args 传递给对象构造函数的参数
     * @return T* 指向新创建对象的指针
     */
    template <typename... Args>
    ATOM_NODISCARD T* allocate(Args&&... args) {
        // 分配内存
        void* memory = memory_pool_.allocate();
        
        try {
            // 在分配的内存上构造对象
            return new (memory) T(std::forward<Args>(args)...);
        } catch (...) {
            // 如果构造失败，释放内存并重新抛出异常
            memory_pool_.deallocate(memory);
            throw;
        }
    }
    
    /**
     * @brief 析构并释放一个对象
     * 
     * @param ptr 要释放的对象指针
     */
    void deallocate(T* ptr) ATOM_NOEXCEPT {
        if (!ptr) return;
        
        // 调用对象的析构函数
        ptr->~T();
        
        // 释放内存
        memory_pool_.deallocate(static_cast<void*>(ptr));
    }
    
    /**
     * @brief 获取对象池的统计信息
     * 
     * @return std::pair<std::size_t, std::size_t> 包含已分配对象数和总容量的配对
     */
    std::pair<std::size_t, std::size_t> get_stats() const {
        return memory_pool_.get_stats();
    }
    
    /**
     * @brief 检查对象池是否为空（没有已分配对象）
     * 
     * @return bool 如果没有已分配对象则返回true
     */
    bool is_empty() const {
        return memory_pool_.is_empty();
    }
    
    /**
     * @brief 重置对象池
     * 
     * 警告：这将使所有已分配但未释放的对象指针无效！
     */
    void reset() {
        memory_pool_.reset();
    }
};

/**
 * @brief 使用对象池的智能指针
 * 
 * 类似于std::unique_ptr，但使用对象池进行内存管理，
 * 提供自动资源管理和性能优化。
 * 
 * @tparam T 指针所管理的对象类型
 */
template <typename T>
class PoolPtr {
private:
    T* ptr_ = nullptr;
    ObjectPool<T>* pool_ = nullptr;

public:
    /**
     * @brief 默认构造函数
     */
    PoolPtr() noexcept = default;
    
    /**
     * @brief 从对象指针和对象池构造
     * 
     * @param ptr 对象指针
     * @param pool 对象池指针
     */
    explicit PoolPtr(T* ptr, ObjectPool<T>* pool) noexcept
        : ptr_(ptr), pool_(pool) {}
    
    /**
     * @brief 析构函数，释放管理的对象
     */
    ~PoolPtr() {
        reset();
    }
    
    // 禁止复制
    PoolPtr(const PoolPtr&) = delete;
    PoolPtr& operator=(const PoolPtr&) = delete;
    
    /**
     * @brief 移动构造函数
     */
    PoolPtr(PoolPtr&& other) noexcept 
        : ptr_(other.ptr_), pool_(other.pool_) {
        other.ptr_ = nullptr;
        other.pool_ = nullptr;
    }
    
    /**
     * @brief 移动赋值运算符
     */
    PoolPtr& operator=(PoolPtr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            pool_ = other.pool_;
            other.ptr_ = nullptr;
            other.pool_ = nullptr;
        }
        return *this;
    }
    
    /**
     * @brief 重置指针，释放当前管理的对象
     * 
     * @param ptr 新的对象指针，默认为nullptr
     * @param pool 新的对象池指针，默认为nullptr
     */
    void reset(T* ptr = nullptr, ObjectPool<T>* pool = nullptr) noexcept {
        if (ptr_ && pool_) {
            pool_->deallocate(ptr_);
        }
        ptr_ = ptr;
        pool_ = pool;
    }
    
    /**
     * @brief 返回指针，并放弃所有权
     * 
     * @return T* 管理的对象指针
     */
    T* release() noexcept {
        T* ptr = ptr_;
        ptr_ = nullptr;
        pool_ = nullptr;
        return ptr;
    }
    
    /**
     * @brief 获取管理的对象指针
     * 
     * @return T* 指向管理的对象的指针
     */
    T* get() const noexcept {
        return ptr_;
    }
    
    /**
     * @brief 解引用运算符
     * 
     * @return T& 引用管理的对象
     */
    T& operator*() const {
        assert(ptr_ != nullptr);
        return *ptr_;
    }
    
    /**
     * @brief 成员访问运算符
     * 
     * @return T* 指向管理的对象的指针
     */
    T* operator->() const noexcept {
        assert(ptr_ != nullptr);
        return ptr_;
    }
    
    /**
     * @brief 布尔转换运算符
     * 
     * @return true 如果管理有效对象
     * @return false 如果不管理对象
     */
    explicit operator bool() const noexcept {
        return ptr_ != nullptr;
    }
    
    /**
     * @brief 交换两个PoolPtr的内容
     * 
     * @param other 要交换的另一个PoolPtr
     */
    void swap(PoolPtr& other) noexcept {
        std::swap(ptr_, other.ptr_);
        std::swap(pool_, other.pool_);
    }
};

/**
 * @brief 从对象池创建PoolPtr的辅助函数
 * 
 * @tparam T 对象类型
 * @tparam Args 构造函数参数类型
 * @param pool 对象池引用
 * @param args 传递给构造函数的参数
 * @return PoolPtr<T> 管理新创建对象的智能指针
 */
template <typename T, typename... Args>
PoolPtr<T> make_pool_ptr(ObjectPool<T>& pool, Args&&... args) {
    return PoolPtr<T>(pool.allocate(std::forward<Args>(args)...), &pool);
}

} // namespace memory
} // namespace atom