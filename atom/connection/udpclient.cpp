/*
 * udpclient.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-5-24

Description: UDP Client Class

*************************************************/

#include "udpclient.hpp"

#include <atomic>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#include <mstcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {
// Constants
constexpr uint16_t MIN_PORT = 1024;
constexpr uint16_t MAX_PORT = 65535;
constexpr size_t MAX_BUFFER_SIZE = 65536;

// Utility functions
bool isValidPort(uint16_t port) { return port >= MIN_PORT && port <= MAX_PORT; }

bool setSocketNonBlocking(int socket) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1)
        return false;
    return fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

std::string getLastErrorMsg() {
#ifdef _WIN32
    char* s = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, WSAGetLastError(),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&s, 0,
                   NULL);
    std::string error(s);
    LocalFree(s);
    return error;
#else
    return std::strerror(errno);
#endif
}
}  // namespace

namespace atom::connection {

class UdpClient::Impl {
public:
    Impl() {
        try {
#ifdef _WIN32
            WSADATA wsaData;
            int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (result != 0) {
                throw std::runtime_error("WSAStartup failed: " +
                                         std::to_string(result));
            }
#endif

            socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (socket_ < 0) {
                throw std::runtime_error("Socket creation failed: " +
                                         getLastErrorMsg());
            }

            // Set socket to non-blocking mode
            if (!setSocketNonBlocking(socket_)) {
                throw std::runtime_error(
                    "Failed to set socket to non-blocking mode");
            }

#ifdef __linux__
            epoll_fd_ = epoll_create1(0);
            if (epoll_fd_ == -1) {
                throw std::runtime_error("Epoll creation failed: " +
                                         getLastErrorMsg());
            }
#endif
        } catch (const std::exception& e) {
            cleanup();
            throw;
        }
    }

    ~Impl() { cleanup(); }

    void cleanup() {
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
#endif

#ifdef _WIN32
        WSACleanup();
#endif
    }

    Impl(Impl&& other) noexcept
        : socket_(std::exchange(other.socket_, -1)),
#ifdef __linux__
          epoll_fd_(std::exchange(other.epoll_fd_, -1)),
#endif
          bound_(other.bound_.load()),
          receivingStopped_(other.receivingStopped_.load()),
          isReceiving_(other.isReceiving_.load()) {
        // Move the thread if it's running
        receivingThread_ = std::move(other.receivingThread_);
    }

    UdpResult<bool> bind(uint16_t port) noexcept {
        try {
            if (!isValidPort(port) &&
                port != 0) {  // Allow port 0 for system-assigned port
                return type::unexpected(UdpError::InvalidParameter);
            }

            struct sockaddr_in address {};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = INADDR_ANY;
            address.sin_port = htons(port);

            // Set SO_REUSEADDR to prevent "address already in use" errors
            int reuseAddr = 1;
            if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR,
                           reinterpret_cast<char*>(&reuseAddr),
                           sizeof(reuseAddr)) < 0) {
                return type::unexpected(UdpError::BindFailed);
            }

            if (::bind(socket_, reinterpret_cast<struct sockaddr*>(&address),
                       sizeof(address)) < 0) {
                return type::unexpected(UdpError::BindFailed);
            }

