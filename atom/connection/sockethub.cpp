#include "sockethub.hpp"

#include <chrono>
#include <cstring>
#include <format>
#include <map>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "atom/log/loguru.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
using socket_t = SOCKET;
const socket_t INVALID_SOCKVAL = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
const socket_t INVALID_SOCKVAL = -1;
#endif

namespace atom::connection {

// Custom exceptions
class SocketException : public std::runtime_error {
public:
    explicit SocketException(const std::string& msg)
        : std::runtime_error(msg) {}
};

// Buffer pool for efficient memory management
class BufferPool {
public:
    explicit BufferPool(size_t bufferSize, size_t initialPoolSize = 16)
        : bufferSize_(bufferSize) {
        for (size_t i = 0; i < initialPoolSize; ++i) {
            buffers_.push(std::make_unique<std::vector<char>>(bufferSize));
        }
    }

    std::unique_ptr<std::vector<char>> acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (buffers_.empty()) {
            return std::make_unique<std::vector<char>>(bufferSize_);
        }
        auto buffer = std::move(buffers_.front());
        buffers_.pop();
        return buffer;
    }

    void release(std::unique_ptr<std::vector<char>> buffer) {
        if (!buffer)
            return;

        std::lock_guard<std::mutex> lock(mutex_);
        // Only keep a reasonable number of buffers in the pool
        if (buffers_.size() < maxPoolSize_) {
            buffer->clear();
            buffers_.push(std::move(buffer));
        }
        // If we exceed max pool size, the buffer will be destroyed
    }

private:
    size_t bufferSize_;
    std::queue<std::unique_ptr<std::vector<char>>> buffers_;
    std::mutex mutex_;
    const size_t maxPoolSize_ = 100;  // Prevent unbounded growth
};

// Client connection class
class ClientConnection {
public:
    ClientConnection(socket_t socket, const std::string& address, int id)
        : socket_(socket),
          address_(address),
          id_(id),
          connected_(true),
          lastActivity_(std::chrono::steady_clock::now()),
          bytesReceived_(0),
          bytesSent_(0) {}

    ~ClientConnection() { disconnect(); }

    [[nodiscard]] bool isConnected() const noexcept {
        return connected_.load();
    }

    [[nodiscard]] socket_t getSocket() const noexcept { return socket_; }

    [[nodiscard]] const std::string& getAddress() const noexcept {
        return address_;
    }

    [[nodiscard]] int getId() const noexcept { return id_; }

    [[nodiscard]] std::chrono::steady_clock::time_point getLastActivity()
        const noexcept {
        return lastActivity_.load();
    }

    [[nodiscard]] uint64_t getBytesReceived() const noexcept {
        return bytesReceived_.load();
    }

    [[nodiscard]] uint64_t getBytesSent() const noexcept {
        return bytesSent_.load();
    }

    [[nodiscard]] std::chrono::steady_clock::time_point getConnectedTime()
        const noexcept {
        return connectedTime_;
    }

    void updateActivity() noexcept {
        lastActivity_.store(std::chrono::steady_clock::now());
    }

    bool send(std::string_view message) {
        if (!isConnected())
            return false;

        try {
            std::lock_guard<std::mutex> lock(writeMutex_);
            int bytesSent = ::send(socket_, message.data(), message.size(), 0);
            if (bytesSent <= 0) {
                LOG_F(ERROR, "Failed to send message to client {}", id_);
                return false;
            }

            bytesSent_ += bytesSent;
            updateActivity();
            return true;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception sending message to client {}: {}", id_,
                  e.what());
            return false;
        }
    }

    void recordReceivedData(size_t bytes) {
        bytesReceived_ += bytes;
        updateActivity();
    }

    void disconnect() {
        if (!connected_.exchange(false))
            return;  // Already disconnected

        std::lock_guard<std::mutex> lock(writeMutex_);
#ifdef _WIN32
        closesocket(socket_);
#else
        close(socket_);
#endif
        LOG_F(INFO, "Client disconnected: {} (ID: {})", address_, id_);
    }

private:
    socket_t socket_;
    std::string address_;
    int id_;
    std::atomic<bool> connected_;
    std::atomic<std::chrono::steady_clock::time_point> lastActivity_;
    std::atomic<uint64_t> bytesReceived_;
    std::atomic<uint64_t> bytesSent_;
    std::chrono::steady_clock::time_point connectedTime_ =
        std::chrono::steady_clock::now();
    std::mutex writeMutex_;
};

