#include "sockethub.hpp"

#include <chrono>
#include <cstring>
#include <format>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <spdlog/spdlog.h>

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

class SocketException : public std::runtime_error {
public:
    explicit SocketException(const std::string& msg)
        : std::runtime_error(msg) {}
};

class BufferPool {
public:
    explicit BufferPool(size_t bufferSize, size_t initialPoolSize = 32)
        : bufferSize_(bufferSize) {
        buffers_.reserve(initialPoolSize);
        for (size_t i = 0; i < initialPoolSize; ++i) {
            buffers_.emplace_back(
                std::make_unique<std::vector<char>>(bufferSize));
        }
    }

    std::unique_ptr<std::vector<char>> acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (buffers_.empty()) {
            return std::make_unique<std::vector<char>>(bufferSize_);
        }
        auto buffer = std::move(buffers_.back());
        buffers_.pop_back();
        return buffer;
    }

    void release(std::unique_ptr<std::vector<char>> buffer) {
        if (!buffer)
            return;

        std::lock_guard<std::mutex> lock(mutex_);
        if (buffers_.size() < maxPoolSize_) {
            buffer->clear();
            buffers_.emplace_back(std::move(buffer));
        }
    }

private:
    size_t bufferSize_;
    std::vector<std::unique_ptr<std::vector<char>>> buffers_;
    std::mutex mutex_;
    const size_t maxPoolSize_ = 128;
};

class ClientConnection {
public:
    ClientConnection(socket_t socket, std::string address, int id)
        : socket_(socket),
          address_(std::move(address)),
          id_(id),
          connected_(true),
          lastActivity_(std::chrono::steady_clock::now()),
          bytesReceived_(0),
          bytesSent_(0) {}

    ~ClientConnection() { disconnect(); }

    [[nodiscard]] bool isConnected() const noexcept {
        return connected_.load(std::memory_order_acquire);
    }

    [[nodiscard]] socket_t getSocket() const noexcept { return socket_; }
    [[nodiscard]] const std::string& getAddress() const noexcept {
        return address_;
    }
    [[nodiscard]] int getId() const noexcept { return id_; }

    [[nodiscard]] std::chrono::steady_clock::time_point getLastActivity()
        const noexcept {
        return lastActivity_.load(std::memory_order_acquire);
    }

    [[nodiscard]] uint64_t getBytesReceived() const noexcept {
        return bytesReceived_.load(std::memory_order_acquire);
    }

    [[nodiscard]] uint64_t getBytesSent() const noexcept {
        return bytesSent_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::chrono::steady_clock::time_point getConnectedTime()
        const noexcept {
        return connectedTime_;
    }

    void updateActivity() noexcept {
        lastActivity_.store(std::chrono::steady_clock::now(),
                            std::memory_order_release);
    }

    bool send(std::string_view message) {
        if (!isConnected())
            return false;

        std::lock_guard<std::mutex> lock(writeMutex_);
        const int bytesSent = ::send(socket_, message.data(),
                                     static_cast<int>(message.size()), 0);
        if (bytesSent <= 0) {
            spdlog::error("Failed to send message to client {}", id_);
            return false;
        }

        bytesSent_.fetch_add(bytesSent, std::memory_order_relaxed);
        updateActivity();
        return true;
    }

    void recordReceivedData(size_t bytes) {
        bytesReceived_.fetch_add(bytes, std::memory_order_relaxed);
        updateActivity();
    }

    void disconnect() {
        if (!connected_.exchange(false, std::memory_order_acq_rel))
            return;

        std::lock_guard<std::mutex> lock(writeMutex_);
#ifdef _WIN32
        closesocket(socket_);
#else
        close(socket_);
#endif
        spdlog::info("Client disconnected: {} (ID: {})", address_, id_);
    }

private:
    socket_t socket_;
    std::string address_;
    int id_;
    std::atomic<bool> connected_;
    std::atomic<std::chrono::steady_clock::time_point> lastActivity_;
    std::atomic<uint64_t> bytesReceived_;
    std::atomic<uint64_t> bytesSent_;
    const std::chrono::steady_clock::time_point connectedTime_ =
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
            spdlog::error("Exception in SocketHubImpl destructor");
        }
    }

    SocketHubImpl(const SocketHubImpl&) = delete;
    SocketHubImpl& operator=(const SocketHubImpl&) = delete;

    void start(int port) {
        if (port <= 0 || port > 65535) {
            throw std::invalid_argument(std::format("Invalid port: {}", port));
        }

        if (running_.load(std::memory_order_acquire)) {
            spdlog::warn("SocketHub already running");
            return;
        }

        if (!initWinsock()) {
            throw SocketException("Failed to initialize socket library");
        }

        serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket_ == INVALID_SOCKVAL) {
            throw SocketException("Failed to create server socket");
        }

