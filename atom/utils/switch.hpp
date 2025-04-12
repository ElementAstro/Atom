#ifndef ATOM_UTILS_SWITCH_HPP
#define ATOM_UTILS_SWITCH_HPP

#include <array>
#include <atomic>
#include <chrono>
#include <concepts>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <ranges>
#include <shared_mutex>
#include <span>
// #include <string> // Replaced by high_performance.hpp
#include <string_view>  // Keep for string_view concept and temporary views
#include <thread>
// #include <unordered_map> // Replaced by high_performance.hpp
#include <variant>
// #include <vector> // Replaced by high_performance.hpp
#include <tuple>  // Needed for std::apply

#include "atom/async/pool.hpp"
#include "atom/containers/high_performance.hpp"  // Include high performance containers
// #include "atom/type/robin_hood.hpp" // Seems unused, consider removing if
// confirmed

#ifdef __cpp_lib_simd
#include <simd>
#endif

#ifdef ATOM_USE_BOOST
#include <boost/algorithm/cxx11/contains.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/find.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/optional.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/iterator_range.hpp>
#endif

#include "atom/error/exception.hpp"
#include "atom/macro.hpp"
#include "atom/type/noncopyable.hpp"

namespace atom::utils {

// Use type aliases from high_performance.hpp
using atom::containers::String;
template <typename K, typename V>
using HashMap = atom::containers::HashMap<K, V>;
template <typename T>
using Vector = atom::containers::Vector<T>;

/**
 * @brief Concept for valid case key types that can be used with StringSwitch
 */
template <typename T>
concept CaseKeyType = std::convertible_to<T, std::string_view>;

/**
 * @brief Concept for valid function types that can be used with StringSwitch
 */
template <typename F, typename... Args>
concept SwitchCallable = std::invocable<F, Args...>;

/**
 * @brief A class for implementing a switch statement with string cases,
 * enhanced with C++20 features and optional Boost support.
 *
 * This class allows you to register functions associated with string keys,
 * similar to a switch statement in JavaScript. It supports multiple return
 * types using std::variant and provides a default function if no match is
 * found.
 *
 * @tparam ThreadSafe Whether to make the switch thread-safe (default: false)
 * @tparam Args The types of additional arguments to pass to the functions.
 */
template <bool ThreadSafe = false, typename... Args>
class StringSwitch : public NonCopyable {
private:
public:
    /**
     * @brief Type alias for the function to be registered.
     *
     * The function can return a std::variant containing either std::monostate,
     * int, or std::string.
     */
    // 支持自定义返回类型的模板参数
    template <typename... RetTypes>
    using CustomReturnType = std::variant<std::monostate, RetTypes...>;

    // Use String from high_performance.hpp in the default return types
    using ReturnType = CustomReturnType<int, String>;
    using Func = std::function<ReturnType(Args...)>;

    // 返回类型特征
    using DefaultReturnTypes = std::tuple<int, String>;

    /**
     * @brief Type alias for the default function.
     *
     * The default function is optional and can be set to handle cases where no
     * match is found.
     */
    using DefaultFunc = std::optional<Func>;

    /**
     * @brief Default constructor.
     */
    StringSwitch() = default;

    /**
     * @brief Register a case with the given string and function.
     *
     * @param str The string key for the case.
     * @param func The function to be associated with the string key.
     * @throws std::runtime_error if the case is already registered.
     */
    template <CaseKeyType KeyType, SwitchCallable<Args...> CallableType>
    void registerCase(KeyType&& str, CallableType&& func) {
        // Construct String from the input key type
        String key{std::string_view(std::forward<KeyType>(str))};
        if (key.empty()) {
            throw std::invalid_argument("Empty key is not allowed");
        }

        if constexpr (ThreadSafe) {
            std::unique_lock lock(mutex_);
            registerCaseImpl(key, std::forward<CallableType>(func));
        } else {
            registerCaseImpl(key, std::forward<CallableType>(func));
        }
    }

    /**
     * @brief Unregister a case with the given string.
     *
     * @param str The string key for the case to be unregistered.
     * @return bool True if the case was found and unregistered, false
     * otherwise.
     */
    template <CaseKeyType KeyType>
    bool unregisterCase(KeyType&& str) {
        // Construct String from the input key type
        String key{std::string_view(std::forward<KeyType>(str))};

        if constexpr (ThreadSafe) {
            std::unique_lock lock(mutex_);
            return unregisterCaseImpl(key);
        } else {
            return unregisterCaseImpl(key);
        }
    }

