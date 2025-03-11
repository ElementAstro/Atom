#ifndef ATOM_MEMORY_SHORT_ALLOC_HPP
#define ATOM_MEMORY_SHORT_ALLOC_HPP

#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <type_traits>

// 跨平台支持
#if defined(_WIN32) || defined(_WIN64)
#define ATOM_PLATFORM_WINDOWS
#elif defined(__APPLE__)
#define ATOM_PLATFORM_APPLE
#elif defined(__linux__)
#define ATOM_PLATFORM_LINUX
#elif defined(__unix__)
#define ATOM_PLATFORM_UNIX
#endif

// 线程支持
#ifdef ATOM_USE_BOOST
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#else
#include <mutex>
#include <shared_mutex>
#endif

#include "atom/macro.hpp"

// 确定是否启用内存追踪
#if !defined(ATOM_MEMORY_STATS_ENABLED)
#if defined(ATOM_DEBUG) || defined(_DEBUG) || defined(DEBUG)
#define ATOM_MEMORY_STATS_ENABLED 1
#else
#define ATOM_MEMORY_STATS_ENABLED 0
#endif
#endif

// 确定是否启用内存验证
#if !defined(ATOM_MEMORY_VALIDATION_ENABLED)
#if defined(ATOM_DEBUG) || defined(_DEBUG) || defined(DEBUG)
#define ATOM_MEMORY_VALIDATION_ENABLED 1
#else
#define ATOM_MEMORY_VALIDATION_ENABLED 0
#endif
#endif

namespace atom::memory {

// 内存工具函数
namespace utils {
// 获取对齐的指针
inline void* alignPointer(void* ptr, std::size_t alignment,
                          std::size_t& space) {
    std::uintptr_t intPtr = reinterpret_cast<std::uintptr_t>(ptr);
    std::uintptr_t aligned = (intPtr + alignment - 1) & ~(alignment - 1);
    std::size_t padding = aligned - intPtr;

    if (space < padding) {
        return nullptr;
    }

    space -= padding;
    return reinterpret_cast<void*>(aligned);
}

// 生成填充内存的模式 (调试用)
inline uint8_t getAllocationPattern() { return 0xAB; }
inline uint8_t getFreedPattern() { return 0xFE; }

// 填充内存区域
inline void fillMemory(void* ptr, size_t size, uint8_t pattern) {
    std::memset(ptr, pattern, size);
}

// 检查内存边界
constexpr size_t MEMORY_CANARY = 0xDEADBEEF;

// 边界检查结构
struct BoundaryCheck {
    size_t startCanary;
    size_t endCanaryOffset;

    static void initialize(void* memory, size_t size) {
        auto* check = static_cast<BoundaryCheck*>(memory);
        check->startCanary = MEMORY_CANARY;
        check->endCanaryOffset = size - sizeof(size_t);

        // 设置结束标记
        size_t* endMarker = reinterpret_cast<size_t*>(
            static_cast<char*>(memory) + check->endCanaryOffset);
        *endMarker = MEMORY_CANARY;
    }

    static bool validate(void* memory) {
        auto* check = static_cast<BoundaryCheck*>(memory);

        if (check->startCanary != MEMORY_CANARY) {
            return false;
        }

        size_t* endMarker = reinterpret_cast<size_t*>(
            static_cast<char*>(memory) + check->endCanaryOffset);

        return *endMarker == MEMORY_CANARY;
    }
};
}  // namespace utils

// 内存统计收集器
class MemoryStats {
public:
    struct ArenaStats {
        std::atomic<size_t> totalAllocations{0};
        std::atomic<size_t> currentAllocations{0};
        std::atomic<size_t> totalBytesAllocated{0};
        std::atomic<size_t> peakBytesAllocated{0};
        std::atomic<size_t> currentBytesAllocated{0};
        std::atomic<size_t> failedAllocations{0};