#ifdef _WIN32
        u_long mode = 1;
        if (ioctlsocket(serverSocket_, FIONBIO, &mode) != 0) {
            throw SocketException("Failed to set non-blocking mode");
        }
#else
        const int flags = fcntl(serverSocket_, F_GETFL, 0);
        if (flags == -1 ||
            fcntl(serverSocket_, F_SETFL, flags | O_NONBLOCK) == -1) {
            throw SocketException("Failed to set non-blocking mode");
        }
#endif

        int opt = 1;
        if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR,
                       reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0) {
            throw SocketException("Failed to set SO_REUSEADDR");
        }

        if (setsockopt(serverSocket_, IPPROTO_TCP, TCP_NODELAY,
                       reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0) {
            spdlog::warn("Failed to set TCP_NODELAY");
        }

        sockaddr_in serverAddress{};
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = INADDR_ANY;
        serverAddress.sin_port = htons(static_cast<uint16_t>(port));

        if (bind(serverSocket_, reinterpret_cast<sockaddr*>(&serverAddress),
                 sizeof(serverAddress)) < 0) {
            throw SocketException(std::format("Failed to bind to port {}: {}",
                                              port, strerror(errno)));
        }

        if (listen(serverSocket_, maxConnections_) < 0) {
            throw SocketException(
                std::format("Failed to listen: {}", strerror(errno)));
        }

#ifdef __linux__
        epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd_ == -1) {
            throw SocketException("Failed to create epoll");
        }

        epoll_event event{};
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = serverSocket_;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, serverSocket_, &event) == -1) {
            throw SocketException("Failed to add server socket to epoll");
        }
