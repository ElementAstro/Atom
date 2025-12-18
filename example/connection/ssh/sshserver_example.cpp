/*
 * sshserver_example.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-12

Description: Comprehensive example usage of the SshServer class.
Demonstrates all features including:
- Basic server configuration
- Port and address settings
- Host key management
- Authentication configuration
- IP filtering (whitelist/blacklist)
- Subsystem configuration
- Connection callbacks
- Logging and statistics
- Security settings

Note: Requires libssh library to be available.

**************************************************/

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#if __has_include(<libssh/libssh.h>)
#include "atom/connection/ssh/sshserver.hpp"

namespace {

class Logger {
public:
    enum Level { LOG_INFO, LOG_SUCCESS, LOG_WARNING, LOG_ERR, LOG_DEBUG };

    static void log(Level level, const std::string& component,
                    const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()) %
                  1000;

        std::cout << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S")
                  << "." << std::setfill('0') << std::setw(3) << ms.count()
                  << "] ";

        switch (level) {
            case LOG_INFO:
                std::cout << "[INFO]    ";
                break;
            case LOG_SUCCESS:
                std::cout << "[SUCCESS] ";
                break;
            case LOG_WARNING:
                std::cout << "[WARN]    ";
                break;
            case LOG_ERR:
                std::cout << "[ERROR]   ";
                break;
            case LOG_DEBUG:
                std::cout << "[DEBUG]   ";
                break;
        }

        std::cout << "[" << component << "] " << message << std::endl;
    }
};

}  // namespace

// Example 1: Basic SSH server
void basicServerExample() {
    Logger::log(Logger::LOG_INFO, "Example1", "=== Basic SSH Server ===");

    try {
        // Create server with config file path
        std::filesystem::path configPath = "/etc/ssh/sshd_config";
        atom::connection::SshServer server(configPath);

        Logger::log(Logger::LOG_INFO, "Example1", "Created SshServer");

        // Configure basic settings
        server.setPort(2222);
        server.setListenAddress("0.0.0.0");

        Logger::log(Logger::LOG_INFO, "Example1",
                    "Port: " + std::to_string(server.getPort()));
        Logger::log(Logger::LOG_INFO, "Example1",
                    "Listen address: " + server.getListenAddress());

        // Start server
        server.start();
        Logger::log(Logger::LOG_SUCCESS, "Example1",
                    "Server started, isRunning: " +
                        std::string(server.isRunning() ? "yes" : "no"));

        // Let it run briefly
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Stop server
        server.stop();
        Logger::log(Logger::LOG_INFO, "Example1", "Server stopped");

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example1",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example1",
                "Basic server example completed\n");
}

// Example 2: Host key configuration
void hostKeyExample() {
    Logger::log(Logger::LOG_INFO, "Example2", "=== Host Key Configuration ===");

    try {
        std::filesystem::path configPath = "/etc/ssh/sshd_config";
        atom::connection::SshServer server(configPath);

        // Set host key file
        std::string hostKeyPath = "/etc/ssh/ssh_host_rsa_key";
        server.setHostKey(hostKeyPath);
        Logger::log(Logger::LOG_INFO, "Example2",
                    "Set host key: " + hostKeyPath);

        // Generate new host key
        std::string newKeyPath = "/tmp/ssh_example_host_key";
        server.generateHostKey(newKeyPath, 4096);
        Logger::log(Logger::LOG_SUCCESS, "Example2",
                    "Generated new host key: " + newKeyPath);

        // Use the new key
        server.setHostKey(newKeyPath);
        Logger::log(Logger::LOG_INFO, "Example2", "Using new host key");

        server.setPort(2223);
        server.start();
        Logger::log(Logger::LOG_SUCCESS, "Example2",
                    "Server started with custom host key");

        std::this_thread::sleep_for(std::chrono::seconds(1));
        server.stop();

        // Clean up
        std::filesystem::remove(newKeyPath);
        std::filesystem::remove(newKeyPath + ".pub");

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example2",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example2", "Host key example completed\n");
}

