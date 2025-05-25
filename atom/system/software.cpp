#include "software.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
#include <aclapi.h>
#include <psapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>
#include <libproc.h>
#include <mach/mach_init.h>
#include <mach/task.h>
#include <mach/task_info.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#elif defined(__ANDROID__)
#include <android/native_activity.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif

#include <spdlog/spdlog.h>
#include "atom/utils/string.hpp"

namespace atom::system {

namespace {
struct MonitorInfo {
    std::thread thread;
    std::atomic<bool> running{true};
    std::function<void(const std::map<std::string, std::string>&)> callback;
    std::string software_name;
    int interval_ms;
};

std::mutex g_monitors_mutex;
std::unordered_map<int, MonitorInfo> g_monitors;
int g_next_monitor_id = 1;
}  // namespace

auto getAppVersion(const fs::path& app_path) -> std::string {
    spdlog::debug("Getting application version for path: {}",
                  app_path.string());

#ifdef _WIN32
    DWORD handle;
    auto wappPath = atom::utils::stringToWString(app_path.string());
    DWORD size = GetFileVersionInfoSizeW(wappPath.c_str(), &handle);
    if (size != 0) {
        LPVOID buffer = malloc(size);
        if (GetFileVersionInfoW(wappPath.c_str(), handle, size, buffer) != 0) {
            LPVOID value;
            UINT length;
            if (VerQueryValue(buffer,
                              TEXT("\\StringFileInfo\\040904b0\\FileVersion"),
                              &value, &length)) {
                std::string version(static_cast<char*>(value), length);
                free(buffer);
                spdlog::info("Found application version: {}", version);
                return version;
            }
        }
        free(buffer);
    }
#elif defined(__APPLE__)
    CFURLRef url = CFURLCreateWithFileSystemPath(nullptr, app_path.c_str(),
                                                 kCFURLPOSIXPathStyle, true);
    if (url != nullptr) {
        CFBundleRef bundle = CFBundleCreate(nullptr, url);
        if (bundle != nullptr) {
            CFStringRef version =
                static_cast<CFStringRef>(CFBundleGetValueForInfoDictionaryKey(
                    bundle, kCFBundleVersionKey));
            if (version != nullptr) {
                char buffer[256];
                if (CFStringGetCString(version, buffer, sizeof(buffer),
                                       kCFStringEncodingUTF8)) {
                    CFRelease(bundle);
                    CFRelease(url);
                    spdlog::info("Found application version: {}", buffer);
                    return std::string(buffer);
                }
            }
            CFRelease(bundle);
        }
        CFRelease(url);
    }
#elif defined(__ANDROID__)
    ANativeActivity* activity = ANativeActivity_getActivity();
    if (activity != nullptr && activity->callbacks != nullptr &&
        activity->callbacks->onGetPackageVersion != nullptr) {
        JNIEnv* env;
        activity->vm->AttachCurrentThread(&env, nullptr);
        jstring package_name = env->NewStringUTF(app_path.c_str());
        jstring version =
            static_cast<jstring>(activity->callbacks->onGetPackageVersion(
                activity->instance, package_name));
        env->DeleteLocalRef(package_name);
        if (version != nullptr) {
            const char* utf8_version = env->GetStringUTFChars(version, nullptr);
            std::string result(utf8_version);
            env->ReleaseStringUTFChars(version, utf8_version);
            env->DeleteLocalRef(version);
            spdlog::info("Found application version: {}", result);
            return result;
        }
        activity->vm->DetachCurrentThread();
    }
#else
    FILE* file = fopen(app_path.c_str(), "rb");
    if (file != nullptr) {
        char buffer[256];
        std::string version;
        while (fgets(buffer, sizeof(buffer), file) != nullptr) {
            if (strncmp(buffer, "@(#)", 4) == 0) {
                char* start = strchr(buffer, ' ');
                if (start != nullptr) {
                    char* end = strchr(start + 1, ' ');
                    if (end != nullptr) {
                        version = std::string(start + 1, end - start - 1);
                        break;
                    }
                }
            }
        }
        fclose(file);
        if (!version.empty()) {
            spdlog::info("Found application version: {}", version);
            return version;
        }
    }
#endif

    spdlog::warn("Version not found for application path: {}",
                 app_path.string());
    return "";
}

auto getAppPermissions(const fs::path& app_path) -> std::vector<std::string> {
    spdlog::debug("Getting application permissions for path: {}",
                  app_path.string());
    std::vector<std::string> permissions;

#ifdef _WIN32
    PSECURITY_DESCRIPTOR securityDescriptor;
    PACL dacl = nullptr;

    if (GetNamedSecurityInfoW(
            atom::utils::stringToWString(app_path.string()).c_str(),
            SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &dacl,
            nullptr, &securityDescriptor) == ERROR_SUCCESS) {
        if (dacl != nullptr) {
            LPVOID ace;
            for (DWORD i = 0; i < dacl->AceCount; ++i) {
                if (GetAce(dacl, i, &ace) != 0) {
                    if (static_cast<PACE_HEADER>(ace)->AceType ==
                        ACCESS_ALLOWED_ACE_TYPE) {
                        auto* allowedAce =
                            static_cast<PACCESS_ALLOWED_ACE>(ace);
                        std::vector<TCHAR> userName(0);
                        std::vector<TCHAR> domainName(0);
                        DWORD nameSize = 0;
                        DWORD domainSize = 0;
                        SID_NAME_USE sidType;

                        LookupAccountSid(nullptr, &allowedAce->SidStart,
                                         nullptr, &nameSize, nullptr,
                                         &domainSize, &sidType);
                        userName.resize(nameSize);
                        domainName.resize(domainSize);
                        if (LookupAccountSid(nullptr, &allowedAce->SidStart,
                                             userName.data(), &nameSize,
                                             domainName.data(), &domainSize,
                                             &sidType)) {
                            std::string permission =
                                std::format("User: {}\\{}", userName.data(),
                                            domainName.data());
                            permissions.push_back(permission);
                            spdlog::debug("Found permission: {}", permission);
                        }
                    }
                }
            }
        }
        LocalFree(securityDescriptor);
    }
#elif defined(__APPLE__) || defined(__linux__)
    struct stat file_stat;
    if (stat(app_path.c_str(), &file_stat) == 0) {
        if (file_stat.st_mode & S_IRUSR)
            permissions.push_back("Owner: Read");
        if (file_stat.st_mode & S_IWUSR)
            permissions.push_back("Owner: Write");
        if (file_stat.st_mode & S_IXUSR)
            permissions.push_back("Owner: Execute");
        if (file_stat.st_mode & S_IRGRP)
            permissions.push_back("Group: Read");
        if (file_stat.st_mode & S_IWGRP)
            permissions.push_back("Group: Write");
        if (file_stat.st_mode & S_IXGRP)
            permissions.push_back("Group: Execute");
        if (file_stat.st_mode & S_IROTH)
            permissions.push_back("Others: Read");
        if (file_stat.st_mode & S_IWOTH)
            permissions.push_back("Others: Write");
        if (file_stat.st_mode & S_IXOTH)
            permissions.push_back("Others: Execute");

        for (const auto& perm : permissions) {
            spdlog::debug("Found permission: {}", perm);
        }
    }
#elif defined(__ANDROID__)
    ANativeActivity* activity = ANativeActivity_getActivity();
    if (activity != nullptr && activity->callbacks != nullptr &&
        activity->callbacks->onGetPackagePermissions != nullptr) {
        JNIEnv* env;
        activity->vm->AttachCurrentThread(&env, nullptr);
        jstring package_name = env->NewStringUTF(app_path.c_str());
        jobjectArray permissions_array = static_cast<jobjectArray>(
            activity->callbacks->onGetPackagePermissions(activity->instance,
                                                         package_name));
        env->DeleteLocalRef(package_name);
        if (permissions_array != nullptr) {
            jsize length = env->GetArrayLength(permissions_array);
            for (jsize i = 0; i < length; ++i) {
                jstring permission = static_cast<jstring>(
                    env->GetObjectArrayElement(permissions_array, i));
                const char* utf8_permission =
                    env->GetStringUTFChars(permission, nullptr);
                permissions.push_back(std::string(utf8_permission));
                spdlog::debug("Found permission: {}", utf8_permission);
                env->ReleaseStringUTFChars(permission, utf8_permission);
                env->DeleteLocalRef(permission);
            }
            env->DeleteLocalRef(permissions_array);
        }
        activity->vm->DetachCurrentThread();
    }
#endif

    spdlog::info("Retrieved {} permissions for application: {}",
                 permissions.size(), app_path.string());
    return permissions;
}

auto getAppPath(const std::string& software_name) -> fs::path {
    spdlog::debug("Getting application path for software: {}", software_name);

#ifdef _WIN32
    WCHAR programFilesPath[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0,
                         programFilesPath) == S_OK) {
        fs::path path(programFilesPath);
        path.append(software_name);
        if (fs::exists(path)) {
            spdlog::info("Found application path: {}", path.string());
            return path;
        }
    }
    spdlog::warn("Application path not found for software: {}", software_name);
    return "";
#elif defined(__APPLE__)
    fs::path app_path("/Applications");
    app_path.append(software_name);
    if (fs::exists(app_path)) {
        spdlog::info("Found application path: {}", app_path.string());
        return app_path;
    }
    spdlog::warn("Application path not found for software: {}", software_name);
    return "";
#elif defined(__linux__)
    std::string command = "which " + software_name;
    std::array<char, 128> buffer;
    std::string result;
    using PcloseDeleter = int (*)(FILE*);
    std::unique_ptr<FILE, PcloseDeleter> pipe(popen(command.c_str(), "r"),
                                              pclose);
    if (!pipe) {
        spdlog::error("Failed to execute command: {}", command);
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    if (!result.empty()) {
        result.pop_back();
        if (fs::exists(result)) {
            spdlog::info("Found application path: {}", result);
            return fs::path(result);
        }
    }
    spdlog::warn("Application path not found for software: {}", software_name);
    return "";
#endif
    spdlog::warn("Using current path as fallback for software: {}",
                 software_name);
    return fs::current_path();
}

auto checkSoftwareInstalled(const std::string& software_name) -> bool {
    spdlog::debug("Checking if software is installed: {}", software_name);
    bool isInstalled = false;

#ifdef _WIN32
    HKEY hKey;
    std::string regPath =
        R"(SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall)";
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      atom::utils::stringToWString(regPath).c_str(), 0,
                      KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD index = 0;
        wchar_t subKeyName[256];
        DWORD subKeyNameSize = sizeof(subKeyName);
        while (RegEnumKeyExW(hKey, index, subKeyName, &subKeyNameSize, nullptr,
                             nullptr, nullptr,
                             nullptr) != ERROR_NO_MORE_ITEMS) {
            HKEY hSubKey;
            if (RegOpenKeyExW(hKey, subKeyName, 0, KEY_READ, &hSubKey) ==
                ERROR_SUCCESS) {
                char displayName[256];
                DWORD displayNameSize = sizeof(displayName);
                if (RegQueryValueExW(hSubKey, L"DisplayName", nullptr, nullptr,
                                     reinterpret_cast<LPBYTE>(displayName),
                                     &displayNameSize) == ERROR_SUCCESS) {
                    if (software_name == displayName) {
                        isInstalled = true;
                        spdlog::info("Software {} is installed", software_name);
                        RegCloseKey(hSubKey);
                        break;
                    }
                }
                RegCloseKey(hSubKey);
            }
            subKeyNameSize = sizeof(subKeyName);
            ++index;
        }
        RegCloseKey(hKey);
    }
#elif defined(__APPLE__)
    std::string command =
        "mdfind \"kMDItemKind == 'Application' && kMDItemFSName == '*" +
        software_name + "*.app'\"";
    std::array<char, 128> buffer;
    std::string result;
    using PcloseDeleter = int (*)(FILE*);
    std::unique_ptr<FILE, PcloseDeleter> pipe(popen(command.c_str(), "r"),
                                              pclose);
    if (pipe) {
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        isInstalled = !result.empty();
        if (isInstalled) {
            spdlog::info("Software {} is installed", software_name);
        } else {
            spdlog::warn("Software {} is not installed", software_name);
        }
    }
#elif defined(__linux__)
    std::string command = "which " + software_name + " > /dev/null 2>&1";
    int result = std::system(command.c_str());
    isInstalled = (result == 0);
    if (isInstalled) {
        spdlog::info("Software {} is installed", software_name);
    } else {
        spdlog::warn("Software {} is not installed", software_name);
    }
#endif

