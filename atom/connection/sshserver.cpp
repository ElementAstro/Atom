/*
 * sshserver.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-5-24

Description: SSH Server

*************************************************/

#include "sshserver.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#undef ERROR
// clang-format on
#else
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <csignal>
#endif

#include "atom/error/exception.hpp"

namespace atom::connection {

class SshServer::Impl {
public:
    explicit Impl(const std::filesystem::path& configFile)
        : configFile_(configFile), isRunning_(false), processId_(0) {
        // Set default values for new parameters
        maxAuthAttempts_ = 6;
        maxConnections_ = 10;
        loginGraceTime_ = 120;
        idleTimeout_ = 300;
        logLevel_ = LogLevel::INFO;
        allowAgentForwarding_ = false;
        allowTcpForwarding_ = false;

        // Default secure ciphers and algorithms
        ciphers_ =
            "chacha20-poly1305@openssh.com,aes256-gcm@openssh.com,aes128-gcm@"
            "openssh.com,aes256-ctr,aes192-ctr,aes128-ctr";
        macs_ =
            "hmac-sha2-512-etm@openssh.com,hmac-sha2-256-etm@openssh.com,hmac-"
            "sha2-512,hmac-sha2-256";
        kexAlgorithms_ =
            "curve25519-sha256@libssh.org,diffie-hellman-group-exchange-sha256";
        serverVersion_ = "SSH-2.0-AtomSSH_1.0";

        loadConfig();
    }

    ~Impl() {
        if (isRunning_) {
            stop(true);
        }
    }

    bool start() {
        std::lock_guard<std::mutex> lock(serverMutex_);

        if (isRunning_) {
            return false;
        }

        // Verify configuration before starting
        auto issues = getConfigurationIssues();
        if (!issues.empty()) {
            lastError_ = "Configuration issues detected: " + issues[0];
            return false;
        }

        saveConfig();

        try {
#ifdef _WIN32
            SECURITY_ATTRIBUTES sa;
            sa.nLength = sizeof(SECURITY_ATTRIBUTES);
            sa.bInheritHandle = TRUE;
            sa.lpSecurityDescriptor = NULL;

            // Create pipe for stdout
            HANDLE hStdOutRead, hStdOutWrite;
            if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
                lastError_ = "Failed to create pipe for process output";
                return false;
            }
            SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFO si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(STARTUPINFO));
            si.cb = sizeof(STARTUPINFO);
            si.hStdOutput = hStdOutWrite;
            si.hStdError = hStdOutWrite;
            si.dwFlags |= STARTF_USESTDHANDLES;

            std::string command = "sshd -f \"" + configFile_.string() + "\"";

            if (CreateProcess(NULL, const_cast<char*>(command.c_str()), NULL,
                              NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si,
                              &pi)) {
                processId_ = pi.dwProcessId;
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
                isRunning_ = true;

                // Start monitoring thread
                monitorThread_ = std::thread(&Impl::monitorSshd, this);

                CloseHandle(hStdOutWrite);
                CloseHandle(hStdOutRead);
                return true;
            } else {
                lastError_ = "Failed to start SSH server process";
                CloseHandle(hStdOutWrite);
                CloseHandle(hStdOutRead);
                return false;
            }
#else
            pid_t pid = fork();
            if (pid < 0) {
                lastError_ = "Fork failed when starting SSH server";
                return false;
            } else if (pid == 0) {
                // Child process
                // Redirect output to log file if set
                if (!logFile_.empty()) {
                    int fd = open(logFile_.c_str(),
                                  O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (fd != -1) {
                        dup2(fd, STDOUT_FILENO);
                        dup2(fd, STDERR_FILENO);
                        close(fd);
                    }
                }

                // Convert LogLevel to sshd -d argument
                std::string debugLevel;
                if (logLevel_ >= LogLevel::DEBUG1) {
                    int level = static_cast<int>(logLevel_) -
                                static_cast<int>(LogLevel::DEBUG) + 1;
                    debugLevel = " -" + std::string(level, 'd');
                }

                // Execute sshd
                std::string command = "/usr/sbin/sshd -f \"" +
                                      configFile_.string() + "\"" + debugLevel;
                execlp("/bin/sh", "sh", "-c", command.c_str(), NULL);

                // If we get here, exec failed
                exit(1);
            } else {
                // Parent process
                processId_ = pid;
                isRunning_ = true;

                // Start monitoring thread
                monitorThread_ = std::thread(&Impl::monitorSshd, this);
                return true;
            }
#endif
        } catch (const std::exception& e) {
            lastError_ =
                std::string("Exception when starting SSH server: ") + e.what();
            return false;
        }
    }