class SocketHubImpl {
public:
    SocketHubImpl()
        : running_(false),
          serverSocket_(INVALID_SOCKVAL),
          nextClientId_(1),
          clientTimeout_(std::chrono::seconds(60)),
          bufferPool_(std::make_unique<BufferPool>(bufferSize_))
#ifdef __linux__
          ,
          epoll_fd_(INVALID_SOCKVAL)
#endif
    {
    }

    ~SocketHubImpl() noexcept {
        try {
            stop();
        } catch (...) {
            // Ensure no exceptions escape destructor
            LOG_F(ERROR, "Exception caught in SocketHubImpl destructor");
        }
    }

    // Prevent copying
    SocketHubImpl(const SocketHubImpl&) = delete;
    SocketHubImpl& operator=(const SocketHubImpl&) = delete;

    void start(int port) {
        // Validate port number
        if (port <= 0 || port > 65535) {
            throw std::invalid_argument(
                std::format("Invalid port number: {}", port));
        }

        if (running_.load()) {
            LOG_F(WARNING, "SocketHub is already running.");
            return;
        }

        try {
            if (!initWinsock()) {
                throw SocketException("Failed to initialize socket library");
            }

            serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
            if (serverSocket_ == INVALID_SOCKVAL) {
                throw SocketException("Failed to create server socket");
            }

            // Set socket to non-blocking mode
#ifdef _WIN32
            u_long mode = 1;
            if (ioctlsocket(serverSocket_, FIONBIO, &mode) != 0) {
                throw SocketException("Failed to set non-blocking mode");
            }
#else
            int flags = fcntl(serverSocket_, F_GETFL, 0);
            if (flags == -1 ||
                fcntl(serverSocket_, F_SETFL, flags | O_NONBLOCK) == -1) {
                throw SocketException("Failed to set non-blocking mode");
            }
#endif

            // Enable address reuse
            int opt = 1;
            if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR,
                           reinterpret_cast<const char*>(&opt),
                           sizeof(opt)) < 0) {
                throw SocketException("Failed to set socket options");
            }

            // Set TCP_NODELAY to disable Nagle's algorithm
            if (setsockopt(serverSocket_, IPPROTO_TCP, TCP_NODELAY,
                           reinterpret_cast<const char*>(&opt),
                           sizeof(opt)) < 0) {
                LOG_F(WARNING, "Failed to set TCP_NODELAY");
            }

            sockaddr_in serverAddress{};
            serverAddress.sin_family = AF_INET;
            serverAddress.sin_addr.s_addr = INADDR_ANY;
            serverAddress.sin_port = htons(static_cast<uint16_t>(port));

#ifdef _WIN32
            if (bind(serverSocket_, reinterpret_cast<sockaddr*>(&serverAddress),
                     sizeof(serverAddress)) == SOCKET_ERROR)
#else
            if (bind(serverSocket_, reinterpret_cast<sockaddr*>(&serverAddress),
                     sizeof(serverAddress)) < 0)
#endif
            {
                throw SocketException(std::format(
                    "Failed to bind server socket: {}", strerror(errno)));
            }

#ifdef _WIN32
            if (listen(serverSocket_, maxConnections_) == SOCKET_ERROR)
#else
            if (listen(serverSocket_, maxConnections_) < 0)
#endif
            {
                throw SocketException(std::format(
                    "Failed to listen on server socket: {}", strerror(errno)));
            }

#ifdef __linux__
            epoll_fd_ = epoll_create1(0);
            if (epoll_fd_ == -1) {
                throw SocketException("Failed to create epoll file descriptor");
            }

            struct epoll_event event;
            event.events =
                EPOLLIN | EPOLLET;  // Edge-triggered for better performance
            event.data.fd = serverSocket_;
            if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, serverSocket_, &event) ==
                -1) {
                throw SocketException("Failed to add server socket to epoll");
            }
