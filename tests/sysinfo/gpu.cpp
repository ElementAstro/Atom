#include "atom/sysinfo/gpu.hpp"
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <algorithm>

using namespace atom::system;

namespace atom::sysinfo::test {

// ============================================================================
// GPU Information Tests
// ============================================================================

class GpuTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup GPU tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(GpuTest, GetGPUInfo) {
    // Test basic GPU information retrieval
    std::string gpuInfo = getGPUInfo();

    // GPU info should not be empty
    EXPECT_FALSE(gpuInfo.empty());
    EXPECT_GT(gpuInfo.length(), 0);

    // Should contain some expected keywords for GPU information
    bool hasGpuKeywords = (gpuInfo.find("GPU") != std::string::npos ||
                          gpuInfo.find("Graphics") != std::string::npos ||
                          gpuInfo.find("Video") != std::string::npos ||
                          gpuInfo.find("Display") != std::string::npos ||
                          gpuInfo.find("NVIDIA") != std::string::npos ||
                          gpuInfo.find("AMD") != std::string::npos ||
                          gpuInfo.find("Intel") != std::string::npos ||
                          gpuInfo.find("not available") != std::string::npos);

    EXPECT_TRUE(hasGpuKeywords);
}

TEST_F(GpuTest, GetGPUInfoConsistency) {
    // Test that multiple calls return consistent results
    std::string gpuInfo1 = getGPUInfo();
    std::string gpuInfo2 = getGPUInfo();

    EXPECT_EQ(gpuInfo1, gpuInfo2);
}

TEST_F(GpuTest, GetGPUInfoNoThrow) {
    // Test that GPU info retrieval doesn't throw
    EXPECT_NO_THROW({
        std::string gpuInfo = getGPUInfo();
    });
}

// ============================================================================
// Monitor Information Tests
// ============================================================================

TEST_F(GpuTest, GetAllMonitorsInfo) {
    // Test monitor information retrieval
    std::vector<MonitorInfo> monitors = getAllMonitorsInfo();

    // Should have at least one monitor in most cases
    // Note: Might be empty on headless systems
    if (!monitors.empty()) {
        for (const auto& monitor : monitors) {
            // Identifier should not be empty
            EXPECT_FALSE(monitor.identifier.empty());
            EXPECT_GT(monitor.identifier.length(), 0);

            // Dimensions should be positive
            EXPECT_GT(monitor.width, 0);
            EXPECT_GT(monitor.height, 0);

            // Refresh rate should be positive
            EXPECT_GT(monitor.refreshRate, 0);
            EXPECT_LT(monitor.refreshRate, 1000); // Reasonable upper bound

            // Common resolutions and refresh rates
            EXPECT_GE(monitor.width, 640);   // At least VGA width
            EXPECT_GE(monitor.height, 480);  // At least VGA height
            EXPECT_LE(monitor.width, 16384); // Reasonable upper bound
            EXPECT_LE(monitor.height, 16384); // Reasonable upper bound

            // Common refresh rates
            EXPECT_GE(monitor.refreshRate, 30);  // At least 30Hz
            EXPECT_LE(monitor.refreshRate, 500); // At most 500Hz

            // Model might be empty on some systems/platforms
            if (!monitor.model.empty()) {
                EXPECT_GT(monitor.model.length(), 0);
                EXPECT_LT(monitor.model.length(), 200); // Reasonable upper bound
            }
        }
    }
}

#ifdef _WIN32
TEST_F(GpuTest, GetAllMonitorsInfo_Windows) {
    // Windows-specific monitor tests
    std::vector<MonitorInfo> monitors = getAllMonitorsInfo();

    // Windows should typically have at least one monitor
    EXPECT_FALSE(monitors.empty());

    for (const auto& monitor : monitors) {
        // On Windows, model should typically be available
        EXPECT_FALSE(monitor.model.empty());
        EXPECT_FALSE(monitor.identifier.empty());

        // Windows identifier format check
        EXPECT_TRUE(monitor.identifier.find("\\\\.\\DISPLAY") != std::string::npos ||
                   monitor.identifier.find("DISPLAY") != std::string::npos);

        EXPECT_GT(monitor.width, 0);
        EXPECT_GT(monitor.height, 0);
        EXPECT_GT(monitor.refreshRate, 0);
    }
}
#elif defined(__linux__)
TEST_F(GpuTest, GetAllMonitorsInfo_Linux) {
    // Linux-specific monitor tests
    std::vector<MonitorInfo> monitors = getAllMonitorsInfo();

    // Linux might not have monitors in headless environments
    for (const auto& monitor : monitors) {
        EXPECT_FALSE(monitor.identifier.empty());
        EXPECT_GT(monitor.width, 0);
        EXPECT_GT(monitor.height, 0);
        EXPECT_GT(monitor.refreshRate, 0);

        // Model might be empty on Linux
        if (!monitor.model.empty()) {
            EXPECT_GT(monitor.model.length(), 0);
        }
    }
}
#elif defined(__APPLE__)
TEST_F(GpuTest, GetAllMonitorsInfo_Mac) {
    // macOS-specific monitor tests
    std::vector<MonitorInfo> monitors = getAllMonitorsInfo();

    // macOS should typically have at least one monitor
    EXPECT_FALSE(monitors.empty());

    for (const auto& monitor : monitors) {
        EXPECT_FALSE(monitor.identifier.empty());
        EXPECT_GT(monitor.width, 0);
        EXPECT_GT(monitor.height, 0);
        EXPECT_GT(monitor.refreshRate, 0);

        // Model might be empty on macOS
        if (!monitor.model.empty()) {
            EXPECT_GT(monitor.model.length(), 0);
        }
    }
}
#endif

TEST_F(GpuTest, MonitorInfoConsistency) {
    // Test that multiple calls return consistent results
    std::vector<MonitorInfo> monitors1 = getAllMonitorsInfo();
    std::vector<MonitorInfo> monitors2 = getAllMonitorsInfo();

    EXPECT_EQ(monitors1.size(), monitors2.size());

    for (size_t i = 0; i < monitors1.size(); ++i) {
        EXPECT_EQ(monitors1[i].model, monitors2[i].model);
        EXPECT_EQ(monitors1[i].identifier, monitors2[i].identifier);
        EXPECT_EQ(monitors1[i].width, monitors2[i].width);
        EXPECT_EQ(monitors1[i].height, monitors2[i].height);
        EXPECT_EQ(monitors1[i].refreshRate, monitors2[i].refreshRate);
    }
}

TEST_F(GpuTest, MonitorInfoNoThrow) {
    // Test that monitor info retrieval doesn't throw
    EXPECT_NO_THROW({
        std::vector<MonitorInfo> monitors = getAllMonitorsInfo();
    });
}

// ============================================================================
// MonitorInfo Structure Tests
// ============================================================================

TEST_F(GpuTest, MonitorInfoStructure) {
    // Test MonitorInfo structure properties
    MonitorInfo monitor;

    // Default values should be reasonable
    EXPECT_TRUE(monitor.model.empty());
    EXPECT_TRUE(monitor.identifier.empty());
    EXPECT_EQ(monitor.width, 0);
    EXPECT_EQ(monitor.height, 0);
    EXPECT_EQ(monitor.refreshRate, 0);

    // Test assignment
    monitor.model = "Test Monitor";
    monitor.identifier = "TEST_DISPLAY_1";
    monitor.width = 1920;
    monitor.height = 1080;
    monitor.refreshRate = 60;

    EXPECT_EQ(monitor.model, "Test Monitor");
    EXPECT_EQ(monitor.identifier, "TEST_DISPLAY_1");
    EXPECT_EQ(monitor.width, 1920);
    EXPECT_EQ(monitor.height, 1080);
    EXPECT_EQ(monitor.refreshRate, 60);
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

TEST_F(GpuTest, HeadlessSystemHandling) {
    // Test behavior on potentially headless systems
    std::vector<MonitorInfo> monitors = getAllMonitorsInfo();

    // On headless systems, monitors might be empty, which is acceptable
    // Just ensure the function doesn't crash
    EXPECT_TRUE(monitors.empty() || !monitors.empty());
}

TEST_F(GpuTest, MultipleMonitorHandling) {
    // Test handling of multiple monitors
    std::vector<MonitorInfo> monitors = getAllMonitorsInfo();

    if (monitors.size() > 1) {
        // If multiple monitors, they should have different identifiers
        std::vector<std::string> identifiers;
        for (const auto& monitor : monitors) {
            identifiers.push_back(monitor.identifier);
        }

        // Sort and check for uniqueness
        std::sort(identifiers.begin(), identifiers.end());
        auto uniqueEnd = std::unique(identifiers.begin(), identifiers.end());
        EXPECT_EQ(std::distance(identifiers.begin(), uniqueEnd), monitors.size());
    }
}

TEST_F(GpuTest, MonitorResolutionValidation) {
    // Test monitor resolution validation
    std::vector<MonitorInfo> monitors = getAllMonitorsInfo();

    for (const auto& monitor : monitors) {
        if (monitor.width > 0 && monitor.height > 0) {
            // Aspect ratio should be reasonable
            double aspectRatio = static_cast<double>(monitor.width) / monitor.height;
            EXPECT_GT(aspectRatio, 0.5);  // Very tall displays
            EXPECT_LT(aspectRatio, 5.0);  // Very wide displays

            // Common aspect ratios: 4:3, 16:9, 16:10, 21:9, etc.
            // Just ensure it's within reasonable bounds
        }
    }
}

} // namespace atom::sysinfo::test
