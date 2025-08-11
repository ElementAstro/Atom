#include "client.hpp"
#include "packet.hpp"

#include <algorithm>
#include <chrono>

namespace mqtt {

Client::Client(bool auto_start_io) : gen_(rd_()) {
    keep_alive_timer_ = std::make_unique<asio::steady_timer>(io_context_);
    ping_timeout_timer_ = std::make_unique<asio::steady_timer>(io_context_);
    reconnect_timer_ = std::make_unique<asio::steady_timer>(io_context_);

    reset_stats();

    if (auto_start_io) {
        start_io_thread();
    }
}

Client::~Client() {
    disconnect();
    stop_io_thread();
}

void Client::async_connect(const std::string& host, uint16_t port,
                           const ConnectionOptions& options,
                           ConnectionHandler callback) {
    if (state_.load() != ConnectionState::DISCONNECTED) {
        if (callback) {
            asio::post(io_context_,
                       [callback]() { callback(ErrorCode::PROTOCOL_ERROR); });
        }
        return;
    }

    broker_host_ = host;
    broker_port_ = port;
    connection_options_ = options;
    connection_handler_ = std::move(callback);

    // Generate client ID if not provided
    if (connection_options_.client_id.empty()) {
        connection_options_.client_id = generate_client_id();
    }

    state_.store(ConnectionState::CONNECTING);

    asio::post(io_context_, [this]() { perform_connect(); });
}

void Client::disconnect(ErrorCode reason) {
    if (state_.load() == ConnectionState::DISCONNECTED) {
        return;
    }

    state_.store(ConnectionState::DISCONNECTING);
    auto_reconnect_ = false;

    asio::post(io_context_, [this, reason]() {
        // Send DISCONNECT packet
        auto disconnect_packet = PacketCodec::serialize_disconnect(
            connection_options_.version, reason);
        send_packet(disconnect_packet);

        // Close transport
        if (transport_ && transport_->is_open()) {
            transport_->close();
        }

        // Cancel timers
        keep_alive_timer_->cancel();
        ping_timeout_timer_->cancel();
        reconnect_timer_->cancel();

        // Cleanup pending operations
        cleanup_pending_operations();

        state_.store(ConnectionState::DISCONNECTED);

        if (disconnection_handler_) {
            disconnection_handler_(reason);
        }
    });
}

void Client::async_publish(Message message,
                           std::function<void(ErrorCode)> callback) {
    if (!is_connected()) {
        if (callback) {
            asio::post(io_context_,
                       [callback]() { callback(ErrorCode::PROTOCOL_ERROR); });
        }
        return;
    }

    asio::post(io_context_, [this, message = std::move(message),
                             callback = std::move(callback)]() mutable {
        uint16_t packet_id = 0;
        if (message.qos != QoS::AT_MOST_ONCE) {
            packet_id = generate_packet_id();
            message.packet_id = packet_id;

            // Store pending operation for QoS > 0
            std::lock_guard lock(pending_operations_mutex_);
            pending_operations_[packet_id] =
                PendingOperation{.message = message,
                                 .timestamp = std::chrono::steady_clock::now(),
                                 .retry_count = 0,
                                 .callback = callback};
        }

        auto packet = PacketCodec::serialize_publish(message, packet_id);
        send_packet(packet);

        // For QoS 0, call callback immediately
        if (message.qos == QoS::AT_MOST_ONCE && callback) {
            callback(ErrorCode::SUCCESS);
        }
    });
}

void Client::async_subscribe(const std::string& topic_filter, QoS qos,
                             std::function<void(ErrorCode)> callback) {
    std::vector<Subscription> subs = {{topic_filter, qos}};
    async_subscribe(subs, [callback = std::move(callback)](
                              const std::vector<ErrorCode>& results) {
        if (callback && !results.empty()) {
            callback(results[0]);
        }
    });
}

void Client::async_subscribe(
    const std::vector<Subscription>& subscriptions,
    std::function<void(std::vector<ErrorCode>)> callback) {
    if (!is_connected()) {
        if (callback) {
            asio::post(io_context_, [callback, &subscriptions]() {
                std::vector<ErrorCode> errors(subscriptions.size(),
                                              ErrorCode::PROTOCOL_ERROR);
                callback(errors);
            });
        }
        return;
    }

    asio::post(
        io_context_, [this, subscriptions, callback = std::move(callback)]() {
            uint16_t packet_id = generate_packet_id();

            // Store pending operation
            std::lock_guard lock(pending_operations_mutex_);
            pending_operations_[packet_id] = PendingOperation{
                .timestamp = std::chrono::steady_clock::now(),
                .retry_count = 0,
                .callback =
                    [callback](ErrorCode) { /* Will be handled in SUBACK */ }};

            auto packet =
                PacketCodec::serialize_subscribe(subscriptions, packet_id);
            send_packet(packet);
        });
}

void Client::async_unsubscribe(const std::string& topic_filter,
                               std::function<void(ErrorCode)> callback) {
    std::vector<std::string> topics = {topic_filter};
    async_unsubscribe(topics, [callback = std::move(callback)](
                                  const std::vector<ErrorCode>& results) {
        if (callback && !results.empty()) {
            callback(results[0]);
        }
    });
}

void Client::async_unsubscribe(
    const std::vector<std::string>& topic_filters,
    std::function<void(std::vector<ErrorCode>)> callback) {
    if (!is_connected()) {
        if (callback) {
            asio::post(io_context_, [callback, topic_filters]() {
                std::vector<ErrorCode> errors(topic_filters.size(),
                                              ErrorCode::PROTOCOL_ERROR);
                callback(errors);
            });
        }
        return;
    }

    asio::post(io_context_, [this, topic_filters,
                             callback = std::move(callback)]() {
        uint16_t packet_id = generate_packet_id();

        // Store pending operation
        std::lock_guard lock(pending_operations_mutex_);
        pending_operations_[packet_id] = PendingOperation{
            .timestamp = std::chrono::steady_clock::now(),
            .retry_count = 0,
            .callback =
                [callback](ErrorCode) { /* Will be handled in UNSUBACK */ }};

        auto packet =
            PacketCodec::serialize_unsubscribe(topic_filters, packet_id);
        send_packet(packet);
    });
}

void Client::setup_ssl_context(const ConnectionOptions& options) {
    if (!options.use_tls)
        return;

    ssl_context_ =
        std::make_unique<asio::ssl::context>(asio::ssl::context::tlsv12_client);

    if (options.verify_certificate) {
        ssl_context_->set_verify_mode(asio::ssl::verify_peer |
                                      asio::ssl::verify_fail_if_no_peer_cert);
        ssl_context_->set_default_verify_paths();
    } else {
        ssl_context_->set_verify_mode(asio::ssl::verify_none);
    }

    if (!options.ca_cert_file.empty()) {
        ssl_context_->load_verify_file(options.ca_cert_file);
    }

    if (!options.cert_file.empty()) {
        ssl_context_->use_certificate_file(options.cert_file,
                                           asio::ssl::context::pem);
    }

    if (!options.private_key_file.empty()) {
        ssl_context_->use_private_key_file(options.private_key_file,
                                           asio::ssl::context::pem);
    }
}

void Client::start_io_thread() {
    if (io_thread_ && io_thread_->joinable()) {
        return;
    }

    io_thread_ = std::make_unique<std::thread>([this]() {
        try {
            io_context_.run();
        } catch (const std::exception& e) {
            // Log error in production code
            notify_error(ErrorCode::UNSPECIFIED_ERROR);
        }
    });
}

void Client::stop_io_thread() {
    io_context_.stop();
    if (io_thread_ && io_thread_->joinable()) {
        io_thread_->join();
    }
    io_thread_.reset();
}

void Client::perform_connect() {
    setup_ssl_context(connection_options_);

    if (connection_options_.use_tls) {
        transport_ = std::make_unique<TLSTransport>(io_context_, *ssl_context_);
    } else {
        transport_ = std::make_unique<TCPTransport>(io_context_);
    }

    transport_->async_connect(
        broker_host_, broker_port_,
        [this](ErrorCode error) { handle_connect_result(error); });
}

void Client::handle_connect_result(ErrorCode error) {
    if (error != ErrorCode::SUCCESS) {
        state_.store(ConnectionState::DISCONNECTED);
        if (connection_handler_) {
            connection_handler_(error);
        }

        if (auto_reconnect_) {
            schedule_reconnect();
        }
        return;
    }

    // Send CONNECT packet
    auto connect_packet = PacketCodec::serialize_connect(connection_options_);
    send_packet(connect_packet);

    // Start reading responses
    start_reading();
}

void Client::start_reading() {
    if (!transport_ || !transport_->is_open()) {
        return;
    }

    transport_->async_read(std::span<uint8_t>(read_buffer_),
                           [this](ErrorCode error, size_t bytes_transferred) {
                               handle_read(error, bytes_transferred);
                           });
}

void Client::handle_read(ErrorCode error, size_t bytes_transferred) {
    if (error != ErrorCode::SUCCESS) {
        handle_transport_error(error);
        return;
    }

    if (bytes_transferred == 0) {
        start_reading();
        return;
    }

    // Update statistics
    update_stats_received(bytes_transferred);
    last_packet_received_ = std::chrono::steady_clock::now();

    // Append to packet buffer
    packet_buffer_.write_bytes(
        std::span<const uint8_t>(read_buffer_.data(), bytes_transferred));

    // Process complete packets
    process_received_data();

    // Continue reading
    start_reading();
}

void Client::process_received_data() {
    packet_buffer_.reset_position();

    while (packet_buffer_.position() < packet_buffer_.size()) {
        size_t start_pos = packet_buffer_.position();

        // Parse packet header
        auto header_data = packet_buffer_.data().subspan(start_pos);
        auto header_result = PacketCodec::parse_header(header_data);

        if (!header_result) {
            // Malformed packet, clear buffer
            packet_buffer_.clear();
            notify_error(ErrorCode::MALFORMED_PACKET);
            return;
        }

        PacketHeader header = *header_result;

        // Calculate header size
        size_t header_size = 1;  // Fixed header byte
        uint32_t remaining_length = header.remaining_length;
        do {
            header_size++;
            remaining_length >>= 7;
        } while (remaining_length > 0);

        // Check if we have the complete packet
        size_t total_packet_size = header_size + header.remaining_length;
        if (start_pos + total_packet_size > packet_buffer_.size()) {
            // Incomplete packet, wait for more data
            break;
        }

        // Extract payload
        auto payload = packet_buffer_.data().subspan(start_pos + header_size,
                                                     header.remaining_length);

        // Handle the packet
        handle_packet(header, payload);

        // Move to next packet
        packet_buffer_ = BinaryBuffer();  // Reset position
        auto remaining_data =
            packet_buffer_.data().subspan(start_pos + total_packet_size);
        packet_buffer_.write_bytes(remaining_data);
        packet_buffer_.reset_position();
    }
}

void Client::handle_packet(const PacketHeader& header,
                           std::span<const uint8_t> payload) {
    switch (header.type) {
        case PacketType::CONNACK:
            handle_connack(payload);
            break;
        case PacketType::PUBLISH:
            handle_publish(header, payload);
            break;
        case PacketType::PUBACK:
            handle_puback(payload);
            break;
        case PacketType::PUBREC:
            handle_pubrec(payload);
            break;
        case PacketType::PUBREL:
            handle_pubrel(payload);
            break;
        case PacketType::PUBCOMP:
            handle_pubcomp(payload);
            break;
        case PacketType::SUBACK:
            handle_suback(payload);
            break;
        case PacketType::UNSUBACK:
            handle_unsuback(payload);
            break;
        case PacketType::PINGRESP:
            handle_pingresp();
            break;
        default:
            // Unknown or unsupported packet type
            notify_error(ErrorCode::PROTOCOL_ERROR);
            break;
    }
}

void Client::send_packet(const BinaryBuffer& packet) {
    if (!transport_ || !transport_->is_open()) {
        return;
    }

    transport_->async_write(packet.data(),
                            [this](ErrorCode error, size_t bytes_transferred) {
                                handle_write(error, bytes_transferred);
                            });
}

void Client::handle_write(ErrorCode error, size_t bytes_transferred) {
    if (error != ErrorCode::SUCCESS) {
        handle_transport_error(error);
        return;
    }

    update_stats_sent(bytes_transferred);
}

void Client::start_keep_alive() {
    if (connection_options_.keep_alive.count() == 0) {
        return;
    }

    auto keep_alive_interval =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            connection_options_.keep_alive * 0.75);

