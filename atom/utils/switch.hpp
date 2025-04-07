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
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

#include "atom/async/pool.hpp"
#include "atom/type/robin_hood.hpp"

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

    using ReturnType = CustomReturnType<int, std::string>;
    using Func = std::function<ReturnType(Args...)>;

    // 返回类型特征
    using DefaultReturnTypes = std::tuple<int, std::string>;

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
        std::string key{std::forward<KeyType>(str)};
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
        std::string key{std::forward<KeyType>(str)};

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
        } else {
            cases_.clear();
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
            std::string_view key{std::forward<KeyType>(str)};

            if constexpr (ThreadSafe) {
                std::shared_lock lock(mutex_);
            }
            return matchImpl(key, args...);
        } catch (const std::exception& e) {
            // Log the exception or handle it appropriately
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
     * @return std::vector<std::string> A vector containing all registered
     * string keys.
     */
    ATOM_NODISCARD auto getCases() const -> std::vector<std::string> {
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
    StringSwitch(std::initializer_list<std::pair<std::string, Func>> initList) {
        try {
            for (auto&& [str, func] : initList) {
                if (str.empty()) {
                    throw std::invalid_argument(
                        "Empty key is not allowed in initializer");
                }
                registerCase(str, std::move(func));
            }
        } catch (const std::exception& e) {
            // Clean up any registered cases and rethrow
            clearCases();
            throw;
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
            std::string_view key{std::forward<KeyType>(str)};

            if constexpr (ThreadSafe) {
                std::shared_lock lock(mutex_);
                return matchWithSpanImpl(key, args);
            } else {
                return matchWithSpanImpl(key, args);
            }
        } catch (const std::exception& e) {
            // Log the exception or handle it appropriately
            return std::nullopt;
        }
    }

    /**
     * @brief Match multiple keys in parallel using C++20 execution policies
     *
     * @param keys Range of keys to match
     * @param args Arguments to pass to each matched function
     * @return std::vector<std::optional<ReturnType>> Results for each key
     */
    template <std::ranges::range KeyRange>
        requires std::convertible_to<std::ranges::range_value_t<KeyRange>,
                                     std::string_view>
    auto matchParallel(const KeyRange& keys, Args... args)
        -> std::vector<std::optional<ReturnType>> {
        auto keyCount = std::ranges::distance(keys);
        std::vector<std::optional<ReturnType>> results;
        results.reserve(keyCount);

        // 使用自适应线程池大小
        const size_t threadCount =
            std::min(static_cast<size_t>(std::thread::hardware_concurrency()),
                     static_cast<size_t>(keyCount));

        // Create local copies of necessary data to avoid race conditions
        std::unordered_map<std::string, Func> cases_copy;
        DefaultFunc defaultFunc_copy;

        {
            std::shared_lock lock(mutex_);
            cases_copy = cases_;
            defaultFunc_copy = defaultFunc_;
        }

        std::vector<std::future<std::optional<ReturnType>>> futures;
        futures.reserve(keyCount);

        // 创建线程池进行任务分配
        async::ThreadPool pool(threadCount);

        // 使用线程池并行处理任务
        for (const auto& key : keys) {
            futures.push_back(
                pool.enqueue([cases_copy, defaultFunc_copy, key, args...]() {
                    std::string_view keyView{key};
                    try {
                        if (cases_copy.contains(std::string{keyView})) {
                            return std::optional<ReturnType>{std::invoke(
                                cases_copy.at(std::string{keyView}), args...)};
                        } else if (defaultFunc_copy) {
                            return std::optional<ReturnType>{
                                std::invoke(*defaultFunc_copy, args...)};
                        }
                    } catch (...) {
                        // Suppress exceptions inside thread
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
        std::string_view key{std::forward<KeyType>(str)};

        if constexpr (ThreadSafe) {
            std::shared_lock lock(mutex_);
            return cases_.contains(std::string{key});
        } else {
            return cases_.contains(std::string{key});
        }
    }

    /**
     * @brief 性能监控结构体
     */
    struct Stats {
        size_t totalCalls{0};
        size_t cacheHits{0};
        size_t cacheMisses{0};
        double hitRatio{0.0};
        double avgResponseTime{0.0};
        size_t errorCount{0};
        size_t totalCases{0};
    };

    /**
     * @brief 获取性能统计信息
     */
    ATOM_NODISCARD auto getStats() const -> Stats {
        Stats stats{};

        const auto& m = metrics_;  // 避免多次成员访问
        stats.totalCalls = m.totalCalls.load(std::memory_order_relaxed);
        stats.cacheHits = m.cacheHits.load(std::memory_order_relaxed);
        stats.cacheMisses = m.cacheMisses.load(std::memory_order_relaxed);
        stats.hitRatio =
            stats.totalCalls > 0
                ? static_cast<double>(stats.cacheHits) / stats.totalCalls
                : 0.0;
        stats.avgResponseTime =
            m.avgResponseTime.load(std::memory_order_relaxed);
        stats.errorCount = m.errorCount.load(std::memory_order_relaxed);

        if constexpr (ThreadSafe) {
            std::shared_lock lock(mutex_);
            stats.totalCases = cases_.size();
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
            auto current = avgResponseTime.load(std::memory_order_relaxed);
            auto count = totalCalls.load(std::memory_order_relaxed);
            if (count > 1) {
                static constexpr double alpha = 0.1;
                avgResponseTime.store((1.0 - alpha) * current + alpha * newTime,
                                      std::memory_order_relaxed);
            } else {
                avgResponseTime.store(newTime, std::memory_order_relaxed);
            }
        }
    };

    Metrics metrics_{};

private:
    template <typename CallableType>
    void registerCaseImpl(const std::string& key, CallableType&& func) {
        if (cases_.contains(key)) {
            THROW_OBJ_ALREADY_EXIST("Case already registered: " + key);
        }
        cases_.emplace(key, std::forward<CallableType>(func));
    }

    bool unregisterCaseImpl(const std::string& key) {
        return cases_.erase(key) > 0;
    }

    auto matchImpl(std::string_view key, Args... args)
        -> std::optional<ReturnType> {
        std::string keyStr{key};

        const auto startTime = std::chrono::steady_clock::now();

        // 更新性能计数
        metrics_.totalCalls.fetch_add(1, std::memory_order_relaxed);
        if (auto cachedResult = checkCache(keyStr)) {
            metrics_.cacheHits.fetch_add(1, std::memory_order_relaxed);
            try {
                auto result = std::invoke(*cachedResult, args...);
                metrics_.updateResponseTime(
                    std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - startTime)
                        .count());
                return result;
            } catch (...) {
                // 缓存调用失败，继续常规查找
                metrics_.errorCount.fetch_add(1, std::memory_order_relaxed);
            }
        }
        metrics_.cacheMisses.fetch_add(1, std::memory_order_relaxed);

        // 常规查找
        if (cases_.contains(keyStr)) {
            const auto& func = cases_.at(keyStr);
            updateCache(keyStr, func);  // 更新缓存
            auto result = std::invoke(func, args...);
            metrics_.updateResponseTime(
                std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                              startTime)
                    .count());
            return result;
        }

        if (defaultFunc_) {
            try {
                auto result = std::invoke(*defaultFunc_, args...);
                metrics_.updateResponseTime(
                    std::chrono::duration<double>(
                        std::chrono::steady_clock::now() - startTime)
                        .count());
                return result;
            } catch (...) {
                // 记录异常并更新计数
                metrics_.errorCount.fetch_add(1, std::memory_order_relaxed);
                return std::nullopt;
            }
        }

        return std::nullopt;
    }

    // 优化的缓存查找
    std::optional<Func> checkCache(const std::string& key) const {
        if constexpr (ThreadSafe) {
            std::shared_lock lock(cacheMutex_);
            for (const auto& entry : cache_) {
                if (entry.first == key) {
                    return entry.second;
                }
            }
        } else {
            for (const auto& entry : cache_) {
                if (entry.first == key) {
                    return entry.second;
                }
            }
        }
        return std::nullopt;
    }

    // 缓存更新策略
    void updateCache(const std::string& key, const Func& func) {
        if constexpr (ThreadSafe) {
            std::unique_lock lock(cacheMutex_);
            cache_[cacheIndex_] = {key, func};
            cacheIndex_ = (cacheIndex_ + 1) % CACHE_SIZE;
        } else {
            cache_[cacheIndex_] = {key, func};
            cacheIndex_ = (cacheIndex_ + 1) % CACHE_SIZE;
        }
    }

    auto matchWithSpanImpl(std::string_view key,
                           std::span<const std::tuple<Args...>> args)
        -> std::optional<ReturnType> {
        std::string keyStr{key};
        if (cases_.contains(keyStr)) {
            try {
                return std::apply(
                    [&](const auto&... tuple_args) {
                        return std::invoke(cases_.at(keyStr), tuple_args...);
                    },
                    args[0]);
            } catch (...) {
                if (defaultFunc_) {
                    try {
                        return std::apply(
                            [&](const auto&... tuple_args) {
                                return std::invoke(*defaultFunc_,
                                                   tuple_args...);
                            },
                            args[0]);
                    } catch (...) {
                        return std::nullopt;
                    }
                }
                return std::nullopt;
            }
        }

        if (defaultFunc_) {
            try {
                return std::apply(
                    [&](const auto&... tuple_args) {
                        return std::invoke(*defaultFunc_, tuple_args...);
                    },
                    args[0]);
            } catch (...) {
                return std::nullopt;
            }
        }

        return std::nullopt;
    }

    auto getCasesImpl() const -> std::vector<std::string> {
        std::vector<std::string> caseList;
        caseList.reserve(cases_.size());

        // Use C++20 ranges when available
        if constexpr (std::ranges::range<decltype(cases_)>) {
            auto keysView = cases_ | std::views::keys;
            std::ranges::copy(keysView, std::back_inserter(caseList));
        } else {
            for (const auto& [key, _] : cases_) {
                caseList.push_back(key);
            }
        }

        return caseList;
    }

    // 使用优化的哈希表实现
    atom::utils::unordered_flat_map<std::string, Func> cases_;

    DefaultFunc defaultFunc_;  ///< The default function to be called if no
                               ///< match is found.

    // LRU缓存配置
    static constexpr size_t CACHE_SIZE = 16;
    mutable std::array<std::pair<std::string, Func>, CACHE_SIZE> cache_;
    mutable size_t cacheIndex_ = 0;
    mutable std::shared_mutex cacheMutex_;  // 缓存同步

    // 性能统计计数器
    mutable std::atomic<size_t> totalCalls_{0};
    mutable std::atomic<size_t> cacheHits_{0};
    mutable std::atomic<size_t> cacheMisses_{0};
    mutable std::atomic<size_t> errorCount_{0};
    mutable std::atomic<double> avgResponseTime_{0.0};

    // Thread synchronization (only used if ThreadSafe is true)
    mutable std::shared_mutex mutex_;
};

// Deduction guide for ThreadSafe parameter
template <typename... Args>
StringSwitch(std::initializer_list<std::pair<
                 std::string, std::function<std::variant<
                                  std::monostate, int, std::string>(Args...)>>>)
    -> StringSwitch<false, Args...>;

}  // namespace atom::utils

#endif  // ATOM_UTILS_SWITCH_HPP