    bool stop(bool force = false) {
        std::lock_guard<std::mutex> lock(serverMutex_);

        if (!isRunning_) {
            return false;
        }

        // Check active connections if not forcing
        if (!force && !activeConnections_.empty()) {
            lastError_ =
                "Cannot stop server with active connections unless force=true";
            return false;
        }

        try {
#ifdef _WIN32
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId_);
            if (hProcess) {
                bool success = TerminateProcess(hProcess, 0);
                CloseHandle(hProcess);

                if (success) {
                    isRunning_ = false;
                    processId_ = 0;

                    // Stop the monitoring thread
                    if (monitorThread_.joinable()) {
                        monitorThreadStop_ = true;
                        monitorThread_.join();
                    }

                    // Clear active connections
                    activeConnections_.clear();
                    return true;
                } else {
                    lastError_ = "Failed to terminate SSH server process";
                    return false;
                }
            } else {
                lastError_ = "Failed to open SSH server process";
                return false;
            }
#else
            // First try graceful shutdown with SIGTERM
            if (!force && kill(processId_, SIGTERM) == 0) {
                // Wait briefly for the process to terminate
                for (int i = 0; i < 10; i++) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    if (kill(processId_, 0) != 0) {
                        // Process has exited
                        isRunning_ = false;
                        processId_ = 0;

                        // Stop the monitoring thread
                        if (monitorThread_.joinable()) {
                            monitorThreadStop_ = true;
                            monitorThread_.join();
                        }

                        // Clear active connections
                        activeConnections_.clear();
                        return true;
                    }
                }
            }

            // If we're forcing or SIGTERM didn't work, use SIGKILL
            if (kill(processId_, SIGKILL) == 0) {
                isRunning_ = false;
                processId_ = 0;

                // Stop the monitoring thread
                if (monitorThread_.joinable()) {
                    monitorThreadStop_ = true;
                    monitorThread_.join();
                }

                // Clear active connections
                activeConnections_.clear();
                return true;
            } else {
                lastError_ = "Failed to kill SSH server process";
                return false;
            }
#endif
        } catch (const std::exception& e) {
            lastError_ =
                std::string("Exception when stopping SSH server: ") + e.what();
            return false;
        }
    }

    bool restart() {
        if (isRunning_) {
            if (!stop(true)) {
                return false;
            }
        }

        // Brief pause to ensure resources are freed
        std::this_thread::sleep_for(std::chrono::seconds(1));

        return start();
    }

    bool isRunning() const {
        if (!isRunning_) {
            return false;
        }

        // Check if the process is actually still running
#ifdef _WIN32
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) {
            return false;
        }

        PROCESSENTRY32 entry{};
        entry.dwSize = sizeof(entry);

        if (!Process32First(snapshot, &entry)) {
            CloseHandle(snapshot);
            return false;
        }

        bool processExists = false;
        do {
            if (entry.th32ProcessID == processId_) {
                processExists = true;
                break;
            }
        } while (Process32Next(snapshot, &entry));

        CloseHandle(snapshot);
        return processExists;
#else
        return (kill(processId_, 0) == 0);