    return isInstalled;
}

auto getProcessInfo(const std::string& software_name)
    -> std::map<std::string, std::string> {
    spdlog::debug("Getting process information for software: {}",
                  software_name);
    std::map<std::string, std::string> info;

#ifdef _WIN32
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 processEntry;
        processEntry.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(snapshot, &processEntry)) {
            do {
                std::string wname(processEntry.szExeFile);
                std::string process_name(wname.begin(), wname.end());

                if (process_name.find(software_name) != std::string::npos) {
                    HANDLE hProcess =
                        OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                    FALSE, processEntry.th32ProcessID);
                    if (hProcess) {
                        PROCESS_MEMORY_COUNTERS_EX pmc;
                        if (GetProcessMemoryInfo(hProcess,
                                                 (PROCESS_MEMORY_COUNTERS*)&pmc,
                                                 sizeof(pmc))) {
                            info["pid"] =
                                std::to_string(processEntry.th32ProcessID);
                            info["memory_usage"] =
                                std::to_string(pmc.WorkingSetSize / 1024) +
                                " KB";

                            FILETIME creation_time, exit_time, kernel_time,
                                user_time;
                            if (GetProcessTimes(hProcess, &creation_time,
                                                &exit_time, &kernel_time,
                                                &user_time)) {
                                ULARGE_INTEGER kernel, user;
                                kernel.LowPart = kernel_time.dwLowDateTime;
                                kernel.HighPart = kernel_time.dwHighDateTime;
                                user.LowPart = user_time.dwLowDateTime;
                                user.HighPart = user_time.dwHighDateTime;

                                info["cpu_time"] =
                                    std::to_string(
                                        (kernel.QuadPart + user.QuadPart) /
                                        10000) +
                                    " ms";
                            }
                        }
                        CloseHandle(hProcess);
                    }
                    break;
                }
            } while (Process32Next(snapshot, &processEntry));
        }
        CloseHandle(snapshot);
    }
