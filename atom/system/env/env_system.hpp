/*
 * env_system.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-16

Description: System information and directories

**************************************************/

#ifndef ATOM_SYSTEM_ENV_SYSTEM_HPP
#define ATOM_SYSTEM_ENV_SYSTEM_HPP

#include "atom/containers/high_performance.hpp"
#include "atom/macro.hpp"

namespace atom::utils {

using atom::containers::String;

/**
 * @brief System information and directories
 */
class EnvSystem {
public:
    /**
     * @brief Gets the user home directory
     * @return The path to the user home directory
     */
    ATOM_NODISCARD static auto getHomeDir() -> String;

    /**
     * @brief Gets the system temporary directory
     * @return The path to the system temporary directory
     */
    ATOM_NODISCARD static auto getTempDir() -> String;

    /**
     * @brief Gets the system configuration directory
     * @return The path to the system configuration directory
     */
    ATOM_NODISCARD static auto getConfigDir() -> String;

    /**
     * @brief Gets the user data directory
     * @return The path to the user data directory
     */
    ATOM_NODISCARD static auto getDataDir() -> String;

    /**
     * @brief Gets the system name
     * @return System name (e.g., "Windows", "Linux", "macOS")
     */
    ATOM_NODISCARD static auto getSystemName() -> String;

    /**
     * @brief Gets the system architecture
     * @return System architecture (e.g., "x86_64", "arm64")
     */
    ATOM_NODISCARD static auto getSystemArch() -> String;

    /**
     * @brief Gets the current username
     * @return Current username
     */
    ATOM_NODISCARD static auto getCurrentUser() -> String;

    /**
     * @brief Gets the hostname
     * @return Hostname
     */
    ATOM_NODISCARD static auto getHostName() -> String;
};

}  // namespace atom::utils

#endif  // ATOM_SYSTEM_ENV_SYSTEM_HPP
