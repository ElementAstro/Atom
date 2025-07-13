#include "nodebugger.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <winternl.h>
#include <tlhelp32.h>
// clang-format on
typedef NTSTATUS(NTAPI* pNtQueryInformationProcess)(
    IN HANDLE ProcessHandle, IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation, IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength OPTIONAL);

#elif defined(__linux__)
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fstream>
#include <string>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace atom::system {

std::atomic<bool> g_monitoringActive = false;
std::thread g_monitoringThread;
std::mutex g_configMutex;
AntiDebugConfig g_currentConfig;

// Basic debugger detection
#ifdef _WIN32
bool isBasicDebuggerAttached() {
    return IsDebuggerPresent() || ((PPEB)__readgsqword(0x60))->BeingDebugged;
}
#elif defined(__linux__)
bool isBasicDebuggerAttached() {
    std::ifstream status("/proc/self/status");
    if (!status.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(status, line)) {
        if (line.find("TracerPid:") == 0) {
            int tracerPid;
            sscanf(line.c_str(), "TracerPid:\t%d", &tracerPid);
            return tracerPid != 0;
        }
    }

    int ptraceResult = ptrace(PTRACE_TRACEME, 0, 1, 0);
    if (ptraceResult < 0) {
        return true;
    } else {
        ptrace(PTRACE_DETACH, 0, 1, 0);
        return false;
    }
}
#elif defined(__APPLE__)
bool isBasicDebuggerAttached() {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
    struct kinfo_proc info;
    size_t size = sizeof(info);

    if (sysctl(mib, 4, &info, &size, NULL, 0) == 0) {
        return (info.kp_proc.p_flag & P_TRACED) != 0;
    }
    return false;
}
#else
bool isBasicDebuggerAttached() { return false; }
#endif

