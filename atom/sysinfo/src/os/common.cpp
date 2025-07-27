#include "common.hpp"

#include <fstream>
#include <sstream>

#include <spdlog/spdlog.h>
#include "atom/error/exception.hpp"

namespace atom::system {

auto parseFile(const std::string& filePath) -> std::pair<std::string, std::string> {
    spdlog::debug("Parsing file: {}", filePath);
    std::ifstream file(filePath);
    if (!file.is_open()) {
        spdlog::error("Cannot open file: {}", filePath);
        THROW_FAIL_TO_OPEN_FILE("Cannot open file: " + filePath);
    }

    std::pair<std::string, std::string> osInfo;
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            std::string key = line.substr(0, delimiterPos);
            std::string value = line.substr(delimiterPos + 1);

            if (!value.empty() && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.size() - 2);
            }

            if (key == "PRETTY_NAME") {
                osInfo.first = value;
                spdlog::debug("Found PRETTY_NAME: {}", value);
            } else if (key == "VERSION") {
                osInfo.second = value;
                spdlog::debug("Found VERSION: {}", value);
            }
        }
    }

    return osInfo;
}

auto isWsl() -> bool {
    spdlog::debug("Checking if running in WSL");
    std::ifstream procVersion("/proc/version");
    std::string line;
    if (procVersion.is_open()) {
        std::getline(procVersion, line);
        procVersion.close();
        bool isWslEnv = line.find("microsoft") != std::string::npos ||
                        line.find("WSL") != std::string::npos;
        spdlog::info("WSL detection result: {}", isWslEnv);
        return isWslEnv;
    } else {
        spdlog::error("Failed to open /proc/version for WSL detection");
    }
    return false;
}

auto checkForUpdates() -> std::vector<std::string> {
    spdlog::debug("Checking for available updates");
    std::vector<std::string> updates;
    spdlog::warn("Update checking not implemented for this platform");
    return updates;
}

}  // namespace atom::system