    /**
     * @brief Clear all registered cases.
     */
    void clearCases() {
        if constexpr (ThreadSafe) {
            std::unique_lock lock(mutex_);
            cases_.clear();
            // Also clear cache when clearing cases
            clearCache();
        } else {
            cases_.clear();
            clearCache();
        }
    }

    /**
     * @brief Match the given string against the registered cases.
     *
     * @param str The string key to match.
     * @param args Additional arguments to pass to the function.
     * @return std::optional<ReturnType> The result of the function call,
     *         or std::nullopt if no match is found.
     */
    template <CaseKeyType KeyType>
    auto match(KeyType&& str, Args... args) -> std::optional<ReturnType> {
        try {
            // Use std::string_view for initial lookup efficiency
            std::string_view keyView{std::forward<KeyType>(str)};

            if constexpr (ThreadSafe) {
                std::shared_lock lock(mutex_);
                // No lock needed for matchImpl as it handles cache locking
                // internally
            }
            // Pass string_view to matchImpl
            return matchImpl(keyView, args...);
        } catch (const std::exception& e) {
            // Log the exception or handle it appropriately
            metrics_.errorCount.fetch_add(1, std::memory_order_relaxed);
            return std::nullopt;
        }
    }

    /**
     * @brief Set the default function to be called if no match is found.
     *
     * @param func The default function.
     */
    template <SwitchCallable<Args...> CallableType>
    void setDefault(CallableType&& func) {
        if constexpr (ThreadSafe) {
            std::unique_lock lock(mutex_);
            defaultFunc_ = std::forward<CallableType>(func);
        } else {
            defaultFunc_ = std::forward<CallableType>(func);
        }
    }

    /**
     * @brief Get a vector of all registered cases.
     *
     * @return Vector<String> A vector containing all registered
     * string keys.
     */
    ATOM_NODISCARD auto getCases() const -> Vector<String> {
        if constexpr (ThreadSafe) {
            std::shared_lock lock(mutex_);
            return getCasesImpl();
        } else {
            return getCasesImpl();
        }
    }

    /**
     * @brief C++20 designated initializers for easier case registration.
     *
     * @param initList An initializer list of pairs containing string keys and
     * functions.
     */
    // Use String in initializer list
    StringSwitch(std::initializer_list<std::pair<String, Func>> initList) {
        try {
            for (auto&& [str, func] : initList) {
                if (str.empty()) {
                    throw std::invalid_argument(
                        "Empty key is not allowed in initializer");
                }
                // No need to construct String again, it's already String
                registerCase(str, std::move(func));
            }
        } catch (const std::exception& e) {
            // Clean up any registered cases and rethrow
            clearCases();
            throw;  // Rethrow the original exception
        }
    }

    /**
     * @brief Match the given string against the registered cases with a span of
     * arguments.
     *
     * @param str The string key to match.
     * @param args A span of additional arguments to pass to the function.
     * @return std::optional<ReturnType> The result of the function call,
     *         or std::nullopt if no match is found.
     */
    template <CaseKeyType KeyType>
    auto matchWithSpan(KeyType&& str, std::span<const std::tuple<Args...>> args)
        -> std::optional<ReturnType> {
        try {
            // Use std::string_view for initial lookup efficiency
            std::string_view keyView{std::forward<KeyType>(str)};

            if constexpr (ThreadSafe) {
                std::shared_lock lock(mutex_);
                // No lock needed for matchWithSpanImpl as it handles cache
                // locking internally
            }
            // Pass string_view to matchWithSpanImpl
            return matchWithSpanImpl(keyView, args);
        } catch (const std::exception& e) {
            // Log the exception or handle it appropriately
            metrics_.errorCount.fetch_add(1, std::memory_order_relaxed);
            return std::nullopt;
        }
    }

