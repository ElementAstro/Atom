#include "sockethub.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <format>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "atom/log/loguru.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
const socket_t INVALID_SOCKVAL = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
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

class SocketHubImpl {
public:
    SocketHubImpl()
        : running_(false),
          serverSocket(INVALID_SOCKVAL)
#ifdef __linux__
          ,
          epoll_fd(INVALID_SOCKVAL)
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

            serverSocket = socket(AF_INET, SOCK_STREAM, 0);
            if (serverSocket == INVALID_SOCKVAL) {
                throw SocketException("Failed to create server socket");
            }

            // Set socket to non-blocking mode
#ifdef _WIN32
            u_long mode = 1;
            if (ioctlsocket(serverSocket, FIONBIO, &mode) != 0) {
                throw SocketException("Failed to set non-blocking mode");
            }
#else
            int flags = fcntl(serverSocket, F_GETFL, 0);
            if (flags == -1 ||
                fcntl(serverSocket, F_SETFL, flags | O_NONBLOCK) == -1) {
                throw SocketException("Failed to set non-blocking mode");
            }
#endif

            // Enable address reuse
            int opt = 1;
            if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR,
                           reinterpret_cast<const char*>(&opt),
                           sizeof(opt)) < 0) {
                throw SocketException("Failed to set socket options");
            }

            sockaddr_in serverAddress{};
            serverAddress.sin_family = AF_INET;
            serverAddress.sin_addr.s_addr = INADDR_ANY;
            serverAddress.sin_port = htons(static_cast<uint16_t>(port));

#ifdef _WIN32
            if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress),
                     sizeof(serverAddress)) == SOCKET_ERROR)
#else
            if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress),
                     sizeof(serverAddress)) < 0)
#endif
            {
                throw SocketException("Failed to bind server socket");
            }

#ifdef _WIN32
            if (listen(serverSocket, maxConnections) == SOCKET_ERROR)
#else
            if (listen(serverSocket, maxConnections) < 0)
#endif
            {
                throw SocketException("Failed to listen on server socket");
            }

#ifdef __linux__
            epoll_fd = epoll_create1(0);
            if (epoll_fd == -1) {
                throw SocketException("Failed to create epoll file descriptor");
            }

            struct epoll_event event;
            event.events =
                EPOLLIN | EPOLLET;  // Edge-triggered for better performance
            event.data.fd = serverSocket;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serverSocket, &event) ==
                -1) {
                throw SocketException("Failed to add server socket to epoll");
            }
#endif

            running_.store(true);
            DLOG_F(INFO, "SocketHub started on port {}", port);

            // Start the accept thread with exception handling
            acceptThread = std::jthread([this](std::stop_token stoken) {
                try {
                    acceptConnections(stoken);
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Exception in accept thread: {}", e.what());
                    running_.store(false);
                }
            });
        } catch (const std::exception& e) {
            cleanupSocket();
            cleanupWinsock();
            LOG_F(ERROR, "Failed to start SocketHub: {}", e.what());
            throw;
        }
    }

    void stop() noexcept {
        if (!running_.load()) {
            LOG_F(WARNING, "SocketHub is not running.");
            return;
        }

        running_.store(false);

        try {
            if (acceptThread.joinable()) {
                acceptThread.request_stop();
                acceptThread.join();
            }

            cleanupSocket();
            cleanupWinsock();
            DLOG_F(INFO, "SocketHub stopped successfully.");
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error during SocketHub shutdown: {}", e.what());
        }
    }

    void addHandler(std::function<void(std::string_view)> handler) {
        if (!handler) {
            throw std::invalid_argument(
                "Invalid message handler (null function)");
        }
        this->handler = std::move(handler);
    }

    [[nodiscard]] auto isRunning() const noexcept -> bool {
        return running_.load(std::memory_order_acquire);
    }

private:
    static constexpr int maxConnections = 64;  // Increased from 10
    static constexpr int bufferSize = 4096;    // Increased from 1024
    static constexpr auto clientTimeout = std::chrono::seconds(60);

    std::atomic<bool> running_{false};
    socket_t serverSocket{INVALID_SOCKVAL};
    std::vector<socket_t> clients;

#ifdef __linux__
    int epoll_fd{INVALID_SOCKVAL};
