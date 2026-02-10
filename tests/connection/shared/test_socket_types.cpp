/*
 * test_socket_types.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-01-29

Description: Unit tests for shared socket types

**************************************************/

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "atom/connection/shared/socket_types.hpp"

using namespace atom::connection;
using namespace std::chrono_literals;

class MessageTest : public ::testing::Test {};

TEST_F(MessageTest, CreateTextMessage) {
    auto msg = Message::createText("Hello, World!", 42);

    EXPECT_EQ(msg.type, Message::Type::TEXT);
    EXPECT_EQ(msg.sender_id, 42);
    EXPECT_EQ(msg.asString(), "Hello, World!");
}

TEST_F(MessageTest, CreateBinaryMessage) {
    std::vector<char> data = {'H', 'e', 'l', 'l', 'o'};
    auto msg = Message::createBinary(data, 123);

    EXPECT_EQ(msg.type, Message::Type::BINARY);
    EXPECT_EQ(msg.sender_id, 123);
    EXPECT_EQ(msg.data, data);
}

TEST_F(MessageTest, AsStringConvertsData) {
    Message msg;
    msg.type = Message::Type::TEXT;
    msg.data = {'T', 'e', 's', 't'};
    msg.sender_id = 1;

    EXPECT_EQ(msg.asString(), "Test");
}

class ClientInfoTest : public ::testing::Test {};

TEST_F(ClientInfoTest, DefaultConstruction) {
    ClientInfo info;

    EXPECT_EQ(info.id, 0);
    EXPECT_TRUE(info.address.empty());
    EXPECT_EQ(info.bytes_received, 0);
    EXPECT_EQ(info.bytes_sent, 0);
    EXPECT_EQ(info.messages_received, 0);
    EXPECT_EQ(info.messages_sent, 0);
    EXPECT_FALSE(info.is_authenticated);
}

TEST_F(ClientInfoTest, FieldAssignment) {
    ClientInfo info;
    info.id = 42;
    info.address = "192.168.1.1";
    info.bytes_received = 1024;
    info.bytes_sent = 2048;
    info.is_authenticated = true;

    EXPECT_EQ(info.id, 42);
    EXPECT_EQ(info.address, "192.168.1.1");
    EXPECT_EQ(info.bytes_received, 1024);
    EXPECT_EQ(info.bytes_sent, 2048);
    EXPECT_TRUE(info.is_authenticated);
}

class SocketHubStatsTest : public ::testing::Test {
protected:
    void SetUp() override { stats_ = std::make_unique<SocketHubStats>(); }

    std::unique_ptr<SocketHubStats> stats_;
};

TEST_F(SocketHubStatsTest, DefaultConstruction) {
    EXPECT_EQ(stats_->total_connections.load(), 0);
    EXPECT_EQ(stats_->active_connections.load(), 0);
    EXPECT_EQ(stats_->messages_received.load(), 0);
    EXPECT_EQ(stats_->messages_sent.load(), 0);
    EXPECT_EQ(stats_->bytes_received.load(), 0);
    EXPECT_EQ(stats_->bytes_sent.load(), 0);
}

TEST_F(SocketHubStatsTest, AtomicIncrement) {
    stats_->total_connections++;
    stats_->active_connections++;
    stats_->messages_received += 10;

    EXPECT_EQ(stats_->total_connections.load(), 1);
    EXPECT_EQ(stats_->active_connections.load(), 1);
    EXPECT_EQ(stats_->messages_received.load(), 10);
}

TEST_F(SocketHubStatsTest, Reset) {
    stats_->total_connections = 100;
    stats_->active_connections = 50;
    stats_->messages_received = 1000;
    stats_->bytes_sent = 10000;

    stats_->reset();

    EXPECT_EQ(stats_->total_connections.load(), 0);
    EXPECT_EQ(stats_->active_connections.load(), 0);
    EXPECT_EQ(stats_->messages_received.load(), 0);
    EXPECT_EQ(stats_->bytes_sent.load(), 0);
}

TEST_F(SocketHubStatsTest, Uptime) {
    // Start time should be set
    auto uptime = stats_->uptime();
    EXPECT_GE(uptime.count(), 0);

    std::this_thread::sleep_for(100ms);

    auto new_uptime = stats_->uptime();
    EXPECT_GE(new_uptime.count(), uptime.count());
}

TEST_F(SocketHubStatsTest, ThreadSafetyIncrement) {
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this]() {
            for (int j = 0; j < 100; ++j) {
                stats_->messages_received++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(stats_->messages_received.load(), 1000);
}

class SocketHubConfigTest : public ::testing::Test {};

TEST_F(SocketHubConfigTest, DefaultValues) {
    SocketHubConfig config;

    EXPECT_FALSE(config.use_ssl);
    EXPECT_GT(config.backlog_size, 0);
    EXPECT_TRUE(config.keep_alive);
    EXPECT_FALSE(config.enable_rate_limiting);
    EXPECT_GT(config.max_connections_per_ip, 0);
    EXPECT_GT(config.max_messages_per_minute, 0);
}

TEST_F(SocketHubConfigTest, CustomConfiguration) {
    SocketHubConfig config;
    config.use_ssl = true;
    config.ssl_cert_file = "server.crt";
    config.ssl_key_file = "server.key";
    config.connection_timeout = 60s;
    config.enable_rate_limiting = true;
    config.max_connections_per_ip = 5;

    EXPECT_TRUE(config.use_ssl);
    EXPECT_EQ(config.ssl_cert_file, "server.crt");
    EXPECT_EQ(config.ssl_key_file, "server.key");
    EXPECT_EQ(config.connection_timeout, 60s);
    EXPECT_TRUE(config.enable_rate_limiting);
    EXPECT_EQ(config.max_connections_per_ip, 5);
}

class LogLevelTest : public ::testing::Test {};

TEST_F(LogLevelTest, EnumOrdering) {
    EXPECT_LT(static_cast<int>(LogLevel::TRACE),
              static_cast<int>(LogLevel::DEBUG_LEVEL));
    EXPECT_LT(static_cast<int>(LogLevel::DEBUG_LEVEL),
              static_cast<int>(LogLevel::INFO_LEVEL));
    EXPECT_LT(static_cast<int>(LogLevel::INFO_LEVEL),
              static_cast<int>(LogLevel::WARNING_LEVEL));
    EXPECT_LT(static_cast<int>(LogLevel::WARNING_LEVEL),
              static_cast<int>(LogLevel::ERROR_LEVEL));
    EXPECT_LT(static_cast<int>(LogLevel::ERROR_LEVEL),
              static_cast<int>(LogLevel::FATAL_LEVEL));
}
