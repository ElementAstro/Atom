#include "win32_utils.h"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>
#include <spdlog/spdlog.h>

// clang-format off
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
// clang-format on

namespace shortcut_detector {
namespace win32_utils {

/**
 * @brief Known keyboard hook DLL modules commonly used by applications
 */
static const std::unordered_set<std::string> knownHookDlls = {
    "HOOK.DLL",         "KBDHOOK.DLL",          "KEYHOOK.DLL",
    "INPUTHOOK.DLL",    "WINHOOK.DLL",          "LLKEYBOARD.DLL",
    "KEYMAGIC.DLL",     "HOOKSPY.DLL",          "KEYBOARDHOOK.DLL",
    "INPUTMANAGERHOOK.DLL", "UIHOOK.DLL"};

std::vector<std::string> getProcessesWithKeyboardHooks() {
    std::vector<std::string> result;

    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        spdlog::error("Failed to create process snapshot: {}", GetLastError());
        return result;
    }

    PROCESSENTRY32 processEntry = {sizeof(PROCESSENTRY32)};

    if (Process32First(snapshot, &processEntry)) {
        do {
            if (checkProcessForKeyboardHook(processEntry.th32ProcessID)) {
                const std::string processName = getProcessName(processEntry.th32ProcessID);
                result.push_back(processName);
                spdlog::debug("Found process with keyboard hook: {}", processName);
            }
        } while (Process32Next(snapshot, &processEntry));
    }

    CloseHandle(snapshot);
    spdlog::debug("Found {} processes with keyboard hooks", result.size());
    return result;
}

bool checkProcessForKeyboardHook(DWORD processId) {
    if (processId == 0) {
        return false;
    }

    const HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                       FALSE, processId);
    if (!hProcess) {
        return false;
    }

    bool result = false;
    HMODULE modules[1024];
    DWORD needed;

    if (EnumProcessModules(hProcess, modules, sizeof(modules), &needed)) {
        const DWORD moduleCount = needed / sizeof(HMODULE);
        for (DWORD i = 0; i < moduleCount; ++i) {
            char moduleName[MAX_PATH];
            if (GetModuleFileNameExA(hProcess, modules[i], moduleName, sizeof(moduleName))) {
                std::string name = moduleName;
                const size_t pos = name.find_last_of("\\/");
                if (pos != std::string::npos) {
                    name = name.substr(pos + 1);
                }

                std::transform(name.begin(), name.end(), name.begin(), ::toupper);

                if (isHookingModule(name)) {
                    result = true;
                    break;
                }
            }
        }
    }

    CloseHandle(hProcess);
    return result;
}

std::string getProcessName(DWORD processId) {
    std::string result = "Unknown Process";

    const HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                       FALSE, processId);
    if (hProcess) {
        char name[MAX_PATH];
        if (GetModuleFileNameExA(hProcess, NULL, name, sizeof(name))) {
            const std::string fullPath = name;
            const size_t pos = fullPath.find_last_of("\\/");
            if (pos != std::string::npos) {
                result = fullPath.substr(pos + 1);
            } else {
                result = fullPath;
            }
        }
        CloseHandle(hProcess);
    }

    return result;
}

bool isHookingModule(const std::string& moduleName) {
    if (knownHookDlls.find(moduleName) != knownHookDlls.end()) {
        return true;
    }

    return (moduleName.find("HOOK") != std::string::npos) ||
           (moduleName.find("KEYB") != std::string::npos &&
            moduleName.find("MONITOR") != std::string::npos) ||
           (moduleName.find("INPUT") != std::string::npos &&
            moduleName.find("HOOK") != std::string::npos);
}

}  // namespace win32_utils
}  // namespace shortcut_detector