            bound_ = true;
            return true;
        } catch (...) {
            return type::unexpected(UdpError::InternalError);
        }
    }

    UdpResult<size_t> send(const RemoteEndpoint& endpoint,
                           std::span<const char> data) noexcept {
        try {
            if (data.empty() || data.size() > MAX_BUFFER_SIZE) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            if (!isValidPort(endpoint.port)) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            struct addrinfo hints {};
            struct addrinfo* result = nullptr;

            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;

            // Use getaddrinfo instead of gethostbyname (which is deprecated)
            int status = getaddrinfo(endpoint.host.c_str(),
                                     std::to_string(endpoint.port).c_str(),
                                     &hints, &result);
            if (status != 0) {
                return type::unexpected(UdpError::HostNotFound);
            }

            std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> resultGuard(
                result, freeaddrinfo);

            ssize_t bytesSent =
                sendto(socket_, data.data(), data.size(), 0,
                       resultGuard->ai_addr, resultGuard->ai_addrlen);

            if (bytesSent < 0) {
                return type::unexpected(UdpError::SendFailed);
            }

            return static_cast<size_t>(bytesSent);
        } catch (...) {
            return type::unexpected(UdpError::InternalError);
        }
    }

    UdpResult<std::pair<std::vector<char>, RemoteEndpoint>> receive(
        size_t maxSize, std::chrono::milliseconds timeout) noexcept {
        try {
            if (maxSize == 0 || maxSize > MAX_BUFFER_SIZE) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            bool hasTimeout = timeout > std::chrono::milliseconds::zero();

            if (hasTimeout) {
#ifdef _WIN32
                // Set receive timeout on Windows
                DWORD timeout_ms = static_cast<DWORD>(timeout.count());
                if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO,
                               reinterpret_cast<const char*>(&timeout_ms),
                               sizeof(timeout_ms)) != 0) {
                    return type::unexpected(UdpError::ReceiveFailed);
                }
#else
                // Use epoll for timeout on Linux/Unix
                struct epoll_event event {};
                event.events = EPOLLIN;
                event.data.fd = socket_;

                if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, socket_, &event) ==
                    -1) {
                    return type::unexpected(UdpError::ReceiveFailed);
                }

                struct epoll_event events[1];
                int nfds = epoll_wait(epoll_fd_, events, 1,
                                      static_cast<int>(timeout.count()));

                if (nfds == 0) {
                    return type::unexpected(UdpError::Timeout);
                } else if (nfds == -1) {
                    return type::unexpected(UdpError::ReceiveFailed);
                }

                // Clean up after epoll
                epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, socket_, nullptr);
#endif
            }

            std::vector<char> data(maxSize);
            struct sockaddr_in clientAddress {};
            socklen_t clientAddressLength = sizeof(clientAddress);

            ssize_t bytesRead =
                recvfrom(socket_, data.data(), maxSize, 0,
                         reinterpret_cast<struct sockaddr*>(&clientAddress),
                         &clientAddressLength);

            if (bytesRead < 0) {
#ifdef _WIN32
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK || error == WSAETIMEDOUT) {
                    return type::unexpected(UdpError::Timeout);
                }
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return type::unexpected(UdpError::Timeout);
                }
#endif
                return type::unexpected(UdpError::ReceiveFailed);
            }

            data.resize(bytesRead);

            RemoteEndpoint remote;
            remote.host = inet_ntoa(clientAddress.sin_addr);
            remote.port = ntohs(clientAddress.sin_port);

            return std::make_pair(std::move(data), std::move(remote));
        } catch (...) {
            return type::unexpected(UdpError::InternalError);
        }
    }

    UdpResult<bool> startReceiving(
        size_t bufferSize,
        const std::function<void(std::span<const char>, const RemoteEndpoint&)>&
            onDataCallback,
        const std::function<void(UdpError, const std::string&)>&
            onErrorCallback) noexcept {
        try {
            if (bufferSize == 0 || bufferSize > MAX_BUFFER_SIZE) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            if (!onDataCallback) {
                return type::unexpected(UdpError::InvalidParameter);
            }

            {
                std::lock_guard<std::mutex> lock(receivingMutex_);
                if (isReceiving_) {
                    stopReceiving();
                }

                receivingStopped_ = false;
                isReceiving_ = true;

                receivingThread_ =
                    std::jthread([this, bufferSize, onDataCallback,
                                  onErrorCallback](std::stop_token stopToken) {
                        receivingLoop(bufferSize, onDataCallback,
                                      onErrorCallback, stopToken);
                    });
            }

            return true;
        } catch (...) {
            return type::unexpected(UdpError::InternalError);
        }
    }

    void stopReceiving() noexcept {
        std::lock_guard<std::mutex> lock(receivingMutex_);
        if (isReceiving_) {
            receivingStopped_ = true;

            if (receivingThread_.joinable()) {
                receivingThread_.request_stop();
                receivingThread_.join();
            }

            isReceiving_ = false;
        }
    }

    bool isReceiving() const noexcept { return isReceiving_.load(); }

