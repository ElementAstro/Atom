/*
 * tcpclient.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-5-24

Description: TCP Client Class

*************************************************/

#include "tcpclient.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/epoll.h>
#endif
#ifdef __APPLE__
#include <sys/event.h>
#endif
#endif

namespace atom::connection {

namespace {
// Helper function to create system_error from socket errors
std::system_error createSystemError(const std::string& message) {
#ifdef _WIN32
    return std::system_error(WSAGetLastError(), std::system_category(),
                             message);
#else
    return std::system_error(errno, std::system_category(), message);
#endif
}

// Helper to make socket non-blocking
bool setNonBlocking(int socket, bool nonBlocking) {
#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1)
        return false;
    flags = nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return fcntl(socket, F_SETFL, flags) == 0;
#endif
}
}  // namespace

class TcpClient::Impl {
public:
    explicit Impl(const Options& options) : options_(options) {
        try {
            // Initialize socket and platform-specific resources (same as
            // before)
            spdlog::info("TCP Client initialized with IPv6: {}",
                         options_.ipv6_enabled);

            socket_ = socket(options_.ipv6_enabled ? AF_INET6 : AF_INET,
                             SOCK_STREAM, IPPROTO_TCP);
            if (socket_ < 0) {
                throw createSystemError("Socket creation failed");
            }

            configureSocket();

// Epoll for Linux, kqueue for macOS
#if defined(__linux__)
            epoll_fd_ = epoll_create1(0);
            if (epoll_fd_ == -1) {
                throw createSystemError(
                    "Failed to create epoll file descriptor");
            }
#elif defined(__APPLE__)
            kqueue_fd_ = kqueue();
            if (kqueue_fd_ == -1) {
                throw createSystemError(
                    "Failed to create kqueue file descriptor");
            }
#endif
        } catch (const std::exception& e) {
            spdlog::error("Initialization failed: {}", e.what());
            last_error_ = createSystemError("Initialization failed");
            cleanupResources();
            throw;
        }
    }

    ~Impl() { cleanupResources(); }

    type::expected<void, std::system_error> connect(
        std::string_view host, uint16_t port,
        std::chrono::milliseconds timeout) {
        try {
            spdlog::info("Connecting to {}:{}", host, port);

            if (port == 0) {
                last_error_ = std::system_error(
                    std::make_error_code(std::errc::invalid_argument),
                    "Invalid port number");
                return type::unexpected(last_error_);
            }

            struct addrinfo hints = {};
            struct addrinfo* result = nullptr;
            hints.ai_family = options_.ipv6_enabled ? AF_UNSPEC : AF_INET;
            hints.ai_socktype = SOCK_STREAM;

            int status =
                getaddrinfo(std::string(host).c_str(),
                            std::to_string(port).c_str(), &hints, &result);
            if (status != 0) {
                last_error_ = std::system_error(
                    std::make_error_code(std::errc::host_unreachable),
                    "Failed to resolve hostname: " +
                        std::string(gai_strerror(status)));
                return type::unexpected(last_error_);
            }

            struct AddrInfoGuard {
                addrinfo* info;
                ~AddrInfoGuard() {
                    if (info)
                        freeaddrinfo(info);
                }
            } addrGuard{result};

            for (struct addrinfo* rp = result; rp != nullptr;
                 rp = rp->ai_next) {
                if (!setNonBlocking(socket_, true)) {
                    continue;
                }

                status = ::connect(socket_, rp->ai_addr, rp->ai_addrlen);

#ifdef _WIN32
                if (status == SOCKET_ERROR &&
                    WSAGetLastError() != WSAEWOULDBLOCK) {
                    continue;
                }
#else
                if (status < 0 && errno != EINPROGRESS) {
                    continue;
                }
#endif

                if (!waitForConnectComplete(timeout)) {
                    continue;
                }

                int error = 0;
                socklen_t len = sizeof(error);
                if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, &error, &len) <
                        0 ||
                    error != 0) {
                    continue;
                }

                setNonBlocking(socket_, false);
                connected_ = true;

#if defined(__linux__)
                struct epoll_event event = {};
                event.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP;
                event.data.fd = socket_;
                if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, socket_, &event) ==
                    -1) {
                    last_error_ =
                        createSystemError("Failed to add socket to epoll");
                    return type::unexpected(last_error_);
                }
#elif defined(__APPLE__)
                struct kevent event;
                EV_SET(&event, socket_, EVFILT_READ, EV_ADD, 0, 0, nullptr);
                if (kevent(kqueue_fd_, &event, 1, nullptr, 0, nullptr) == -1) {
                    last_error_ =
                        createSystemError("Failed to add socket to kqueue");
                    return type::unexpected(last_error_);
                }
#endif

