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
#ifdef _WIN32
            WSADATA wsaData;
            int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (result != 0) {
                throw std::runtime_error("WSAStartup failed with error: " +
                                         std::to_string(result));
            }
#endif
            // Create socket based on IPv4/IPv6 preference
            socket_ = socket(options.ipv6_enabled ? AF_INET6 : AF_INET,
                             SOCK_STREAM, IPPROTO_TCP);
            if (socket_ < 0) {
                throw createSystemError("Socket creation failed");
            }

            // Configure socket options
            configureSocket();

#if defined(__linux__)
            // Create epoll for async I/O on Linux
            epoll_fd_ = epoll_create1(0);
            if (epoll_fd_ == -1) {
                throw createSystemError(
                    "Failed to create epoll file descriptor");
            }
#elif defined(__APPLE__)
            // Create kqueue for async I/O on macOS
            kqueue_fd_ = kqueue();
            if (kqueue_fd_ == -1) {
                throw createSystemError(
                    "Failed to create kqueue file descriptor");
            }
#endif
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
        try {
            if (port == 0) {
                return type::unexpected(std::system_error(
                    std::make_error_code(std::errc::invalid_argument),
                    "Invalid port number"));
            }

            // Resolve hostname
            struct addrinfo hints = {};
            struct addrinfo* result = nullptr;

            hints.ai_family = options_.ipv6_enabled ? AF_UNSPEC : AF_INET;
            hints.ai_socktype = SOCK_STREAM;

            int status =
                getaddrinfo(std::string(host).c_str(),
                            std::to_string(port).c_str(), &hints, &result);
            if (status != 0) {
                return type::unexpected(std::system_error(
                    std::make_error_code(std::errc::host_unreachable),
                    "Failed to resolve hostname: " +
                        std::string(gai_strerror(status))));
            }

            // Smart pointer for automatic cleanup
            struct AddrInfoGuard {
                addrinfo* info;
                ~AddrInfoGuard() {
                    if (info)
                        freeaddrinfo(info);
                }
            } addrGuard{result};

            // Try to connect to each address
            for (struct addrinfo* rp = result; rp != nullptr;
                 rp = rp->ai_next) {
                // Configure socket timeout
                if (timeout > std::chrono::milliseconds::zero()) {
                    setSocketTimeout(timeout);
                }

                // Make socket non-blocking for timeout support
                if (!setNonBlocking(socket_, true)) {
                    continue;  // Try next address
                }

                // Attempt connection
                status = ::connect(socket_, rp->ai_addr, rp->ai_addrlen);

#ifdef _WIN32
                if (status == SOCKET_ERROR &&
                    WSAGetLastError() != WSAEWOULDBLOCK) {
                    continue;  // Try next address
                }
#else
                if (status < 0 && errno != EINPROGRESS) {
                    continue;  // Try next address
                }
#endif

                // Wait for the connection to complete or timeout
                if (!waitForConnectComplete(timeout)) {
                    continue;  // Try next address
                }

                // Verify connection success
                int error = 0;
                socklen_t len = sizeof(error);
                if (getsockopt(socket_, SOL_SOCKET, SO_ERROR,
#ifdef _WIN32
                               reinterpret_cast<char*>(&error),
#else
                               &error,
#endif
                               &len) < 0 ||
                    error != 0) {
                    continue;  // Try next address
                }

                // Restore blocking mode
                setNonBlocking(socket_, false);

                // Connection successful
                connected_ = true;

#if defined(__linux__)
                // Add socket to epoll
                struct epoll_event event = {};
                event.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP;
                event.data.fd = socket_;
                if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, socket_, &event) ==
                    -1) {
                    return type::unexpected(
                        createSystemError("Failed to add socket to epoll"));
                }