    keep_alive_timer_->expires_after(keep_alive_interval);
    keep_alive_timer_->async_wait([this](const std::error_code& ec) {
        if (!ec) {
            handle_keep_alive_timeout();
        }
    });
}

void Client::handle_keep_alive_timeout() {
    if (!is_connected()) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto time_since_last_packet = now - last_packet_received_;

    if (time_since_last_packet >= connection_options_.keep_alive) {
        send_ping_request();
    }

    // Schedule next keep-alive check
    start_keep_alive();
}

void Client::send_ping_request() {
    auto ping_packet = PacketCodec::serialize_pingreq();
    send_packet(ping_packet);

    // Start ping timeout timer
    ping_timeout_timer_->expires_after(std::chrono::seconds(30));
    ping_timeout_timer_->async_wait([this](const std::error_code& ec) {
        if (!ec) {
            handle_ping_timeout();
        }
    });
}

void Client::handle_ping_timeout() {
    // No PINGRESP received, consider connection lost
    handle_transport_error(ErrorCode::SERVER_UNAVAILABLE);
}

void Client::schedule_reconnect() {
    if (!auto_reconnect_ || state_.load() != ConnectionState::DISCONNECTED) {
        return;
    }

    reconnect_timer_->expires_after(reconnect_delay_);
    reconnect_timer_->async_wait([this](const std::error_code& ec) {
        if (!ec) {
            handle_reconnect_timer();
        }
    });

    // Exponential backoff with jitter
    reconnect_delay_ = std::min(reconnect_delay_ * 2, MAX_RECONNECT_DELAY);

    // Add jitter to prevent thundering herd
    std::uniform_int_distribution<int> jitter_dist(0, 1000);
    auto jitter = std::chrono::duration_cast<decltype(reconnect_delay_)>(
        std::chrono::milliseconds(jitter_dist(gen_)));
    reconnect_delay_ += jitter;
}