#endif

            serverPort_ = port;
            running_.store(true);
            DLOG_F(INFO, "SocketHub started on port {}", port);

            // Start the accept thread with exception handling
            acceptThread_ = std::jthread([this](std::stop_token stoken) {
                try {
                    acceptConnections(stoken);
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Exception in accept thread: {}", e.what());
                    running_.store(false);
                }
            });

            // Start timeout checker thread
            timeoutThread_ = std::jthread([this](std::stop_token stoken) {
                try {
                    checkClientTimeouts(stoken);
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Exception in timeout thread: {}", e.what());
                }
            });
        } catch (const std::exception& e) {
            cleanupResources();
            LOG_F(ERROR, "Failed to start SocketHub: {}", e.what());
            throw;
        }
    }

    void stop() noexcept {
        if (!running_.exchange(false)) {
            return;  // Already stopped
        }

        try {
            LOG_F(INFO, "Stopping SocketHub...");

            if (acceptThread_.joinable()) {
                acceptThread_.request_stop();
            }

            if (timeoutThread_.joinable()) {
                timeoutThread_.request_stop();
            }

            cleanupResources();

            if (acceptThread_.joinable()) {
                acceptThread_.join();
            }

            if (timeoutThread_.joinable()) {
                timeoutThread_.join();
            }

            DLOG_F(INFO, "SocketHub stopped successfully.");
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error during SocketHub shutdown: {}", e.what());
        }
    }

    void addMessageHandler(std::function<void(std::string_view)> handler) {
        if (!handler) {
            throw std::invalid_argument(
                "Invalid message handler (null function)");
        }
        std::lock_guard<std::mutex> lock(handlerMutex_);
        messageHandler_ = std::move(handler);
    }

    void addConnectHandler(std::function<void(int, std::string_view)> handler) {
        if (!handler) {
            throw std::invalid_argument(
                "Invalid connect handler (null function)");
        }
        std::lock_guard<std::mutex> lock(handlerMutex_);
        connectHandler_ = std::move(handler);
    }

    void addDisconnectHandler(
        std::function<void(int, std::string_view)> handler) {
        if (!handler) {
            throw std::invalid_argument(
                "Invalid disconnect handler (null function)");
        }
        std::lock_guard<std::mutex> lock(handlerMutex_);
        disconnectHandler_ = std::move(handler);
    }

    size_t broadcast(std::string_view message) {
        if (message.empty() || !running_.load()) {
            return 0;
        }

        std::shared_lock<std::shared_mutex> lock(clientsMutex_);
        size_t successCount = 0;

        for (const auto& [_, client] : clients_) {
            if (client && client->isConnected() && client->send(message)) {
                successCount++;
            }
        }

        return successCount;
    }

    bool sendTo(int clientId, std::string_view message) {
        if (message.empty() || !running_.load()) {
            return false;
        }

        std::shared_lock<std::shared_mutex> lock(clientsMutex_);

        auto it = clients_.find(clientId);
        if (it != clients_.end() && it->second && it->second->isConnected()) {
            return it->second->send(message);
        }

        return false;
    }

    std::vector<ClientInfo> getConnectedClients() const {
        std::shared_lock<std::shared_mutex> lock(clientsMutex_);
        std::vector<ClientInfo> result;
        result.reserve(clients_.size());

        for (const auto& [id, client] : clients_) {
            if (client && client->isConnected()) {
                ClientInfo info;
                info.id = client->getId();
                info.address = client->getAddress();
                info.connectedTime = client->getConnectedTime();
                info.bytesReceived = client->getBytesReceived();
                info.bytesSent = client->getBytesSent();
                result.push_back(info);
            }
        }

        return result;
    }

    size_t getClientCount() const noexcept {
        std::shared_lock<std::shared_mutex> lock(clientsMutex_);
        size_t count = 0;

        for (const auto& [_, client] : clients_) {
            if (client && client->isConnected()) {
                count++;
            }
        }

        return count;
    }

    void setClientTimeout(std::chrono::seconds timeout) {
        if (timeout.count() > 0) {
            clientTimeout_ = timeout;
            LOG_F(INFO, "Client timeout set to {} seconds", timeout.count());
        } else {
            LOG_F(WARNING, "Invalid timeout value");
        }
    }

    [[nodiscard]] bool isRunning() const noexcept {
        return running_.load(std::memory_order_acquire);
    }

    [[nodiscard]] int getPort() const noexcept { return serverPort_; }