// Example 3: Authentication configuration
void authenticationExample() {
    Logger::log(Logger::LOG_INFO, "Example3",
                "=== Authentication Configuration ===");

    try {
        std::filesystem::path configPath = "/etc/ssh/sshd_config";
        atom::connection::SshServer server(configPath);

        // Password authentication
        server.setPasswordAuthentication(true);
        Logger::log(Logger::LOG_INFO, "Example3",
                    "Password authentication: enabled");

        // Public key authentication
        server.setPublicKeyAuthentication(true);
        Logger::log(Logger::LOG_INFO, "Example3",
                    "Public key authentication: enabled");

        // Keyboard-interactive authentication
        server.setKeyboardInteractiveAuthentication(false);
        Logger::log(Logger::LOG_INFO, "Example3",
                    "Keyboard-interactive: disabled");

        // Root login
        server.allowRootLogin(false);
        Logger::log(Logger::LOG_INFO, "Example3", "Root login: disabled");

        // Authorized keys file
        server.setAuthorizedKeysFile("~/.ssh/authorized_keys");
        Logger::log(Logger::LOG_INFO, "Example3",
                    "Authorized keys file: ~/.ssh/authorized_keys");

        // Max authentication attempts
        server.setMaxAuthTries(3);
        Logger::log(Logger::LOG_INFO, "Example3", "Max auth tries: 3");

        // Login grace time
        server.setLoginGraceTime(std::chrono::seconds(60));
        Logger::log(Logger::LOG_INFO, "Example3",
                    "Login grace time: 60 seconds");

        server.setPort(2224);
        server.start();
        Logger::log(Logger::LOG_SUCCESS, "Example3",
                    "Server started with auth config");

        std::this_thread::sleep_for(std::chrono::seconds(1));
        server.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example3",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example3",
                "Authentication example completed\n");
}

// Example 4: IP filtering
void ipFilteringExample() {
    Logger::log(Logger::LOG_INFO, "Example4", "=== IP Filtering ===");

    try {
        std::filesystem::path configPath = "/etc/ssh/sshd_config";
        atom::connection::SshServer server(configPath);

        // Add to whitelist
        server.addToWhitelist("192.168.1.0/24");
        server.addToWhitelist("10.0.0.0/8");
        server.addToWhitelist("127.0.0.1");
        Logger::log(Logger::LOG_INFO, "Example4", "Added IPs to whitelist");

        // Add to blacklist
        server.addToBlacklist("192.168.1.100");
        server.addToBlacklist("10.0.0.50");
        Logger::log(Logger::LOG_INFO, "Example4", "Added IPs to blacklist");

        // Check if IP is allowed
        std::vector<std::string> testIPs = {"192.168.1.50", "192.168.1.100",
                                            "10.0.0.1", "172.16.0.1"};

        for (const auto& ip : testIPs) {
            bool allowed = server.isIpAllowed(ip);
            Logger::log(allowed ? Logger::LOG_SUCCESS : Logger::LOG_WARNING,
                        "Example4",
                        ip + ": " + (allowed ? "ALLOWED" : "BLOCKED"));
        }

        // Remove from blacklist
        server.removeFromBlacklist("192.168.1.100");
        Logger::log(Logger::LOG_INFO, "Example4",
                    "Removed 192.168.1.100 from blacklist");

        // Clear lists
        server.clearWhitelist();
        server.clearBlacklist();
        Logger::log(Logger::LOG_INFO, "Example4",
                    "Cleared whitelist and blacklist");

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example4",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example4",
                "IP filtering example completed\n");
}

