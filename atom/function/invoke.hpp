/*!
 * \file invoke.hpp
 * \brief An implementation of invoke function utilities with C++20/23 features
 * \author Max Qian <lightapt.com>, Enhanced by Claude AI
 * \date 2023-03-29, Updated 2025-03-13
 */

#ifndef ATOM_META_INVOKE_HPP
#define ATOM_META_INVOKE_HPP

#include <chrono>
#include <concepts>
#include <exception>
#include <expected>  // C++23
#include <format>    // C++20
#include <functional>
#include <future>
#include <latch>  // C++20
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <source_location>  // C++20
#include <span>             // C++20
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

// Error handling type for function results
template <typename T>
using Result = type::expected<T, std::error_code>;  // C++23

// Namespace for implementation details
namespace detail {
// Hash function for tuples of arguments
template <typename Tuple, std::size_t... Is>
std::size_t hash_tuple_impl(const Tuple& t, std::index_sequence<Is...>) {
    std::size_t seed = 0;
    (void)std::initializer_list<int>{
        (seed ^= std::hash<std::tuple_element_t<Is, Tuple>>{}(std::get<Is>(t)) +
                 0x9e3779b9 + (seed << 6) + (seed >> 2),
         0)...};
    return seed;
}

template <typename... Args>
std::size_t hash_tuple(const std::tuple<Args...>& t) {
    return hash_tuple_impl(t, std::index_sequence_for<Args...>{});
}

// Exception handling helper to preserve stack trace
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

// Improved tuple hasher for cache functions
struct TupleHasher {
    template <typename... Args>
    std::size_t operator()(const std::tuple<Args...>& t) const {
        return detail::hash_tuple(t);
    }
};

/**
 * @brief Function metadata structure for capturing caller information
 */
struct FunctionCallInfo {
    std::string_view function_name;
    std::source_location location;
    std::chrono::system_clock::time_point timestamp;

    FunctionCallInfo(std::string_view name = {},
                     std::source_location loc = std::source_location::current())
        : function_name(name),
          location(loc),
          timestamp(std::chrono::system_clock::now()) {}

