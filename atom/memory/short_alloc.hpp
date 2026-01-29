#ifndef ATOM_MEMORY_SHORT_ALLOC_HPP
#define ATOM_MEMORY_SHORT_ALLOC_HPP

#include <immintrin.h>  // For memory prefetching
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>

// Cache line size for alignment optimizations
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

// 跨平台支持
#include "atom/macro.hpp"

// 线程支持
#ifdef ATOM_USE_BOOST
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/shared_mutex.hpp>
#else
#include <mutex>
#include <shared_mutex>
#endif

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

/**
 * @brief 分配策略枚举
 */
enum class AllocationStrategy {
    FirstFit,  // 第一个适合的空闲块
    BestFit,   // 最合适大小的空闲块
    WorstFit   // 最大的空闲块
};

// Enhanced memory statistics collector with advanced debugging and performance
// features
class MemoryStats {
public:
    struct alignas(CACHE_LINE_SIZE) ArenaStats {
        // Basic allocation statistics (atomic for thread safety)
        std::atomic<size_t> totalAllocations{0};
        std::atomic<size_t> currentAllocations{0};
        std::atomic<size_t> totalBytesAllocated{0};
        std::atomic<size_t> peakBytesAllocated{0};
        std::atomic<size_t> currentBytesAllocated{0};
        std::atomic<size_t> failedAllocations{0};

        // Advanced performance metrics
        std::atomic<size_t> fragmentationEvents{
            0};  ///< Number of fragmentation events
        std::atomic<size_t> coalescingOperations{
            0};  ///< Number of block coalescing operations
        std::atomic<size_t> splitOperations{
            0};  ///< Number of block split operations
        std::atomic<size_t> memoryLeaks{0};  ///< Detected memory leaks
        std::atomic<size_t> corruptionDetections{
            0};  ///< Memory corruption detections
        std::atomic<size_t> doubleFreesDetected{0};  ///< Double free detections

        // Timing statistics (in nanoseconds)
        std::atomic<uint64_t> totalAllocationTime{
            0};  ///< Total allocation time
        std::atomic<uint64_t> totalDeallocationTime{
            0};  ///< Total deallocation time
        std::atomic<uint64_t> maxAllocationTime{
            0};  ///< Maximum allocation time
        std::atomic<uint64_t> maxDeallocationTime{
            0};  ///< Maximum deallocation time

        // Strategy-specific metrics
        std::atomic<size_t> firstFitAttempts{
            0};  ///< First-fit strategy attempts
        std::atomic<size_t> bestFitAttempts{0};  ///< Best-fit strategy attempts
        std::atomic<size_t> worstFitAttempts{
            0};                                 ///< Worst-fit strategy attempts
        std::atomic<size_t> strategyMisses{0};  ///< Strategy allocation misses

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

        void recordFailedAllocation() {
            failedAllocations.fetch_add(1, std::memory_order_relaxed);
        }

        void recordFragmentation() {
            fragmentationEvents.fetch_add(1, std::memory_order_relaxed);
        }

        void recordCoalescing() {
            coalescingOperations.fetch_add(1, std::memory_order_relaxed);
        }

        void recordSplit() {
            splitOperations.fetch_add(1, std::memory_order_relaxed);
        }

        void recordMemoryLeak() {
            memoryLeaks.fetch_add(1, std::memory_order_relaxed);
        }

        void recordCorruption() {
            corruptionDetections.fetch_add(1, std::memory_order_relaxed);
        }

        void recordDoubleFree() {
            doubleFreesDetected.fetch_add(1, std::memory_order_relaxed);
        }

        void recordAllocationTime(uint64_t duration) {
            totalAllocationTime.fetch_add(duration, std::memory_order_relaxed);
            uint64_t current_max =
                maxAllocationTime.load(std::memory_order_relaxed);
            while (duration > current_max &&
                   !maxAllocationTime.compare_exchange_weak(
                       current_max, duration, std::memory_order_relaxed)) {
                // Keep trying until we successfully update or find a larger
                // value
            }
        }

        void recordDeallocationTime(uint64_t duration) {
            totalDeallocationTime.fetch_add(duration,
                                            std::memory_order_relaxed);
            uint64_t current_max =
                maxDeallocationTime.load(std::memory_order_relaxed);
            while (duration > current_max &&
                   !maxDeallocationTime.compare_exchange_weak(
                       current_max, duration, std::memory_order_relaxed)) {
                // Keep trying until we successfully update or find a larger
                // value
            }
        }

        void recordStrategyAttempt(AllocationStrategy strategy) {
            switch (strategy) {
                case AllocationStrategy::FirstFit:
                    firstFitAttempts.fetch_add(1, std::memory_order_relaxed);
                    break;
                case AllocationStrategy::BestFit:
                    bestFitAttempts.fetch_add(1, std::memory_order_relaxed);
                    break;
                case AllocationStrategy::WorstFit:
                    worstFitAttempts.fetch_add(1, std::memory_order_relaxed);
                    break;
            }
        }