#endif
    }

    void setPort(int port) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        port_ = port;
    }

    int getPort() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return port_;
    }

    void setListenAddress(const std::string& address) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        listenAddress_ = address;
    }

    std::string getListenAddress() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return listenAddress_;
    }

    void setHostKey(const std::filesystem::path& keyFile) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        hostKey_ = keyFile;
    }

    std::filesystem::path getHostKey() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return hostKey_;
    }

    void setAuthorizedKeys(const std::vector<std::filesystem::path>& keyFiles) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        authorizedKeys_ = keyFiles;
    }

    std::vector<std::filesystem::path> getAuthorizedKeys() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return authorizedKeys_;
    }

    void allowRootLogin(bool allow) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        allowRootLogin_ = allow;
    }

    bool isRootLoginAllowed() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return allowRootLogin_;
    }

    void setPasswordAuthentication(bool enable) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        passwordAuthentication_ = enable;
    }

    bool isPasswordAuthenticationEnabled() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return passwordAuthentication_;
    }

    void setSubsystem(const std::string& name, const std::string& command) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        subsystems_[name] = command;
    }

    void removeSubsystem(const std::string& name) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        subsystems_.erase(name);
    }

    std::string getSubsystem(const std::string& name) const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        auto it = subsystems_.find(name);
        if (it != subsystems_.end()) {
            return it->second;
        }
        return {};
    }

    std::vector<SshConnection> getActiveConnections() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        std::vector<SshConnection> connections;

        for (const auto& [sessionId, connection] : activeConnections_) {
            connections.push_back(connection);
        }

        return connections;
    }

    bool disconnectClient(const std::string& sessionId) {
        std::lock_guard<std::mutex> lock(serverMutex_);

        auto it = activeConnections_.find(sessionId);
        if (it == activeConnections_.end()) {
            return false;
        }

        // Execute the kill command for the specific session
#ifdef _WIN32
        // On Windows, we need to find and kill the specific child process
        // This requires more complex implementation
        return false;
#else
        // On Unix, we can use ssh-kill command if available
        std::string command = "ssh-kill " + sessionId + " 2>/dev/null";
        int result = system(command.c_str());

        if (result == 0) {
            // Remove from active connections
            activeConnections_.erase(sessionId);
            return true;
        }

        return false;
#endif
    }

    void setMaxAuthAttempts(int maxAttempts) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        maxAuthAttempts_ = maxAttempts;
    }

    int getMaxAuthAttempts() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return maxAuthAttempts_;
    }

    void setMaxConnections(int maxConnections) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        maxConnections_ = maxConnections;
    }

    int getMaxConnections() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return maxConnections_;
    }

    void setLoginGraceTime(int seconds) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        loginGraceTime_ = seconds;
    }

    int getLoginGraceTime() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return loginGraceTime_;
    }

    void setIdleTimeout(int seconds) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        idleTimeout_ = seconds;
    }

    int getIdleTimeout() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return idleTimeout_;
    }

    void allowIpAddress(const std::string& ipAddress) {
        std::lock_guard<std::mutex> lock(serverMutex_);

        // Remove from denied list if present
        auto it = std::find(deniedIps_.begin(), deniedIps_.end(), ipAddress);
        if (it != deniedIps_.end()) {
            deniedIps_.erase(it);
        }

        // Add to allowed list if not already present
        if (std::find(allowedIps_.begin(), allowedIps_.end(), ipAddress) ==
            allowedIps_.end()) {
            allowedIps_.push_back(ipAddress);
        }
    }

    void denyIpAddress(const std::string& ipAddress) {
        std::lock_guard<std::mutex> lock(serverMutex_);

        // Remove from allowed list if present
        auto it = std::find(allowedIps_.begin(), allowedIps_.end(), ipAddress);
        if (it != allowedIps_.end()) {
            allowedIps_.erase(it);
        }

        // Add to denied list if not already present
        if (std::find(deniedIps_.begin(), deniedIps_.end(), ipAddress) ==
            deniedIps_.end()) {
            deniedIps_.push_back(ipAddress);
        }
    }

    bool isIpAddressAllowed(const std::string& ipAddress) const {
        std::lock_guard<std::mutex> lock(serverMutex_);

        // Check if explicitly denied
        if (std::find(deniedIps_.begin(), deniedIps_.end(), ipAddress) !=
            deniedIps_.end()) {
            return false;
        }

        // If allowed list is empty, all IPs are allowed
        if (allowedIps_.empty()) {
            return true;
        }

        // Check if explicitly allowed
        return std::find(allowedIps_.begin(), allowedIps_.end(), ipAddress) !=
               allowedIps_.end();
    }

    void allowAgentForwarding(bool allow) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        allowAgentForwarding_ = allow;
    }

    bool isAgentForwardingAllowed() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return allowAgentForwarding_;
    }

    void allowTcpForwarding(bool allow) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        allowTcpForwarding_ = allow;
    }

    bool isTcpForwardingAllowed() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return allowTcpForwarding_;
    }

    void setLogLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        logLevel_ = level;
    }

    LogLevel getLogLevel() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return logLevel_;
    }

    void setLogFile(const std::filesystem::path& logFile) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        logFile_ = logFile;
    }

    std::filesystem::path getLogFile() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return logFile_;
    }

    bool generateHostKey(const std::string& keyType, int keySize,
                         const std::filesystem::path& outputPath) {
        std::lock_guard<std::mutex> lock(serverMutex_);

        if (keyType != "rsa" && keyType != "dsa" && keyType != "ecdsa" &&
            keyType != "ed25519") {
            lastError_ = "Unsupported key type: " + keyType;
            return false;
        }

        std::string command;

#ifdef _WIN32
        command = "ssh-keygen -t " + keyType;
        if (keyType != "ed25519") {
            command += " -b " + std::to_string(keySize);
        }
        command += " -f \"" + outputPath.string() + "\" -N \"\"";

        // Execute the command
        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(STARTUPINFO));
        si.cb = sizeof(STARTUPINFO);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        if (CreateProcess(NULL, const_cast<char*>(command.c_str()), NULL, NULL,
                          FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            // Wait for the process to complete
            WaitForSingleObject(pi.hProcess, INFINITE);

            // Get the exit code
            DWORD exitCode;
            GetExitCodeProcess(pi.hProcess, &exitCode);

            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);

            return (exitCode == 0);
        } else {
            lastError_ = "Failed to execute ssh-keygen command";
            return false;
        }
