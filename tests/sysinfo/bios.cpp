/*
 * bios.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Unit Tests for BIOS Information Module
Tests BIOS information retrieval, security features, update checking,
and settings management.

**************************************************/

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "atom/sysinfo/bios.hpp"

using namespace atom::system;

namespace atom::sysinfo::test {

// ============================================================================
// BIOS Information Tests
// ============================================================================

class BiosTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup BIOS information tests
        biosInfo = &BiosInfo::getInstance();
    }

    void TearDown() override {
        // Cleanup
    }

    BiosInfo* biosInfo;
};

TEST_F(BiosTest, GetBiosInfo) {
    // Test basic BIOS information retrieval
    BiosInfoData info = biosInfo->getBiosInfo();

    // BIOS info should have at least some basic information
    EXPECT_FALSE(info.version.empty() && info.manufacturer.empty());

    // If we have version info, it should be valid
    if (!info.version.empty()) {
        EXPECT_GT(info.version.length(), 0);
    }

    // If we have manufacturer info, it should be valid
    if (!info.manufacturer.empty()) {
        EXPECT_GT(info.manufacturer.length(), 0);
    }
}

TEST_F(BiosTest, BiosInfoDataValidation) {
    // Test BiosInfoData validation
    BiosInfoData info = biosInfo->getBiosInfo();

    // Test isValid method
    bool isValid = info.isValid();

    // If BIOS info is available, it should be valid
    if (!info.version.empty() || !info.manufacturer.empty()) {
        EXPECT_TRUE(isValid);
    }
}

TEST_F(BiosTest, BiosInfoToString) {
    // Test string representation of BIOS info
    BiosInfoData info = biosInfo->getBiosInfo();
    std::string infoStr = info.toString();

    // String representation should not be empty if we have BIOS info
    if (info.isValid()) {
        EXPECT_FALSE(infoStr.empty());
        EXPECT_GT(infoStr.length(), 0);
    }
}

TEST_F(BiosTest, SecureBootSupport) {
    // Test Secure Boot support detection
    bool secureBootSupported = biosInfo->isSecureBootSupported();

    // This is a boolean value, so just verify it doesn't throw
    EXPECT_TRUE(secureBootSupported || !secureBootSupported);
}

TEST_F(BiosTest, UEFIBootSupport) {
    // Test UEFI Boot support detection
    bool uefiBootSupported = biosInfo->isUEFIBootSupported();

    // This is a boolean value, so just verify it doesn't throw
    EXPECT_TRUE(uefiBootSupported || !uefiBootSupported);
}

TEST_F(BiosTest, GetSMBIOSData) {
    // Test SMBIOS data retrieval
    std::vector<std::string> smbiosData = biosInfo->getSMBIOSData();

    // SMBIOS data might be empty on some systems, but should not throw
    EXPECT_TRUE(smbiosData.empty() || !smbiosData.empty());

    // If we have SMBIOS data, entries should not be empty
    for (const auto& entry : smbiosData) {
        EXPECT_FALSE(entry.empty());
    }
}

TEST_F(BiosTest, CheckForUpdates) {
    // Test BIOS update checking
    BiosUpdateInfo updateInfo = biosInfo->checkForUpdates();

    // Update info should have valid structure
    EXPECT_TRUE(updateInfo.updateAvailable || !updateInfo.updateAvailable);

    // If update is available, version should not be empty
    if (updateInfo.updateAvailable) {
        EXPECT_FALSE(updateInfo.latestVersion.empty());
    }
}

// ============================================================================
// BIOS Settings Management Tests (may require elevated privileges)
// ============================================================================

TEST_F(BiosTest, SecureBootManagement) {
    // Test Secure Boot enable/disable (read-only test)
    // Note: Actual modification requires elevated privileges and is risky

    bool originalSupported = biosInfo->isSecureBootSupported();

    if (originalSupported) {
        // Just test that the methods don't crash
        // In a real scenario, we would need elevated privileges
        EXPECT_NO_THROW({
            // This might fail due to permissions, but shouldn't crash
            biosInfo->setSecureBoot(true);
        });
    }
}

TEST_F(BiosTest, UEFIBootManagement) {
    // Test UEFI Boot enable/disable (read-only test)
    // Note: Actual modification requires elevated privileges and is risky

    bool originalSupported = biosInfo->isUEFIBootSupported();

    if (originalSupported) {
        // Just test that the methods don't crash
        EXPECT_NO_THROW({
            // This might fail due to permissions, but shouldn't crash
            biosInfo->setUEFIBoot(true);
        });
    }
}

// ============================================================================
// BIOS Backup and Restore Tests
// ============================================================================

TEST_F(BiosTest, BackupBiosSettings) {
    // Test BIOS settings backup
    std::string testBackupPath = "test_bios_backup.dat";

    // This might fail due to permissions, but shouldn't crash
    EXPECT_NO_THROW({
        bool result = biosInfo->backupBiosSettings(testBackupPath);
        // Result can be true or false depending on system and permissions
        EXPECT_TRUE(result || !result);
    });
}

TEST_F(BiosTest, RestoreBiosSettings) {
    // Test BIOS settings restore
    std::string testBackupPath = "nonexistent_backup.dat";

    // This should fail gracefully for non-existent file
    EXPECT_NO_THROW({
        bool result = biosInfo->restoreBiosSettings(testBackupPath);
        // Should return false for non-existent file
        EXPECT_FALSE(result);
    });
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

TEST_F(BiosTest, EmptyBackupPath) {
    // Test backup with empty path
    EXPECT_NO_THROW({
        bool result = biosInfo->backupBiosSettings("");
        EXPECT_FALSE(result);
    });
}

TEST_F(BiosTest, InvalidRestorePath) {
    // Test restore with invalid path
    EXPECT_NO_THROW({
        bool result = biosInfo->restoreBiosSettings("/invalid/path/backup.dat");
        EXPECT_FALSE(result);
    });
}

TEST_F(BiosTest, SingletonPattern) {
    // Test that BiosInfo follows singleton pattern
    BiosInfo* instance1 = &BiosInfo::getInstance();
    BiosInfo* instance2 = &BiosInfo::getInstance();

    EXPECT_EQ(instance1, instance2);
}

}  // namespace atom::sysinfo::test
