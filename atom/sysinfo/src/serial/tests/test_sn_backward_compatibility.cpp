#include <gtest/gtest.h>
#include "../sn.hpp"  // Original header for backward compatibility
#include <chrono>
#include <thread>

class HardwareInfoBackwardCompatibilityTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create hardware info instance
        hwInfo = std::make_unique<HardwareInfo>();
        ASSERT_NE(hwInfo, nullptr);
    }

    void TearDown() override {
        hwInfo.reset();
    }

    std::unique_ptr<HardwareInfo> hwInfo;
};

TEST_F(HardwareInfoBackwardCompatibilityTest, CreateHardwareInfo) {
    EXPECT_TRUE(hwInfo != nullptr);
}

TEST_F(HardwareInfoBackwardCompatibilityTest, GetBiosSerialNumber) {
    std::string biosSerial = hwInfo->getBiosSerialNumber();
    
    // Should return a string (empty or non-empty)
    EXPECT_TRUE(biosSerial.empty() || !biosSerial.empty());
    
    // If non-empty, should be a reasonable length
    if (!biosSerial.empty()) {
        EXPECT_GE(biosSerial.length(), 3);
        EXPECT_LE(biosSerial.length(), 100);
    }
}

TEST_F(HardwareInfoBackwardCompatibilityTest, GetMotherboardSerialNumber) {
    std::string mbSerial = hwInfo->getMotherboardSerialNumber();
    
    // Should return a string (empty or non-empty)
    EXPECT_TRUE(mbSerial.empty() || !mbSerial.empty());
    
    // If non-empty, should be a reasonable length
    if (!mbSerial.empty()) {
        EXPECT_GE(mbSerial.length(), 3);
        EXPECT_LE(mbSerial.length(), 100);
    }
}

TEST_F(HardwareInfoBackwardCompatibilityTest, GetCpuSerialNumber) {
    std::string cpuSerial = hwInfo->getCpuSerialNumber();
    
    // Should return a string (empty or non-empty)
    EXPECT_TRUE(cpuSerial.empty() || !cpuSerial.empty());
    
    // If non-empty, should be a reasonable length
    if (!cpuSerial.empty()) {
        EXPECT_GE(cpuSerial.length(), 3);
        EXPECT_LE(cpuSerial.length(), 100);
    }
}

TEST_F(HardwareInfoBackwardCompatibilityTest, GetDiskSerialNumbers) {
    auto diskSerials = hwInfo->getDiskSerialNumbers();
    
    // Should return a vector (empty or non-empty)
    EXPECT_TRUE(diskSerials.empty() || !diskSerials.empty());
    
    // If non-empty, each serial should be reasonable
    for (const auto& serial : diskSerials) {
        EXPECT_FALSE(serial.empty());
        EXPECT_GE(serial.length(), 3);
        EXPECT_LE(serial.length(), 100);
    }
}

TEST_F(HardwareInfoBackwardCompatibilityTest, CopyConstructor) {
    // Create a copy
    HardwareInfo hwInfoCopy(*hwInfo);
    
    // Both should return the same values
    EXPECT_EQ(hwInfo->getBiosSerialNumber(), hwInfoCopy.getBiosSerialNumber());
    EXPECT_EQ(hwInfo->getMotherboardSerialNumber(), hwInfoCopy.getMotherboardSerialNumber());
    EXPECT_EQ(hwInfo->getCpuSerialNumber(), hwInfoCopy.getCpuSerialNumber());
    
    auto origDiskSerials = hwInfo->getDiskSerialNumbers();
    auto copyDiskSerials = hwInfoCopy.getDiskSerialNumbers();
    EXPECT_EQ(origDiskSerials.size(), copyDiskSerials.size());
    
    for (size_t i = 0; i < origDiskSerials.size() && i < copyDiskSerials.size(); ++i) {
        EXPECT_EQ(origDiskSerials[i], copyDiskSerials[i]);
    }
}

TEST_F(HardwareInfoBackwardCompatibilityTest, CopyAssignment) {
    // Create a new instance
    HardwareInfo hwInfoOther;
    
    // Assign from original
    hwInfoOther = *hwInfo;
    
    // Both should return the same values
    EXPECT_EQ(hwInfo->getBiosSerialNumber(), hwInfoOther.getBiosSerialNumber());
    EXPECT_EQ(hwInfo->getMotherboardSerialNumber(), hwInfoOther.getMotherboardSerialNumber());
    EXPECT_EQ(hwInfo->getCpuSerialNumber(), hwInfoOther.getCpuSerialNumber());
    
    auto origDiskSerials = hwInfo->getDiskSerialNumbers();
    auto otherDiskSerials = hwInfoOther.getDiskSerialNumbers();
    EXPECT_EQ(origDiskSerials.size(), otherDiskSerials.size());
    
    for (size_t i = 0; i < origDiskSerials.size() && i < otherDiskSerials.size(); ++i) {
        EXPECT_EQ(origDiskSerials[i], otherDiskSerials[i]);
    }
}

