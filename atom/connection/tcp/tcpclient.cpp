/*
 * tcpclient.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-5-24

Description: TCP Client Class with native socket backend

*************************************************/

#include "tcpclient.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <thread>

#include "atom/log/loguru.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
using ssize_t = int;
#endif
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

#ifdef _WIN32
constexpr auto INVALID_SOCKET_VALUE = INVALID_SOCKET;
using SocketType = SOCKET;
#else
constexpr int INVALID_SOCKET_VALUE = -1;
using SocketType = int;
#endif

std::system_error createSocketError(const std::string& message) {
#ifdef _WIN32
    return std::system_error(WSAGetLastError(), std::system_category(),
                             message);
#else
    return std::system_error(errno, std::system_category(), message);
#endif
}

bool setNonBlocking(SocketType socket, bool nonBlocking) {
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

void closeSocket(SocketType socket) {
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

/**
 * @brief Unified socket wait helper - eliminates code duplication
 */
enum class WaitType { Read, Write, Connect };

bool waitForSocket(SocketType socket, WaitType type,
                   std::chrono::milliseconds timeout) {
    fd_set read_fds, write_fds, error_fds;
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_ZERO(&error_fds);

    fd_set* read_ptr = nullptr;
    fd_set* write_ptr = nullptr;

    switch (type) {
        case WaitType::Read:
            FD_SET(socket, &read_fds);
            read_ptr = &read_fds;
            break;
        case WaitType::Write:
            FD_SET(socket, &write_fds);
            write_ptr = &write_fds;
            break;
        case WaitType::Connect:
            FD_SET(socket, &write_fds);
            FD_SET(socket, &error_fds);
            write_ptr = &write_fds;
            break;
    }

    struct timeval tv;
    detail::msToTimeval(timeout, tv.tv_sec, tv.tv_usec);

    int result =
        select(static_cast<int>(socket) + 1, read_ptr, write_ptr,
               type == WaitType::Connect ? &error_fds : nullptr,
               timeout > std::chrono::milliseconds::zero() ? &tv : nullptr);

    if (result <= 0) {
        return false;
    }

    switch (type) {
        case WaitType::Read:
            return FD_ISSET(socket, &read_fds);
        case WaitType::Write:
        case WaitType::Connect:
            return FD_ISSET(socket, &write_fds);
    }

    return false;
}

}  // namespace

class TcpClient::Impl {
public:
    explicit Impl(const Options& options) : options_(options) {
        try {
#ifdef _WIN32
            WSADATA wsaData;
            int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (result != 0) {
                throw std::runtime_error("WSAStartup failed: " +
                                         std::to_string(result));
            }
            wsa_initialized_ = true;
#endif
            createSocket();
            configureSocket();
            initPlatformIO();
        } catch (const std::exception& e) {
            last_error_ = std::system_error(
                std::make_error_code(std::errc::io_error), e.what());
            cleanupResources();
            throw;
        }
    }

    ~Impl() { cleanupResources(); }

    type::expected<void, std::system_error> connect(
        std::string_view host, uint16_t port,
        std::chrono::milliseconds timeout) {
        if (port == 0) {
            return type::unexpected(std::system_error(
                std::make_error_code(std::errc::invalid_argument),
                "Invalid port number"));
        }

        // Resolve hostname
        auto resolve_result = resolveHost(host, port);
        if (!resolve_result) {
            return type::unexpected(resolve_result.error().error());
        }

        auto& endpoints = *resolve_result;

        // Try each resolved address
        for (auto* rp = endpoints.get(); rp != nullptr; rp = rp->ai_next) {
            auto connect_result = tryConnect(rp, timeout);
            if (connect_result) {
                connected_ = true;
                registerWithPlatformIO();

                if (onConnectedCallback_) {
                    onConnectedCallback_();
                }
                return {};
            }
        }

        return type::unexpected(std::system_error(
            std::make_error_code(std::errc::connection_refused),
            "Failed to connect to any resolved address"));
    }

    Task<type::expected<void, std::system_error>> connect_async(
        std::string_view host, uint16_t port,
        std::chrono::milliseconds timeout) {
        auto result = connect(host, port, timeout);
        co_return result;
    }

    void disconnect() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (connected_) {
            stopReceiving();
            closeSocket(socket_);
            connected_ = false;

            // Recreate socket for reuse
            try {
                createSocket();
                configureSocket();
            } catch (...) {
                socket_ = INVALID_SOCKET_VALUE;
            }

            if (onDisconnectedCallback_) {
                onDisconnectedCallback_();
            }
        }
    }

    type::expected<size_t, std::system_error> send(std::span<const char> data) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!connected_) {
            auto error = std::system_error(
                std::make_error_code(std::errc::not_connected),
                "Not connected");
            last_error_ = error;
            return type::unexpected(error);
        }

        if (data.empty()) {
            return 0;
        }

        return sendChunked(data);
    }

    Task<type::expected<size_t, std::system_error>> send_async(
        std::span<const char> data) {
        auto result = send(data);
        co_return result;
    }

    type::expected<std::vector<char>, std::system_error> receive(
        size_t max_size, std::chrono::milliseconds timeout) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!connected_) {
            auto error = std::system_error(
                std::make_error_code(std::errc::not_connected),
                "Not connected");
            last_error_ = error;
            return type::unexpected(error);
        }

        if (max_size == 0) {
            return std::vector<char>{};
        }

        // Apply timeout
        if (timeout > std::chrono::milliseconds::zero()) {
            setSocketTimeout(timeout);
        }

        // Wait for data
        if (!waitForSocket(socket_, WaitType::Read, timeout)) {
            auto error =
                std::system_error(std::make_error_code(std::errc::timed_out),
                                  "Receive timed out");
            last_error_ = error;
            return type::unexpected(error);
        }

        return receiveData(max_size);
    }

    Task<type::expected<std::vector<char>, std::system_error>> receive_async(
        size_t max_size, std::chrono::milliseconds timeout) {
        auto result = receive(max_size, timeout);
        co_return result;
    }

    [[nodiscard]] bool isConnected() const { return connected_; }

    [[nodiscard]] const std::system_error& getLastError() const {
        return last_error_;
    }

    void setCallbacks(const TcpClient::OnConnectedCallback& connected,
                      const TcpClient::OnDisconnectedCallback& disconnected,
                      const TcpClient::OnDataReceivedCallback& dataReceived,
                      const TcpClient::OnErrorCallback& error) {
        onConnectedCallback_ = connected;
        onDisconnectedCallback_ = disconnected;
        onDataReceivedCallback_ = dataReceived;
        onErrorCallback_ = error;
    }

    void startReceiving(size_t buffer_size) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!connected_) {
            return;
        }

        stopReceivingLocked();

        size_t actual_buffer_size =
            std::max(buffer_size, options_.receive_buffer_size);
        receiving_stopped_.store(false);

        receiving_thread_ = std::jthread(
            [this, actual_buffer_size](std::stop_token stop_token) {
                receiveLoop(actual_buffer_size, stop_token);
            });
    }

    void stopReceiving() {
        std::lock_guard<std::mutex> lock(mutex_);
        stopReceivingLocked();
    }