        void recordAllocation(size_t bytes) {
            totalAllocations++;
            currentAllocations++;
            totalBytesAllocated += bytes;
            currentBytesAllocated += bytes;

            // 更新峰值
            size_t current = currentBytesAllocated.load();
            size_t peak = peakBytesAllocated.load();
            while (current > peak &&
                   !peakBytesAllocated.compare_exchange_weak(peak, current)) {
                // 循环直到成功或current不再大于peak
            }
        }

        void recordDeallocation(size_t bytes) {
            if (currentAllocations > 0) {
                currentAllocations--;
            }

            if (currentBytesAllocated >= bytes) {
                currentBytesAllocated -= bytes;
            }
        }

        void recordFailedAllocation() { failedAllocations++; }

        std::string getReport() const {
            std::stringstream ss;
            ss << "Arena Statistics:\n"
               << "  Total Allocations: " << totalAllocations << "\n"
               << "  Current Allocations: " << currentAllocations << "\n"
               << "  Total Bytes Allocated: " << totalBytesAllocated << "\n"
               << "  Peak Memory Usage: " << peakBytesAllocated << " bytes\n"
               << "  Current Memory Usage: " << currentBytesAllocated
               << " bytes\n"
               << "  Failed Allocations: " << failedAllocations;
            return ss.str();
        }

        void reset() {
            totalAllocations = 0;
            currentAllocations = 0;
            totalBytesAllocated = 0;
            peakBytesAllocated = 0;
            currentBytesAllocated = 0;
            failedAllocations = 0;
        }
    };

    static ArenaStats& getStats() {
        static ArenaStats stats;
        return stats;
    }
};

/**
 * @brief 分配策略枚举
 */
enum class AllocationStrategy {
    FirstFit,  // 第一个适合的空闲块
    BestFit,   // 最合适大小的空闲块
    WorstFit   // 最大的空闲块
};

/**
 * @brief 增强版固定大小内存区域，用于为指定对齐的对象分配内存
 *
 * 此类提供多种分配策略、统计信息、调试支持以及线程安全分配
 *
 * @tparam N 内存区域大小，以字节为单位
 * @tparam alignment 内存分配的对齐要求，默认为 alignof(std::max_align_t)
 * @tparam ThreadSafe 是否启用线程安全特性
 * @tparam Strategy 使用的内存分配策略
 */
template <std::size_t N, std::size_t alignment = alignof(std::max_align_t),
          bool ThreadSafe = true,
          AllocationStrategy Strategy = AllocationStrategy::FirstFit>
class Arena {
public:
// 决定使用哪种互斥类型
#ifdef ATOM_USE_BOOST
    using MutexType = boost::mutex;
    using SharedMutexType = boost::shared_mutex;
    using LockGuard = boost::lock_guard<MutexType>;
    using ReadLockGuard = boost::shared_lock<SharedMutexType>;
    using WriteLockGuard = boost::unique_lock<SharedMutexType>;
#else
    using MutexType = std::mutex;
    using SharedMutexType = std::shared_mutex;
    using LockGuard = std::lock_guard<MutexType>;
    using ReadLockGuard = std::shared_lock<SharedMutexType>;
    using WriteLockGuard = std::unique_lock<SharedMutexType>;
#endif

private:
    // 内存块标头
    struct Block {
        std::size_t size;      // 块的大小（不包括标头）
        bool used;             // 是否已被分配
        std::size_t offset;    // 从区域开始的偏移量
        std::size_t checksum;  // 用于验证的校验和

        // 计算校验和以检测损坏
        std::size_t calculateChecksum() const {
            return (size ^ offset) + 0x12345678;
        }

        void updateChecksum() { checksum = calculateChecksum(); }

        bool isValid() const { return checksum == calculateChecksum(); }
    };

    // 空闲块链表节点
    struct FreeBlock {
        Block block;
        FreeBlock* next;
    };

    alignas(alignment) std::array<char, N> buffer_{};  // 主内存缓冲区
    char* start_;                                      // 缓冲区起始指针
    char* end_;                                        // 缓冲区结束指针
    Block* firstBlock_;                                // 第一个块指针
    FreeBlock* freeList_;                              // 空闲块链表