void Client::handle_reconnect_timer() {
    if (auto_reconnect_ && state_.load() == ConnectionState::DISCONNECTED) {
        {
            std::unique_lock lock(stats_mutex_);
            stats_.reconnect_count++;
        }

        state_.store(ConnectionState::CONNECTING);
        perform_connect();
    }
}

uint16_t Client::generate_packet_id() noexcept {
    uint16_t id = next_packet_id_.fetch_add(1, std::memory_order_relaxed);
    if (id == 0) {
        id = next_packet_id_.fetch_add(1, std::memory_order_relaxed);
    }
    return id;
}

std::string Client::generate_client_id() const {
    std::uniform_int_distribution<> dist(0, 15);
    std::string client_id = "mqtt_cpp_";

    for (int i = 0; i < 8; ++i) {
        char c = dist(gen_) < 10 ? '0' + dist(gen_) : 'a' + (dist(gen_) - 10);
        client_id += c;
    }

    return client_id;
}

void Client::handle_connack(std::span<const uint8_t> data) {
    auto result = PacketCodec::parse_connack(data, connection_options_.version);

    if (!result || *result != ErrorCode::SUCCESS) {
        ErrorCode error = result ? *result : ErrorCode::PROTOCOL_ERROR;
        state_.store(ConnectionState::DISCONNECTED);

        if (connection_handler_) {
            connection_handler_(error);
        }

        if (auto_reconnect_) {
            schedule_reconnect();
        }
        return;
    }

    // Connection successful
    state_.store(ConnectionState::CONNECTED);
    reconnect_delay_ = std::chrono::seconds(1);  // Reset reconnect delay
    last_packet_received_ = std::chrono::steady_clock::now();

    {
        std::unique_lock lock(stats_mutex_);
        stats_.connected_since = std::chrono::steady_clock::now();
    }

    // Start keep-alive
    start_keep_alive();

    if (connection_handler_) {
        connection_handler_(ErrorCode::SUCCESS);
    }
}