// Example 5: Subsystem configuration
void subsystemExample() {
    Logger::log(Logger::LOG_INFO, "Example5",
                "=== Subsystem Configuration ===");

    try {
        std::filesystem::path configPath = "/etc/ssh/sshd_config";
        atom::connection::SshServer server(configPath);

        // Add SFTP subsystem
        server.addSubsystem("sftp", "/usr/lib/openssh/sftp-server");
        Logger::log(Logger::LOG_INFO, "Example5", "Added SFTP subsystem");

        // Add custom subsystem
        server.addSubsystem("custom", "/usr/local/bin/custom-handler");
        Logger::log(Logger::LOG_INFO, "Example5", "Added custom subsystem");

        // Remove subsystem
        server.removeSubsystem("custom");
        Logger::log(Logger::LOG_INFO, "Example5", "Removed custom subsystem");

        server.setPort(2225);
        server.start();
        Logger::log(Logger::LOG_SUCCESS, "Example5",
                    "Server started with subsystems");

        std::this_thread::sleep_for(std::chrono::seconds(1));
        server.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example5",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example5", "Subsystem example completed\n");
}

// Example 6: Connection callbacks
void callbacksExample() {
    Logger::log(Logger::LOG_INFO, "Example6", "=== Connection Callbacks ===");

    try {
        std::filesystem::path configPath = "/etc/ssh/sshd_config";
        atom::connection::SshServer server(configPath);

        // Set connection callback
        server.setConnectionCallback(
            [](const std::string& clientIp, bool connected) {
                if (connected) {
                    Logger::log(Logger::LOG_SUCCESS, "Callback",
                                "Client connected: " + clientIp);
                } else {
                    Logger::log(Logger::LOG_INFO, "Callback",
                                "Client disconnected: " + clientIp);
                }
            });

        // Set authentication failure callback
        server.setAuthFailureCallback([](const std::string& username,
                                         const std::string& clientIp,
                                         const std::string& method) {
            Logger::log(Logger::LOG_WARNING, "AuthFail",
                        "Auth failed - User: " + username +
                            ", IP: " + clientIp + ", Method: " + method);
        });

        Logger::log(Logger::LOG_INFO, "Example6", "Callbacks registered");

        server.setPort(2226);
        server.start();
        Logger::log(Logger::LOG_SUCCESS, "Example6",
                    "Server started with callbacks");

        std::this_thread::sleep_for(std::chrono::seconds(2));
        server.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example6",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example6", "Callbacks example completed\n");
}

// Example 7: Security settings
void securitySettingsExample() {
    Logger::log(Logger::LOG_INFO, "Example7", "=== Security Settings ===");

    try {
        std::filesystem::path configPath = "/etc/ssh/sshd_config";
        atom::connection::SshServer server(configPath);

        // Set ciphers
        std::vector<std::string> ciphers = {
            "aes256-gcm@openssh.com", "aes128-gcm@openssh.com", "aes256-ctr",
            "aes192-ctr", "aes128-ctr"};
        server.setCiphers(ciphers);
        Logger::log(Logger::LOG_INFO, "Example7",
                    "Set " + std::to_string(ciphers.size()) + " ciphers");

        // Set MACs
        std::vector<std::string> macs = {"hmac-sha2-512-etm@openssh.com",
                                         "hmac-sha2-256-etm@openssh.com",
                                         "hmac-sha2-512", "hmac-sha2-256"};
        server.setMACs(macs);
        Logger::log(Logger::LOG_INFO, "Example7",
                    "Set " + std::to_string(macs.size()) + " MACs");

        // Set key exchange algorithms
        std::vector<std::string> kexAlgorithms = {
            "curve25519-sha256", "curve25519-sha256@libssh.org",
            "ecdh-sha2-nistp521", "ecdh-sha2-nistp384", "ecdh-sha2-nistp256"};
        server.setKexAlgorithms(kexAlgorithms);
        Logger::log(
            Logger::LOG_INFO, "Example7",
            "Set " + std::to_string(kexAlgorithms.size()) + " KEX algorithms");

        // Strict modes
        server.setStrictModes(true);
        Logger::log(Logger::LOG_INFO, "Example7", "Strict modes: enabled");

        // TCP forwarding
        server.setTcpForwarding(false);
        Logger::log(Logger::LOG_INFO, "Example7", "TCP forwarding: disabled");

        // X11 forwarding
        server.setX11Forwarding(false);
        Logger::log(Logger::LOG_INFO, "Example7", "X11 forwarding: disabled");

        // Agent forwarding
        server.setAgentForwarding(false);
        Logger::log(Logger::LOG_INFO, "Example7", "Agent forwarding: disabled");

        server.setPort(2227);
        server.start();
        Logger::log(Logger::LOG_SUCCESS, "Example7",
                    "Server started with security settings");

        std::this_thread::sleep_for(std::chrono::seconds(1));
        server.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example7",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example7",
                "Security settings example completed\n");
}

