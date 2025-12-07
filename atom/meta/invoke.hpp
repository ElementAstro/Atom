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
#include <stop_token>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <version>

#include "atom/error/exception.hpp"
#include "atom/type/expected.hpp"

// C++23 feature detection
#if __cpp_lib_expected >= 202202L
#include <expected>
#define ATOM_INVOKE_HAS_STD_EXPECTED 1
#else
#define ATOM_INVOKE_HAS_STD_EXPECTED 0
#endif

#if __cpp_lib_jthread >= 201911L
#define ATOM_INVOKE_HAS_JTHREAD 1
#else
#define ATOM_INVOKE_HAS_JTHREAD 0
#endif

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
[[nodiscard]] constexpr auto delayMemberVarInvoke(M T::*memberVar, T* obj) {
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
                g = std::get<0>(std::forward_as_tuple(gs...))](auto&&... args) {
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

//==============================================================================
// C++23 Enhanced Invocation Utilities
//==============================================================================

#if ATOM_INVOKE_HAS_STD_EXPECTED
/**
 * @brief Safe call using std::expected (C++23)
 */
template <typename Func, typename... Args>
    requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
[[nodiscard]] auto safeCallExpected(Func&& func, Args&&... args)
    -> std::expected<
        std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>,
        std::exception_ptr> {
    using ReturnType =
        std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>;

    try {
        if constexpr (std::is_void_v<ReturnType>) {
            std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
            return {};
        } else {
            return std::invoke(std::forward<Func>(func),
                               std::forward<Args>(args)...);
        }
    } catch (...) {
        return std::unexpected(std::current_exception());
    }
}
#endif

/**
 * @brief Invoke function with cancellation support using stop_token
 */
template <typename Func, typename... Args>
    requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
auto invokeWithCancellation(std::stop_token stop_token, Func&& func,
                            Args&&... args)
    -> std::optional<
        std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>> {
    using ReturnType =
        std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>;

    if (stop_token.stop_requested()) {
        return std::nullopt;
    }

    if constexpr (std::is_void_v<ReturnType>) {
        std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
        return std::nullopt;  // void functions return nullopt for success
    } else {
        return std::invoke(std::forward<Func>(func),
                           std::forward<Args>(args)...);
    }
}

/**
 * @brief Parallel batch call with cancellation support
 */
template <typename Func, typename... Args>
    requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
[[nodiscard]] auto parallelBatchCallCancellable(
    std::stop_token stop_token, Func&& func,
    const std::vector<std::tuple<Args...>>& argsList, size_t maxThreads = 0) {
    using ReturnType =
        std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>;
    std::vector<std::optional<ReturnType>> results(argsList.size());

    if (argsList.empty() || stop_token.stop_requested()) {
        return results;
    }

    if (maxThreads == 0) {
        maxThreads = std::thread::hardware_concurrency();
    }

    maxThreads = std::min(maxThreads, argsList.size());
    std::atomic<size_t> next_index(0);
    std::atomic<size_t> completed(0);

    auto worker = [&]() {
        while (!stop_token.stop_requested()) {
            size_t index = next_index.fetch_add(1, std::memory_order_relaxed);
            if (index >= argsList.size()) {
                break;
            }

            try {
                if constexpr (std::is_void_v<ReturnType>) {
                    std::apply(func, argsList[index]);
                    results[index] = std::nullopt;
                } else {
                    results[index] = std::apply(func, argsList[index]);
                }
            } catch (...) {
                // Store nullopt on error
            }
            completed.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::jthread> threads;
    threads.reserve(maxThreads);

    for (size_t i = 0; i < maxThreads; ++i) {
        threads.emplace_back(worker);
    }

    // Wait for completion or cancellation
    while (completed.load() < argsList.size() && !stop_token.stop_requested()) {
        std::this_thread::yield();
    }

    return results;
}

/**
 * @brief Pipeline execution - chain multiple functions
 */
template <typename... Funcs>
class Pipeline {
    std::tuple<Funcs...> funcs_;

public:
    constexpr explicit Pipeline(Funcs... funcs) : funcs_(std::move(funcs)...) {}

    template <typename Input>
    constexpr auto operator()(Input&& input) const {
        return executeImpl(std::forward<Input>(input),
                           std::make_index_sequence<sizeof...(Funcs)>{});
    }

private:
    template <typename Input, std::size_t... Is>
    constexpr auto executeImpl(Input&& input,
                               std::index_sequence<Is...>) const {
        return executeChain(std::forward<Input>(input),
                            std::get<Is>(funcs_)...);
    }

    template <typename Input, typename F>
    static constexpr auto executeChain(Input&& input, F&& func) {
        return std::invoke(std::forward<F>(func), std::forward<Input>(input));
    }

    template <typename Input, typename F, typename... Rest>
    static constexpr auto executeChain(Input&& input, F&& func,
                                       Rest&&... rest) {
        return executeChain(
            std::invoke(std::forward<F>(func), std::forward<Input>(input)),
            std::forward<Rest>(rest)...);
    }
};

/**
 * @brief Create a pipeline from functions
 */
template <typename... Funcs>
constexpr auto makePipeline(Funcs&&... funcs)
    -> Pipeline<std::decay_t<Funcs>...> {
    return Pipeline<std::decay_t<Funcs>...>(std::forward<Funcs>(funcs)...);
}

/**
 * @brief Conditional invocation - invoke based on predicate
 */
template <typename Predicate, typename TrueFunc, typename FalseFunc>
class ConditionalInvoke {
    Predicate pred_;
    TrueFunc true_func_;
    FalseFunc false_func_;

public:
    constexpr ConditionalInvoke(Predicate pred, TrueFunc true_f,
                                FalseFunc false_f)
        : pred_(std::move(pred)),
          true_func_(std::move(true_f)),
          false_func_(std::move(false_f)) {}

    template <typename... Args>
    constexpr auto operator()(Args&&... args) const {
        if (std::invoke(pred_, args...)) {
            return std::invoke(true_func_, std::forward<Args>(args)...);
        } else {
            return std::invoke(false_func_, std::forward<Args>(args)...);
        }
    }
};

/**
 * @brief Create a conditional invocation
 */
template <typename Predicate, typename TrueFunc, typename FalseFunc>
constexpr auto makeConditionalInvoke(Predicate&& pred, TrueFunc&& true_f,
                                     FalseFunc&& false_f) {
    return ConditionalInvoke<std::decay_t<Predicate>, std::decay_t<TrueFunc>,
                             std::decay_t<FalseFunc>>(
        std::forward<Predicate>(pred), std::forward<TrueFunc>(true_f),
        std::forward<FalseFunc>(false_f));
}

/**
 * @brief Rate limiter for function invocations
 */
template <typename Func>
class RateLimitedInvoke {
    Func func_;
    std::chrono::steady_clock::duration min_interval_;
    mutable std::chrono::steady_clock::time_point last_call_;
    mutable std::mutex mutex_;

public:
    constexpr RateLimitedInvoke(Func func,
                                std::chrono::steady_clock::duration interval)
        : func_(std::move(func)),
          min_interval_(interval),
          last_call_(std::chrono::steady_clock::time_point::min()) {}

    template <typename... Args>
    auto operator()(Args&&... args) const
        -> std::optional<std::invoke_result_t<Func, Args...>> {
        std::lock_guard lock(mutex_);
        auto now = std::chrono::steady_clock::now();

        if (now - last_call_ < min_interval_) {
            return std::nullopt;  // Rate limited
        }

        last_call_ = now;
        return std::invoke(func_, std::forward<Args>(args)...);
    }
};

/**
 * @brief Create a rate-limited invocation
 */
template <typename Func>
auto makeRateLimited(Func&& func, std::chrono::steady_clock::duration interval)
    -> RateLimitedInvoke<std::decay_t<Func>> {
    return RateLimitedInvoke<std::decay_t<Func>>(std::forward<Func>(func),
                                                 interval);
}

/**
 * @brief Debounced function invocation
 */
template <typename Func>
class DebouncedInvoke {
    Func func_;
    std::chrono::steady_clock::duration delay_;
    mutable std::optional<std::chrono::steady_clock::time_point> pending_call_;
    mutable std::mutex mutex_;

public:
    constexpr DebouncedInvoke(Func func,
                              std::chrono::steady_clock::duration delay)
        : func_(std::move(func)), delay_(delay) {}

    template <typename... Args>
    void schedule(Args&&... args) const {
        std::lock_guard lock(mutex_);
        pending_call_ = std::chrono::steady_clock::now() + delay_;
        // In a real implementation, this would schedule an async call
    }

    bool shouldExecute() const {
        std::lock_guard lock(mutex_);
        if (!pending_call_)
            return false;
        return std::chrono::steady_clock::now() >= *pending_call_;
    }
};

//==============================================================================
// Integration with func_traits.hpp
//==============================================================================

/**
 * @brief Invoke a function with type-checked arguments using FunctionTraits
 */
template <typename Func, typename... Args>
    requires requires {
        typename FunctionTraits<std::decay_t<Func>>::return_type;
    }
auto invokeWithTraits(Func&& func, Args&&... args) {
    using Traits = FunctionTraits<std::decay_t<Func>>;
    static_assert(sizeof...(Args) == Traits::arity,
                  "Argument count must match function arity");
    return std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
}

/**
 * @brief Get invocation info using FunctionTraits
 */
template <typename Func>
auto getInvocationInfo() -> FunctionCallInfo {
    using Traits = FunctionTraits<std::decay_t<Func>>;
    FunctionCallInfo info;
    info.functionName = typeid(Func).name();
    // Use traits to populate additional info
    return info;
}

/**
 * @brief Invoke with automatic argument type conversion
 */
template <typename Func, typename ArgTuple>
auto invokeFromTuple(Func&& func, ArgTuple&& args) {
    return std::apply(std::forward<Func>(func), std::forward<ArgTuple>(args));
}

/**
 * @brief Invoke chain combining multiple functions
 */
template <typename... Funcs>
class InvokeChain {
    std::tuple<Funcs...> funcs_;

public:
    constexpr explicit InvokeChain(Funcs... funcs)
        : funcs_(std::move(funcs)...) {}

    template <typename Arg>
    auto operator()(Arg&& arg) const {
        return invokeChainImpl(std::forward<Arg>(arg),
                               std::index_sequence_for<Funcs...>{});
    }

private:
    template <typename Arg, std::size_t... Is>
    auto invokeChainImpl(Arg&& arg, std::index_sequence<Is...>) const {
        auto result = std::forward<Arg>(arg);
        ((result = std::get<Is>(funcs_)(std::move(result))), ...);
        return result;
    }
};

/**
 * @brief Create an invoke chain
 */
template <typename... Funcs>
auto makeInvokeChain(Funcs&&... funcs) {
    return InvokeChain<std::decay_t<Funcs>...>(std::forward<Funcs>(funcs)...);
}

/**
 * @brief Invocation dispatcher based on argument count
 */
template <typename Func>
class InvocationDispatcher {
    Func func_;

public:
    constexpr explicit InvocationDispatcher(Func func)
        : func_(std::move(func)) {}

    /**
     * @brief Invoke with no arguments
     */
    auto invoke() const
        requires(FunctionTraits<Func>::arity == 0)
    {
        return func_();
    }

    /**
     * @brief Invoke with one argument
     */
    template <typename Arg>
    auto invoke(Arg&& arg) const
        requires(FunctionTraits<Func>::arity == 1)
    {
        return func_(std::forward<Arg>(arg));
    }

    /**
     * @brief Invoke with two arguments
     */
    template <typename Arg1, typename Arg2>
    auto invoke(Arg1&& arg1, Arg2&& arg2) const
        requires(FunctionTraits<Func>::arity == 2)
    {
        return func_(std::forward<Arg1>(arg1), std::forward<Arg2>(arg2));
    }

    /**
     * @brief Invoke with variadic arguments
     */
    template <typename... Args>
    auto invoke(Args&&... args) const
        requires(FunctionTraits<Func>::arity > 2)
    {
        return func_(std::forward<Args>(args)...);
    }
};

/**
 * @brief Create an invocation dispatcher
 */
template <typename Func>
auto makeDispatcher(Func&& func) {
    return InvocationDispatcher<std::decay_t<Func>>(std::forward<Func>(func));
}

/**
 * @brief Invoke with timeout and result capture
 */
template <typename Func, typename... Args>
auto invokeWithTimeout(Func&& func, std::chrono::milliseconds timeout,
                       Args&&... args)
    -> std::optional<std::invoke_result_t<Func, Args...>> {
    using Result = std::invoke_result_t<Func, Args...>;

    auto future = std::async(std::launch::async, std::forward<Func>(func),
                             std::forward<Args>(args)...);

    if (future.wait_for(timeout) == std::future_status::ready) {
        if constexpr (std::is_void_v<Result>) {
            future.get();
            return std::nullopt;
        } else {
            return future.get();
        }
    }
    return std::nullopt;  // Timeout
}

/**
 * @brief Concept-constrained invocation
 */
template <typename Func, typename... Args>
    requires NothrowInvokable<Func, Args...>
auto safeInvoke(Func&& func, Args&&... args) noexcept {
    return std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
}

/**
 * @brief Invoke and log result
 */
template <typename Func, typename Logger, typename... Args>
auto invokeAndLog(Func&& func, Logger&& logger, Args&&... args) {
    using Traits = FunctionTraits<std::decay_t<Func>>;
    using Result = typename Traits::return_type;

    logger("Invoking function");

    if constexpr (std::is_void_v<Result>) {
        std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
        logger("Function completed (void)");
    } else {
        auto result =
            std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
        logger("Function completed with result");
        return result;
    }
}

//==============================================================================
// Advanced Invocation Utilities
//==============================================================================

/**
 * @brief Invoke with exception handling and custom error handler
 */
template <typename Func, typename ErrorHandler, typename... Args>
auto invokeWithErrorHandler(Func&& func, ErrorHandler&& handler,
                            Args&&... args) {
    using Result = std::invoke_result_t<Func, Args...>;

    try {
        return std::invoke(std::forward<Func>(func),
                           std::forward<Args>(args)...);
    } catch (const std::exception& e) {
        return handler(e);
    } catch (...) {
        return handler(std::runtime_error("Unknown exception"));
    }
}

/**
 * @brief Invoke and measure execution time
 */
template <typename Func, typename... Args>
auto invokeAndMeasure(Func&& func, Args&&... args) {
    using Result = std::invoke_result_t<Func, Args...>;

    auto start = std::chrono::high_resolution_clock::now();

    if constexpr (std::is_void_v<Result>) {
        std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end -
                                                                    start);
    } else {
        auto result =
            std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        return std::pair{std::move(result), duration};
    }
}

/**
 * @brief Invoke with pre and post hooks
 */
template <typename Func, typename PreHook, typename PostHook, typename... Args>
auto invokeWithHooks(Func&& func, PreHook&& pre, PostHook&& post,
                     Args&&... args) {
    using Result = std::invoke_result_t<Func, Args...>;

    pre();

    if constexpr (std::is_void_v<Result>) {
        std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
        post();
    } else {
        auto result =
            std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
        post();
        return result;
    }
}

/**
 * @brief Invoke conditionally
 */
template <typename Condition, typename Func, typename... Args>
auto invokeIf(Condition&& condition, Func&& func, Args&&... args)
    -> std::optional<std::invoke_result_t<Func, Args...>> {
    if (condition()) {
        return std::invoke(std::forward<Func>(func),
                           std::forward<Args>(args)...);
    }
    return std::nullopt;
}

/**
 * @brief Invoke or return default
 */
template <typename Default, typename Func, typename... Args>
auto invokeOrDefault(Default&& default_value, Func&& func, Args&&... args) {
    using Result = std::invoke_result_t<Func, Args...>;

    try {
        return std::invoke(std::forward<Func>(func),
                           std::forward<Args>(args)...);
    } catch (...) {
        return static_cast<Result>(std::forward<Default>(default_value));
    }
}

/**
 * @brief Invoke all functions in sequence
 */
template <typename... Funcs>
void invokeAll(Funcs&&... funcs) {
    (std::invoke(std::forward<Funcs>(funcs)), ...);
}

/**
 * @brief Invoke and collect results
 */
template <typename... Funcs>
auto invokeAndCollect(Funcs&&... funcs) {
    return std::tuple{std::invoke(std::forward<Funcs>(funcs))...};
}

/**
 * @brief Invoke with argument transformation
 */
template <typename Func, typename Transform, typename... Args>
auto invokeWithTransform(Func&& func, Transform&& transform, Args&&... args) {
    return std::invoke(std::forward<Func>(func),
                       transform(std::forward<Args>(args))...);
}

/**
 * @brief Async invocation with callback
 */
template <typename Func, typename Callback, typename... Args>
void invokeAsync(Func&& func, Callback&& callback, Args&&... args) {
    std::thread([func = std::forward<Func>(func),
                 callback = std::forward<Callback>(callback),
                 ... args = std::forward<Args>(args)]() mutable {
        try {
            if constexpr (std::is_void_v<std::invoke_result_t<Func, Args...>>) {
                std::invoke(func, std::forward<Args>(args)...);
                callback();
            } else {
                auto result = std::invoke(func, std::forward<Args>(args)...);
                callback(std::move(result));
            }
        } catch (const std::exception& e) {
            // Optionally handle exception
        }
    }).detach();
}

/**
 * @brief Invoke with validation
 */
template <typename Validator, typename Func, typename... Args>
auto invokeWithValidation(Validator&& validator, Func&& func, Args&&... args)
    -> std::optional<std::invoke_result_t<Func, Args...>> {
    if (!validator(args...)) {
        return std::nullopt;
    }
    return std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
}

/**
 * @brief Invoke and transform result
 */
template <typename Func, typename ResultTransform, typename... Args>
auto invokeAndTransformResult(Func&& func, ResultTransform&& transform,
                              Args&&... args) {
    auto result =
        std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
    return transform(std::move(result));
}

/**
 * @brief Parallel invoke multiple functions
 */
template <typename... Funcs>
auto parallelInvoke(Funcs&&... funcs) {
    return std::tuple{
        std::async(std::launch::async, std::forward<Funcs>(funcs))...};
}

/**
 * @brief Invoke with retry and backoff
 */
template <typename Func, typename... Args>
auto invokeWithBackoff(Func&& func, std::size_t max_retries,
                       std::chrono::milliseconds initial_delay, Args&&... args)
    -> std::optional<std::invoke_result_t<Func, Args...>> {
    auto delay = initial_delay;

    for (std::size_t attempt = 0; attempt < max_retries; ++attempt) {
        try {
            return std::invoke(std::forward<Func>(func),
                               std::forward<Args>(args)...);
        } catch (...) {
            if (attempt + 1 < max_retries) {
                std::this_thread::sleep_for(delay);
                delay *= 2;  // Exponential backoff
            }
        }
    }
    return std::nullopt;
}

/**
 * @brief Invoke first successful function
 */
template <typename... Funcs>
auto invokeFirstSuccess(Funcs&&... funcs) {
    using FirstResult =
        std::invoke_result_t<std::tuple_element_t<0, std::tuple<Funcs...>>>;
    std::optional<FirstResult> result;

    ((
         result = [&]() -> std::optional<FirstResult> {
             try {
                 return std::invoke(std::forward<Funcs>(funcs));
             } catch (...) {
                 return std::nullopt;
             }
         }(),
         result.has_value()) ||
     ...);

    return result;
}

/**
 * @brief Invocation statistics tracker
 */
class InvocationStats {
    std::atomic<std::size_t> total_calls_{0};
    std::atomic<std::size_t> successful_calls_{0};
    std::atomic<std::size_t> failed_calls_{0};
    std::atomic<std::chrono::nanoseconds::rep> total_time_{0};
    mutable std::mutex mutex_;

public:
    template <typename Func, typename... Args>
    auto track(Func&& func, Args&&... args) {
        ++total_calls_;
        auto start = std::chrono::high_resolution_clock::now();

        try {
            auto result = std::invoke(std::forward<Func>(func),
                                      std::forward<Args>(args)...);
            ++successful_calls_;

            auto duration =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::high_resolution_clock::now() - start);
            total_time_ += duration.count();

            return result;
        } catch (...) {
            ++failed_calls_;
            throw;
        }
    }

    [[nodiscard]] std::size_t totalCalls() const { return total_calls_.load(); }
    [[nodiscard]] std::size_t successfulCalls() const {
        return successful_calls_.load();
    }
    [[nodiscard]] std::size_t failedCalls() const {
        return failed_calls_.load();
    }
    [[nodiscard]] double successRate() const {
        auto total = total_calls_.load();
        return total > 0 ? static_cast<double>(successful_calls_.load()) / total
                         : 0.0;
    }
    [[nodiscard]] std::chrono::nanoseconds averageTime() const {
        auto total = successful_calls_.load();
        if (total == 0)
            return std::chrono::nanoseconds{0};
        return std::chrono::nanoseconds{total_time_.load() / total};
    }

    void reset() {
        total_calls_.store(0);
        successful_calls_.store(0);
        failed_calls_.store(0);
        total_time_.store(0);
    }
};

}  // namespace atom::meta

#endif  // ATOM_META_INVOKE_HPP