    mutable std::conditional_t<ThreadSafe, SharedMutexType, std::nullptr_t>
        mutex_;

#if ATOM_MEMORY_STATS_ENABLED
    MemoryStats::ArenaStats stats_;
#endif

    bool isInitialized_{false};

public:
    Arena() ATOM_NOEXCEPT { initialize(); }

    ~Arena() {
        if constexpr (ThreadSafe) {
            WriteLockGuard lock(mutex_);
            cleanup();
        } else {
            cleanup();
        }
    }

    Arena(const Arena&) = delete;
    auto operator=(const Arena&) -> Arena& = delete;

    /**
     * @brief 初始化内存区域
     */
    void initialize() ATOM_NOEXCEPT {
        if constexpr (ThreadSafe) {
            WriteLockGuard lock(mutex_);
            initializeInternal();
        } else {
            initializeInternal();
        }
    }

    /**
     * @brief 从区域分配内存
     *
     * @param size 要分配的字节数
     * @return void* 指向已分配内存的指针
     * @throw std::bad_alloc 如果没有足够的内存满足请求
     */
    auto allocate(std::size_t size) -> void* {
        if (size == 0)
            return nullptr;

        const std::size_t alignedSize = alignSize(size);

        if constexpr (ThreadSafe) {
            WriteLockGuard lock(mutex_);
            return allocateInternal(alignedSize);
        } else {
            return allocateInternal(alignedSize);
        }
    }

    /**
     * @brief 将内存归还给区域
     *
     * @param p 要释放的内存指针
     */
    void deallocate(void* p) ATOM_NOEXCEPT {
        if (p == nullptr)
            return;

        if constexpr (ThreadSafe) {
            WriteLockGuard lock(mutex_);
            deallocateInternal(p);
        } else {
            deallocateInternal(p);
        }
    }

    /**
     * @brief 释放内存，支持标准接口兼容
     *
     * @param p 要释放的内存指针
     * @param n 分配时的大小（忽略）
     */
    void deallocate(void* p, std::size_t) ATOM_NOEXCEPT { deallocate(p); }

    /**
     * @brief 获取区域总大小
     *
     * @return constexpr std::size_t 区域大小（字节）
     */
    static ATOM_CONSTEXPR auto size() ATOM_NOEXCEPT -> std::size_t { return N; }

    /**
     * @brief 获取区域已使用内存
     *
     * @return std::size_t 已使用字节数
     */
    ATOM_NODISCARD auto used() const ATOM_NOEXCEPT -> std::size_t {
        if constexpr (ThreadSafe) {
            ReadLockGuard lock(mutex_);
            return usedInternal();
        } else {
            return usedInternal();
        }
    }

    /**
     * @brief 获取区域剩余内存
     *
     * @return std::size_t 剩余字节数
     */
    ATOM_NODISCARD auto remaining() const ATOM_NOEXCEPT -> std::size_t {
        if constexpr (ThreadSafe) {
            ReadLockGuard lock(mutex_);
            return remainingInternal();
        } else {
            return remainingInternal();
        }
    }

    /**
     * @brief 重置区域到初始状态
     */
    void reset() ATOM_NOEXCEPT {
        if constexpr (ThreadSafe) {
            WriteLockGuard lock(mutex_);
            resetInternal();
        } else {
            resetInternal();
        }
    }

    /**
     * @brief 获取内存使用统计信息
     *
     * @return std::string 统计报告
     */
    std::string getStats() const {
#if ATOM_MEMORY_STATS_ENABLED
        if constexpr (ThreadSafe) {
            ReadLockGuard lock(mutex_);
            return stats_.getReport();
        } else {
            return stats_.getReport();
        }
#else
        return "Memory statistics disabled. Define ATOM_MEMORY_STATS_ENABLED "
               "to enable.";
#endif
    }

    /**
     * @brief 尝试整理碎片
     *
     * @return size_t 合并的空闲块数量
     */
    size_t defragment() {
        if constexpr (ThreadSafe) {
            WriteLockGuard lock(mutex_);
            return defragmentInternal();
        } else {
            return defragmentInternal();
        }
    }

