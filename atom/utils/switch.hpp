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
#include <string_view>
#include <thread>
#include <tuple>
#include <variant>

#include "atom/containers/high_performance.hpp"
#include "atom/type/noncopyable.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/algorithm/string.hpp>
#include <boost/range/algorithm.hpp>
#endif

namespace atom::utils {

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
 * enhanced with C++20 features
 *
 * This class allows you to register functions associated with string keys,
 * similar to a switch statement in JavaScript. It supports multiple return
 * types using std::variant and provides a default function if no match is
 * found.
 *
 * @tparam ThreadSafe Whether to make the switch thread-safe (default: false)
 * @tparam Args The types of additional arguments to pass to the functions
 */
template <bool ThreadSafe = false, typename... Args>
class StringSwitch : public NonCopyable {
public:
    /**
     * @brief Type alias for custom return types
     * @tparam RetTypes Additional return types to support
     */
    template <typename... RetTypes>
    using CustomReturnType = std::variant<std::monostate, RetTypes...>;

    /**
     * @brief Default return type supporting int and String
     */
    using ReturnType = CustomReturnType<int, String>;

    /**
     * @brief Function type for registered cases
     */
    using Func = std::function<ReturnType(Args...)>;

    /**
     * @brief Optional default function type
     */
    using DefaultFunc = std::optional<Func>;

    /**
     * @brief Performance metrics structure
     */
    struct Stats {
        std::atomic<size_t> totalCalls{0};
        std::atomic<size_t> cacheHits{0};
        std::atomic<size_t> cacheMisses{0};
        std::atomic<size_t> errorCount{0};
        std::atomic<double> avgResponseTime{0.0};
        size_t totalCases{0};

        /**
         * @brief Get a thread-safe snapshot of statistics
         * @return Snapshot structure with current values
         */
        struct Snapshot {
            size_t totalCalls;
            size_t cacheHits;
            size_t cacheMisses;
            double hitRatio;
            double avgResponseTime;
            size_t errorCount;
            size_t totalCases;
        };

        auto getSnapshot() const -> Snapshot {
            Snapshot snapshot;
            snapshot.totalCalls = totalCalls.load(std::memory_order_relaxed);
            snapshot.cacheHits = cacheHits.load(std::memory_order_relaxed);
            snapshot.cacheMisses = cacheMisses.load(std::memory_order_relaxed);
            snapshot.errorCount = errorCount.load(std::memory_order_relaxed);
            snapshot.avgResponseTime =
                avgResponseTime.load(std::memory_order_relaxed);
            snapshot.hitRatio = (snapshot.totalCalls > 0)
                                    ? static_cast<double>(snapshot.cacheHits) /
                                          snapshot.totalCalls
                                    : 0.0;
            snapshot.totalCases = totalCases;
            return snapshot;
        }
    };

    /**
     * @brief Default constructor
     */
    StringSwitch() = default;

    /**
     * @brief Construct with initializer list for easier case registration
     * @param initList List of key-function pairs
     * @throws std::invalid_argument if any key is empty
     */
    StringSwitch(std::initializer_list<std::pair<String, Func>> initList) {
        try {
            for (auto&& [str, func] : initList) {
                if (str.empty()) {
                    throw std::invalid_argument(
                        "Empty key is not allowed in initializer");
                }
                registerCase(str, std::move(func));
            }
        } catch (const std::exception&) {
            clearCases();
            throw;
        }
    }

