/*
 * sshserver.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-5-24

Description: SSH Server

*************************************************/

#ifndef ATOM_CONNECTION_SSHSERVER_HPP
#define ATOM_CONNECTION_SSHSERVER_HPP

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "atom/type/noncopyable.hpp"

#include "atom/macro.hpp"

namespace atom::connection {

/**
 * @struct SshConnection
 * @brief Represents information about an active SSH connection.
 */
struct SshConnection {
    std::string username;   ///< Username used for the connection
    std::string ipAddress;  ///< Remote IP address
    int port;               ///< Remote port
    std::chrono::system_clock::time_point connectedTime;  ///< Connection time
    std::string sessionId;  ///< Unique session identifier
};

/**
 * @enum LogLevel
 * @brief Defines different levels of logging for the SSH server.
 */
enum class LogLevel {
    QUIET,    ///< No logging
    FATAL,    ///< Log only fatal errors
    ERROR,    ///< Log errors
    INFO,     ///< Log general information
    VERBOSE,  ///< Log detailed information
    DEBUG,    ///< Log debug information
    DEBUG1,   ///< More detailed debug information (level 1)
    DEBUG2,   ///< More detailed debug information (level 2)
    DEBUG3    ///< Most detailed debug information (level 3)
};

/**
 * @class SshServer
 * @brief Represents an SSH server for handling secure shell connections.
 *
 * This class provides methods to configure and manage an SSH server, handling
 * client connections and user authentication through various methods including
 * public key and password authentication.
 */
class SshServer : public NonCopyable {
public:
    /**
     * @brief Constructor for SshServer.
     *
     * Initializes the SSH server with a specified configuration file.
     *
     * @param configFile The path to the configuration file for the SSH server.
     */
    explicit SshServer(const std::filesystem::path& configFile);

    /**
     * @brief Destructor for SshServer.
     *
     * Cleans up resources used by the SSH server.
     */
    ~SshServer() override;

    /**
     * @brief Starts the SSH server.
     *
     * This method will begin listening for incoming connections on the
     * configured port and address.
     *
     * @return true if the server was started successfully, false otherwise.
     */
    ATOM_NODISCARD auto start() -> bool;

    /**
     * @brief Stops the SSH server.
     *
     * This method will stop the server from accepting new connections and
     * cleanly shut down any existing connections.
     *
     * @param force If true, forcefully terminate the server even if it has
     * active connections.
     * @return true if the server was stopped successfully, false otherwise.
     */
    ATOM_NODISCARD auto stop(bool force = false) -> bool;

    /**
     * @brief Restarts the SSH server.
     *
     * Stops the server if it's running and then starts it again.
     *
     * @return true if the server was restarted successfully, false otherwise.
     */
    ATOM_NODISCARD auto restart() -> bool;

    /**
     * @brief Checks if the SSH server is currently running.
     *
     * @return true if the server is running, false otherwise.
     */
    ATOM_NODISCARD auto isRunning() const -> bool;

    /**
     * @brief Sets the port on which the SSH server listens for connections.
     *
     * @param port The port number to listen on.
     *
     * This method updates the server's listening port to the specified value.
     */
    void setPort(int port);

    /**
     * @brief Gets the port on which the SSH server is listening.
     *
     * @return The current listening port.
     */
    ATOM_NODISCARD auto getPort() const -> int;

    /**
     * @brief Sets the address on which the SSH server listens for connections.
     *
     * @param address The IP address or hostname for listening.
     *
     * The server will bind to this address, allowing connections from it.
     */
    void setListenAddress(const std::string& address);

    /**
     * @brief Gets the address on which the SSH server is listening.
     *
     * @return The current listening address as a string.
     */
    ATOM_NODISCARD auto getListenAddress() const -> std::string;

    /**
     * @brief Sets the host key file used for SSH connections.
     *
     * @param keyFile The path to the host key file.
     *
     * The host key is used to establish the identity of the server,
     * enabling secure communication with clients.
     */
    void setHostKey(const std::filesystem::path& keyFile);

    /**
     * @brief Gets the path to the host key file.
     *
     * @return The current host key file path.
     */
    ATOM_NODISCARD auto getHostKey() const -> std::filesystem::path;

    /**
     * @brief Sets the list of authorized public key files for user
     * authentication.
     *
     * @param keyFiles A vector of paths to public key files.
     *
     * This method updates the SSH server to allow authentication using the
     * specified public keys.
     */
    void setAuthorizedKeys(const std::vector<std::filesystem::path>& keyFiles);

    /**
     * @brief Gets the list of authorized public key files.
     *
     * @return A vector of paths to authorized public key files.
     */
    ATOM_NODISCARD auto getAuthorizedKeys() const
        -> std::vector<std::filesystem::path>;

