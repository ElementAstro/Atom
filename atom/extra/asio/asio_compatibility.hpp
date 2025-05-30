#pragma once

/**
 * @file asio_compatibility.hpp
 * @brief Compatibility layer for using either standalone or Boost ASIO
 */

#ifdef USE_BOOST_ASIO
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#ifdef USE_SSL
#include <boost/asio/ssl.hpp>
#endif

namespace net = boost::asio;
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

namespace net = asio;
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