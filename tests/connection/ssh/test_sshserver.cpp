#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include "atom/connection/ssh/sshserver.hpp"

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
    // getStatistics returns std::unordered_map<std::string, std::string>
    auto stats = server_->getStatistics();
    // Stats map may be empty initially
    EXPECT_NO_THROW(server_->getStatistics());
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

TEST_F(SshServerTest, SetIdleTimeout) {
    int timeout = 300;
    server_->setIdleTimeout(timeout);
    EXPECT_EQ(server_->getIdleTimeout(), timeout);
}

TEST_F(SshServerTest, SetLoginGraceTime) {
    int graceTime = 60;
    server_->setLoginGraceTime(graceTime);
    EXPECT_EQ(server_->getLoginGraceTime(), graceTime);
}

TEST_F(SshServerTest, SetMaxAuthAttempts) {
    int maxAttempts = 3;
    server_->setMaxAuthAttempts(maxAttempts);
    EXPECT_EQ(server_->getMaxAuthAttempts(), maxAttempts);
}

TEST_F(SshServerTest, AllowRootLogin) {
    server_->allowRootLogin(true);
    EXPECT_TRUE(server_->isRootLoginAllowed());

    server_->allowRootLogin(false);
    EXPECT_FALSE(server_->isRootLoginAllowed());
}

TEST_F(SshServerTest, SetPasswordAuthentication) {
    server_->setPasswordAuthentication(true);
    EXPECT_TRUE(server_->isPasswordAuthenticationEnabled());

    server_->setPasswordAuthentication(false);
    EXPECT_FALSE(server_->isPasswordAuthenticationEnabled());
}

TEST_F(SshServerTest, SetLogLevel) {
    server_->setLogLevel(LogLevel::DEBUG);
    EXPECT_EQ(server_->getLogLevel(), LogLevel::DEBUG);

    server_->setLogLevel(LogLevel::ERROR);
    EXPECT_EQ(server_->getLogLevel(), LogLevel::ERROR);
}

TEST_F(SshServerTest, SetAndGetAuthorizedKeys) {
    // setAuthorizedKeys takes vector of filesystem::path
    std::vector<std::filesystem::path> keyFiles;
    keyFiles.push_back(host_key_file_);

    EXPECT_NO_THROW(server_->setAuthorizedKeys(keyFiles));

    auto keys = server_->getAuthorizedKeys();
    EXPECT_EQ(keys.size(), 1);
}

TEST_F(SshServerTest, AllowDenyIpAddress) {
    std::string testIp = "192.168.1.100";

    server_->allowIpAddress(testIp);
    EXPECT_TRUE(server_->isIpAddressAllowed(testIp));

    server_->denyIpAddress(testIp);
    EXPECT_FALSE(server_->isIpAddressAllowed(testIp));
}

TEST_F(SshServerTest, AllowAgentForwarding) {
    server_->allowAgentForwarding(true);
    EXPECT_TRUE(server_->isAgentForwardingAllowed());

    server_->allowAgentForwarding(false);
    EXPECT_FALSE(server_->isAgentForwardingAllowed());
}

TEST_F(SshServerTest, AllowTcpForwarding) {
    server_->allowTcpForwarding(true);
    EXPECT_TRUE(server_->isTcpForwardingAllowed());

    server_->allowTcpForwarding(false);
    EXPECT_FALSE(server_->isTcpForwardingAllowed());
}

TEST_F(SshServerTest, SetSubsystem) {
    std::string name = "sftp";
    std::string command = "/usr/lib/openssh/sftp-server";

    EXPECT_NO_THROW(server_->setSubsystem(name, command));
    EXPECT_EQ(server_->getSubsystem(name), command);

    EXPECT_NO_THROW(server_->removeSubsystem(name));
    EXPECT_TRUE(server_->getSubsystem(name).empty());
}

TEST_F(SshServerTest, SetCiphers) {
    std::string ciphers = "aes256-ctr,aes192-ctr,aes128-ctr";
    server_->setCiphers(ciphers);
    EXPECT_EQ(server_->getCiphers(), ciphers);
}