    /**
     * @brief Register a case with the given string and function
     * @tparam KeyType Type that can be converted to string_view
     * @tparam CallableType Function type compatible with Args...
     * @param str The string key for the case
     * @param func The function to be associated with the string key
     * @throws std::invalid_argument if key is empty
     * @throws std::runtime_error if the case is already registered
     */
    template <CaseKeyType KeyType, SwitchCallable<Args...> CallableType>
    void registerCase(KeyType&& str, CallableType&& func) {
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
     * @brief Unregister a case with the given string
     * @tparam KeyType Type that can be converted to string_view
     * @param str The string key for the case to be unregistered
     * @return True if the case was found and unregistered, false otherwise
     */
    template <CaseKeyType KeyType>
    bool unregisterCase(KeyType&& str) {
        String key{std::string_view(std::forward<KeyType>(str))};

        if constexpr (ThreadSafe) {
            std::unique_lock lock(mutex_);
            return unregisterCaseImpl(key);
        } else {
            return unregisterCaseImpl(key);
        }
    }

    /**
     * @brief Clear all registered cases
     */
    void clearCases() {
        if constexpr (ThreadSafe) {
            std::unique_lock lock(mutex_);
            cases_.clear();
            clearCache();
        } else {
            cases_.clear();
            clearCache();
        }
    }

    /**
     * @brief Match the given string against the registered cases
     * @tparam KeyType Type that can be converted to string_view
     * @param str The string key to match
     * @param args Additional arguments to pass to the function
     * @return The result of the function call, or std::nullopt if no match is
     * found
     */
    template <CaseKeyType KeyType>
    auto match(KeyType&& str, Args... args) -> std::optional<ReturnType> {
        try {
            std::string_view keyView{std::forward<KeyType>(str)};

            if constexpr (ThreadSafe) {
                std::shared_lock lock(mutex_);
            }
            return matchImpl(keyView, args...);
        } catch (const std::exception&) {
            metrics_.errorCount.fetch_add(1, std::memory_order_relaxed);
            return std::nullopt;
        }
    }

    /**
     * @brief Set the default function to be called if no match is found
     * @tparam CallableType Function type compatible with Args...
     * @param func The default function
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
     * @brief Get a vector of all registered cases
     * @return Vector containing all registered string keys
     */
    [[nodiscard]] auto getCases() const -> Vector<String> {
        if constexpr (ThreadSafe) {
            std::shared_lock lock(mutex_);
            return getCasesImpl();
        } else {
            return getCasesImpl();
        }
    }

    /**
     * @brief Match the given string with a span of arguments
     * @tparam KeyType Type that can be converted to string_view
     * @param str The string key to match
     * @param args A span of tupled arguments to pass to the function
     * @return The result of the function call, or std::nullopt if no match is
     * found
     */
    template <CaseKeyType KeyType>
    auto matchWithSpan(KeyType&& str, std::span<const std::tuple<Args...>> args)
        -> std::optional<ReturnType> {
        try {
            std::string_view keyView{std::forward<KeyType>(str)};

            if constexpr (ThreadSafe) {
                std::shared_lock lock(mutex_);
            }
            return matchWithSpanImpl(keyView, args);
        } catch (const std::exception&) {
            metrics_.errorCount.fetch_add(1, std::memory_order_relaxed);
            return std::nullopt;
        }
    }

    /**
     * @brief Match multiple keys in parallel using thread pool
     * @tparam KeyRange Range type containing keys
     * @param keys Range of keys to match
     * @param args Arguments to pass to each matched function
     * @return Vector of results for each key
     */
    template <std::ranges::range KeyRange>
        requires std::convertible_to<std::ranges::range_value_t<KeyRange>,
                                     std::string_view>
    auto matchParallel(const KeyRange& keys, Args... args)
        -> Vector<std::optional<ReturnType>> {
        auto keyCount = std::ranges::distance(keys);
        Vector<std::optional<ReturnType>> results;
        results.reserve(keyCount);

        HashMap<String, Func> casesCopy;
        DefaultFunc defaultFuncCopy;

        {
            if constexpr (ThreadSafe) {
                std::shared_lock lock(mutex_);
            }
            casesCopy = cases_;
            defaultFuncCopy = defaultFunc_;
        }

        // Use standard threads for parallel execution instead of ThreadPool
        const size_t threadCount = std::min({
            static_cast<size_t>(std::thread::hardware_concurrency()),
            static_cast<size_t>(keyCount),
            static_cast<size_t>(8)  // Cap at 8 threads for safety
        });

        Vector<std::future<Vector<std::optional<ReturnType>>>> futures;
        futures.reserve(threadCount);

        auto keyVec = Vector<String>();
        keyVec.reserve(keyCount);
        for (const auto& keyLike : keys) {
            keyVec.emplace_back(std::string_view(keyLike));
        }

        const size_t keysPerThread = keyCount / threadCount;
        const size_t remainder = keyCount % threadCount;

        for (size_t i = 0; i < threadCount; ++i) {
            size_t startIdx = i * keysPerThread;
            size_t endIdx = (i == threadCount - 1)
                                ? startIdx + keysPerThread + remainder
                                : startIdx + keysPerThread;

            futures.push_back(std::async(
                std::launch::async, [&casesCopy, &defaultFuncCopy, &keyVec,
                                     startIdx, endIdx, args...]() {
                    Vector<std::optional<ReturnType>> threadResults;
                    threadResults.reserve(endIdx - startIdx);

                    for (size_t j = startIdx; j < endIdx; ++j) {
                        const auto& key = keyVec[j];
                        try {
                            if (casesCopy.contains(key)) {
                                threadResults.push_back(
                                    std::invoke(casesCopy.at(key), args...));
                            } else if (defaultFuncCopy) {
                                threadResults.push_back(
                                    std::invoke(*defaultFuncCopy, args...));
                            } else {
                                threadResults.push_back(std::nullopt);
                            }
                        } catch (...) {
                            threadResults.push_back(std::nullopt);
                        }
                    }
                    return threadResults;
                }));
        }

        for (auto& future : futures) {
            auto threadResults = future.get();
            for (auto& result : threadResults) {
                results.push_back(std::move(result));
            }
        }

        return results;
    }

    /**
     * @brief Check if a case exists
     * @tparam KeyType Type that can be converted to string_view
     * @param str The string key to check
     * @return True if the case exists, false otherwise
     */
    template <CaseKeyType KeyType>
    bool hasCase(KeyType&& str) const {
        String key{std::string_view(std::forward<KeyType>(str))};

        if constexpr (ThreadSafe) {
            std::shared_lock lock(mutex_);
            return cases_.contains(key);
        } else {
            return cases_.contains(key);
        }
    }

    /**
     * @brief Get performance statistics
     * @return Statistics structure with current metrics
     */
    [[nodiscard]] auto getStats() const -> Stats {
        Stats stats{};

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

        if constexpr (ThreadSafe) {
            std::shared_lock lock(mutex_);
            stats.totalCases = cases_.size();
        } else {
            stats.totalCases = cases_.size();
        }

        return stats;
    }

    /**
     * @brief Reset performance statistics
     */
    void resetStats() noexcept { metrics_.reset(); }

    /**
     * @brief Get the number of registered cases
     * @return Number of cases
     */
    [[nodiscard]] size_t size() const noexcept {
        if constexpr (ThreadSafe) {
            std::shared_lock lock(mutex_);
            return cases_.size();
        } else {
            return cases_.size();
        }
    }

    /**
     * @brief Check if the switch is empty
     * @return True if no cases are registered
     */
    bool empty() const noexcept {
        if constexpr (ThreadSafe) {
            std::shared_lock lock(mutex_);
            return cases_.empty();
        } else {
            return cases_.empty();
        }
    }

private:
    /**
     * @brief Internal metrics structure using atomics
     */
    struct Metrics {
        std::atomic<size_t> totalCalls{0};
        std::atomic<size_t> cacheHits{0};
        std::atomic<size_t> cacheMisses{0};
        std::atomic<size_t> errorCount{0};
        std::atomic<double> avgResponseTime{0.0};

        void reset() noexcept {
            totalCalls.store(0, std::memory_order_relaxed);
            cacheHits.store(0, std::memory_order_relaxed);
            cacheMisses.store(0, std::memory_order_relaxed);
            errorCount.store(0, std::memory_order_relaxed);
            avgResponseTime.store(0.0, std::memory_order_relaxed);
        }

        void updateResponseTime(double newTime) noexcept {
            auto currentAvg = avgResponseTime.load(std::memory_order_relaxed);
            auto count = totalCalls.load(std::memory_order_relaxed);

            double newAvg;
            do {
                if (count > 1) {
                    static constexpr double alpha = 0.1;
                    newAvg = (1.0 - alpha) * currentAvg + alpha * newTime;
                } else {
                    newAvg = newTime;
                }
            } while (!avgResponseTime.compare_exchange_weak(
                currentAvg, newAvg, std::memory_order_relaxed,
                std::memory_order_relaxed));
        }
    };

    template <typename CallableType>
    void registerCaseImpl(const String& key, CallableType&& func) {
        if (cases_.contains(key)) {
            throw std::runtime_error("Case already registered: " + String(key));
        }
        cases_.emplace(key, std::forward<CallableType>(func));
    }

    bool unregisterCaseImpl(const String& key) { return cases_.erase(key) > 0; }

    auto matchImpl(std::string_view keyView, Args... args)
        -> std::optional<ReturnType> {
        const auto startTime = std::chrono::steady_clock::now();
        metrics_.totalCalls.fetch_add(1, std::memory_order_relaxed);

        if (auto cachedResult = checkCache(keyView)) {
            metrics_.cacheHits.fetch_add(1, std::memory_order_relaxed);
            try {
                auto result = std::invoke(*cachedResult, args...);
                auto duration =
                    std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - startTime)
                        .count();
                metrics_.updateResponseTime(duration);
                return result;
            } catch (...) {
                metrics_.errorCount.fetch_add(1, std::memory_order_relaxed);
            }
        }

        metrics_.cacheMisses.fetch_add(1, std::memory_order_relaxed);

        String keyStr{keyView};
        auto it = cases_.find(keyStr);
        if (it != cases_.end()) {
            const auto& func = it->second;
            updateCache(keyStr, func);
            try {
                auto result = std::invoke(func, args...);
                auto duration =
                    std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - startTime)
                        .count();
                metrics_.updateResponseTime(duration);
                return result;
            } catch (...) {
                metrics_.errorCount.fetch_add(1, std::memory_order_relaxed);
            }
        }

