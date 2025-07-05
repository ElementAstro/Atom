/*
 * tcpclient.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-5-24

Description: TCP Client Class

*************************************************/

#ifndef ATOM_CONNECTION_TCPCLIENT_HPP
#define ATOM_CONNECTION_TCPCLIENT_HPP

#include <chrono>
#include <concepts>
#include <coroutine>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "atom/type/expected.hpp"
#include "atom/type/noncopyable.hpp"

namespace atom::connection {

// Forward declarations
class Error;

/**
 * @brief Task type for coroutine-based asynchronous operations
 */
template <typename T>
class [[nodiscard]] Task {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        T result;
        std::exception_ptr exception;

        Task get_return_object() {
            return Task(handle_type::from_promise(*this));
        }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void unhandled_exception() { exception = std::current_exception(); }
        void return_value(T value) { result = std::move(value); }
    };

    Task(handle_type h) : handle_(h) {}
    ~Task() {
        if (handle_)
            handle_.destroy();
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_)
                handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    T result() const {
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
        return handle_.promise().result;
    }

    bool done() const { return handle_.done(); }

    bool await_ready() const { return false; }
    void await_suspend([[maybe_unused]] std::coroutine_handle<> awaiting) {
        handle_.promise().exception = nullptr;
        handle_.promise().result = T{};
        handle_.resume();
    }
    T await_resume() {
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
        return handle_.promise().result;
    }

private:
    handle_type handle_;
};

template <>
struct Task<void>::promise_type {
    std::exception_ptr exception;

    Task<void> get_return_object() {
        return Task(handle_type::from_promise(*this));
    }
    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void unhandled_exception() { exception = std::current_exception(); }
    void return_void() {}
};

/**
 * @brief Concept for valid callback types
 */
template <typename T>
concept CallbackInvocable =
    std::invocable<T> || std::invocable<T, const std::vector<char>&> ||
    std::invocable<T, const std::string&>;

/**
 * @class TcpClient
 * @brief Represents a TCP client for connecting to a server and
 * sending/receiving data with modern C++20 features.
 */
class TcpClient : public NonCopyable {
public:
    using OnConnectedCallback = std::function<void()>;
    using OnDisconnectedCallback = std::function<void()>;
    using OnDataReceivedCallback = std::function<void(std::span<const char>)>;
    using OnErrorCallback = std::function<void(const std::system_error&)>;

    /**
     * @brief Configuration options for TCP client
     */
    struct Options {
        bool ipv6_enabled{false};         /**< Enable IPv6 support */
        bool keep_alive{true};            /**< Enable TCP keepalive */
        bool no_delay{true};              /**< Disable Nagle's algorithm */
        size_t receive_buffer_size{8192}; /**< Size of receive buffer */
        size_t send_buffer_size{8192};    /**< Size of send buffer */
    };

    /**
     * @brief Constructor.
     * @param options Configuration options for the TCP client
     */
    explicit TcpClient(Options options);

    /**
     * @brief Destructor.
     */
    ~TcpClient() override;

    /**
     * @brief Connects to a TCP server.
     * @param host The hostname or IP address of the server.
     * @param port The port number of the server.
     * @param timeout The connection timeout duration.
     * @return type::expected with void on success or error on failure
     */
    auto connect(
        std::string_view host, uint16_t port,
        std::chrono::milliseconds timeout = std::chrono::milliseconds::zero())
        -> type::expected<void, std::system_error>;

    /**
     * @brief Asynchronously connects to a TCP server.
     * @param host The hostname or IP address of the server.
     * @param port The port number of the server.
     * @param timeout The connection timeout duration.
     * @return Task that completes when connection succeeds or fails
     */
    auto connect_async(
        std::string_view host, uint16_t port,
        std::chrono::milliseconds timeout = std::chrono::milliseconds::zero())
        -> Task<type::expected<void, std::system_error>>;

    /**
     * @brief Disconnects from the server.
     */
    void disconnect();

    /**
     * @brief Sends data to the server.
     * @param data The data to be sent.
     * @return type::expected with bytes sent on success or error on failure
     */
    auto send(std::span<const char> data)
        -> type::expected<size_t, std::system_error>;

    /**
     * @brief Sends data to the server asynchronously.
     * @param data The data to be sent.
     * @return Task that completes when send succeeds or fails
     */
    auto send_async(std::span<const char> data)
        -> Task<type::expected<size_t, std::system_error>>;

    /**
     * @brief Receives data from the server.
     * @param max_size The maximum number of bytes to receive.
     * @param timeout The receive timeout duration.
     * @return type::expected with received data or error on failure
     */
    auto receive(size_t max_size, std::chrono::milliseconds timeout =
                                      std::chrono::milliseconds::zero())
        -> type::expected<std::vector<char>, std::system_error>;

    /**
     * @brief Receives data from the server asynchronously.
     * @param max_size The maximum number of bytes to receive.
     * @param timeout The receive timeout duration.
     * @return Task that completes when receive succeeds or fails
     */
    auto receive_async(size_t max_size, std::chrono::milliseconds timeout =
                                            std::chrono::milliseconds::zero())
        -> Task<type::expected<std::vector<char>, std::system_error>>;

    /**
     * @brief Checks if the client is connected to the server.
     * @return True if connected, false otherwise.
     */
    [[nodiscard]] auto isConnected() const -> bool;

    /**
     * @brief Sets the callback function to be called when connected to the
     * server.
     * @param callback The callback function.
     */
    template <CallbackInvocable Callback>
    void setOnConnectedCallback(Callback&& callback) {
        onConnectedCallback_ = std::forward<Callback>(callback);
    }

    /**
     * @brief Sets the callback function to be called when disconnected from the
     * server.
     * @param callback The callback function.
     */
    template <CallbackInvocable Callback>
    void setOnDisconnectedCallback(Callback&& callback) {
        onDisconnectedCallback_ = std::forward<Callback>(callback);
    }

    /**
     * @brief Sets the callback function to be called when data is received from
     * the server.
     * @param callback The callback function.
     */
    template <CallbackInvocable Callback>
    void setOnDataReceivedCallback(Callback&& callback) {
        onDataReceivedCallback_ = std::forward<Callback>(callback);
    }

    /**
     * @brief Sets the callback function to be called when an error occurs.
     * @param callback The callback function.
     */
    template <CallbackInvocable Callback>
    void setOnErrorCallback(Callback&& callback) {
        onErrorCallback_ = std::forward<Callback>(callback);
    }

    /**
     * @brief Starts receiving data from the server.
     * @param buffer_size The size of the receive buffer.
     */
    void startReceiving(size_t buffer_size);

    /**
     * @brief Stops receiving data from the server.
     */
    void stopReceiving();

    /**
     * @brief Gets the last error that occurred
     * @return The last error
     */
    [[nodiscard]] auto getLastError() const -> const std::system_error&;

private:
    class Impl; /**< Forward declaration of the implementation class. */
    std::unique_ptr<Impl> impl_; /**< Pointer to the implementation object. */

    // Callbacks stored in the main class to avoid exposing them in the impl
    OnConnectedCallback onConnectedCallback_;
    OnDisconnectedCallback onDisconnectedCallback_;
    OnDataReceivedCallback onDataReceivedCallback_;
    OnErrorCallback onErrorCallback_;
};

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_TCPCLIENT_HPP
