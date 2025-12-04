/*
 * iteration.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-11

Description: Cache-Friendly Component Iteration System
Provides SIMD-optimized iteration patterns, data-oriented design,
and cache-locality optimizations for high-performance component processing.

**************************************************/

#ifndef ATOM_COMPONENT_ITERATION_HPP
#define ATOM_COMPONENT_ITERATION_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

#ifdef __AVX2__
#include <immintrin.h>  // For SIMD intrinsics
#endif

#include "../component.hpp"

namespace atom::components {

/**
 * @brief Cache line size for alignment optimizations
 */
static constexpr size_t CACHE_LINE_SIZE = 64;

/**
 * @brief SIMD vector width for different data types
 */
template <typename T>
#ifdef __AVX2__
static constexpr size_t SIMD_WIDTH = sizeof(__m256) / sizeof(T);
#else
static constexpr size_t SIMD_WIDTH = 4 / sizeof(T);  // Fallback for non-AVX2
#endif

/**
 * @brief Type trait for SIMD-compatible types
 */
template <typename T>
struct is_simd_compatible
    : std::bool_constant<std::is_arithmetic_v<T> &&
                         (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 ||
                          sizeof(T) == 8)> {};

template <typename T>
inline constexpr bool is_simd_compatible_v = is_simd_compatible<T>::value;

/**
 * @brief Type trait for component types that support batch operations
 */
template <typename T>
struct is_batch_processable {
    template <typename U>
    static auto test(int) -> decltype(std::declval<U&>().batchUpdate(),
                                      std::declval<U&>().getUpdateData(),
                                      std::declval<U&>().getUpdateDataSize(),
                                      std::true_type{});

    template <typename>
    static std::false_type test(...);

    static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T>
inline constexpr bool is_batch_processable_v = is_batch_processable<T>::value;

/**
 * @brief Structure of Arrays (SoA) container for cache-friendly data layout
 */
template <typename... Components>
class ComponentSoA {
public:
    static constexpr size_t NUM_COMPONENTS = sizeof...(Components);

    /**
     * @brief Constructs SoA with initial capacity
     * @param capacity Initial capacity
     */
    explicit ComponentSoA(size_t capacity = 1024);

    /**
     * @brief Adds a component set to the SoA
     * @param components Component instances
     * @return Index of the added component set
     */
    size_t add(Components&&... components);

    /**
     * @brief Removes a component set by index
     * @param index Index to remove
     */
    void remove(size_t index);

    /**
     * @brief Gets the number of active component sets
     * @return Number of active component sets
     */
    [[nodiscard]] size_t size() const noexcept { return size_; }

    /**
     * @brief Gets the capacity
     * @return Current capacity
     */
    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }

    /**
     * @brief Iterates over all component sets with a function
     * @tparam Func Function type
     * @param func Function to apply to each component set
     */
    template <typename Func>
    void forEach(Func&& func);

    /**
     * @brief Parallel iteration over component sets
     * @tparam Func Function type
     * @param func Function to apply to each component set
     * @param numThreads Number of threads to use (0 = auto)
     */
    template <typename Func>
    void forEachParallel(Func&& func, size_t numThreads = 0);

    /**
     * @brief SIMD-optimized batch processing for arithmetic operations
     * @tparam T Data type
     * @tparam Op Operation type
     * @param data Data array
     * @param operation SIMD operation
     */
    template <typename T, typename Op>
    typename std::enable_if_t<is_simd_compatible_v<T>, void> simdProcess(
        T* data, size_t size, Op&& operation);

    /**
     * @brief Gets a specific component array
     * @tparam T Component type
     * @return Pointer and size of components
     */
    template <typename T>
    [[nodiscard]] std::pair<T*, size_t> getComponents();

    /**
     * @brief Gets a specific component array (const version)
     * @tparam T Component type
     * @return Const pointer and size of components
     */
    template <typename T>
    [[nodiscard]] std::pair<const T*, size_t> getComponents() const;

