#pragma once

#include <asio.hpp>
#include <asio/ssl.hpp>
#include "types.hpp"

/**
 * @file protocol.hpp
 * @brief MQTT protocol connection state, pending operation, and transport
 * interfaces/implementations.
 */

namespace mqtt {

/**
 * @enum ConnectionState
 * @brief Represents the current state of the MQTT connection.
 *
 * Used to track the lifecycle of a client connection to the MQTT broker.
 */
enum class ConnectionState {
    DISCONNECTED,  ///< The client is not connected to the broker.
    CONNECTING,    ///< The client is in the process of connecting.
    CONNECTED,     ///< The client is connected to the broker.
    DISCONNECTING  ///< The client is in the process of disconnecting.
};

/**
 * @struct PendingOperation
 * @brief Represents an in-flight MQTT operation for QoS tracking.
 *
 * Stores the message, timestamp of the operation, retry count, and a callback
 * to be invoked upon completion or error. Used for managing retransmissions and
 * acknowledgments for QoS 1/2 messages.
 */
struct PendingOperation {
    Message message;  ///< The MQTT message associated with this operation.
    std::chrono::steady_clock::time_point
        timestamp;  ///< Time when the operation was initiated.
    uint8_t retry_count{
        0};  ///< Number of times this operation has been retried.
    std::function<void(ErrorCode)>
        callback;  ///< Callback to invoke on completion or error.
};

/**
 * @class ITransport
 * @brief Abstract interface for network transport used by the MQTT client.
 *
 * Provides asynchronous connect, read, and write operations, as well as
 * connection state management. Implementations may use plain TCP, SSL/TLS, or
 * other transport mechanisms.
 */
class ITransport {
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~ITransport() = default;

    /**
     * @brief Asynchronously connect to the specified host and port.
     * @param host Hostname or IP address to connect to.
     * @param port Port number to connect to.
     * @param callback Callback invoked with the result of the connection
     * attempt.
     */
    virtual void async_connect(const std::string& host, uint16_t port,
                               std::function<void(ErrorCode)> callback) = 0;

    /**
     * @brief Asynchronously write data to the transport.
     * @param data Span of bytes to write.
     * @param callback Callback invoked with the result and number of bytes
     * written.
     */
    virtual void async_write(
        std::span<const uint8_t> data,
        std::function<void(ErrorCode, size_t)> callback) = 0;

    /**
     * @brief Asynchronously read data from the transport.
     * @param buffer Span of bytes to fill with received data.
     * @param callback Callback invoked with the result and number of bytes
     * read.
     */
    virtual void async_read(
        std::span<uint8_t> buffer,
        std::function<void(ErrorCode, size_t)> callback) = 0;

    /**
     * @brief Close the transport connection.
     */
    virtual void close() = 0;

    /**
     * @brief Check if the transport is currently open.
     * @return True if the transport is open, false otherwise.
     */
    virtual bool is_open() const = 0;
};

/**
 * @class TCPTransport
 * @brief Concrete implementation of ITransport using plain TCP sockets.
 *
 * Provides asynchronous connect, read, and write operations over a TCP socket.
 */
class TCPTransport : public ITransport {
private:
    asio::ip::tcp::socket socket_;  ///< Underlying TCP socket.

public:
    /**
     * @brief Construct a TCPTransport with the given io_context.
     * @param io_context The ASIO I/O context to use.
     */
    explicit TCPTransport(asio::io_context& io_context) : socket_(io_context) {}

    /**
     * @copydoc ITransport::async_connect
     */
    void async_connect(const std::string& host, uint16_t port,
                       std::function<void(ErrorCode)> callback) override;

    /**
     * @copydoc ITransport::async_write
     */
    void async_write(std::span<const uint8_t> data,
                     std::function<void(ErrorCode, size_t)> callback) override;

    /**
     * @copydoc ITransport::async_read
     */
    void async_read(std::span<uint8_t> buffer,
                    std::function<void(ErrorCode, size_t)> callback) override;

    /**
     * @copydoc ITransport::close
     */
    void close() override;

