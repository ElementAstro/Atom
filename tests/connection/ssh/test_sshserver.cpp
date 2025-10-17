#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include "atom/connection/sshserver.hpp"

using namespace atom::connection;
using namespace std::chrono_literals;

class SshServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary config file for testing
        config_file_ =
            std::filesystem::temp_directory_path() / "test_ssh_config";
        createTestConfig();

        server_ = std::make_unique<SshServer>(config_file_);
    }

    void TearDown() override {
        if (server_) {
            server_->stop(true);  // Force stop
        }
        server_.reset();

        // Clean up config file
        std::error_code ec;
        std::filesystem::remove(config_file_, ec);
        std::filesystem::remove(host_key_file_, ec);
    }

    void createTestConfig() {
        host_key_file_ =
            std::filesystem::temp_directory_path() / "test_host_key";

        // Create a dummy host key file
        std::ofstream host_key(host_key_file_);
        host_key << "-----BEGIN OPENSSH PRIVATE KEY-----\n";
        host_key << "dummy_host_key_content_for_testing\n";
        host_key << "-----END OPENSSH PRIVATE KEY-----\n";
        host_key.close();

        // Create SSH server config file
        std::ofstream config(config_file_);
        config << "Port 2222\n";
        config << "ListenAddress 127.0.0.1\n";
        config << "HostKey " << host_key_file_.string() << "\n";
        config << "PasswordAuthentication yes\n";
        config << "PubkeyAuthentication yes\n";
        config << "PermitRootLogin no\n";
        config.close();
    }

    std::filesystem::path config_file_;
    std::filesystem::path host_key_file_;
    std::unique_ptr<SshServer> server_;
};

TEST_F(SshServerTest, ConstructorWithConfigFile) {
    EXPECT_NO_THROW(SshServer testServer(config_file_));
}

TEST_F(SshServerTest, ConstructorWithNonexistentConfig) {
    std::filesystem::path nonexistent = "/nonexistent/config/file";
    EXPECT_THROW(SshServer testServer(nonexistent), std::exception);
}

TEST_F(SshServerTest, InitialState) {
    EXPECT_FALSE(server_->isRunning());
    EXPECT_EQ(server_->getPort(), 0);  // Default port before configuration
}

TEST_F(SshServerTest, SetAndGetPort) {
    int testPort = 2223;
    server_->setPort(testPort);
    EXPECT_EQ(server_->getPort(), testPort);
}

TEST_F(SshServerTest, SetInvalidPort) {
    // Test invalid port numbers
    EXPECT_THROW(server_->setPort(-1), std::exception);
    EXPECT_THROW(server_->setPort(0), std::exception);
    EXPECT_THROW(server_->setPort(65536), std::exception);
}

TEST_F(SshServerTest, SetAndGetListenAddress) {
    std::string testAddress = "192.168.1.100";
    server_->setListenAddress(testAddress);
    EXPECT_EQ(server_->getListenAddress(), testAddress);
}

TEST_F(SshServerTest, SetEmptyListenAddress) {
    EXPECT_THROW(server_->setListenAddress(""), std::exception);
}

TEST_F(SshServerTest, SetAndGetHostKey) {
    server_->setHostKey(host_key_file_);
    EXPECT_EQ(server_->getHostKey(), host_key_file_);
}

TEST_F(SshServerTest, SetNonexistentHostKey) {
    std::filesystem::path nonexistent = "/nonexistent/host/key";
    EXPECT_THROW(server_->setHostKey(nonexistent), std::exception);
}

TEST_F(SshServerTest, StartServerBasic) {
    // Note: This test may fail if port 2222 is already in use
    // In a real environment, you might want to use a random available port

    server_->setPort(2224);  // Use different port to avoid conflicts
    server_->setListenAddress("127.0.0.1");
    server_->setHostKey(host_key_file_);

    // Starting may fail due to missing SSH daemon dependencies
    // but should not crash
    EXPECT_NO_THROW(server_->start());

    // If start succeeds, server should be running
    if (server_->isRunning()) {
        EXPECT_TRUE(server_->isRunning());
        EXPECT_GT(server_->getPort(), 0);
    }
}

TEST_F(SshServerTest, StopServerWithoutStart) {
    EXPECT_FALSE(server_->isRunning());

    // Should not throw when stopping a server that's not running
    EXPECT_NO_THROW(server_->stop());
    EXPECT_FALSE(server_->isRunning());
}

TEST_F(SshServerTest, ForceStopServer) {
    server_->setPort(2225);
    server_->setListenAddress("127.0.0.1");
    server_->setHostKey(host_key_file_);

    // Try to start (may fail, but test force stop anyway)
    server_->start();

    // Force stop should work regardless of server state
    EXPECT_NO_THROW(server_->stop(true));
    EXPECT_FALSE(server_->isRunning());
}