#elif defined(__APPLE__)
                // Add socket to kqueue
                struct kevent event;
                EV_SET(&event, socket_, EVFILT_READ, EV_ADD, 0, 0, nullptr);
                if (kevent(kqueue_fd_, &event, 1, nullptr, 0, nullptr) == -1) {
                    return type::unexpected(
                        createSystemError("Failed to add socket to kqueue"));
                }
#endif

                // Invoke connection callback
                if (onConnectedCallback_) {
                    onConnectedCallback_();
                }

                return {};  // Success
            }

            // If we got here, all connection attempts failed
            return type::unexpected(std::system_error(
                std::make_error_code(std::errc::connection_refused),
                "Failed to connect to any resolved address"));
        } catch (const std::exception& e) {
            auto error = std::system_error(
                std::make_error_code(std::errc::io_error),
                "Connection failed: " + std::string(e.what()));
            last_error_ = error;
            return type::unexpected(error);
        }
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

#ifdef _WIN32
            closesocket(socket_);
#else
            close(socket_);
#endif
            connected_ = false;

            // Recreate socket for reuse
            socket_ = socket(options_.ipv6_enabled ? AF_INET6 : AF_INET,
                             SOCK_STREAM, IPPROTO_TCP);
            if (socket_ >= 0) {
                configureSocket();
            }

            // Invoke disconnection callback
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
            return 0;  // Nothing to send
        }

        try {
            // Handle large data by sending in chunks
            size_t total_sent = 0;
            size_t remaining = data.size();

            while (remaining > 0) {
                // Calculate chunk size (limited by SO_SNDBUF)
                size_t chunk_size =
                    std::min(remaining, options_.send_buffer_size);

                ssize_t bytes_sent =
                    ::send(socket_, data.data() + total_sent, chunk_size,
#ifdef _WIN32
                           0
#else
                           MSG_NOSIGNAL  // Prevent SIGPIPE
#endif
                    );

                if (bytes_sent < 0) {
#ifdef _WIN32
                    if (WSAGetLastError() == WSAEWOULDBLOCK) {
                        // Wait until socket is writable
                        if (!waitForSendReady(std::chrono::seconds(5))) {
                            auto error =
                                createSystemError("Send operation timed out");
                            last_error_ = error;
                            return type::unexpected(error);
                        }
                        continue;  // Retry send
                    }
#else
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // Wait until socket is writable
                        if (!waitForSendReady(std::chrono::seconds(5))) {
                            auto error =
                                createSystemError("Send operation timed out");
                            last_error_ = error;
                            return type::unexpected(error);
                        }
                        continue;  // Retry send
                    }
#endif

                    auto error = createSystemError("Send failed");
                    last_error_ = error;
                    return type::unexpected(error);
                }

                total_sent += bytes_sent;
                remaining -= bytes_sent;
            }

            return total_sent;
        } catch (const std::exception& e) {
            auto error = std::system_error(
                std::make_error_code(std::errc::io_error),
                "Send operation failed: " + std::string(e.what()));
            last_error_ = error;
            return type::unexpected(error);
        }
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
            return std::vector<char>{};  // Requested zero bytes
        }

        try {
            // Apply timeout if specified
            if (timeout > std::chrono::milliseconds::zero()) {
                setSocketTimeout(timeout);
            }

            // Wait until data is available or timeout
            if (!waitForReceiveReady(timeout)) {
                auto error = std::system_error(
                    std::make_error_code(std::errc::timed_out),
                    "Receive operation timed out");
                last_error_ = error;
                return type::unexpected(error);
            }

            // Create buffer limited by max_size and receive buffer size
            size_t buffer_size =
                std::min(max_size, options_.receive_buffer_size);
            std::vector<char> buffer(buffer_size);

            // Perform the receive
            ssize_t bytes_read = ::recv(socket_, buffer.data(), buffer_size, 0);

            if (bytes_read < 0) {
                auto error = createSystemError("Receive failed");
                last_error_ = error;
                return type::unexpected(error);
            } else if (bytes_read == 0) {
                // Connection closed by peer
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

            // Resize buffer to actual bytes read
            buffer.resize(bytes_read);
            return buffer;

        } catch (const std::exception& e) {
            auto error = std::system_error(
                std::make_error_code(std::errc::io_error),
                "Receive operation failed: " + std::string(e.what()));
            last_error_ = error;
            return type::unexpected(error);
        }
    }

    Task<type::expected<std::vector<char>, std::system_error>> receive_async(
        size_t max_size, std::chrono::milliseconds timeout) {
        auto result = receive(max_size, timeout);
        co_return result;
    }

    [[nodiscard]] bool isConnected() const { return connected_; }

    void setOnConnectedCallback(const std::function<void()>& callback) {
        onConnectedCallback_ = callback;
    }

    void setOnDisconnectedCallback(const std::function<void()>& callback) {
        onDisconnectedCallback_ = callback;
    }

    void setOnDataReceivedCallback(
        const std::function<void(std::span<const char>)>& callback) {
        onDataReceivedCallback_ = callback;
    }

    void setOnErrorCallback(
        const std::function<void(const std::system_error&)>& callback) {
        onErrorCallback_ = callback;
    }

    void startReceiving(size_t buffer_size) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!connected_) {
            return;
        }

        stopReceiving();

        // Use at least the minimum buffer size
        size_t actual_buffer_size =
            std::max(buffer_size, options_.receive_buffer_size);
        receiving_stopped_.store(false);

        // Launch the receiving thread
        receiving_thread_ = std::jthread(
            [this, actual_buffer_size](std::stop_token stop_token) {
                receiveLoop(actual_buffer_size, stop_token);
            });
    }

    void stopReceiving() {
        receiving_stopped_.store(true);

        if (receiving_thread_.joinable()) {
            receiving_thread_.request_stop();
            receiving_thread_.join();
        }
    }

    [[nodiscard]] const std::system_error& getLastError() const {
        return last_error_;
    }

