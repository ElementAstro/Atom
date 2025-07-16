/*
 * udp_server.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-4
Revision Date: 2024-05-22

Description: A high-performance, Asio-based asynchronous
             UDP server utilizing modern C++ concurrency.

*************************************************/

#include "async_udpserver.hpp"

#include <algorithm>
#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>
#include <vector>

#include <fmt/format.h>
#include <spdlog/spdlog.h>

namespace atom::async::connection {

// Default buffer size increased for better performance with larger messages
constexpr std::size_t DEFAULT_BUFFER_SIZE = 8192;
// Default number of worker threads
constexpr unsigned int DEFAULT_THREAD_COUNT = 1;
// Maximum queue size for outgoing messages
constexpr std::size_t MAX_QUEUE_SIZE = 1000;

class UdpSocketHub::Impl {
public:
    Impl(unsigned int numThreads = DEFAULT_THREAD_COUNT)
        : socket_(io_context_),
          receiveBufferSize_(DEFAULT_BUFFER_SIZE),
          numThreads_(numThreads > 0 ? numThreads : 1),
          running_(false),
          ipFilterEnabled_(false) {
        // Initialize atomic shared pointers with empty collections
        handlers_.store(std::make_shared<const std::vector<MessageHandler>>());
        errorHandlers_.store(
            std::make_shared<const std::vector<ErrorHandler>>());
        multicastGroups_.store(
            std::make_shared<const std::unordered_set<std::string>>());
        allowedIps_.store(
            std::make_shared<const std::unordered_set<asio::ip::address>>());
    }

    ~Impl() { stop(); }

    bool start(unsigned short port, bool ipv6) {
        if (running_.exchange(true)) {
            spdlog::warn("UDP server is already running.");
            return false;
        }

        try {
            auto protocol = ipv6 ? asio::ip::udp::v6() : asio::ip::udp::v4();
            asio::ip::udp::endpoint endpoint(protocol, port);

            socket_.open(endpoint.protocol());
            socket_.set_option(asio::ip::udp::socket::reuse_address(true));
            socket_.bind(endpoint);

            receiveBuffer_.resize(receiveBufferSize_);

            doReceive();

            // Start I/O threads using C++20 jthread for automatic management
            io_threads_.reserve(numThreads_);
            for (unsigned int i = 0; i < numThreads_; ++i) {
                io_threads_.emplace_back([this] {
                    try {
                        io_context_.run();
                    } catch (const std::exception& e) {
                        notifyError(
                            fmt::format("IO Context exception: {}", e.what()));
                    }
                });
            }

            startOutgoingMessageWorker();
            spdlog::info("UDP server started on port {}", port);
            return true;
        } catch (const std::exception& e) {
            notifyError(
                fmt::format("Failed to start UDP server: {}", e.what()));
            running_ = false;  // Reset state on failure
            return false;
        }
    }

    void stop() {
        if (!running_.exchange(false)) {
            return;
        }

        spdlog::info("Stopping UDP server...");

        // Cooperatively stop all worker threads
        stopSource_.request_stop();

        asio::error_code ec;
        if (socket_.is_open()) {
            [[maybe_unused]] auto res = socket_.close(ec);
            if (ec) {
                notifyError("Error closing socket", ec);
            }
        }

        io_context_.stop();
        outgoingCV_.notify_all();

        // jthreads will auto-join in their destructors
        io_threads_.clear();
        // The outgoing thread jthread will also auto-join

        if (!io_context_.stopped()) {
            io_context_.restart();
        }
        spdlog::info("UDP server stopped.");
    }

    [[nodiscard]] bool isRunning() const noexcept {
        return running_.load(std::memory_order_relaxed);
    }

    void addMessageHandler(MessageHandler handler) {
        std::scoped_lock lock(handlerWriteMutex_);
        auto oldHandlers = handlers_.load(std::memory_order_relaxed);
        auto newHandlers =
            std::make_shared<std::vector<MessageHandler>>(*oldHandlers);
        newHandlers->push_back(std::move(handler));
        handlers_.store(newHandlers, std::memory_order_release);
    }

