#ifndef ATOM_EXTRA_BEAST_WS_HPP
#define ATOM_EXTRA_BEAST_WS_HPP

#include <spdlog/spdlog.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <chrono>
#include <concepts>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace beast = boost::beast;
namespace net = boost::asio;
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

template <typename T>
concept CompletionHandler = requires(T h, beast::error_code ec) {
    { h(ec) } -> std::same_as<void>;
};

template <typename T>
concept DataCompletionHandler =
    requires(T h, beast::error_code ec, std::size_t n) {
        { h(ec, n) } -> std::same_as<void>;
    };

template <typename T>
concept ReadCompletionHandler =
    requires(T h, beast::error_code ec, std::string s) {
        { h(ec, s) } -> std::same_as<void>;
    };

template <typename T>
concept JsonCompletionHandler = requires(T h, beast::error_code ec, json j) {
    { h(ec, j) } -> std::same_as<void>;
};

/**
 * @class WSClient
 * @brief A WebSocket client class for managing WebSocket connections and
 * communication.
 */
class WSClient : public std::enable_shared_from_this<WSClient> {
public:
    /**
     * @brief Constructs a WSClient with the given I/O context.
     * @param ioc The I/O context to use for asynchronous operations.
     * @throws std::bad_alloc If memory allocation fails for internal
     * components.
     */
    explicit WSClient(net::io_context& ioc);

    WSClient(const WSClient&) = delete;
    WSClient& operator=(const WSClient&) = delete;
    WSClient(WSClient&&) noexcept = default;
    WSClient& operator=(WSClient&&) noexcept = default;
    ~WSClient() noexcept;

    /**
     * @brief Sets the timeout duration for WebSocket operations.
     * @param timeout The timeout duration in seconds.
     */
    void setTimeout(std::chrono::seconds timeout) noexcept;

    /**
     * @brief Sets the reconnection options.
     * @param retries The number of reconnection attempts.
     * @param interval The interval between reconnection attempts in seconds.
     * @throws std::invalid_argument If retries is negative or interval is
     * non-positive.
     */
    void setReconnectOptions(int retries, std::chrono::seconds interval);

    /**
     * @brief Sets the interval for sending ping messages.
     * @param interval The ping interval in seconds.
     * @throws std::invalid_argument If interval is non-positive.
     */
    void setPingInterval(std::chrono::seconds interval);

    /**
     * @brief Connects to the WebSocket server.
     * @param host The server host.
     * @param port The server port.
     * @throws beast::system_error On connection failure.
     * @throws std::invalid_argument If host or port is invalid.
     */
    void connect(std::string_view host, std::string_view port);

    /**
     * @brief Sends a message to the WebSocket server.
     * @param message The message to send.
     * @throws beast::system_error On sending failure.
     * @throws std::logic_error If not connected.
     */
    void send(std::string_view message);

    /**
     * @brief Receives a message from the WebSocket server.
     * @return The received message.
     * @throws beast::system_error On receiving failure.
     * @throws std::logic_error If not connected.
     */
    [[nodiscard]] std::string receive();

    /**
     * @brief Checks if the connection is established.
     * @return true if connected, false otherwise.
     */
    [[nodiscard]] bool isConnected() const noexcept;

    /**
     * @brief Closes the WebSocket connection.
     * @throws beast::system_error On closing failure.
     */
    void close();

    /**
     * @brief Asynchronously connects to the WebSocket server.
     * @param host The server host.
     * @param port The server port.
     * @param handler The handler to call when the operation completes.
     * @throws std::invalid_argument If host or port is invalid (synchronous
     * check).
     */
    template <CompletionHandler ConnectHandler>
    void asyncConnect(std::string_view host, std::string_view port,
                      ConnectHandler&& handler);

    /**
     * @brief Asynchronously sends a message to the WebSocket server.
     * @param message The message to send.
     * @param handler The handler to call when the operation completes.
     */
    template <DataCompletionHandler WriteHandler>
    void asyncSend(std::string_view message, WriteHandler&& handler);

    /**
     * @brief Asynchronously receives a message from the WebSocket server.
     * @param handler The handler to call when the operation completes.
     */
    template <ReadCompletionHandler ReadHandler>
    void asyncReceive(ReadHandler&& handler);

    /**
     * @brief Asynchronously closes the WebSocket connection.
     * @param handler The handler to call when the operation completes.
     */
    template <CompletionHandler CloseHandler>
    void asyncClose(CloseHandler&& handler);