        if (defaultFunc_) {
            try {
                auto result = std::invoke(*defaultFunc_, args...);
                auto duration =
                    std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - startTime)
                        .count();
                metrics_.updateResponseTime(duration);
                return result;
            } catch (...) {
                metrics_.errorCount.fetch_add(1, std::memory_order_relaxed);
            }
        }

        return std::nullopt;
    }

    std::optional<Func> checkCache(std::string_view key) const {
        if constexpr (ThreadSafe) {
            std::shared_lock lock(cacheMutex_);
        }
        for (const auto& entry : cache_) {
            if (entry.first == key) {
                return entry.second;
            }
        }
        return std::nullopt;
    }

    void updateCache(const String& key, const Func& func) {
        if constexpr (ThreadSafe) {
            std::unique_lock lock(cacheMutex_);
        }
        cache_[cacheIndex_] = {key, func};
        cacheIndex_ = (cacheIndex_ + 1) % CACHE_SIZE;
    }

    void clearCache() {
        if constexpr (ThreadSafe) {
            std::unique_lock lock(cacheMutex_);
        }
        for (auto& entry : cache_) {
            entry = {};
        }
        cacheIndex_ = 0;
    }

    auto matchWithSpanImpl(std::string_view keyView,
                           std::span<const std::tuple<Args...>> args)
        -> std::optional<ReturnType> {
        String keyStr{keyView};
        auto it = cases_.find(keyStr);
        if (it != cases_.end()) {
            try {
                return std::apply(
                    [&](const auto&... tupleArgs) {
                        return std::invoke(it->second, tupleArgs...);
                    },
                    args[0]);
            } catch (...) {
                metrics_.errorCount.fetch_add(1, std::memory_order_relaxed);
            }
        }

        if (defaultFunc_) {
            try {
                return std::apply(
                    [&](const auto&... tupleArgs) {
                        return std::invoke(*defaultFunc_, tupleArgs...);
                    },
                    args[0]);
            } catch (...) {
                metrics_.errorCount.fetch_add(1, std::memory_order_relaxed);
            }
        }

        return std::nullopt;
    }

    auto getCasesImpl() const -> Vector<String> {
        Vector<String> caseList;
        caseList.reserve(cases_.size());

        for (const auto& [key, _] : cases_) {
            caseList.push_back(key);
        }

        return caseList;
    }

    static constexpr size_t CACHE_SIZE = 16;

    HashMap<String, Func> cases_;
    DefaultFunc defaultFunc_;
    Metrics metrics_{};

    mutable std::array<std::pair<String, Func>, CACHE_SIZE> cache_;
    mutable size_t cacheIndex_ = 0;
    mutable std::conditional_t<ThreadSafe, std::shared_mutex, int> cacheMutex_;
    mutable std::conditional_t<ThreadSafe, std::shared_mutex, int> mutex_;
};

/**
 * @brief Deduction guide for ThreadSafe parameter
 */
template <typename... Args>
StringSwitch(
    std::initializer_list<std::pair<
        String,
        std::function<std::variant<std::monostate, int, String>(Args...)>>>)
    -> StringSwitch<false, Args...>;

}  // namespace atom::utils

#endif  // ATOM_UTILS_SWITCH_HPP
