#include "async_udpclient.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <asio.hpp>
#include <atomic>
#include <future>
#include <mutex>
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
                spdlog::error("Unhandled exception in I/O context: {}",
                              e.what());
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
                endpoint =
                    use_ipv6_
                        ? asio::ip::udp::endpoint(asio::ip::udp::v6(), port)
                        : asio::ip::udp::endpoint(asio::ip::udp::v4(), port);
            } else {
                auto addr = asio::ip::address::from_string(address);
                endpoint = asio::ip::udp::endpoint(addr, port);
            }

            socket_ = asio::ip::udp::socket(io_context_);
            socket_.open(endpoint.protocol());
            socket_.bind(endpoint);

            auto status_msg =
                fmt::format("Bound to {}:{}", endpoint.address().to_string(),
                            endpoint.port());
            spdlog::info(status_msg);
            if (onStatusCallback_) {
                onStatusCallback_(status_msg);
            }

            return true;
        } catch (const std::exception& e) {
            auto error_msg = fmt::format("Bind error: {}", e.what());
            spdlog::error(error_msg);
            if (onErrorCallback_) {
                onErrorCallback_(error_msg, -1);
            }
            return false;
        }
    }

    bool send(const std::string& host, int port,
              const std::vector<char>& data) {
        try {
            asio::ip::udp::resolver resolver(io_context_);
            asio::ip::udp::endpoint destination;

            if (host == "255.255.255.255") {
                destination = asio::ip::udp::endpoint(
                    asio::ip::address_v4::broadcast(), port);
            } else {
                destination =
                    *resolver.resolve(host, std::to_string(port)).begin();
            }

            if (!socket_.is_open()) {
                socket_.open(use_ipv6_ ? asio::ip::udp::v6()
                                       : asio::ip::udp::v4());
            }

            std::size_t sent = socket_.send_to(asio::buffer(data), destination);

            stats_.packets_sent.fetch_add(1, std::memory_order_relaxed);
            stats_.bytes_sent.fetch_add(sent, std::memory_order_relaxed);

            auto status_msg =
                fmt::format("Sent {} bytes to {}:{}", sent, host, port);
            spdlog::debug(status_msg);
            if (onStatusCallback_) {
                onStatusCallback_(status_msg);
            }

            return true;
        } catch (const std::exception& e) {
            auto error_msg = fmt::format("Send error: {}", e.what());
            spdlog::error(error_msg);
            if (onErrorCallback_) {
                onErrorCallback_(error_msg, -2);
            }
            return false;
        }
    }

    bool send(const std::string& host, int port, const std::string& data) {
        return send(host, port, std::vector<char>(data.begin(), data.end()));
    }

    bool sendWithTimeout(const std::string& host, int port,
                         const std::vector<char>& data,
                         std::chrono::milliseconds timeout) {
        auto promise = std::make_shared<std::promise<bool>>();
        auto future = promise->get_future();

        asio::post(io_context_, [this, host, port, data, promise]() {
            promise->set_value(send(host, port, data));
        });

        if (future.wait_for(timeout) == std::future_status::timeout) {
            spdlog::warn("Send operation to {}:{} timed out", host, port);
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

            if (!socket_.is_open()) {
                socket_.open(use_ipv6_ ? asio::ip::udp::v6()
                                       : asio::ip::udp::v4());
            }

            if (timeout.count() > 0) {
                socket_.non_blocking(true);
                asio::error_code ec;
                std::size_t received = 0;
                auto start = std::chrono::steady_clock::now();
                while (std::chrono::steady_clock::now() < start + timeout) {
                    received = socket_.receive_from(asio::buffer(data),
                                                    senderEndpoint, 0, ec);
                    if (ec != asio::error::would_block)
                        break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                socket_.non_blocking(false);

                if (ec && ec != asio::error::would_block)
                    throw std::system_error(ec);
                if (ec == asio::error::would_block) {
                    spdlog::warn("Receive operation timed out");
                    if (onErrorCallback_)
                        onErrorCallback_("Receive operation timed out", -4);
                    return {};
                }
                data.resize(received);
            } else {
                std::size_t received =
                    socket_.receive_from(asio::buffer(data), senderEndpoint);
                data.resize(received);
            }

            remoteHost = senderEndpoint.address().to_string();
            remotePort = senderEndpoint.port();

            stats_.packets_received.fetch_add(1, std::memory_order_relaxed);
            stats_.bytes_received.fetch_add(data.size(),
                                            std::memory_order_relaxed);

            auto status_msg = fmt::format("Received {} bytes from {}:{}",
                                          data.size(), remoteHost, remotePort);
            spdlog::debug(status_msg);
            if (onStatusCallback_)
                onStatusCallback_(status_msg);

            return data;
        } catch (const std::exception& e) {
            auto error_msg = fmt::format("Receive error: {}", e.what());
            spdlog::error(error_msg);
            if (onErrorCallback_)
                onErrorCallback_(error_msg, -5);
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
        if (is_receiving_.exchange(true))
            return;

        if (!socket_.is_open()) {
            spdlog::error("Cannot start receiving: Socket not open");
            if (onErrorCallback_)
                onErrorCallback_("Cannot start receiving: Socket not open", -6);
            is_receiving_ = false;
            return;
        }

        receive_buffer_.resize(bufferSize);
        spdlog::info("Started asynchronous receiving");
        if (onStatusCallback_)
            onStatusCallback_("Started asynchronous receiving");

        doReceive();
    }

    void stopReceiving() {
        if (!is_receiving_.exchange(false))
            return;

        spdlog::info("Stopped asynchronous receiving");
        if (onStatusCallback_)
            onStatusCallback_("Stopped asynchronous receiving");
    }

    bool setSocketOption(SocketOption option, int value) {
        try {
            if (!socket_.is_open()) {
                spdlog::error("Cannot set socket option: Socket not open");
                if (onErrorCallback_)
                    onErrorCallback_(
                        "Cannot set socket option: Socket not open", -7);
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
                default:
                    spdlog::warn("Unsupported socket option: {}",
                                 static_cast<int>(option));
                    if (onErrorCallback_)
                        onErrorCallback_(
                            "Unsupported or read-only socket option", -8);
                    return false;
            }

            auto status_msg = fmt::format("Socket option set: {} = {}",
                                          static_cast<int>(option), value);
            spdlog::info(status_msg);
            if (onStatusCallback_)
                onStatusCallback_(status_msg);

            return true;
        } catch (const std::exception& e) {
            auto error_msg =
                fmt::format("Error setting socket option: {}", e.what());
            spdlog::error(error_msg);
            if (onErrorCallback_)
                onErrorCallback_(error_msg, -9);
            return false;
        }
    }

    bool setTTL(int ttl) {
        try {
            if (!socket_.is_open()) {
                spdlog::error("Cannot set TTL: Socket not open");
                if (onErrorCallback_)
                    onErrorCallback_("Cannot set TTL: Socket not open", -10);
                return false;
            }
            socket_.set_option(asio::ip::unicast::hops(ttl));
            auto status_msg = fmt::format("TTL set to {}", ttl);
            spdlog::info(status_msg);
            if (onStatusCallback_)
                onStatusCallback_(status_msg);
            return true;
        } catch (const std::exception& e) {
            auto error_msg = fmt::format("Error setting TTL: {}", e.what());
            spdlog::error(error_msg);
            if (onErrorCallback_)
                onErrorCallback_(error_msg, -11);
            return false;
        }
    }

    bool joinMulticastGroup(const std::string& multicastAddress,
                            const std::string& interfaceAddress) {
        try {
            if (!socket_.is_open()) {
                spdlog::error("Cannot join multicast group: Socket not open");
                if (onErrorCallback_)
                    onErrorCallback_(
                        "Cannot join multicast group: Socket not open", -12);
                return false;
            }
            auto multicast = asio::ip::address::from_string(multicastAddress);
            if (!multicast.is_multicast()) {
                auto error_msg = fmt::format("Not a multicast address: {}",
                                             multicastAddress);
                spdlog::error(error_msg);
                if (onErrorCallback_)
                    onErrorCallback_(error_msg, -13);
                return false;
            }

            if (multicast.is_v6()) {
                socket_.set_option(
                    asio::ip::multicast::join_group(multicast.to_v6()));
            } else {
                socket_.set_option(
                    asio::ip::multicast::join_group(multicast.to_v4()));
            }

            {
                std::lock_guard<std::mutex> lock(multicast_mutex_);
                joined_multicast_groups_[multicastAddress] = interfaceAddress;
            }

            auto status_msg =
                fmt::format("Joined multicast group: {}", multicastAddress);
            spdlog::info(status_msg);
            if (onStatusCallback_)
                onStatusCallback_(status_msg);
            return true;
        } catch (const std::exception& e) {
            auto error_msg =
                fmt::format("Error joining multicast group: {}", e.what());
            spdlog::error(error_msg);
            if (onErrorCallback_)
                onErrorCallback_(error_msg, -14);
            return false;
        }
    }

    bool leaveMulticastGroup(const std::string& multicastAddress,
                             const std::string& interfaceAddress) {
        try {
            if (!socket_.is_open()) {
                spdlog::error("Cannot leave multicast group: Socket not open");
                if (onErrorCallback_)
                    onErrorCallback_(
                        "Cannot leave multicast group: Socket not open", -15);
                return false;
            }
            auto multicast = asio::ip::address::from_string(multicastAddress);
            if (!multicast.is_multicast()) {
                auto error_msg = fmt::format("Not a multicast address: {}",
                                             multicastAddress);
                spdlog::error(error_msg);
                if (onErrorCallback_)
                    onErrorCallback_(error_msg, -16);
                return false;
            }

            if (multicast.is_v6()) {
                socket_.set_option(
                    asio::ip::multicast::leave_group(multicast.to_v6()));
            } else {
                socket_.set_option(
                    asio::ip::multicast::leave_group(multicast.to_v4()));
            }

            {
                std::lock_guard<std::mutex> lock(multicast_mutex_);
                joined_multicast_groups_.erase(multicastAddress);
            }

            auto status_msg =
                fmt::format("Left multicast group: {}", multicastAddress);
            spdlog::info(status_msg);
            if (onStatusCallback_)
                onStatusCallback_(status_msg);
            return true;
        } catch (const std::exception& e) {
            auto error_msg =
                fmt::format("Error leaving multicast group: {}", e.what());
            spdlog::error(error_msg);
            if (onErrorCallback_)
                onErrorCallback_(error_msg, -17);
            return false;
        }
    }

    std::pair<std::string, int> getLocalEndpoint() const {
        try {
            if (!socket_.is_open())
                return {"", 0};
            auto endpoint = socket_.local_endpoint();
            return {endpoint.address().to_string(), endpoint.port()};
        } catch (const std::exception& e) {
            auto error_msg =
                fmt::format("Error getting local endpoint: {}", e.what());
            spdlog::error(error_msg);
            if (onErrorCallback_)
                onErrorCallback_(error_msg, -18);
            return {"", 0};
        }
    }

    bool isOpen() const { return socket_.is_open(); }

    void close() {
        if (!socket_.is_open())
            return;

        is_receiving_ = false;

        std::unordered_map<std::string, std::string> groups_to_leave;
        {
            std::lock_guard<std::mutex> lock(multicast_mutex_);
            groups_to_leave = joined_multicast_groups_;
        }

        for (const auto& [group, interface_addr] : groups_to_leave) {
            leaveMulticastGroup(group, interface_addr);
        }

        {
            std::lock_guard<std::mutex> lock(multicast_mutex_);
            joined_multicast_groups_.clear();
        }

        try {
            asio::error_code ec;
            [[maybe_unused]] auto res = socket_.close(ec);
            if (ec) {
                spdlog::error("Error closing socket: {}", ec.message());
                if (onErrorCallback_)
                    onErrorCallback_(
                        std::string("Error closing socket: ") + ec.message(),
                        -19);
            } else {
                spdlog::info("Socket closed");
                if (onStatusCallback_)
                    onStatusCallback_("Socket closed");
            }
        } catch (const std::exception& e) {
            auto error_msg =
                fmt::format("Exception closing socket: {}", e.what());
            spdlog::error(error_msg);
            if (onErrorCallback_)
                onErrorCallback_(error_msg, -19);
        }
    }

    Statistics getStatistics() const { return stats_; }

    void resetStatistics() {
        stats_.reset();
        spdlog::info("Statistics reset");
        if (onStatusCallback_)
            onStatusCallback_("Statistics reset");
    }

private:
    void doReceive() {
        if (!is_receiving_.load(std::memory_order_relaxed) ||
            !socket_.is_open())
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

                        stats_.packets_received.fetch_add(
                            1, std::memory_order_relaxed);
                        stats_.bytes_received.fetch_add(
                            bytes_recvd, std::memory_order_relaxed);

                        onDataReceivedCallback_(data, remote_host, remote_port);

                        if (onStatusCallback_) {
                            onStatusCallback_(fmt::format(
                                "Async received {} bytes from {}:{}",
                                bytes_recvd, remote_host, remote_port));
                        }
                    }
                    doReceive();
                } else if (ec) {
                    if (is_receiving_.load(std::memory_order_relaxed) &&
                        ec != asio::error::operation_aborted) {
                        auto error_msg = fmt::format("Async receive error: {}",
                                                     ec.message());
                        spdlog::error(error_msg);
                        if (onErrorCallback_)
                            onErrorCallback_(error_msg, ec.value());

                        // Optional: Decide if we should stop or retry on error
                        // For now, we stop to avoid potential tight loop on
                        // persistent errors.
                        is_receiving_ = false;
                    }
                } else {
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
    bool use_ipv6_;

    OnDataReceivedCallback onDataReceivedCallback_;
    OnErrorCallback onErrorCallback_;
    OnStatusCallback onStatusCallback_;

    Statistics stats_;

    mutable std::mutex multicast_mutex_;
    std::unordered_map<std::string, std::string> joined_multicast_groups_;
};

// Main class implementations delegating to Impl
UdpClient::UdpClient() : impl_(std::make_unique<Impl>()) {}
UdpClient::UdpClient(bool use_ipv6) : impl_(std::make_unique<Impl>(use_ipv6)) {}
UdpClient::~UdpClient() = default;
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