#elif defined(__APPLE__)
    int pid_count = proc_listallpids(NULL, 0);
    std::vector<pid_t> pids(pid_count);
    proc_listallpids(pids.data(), sizeof(pid_t) * pid_count);

    for (int i = 0; i < pid_count; i++) {
        char name[PROC_PIDPATHINFO_MAXSIZE];
        if (proc_name(pids[i], name, sizeof(name)) > 0) {
            std::string process_name(name);
            if (process_name.find(software_name) != std::string::npos) {
                info["pid"] = std::to_string(pids[i]);

                struct proc_taskinfo proc_info;
                if (proc_pidinfo(pids[i], PROC_PIDTASKINFO, 0, &proc_info,
                                 sizeof(proc_info)) > 0) {
                    info["memory_usage"] =
                        std::to_string(proc_info.pti_resident_size / 1024) +
                        " KB";
                    info["cpu_usage"] =
                        std::to_string(proc_info.pti_total_user +
                                       proc_info.pti_total_system) +
                        " ticks";
                }
                break;
            }
        }
    }
#elif defined(__linux__)
    DIR* dir = opendir("/proc");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_DIR) {
                char* end;
                long pid = strtol(entry->d_name, &end, 10);
                if (*end == '\0') {
                    std::string comm_path =
                        "/proc/" + std::string(entry->d_name) + "/comm";
                    FILE* comm_file = fopen(comm_path.c_str(), "r");
                    if (comm_file) {
                        char process_name[256];
                        if (fgets(process_name, sizeof(process_name),
                                  comm_file) != nullptr) {
                            size_t len = strlen(process_name);
                            if (len > 0 && process_name[len - 1] == '\n')
                                process_name[len - 1] = '\0';

                            if (strstr(process_name, software_name.c_str()) !=
                                nullptr) {
                                info["pid"] = std::to_string(pid);

                                std::string status_path =
                                    "/proc/" + std::string(entry->d_name) +
                                    "/status";
                                FILE* status_file =
                                    fopen(status_path.c_str(), "r");
                                if (status_file) {
                                    char line[256];
                                    while (fgets(line, sizeof(line),
                                                 status_file) != nullptr) {
                                        if (strncmp(line, "VmRSS:", 6) == 0) {
                                            char* value = line + 6;
                                            while (*value == ' ')
                                                value++;
                                            info["memory_usage"] = std::string(
                                                value, strcspn(value, "\n"));
                                        }
                                    }
                                    fclose(status_file);
                                }

                                std::string stat_path =
                                    "/proc/" + std::string(entry->d_name) +
                                    "/stat";
                                FILE* stat_file = fopen(stat_path.c_str(), "r");
                                if (stat_file) {
                                    unsigned long utime, stime;
                                    if (fscanf(
                                            stat_file,
                                            "%*d %*s %*c %*d %*d %*d %*d %*d "
                                            "%*u %*u %*u %*u %*u %lu %lu",
                                            &utime, &stime) == 2) {
                                        info["cpu_time"] =
                                            std::to_string(utime + stime) +
                                            " jiffies";
                                    }
                                    fclose(stat_file);
                                }
                                break;
                            }
                        }
                        fclose(comm_file);
                    }
                }
            }
        }
        closedir(dir);
    }