    /**
     * @brief 验证内存区域的完整性
     *
     * @return true 如果内存区域完整
     * @return false 如果检测到损坏
     */
    bool validate() const {
        if constexpr (ThreadSafe) {
            ReadLockGuard lock(mutex_);
            return validateInternal();
        } else {
            return validateInternal();
        }
    }

    /**
     * @brief 检查指针是否在此区域中分配
     *
     * @param p 要检查的指针
     * @return true 如果指针属于此区域
     * @return false 若指针不属于此区域
     */
    bool owns(const void* p) const ATOM_NOEXCEPT {
        if constexpr (ThreadSafe) {
            ReadLockGuard lock(mutex_);
            return ownsInternal(p);
        } else {
            return ownsInternal(p);
        }
    }

private:
    void initializeInternal() ATOM_NOEXCEPT {
        if (isInitialized_)
            return;

        start_ = buffer_.data();
        end_ = start_ + N;

        // 初始化为单个空闲块
        firstBlock_ = reinterpret_cast<Block*>(start_);
        firstBlock_->size = N - sizeof(Block);
        firstBlock_->used = false;
        firstBlock_->offset = 0;
        firstBlock_->updateChecksum();

        // 初始化空闲链表
        freeList_ = reinterpret_cast<FreeBlock*>(firstBlock_);
        freeList_->next = nullptr;

#if ATOM_MEMORY_STATS_ENABLED
        stats_.reset();
#endif

// 在调试模式下填充内存
#if ATOM_MEMORY_VALIDATION_ENABLED
        utils::fillMemory(start_ + sizeof(Block), N - sizeof(Block),
                          utils::getAllocationPattern());
#endif

        isInitialized_ = true;
    }

    void cleanup() ATOM_NOEXCEPT {
        // 清理所有分配的内存，包括调试验证
        if (!isInitialized_)
            return;

#if ATOM_MEMORY_VALIDATION_ENABLED
        // 检查是否存在未释放的内存
        Block* block = firstBlock_;
        while (block < reinterpret_cast<Block*>(end_)) {
            if (block->used) {
// 可以记录内存泄漏
#if ATOM_MEMORY_STATS_ENABLED
                // 在此添加泄漏报告逻辑
#endif
            }

            char* blockEnd =
                reinterpret_cast<char*>(block) + sizeof(Block) + block->size;
            if (blockEnd >= end_)
                break;

            block = reinterpret_cast<Block*>(blockEnd);
        }
#endif

        // 重置变量
        firstBlock_ = nullptr;
        freeList_ = nullptr;
        isInitialized_ = false;
    }