// Timing-based detection
bool isDebuggerDetectedByTiming(uint32_t threshold) {
    auto start = std::chrono::high_resolution_clock::now();

    [[maybe_unused]] int counter = 0;  // Fixed: Removed volatile qualifier
    for (int i = 0; i < 10000; i++) {
        counter += i % 2;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();

    return (duration > threshold);
}

// Exception-based detection
#ifdef _WIN32
bool isDebuggerDetectedByException() {
    bool debuggerDetected = true;

    try {
        RaiseException(DBG_PRINTEXCEPTION_C, 0, 0, NULL);
    } catch (...) {  // Fixed: Replaced __except with try-catch
        debuggerDetected = false;
    }

    return debuggerDetected;
}
#else
bool isDebuggerDetectedByException() { return false; }
#endif

// Hardware breakpoint detection
#ifdef _WIN32
bool checkHardwareBreakpoints() {
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    if (GetThreadContext(GetCurrentThread(), &ctx)) {
        return (ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0);
    }
    return false;
}
#else
bool checkHardwareBreakpoints() { return false; }
#endif

// Memory breakpoint detection
bool checkMemoryBreakpoints(const void* address, size_t size) {
    static std::vector<uint8_t> originalBytes;
    static bool initialized = false;

    if (!initialized) {
        originalBytes.resize(size);
        memcpy(originalBytes.data(), address, size);
        initialized = true;
        return false;
    }

    std::vector<uint8_t> currentBytes(size);
    memcpy(currentBytes.data(), address, size);

    return memcmp(originalBytes.data(), currentBytes.data(), size) != 0;
}

// Process environment detection
#ifdef _WIN32
bool checkProcessEnvironment() {
    PPEB pPeb = (PPEB)__readgsqword(0x60);
    return pPeb->BeingDebugged;
}

bool checkParentProcess() {
    DWORD parentPID = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        DWORD currentPID = GetCurrentProcessId();
        do {
            if (pe32.th32ProcessID == currentPID) {
                parentPID = pe32.th32ParentProcessID;
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);

    if (parentPID == 0)
        return false;

    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return false;

    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (pe32.th32ProcessID == parentPID) {
                char lowerName[MAX_PATH];
                strcpy_s(lowerName, pe32.szExeFile);
                for (char* p = lowerName; *p; ++p)
                    *p = tolower(*p);

                if (strstr(lowerName, "dbg") || strstr(lowerName, "debug") ||
                    strstr(lowerName, "x64dbg") ||
                    strstr(lowerName, "windbg") ||
                    strstr(lowerName, "ollydbg") || strstr(lowerName, "ida")) {
                    CloseHandle(hSnapshot);
                    return true;
                }
                break;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return false;
}
#else
bool checkProcessEnvironment() { return false; }

bool checkParentProcess() { return false; }
#endif

// Thread context check
#ifdef _WIN32
bool checkThreadContext() {
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS | CONTEXT_FULL;

    if (!GetThreadContext(GetCurrentThread(), &ctx)) {
        return false;
    }

    return (ctx.EFlags & 0x100) != 0;
}
#else
bool checkThreadContext() { return false; }
#endif

// Core detection function
bool isDebuggerAttached(DebuggerDetectionMethod method) {
    switch (method) {
        case DebuggerDetectionMethod::BASIC_CHECK:
            return isBasicDebuggerAttached();

        case DebuggerDetectionMethod::TIMING_CHECK:
            return isDebuggerDetectedByTiming(g_currentConfig.timingThreshold);

        case DebuggerDetectionMethod::EXCEPTION_BASED:
            return isDebuggerDetectedByException();

        case DebuggerDetectionMethod::HARDWARE_BREAKPOINTS:
            return checkHardwareBreakpoints();

        case DebuggerDetectionMethod::MEMORY_BREAKPOINTS:
            return checkMemoryBreakpoints(
                reinterpret_cast<void*>(&isDebuggerAttached),
                100);  // Fixed: Removed invalid static_cast

        case DebuggerDetectionMethod::PROCESS_ENVIRONMENT:
            return checkProcessEnvironment();

        case DebuggerDetectionMethod::PARENT_PROCESS:
            return checkParentProcess();

        case DebuggerDetectionMethod::THREAD_CONTEXT:
            return checkThreadContext();

        case DebuggerDetectionMethod::ALL_METHODS:
            return isBasicDebuggerAttached() ||
                   isDebuggerDetectedByTiming(
                       g_currentConfig.timingThreshold) ||
                   isDebuggerDetectedByException() ||
                   checkHardwareBreakpoints() ||
                   checkMemoryBreakpoints(
                       reinterpret_cast<void*>(&isDebuggerAttached),
                       100) ||  // Fixed: Removed invalid static_cast
                   checkProcessEnvironment() ||
                   checkParentProcess() || checkThreadContext();

        default:
            return isBasicDebuggerAttached();
    }
}

// Anti-debug action execution
void executeAntiDebugAction(AntiDebugAction action,
                            std::function<void()> customAction) {
    switch (action) {
        case AntiDebugAction::EXIT:
            std::exit(EXIT_FAILURE);
            break;

        case AntiDebugAction::CRASH:
            *reinterpret_cast<volatile int*>(0) = 0;
            __builtin_trap();
            break;

        case AntiDebugAction::MISLEAD:
            srand(static_cast<unsigned>(time(nullptr)));
            for (int i = 0; i < 100; i++) {
                volatile int* ptr = new int[rand() % 1000 + 1];
                for (int j = 0; j < (rand() % 1000 + 1); j++) {
                    ptr[j] = rand();
                }
            }
            break;

        case AntiDebugAction::CORRUPT_MEMORY: {
            void* baseAddr = reinterpret_cast<void*>(executeAntiDebugAction);
            uint8_t* bytes = reinterpret_cast<uint8_t*>(baseAddr);

#ifdef _WIN32
            DWORD oldProtect;
            VirtualProtect(baseAddr, 100, PAGE_EXECUTE_READWRITE, &oldProtect);
#endif

            for (int i = 20; i < 30; i++) {
                bytes[i] ^= 0xFF;
            }

#ifdef _WIN32
            VirtualProtect(baseAddr, 100, oldProtect, &oldProtect);
#endif
        } break;

        case AntiDebugAction::CUSTOM:
            if (customAction) {
                customAction();
            }
            break;
    }
}

// Main handler that checks for debuggers and performs the specified action
void handleDebuggerDetection(const AntiDebugConfig& config) {
    {
        std::lock_guard<std::mutex> lock(g_configMutex);
        g_currentConfig = config;
    }

    if (!config.enabled)
        return;

    if (isDebuggerAttached(config.method)) {
        executeAntiDebugAction(config.action, config.customAction);
    }

    if (config.continuousMonitoring) {
        g_monitoringActive = true;
        g_monitoringThread = std::thread([config]() {
            while (g_monitoringActive) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(config.checkInterval));
                if (isDebuggerAttached(config.method)) {
                    executeAntiDebugAction(config.action, config.customAction);
                }
            }
        });
    }
}

void startAntiDebugMonitoring(const AntiDebugConfig& config) {
    handleDebuggerDetection(config);
}

void stopAntiDebugMonitoring() {
    g_monitoringActive = false;
    if (g_monitoringThread.joinable()) {
        g_monitoringThread.join();
    }
}
}  // namespace atom::system
