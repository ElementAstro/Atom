/*
 * utils.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "utils.hpp"

#include <cstdlib>
#include <sstream>

#include "executor.hpp"

#include <spdlog/spdlog.h>

namespace atom::system {

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

}  // namespace atom::system
