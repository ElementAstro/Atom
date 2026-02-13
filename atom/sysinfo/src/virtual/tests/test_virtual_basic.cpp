#include <gtest/gtest.h>
#include "../virtual.hpp"
#include "../common.hpp"
#include "../detection.hpp"
#include "../hypervisor.hpp"
#include "../container.hpp"

using namespace atom::system::virtual_env;

class VirtualDetectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }

    void TearDown() override {
        // Cleanup test environment
    }
};

// Test basic virtualization detection
TEST_F(VirtualDetectionTest, BasicVirtualizationDetection) {
    VirtualizationDetector detector;

    // Test that detection doesn't crash
    EXPECT_NO_THROW({
        auto info = detector.detect();
        EXPECT_GE(info.confidence_score, 0.0);
        EXPECT_LE(info.confidence_score, 1.0);
    });
}

// Test individual detection methods
TEST_F(VirtualDetectionTest, IndividualDetectionMethods) {
    // Test CPUID detection
    EXPECT_NO_THROW({
        bool result = detection::cpuid::isHypervisorPresent();
        // Result can be true or false, just ensure it doesn't crash
    });

    // Test BIOS detection
    EXPECT_NO_THROW({
        bool result = detection::bios::checkBIOSInfo();
    });

    // Test hardware detection
    EXPECT_NO_THROW({
        bool result = detection::hardware::checkNetworkAdapters();
    });

    // Test process detection
    EXPECT_NO_THROW({
        bool result = detection::processes::checkVirtualizationProcesses();
    });
}

// Test hypervisor detection
TEST_F(VirtualDetectionTest, HypervisorDetection) {
    EXPECT_NO_THROW({
        auto vendor = hypervisor::getHypervisorVendor();
        auto type = hypervisor::detectHypervisorType();
        auto info = hypervisor::getHypervisorInfo();

        // Vendor can be empty if no hypervisor
        EXPECT_GE(vendor.length(), 0);

        // Type should be valid enum value
        EXPECT_NE(hypervisor::hypervisorTypeToString(type), "");

        // Info should have valid confidence
        EXPECT_GE(info.detection_confidence, 0.0);
        EXPECT_LE(info.detection_confidence, 1.0);
    });
}

// Test container detection
TEST_F(VirtualDetectionTest, ContainerDetection) {
    EXPECT_NO_THROW({
        bool isContainer = container::isContainer();
        auto containerType = container::detectContainerType();
        auto containerRuntime = container::detectContainerRuntime();
        auto containerInfo = container::getContainerInfo();

        // If container is detected, type should not be unknown
        if (isContainer) {
            EXPECT_NE(containerType, container::ContainerType::UNKNOWN);
        }

        // Container info should have valid confidence
        EXPECT_GE(containerInfo.detection_confidence, 0.0);
        EXPECT_LE(containerInfo.detection_confidence, 1.0);
    });
}

// Test platform-specific detection
TEST_F(VirtualDetectionTest, PlatformSpecificDetection) {
    EXPECT_NO_THROW({
        std::string platformName = platform::getPlatformName();
        EXPECT_FALSE(platformName.empty());

        bool isWindows = platform::isWindows();
        bool isLinux = platform::isLinux();
        bool isMacOS = platform::isMacOS();

        // Exactly one platform should be true
        int platformCount = (isWindows ? 1 : 0) + (isLinux ? 1 : 0) + (isMacOS ? 1 : 0);
        EXPECT_EQ(platformCount, 1);
    });
}

// Test utility functions
TEST_F(VirtualDetectionTest, UtilityFunctions) {
    // Test keyword detection
    EXPECT_TRUE(containsVMKeywords("VMware Virtual Platform"));
    EXPECT_TRUE(containsVMKeywords("VirtualBox"));
    EXPECT_TRUE(containsVMKeywords("QEMU"));
    EXPECT_FALSE(containsVMKeywords("Dell Inc."));
    EXPECT_FALSE(containsVMKeywords("HP"));

    // Test container keyword detection
    EXPECT_TRUE(containsContainerKeywords("docker"));
    EXPECT_TRUE(containsContainerKeywords("lxc"));
    EXPECT_TRUE(containsContainerKeywords("kubepods"));
    EXPECT_FALSE(containsContainerKeywords("systemd"));

    // Test cloud keyword detection
    EXPECT_TRUE(containsCloudKeywords("amazon"));
    EXPECT_TRUE(containsCloudKeywords("google"));
    EXPECT_TRUE(containsCloudKeywords("azure"));
    EXPECT_FALSE(containsCloudKeywords("local"));

    // Test string utilities
    EXPECT_EQ(toLowercase("HELLO"), "hello");
    EXPECT_EQ(toLowercase("MiXeD"), "mixed");
    EXPECT_EQ(toLowercase(""), "");
}

// Test confidence calculation
TEST_F(VirtualDetectionTest, ConfidenceCalculation) {
    std::vector<DetectionResult> results = {
        {"Method1", true, 0.8, "Test method 1", {}},
        {"Method2", false, 0.0, "Test method 2", {}},
        {"Method3", true, 0.6, "Test method 3", {}}
    };

    double confidence = calculateConfidenceScore(results);
    EXPECT_GE(confidence, 0.0);
    EXPECT_LE(confidence, 1.0);
}

// Test detection report generation
TEST_F(VirtualDetectionTest, DetectionReport) {
    VirtualizationDetector detector;

    EXPECT_NO_THROW({
        std::string report = detector.getDetectionReport();
        EXPECT_FALSE(report.empty());
        EXPECT_NE(report.find("Virtualization Detection Report"), std::string::npos);
    });
}

// Test method enabling/disabling
TEST_F(VirtualDetectionTest, MethodControl) {
    VirtualizationDetector detector;

    auto methods = detector.getAvailableDetectionMethods();
    EXPECT_FALSE(methods.empty());

    // Test enabling/disabling methods
    for (const auto& method : methods) {
        EXPECT_NO_THROW({
            detector.setDetectionMethod(method, false);
            detector.setDetectionMethod(method, true);
        });
    }
}

// Test global convenience functions
TEST_F(VirtualDetectionTest, GlobalFunctions) {
    EXPECT_NO_THROW({
        bool isVirtual = isVirtualEnvironment();
        bool isContainer = isContainerEnvironment();
        std::string virtType = getVirtualizationType();
        std::string containerType = getContainerType();
        auto info = getVirtualizationInfo();

        // Functions should not crash and return valid data
        EXPECT_GE(virtType.length(), 0);
        EXPECT_GE(containerType.length(), 0);
        EXPECT_GE(info.confidence_score, 0.0);
        EXPECT_LE(info.confidence_score, 1.0);
    });
}

// Test error handling
TEST_F(VirtualDetectionTest, ErrorHandling) {
    VirtualizationDetector detector;

    // Test with invalid method name
    EXPECT_NO_THROW({
        detector.setDetectionMethod("InvalidMethod", true);
    });

    // Test multiple detections
    EXPECT_NO_THROW({
        for (int i = 0; i < 5; ++i) {
            auto info = detector.detect();
            EXPECT_GE(info.confidence_score, 0.0);
            EXPECT_LE(info.confidence_score, 1.0);
        }
    });
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