    void* allocateInternal(std::size_t alignedSize) {
        if (!isInitialized_)
            initialize();

        // 根据分配策略找到合适的块
        Block* targetBlock = nullptr;
        Block* bestBlock = nullptr;
        std::size_t bestSize = std::numeric_limits<std::size_t>::max();
        std::size_t worstSize = 0;

        for (FreeBlock* current = freeList_; current != nullptr;
             current = current->next) {
            Block* block = &current->block;

            if (!block->isValid()) {
                // 检测到内存损坏
                throw std::runtime_error("Memory corruption detected in arena");
            }

            if (block->size >= alignedSize) {
                if constexpr (Strategy == AllocationStrategy::FirstFit) {
                    targetBlock = block;
                    break;
                } else if constexpr (Strategy == AllocationStrategy::BestFit) {
                    if (block->size < bestSize) {
                        bestSize = block->size;
                        bestBlock = block;
                    }
                } else if constexpr (Strategy == AllocationStrategy::WorstFit) {
                    if (block->size > worstSize) {
                        worstSize = block->size;
                        bestBlock = block;
                    }
                }
            }
        }

        if constexpr (Strategy == AllocationStrategy::FirstFit) {
            // 目标块已经在循环中设置
        } else {
            targetBlock = bestBlock;
        }

        if (targetBlock == nullptr) {
// 没有找到合适的块
#if ATOM_MEMORY_STATS_ENABLED
            stats_.recordFailedAllocation();
#endif
            throw std::bad_alloc();
        }

        // 检查是否需要分割块
        if (targetBlock->size >= alignedSize + sizeof(Block) + alignment) {
            // 分割块
            char* newBlockPtr = reinterpret_cast<char*>(targetBlock) +
                                sizeof(Block) + alignedSize;
            Block* newBlock = reinterpret_cast<Block*>(newBlockPtr);

            newBlock->size = targetBlock->size - alignedSize - sizeof(Block);
            newBlock->used = false;
            newBlock->offset =
                targetBlock->offset + sizeof(Block) + alignedSize;
            newBlock->updateChecksum();

            // 在空闲列表中替换目标块
            replaceInFreeList(targetBlock, newBlock);

            // 更新目标块大小
            targetBlock->size = alignedSize;
        } else {
            // 使用整个块
            removeFromFreeList(targetBlock);
        }

        // 标记块为已使用
        targetBlock->used = true;
        targetBlock->updateChecksum();

        // 返回数据指针
        void* dataPtr = reinterpret_cast<char*>(targetBlock) + sizeof(Block);

#if ATOM_MEMORY_VALIDATION_ENABLED
        // 使用模式填充已分配内存
        utils::fillMemory(dataPtr, targetBlock->size,
                          utils::getAllocationPattern());
#endif

#if ATOM_MEMORY_STATS_ENABLED
        stats_.recordAllocation(targetBlock->size);
#endif

        return dataPtr;
    }

    void deallocateInternal(void* p) ATOM_NOEXCEPT {
        if (!isInitialized_ || p == nullptr)
            return;

        // 获取块指针
        Block* block =
            reinterpret_cast<Block*>(static_cast<char*>(p) - sizeof(Block));

        // 验证块是否有效
        if (!block->isValid()) {
            // 处理无效块的情况
            assert(false && "Memory corruption detected during deallocation");
            return;
        }

        // 验证块是否已分配
        if (!block->used) {
            assert(false && "Double free detected");
            return;
        }

        // 标记为未使用
        block->used = false;
        block->updateChecksum();

#if ATOM_MEMORY_VALIDATION_ENABLED
        // 使用已释放模式填充内存
        utils::fillMemory(p, block->size, utils::getFreedPattern());
#endif

#if ATOM_MEMORY_STATS_ENABLED
        stats_.recordDeallocation(block->size);
#endif

        // 添加到空闲列表
        addToFreeList(block);

        // 尝试合并相邻的空闲块
        coalesceFreeBlocks();
    }

    std::size_t usedInternal() const ATOM_NOEXCEPT {
        if (!isInitialized_)
            return 0;

        std::size_t usedBytes = 0;
        Block* block = firstBlock_;

        while (block < reinterpret_cast<Block*>(end_)) {
            if (block->used) {
                usedBytes += block->size + sizeof(Block);
            }

            char* blockEnd =
                reinterpret_cast<char*>(block) + sizeof(Block) + block->size;
            if (blockEnd >= end_)
                break;

            block = reinterpret_cast<Block*>(blockEnd);
        }

        return usedBytes;
    }

    std::size_t remainingInternal() const ATOM_NOEXCEPT {
        return N - usedInternal();
    }

    void resetInternal() ATOM_NOEXCEPT {
        cleanup();
        initializeInternal();
    }

    // 将块添加到空闲列表
    void addToFreeList(Block* block) ATOM_NOEXCEPT {
        FreeBlock* freeBlock = reinterpret_cast<FreeBlock*>(block);
        freeBlock->next = freeList_;
        freeList_ = freeBlock;
    }

    // 从空闲列表中移除块
    void removeFromFreeList(Block* block) ATOM_NOEXCEPT {
        if (freeList_ == nullptr)
            return;

        FreeBlock* current = freeList_;
        FreeBlock* target = reinterpret_cast<FreeBlock*>(block);

        if (current == target) {
            freeList_ = current->next;
            return;
        }

        while (current->next != nullptr) {
            if (current->next == target) {
                current->next = target->next;
                return;
            }
            current = current->next;
        }
    }

