#include "win32_utils.h"

#include <algorithm>
#include <string>
#include <unordered_set>
#include <vector>

// clang-format off
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
// clang-format on

namespace shortcut_detector {
namespace win32_utils {

// Common hook DLL modules
static const std::unordered_set<std::string> knownHookDlls = {
    "HOOK.DLL",         "KBDHOOK.DLL",          "KEYHOOK.DLL",  "INPUTHOOK.DLL",
    "WINHOOK.DLL",      "LLKEYBOARD.DLL",       "KEYMAGIC.DLL", "HOOKSPY.DLL",
    "KEYBOARDHOOK.DLL", "INPUTMANAGERHOOK.DLL", "UIHOOK.DLL"};

std::vector<std::string> getProcessesWithKeyboardHooks() {
    std::vector<std::string> result;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return result;
    }

    PROCESSENTRY32 processEntry = {sizeof(PROCESSENTRY32)};

    if (Process32First(snapshot, &processEntry)) {
        do {
            if (checkProcessForKeyboardHook(processEntry.th32ProcessID)) {
                result.push_back(getProcessName(processEntry.th32ProcessID));
            }
        } while (Process32Next(snapshot, &processEntry));
    }

    CloseHandle(snapshot);
    return result;
}

bool checkProcessForKeyboardHook(DWORD processId) {
    if (processId == 0) {
        return false;
    }

    // Open process for querying information
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                  FALSE, processId);
    if (!hProcess) {
        return false;
    }

    bool result = false;

    // Enumerate all modules in the process
    HMODULE modules[1024];
    DWORD needed;

    if (EnumProcessModules(hProcess, modules, sizeof(modules), &needed)) {
        for (unsigned i = 0; i < (needed / sizeof(HMODULE)); i++) {
            char moduleName[MAX_PATH];
            if (GetModuleFileNameExA(hProcess, modules[i], moduleName,
                                     sizeof(moduleName))) {
                std::string name = moduleName;
                // Extract just the filename
                size_t pos = name.find_last_of("\\/");
                if (pos != std::string::npos) {
                    name = name.substr(pos + 1);
                }

                // Convert to uppercase for case-insensitive comparison
                std::transform(name.begin(), name.end(), name.begin(),
                               ::toupper);

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

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                  FALSE, processId);
    if (hProcess) {
        char name[MAX_PATH];
        if (GetModuleFileNameExA(hProcess, NULL, name, sizeof(name))) {
            // Extract just the filename
            std::string fullPath = name;
            size_t pos = fullPath.find_last_of("\\/");
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
    // Check against our list of known hook DLLs
    if (knownHookDlls.find(moduleName) != knownHookDlls.end()) {
        return true;
    }

    // Also check for common naming patterns
    return (moduleName.find("HOOK") != std::string::npos) ||
           (moduleName.find("KEYB") != std::string::npos &&
            moduleName.find("MONITOR") != std::string::npos) ||
           (moduleName.find("INPUT") != std::string::npos &&
            moduleName.find("HOOK") != std::string::npos);
}

}  // namespace win32_utils
}  // namespace shortcut_detector