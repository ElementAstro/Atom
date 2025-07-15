#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "atom/connection/sshserver.hpp"

// Define a temporary directory for test files
const std::filesystem::path TEST_TEMP_DIR =
    std::filesystem::temp_directory_path() / "sshserver_test";

namespace atom::connection::test {

// Mock classes for callbacks
class MockNewConnectionCallback {
public:
    MOCK_METHOD(void, call, (const SshConnection& conn), ());
};

class MockConnectionClosedCallback {
public:
    MOCK_METHOD(void, call, (const SshConnection& conn), ());
};

class MockAuthenticationFailureCallback {
public:
    MOCK_METHOD(void, call,
                (const std::string& username, const std::string& ipAddress),
                ());
};

class SshServerTest : public ::testing::Test {
protected:
    std::filesystem::path config_file_;
    std::filesystem::path host_key_file_;
    std::filesystem::path auth_keys_file_;
    std::filesystem::path log_file_;
    std::unique_ptr<SshServer> server_;

    void SetUp() override {
        // Create a unique temporary directory for each test
        std::filesystem::create_directories(TEST_TEMP_DIR);
        config_file_ = TEST_TEMP_DIR / "sshd_config_test";
        host_key_file_ = TEST_TEMP_DIR / "ssh_host_rsa_key";
        auth_keys_file_ = TEST_TEMP_DIR / "authorized_keys";
        log_file_ = TEST_TEMP_DIR / "sshd_test.log";

        // Initialize server with a new config file for each test
        server_ = std::make_unique<SshServer>(config_file_);

        // Set up basic valid configuration for most tests
        server_->setPort(2222);
        server_->setListenAddress("127.0.0.1");
        server_->setHostKey(host_key_file_);
        server_->setPasswordAuthentication(
            true);  // Enable password auth for simplicity
        server_->setLogFile(log_file_);

        // Generate a dummy host key if it doesn't exist (needed for config
        // verification)
        if (!std::filesystem::exists(host_key_file_)) {
            server_->generateHostKey("rsa", 2048, host_key_file_);
        }
    }

    void TearDown() override {
        if (server_->isRunning()) {
            server_->stop(true);  // Force stop if still running
        }
        server_.reset();  // Ensure server is destroyed before removing files

        // Clean up temporary directory
        std::error_code ec;
        std::filesystem::remove_all(TEST_TEMP_DIR, ec);
        if (ec) {
            // Log error if cleanup fails, but don't fail the test
            std::cerr << "Error removing test directory " << TEST_TEMP_DIR
                      << ": " << ec.message() << std::endl;
        }
    }