    std::string to_string() const {
        return std::format(
            "Function: {}, File: {}:{}, Line: {}, Column: {}, Time: {}",
            function_name, location.file_name(), location.function_name(),
            location.line(), location.column(),
            std::chrono::system_clock::to_time_t(timestamp));
    }
};

/**
 * @brief Validates that arguments meet specified criteria before invoking a
 * function
 *
 * @tparam Validator A validator functor that returns bool
 * @tparam Func The function type to invoke
 * @param validator Function that validates the arguments
 * @param func Function to invoke if validation passes
 * @return A callable that validates before invoking
 */
template <typename Validator, typename Func>
[[nodiscard]] auto validate_then_invoke(Validator&& validator, Func&& func) {
    return
        [validator = std::forward<Validator>(validator),
         func = std::forward<Func>(func)](
            auto&&... args) -> std::invoke_result_t<Func, decltype(args)...> {
            if (!std::invoke(validator, args...)) {
                throw std::invalid_argument("Input validation failed");
            }
            return std::invoke(func, std::forward<decltype(args)>(args)...);
        };
}

/**
 * @brief Delays the invocation of a function with given arguments
 *
 * @tparam F The function type
 * @tparam Args The argument types
 * @param func The function to be invoked
 * @param args The arguments to be passed to the function
 * @return A callable object that invokes the function with stored arguments
 */
template <typename F, typename... Args>
    requires std::invocable<std::decay_t<F>, std::decay_t<Args>...>
[[nodiscard]] auto delayInvoke(F&& func, Args&&... args) {
    return
        [func = std::forward<F>(func),
         args = std::make_tuple(std::forward<Args>(
             args)...)]() mutable noexcept(std::is_nothrow_invocable_v<F,
                                                                       Args...>)
            -> std::invoke_result_t<F, Args...> {
            return std::apply(std::move(func), std::move(args));
        };
}

/**
 * @brief Delays the invocation of a member function with given arguments
 *
 * @tparam R The return type of the member function
 * @tparam T The class type of the member function
 * @tparam Args The argument types
 * @param func The member function to be invoked
 * @param obj The object on which the member function will be invoked
 * @return A callable that invokes the member function when called
 */
template <typename R, typename T, typename... Args>
[[nodiscard]] auto delayMemInvoke(R (T::*func)(Args...), T* obj) {
    static_assert(std::is_member_function_pointer_v<decltype(func)>,
                  "First parameter must be a member function pointer");

    return [func, obj = std::addressof(*obj)](Args... args) noexcept(
               noexcept((obj->*func)(std::forward<Args>(args)...))) -> R {
        if (obj == nullptr) {
            throw std::invalid_argument(
                "Null object pointer in delayMemInvoke");
        }
        return (obj->*func)(std::forward<Args>(args)...);
    };
}

/**
 * @brief Delays the invocation of a const member function with given arguments
 *
 * @tparam R The return type of the member function
 * @tparam T The class type of the member function
 * @tparam Args The argument types
 * @param func The const member function to be invoked
 * @param obj The object on which the member function will be invoked
 * @return A callable that invokes the const member function when called
 */
template <typename R, typename T, typename... Args>
[[nodiscard]] auto delayMemInvoke(R (T::*func)(Args...) const, const T* obj) {
    static_assert(std::is_member_function_pointer_v<decltype(func)>,
                  "First parameter must be a member function pointer");

    return [func, obj = std::addressof(*obj)](Args... args) noexcept(
               noexcept((obj->*func)(std::forward<Args>(args)...))) -> R {
        if (obj == nullptr) {
            throw std::invalid_argument(
                "Null object pointer in delayMemInvoke");
        }
        return (obj->*func)(std::forward<Args>(args)...);
    };
}

/**
 * @brief Delays the invocation of a static member function with given arguments
 *
 * @tparam R The return type of the static member function
 * @tparam T The class type of the static member function
 * @tparam Args The argument types
 * @param func The static member function to be invoked
 * @return A callable that invokes the static member function when called
 */
template <typename R, typename T, typename... Args>
[[nodiscard]] auto delayStaticMemInvoke(R (*func)(Args...)) {
    return [func](Args... args) noexcept(
               noexcept(func(std::forward<Args>(args)...))) -> R {
        return func(std::forward<Args>(args)...);
    };
}

/**
 * @brief Delays the invocation of a member variable access
 *
 * @tparam T The class type of the member variable
 * @tparam M The type of the member variable
 * @param memberVar The member variable to be accessed
 * @param obj The object on which the member variable will be accessed
 * @return A callable that returns the member variable when called
 */
template <typename T, typename M>
[[nodiscard]] auto delayMemberVarInvoke(M T::*memberVar, T* obj) {
    return [memberVar, obj = std::addressof(*obj)]() -> M& {
        if (obj == nullptr) {
            throw std::invalid_argument(
                "Null object pointer in delayMemberVarInvoke");
        }
        return (obj->*memberVar);
    };
}

/**
 * @brief Creates a type-erased callable that can be invoked later
 *
 * @tparam R The return type of the function
 * @tparam F The function type
 * @tparam Args The argument types
 * @param func The function to wrap
 * @param args The arguments to store
 * @return std::function<R()> A type-erased callable
 */
template <typename R, typename F, typename... Args>
    requires std::invocable<std::decay_t<F>, std::decay_t<Args>...> &&
             std::convertible_to<
                 std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>,
                 R>
[[nodiscard]] std::function<R()> makeDeferred(F&& func, Args&&... args) {
    return
        [func = std::forward<F>(func),
         args = std::make_tuple(std::forward<Args>(args)...)]() mutable -> R {
            return static_cast<R>(std::apply(std::move(func), std::move(args)));
        };
}

/**
 * @brief Composes multiple functions into a single function
 *
 * @tparam F The first function type
 * @tparam Gs Additional function types to compose
 * @param f The first function
 * @param gs Additional functions
 * @return A function composition g(f(x))
 */
template <typename F, typename... Gs>
    requires(sizeof...(Gs) > 0)
[[nodiscard]] auto compose(F&& f, Gs&&... gs) {
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
 * @brief Creates a function that applies a transformation to each argument
 * before invoking target function
 *
 * @tparam Transform The transformation function to apply to arguments
 * @tparam Func The target function to invoke
 * @param transform Function that transforms each argument
 * @param func Function to invoke with transformed arguments
 * @return A callable that transforms arguments then invokes func
 */
template <typename Transform, typename Func>
[[nodiscard]] auto transform_args(Transform&& transform, Func&& func) {
    return [transform = std::forward<Transform>(transform),
            func = std::forward<Func>(func)](auto&&... args) {
        return std::invoke(
            func,
            std::invoke(transform, std::forward<decltype(args)>(args))...);
    };
}

/**
 * @brief Safely calls a function with given arguments, catching any exceptions
 *
 * @tparam Func The function type
 * @tparam Args The argument types
 * @param func The function to be called
 * @param args The arguments to be passed to the function
 * @return The result of the function call, or a default-constructed value if an
 * exception occurs
 */
template <typename Func, typename... Args>
    requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
[[nodiscard]] auto safeCall(Func&& func, Args&&... args) {
    try {
        return std::invoke(std::forward<Func>(func),
                           std::forward<Args>(args)...);
    } catch (...) {
        using ReturnType =
            std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>;
        if constexpr (std::is_default_constructible_v<ReturnType>) {
            return ReturnType{};
        } else {
            THROW_RUNTIME_ERROR("An exception occurred in safe_call");
        }
    }
}

/**
 * @brief Safely calls a function with given arguments, returning a Result
 * object
 *
 * @tparam Func The function type
 * @tparam Args The argument types
 * @param func The function to call
 * @param args The arguments to pass
 * @return Result containing either the function result or an error
 */
template <typename Func, typename... Args>
    requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
[[nodiscard]] auto safeCallResult(Func&& func, Args&&... args)
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
    } catch (const std::exception& e) {
        return type::unexpected(
            std::make_error_code(std::errc::invalid_argument));
    } catch (...) {
        return type::unexpected(
            std::make_error_code(std::errc::operation_canceled));
    }
}