private:
    static constexpr int maxConnections_ =
        128;                                  // Maximum concurrent connections
    static constexpr int bufferSize_ = 8192;  // Receive buffer size

    std::atomic<bool> running_{false};
    socket_t serverSocket_{INVALID_SOCKVAL};
    int serverPort_{0};
    std::jthread acceptThread_;
    std::jthread timeoutThread_;
    std::atomic<int> nextClientId_{1};
    std::chrono::seconds clientTimeout_;
    std::unique_ptr<BufferPool> bufferPool_;

#ifdef __linux__
    int epoll_fd_{INVALID_SOCKVAL};
#endif

    // Client management
    std::map<int, std::shared_ptr<ClientConnection>> clients_;
    mutable std::shared_mutex clientsMutex_;

    // Event handlers
    std::function<void(std::string_view)> messageHandler_;
    std::function<void(int, std::string_view)> connectHandler_;
    std::function<void(int, std::string_view)> disconnectHandler_;
    std::mutex handlerMutex_;

    bool initWinsock() {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            LOG_F(ERROR, "Failed to initialize Winsock.");
            return false;
        }
#endif
        return true;
    }

    void cleanupWinsock() noexcept {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void closeSocket(socket_t socket) noexcept {
        try {
#ifdef _WIN32
            closesocket(socket);
#else
            close(socket);
#endif
        } catch (...) {
            LOG_F(ERROR, "Exception caught while closing socket");
        }
    }

    void acceptConnections(std::stop_token stoken) {
#ifdef __linux__
        std::vector<epoll_event> events(maxConnections_);

        while (!stoken.stop_requested() && running_.load()) {
            int numEvents =
                epoll_wait(epoll_fd_, events.data(), events.size(), 100);

            if (numEvents < 0) {
                if (errno == EINTR) {
                    continue;  // Interrupted, try again
                }
                LOG_F(ERROR, "epoll_wait failed: {}", strerror(errno));
                break;
            }

            for (int i = 0; i < numEvents; i++) {
                // New connection on server socket
                if (events[i].data.fd == serverSocket_) {
                    acceptNewConnections();
                    continue;
                }

                socket_t clientSocket = events[i].data.fd;

                // Find client by socket
                std::shared_ptr<ClientConnection> client;
                {
                    std::shared_lock<std::shared_mutex> lock(clientsMutex_);
                    for (const auto& [_, c] : clients_) {
                        if (c && c->getSocket() == clientSocket) {
                            client = c;
                            break;
                        }
                    }
                }

                if (!client) {
                    // Client not found, remove from epoll
                    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, clientSocket, nullptr);
                    continue;
                }

                if (events[i].events & EPOLLIN) {
                    handleClientData(client);
                }

                if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                    client->disconnect();
                    disconnectClient(client->getId());
                }
            }
        }
#else
        // Windows/other select-based event loop
        fd_set readfds;
        timeval timeout{0, 100000};  // 100ms timeout

        while (!stoken.stop_requested() && running_.load()) {
            FD_ZERO(&readfds);
            FD_SET(serverSocket_, &readfds);

            // Add client sockets to select set
            socket_t maxSocket = serverSocket_;
            std::vector<std::shared_ptr<ClientConnection>> activeClients;
            {
                std::shared_lock<std::shared_mutex> lock(clientsMutex_);
                activeClients.reserve(clients_.size());
                for (const auto& [_, client] : clients_) {
                    if (client && client->isConnected()) {
                        socket_t sock = client->getSocket();
                        FD_SET(sock, &readfds);
                        activeClients.push_back(client);
                        if (sock > maxSocket) {
                            maxSocket = sock;
                        }
                    }
                }
            }

            // Wait for activity
            int activity =
                select(maxSocket + 1, &readfds, nullptr, nullptr, &timeout);

            if (activity < 0) {
                if (errno == EINTR) {
                    continue;  // Interrupted, try again
                }
                LOG_F(ERROR, "select failed: {}", strerror(errno));
                break;
            }

            // New connection on server socket
            if (FD_ISSET(serverSocket_, &readfds)) {
                acceptNewConnections();
            }

            // Check client sockets for activity
            for (const auto& client : activeClients) {
                if (client && client->isConnected()) {
                    socket_t sock = client->getSocket();
                    if (FD_ISSET(sock, &readfds)) {
                        handleClientData(client);
                    }
                }
            }
        }