                spdlog::info("Connected to {}:{}", host, port);

                if (onConnectedCallback_) {
                    onConnectedCallback_();
                }

                return {};  // Success
            }

            last_error_ = std::system_error(
                std::make_error_code(std::errc::connection_refused),
                "Failed to connect to any resolved address");
            return type::unexpected(last_error_);
        } catch (const std::exception& e) {
            spdlog::error("Connection failed: {}", e.what());
            last_error_ = createSystemError("Connection failed");
            return type::unexpected(last_error_);
        }
    }

    Task<type::expected<void, std::system_error>> connect_async(
        std::string_view host, uint16_t port,
        std::chrono::milliseconds timeout) {
        auto result = connect(host, port, timeout);
        co_return result;
    }

    void disconnect() {
        if (connected_.exchange(false)) {
            stopReceiving();
            cleanupResources();

            spdlog::info("Disconnected from the server.");

            if (onDisconnectedCallback_) {
                onDisconnectedCallback_();
            }
        }
    }

    type::expected<size_t, std::system_error> send(std::span<const char> data) {
        if (!connected_.load(std::memory_order_acquire)) {
            spdlog::warn("Not connected, cannot send data.");
            last_error_ = std::system_error(
                std::make_error_code(std::errc::not_connected),
                "Not connected");
            return type::unexpected(last_error_);
        }

        try {
            size_t total_sent = 0;
            size_t remaining = data.size();
            spdlog::debug("Sending {} bytes.", remaining);

            while (remaining > 0) {
                size_t chunk_size =
                    std::min(remaining, options_.send_buffer_size);
                ssize_t bytes_sent =
                    ::send(socket_, data.data() + total_sent, chunk_size, 0);

                if (bytes_sent < 0) {
                    spdlog::error("Send failed: {}", strerror(errno));
                    last_error_ = createSystemError("Send failed");
                    return type::unexpected(last_error_);
                }

                total_sent += bytes_sent;
                remaining -= bytes_sent;
            }

            spdlog::debug("Sent {} bytes successfully.", total_sent);
            return total_sent;
        } catch (const std::exception& e) {
            spdlog::error("Send operation failed: {}", e.what());
            last_error_ =
                std::system_error(std::make_error_code(std::errc::io_error),
                                  "Send operation failed");
            return type::unexpected(last_error_);
        }
    }

    Task<type::expected<size_t, std::system_error>> send_async(
        std::span<const char> data) {
        auto result = send(data);
        co_return result;
    }

    type::expected<std::vector<char>, std::system_error> receive(
        size_t max_size, std::chrono::milliseconds timeout) {
        if (!connected_.load(std::memory_order_acquire)) {
            spdlog::warn("Not connected, cannot receive data.");
            last_error_ = std::system_error(
                std::make_error_code(std::errc::not_connected),
                "Not connected");
            return type::unexpected(last_error_);
        }

        try {
            if (!waitForReceiveReady(timeout)) {
                last_error_ = std::system_error(
                    std::make_error_code(std::errc::timed_out),
                    "Receive timeout");
                return type::unexpected(last_error_);
            }

            std::vector<char> buffer(max_size);
            ssize_t bytes_received =
                ::recv(socket_, buffer.data(), max_size, 0);

            if (bytes_received < 0) {
                last_error_ = createSystemError("Receive failed");
                return type::unexpected(last_error_);
            } else if (bytes_received == 0) {
                connected_ = false;
                last_error_ = std::system_error(
                    std::make_error_code(std::errc::connection_reset),
                    "Connection closed by peer");
                return type::unexpected(last_error_);
            }

            buffer.resize(bytes_received);
            spdlog::debug("Received {} bytes.", bytes_received);
            return buffer;
        } catch (const std::exception& e) {
            spdlog::error("Receive operation failed: {}", e.what());
            last_error_ =
                std::system_error(std::make_error_code(std::errc::io_error),
                                  "Receive operation failed");
            return type::unexpected(last_error_);
        }
    }

    Task<type::expected<std::vector<char>, std::system_error>> receive_async(
        size_t max_size, std::chrono::milliseconds timeout) {
        auto result = receive(max_size, timeout);
        co_return result;
    }

    bool isConnected() const {
        return connected_.load(std::memory_order_acquire);
    }

    void setOnConnectedCallback(std::function<void()> callback) {
        onConnectedCallback_ = std::move(callback);
    }

    void setOnDisconnectedCallback(std::function<void()> callback) {
        onDisconnectedCallback_ = std::move(callback);
    }

    void setOnDataReceivedCallback(
        std::function<void(std::span<const char>)> callback) {
        onDataReceivedCallback_ = std::move(callback);
    }

    void setOnErrorCallback(
        std::function<void(const std::system_error&)> callback) {
        onErrorCallback_ = std::move(callback);
    }

    const std::system_error& getLastError() const { return last_error_; }

    void startReceiving(size_t buffer_size) {
        if (!connected_.load(std::memory_order_acquire)) {
            spdlog::warn("Not connected, cannot start receiving.");
            return;
        }

        receiving_thread_ =
            std::jthread([this, buffer_size](std::stop_token stop_token) {
                receiveLoop(buffer_size, stop_token);
            });

        spdlog::info("Started receiving data.");
    }

    void stopReceiving() {
        receiving_stopped_.store(true, std::memory_order_release);

        if (receiving_thread_.joinable()) {
            receiving_thread_.request_stop();
            receiving_thread_.join();
            spdlog::info("Stopped receiving data.");
        }
    }