#else
        command = "ssh-keygen -t " + keyType;
        if (keyType != "ed25519") {
            command += " -b " + std::to_string(keySize);
        }
        command += " -f \"" + outputPath.string() + "\" -N \"\" -q";

        int result = system(command.c_str());
        return (result == 0);
#endif
    }

    bool verifyConfiguration() const {
        return getConfigurationIssues().empty();
    }

    std::vector<std::string> getConfigurationIssues() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        std::vector<std::string> issues;

        // Check required configuration
        if (port_ <= 0 || port_ > 65535) {
            issues.push_back("Invalid port number: " + std::to_string(port_));
        }

        if (listenAddress_.empty()) {
            issues.push_back("Listen address is not specified");
        }

        if (hostKey_.empty()) {
            issues.push_back("Host key file is not specified");
        } else if (!std::filesystem::exists(hostKey_)) {
            issues.push_back("Host key file does not exist: " +
                             hostKey_.string());
        }

        // Check for valid login methods
        if (!passwordAuthentication_ && authorizedKeys_.empty()) {
            issues.push_back(
                "No authentication methods enabled (neither password nor "
                "public key)");
        }

        // Check if log file is writable
        if (!logFile_.empty()) {
            std::error_code ec;
            if (std::filesystem::exists(logFile_)) {
                // Check if writable
                std::ofstream testFile(logFile_, std::ios::app);
                if (!testFile) {
                    issues.push_back("Log file is not writable: " +
                                     logFile_.string());
                }
            } else {
                // Check if parent directory exists and is writable
                auto parentDir = logFile_.parent_path();
                if (!std::filesystem::exists(parentDir)) {
                    issues.push_back(
                        "Log file parent directory does not exist: " +
                        parentDir.string());
                } else {
                    // Try to create a test file in the directory
                    auto testFilePath = parentDir / "test.tmp";
                    std::ofstream testFile(testFilePath);
                    if (!testFile) {
                        issues.push_back(
                            "Log file directory is not writable: " +
                            parentDir.string());
                    } else {
                        testFile.close();
                        std::filesystem::remove(testFilePath, ec);
                    }
                }
            }
        }

        return issues;
    }

    void setCiphers(const std::string& ciphers) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        ciphers_ = ciphers;
    }

    std::string getCiphers() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return ciphers_;
    }

    void setMACs(const std::string& macs) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        macs_ = macs;
    }

    std::string getMACs() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return macs_;
    }

    void setKexAlgorithms(const std::string& kexAlgorithms) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        kexAlgorithms_ = kexAlgorithms;
    }

    std::string getKexAlgorithms() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return kexAlgorithms_;
    }

    void onNewConnection(std::function<void(const SshConnection&)> callback) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        newConnectionCallback_ = callback;
    }

    void onConnectionClosed(
        std::function<void(const SshConnection&)> callback) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        connectionClosedCallback_ = callback;
    }

    void onAuthenticationFailure(
        std::function<void(const std::string&, const std::string&)> callback) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        authFailureCallback_ = callback;
    }

    std::unordered_map<std::string, std::string> getStatistics() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        std::unordered_map<std::string, std::string> stats;

        stats["uptime"] = getUptimeString();
        stats["active_connections"] = std::to_string(activeConnections_.size());
        stats["total_connections"] = std::to_string(totalConnections_);
        stats["failed_auth_attempts"] = std::to_string(failedAuthAttempts_);

        return stats;
    }

    std::string getServerVersion() const {
        std::lock_guard<std::mutex> lock(serverMutex_);
        return serverVersion_;
    }

    void setServerVersion(const std::string& version) {
        std::lock_guard<std::mutex> lock(serverMutex_);
        serverVersion_ = version;
    }