    /**
     * @brief Asynchronously sends a JSON object to the WebSocket server.
     * @param json_data The JSON object to send.
     * @param handler The handler to call when the operation completes.
     */
    template <DataCompletionHandler JsonWriteHandler>
    void asyncSendJson(const json& json_data, JsonWriteHandler&& handler);

    /**
     * @brief Asynchronously receives a JSON object from the WebSocket server.
     * @param handler The handler to call when the operation completes.
     */
    template <JsonCompletionHandler JsonHandler>
    void asyncReceiveJson(JsonHandler&& handler);

private:
    /**
     * @brief Starts the ping timer to send periodic ping messages.
     */
    void startPing();

    /**
     * @brief Handles connection errors and retries if necessary.
     * @param ec The error code from the failed connection attempt.
     * @param handler The handler to call when the reconnection process
     * completes or fails definitively.
     */
    template <CompletionHandler ConnectHandler>
    void handleConnectError(beast::error_code ec, ConnectHandler&& handler);

    /**
     * @brief Validates connection parameters.
     * @param host The server host.
     * @param port The server port.
     * @throws std::invalid_argument If parameters are invalid.
     */
    static void validateConnectionParams(std::string_view host,
                                         std::string_view port);

    std::shared_ptr<tcp::resolver> resolver_;
    std::shared_ptr<websocket::stream<tcp::socket>> ws_;
    std::shared_ptr<net::steady_timer> ping_timer_;
    std::chrono::seconds timeout_{30};
    std::chrono::seconds ping_interval_{10};
    std::chrono::seconds reconnect_interval_{5};
    int max_retries_{3};
    int retry_count_{0};
    bool is_connected_{false};
    std::string last_host_;
    std::string last_port_;
};

template <CompletionHandler ConnectHandler>
void WSClient::asyncConnect(std::string_view host, std::string_view port,
                            ConnectHandler&& handler) {
    try {
        validateConnectionParams(host, port);
        last_host_ = host;
        last_port_ = port;
        retry_count_ = 0;

        resolver_->async_resolve(
            std::string(host), std::string(port),
            [this, self = shared_from_this(),
             handler = std::forward<ConnectHandler>(handler)](
                beast::error_code ec,
                tcp::resolver::results_type results) mutable {
                if (ec) {
                    handleConnectError(ec, std::move(handler));
                    return;
                }

                beast::get_lowest_layer(*ws_).async_connect(
                    results, [this, self = shared_from_this(),
                              handler = std::move(handler),
                              host_str = std::string(last_host_)](
                                 beast::error_code ec,
                                 const tcp::resolver::results_type::
                                     endpoint_type&) mutable {
                        if (ec) {
                            handleConnectError(ec, std::move(handler));
                            return;
                        }

                        ws_->async_handshake(
                            host_str, "/",
                            [this, self = shared_from_this(),
                             handler = std::move(handler)](
                                beast::error_code ec) mutable {
                                if (!ec) {
                                    is_connected_ = true;
                                    startPing();
                                } else {
                                    handleConnectError(ec, std::move(handler));
                                    return;
                                }
                                handler(ec);
                            });
                    });
            });
    } catch (const std::invalid_argument&) {
        net::post(ws_->get_executor(),
                  [handler = std::forward<ConnectHandler>(handler)]() mutable {
                      handler(beast::error_code{net::error::invalid_argument,
                                                beast::generic_category()});
                  });
    }
}

template <DataCompletionHandler WriteHandler>
void WSClient::asyncSend(std::string_view message, WriteHandler&& handler) {
    if (!is_connected_) {
        net::post(ws_->get_executor(),
                  [handler = std::forward<WriteHandler>(handler)]() mutable {
                      handler(beast::error_code{net::error::not_connected,
                                                beast::generic_category()},
                              0);
                  });
        return;
    }

    auto shared_message = std::make_shared<std::string>(message);
    ws_->async_write(
        net::buffer(*shared_message),
        [shared_message, handler = std::forward<WriteHandler>(handler),
         self = shared_from_this()](beast::error_code ec,
                                    std::size_t bytes_transferred) {
            handler(ec, bytes_transferred);
        });
}

