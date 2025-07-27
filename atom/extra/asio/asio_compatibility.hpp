#pragma once

/**
 * @file asio_compatibility.hpp
 * @brief Advanced ASIO compatibility layer with cutting-edge C++23 concurrency primitives
 */

#include <atomic>
#include <concepts>
#include <coroutine>
#include <memory>
#include <span>
#include <thread>
#include <version>

// C++23 feature detection
#if __cpp_lib_atomic_wait >= 201907L
#define ATOM_HAS_ATOMIC_WAIT 1
#endif

#if __cpp_lib_jthread >= 201911L
#define ATOM_HAS_JTHREAD 1
#endif

#if __cpp_lib_barrier >= 201907L
#define ATOM_HAS_BARRIER 1
#endif

#ifdef USE_BOOST_ASIO
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/system/error_code.hpp>
#ifdef USE_SSL
#include <boost/asio/ssl.hpp>
#endif

namespace net {
    using namespace boost::asio;
}
using error_code = boost::system::error_code;
#else
#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/experimental/as_tuple.hpp>
#ifdef USE_SSL
#include <asio/ssl.hpp>
#endif

namespace net {
    using namespace asio;
}
using error_code = asio::error_code;
#endif

/**
 * @brief Common ASIO type aliases and namespace imports
 */
using tcp = net::ip::tcp;
using udp = net::ip::udp;
using net::co_spawn;
using net::detached;
using net::use_awaitable;
using net::this_coro::executor;

#ifdef USE_SSL
using ssl_context = net::ssl::context;
#ifdef USE_BOOST_ASIO
namespace ssl = boost::asio::ssl;
#else
namespace ssl = net::ssl;
#endif
#endif

/**
 * @brief Result tuple type for async operations
 */
template <typename T>
using result_tuple = std::tuple<error_code, T>;

/**
 * @brief Helper for wrapping async operations to return result tuples
 * @param op The async operation to wrap
 * @return Wrapped operation returning a tuple of (error_code, result)
 */
template <typename AsyncOperation>
auto as_tuple_awaitable(AsyncOperation&& op) {
    return std::forward<AsyncOperation>(op)(
        net::experimental::as_tuple(use_awaitable));
}

/**
 * @brief Advanced memory ordering concepts for lock-free programming
 */
namespace atom::extra::asio::concurrency {

/**
 * @brief Memory ordering utilities for high-performance concurrent operations
 */
enum class memory_order_policy {
    relaxed = static_cast<int>(std::memory_order_relaxed),
    acquire = static_cast<int>(std::memory_order_acquire),
    release = static_cast<int>(std::memory_order_release),
    acq_rel = static_cast<int>(std::memory_order_acq_rel),
    seq_cst = static_cast<int>(std::memory_order_seq_cst)
};

/**
 * @brief CPU pause instruction for optimized spinlocks
 */
inline void cpu_pause() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(_M_ARM64)
    __asm__ volatile("yield" ::: "memory");
#else
    std::this_thread::yield();
#endif
}

/**
 * @brief Cache line size for optimal memory layout
 */
constexpr std::size_t cache_line_size = std::hardware_destructive_interference_size;

/**
 * @brief Aligned allocation for cache-friendly data structures
 */
template<typename T, std::size_t Alignment = cache_line_size>
struct alignas(Alignment) cache_aligned {
    T value;

    template<typename... Args>
    constexpr cache_aligned(Args&&... args) : value(std::forward<Args>(args)...) {}

    constexpr T& get() noexcept { return value; }
    constexpr const T& get() const noexcept { return value; }
};

} // namespace atom::extra::asio::concurrency