    /**
     * @copydoc ITransport::is_open
     */
    bool is_open() const override;
};

inline void TCPTransport::async_connect(
    const std::string& host, uint16_t port,
    std::function<void(ErrorCode)> callback) {
    asio::ip::tcp::resolver resolver(socket_.get_executor());
    resolver.async_resolve(
        host, std::to_string(port),
        [this, callback = std::move(callback)](
            const std::error_code& resolve_ec,
            const asio::ip::tcp::resolver::results_type& results) {
            if (resolve_ec) {
                callback(ErrorCode::SERVER_UNAVAILABLE);
                return;
            }
            asio::async_connect(socket_, results,
                                [callback](const std::error_code& connect_ec,
                                           const asio::ip::tcp::endpoint&) {
                                    callback(connect_ec
                                                 ? ErrorCode::SERVER_UNAVAILABLE
                                                 : ErrorCode::SUCCESS);
                                });
        });
}

inline void TCPTransport::async_write(
    std::span<const uint8_t> data,
    std::function<void(ErrorCode, size_t)> callback) {
    asio::async_write(
        socket_, asio::buffer(data.data(), data.size()),
        [callback = std::move(callback)](const std::error_code& ec,
                                         size_t bytes_transferred) {
            callback(ec ? ErrorCode::UNSPECIFIED_ERROR : ErrorCode::SUCCESS,
                     bytes_transferred);
        });
}

inline void TCPTransport::async_read(
    std::span<uint8_t> buffer,
    std::function<void(ErrorCode, size_t)> callback) {
    socket_.async_read_some(
        asio::buffer(buffer.data(), buffer.size()),
        [callback = std::move(callback)](const std::error_code& ec,
                                         size_t bytes_transferred) {
            callback(ec ? ErrorCode::UNSPECIFIED_ERROR : ErrorCode::SUCCESS,
                     bytes_transferred);
        });
}

inline void TCPTransport::close() {
    std::error_code ec;
    auto res = socket_.close(ec);
}

inline bool TCPTransport::is_open() const { return socket_.is_open(); }

/**
 * @class TLSTransport
 * @brief Concrete implementation of ITransport using SSL/TLS over TCP.
 *
 * Provides asynchronous connect, read, and write operations over an SSL/TLS
 * socket.
 */
class TLSTransport : public ITransport {
private:
    asio::ssl::stream<asio::ip::tcp::socket>
        ssl_socket_;  ///< Underlying SSL/TLS socket.

public:
    /**
     * @brief Construct a TLSTransport with the given io_context and
     * ssl_context.
     * @param io_context The ASIO I/O context to use.
     * @param ssl_context The ASIO SSL context to use.
     */
    TLSTransport(asio::io_context& io_context, asio::ssl::context& ssl_context)
        : ssl_socket_(io_context, ssl_context) {}

    /**
     * @copydoc ITransport::async_connect
     */
    void async_connect(const std::string& host, uint16_t port,
                       std::function<void(ErrorCode)> callback) override;

    /**
     * @copydoc ITransport::async_write
     */
    void async_write(std::span<const uint8_t> data,
                     std::function<void(ErrorCode, size_t)> callback) override;

    /**
     * @copydoc ITransport::async_read
     */
    void async_read(std::span<uint8_t> buffer,
                    std::function<void(ErrorCode, size_t)> callback) override;

    /**
     * @copydoc ITransport::close
     */
    void close() override;

    /**
     * @copydoc ITransport::is_open
     */
    bool is_open() const override;
};

inline void TLSTransport::async_connect(
    const std::string& host, uint16_t port,
    std::function<void(ErrorCode)> callback) {
    asio::ip::tcp::resolver resolver(ssl_socket_.get_executor());
    resolver.async_resolve(
        host, std::to_string(port),
        [this, host, callback = std::move(callback)](
            const std::error_code& resolve_ec,
            const asio::ip::tcp::resolver::results_type& results) {
            if (resolve_ec) {
                callback(ErrorCode::SERVER_UNAVAILABLE);
                return;
            }
            asio::async_connect(
                ssl_socket_.lowest_layer(), results,
                [this, host, callback](const std::error_code& connect_ec,
                                       const asio::ip::tcp::endpoint&) {
                    if (connect_ec) {
                        callback(ErrorCode::SERVER_UNAVAILABLE);
                        return;
                    }
                    // Perform SSL handshake
                    ssl_socket_.async_handshake(
                        asio::ssl::stream_base::client,
                        [callback](const std::error_code& handshake_ec) {
                            callback(handshake_ec ? ErrorCode::NOT_AUTHORIZED
                                                  : ErrorCode::SUCCESS);
                        });
                });
        });
}

inline void TLSTransport::async_write(
    std::span<const uint8_t> data,
    std::function<void(ErrorCode, size_t)> callback) {
    asio::async_write(
        ssl_socket_, asio::buffer(data.data(), data.size()),
        [callback = std::move(callback)](const std::error_code& ec,
                                         size_t bytes_transferred) {
            callback(ec ? ErrorCode::UNSPECIFIED_ERROR : ErrorCode::SUCCESS,
                     bytes_transferred);
        });
}

inline void TLSTransport::async_read(
    std::span<uint8_t> buffer,
    std::function<void(ErrorCode, size_t)> callback) {
    ssl_socket_.async_read_some(
        asio::buffer(buffer.data(), buffer.size()),
        [callback = std::move(callback)](const std::error_code& ec,
                                         size_t bytes_transferred) {
            callback(ec ? ErrorCode::UNSPECIFIED_ERROR : ErrorCode::SUCCESS,
                     bytes_transferred);
        });
}

inline void TLSTransport::close() {
    std::error_code ec;
    auto res = ssl_socket_.lowest_layer().close(ec);
}

inline bool TLSTransport::is_open() const {
    return ssl_socket_.lowest_layer().is_open();
}

}  // namespace mqtt