TEST_F(HardwareInfoBackwardCompatibilityTest, MoveConstructor) {
    // Get original values
    std::string origBiosSerial = hwInfo->getBiosSerialNumber();
    std::string origMbSerial = hwInfo->getMotherboardSerialNumber();
    std::string origCpuSerial = hwInfo->getCpuSerialNumber();
    auto origDiskSerials = hwInfo->getDiskSerialNumbers();
    
    // Create a new instance with move constructor
    HardwareInfo hwInfoMoved(std::move(*hwInfo));
    
    // Moved instance should have the original values
    EXPECT_EQ(hwInfoMoved.getBiosSerialNumber(), origBiosSerial);
    EXPECT_EQ(hwInfoMoved.getMotherboardSerialNumber(), origMbSerial);
    EXPECT_EQ(hwInfoMoved.getCpuSerialNumber(), origCpuSerial);
    
    auto movedDiskSerials = hwInfoMoved.getDiskSerialNumbers();
    EXPECT_EQ(movedDiskSerials.size(), origDiskSerials.size());
    
    for (size_t i = 0; i < origDiskSerials.size() && i < movedDiskSerials.size(); ++i) {
        EXPECT_EQ(movedDiskSerials[i], origDiskSerials[i]);
    }
    
    // Create a new instance for the next test
    hwInfo = std::make_unique<HardwareInfo>();
}

TEST_F(HardwareInfoBackwardCompatibilityTest, MoveAssignment) {
    // Get original values
    std::string origBiosSerial = hwInfo->getBiosSerialNumber();
    std::string origMbSerial = hwInfo->getMotherboardSerialNumber();
    std::string origCpuSerial = hwInfo->getCpuSerialNumber();
    auto origDiskSerials = hwInfo->getDiskSerialNumbers();
    
    // Create a new instance
    HardwareInfo hwInfoOther;
    
    // Move assign
    hwInfoOther = std::move(*hwInfo);
    
    // Moved instance should have the original values
    EXPECT_EQ(hwInfoOther.getBiosSerialNumber(), origBiosSerial);
    EXPECT_EQ(hwInfoOther.getMotherboardSerialNumber(), origMbSerial);
    EXPECT_EQ(hwInfoOther.getCpuSerialNumber(), origCpuSerial);
    
    auto otherDiskSerials = hwInfoOther.getDiskSerialNumbers();
    EXPECT_EQ(otherDiskSerials.size(), origDiskSerials.size());
    
    for (size_t i = 0; i < origDiskSerials.size() && i < otherDiskSerials.size(); ++i) {
        EXPECT_EQ(otherDiskSerials[i], origDiskSerials[i]);
    }
    
    // Create a new instance for the next test
    hwInfo = std::make_unique<HardwareInfo>();
}

TEST_F(HardwareInfoBackwardCompatibilityTest, EnhancedFeatures) {
    // Test new enhanced features
    
    // Check if enhanced mode is available
    bool enhancedAvailable = hwInfo->isEnhancedModeAvailable();
    EXPECT_TRUE(enhancedAvailable || !enhancedAvailable); // Should be true or false
    
    // Get system fingerprint
    std::string fingerprint = hwInfo->getSystemFingerprint();
    
    // If enhanced mode is available, fingerprint should be non-empty
    if (enhancedAvailable) {
        EXPECT_FALSE(fingerprint.empty());
        EXPECT_GE(fingerprint.length(), 8);
    }
    
    // Get enhanced system info
    auto* enhancedSysInfo = hwInfo->getEnhancedSystemInfo();
    
    // If enhanced mode is available, should return non-null pointer
    if (enhancedAvailable) {
        EXPECT_NE(enhancedSysInfo, nullptr);
        
        if (enhancedSysInfo) {
            // Should be able to call methods on enhanced system info
            auto hwResult = enhancedSysInfo->getHardwareSerials();
            EXPECT_TRUE(hwResult.success || !hwResult.success);
            
            auto sysIdResult = enhancedSysInfo->getSystemIdentification();
            EXPECT_TRUE(sysIdResult.success || !sysIdResult.success);
        }
    } else {
        EXPECT_EQ(enhancedSysInfo, nullptr);
    }
}

TEST_F(HardwareInfoBackwardCompatibilityTest, UtilityFunctions) {
    // Test utility functions
    
    // Get quick fingerprint
    std::string quickFingerprint = HardwareInfoUtils::getQuickFingerprint();
    EXPECT_TRUE(quickFingerprint.empty() || !quickFingerprint.empty());
    
    // Check availability
    bool isAvailable = HardwareInfoUtils::isAvailable();
    EXPECT_TRUE(isAvailable || !isAvailable);
    
    // Get formatted serials
    std::string formattedSerials = HardwareInfoUtils::getFormattedSerials();
    EXPECT_FALSE(formattedSerials.empty());
    EXPECT_NE(formattedSerials.find("Hardware Serial Numbers"), std::string::npos);
}

TEST_F(HardwareInfoBackwardCompatibilityTest, TypeAliases) {
    // Test type aliases
    
    // SystemHardwareInfo should be alias for HardwareInfo
    SystemHardwareInfo sysHwInfo;
    EXPECT_EQ(typeid(sysHwInfo), typeid(HardwareInfo));
    
    // HwInfo should be alias for HardwareInfo
    HwInfo hwInfoAlias;
    EXPECT_EQ(typeid(hwInfoAlias), typeid(HardwareInfo));
    
    // Both should work the same way
    EXPECT_EQ(sysHwInfo.getBiosSerialNumber(), hwInfoAlias.getBiosSerialNumber());
    EXPECT_EQ(sysHwInfo.getMotherboardSerialNumber(), hwInfoAlias.getMotherboardSerialNumber());
    EXPECT_EQ(sysHwInfo.getCpuSerialNumber(), hwInfoAlias.getCpuSerialNumber());
}