    /**
     * @brief Defragments the SoA by removing gaps
     */
    void defragment();

    /**
     * @brief Gets fragmentation ratio
     * @return Fragmentation ratio (0.0 = no fragmentation, 1.0 = fully
     * fragmented)
     */
    [[nodiscard]] double getFragmentationRatio() const noexcept;

private:
    static constexpr size_t ALIGNMENT = CACHE_LINE_SIZE;

    size_t capacity_;
    std::atomic<size_t> size_{0};

    // Component arrays (aligned for cache efficiency)
    std::tuple<std::vector<Components>...> componentArrays_;

    // Free list for efficient removal/addition
    std::vector<size_t> freeIndices_;

    // Helper methods
    void resize(size_t newCapacity);

    template <size_t Index, typename ComponentType>
    void addComponentAtIndex(size_t index, ComponentType&& component);

    template <size_t Index>
    void removeComponentAtIndex(size_t index);
};

/**
 * @brief Cache-friendly component iterator with prefetching
 */
template <typename ComponentType>
class CacheOptimizedIterator {
public:
    using value_type = ComponentType;
    using pointer = ComponentType*;
    using reference = ComponentType&;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::random_access_iterator_tag;

    /**
     * @brief Constructs iterator with prefetch distance
     * @param ptr Pointer to component data
     * @param prefetchDistance Distance for prefetching (in cache lines)
     */
    explicit CacheOptimizedIterator(pointer ptr, size_t prefetchDistance = 2);

    // Iterator operations
    reference operator*() const;
    pointer operator->() const;
    CacheOptimizedIterator& operator++();
    CacheOptimizedIterator operator++(int);
    CacheOptimizedIterator& operator--();
    CacheOptimizedIterator operator--(int);
    CacheOptimizedIterator& operator+=(difference_type n);
    CacheOptimizedIterator& operator-=(difference_type n);
    CacheOptimizedIterator operator+(difference_type n) const;
    CacheOptimizedIterator operator-(difference_type n) const;
    difference_type operator-(const CacheOptimizedIterator& other) const;
    reference operator[](difference_type n) const;

    // Comparison operations
    bool operator==(const CacheOptimizedIterator& other) const;
    bool operator!=(const CacheOptimizedIterator& other) const;
    bool operator<(const CacheOptimizedIterator& other) const;
    bool operator<=(const CacheOptimizedIterator& other) const;
    bool operator>(const CacheOptimizedIterator& other) const;
    bool operator>=(const CacheOptimizedIterator& other) const;

private:
    pointer ptr_;
    size_t prefetchDistance_;

    void prefetch() const;
};

/**
 * @brief Batch processor for component operations
 */
class ComponentBatchProcessor {
public:
    /**
     * @brief Configuration for batch processing
     */
    struct Config {
        size_t batchSize = 64;  // Components per batch
        size_t numThreads = 0;  // 0 = auto-detect
        bool enableSIMD = true;
        bool enablePrefetch = true;
        size_t prefetchDistance = 2;  // Cache lines
    };

    /**
     * @brief Constructs batch processor with default configuration
     */
    ComponentBatchProcessor();

    /**
     * @brief Constructs batch processor with configuration
     * @param config Processor configuration
     */
    explicit ComponentBatchProcessor(const Config& config);

    /**
     * @brief Processes components in batches
     * @tparam ComponentType Component type
     * @tparam Func Function type
     * @param components Component container
     * @param func Function to apply to each batch
     */
    template <typename ComponentType, typename Func>
    void processBatches(ComponentType* components, size_t count, Func&& func);

    /**
     * @brief SIMD-optimized batch update for arithmetic components
     * @tparam T Data type
     * @param data Data to process
     * @param updateFunc Update function
     */
    template <typename T>
    typename std::enable_if_t<is_simd_compatible_v<T>, void> simdBatchUpdate(
        T* data, size_t count, std::function<T(T)> updateFunc);

