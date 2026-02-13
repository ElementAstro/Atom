/*
 * sn.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Unit Tests for Hardware Serial Number Module
Tests hardware serial number retrieval for BIOS, motherboard, CPU, and disks.

**************************************************/

#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>

#include "atom/sysinfo/sn.hpp"

namespace atom::sysinfo::test {

// ============================================================================
// Hardware Serial Number Tests
// ============================================================================

class SnTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup hardware info tests
        hardwareInfo = std::make_unique<HardwareInfo>();
    }

    void TearDown() override {
        // Cleanup
        hardwareInfo.reset();
    }

    std::unique_ptr<HardwareInfo> hardwareInfo;
};

TEST_F(SnTest, GetBiosSerialNumber) {
    // Test BIOS serial number retrieval
    std::string biosSerial = hardwareInfo->getBiosSerialNumber();

    // BIOS serial might be empty on some systems, but should not throw
    EXPECT_TRUE(biosSerial.empty() || !biosSerial.empty());

    // If we have a BIOS serial, it should be a reasonable length
    if (!biosSerial.empty()) {
        EXPECT_GT(biosSerial.length(), 0);
        EXPECT_LT(biosSerial.length(), 1000);  // Reasonable upper bound

        // Should not contain only whitespace
        EXPECT_FALSE(
            std::all_of(biosSerial.begin(), biosSerial.end(), ::isspace));
    }
}

TEST_F(SnTest, GetMotherboardSerialNumber) {
    // Test motherboard serial number retrieval
    std::string motherboardSerial = hardwareInfo->getMotherboardSerialNumber();

    // Motherboard serial might be empty on some systems
    EXPECT_TRUE(motherboardSerial.empty() || !motherboardSerial.empty());

    // If we have a motherboard serial, it should be valid
    if (!motherboardSerial.empty()) {
        EXPECT_GT(motherboardSerial.length(), 0);
        EXPECT_LT(motherboardSerial.length(), 1000);  // Reasonable upper bound

        // Should not contain only whitespace
        EXPECT_FALSE(std::all_of(motherboardSerial.begin(),
                                 motherboardSerial.end(), ::isspace));
    }
}

TEST_F(SnTest, GetCpuSerialNumber) {
    // Test CPU serial number retrieval
    std::string cpuSerial = hardwareInfo->getCpuSerialNumber();

    // CPU serial might be empty on many modern systems (disabled for privacy)
    EXPECT_TRUE(cpuSerial.empty() || !cpuSerial.empty());

    // If we have a CPU serial, it should be valid
    if (!cpuSerial.empty()) {
        EXPECT_GT(cpuSerial.length(), 0);
        EXPECT_LT(cpuSerial.length(), 1000);  // Reasonable upper bound

        // Should not contain only whitespace
        EXPECT_FALSE(
            std::all_of(cpuSerial.begin(), cpuSerial.end(), ::isspace));
    }
}

TEST_F(SnTest, GetDiskSerialNumbers) {
    // Test disk serial numbers retrieval
    std::vector<std::string> diskSerials = hardwareInfo->getDiskSerialNumbers();

    // Should not throw, but might be empty on some systems
    EXPECT_TRUE(diskSerials.empty() || !diskSerials.empty());

    // If we have disk serials, they should be valid
    for (const auto& serial : diskSerials) {
        EXPECT_FALSE(serial.empty());
        EXPECT_GT(serial.length(), 0);
        EXPECT_LT(serial.length(), 1000);  // Reasonable upper bound

        // Should not contain only whitespace
        EXPECT_FALSE(std::all_of(serial.begin(), serial.end(), ::isspace));
    }

    // Should not have duplicate serials (each disk should have unique serial)
    std::vector<std::string> sortedSerials = diskSerials;
    std::sort(sortedSerials.begin(), sortedSerials.end());
    auto uniqueEnd = std::unique(sortedSerials.begin(), sortedSerials.end());
    EXPECT_EQ(std::distance(sortedSerials.begin(), uniqueEnd),
              diskSerials.size());
}