    /**
     * @brief Match multiple keys in parallel using C++20 execution policies
     *
     * @param keys Range of keys to match
     * @param args Arguments to pass to each matched function
     * @return Vector<std::optional<ReturnType>> Results for each key
     */
    template <std::ranges::range KeyRange>
        requires std::convertible_to<std::ranges::range_value_t<KeyRange>,
                                     std::string_view>
    auto matchParallel(const KeyRange& keys, Args... args)
        -> Vector<std::optional<ReturnType>> {
        auto keyCount = std::ranges::distance(keys);
        Vector<std::optional<ReturnType>> results;
        results.reserve(keyCount);

        // 使用自适应线程池大小
        const size_t threadCount = std::min(
            {static_cast<size_t>(std::thread::hardware_concurrency()),
             static_cast<size_t>(keyCount)});  // Consider pool max threads

        // Create local copies of necessary data to avoid race conditions
        HashMap<String, Func> cases_copy;
        DefaultFunc defaultFunc_copy;

        {
            // Use shared lock for reading cases_ and defaultFunc_
            std::shared_lock lock(mutex_);
            cases_copy = cases_;
            defaultFunc_copy = defaultFunc_;
        }

        Vector<std::future<std::optional<ReturnType>>> futures;
        futures.reserve(keyCount);

        // 创建线程池进行任务分配
        async::ThreadPool pool(threadCount);

        // 使用线程池并行处理任务
        for (const auto& key_like : keys) {
            // Capture key_like by value for the lambda
            futures.push_back(pool.enqueue(
                [&cases_copy, &defaultFunc_copy, key_like, args...]() {
                    // Convert key_like to String inside the thread task
                    String key{std::string_view(key_like)};
                    try {
                        // Use the copied map
                        if (cases_copy.contains(key)) {
                            return std::optional<ReturnType>{
                                std::invoke(cases_copy.at(key), args...)};
                        } else if (defaultFunc_copy) {
                            // Use the copied default function
                            return std::optional<ReturnType>{
                                std::invoke(*defaultFunc_copy, args...)};
                        }
                    } catch (...) {
                        // Suppress exceptions inside thread, maybe log?
                        // Consider adding error tracking per future if needed
                    }
                    return std::optional<ReturnType>{std::nullopt};
                }));
        }

        // Collect results
        for (auto& future : futures) {
            results.push_back(future.get());
        }

        return results;
    }

    /**
     * @brief Check if a case exists
     *
     * @param str The string key to check
     * @return bool True if the case exists, false otherwise
     */
    template <CaseKeyType KeyType>
    bool hasCase(KeyType&& str) const {
        // Construct String for lookup
        String key{std::string_view(std::forward<KeyType>(str))};

        if constexpr (ThreadSafe) {
            std::shared_lock lock(mutex_);
            return cases_.contains(key);
        } else {
            return cases_.contains(key);
        }
    }

    /**
     * @brief 性能监控结构体
     */
    struct Stats {
        std::atomic<size_t> totalCalls{
            0};  // Use atomic for thread-safe reading if needed elsewhere
        std::atomic<size_t> cacheHits{0};
        std::atomic<size_t> cacheMisses{0};
        double hitRatio{0.0};  // Calculated, not atomic
        std::atomic<double> avgResponseTime{
            0.0};  // Use atomic double if available/needed, otherwise handle
                   // carefully
        std::atomic<size_t> errorCount{0};
        size_t totalCases{0};  // Read under lock, not atomic

        // Provide a method to get non-atomic values safely
        auto getSnapshot() const {
            struct Snapshot {
                size_t totalCalls;
                size_t cacheHits;
                size_t cacheMisses;
                double hitRatio;
                double avgResponseTime;
                size_t errorCount;
                size_t totalCases;
            };
            Snapshot s;
            s.totalCalls = totalCalls.load(std::memory_order_relaxed);
            s.cacheHits = cacheHits.load(std::memory_order_relaxed);
            s.cacheMisses = cacheMisses.load(std::memory_order_relaxed);
            s.hitRatio = (s.totalCalls > 0)
                             ? static_cast<double>(s.cacheHits) / s.totalCalls
                             : 0.0;
            // For atomic double, load directly. If not atomic, needs careful
            // handling or locking. Assuming metrics_.avgResponseTime is
            // atomic<double> or similar.
            s.avgResponseTime = avgResponseTime.load(std::memory_order_relaxed);
            s.errorCount = errorCount.load(std::memory_order_relaxed);
            s.totalCases = totalCases;  // Already read under lock in getStats
            return s;
        }
    };