    void removeMessageHandler(MessageHandler handler) {
        std::scoped_lock lock(handlerWriteMutex_);
        auto oldHandlers = handlers_.load(std::memory_order_relaxed);
        auto newHandlers = std::make_shared<std::vector<MessageHandler>>();
        newHandlers->reserve(oldHandlers->size());

        auto target = handler.target<void(
            const std::string&, const std::string&, unsigned short)>();
        std::copy_if(
            oldHandlers->begin(), oldHandlers->end(),
            std::back_inserter(*newHandlers), [&](const MessageHandler& h) {
                return h.target<void(const std::string&, const std::string&,
                                     unsigned short)>() != target;
            });

        handlers_.store(newHandlers, std::memory_order_release);
    }

    void addErrorHandler(ErrorHandler handler) {
        std::scoped_lock lock(errorHandlersWriteMutex_);
        auto oldHandlers = errorHandlers_.load(std::memory_order_relaxed);
        auto newHandlers =
            std::make_shared<std::vector<ErrorHandler>>(*oldHandlers);
        newHandlers->push_back(std::move(handler));
        errorHandlers_.store(newHandlers, std::memory_order_release);
    }

    void removeErrorHandler(ErrorHandler handler) {
        std::scoped_lock lock(errorHandlersWriteMutex_);
        auto oldHandlers = errorHandlers_.load(std::memory_order_relaxed);
        auto newHandlers = std::make_shared<std::vector<ErrorHandler>>();
        newHandlers->reserve(oldHandlers->size());

        auto target =
            handler.target<void(const std::string&, const std::error_code&)>();
        std::copy_if(
            oldHandlers->begin(), oldHandlers->end(),
            std::back_inserter(*newHandlers), [&](const ErrorHandler& h) {
                return h.target<void(const std::string&,
                                     const std::error_code&)>() != target;
            });

        errorHandlers_.store(newHandlers, std::memory_order_release);
    }

    bool sendTo(const std::string& message, const std::string& ipAddress,
                unsigned short port) {
        if (!isRunning()) {
            notifyError("Cannot send message: Server is not running");
            return false;
        }
        try {
            return queueOutgoingMessage(
                {message, asio::ip::udp::endpoint(
                              asio::ip::make_address(ipAddress), port)});
        } catch (const std::system_error& e) {
            notifyError(fmt::format("Failed to resolve address {}: {}",
                                    ipAddress, e.what()),
                        e.code());
            return false;
        }
    }

    bool broadcast(const std::string& message, unsigned short port) {
        if (!isRunning()) {
            notifyError("Cannot broadcast message: Server is not running");
            return false;
        }
        asio::error_code ec;
        [[maybe_unused]] auto res =
            socket_.set_option(asio::socket_base::broadcast(true), ec);
        if (ec) {
            notifyError("Failed to enable broadcast option", ec);
            return false;
        }
        return queueOutgoingMessage(
            {message,
             asio::ip::udp::endpoint(asio::ip::address_v4::broadcast(), port)});
    }

    bool joinMulticastGroup(const std::string& multicastAddress) {
        if (!isRunning()) {
            notifyError("Cannot join multicast group: Server is not running");
            return false;
        }
        try {
            auto multicastAddr = asio::ip::make_address(multicastAddress);
            if (!multicastAddr.is_multicast()) {
                notifyError(fmt::format("Invalid multicast address: {}",
                                        multicastAddress));
                return false;
            }
            socket_.set_option(asio::ip::multicast::join_group(multicastAddr));

            std::scoped_lock lock(multicastWriteMutex_);
            auto oldGroups = multicastGroups_.load(std::memory_order_relaxed);
            auto newGroups =
                std::make_shared<std::unordered_set<std::string>>(*oldGroups);
            newGroups->insert(multicastAddress);
            multicastGroups_.store(newGroups, std::memory_order_release);

            spdlog::info("Joined multicast group: {}", multicastAddress);
            return true;
        } catch (const std::system_error& e) {
            notifyError(fmt::format("Failed to join multicast group {}: {}",
                                    multicastAddress, e.what()),
                        e.code());
            return false;
        }
    }