void Client::handle_publish(const PacketHeader& header,
                            std::span<const uint8_t> data) {
    auto message_result = PacketCodec::parse_publish(header, data);

    if (!message_result) {
        notify_error(ErrorCode::MALFORMED_PACKET);
        return;
    }

    Message message = *message_result;

    // Send acknowledgment for QoS > 0
    if (message.qos == QoS::AT_LEAST_ONCE) {
        BinaryBuffer puback;
        puback.write<uint8_t>(static_cast<uint8_t>(PacketType::PUBACK) << 4);
        puback.write<uint8_t>(2);  // Remaining length
        puback.write<uint16_t>(message.packet_id);
        send_packet(puback);
    } else if (message.qos == QoS::EXACTLY_ONCE) {
        BinaryBuffer pubrec;
        pubrec.write<uint8_t>(static_cast<uint8_t>(PacketType::PUBREC) << 4);
        pubrec.write<uint8_t>(2);  // Remaining length
        pubrec.write<uint16_t>(message.packet_id);
        send_packet(pubrec);
    }

    // Update statistics
    {
        std::unique_lock lock(stats_mutex_);
        stats_.messages_received++;
    }

    // Notify message handler
    if (message_handler_) {
        message_handler_(message);
    }
}

void Client::handle_puback(std::span<const uint8_t> data) {
    if (data.size() < 2) {
        notify_error(ErrorCode::MALFORMED_PACKET);
        return;
    }

    uint16_t packet_id = (static_cast<uint16_t>(data[0]) << 8) | data[1];

    std::lock_guard lock(pending_operations_mutex_);
    auto it = pending_operations_.find(packet_id);
    if (it != pending_operations_.end()) {
        if (it->second.callback) {
            it->second.callback(ErrorCode::SUCCESS);
        }
        pending_operations_.erase(it);
    }
}

