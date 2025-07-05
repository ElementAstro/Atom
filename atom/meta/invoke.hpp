/*!
 * \file invoke.hpp
 * \brief High-performance function invocation utilities with C++20/23 features
 * \author Max Qian <lightapt.com>, Enhanced by Claude AI
 * \date 2023-03-29, Updated 2025-05-26
 */

#ifndef ATOM_META_INVOKE_HPP
#define ATOM_META_INVOKE_HPP

#include <chrono>
#include <concepts>
#include <exception>
#include <format>
#include <functional>
#include <future>
#include <latch>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <source_location>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "atom/error/exception.hpp"
#include "atom/type/expected.hpp"

#ifdef ATOM_USE_BOOST
#include <boost/any.hpp>
#include <boost/asio.hpp>
#include <boost/functional.hpp>
#include <boost/thread.hpp>
#endif

namespace atom::meta {

/**
 * \brief Result type for function calls using expected
 * \tparam T The expected result type
 */
template <typename T>
using Result = type::expected<T, std::error_code>;

namespace detail {
/**
 * \brief Hash function implementation for tuples
 * \tparam Tuple The tuple type to hash
 * \tparam Is Index sequence for tuple elements
 * \param t The tuple to hash
 * \param Index sequence
 * \return Hash value
 */
template <typename Tuple, std::size_t... Is>
constexpr std::size_t hash_tuple_impl(const Tuple& t,
                                      std::index_sequence<Is...>) noexcept {
    std::size_t seed = 0;
    ((seed ^= std::hash<std::tuple_element_t<Is, Tuple>>{}(std::get<Is>(t)) +
              0x9e3779b9 + (seed << 6) + (seed >> 2)),
     ...);
    return seed;
}

/**
 * \brief Hash function for tuples
 * \tparam Args Tuple element types
 * \param t The tuple to hash
 * \return Hash value
 */
template <typename... Args>
constexpr std::size_t hash_tuple(const std::tuple<Args...>& t) noexcept {
    return hash_tuple_impl(t, std::index_sequence_for<Args...>{});
}

/**
 * \brief Format exception message with nested exception support
 * \param e The exception to format
 * \param level Nesting level for indentation
 * \return Formatted exception message
 */
[[nodiscard]] inline std::string format_exception_message(
    const std::exception& e, int level = 0) {
    std::string result(level * 2, ' ');
    result += e.what();

    try {
        std::rethrow_if_nested(e);
    } catch (const std::exception& nested) {
        result += "\n" + format_exception_message(nested, level + 1);
    } catch (...) {
        result += "\n  Unknown nested exception";
    }

    return result;
}
}  // namespace detail

/**
 * \brief Tuple hasher for use in hash maps
 */
struct TupleHasher {
    /**
     * \brief Hash operator for tuples
     * \tparam Args Tuple element types
     * \param t The tuple to hash
     * \return Hash value
     */
    template <typename... Args>
    std::size_t operator()(const std::tuple<Args...>& t) const noexcept {
        return detail::hash_tuple(t);
    }
};

/**
 * \brief Function call metadata for diagnostics
 */
struct FunctionCallInfo {
    std::string_view function_name;
    std::source_location location;
    std::chrono::system_clock::time_point timestamp;

    /**
     * \brief Constructor with function name and source location
     * \param name Function name
     * \param loc Source location
     */
    FunctionCallInfo(
        std::string_view name = {},
        std::source_location loc = std::source_location::current()) noexcept
        : function_name(name),
          location(loc),
          timestamp(std::chrono::system_clock::now()) {}