// Example 8: Logging configuration
void loggingExample() {
    Logger::log(Logger::LOG_INFO, "Example8", "=== Logging Configuration ===");

    try {
        std::filesystem::path configPath = "/etc/ssh/sshd_config";
        atom::connection::SshServer server(configPath);

        // Enable logging
        server.enableLogging(true);
        Logger::log(Logger::LOG_INFO, "Example8", "Logging enabled");

        // Set log level
        server.setLogLevel(atom::connection::SshServer::LogLevel::DEBUG);
        Logger::log(Logger::LOG_INFO, "Example8", "Log level: DEBUG");

        // Set log file
        server.setLogFile("/tmp/ssh_server_example.log");
        Logger::log(Logger::LOG_INFO, "Example8",
                    "Log file: /tmp/ssh_server_example.log");

        server.setPort(2228);
        server.start();
        Logger::log(Logger::LOG_SUCCESS, "Example8",
                    "Server started with logging");

        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Get statistics
        auto stats = server.getStatistics();
        Logger::log(Logger::LOG_INFO, "Example8", "=== Server Statistics ===");
        Logger::log(
            Logger::LOG_INFO, "Example8",
            "Total connections: " + std::to_string(stats.total_connections));
        Logger::log(
            Logger::LOG_INFO, "Example8",
            "Active connections: " + std::to_string(stats.active_connections));
        Logger::log(Logger::LOG_INFO, "Example8",
                    "Auth failures: " + std::to_string(stats.auth_failures));
        Logger::log(Logger::LOG_INFO, "Example8",
                    "Bytes received: " + std::to_string(stats.bytes_received));
        Logger::log(Logger::LOG_INFO, "Example8",
                    "Bytes sent: " + std::to_string(stats.bytes_sent));

        server.stop();

        // Clean up log file
        std::filesystem::remove("/tmp/ssh_server_example.log");

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example8",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example8", "Logging example completed\n");
}

// Example 9: Banner and MOTD
void bannerExample() {
    Logger::log(Logger::LOG_INFO, "Example9", "=== Banner and MOTD ===");

    try {
        std::filesystem::path configPath = "/etc/ssh/sshd_config";
        atom::connection::SshServer server(configPath);

        // Set banner
        std::string banner = R"(
*******************************************
*  Welcome to Example SSH Server          *
*  Unauthorized access is prohibited      *
*******************************************
)";
        server.setBanner(banner);
        Logger::log(Logger::LOG_INFO, "Example9", "Banner set");

        // Print MOTD
        server.setPrintMotd(true);
        Logger::log(Logger::LOG_INFO, "Example9", "Print MOTD: enabled");

        server.setPort(2229);
        server.start();
        Logger::log(Logger::LOG_SUCCESS, "Example9",
                    "Server started with banner");

        std::this_thread::sleep_for(std::chrono::seconds(1));
        server.stop();

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example9",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example9", "Banner example completed\n");
}