private:
    void cleanupResources() {
        if (socket_ != -1) {
#ifdef _WIN32
            closesocket(socket_);
#else
            close(socket_);
#endif
            socket_ = -1;
            spdlog::info("Socket closed and resources cleaned up.");
        }

#if defined(__linux__)
        if (epoll_fd_ != -1) {
            close(epoll_fd_);
            epoll_fd_ = -1;
        }
#elif defined(__APPLE__)
        if (kqueue_fd_ != -1) {
            close(kqueue_fd_);
            kqueue_fd_ = -1;
        }
#endif
    }

    void receiveLoop(size_t buffer_size, const std::stop_token& stop_token) {
        std::vector<char> buffer(buffer_size);

        spdlog::debug("Receiving data with buffer size: {}", buffer_size);

        while (!receiving_stopped_.load(std::memory_order_acquire) &&
               !stop_token.stop_requested()) {
            try {
                ssize_t bytes_read =
                    ::recv(socket_, buffer.data(), buffer.size(), 0);

                if (bytes_read < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(100));
                        continue;
                    }
                    last_error_ = createSystemError("Receive failed");
                    if (onErrorCallback_) {
                        onErrorCallback_(last_error_);
                    }
                    break;
                } else if (bytes_read == 0) {
                    spdlog::warn("Connection closed by peer.");
                    connected_ = false;
                    break;
                }

                std::span<const char> data_view(buffer.data(), bytes_read);

                if (onDataReceivedCallback_) {
                    onDataReceivedCallback_(data_view);
                }

                spdlog::debug("Received {} bytes.", bytes_read);
            } catch (const std::exception& e) {
                spdlog::error("Receive error: {}", e.what());
                last_error_ = createSystemError("Receive error");
                if (onErrorCallback_) {
                    onErrorCallback_(last_error_);
                }
                break;
            }
        }

        stopReceiving();
    }

    bool waitForConnectComplete(std::chrono::milliseconds timeout) {
        fd_set write_fds, error_fds;
        FD_ZERO(&write_fds);
        FD_ZERO(&error_fds);

#ifdef _WIN32
        FD_SET(socket_, &write_fds);
        FD_SET(socket_, &error_fds);
#else
        FD_SET(socket_, &write_fds);
        FD_SET(socket_, &error_fds);
#endif

        struct timeval tv;
        tv.tv_sec = timeout.count() / 1000;
        tv.tv_usec = (timeout.count() % 1000) * 1000;

        int result =
            select(socket_ + 1, nullptr, &write_fds, &error_fds,
                   timeout > std::chrono::milliseconds::zero() ? &tv : nullptr);
        return result > 0 && FD_ISSET(socket_, &write_fds);
    }

    bool waitForSendReady(std::chrono::milliseconds timeout) {
        fd_set write_fds;
        FD_ZERO(&write_fds);

#ifdef _WIN32
        FD_SET(socket_, &write_fds);
#else
        FD_SET(socket_, &write_fds);
#endif

        struct timeval tv;
        tv.tv_sec = timeout.count() / 1000;
        tv.tv_usec = (timeout.count() % 1000) * 1000;

        int result =
            select(socket_ + 1, nullptr, &write_fds, nullptr,
                   timeout > std::chrono::milliseconds::zero() ? &tv : nullptr);
        return result > 0 && FD_ISSET(socket_, &write_fds);
    }

    bool waitForReceiveReady(std::chrono::milliseconds timeout) {
        fd_set read_fds;
        FD_ZERO(&read_fds);

#ifdef _WIN32
        FD_SET(socket_, &read_fds);
#else
        FD_SET(socket_, &read_fds);
#endif

        struct timeval tv;
        tv.tv_sec = timeout.count() / 1000;
        tv.tv_usec = (timeout.count() % 1000) * 1000;

        int result =
            select(socket_ + 1, &read_fds, nullptr, nullptr,
                   timeout > std::chrono::milliseconds::zero() ? &tv : nullptr);
        return result > 0 && FD_ISSET(socket_, &read_fds);
    }

    void configureSocket() {
        int opt = 1;

        if (options_.keep_alive) {
            setsockopt(socket_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
        }

        if (options_.no_delay) {
            setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        }

        int recv_size = static_cast<int>(options_.receive_buffer_size);
        int send_size = static_cast<int>(options_.send_buffer_size);

        setsockopt(socket_, SOL_SOCKET, SO_RCVBUF, &recv_size,
                   sizeof(recv_size));
        setsockopt(socket_, SOL_SOCKET, SO_SNDBUF, &send_size,
                   sizeof(send_size));
    }

    Options options_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> receiving_stopped_{false};
    std::mutex mutex_;  // Mutex needed for certain critical sections
    std::jthread receiving_thread_;  // For asynchronous receiving
    int socket_ = -1;
    int epoll_fd_ = -1;
    int kqueue_fd_ = -1;

    // Callbacks
    std::function<void()> onConnectedCallback_;
    std::function<void()> onDisconnectedCallback_;
    std::function<void(std::span<const char>)> onDataReceivedCallback_;
    std::function<void(const std::system_error&)> onErrorCallback_;

    mutable std::system_error last_error_{std::error_code{}, ""};
};