private:
    void loadConfig() {
        std::lock_guard<std::mutex> lock(serverMutex_);

        if (!std::filesystem::exists(configFile_)) {
            // Create default configuration if file doesn't exist
            saveConfig();
            return;
        }

        std::ifstream file(configFile_);
        if (!file) {
            THROW_RUNTIME_ERROR("Failed to open SSH server configuration file");
        }

        std::string line;
        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') {
                continue;
            }

            std::istringstream iss(line);
            std::string key, value;
            if (std::getline(iss, key, ' ') && std::getline(iss, value)) {
                if (key == "Port") {
                    port_ = std::stoi(value);
                } else if (key == "ListenAddress") {
                    listenAddress_ = value;
                } else if (key == "HostKey") {
                    hostKey_ = value;
                } else if (key == "AuthorizedKeysFile") {
                    authorizedKeys_.push_back(value);
                } else if (key == "PermitRootLogin") {
                    allowRootLogin_ = (value == "yes");
                } else if (key == "PasswordAuthentication") {
                    passwordAuthentication_ = (value == "yes");
                } else if (key == "MaxAuthTries") {
                    maxAuthAttempts_ = std::stoi(value);
                } else if (key == "MaxStartups") {
                    // Parse format like "10:30:100"
                    size_t pos = value.find(':');
                    if (pos != std::string::npos) {
                        maxConnections_ = std::stoi(value.substr(0, pos));
                    } else {
                        maxConnections_ = std::stoi(value);
                    }
                } else if (key == "LoginGraceTime") {
                    loginGraceTime_ = std::stoi(value);
                } else if (key == "ClientAliveInterval") {
                    idleTimeout_ = std::stoi(value);
                } else if (key == "LogLevel") {
                    if (value == "QUIET")
                        logLevel_ = LogLevel::QUIET;
                    else if (value == "FATAL")
                        logLevel_ = LogLevel::FATAL;
                    else if (value == "ERROR")
                        logLevel_ = LogLevel::ERROR;
                    else if (value == "INFO")
                        logLevel_ = LogLevel::INFO;
                    else if (value == "VERBOSE")
                        logLevel_ = LogLevel::VERBOSE;
                    else if (value == "DEBUG")
                        logLevel_ = LogLevel::DEBUG;
                    else if (value == "DEBUG1")
                        logLevel_ = LogLevel::DEBUG1;
                    else if (value == "DEBUG2")
                        logLevel_ = LogLevel::DEBUG2;
                    else if (value == "DEBUG3")
                        logLevel_ = LogLevel::DEBUG3;
                } else if (key == "SyslogFacility" && !logFile_.empty()) {
                    // If using file logging, ignore syslog setting
                } else if (key == "AllowAgentForwarding") {
                    allowAgentForwarding_ = (value == "yes");
                } else if (key == "AllowTcpForwarding") {
                    allowTcpForwarding_ = (value == "yes");
                } else if (key == "Ciphers") {
                    ciphers_ = value;
                } else if (key == "MACs") {
                    macs_ = value;
                } else if (key == "KexAlgorithms") {
                    kexAlgorithms_ = value;
                } else if (key == "Subsystem") {
                    std::istringstream subsystemIss(value);
                    std::string subsystemName, subsystemCommand;
                    if (std::getline(subsystemIss, subsystemName, ' ') &&
                        std::getline(subsystemIss, subsystemCommand)) {
                        subsystems_[subsystemName] = subsystemCommand;
                    }
                } else if (key == "AllowUsers") {
                    std::istringstream usersIss(value);
                    std::string user;
                    while (usersIss >> user) {
                        allowedUsers_.push_back(user);
                    }
                } else if (key == "DenyUsers") {
                    std::istringstream usersIss(value);
                    std::string user;
                    while (usersIss >> user) {
                        deniedUsers_.push_back(user);
                    }
                }
            }
        }
    }

    void saveConfig() {
        std::lock_guard<std::mutex> lock(serverMutex_);

        std::ofstream file(configFile_);
        if (!file) {
            THROW_RUNTIME_ERROR("Failed to save SSH server configuration file");
        }

        // Write header
        file << "# SSH Server Configuration\n";
        file << "# Generated by AtomSSH Server\n\n";

        // Basic settings
        file << "Port " << port_ << '\n';
        file << "ListenAddress " << listenAddress_ << '\n';
        file << "HostKey " << hostKey_.string() << '\n';
        file << "PermitRootLogin " << (allowRootLogin_ ? "yes" : "no") << '\n';
        file << "PasswordAuthentication "
             << (passwordAuthentication_ ? "yes" : "no") << '\n';

        // Authentication settings
        file << "MaxAuthTries " << maxAuthAttempts_ << '\n';
        file << "LoginGraceTime " << loginGraceTime_ << '\n';

        // Connection settings
        file << "MaxStartups " << maxConnections_ << '\n';
        file << "ClientAliveInterval " << idleTimeout_ << '\n';
        file << "ClientAliveCountMax 3\n";

        // Security settings
        file << "AllowAgentForwarding "
             << (allowAgentForwarding_ ? "yes" : "no") << '\n';
        file << "AllowTcpForwarding " << (allowTcpForwarding_ ? "yes" : "no")
             << '\n';
        file << "X11Forwarding no\n";
        file << "PermitTunnel no\n";
        file << "PermitUserEnvironment no\n";

        // Cryptographic settings
        file << "Ciphers " << ciphers_ << '\n';
        file << "MACs " << macs_ << '\n';
        file << "KexAlgorithms " << kexAlgorithms_ << '\n';

        // Key authentication
        for (const auto& keyFile : authorizedKeys_) {
            file << "AuthorizedKeysFile " << keyFile.string() << '\n';
        }

        // Subsystems
        for (const auto& [name, command] : subsystems_) {
            file << "Subsystem " << name << " " << command << '\n';
        }

        // Allowed/denied users
        if (!allowedUsers_.empty()) {
            file << "AllowUsers";
            for (const auto& user : allowedUsers_) {
                file << " " << user;
            }
            file << '\n';
        }

        if (!deniedUsers_.empty()) {
            file << "DenyUsers";
            for (const auto& user : deniedUsers_) {
                file << " " << user;
            }
            file << '\n';
        }

        // Logging
        std::string logLevelStr;
        switch (logLevel_) {
            case LogLevel::QUIET:
                logLevelStr = "QUIET";
                break;
            case LogLevel::FATAL:
                logLevelStr = "FATAL";
                break;
            case LogLevel::ERROR:
                logLevelStr = "ERROR";
                break;
            case LogLevel::INFO:
                logLevelStr = "INFO";
                break;
            case LogLevel::VERBOSE:
                logLevelStr = "VERBOSE";
                break;
            case LogLevel::DEBUG:
                logLevelStr = "DEBUG";
                break;
            case LogLevel::DEBUG1:
                logLevelStr = "DEBUG1";
                break;
            case LogLevel::DEBUG2:
                logLevelStr = "DEBUG2";
                break;
            case LogLevel::DEBUG3:
                logLevelStr = "DEBUG3";
                break;
        }
        file << "LogLevel " << logLevelStr << '\n';

        // If we're using a custom log file, specify it
        if (!logFile_.empty()) {
            file << "# Custom log file is handled by wrapper: "
                 << logFile_.string() << '\n';
        }
    }

    void monitorSshd() {
        startTime_ = std::chrono::system_clock::now();

        while (!monitorThreadStop_ && isRunning_) {
            // Check process exists
            if (!isRunning()) {
                // Server stopped unexpectedly
                isRunning_ = false;
                break;
            }

            // Update active connections by parsing logs or using appropriate
            // system calls
            updateActiveConnections();

            // Sleep for a short time before checking again
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    void updateActiveConnections() {
        // This is a placeholder. In a real implementation, this would:
        // 1. Parse the SSH server logs to detect new connections and
        // disconnections
        // 2. Update the activeConnections_ map accordingly
        // 3. Call the appropriate callbacks

        // For now, just simulate connection activity at random intervals
        static auto lastUpdateTime = std::chrono::system_clock::now();
        auto now = std::chrono::system_clock::now();

        if (now - lastUpdateTime > std::chrono::seconds(10)) {
            lastUpdateTime = now;

            // Simulate a new connection (1 in 3 chance)
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 10);

            if (dis(gen) <= 3) {
                // Generate a random connection
                SshConnection conn;
                conn.username = "user" + std::to_string(dis(gen));
                conn.ipAddress = "192.168.1." + std::to_string(dis(gen) * 10);
                conn.port = 22;
                conn.connectedTime = now;
                conn.sessionId = generateSessionId();

                // Add to active connections
                activeConnections_[conn.sessionId] = conn;
                totalConnections_++;

                // Call callback if registered
                if (newConnectionCallback_) {
                    newConnectionCallback_(conn);
                }
            }

            // Simulate a disconnection (1 in 5 chance)
            if (!activeConnections_.empty() && dis(gen) <= 2) {
                // Pick a random connection to disconnect
                std::uniform_int_distribution<> connDis(
                    0, activeConnections_.size() - 1);
                auto it = activeConnections_.begin();
                std::advance(it, connDis(gen) % activeConnections_.size());

                // Call callback if registered
                if (connectionClosedCallback_) {
                    connectionClosedCallback_(it->second);
                }

                // Remove from active connections
                activeConnections_.erase(it);
            }

            // Simulate authentication failure (1 in 7 chance)
            if (dis(gen) <= 1) {
                std::string username = "user" + std::to_string(dis(gen));
                std::string ipAddress =
                    "192.168.1." + std::to_string(dis(gen) * 10);

                failedAuthAttempts_++;

                // Call callback if registered
                if (authFailureCallback_) {
                    authFailureCallback_(username, ipAddress);
                }
            }
        }
    }

    std::string generateSessionId() const {
        std::stringstream ss;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);

        ss << std::hex;
        for (int i = 0; i < 16; ++i) {
            ss << dis(gen);
        }

        return ss.str();
    }

    std::string getUptimeString() const {
        auto now = std::chrono::system_clock::now();
        auto uptime = now - startTime_;

        auto hours =
            std::chrono::duration_cast<std::chrono::hours>(uptime).count();
        auto minutes =
            std::chrono::duration_cast<std::chrono::minutes>(uptime).count() %
            60;
        auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(uptime).count() %
            60;

        std::stringstream ss;
        if (hours > 0) {
            ss << hours << "h ";
        }
        if (hours > 0 || minutes > 0) {
            ss << minutes << "m ";
        }
        ss << seconds << "s";

        return ss.str();
    }

    std::filesystem::path configFile_;
    int port_ = 22;
    std::string listenAddress_ = "0.0.0.0";
    std::filesystem::path hostKey_;
    std::vector<std::filesystem::path> authorizedKeys_;
    bool allowRootLogin_ = false;
    bool passwordAuthentication_ = false;
    std::unordered_map<std::string, std::string> subsystems_;

    // New configuration parameters
    int maxAuthAttempts_;
    int maxConnections_;
    int loginGraceTime_;
    int idleTimeout_;
    bool allowAgentForwarding_;
    bool allowTcpForwarding_;
    std::string ciphers_;
    std::string macs_;
    std::string kexAlgorithms_;
    std::string serverVersion_;
    LogLevel logLevel_;
    std::filesystem::path logFile_;
    std::vector<std::string> allowedUsers_;
    std::vector<std::string> deniedUsers_;
    std::vector<std::string> allowedIps_;
    std::vector<std::string> deniedIps_;

    // Runtime state
    bool isRunning_;
    uint64_t processId_;
    std::thread monitorThread_;
    bool monitorThreadStop_ = false;
    std::unordered_map<std::string, SshConnection> activeConnections_;
    std::chrono::system_clock::time_point startTime_;
    uint64_t totalConnections_ = 0;
    uint64_t failedAuthAttempts_ = 0;

    // Callbacks
    std::function<void(const SshConnection&)> newConnectionCallback_;
    std::function<void(const SshConnection&)> connectionClosedCallback_;
    std::function<void(const std::string&, const std::string&)>
        authFailureCallback_;

    // Error handling
    mutable std::string lastError_;

    // Thread safety
    mutable std::mutex serverMutex_;
};