#endif
    }

    void acceptNewConnections() {
        // Accept multiple connections at once for better performance
        for (int i = 0; i < 16 && running_.load(); ++i) {
            sockaddr_in clientAddress{};
            socklen_t clientAddressLength = sizeof(clientAddress);

            socket_t clientSocket = accept(
                serverSocket_, reinterpret_cast<sockaddr*>(&clientAddress),
                &clientAddressLength);

            if (clientSocket == INVALID_SOCKVAL) {
#ifdef _WIN32
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK)
                    break;  // No more connections waiting
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;  // No more connections waiting
#endif
                if (running_.load()) {
                    LOG_F(ERROR, "Failed to accept client connection");
                }
                break;
            }

            // Set client socket to non-blocking mode
#ifdef _WIN32
            u_long mode = 1;
            if (ioctlsocket(clientSocket, FIONBIO, &mode) != 0) {
                LOG_F(ERROR,
                      "Failed to set client socket to non-blocking mode");
                closeSocket(clientSocket);
                continue;
            }
#else
            int flags = fcntl(clientSocket, F_GETFL, 0);
            if (flags == -1 ||
                fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK) == -1) {
                LOG_F(ERROR,
                      "Failed to set client socket to non-blocking mode");
                closeSocket(clientSocket);
                continue;
            }
#endif

            // Set TCP_NODELAY to disable Nagle's algorithm
            int opt = 1;
            if (setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY,
                           reinterpret_cast<const char*>(&opt),
                           sizeof(opt)) < 0) {
                LOG_F(WARNING, "Failed to set TCP_NODELAY on client socket");
            }

            // Get client IP address
            char clientIp[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clientAddress.sin_addr), clientIp,
                      INET_ADDRSTRLEN);
            std::string clientAddrStr =
                std::format("{}:{}", clientIp, ntohs(clientAddress.sin_port));
            int clientId = nextClientId_++;

            // Check if max connections reached
            {
                std::shared_lock<std::shared_mutex> lock(clientsMutex_);
                size_t activeClients = 0;
                for (const auto& [_, client] : clients_) {
                    if (client && client->isConnected()) {
                        activeClients++;
                    }
                }

                if (activeClients >= maxConnections_) {
                    LOG_F(WARNING,
                          "Maximum connections reached, rejecting client");
                    closeSocket(clientSocket);
                    continue;
                }
            }

            LOG_F(INFO, "New client connected: {} (ID: {})", clientAddrStr,
                  clientId);

            // Create client connection
            auto client = std::make_shared<ClientConnection>(
                clientSocket, clientAddrStr, clientId);

#ifdef __linux__
            // Add to epoll
            epoll_event event;
            event.events = EPOLLIN | EPOLLET;
            event.data.fd = clientSocket;
            if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, clientSocket, &event) ==
                -1) {
                LOG_F(ERROR, "Failed to add client socket to epoll");
                continue;
            }
#endif

            // Add to clients map
            {
                std::unique_lock<std::shared_mutex> lock(clientsMutex_);
                clients_[clientId] = client;
            }

            // Notify connect handler
            {
                std::lock_guard<std::mutex> lock(handlerMutex_);
                if (connectHandler_) {
                    try {
                        connectHandler_(clientId, clientAddrStr);
                    } catch (const std::exception& e) {
                        LOG_F(ERROR, "Exception in connect handler: {}",
                              e.what());
                    }
                }
            }
        }
    }

    void handleClientData(std::shared_ptr<ClientConnection> client) {
        if (!client || !client->isConnected())
            return;

        auto buffer = bufferPool_->acquire();
        socket_t sock = client->getSocket();

        int bytesRead = recv(sock, buffer->data(), buffer->size(), 0);

        if (bytesRead > 0) {
            // Update client stats
            client->recordReceivedData(bytesRead);

            // Process message
            std::string_view message(buffer->data(), bytesRead);
            std::lock_guard<std::mutex> lock(handlerMutex_);
            if (messageHandler_) {
                try {
                    messageHandler_(message);
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Exception in message handler: {}", e.what());
                }
            }
        } else if (bytesRead == 0) {
            // Client closed connection
            client->disconnect();
            disconnectClient(client->getId());
        } else {
            // Check if error is non-blocking related
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                LOG_F(ERROR, "Error receiving data from client: {}", error);
                client->disconnect();
                disconnectClient(client->getId());
            }
#else
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                LOG_F(ERROR, "Error receiving data from client: {}",
                      strerror(errno));
                client->disconnect();
                disconnectClient(client->getId());
            }