    /**
     * @brief Enables or disables root login to the SSH server.
     *
     * @param allow true to permit root login, false to deny it.
     *
     * This method must be configured with caution, as enabling root login
     * can pose a security risk.
     */
    void allowRootLogin(bool allow);

    /**
     * @brief Checks if root login is allowed.
     *
     * @return true if root login is permitted, false otherwise.
     */
    ATOM_NODISCARD auto isRootLoginAllowed() const -> bool;

    /**
     * @brief Enables or disables password authentication for the SSH server.
     *
     * @param enable true to enable password authentication, false to disable
     * it.
     */
    void setPasswordAuthentication(bool enable);

    /**
     * @brief Checks if password authentication is enabled.
     *
     * @return true if password authentication is enabled, false otherwise.
     */
    ATOM_NODISCARD auto isPasswordAuthenticationEnabled() const -> bool;

    /**
     * @brief Sets a subsystem for handling a specific command.
     *
     * @param name The name of the subsystem.
     * @param command The command that the subsystem will execute.
     *
     * This allows for additional functionality to be added to the SSH server,
     * such as file transfers or other custom commands.
     */
    void setSubsystem(const std::string& name, const std::string& command);

    /**
     * @brief Removes a previously set subsystem by name.
     *
     * @param name The name of the subsystem to remove.
     *
     * After this method is called, the subsystem will no longer be available.
     */
    void removeSubsystem(const std::string& name);

    /**
     * @brief Gets the command associated with a subsystem by name.
     *
     * @param name The name of the subsystem.
     * @return The command associated with the subsystem.
     *
     * If the subsystem does not exist, an empty string may be returned.
     */
    ATOM_NODISCARD auto getSubsystem(const std::string& name) const
        -> std::string;

    /**
     * @brief Gets all active connections to the server.
     *
     * @return A vector of SshConnection structures representing active
     * connections.
     */
    ATOM_NODISCARD auto getActiveConnections() const
        -> std::vector<SshConnection>;

    /**
     * @brief Disconnects a specific client session.
     *
     * @param sessionId The unique session ID to disconnect.
     * @return true if the client was disconnected, false if the session wasn't
     * found.
     */
    ATOM_NODISCARD auto disconnectClient(const std::string& sessionId) -> bool;

    /**
     * @brief Sets the maximum number of authentication attempts allowed.
     *
     * @param maxAttempts The maximum number of attempts before blocking a
     * client.
     */
    void setMaxAuthAttempts(int maxAttempts);

    /**
     * @brief Gets the maximum number of authentication attempts allowed.
     *
     * @return The current maximum authentication attempts setting.
     */
    ATOM_NODISCARD auto getMaxAuthAttempts() const -> int;

    /**
     * @brief Sets the maximum number of concurrent connections allowed.
     *
     * @param maxConnections The maximum number of simultaneous connections.
     */
    void setMaxConnections(int maxConnections);

    /**
     * @brief Gets the maximum number of concurrent connections allowed.
     *
     * @return The current maximum connections setting.
     */
    ATOM_NODISCARD auto getMaxConnections() const -> int;

    /**
     * @brief Sets the login grace time in seconds.
     *
     * @param seconds The number of seconds to wait for authentication before
     * disconnecting.
     */
    void setLoginGraceTime(int seconds);

    /**
     * @brief Gets the current login grace time.
     *
     * @return The login grace time in seconds.
     */
    ATOM_NODISCARD auto getLoginGraceTime() const -> int;

    /**
     * @brief Sets the server's idle timeout.
     *
     * @param seconds The number of seconds after which idle connections are
     * terminated.
     */
    void setIdleTimeout(int seconds);

    /**
     * @brief Gets the current idle timeout.
     *
     * @return The idle timeout in seconds.
     */
    ATOM_NODISCARD auto getIdleTimeout() const -> int;

    /**
     * @brief Adds an IP address to the allowed list.
     *
     * @param ipAddress The IP address to allow.
     */
    void allowIpAddress(const std::string& ipAddress);

    /**
     * @brief Adds an IP address to the denied list.
     *
     * @param ipAddress The IP address to deny.
     */
    void denyIpAddress(const std::string& ipAddress);

    /**
     * @brief Checks if an IP address is allowed to connect.
     *
     * @param ipAddress The IP address to check.
     * @return true if the IP is allowed, false otherwise.
     */
    ATOM_NODISCARD auto isIpAddressAllowed(const std::string& ipAddress) const
        -> bool;

    /**
     * @brief Sets whether to allow agent forwarding.
     *
     * @param allow true to allow agent forwarding, false to disallow.
     */
    void allowAgentForwarding(bool allow);