    /**
     * @brief 获取性能统计信息
     */
    ATOM_NODISCARD auto getStats() const -> Stats {
        Stats stats{};  // Create non-atomic struct

        // Load atomic values into the non-atomic struct
        stats.totalCalls.store(
            metrics_.totalCalls.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        stats.cacheHits.store(
            metrics_.cacheHits.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        stats.cacheMisses.store(
            metrics_.cacheMisses.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        stats.avgResponseTime.store(
            metrics_.avgResponseTime.load(std::memory_order_relaxed),
            std::memory_order_relaxed);
        stats.errorCount.store(
            metrics_.errorCount.load(std::memory_order_relaxed),
            std::memory_order_relaxed);

        // Calculate hit ratio based on loaded values
        size_t total = stats.totalCalls.load(std::memory_order_relaxed);
        stats.hitRatio = (total > 0) ? static_cast<double>(stats.cacheHits.load(
                                           std::memory_order_relaxed)) /
                                           total
                                     : 0.0;

        // Read totalCases under lock
        if constexpr (ThreadSafe) {
            std::shared_lock lock(mutex_);
            stats.totalCases = cases_.size();
        } else {
            stats.totalCases = cases_.size();
        }

        return stats;
    }

    /**
     * @brief 重置性能统计
     */
    void resetStats() noexcept { metrics_.reset(); }

    ATOM_NODISCARD size_t size() const noexcept {
        if constexpr (ThreadSafe) {
            std::shared_lock lock(mutex_);
            return cases_.size();
        } else {
            return cases_.size();
        }
    }

    bool empty() const noexcept {
        if constexpr (ThreadSafe) {
            std::shared_lock lock(mutex_);
            return cases_.empty();
        } else {
            return cases_.empty();
        }
    }

private:
    // Internal Metrics struct using atomics
    struct Metrics {
        std::atomic<size_t> totalCalls{0};
        std::atomic<size_t> cacheHits{0};
        std::atomic<size_t> cacheMisses{0};
        std::atomic<size_t> errorCount{0};
        // std::atomic<double> is C++20, use custom implementation or lock if
        // needed for pre-C++20 Assuming std::atomic<double> is available or a
        // suitable replacement exists
        std::atomic<double> avgResponseTime{0.0};

        void reset() noexcept {
            totalCalls.store(0, std::memory_order_relaxed);
            cacheHits.store(0, std::memory_order_relaxed);
            cacheMisses.store(0, std::memory_order_relaxed);
            errorCount.store(0, std::memory_order_relaxed);
            avgResponseTime.store(0.0, std::memory_order_relaxed);
        }

        // Exponential Moving Average for response time
        void updateResponseTime(double newTime) noexcept {
            auto currentAvg = avgResponseTime.load(std::memory_order_relaxed);
            // Use fetch_add to safely increment count and get previous value
            auto count = totalCalls.fetch_add(
                0, std::memory_order_relaxed);  // Read current count

            // Use compare-exchange loop for robust atomic update of double
            double newAvg;
            do {
                if (count > 1) {
                    // Simple EMA calculation
                    static constexpr double alpha = 0.1;  // Smoothing factor
                    newAvg = (1.0 - alpha) * currentAvg + alpha * newTime;
                } else {
                    newAvg = newTime;  // First measurement
                }
            } while (!avgResponseTime.compare_exchange_weak(
                currentAvg, newAvg, std::memory_order_relaxed,
                std::memory_order_relaxed));
        }
    };

    Metrics metrics_{};

private:
    template <typename CallableType>
    // Accept String by const reference
    void registerCaseImpl(const String& key, CallableType&& func) {
        if (cases_.contains(key)) {
            // Use String in exception message
            THROW_OBJ_ALREADY_EXIST("Case already registered: " + String(key));
        }
        // Emplace with the String key
        cases_.emplace(key, std::forward<CallableType>(func));
    }

    // Accept String by const reference
    bool unregisterCaseImpl(const String& key) { return cases_.erase(key) > 0; }

    // Accept std::string_view for lookup efficiency
    auto matchImpl(std::string_view keyView, Args... args)
        -> std::optional<ReturnType> {
        const auto startTime = std::chrono::steady_clock::now();
        metrics_.totalCalls.fetch_add(1, std::memory_order_relaxed);

        // Check cache using string_view
        if (auto cachedResult = checkCache(keyView)) {
            metrics_.cacheHits.fetch_add(1, std::memory_order_relaxed);
            try {
                auto result = std::invoke(*cachedResult, args...);
                // Calculate duration and update response time
                auto duration =
                    std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - startTime)
                        .count();
                metrics_.updateResponseTime(duration);
                return result;
            } catch (...) {
                metrics_.errorCount.fetch_add(1, std::memory_order_relaxed);
                // Fall through to regular lookup if cache call fails
            }
        }

        metrics_.cacheMisses.fetch_add(1, std::memory_order_relaxed);

        // Regular lookup requires constructing String
        String keyStr{keyView};
        auto it = cases_.find(keyStr);
        if (it != cases_.end()) {
            const auto& func = it->second;
            updateCache(keyStr, func);  // Update cache with String key
            try {
                auto result = std::invoke(func, args...);
                // Calculate duration and update response time
                auto duration =
                    std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - startTime)
                        .count();
                metrics_.updateResponseTime(duration);
                return result;
            } catch (...) {
                metrics_.errorCount.fetch_add(1, std::memory_order_relaxed);
                // Fall through to default if case function throws
            }
        }

        // Try default function
        if (defaultFunc_) {
            try {
                auto result = std::invoke(*defaultFunc_, args...);
                // Calculate duration and update response time
                auto duration =
                    std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - startTime)
                        .count();
                metrics_.updateResponseTime(duration);
                return result;
            } catch (...) {
                metrics_.errorCount.fetch_add(1, std::memory_order_relaxed);
                return std::nullopt;  // Default function failed
            }
        }

        return std::nullopt;  // No match and no default (or default failed)
    }