#endif

        serverPort_ = port;
        running_.store(true, std::memory_order_release);
        spdlog::info("SocketHub started on port {}", port);

        acceptThread_ = std::jthread(
            [this](std::stop_token stoken) { acceptConnections(stoken); });

        timeoutThread_ = std::jthread(
            [this](std::stop_token stoken) { checkClientTimeouts(stoken); });
    }

    void stop() noexcept {
        if (!running_.exchange(false, std::memory_order_acq_rel))
            return;

        spdlog::info("Stopping SocketHub...");

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

        spdlog::info("SocketHub stopped");
    }

    void addMessageHandler(std::function<void(std::string_view)> handler) {
        if (!handler) {
            throw std::invalid_argument("Invalid message handler");
        }
        std::lock_guard<std::mutex> lock(handlerMutex_);
        messageHandler_ = std::move(handler);
    }

    void addConnectHandler(std::function<void(int, std::string_view)> handler) {
        if (!handler) {
            throw std::invalid_argument("Invalid connect handler");
        }
        std::lock_guard<std::mutex> lock(handlerMutex_);
        connectHandler_ = std::move(handler);
    }

    void addDisconnectHandler(
        std::function<void(int, std::string_view)> handler) {
        if (!handler) {
            throw std::invalid_argument("Invalid disconnect handler");
        }
        std::lock_guard<std::mutex> lock(handlerMutex_);
        disconnectHandler_ = std::move(handler);
    }

    size_t broadcast(std::string_view message) {
        if (message.empty() || !running_.load(std::memory_order_acquire)) {
            return 0;
        }

        std::shared_lock<std::shared_mutex> lock(clientsMutex_);
        size_t successCount = 0;

        for (const auto& [_, client] : clients_) {
            if (client && client->isConnected() && client->send(message)) {
                ++successCount;
            }
        }

        return successCount;
    }

    bool sendTo(int clientId, std::string_view message) {
        if (message.empty() || !running_.load(std::memory_order_acquire)) {
            return false;
        }

        std::shared_lock<std::shared_mutex> lock(clientsMutex_);
        const auto it = clients_.find(clientId);
        return it != clients_.end() && it->second &&
               it->second->isConnected() && it->second->send(message);
    }

    std::vector<ClientInfo> getConnectedClients() const {
        std::shared_lock<std::shared_mutex> lock(clientsMutex_);
        std::vector<ClientInfo> result;
        result.reserve(clients_.size());

        for (const auto& [id, client] : clients_) {
            if (client && client->isConnected()) {
                result.emplace_back(
                    ClientInfo{.id = client->getId(),
                               .address = client->getAddress(),
                               .connectedTime = client->getConnectedTime(),
                               .bytesReceived = client->getBytesReceived(),
                               .bytesSent = client->getBytesSent()});
            }
        }

        return result;
    }

    size_t getClientCount() const noexcept {
        std::shared_lock<std::shared_mutex> lock(clientsMutex_);
        return std::count_if(
            clients_.begin(), clients_.end(), [](const auto& pair) {
                return pair.second && pair.second->isConnected();
            });
    }

    void setClientTimeout(std::chrono::seconds timeout) {
        if (timeout.count() > 0) {
            clientTimeout_ = timeout;
            spdlog::info("Client timeout set to {} seconds", timeout.count());
        } else {
            spdlog::warn("Invalid timeout value");
        }
    }

    [[nodiscard]] bool isRunning() const noexcept {
        return running_.load(std::memory_order_acquire);
    }

    [[nodiscard]] int getPort() const noexcept { return serverPort_; }

private:
    static constexpr int maxConnections_ = 1024;
    static constexpr int bufferSize_ = 16384;

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

    std::map<int, std::shared_ptr<ClientConnection>> clients_;
    mutable std::shared_mutex clientsMutex_;

    std::function<void(std::string_view)> messageHandler_;
    std::function<void(int, std::string_view)> connectHandler_;
    std::function<void(int, std::string_view)> disconnectHandler_;
    std::mutex handlerMutex_;

    bool initWinsock() {
#ifdef _WIN32
        WSADATA wsaData;
        return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
#else
        return true;
#endif
    }

    void cleanupWinsock() noexcept {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void closeSocket(socket_t socket) noexcept {
#ifdef _WIN32
        closesocket(socket);
#else
        close(socket);
#endif
    }

    void acceptConnections(std::stop_token stoken) {
#ifdef __linux__
        std::vector<epoll_event> events(maxConnections_);

        while (!stoken.stop_requested() &&
               running_.load(std::memory_order_acquire)) {
            const int numEvents = epoll_wait(
                epoll_fd_, events.data(), static_cast<int>(events.size()), 100);

            if (numEvents < 0) {
                if (errno == EINTR)
                    continue;
                spdlog::error("epoll_wait failed: {}", strerror(errno));
                break;
            }

            for (int i = 0; i < numEvents; ++i) {
                if (events[i].data.fd == serverSocket_) {
                    acceptNewConnections();
                    continue;
                }

                handleClientSocket(events[i]);
            }
        }
#else
        selectEventLoop(stoken);
#endif
    }

#ifdef __linux__
    void handleClientSocket(const epoll_event& event) {
        const socket_t clientSocket = event.data.fd;

        std::shared_ptr<ClientConnection> client;
        {
            std::shared_lock<std::shared_mutex> lock(clientsMutex_);
            const auto it = std::find_if(clients_.begin(), clients_.end(),
                                         [clientSocket](const auto& pair) {
                                             return pair.second &&
                                                    pair.second->getSocket() ==
                                                        clientSocket;
                                         });
            if (it != clients_.end()) {
                client = it->second;
            }
        }

        if (!client) {
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, clientSocket, nullptr);
            return;
        }

        if (event.events & EPOLLIN) {
            handleClientData(client);
        }

        if (event.events & (EPOLLHUP | EPOLLERR)) {
            client->disconnect();
            disconnectClient(client->getId());
        }
    }