    /**
     * @brief Gets processing statistics
     * @return Processing statistics
     */
    struct Statistics {
        uint64_t totalBatches = 0;
        uint64_t totalComponents = 0;
        std::chrono::microseconds totalProcessingTime{0};
        std::chrono::microseconds avgBatchTime{0};
        double simdUtilization = 0.0;  // Percentage of SIMD usage
    };

    [[nodiscard]] const Statistics& getStatistics() const noexcept {
        return statistics_;
    }

    /**
     * @brief Resets processing statistics
     */
    void resetStatistics() noexcept;

private:
    Config config_;
    Statistics statistics_;

    // SIMD operation helpers
    template <typename T>
    void simdAdd(T* a, const T* b, size_t count);

    template <typename T>
    void simdMultiply(T* a, const T* b, size_t count);

    template <typename T>
    void simdApplyFunction(T* data, size_t count, std::function<T(T)> func);
};

/**
 * @brief Memory layout optimizer for component data
 */
class MemoryLayoutOptimizer {
public:
    /**
     * @brief Analyzes memory access patterns
     * @tparam ComponentType Component type
     * @param components Component data
     * @return Access pattern analysis
     */
    template <typename ComponentType>
    struct AccessPattern {
        double spatialLocality;         // 0.0 = poor, 1.0 = excellent
        double temporalLocality;        // 0.0 = poor, 1.0 = excellent
        size_t cacheLineUtilization;    // Bytes per cache line used
        std::vector<size_t> hotFields;  // Frequently accessed field offsets
    };

    template <typename ComponentType>
    [[nodiscard]] AccessPattern<ComponentType> analyzeAccessPattern(
        const ComponentType* components, size_t count) const;

    /**
     * @brief Suggests optimal memory layout
     * @tparam ComponentType Component type
     * @param pattern Access pattern
     * @return Layout suggestions
     */
    template <typename ComponentType>
    struct LayoutSuggestion {
        bool useStructOfArrays;             // Recommend SoA over AoS
        std::vector<size_t> fieldGrouping;  // Group related fields
        size_t recommendedAlignment;
        size_t recommendedPadding;
    };

    template <typename ComponentType>
    [[nodiscard]] LayoutSuggestion<ComponentType> suggestLayout(
        const AccessPattern<ComponentType>& pattern) const;

private:
    // Performance counters (would be implemented with platform-specific code)
    mutable uint64_t cacheHits_ = 0;
    mutable uint64_t cacheMisses_ = 0;
};

/**
 * @brief Advanced parallel component processor with work-stealing
 */
class ParallelComponentProcessor {
public:
    /**
     * @brief Configuration for parallel processing
     */
    struct ParallelConfig {
        size_t numThreads = 0;           // 0 = auto-detect
        size_t minBatchSize = 32;        // Minimum components per batch
        size_t maxBatchSize = 256;       // Maximum components per batch
        bool enableWorkStealing = true;  // Enable work-stealing between threads
        bool enableLoadBalancing = true;  // Dynamic load balancing
        size_t stealThreshold = 8;        // Minimum work to steal
    };

    ParallelComponentProcessor();
    explicit ParallelComponentProcessor(const ParallelConfig& config);
    ~ParallelComponentProcessor();

    /**
     * @brief Processes components in parallel with optimal load balancing
     * @tparam ComponentType Component type
     * @tparam Func Function type
     * @param components Component container
     * @param func Function to apply
     */
    template <typename ComponentType, typename Func>
    void processParallel(std::span<ComponentType> components, Func&& func);

