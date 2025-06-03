#include "udpserver.hpp"

#include <algorithm>
#include <atomic>
#include <format>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#endif

#include "spdlog/spdlog.h"

namespace atom::connection {

namespace {

#ifdef _WIN32
using SocketType = SOCKET;
constexpr SocketType INVALID_SOCKET_VALUE = INVALID_SOCKET;
#else
using SocketType = int;
constexpr SocketType INVALID_SOCKET_VALUE = -1;
#endif

constexpr size_t DEFAULT_BUFFER_SIZE = 4096;
constexpr std::uint16_t MIN_PORT = 1024;
constexpr std::uint16_t MAX_PORT = 65535;

/**
 * @brief Validates an IP address string
 */
[[nodiscard]] bool isValidIpAddress(std::string_view ip) noexcept {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip.data(), &(sa.sin_addr)) == 1;
}

/**
 * @brief Validates a port number
 */
[[nodiscard]] bool isValidPort(std::uint16_t port) noexcept {
    return port >= MIN_PORT && port <= MAX_PORT;
}

/**
 * @brief Sets a socket to non-blocking mode
 */
[[nodiscard]] bool setNonBlocking(SocketType socket) noexcept {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    return fcntl(socket, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
}

/**
 * @brief Gets the last system error message
 */
[[nodiscard]] std::string getLastErrorMessage() noexcept {
#ifdef _WIN32
    DWORD errorCode = WSAGetLastError();
    char* errorMsg = nullptr;

    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                   nullptr, errorCode, 0, reinterpret_cast<char*>(&errorMsg), 0,
                   nullptr);

    std::string message = errorMsg ? errorMsg : "Unknown error";
    if (errorMsg) {
        LocalFree(errorMsg);
    }
    return std::format("Error code {}: {}", errorCode, message);
#else
    return std::string(strerror(errno));
#endif
}

}  // namespace

class UdpSocketHub::Impl {
public:
    Impl()
        : running_(false),
          socket_(INVALID_SOCKET_VALUE),
          bufferSize_(DEFAULT_BUFFER_SIZE) {}

    ~Impl() { stop(); }

    type::expected<void, UdpError> start(std::uint16_t port) noexcept {
        if (running_.load(std::memory_order_acquire)) {
            spdlog::debug("UDP server already running on port {}", port);
            return {};
        }

        if (!isValidPort(port)) {
            spdlog::error("Invalid port number: {}", port);
            return type::unexpected(UdpError::InvalidPort);
        }

        try {
            if (!initNetworking()) {
                spdlog::error("Failed to initialize networking");
                return type::unexpected(UdpError::NetworkInitFailed);
            }

            socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (socket_ == INVALID_SOCKET_VALUE) {
                spdlog::error("Failed to create UDP socket: {}",
                              getLastErrorMessage());
                cleanupNetworking();
                return type::unexpected(UdpError::SocketCreationFailed);
            }

            if (!setNonBlocking(socket_)) {
                spdlog::warn("Could not set socket to non-blocking mode");
            }

            sockaddr_in serverAddr{};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(port);
            serverAddr.sin_addr.s_addr = INADDR_ANY;

            if (bind(socket_, reinterpret_cast<sockaddr*>(&serverAddr),
                     sizeof(serverAddr)) < 0) {
                spdlog::error("Failed to bind socket to port {}: {}", port,
                              getLastErrorMessage());
                closeSocket();
                cleanupNetworking();
                return type::unexpected(UdpError::BindFailed);
            }

            running_.store(true, std::memory_order_release);
            receiverThread_ = std::jthread(
                [this](std::stop_token stoken) { receiveMessages(stoken); });

            spdlog::info("UDP server started successfully on port {}", port);
            return {};
        } catch (const std::exception& e) {
            spdlog::error("Exception during UDP server startup: {}", e.what());
            stop();
            return type::unexpected(UdpError::BindFailed);
        }
    }

    void stop() noexcept {
        if (!running_.load(std::memory_order_acquire)) {
            return;
        }

        spdlog::info("Stopping UDP server");
        running_.store(false, std::memory_order_release);

        if (receiverThread_.joinable()) {
            receiverThread_.request_stop();
            receiverThread_.join();
        }

        closeSocket();
        cleanupNetworking();

        std::lock_guard lock(handlersMutex_);
        handlers_.clear();

        spdlog::info("UDP server stopped successfully");
    }

    [[nodiscard]] bool isRunning() const noexcept {
        return running_.load(std::memory_order_acquire);
    }

    void addMessageHandler(MessageHandler handler) {
        std::lock_guard lock(handlersMutex_);
        handlers_.push_back(std::move(handler));
        spdlog::debug("Added message handler, total handlers: {}",
                      handlers_.size());
    }

    void removeMessageHandler(const MessageHandler& handler) {
        std::lock_guard lock(handlersMutex_);

        const auto& targetType = handler.target_type();
        handlers_.erase(std::remove_if(handlers_.begin(), handlers_.end(),
                                       [&targetType](const MessageHandler& h) {
                                           return h.target_type() == targetType;
                                       }),
                        handlers_.end());

        spdlog::debug("Removed message handler, remaining handlers: {}",
                      handlers_.size());
    }