#endif

    if (!info.empty()) {
        spdlog::info("Retrieved process information for software: {}",
                     software_name);
    } else {
        spdlog::warn("No process information found for software: {}",
                     software_name);
    }
    return info;
}

auto launchSoftware(const fs::path& software_path,
                    const std::vector<std::string>& args) -> bool {
    spdlog::info("Launching software at path: {}", software_path.string());

#ifdef _WIN32
    std::wstring cmd = atom::utils::stringToWString(software_path.string());

    for (const auto& arg : args) {
        cmd += L" " + atom::utils::stringToWString(arg);
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (CreateProcessW(NULL, const_cast<LPWSTR>(cmd.c_str()), NULL, NULL, FALSE,
                       0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        spdlog::info("Successfully launched software: {}",
                     software_path.string());
        return true;
    } else {
        spdlog::error("Failed to launch software: {}. Error code: {}",
                      software_path.string(), GetLastError());
        return false;
    }
#elif defined(__APPLE__) || defined(__linux__)
    std::string cmd = software_path.string();

    pid_t pid = fork();

    if (pid < 0) {
        spdlog::error("Fork failed when trying to launch: {}", cmd);
        return false;
    } else if (pid == 0) {
        std::vector<char*> c_args;
        c_args.push_back(const_cast<char*>(cmd.c_str()));

        for (const auto& arg : args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        execv(cmd.c_str(), c_args.data());

        spdlog::error("execv failed when trying to launch: {}", cmd);
        exit(1);
    } else {
        spdlog::info("Successfully launched software: {} with PID: {}", cmd,
                     pid);
        return true;
    }
#else
    spdlog::error("launchSoftware not implemented for this platform");
    return false;
#endif
}

auto terminateSoftware(const std::string& software_name) -> bool {
    spdlog::info("Terminating software: {}", software_name);

#ifdef _WIN32
    bool success = false;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 processEntry;
        processEntry.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(snapshot, &processEntry)) {
            do {
                std::string wname(processEntry.szExeFile);
                std::string process_name(wname.begin(), wname.end());

                if (process_name.find(software_name) != std::string::npos) {
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE,
                                                  processEntry.th32ProcessID);
                    if (hProcess) {
                        if (TerminateProcess(hProcess, 0)) {
                            spdlog::info(
                                "Successfully terminated process: {} (PID: {})",
                                process_name, processEntry.th32ProcessID);
                            success = true;
                        } else {
                            spdlog::error(
                                "Failed to terminate process: {} (PID: {}). "
                                "Error: {}",
                                process_name, processEntry.th32ProcessID,
                                GetLastError());
                        }
                        CloseHandle(hProcess);
                    }
                }
            } while (Process32Next(snapshot, &processEntry));
        }
        CloseHandle(snapshot);
    }
    return success;
#elif defined(__APPLE__) || defined(__linux__)
    bool success = false;

    auto process_info = getProcessInfo(software_name);
    if (process_info.find("pid") != process_info.end()) {
        int pid = std::stoi(process_info["pid"]);

        if (kill(pid, SIGTERM) == 0) {
            spdlog::info("Successfully sent SIGTERM to process: {} (PID: {})",
                         software_name, pid);

            int status;
            pid_t result = waitpid(pid, &status, WNOHANG);

            if (result == 0) {
                spdlog::info(
                    "Process didn't terminate with SIGTERM, trying SIGKILL");
                if (kill(pid, SIGKILL) == 0) {
                    spdlog::info(
                        "Successfully sent SIGKILL to process: {} (PID: {})",
                        software_name, pid);
                    success = true;
                } else {
                    spdlog::error(
                        "Failed to send SIGKILL to process: {} (PID: {})",
                        software_name, pid);
                }
            } else {
                success = true;
            }
        } else {
            spdlog::error("Failed to send SIGTERM to process: {} (PID: {})",
                          software_name, pid);
        }
    } else {
        spdlog::warn("Process not found: {}", software_name);
    }

    return success;
#else
    spdlog::error("terminateSoftware not implemented for this platform");
    return false;
#endif
}