template <ReadCompletionHandler ReadHandler>
void WSClient::asyncReceive(ReadHandler&& handler) {
    if (!is_connected_) {
        net::post(ws_->get_executor(),
                  [handler = std::forward<ReadHandler>(handler)]() mutable {
                      handler(beast::error_code{net::error::not_connected,
                                                beast::generic_category()},
                              "");
                  });
        return;
    }

    auto buffer = std::make_shared<beast::flat_buffer>();
    ws_->async_read(
        *buffer,
        [buffer, handler = std::forward<ReadHandler>(handler),
         self = shared_from_this()](beast::error_code ec, std::size_t) {
            if (ec) {
                handler(ec, "");
            } else {
                handler(ec, beast::buffers_to_string(buffer->data()));
            }
        });
}

template <CompletionHandler CloseHandler>
void WSClient::asyncClose(CloseHandler&& handler) {
    if (!is_connected_) {
        net::post(ws_->get_executor(),
                  [handler = std::forward<CloseHandler>(handler)]() mutable {
                      handler(beast::error_code());
                  });
        return;
    }

    ws_->async_close(websocket::close_code::normal,
                     [this, handler = std::forward<CloseHandler>(handler),
                      self = shared_from_this()](beast::error_code ec) {
                         is_connected_ = false;
                         if (ping_timer_) {
                             ping_timer_->cancel();
                         }
                         handler(ec);
                     });
}

template <DataCompletionHandler JsonWriteHandler>
void WSClient::asyncSendJson(const json& json_data,
                             JsonWriteHandler&& handler) {
    if (!is_connected_) {
        net::post(
            ws_->get_executor(),
            [handler = std::forward<JsonWriteHandler>(handler)]() mutable {
                handler(beast::error_code{net::error::not_connected,
                                          beast::generic_category()},
                        0);
            });
        return;
    }

    try {
        std::string message = json_data.dump();
        asyncSend(message, std::forward<JsonWriteHandler>(handler));
    } catch (const json::exception& e) {
        spdlog::error("JSON serialization error: {}", e.what());
        net::post(
            ws_->get_executor(),
            [handler = std::forward<JsonWriteHandler>(handler)]() mutable {
                handler(beast::error_code{net::error::invalid_argument,
                                          beast::generic_category()},
                        0);
            });
    }
}

template <JsonCompletionHandler JsonHandler>
void WSClient::asyncReceiveJson(JsonHandler&& handler) {
    if (!is_connected_) {
        net::post(ws_->get_executor(),
                  [handler = std::forward<JsonHandler>(handler)]() mutable {
                      handler(beast::error_code{net::error::not_connected,
                                                beast::generic_category()},
                              json{});
                  });
        return;
    }

    asyncReceive([handler = std::forward<JsonHandler>(handler),
                  self = shared_from_this()](beast::error_code ec,
                                             const std::string& message) {
        if (ec) {
            handler(ec, json{});
        } else {
            try {
                auto json_data = json::parse(message);
                handler(ec, std::move(json_data));
            } catch (const json::parse_error& e) {
                handler(beast::error_code{e.id, beast::generic_category()},
                        json{});
            }
        }
    });
}

template <CompletionHandler ConnectHandler>
void WSClient::handleConnectError(beast::error_code ec,
                                  ConnectHandler&& handler) {
    is_connected_ = false;
    if (retry_count_ < max_retries_) {
        ++retry_count_;
        spdlog::warn(
            "Connection failed: {}. Retrying attempt {}/{} in {} seconds...",
            ec.message(), retry_count_, max_retries_,
            reconnect_interval_.count());

        if (ws_ && ws_->is_open()) {
            beast::error_code close_ec;
            auto result = beast::get_lowest_layer(*ws_).close(close_ec);
            if (close_ec && close_ec != net::error::not_connected) {
                spdlog::debug(
                    "Error closing socket before retry: {} (result: {})",
                    close_ec.message(), result.message());
            }
        }

        ws_ = std::make_shared<websocket::stream<tcp::socket>>(
            net::make_strand(ping_timer_->get_executor()));

        ping_timer_->expires_after(reconnect_interval_);
        ping_timer_->async_wait([this, self = shared_from_this(),
                                 handler =
                                     std::forward<ConnectHandler>(handler)](
                                    beast::error_code wait_ec) mutable {
            if (!wait_ec) {
                asyncConnect(last_host_, last_port_,
                             std::forward<ConnectHandler>(handler));
            } else {
                spdlog::error("Reconnect timer failed: {}", wait_ec.message());
                handler(wait_ec);
            }
        });
    } else {
        spdlog::error("Failed to connect after {} retries: {}. Giving up.",
                      max_retries_, ec.message());
        handler(ec);
    }
}

#endif  // ATOM_EXTRA_BEAST_WS_HPP