    type::expected<void, UdpError> sendTo(std::string_view message,
                                          std::string_view ip,
                                          std::uint16_t port) noexcept {
        if (!running_.load(std::memory_order_acquire)) {
            spdlog::error("Cannot send message - UDP server is not running");
            return type::unexpected(UdpError::NotRunning);
        }

        if (!isValidIpAddress(ip)) {
            spdlog::error("Invalid IP address: {}", ip);
            return type::unexpected(UdpError::InvalidAddress);
        }

        if (!isValidPort(port)) {
            spdlog::error("Invalid port number: {}", port);
            return type::unexpected(UdpError::InvalidPort);
        }

        try {
            sockaddr_in targetAddr{};
            targetAddr.sin_family = AF_INET;
            targetAddr.sin_port = htons(port);

            if (inet_pton(AF_INET, ip.data(), &targetAddr.sin_addr) != 1) {
                spdlog::error("Failed to convert IP address: {}", ip);
                return type::unexpected(UdpError::InvalidAddress);
            }

            const auto sendResult = sendto(
                socket_, message.data(), message.size(), 0,
                reinterpret_cast<sockaddr*>(&targetAddr), sizeof(targetAddr));

            if (sendResult < 0) {
                spdlog::error("Failed to send message to {}:{}: {}", ip, port,
                              getLastErrorMessage());
                return type::unexpected(UdpError::SendFailed);
            }

            if (static_cast<size_t>(sendResult) < message.size()) {
                spdlog::warn("Partial message sent to {}:{}: {} of {} bytes",
                             ip, port, static_cast<size_t>(sendResult),
                             message.size());
            } else {
                spdlog::debug("Successfully sent {} bytes to {}:{}",
                              static_cast<size_t>(sendResult), ip, port);
            }

            return {};
        } catch (const std::exception& e) {
            spdlog::error("Exception during sendTo: {}", e.what());
            return type::unexpected(UdpError::SendFailed);
        }
    }

    void setBufferSize(std::size_t size) noexcept {
        if (size > 0) {
            bufferSize_ = size;
            spdlog::info("UDP receive buffer size set to {} bytes", size);
        } else {
            spdlog::warn("Invalid buffer size {}, keeping current size {}",
                         size, bufferSize_);
        }
    }

private:
    [[nodiscard]] bool initNetworking() noexcept {
#ifdef _WIN32
        WSADATA wsaData;
        const int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            spdlog::error("WSAStartup failed with error: {}", result);
            return false;
        }
        return true;
#else
        return true;
#endif
    }

    void cleanupNetworking() noexcept {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void closeSocket() noexcept {
        if (socket_ != INVALID_SOCKET_VALUE) {
#ifdef _WIN32
            closesocket(socket_);
#else
            close(socket_);
#endif
            socket_ = INVALID_SOCKET_VALUE;
        }
    }

    void receiveMessages(std::stop_token stoken) {
        std::vector<char> buffer(bufferSize_);
        spdlog::debug("Message receiver thread started with buffer size {}",
                      bufferSize_);

        while (!stoken.stop_requested() &&
               running_.load(std::memory_order_acquire)) {
            try {
                sockaddr_in clientAddr{};
                socklen_t clientAddrSize = sizeof(clientAddr);

                const auto bytesReceived = recvfrom(
                    socket_, buffer.data(), buffer.size(), 0,
                    reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrSize);

                if (bytesReceived < 0) {
#ifdef _WIN32
                    const auto lastError = WSAGetLastError();
                    if (lastError == WSAEWOULDBLOCK) {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(1));
                        continue;
                    }
#else
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(1));
                        continue;
                    }
#endif
                    if (running_.load(std::memory_order_acquire)) {
                        spdlog::error("recvfrom failed: {}",
                                      getLastErrorMessage());
                    }
                    continue;
                }

                if (bytesReceived == 0) {
                    continue;
                }

                char clientIpBuffer[INET_ADDRSTRLEN];
                const char* ipResult =
                    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIpBuffer,
                              INET_ADDRSTRLEN);

                if (!ipResult) {
                    spdlog::error("Failed to convert client IP address");
                    continue;
                }

                std::string clientIp(clientIpBuffer);
                int clientPort = ntohs(clientAddr.sin_port);
                std::string message(buffer.data(), bytesReceived);

                spdlog::debug("Received {} bytes from {}:{}", bytesReceived,
                              clientIp, clientPort);

                std::vector<MessageHandler> currentHandlers;
                {
                    std::lock_guard lock(handlersMutex_);
                    currentHandlers = handlers_;
                }

                for (const auto& handler : currentHandlers) {
                    try {
                        handler(message, clientIp, clientPort);
                    } catch (const std::exception& e) {
                        spdlog::error("Exception in message handler: {}",
                                      e.what());
                    }
                }
            } catch (const std::exception& e) {
                spdlog::error("Exception in message receiver: {}", e.what());
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        spdlog::debug("Message receiver thread stopped");
    }

    std::atomic<bool> running_;
    SocketType socket_;
    std::jthread receiverThread_;
    std::vector<MessageHandler> handlers_;
    std::mutex handlersMutex_;
    std::size_t bufferSize_;
};

UdpSocketHub::UdpSocketHub() : impl_(std::make_unique<Impl>()) {}

UdpSocketHub::~UdpSocketHub() = default;

type::expected<void, UdpError> UdpSocketHub::start(
    std::uint16_t port) noexcept {
    return impl_->start(port);
}

void UdpSocketHub::stop() noexcept { impl_->stop(); }

bool UdpSocketHub::isRunning() const noexcept { return impl_->isRunning(); }

void UdpSocketHub::addMessageHandlerImpl(MessageHandler handler) {
    impl_->addMessageHandler(std::move(handler));
}

void UdpSocketHub::removeMessageHandlerImpl(MessageHandler handler) {
    impl_->removeMessageHandler(handler);
}

type::expected<void, UdpError> UdpSocketHub::sendTo(
    std::string_view message, std::string_view ip,
    std::uint16_t port) noexcept {
    return impl_->sendTo(message, ip, port);
}

void UdpSocketHub::setBufferSize(std::size_t size) noexcept {
    impl_->setBufferSize(size);
}

}  // namespace atom::connection