auto monitorSoftwareUsage(
    const std::string& software_name,
    std::function<void(const std::map<std::string, std::string>&)> callback,
    int interval_ms) -> int {
    spdlog::info("Starting monitoring for software: {} with interval: {} ms",
                 software_name, interval_ms);

    std::lock_guard<std::mutex> lock(g_monitors_mutex);
    int monitor_id = g_next_monitor_id++;

    MonitorInfo& info = g_monitors[monitor_id];
    info.software_name = software_name;
    info.callback = callback;
    info.interval_ms = interval_ms;
    info.running = true;

    info.thread = std::thread([monitor_id]() {
        auto& monitor = g_monitors[monitor_id];

        while (monitor.running) {
            auto process_info = getProcessInfo(monitor.software_name);

            if (!process_info.empty()) {
                monitor.callback(process_info);
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(monitor.interval_ms));
        }

        spdlog::info("Monitoring thread for software: {} (ID: {}) exiting",
                     monitor.software_name, monitor_id);
    });

    spdlog::info("Started monitoring for software: {} with ID: {}",
                 software_name, monitor_id);
    return monitor_id;
}

auto stopMonitoring(int monitor_id) -> bool {
    spdlog::info("Stopping monitoring for ID: {}", monitor_id);

    std::lock_guard<std::mutex> lock(g_monitors_mutex);
    auto it = g_monitors.find(monitor_id);

    if (it != g_monitors.end()) {
        it->second.running = false;
        if (it->second.thread.joinable()) {
            it->second.thread.join();
        }
        g_monitors.erase(it);
        spdlog::info("Successfully stopped monitoring for ID: {}", monitor_id);
        return true;
    }

    spdlog::warn("Monitor ID not found: {}", monitor_id);
    return false;
}