/**
 * @brief Safely tries to call a function, catching any exceptions
 *
 * @tparam F The function type
 * @tparam Args The argument types
 * @param func The function to be called
 * @param args The arguments to be passed to the function
 * @return A variant containing either the result of the function call or an
 * exception pointer
 */
template <typename F, typename... Args>
    requires std::is_invocable_v<std::decay_t<F>, std::decay_t<Args>...>
[[nodiscard]] auto safeTryCatch(F&& func, Args&&... args) {
    using ReturnType =
        std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
    using ResultType = std::variant<ReturnType, std::exception_ptr>;

    try {
        if constexpr (std::is_same_v<ReturnType, void>) {
            std::invoke(std::forward<F>(func), std::forward<Args>(args)...);
            return ResultType{};  // Empty variant for void functions
        } else {
            return ResultType{std::invoke(std::forward<F>(func),
                                          std::forward<Args>(args)...)};
        }
    } catch (...) {
        return ResultType{std::current_exception()};
    }
}

/**
 * @brief Safely tries a function with detailed diagnostic information
 *
 * @tparam Func The function type
 * @tparam Args The argument types
 * @param func The function to call
 * @param func_name Name of the function (for diagnostic info)
 * @param args The arguments to pass
 * @return A variant with the result or diagnostic information
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
            return ResultType{std::in_place_index<0>};  // Void success case
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
 * @brief Safely tries to call a function, returning a default value on
 * exception
 *
 * @tparam Func The function type
 * @tparam Args The argument types
 * @param func The function to be called
 * @param default_value The default value to return if an exception occurs
 * @param args The arguments to be passed to the function
 * @return The result of the function call, or the default value if an exception
 * occurs
 */
template <typename Func, typename... Args>
    requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
[[nodiscard]] auto safeTryCatchOrDefault(
    Func&& func,
    std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>
        default_value,
    Args&&... args) {
    try {
        return std::invoke(std::forward<Func>(func),
                           std::forward<Args>(args)...);
    } catch (...) {
        return default_value;
    }
}

/**
 * @brief Safely tries to call a function, using a custom handler on exception
 *
 * @tparam Func The function type
 * @tparam Args The argument types
 * @param func The function to be called
 * @param handler The custom handler to be called if an exception occurs
 * @param args The arguments to be passed to the function
 * @return The result of the function call, or a default value from the handler
 */
template <typename Func, typename... Args>
    requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
[[nodiscard]] auto safeTryCatchWithCustomHandler(
    Func&& func, const std::function<void(std::exception_ptr)>& handler,
    Args&&... args) {
    try {
        return std::invoke(std::forward<Func>(func),
                           std::forward<Args>(args)...);
    } catch (...) {
        handler(std::current_exception());
        using ReturnType =
            std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>;
        if constexpr (std::is_default_constructible_v<ReturnType>) {
            return ReturnType{};
        } else {
            throw;
        }
    }
}

