#include "async_udpclient.hpp"

#include <asio.hpp>
#include <future>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>

namespace atom::async::connection {

class UdpClient::Impl {
public:
    Impl()
        : io_context_(),
          work_guard_(asio::make_work_guard(io_context_)),
          socket_(io_context_),
          is_receiving_(false),
          use_ipv6_(false) {
        startContext();
    }

    Impl(bool use_ipv6)
        : io_context_(),
          work_guard_(asio::make_work_guard(io_context_)),
          socket_(io_context_),
          is_receiving_(false),
          use_ipv6_(use_ipv6) {
        startContext();
    }

    ~Impl() {
        close();
        stopContext();
    }

    void startContext() {
        io_thread_ = std::thread([this]() {
            try {
                io_context_.run();
            } catch (const std::exception& e) {
                if (onErrorCallback_) {
                    onErrorCallback_(e.what(), 0);
                }
            }
        });
    }

    void stopContext() {
        work_guard_.reset();
        io_context_.stop();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
    }

    bool bind(int port, const std::string& address) {
        try {
            close();

            asio::ip::udp::endpoint endpoint;

            if (address.empty()) {
                if (use_ipv6_) {
                    endpoint =
                        asio::ip::udp::endpoint(asio::ip::udp::v6(), port);
                } else {
                    endpoint =
                        asio::ip::udp::endpoint(asio::ip::udp::v4(), port);
                }
            } else {
                auto addr = asio::ip::address::from_string(address);
                endpoint = asio::ip::udp::endpoint(addr, port);
            }

            socket_ = asio::ip::udp::socket(io_context_);
            socket_.open(endpoint.protocol());
            socket_.bind(endpoint);

            if (onStatusCallback_) {
                std::stringstream ss;
                ss << "Bound to " << endpoint.address().to_string() << ":"
                   << endpoint.port();
                onStatusCallback_(ss.str());
            }

            return true;
        } catch (const std::exception& e) {
            if (onErrorCallback_) {
                onErrorCallback_(std::string("Bind error: ") + e.what(), -1);
            }
            return false;
        }
    }

    bool send(const std::string& host, int port,
              const std::vector<char>& data) {
        try {
            // Create resolver and resolve the host
            asio::ip::udp::resolver resolver(io_context_);
            asio::ip::udp::endpoint destination;

            if (host == "255.255.255.255") {
                // Handle broadcast address specially
                destination = asio::ip::udp::endpoint(
                    asio::ip::address_v4::broadcast(), port);
            } else {
                // Regular address resolution
                destination =
                    *resolver.resolve(host, std::to_string(port)).begin();
            }

            // Ensure socket is open
            if (!socket_.is_open()) {
                if (use_ipv6_) {
                    socket_.open(asio::ip::udp::v6());
                } else {
                    socket_.open(asio::ip::udp::v4());
                }
            }

            // Send the data
            std::size_t sent = socket_.send_to(asio::buffer(data), destination);

            // Update statistics
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.packets_sent++;
            stats_.bytes_sent += sent;

            if (onStatusCallback_) {
                std::stringstream ss;
                ss << "Sent " << sent << " bytes to " << host << ":" << port;
                onStatusCallback_(ss.str());
            }

            return true;
        } catch (const std::exception& e) {
            if (onErrorCallback_) {
                onErrorCallback_(std::string("Send error: ") + e.what(), -2);
            }
            return false;
        }
    }

    bool send(const std::string& host, int port, const std::string& data) {
        std::vector<char> data_vec(data.begin(), data.end());
        return send(host, port, data_vec);
    }

    bool sendWithTimeout(const std::string& host, int port,
                         const std::vector<char>& data,
                         std::chrono::milliseconds timeout) {
        // Create a promise and future for async operation
        std::promise<bool> promise;
        auto future = promise.get_future();

        // Post send operation to io_context
        asio::post(io_context_, [this, host, port, data,
                                 promise = std::move(promise)]() mutable {
            bool result = send(host, port, data);
            promise.set_value(result);
        });

        // Wait for operation with timeout
        if (future.wait_for(timeout) == std::future_status::timeout) {
            if (onErrorCallback_) {
                onErrorCallback_("Send operation timed out", -3);
            }
            return false;
        }

        return future.get();
    }