    bool leaveMulticastGroup(const std::string& multicastAddress) {
        if (!isRunning()) {
            notifyError("Cannot leave multicast group: Server is not running");
            return false;
        }
        try {
            auto multicastAddr = asio::ip::make_address(multicastAddress);
            if (!multicastAddr.is_multicast()) {
                notifyError(fmt::format("Invalid multicast address: {}",
                                        multicastAddress));
                return false;
            }
            socket_.set_option(asio::ip::multicast::leave_group(multicastAddr));

            std::scoped_lock lock(multicastWriteMutex_);
            auto oldGroups = multicastGroups_.load(std::memory_order_relaxed);
            auto newGroups =
                std::make_shared<std::unordered_set<std::string>>(*oldGroups);
            newGroups->erase(multicastAddress);
            multicastGroups_.store(newGroups, std::memory_order_release);

            spdlog::info("Left multicast group: {}", multicastAddress);
            return true;
        } catch (const std::system_error& e) {
            notifyError(fmt::format("Failed to leave multicast group {}: {}",
                                    multicastAddress, e.what()),
                        e.code());
            return false;
        }
    }

    bool sendToMulticast(const std::string& message,
                         const std::string& multicastAddress,
                         unsigned short port) {
        if (!isRunning()) {
            notifyError("Cannot send multicast message: Server is not running");
            return false;
        }
        try {
            auto multicastAddr = asio::ip::make_address(multicastAddress);
            if (!multicastAddr.is_multicast()) {
                notifyError(fmt::format("Invalid multicast address: {}",
                                        multicastAddress));
                return false;
            }
            socket_.set_option(asio::ip::multicast::hops(1));
            return queueOutgoingMessage(
                {message, asio::ip::udp::endpoint(multicastAddr, port)});
        } catch (const std::system_error& e) {
            notifyError(
                fmt::format("Failed to prepare multicast message for {}: {}",
                            multicastAddress, e.what()),
                e.code());
            return false;
        }
    }

    template <typename T>
    bool setSocketOption(SocketOption option, const T& value) {
        if (!socket_.is_open()) {
            notifyError("Cannot set socket option: Socket is not open");
            return false;
        }
        try {
            switch (option) {
                case SocketOption::Broadcast:
                    if constexpr (std::is_convertible_v<T, bool>) {
                        socket_.set_option(asio::socket_base::broadcast(
                            static_cast<bool>(value)));
                    } else {
                        notifyError(
                            "Invalid type for Broadcast option, bool "
                            "expected.");
                        return false;
                    }
                    break;
                case SocketOption::ReuseAddress:
                    if constexpr (std::is_convertible_v<T, bool>) {
                        socket_.set_option(asio::socket_base::reuse_address(
                            static_cast<bool>(value)));
                    } else {
                        notifyError(
                            "Invalid type for ReuseAddress option, bool "
                            "expected.");
                        return false;
                    }
                    break;
                case SocketOption::ReceiveBufferSize:
                    if constexpr (std::is_convertible_v<T, int>) {
                        socket_.set_option(
                            asio::socket_base::receive_buffer_size(
                                static_cast<int>(value)));
                    } else {
                        notifyError(
                            "Invalid type for ReceiveBufferSize option, int "
                            "expected.");
                        return false;
                    }
                    break;
                case SocketOption::SendBufferSize:
                    if constexpr (std::is_convertible_v<T, int>) {
                        socket_.set_option(asio::socket_base::send_buffer_size(
                            static_cast<int>(value)));
                    } else {
                        notifyError(
                            "Invalid type for SendBufferSize option, int "
                            "expected.");
                        return false;
                    }
                    break;
            }
            return true;
        } catch (const std::system_error& e) {
            notifyError(
                fmt::format("Failed to set socket option: {}", e.what()),
                e.code());
            return false;
        }
    }

    bool setReceiveBufferSize(std::size_t size) {
        if (size == 0) {
            notifyError("Invalid buffer size: 0");
            return false;
        }
        receiveBufferSize_ = size;
        if (isRunning()) {
            receiveBuffer_.resize(size);
        }
        return setSocketOption(SocketOption::ReceiveBufferSize,
                               static_cast<int>(size));
    }