    // 在空闲列表中替换块
    void replaceInFreeList(Block* oldBlock, Block* newBlock) ATOM_NOEXCEPT {
        if (freeList_ == nullptr)
            return;

        FreeBlock* current = freeList_;
        FreeBlock* oldFree = reinterpret_cast<FreeBlock*>(oldBlock);
        FreeBlock* newFree = reinterpret_cast<FreeBlock*>(newBlock);

        if (current == oldFree) {
            newFree->next = current->next;
            freeList_ = newFree;
            return;
        }

        while (current->next != nullptr) {
            if (current->next == oldFree) {
                newFree->next = oldFree->next;
                current->next = newFree;
                return;
            }
            current = current->next;
        }
    }

    // 合并相邻的空闲块
    void coalesceFreeBlocks() ATOM_NOEXCEPT {
        if (!isInitialized_)
            return;

        bool merged;
        do {
            merged = false;
            Block* block = firstBlock_;

            while (block < reinterpret_cast<Block*>(end_)) {
                if (!block->used) {
                    // 查找下一个块
                    char* nextBlockPtr = reinterpret_cast<char*>(block) +
                                         sizeof(Block) + block->size;
                    if (nextBlockPtr >= end_)
                        break;

                    Block* nextBlock = reinterpret_cast<Block*>(nextBlockPtr);

                    // 如果下一个块也是空闲的，合并它们
                    if (!nextBlock->used) {
                        // 从空闲列表中移除两个块
                        removeFromFreeList(block);
                        removeFromFreeList(nextBlock);

                        // 合并大小
                        block->size += sizeof(Block) + nextBlock->size;
                        block->updateChecksum();

                        // 将合并后的块添加回空闲列表
                        addToFreeList(block);

                        merged = true;
                        break;
                    }
                }

                char* blockEnd = reinterpret_cast<char*>(block) +
                                 sizeof(Block) + block->size;
                if (blockEnd >= end_)
                    break;

                block = reinterpret_cast<Block*>(blockEnd);
            }
        } while (merged);
    }

    // 碎片整理
    size_t defragmentInternal() {
        size_t mergeCount = 0;
        bool merged;

        do {
            merged = false;
            Block* block = firstBlock_;

            while (block < reinterpret_cast<Block*>(end_)) {
                if (!block->used) {
                    // 查找下一个块
                    char* nextBlockPtr = reinterpret_cast<char*>(block) +
                                         sizeof(Block) + block->size;
                    if (nextBlockPtr >= end_)
                        break;

                    Block* nextBlock = reinterpret_cast<Block*>(nextBlockPtr);

                    // 如果下一个块也是空闲的，合并它们
                    if (!nextBlock->used) {
                        // 从空闲列表中移除两个块
                        removeFromFreeList(block);
                        removeFromFreeList(nextBlock);

                        // 合并大小
                        block->size += sizeof(Block) + nextBlock->size;
                        block->updateChecksum();

                        // 将合并后的块添加回空闲列表
                        addToFreeList(block);

                        merged = true;
                        mergeCount++;
                        break;
                    }
                }

                char* blockEnd = reinterpret_cast<char*>(block) +
                                 sizeof(Block) + block->size;
                if (blockEnd >= end_)
                    break;

                block = reinterpret_cast<Block*>(blockEnd);
            }
        } while (merged);

        return mergeCount;
    }

    // 验证内存区域完整性
    bool validateInternal() const {
        if (!isInitialized_)
            return true;

        Block* block = firstBlock_;

        while (block < reinterpret_cast<Block*>(end_)) {
            if (!block->isValid()) {
                return false;
            }

            char* blockEnd =
                reinterpret_cast<char*>(block) + sizeof(Block) + block->size;
            if (blockEnd > end_)
                return false;

            if (blockEnd == end_)
                break;

            block = reinterpret_cast<Block*>(blockEnd);
        }

        return true;
    }