/**
 * @brief Executes a function asynchronously
 *
 * @tparam Func The function type
 * @tparam Args The argument types
 * @param func The function to execute
 * @param args The arguments to pass
 * @return std::future with the result
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
 * @brief Executes a function with retry logic on failure
 *
 * @tparam Func The function type
 * @tparam Args The argument types
 * @param func The function to call
 * @param retries Number of retry attempts
 * @param backoff_ms Milliseconds to wait between retries
 * @param args Function arguments
 * @return Result of successful function call
 * @throws Re-throws the last exception if all retries fail
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
                // Optional exponential backoff
                backoff_ms = backoff_ms * 2;
            }
        }
    }

    std::rethrow_exception(last_exception);
}

/**
 * @brief Executes a function with a timeout using modern C++20 features
 *
 * @tparam Func The function type
 * @tparam Rep Clock representation type
 * @tparam Period Clock period type
 * @tparam Args The argument types
 * @param func The function to execute
 * @param timeout The maximum duration to wait
 * @param args The arguments to pass
 * @return Optional with the result or nullopt if timed out
 * @throws std::runtime_error if the operation times out
 */
template <typename Func, typename Rep, typename Period, typename... Args>
    requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
[[nodiscard]] auto timeoutCall(Func&& func,
                               std::chrono::duration<Rep, Period> timeout,
                               Args&&... args) {
    // using ReturnType = std::invoke_result_t<std::decay_t<Func>,
    // std::decay_t<Args>...>;

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
                resultOpt.emplace();  // Empty value for void return types
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

    // The threads will be automatically joined by std::jthread

    if (exception_ptr) {
        std::rethrow_exception(exception_ptr);
    }

    if (!resultOpt.has_value()) {
        THROW_RUNTIME_ERROR("Function call timed out");
    }

    return resultOpt.value_or(ReturnType{});
#else
    auto future = std::async(std::launch::async, std::forward<Func>(func),
                             std::forward<Args>(args)...);

    auto status = future.wait_for(timeout);

    if (status == std::future_status::timeout) {
        THROW_RUNTIME_ERROR("Function call timed out");
    }

    // Get will propagate any exception from the called function
    return future.get();
#endif
}

/**
 * @brief Policy for cache expiration
 */
enum class CachePolicy {
    Never,        // Never expire cached values
    Count,        // Expire after N uses
    Time,         // Expire after time duration
    CountAndTime  // Expire after either condition
};

/**
 * @brief Cache configuration options
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
 * @brief Caches the result of a function call with given arguments
 *
 * @tparam Func The function type
 * @tparam Args The argument types
 * @param func The function to be called
 * @param args The arguments to be passed to the function
 * @return The cached result of the function call
 */
template <typename Func, typename... Args>
    requires std::invocable<std::decay_t<Func>, std::decay_t<Args>...>
[[nodiscard]] auto cacheCall(Func&& func, Args&&... args) {
    using ReturnType =
        std::invoke_result_t<std::decay_t<Func>, std::decay_t<Args>...>;
    static std::unordered_map<std::tuple<std::decay_t<Args>...>, ReturnType,
                              TupleHasher>
        cache;
    static std::shared_mutex mutex;

    auto key = std::make_tuple(std::forward<Args>(args)...);
    {
        std::shared_lock lock(mutex);
        if (auto it = cache.find(key); it != cache.end()) {
            return it->second;
        }
    }

    auto result =
        std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);

    {
        std::unique_lock lock(mutex);
        cache[key] = result;
    }

    return result;
}

/**
 * @brief Creates a memoized version of a function with cache control
 *
 * @tparam Func The function type
 * @tparam Duration Time duration type for cache TTL
 * @param func The function to memoize
 * @param options Cache configuration options
 * @return A memoized version of the function
 */