// Example 10: Complete server configuration
void completeConfigExample() {
    Logger::log(Logger::LOG_INFO, "Example10",
                "=== Complete Server Configuration ===");

    try {
        std::filesystem::path configPath = "/etc/ssh/sshd_config";
        atom::connection::SshServer server(configPath);

        // Network settings
        server.setPort(2230);
        server.setListenAddress("0.0.0.0");

        // Authentication
        server.setPasswordAuthentication(true);
        server.setPublicKeyAuthentication(true);
        server.allowRootLogin(false);
        server.setMaxAuthTries(3);
        server.setLoginGraceTime(std::chrono::seconds(60));

        // Security
        server.setStrictModes(true);
        server.setTcpForwarding(false);
        server.setX11Forwarding(false);

        // Callbacks
        server.setConnectionCallback([](const std::string& ip, bool connected) {
            Logger::log(Logger::LOG_INFO, "Server",
                        (connected ? "Connected: " : "Disconnected: ") + ip);
        });

        // Logging
        server.enableLogging(true);
        server.setLogLevel(atom::connection::SshServer::LogLevel::INFO);

        // Banner
        server.setBanner("Welcome to the secure SSH server!\n");

        Logger::log(Logger::LOG_INFO, "Example10",
                    "Complete configuration applied");

        // Start server
        server.start();
        Logger::log(
            Logger::LOG_SUCCESS, "Example10",
            "Server started on port " + std::to_string(server.getPort()));

        // Run for a while
        Logger::log(Logger::LOG_INFO, "Example10",
                    "Server running for 5 seconds...");
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            Logger::log(Logger::LOG_DEBUG, "Example10",
                        "Heartbeat " + std::to_string(i + 1));
        }

        // Get final statistics
        auto stats = server.getStatistics();
        Logger::log(Logger::LOG_INFO, "Example10", "Final stats:");
        Logger::log(
            Logger::LOG_INFO, "Example10",
            "  Total connections: " + std::to_string(stats.total_connections));

        server.stop();
        Logger::log(Logger::LOG_SUCCESS, "Example10",
                    "Server stopped gracefully");

    } catch (const std::exception& e) {
        Logger::log(Logger::LOG_ERR, "Example10",
                    "Exception: " + std::string(e.what()));
    }

    Logger::log(Logger::LOG_INFO, "Example10",
                "Complete config example completed\n");
}

int main() {
    Logger::log(Logger::LOG_INFO, "Main",
                "========================================");
    Logger::log(Logger::LOG_INFO, "Main", "  SshServer Comprehensive Examples");
    Logger::log(Logger::LOG_INFO, "Main",
                "========================================");
    Logger::log(Logger::LOG_WARNING, "Main",
                "Note: Requires root/admin privileges and libssh library");
    Logger::log(Logger::LOG_INFO, "Main", "");

    // Run all examples
    basicServerExample();
    hostKeyExample();
    authenticationExample();
    ipFilteringExample();
    subsystemExample();
    callbacksExample();
    securitySettingsExample();
    loggingExample();
    bannerExample();
    completeConfigExample();

    Logger::log(Logger::LOG_SUCCESS, "Main",
                "========================================");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "  All SshServer examples completed!");
    Logger::log(Logger::LOG_SUCCESS, "Main",
                "========================================");

    return 0;
}

#else
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  SshServer Example" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    std::cout << "This example requires libssh library to be available."
              << std::endl;
    std::cout << "Please install libssh and rebuild with SSH support enabled."
              << std::endl;
    std::cout << std::endl;
    std::cout << "Installation:" << std::endl;
    std::cout << "  Ubuntu/Debian: sudo apt install libssh-dev" << std::endl;
    std::cout << "  Fedora/RHEL:   sudo dnf install libssh-devel" << std::endl;
    std::cout << "  macOS:         brew install libssh" << std::endl;
    std::cout << "  Windows:       vcpkg install libssh" << std::endl;
    std::cout << std::endl;
    std::cout << "Note: Running an SSH server requires root/admin privileges."
              << std::endl;
    return 0;
}
#endif