    // Helper to create a dummy authorized_keys file
    void createDummyAuthorizedKeys(
        const std::string& content =
            "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQC... testuser@example.com") {
        std::ofstream ofs(auth_keys_file_);
        ofs << content;
        ofs.close();
        server_->setAuthorizedKeys({auth_keys_file_});
    }
};

// Test default constructor values and basic setup
TEST_F(SshServerTest, ConstructorAndDefaults) {
    EXPECT_FALSE(server_->isRunning());
    EXPECT_EQ(server_->getPort(), 2222);                  // Set in SetUp
    EXPECT_EQ(server_->getListenAddress(), "127.0.0.1");  // Set in SetUp
    EXPECT_EQ(server_->getHostKey(), host_key_file_);     // Set in SetUp
    EXPECT_FALSE(server_->isRootLoginAllowed());
    EXPECT_TRUE(server_->isPasswordAuthenticationEnabled());  // Set in SetUp
    EXPECT_EQ(server_->getMaxAuthAttempts(), 6);
    EXPECT_EQ(server_->getMaxConnections(), 10);
    EXPECT_EQ(server_->getLoginGraceTime(), 120);
    EXPECT_EQ(server_->getIdleTimeout(), 300);
    EXPECT_FALSE(server_->isAgentForwardingAllowed());
    EXPECT_FALSE(server_->isTcpForwardingAllowed());
    EXPECT_EQ(server_->getLogLevel(), LogLevel::INFO);
    EXPECT_EQ(server_->getLogFile(), log_file_);
    EXPECT_EQ(server_->getServerVersion(), "SSH-2.0-AtomSSH_1.0");
    EXPECT_TRUE(server_->getCiphers().find("chacha20-poly1305") !=
                std::string::npos);
    EXPECT_TRUE(server_->getMACs().find("hmac-sha2-512") != std::string::npos);
    EXPECT_TRUE(server_->getKexAlgorithms().find("curve25519-sha256") !=
                std::string::npos);
}

// Test configuration setters and getters
TEST_F(SshServerTest, ConfigurationSettersAndGetters) {
    server_->setPort(8022);
    EXPECT_EQ(server_->getPort(), 8022);

    server_->setListenAddress("0.0.0.0");
    EXPECT_EQ(server_->getListenAddress(), "0.0.0.0");

    std::filesystem::path new_host_key = TEST_TEMP_DIR / "new_host_key";
    server_->setHostKey(new_host_key);
    EXPECT_EQ(server_->getHostKey(), new_host_key);

    createDummyAuthorizedKeys();  // Creates auth_keys_file_ and sets it
    EXPECT_EQ(server_->getAuthorizedKeys().size(), 1);
    EXPECT_EQ(server_->getAuthorizedKeys()[0], auth_keys_file_);

    server_->allowRootLogin(true);
    EXPECT_TRUE(server_->isRootLoginAllowed());

    server_->setPasswordAuthentication(false);
    EXPECT_FALSE(server_->isPasswordAuthenticationEnabled());

    server_->setMaxAuthAttempts(3);
    EXPECT_EQ(server_->getMaxAuthAttempts(), 3);

    server_->setMaxConnections(50);
    EXPECT_EQ(server_->getMaxConnections(), 50);

    server_->setLoginGraceTime(60);
    EXPECT_EQ(server_->getLoginGraceTime(), 60);

    server_->setIdleTimeout(180);
    EXPECT_EQ(server_->getIdleTimeout(), 180);

    server_->allowAgentForwarding(true);
    EXPECT_TRUE(server_->isAgentForwardingAllowed());

    server_->allowTcpForwarding(true);
    EXPECT_TRUE(server_->isTcpForwardingAllowed());

    server_->setLogLevel(LogLevel::DEBUG3);
    EXPECT_EQ(server_->getLogLevel(), LogLevel::DEBUG3);

    std::filesystem::path new_log_file = TEST_TEMP_DIR / "new_log.log";
    server_->setLogFile(new_log_file);
    EXPECT_EQ(server_->getLogFile(), new_log_file);

    server_->setCiphers("aes256-ctr");
    EXPECT_EQ(server_->getCiphers(), "aes256-ctr");

    server_->setMACs("hmac-sha2-256");
    EXPECT_EQ(server_->getMACs(), "hmac-sha2-256");

    server_->setKexAlgorithms("diffie-hellman-group14-sha1");
    EXPECT_EQ(server_->getKexAlgorithms(), "diffie-hellman-group14-sha1");

    server_->setServerVersion("SSH-2.0-CustomServer");
    EXPECT_EQ(server_->getServerVersion(), "SSH-2.0-CustomServer");
}

// Test IP filtering
TEST_F(SshServerTest, IpFiltering) {
    server_->allowIpAddress("192.168.1.1");
    server_->denyIpAddress("192.168.1.2");

    EXPECT_TRUE(server_->isIpAddressAllowed("192.168.1.1"));
    EXPECT_FALSE(server_->isIpAddressAllowed("192.168.1.2"));
    EXPECT_TRUE(server_->isIpAddressAllowed(
        "192.168.1.3"));  // Not explicitly allowed or denied, so allowed by
                          // default

    // Test allowing an IP that was previously denied
    server_->allowIpAddress("192.168.1.2");
    EXPECT_TRUE(server_->isIpAddressAllowed("192.168.1.2"));

    // Test denying an IP that was previously allowed
    server_->denyIpAddress("192.168.1.1");
    EXPECT_FALSE(server_->isIpAddressAllowed("192.168.1.1"));

    // Test with empty allowed list (all allowed by default)
    server_->allowIpAddress("1.1.1.1");  // Add one
    server_->denyIpAddress("1.1.1.1");   // Deny it
    server_->allowIpAddress("1.1.1.1");  // Allow it again
    // Clear all lists to test default behavior
    server_->denyIpAddress("1.1.1.1");   // Remove from allowed
    server_->allowIpAddress("1.1.1.1");  // Remove from denied
    // The internal lists are not directly exposed to clear, so we rely on the
    // logic that if allowedIps_ is empty, all are allowed unless in deniedIps_.
    // The current implementation of allow/deny removes from the other list.
    // So, to test "empty allowed list", we need to ensure no IPs are in
    // allowedIps_ and no IPs are in deniedIps_. This is hard to test directly
    // without exposing internal lists. Assuming default state after
    // construction, all are allowed.
    EXPECT_TRUE(server_->isIpAddressAllowed("10.0.0.1"));
}

// Test Subsystems
TEST_F(SshServerTest, SubsystemManagement) {
    server_->setSubsystem("sftp", "/usr/lib/openssh/sftp-server");
    server_->setSubsystem("git-receive-pack", "/usr/bin/git-receive-pack");

    EXPECT_EQ(server_->getSubsystem("sftp"), "/usr/lib/openssh/sftp-server");
    EXPECT_EQ(server_->getSubsystem("git-receive-pack"),
              "/usr/bin/git-receive-pack");
    EXPECT_EQ(server_->getSubsystem("non-existent"), "");

    server_->removeSubsystem("sftp");
    EXPECT_EQ(server_->getSubsystem("sftp"), "");
}

// Test host key generation
TEST_F(SshServerTest, GenerateHostKey) {
    std::filesystem::path generated_key = TEST_TEMP_DIR / "generated_host_key";
    EXPECT_TRUE(server_->generateHostKey("rsa", 2048, generated_key));
    EXPECT_TRUE(std::filesystem::exists(generated_key));
    EXPECT_TRUE(std::filesystem::exists(generated_key.string() + ".pub"));

    // Test unsupported key type
    EXPECT_FALSE(server_->generateHostKey("unsupported", 2048, generated_key));
}

// Test configuration verification
TEST_F(SshServerTest, VerifyConfiguration) {
    // Initial setup in SetUp should be valid
    EXPECT_TRUE(server_->verifyConfiguration());
    EXPECT_TRUE(server_->getConfigurationIssues().empty());

    // Make it invalid: no auth method
    server_->setPasswordAuthentication(false);
    server_->setAuthorizedKeys({});
    EXPECT_FALSE(server_->verifyConfiguration());
    EXPECT_FALSE(server_->getConfigurationIssues().empty());
    EXPECT_THAT(server_->getConfigurationIssues(),
                testing::Contains(
                    testing::HasSubstr("No authentication methods enabled")));

    // Make it invalid: invalid port
    server_->setPasswordAuthentication(true);  // Re-enable auth
    server_->setPort(0);
    EXPECT_FALSE(server_->verifyConfiguration());
    EXPECT_THAT(server_->getConfigurationIssues(),
                testing::Contains(testing::HasSubstr("Invalid port number")));

    // Make it invalid: missing host key
    std::filesystem::remove(host_key_file_);
    EXPECT_FALSE(server_->verifyConfiguration());
    EXPECT_THAT(
        server_->getConfigurationIssues(),
        testing::Contains(testing::HasSubstr("Host key file does not exist")));
}

// Test server start/stop lifecycle
TEST_F(SshServerTest, StartStopServer) {
    EXPECT_FALSE(server_->isRunning());
    EXPECT_TRUE(server_->start());
    EXPECT_TRUE(server_->isRunning());

    EXPECT_TRUE(server_->stop());
    EXPECT_FALSE(server_->isRunning());
}

TEST_F(SshServerTest, StartAlreadyRunning) {
    EXPECT_TRUE(server_->start());
    EXPECT_TRUE(server_->isRunning());
    EXPECT_FALSE(server_->start());  // Should return false if already running
    EXPECT_TRUE(server_->isRunning());
}

TEST_F(SshServerTest, StopNotRunning) {
    EXPECT_FALSE(server_->isRunning());
    EXPECT_FALSE(server_->stop());  // Should return false if not running
}

TEST_F(SshServerTest, RestartServer) {
    EXPECT_TRUE(server_->start());
    EXPECT_TRUE(server_->isRunning());

    EXPECT_TRUE(server_->restart());
    EXPECT_TRUE(server_->isRunning());  // Should be running after restart
}

// Test statistics
TEST_F(SshServerTest, GetStatistics) {
    EXPECT_TRUE(server_->start());

    auto stats = server_->getStatistics();
    EXPECT_EQ(stats["active_connections"], "0");
    EXPECT_EQ(stats["total_connections"], "0");
    EXPECT_EQ(stats["failed_auth_attempts"], "0");
    EXPECT_FALSE(stats["uptime"].empty());

    // Allow some time for simulated activity
    std::this_thread::sleep_for(std::chrono::seconds(15));

    stats = server_->getStatistics();
    EXPECT_GT(std::stoi(stats["active_connections"]), 0);
    EXPECT_GT(std::stoi(stats["total_connections"]), 0);
    EXPECT_GT(std::stoi(stats["failed_auth_attempts"]), 0);
    EXPECT_FALSE(stats["uptime"].empty());

    EXPECT_TRUE(server_->stop());
}

// Test callbacks
TEST_F(SshServerTest, CallbacksInvoked) {
    MockNewConnectionCallback mock_new_conn_cb;
    MockConnectionClosedCallback mock_closed_conn_cb;
    MockAuthenticationFailureCallback mock_auth_fail_cb;

    server_->onNewConnection(std::bind(&MockNewConnectionCallback::call,
                                       &mock_new_conn_cb,
                                       std::placeholders::_1));
    server_->onConnectionClosed(std::bind(&MockConnectionClosedCallback::call,
                                          &mock_closed_conn_cb,
                                          std::placeholders::_1));
    server_->onAuthenticationFailure(
        std::bind(&MockAuthenticationFailureCallback::call, &mock_auth_fail_cb,
                  std::placeholders::_1, std::placeholders::_2));

    // Expect at least one call for each type of event within a reasonable time
    EXPECT_CALL(mock_new_conn_cb, call(testing::An<const SshConnection&>()))
        .Times(testing::AtLeast(1));
    EXPECT_CALL(mock_closed_conn_cb, call(testing::An<const SshConnection&>()))
        .Times(testing::AtLeast(1));
    EXPECT_CALL(mock_auth_fail_cb, call(testing::An<const std::string&>(),
                                        testing::An<const std::string&>()))
        .Times(testing::AtLeast(1));

    EXPECT_TRUE(server_->start());

    // Give enough time for the internal simulation to trigger callbacks
    std::this_thread::sleep_for(std::chrono::seconds(20));

    EXPECT_TRUE(server_->stop());
}

// Test active connections and disconnectClient (simulated)
TEST_F(SshServerTest, ActiveConnectionsAndDisconnect) {
    EXPECT_TRUE(server_->start());
    std::this_thread::sleep_for(
        std::chrono::seconds(15));  // Allow connections to build up

    std::vector<SshConnection> connections = server_->getActiveConnections();
    EXPECT_GT(connections.size(), 0);

    if (!connections.empty()) {
        std::string sessionIdToDisconnect = connections[0].sessionId;
        // Note: disconnectClient currently has limited real functionality on
        // Windows and relies on 'ssh-kill' on Unix, which might not be
        // available or work in test env. For this test, we'll just check if the
        // call returns true/false based on session existence. The actual
        // process termination is not easily verifiable in a unit test.
        bool disconnected = server_->disconnectClient(sessionIdToDisconnect);
#ifdef _WIN32
        // On Windows, it's expected to return false as per current Impl
        EXPECT_FALSE(disconnected);
#else
        // On Unix, it might return true if ssh-kill is available and works
        // We can't reliably assert true/false without knowing the system setup
        // but we can check if the connection is eventually removed from the
        // list by the monitor thread.
        if (disconnected) {
            std::this_thread::sleep_for(
                std::chrono::seconds(2));  // Give time for monitor to update
            std::vector<SshConnection> updated_connections =
                server_->getActiveConnections();
            bool found = false;
            for (const auto& conn : updated_connections) {
                if (conn.sessionId == sessionIdToDisconnect) {
                    found = true;
                    break;
                }
            }
            EXPECT_FALSE(
                found);  // Should be removed if disconnectClient returned true
        }
#endif
    }

    EXPECT_TRUE(server_->stop());
}

// Test loadConfig and saveConfig
TEST_F(SshServerTest, LoadSaveConfig) {
    // Modify some settings
    server_->setPort(9000);
    server_->setListenAddress("1.2.3.4");
    server_->setPasswordAuthentication(false);
    server_->setLogLevel(LogLevel::DEBUG);
    server_->setSubsystem("testsub", "/bin/echo");
    server_->allowIpAddress("192.168.1.100");
    server_->denyIpAddress("192.168.1.101");
    server_->setServerVersion("TEST-VERSION");

    // Start and stop to trigger saveConfig (implicitly called by start)
    EXPECT_TRUE(server_->start());
    EXPECT_TRUE(server_->stop());

    // Create a new server instance to load the saved config
    SshServer new_server(config_file_);
    // Need to explicitly call start to load config (or expose a load method)
    // The Impl constructor calls loadConfig, so just creating a new server is
    // enough.

    EXPECT_EQ(new_server.getPort(), 9000);
    EXPECT_EQ(new_server.getListenAddress(), "1.2.3.4");
    EXPECT_FALSE(new_server.isPasswordAuthenticationEnabled());
    EXPECT_EQ(new_server.getLogLevel(), LogLevel::DEBUG);
    EXPECT_EQ(new_server.getSubsystem("testsub"), "/bin/echo");
    EXPECT_TRUE(new_server.isIpAddressAllowed("192.168.1.100"));
    EXPECT_FALSE(new_server.isIpAddressAllowed("192.168.1.101"));
    EXPECT_EQ(new_server.getServerVersion(), "TEST-VERSION");
}

}  // namespace atom::connection::test