    // 检查指针是否在此区域中
    bool ownsInternal(const void* p) const ATOM_NOEXCEPT {
        return start_ <= p && p < end_;
    }

    // 对齐大小到对齐边界
    std::size_t alignSize(std::size_t size) const ATOM_NOEXCEPT {
        return (size + alignment - 1) & ~(alignment - 1);
    }
};

/**
 * @brief 增强版简单分配器，使用固定大小内存区域进行分配
 *
 * 此分配器提供了使用固定大小内存区域进行动态分配的方法，消除了从堆分配动态内存的需要。
 * 它在内存分配性能或内存碎片化是关注点的场景中很有用。
 *
 * @tparam T 要分配的对象类型
 * @tparam N 固定大小内存区域的大小（字节）
 * @tparam Align 内存分配的对齐要求，默认为 alignof(std::max_align_t)
 * @tparam ThreadSafe 是否启用线程安全特性
 * @tparam Strategy 内存分配策略
 */
template <class T, std::size_t N, std::size_t Align = alignof(std::max_align_t),
          bool ThreadSafe = true,
          AllocationStrategy Strategy = AllocationStrategy::FirstFit>
class ShortAlloc {
public:
    using value_type = T;
    using arena_type = Arena<N, Align, ThreadSafe, Strategy>;

    static ATOM_CONSTEXPR auto ALIGNMENT = Align;
    static ATOM_CONSTEXPR auto SIZE = N;

private:
    arena_type& arena_;

public:
    explicit ShortAlloc(arena_type& a) ATOM_NOEXCEPT : arena_(a) {}

    template <class U>
    ShortAlloc(const ShortAlloc<U, N, ALIGNMENT, ThreadSafe, Strategy>& a)
        ATOM_NOEXCEPT : arena_(a.arena_) {}

    /**
     * @brief 分配内存
     *
     * @param n 要分配的对象数量
     * @return T* 指向分配内存的指针
     * @throw std::bad_alloc 如果没有足够的内存
     */
    auto allocate(std::size_t n) -> T* {
        if (n == 0)
            return nullptr;
        if (n > SIZE / sizeof(T)) {
            throw std::bad_alloc();
        }

        void* ptr = arena_.allocate(n * sizeof(T));
        if (ptr == nullptr) {
            throw std::bad_alloc();
        }

        return static_cast<T*>(ptr);
    }

    /**
     * @brief 释放内存
     *
     * @param p 要释放的内存指针
     * @param n 释放的对象数量
     */
    void deallocate(T* p, std::size_t) ATOM_NOEXCEPT { arena_.deallocate(p); }

    /**
     * @brief 构造对象
     *
     * @tparam U 对象类型
     * @tparam Args 构造函数参数类型
     * @param p 指向构造位置的指针
     * @param args 传递给构造函数的参数
     */
    template <class U, class... Args>
    void construct(U* p, Args&&... args) {
        ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
    }

    /**
     * @brief 销毁对象
     *
     * @tparam U 对象类型
     * @param p 指向要销毁对象的指针
     */
    template <class U>
    void destroy(U* p) ATOM_NOEXCEPT {
        if (p != nullptr) {
            p->~U();
        }
    }

    /**
     * @brief 重绑定类型模板
     *
     * @tparam U 新类型
     */
    template <class U>
    struct rebind {
        using other = ShortAlloc<U, N, Align, ThreadSafe, Strategy>;
    };

    /**
     * @brief 获取分配器使用的内存区域
     *
     * @return arena_type& 内存区域引用
     */
    arena_type& arena() const ATOM_NOEXCEPT { return arena_; }

    /**
     * @brief 检查指针是否由此分配器分配
     *
     * @param p 要检查的指针
     * @return true 如果指针由此分配器分配
     */
    bool owns(const T* p) const ATOM_NOEXCEPT { return arena_.owns(p); }

    /**
     * @brief 获取内存统计信息
     *
     * @return std::string 统计报告
     */
    std::string getStats() const { return arena_.getStats(); }

