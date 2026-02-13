/*
 * validation.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "validation.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <unordered_set>

#include "utils.hpp"

#include <spdlog/spdlog.h>

namespace atom::system {

// Unified dangerous commands list (merged from executor.cpp and utils.cpp)
namespace {
    const std::unordered_set<std::string> DANGEROUS_COMMANDS = {
        "rm", "del", "format", "fdisk", "mkfs", "dd", "shutdown", "reboot",
        "halt", "poweroff", "init", "kill", "killall", "pkill", "chmod", "chown"
    };

    const std::unordered_set<std::string> PRIVILEGED_COMMANDS = {
        "sudo", "su", "mount", "umount", "iptables", "systemctl", "service",
        "passwd", "useradd", "userdel", "groupadd", "groupdel"
    };

    const std::regex COMMAND_INJECTION_PATTERN(R"([;&|`$(){}[\]<>])");
    const std::regex SHELL_INJECTION_PATTERN(R"([;&|`$(){}[\]<>*?])");
    const std::regex COMMAND_PATTERN(R"(^\s*([^\s]+))");
}

auto validateCommand(const std::string &command) -> bool {
    if (command.empty()) {
        spdlog::warn("Empty command provided for validation");
        return false;
    }

    // Check for command injection patterns
    if (std::regex_search(command, COMMAND_INJECTION_PATTERN)) {
        spdlog::warn("Command contains potentially dangerous characters: {}", command);
        return false;
    }

    // Extract the base command (first word)
    std::istringstream iss(command);
    std::string baseCommand;
    iss >> baseCommand;

    // Remove path if present
    size_t lastSlash = baseCommand.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        baseCommand = baseCommand.substr(lastSlash + 1);
    }

    // Check against dangerous commands list
    if (DANGEROUS_COMMANDS.find(baseCommand) != DANGEROUS_COMMANDS.end()) {
        spdlog::warn("Command '{}' is in the dangerous commands list", baseCommand);
        return false;
    }

    spdlog::debug("Command validation passed for: {}", command);
    return true;
}

auto validateCommandDetailed(const std::string &command) -> ValidationResult {
    ValidationResult result;

    if (command.empty()) {
        result.errorMessage = "Command is empty";
        return result;
    }

    // Check for shell injection patterns
    if (std::regex_search(command, SHELL_INJECTION_PATTERN)) {
        result.warnings.push_back("Command contains potentially dangerous shell characters");
        result.securityScore -= 0.3;
    }

    // Extract base command
    std::smatch match;
    if (std::regex_search(command, match, COMMAND_PATTERN)) {
        std::string baseCommand = match[1].str();

        // Remove path if present
        size_t lastSlash = baseCommand.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            baseCommand = baseCommand.substr(lastSlash + 1);
        }

        // Check against dangerous commands
        if (DANGEROUS_COMMANDS.find(baseCommand) != DANGEROUS_COMMANDS.end()) {
            result.warnings.push_back("Command '" + baseCommand + "' is potentially dangerous");
            result.securityScore -= 0.5;
        }

        // Check against privileged commands
        if (PRIVILEGED_COMMANDS.find(baseCommand) != PRIVILEGED_COMMANDS.end()) {
            result.warnings.push_back("Command '" + baseCommand + "' requires elevated privileges");
            result.securityScore -= 0.2;
        }
    }

    // Calculate final security score (0.0 to 1.0)
    result.securityScore = std::max(0.0, 1.0 + result.securityScore);

    // Command is valid if security score is above threshold
    result.isValid = result.securityScore >= 0.3;

    if (!result.isValid) {
        result.errorMessage = "Command failed security validation";
    }

    spdlog::debug("Command validation: '{}', valid: {}, score: {:.2f}",
                  command, result.isValid, result.securityScore);

    return result;
}

auto sanitizeCommand(const std::string &command) -> std::string {
    std::string sanitized = command;

    // Remove or escape dangerous characters
    std::regex dangerousChars(R"([;&|`$])");
    sanitized = std::regex_replace(sanitized, dangerousChars, "");

    // Trim whitespace
    sanitized.erase(0, sanitized.find_first_not_of(" \t\n\r"));
    sanitized.erase(sanitized.find_last_not_of(" \t\n\r") + 1);

    spdlog::debug("Sanitized command: '{}' -> '{}'", command, sanitized);
    return sanitized;
}

auto requiresElevatedPrivileges(const std::string &command) -> bool {
    auto args = parseCommandArguments(command);
    if (args.empty()) {
        return false;
    }

    std::string baseCommand = args[0];

    // Remove path if present
    size_t lastSlash = baseCommand.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        baseCommand = baseCommand.substr(lastSlash + 1);
    }

    bool requiresPrivileges = PRIVILEGED_COMMANDS.find(baseCommand) != PRIVILEGED_COMMANDS.end();

    spdlog::debug("Command '{}' requires elevated privileges: {}", command, requiresPrivileges);
    return requiresPrivileges;
}

}  // namespace atom::system