        void recordStrategyMiss() {
            strategyMisses.fetch_add(1, std::memory_order_relaxed);
        }

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
            fragmentationEvents = 0;
            coalescingOperations = 0;
            splitOperations = 0;
            memoryLeaks = 0;
            corruptionDetections = 0;
            doubleFreesDetected = 0;
            totalAllocationTime = 0;
            totalDeallocationTime = 0;
            maxAllocationTime = 0;
            maxDeallocationTime = 0;
            firstFitAttempts = 0;
            bestFitAttempts = 0;
            worstFitAttempts = 0;
            strategyMisses = 0;
        }

        // Performance calculation helpers
        double getAverageAllocationTime() const noexcept {
            size_t count = totalAllocations.load(std::memory_order_relaxed);
            return count > 0
                       ? static_cast<double>(totalAllocationTime.load()) / count
                       : 0.0;
        }

        double getAverageDeallocationTime() const noexcept {
            size_t count = totalAllocations.load() - currentAllocations.load();
            return count > 0
                       ? static_cast<double>(totalDeallocationTime.load()) /
                             count
                       : 0.0;
        }

        double getFragmentationRatio() const noexcept {
            size_t total_ops = totalAllocations.load();
            return total_ops > 0
                       ? static_cast<double>(fragmentationEvents.load()) /
                             total_ops
                       : 0.0;
        }

        double getFailureRatio() const noexcept {
            size_t total_attempts =
                totalAllocations.load() + failedAllocations.load();
            return total_attempts > 0
                       ? static_cast<double>(failedAllocations.load()) /
                             total_attempts
                       : 0.0;
        }

        double getMemoryEfficiency() const noexcept {
            size_t peak = peakBytesAllocated.load();
            size_t total = totalBytesAllocated.load();
            return total > 0 ? static_cast<double>(peak) / total : 0.0;
        }
    };

    static ArenaStats& getStats() {
        static ArenaStats stats;
        return stats;
    }
};

/**
 * @brief Configuration for Arena optimizations and debugging
 */
struct ArenaConfig {
    bool enable_stats{true};           ///< Enable performance statistics
    bool enable_debugging{true};       ///< Enable debugging features
    bool enable_prefetching{true};     ///< Enable memory prefetching
    bool enable_coalescing{true};      ///< Enable automatic block coalescing
    bool enable_leak_detection{true};  ///< Enable memory leak detection
    bool enable_corruption_detection{
        true};                        ///< Enable memory corruption detection
    size_t coalescing_threshold{64};  ///< Minimum size for coalescing
    size_t prefetch_distance{1};      ///< Number of blocks to prefetch ahead
};

