/*
 * udp_server.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-4

Description: A simple Asio-based UDP server.

*************************************************/

#include "async_udpserver.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <mutex>
#include <queue>
#include <set>
#include <shared_mutex>
#include <thread>
#include <vector>

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
          running_(false),
          receiveBufferSize_(DEFAULT_BUFFER_SIZE),
          numThreads_(numThreads > 0 ? numThreads : 1),
          ipFilterEnabled_(false) {}

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
            stop();
            return false;
        }
    }

    void stop() {
        if (!running_.exchange(false)) {
            return;
        }

        spdlog::info("Stopping UDP server...");
        try {
            asio::error_code ec;
            [[maybe_unused]] auto res = socket_.close(ec);
            if (ec) {
                notifyError("Error closing socket", ec);
            }
        } catch (const std::exception& e) {
            notifyError(
                fmt::format("Exception while closing socket: {}", e.what()));
        }

        io_context_.stop();
        outgoingCV_.notify_all();

        for (auto& thread : io_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        io_threads_.clear();

        if (outgoingThread_.joinable()) {
            outgoingThread_.join();
        }

        io_context_.restart();
        spdlog::info("UDP server stopped.");
    }

    [[nodiscard]] bool isRunning() const noexcept {
        return running_.load(std::memory_order_relaxed);
    }

    void addMessageHandler(MessageHandler handler) {
        std::unique_lock<std::shared_mutex> lock(handlersMutex_);
        handlers_.push_back(std::move(handler));
    }

    void removeMessageHandler(MessageHandler handler) {
        std::unique_lock<std::shared_mutex> lock(handlersMutex_);
        handlers_.erase(
            std::remove_if(
                handlers_.begin(), handlers_.end(),
                [&](const MessageHandler& h) {
                    return h.target<void(const std::string&, const std::string&,
                                         unsigned short)>() ==
                           handler.target<void(const std::string&,
                                               const std::string&,
                                               unsigned short)>();
                }),
            handlers_.end());
    }

    void addErrorHandler(ErrorHandler handler) {
        std::unique_lock<std::shared_mutex> lock(errorHandlersMutex_);
        errorHandlers_.push_back(std::move(handler));
    }

    void removeErrorHandler(ErrorHandler handler) {
        std::unique_lock<std::shared_mutex> lock(errorHandlersMutex_);
        errorHandlers_.erase(
            std::remove_if(
                errorHandlers_.begin(), errorHandlers_.end(),
                [&](const ErrorHandler& h) {
                    return h.target<void(const std::string&,
                                         const std::error_code&)>() ==
                           handler.target<void(const std::string&,
                                               const std::error_code&)>();
                }),
            errorHandlers_.end());
    }

    bool sendTo(const std::string& message, const std::string& ipAddress,
                unsigned short port) {
        if (!isRunning()) {
            notifyError("Cannot send message: Server is not running");
            return false;
        }
        try {
            return queueOutgoingMessage(
                {message,
                 asio::ip::udp::endpoint(asio::ip::make_address(ipAddress),
                                         port),
                 false});
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
        return queueOutgoingMessage(
            {message,
             asio::ip::udp::endpoint(asio::ip::address_v4::broadcast(), port),
             true});
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
            std::unique_lock<std::shared_mutex> lock(multicastMutex_);
            multicastGroups_.insert(multicastAddress);
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
            std::unique_lock<std::shared_mutex> lock(multicastMutex_);
            multicastGroups_.erase(multicastAddress);
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
                {message, asio::ip::udp::endpoint(multicastAddr, port), false});
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
        if (!isRunning()) {
            notifyError("Cannot set socket option: Server is not running");
            return false;
        }
        try {
            switch (option) {
                case SocketOption::Broadcast:
                    socket_.set_option(
                        asio::socket_base::broadcast(static_cast<bool>(value)));
                    break;
                case SocketOption::ReuseAddress:
                    socket_.set_option(asio::socket_base::reuse_address(
                        static_cast<bool>(value)));
                    break;
                case SocketOption::ReceiveBufferSize:
                    socket_.set_option(asio::socket_base::receive_buffer_size(
                        static_cast<int>(value)));
                    break;
                case SocketOption::SendBufferSize:
                    socket_.set_option(asio::socket_base::send_buffer_size(
                        static_cast<int>(value)));
                    break;
                case SocketOption::ReceiveTimeout:  // Fallthrough
                case SocketOption::SendTimeout:     // Fallthrough
                default:
                    notifyError("Unsupported or unknown socket option");
                    return false;
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
        receiveBuffer_.resize(size);
        return setSocketOption(SocketOption::ReceiveBufferSize,
                               static_cast<int>(size));
    }

    bool setReceiveTimeout(const std::chrono::milliseconds& timeout) {
        if (!isRunning()) {
            notifyError("Cannot set receive timeout: Server is not running");
            return false;
        }
        try {
#if defined(ASIO_WINDOWS) || defined(__CYGWIN__)
            DWORD milliseconds = static_cast<DWORD>(timeout.count());
            setsockopt(socket_.native_handle(), SOL_SOCKET, SO_RCVTIMEO,
                       (const char*)&milliseconds, sizeof(milliseconds));
#else
            struct timeval tv;
            tv.tv_sec = static_cast<long>(timeout.count() / 1000);
            tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
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
            std::unique_lock<std::shared_mutex> lock(ipFilterMutex_);
            allowedIps_.insert(asio::ip::make_address(ip));
            ipFilterEnabled_ = true;
        } catch (const std::system_error& e) {
            notifyError(
                fmt::format("Failed to add IP filter for {}: {}", ip, e.what()),
                e.code());
        }
    }

    void removeAllowedIp(const std::string& ip) {
        try {
            std::unique_lock<std::shared_mutex> lock(ipFilterMutex_);
            allowedIps_.erase(asio::ip::make_address(ip));
            ipFilterEnabled_ = !allowedIps_.empty();
        } catch (const std::system_error& e) {
            notifyError(fmt::format("Failed to remove IP filter for {}: {}", ip,
                                    e.what()),
                        e.code());
        }
    }

    void clearIpFilters() {
        std::unique_lock<std::shared_mutex> lock(ipFilterMutex_);
        allowedIps_.clear();
        ipFilterEnabled_ = false;
    }

private:
    struct OutgoingMessage {
        std::string message;
        asio::ip::udp::endpoint endpoint;
        bool isBroadcast;
    };

    void doReceive() {
        socket_.async_receive_from(
            asio::buffer(receiveBuffer_), senderEndpoint_,
            [this](std::error_code errorCode, std::size_t bytesReceived) {
                if (errorCode) {
                    if (isRunning() &&
                        errorCode != asio::error::operation_aborted) {
                        notifyError("Receive error", errorCode);
                        doReceive();
                    }
                    return;
                }

                if (bytesReceived > 0) {
                    stats_.bytesReceived.fetch_add(bytesReceived,
                                                   std::memory_order_relaxed);
                    stats_.messagesReceived.fetch_add(
                        1, std::memory_order_relaxed);

                    if (ipFilterEnabled_) {
                        std::shared_lock<std::shared_mutex> lock(
                            ipFilterMutex_);
                        if (allowedIps_.find(senderEndpoint_.address()) ==
                            allowedIps_.end()) {
                            if (isRunning())
                                doReceive();
                            return;
                        }
                    }

                    auto message = std::make_shared<std::string>(
                        receiveBuffer_.data(), bytesReceived);
                    auto senderIp = std::make_shared<std::string>(
                        senderEndpoint_.address().to_string());
                    unsigned short senderPort = senderEndpoint_.port();

                    asio::post(io_context_, [this, message, senderIp,
                                             senderPort]() {
                        notifyMessageHandlers(*message, *senderIp, senderPort);
                    });
                }

                if (isRunning()) {
                    doReceive();
                }
            });
    }

    void notifyMessageHandlers(const std::string& message,
                               const std::string& senderIp,
                               unsigned short senderPort) {
        std::vector<MessageHandler> handlersCopy;
        {
            std::shared_lock<std::shared_mutex> lock(handlersMutex_);
            handlersCopy = handlers_;
        }

        for (const auto& handler : handlersCopy) {
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
            spdlog::error("UDP Socket Error: {} (Code: {}, {})", errorMessage,
                          ec.value(), ec.message());
        } else {
            spdlog::error("UDP Socket Error: {}", errorMessage);
        }

        std::vector<ErrorHandler> handlersCopy;
        {
            std::shared_lock<std::shared_mutex> lock(errorHandlersMutex_);
            handlersCopy = errorHandlers_;
        }

        for (const auto& handler : handlersCopy) {
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
        outgoingThread_ = std::thread([this] {
            while (isRunning()) {
                std::unique_lock<std::mutex> lock(outgoingQueueMutex_);
                outgoingCV_.wait(lock, [this] {
                    return !outgoingQueue_.empty() || !isRunning();
                });

                if (!isRunning() && outgoingQueue_.empty())
                    break;

                if (!outgoingQueue_.empty()) {
                    OutgoingMessage msg = std::move(outgoingQueue_.front());
                    outgoingQueue_.pop();
                    lock.unlock();

                    try {
                        if (msg.isBroadcast) {
                            socket_.set_option(
                                asio::socket_base::broadcast(true));
                        }
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
                        if (msg.isBroadcast) {
                            socket_.set_option(
                                asio::socket_base::broadcast(false));
                        }
                    } catch (const std::system_error& e) {
                        notifyError(
                            fmt::format("Exception while sending message: {}",
                                        e.what()),
                            e.code());
                    }
                }
            }
        });
    }

    asio::io_context io_context_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint senderEndpoint_;
    std::vector<char> receiveBuffer_;
    std::size_t receiveBufferSize_;

    std::vector<std::thread> io_threads_;
    std::thread outgoingThread_;
    unsigned int numThreads_;

    std::atomic<bool> running_;

    mutable std::shared_mutex handlersMutex_;
    std::vector<MessageHandler> handlers_;

    mutable std::shared_mutex errorHandlersMutex_;
    std::vector<ErrorHandler> errorHandlers_;

    std::queue<OutgoingMessage> outgoingQueue_;
    std::mutex outgoingQueueMutex_;
    std::condition_variable outgoingCV_;

    mutable std::shared_mutex multicastMutex_;
    std::set<std::string> multicastGroups_;

    mutable std::shared_mutex ipFilterMutex_;
    std::set<asio::ip::address> allowedIps_;
    std::atomic<bool> ipFilterEnabled_;

    Statistics stats_;
};

// UdpSocketHub implementation
UdpSocketHub::UdpSocketHub() : impl_(std::make_unique<Impl>()) {}
UdpSocketHub::UdpSocketHub(unsigned int numThreads)
    : impl_(std::make_unique<Impl>(numThreads)) {}
UdpSocketHub::~UdpSocketHub() = default;

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