#endif

    std::map<socket_t, std::jthread> clientThreads_;
    std::mutex clientMutex;
    std::jthread acceptThread;

    std::function<void(std::string_view)> handler;

    // Track last activity time for each client
    std::map<socket_t, std::chrono::steady_clock::time_point> clientActivity;

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
        std::array<epoll_event, maxConnections> events;

        while (!stoken.stop_requested() && running_.load()) {
            // Use a timeout for epoll_wait to periodically check for client
            // timeouts
            int n = epoll_wait(epoll_fd, events.data(), events.size(), 1000);

            if (n < 0) {
                if (errno == EINTR)
                    continue;  // Interrupted, try again
                LOG_F(ERROR, "epoll_wait failed: {}", strerror(errno));
                break;
            }

            // Check for client timeouts
            checkClientTimeouts();

            // Process events
            for (int i = 0; i < n; i++) {
                if (events[i].data.fd == serverSocket) {
                    handleNewConnections();
                } else {
                    // Event on existing client socket
                    updateClientActivity(events[i].data.fd);
                    if (events[i].events & EPOLLIN) {
                        handleClientData(events[i].data.fd);
                    }
                    if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                        disconnectClient(events[i].data.fd);
                    }
                }
            }
        }
#else
        fd_set readfds;
        timeval timeout;
        timeout.tv_sec = 1;  // 1 second timeout for select
        timeout.tv_usec = 0;

        while (!stoken.stop_requested() && running_.load()) {
            FD_ZERO(&readfds);
            FD_SET(serverSocket, &readfds);

            // Add all client sockets
            {
                std::scoped_lock lock(clientMutex);
                for (const auto& client : clients) {
                    FD_SET(client, &readfds);
                }
            }

            // Calculate max socket for select
            socket_t maxSocket = serverSocket;
            {
                std::scoped_lock lock(clientMutex);
                for (const auto& client : clients) {
                    if (client > maxSocket) {
                        maxSocket = client;
                    }
                }
            }

            // Wait for activity
            int activity =
                select(maxSocket + 1, &readfds, nullptr, nullptr, &timeout);

            if (activity < 0 && errno != EINTR) {
                LOG_F(ERROR, "select failed: {}", strerror(errno));
                break;
            }

            // Check for client timeouts
            checkClientTimeouts();

            // New connection on server socket
            if (FD_ISSET(serverSocket, &readfds)) {
                handleNewConnections();
            }

            // Check for data on client sockets
            std::vector<socket_t> activeClients;
            {
                std::scoped_lock lock(clientMutex);
                activeClients = clients;  // Make a copy to avoid holding lock
                                          // during processing
            }

            for (const auto& client : activeClients) {
                if (FD_ISSET(client, &readfds)) {
                    updateClientActivity(client);
                    handleClientData(client);
                }
            }
        }
#endif
    }

    void handleNewConnections() {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);

        // Accept multiple connections at once - loop until no more pending
        // connections
        while (running_.load()) {
            socket_t clientSocket = accept(
                serverSocket, reinterpret_cast<sockaddr*>(&clientAddress),
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

            char clientIp[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(clientAddress.sin_addr), clientIp,
                      INET_ADDRSTRLEN);
            LOG_F(INFO, "New client connected: {}:{}", clientIp,
                  ntohs(clientAddress.sin_port));

#ifdef __linux__
            // Make client socket non-blocking
            int flags = fcntl(clientSocket, F_GETFL, 0);
            if (flags != -1) {
                fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);
            }

            // Add to epoll
            struct epoll_event event;
            event.events = EPOLLIN | EPOLLET;
            event.data.fd = clientSocket;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, clientSocket, &event) ==
                -1) {
                LOG_F(ERROR, "Failed to add client socket to epoll");
                closeSocket(clientSocket);
                continue;
            }