#else
    void selectEventLoop(std::stop_token stoken) {
        while (!stoken.stop_requested() &&
               running_.load(std::memory_order_acquire)) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(serverSocket_, &readfds);

            socket_t maxSocket = serverSocket_;
            std::vector<std::shared_ptr<ClientConnection>> activeClients;

            {
                std::shared_lock<std::shared_mutex> lock(clientsMutex_);
                activeClients.reserve(clients_.size());
                for (const auto& [_, client] : clients_) {
                    if (client && client->isConnected()) {
                        const socket_t sock = client->getSocket();
                        FD_SET(sock, &readfds);
                        activeClients.push_back(client);
                        if (sock > maxSocket)
                            maxSocket = sock;
                    }
                }
            }

            timeval timeout{0, 100000};
            const int activity = select(static_cast<int>(maxSocket + 1),
                                        &readfds, nullptr, nullptr, &timeout);

            if (activity < 0) {
                if (errno == EINTR)
                    continue;
                spdlog::error("select failed: {}", strerror(errno));
                break;
            }

            if (FD_ISSET(serverSocket_, &readfds)) {
                acceptNewConnections();
            }

            for (const auto& client : activeClients) {
                if (client && client->isConnected() &&
                    FD_ISSET(client->getSocket(), &readfds)) {
                    handleClientData(client);
                }
            }
        }
    }
#endif

    void acceptNewConnections() {
        for (int i = 0; i < 32 && running_.load(std::memory_order_acquire);
             ++i) {
            sockaddr_in clientAddress{};
            socklen_t clientAddressLength = sizeof(clientAddress);

            const socket_t clientSocket = accept(
                serverSocket_, reinterpret_cast<sockaddr*>(&clientAddress),
                &clientAddressLength);

            if (clientSocket == INVALID_SOCKVAL) {
#ifdef _WIN32
                if (WSAGetLastError() == WSAEWOULDBLOCK)
                    break;
#else
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
#endif
                if (running_.load(std::memory_order_acquire)) {
                    spdlog::error("Failed to accept connection");
                }
                break;
            }

            if (!configureClientSocket(clientSocket)) {
                closeSocket(clientSocket);
                continue;
            }

            char clientIp[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp,
                      INET_ADDRSTRLEN);
            const std::string clientAddr =
                std::format("{}:{}", clientIp, ntohs(clientAddress.sin_port));
            const int clientId =
                nextClientId_.fetch_add(1, std::memory_order_relaxed);

            if (!checkConnectionLimit()) {
                spdlog::warn("Max connections reached, rejecting client");
                closeSocket(clientSocket);
                continue;
            }

            spdlog::info("New client: {} (ID: {})", clientAddr, clientId);

            auto client = std::make_shared<ClientConnection>(
                clientSocket, clientAddr, clientId);

#ifdef __linux__
            epoll_event event{};
            event.events = EPOLLIN | EPOLLET;
            event.data.fd = clientSocket;
            if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, clientSocket, &event) ==
                -1) {
                spdlog::error("Failed to add client to epoll");
                continue;
            }