SshServer::SshServer(const std::filesystem::path& configFile)
    : impl_(std::make_unique<Impl>(configFile)) {}

SshServer::~SshServer() = default;

bool SshServer::start() { return impl_->start(); }

bool SshServer::stop(bool force) { return impl_->stop(force); }

bool SshServer::restart() { return impl_->restart(); }

bool SshServer::isRunning() const { return impl_->isRunning(); }

void SshServer::setPort(int port) { impl_->setPort(port); }

int SshServer::getPort() const { return impl_->getPort(); }

void SshServer::setListenAddress(const std::string& address) {
    impl_->setListenAddress(address);
}

std::string SshServer::getListenAddress() const {
    return impl_->getListenAddress();
}

void SshServer::setHostKey(const std::filesystem::path& keyFile) {
    impl_->setHostKey(keyFile);
}

std::filesystem::path SshServer::getHostKey() const {
    return impl_->getHostKey();
}

void SshServer::setAuthorizedKeys(
    const std::vector<std::filesystem::path>& keyFiles) {
    impl_->setAuthorizedKeys(keyFiles);
}

std::vector<std::filesystem::path> SshServer::getAuthorizedKeys() const {
    return impl_->getAuthorizedKeys();
}

void SshServer::allowRootLogin(bool allow) { impl_->allowRootLogin(allow); }

