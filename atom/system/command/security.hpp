/*
 * security.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_SECURITY_HPP
#define ATOM_SYSTEM_COMMAND_SECURITY_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/macro.hpp"
#include "resource_monitor.hpp"
#include "types.hpp"

namespace atom::system {

/**
 * @brief Security policy levels
 */
enum class SecurityLevel {
    PERMISSIVE,  // Allow most commands with warnings
    MODERATE,    // Block dangerous commands, allow with confirmation
    STRICT,      // Block all potentially dangerous operations
    PARANOID     // Block everything except whitelisted commands
};

/**
 * @brief Security audit event types
 */
enum class AuditEventType {
    COMMAND_EXECUTED,
    COMMAND_BLOCKED,
    SECURITY_VIOLATION,
    PRIVILEGE_ESCALATION,
    SUSPICIOUS_ACTIVITY,
    RESOURCE_LIMIT_EXCEEDED
};

/**
 * @brief Security audit event
 */
struct AuditEvent {
    AuditEventType type;
    std::string command;
    std::string user;
    std::string workingDirectory;
    std::chrono::system_clock::time_point timestamp;
    std::string details;
    SecurityLevel securityLevel;
    bool wasBlocked = false;
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * @brief Security policy configuration
 */
struct SecurityPolicy {
    SecurityLevel level = SecurityLevel::MODERATE;
    std::vector<std::string> whitelist;
    std::vector<std::string> blacklist;
    ResourceLimits resourceLimits;
    bool enableAuditLogging = true;
    bool enableRealTimeMonitoring = true;
    bool requireConfirmationForDangerous = true;
    std::string auditLogFile = "command_audit.log";
};

/**
 * @brief Command security manager
 */
class SecurityManager {
public:
    explicit SecurityManager(const SecurityPolicy& policy = {});
    ~SecurityManager();

    /**
     * @brief Check if a command is allowed to execute
     *
     * @param command The command to check
     * @param user The user attempting to execute the command
     * @return true if command is allowed
     */
    ATOM_NODISCARD auto isCommandAllowed(const std::string& command,
                                        const std::string& user = "") const -> bool;

    /**
     * @brief Validate and potentially modify a command for security
     *
     * @param command The command to validate
     * @param user The user attempting to execute the command
     * @return Sanitized command or empty string if blocked
     */
    ATOM_NODISCARD auto validateAndSanitizeCommand(const std::string& command,
                                                   const std::string& user = "") const -> std::string;

    /**
     * @brief Execute a command with security monitoring
     *
     * @param command The command to execute
     * @param config Execution configuration
     * @param user The user executing the command
     * @return ExecutionResult with security information
     */
    ATOM_NODISCARD auto executeSecureCommand(const std::string& command,
                                            const ExecutionConfig& config = {},
                                            const std::string& user = "") -> ExecutionResult;

    /**
     * @brief Log an audit event
     *
     * @param event The audit event to log
     */
    void logAuditEvent(const AuditEvent& event);

    /**
     * @brief Get recent audit events
     *
     * @param count Number of events to retrieve
     * @return Vector of recent audit events
     */
    ATOM_NODISCARD auto getRecentAuditEvents(size_t count = 100) const -> std::vector<AuditEvent>;

    /**
     * @brief Get security statistics
     *
     * @return String containing formatted security statistics
     */
    ATOM_NODISCARD auto getSecurityStatistics() const -> std::string;

    /**
     * @brief Update security policy
     *
     * @param policy New security policy
     */
    void updateSecurityPolicy(const SecurityPolicy& policy);

    /**
     * @brief Get current security policy
     *
     * @return Current security policy
     */
    ATOM_NODISCARD auto getSecurityPolicy() const -> const SecurityPolicy&;

    /**
     * @brief Start real-time monitoring
     *
     * @param callback Function called when suspicious activity is detected
     */
    void startMonitoring(std::function<void(const AuditEvent&)> callback = nullptr);

    /**
     * @brief Stop real-time monitoring
     */
    void stopMonitoring();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

/**
 * @brief Create a global security manager instance
 *
 * @param policy Security policy to use
 * @return Shared pointer to SecurityManager
 */
ATOM_NODISCARD auto getGlobalSecurityManager(const SecurityPolicy& policy = {})
    -> std::shared_ptr<SecurityManager>;

/**
 * @brief Execute a command with global security policies
 *
 * @param command The command to execute
 * @param config Execution configuration
 * @param user The user executing the command
 * @return ExecutionResult with security validation
 */
ATOM_NODISCARD auto executeCommandSecure(const std::string& command,
                                        const ExecutionConfig& config = {},
                                        const std::string& user = "") -> ExecutionResult;

}  // namespace atom::system

#endif