    /**
     * \brief Convert to string representation
     * \return Formatted string with call information
     */
    [[nodiscard]] std::string to_string() const {
        return std::format(
            "Function: {}, File: {}:{}, Line: {}, Column: {}, Time: {}",
            function_name, location.file_name(), location.function_name(),
            location.line(), location.column(),
            std::chrono::system_clock::to_time_t(timestamp));
    }
};

/**
 * \brief Validates arguments before function invocation
 * \tparam Validator Validator function type
 * \tparam Func Function type to invoke
 * \param validator Function that validates arguments
 * \param func Function to invoke if validation passes
 * \return Callable that validates before invoking
 */
template <typename Validator, typename Func>
[[nodiscard]] constexpr auto validate_then_invoke(Validator&& validator,
                                                  Func&& func) {
    return
        [validator = std::forward<Validator>(validator),
         func = std::forward<Func>(func)](
            auto&&... args) -> std::invoke_result_t<Func, decltype(args)...> {
            if (!std::invoke(validator, args...)) {
                THROW_INVALID_ARGUMENT("Input validation failed");
            }
            return std::invoke(func, std::forward<decltype(args)>(args)...);
        };
}

/**
 * \brief Creates a delayed invocation callable
 * \tparam F Function type
 * \tparam Args Argument types
 * \param func Function to be invoked later
 * \param args Arguments to be captured
 * \return Callable that invokes function with captured arguments
 */
template <typename F, typename... Args>
    requires std::invocable<std::decay_t<F>, std::decay_t<Args>...>
[[nodiscard]] constexpr auto delayInvoke(F&& func, Args&&... args) {
    return
        [func = std::forward<F>(func),
         args_tuple = std::make_tuple(std::forward<Args>(
             args)...)]() mutable noexcept(std::is_nothrow_invocable_v<F,
                                                                       Args...>)
            -> std::invoke_result_t<F, Args...> {
            return std::apply(std::move(func), std::move(args_tuple));
        };
}

/**
 * \brief Creates a delayed member function invocation
 * \tparam R Return type
 * \tparam T Class type
 * \tparam Args Argument types
 * \param func Member function pointer
 * \param obj Object pointer
 * \return Callable that invokes member function
 */
template <typename R, typename T, typename... Args>
[[nodiscard]] constexpr auto delayMemInvoke(R (T::*func)(Args...), T* obj) {
    static_assert(std::is_member_function_pointer_v<decltype(func)>);

    return [func, obj](Args... args) noexcept(
               noexcept((obj->*func)(std::forward<Args>(args)...))) -> R {
        if (obj == nullptr) [[unlikely]] {
            THROW_INVALID_ARGUMENT("Null object pointer in delayMemInvoke");
        }
        return (obj->*func)(std::forward<Args>(args)...);
    };
}

/**
 * \brief Creates a delayed const member function invocation
 * \tparam R Return type
 * \tparam T Class type
 * \tparam Args Argument types
 * \param func Const member function pointer
 * \param obj Const object pointer
 * \return Callable that invokes const member function
 */
template <typename R, typename T, typename... Args>
[[nodiscard]] constexpr auto delayMemInvoke(R (T::*func)(Args...) const,
                                            const T* obj) {
    static_assert(std::is_member_function_pointer_v<decltype(func)>);

    return [func, obj](Args... args) noexcept(
               noexcept((obj->*func)(std::forward<Args>(args)...))) -> R {
        if (obj == nullptr) [[unlikely]] {
            THROW_INVALID_ARGUMENT("Null object pointer in delayMemInvoke");
        }
        return (obj->*func)(std::forward<Args>(args)...);
    };
}

/**
 * \brief Creates a delayed member variable access
 * \tparam T Class type
 * \tparam M Member type
 * \param memberVar Member variable pointer
 * \param obj Object pointer
 * \return Callable that returns member variable reference
 */
template <typename T, typename M>
[[nodiscard]] constexpr auto delayMemberVarInvoke(M T::* memberVar, T* obj) {
    return [memberVar, obj]() -> M& {
        if (obj == nullptr) [[unlikely]] {
            THROW_INVALID_ARGUMENT(
                "Null object pointer in delayMemberVarInvoke");
        }
        return (obj->*memberVar);
    };
}

/**
 * \brief Creates a type-erased deferred callable
 * \tparam R Return type
 * \tparam F Function type
 * \tparam Args Argument types
 * \param func Function to wrap
 * \param args Arguments to store
 * \return Type-erased callable
 */
template <typename R, typename F, typename... Args>
    requires std::invocable<std::decay_t<F>, std::decay_t<Args>...> &&
             std::convertible_to<
                 std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>,
                 R>
[[nodiscard]] std::function<R()> makeDeferred(F&& func, Args&&... args) {
    return [func = std::forward<F>(func),
            args_tuple =
                std::make_tuple(std::forward<Args>(args)...)]() mutable -> R {
        return static_cast<R>(
            std::apply(std::move(func), std::move(args_tuple)));
    };
}

/**
 * \brief Composes multiple functions into a single function
 * \tparam F First function type
 * \tparam Gs Additional function types
 * \param f First function
 * \param gs Additional functions
 * \return Function composition g(f(x))
 */
template <typename F, typename... Gs>
    requires(sizeof...(Gs) > 0)
[[nodiscard]] constexpr auto compose(F&& f, Gs&&... gs) {
    if constexpr (sizeof...(Gs) == 1) {
        return [f = std::forward<F>(f),
                g = std::get<0>(std::forward_as_tuple(gs...))](auto&&... args)
                   -> decltype(g(f(std::forward<decltype(args)>(args)...))) {
            return g(f(std::forward<decltype(args)>(args)...));
        };
    } else {
        auto composed_rest = compose(std::forward<Gs>(gs)...);
        return [f = std::forward<F>(f),
                composed_rest = std::move(composed_rest)](auto&&... args) {
            return composed_rest(f(std::forward<decltype(args)>(args)...));
        };
    }
}

/**
 * \brief Transforms arguments before function invocation
 * \tparam Transform Transformation function type
 * \tparam Func Target function type
 * \param transform Function to transform arguments
 * \param func Target function
 * \return Callable that transforms arguments then invokes function
 */
template <typename Transform, typename Func>
[[nodiscard]] constexpr auto transform_args(Transform&& transform,
                                            Func&& func) {
    return [transform = std::forward<Transform>(transform),
            func = std::forward<Func>(func)](auto&&... args) {
        return std::invoke(
            func,
            std::invoke(transform, std::forward<decltype(args)>(args))...);
    };
}

/**
 * \brief Safely calls a function, returning Result type
 * \tparam Func Function type
 * \tparam Args Argument types
 * \param func Function to call
 * \param args Arguments to pass
 * \return Result containing either the function result or an error
 */
template <typename Func, typename... Args>
    requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
[[nodiscard]] auto safeCall(Func&& func, Args&&... args)
    -> Result<std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>> {
    using ReturnType =
        std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>;

    try {
        if constexpr (std::is_void_v<ReturnType>) {
            std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
            return Result<ReturnType>{std::in_place};
        } else {
            return Result<ReturnType>{std::invoke(std::forward<Func>(func),
                                                  std::forward<Args>(args)...)};
        }
    } catch (const std::exception&) {
        return type::unexpected(
            std::make_error_code(std::errc::invalid_argument));
    } catch (...) {
        return type::unexpected(
            std::make_error_code(std::errc::operation_canceled));
    }
}

/**
 * \brief Safely calls a function with exception handling and diagnostics
 * \tparam Func Function type
 * \tparam Args Argument types
 * \param func Function to call
 * \param func_name Function name for diagnostics
 * \param args Arguments to pass
 * \return Variant with result or diagnostic information
 */
template <typename Func, typename... Args>
    requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
[[nodiscard]] auto safeTryWithDiagnostics(Func&& func,
                                          std::string_view func_name,
                                          Args&&... args) {
    using ReturnType =
        std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>;
    using DiagnosticInfo = std::pair<std::exception_ptr, FunctionCallInfo>;
    using ResultType = std::variant<ReturnType, DiagnosticInfo>;

    FunctionCallInfo info{func_name};

    try {
        if constexpr (std::is_void_v<ReturnType>) {
            std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
            return ResultType{std::in_place_index<0>};
        } else {
            return ResultType{std::in_place_index<0>,
                              std::invoke(std::forward<Func>(func),
                                          std::forward<Args>(args)...)};
        }
    } catch (...) {
        return ResultType{std::in_place_index<1>, std::current_exception(),
                          info};
    }
}

/**
 * \brief Safely calls a function with default value fallback
 * \tparam Func Function type
 * \tparam Args Argument types
 * \param func Function to call
 * \param default_value Default value on exception
 * \param args Arguments to pass
 * \return Function result or default value
 */
template <typename Func, typename... Args>
    requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
[[nodiscard]] auto safeTryOrDefault(
    Func&& func,
    std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>
        default_value,
    Args&&... args) noexcept {
    try {
        return std::invoke(std::forward<Func>(func),
                           std::forward<Args>(args)...);
    } catch (...) {
        return default_value;
    }
}

/**
 * \brief Executes a function asynchronously
 * \tparam Func Function type
 * \tparam Args Argument types
 * \param func Function to execute
 * \param args Arguments to pass
 * \return Future with the result
 */
template <typename Func, typename... Args>
    requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
[[nodiscard]] auto asyncCall(Func&& func, Args&&... args) {
#ifdef ATOM_USE_BOOST
    return boost::async(
        boost::launch::async,
        [func = std::forward<Func>(func),
         ... capturedArgs = std::forward<Args>(args)]() mutable {
            try {
                return std::invoke(std::move(func), std::move(capturedArgs)...);
            } catch (...) {
                std::throw_with_nested(
                    std::runtime_error("Exception in async task execution"));
            }
        });
#else
    return std::async(
        std::launch::async,
        [func = std::forward<Func>(func),
         ... capturedArgs = std::forward<Args>(args)]() mutable {
            try {
                return std::invoke(std::move(func), std::move(capturedArgs)...);
            } catch (...) {
                std::throw_with_nested(
                    std::runtime_error("Exception in async task execution"));
            }
        });
#endif
}

/**
 * \brief Executes a function with retry logic
 * \tparam Func Function type
 * \tparam Args Argument types
 * \param func Function to call
 * \param retries Number of retry attempts
 * \param backoff_ms Milliseconds between retries
 * \param args Function arguments
 * \return Result of successful function call
 * \throws Re-throws last exception if all retries fail
 */
template <typename Func, typename... Args>
    requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
[[nodiscard]] auto retryCall(
    Func&& func, int retries,
    std::chrono::milliseconds backoff_ms = std::chrono::milliseconds(0),
    Args&&... args) {
    std::exception_ptr last_exception;

    while (retries-- > 0) {
        try {
            return std::invoke(std::forward<Func>(func),
                               std::forward<Args>(args)...);
        } catch (...) {
            last_exception = std::current_exception();

            if (retries > 0 && backoff_ms.count() > 0) {
                std::this_thread::sleep_for(backoff_ms);
                backoff_ms *= 2;  // Exponential backoff
            }
        }
    }

    std::rethrow_exception(last_exception);
}

/**
 * \brief Executes a function with timeout
 * \tparam Func Function type
 * \tparam Rep Clock representation type
 * \tparam Period Clock period type
 * \tparam Args Argument types
 * \param func Function to execute
 * \param timeout Maximum duration to wait
 * \param args Arguments to pass
 * \return Function result
 * \throws std::runtime_error if timeout occurs
 */
template <typename Func, typename Rep, typename Period, typename... Args>
    requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
[[nodiscard]] auto timeoutCall(Func&& func,
                               std::chrono::duration<Rep, Period> timeout,
                               Args&&... args) {
    using ReturnType =
        std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>;

#ifdef ATOM_USE_BOOST
    boost::asio::io_context io;
    boost::asio::steady_timer timer(io, timeout);
    std::optional<ReturnType> resultOpt;
    std::atomic<bool> completed = false;
    std::exception_ptr exception_ptr;

    std::jthread ioThread([&io]() { io.run(); });

    std::jthread funcThread([&]() {
        try {
            if constexpr (std::is_void_v<ReturnType>) {
                std::invoke(std::forward<Func>(func),
                            std::forward<Args>(args)...);
                resultOpt.emplace();
            } else {
                resultOpt = std::invoke(std::forward<Func>(func),
                                        std::forward<Args>(args)...);
            }
            completed = true;
            timer.cancel();
        } catch (...) {
            exception_ptr = std::current_exception();
            completed = true;
            timer.cancel();
        }
    });

    timer.async_wait([&](const boost::system::error_code& ec) {
        if (!ec && !completed.load()) {
            THROW_RUNTIME_ERROR("Function call timed out");
        }
    });

    if (exception_ptr) {
        std::rethrow_exception(exception_ptr);
    }

    if (!resultOpt.has_value()) {
        THROW_RUNTIME_ERROR("Function call timed out");
    }

    if constexpr (std::is_void_v<ReturnType>) {
        return;
    } else {
        return resultOpt.value();
    }
#else
    auto future = std::async(std::launch::async, std::forward<Func>(func),
                             std::forward<Args>(args)...);

    if (future.wait_for(timeout) == std::future_status::timeout) {
        THROW_RUNTIME_ERROR("Function call timed out");
    }

    return future.get();
#endif
}

/**
 * \brief Cache policy enumeration
 */
enum class CachePolicy {
    Never,        ///< Never expire cached values
    Count,        ///< Expire after N uses
    Time,         ///< Expire after time duration
    CountAndTime  ///< Expire after either condition
};

/**
 * \brief Cache configuration options
 * \tparam Duration Time duration type
 */
template <typename Duration = std::chrono::seconds>
struct CacheOptions {
    CachePolicy policy = CachePolicy::Never;
    size_t max_size = std::numeric_limits<size_t>::max();
    size_t max_uses = std::numeric_limits<size_t>::max();
    Duration ttl{std::numeric_limits<typename Duration::rep>::max()};
    bool thread_safe = true;
};

/**
 * \brief Creates a memoized version of a function
 * \tparam Func Function type
 * \tparam Duration Time duration type for cache TTL
 * \param func Function to memoize
 * \param options Cache configuration options
 * \return Memoized version of the function
 */
template <typename Func, typename Duration = std::chrono::seconds>
[[nodiscard]] auto memoize(Func&& func, CacheOptions<Duration> options = {}) {
    using FuncType = std::decay_t<Func>;

    return [func = std::forward<Func>(func), options]<typename... Args>(
               Args&&... args) -> std::invoke_result_t<FuncType, Args...> {
        using ReturnType = std::invoke_result_t<FuncType, Args...>;
        using KeyType = std::tuple<std::decay_t<Args>...>;

        struct CacheEntry {
            ReturnType value;
            std::chrono::steady_clock::time_point timestamp;
            std::atomic<size_t> use_count = 0;
        };

        static auto cache = std::make_shared<
            std::unordered_map<KeyType, CacheEntry, TupleHasher>>();
        static auto mutex = std::make_shared<std::shared_mutex>();

        KeyType key{args...};

        if (options.thread_safe) {
            std::shared_lock lock(*mutex);
            auto it = cache->find(key);

            if (it != cache->end()) {
                auto& entry = it->second;
                auto now = std::chrono::steady_clock::now();

                bool expired = false;
                switch (options.policy) {
                    case CachePolicy::Count:
                        expired = (++entry.use_count > options.max_uses);
                        break;
                    case CachePolicy::Time:
                        expired = (now - entry.timestamp > options.ttl);
                        break;
                    case CachePolicy::CountAndTime:
                        expired = (++entry.use_count > options.max_uses) ||
                                  (now - entry.timestamp > options.ttl);
                        break;
                    case CachePolicy::Never:
                    default:
                        break;
                }

                if (!expired) {
                    return entry.value;
                }
            }
        }

        auto result = std::invoke(func, std::forward<Args>(args)...);

        if (options.thread_safe) {
            std::unique_lock lock(*mutex);

            if (cache->size() >= options.max_size) {
                auto oldest = std::min_element(
                    cache->begin(), cache->end(),
                    [](const auto& a, const auto& b) {
                        return a.second.timestamp < b.second.timestamp;
                    });
                cache->erase(oldest);
            }

            (*cache)[key] = {result, std::chrono::steady_clock::now(), 1};
        }

        return result;
    };
}

/**
 * \brief Processes function calls in parallel batches
 * \tparam Func Function type
 * \tparam Args Argument types
 * \param func Function to execute
 * \param argsList List of argument tuples
 * \param maxThreads Maximum number of threads
 * \return Vector of results
 */
template <typename Func, typename... Args>
    requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
[[nodiscard]] auto parallelBatchCall(
    Func&& func, const std::vector<std::tuple<Args...>>& argsList,
    size_t maxThreads = 0) {
    using ReturnType =
        std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>;
    std::vector<ReturnType> results(argsList.size());

    if (argsList.empty()) {
        return results;
    }

    if (maxThreads == 0) {
        maxThreads = std::thread::hardware_concurrency();
    }

    maxThreads = std::min(maxThreads, argsList.size());
    std::latch completion_latch(argsList.size());
    std::atomic<size_t> next_index(0);
    std::exception_ptr first_exception;
    std::mutex exception_mutex;

    auto worker = [&]() {
        while (true) {
            size_t index = next_index.fetch_add(1, std::memory_order_relaxed);
            if (index >= argsList.size()) {
                break;
            }

            try {
                if constexpr (std::is_void_v<ReturnType>) {
                    std::apply(func, argsList[index]);
                } else {
                    results[index] = std::apply(func, argsList[index]);
                }
            } catch (...) {
                std::lock_guard lock(exception_mutex);
                if (!first_exception) {
                    first_exception = std::current_exception();
                }
            }

            completion_latch.count_down();
        }
    };

    std::vector<std::jthread> threads;
    threads.reserve(maxThreads);

    for (size_t i = 0; i < maxThreads; ++i) {
        threads.emplace_back(worker);
    }

    completion_latch.wait();

    if (first_exception) {
        std::rethrow_exception(first_exception);
    }

    return results;
}

/**
 * \brief Processes sequential function calls with argument batches
 * \tparam Func Function type
 * \tparam Args Argument types
 * \param func Function to call
 * \param argsList List of argument tuples
 * \return Vector of results
 */
template <typename Func, typename... Args>
    requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
[[nodiscard]] auto batchCall(Func&& func,
                             const std::vector<std::tuple<Args...>>& argsList) {
    std::vector<std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>>
        results;
    results.reserve(argsList.size());

    for (const auto& args : argsList) {
        results.push_back(std::apply(std::forward<Func>(func), args));
    }

    return results;
}

/**
 * \brief Creates an instrumented function that collects performance metrics
 * \tparam Func Function type
 * \param func Function to instrument
 * \param name Function name for metrics
 * \return Instrumented version of the function
 */
template <typename Func>
[[nodiscard]] auto instrument(Func&& func, std::string name = "") {
    struct Metrics {
        std::mutex mutex;
        std::string function_name;
        std::atomic<uint64_t> call_count{0};
        std::atomic<uint64_t> exception_count{0};
        std::chrono::nanoseconds total_execution_time{0};
        std::chrono::nanoseconds min_execution_time{
            std::numeric_limits<int64_t>::max()};
        std::chrono::nanoseconds max_execution_time{0};

        /**
         * \brief Generate performance report
         * \return Formatted performance metrics string
         */
        [[nodiscard]] std::string report() const {
            uint64_t count = call_count.load();
            if (count == 0) {
                return function_name + ": No calls";
            }

            auto avg_ns = total_execution_time / count;

            return std::format(
                "{}: {} calls, {} exceptions, avg time: {}ns, min: {}ns, max: "
                "{}ns",
                function_name, count, exception_count.load(), avg_ns.count(),
                min_execution_time.count(), max_execution_time.count());
        }
    };

    auto metrics = std::make_shared<Metrics>();
    metrics->function_name =
        name.empty() ? "anonymous_function" : std::move(name);

    return [func = std::forward<Func>(func), metrics]<typename... Args>(
               Args&&... args) -> std::invoke_result_t<Func, Args...> {
        metrics->call_count++;
        auto start = std::chrono::high_resolution_clock::now();

        try {
            if constexpr (std::is_void_v<std::invoke_result_t<Func, Args...>>) {
                std::invoke(func, std::forward<Args>(args)...);

                auto duration =
                    std::chrono::high_resolution_clock::now() - start;
                {
                    std::lock_guard lock(metrics->mutex);
                    metrics->total_execution_time += duration;
                    metrics->min_execution_time =
                        std::min(metrics->min_execution_time, duration);
                    metrics->max_execution_time =
                        std::max(metrics->max_execution_time, duration);
                }
                return;
            } else {
                auto result = std::invoke(func, std::forward<Args>(args)...);

                auto duration =
                    std::chrono::high_resolution_clock::now() - start;
                {
                    std::lock_guard lock(metrics->mutex);
                    metrics->total_execution_time += duration;
                    metrics->min_execution_time =
                        std::min(metrics->min_execution_time, duration);
                    metrics->max_execution_time =
                        std::max(metrics->max_execution_time, duration);
                }

                return result;
            }
        } catch (...) {
            metrics->exception_count++;
            throw;
        }
    };
}

}  // namespace atom::meta

#endif  // ATOM_META_INVOKE_HPP