#endif

            // Add to clients list
            {
                std::scoped_lock lock(clientMutex);
                if (clients.size() >= maxConnections) {
                    LOG_F(WARNING,
                          "Maximum connections reached, rejecting client");
                    closeSocket(clientSocket);
                    continue;
                }
                clients.push_back(clientSocket);
                updateClientActivity(clientSocket);
            }

            // Start client handler thread
            clientThreads_[clientSocket] =
                std::jthread([this, clientSocket](std::stop_token stoken) {
                    try {
                        handleClientMessages(clientSocket, stoken);
                    } catch (const std::exception& e) {
                        LOG_F(ERROR, "Exception in client thread: {}",
                              e.what());
                        disconnectClient(clientSocket);
                    }
                });
        }
    }

    void handleClientData(socket_t clientSocket) {
        updateClientActivity(clientSocket);

        // Read available data
        std::array<char, bufferSize> buffer;
        int bytesRead = recv(clientSocket, buffer.data(), buffer.size(), 0);

        if (bytesRead > 0) {
            std::string_view message(buffer.data(), bytesRead);
            if (handler) {
                handler(message);
            }
        } else if (bytesRead == 0 ||
#ifdef _WIN32
                   WSAGetLastError() != WSAEWOULDBLOCK
#else
                   (errno != EAGAIN && errno != EWOULDBLOCK)
#endif
        ) {
            // Connection closed or error
            disconnectClient(clientSocket);
        }
    }

    void handleClientMessages(socket_t clientSocket, std::stop_token stoken) {
        std::array<char, bufferSize> buffer;

        while (!stoken.stop_requested() && running_.load()) {
            try {
                std::fill(buffer.begin(), buffer.end(), 0);
                int bytesRead =
                    recv(clientSocket, buffer.data(), buffer.size(), 0);

                if (bytesRead > 0) {
                    updateClientActivity(clientSocket);
                    std::string_view message(buffer.data(), bytesRead);

                    if (handler) {
                        handler(message);
                    }
                } else if (bytesRead == 0) {
                    // Connection closed by client
                    LOG_F(INFO, "Client disconnected gracefully");
                    break;
                } else {
#ifdef _WIN32
                    int error = WSAGetLastError();
                    if (error == WSAEWOULDBLOCK) {
                        // No data available, sleep a bit to avoid busy waiting
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(10));
                        continue;
                    }
#else
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // No data available, sleep a bit to avoid busy waiting
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(10));
                        continue;
                    }
#endif
                    // Socket error
#ifdef _WIN32
                    LOG_F(ERROR, "Socket error during recv: {} ({})", error,
                          strerror(error));
#else
                    LOG_F(ERROR, "Socket error during recv: {} ({})", errno,
                          strerror(errno));
#endif
                    break;
                }
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Exception in message handling: {}", e.what());
                break;
            }
        }

        // Thread is exiting, disconnect client if still in the list
        disconnectClient(clientSocket);
    }

    void updateClientActivity(socket_t clientSocket) {
        std::scoped_lock lock(clientMutex);
        clientActivity[clientSocket] = std::chrono::steady_clock::now();
    }

    void checkClientTimeouts() {
        std::scoped_lock lock(clientMutex);
        auto now = std::chrono::steady_clock::now();

        std::vector<socket_t> timedOutClients;
        for (const auto& [socket, time] : clientActivity) {
            if (now - time > clientTimeout) {
                timedOutClients.push_back(socket);
            }
        }

        for (const auto& socket : timedOutClients) {
            LOG_F(INFO, "Client timed out");
            disconnectClientInternal(socket);
        }
    }

    void disconnectClient(socket_t clientSocket) {
        std::scoped_lock lock(clientMutex);
        disconnectClientInternal(clientSocket);
    }

    void disconnectClientInternal(socket_t clientSocket) {
        // Called with clientMutex already locked
        if (std::find(clients.begin(), clients.end(), clientSocket) !=
            clients.end()) {
#ifdef __linux__
            // Remove from epoll
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, clientSocket, nullptr);
#endif
            closeSocket(clientSocket);

            // Remove from clients list
            clients.erase(
                std::remove(clients.begin(), clients.end(), clientSocket),
                clients.end());
            clientActivity.erase(clientSocket);

            // Handle thread if exists
            auto threadIt = clientThreads_.find(clientSocket);
            if (threadIt != clientThreads_.end()) {
                if (threadIt->second.joinable()) {
                    threadIt->second.request_stop();
                    // Don't join here to avoid deadlock - thread will clean
                    // itself up
                }
                clientThreads_.erase(threadIt);
            }
        }
    }

    void cleanupSocket() noexcept {
        try {
            std::scoped_lock lock(clientMutex);

            // Request all client threads to stop
            for (auto& [_, thread] : clientThreads_) {
                thread.request_stop();
            }

            // Close all client sockets
            for (const auto& client : clients) {
                closeSocket(client);
            }
            clients.clear();
            clientActivity.clear();

            // Close server socket
            if (serverSocket != INVALID_SOCKVAL) {
                closeSocket(serverSocket);
                serverSocket = INVALID_SOCKVAL;
            }

#ifdef __linux__
            if (epoll_fd != INVALID_SOCKVAL) {
                close(epoll_fd);
                epoll_fd = INVALID_SOCKVAL;
            }
#endif

            // Clean up all threads outside the lock to avoid deadlock
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception during socket cleanup: {}", e.what());
        }

        // Clean up threads outside the lock
        try {
            for (auto& [_, thread] : clientThreads_) {
                if (thread.joinable()) {
                    thread.join();
                }
            }
            clientThreads_.clear();
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception during thread cleanup: {}", e.what());
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
    impl_->addHandler(std::move(handler));
}

auto SocketHub::isRunning() const noexcept -> bool {
    return impl_->isRunning();
}

}  // namespace atom::connection