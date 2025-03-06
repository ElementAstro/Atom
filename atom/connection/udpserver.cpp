#include "udpserver.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <format>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#endif

#include "atom/log/loguru.hpp"

namespace atom::connection {

namespace {
// Constants for socket operations
constexpr int INVALID_SOCKET_VALUE =
#ifdef _WIN32
    INVALID_SOCKET
#else
    -1
#endif
    ;

constexpr size_t DEFAULT_BUFFER_SIZE = 4096;
constexpr std::uint16_t MIN_PORT = 1024;  // Avoid system ports
constexpr std::uint16_t MAX_PORT = 65535;

/**
 * @brief Validates an IP address string
 * @param ip The IP address to validate
 * @return true if valid, false otherwise
 */
[[nodiscard]] bool isValidIpAddress(std::string_view ip) noexcept {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, ip.data(), &(sa.sin_addr)) == 1;
}

/**
 * @brief Validates a port number
 * @param port The port to validate
 * @return true if valid, false otherwise
 */
[[nodiscard]] bool isValidPort(std::uint16_t port) noexcept {
    return port >= MIN_PORT && port <= MAX_PORT;
}

/**
 * @brief Sets a socket to non-blocking mode
 * @param socket The socket to modify
 * @return true if successful, false otherwise
 */
[[nodiscard]] bool setNonBlocking(int socket) noexcept {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1)
        return false;
    return fcntl(socket, F_SETFL, flags | O_NONBLOCK) != -1;
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
            return {};  // Already running, not an error
        }

        if (!isValidPort(port)) {
            LOG_F(ERROR, "Invalid port number: %u", port);
            return type::unexpected(UdpError::InvalidPort);
        }

        try {
            if (!initNetworking()) {
                LOG_F(ERROR, "Networking initialization failed");
                return type::unexpected(UdpError::NetworkInitFailed);
            }

            socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (socket_ == INVALID_SOCKET_VALUE) {
                LOG_F(ERROR, "Failed to create socket: %s",
                      getLastErrorMessage().c_str());
                cleanupNetworking();
                return type::unexpected(UdpError::SocketCreationFailed);
            }

            // Set socket to non-blocking mode
            if (!setNonBlocking(socket_)) {
                LOG_F(WARNING, "Could not set socket to non-blocking mode");
            }

            // Set up socket address
            sockaddr_in serverAddr{};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(port);
            serverAddr.sin_addr.s_addr = INADDR_ANY;

            // Bind socket to address
            if (bind(socket_, reinterpret_cast<sockaddr*>(&serverAddr),
                     sizeof(serverAddr)) < 0) {
                LOG_F(ERROR, "Bind failed: %s", getLastErrorMessage().c_str());
                closeSocket();
                cleanupNetworking();
                return type::unexpected(UdpError::BindFailed);
            }

            // Start the receiver thread
            running_.store(true, std::memory_order_release);
            receiverThread_ = std::jthread(
                [this](std::stop_token stoken) { receiveMessages(stoken); });

            LOG_F(INFO, "UDP socket hub started on port %u", port);
            return {};
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception during UDP socket hub start: %s", e.what());
            stop();
            return type::unexpected(UdpError::BindFailed);
        }
    }

    void stop() noexcept {
        if (!running_.load(std::memory_order_acquire)) {
            return;
        }

        running_.store(false, std::memory_order_release);

        if (receiverThread_.joinable()) {
            receiverThread_.request_stop();
            receiverThread_.join();
        }

        closeSocket();
        cleanupNetworking();
        LOG_F(INFO, "UDP socket hub stopped");
    }

    [[nodiscard]] bool isRunning() const noexcept {
        return running_.load(std::memory_order_acquire);
    }

    void addMessageHandler(MessageHandler handler) {
        std::lock_guard lock(handlersMutex_);
        handlers_.push_back(std::move(handler));
    }

    void removeMessageHandler([[maybe_unused]] MessageHandler handler) {
        std::lock_guard lock(handlersMutex_);

        /*
        TODO: Implement handler removal logic
        auto targetType = handler.target_type();
        auto target =
            handler.target<void(const std::string&, const std::string&, int)>();

        auto it = std::ranges::find_if(handlers_, [&](const auto& h) {
            return h.target_type() == targetType &&
                   h.target<void(const std::string&, const std::string&,
                                 int)>() == target;
        });

        if (it != handlers_.end()) {
            handlers_.erase(it);
        }
        */
    }

    type::expected<void, UdpError> sendTo(std::string_view message,
                                          std::string_view ip,
                                          std::uint16_t port) noexcept {
        if (!running_.load(std::memory_order_acquire)) {
            LOG_F(ERROR, "Cannot send message - server is not running");
            return type::unexpected(UdpError::NotRunning);
        }

        if (!isValidIpAddress(ip)) {
            LOG_F(ERROR, "Invalid IP address: %s", std::string(ip).c_str());
            return type::unexpected(UdpError::InvalidAddress);
        }

        if (!isValidPort(port)) {
            LOG_F(ERROR, "Invalid port number: %u", port);
            return type::unexpected(UdpError::InvalidPort);
        }

        try {
            sockaddr_in targetAddr{};
            targetAddr.sin_family = AF_INET;
            targetAddr.sin_port = htons(port);

            if (inet_pton(AF_INET, ip.data(), &targetAddr.sin_addr) != 1) {
                LOG_F(ERROR, "Failed to convert IP address: %s",
                      std::string(ip).c_str());
                return type::unexpected(UdpError::InvalidAddress);
            }

            const auto sendResult = sendto(
                socket_, message.data(), message.size(), 0,
                reinterpret_cast<sockaddr*>(&targetAddr), sizeof(targetAddr));

            if (sendResult < 0) {
                LOG_F(ERROR, "Failed to send message: %s",
                      getLastErrorMessage().c_str());
                return type::unexpected(UdpError::SendFailed);
            }

            if (static_cast<size_t>(sendResult) < message.size()) {
                LOG_F(WARNING, "Partial message sent: %zu of %zu bytes",
                      static_cast<size_t>(sendResult), message.size());
            }

            return {};
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception during sendTo: %s", e.what());
            return type::unexpected(UdpError::SendFailed);
        }
    }

    void setBufferSize(std::size_t size) noexcept {
        if (size > 0) {
            bufferSize_ = size;
            LOG_F(INFO, "Receive buffer size set to %zu bytes", size);
        }
    }

