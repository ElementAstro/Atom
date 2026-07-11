/*
 * tcpclient.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-5-24

Description: TCP Client Class with native socket backend

*************************************************/

#ifndef ATOM_CONNECTION_TCPCLIENT_HPP
#define ATOM_CONNECTION_TCPCLIENT_HPP

#include <chrono>
#include <concepts>
#include <coroutine>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "atom/type/expected.hpp"
#include "atom/type/noncopyable.hpp"
#include "tcp_common.hpp"

namespace atom::connection {

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

    explicit Task(handle_type h) : handle_(h) {}
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

    [[nodiscard]] T result() const {
        if (handle_.promise().exception) {
            std::rethrow_exception(handle_.promise().exception);
        }
        return handle_.promise().result;
    }

    [[nodiscard]] bool done() const { return handle_.done(); }

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
    std::invocable<T> || std::invocable<T, std::span<const char>> ||
    std::invocable<T, const std::system_error&>;

/**
 * @class TcpClient
 * @brief TCP client using native socket API with C++20 coroutines
 *
 * Features:
 * - Cross-platform (Windows/Linux/macOS)
 * - Platform-specific optimizations (epoll/kqueue)
 * - Coroutine-based async operations
 */
class TcpClient : public atom::type::NonCopyable {
public:
    using OnConnectedCallback = TcpCallbacks::OnConnected;
    using OnDisconnectedCallback = TcpCallbacks::OnDisconnected;
    using OnDataReceivedCallback = TcpCallbacks::OnDataReceived;
    using OnErrorCallback = TcpCallbacks::OnError;

    /**
     * @brief Configuration options for TCP client
     */
    struct Options {
        bool ipv6_enabled{false};
        bool keep_alive{true};
        bool no_delay{true};
        size_t receive_buffer_size{8192};
        size_t send_buffer_size{8192};

        // Conversion from unified config
        static Options fromConfig(const TcpClientConfig& config) {
            return Options{.ipv6_enabled = config.ipv6_enabled,
                           .keep_alive = config.keep_alive,
                           .no_delay = config.no_delay,
                           .receive_buffer_size = config.receive_buffer_size,
                           .send_buffer_size = config.send_buffer_size};
        }
    };

    explicit TcpClient(Options options);
    explicit TcpClient(const TcpClientConfig& config);
    ~TcpClient() override;

    /**
     * @brief Connects to a TCP server
     */
    auto connect(
        std::string_view host, uint16_t port,
        std::chrono::milliseconds timeout = std::chrono::milliseconds::zero())
        -> type::expected<void, std::system_error>;

    /**
     * @brief Asynchronously connects to a TCP server
     */
    auto connect_async(
        std::string_view host, uint16_t port,
        std::chrono::milliseconds timeout = std::chrono::milliseconds::zero())
        -> Task<type::expected<void, std::system_error>>;

    /**
     * @brief Disconnects from the server
     */
    void disconnect();

    /**
     * @brief Sends data to the server
     */
    auto send(std::span<const char> data)
        -> type::expected<size_t, std::system_error>;

    /**
     * @brief Sends data to the server asynchronously
     */
    auto send_async(std::span<const char> data)
        -> Task<type::expected<size_t, std::system_error>>;

    /**
     * @brief Receives data from the server
     */
    auto receive(size_t max_size, std::chrono::milliseconds timeout =
                                      std::chrono::milliseconds::zero())
        -> type::expected<std::vector<char>, std::system_error>;

    /**
     * @brief Receives data asynchronously
     */
    auto receive_async(size_t max_size, std::chrono::milliseconds timeout =
                                            std::chrono::milliseconds::zero())
        -> Task<type::expected<std::vector<char>, std::system_error>>;

    [[nodiscard]] auto isConnected() const -> bool;
    [[nodiscard]] auto getLastError() const -> const std::system_error&;

    template <CallbackInvocable Callback>
    void setOnConnectedCallback(Callback&& callback) {
        onConnectedCallback_ = std::forward<Callback>(callback);
    }

    template <CallbackInvocable Callback>
    void setOnDisconnectedCallback(Callback&& callback) {
        onDisconnectedCallback_ = std::forward<Callback>(callback);
    }

    template <CallbackInvocable Callback>
    void setOnDataReceivedCallback(Callback&& callback) {
        onDataReceivedCallback_ = std::forward<Callback>(callback);
    }

    template <CallbackInvocable Callback>
    void setOnErrorCallback(Callback&& callback) {
        onErrorCallback_ = std::forward<Callback>(callback);
    }

    void startReceiving(size_t buffer_size);
    void stopReceiving();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    OnConnectedCallback onConnectedCallback_;
    OnDisconnectedCallback onDisconnectedCallback_;
    OnDataReceivedCallback onDataReceivedCallback_;
    OnErrorCallback onErrorCallback_;
};

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_TCPCLIENT_HPP