private:
    void createSocket() {
        socket_ = socket(options_.ipv6_enabled ? AF_INET6 : AF_INET,
                         SOCK_STREAM, IPPROTO_TCP);
        if (socket_ == INVALID_SOCKET_VALUE) {
            throw createSocketError("Socket creation failed");
        }
    }

    void configureSocket() {
        int opt = 1;

        if (options_.keep_alive) {
            setsockopt(socket_, SOL_SOCKET, SO_KEEPALIVE,
                       reinterpret_cast<const char*>(&opt), sizeof(opt));
        }

        if (options_.no_delay) {
            setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY,
                       reinterpret_cast<const char*>(&opt), sizeof(opt));
        }

        int recv_size = static_cast<int>(options_.receive_buffer_size);
        int send_size = static_cast<int>(options_.send_buffer_size);

        setsockopt(socket_, SOL_SOCKET, SO_RCVBUF,
                   reinterpret_cast<const char*>(&recv_size),
                   sizeof(recv_size));
        setsockopt(socket_, SOL_SOCKET, SO_SNDBUF,
                   reinterpret_cast<const char*>(&send_size),
                   sizeof(send_size));
    }

    void initPlatformIO() {
#if defined(__linux__)
        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ == -1) {
            throw createSocketError("Failed to create epoll");
        }
#elif defined(__APPLE__)
        kqueue_fd_ = kqueue();
        if (kqueue_fd_ == -1) {
            throw createSocketError("Failed to create kqueue");
        }
#endif
    }

    void registerWithPlatformIO() {
#if defined(__linux__)
        struct epoll_event event = {};
        event.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP;
        event.data.fd = socket_;
        epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, socket_, &event);