#endif
        }

        bufferPool_->release(std::move(buffer));
    }

    void disconnectClient(int clientId) {
        std::string clientAddr;

        // Capture client address before removal
        {
            std::shared_lock<std::shared_mutex> lock(clientsMutex_);
            auto it = clients_.find(clientId);
            if (it != clients_.end() && it->second) {
                clientAddr = it->second->getAddress();
            }
        }

        // Remove client
        {
            std::unique_lock<std::shared_mutex> lock(clientsMutex_);
            clients_.erase(clientId);
        }

        // Notify handler
        if (!clientAddr.empty()) {
            std::lock_guard<std::mutex> lock(handlerMutex_);
            if (disconnectHandler_) {
                try {
                    disconnectHandler_(clientId, clientAddr);
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Exception in disconnect handler: {}",
                          e.what());
                }
            }
        }
    }

    void checkClientTimeouts(std::stop_token stoken) {
        while (!stoken.stop_requested() && running_.load()) {
            // Check every second
            std::this_thread::sleep_for(std::chrono::seconds(1));

            auto now = std::chrono::steady_clock::now();
            std::vector<std::shared_ptr<ClientConnection>> timeoutClients;

            // Identify timed out clients
            {
                std::shared_lock<std::shared_mutex> lock(clientsMutex_);
                for (const auto& [_, client] : clients_) {
                    if (client && client->isConnected()) {
                        auto lastActivity = client->getLastActivity();
                        if (now - lastActivity > clientTimeout_) {
                            timeoutClients.push_back(client);
                        }
                    }
                }
            }

            // Disconnect timed out clients
            for (auto& client : timeoutClients) {
                LOG_F(INFO, "Client timed out: {} (ID: {})",
                      client->getAddress(), client->getId());
                client->disconnect();
                disconnectClient(client->getId());
            }
        }
    }

    void cleanupResources() noexcept {
        try {
            // Delete all clients
            {
                std::unique_lock<std::shared_mutex> lock(clientsMutex_);
                clients_.clear();
            }

#ifdef __linux__
            if (epoll_fd_ != INVALID_SOCKVAL) {
                close(epoll_fd_);
                epoll_fd_ = INVALID_SOCKVAL;
            }
#endif

            if (serverSocket_ != INVALID_SOCKVAL) {
                closeSocket(serverSocket_);
                serverSocket_ = INVALID_SOCKVAL;
            }

            cleanupWinsock();
            serverPort_ = 0;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception during resource cleanup: {}", e.what());
        }
    }
};

// SocketHub implementation

SocketHub::SocketHub() : impl_(std::make_unique<SocketHubImpl>()) {}

SocketHub::~SocketHub() noexcept = default;

SocketHub::SocketHub(SocketHub&&) noexcept = default;
SocketHub& SocketHub::operator=(SocketHub&&) noexcept = default;

void SocketHub::start(int port) { impl_->start(port); }

void SocketHub::stop() noexcept { impl_->stop(); }

void SocketHub::addHandlerImpl(std::function<void(std::string_view)> handler) {
    impl_->addMessageHandler(std::move(handler));
}

void SocketHub::addConnectHandlerImpl(
    std::function<void(int, std::string_view)> handler) {
    impl_->addConnectHandler(std::move(handler));
}

void SocketHub::addDisconnectHandlerImpl(
    std::function<void(int, std::string_view)> handler) {
    impl_->addDisconnectHandler(std::move(handler));
}

size_t SocketHub::broadcast(std::string_view message) {
    return impl_->broadcast(message);
}

bool SocketHub::sendTo(int clientId, std::string_view message) {
    return impl_->sendTo(clientId, message);
}

std::vector<ClientInfo> SocketHub::getConnectedClients() const {
    return impl_->getConnectedClients();
}

size_t SocketHub::getClientCount() const noexcept {
    return impl_->getClientCount();
}

bool SocketHub::isRunning() const noexcept { return impl_->isRunning(); }

void SocketHub::setClientTimeout(std::chrono::seconds timeout) {
    impl_->setClientTimeout(timeout);
}

int SocketHub::getPort() const noexcept { return impl_->getPort(); }

}  // namespace atom::connection