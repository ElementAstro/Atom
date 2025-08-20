/*
 * crash.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-4-4

Description: Crash Report

**************************************************/

#include "crash.hpp"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#ifdef _WIN32
#include <dbghelp.h>
#include <windows.h>
#endif

#include "atom/error/stacktrace.hpp"
#include "atom/sysinfo/cpu.hpp"
#include "atom/sysinfo/disk.hpp"
#include "atom/sysinfo/memory.hpp"
#include "atom/sysinfo/os.hpp"
#include "atom/utils/time.hpp"
#include "crash_quotes.hpp"
#include "env.hpp"
#include "platform.hpp"

#include <spdlog/spdlog.h>

namespace atom::system {

auto getSystemInfo() -> std::string {
    spdlog::info("Collecting system information for crash report");

    std::stringstream sss;

    try {
        auto osInfo = getOperatingSystemInfo();
        sss << "==================== System Information ====================\n";
        sss << std::format("Operating System: {} {}\n", osInfo.osName,
                           osInfo.osVersion);
        sss << std::format("Architecture: {}\n", osInfo.architecture);
        sss << std::format("Kernel Version: {}\n", osInfo.kernelVersion);
        sss << std::format("Computer Name: {}\n", osInfo.computerName);
        sss << std::format("Compiler: {}\n", osInfo.compiler);
        sss << std::format("GUI: {}\n\n", ATOM_HAS_GUI() ? "Yes" : "No");

        sss << "==================== CPU Information ====================\n";
        sss << std::format("Usage: {:.2f}%\n", getCurrentCpuUsage());
        sss << std::format("Model: {}\n", getCPUModel());
        sss << std::format("Frequency: {:.2f} GHz\n", getProcessorFrequency());
        sss << std::format("Temperature: {:.1f} °C\n",
                           getCurrentCpuTemperature());
        sss << std::format("Cores: {}\n", getNumberOfPhysicalCores());
        sss << std::format("Packages: {}\n\n", getNumberOfPhysicalPackages());

        sss << "==================== Memory Status ====================\n";
        sss << std::format("Usage: {:.2f}%\n", getMemoryUsage());
        sss << std::format("Total: {:.2f} MB\n",
                           static_cast<double>(getTotalMemorySize()));
        sss << std::format("Free: {:.2f} MB\n\n",
                           static_cast<double>(getAvailableMemorySize()));

        sss << "==================== Disk Usage ====================\n";
        for (const auto& [drive, usage] : getDiskUsage()) {
            sss << std::format("{}: {:.2f}%\n", drive, usage);
        }
    } catch (const std::exception& e) {
        spdlog::error("Error collecting system information: {}", e.what());
        sss << "Error collecting system information: " << e.what() << "\n";
    }

    spdlog::info("System information collection completed");
    return sss.str();
}

void saveCrashLog(std::string_view error_msg) {
    spdlog::critical("Crash detected, saving crash log with error: {}",
                     error_msg);

    try {
        std::string systemInfo = getSystemInfo();
        std::string environmentInfo;
        environmentInfo.reserve(1024);

        try {
            for (const auto& [key, value] : utils::Env::Environ()) {
                environmentInfo += std::format("{}: {}\n", key, value);
            }
        } catch (const std::exception& e) {
            spdlog::error("Failed to collect environment variables: {}",
                          e.what());
            environmentInfo = "Failed to collect environment variables\n";
        }

        std::stringstream sss;
        sss << "==================== Crash Report ====================\n";
        sss << std::format("Program crashed at: {}\n",
                           utils::getChinaTimestampString());
        sss << std::format("Error message: {}\n\n", error_msg);

        sss << "==================== Stack Trace ====================\n";
        try {
            atom::error::StackTrace stackTrace;
            sss << stackTrace.toString() << "\n\n";
        } catch (const std::exception& e) {
            spdlog::error("Failed to generate stack trace: {}", e.what());
            sss << "Failed to generate stack trace: " << e.what() << "\n\n";
        }

        sss << systemInfo << "\n";

        sss << "================= Environment Variables ===================\n";
        sss << environmentInfo << "\n";

        try {
            QuoteManager quotes;
            if (quotes.loadQuotesFromJson("./quotes.json")) {
                std::string quote = quotes.getRandomQuote();
                if (!quote.empty()) {
                    sss << std::format(
                        "============ Famous Saying: {} ============\n", quote);
                }
            }
        } catch (const std::exception& e) {
            spdlog::warn("Failed to load quotes: {}", e.what());
        }

        auto now = std::chrono::system_clock::now();
        std::time_t nowC = std::chrono::system_clock::to_time_t(now);
        std::tm localTime;

#ifdef _WIN32
        if (localtime_s(&localTime, &nowC) != 0) {
#else
        if (localtime_r(&nowC, &localTime) == nullptr) {
#endif
            spdlog::error("Failed to get local time for crash report filename");
            throw std::runtime_error("Failed to get local time");
        }

        std::stringstream logFileName;
        logFileName << "crash_report/crash_"
                    << std::put_time(&localTime, "%Y%m%d_%H%M%S") << ".log";

        std::filesystem::path dirPath("crash_report");
        if (!std::filesystem::exists(dirPath)) {
            std::filesystem::create_directories(dirPath);
            spdlog::info("Created crash_report directory");
        }

        std::ofstream ofs(logFileName.str(), std::ios::out | std::ios::trunc);
        if (ofs.good()) {
            ofs << sss.str();
            ofs.close();
            spdlog::info("Crash log saved to {}", logFileName.str());
        } else {
            spdlog::error("Failed to save crash log to {}", logFileName.str());
            throw std::runtime_error("Failed to write crash log file");
        }

#ifdef _WIN32
        try {
            std::stringstream dumpFileName;
            dumpFileName << "crash_report/crash_"
                         << std::put_time(&localTime, "%Y%m%d_%H%M%S")
                         << ".dmp";
            std::string dumpFile = dumpFileName.str();

            HANDLE hFile = CreateFileA(
                dumpFile.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hFile == INVALID_HANDLE_VALUE) {
                spdlog::error("Failed to create dump file {}", dumpFile);
            } else {
                MINIDUMP_EXCEPTION_INFORMATION mdei;
                mdei.ThreadId = GetCurrentThreadId();
                mdei.ExceptionPointers = nullptr;
                mdei.ClientPointers = FALSE;

                BOOL dumpResult = MiniDumpWriteDump(
                    GetCurrentProcess(), GetCurrentProcessId(), hFile,
                    MiniDumpNormal, nullptr, nullptr, nullptr);

                if (dumpResult) {
                    spdlog::info("Minidump file created at {}", dumpFile);
                } else {
                    spdlog::error("Failed to write minidump file {}, error: {}",
                                  dumpFile, GetLastError());
                }
                CloseHandle(hFile);
            }
        } catch (const std::exception& e) {
            spdlog::error("Exception while creating minidump: {}", e.what());
        }
#endif

    } catch (const std::exception& e) {
        spdlog::critical("Critical error while saving crash log: {}", e.what());

        try {
            std::ofstream emergencyLog("emergency_crash.log",
                                       std::ios::out | std::ios::app);
            if (emergencyLog.good()) {
                emergencyLog
                    << std::format("Emergency crash log - {}: {}\n",
                                   utils::getChinaTimestampString(), error_msg);
                emergencyLog << std::format("Error saving full crash log: {}\n",
                                            e.what());
                emergencyLog.close();
                spdlog::info("Emergency crash log written");
            }
        } catch (...) {
            spdlog::critical("Failed to write emergency crash log");
        }
    }

    spdlog::info("Crash log processing completed");
}

}  // namespace atom::system