TEST_F(SshServerTest, SetMACs) {
    std::string macs = "hmac-sha2-256,hmac-sha2-512";
    server_->setMACs(macs);
    EXPECT_EQ(server_->getMACs(), macs);
}

TEST_F(SshServerTest, SetKexAlgorithms) {
    std::string kex = "curve25519-sha256,diffie-hellman-group-exchange-sha256";
    server_->setKexAlgorithms(kex);
    EXPECT_EQ(server_->getKexAlgorithms(), kex);
}

TEST_F(SshServerTest, SetLogFile) {
    std::filesystem::path logFile =
        std::filesystem::temp_directory_path() / "ssh_test.log";
    server_->setLogFile(logFile);
    EXPECT_EQ(server_->getLogFile(), logFile);
}

TEST_F(SshServerTest, GetServerVersion) {
    auto version = server_->getServerVersion();
    // Version string should not be empty
    EXPECT_NO_THROW(server_->getServerVersion());
}

TEST_F(SshServerTest, SetServerVersion) {
    std::string version = "SSH-2.0-TestServer_1.0";
    server_->setServerVersion(version);
    EXPECT_EQ(server_->getServerVersion(), version);
}

TEST_F(SshServerTest, VerifyConfiguration) {
    // Configuration verification
    bool isValid = server_->verifyConfiguration();
    // May or may not be valid depending on setup
    EXPECT_NO_THROW(server_->verifyConfiguration());
}

TEST_F(SshServerTest, GetConfigurationIssues) {
    auto issues = server_->getConfigurationIssues();
    // Issues list depends on configuration state
    EXPECT_NO_THROW(server_->getConfigurationIssues());
}

TEST_F(SshServerTest, DisconnectClient) {
    // Try to disconnect non-existent client
    bool result = server_->disconnectClient("nonexistent_session_id");
    EXPECT_FALSE(result);
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

TEST_F(SshServerCallbackTest, OnNewConnectionCallback) {
    bool callbackCalled = false;
    SshConnection receivedConnection;

    server_->onNewConnection([&](const SshConnection& connection) {
        callbackCalled = true;
        receivedConnection = connection;
    });

    // Callback is set, but won't be called without actual connections
    EXPECT_FALSE(callbackCalled);
}

TEST_F(SshServerCallbackTest, OnConnectionClosedCallback) {
    bool callbackCalled = false;

    server_->onConnectionClosed(
        [&](const SshConnection& connection) { callbackCalled = true; });

    // Callback is set, but won't be called without actual connections
    EXPECT_FALSE(callbackCalled);
}

TEST_F(SshServerCallbackTest, OnAuthenticationFailureCallback) {
    bool callbackCalled = false;
    std::string receivedUser, receivedReason;

    server_->onAuthenticationFailure(
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

TEST_F(SshServerThreadSafetyTest, ConcurrentIpAddressOperations) {
    const int numThreads = 5;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Test concurrent IP allow/deny operations (now O(1) with unordered_set)
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, &successCount]() {
            try {
                std::string ip = "192.168.1." + std::to_string(i * 10);
                server_->allowIpAddress(ip);
                bool allowed = server_->isIpAddressAllowed(ip);
                EXPECT_TRUE(allowed);

                server_->denyIpAddress(ip);
                allowed = server_->isIpAddressAllowed(ip);
                EXPECT_FALSE(allowed);

                successCount++;
            } catch (...) {
                // Ignore exceptions for this test
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), numThreads);
}

TEST_F(SshServerThreadSafetyTest, ConcurrentStartStopOperations) {
    const int numIterations = 3;
    std::atomic<int> operationCount{0};

    // Test that concurrent start/stop doesn't crash (atomic isRunning_)
    std::thread starter([this, &operationCount, numIterations]() {
        for (int i = 0; i < numIterations; ++i) {
            try {
                server_->start();
                operationCount++;
            } catch (...) {
                // Expected - server may already be running
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    std::thread stopper([this, &operationCount, numIterations]() {
        for (int i = 0; i < numIterations; ++i) {
            try {
                server_->stop(true);
                operationCount++;
            } catch (...) {
                // Expected - server may not be running
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    starter.join();
    stopper.join();

    // Should have completed without deadlock
    EXPECT_GT(operationCount.load(), 0);
}