    /**
     * @brief 执行内存整理
     *
     * @return size_t 整理的块数量
     */
    size_t defragment() { return arena_.defragment(); }

    /**
     * @brief 验证内存区域完整性
     *
     * @return true 如果内存区域完整
     */
    bool validate() const { return arena_.validate(); }

    // 重置内存区域
    void reset() ATOM_NOEXCEPT { arena_.reset(); }

    // 分配器比较操作符
    template <class T1, std::size_t N1, std::size_t A1, bool TS1,
              AllocationStrategy S1, class U, std::size_t M, std::size_t A2,
              bool TS2, AllocationStrategy S2>
    friend auto operator==(const ShortAlloc<T1, N1, A1, TS1, S1>& x,
                           const ShortAlloc<U, M, A2, TS2, S2>& y)
        ATOM_NOEXCEPT->bool;

    template <class U, std::size_t M, std::size_t A2, bool TS2,
              AllocationStrategy S2>
    friend class ShortAlloc;
};

template <class T, std::size_t N, std::size_t A1, bool TS1,
          AllocationStrategy S1, class U, std::size_t M, std::size_t A2,
          bool TS2, AllocationStrategy S2>
inline auto operator==(const ShortAlloc<T, N, A1, TS1, S1>& x,
                       const ShortAlloc<U, M, A2, TS2, S2>& y)
    ATOM_NOEXCEPT->bool {
    return N == M && A1 == A2 && TS1 == TS2 && S1 == S2 &&
           &x.arena_ == &y.arena_;
}

template <class T, std::size_t N, std::size_t A1, bool TS1,
          AllocationStrategy S1, class U, std::size_t M, std::size_t A2,
          bool TS2, AllocationStrategy S2>
inline auto operator!=(const ShortAlloc<T, N, A1, TS1, S1>& x,
                       const ShortAlloc<U, M, A2, TS2, S2>& y)
    ATOM_NOEXCEPT->bool {
    return !(x == y);
}

/**
 * @brief 使用特定分配器分配具有自定义删除器的 unique_ptr
 *
 * @tparam Alloc 分配器类型
 * @tparam T 要分配的对象类型
 * @tparam Args 构造函数参数类型
 * @param alloc 分配器实例
 * @param args 传递给 T 构造函数的参数
 * @return std::unique_ptr<T, std::function<void(T*)>> 带有自定义删除器的已分配
 * unique_ptr
 */
template <typename Alloc, typename T, typename... Args>
auto allocateUnique(Alloc& alloc, Args&&... args)
    -> std::unique_ptr<T, std::function<void(T*)>> {
    using AllocTraits = std::allocator_traits<Alloc>;

    // 分配内存
    T* p = AllocTraits::allocate(alloc, 1);
    try {
        // 构造对象
        AllocTraits::construct(alloc, p, std::forward<Args>(args)...);
    } catch (...) {
        // 出现异常时释放内存
        AllocTraits::deallocate(alloc, p, 1);
        throw;
    }

    // 创建自定义删除器
    return std::unique_ptr<T, std::function<void(T*)>>(
        p, [alloc = std::addressof(alloc)](T* ptr) mutable {
            if (ptr) {
                AllocTraits::destroy(*alloc, ptr);
                AllocTraits::deallocate(*alloc, ptr, 1);
            }
        });
}

/**
 * @brief 创建使用自定义分配器的容器
 *
 * @tparam Container 容器类型
 * @tparam Arena 内存区域类型
 * @param arena 内存区域实例
 * @return Container 使用自定义分配器的容器
 */
template <template <typename, typename> class Container, typename T,
          std::size_t N, std::size_t Align = alignof(std::max_align_t)>
auto makeArenaContainer(Arena<N, Align>& arena) {
    using Allocator = ShortAlloc<T, N, Align>;
    return Container<T, Allocator>(Allocator(arena));
}

}  // namespace atom::memory

#endif  // ATOM_MEMORY_SHORT_ALLOC_HPP