auto checkSoftwareUpdates(const std::string& software_name,
                          const std::string& current_version) -> std::string {
    spdlog::info("Checking updates for software: {} (current version: {})",
                 software_name, current_version);

#ifdef _WIN32
    if (software_name == "Microsoft Office") {
        return "16.0.14729.20254";
    } else if (software_name == "Google Chrome") {
        return "96.0.4664.110";
    }
#elif defined(__APPLE__)
    if (software_name == "Safari") {
        return "15.2";
    } else if (software_name == "Final Cut Pro") {
        return "10.6.1";
    }
#elif defined(__linux__)
    std::string cmd = "apt-cache policy " + software_name +
                      " | grep Candidate | awk '{print $2}'";
    std::array<char, 128> buffer;
    std::string result;
    using PcloseDeleter = int (*)(FILE*);
    std::unique_ptr<FILE, PcloseDeleter> pipe(popen(cmd.c_str(), "r"), pclose);

    if (pipe) {
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }

        if (!result.empty()) {
            result.erase(std::remove(result.begin(), result.end(), '\n'),
                         result.end());

            if (result != current_version) {
                spdlog::info("Update available for {}: {} -> {}", software_name,
                             current_version, result);
                return result;
            }
        }
    }
#endif

    spdlog::debug("No updates found for software: {}", software_name);
    return "";
}

}  // namespace atom::system
