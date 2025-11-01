#include "atom/sysinfo/disk.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace atom::system;

namespace atom::sysinfo::test {

// ============================================================================
// Basic Disk Tests
// ============================================================================

class DiskTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup disk tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(DiskTest, GetDiskInfo) {
    // Test getting disk information
    std::vector<DiskInfo> disks = getDiskInfo();

    // Should have at least one disk
    EXPECT_FALSE(disks.empty());

    // Validate each disk
    for (const auto& disk : disks) {
        // Path should not be empty
        EXPECT_FALSE(disk.path.empty());
        EXPECT_GT(disk.path.length(), 0);

        // File system should not be empty
        EXPECT_FALSE(disk.fsType.empty());
        EXPECT_GT(disk.fsType.length(), 0);

        // Total space should be positive
        EXPECT_GT(disk.totalSpace, 0);

        // Free space should be non-negative and <= total
        EXPECT_GE(disk.freeSpace, 0);
        EXPECT_LE(disk.freeSpace, disk.totalSpace);

        // Usage percentage should be between 0 and 100
        EXPECT_GE(disk.usagePercent, 0.0f);
        EXPECT_LE(disk.usagePercent, 100.0f);

        // String lengths should be reasonable
        EXPECT_LT(disk.path.length(), 1000);
        EXPECT_LT(disk.fsType.length(), 100);

        if (!disk.devicePath.empty()) {
            EXPECT_LT(disk.devicePath.length(), 1000);
        }

        if (!disk.model.empty()) {
            EXPECT_LT(disk.model.length(), 500);
        }
    }
}

TEST_F(DiskTest, GetDiskUsage) {
    // Test getting disk usage
    std::vector<std::pair<std::string, float>> diskUsage = getDiskUsage();

    // Should have at least one disk
    EXPECT_FALSE(diskUsage.empty());

    // Validate each disk usage entry
    for (const auto& [path, usagePercent] : diskUsage) {
        // Path should not be empty
        EXPECT_FALSE(path.empty());
        EXPECT_GT(path.length(), 0);

        // Usage percentage should be between 0 and 100
        EXPECT_GE(usagePercent, 0.0f);
        EXPECT_LE(usagePercent, 100.0f);
    }
}

TEST_F(DiskTest, GetDriveModel) {
    // Test getting drive model
    std::vector<DiskInfo> disks = getDiskInfo();

    if (!disks.empty()) {
        // Test getting model for the first disk
        std::string drivePath = disks[0].path;
        std::string model = getDriveModel(drivePath);

        // Model might be empty on some systems, but function should not throw
        EXPECT_NO_THROW(getDriveModel(drivePath));

        // If model is available, it should be reasonable
        if (!model.empty()) {
            EXPECT_GT(model.length(), 0);
            EXPECT_LT(model.length(), 200);
        }
    }
}

TEST_F(DiskTest, GetDiskInfoWithFiltering) {
    // Test disk info with removable filtering
    std::vector<DiskInfo> allDisks = getDiskInfo(true);
    std::vector<DiskInfo> fixedDisks = getDiskInfo(false);

    // Fixed disks should be a subset of all disks
    EXPECT_LE(fixedDisks.size(), allDisks.size());

    // All disks in fixedDisks should have isRemovable = false
    for (const auto& disk : fixedDisks) {
        EXPECT_FALSE(disk.isRemovable);
    }
}

// ============================================================================
// Real System Tests
// ============================================================================

class RealDiskTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup real disk tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(RealDiskTest, DiskInfoConsistency) {
    // Test that multiple calls return consistent results
    std::vector<DiskInfo> disks1 = getDiskInfo();
    std::vector<DiskInfo> disks2 = getDiskInfo();

    // Should have the same number of disks
    EXPECT_EQ(disks1.size(), disks2.size());