template <typename Func, typename Duration = std::chrono::seconds>
[[nodiscard]] auto memoize(Func&& func, CacheOptions<Duration> options = {}) {
    using FuncType = std::decay_t<Func>;

    return [func = std::forward<Func>(func), options]<typename... Args>(
               Args&&... args) -> std::invoke_result_t<FuncType, Args...> {
        using ReturnType = std::invoke_result_t<FuncType, Args...>;
        using KeyType = std::tuple<std::decay_t<Args>...>;

        // Cache entry with metadata
        struct CacheEntry {
            ReturnType value;
            std::chrono::steady_clock::time_point timestamp;
            std::atomic<size_t> use_count = 0;
        };

        // Create shared cache and mutex
        static auto cache = std::make_shared<
            std::unordered_map<KeyType, CacheEntry, TupleHasher>>();
        static auto mutex = std::make_shared<std::shared_mutex>();

        KeyType key{args...};
        bool needs_computation = false;

        // Check cache with shared lock first
        if (options.thread_safe) {
            std::shared_lock lock(*mutex);
            auto it = cache->find(key);

            if (it != cache->end()) {
                auto& entry = it->second;
                auto now = std::chrono::steady_clock::now();

                // Check expiration based on policy
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

                needs_computation = true;
            } else {
                needs_computation = true;
            }
        } else {
            // Non-thread-safe path
            auto it = cache->find(key);

            if (it != cache->end()) {
                auto& entry = it->second;
                auto now = std::chrono::steady_clock::now();

                // Check expiration based on policy
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

                needs_computation = true;
            } else {
                needs_computation = true;
            }
        }

        // Compute new value and update cache
        if (needs_computation) {
            auto result = std::invoke(func, std::forward<Args>(args)...);

            if (options.thread_safe) {
                std::unique_lock lock(*mutex);

                // Enforce max size limit with LRU-like eviction
                if (cache->size() >= options.max_size) {
                    // Simple eviction strategy - remove oldest entry
                    auto oldest = cache->begin();
                    for (auto it = cache->begin(); it != cache->end(); ++it) {
                        if (it->second.timestamp < oldest->second.timestamp) {
                            oldest = it;
                        }
                    }
                    cache->erase(oldest);
                }

                // Update or insert new entry
                (*cache)[key] = {
                    result, std::chrono::steady_clock::now(),
                    1  // Initial use count
                };
            } else {
                // Non-thread-safe path
                // Enforce max size limit with LRU-like eviction
                if (cache->size() >= options.max_size) {
                    // Simple eviction strategy - remove oldest entry
                    auto oldest = cache->begin();
                    for (auto it = cache->begin(); it != cache->end(); ++it) {
                        if (it->second.timestamp < oldest->second.timestamp) {
                            oldest = it;
                        }
                    }
                    cache->erase(oldest);
                }

                // Update or insert new entry
                (*cache)[key] = {
                    result, std::chrono::steady_clock::now(),
                    1  // Initial use count
                };
            }

            return result;
        }

        // This should never happen but is here as a safeguard
        return std::invoke(func, std::forward<Args>(args)...);
    };
}

/**
 * @brief Processes a batch of function calls in parallel
 *
 * @tparam Func The function type
 * @tparam Args The argument types
 * @param func The function to execute
 * @param argsList List of argument tuples
 * @param maxThreads Maximum number of threads (0 = hardware concurrency)
 * @return Vector of results
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

    // Use C++20 latch for thread coordination
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

    // Wait for all tasks to complete
    completion_latch.wait();

    // Rethrow any exception that occurred
    if (first_exception) {
        std::rethrow_exception(first_exception);
    }

    return results;
}

/**
 * @brief Processes a batch of function calls with given arguments
 *
 * @tparam Func The function type
 * @tparam Args The argument types
 * @param func The function to be called
 * @param argsList The list of argument tuples to be passed to the function
 * @return A vector containing the results of the function calls
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
 * @brief Creates an instrumented function that collects performance metrics
 *
 * @tparam Func The function type
 * @param func The function to instrument
 * @param name The function name for metrics
 * @return An instrumented version of the function
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

        std::string report() const {
            uint64_t count = call_count.load();
            if (count == 0)
                return function_name + ": No calls";

            auto avg_ns = total_execution_time / count;

            return std::format(
                "{}: {} calls, {} exceptions, avg time: {}ns, min: {}ns, max: "
                "{}ns",
                function_name, count, exception_count.load(), avg_ns.count(),
                min_execution_time.count(), max_execution_time.count());
        }
    };

    auto metrics = std::make_shared<Metrics>();
    metrics->function_name = name.empty() ? "anonymous_function" : name;

    return [func = std::forward<Func>(func), metrics]<typename... Args>(
               Args&&... args) -> std::invoke_result_t<Func, Args...> {
        metrics->call_count++;
        auto start = std::chrono::high_resolution_clock::now();

        try {
            if constexpr (std::is_void_v<std::invoke_result_t<Func, Args...>>) {
                std::invoke(func, std::forward<Args>(args)...);

                auto duration =
                    std::chrono::high_resolution_clock::now() - start;

                // Update metrics
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

                // Update metrics
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