// TcpClient Class Implementation

TcpClient::TcpClient(Options options)
    : impl_(std::make_unique<Impl>(options)) {}

TcpClient::~TcpClient() = default;

type::expected<void, std::system_error> TcpClient::connect(
    std::string_view host, uint16_t port, std::chrono::milliseconds timeout) {
    auto result = impl_->connect(host, port, timeout);
    if (result.has_value() && onConnectedCallback_) {
        onConnectedCallback_();
    }
    return result;
}

Task<type::expected<void, std::system_error>> TcpClient::connect_async(
    std::string_view host, uint16_t port, std::chrono::milliseconds timeout) {
    auto result = co_await impl_->connect_async(host, port, timeout);
    if (result.has_value() && onConnectedCallback_) {
        onConnectedCallback_();
    }
    co_return result;
}

void TcpClient::disconnect() {
    impl_->disconnect();
    if (onDisconnectedCallback_) {
        onDisconnectedCallback_();
    }
}

type::expected<size_t, std::system_error> TcpClient::send(
    std::span<const char> data) {
    return impl_->send(data);
}

Task<type::expected<size_t, std::system_error>> TcpClient::send_async(
    std::span<const char> data) {
    co_return co_await impl_->send_async(data);
}

type::expected<std::vector<char>, std::system_error> TcpClient::receive(
    size_t max_size, std::chrono::milliseconds timeout) {
    return impl_->receive(max_size, timeout);
}

Task<type::expected<std::vector<char>, std::system_error>>
TcpClient::receive_async(size_t max_size, std::chrono::milliseconds timeout) {
    co_return co_await impl_->receive_async(max_size, timeout);
}

bool TcpClient::isConnected() const { return impl_->isConnected(); }

void TcpClient::startReceiving(size_t buffer_size) {
    impl_->setOnConnectedCallback(onConnectedCallback_);
    impl_->setOnDisconnectedCallback(onDisconnectedCallback_);
    impl_->setOnDataReceivedCallback(onDataReceivedCallback_);
    impl_->setOnErrorCallback(onErrorCallback_);
    impl_->startReceiving(buffer_size);
}

void TcpClient::stopReceiving() { impl_->stopReceiving(); }

const std::system_error& TcpClient::getLastError() const {
    return impl_->getLastError();
}

}  // namespace atom::connection
