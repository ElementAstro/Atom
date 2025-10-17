/*
 * gpu.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - GPU

**************************************************/

#ifndef ATOM_SYSTEM_MODULE_GPU_HPP
#define ATOM_SYSTEM_MODULE_GPU_HPP

#include <string>
#include <vector>

namespace atom::system {

/**
 * @brief Get GPU information from the system
 * @return std::string GPU information as a formatted string
 */
[[nodiscard]] auto getGPUInfo() -> std::string;

/**
 * @brief Structure containing monitor information
 */
struct alignas(128) MonitorInfo {
    std::string model;       ///< Monitor model name
    std::string identifier;  ///< Monitor identifier
    int width{0};            ///< Screen width in pixels
    int height{0};           ///< Screen height in pixels
    int refreshRate{0};      ///< Refresh rate in Hz
};

/**
 * @brief Get information for all connected monitors
 * @return std::vector<MonitorInfo> Vector containing all monitor information
 */
[[nodiscard]] auto getAllMonitorsInfo() -> std::vector<MonitorInfo>;

}  // namespace atom::system

#endif