private:
    void receivingLoop(
        size_t bufferSize,
        const std::function<void(std::span<const char>, const RemoteEndpoint&)>&
            onDataCallback,
        const std::function<void(UdpError, const std::string&)>&
            onErrorCallback,
        std::stop_token stopToken) {
        std::vector<char> buffer(bufferSize);

        while (!receivingStopped_ && !stopToken.stop_requested()) {
            struct sockaddr_in clientAddress {};
            socklen_t clientAddressLength = sizeof(clientAddress);

            ssize_t bytesRead =
                recvfrom(socket_, buffer.data(), buffer.size(), 0,
                         reinterpret_cast<struct sockaddr*>(&clientAddress),
                         &clientAddressLength);

            if (bytesRead > 0) {
                try {
                    RemoteEndpoint remote;
                    remote.host = inet_ntoa(clientAddress.sin_addr);
                    remote.port = ntohs(clientAddress.sin_port);

                    onDataCallback(
                        std::span<const char>{buffer.data(),
                                              static_cast<size_t>(bytesRead)},
                        remote);
                } catch (const std::exception& e) {
                    if (onErrorCallback) {
                        onErrorCallback(UdpError::InternalError,
                                        "Exception in data callback: " +
                                            std::string(e.what()));
                    }
                }
            } else if (bytesRead < 0) {
#ifdef _WIN32
                int error = WSAGetLastError();
                if (error != WSAEWOULDBLOCK && error != WSAETIMEDOUT &&
                    onErrorCallback) {
                    onErrorCallback(UdpError::ReceiveFailed,
                                    "Receive error: " + getLastErrorMsg());
                }
#else
                if (errno != EAGAIN && errno != EWOULDBLOCK &&
                    onErrorCallback) {
                    onErrorCallback(UdpError::ReceiveFailed,
                                    "Receive error: " + getLastErrorMsg());
                }
#endif
            }

            // Small sleep to avoid busy-waiting and high CPU usage
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

#ifdef _WIN32
    SOCKET socket_ = INVALID_SOCKET;
#else
    int socket_ = -1;
    int epoll_fd_ = -1;
#endif
    std::atomic<bool> bound_ = false;
    std::jthread receivingThread_;
    std::atomic<bool> receivingStopped_ = false;
    std::atomic<bool> isReceiving_ = false;
    std::mutex receivingMutex_;
};

// UdpClient implementation
UdpClient::UdpClient() : impl_(std::make_unique<Impl>()) {}

UdpClient::UdpClient(uint16_t port) : impl_(std::make_unique<Impl>()) {
    auto result = bind(port);
    if (!result) {
        throw std::runtime_error("Failed to bind UDP socket to port " +
                                 std::to_string(port));
    }
}

UdpClient::~UdpClient() = default;

UdpClient::UdpClient(UdpClient&&) noexcept = default;
UdpClient& UdpClient::operator=(UdpClient&&) noexcept = default;

UdpResult<bool> UdpClient::bind(uint16_t port) noexcept {
    return impl_->bind(port);
}

UdpResult<size_t> UdpClient::send(const RemoteEndpoint& endpoint,
                                  std::span<const char> data) noexcept {
    return impl_->send(endpoint, data);
}

UdpResult<size_t> UdpClient::send(const RemoteEndpoint& endpoint,
                                  const std::string& data) noexcept {
    return impl_->send(endpoint,
                       std::span<const char>(data.data(), data.size()));
}

UdpResult<std::pair<std::vector<char>, RemoteEndpoint>> UdpClient::receive(
    size_t maxSize, std::chrono::milliseconds timeout) noexcept {
    return impl_->receive(maxSize, timeout);
}

void UdpClient::ReceiveAwaitable::await_suspend(std::coroutine_handle<> h) {
    // Execute the receive operation asynchronously
    std::thread([this, h]() {
        result_ = client.receive(maxSize, timeout);
        h.resume();
    }).detach();
}

UdpResult<std::pair<std::vector<char>, RemoteEndpoint>>
UdpClient::ReceiveAwaitable::await_resume() {
    return result_;
}

UdpResult<bool> UdpClient::startReceiving(size_t bufferSize) noexcept {
    return impl_->startReceiving(
        bufferSize,
        [this](std::span<const char> data, const RemoteEndpoint& endpoint) {
            if (onDataReceivedCallback_) {
                onDataReceivedCallback_(data, endpoint);
            }
        },
        [this](UdpError error, const std::string& message) {
            if (onErrorCallback_) {
                onErrorCallback_(error, message);
            }
        });
}

void UdpClient::stopReceiving() noexcept { impl_->stopReceiving(); }

bool UdpClient::isReceiving() const noexcept { return impl_->isReceiving(); }

}  // namespace atom::connection