/**
 * @brief Enhanced fixed-size memory arena with advanced allocation strategies
 * and debugging
 *
 * Features:
 * - Multiple allocation strategies (FirstFit, BestFit, WorstFit)
 * - Comprehensive performance monitoring and statistics
 * - Advanced debugging with memory corruption detection
 * - Memory leak detection and reporting
 * - Cache-optimized memory prefetching
 * - Automatic block coalescing for reduced fragmentation
 * - Thread-safe operations with configurable locking
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
    ArenaConfig config_;  ///< Configuration options
    std::unordered_map<void*, size_t>
        allocation_map_;  ///< Track allocations for leak detection

public:
    explicit Arena(const ArenaConfig& config = ArenaConfig{}) ATOM_NOEXCEPT
        : config_(config) {
        initialize();
    }

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
     * @brief Enhanced memory allocation with performance monitoring and
     * debugging
     *
     * @param size 要分配的字节数
     * @return void* 指向已分配内存的指针
     * @throw std::bad_alloc 如果没有足够的内存满足请求
     */
    auto allocate(std::size_t size) -> void* {
        if (size == 0)
            return nullptr;

        auto start_time =
            config_.enable_stats
                ? std::chrono::high_resolution_clock::now()
                : std::chrono::high_resolution_clock::time_point{};

        const std::size_t alignedSize = alignSize(size);

        void* result = nullptr;
        if constexpr (ThreadSafe) {
            WriteLockGuard lock(mutex_);
            result = allocateInternal(alignedSize);
        } else {
            result = allocateInternal(alignedSize);
        }

        // Record timing statistics
        if (config_.enable_stats && result != nullptr) {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::nanoseconds>(end_time -
                                                                     start_time)
                    .count();
#if ATOM_MEMORY_STATS_ENABLED
            stats_.recordAllocationTime(static_cast<uint64_t>(duration));
            stats_.recordStrategyAttempt(Strategy);
#else
            (void)duration;  // Suppress unused variable warning
#endif
        }

        // Track allocation for leak detection
        if (config_.enable_leak_detection && result != nullptr) {
            if constexpr (ThreadSafe) {
                WriteLockGuard lock(mutex_);
                allocation_map_[result] = alignedSize;
            } else {
                allocation_map_[result] = alignedSize;
            }
        }

        // Prefetch memory for better cache performance
        if (config_.enable_prefetching && result != nullptr) {
            prefetchMemoryRegion(result, alignedSize);
        }

        return result;
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

    /**
     * @brief Get enhanced performance metrics
     *
     * @return Tuple of (avg_alloc_time, avg_dealloc_time, fragmentation_ratio,
     * failure_ratio, efficiency)
     */
    [[nodiscard]] auto getPerformanceMetrics() const
        -> std::tuple<double, double, double, double, double> {
        if constexpr (ThreadSafe) {
            ReadLockGuard lock(mutex_);
#if ATOM_MEMORY_STATS_ENABLED
            return std::make_tuple(stats_.getAverageAllocationTime(),
                                   stats_.getAverageDeallocationTime(),
                                   stats_.getFragmentationRatio(),
                                   stats_.getFailureRatio(),
                                   stats_.getMemoryEfficiency());
#else
            return std::make_tuple(0.0, 0.0, 0.0, 0.0, 0.0);
#endif
        } else {
#if ATOM_MEMORY_STATS_ENABLED
            return std::make_tuple(stats_.getAverageAllocationTime(),
                                   stats_.getAverageDeallocationTime(),
                                   stats_.getFragmentationRatio(),
                                   stats_.getFailureRatio(),
                                   stats_.getMemoryEfficiency());
#else
            return std::make_tuple(0.0, 0.0, 0.0, 0.0, 0.0);
#endif
        }
    }

    /**
     * @brief Get current configuration
     *
     * @return Current arena configuration
     */
    [[nodiscard]] const ArenaConfig& getConfig() const noexcept {
        return config_;
    }

    /**
     * @brief Update configuration
     *
     * @param new_config New configuration to apply
     */
    void updateConfig(const ArenaConfig& new_config) {
        if constexpr (ThreadSafe) {
            WriteLockGuard lock(mutex_);
            config_ = new_config;
        } else {
            config_ = new_config;
        }
    }

    /**
     * @brief Check for memory leaks
     *
     * @return Number of detected memory leaks
     */
    [[nodiscard]] size_t checkMemoryLeaks() const {
        if constexpr (ThreadSafe) {
            ReadLockGuard lock(mutex_);
            return allocation_map_.size();
        } else {
            return allocation_map_.size();
        }
    }

    /**
     * @brief Force garbage collection and coalescing
     */
    void garbageCollect() {
        if constexpr (ThreadSafe) {
            WriteLockGuard lock(mutex_);
            coalesceFreeBlocks();
        } else {
            coalesceFreeBlocks();
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

    /**
     * @brief Prefetch memory region for better cache performance
     */
    void prefetchMemoryRegion(void* ptr, size_t size) const noexcept {
        if (!config_.enable_prefetching || ptr == nullptr)
            return;

        char* memory = static_cast<char*>(ptr);
        size_t prefetch_size = std::min(
            size,
            static_cast<size_t>(CACHE_LINE_SIZE * config_.prefetch_distance));

        for (size_t offset = 0; offset < prefetch_size;
             offset += CACHE_LINE_SIZE) {
            _mm_prefetch(memory + offset, _MM_HINT_T0);
        }
    }

    /**
     * @brief Detect and report memory leaks
     */
    void detectMemoryLeaks() const {
        if (!config_.enable_leak_detection)
            return;

        size_t leak_count = allocation_map_.size();
        if (leak_count > 0) {
#if ATOM_MEMORY_STATS_ENABLED
            if (config_.enable_stats) {
                // Update leak statistics for each leaked allocation
                for (size_t i = 0; i < leak_count; ++i) {
                    stats_.recordMemoryLeak();
                }
            }
#endif

            // Log memory leaks in debug mode
            assert(false && "Memory leaks detected in Arena");
        }
    }

    /**
     * @brief Enhanced corruption detection with detailed reporting
     */
    void validateMemoryIntegrity() const {
        if (!config_.enable_corruption_detection)
            return;

        // Walk through all blocks and validate checksums
        Block* current = firstBlock_;
        while (current != nullptr && reinterpret_cast<char*>(current) < end_) {
            if (!current->isValid()) {
#if ATOM_MEMORY_STATS_ENABLED
                if (config_.enable_stats) {
                    stats_.recordCorruption();
                }
#endif
                // Log corruption details
                assert(false && "Memory corruption detected in Arena block");
                return;
            }

            // Move to next block
            char* nextPtr = reinterpret_cast<char*>(current) + sizeof(Block) +
                            current->size;
            current = reinterpret_cast<Block*>(nextPtr);
        }
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