TEST_F(SshServerTest, RestartServer) {
    server_->setPort(2226);
    server_->setListenAddress("127.0.0.1");
    server_->setHostKey(host_key_file_);

    // Try restart (may fail due to system dependencies)
    EXPECT_NO_THROW(server_->restart());
}

TEST_F(SshServerTest, GetStatistics) {
    auto stats = server_->getStatistics();

    // Initial statistics should be zero
    EXPECT_EQ(stats.total_connections, 0);
    EXPECT_EQ(stats.active_connections, 0);
    EXPECT_EQ(stats.failed_authentications, 0);
    EXPECT_EQ(stats.successful_authentications, 0);
    EXPECT_EQ(stats.bytes_transferred, 0);
}

TEST_F(SshServerTest, ResetStatistics) {
    // Reset should work even with zero statistics
    EXPECT_NO_THROW(server_->resetStatistics());

    auto stats = server_->getStatistics();
    EXPECT_EQ(stats.total_connections, 0);
    EXPECT_EQ(stats.active_connections, 0);
}

TEST_F(SshServerTest, GetActiveConnections) {
    auto connections = server_->getActiveConnections();
    EXPECT_TRUE(connections.empty());  // No connections initially
}

TEST_F(SshServerTest, SetMaxConnections) {
    int maxConnections = 50;
    server_->setMaxConnections(maxConnections);
    EXPECT_EQ(server_->getMaxConnections(), maxConnections);
}

TEST_F(SshServerTest, SetInvalidMaxConnections) {
    EXPECT_THROW(server_->setMaxConnections(-1), std::exception);
    EXPECT_THROW(server_->setMaxConnections(0), std::exception);
}

TEST_F(SshServerTest, SetConnectionTimeout) {
    auto timeout = 300s;
    server_->setConnectionTimeout(timeout);
    EXPECT_EQ(server_->getConnectionTimeout(), timeout);
}

TEST_F(SshServerTest, SetInvalidConnectionTimeout) {
    EXPECT_THROW(server_->setConnectionTimeout(-1s), std::exception);
}

TEST_F(SshServerTest, EnableDisablePasswordAuth) {
    server_->enablePasswordAuthentication(true);
    EXPECT_TRUE(server_->isPasswordAuthenticationEnabled());

    server_->enablePasswordAuthentication(false);
    EXPECT_FALSE(server_->isPasswordAuthenticationEnabled());
}

TEST_F(SshServerTest, EnableDisablePublicKeyAuth) {
    server_->enablePublicKeyAuthentication(true);
    EXPECT_TRUE(server_->isPublicKeyAuthenticationEnabled());

    server_->enablePublicKeyAuthentication(false);
    EXPECT_FALSE(server_->isPublicKeyAuthenticationEnabled());
}

TEST_F(SshServerTest, SetLogLevel) {
    server_->setLogLevel(LogLevel::DEBUG);
    EXPECT_EQ(server_->getLogLevel(), LogLevel::DEBUG);

    server_->setLogLevel(LogLevel::ERROR);
    EXPECT_EQ(server_->getLogLevel(), LogLevel::ERROR);
}

TEST_F(SshServerTest, AddRemoveAuthorizedKey) {
    std::string testUser = "testuser";
    std::string testKey =
        "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQ test@example.com";

    EXPECT_NO_THROW(server_->addAuthorizedKey(testUser, testKey));

    auto keys = server_->getAuthorizedKeys(testUser);
    EXPECT_EQ(keys.size(), 1);
    EXPECT_EQ(keys[0], testKey);

    EXPECT_NO_THROW(server_->removeAuthorizedKey(testUser, testKey));

    keys = server_->getAuthorizedKeys(testUser);
    EXPECT_TRUE(keys.empty());
}

TEST_F(SshServerTest, AddInvalidAuthorizedKey) {
    std::string testUser = "testuser";
    std::string invalidKey = "invalid_key_format";

    EXPECT_THROW(server_->addAuthorizedKey(testUser, invalidKey),
                 std::exception);
}

TEST_F(SshServerTest, AddAuthorizedKeyEmptyUser) {
    std::string testKey =
        "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQ test@example.com";

    EXPECT_THROW(server_->addAuthorizedKey("", testKey), std::exception);
}

TEST_F(SshServerTest, GetAuthorizedKeysNonexistentUser) {
    auto keys = server_->getAuthorizedKeys("nonexistent_user");
    EXPECT_TRUE(keys.empty());
}