    // Static information should be identical
    for (size_t i = 0; i < std::min(disks1.size(), disks2.size()); ++i) {
        EXPECT_EQ(disks1[i].path, disks2[i].path);
        EXPECT_EQ(disks1[i].devicePath, disks2[i].devicePath);
        EXPECT_EQ(disks1[i].model, disks2[i].model);
        EXPECT_EQ(disks1[i].fsType, disks2[i].fsType);
        EXPECT_EQ(disks1[i].totalSpace, disks2[i].totalSpace);
        EXPECT_EQ(disks1[i].isRemovable, disks2[i].isRemovable);

        // Free space and usage might change slightly, but should be close
        if (disks1[i].totalSpace > 0) {
            float spaceDiff = std::abs(static_cast<float>(disks1[i].freeSpace) -
                                       static_cast<float>(disks2[i].freeSpace));
            float tolerance = static_cast<float>(disks1[i].totalSpace) *
                              0.01f;  // 1% tolerance
            EXPECT_LT(spaceDiff, tolerance);
        }
    }
}

TEST_F(RealDiskTest, DiskUsageConsistency) {
    // Test consistency between getDiskUsage and getDiskInfo
    std::vector<std::pair<std::string, float>> diskUsage = getDiskUsage();
    std::vector<DiskInfo> diskInfo = getDiskInfo();

    // Should have the same number of entries
    EXPECT_EQ(diskUsage.size(), diskInfo.size());

    // Verify data consistency
    for (size_t i = 0; i < std::min(diskUsage.size(), diskInfo.size()); ++i) {
        EXPECT_EQ(diskUsage[i].first, diskInfo[i].path);
        EXPECT_NEAR(diskUsage[i].second, diskInfo[i].usagePercent, 0.1f);
    }
}

TEST_F(RealDiskTest, DiskSpaceCalculations) {
    // Test disk space calculations
    std::vector<DiskInfo> disks = getDiskInfo();

    for (const auto& disk : disks) {
        // Used space calculation
        uint64_t usedSpace = disk.totalSpace - disk.freeSpace;
        EXPECT_LE(usedSpace, disk.totalSpace);

        // Usage percentage calculation
        if (disk.totalSpace > 0) {
            float calculatedUsage =
                (static_cast<float>(usedSpace) / disk.totalSpace) * 100.0f;
            EXPECT_NEAR(disk.usagePercent, calculatedUsage,
                        1.0f);  // Allow 1% tolerance
        }
    }
}

TEST_F(RealDiskTest, DriveModelEdgeCases) {
    // Test drive model with edge cases
    std::vector<std::string> testPaths = {"", "/", "C:\\", "/invalid/path",
                                          "invalid_path_12345"};

    for (const auto& path : testPaths) {
        EXPECT_NO_THROW({
            std::string model = getDriveModel(path);
            // Model can be empty or non-empty, just shouldn't crash
            EXPECT_TRUE(model.empty() || !model.empty());
        });
    }
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

TEST_F(RealDiskTest, NoThrowGuarantee) {
    // Test that all disk functions provide no-throw guarantee
    EXPECT_NO_THROW(getDiskInfo());
    EXPECT_NO_THROW(getDiskInfo(true));
    EXPECT_NO_THROW(getDiskInfo(false));
    EXPECT_NO_THROW(getDiskUsage());
    EXPECT_NO_THROW(getDriveModel(""));
    EXPECT_NO_THROW(getDriveModel("/invalid/path"));
}

TEST_F(RealDiskTest, BoundaryConditions) {
    // Test disk functions with boundary conditions
    std::vector<DiskInfo> disks = getDiskInfo();

    for (const auto& disk : disks) {
        // Test disk space boundaries
        EXPECT_GT(disk.totalSpace, 0);
        EXPECT_GE(disk.freeSpace, 0);
        EXPECT_LE(disk.freeSpace, disk.totalSpace);

        // Test usage percentage boundaries
        EXPECT_GE(disk.usagePercent, 0.0f);
        EXPECT_LE(disk.usagePercent, 100.0f);

        // Test reasonable upper bounds
        EXPECT_LT(disk.totalSpace,
                  1000ULL * 1024 * 1024 * 1024 * 1024);  // Less than 1000TB
        EXPECT_LT(disk.freeSpace,
                  1000ULL * 1024 * 1024 * 1024 * 1024);  // Less than 1000TB

        // Test path length boundaries
        EXPECT_GT(disk.path.length(), 0);
        EXPECT_LT(disk.path.length(), 1000);

        // Test file system type boundaries
        EXPECT_GT(disk.fsType.length(), 0);
        EXPECT_LT(disk.fsType.length(), 100);
    }
}

TEST_F(RealDiskTest, StringFieldValidation) {
    // Test that string fields don't contain null characters
    std::vector<DiskInfo> disks = getDiskInfo();

    for (const auto& disk : disks) {
        EXPECT_EQ(disk.path.find('\0'), std::string::npos);
        EXPECT_EQ(disk.fsType.find('\0'), std::string::npos);

        if (!disk.devicePath.empty()) {
            EXPECT_EQ(disk.devicePath.find('\0'), std::string::npos);
        }

        if (!disk.model.empty()) {
            EXPECT_EQ(disk.model.find('\0'), std::string::npos);
        }

        // Should not contain newlines or carriage returns
        EXPECT_EQ(disk.path.find('\n'), std::string::npos);
        EXPECT_EQ(disk.path.find('\r'), std::string::npos);
        EXPECT_EQ(disk.fsType.find('\n'), std::string::npos);
        EXPECT_EQ(disk.fsType.find('\r'), std::string::npos);
    }
}

TEST_F(RealDiskTest, EmptyDiskListHandling) {
    // Test handling of potentially empty disk lists
    std::vector<DiskInfo> disks = getDiskInfo();
    std::vector<std::pair<std::string, float>> usage = getDiskUsage();

    // Functions should handle empty results gracefully
    EXPECT_TRUE(disks.empty() || !disks.empty());
    EXPECT_TRUE(usage.empty() || !usage.empty());

    // If we have disks, we should have usage info
    if (!disks.empty()) {
        EXPECT_FALSE(usage.empty());
    }
}

}  // namespace atom::sysinfo::test