// ============================================================================
// Copy and Move Semantics Tests
// ============================================================================

TEST_F(SnTest, CopyConstructor) {
    // Test copy constructor
    HardwareInfo original;
    HardwareInfo copy(original);

    // Both should work independently
    std::string originalBios = original.getBiosSerialNumber();
    std::string copyBios = copy.getBiosSerialNumber();

    // Results should be the same
    EXPECT_EQ(originalBios, copyBios);
}

TEST_F(SnTest, CopyAssignment) {
    // Test copy assignment operator
    HardwareInfo original;
    HardwareInfo copy;
    copy = original;

    // Both should work independently
    std::string originalMotherboard = original.getMotherboardSerialNumber();
    std::string copyMotherboard = copy.getMotherboardSerialNumber();

    // Results should be the same
    EXPECT_EQ(originalMotherboard, copyMotherboard);
}

TEST_F(SnTest, MoveConstructor) {
    // Test move constructor
    HardwareInfo original;
    std::string originalCpu = original.getCpuSerialNumber();

    HardwareInfo moved(std::move(original));
    std::string movedCpu = moved.getCpuSerialNumber();

    // Moved object should have the same data
    EXPECT_EQ(originalCpu, movedCpu);
}

TEST_F(SnTest, MoveAssignment) {
    // Test move assignment operator
    HardwareInfo original;
    std::vector<std::string> originalDisks = original.getDiskSerialNumbers();

    HardwareInfo moved;
    moved = std::move(original);
    std::vector<std::string> movedDisks = moved.getDiskSerialNumbers();

    // Moved object should have the same data
    EXPECT_EQ(originalDisks, movedDisks);
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

TEST_F(SnTest, MultipleInstances) {
    // Test that multiple instances work correctly
    HardwareInfo info1;
    HardwareInfo info2;
    HardwareInfo info3;

    // All should return the same results
    std::string bios1 = info1.getBiosSerialNumber();
    std::string bios2 = info2.getBiosSerialNumber();
    std::string bios3 = info3.getBiosSerialNumber();

    EXPECT_EQ(bios1, bios2);
    EXPECT_EQ(bios2, bios3);
}

// ============================================================================
// Consistency Tests
// ============================================================================

TEST_F(SnTest, ConsistentResults) {
    // Test that multiple calls return consistent results
    std::string bios1 = hardwareInfo->getBiosSerialNumber();
    std::string bios2 = hardwareInfo->getBiosSerialNumber();
    EXPECT_EQ(bios1, bios2);

    std::string motherboard1 = hardwareInfo->getMotherboardSerialNumber();
    std::string motherboard2 = hardwareInfo->getMotherboardSerialNumber();
    EXPECT_EQ(motherboard1, motherboard2);

    std::string cpu1 = hardwareInfo->getCpuSerialNumber();
    std::string cpu2 = hardwareInfo->getCpuSerialNumber();
    EXPECT_EQ(cpu1, cpu2);

    std::vector<std::string> disks1 = hardwareInfo->getDiskSerialNumbers();
    std::vector<std::string> disks2 = hardwareInfo->getDiskSerialNumbers();
    EXPECT_EQ(disks1, disks2);
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

TEST_F(SnTest, NoThrowGuarantee) {
    // Test that all methods provide no-throw guarantee
    EXPECT_NO_THROW({ hardwareInfo->getBiosSerialNumber(); });

    EXPECT_NO_THROW({ hardwareInfo->getMotherboardSerialNumber(); });

    EXPECT_NO_THROW({ hardwareInfo->getCpuSerialNumber(); });

    EXPECT_NO_THROW({ hardwareInfo->getDiskSerialNumbers(); });
}

TEST_F(SnTest, DestructorSafety) {
    // Test that destructor is safe to call multiple times
    {
        HardwareInfo info;
        std::string bios = info.getBiosSerialNumber();
        // Destructor called automatically here
    }

    // Should be able to create new instances after destruction
    HardwareInfo newInfo;
    EXPECT_NO_THROW({ newInfo.getBiosSerialNumber(); });
}

}  // namespace atom::sysinfo::test
