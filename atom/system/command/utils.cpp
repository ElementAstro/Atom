/*
 * utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "utils.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

#include "executor.hpp"
#include "validation.hpp"

#ifdef _WIN32
// Windows headers would go here
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

#include <spdlog/spdlog.h>

namespace atom::system {

auto parseCommandArguments(const std::string &command) -> std::vector<std::string> {
    std::vector<std::string> args;
    std::istringstream iss(command);
    std::string arg;

    bool inQuotes = false;
    char quoteChar = '\0';
    std::string currentArg;

    for (char c : command) {
        if (!inQuotes && (c == '"' || c == '\'')) {
            inQuotes = true;
            quoteChar = c;
        } else if (inQuotes && c == quoteChar) {
            inQuotes = false;
            quoteChar = '\0';
        } else if (!inQuotes && std::isspace(c)) {
            if (!currentArg.empty()) {
                args.push_back(currentArg);
                currentArg.clear();
            }
        } else {
            currentArg += c;
        }
    }

    if (!currentArg.empty()) {
        args.push_back(currentArg);
    }

    spdlog::debug("Parsed {} arguments from command: '{}'", args.size(), command);
    return args;
}

auto buildCommandString(const std::vector<std::string> &args) -> std::string {
    if (args.empty()) {
        return "";
    }

    std::ostringstream oss;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            oss << " ";
        }

        const std::string &arg = args[i];
        // Quote arguments that contain spaces or special characters
        if (arg.find_first_of(" \t\n\r\"'") != std::string::npos) {
            oss << "\"" << arg << "\"";
        } else {
            oss << arg;
        }
    }

    std::string result = oss.str();
    spdlog::debug("Built command string from {} arguments: '{}'", args.size(), result);
    return result;
}

auto getCommandMetrics(const std::string &command) -> CommandMetrics {
    CommandMetrics metrics;

    auto startTime = std::chrono::steady_clock::now();

    ExecutionConfig config;
    config.enableLogging = false; // Avoid recursive logging
    auto result = executeCommandEnhanced(command, config);

    auto endTime = std::chrono::steady_clock::now();

    metrics.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);
    metrics.outputSize = result.output.size();
    metrics.wasSuccessful = (result.exitCode == 0);

    spdlog::debug("Command metrics for '{}': time={}ms, output={}bytes, success={}",
                  command, metrics.executionTime.count(), metrics.outputSize,
                  metrics.wasSuccessful);

    return metrics;
}

auto isCommandAvailable(const std::string &command) -> bool {
    std::string checkCommand;
#ifdef _WIN32
    checkCommand = "where " + command + " > nul 2>&1";
#else
    checkCommand = "command -v " + command + " > /dev/null 2>&1";
#endif
    return executeCommandSimple(checkCommand);
}

auto executeCommandGetLines(const std::string &command)
    -> std::vector<std::string> {
    spdlog::debug("Executing command and getting lines: {}", command);

    std::vector<std::string> lines;
    auto output = executeCommand(command);

    std::istringstream ss(output);
    std::string line;

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }

    spdlog::debug("Command returned {} lines", lines.size());
    return lines;
}

auto pipeCommands(const std::string &firstCommand,
                  const std::string &secondCommand) -> std::string {
    spdlog::debug("Piping commands: '{}' | '{}'", firstCommand, secondCommand);

#ifdef _WIN32
    std::string tempFile = std::tmpnam(nullptr);
    std::string combinedCommand = firstCommand + " > " + tempFile + " && " +
                                  secondCommand + " < " + tempFile +
                                  " && del " + tempFile;
#else
    std::string combinedCommand = firstCommand + " | " + secondCommand;
#endif

    auto result = executeCommand(combinedCommand);
    spdlog::debug("Pipe commands completed");
    return result;
}

auto executeCommandGetLinesEnhanced(
    const std::string &command,
    bool trimWhitespace,
    bool skipEmptyLines,
    size_t maxLines) -> std::vector<std::string> {

    spdlog::debug("Executing enhanced command and getting lines: {}", command);

    std::vector<std::string> lines;
    auto output = executeCommand(command);

    std::istringstream ss(output);
    std::string line;
    size_t lineCount = 0;

    while (std::getline(ss, line) && (maxLines == 0 || lineCount < maxLines)) {
        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Trim whitespace if requested
        if (trimWhitespace) {
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);
        }

        // Skip empty lines if requested
        if (skipEmptyLines && line.empty()) {
            continue;
        }

        lines.push_back(line);
        lineCount++;
    }

    spdlog::debug("Enhanced command returned {} lines", lines.size());
    return lines;
}

auto pipeCommandsEnhanced(
    const std::vector<std::string> &commands,
    const PipeConfig &config) -> ExecutionResult {

    spdlog::debug("Enhanced piping {} commands", commands.size());

    ExecutionResult result;

    if (commands.empty()) {
        result.exitCode = -1;
        result.error = "No commands provided for piping";
        return result;
    }

    // Validate commands if requested
    if (config.validateCommands) {
        for (const auto &command : commands) {
            auto validation = validateCommandDetailed(command);
            if (!validation.isValid) {
                result.exitCode = -1;
                result.error = "Command validation failed: " + validation.errorMessage;
                return result;
            }
        }
    }

    auto startTime = std::chrono::steady_clock::now();

    // Build the piped command
    std::ostringstream pipeCommand;
    for (size_t i = 0; i < commands.size(); ++i) {
        if (i > 0) {
            pipeCommand << " | ";
        }
        pipeCommand << commands[i];
    }

    // Add stderr capture if requested
    if (config.captureStderr) {
        pipeCommand << " 2>&1";
    }

    ExecutionConfig execConfig;
    execConfig.timeout = config.timeout;
    execConfig.maxOutputSize = config.maxOutputSize;

    result = executeCommandEnhanced(pipeCommand.str(), execConfig);

    auto endTime = std::chrono::steady_clock::now();
    result.executionTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    spdlog::debug("Enhanced pipe commands completed in {}ms",
                  result.executionTime.count());

    return result;
}

auto findCommandPath(const std::string &command) -> std::string {
    spdlog::debug("Finding path for command: {}", command);

#ifdef _WIN32
    std::string whereCommand = "where " + command;
#else
    std::string whereCommand = "which " + command;
#endif

    auto output = executeCommand(whereCommand);

    // Get the first line (first match)
    std::istringstream ss(output);
    std::string path;
    if (std::getline(ss, path)) {
        // Remove carriage return if present
        if (!path.empty() && path.back() == '\r') {
            path.pop_back();
        }
        spdlog::debug("Found command path: {}", path);
        return path;
    }

    spdlog::debug("Command path not found for: {}", command);
    return "";
}

auto getSystemAliases() -> std::unordered_map<std::string, std::string> {
    std::unordered_map<std::string, std::string> aliases;

#ifdef _WIN32
    // Windows doesn't have traditional aliases, but we could check doskey
    spdlog::debug("Alias detection not implemented for Windows");
#else
    auto output = executeCommand("alias 2>/dev/null || true");
    std::istringstream ss(output);
    std::string line;

    std::regex aliasPattern(R"(alias\s+([^=]+)='([^']*)')");

    while (std::getline(ss, line)) {
        std::smatch match;
        if (std::regex_search(line, match, aliasPattern)) {
            std::string aliasName = match[1].str();
            std::string aliasValue = match[2].str();
            aliases[aliasName] = aliasValue;
        }
    }
#endif

    spdlog::debug("Found {} system aliases", aliases.size());
    return aliases;
}

auto expandCommandAliases(const std::string &command) -> std::string {
    auto aliases = getSystemAliases();

    auto args = parseCommandArguments(command);
    if (args.empty()) {
        return command;
    }

    std::string baseCommand = args[0];
    auto it = aliases.find(baseCommand);

    if (it != aliases.end()) {
        // Replace the base command with its alias expansion
        args[0] = it->second;
        std::string expanded = buildCommandString(args);
        spdlog::debug("Expanded alias: '{}' -> '{}'", command, expanded);
        return expanded;
    }

    return command;
}

auto getCommandCompletions(
    const std::string &partialCommand,
    size_t maxSuggestions) -> std::vector<std::string> {

    std::vector<std::string> completions;

#ifdef _WIN32
    // Windows command completion would be more complex
    spdlog::debug("Command completion not fully implemented for Windows");
#else
    // Use bash completion if available
    std::string bashCommand = "compgen -c " + partialCommand + " 2>/dev/null | head -" +
                             std::to_string(maxSuggestions);

    auto lines = executeCommandGetLines(bashCommand);
    for (const auto &line : lines) {
        if (!line.empty()) {
            completions.push_back(line);
        }
    }
#endif

    spdlog::debug("Found {} completions for '{}'", completions.size(), partialCommand);
    return completions;
}

}  // namespace atom::system