void Client::handle_pubrec(std::span<const uint8_t> data) {
    if (data.size() < 2) {
        notify_error(ErrorCode::MALFORMED_PACKET);
        return;
    }

    uint16_t packet_id = (static_cast<uint16_t>(data[0]) << 8) | data[1];

    // Send PUBREL
    BinaryBuffer pubrel;
    pubrel.write<uint8_t>((static_cast<uint8_t>(PacketType::PUBREL) << 4) |
                          0x02);
    pubrel.write<uint8_t>(2);  // Remaining length
    pubrel.write<uint16_t>(packet_id);
    send_packet(pubrel);
}

void Client::handle_pubrel(std::span<const uint8_t> data) {
    if (data.size() < 2) {
        notify_error(ErrorCode::MALFORMED_PACKET);
        return;
    }

    uint16_t packet_id = (static_cast<uint16_t>(data[0]) << 8) | data[1];

    // Send PUBCOMP
    BinaryBuffer pubcomp;
    pubcomp.write<uint8_t>(static_cast<uint8_t>(PacketType::PUBCOMP) << 4);
    pubcomp.write<uint8_t>(2);  // Remaining length
    pubcomp.write<uint16_t>(packet_id);
    send_packet(pubcomp);
}

void Client::handle_pubcomp(std::span<const uint8_t> data) {
    if (data.size() < 2) {
        notify_error(ErrorCode::MALFORMED_PACKET);
        return;
    }

    uint16_t packet_id = (static_cast<uint16_t>(data[0]) << 8) | data[1];

    std::lock_guard lock(pending_operations_mutex_);
    auto it = pending_operations_.find(packet_id);
    if (it != pending_operations_.end()) {
        if (it->second.callback) {
            it->second.callback(ErrorCode::SUCCESS);
        }
        pending_operations_.erase(it);
    }
}

void Client::handle_suback(std::span<const uint8_t> data) {
    auto result = PacketCodec::parse_suback(data);

    if (!result) {
        notify_error(ErrorCode::MALFORMED_PACKET);
        return;
    }

    // For now, just remove the pending operation
    // In a full implementation, you'd call the subscription callback with
    // results
    if (data.size() >= 2) {
        uint16_t packet_id = (static_cast<uint16_t>(data[0]) << 8) | data[1];
        std::lock_guard lock(pending_operations_mutex_);
        pending_operations_.erase(packet_id);
    }
}

void Client::handle_unsuback(std::span<const uint8_t> data) {
    auto result = PacketCodec::parse_unsuback(data);

    if (!result) {
        notify_error(ErrorCode::MALFORMED_PACKET);
        return;
    }

    // For now, just remove the pending operation
    // In a full implementation, you'd call the unsubscription callback with
    // results
    if (data.size() >= 2) {
        uint16_t packet_id = (static_cast<uint16_t>(data[0]) << 8) | data[1];
        std::lock_guard lock(pending_operations_mutex_);
        pending_operations_.erase(packet_id);
    }
}

void Client::handle_pingresp() {
    // Cancel ping timeout timer
    ping_timeout_timer_->cancel();
}

void Client::update_stats_sent(size_t bytes) {
    std::unique_lock lock(stats_mutex_);
    stats_.bytes_sent += bytes;
    stats_.messages_sent++;
}

void Client::update_stats_received(size_t bytes) {
    std::unique_lock lock(stats_mutex_);
    stats_.bytes_received += bytes;
}

void Client::cleanup_pending_operations() {
    std::lock_guard lock(pending_operations_mutex_);

    for (auto& [packet_id, operation] : pending_operations_) {
        if (operation.callback) {
            operation.callback(ErrorCode::UNSPECIFIED_ERROR);
        }
    }

    pending_operations_.clear();
}

void Client::notify_error(ErrorCode error) {
    if (disconnection_handler_) {
        disconnection_handler_(error);
    }
}

void Client::handle_transport_error(ErrorCode error) {
    if (state_.load() == ConnectionState::DISCONNECTED) {
        return;
    }

    state_.store(ConnectionState::DISCONNECTED);

    // Close transport
    if (transport_) {
        transport_->close();
    }

    // Cancel timers
    keep_alive_timer_->cancel();
    ping_timeout_timer_->cancel();

    // Cleanup pending operations
    cleanup_pending_operations();

    notify_error(error);

    if (auto_reconnect_) {
        schedule_reconnect();
    }
}

}  // namespace mqtt
