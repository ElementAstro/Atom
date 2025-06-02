#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace shortcut_detector {
namespace win32_utils {

/**
 * @brief Get a list of processes that have keyboard hooks installed
 *
 * @return std::vector<std::string> Process names
 */
std::vector<std::string> getProcessesWithKeyboardHooks();

/**
 * @brief Check if a process has keyboard hooks
 *
 * @param processId Process ID
 * @return bool True if process has keyboard hooks
 */
bool checkProcessForKeyboardHook(DWORD processId);

/**
 * @brief Get executable name from process ID
 *
 * @param processId Process ID
 * @return std::string Process name
 */
std::string getProcessName(DWORD processId);

/**
 * @brief Check if module is a hook DLL
 *
 * @param moduleName Module name
 * @return bool True if likely to be hook DLL
 */
bool isHookingModule(const std::string& moduleName);

}  // namespace win32_utils
}  // namespace shortcut_detector