    int batchSend(const std::vector<std::pair<std::string, int>>& destinations,
                  const std::vector<char>& data) {
        int success_count = 0;

        for (const auto& dest : destinations) {
            if (send(dest.first, dest.second, data)) {
                success_count++;
            }
        }

        return success_count;
    }

    std::vector<char> receive(size_t size, std::string& remoteHost,
                              int& remotePort,
                              std::chrono::milliseconds timeout) {
        try {
            std::vector<char> data(size);
            asio::ip::udp::endpoint senderEndpoint;

            // Ensure socket is open
            if (!socket_.is_open()) {
                if (use_ipv6_) {
                    socket_.open(asio::ip::udp::v6());
                } else {
                    socket_.open(asio::ip::udp::v4());
                }
            }

            if (timeout.count() > 0) {
                // Set receive timeout
                socket_.non_blocking(true);

                asio::error_code ec;
                std::size_t received = 0;

                auto start = std::chrono::steady_clock::now();
                auto timeoutPoint = start + timeout;

                // Poll until data received or timeout
                while (std::chrono::steady_clock::now() < timeoutPoint) {
                    received = socket_.receive_from(asio::buffer(data),
                                                    senderEndpoint, 0, ec);

                    if (ec != asio::error::would_block) {
                        break;
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                socket_.non_blocking(false);

                if (ec && ec != asio::error::would_block) {
                    throw std::system_error(ec);
                }

                if (ec == asio::error::would_block) {
                    // Timeout occurred
                    if (onErrorCallback_) {
                        onErrorCallback_("Receive operation timed out", -4);
                    }
                    return {};
                }

                data.resize(received);
            } else {
                // Blocking receive
                std::size_t received =
                    socket_.receive_from(asio::buffer(data), senderEndpoint);
                data.resize(received);
            }

            remoteHost = senderEndpoint.address().to_string();
            remotePort = senderEndpoint.port();

            // Update statistics
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.packets_received++;
            stats_.bytes_received += data.size();

            if (onStatusCallback_) {
                std::stringstream ss;
                ss << "Received " << data.size() << " bytes from " << remoteHost
                   << ":" << remotePort;
                onStatusCallback_(ss.str());
            }

            return data;
        } catch (const std::exception& e) {
            if (onErrorCallback_) {
                onErrorCallback_(std::string("Receive error: ") + e.what(), -5);
            }
            return {};
        }
    }

    void setOnDataReceivedCallback(const OnDataReceivedCallback& callback) {
        onDataReceivedCallback_ = callback;
    }

    void setOnErrorCallback(const OnErrorCallback& callback) {
        onErrorCallback_ = callback;
    }

    void setOnStatusCallback(const OnStatusCallback& callback) {
        onStatusCallback_ = callback;
    }

    void startReceiving(size_t bufferSize) {
        std::lock_guard<std::mutex> lock(receive_mutex_);

        if (is_receiving_) {
            return;
        }

        if (!socket_.is_open()) {
            if (onErrorCallback_) {
                onErrorCallback_("Cannot start receiving: Socket not open", -6);
                return;
            }
        }

        is_receiving_ = true;
        receive_buffer_.resize(bufferSize);

        if (onStatusCallback_) {
            onStatusCallback_("Started asynchronous receiving");
        }

        doReceive();
    }

    void stopReceiving() {
        std::lock_guard<std::mutex> lock(receive_mutex_);
        is_receiving_ = false;

        if (onStatusCallback_) {
            onStatusCallback_("Stopped asynchronous receiving");
        }
    }

    bool setSocketOption(SocketOption option, int value) {
        try {
            if (!socket_.is_open()) {
                if (onErrorCallback_) {
                    onErrorCallback_(
                        "Cannot set socket option: Socket not open", -7);
                }
                return false;
            }

            switch (option) {
                case SocketOption::Broadcast:
                    socket_.set_option(
                        asio::socket_base::broadcast(value != 0));
                    break;

                case SocketOption::ReuseAddress:
                    socket_.set_option(
                        asio::socket_base::reuse_address(value != 0));
                    break;

                case SocketOption::ReceiveBufferSize:
                    socket_.set_option(
                        asio::socket_base::receive_buffer_size(value));
                    break;

                case SocketOption::SendBufferSize:
                    socket_.set_option(
                        asio::socket_base::send_buffer_size(value));
                    break;

                case SocketOption::ReceiveTimeout:
                    if (onErrorCallback_) {
                        onErrorCallback_(
                            "Receive timeout not supported, use receive with "
                            "timeout parameter instead",
                            -8);
                    }
                    break;

                case SocketOption::SendTimeout:
                    if (onErrorCallback_) {
                        onErrorCallback_(
                            "Send timeout not supported, use sendWithTimeout "
                            "instead",
                            -8);
                    }
                    break;

                default:
                    if (onErrorCallback_) {
                        onErrorCallback_("Unknown socket option", -8);
                    }
                    return false;
            }

            if (onStatusCallback_) {
                std::stringstream ss;
                ss << "Socket option set: " << static_cast<int>(option) << " = "
                   << value;
                onStatusCallback_(ss.str());
            }

            return true;
        } catch (const std::exception& e) {
            if (onErrorCallback_) {
                onErrorCallback_(
                    std::string("Error setting socket option: ") + e.what(),
                    -9);
            }
            return false;
        }
    }

    bool setTTL(int ttl) {
        try {
            if (!socket_.is_open()) {
                if (onErrorCallback_) {
                    onErrorCallback_("Cannot set TTL: Socket not open", -10);
                }
                return false;
            }

            socket_.set_option(asio::ip::unicast::hops(ttl));

            if (onStatusCallback_) {
                std::stringstream ss;
                ss << "TTL set to " << ttl;
                onStatusCallback_(ss.str());
            }

            return true;
        } catch (const std::exception& e) {
            if (onErrorCallback_) {
                onErrorCallback_(std::string("Error setting TTL: ") + e.what(),
                                 -11);
            }
            return false;
        }
    }

    bool joinMulticastGroup(const std::string& multicastAddress,
                            const std::string& interfaceAddress) {
        try {
            if (!socket_.is_open()) {
                if (onErrorCallback_) {
                    onErrorCallback_(
                        "Cannot join multicast group: Socket not open", -12);
                }
                return false;
            }

            auto multicast = asio::ip::address::from_string(multicastAddress);

            if (!multicast.is_multicast()) {
                if (onErrorCallback_) {
                    onErrorCallback_(
                        "Not a multicast address: " + multicastAddress, -13);
                }
                return false;
            }

            if (multicast.is_v6()) {
                asio::ip::multicast::join_group option;

                if (!interfaceAddress.empty()) {
                    auto interface_addr =
                        asio::ip::address_v6::from_string(interfaceAddress);
                    option = asio::ip::multicast::join_group(
                        multicast.to_v6(), interface_addr.to_bytes()[0]);
                } else {
                    option = asio::ip::multicast::join_group(multicast.to_v6());
                }

                socket_.set_option(option);
            } else {
                asio::ip::multicast::join_group option;

                if (!interfaceAddress.empty()) {
                    auto interface_addr =
                        asio::ip::address_v4::from_string(interfaceAddress);
                    option = asio::ip::multicast::join_group(multicast.to_v4(),
                                                             interface_addr);
                } else {
                    option = asio::ip::multicast::join_group(multicast.to_v4());
                }

                socket_.set_option(option);
            }

            // Record joined group for later
            joined_multicast_groups_[multicastAddress] = interfaceAddress;

            if (onStatusCallback_) {
                std::stringstream ss;
                ss << "Joined multicast group: " << multicastAddress;
                if (!interfaceAddress.empty()) {
                    ss << " on interface " << interfaceAddress;
                }
                onStatusCallback_(ss.str());
            }

            return true;
        } catch (const std::exception& e) {
            if (onErrorCallback_) {
                onErrorCallback_(
                    std::string("Error joining multicast group: ") + e.what(),
                    -14);
            }
            return false;
        }
    }

    bool leaveMulticastGroup(const std::string& multicastAddress,
                             const std::string& interfaceAddress) {
        try {
            if (!socket_.is_open()) {
                if (onErrorCallback_) {
                    onErrorCallback_(
                        "Cannot leave multicast group: Socket not open", -15);
                }
                return false;
            }

            auto multicast = asio::ip::address::from_string(multicastAddress);

            if (!multicast.is_multicast()) {
                if (onErrorCallback_) {
                    onErrorCallback_(
                        "Not a multicast address: " + multicastAddress, -16);
                }
                return false;
            }

            if (multicast.is_v6()) {
                asio::ip::multicast::leave_group option;

                if (!interfaceAddress.empty()) {
                    auto interface_addr =
                        asio::ip::address_v6::from_string(interfaceAddress);
                    option = asio::ip::multicast::leave_group(
                        multicast.to_v6(), interface_addr.to_bytes()[0]);
                } else {
                    option =
                        asio::ip::multicast::leave_group(multicast.to_v6());
                }

                socket_.set_option(option);
            } else {
                asio::ip::multicast::leave_group option;

                if (!interfaceAddress.empty()) {
                    auto interface_addr =
                        asio::ip::address_v4::from_string(interfaceAddress);
                    option = asio::ip::multicast::leave_group(multicast.to_v4(),
                                                              interface_addr);
                } else {
                    option =
                        asio::ip::multicast::leave_group(multicast.to_v4());
                }

                socket_.set_option(option);
            }

            // Remove from joined groups
            joined_multicast_groups_.erase(multicastAddress);

            if (onStatusCallback_) {
                std::stringstream ss;
                ss << "Left multicast group: " << multicastAddress;
                if (!interfaceAddress.empty()) {
                    ss << " on interface " << interfaceAddress;
                }
                onStatusCallback_(ss.str());
            }

            return true;
        } catch (const std::exception& e) {
            if (onErrorCallback_) {
                onErrorCallback_(
                    std::string("Error leaving multicast group: ") + e.what(),
                    -17);
            }
            return false;
        }
    }

    std::pair<std::string, int> getLocalEndpoint() const {
        try {
            if (!socket_.is_open()) {
                return {"", 0};
            }

            auto endpoint = socket_.local_endpoint();
            return {endpoint.address().to_string(), endpoint.port()};
        } catch (const std::exception& e) {
            if (onErrorCallback_) {
                onErrorCallback_(
                    std::string("Error getting local endpoint: ") + e.what(),
                    -18);
            }
            return {"", 0};
        }
    }

    bool isOpen() const { return socket_.is_open(); }

    void close() {
        std::lock_guard<std::mutex> lock(receive_mutex_);

        if (!socket_.is_open()) {
            return;
        }

        is_receiving_ = false;

        // Leave any multicast groups we've joined
        for (const auto& [group, interface_addr] : joined_multicast_groups_) {
            try {
                leaveMulticastGroup(group, interface_addr);
            } catch (...) {
                // Ignore errors during cleanup
            }
        }

        joined_multicast_groups_.clear();

        try {
            socket_.close();

            if (onStatusCallback_) {
                onStatusCallback_("Socket closed");
            }
        } catch (const std::exception& e) {
            if (onErrorCallback_) {
                onErrorCallback_(
                    std::string("Error closing socket: ") + e.what(), -19);
            }
        }
    }

    Statistics getStatistics() const {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_;
    }

    void resetStatistics() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.reset();

        if (onStatusCallback_) {
            onStatusCallback_("Statistics reset");
        }
    }

private:
    void doReceive() {
        if (!is_receiving_ || !socket_.is_open())
            return;

        socket_.async_receive_from(
            asio::buffer(receive_buffer_), remote_endpoint_,
            [this](std::error_code ec, std::size_t bytes_recvd) {
                if (!ec && bytes_recvd > 0) {
                    if (onDataReceivedCallback_) {
                        auto data = std::vector<char>(
                            receive_buffer_.begin(),
                            receive_buffer_.begin() + bytes_recvd);

                        std::string remote_host =
                            remote_endpoint_.address().to_string();
                        int remote_port = remote_endpoint_.port();

                        // Update statistics
                        {
                            std::lock_guard<std::mutex> lock(stats_mutex_);
                            stats_.packets_received++;
                            stats_.bytes_received += bytes_recvd;
                        }

                        // Invoke callback
                        onDataReceivedCallback_(data, remote_host, remote_port);

                        if (onStatusCallback_) {
                            std::stringstream ss;
                            ss << "Async received " << bytes_recvd
                               << " bytes from " << remote_host << ":"
                               << remote_port;
                            onStatusCallback_(ss.str());
                        }
                    }

                    // Continue receiving if still active
                    doReceive();
                } else if (ec) {
                    // Only report error if we're still in receiving state and
                    // not due to closed socket
                    if (is_receiving_ && ec != asio::error::operation_aborted) {
                        if (onErrorCallback_) {
                            onErrorCallback_(
                                std::string("Async receive error: ") +
                                    ec.message(),
                                ec.value());
                        }

                        // Try to restart receiving after a short delay
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(100));
                        doReceive();
                    }
                } else {
                    // Zero bytes received but no error - continue receiving
                    doReceive();
                }
            });
    }