private:
    [[nodiscard]] bool initNetworking() noexcept {
#ifdef _WIN32
        WSADATA wsaData;
        const int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            LOG_F(ERROR, "WSAStartup failed with error: %d", result);
            return false;
        }
        return true;
#else
        return true;  // On Linux, no initialization needed
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

    [[nodiscard]] std::string getLastErrorMessage() const noexcept {
#ifdef _WIN32
        DWORD errorCode = WSAGetLastError();
        char* errorMsg = nullptr;

        FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            nullptr, errorCode, 0, reinterpret_cast<char*>(&errorMsg), 0,
            nullptr);

        std::string message = errorMsg ? errorMsg : "Unknown error";
        LocalFree(errorMsg);
        return std::format("Error code {}: {}", errorCode, message);
#else
        return std::string(strerror(errno));
#endif
    }

    void receiveMessages(std::stop_token stoken) {
        // Allocate buffer dynamically based on the configured size
        std::vector<char> buffer(bufferSize_);

        while (!stoken.stop_requested() &&
               running_.load(std::memory_order_acquire)) {
            try {
                sockaddr_in clientAddr{};
                socklen_t clientAddrSize = sizeof(clientAddr);

                const auto bytesReceived = recvfrom(
                    socket_, buffer.data(), buffer.size(), 0,
                    reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrSize);

                if (bytesReceived < 0) {
// Check if the error is because of non-blocking operation
#ifdef _WIN32
                    const auto lastError = WSAGetLastError();
                    if (lastError == WSAEWOULDBLOCK) {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(10));
                        continue;
                    }
#else
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(10));
                        continue;
                    }
#endif

                    LOG_F(ERROR, "recvfrom failed: %s",
                          getLastErrorMessage().c_str());
                    continue;
                }

                if (bytesReceived == 0) {
                    continue;  // Empty datagram
                }

                // Create a string view for the message
                std::string_view messageView(buffer.data(), bytesReceived);

                // Convert client address to string
                char clientIpBuffer[INET_ADDRSTRLEN];
                const char* ipResult =
                    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIpBuffer,
                              INET_ADDRSTRLEN);

                if (!ipResult) {
                    LOG_F(ERROR, "Failed to convert client IP address");
                    continue;
                }

                std::string clientIp(clientIpBuffer);
                int clientPort = ntohs(clientAddr.sin_port);

                // Create a full string copy for handlers
                // (we need to ensure the data persists beyond this function
                // call)
                std::string message(messageView);

                // Process the message with registered handlers
                std::vector<MessageHandler> currentHandlers;
                {
                    std::lock_guard lock(handlersMutex_);
                    currentHandlers = handlers_;  // Make a copy to avoid
                                                  // holding lock during calls
                }

                for (const auto& handler : currentHandlers) {
                    try {
                        handler(message, clientIp, clientPort);
                    } catch (const std::exception& e) {
                        LOG_F(ERROR, "Exception in message handler: %s",
                              e.what());
                    }
                }
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Exception in message receiver: %s", e.what());
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    std::atomic<bool> running_;
    int socket_;
    std::jthread receiverThread_;
    std::vector<MessageHandler> handlers_;
    std::mutex handlersMutex_;
    std::size_t bufferSize_;
};

// Implementation of UdpSocketHub methods that delegate to Impl
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
    impl_->removeMessageHandler(std::move(handler));
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