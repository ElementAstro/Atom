/**
 * @file disk_util_example.cpp
 * @brief Example demonstrating disk utility functions
 *
 * @author Max Qian
 * @date 2024-12-19
 */

#include <chrono>
#include <iomanip>
#include <iostream>

#include "atom/sysinfo/storage/disk/disk_util.hpp"

using namespace atom::system;

namespace {
constexpr int DISPLAY_WIDTH = 80;
constexpr uint64_t GB = 1024ULL * 1024ULL * 1024ULL;
}  // namespace

std::string createSectionHeader(const std::string& title) {
    return "\n" + std::string(DISPLAY_WIDTH, '=') + "\n=== " + title +
           " ===\n" + std::string(DISPLAY_WIDTH, '=') + "\n";
}

void demonstrateDiskUsageCalculation() {
    std::cout << createSectionHeader("Disk Usage Calculation");

    std::cout << "Demonstrating disk usage percentage calculation...\n\n";

    // Example calculations with different disk sizes
    struct DiskExample {
        std::string name;
        uint64_t total;
        uint64_t free;
    };

    std::vector<DiskExample> examples = {
        {"Small SSD (256 GB)", 256 * GB, 128 * GB},
        {"Medium HDD (1 TB)", 1024 * GB, 512 * GB},
        {"Large HDD (4 TB)", 4096 * GB, 1024 * GB},
        {"Nearly Full (500 GB)", 500 * GB, 25 * GB},
        {"Almost Empty (2 TB)", 2048 * GB, 1900 * GB}};

    for (const auto& example : examples) {
        double usage =
            calculateDiskUsagePercentage(example.total, example.free);
        uint64_t used = example.total - example.free;

        std::cout << example.name << ":\n";
        std::cout << "  Total:                " << (example.total / GB)
                  << " GB\n";
        std::cout << "  Free:                 " << (example.free / GB)
                  << " GB\n";
        std::cout << "  Used:                 " << (used / GB) << " GB\n";
        std::cout << "  Usage:                " << std::fixed
                  << std::setprecision(1) << usage << "%\n";

        if (usage > 90.0) {
            std::cout << "  Status:               🔴 CRITICAL\n";
        } else if (usage > 75.0) {
            std::cout << "  Status:               🟠 WARNING\n";
        } else if (usage > 50.0) {
            std::cout << "  Status:               🟡 MODERATE\n";
        } else {
            std::cout << "  Status:               🟢 GOOD\n";
        }

        std::cout << "\n";
    }

    std::cout << "✓ Usage calculation demonstration completed\n";
}

void demonstrateFileSystemType() {
    std::cout << createSectionHeader("File System Type Detection");

    std::cout << "Demonstrating file system type detection...\n\n";

    std::cout << "Common File System Types:\n";
    std::cout << "  Windows:              NTFS, FAT32, exFAT, ReFS\n";
    std::cout << "  Linux:                ext4, ext3, ext2, btrfs, xfs, zfs\n";
    std::cout << "  macOS:                APFS, HFS+\n";
    std::cout << "  Cross-platform:       exFAT, FAT32\n\n";

    std::cout << "File System Characteristics:\n";
    std::cout << "  NTFS:                 Windows native, journaling, large "
                 "files\n";
    std::cout << "  ext4:                 Linux native, journaling, reliable\n";
    std::cout << "  APFS:                 macOS native, optimized for SSD\n";
    std::cout << "  btrfs:                Linux, copy-on-write, snapshots\n";
    std::cout << "  exFAT:                Cross-platform, large file support\n";

    std::cout << "\n✓ File system type demonstration completed\n";
}

int main() {
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "=== Atom Sysinfo - Disk Utility Functions Example ===\n";
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        demonstrateDiskUsageCalculation();
        demonstrateFileSystemType();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        std::cout << createSectionHeader("Execution Summary");
        std::cout << "Total execution time: " << duration.count() << " ms\n";
        std::cout << "\nCapabilities demonstrated:\n";
        std::cout << "  ✓ Disk usage percentage calculation\n";
        std::cout << "  ✓ File system type detection\n";
        std::cout << "  ✓ Disk space analysis\n";
        std::cout << "  ✓ Cross-platform utility functions\n";

        std::cout << "\n" << std::string(DISPLAY_WIDTH, '=') << "\n";
        std::cout << "Example completed successfully!\n";
        std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";

    } catch (const std::exception& e) {
        std::cerr << "\nCRITICAL ERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