    bool setReceiveTimeout(const std::chrono::milliseconds& timeout) {
        if (!socket_.is_open()) {
            notifyError("Cannot set receive timeout: Socket is not open");
            return false;
        }
        try {
#if defined(ASIO_WINDOWS) || defined(__CYGWIN__)
            DWORD milliseconds = static_cast<DWORD>(timeout.count());
            setsockopt(socket_.native_handle(), SOL_SOCKET, SO_RCVTIMEO,
                       (const char*)&milliseconds, sizeof(milliseconds));
#else
            struct timeval tv;
            tv.tv_sec =
                std::chrono::duration_cast<std::chrono::seconds>(timeout)
                    .count();
            tv.tv_usec = std::chrono::duration_cast<std::chrono::microseconds>(
                             timeout % std::chrono::seconds(1))
                             .count();
            setsockopt(socket_.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv,
                       sizeof(tv));
#endif
            return true;
        } catch (const std::system_error& e) {
            notifyError(
                fmt::format("Failed to set receive timeout: {}", e.what()),
                e.code());
            return false;
        }
    }

    Statistics getStatistics() const { return stats_; }

    void resetStatistics() {
        stats_.reset();
        spdlog::info("UDP server statistics have been reset.");
    }

    void addAllowedIp(const std::string& ip) {
        try {
            std::scoped_lock lock(ipFilterWriteMutex_);
            auto oldIps = allowedIps_.load(std::memory_order_relaxed);
            auto newIps =
                std::make_shared<std::unordered_set<asio::ip::address>>(
                    *oldIps);
            newIps->insert(asio::ip::make_address(ip));
            allowedIps_.store(newIps, std::memory_order_release);
            ipFilterEnabled_.store(true, std::memory_order_release);
        } catch (const std::system_error& e) {
            notifyError(
                fmt::format("Failed to add IP filter for {}: {}", ip, e.what()),
                e.code());
        }
    }

    void removeAllowedIp(const std::string& ip) {
        try {
            std::scoped_lock lock(ipFilterWriteMutex_);
            auto oldIps = allowedIps_.load(std::memory_order_relaxed);
            auto newIps =
                std::make_shared<std::unordered_set<asio::ip::address>>(
                    *oldIps);
            newIps->erase(asio::ip::make_address(ip));
            ipFilterEnabled_.store(!newIps->empty(), std::memory_order_release);
            allowedIps_.store(newIps, std::memory_order_release);
        } catch (const std::system_error& e) {
            notifyError(fmt::format("Failed to remove IP filter for {}: {}", ip,
                                    e.what()),
                        e.code());
        }
    }

    void clearIpFilters() {
        std::scoped_lock lock(ipFilterWriteMutex_);
        allowedIps_.store(
            std::make_shared<const std::unordered_set<asio::ip::address>>());
        ipFilterEnabled_.store(false, std::memory_order_release);
    }

private:
    struct OutgoingMessage {
        std::string message;
        asio::ip::udp::endpoint endpoint;
    };

    void doReceive() {
        socket_.async_receive_from(
            asio::buffer(receiveBuffer_), senderEndpoint_,
            [this](std::error_code errorCode, std::size_t bytesReceived) {
                if (errorCode) {
                    // operation_aborted is expected on clean shutdown
                    if (errorCode != asio::error::operation_aborted) {
                        notifyError("Receive error", errorCode);
                    }
                    return;
                }

                if (bytesReceived > 0) {
                    stats_.bytesReceived.fetch_add(bytesReceived,
                                                   std::memory_order_relaxed);
                    stats_.messagesReceived.fetch_add(
                        1, std::memory_order_relaxed);

                    // IP filter check is lock-free
                    if (ipFilterEnabled_.load(std::memory_order_acquire)) {
                        auto currentAllowedIps =
                            allowedIps_.load(std::memory_order_acquire);
                        if (currentAllowedIps->find(
                                senderEndpoint_.address()) ==
                            currentAllowedIps->end()) {
                            doReceive();  // Silently drop and wait for next
                            return;
                        }
                    }

                    auto message = std::make_shared<std::string>(
                        receiveBuffer_.data(), bytesReceived);
                    auto senderIp = std::make_shared<std::string>(
                        senderEndpoint_.address().to_string());
                    unsigned short senderPort = senderEndpoint_.port();

                    // Post handler execution to the thread pool to unblock the
                    // receiver
                    asio::post(io_context_, [this, message, senderIp,
                                             senderPort]() {
                        notifyMessageHandlers(*message, *senderIp, senderPort);
                    });
                }

                // Continue the receive loop if the server is still running
                if (isRunning()) {
                    doReceive();
                }
            });
    }