#elif defined(__APPLE__)
        struct kevent event;
        EV_SET(&event, socket_, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        kevent(kqueue_fd_, &event, 1, nullptr, 0, nullptr);
#endif
    }

    void setSocketTimeout(std::chrono::milliseconds timeout) {
#ifdef _WIN32
        DWORD tv = static_cast<DWORD>(timeout.count());
        setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&tv), sizeof(tv));
        setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
        struct timeval tv;
        detail::msToTimeval(timeout, tv.tv_sec, tv.tv_usec);
        setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
    }

    struct AddrInfoDeleter {
        void operator()(addrinfo* info) const {
            if (info)
                freeaddrinfo(info);
        }
    };
    using AddrInfoPtr = std::unique_ptr<addrinfo, AddrInfoDeleter>;

    type::expected<AddrInfoPtr, std::system_error> resolveHost(
        std::string_view host, uint16_t port) {
        struct addrinfo hints = {};
        struct addrinfo* result = nullptr;

        hints.ai_family = options_.ipv6_enabled ? AF_UNSPEC : AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        int status = getaddrinfo(std::string(host).c_str(),
                                 std::to_string(port).c_str(), &hints, &result);
        if (status != 0) {
            return type::unexpected(std::system_error(
                std::make_error_code(std::errc::host_unreachable),
                "Failed to resolve: " + std::string(gai_strerror(status))));
        }

        return AddrInfoPtr(result);
    }

    type::expected<void, std::system_error> tryConnect(
        addrinfo* addr, std::chrono::milliseconds timeout) {
        if (!setNonBlocking(socket_, true)) {
            return type::unexpected(
                createSocketError("Failed to set non-blocking"));
        }

        int status = ::connect(socket_, addr->ai_addr, addr->ai_addrlen);

#ifdef _WIN32
        bool in_progress =
            (status == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK);
#else
        bool in_progress = (status < 0 && errno == EINPROGRESS);
#endif

        if (status == 0 || in_progress) {
            if (!waitForSocket(socket_, WaitType::Connect, timeout)) {
                return type::unexpected(std::system_error(
                    std::make_error_code(std::errc::timed_out),
                    "Connection timed out"));
            }

            // Verify connection success
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(socket_, SOL_SOCKET, SO_ERROR,
                           reinterpret_cast<char*>(&error), &len) < 0 ||
                error != 0) {
                return type::unexpected(std::system_error(
                    std::make_error_code(std::errc::connection_refused),
                    "Connection verification failed"));
            }

            setNonBlocking(socket_, false);
            return {};
        }

        return type::unexpected(createSocketError("Connect failed"));
    }

    type::expected<size_t, std::system_error> sendChunked(
        std::span<const char> data) {
        size_t total_sent = 0;
        size_t remaining = data.size();

        while (remaining > 0) {
            size_t chunk_size = std::min(remaining, options_.send_buffer_size);

            ssize_t bytes_sent = ::send(socket_, data.data() + total_sent,
                                        static_cast<int>(chunk_size),
#ifdef _WIN32
                                        0
#else
                                        MSG_NOSIGNAL
#endif
            );

            if (bytes_sent < 0) {
#ifdef _WIN32
                if (WSAGetLastError() == WSAEWOULDBLOCK) {
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
#endif
                    if (!waitForSocket(socket_, WaitType::Write,
                                       std::chrono::seconds(5))) {
                        auto error = createSocketError("Send timed out");
                        last_error_ = error;
                        return type::unexpected(error);
                    }
                    continue;
                }

                auto error = createSocketError("Send failed");
                last_error_ = error;
                return type::unexpected(error);
            }

            total_sent += bytes_sent;
            remaining -= bytes_sent;
        }

        return total_sent;
    }

    type::expected<std::vector<char>, std::system_error> receiveData(
        size_t max_size) {
        size_t buffer_size = std::min(max_size, options_.receive_buffer_size);
        std::vector<char> buffer(buffer_size);

        ssize_t bytes_read =
            ::recv(socket_, buffer.data(), static_cast<int>(buffer_size), 0);

        if (bytes_read < 0) {
            auto error = createSocketError("Receive failed");
            last_error_ = error;
            return type::unexpected(error);
        }

        if (bytes_read == 0) {
            connected_ = false;
            if (onDisconnectedCallback_) {
                onDisconnectedCallback_();
            }
            auto error = std::system_error(
                std::make_error_code(std::errc::connection_reset),
                "Connection closed by peer");
            last_error_ = error;
            return type::unexpected(error);
        }

        buffer.resize(bytes_read);
        return buffer;
    }

    void receiveLoop(size_t buffer_size, const std::stop_token& stop_token) {
        std::vector<char> buffer(buffer_size);

        while (!receiving_stopped_.load() && !stop_token.stop_requested()) {
            try {
                bool has_data = waitForPlatformIO();

                if (!has_data) {
                    continue;
                }

                std::unique_lock<std::mutex> lock(mutex_);
                if (!connected_) {
                    break;
                }

                ssize_t bytes_read = ::recv(socket_, buffer.data(),
                                            static_cast<int>(buffer.size()), 0);

                if (bytes_read < 0) {
#ifdef _WIN32
                    if (WSAGetLastError() == WSAEWOULDBLOCK)
                        continue;
#else
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        continue;
#endif
                    throw createSocketError("Receive failed");
                }

                if (bytes_read == 0) {
                    connected_ = false;
                    lock.unlock();
                    if (onDisconnectedCallback_) {
                        onDisconnectedCallback_();
                    }
                    break;
                }

                std::span<const char> data_view(buffer.data(), bytes_read);
                lock.unlock();

                if (onDataReceivedCallback_) {
                    onDataReceivedCallback_(data_view);
                }

            } catch (const std::system_error& e) {
                last_error_ = e;
                if (onErrorCallback_) {
                    onErrorCallback_(e);
                }
                if (e.code().value() != EINTR) {
                    break;
                }
            }
        }
    }

    bool waitForPlatformIO() {
#if defined(__linux__)
        struct epoll_event events[10];
        int num_events = epoll_wait(epoll_fd_, events, 10, 100);

        if (num_events < 0) {
            if (errno == EINTR)
                return false;
            throw createSocketError("epoll_wait failed");
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].events & EPOLLIN)
                return true;
            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                connected_ = false;
                if (onDisconnectedCallback_) {
                    onDisconnectedCallback_();
                }
                return false;
            }
        }
        return false;