    // Optimized cache lookup using string_view
    std::optional<Func> checkCache(std::string_view key) const {
        // Use shared lock for reading cache
        std::shared_lock lock(cacheMutex_);
        // Iterate through the cache array
        for (const auto& entry : cache_) {
            // Compare string_view directly with cached String
            // Assuming String is comparable with std::string_view
            if (entry.first == key) {
                return entry.second;  // Return function if found
            }
        }
        return std::nullopt;  // Not found in cache
    }

    // Cache update strategy (LRU-like via simple round-robin index)
    // Accept String by const reference
    void updateCache(const String& key, const Func& func) {
        // Use unique lock for writing to cache
        std::unique_lock lock(cacheMutex_);
        // Overwrite the entry at the current index
        cache_[cacheIndex_] = {key, func};
        // Move to the next index, wrapping around
        cacheIndex_ = (cacheIndex_ + 1) % CACHE_SIZE;
    }

    // Clear the cache (called when cases are cleared)
    void clearCache() {
        if constexpr (ThreadSafe) {
            std::unique_lock lock(cacheMutex_);
            // Reset cache entries (optional, depending on desired behavior)
            for (auto& entry : cache_) {
                entry =
                    {};  // Reset pair to default (empty string, default Func)
            }
            cacheIndex_ = 0;
        } else {
            for (auto& entry : cache_) {
                entry = {};
            }
            cacheIndex_ = 0;
        }
    }

    // Accept std::string_view for lookup efficiency
    auto matchWithSpanImpl(std::string_view keyView,
                           std::span<const std::tuple<Args...>> args)
        -> std::optional<ReturnType> {
        // Construct String only if needed for map lookup
        String keyStr{keyView};
        auto it = cases_.find(keyStr);
        if (it != cases_.end()) {
            try {
                // Apply the first tuple in the span to the function
                return std::apply(
                    [&](const auto&... tuple_args) {
                        return std::invoke(it->second, tuple_args...);
                    },
                    args[0]);
            } catch (...) {
                metrics_.errorCount.fetch_add(1, std::memory_order_relaxed);
                // Fall through to default if case function throws
            }
        }

        if (defaultFunc_) {
            try {
                // Apply the first tuple in the span to the default function
                return std::apply(
                    [&](const auto&... tuple_args) {
                        return std::invoke(*defaultFunc_, tuple_args...);
                    },
                    args[0]);
            } catch (...) {
                metrics_.errorCount.fetch_add(1, std::memory_order_relaxed);
                return std::nullopt;  // Default function failed
            }
        }

        return std::nullopt;  // No match and no default (or default failed)
    }

    auto getCasesImpl() const -> Vector<String> {
        Vector<String> caseList;
        caseList.reserve(cases_.size());

        // Use C++20 ranges if HashMap supports it, otherwise iterate
        // Assuming HashMap provides iterators like std::unordered_map
        for (const auto& [key, _] : cases_) {
            caseList.push_back(key);
        }
        // TODO: If HashMap supports ranges directly:
        // if constexpr (std::ranges::range<decltype(cases_)>) {
        //     auto keysView = cases_ | std::views::keys;
        //     std::ranges::copy(keysView, std::back_inserter(caseList));
        // } else { ... }

        return caseList;
    }

    // Use HashMap from high_performance.hpp
    HashMap<String, Func> cases_;

    DefaultFunc defaultFunc_;  ///< The default function to be called if no
                               ///< match is found.

    // LRU-like cache configuration
    static constexpr size_t CACHE_SIZE = 16;  // Keep cache size reasonable
    // Use String in cache pair
    mutable std::array<std::pair<String, Func>, CACHE_SIZE> cache_;
    mutable size_t cacheIndex_ = 0;
    // Mutex for synchronizing cache access (read/write)
    mutable std::shared_mutex cacheMutex_;

    // Thread synchronization for cases_ and defaultFunc_ (only used if
    // ThreadSafe is true)
    mutable std::shared_mutex mutex_;
};

// Deduction guide for ThreadSafe parameter
// Update deduction guide to use String
template <typename... Args>
StringSwitch(
    std::initializer_list<std::pair<
        String,
        std::function<std::variant<std::monostate, int, String>(Args...)>>>)
    -> StringSwitch<false, Args...>;

}  // namespace atom::utils

#endif  // ATOM_UTILS_SWITCH_HPP