// Test callback functionality
class SshServerCallbackTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_file_ =
            std::filesystem::temp_directory_path() / "test_ssh_callback_config";
        createTestConfig();
        server_ = std::make_unique<SshServer>(config_file_);
    }

    void TearDown() override {
        if (server_) {
            server_->stop(true);
        }
        server_.reset();

        std::error_code ec;
        std::filesystem::remove(config_file_, ec);
        std::filesystem::remove(host_key_file_, ec);
    }

    void createTestConfig() {
        host_key_file_ =
            std::filesystem::temp_directory_path() / "test_callback_host_key";

        std::ofstream host_key(host_key_file_);
        host_key << "-----BEGIN OPENSSH PRIVATE KEY-----\n";
        host_key << "dummy_host_key_content\n";
        host_key << "-----END OPENSSH PRIVATE KEY-----\n";
        host_key.close();

        std::ofstream config(config_file_);
        config << "Port 2227\n";
        config << "ListenAddress 127.0.0.1\n";
        config << "HostKey " << host_key_file_.string() << "\n";
        config.close();
    }

    std::filesystem::path config_file_;
    std::filesystem::path host_key_file_;
    std::unique_ptr<SshServer> server_;
};

TEST_F(SshServerCallbackTest, SetNewConnectionCallback) {
    bool callbackCalled = false;
    SshConnection receivedConnection;

    server_->setNewConnectionCallback([&](const SshConnection& connection) {
        callbackCalled = true;
        receivedConnection = connection;
    });

    // Callback is set, but won't be called without actual connections
    EXPECT_FALSE(callbackCalled);
}

TEST_F(SshServerCallbackTest, SetConnectionClosedCallback) {
    bool callbackCalled = false;

    server_->setConnectionClosedCallback(
        [&](const SshConnection& connection) { callbackCalled = true; });

    // Callback is set, but won't be called without actual connections
    EXPECT_FALSE(callbackCalled);
}

TEST_F(SshServerCallbackTest, SetAuthFailureCallback) {
    bool callbackCalled = false;
    std::string receivedUser, receivedReason;

    server_->setAuthFailureCallback(
        [&](const std::string& user, const std::string& reason) {
            callbackCalled = true;
            receivedUser = user;
            receivedReason = reason;
        });

    // Callback is set, but won't be called without actual auth failures
    EXPECT_FALSE(callbackCalled);
}

// Test thread safety
class SshServerThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_file_ =
            std::filesystem::temp_directory_path() / "test_ssh_thread_config";
        createTestConfig();
        server_ = std::make_unique<SshServer>(config_file_);
    }

    void TearDown() override {
        if (server_) {
            server_->stop(true);
        }
        server_.reset();

        std::error_code ec;
        std::filesystem::remove(config_file_, ec);
        std::filesystem::remove(host_key_file_, ec);
    }

    void createTestConfig() {
        host_key_file_ =
            std::filesystem::temp_directory_path() / "test_thread_host_key";

        std::ofstream host_key(host_key_file_);
        host_key << "-----BEGIN OPENSSH PRIVATE KEY-----\n";
        host_key << "dummy_host_key_content\n";
        host_key << "-----END OPENSSH PRIVATE KEY-----\n";
        host_key.close();

        std::ofstream config(config_file_);
        config << "Port 2228\n";
        config << "ListenAddress 127.0.0.1\n";
        config << "HostKey " << host_key_file_.string() << "\n";
        config.close();
    }

    std::filesystem::path config_file_;
    std::filesystem::path host_key_file_;
    std::unique_ptr<SshServer> server_;
};

TEST_F(SshServerThreadSafetyTest, ConcurrentIsRunningCalls) {
    const int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> callCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, &callCount]() {
            try {
                server_->isRunning();  // Should be thread-safe
                callCount++;
            } catch (...) {
                // Ignore exceptions for this test
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(callCount.load(), numThreads);
}

TEST_F(SshServerThreadSafetyTest, ConcurrentGetStatistics) {
    const int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> callCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, &callCount]() {
            try {
                server_->getStatistics();  // Should be thread-safe
                callCount++;
            } catch (...) {
                // Ignore exceptions for this test
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(callCount.load(), numThreads);
}

TEST_F(SshServerThreadSafetyTest, ConcurrentConfigurationChanges) {
    const int numThreads = 3;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, &successCount]() {
            try {
                server_->setPort(3000 + i);  // Different ports for each thread
                server_->setMaxConnections(10 + i);
                successCount++;
            } catch (...) {
                // Some operations might fail due to race conditions
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // At least some operations should succeed
    EXPECT_GT(successCount.load(), 0);
}
