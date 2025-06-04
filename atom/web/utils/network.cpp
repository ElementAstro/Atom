/*
 * network.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "network.hpp"

#include <spdlog/spdlog.h>
#include <string>
#include <vector>
#include "port.hpp"


namespace atom::web {

/**
 * @brief Check if the device has active internet connectivity
 * @return true if internet is available, false otherwise
 */
auto checkInternetConnectivity() -> bool {
    try {
        static const std::vector<std::string> reliableHosts = {
            "8.8.8.8", "1.1.1.1", "208.67.222.222"};
        for (const auto& host : reliableHosts) {
            if (scanPort(host, 53, std::chrono::milliseconds(2000))) {
                return true;
            }
        }
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Error checking internet connectivity: {}", e.what());
        return false;
    }
}

}  // namespace atom::web