#elif defined(__APPLE__)
        struct kevent events[10];
        struct timespec timeout = {0, 100000000};  // 100ms

        int num_events = kevent(kqueue_fd_, nullptr, 0, events, 10, &timeout);

        if (num_events < 0) {
            if (errno == EINTR)
                return false;
            throw createSocketError("kevent failed");
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].filter == EVFILT_READ)
                return true;
        }
        return false;

#else
    return waitForSocket(socket_, WaitType::Read,
                         std::chrono::milliseconds(100));
#endif
    }

    void stopReceivingLocked() {
        receiving_stopped_.store(true);
        if (receiving_thread_.joinable()) {
            receiving_thread_.request_stop();
            receiving_thread_.join();
        }
    }

    void cleanupResources() {
        stopReceiving();

        if (socket_ != INVALID_SOCKET_VALUE) {
            closeSocket(socket_);
            socket_ = INVALID_SOCKET_VALUE;
        }

#ifdef __linux__
        if (epoll_fd_ >= 0) {
            close(epoll_fd_);
            epoll_fd_ = -1;
        }
#elif defined(__APPLE__)
        if (kqueue_fd_ >= 0) {
            close(kqueue_fd_);
            kqueue_fd_ = -1;
        }
#endif

#ifdef _WIN32
        if (wsa_initialized_) {
            WSACleanup();
            wsa_initialized_ = false;
        }
#endif
    }

    // Socket state
    SocketType socket_ = INVALID_SOCKET_VALUE;
#ifdef _WIN32
    bool wsa_initialized_ = false;
#endif

#ifdef __linux__
    int epoll_fd_ = -1;
#elif defined(__APPLE__)
    int kqueue_fd_ = -1;
#endif

    // Configuration
    Options options_;
    std::atomic<bool> connected_{false};

    // Threading
    std::mutex mutex_;
    std::jthread receiving_thread_;
    std::atomic<bool> receiving_stopped_{false};

    // Callbacks
    TcpClient::OnConnectedCallback onConnectedCallback_;
    TcpClient::OnDisconnectedCallback onDisconnectedCallback_;
    TcpClient::OnDataReceivedCallback onDataReceivedCallback_;
    TcpClient::OnErrorCallback onErrorCallback_;

    // Error tracking
    std::system_error last_error_{std::error_code(), ""};
};

// TcpClient public implementation

TcpClient::TcpClient(Options options)
    : impl_(std::make_unique<Impl>(options)) {}

TcpClient::TcpClient(const TcpClientConfig& config)
    : impl_(std::make_unique<Impl>(Options::fromConfig(config))) {}

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

const std::system_error& TcpClient::getLastError() const {
    return impl_->getLastError();
}

void TcpClient::startReceiving(size_t buffer_size) {
    impl_->setCallbacks(onConnectedCallback_, onDisconnectedCallback_,
                        onDataReceivedCallback_, onErrorCallback_);
    impl_->startReceiving(buffer_size);
}

void TcpClient::stopReceiving() { impl_->stopReceiving(); }

}  // namespace atom::connection