bool SshServer::isRootLoginAllowed() const {
    return impl_->isRootLoginAllowed();
}

void SshServer::setPasswordAuthentication(bool enable) {
    impl_->setPasswordAuthentication(enable);
}

bool SshServer::isPasswordAuthenticationEnabled() const {
    return impl_->isPasswordAuthenticationEnabled();
}

void SshServer::setSubsystem(const std::string& name,
                             const std::string& command) {
    impl_->setSubsystem(name, command);
}

void SshServer::removeSubsystem(const std::string& name) {
    impl_->removeSubsystem(name);
}

std::string SshServer::getSubsystem(const std::string& name) const {
    return impl_->getSubsystem(name);
}

std::vector<SshConnection> SshServer::getActiveConnections() const {
    return impl_->getActiveConnections();
}

bool SshServer::disconnectClient(const std::string& sessionId) {
    return impl_->disconnectClient(sessionId);
}

void SshServer::setMaxAuthAttempts(int maxAttempts) {
    impl_->setMaxAuthAttempts(maxAttempts);
}

int SshServer::getMaxAuthAttempts() const {
    return impl_->getMaxAuthAttempts();
}

void SshServer::setMaxConnections(int maxConnections) {
    impl_->setMaxConnections(maxConnections);
}

int SshServer::getMaxConnections() const { return impl_->getMaxConnections(); }

