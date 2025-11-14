/**
 * @file disk_info_example.cpp
 * @brief Example demonstrating detailed disk information retrieval
 *
 * @author Max Qian
 * @date 2024-12-19
 */

#include <chrono>
#include <iomanip>
#include <iostream>

#include "atom/sysinfo/storage/disk/disk_info.hpp"

using namespace atom::system;

namespace {
constexpr int DISPLAY_WIDTH = 80;
constexpr double GB_DIVISOR = 1024.0 * 1024.0 * 1024.0;
}  // namespace

std::string createSectionHeader(const std::string& title) {
    return "\n" + std::string(DISPLAY_WIDTH, '=') + "\n=== " + title +
           " ===\n" + std::string(DISPLAY_WIDTH, '=') + "\n";
}

std::string formatBytes(uint64_t bytes) {
    double gb = bytes / GB_DIVISOR;
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << gb << " GB";
    return oss.str();
}

void demonstrateDiskInfo() {
    std::cout << createSectionHeader("Detailed Disk Information");

    try {
        auto disks = getDiskInfo(true);

        if (disks.empty()) {
            std::cout << "No disks detected.\n";
            return;
        }

        std::cout << "Total Disks: " << disks.size() << "\n\n";

        for (size_t i = 0; i < disks.size(); ++i) {
            const auto& disk = disks[i];

            std::cout << "Disk " << (i + 1) << ":\n";
            std::cout << "  Path:                 " << disk.path << "\n";
            std::cout << "  Filesystem:           " << disk.filesystem << "\n";
            std::cout << "  Total Space:          "
                      << formatBytes(disk.totalSpace) << "\n";
            std::cout << "  Free Space:           "
                      << formatBytes(disk.freeSpace) << "\n";
            std::cout << "  Used Space:           "
                      << formatBytes(disk.totalSpace - disk.freeSpace) << "\n";
            std::cout << "  Usage:                " << std::fixed
                      << std::setprecision(1) << disk.usagePercentage << "%\n";

            if (disk.usagePercentage > 90.0) {
                std::cout
                    << "  Status:               🔴 CRITICAL - Nearly full\n";
            } else if (disk.usagePercentage > 75.0) {
                std::cout
                    << "  Status:               🟠 WARNING - Getting full\n";
            } else if (disk.usagePercentage > 50.0) {
                std::cout
                    << "  Status:               🟡 MODERATE - Half full\n";
            } else {
                std::cout
                    << "  Status:               🟢 GOOD - Plenty of space\n";
            }

            if (!disk.model.empty()) {
                std::cout << "  Model:                " << disk.model << "\n";
            }

            if (i < disks.size() - 1) {
                std::cout << "\n";
            }
        }

        std::cout << "\n✓ Disk information retrieval completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error: " << e.what() << "\n";
    }
}

void demonstrateDiskUsage() {
    std::cout << createSectionHeader("Disk Usage Summary");

    try {
        auto usage = getDiskUsage();

        if (usage.empty()) {
            std::cout << "No disk usage information available.\n";
            return;
        }

        std::cout << "Disk Usage Summary:\n\n";

        for (const auto& [path, percent] : usage) {
            std::cout << "  " << std::setw(20) << std::left << path << " "
                      << std::fixed << std::setprecision(1) << std::setw(6)
                      << std::right << percent << "%\n";
        }

        std::cout << "\n✓ Disk usage summary completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error: " << e.what() << "\n";
    }
}

int main() {
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "=== Atom Sysinfo - Disk Information Example ===\n";
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        demonstrateDiskInfo();
        demonstrateDiskUsage();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        std::cout << createSectionHeader("Execution Summary");
        std::cout << "Total execution time: " << duration.count() << " ms\n";
        std::cout << "\nCapabilities demonstrated:\n";
        std::cout << "  ✓ Detailed disk information retrieval\n";
        std::cout << "  ✓ Disk usage monitoring\n";
        std::cout << "  ✓ Filesystem type detection\n";
        std::cout << "  ✓ Space utilization analysis\n";

        std::cout << "\n" << std::string(DISPLAY_WIDTH, '=') << "\n";
        std::cout << "Example completed successfully!\n";
        std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";

    } catch (const std::exception& e) {
        std::cerr << "\nCRITICAL ERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
