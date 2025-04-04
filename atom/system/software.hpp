#ifndef ATOM_SYSTEM_SOFTWARE_HPP
#define ATOM_SYSTEM_SOFTWARE_HPP

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace atom::system {
/**
 * @brief Check whether the specified software is installed.
 *
 * @param software_name The name of the software.
 * @return true if the software is installed.
 * @return false if the software is not installed or an error occurred.
 */
auto checkSoftwareInstalled(const std::string& software_name) -> bool;

/**
 * @brief Get the version of the specified application.
 *
 * @param app_path The path to the application.
 * @return The version of the application.
 */
auto getAppVersion(const fs::path& app_path) -> std::string;

/**
 * @brief Get the path to the specified application.
 *
 * @param software_name The name of the software.
 * @return The path to the application.
 */
auto getAppPath(const std::string& software_name) -> fs::path;

/**
 * @brief Get the permissions of the specified application.
 *
 * @param app_path The path to the application.
 * @return The permissions of the application.
 */
auto getAppPermissions(const fs::path& app_path) -> std::vector<std::string>;

/**
 * @brief Get process information for a running software.
 *
 * @param software_name The name of the software.
 * @return A map containing process info (pid, cpu usage, memory usage, etc).
 */
auto getProcessInfo(const std::string& software_name)
    -> std::map<std::string, std::string>;

/**
 * @brief Launch a software application.
 *
 * @param software_path The path to the software.
 * @param args Command line arguments to pass to the software.
 * @return true if the software was launched successfully.
 * @return false if the software failed to launch.
 */
auto launchSoftware(const fs::path& software_path,
                    const std::vector<std::string>& args = {}) -> bool;

/**
 * @brief Terminate a running software application.
 *
 * @param software_name The name of the software.
 * @return true if the software was terminated successfully.
 * @return false if the software failed to terminate or wasn't running.
 */
auto terminateSoftware(const std::string& software_name) -> bool;

/**
 * @brief Monitor software usage with a callback.
 *
 * @param software_name The name of the software.
 * @param callback The function to call when usage changes.
 * @param interval_ms The monitoring interval in milliseconds.
 * @return A monitoring ID that can be used to stop monitoring.
 */
auto monitorSoftwareUsage(
    const std::string& software_name,
    std::function<void(const std::map<std::string, std::string>&)> callback,
    int interval_ms = 1000) -> int;

/**
 * @brief Stop monitoring software usage.
 *
 * @param monitor_id The monitoring ID returned by monitorSoftwareUsage.
 * @return true if monitoring was stopped successfully.
 * @return false if the monitor_id was invalid.
 */
auto stopMonitoring(int monitor_id) -> bool;

/**
 * @brief Check if a software has updates available.
 *
 * @param software_name The name of the software.
 * @param current_version The current version of the software.
 * @return A string containing the latest version if updates are available,
 * empty string otherwise.
 */
auto checkSoftwareUpdates(const std::string& software_name,
                          const std::string& current_version) -> std::string;

}  // namespace atom::system

#endif