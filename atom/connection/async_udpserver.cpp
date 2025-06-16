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

#include <algorithm>
#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <set>
#include <thread>

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
          numThreads_(numThreads),
          ipFilterEnabled_(false) {
        resetStatistics();
    }

    ~Impl() { stop(); }

    bool start(unsigned short port, bool ipv6) {
        if (running_) {
            return false;  // Already running
        }

        try {
            auto protocol = ipv6 ? asio::ip::udp::v6() : asio::ip::udp::v4();
            asio::ip::udp::endpoint endpoint(protocol, port);

            socket_.open(endpoint.protocol());

            // Set reuse address option to avoid "address already in use" errors
            socket_.set_option(asio::ip::udp::socket::reuse_address(true));

            socket_.bind(endpoint);

            // Resize the receive buffer
            receiveBuffer_.resize(receiveBufferSize_);

            running_ = true;
            doReceive();

            // Start the worker threads
            for (unsigned int i = 0; i < numThreads_; ++i) {
                io_threads_.emplace_back([this] {
                    try {
                        io_context_.run();
                    } catch (const std::exception& e) {
                        notifyError("IO Context exception: " +
                                    std::string(e.what()));
                    }
                });
            }

            // Start the outgoing message worker
            startOutgoingMessageWorker();

            return true;
        } catch (const std::exception& e) {
            notifyError("Failed to start UDP server: " + std::string(e.what()));
            stop();
            return false;
        }
    }

    void stop() {
        if (!running_) {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }

        try {
            socket_.close();
        } catch (const std::exception& e) {
            // Just log the error and continue shutting down
            std::cerr << "Error closing socket: " << e.what() << std::endl;
        }

        io_context_.stop();

        // Signal the outgoing message worker to stop
        outgoingCV_.notify_all();

        // Wait for all threads to finish
        for (auto& thread : io_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        io_threads_.clear();

        // Wait for the outgoing message worker to finish
        if (outgoingThread_.joinable()) {
            outgoingThread_.join();
        }

        // Reset IO context for potential restart
        io_context_.restart();
    }

    [[nodiscard]] auto isRunning() const -> bool {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

    void addMessageHandler(MessageHandler handler) {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        handlers_.push_back(std::move(handler));
    }

    void removeMessageHandler(MessageHandler handler) {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        handlers_.erase(
            std::remove_if(
                handlers_.begin(), handlers_.end(),
                [&](const MessageHandler& handlerToRemove) {
                    return handler.target<void(const std::string&,
                                               const std::string&,
                                               unsigned short)>() ==
                           handlerToRemove.target<void(const std::string&,
                                                       const std::string&,
                                                       unsigned short)>();
                }),
            handlers_.end());
    }

    void addErrorHandler(ErrorHandler handler) {
        std::lock_guard<std::mutex> lock(errorHandlersMutex_);
        errorHandlers_.push_back(std::move(handler));
    }

    void removeErrorHandler(ErrorHandler handler) {
        std::lock_guard<std::mutex> lock(errorHandlersMutex_);
        errorHandlers_.erase(
            std::remove_if(
                errorHandlers_.begin(), errorHandlers_.end(),
                [&](const ErrorHandler& handlerToRemove) {
                    return handler.target<void(const std::string&,
                                               const std::error_code&)>() ==
                           handlerToRemove.target<void(
                               const std::string&, const std::error_code&)>();
                }),
            errorHandlers_.end());
    }

    bool sendTo(const std::string& message, const std::string& ipAddress,
                unsigned short port) {
        if (!isRunning()) {
            notifyError("Cannot send message: Server is not running", {});
            return false;
        }

        try {
            // Create a message info object
            OutgoingMessage msg;
            msg.message = message;
            msg.endpoint = asio::ip::udp::endpoint(
                asio::ip::make_address(ipAddress), port);
            msg.isBroadcast = false;

            // Queue the message for sending
            return queueOutgoingMessage(std::move(msg));
        } catch (const std::exception& e) {
            notifyError("Failed to prepare message for sending: " +
                        std::string(e.what()));
            return false;
        }
    }

    bool broadcast(const std::string& message, unsigned short port) {
        if (!isRunning()) {
            notifyError("Cannot broadcast message: Server is not running", {});
            return false;
        }

        try {
            // Enable broadcast permission
            socket_.set_option(asio::socket_base::broadcast(true));

            // Create a message info object
            OutgoingMessage msg;
            msg.message = message;
            msg.endpoint = asio::ip::udp::endpoint(
                asio::ip::address_v4::broadcast(), port);
            msg.isBroadcast = true;

            // Queue the message for sending
            return queueOutgoingMessage(std::move(msg));
        } catch (const std::exception& e) {
            notifyError("Failed to prepare broadcast message: " +
                        std::string(e.what()));
            return false;
        }
    }

    bool joinMulticastGroup(const std::string& multicastAddress) {
        if (!isRunning()) {
            notifyError("Cannot join multicast group: Server is not running",
                        {});
            return false;
        }

        try {
            auto multicastAddr = asio::ip::make_address(multicastAddress);

            // Check if it's a valid multicast address
            if (!multicastAddr.is_multicast()) {
                notifyError("Invalid multicast address: " + multicastAddress,
                            {});
                return false;
            }

            // Join the multicast group
            if (multicastAddr.is_v4()) {
                socket_.set_option(
                    asio::ip::multicast::join_group(multicastAddr.to_v4()));
            } else {
                // For IPv6, we'd need to specify the interface index
                // This is a simplified implementation
                socket_.set_option(
                    asio::ip::multicast::join_group(multicastAddr.to_v6()));
            }

            std::lock_guard<std::mutex> lock(multicastMutex_);
            multicastGroups_.insert(multicastAddress);
            return true;
        } catch (const std::exception& e) {
            notifyError("Failed to join multicast group: " +
                        std::string(e.what()));
            return false;
        }
    }

    bool leaveMulticastGroup(const std::string& multicastAddress) {
        if (!isRunning()) {
            notifyError("Cannot leave multicast group: Server is not running",
                        {});
            return false;
        }

        try {
            auto multicastAddr = asio::ip::make_address(multicastAddress);

            // Check if it's a valid multicast address
            if (!multicastAddr.is_multicast()) {
                notifyError("Invalid multicast address: " + multicastAddress,
                            {});
                return false;
            }

            // Leave the multicast group
            if (multicastAddr.is_v4()) {
                socket_.set_option(
                    asio::ip::multicast::leave_group(multicastAddr.to_v4()));
            } else {
                socket_.set_option(
                    asio::ip::multicast::leave_group(multicastAddr.to_v6()));
            }

            std::lock_guard<std::mutex> lock(multicastMutex_);
            multicastGroups_.erase(multicastAddress);
            return true;
        } catch (const std::exception& e) {
            notifyError("Failed to leave multicast group: " +
                        std::string(e.what()));
            return false;
        }
    }

    bool sendToMulticast(const std::string& message,
                         const std::string& multicastAddress,
                         unsigned short port) {
        if (!isRunning()) {
            notifyError("Cannot send multicast message: Server is not running",
                        {});
            return false;
        }

        try {
            auto multicastAddr = asio::ip::make_address(multicastAddress);

            // Check if it's a valid multicast address
            if (!multicastAddr.is_multicast()) {
                notifyError("Invalid multicast address: " + multicastAddress,
                            {});
                return false;
            }

            // Create a message info object
            OutgoingMessage msg;
            msg.message = message;
            msg.endpoint = asio::ip::udp::endpoint(multicastAddr, port);
            msg.isBroadcast = false;  // Multicast is not broadcast

            // Set TTL (Time To Live) for multicast
            socket_.set_option(asio::ip::multicast::hops(1));

            // Queue the message for sending
            return queueOutgoingMessage(std::move(msg));
        } catch (const std::exception& e) {
            notifyError("Failed to prepare multicast message: " +
                        std::string(e.what()));
            return false;
        }
    }

    template <typename T>
    bool setSocketOption(SocketOption option, const T& value) {
        if (!isRunning()) {
            notifyError("Cannot set socket option: Server is not running", {});
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
                case SocketOption::ReceiveTimeout:
                    // Use deadline_timer or steady_timer for timeouts instead
                    // This version just logs that timeout options aren't
                    // directly supported
                    notifyError(
                        "ReceiveTimeout option not directly supported in Asio. "
                        "Use async operations with timers instead.");
                    return false;
                    break;
                case SocketOption::SendTimeout:
                    // Use deadline_timer or steady_timer for timeouts instead
                    // This version just logs that timeout options aren't
                    // directly supported
                    notifyError(
                        "SendTimeout option not directly supported in Asio. "
                        "Use async operations with timers instead.");
                    return false;
                    break;
                    break;
                default:
                    notifyError("Unknown socket option", {});
                    return false;
            }
            return true;
        } catch (const std::exception& e) {
            notifyError("Failed to set socket option: " +
                        std::string(e.what()));
            return false;
        }
    }

    bool setReceiveBufferSize(std::size_t size) {
        if (size == 0) {
            notifyError("Invalid buffer size: 0", {});
            return false;
        }

        receiveBufferSize_ = size;
        receiveBuffer_.resize(size);

        // Also update the socket option
        try {
            socket_.set_option(
                asio::socket_base::receive_buffer_size(static_cast<int>(size)));
            return true;
        } catch (const std::exception& e) {
            notifyError("Failed to set receive buffer size: " +
                        std::string(e.what()));
            return false;
        }
    }

    bool setReceiveTimeout(const std::chrono::milliseconds& timeout) {
        try {
// Use socket-level timeout operation instead
#if defined(ASIO_WINDOWS) || defined(__CYGWIN__)
            // Windows-specific implementation
            DWORD milliseconds = static_cast<DWORD>(timeout.count());
            socket_.set_option(
                asio::detail::socket_option::integer<SOL_SOCKET, SO_RCVTIMEO>(
                    milliseconds));
#else
            // POSIX implementation
            struct timeval tv;
            tv.tv_sec = static_cast<long>(timeout.count() / 1000);
            tv.tv_usec = static_cast<long>((timeout.count() % 1000) * 1000);
            ::setsockopt(socket_.native_handle(), SOL_SOCKET, SO_RCVTIMEO, &tv,
                         sizeof(tv));
#endif
            return true;
        } catch (const std::exception& e) {
            notifyError("Failed to set receive timeout: " +
                        std::string(e.what()));
            return false;
        }
    }

    Statistics getStatistics() const {
        std::lock_guard<std::mutex> lock(statsMutex_);
        return stats_;
    }

    void resetStatistics() {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_ = Statistics{};
    }

    void addAllowedIp(const std::string& ip) {
        try {
            std::lock_guard<std::mutex> lock(ipFilterMutex_);
            auto address = asio::ip::make_address(ip);
            allowedIps_.insert(address);
            ipFilterEnabled_ = true;
        } catch (const std::exception& e) {
            notifyError("Failed to add IP filter: " + std::string(e.what()));
        }
    }

    void removeAllowedIp(const std::string& ip) {
        try {
            std::lock_guard<std::mutex> lock(ipFilterMutex_);
            auto address = asio::ip::make_address(ip);
            allowedIps_.erase(address);
            ipFilterEnabled_ = !allowedIps_.empty();
        } catch (const std::exception& e) {
            notifyError("Failed to remove IP filter: " + std::string(e.what()));
        }
    }

    void clearIpFilters() {
        std::lock_guard<std::mutex> lock(ipFilterMutex_);
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
                        doReceive();  // Continue receiving messages
                    }
                    return;
                }

                if (bytesReceived > 0) {
                    std::string message(receiveBuffer_.data(), bytesReceived);
                    std::string senderIp =
                        senderEndpoint_.address().to_string();
                    unsigned short senderPort = senderEndpoint_.port();

                    // Update statistics
                    {
                        std::lock_guard<std::mutex> lock(statsMutex_);
                        stats_.bytesReceived += bytesReceived;
                        stats_.messagesReceived++;
                    }

                    // Check IP filter if enabled
                    bool allowed = true;
                    if (ipFilterEnabled_) {
                        std::lock_guard<std::mutex> lock(ipFilterMutex_);
                        allowed = allowedIps_.find(senderEndpoint_.address()) !=
                                  allowedIps_.end();
                    }

                    if (allowed) {
                        // Notify handlers on a separate thread to avoid
                        // blocking the IO thread
                        asio::post(io_context_,
                                   [this, message, senderIp, senderPort]() {
                                       notifyMessageHandlers(message, senderIp,
                                                             senderPort);
                                   });
                    }
                }

                // Continue receiving if we're still running
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
            std::lock_guard<std::mutex> lock(handlersMutex_);
            handlersCopy = handlers_;  // Make a copy to avoid holding the lock
                                       // during execution
        }

        for (const auto& handler : handlersCopy) {
            try {
                handler(message, senderIp, senderPort);
            } catch (const std::exception& e) {
                notifyError("Exception in message handler: " +
                            std::string(e.what()));
            }
        }
    }

    void notifyError(const std::string& errorMessage,
                     const std::error_code& ec = std::error_code()) {
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.errors++;
        }

        // Output to stderr for debugging
        std::cerr << "UDP Socket Error: " << errorMessage;
        if (ec) {
            std::cerr << " (Code: " << ec.value() << ", " << ec.message()
                      << ")";
        }
        std::cerr << std::endl;

        std::vector<ErrorHandler> handlersCopy;
        {
            std::lock_guard<std::mutex> lock(errorHandlersMutex_);
            handlersCopy =
                errorHandlers_;  // Make a copy to avoid holding the lock
        }

        for (const auto& handler : handlersCopy) {
            try {
                handler(errorMessage, ec);
            } catch (const std::exception& e) {
                std::cerr << "Exception in error handler: " << e.what()
                          << std::endl;
            }
        }
    }

    bool queueOutgoingMessage(OutgoingMessage&& msg) {
        std::unique_lock<std::mutex> lock(outgoingQueueMutex_);

        // Check if the queue is full
        if (outgoingQueue_.size() >= MAX_QUEUE_SIZE) {
            lock.unlock();
            notifyError("Outgoing message queue is full, message discarded");
            return false;
        }

        outgoingQueue_.push(std::move(msg));
        lock.unlock();

        // Notify the outgoing worker thread
        outgoingCV_.notify_one();
        return true;
    }

    void startOutgoingMessageWorker() {
        outgoingThread_ = std::thread([this] {
            while (true) {
                std::unique_lock<std::mutex> lock(outgoingQueueMutex_);

                // Wait for a message or until we're told to stop
                outgoingCV_.wait(lock, [this] {
                    return !outgoingQueue_.empty() || !running_;
                });

                // If we're shutting down and the queue is empty, exit
                if (!running_ && outgoingQueue_.empty()) {
                    break;
                }

                // Get the next message to send
                OutgoingMessage msg;
                if (!outgoingQueue_.empty()) {
                    msg = std::move(outgoingQueue_.front());
                    outgoingQueue_.pop();
                    lock.unlock();  // Release the lock before sending

                    // Actually send the message
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
                            // Update statistics
                            std::lock_guard<std::mutex> statsLock(statsMutex_);
                            stats_.bytesSent += bytesSent;
                            stats_.messagesSent++;
                        }

                        if (msg.isBroadcast) {
                            socket_.set_option(
                                asio::socket_base::broadcast(false));
                        }
                    } catch (const std::exception& e) {
                        notifyError("Exception while sending message: " +
                                    std::string(e.what()));
                    }
                } else {
                    lock.unlock();
                }
            }
        });
    }

    // ASIO communication members
    asio::io_context io_context_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint senderEndpoint_;
    std::vector<char> receiveBuffer_;
    std::size_t receiveBufferSize_;

    // Thread management
    std::vector<std::thread> io_threads_;
    std::thread outgoingThread_;
    unsigned int numThreads_;

    // State management
    mutable std::mutex mutex_;  // Protects running_ flag
    bool running_;

    // Handler management
    mutable std::mutex handlersMutex_;
    std::vector<MessageHandler> handlers_;

    mutable std::mutex errorHandlersMutex_;
    std::vector<ErrorHandler> errorHandlers_;

    // Outgoing message queue
    std::queue<OutgoingMessage> outgoingQueue_;
    std::mutex outgoingQueueMutex_;
    std::condition_variable outgoingCV_;

    // Multicast groups
    std::mutex multicastMutex_;
    std::set<std::string> multicastGroups_;

    // IP filtering
    std::mutex ipFilterMutex_;
    std::set<asio::ip::address> allowedIps_;
    std::atomic<bool> ipFilterEnabled_;

    // Statistics
    mutable std::mutex statsMutex_;
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

auto UdpSocketHub::isRunning() const -> bool { return impl_->isRunning(); }

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
template bool UdpSocketHub::setSocketOption<bool>(SocketOption option,
                                                  const bool& value);
template bool UdpSocketHub::setSocketOption<int>(SocketOption option,
                                                 const int& value);
template bool UdpSocketHub::setSocketOption<unsigned int>(
    SocketOption option, const unsigned int& value);

}  // namespace atom::async::connection