    void notifyMessageHandlers(const std::string& message,
                               const std::string& senderIp,
                               unsigned short senderPort) {
        // This read is lock-free
        auto currentHandlers = handlers_.load(std::memory_order_acquire);
        if (currentHandlers->empty()) {
            return;
        }

        for (const auto& handler : *currentHandlers) {
            try {
                handler(message, senderIp, senderPort);
            } catch (const std::exception& e) {
                notifyError(
                    fmt::format("Exception in message handler: {}", e.what()));
            }
        }
    }

    void notifyError(const std::string& errorMessage,
                     const std::error_code& ec = {}) {
        stats_.errors.fetch_add(1, std::memory_order_relaxed);
        if (ec) {
            spdlog::error("UDP Socket Error: {} (Code: {}, Message: {})",
                          errorMessage, ec.value(), ec.message());
        } else {
            spdlog::error("UDP Socket Error: {}", errorMessage);
        }

        // This read is lock-free
        auto currentHandlers = errorHandlers_.load(std::memory_order_acquire);
        if (currentHandlers->empty()) {
            return;
        }

        for (const auto& handler : *currentHandlers) {
            try {
                handler(errorMessage, ec);
            } catch (const std::exception& e) {
                spdlog::error("Exception in error handler: {}", e.what());
            }
        }
    }

    bool queueOutgoingMessage(OutgoingMessage&& msg) {
        std::unique_lock<std::mutex> lock(outgoingQueueMutex_);
        if (outgoingQueue_.size() >= MAX_QUEUE_SIZE) {
            lock.unlock();
            notifyError("Outgoing message queue is full, message discarded");
            return false;
        }
        outgoingQueue_.push(std::move(msg));
        lock.unlock();
        outgoingCV_.notify_one();
        return true;
    }

    void startOutgoingMessageWorker() {
        outgoingThread_ = std::jthread(
            [this](std::stop_token st) {
                std::queue<OutgoingMessage> localQueue;
                while (!st.stop_requested()) {
                    {
                        std::unique_lock<std::mutex> lock(outgoingQueueMutex_);
                        // Wait until the queue has items or a stop is requested
                        outgoingCV_.wait(lock, st, [this] {
                            return !outgoingQueue_.empty();
                        });

                        // After waking, drain the entire queue to a local one
                        // This minimizes lock holding time.
                        if (!outgoingQueue_.empty()) {
                            localQueue.swap(outgoingQueue_);
                        }
                    }  // Mutex is unlocked here

                    // Process all drained messages without holding the lock
                    while (!localQueue.empty()) {
                        OutgoingMessage& msg = localQueue.front();
                        try {
                            std::error_code ec;
                            std::size_t bytesSent = socket_.send_to(
                                asio::buffer(msg.message), msg.endpoint, 0, ec);
                            if (ec) {
                                notifyError("Failed to send message", ec);
                            } else {
                                stats_.bytesSent.fetch_add(
                                    bytesSent, std::memory_order_relaxed);
                                stats_.messagesSent.fetch_add(
                                    1, std::memory_order_relaxed);
                            }
                        } catch (const std::system_error& e) {
                            notifyError(
                                fmt::format(
                                    "Exception while sending message: {}",
                                    e.what()),
                                e.code());
                        }
                        localQueue.pop();
                    }
                }
            },
            stopSource_.get_token());
    }

    asio::io_context io_context_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint senderEndpoint_;
    std::vector<char> receiveBuffer_;
    std::size_t receiveBufferSize_;

    std::vector<std::jthread> io_threads_;
    std::jthread outgoingThread_;
    unsigned int numThreads_;
    std::stop_source stopSource_;

    std::atomic<bool> running_;

