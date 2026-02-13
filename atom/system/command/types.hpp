/*
 * types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSTEM_COMMAND_TYPES_HPP
#define ATOM_SYSTEM_COMMAND_TYPES_HPP

#include <chrono>
#include <string>
#include <vector>

namespace atom::system {

// Forward declarations
enum class TaskPriority;
struct CommandSystemMetrics;

/**
 * @brief Configuration for command execution
 */
struct ExecutionConfig {
    bool openTerminal = false;
    bool validateCommand = true;
    bool enableLogging = true;
    size_t bufferSize = 8192;  // Increased default buffer size
    std::chrono::milliseconds timeout = std::chrono::milliseconds::zero();
    size_t maxOutputSize = 1024 * 1024;  // 1MB default limit
    bool streamOutput = false;
    bool captureStderr = true;
};

/**
 * @brief Result of command execution
 */
struct ExecutionResult {
    std::string output;
    std::string error;
    int exitCode = 0;
    std::chrono::milliseconds executionTime{0};
    bool timedOut = false;
    bool wasKilled = false;
};

/**
 * @brief Command validation result
 */
struct ValidationResult {
    bool isValid = false;
    std::string errorMessage;
    std::vector<std::string> warnings;
    double securityScore = 0.0; // 0.0 = very dangerous, 1.0 = safe
};

/**
 * @brief Command performance metrics
 */
struct CommandMetrics {
    std::chrono::milliseconds executionTime{0};
    size_t outputSize = 0;
    size_t memoryUsage = 0;
    double cpuUsage = 0.0;
    bool wasSuccessful = false;
};

/**
 * @brief Pipe configuration for command chaining
 */
struct PipeConfig {
    bool captureStderr = true;
    bool validateCommands = true;
    std::chrono::milliseconds timeout{30000}; // 30 seconds default
    size_t maxOutputSize = 1024 * 1024; // 1MB default
};

}  // namespace atom::system

#endif  // ATOM_SYSTEM_COMMAND_TYPES_HPP
