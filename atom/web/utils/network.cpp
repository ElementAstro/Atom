/*
 * network.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "network.hpp"

#include <string>
#include <vector>

#include "atom/log/loguru.hpp"
#include "port.hpp"

namespace atom::web {

auto checkInternetConnectivity() -> bool {
    try {
        const std::vector<std::string> reliableHosts = {"8.8.8.8", "1.1.1.1",
                                                        "208.67.222.222"};

        for (const auto& host : reliableHosts) {
            if (scanPort(host, 53, std::chrono::milliseconds(2000))) {
                return true;
            }
        }

        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Error checking internet connectivity: {}", e.what());
        return false;
    }
}

}  // namespace atom::web