    /**
     * @brief Processes components with dependency-aware scheduling
     * @tparam ComponentType Component type
     * @tparam Func Function type
     * @param components Component container
     * @param dependencies Dependency graph
     * @param func Function to apply
     */
    template <typename ComponentType, typename Func>
    void processWithDependencies(
        std::span<ComponentType> components,
        const std::vector<std::vector<size_t>>& dependencies, Func&& func);

private:
    ParallelConfig config_;
    class WorkStealingQueue;
    std::vector<std::unique_ptr<WorkStealingQueue>> workQueues_;
    std::vector<std::thread> workers_;
    std::atomic<bool> shutdown_{false};
};

/**
 * @brief Memory-efficient component archetype system
 *
 * Groups components by type signature for optimal memory layout
 * and cache performance.
 */
template <typename... ComponentTypes>
class ComponentArchetype {
public:
    using ComponentTuple = std::tuple<ComponentTypes...>;
    static constexpr size_t NUM_TYPES = sizeof...(ComponentTypes);

    /**
     * @brief Adds a component set to the archetype
     * @param components Component instances
     * @return Entity ID
     */
    size_t addEntity(ComponentTypes... components);

    /**
     * @brief Removes an entity from the archetype
     * @param entityId Entity ID to remove
     */
    void removeEntity(size_t entityId);

    /**
     * @brief Iterates over all entities with SIMD optimization
     * @tparam Func Function type
     * @param func Function to apply to each entity
     */
    template <typename Func>
    void forEach(Func&& func);

    /**
     * @brief Gets component data for SIMD processing
     * @tparam T Component type
     * @return Span of component data
     */
    template <typename T>
    std::span<T> getComponentData();

    /**
     * @brief Gets entity count
     * @return Number of entities
     */
    [[nodiscard]] size_t size() const noexcept { return entityCount_; }

private:
    // Structure of Arrays for each component type
    std::tuple<std::vector<ComponentTypes>...> componentArrays_;
    std::vector<size_t> entityIds_;
    size_t entityCount_{0};
    size_t nextEntityId_{1};

    // Free list for entity ID reuse
    std::vector<size_t> freeEntityIds_;
};

}  // namespace atom::components

// Hash specialization for std::vector<std::type_index>
namespace std {
template <>
struct hash<std::vector<std::type_index>> {
    std::size_t operator()(const std::vector<std::type_index>& vec) const {
        std::size_t seed = vec.size();
        for (const auto& i : vec) {
            seed ^= std::hash<std::type_index>{}(i) + 0x9e3779b9 + (seed << 6) +
                    (seed >> 2);
        }
        return seed;
    }
};
}  // namespace std

namespace atom::components {

/**
 * @brief High-performance component query system
 */
class ComponentQueryEngine {
public:
    /**
     * @brief Query builder for component selection
     */
    class QueryBuilder {
    public:
        template <typename T>
        QueryBuilder& with();

        template <typename T>
        QueryBuilder& without();

        template <typename T>
        QueryBuilder& optional();

        /**
         * @brief Executes the query
         * @tparam Func Function type
         * @param func Function to apply to matching entities
         */
        template <typename Func>
        void execute(Func&& func);

    private:
        std::vector<std::type_index> requiredTypes_;
        std::vector<std::type_index> excludedTypes_;
        std::vector<std::type_index> optionalTypes_;
        ComponentQueryEngine* engine_;

        friend class ComponentQueryEngine;
        explicit QueryBuilder(ComponentQueryEngine* engine) : engine_(engine) {}
    };

    /**
     * @brief Creates a new query
     * @return Query builder
     */
    QueryBuilder query() { return QueryBuilder(this); }

    /**
     * @brief Registers a component archetype
     * @tparam ComponentTypes Component types
     * @param archetype Archetype instance
     */
    template <typename... ComponentTypes>
    void registerArchetype(
        std::shared_ptr<ComponentArchetype<ComponentTypes...>> archetype);

private:
    std::unordered_map<std::vector<std::type_index>, std::shared_ptr<void>>
        archetypes_;
    std::mutex archetypeMutex_;
};

}  // namespace atom::components

#endif  // ATOM_COMPONENT_ITERATION_HPP