void SshServer::setLoginGraceTime(int seconds) {
    impl_->setLoginGraceTime(seconds);
}

int SshServer::getLoginGraceTime() const { return impl_->getLoginGraceTime(); }

void SshServer::setIdleTimeout(int seconds) { impl_->setIdleTimeout(seconds); }

int SshServer::getIdleTimeout() const { return impl_->getIdleTimeout(); }

void SshServer::allowIpAddress(const std::string& ipAddress) {
    impl_->allowIpAddress(ipAddress);
}

void SshServer::denyIpAddress(const std::string& ipAddress) {
    impl_->denyIpAddress(ipAddress);
}

bool SshServer::isIpAddressAllowed(const std::string& ipAddress) const {
    return impl_->isIpAddressAllowed(ipAddress);
}

void SshServer::allowAgentForwarding(bool allow) {
    impl_->allowAgentForwarding(allow);
}

bool SshServer::isAgentForwardingAllowed() const {
    return impl_->isAgentForwardingAllowed();
}

void SshServer::allowTcpForwarding(bool allow) {
    impl_->allowTcpForwarding(allow);
}

bool SshServer::isTcpForwardingAllowed() const {
    return impl_->isTcpForwardingAllowed();
}

void SshServer::setLogLevel(LogLevel level) { impl_->setLogLevel(level); }

LogLevel SshServer::getLogLevel() const { return impl_->getLogLevel(); }

void SshServer::setLogFile(const std::filesystem::path& logFile) {
    impl_->setLogFile(logFile);
}

std::filesystem::path SshServer::getLogFile() const {
    return impl_->getLogFile();
}

bool SshServer::generateHostKey(const std::string& keyType, int keySize,
                                const std::filesystem::path& outputPath) {
    return impl_->generateHostKey(keyType, keySize, outputPath);
}

bool SshServer::verifyConfiguration() const {
    return impl_->verifyConfiguration();
}

std::vector<std::string> SshServer::getConfigurationIssues() const {
    return impl_->getConfigurationIssues();
}

void SshServer::setCiphers(const std::string& ciphers) {
    impl_->setCiphers(ciphers);
}

std::string SshServer::getCiphers() const { return impl_->getCiphers(); }

void SshServer::setMACs(const std::string& macs) { impl_->setMACs(macs); }

std::string SshServer::getMACs() const { return impl_->getMACs(); }

void SshServer::setKexAlgorithms(const std::string& kexAlgorithms) {
    impl_->setKexAlgorithms(kexAlgorithms);
}

std::string SshServer::getKexAlgorithms() const {
    return impl_->getKexAlgorithms();
}

void SshServer::onNewConnection(
    std::function<void(const SshConnection&)> callback) {
    impl_->onNewConnection(callback);
}

void SshServer::onConnectionClosed(
    std::function<void(const SshConnection&)> callback) {
    impl_->onConnectionClosed(callback);
}

void SshServer::onAuthenticationFailure(
    std::function<void(const std::string&, const std::string&)> callback) {
    impl_->onAuthenticationFailure(callback);
}

std::unordered_map<std::string, std::string> SshServer::getStatistics() const {
    return impl_->getStatistics();
}

std::string SshServer::getServerVersion() const {
    return impl_->getServerVersion();
}

void SshServer::setServerVersion(const std::string& version) {
    impl_->setServerVersion(version);
}

}  // namespace atom::connection