    // High-performance, copy-on-write collections for lock-free reads
    std::atomic<std::shared_ptr<const std::vector<MessageHandler>>> handlers_;
    std::mutex handlerWriteMutex_;

    std::atomic<std::shared_ptr<const std::vector<ErrorHandler>>>
        errorHandlers_;
    std::mutex errorHandlersWriteMutex_;

    std::atomic<std::shared_ptr<const std::unordered_set<std::string>>>
        multicastGroups_;
    std::mutex multicastWriteMutex_;

    std::atomic<std::shared_ptr<const std::unordered_set<asio::ip::address>>>
        allowedIps_;
    std::mutex ipFilterWriteMutex_;
    std::atomic<bool> ipFilterEnabled_;

    // High-throughput outgoing message queue
    std::queue<OutgoingMessage> outgoingQueue_;
    std::mutex outgoingQueueMutex_;
    std::condition_variable_any outgoingCV_;

    Statistics stats_;
};

// UdpSocketHub PIMPL forwarding
UdpSocketHub::UdpSocketHub() : impl_(std::make_unique<Impl>()) {}
UdpSocketHub::UdpSocketHub(unsigned int numThreads)
    : impl_(std::make_unique<Impl>(numThreads)) {}
UdpSocketHub::~UdpSocketHub() { impl_->stop(); }

bool UdpSocketHub::start(unsigned short port, bool ipv6) {
    return impl_->start(port, ipv6);
}
void UdpSocketHub::stop() { impl_->stop(); }
bool UdpSocketHub::isRunning() const noexcept { return impl_->isRunning(); }
void UdpSocketHub::addMessageHandler(MessageHandler handler) {
    impl_->addMessageHandler(std::move(handler));
}
void UdpSocketHub::removeMessageHandler(MessageHandler handler) {
    impl_->removeMessageHandler(std::move(handler));
}
void UdpSocketHub::addErrorHandler(ErrorHandler handler) {
    impl_->addErrorHandler(std::move(handler));
}
void UdpSocketHub::removeErrorHandler(ErrorHandler handler) {
    impl_->removeErrorHandler(std::move(handler));
}
bool UdpSocketHub::sendTo(const std::string& message,
                          const std::string& ipAddress, unsigned short port) {
    return impl_->sendTo(message, ipAddress, port);
}
bool UdpSocketHub::broadcast(const std::string& message, unsigned short port) {
    return impl_->broadcast(message, port);
}
bool UdpSocketHub::joinMulticastGroup(const std::string& multicastAddress) {
    return impl_->joinMulticastGroup(multicastAddress);
}
bool UdpSocketHub::leaveMulticastGroup(const std::string& multicastAddress) {
    return impl_->leaveMulticastGroup(multicastAddress);
}
bool UdpSocketHub::sendToMulticast(const std::string& message,
                                   const std::string& multicastAddress,
                                   unsigned short port) {
    return impl_->sendToMulticast(message, multicastAddress, port);
}

template <typename T>
bool UdpSocketHub::setSocketOption(SocketOption option, const T& value) {
    return impl_->setSocketOption(option, value);
}

bool UdpSocketHub::setReceiveBufferSize(std::size_t size) {
    return impl_->setReceiveBufferSize(size);
}
bool UdpSocketHub::setReceiveTimeout(const std::chrono::milliseconds& timeout) {
    return impl_->setReceiveTimeout(timeout);
}
UdpSocketHub::Statistics UdpSocketHub::getStatistics() const {
    return impl_->getStatistics();
}
void UdpSocketHub::resetStatistics() { impl_->resetStatistics(); }
void UdpSocketHub::addAllowedIp(const std::string& ip) {
    impl_->addAllowedIp(ip);
}
void UdpSocketHub::removeAllowedIp(const std::string& ip) {
    impl_->removeAllowedIp(ip);
}
void UdpSocketHub::clearIpFilters() { impl_->clearIpFilters(); }

// Explicit template instantiations for common socket options
template bool UdpSocketHub::setSocketOption<bool>(SocketOption, const bool&);
template bool UdpSocketHub::setSocketOption<int>(SocketOption, const int&);

}  // namespace atom::async::connection