/*
 * security.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "security.hpp"

#include <atomic>
#include <fstream>
#include <mutex>
#include <regex>
#include <thread>
#include <unordered_set>

#include "executor.hpp"
#include "validation.hpp"
#include "utils.hpp"

#include <spdlog/spdlog.h>

namespace atom::system {

// Global instances
namespace {
    std::shared_ptr<SecurityManager> g_securityManager;
    std::mutex g_globalMutex;
}

// SecurityManager implementation
class SecurityManager::Impl {
public:
    explicit Impl(const SecurityPolicy& policy) : policy_(policy) {
        if (policy_.enableAuditLogging) {
            initializeAuditLogging();
        }
    }

    ~Impl() {
        stopMonitoring();
    }

    auto isCommandAllowed(const std::string& command, const std::string& user) const -> bool {
        // Check whitelist first
        if (!policy_.whitelist.empty()) {
            auto validation = validateCommandDetailed(command);
            std::string baseCommand = extractBaseCommand(command);

            bool inWhitelist = std::find(policy_.whitelist.begin(),
                                       policy_.whitelist.end(),
                                       baseCommand) != policy_.whitelist.end();

            if (policy_.level == SecurityLevel::PARANOID) {
                return inWhitelist;
            }
        }

        // Check blacklist
        std::string baseCommand = extractBaseCommand(command);
        bool inBlacklist = std::find(policy_.blacklist.begin(),
                                   policy_.blacklist.end(),
                                   baseCommand) != policy_.blacklist.end();

        if (inBlacklist) {
            return false;
        }

        // Apply security level policies
        auto validation = validateCommandDetailed(command);

        switch (policy_.level) {
            case SecurityLevel::PERMISSIVE:
                return true;

            case SecurityLevel::MODERATE:
                return validation.securityScore >= 0.3;

            case SecurityLevel::STRICT:
                return validation.securityScore >= 0.7;

            case SecurityLevel::PARANOID:
                // Already handled above with whitelist
                return validation.securityScore >= 0.9;
        }

        return false;
    }

    auto validateAndSanitizeCommand(const std::string& command, const std::string& user) const -> std::string {
        if (!isCommandAllowed(command, user)) {
            logSecurityEvent(AuditEventType::COMMAND_BLOCKED, command, user,
                           "Command blocked by security policy");
            return "";
        }

        std::string sanitized = sanitizeCommand(command);

        if (sanitized != command) {
            logSecurityEvent(AuditEventType::COMMAND_EXECUTED, sanitized, user,
                           "Command was sanitized before execution");
        }

        return sanitized;
    }

    auto executeSecureCommand(const std::string& command, const ExecutionConfig& config,
                             const std::string& user) -> ExecutionResult {
        ExecutionResult result;

        std::string sanitizedCommand = validateAndSanitizeCommand(command, user);
        if (sanitizedCommand.empty()) {
            result.exitCode = -1;
            result.error = "Command blocked by security policy";
            result.wasKilled = true;
            return result;
        }

        // Apply resource limits
        ExecutionConfig secureConfig = config;
        if (policy_.resourceLimits.maxExecutionTime.count() > 0) {
            secureConfig.timeout = std::min(secureConfig.timeout, policy_.resourceLimits.maxExecutionTime);
        }
        secureConfig.maxOutputSize = std::min(secureConfig.maxOutputSize, policy_.resourceLimits.maxOutputSize);

        auto startTime = std::chrono::system_clock::now();

        // Execute with enhanced monitoring
        result = executeCommandEnhanced(sanitizedCommand, secureConfig);

        // Log execution event
        AuditEvent event;
        event.type = AuditEventType::COMMAND_EXECUTED;
        event.command = command;
        event.user = user;
        event.timestamp = startTime;
        event.securityLevel = policy_.level;
        event.wasBlocked = false;
        event.details = "Exit code: " + std::to_string(result.exitCode) +
                       ", Execution time: " + std::to_string(result.executionTime.count()) + "ms";

        logAuditEvent(event);

        return result;
    }

    void logAuditEvent(const AuditEvent& event) {
        std::lock_guard<std::mutex> lock(auditMutex_);

        auditEvents_.push_back(event);

        // Keep only recent events in memory
        if (auditEvents_.size() > 1000) {
            auditEvents_.erase(auditEvents_.begin(), auditEvents_.begin() + 100);
        }

        if (policy_.enableAuditLogging && auditFile_.is_open()) {
            writeAuditEventToFile(event);
        }

        // Update statistics
        updateSecurityStatistics(event);
    }

    auto getRecentAuditEvents(size_t count) const -> std::vector<AuditEvent> {
        std::lock_guard<std::mutex> lock(auditMutex_);

        size_t startIndex = auditEvents_.size() > count ? auditEvents_.size() - count : 0;
        return std::vector<AuditEvent>(auditEvents_.begin() + startIndex, auditEvents_.end());
    }

    auto getSecurityStatistics() const -> std::string {
        std::lock_guard<std::mutex> lock(auditMutex_);

        std::ostringstream stats;
        stats << "Security Statistics:\n";
        stats << "  Total Commands: " << totalCommands_ << "\n";
        stats << "  Blocked Commands: " << blockedCommands_ << "\n";
        stats << "  Security Violations: " << securityViolations_ << "\n";
        stats << "  Current Security Level: " << securityLevelToString(policy_.level) << "\n";

        if (totalCommands_ > 0) {
            double blockRate = (blockedCommands_ * 100.0) / totalCommands_;
            stats << "  Block Rate: " << std::fixed << std::setprecision(2) << blockRate << "%\n";
        }

        return stats.str();
    }

    void updateSecurityPolicy(const SecurityPolicy& policy) {
        std::lock_guard<std::mutex> lock(auditMutex_);
        policy_ = policy;

        if (policy_.enableAuditLogging && !auditFile_.is_open()) {
            initializeAuditLogging();
        } else if (!policy_.enableAuditLogging && auditFile_.is_open()) {
            auditFile_.close();
        }
    }

    auto getSecurityPolicy() const -> const SecurityPolicy& {
        return policy_;
    }

    void startMonitoring(std::function<void(const AuditEvent&)> callback) {
        monitoringCallback_ = callback;
        monitoringActive_ = true;

        monitoringThread_ = std::thread([this]() {
            while (monitoringActive_) {
                // Monitor for suspicious patterns
                checkForSuspiciousActivity();
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        });
    }

    void stopMonitoring() {
        monitoringActive_ = false;
        if (monitoringThread_.joinable()) {
            monitoringThread_.join();
        }
    }

private:
    std::string extractBaseCommand(const std::string& command) const {
        auto args = parseCommandArguments(command);
        if (args.empty()) return "";

        std::string baseCommand = args[0];
        size_t lastSlash = baseCommand.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            baseCommand = baseCommand.substr(lastSlash + 1);
        }

        return baseCommand;
    }

    void logSecurityEvent(AuditEventType type, const std::string& command,
                         const std::string& user, const std::string& details) const {
        AuditEvent event;
        event.type = type;
        event.command = command;
        event.user = user;
        event.timestamp = std::chrono::system_clock::now();
        event.details = details;
        event.securityLevel = policy_.level;
        event.wasBlocked = (type == AuditEventType::COMMAND_BLOCKED);

        const_cast<Impl*>(this)->logAuditEvent(event);
    }

    void initializeAuditLogging() {
        auditFile_.open(policy_.auditLogFile, std::ios::app);
        if (!auditFile_.is_open()) {
            spdlog::error("Failed to open audit log file: {}", policy_.auditLogFile);
        } else {
            spdlog::info("Audit logging initialized: {}", policy_.auditLogFile);
        }
    }

    void writeAuditEventToFile(const AuditEvent& event) {
        if (!auditFile_.is_open()) return;

        auto time_t = std::chrono::system_clock::to_time_t(event.timestamp);
        auditFile_ << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << " | ";
        auditFile_ << auditEventTypeToString(event.type) << " | ";
        auditFile_ << event.user << " | ";
        auditFile_ << event.command << " | ";
        auditFile_ << event.details << "\n";
        auditFile_.flush();
    }

    void updateSecurityStatistics(const AuditEvent& event) {
        totalCommands_++;

        if (event.type == AuditEventType::COMMAND_BLOCKED) {
            blockedCommands_++;
        } else if (event.type == AuditEventType::SECURITY_VIOLATION) {
            securityViolations_++;
        }
    }

    void checkForSuspiciousActivity() {
        // Implementation for detecting suspicious patterns
        // This is a simplified version
        std::lock_guard<std::mutex> lock(auditMutex_);

        if (auditEvents_.size() < 10) return;

        // Check for rapid command execution (potential automation)
        auto now = std::chrono::system_clock::now();
        auto fiveMinutesAgo = now - std::chrono::minutes(5);

        size_t recentCommands = 0;
        for (const auto& event : auditEvents_) {
            if (event.timestamp > fiveMinutesAgo) {
                recentCommands++;
            }
        }

        if (recentCommands > 50) { // More than 50 commands in 5 minutes
            AuditEvent suspiciousEvent;
            suspiciousEvent.type = AuditEventType::SUSPICIOUS_ACTIVITY;
            suspiciousEvent.timestamp = now;
            suspiciousEvent.details = "High command execution rate detected: " +
                                    std::to_string(recentCommands) + " commands in 5 minutes";

            if (monitoringCallback_) {
                monitoringCallback_(suspiciousEvent);
            }
        }
    }

    std::string securityLevelToString(SecurityLevel level) const {
        switch (level) {
            case SecurityLevel::PERMISSIVE: return "Permissive";
            case SecurityLevel::MODERATE: return "Moderate";
            case SecurityLevel::STRICT: return "Strict";
            case SecurityLevel::PARANOID: return "Paranoid";
            default: return "Unknown";
        }
    }

    std::string auditEventTypeToString(AuditEventType type) const {
        switch (type) {
            case AuditEventType::COMMAND_EXECUTED: return "EXECUTED";
            case AuditEventType::COMMAND_BLOCKED: return "BLOCKED";
            case AuditEventType::SECURITY_VIOLATION: return "VIOLATION";
            case AuditEventType::PRIVILEGE_ESCALATION: return "PRIVILEGE_ESC";
            case AuditEventType::SUSPICIOUS_ACTIVITY: return "SUSPICIOUS";
            case AuditEventType::RESOURCE_LIMIT_EXCEEDED: return "RESOURCE_LIMIT";
            default: return "UNKNOWN";
        }
    }

    SecurityPolicy policy_;
    mutable std::mutex auditMutex_;
    std::vector<AuditEvent> auditEvents_;
    std::ofstream auditFile_;

    // Statistics
    std::atomic<size_t> totalCommands_{0};
    std::atomic<size_t> blockedCommands_{0};
    std::atomic<size_t> securityViolations_{0};

    // Monitoring
    std::atomic<bool> monitoringActive_{false};
    std::thread monitoringThread_;
    std::function<void(const AuditEvent&)> monitoringCallback_;
};

SecurityManager::SecurityManager(const SecurityPolicy& policy)
    : pImpl_(std::make_unique<Impl>(policy)) {}

SecurityManager::~SecurityManager() = default;

auto SecurityManager::isCommandAllowed(const std::string& command, const std::string& user) const -> bool {
    return pImpl_->isCommandAllowed(command, user);
}

auto SecurityManager::validateAndSanitizeCommand(const std::string& command, const std::string& user) const -> std::string {
    return pImpl_->validateAndSanitizeCommand(command, user);
}

auto SecurityManager::executeSecureCommand(const std::string& command, const ExecutionConfig& config, const std::string& user) -> ExecutionResult {
    return pImpl_->executeSecureCommand(command, config, user);
}

void SecurityManager::logAuditEvent(const AuditEvent& event) {
    pImpl_->logAuditEvent(event);
}

auto SecurityManager::getRecentAuditEvents(size_t count) const -> std::vector<AuditEvent> {
    return pImpl_->getRecentAuditEvents(count);
}

auto SecurityManager::getSecurityStatistics() const -> std::string {
    return pImpl_->getSecurityStatistics();
}

void SecurityManager::updateSecurityPolicy(const SecurityPolicy& policy) {
    pImpl_->updateSecurityPolicy(policy);
}

auto SecurityManager::getSecurityPolicy() const -> const SecurityPolicy& {
    return pImpl_->getSecurityPolicy();
}

void SecurityManager::startMonitoring(std::function<void(const AuditEvent&)> callback) {
    pImpl_->startMonitoring(callback);
}

void SecurityManager::stopMonitoring() {
    pImpl_->stopMonitoring();
}

// Global functions
auto getGlobalSecurityManager(const SecurityPolicy& policy) -> std::shared_ptr<SecurityManager> {
    std::lock_guard<std::mutex> lock(g_globalMutex);

    if (!g_securityManager) {
        g_securityManager = std::make_shared<SecurityManager>(policy);
        spdlog::info("Created global security manager");
    }

    return g_securityManager;
}

auto executeCommandSecure(const std::string& command, const ExecutionConfig& config, const std::string& user) -> ExecutionResult {
    auto securityManager = getGlobalSecurityManager();
    return securityManager->executeSecureCommand(command, config, user);
}

}  // namespace atom::system