    asio::io_context io_context_;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint remote_endpoint_;
    std::vector<char> receive_buffer_;
    std::thread io_thread_;
    std::atomic<bool> is_receiving_;

    OnDataReceivedCallback onDataReceivedCallback_;
    OnErrorCallback onErrorCallback_;
    OnStatusCallback onStatusCallback_;

    mutable std::mutex stats_mutex_;
    Statistics stats_;

    std::mutex receive_mutex_;
    bool use_ipv6_;

    // Track joined multicast groups for proper cleanup
    std::unordered_map<std::string, std::string> joined_multicast_groups_;
};

// Main class implementations delegating to Impl

UdpClient::UdpClient() : impl_(std::make_unique<Impl>()) {}

UdpClient::UdpClient(bool use_ipv6) : impl_(std::make_unique<Impl>(use_ipv6)) {}

UdpClient::~UdpClient() = default;

// Move operations
UdpClient::UdpClient(UdpClient&&) noexcept = default;
UdpClient& UdpClient::operator=(UdpClient&&) noexcept = default;

bool UdpClient::bind(int port, const std::string& address) {
    return impl_->bind(port, address);
}

bool UdpClient::send(const std::string& host, int port,
                     const std::vector<char>& data) {
    return impl_->send(host, port, data);
}

bool UdpClient::send(const std::string& host, int port,
                     const std::string& data) {
    return impl_->send(host, port, data);
}

bool UdpClient::sendWithTimeout(const std::string& host, int port,
                                const std::vector<char>& data,
                                std::chrono::milliseconds timeout) {
    return impl_->sendWithTimeout(host, port, data, timeout);
}

int UdpClient::batchSend(
    const std::vector<std::pair<std::string, int>>& destinations,
    const std::vector<char>& data) {
    return impl_->batchSend(destinations, data);
}

std::vector<char> UdpClient::receive(size_t size, std::string& remoteHost,
                                     int& remotePort,
                                     std::chrono::milliseconds timeout) {
    return impl_->receive(size, remoteHost, remotePort, timeout);
}

void UdpClient::setOnDataReceivedCallback(
    const OnDataReceivedCallback& callback) {
    impl_->setOnDataReceivedCallback(callback);
}

void UdpClient::setOnErrorCallback(const OnErrorCallback& callback) {
    impl_->setOnErrorCallback(callback);
}

void UdpClient::setOnStatusCallback(const OnStatusCallback& callback) {
    impl_->setOnStatusCallback(callback);
}

void UdpClient::startReceiving(size_t bufferSize) {
    impl_->startReceiving(bufferSize);
}

void UdpClient::stopReceiving() { impl_->stopReceiving(); }

bool UdpClient::setSocketOption(SocketOption option, int value) {
    return impl_->setSocketOption(option, value);
}

bool UdpClient::setTTL(int ttl) { return impl_->setTTL(ttl); }

bool UdpClient::joinMulticastGroup(const std::string& multicastAddress,
                                   const std::string& interfaceAddress) {
    return impl_->joinMulticastGroup(multicastAddress, interfaceAddress);
}

bool UdpClient::leaveMulticastGroup(const std::string& multicastAddress,
                                    const std::string& interfaceAddress) {
    return impl_->leaveMulticastGroup(multicastAddress, interfaceAddress);
}

std::pair<std::string, int> UdpClient::getLocalEndpoint() const {
    return impl_->getLocalEndpoint();
}

bool UdpClient::isOpen() const { return impl_->isOpen(); }

void UdpClient::close() { impl_->close(); }

UdpClient::Statistics UdpClient::getStatistics() const {
    return impl_->getStatistics();
}

void UdpClient::resetStatistics() { impl_->resetStatistics(); }

}  // namespace atom::async::connection