#endif

            {
                std::unique_lock<std::shared_mutex> lock(clientsMutex_);
                clients_[clientId] = client;
            }

            {
                std::lock_guard<std::mutex> lock(handlerMutex_);
                if (connectHandler_) {
                    try {
                        connectHandler_(clientId, clientAddr);
                    } catch (const std::exception& e) {
                        spdlog::error("Connect handler exception: {}",
                                      e.what());
                    }
                }
            }
        }
    }

    bool configureClientSocket(socket_t clientSocket) {
#ifdef _WIN32
        u_long mode = 1;
        if (ioctlsocket(clientSocket, FIONBIO, &mode) != 0) {
            spdlog::error("Failed to set client socket non-blocking");
            return false;
        }
#else
        const int flags = fcntl(clientSocket, F_GETFL, 0);
        if (flags == -1 ||
            fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK) == -1) {
            spdlog::error("Failed to set client socket non-blocking");
            return false;
        }
#endif

        int opt = 1;
        if (setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY,
                       reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0) {
            spdlog::warn("Failed to set TCP_NODELAY on client socket");
        }

        return true;
    }

    bool checkConnectionLimit() {
        std::shared_lock<std::shared_mutex> lock(clientsMutex_);
        return std::count_if(
                   clients_.begin(), clients_.end(), [](const auto& pair) {
                       return pair.second && pair.second->isConnected();
                   }) < maxConnections_;
    }

    void handleClientData(std::shared_ptr<ClientConnection> client) {
        if (!client || !client->isConnected())
            return;

        auto buffer = bufferPool_->acquire();
        const socket_t sock = client->getSocket();

        const int bytesRead =
            recv(sock, buffer->data(), static_cast<int>(buffer->size()), 0);

        if (bytesRead > 0) {
            client->recordReceivedData(bytesRead);

            const std::string_view message(buffer->data(), bytesRead);
            std::lock_guard<std::mutex> lock(handlerMutex_);
            if (messageHandler_) {
                try {
                    messageHandler_(message);
                } catch (const std::exception& e) {
                    spdlog::error("Message handler exception: {}", e.what());
                }
            }
        } else if (bytesRead == 0) {
            client->disconnect();
            disconnectClient(client->getId());
        } else {
#ifdef _WIN32
            if (WSAGetLastError() != WSAEWOULDBLOCK) {
                spdlog::error("Client read error: {}", WSAGetLastError());
                client->disconnect();
                disconnectClient(client->getId());
            }
#else
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                spdlog::error("Client read error: {}", strerror(errno));
                client->disconnect();
                disconnectClient(client->getId());
            }
#endif
        }

        bufferPool_->release(std::move(buffer));
    }

    void disconnectClient(int clientId) {
        std::string clientAddr;

        {
            std::shared_lock<std::shared_mutex> lock(clientsMutex_);
            const auto it = clients_.find(clientId);
            if (it != clients_.end() && it->second) {
                clientAddr = it->second->getAddress();
            }
        }

        {
            std::unique_lock<std::shared_mutex> lock(clientsMutex_);
            clients_.erase(clientId);
        }

        if (!clientAddr.empty()) {
            std::lock_guard<std::mutex> lock(handlerMutex_);
            if (disconnectHandler_) {
                try {
                    disconnectHandler_(clientId, clientAddr);
                } catch (const std::exception& e) {
                    spdlog::error("Disconnect handler exception: {}", e.what());
                }
            }
        }
    }

    void checkClientTimeouts(std::stop_token stoken) {
        while (!stoken.stop_requested() &&
               running_.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            const auto now = std::chrono::steady_clock::now();
            std::vector<std::shared_ptr<ClientConnection>> timeoutClients;

            {
                std::shared_lock<std::shared_mutex> lock(clientsMutex_);
                for (const auto& [_, client] : clients_) {
                    if (client && client->isConnected() &&
                        (now - client->getLastActivity()) > clientTimeout_) {
                        timeoutClients.push_back(client);
                    }
                }
            }

            for (auto& client : timeoutClients) {
                spdlog::info("Client timeout: {} (ID: {})",
                             client->getAddress(), client->getId());
                client->disconnect();
                disconnectClient(client->getId());
            }
        }
    }

    void cleanupResources() noexcept {
        try {
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
            spdlog::error("Resource cleanup error: {}", e.what());
        }
    }
};

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