private:
    void configureSocket() {
        // Set socket options
        int opt = 1;

        // TCP keep-alive
        if (options_.keep_alive) {
            setsockopt(socket_, SOL_SOCKET, SO_KEEPALIVE,
#ifdef _WIN32
                       reinterpret_cast<const char*>(&opt),
#else
                       &opt,
#endif
                       sizeof(opt));
        }

        // Disable Nagle's algorithm (TCP_NODELAY)
        if (options_.no_delay) {
            setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY,
#ifdef _WIN32
                       reinterpret_cast<const char*>(&opt),
#else
                       &opt,
#endif
                       sizeof(opt));
        }

        // Configure send and receive buffer sizes
        int recv_size = static_cast<int>(options_.receive_buffer_size);
        int send_size = static_cast<int>(options_.send_buffer_size);

        setsockopt(socket_, SOL_SOCKET, SO_RCVBUF,
#ifdef _WIN32
                   reinterpret_cast<const char*>(&recv_size),
#else
                   &recv_size,
#endif
                   sizeof(recv_size));

        setsockopt(socket_, SOL_SOCKET, SO_SNDBUF,
#ifdef _WIN32
                   reinterpret_cast<const char*>(&send_size),
#else
                   &send_size,
#endif
                   sizeof(send_size));
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
        tv.tv_sec = timeout.count() / 1000;
        tv.tv_usec = (timeout.count() % 1000) * 1000;
        setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
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

    void receiveLoop(size_t buffer_size, const std::stop_token& stop_token) {
        std::vector<char> buffer(buffer_size);

        while (!receiving_stopped_.load() && !stop_token.stop_requested()) {
            try {
#if defined(__linux__)
                // Use epoll for efficient I/O waiting on Linux
                struct epoll_event events[10];
                int num_events = epoll_wait(epoll_fd_, events, 10, 100);

                if (num_events < 0) {
                    if (errno == EINTR)
                        continue;  // Interrupted
                    throw createSystemError("epoll_wait failed");
                }

                bool has_data = false;
                for (int i = 0; i < num_events; i++) {
                    if (events[i].events & EPOLLIN) {
                        has_data = true;
                        break;
                    }

                    if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                        // Socket error or hangup
                        connected_ = false;
                        if (onDisconnectedCallback_) {
                            onDisconnectedCallback_();
                        }
                        return;
                    }
                }

                if (!has_data) {
                    continue;  // No data available
                }

#elif defined(__APPLE__)
                // Use kqueue for efficient I/O waiting on macOS
                struct kevent events[10];
                struct timespec timeout = {0, 100000000};  // 100ms

                int num_events =
                    kevent(kqueue_fd_, nullptr, 0, events, 10, &timeout);

                if (num_events < 0) {
                    if (errno == EINTR)
                        continue;  // Interrupted
                    throw createSystemError("kevent failed");
                }

                bool has_data = false;
                for (int i = 0; i < num_events; i++) {
                    if (events[i].filter == EVFILT_READ) {
                        has_data = true;
                        break;
                    }
                }

                if (!has_data) {
                    continue;  // No data available
                }

#else
                // Use select for other platforms
                if (!waitForReceiveReady(std::chrono::milliseconds(100))) {
                    continue;  // No data or timeout
                }
#endif

                // Lock for the recv operation
                std::unique_lock<std::mutex> lock(mutex_);

                if (!connected_) {
                    break;
                }

                ssize_t bytes_read =
                    ::recv(socket_, buffer.data(), buffer.size(), 0);

                if (bytes_read < 0) {
#ifdef _WIN32
                    if (WSAGetLastError() == WSAEWOULDBLOCK) {
                        continue;  // No data available
                    }
#else
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;  // No data available
                    }
#endif
                    throw createSystemError(
                        "Receive failed in background thread");
                } else if (bytes_read == 0) {
                    // Connection closed
                    connected_ = false;
                    lock.unlock();  // Unlock before callback

                    if (onDisconnectedCallback_) {
                        onDisconnectedCallback_();
                    }
                    break;
                }

                // Create a data view of valid size
                std::span<const char> data_view(buffer.data(), bytes_read);
                lock.unlock();  // Unlock before callback

                if (onDataReceivedCallback_) {
                    onDataReceivedCallback_(data_view);
                }

            } catch (const std::system_error& e) {
                last_error_ = e;
                if (onErrorCallback_) {
                    onErrorCallback_(e);
                }

                // If the error is fatal, break the loop
                if (e.code().value() != EINTR) {
                    break;
                }
            } catch (const std::exception& e) {
                auto error = std::system_error(
                    std::make_error_code(std::errc::io_error),
                    "Receive thread error: " + std::string(e.what()));
                last_error_ = error;

                if (onErrorCallback_) {
                    onErrorCallback_(error);
                }
                break;
            }
        }
    }

    void cleanupResources() {
        stopReceiving();

        if (socket_ >= 0) {
#ifdef _WIN32
            closesocket(socket_);
#else
            close(socket_);
#endif
            socket_ = -1;
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
        WSACleanup();
#endif
    }

    // Socket and connection state
#ifdef _WIN32
    SOCKET socket_ = INVALID_SOCKET;
#else
    int socket_ = -1;
#endif

#ifdef __linux__
    int epoll_fd_ = -1;
#elif defined(__APPLE__)
    int kqueue_fd_ = -1;
#endif

    // Flags and options
    Options options_;
    std::atomic<bool> connected_{false};

    // Threading support
    std::mutex mutex_;
    std::jthread receiving_thread_;
    std::atomic<bool> receiving_stopped_{false};

    // Callbacks
    std::function<void()> onConnectedCallback_;
    std::function<void()> onDisconnectedCallback_;
    std::function<void(std::span<const char>)> onDataReceivedCallback_;
    std::function<void(const std::system_error&)> onErrorCallback_;

    // Error tracking
    std::system_error last_error_{std::error_code(), ""};
};

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