    /**
     * @brief Checks if agent forwarding is allowed.
     *
     * @return true if agent forwarding is allowed, false otherwise.
     */
    ATOM_NODISCARD auto isAgentForwardingAllowed() const -> bool;

    /**
     * @brief Sets whether to allow TCP forwarding.
     *
     * @param allow true to allow TCP forwarding, false to disallow.
     */
    void allowTcpForwarding(bool allow);

    /**
     * @brief Checks if TCP forwarding is allowed.
     *
     * @return true if TCP forwarding is allowed, false otherwise.
     */
    ATOM_NODISCARD auto isTcpForwardingAllowed() const -> bool;

    /**
     * @brief Sets the server's log level.
     *
     * @param level The desired log level.
     */
    void setLogLevel(LogLevel level);

    /**
     * @brief Gets the server's current log level.
     *
     * @return The current log level.
     */
    ATOM_NODISCARD auto getLogLevel() const -> LogLevel;

    /**
     * @brief Sets the log file path.
     *
     * @param logFile The path where logs should be written.
     */
    void setLogFile(const std::filesystem::path& logFile);

    /**
     * @brief Gets the current log file path.
     *
     * @return The log file path.
     */
    ATOM_NODISCARD auto getLogFile() const -> std::filesystem::path;

    /**
     * @brief Generates a new host key.
     *
     * @param keyType The type of key to generate (e.g., "rsa", "ed25519").
     * @param keySize The size of the key in bits (for key types that support
     * it).
     * @param outputPath Where to save the generated key.
     * @return true if the key was generated successfully, false otherwise.
     */
    ATOM_NODISCARD auto generateHostKey(const std::string& keyType, int keySize,
                                        const std::filesystem::path& outputPath)
        -> bool;

    /**
     * @brief Verifies the current server configuration.
     *
     * @return true if the configuration is valid, false if there are issues.
     */
    ATOM_NODISCARD auto verifyConfiguration() const -> bool;

    /**
     * @brief Gets detailed information about configuration problems.
     *
     * @return A vector of strings describing configuration issues.
     */
    ATOM_NODISCARD auto getConfigurationIssues() const
        -> std::vector<std::string>;

    /**
     * @brief Sets allowed ciphers for encryption.
     *
     * @param ciphers A comma-separated list of cipher algorithms.
     */
    void setCiphers(const std::string& ciphers);

    /**
     * @brief Gets the currently allowed ciphers.
     *
     * @return A string containing the allowed ciphers.
     */
    ATOM_NODISCARD auto getCiphers() const -> std::string;

    /**
     * @brief Sets allowed MACs (Message Authentication Codes).
     *
     * @param macs A comma-separated list of MAC algorithms.
     */
    void setMACs(const std::string& macs);

    /**
     * @brief Gets the currently allowed MACs.
     *
     * @return A string containing the allowed MACs.
     */
    ATOM_NODISCARD auto getMACs() const -> std::string;

    /**
     * @brief Sets allowed key exchange algorithms.
     *
     * @param kexAlgorithms A comma-separated list of key exchange algorithms.
     */
    void setKexAlgorithms(const std::string& kexAlgorithms);

    /**
     * @brief Gets the currently allowed key exchange algorithms.
     *
     * @return A string containing the allowed key exchange algorithms.
     */
    ATOM_NODISCARD auto getKexAlgorithms() const -> std::string;

    /**
     * @brief Registers a callback function to be called when a new connection
     * is established.
     *
     * @param callback The function to call when a new connection is
     * established.
     */
    void onNewConnection(std::function<void(const SshConnection&)> callback);

    /**
     * @brief Registers a callback function to be called when a connection is
     * closed.
     *
     * @param callback The function to call when a connection is closed.
     */
    void onConnectionClosed(std::function<void(const SshConnection&)> callback);

    /**
     * @brief Registers a callback function to be called when authentication
     * fails.
     *
     * @param callback The function to call when authentication fails.
     */
    void onAuthenticationFailure(
        std::function<void(const std::string&, const std::string&)> callback);

    /**
     * @brief Gets server statistics.
     *
     * @return A map of statistic name to value.
     */
    ATOM_NODISCARD auto getStatistics() const
        -> std::unordered_map<std::string, std::string>;

    /**
     * @brief Gets the server version.
     *
     * @return The server version string.
     */
    ATOM_NODISCARD auto getServerVersion() const -> std::string;

    /**
     * @brief Sets the server version string.
     *
     * @param version The server version string to use.
     */
    void setServerVersion(const std::string& version);

private:
    class Impl;  ///< Forward declaration of the implementation class.
    std::unique_ptr<Impl> impl_;  ///< Pointer to the implementation object
                                  ///< holding the core functionalities.
};

}  // namespace atom::connection

#endif  // ATOM_CONNECTION_SSHSERVER_HPP