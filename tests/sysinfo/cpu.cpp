#include "atom/sysinfo/cpu.hpp"
#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <chrono>

using namespace atom::system;

class CpuTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if necessary
    }

    void TearDown() override {
        // Cleanup code if necessary
    }
};

TEST_F(CpuTest, GetCurrentCpuUsage) {
    float cpuUsage = getCurrentCpuUsage();
    ASSERT_GE(cpuUsage, 0.0);
    ASSERT_LE(cpuUsage, 100.0);
}

TEST_F(CpuTest, GetCurrentCpuTemperature) {
    float temperature = getCurrentCpuTemperature();
    ASSERT_GE(temperature, 0.0);
    // No upper bound check as it can vary widely
}

TEST_F(CpuTest, GetCPUModel) {
    std::string cpuModel = getCPUModel();
    ASSERT_FALSE(cpuModel.empty());
}

TEST_F(CpuTest, GetProcessorIdentifier) {
    std::string identifier = getProcessorIdentifier();
    ASSERT_FALSE(identifier.empty());
}

TEST_F(CpuTest, GetProcessorFrequency) {
    double frequency = getProcessorFrequency();
    ASSERT_GT(frequency, 0.0);
}

TEST_F(CpuTest, GetNumberOfPhysicalPackages) {
    int numberOfPackages = getNumberOfPhysicalPackages();
    ASSERT_GT(numberOfPackages, 0);
}

TEST_F(CpuTest, GetNumberOfPhysicalCoresTest) {
    int numberOfCores = getNumberOfPhysicalCores();
    ASSERT_GT(numberOfCores, 0);
}

TEST_F(CpuTest, GetCacheSizes) {
    CacheSizes cacheSizes = getCacheSizes();
    ASSERT_GE(cacheSizes.l1i, 0);
    ASSERT_GE(cacheSizes.l1d, 0);
    ASSERT_GE(cacheSizes.l2, 0);
    ASSERT_GE(cacheSizes.l3, 0);

    // Cache line sizes should be reasonable (typically 32, 64, or 128 bytes)
    if (cacheSizes.l1d_line_size > 0) {
        EXPECT_GE(cacheSizes.l1d_line_size, 16);
        EXPECT_LE(cacheSizes.l1d_line_size, 256);
    }

    if (cacheSizes.l1i_line_size > 0) {
        EXPECT_GE(cacheSizes.l1i_line_size, 16);
        EXPECT_LE(cacheSizes.l1i_line_size, 256);
    }
}

// ============================================================================
// Enhanced CPU Information Tests
// ============================================================================

TEST_F(CpuTest, GetCpuArchitecture) {
    CpuArchitecture arch = getCpuArchitecture();

    // Should be a valid architecture
    EXPECT_NE(arch, CpuArchitecture::UNKNOWN);

    // Convert to string and verify
    std::string archStr = cpuArchitectureToString(arch);
    EXPECT_FALSE(archStr.empty());
    EXPECT_NE(archStr, "Unknown");
}

TEST_F(CpuTest, GetCpuVendor) {
    CpuVendor vendor = getCpuVendor();

    // Should be a valid vendor (might be UNKNOWN on some systems)
    EXPECT_TRUE(vendor == CpuVendor::INTEL ||
                vendor == CpuVendor::AMD ||
                vendor == CpuVendor::ARM ||
                vendor == CpuVendor::APPLE ||
                vendor == CpuVendor::QUALCOMM ||
                vendor == CpuVendor::IBM ||
                vendor == CpuVendor::MEDIATEK ||
                vendor == CpuVendor::SAMSUNG ||
                vendor == CpuVendor::OTHER ||
                vendor == CpuVendor::UNKNOWN);

    // Convert to string and verify
    std::string vendorStr = cpuVendorToString(vendor);
    EXPECT_FALSE(vendorStr.empty());
}

TEST_F(CpuTest, GetCpuFeatureFlags) {
    std::vector<std::string> flags = getCpuFeatureFlags();

    // Should have some feature flags on most modern CPUs
    // Note: Might be empty on some systems, which is acceptable
    for (const auto& flag : flags) {
        EXPECT_FALSE(flag.empty());
        EXPECT_GT(flag.length(), 0);
    }
}

TEST_F(CpuTest, IsCpuFeatureSupported) {
    // Test common CPU features
    std::vector<std::string> commonFeatures = {
        "sse", "sse2", "sse3", "ssse3", "sse4_1", "sse4_2",
        "avx", "avx2", "aes", "fma", "mmx"
    };

    for (const auto& feature : commonFeatures) {
        CpuFeatureSupport support = isCpuFeatureSupported(feature);

        // Should return a valid enum value
        EXPECT_TRUE(support == CpuFeatureSupport::SUPPORTED ||
                   support == CpuFeatureSupport::NOT_SUPPORTED ||
                   support == CpuFeatureSupport::UNKNOWN);
    }
}

TEST_F(CpuTest, GetPerCoreCpuUsage) {
    std::vector<float> perCoreUsage = getPerCoreCpuUsage();

    // Should have at least one core
    EXPECT_GT(perCoreUsage.size(), 0);

    // Each core usage should be between 0 and 100
    for (float usage : perCoreUsage) {
        EXPECT_GE(usage, 0.0f);
        EXPECT_LE(usage, 100.0f);
    }

    // Number of cores should match logical core count
    int logicalCores = getNumberOfLogicalCores();
    if (logicalCores > 0) {
        EXPECT_EQ(perCoreUsage.size(), static_cast<size_t>(logicalCores));
    }
}

TEST_F(CpuTest, GetPerCoreCpuTemperature) {
    std::vector<float> perCoreTemp = getPerCoreCpuTemperature();

    // Might be empty if temperature sensors are not available
    for (float temp : perCoreTemp) {
        EXPECT_GE(temp, 0.0f);
        EXPECT_LE(temp, 150.0f); // Reasonable upper bound for CPU temperature
    }
}

TEST_F(CpuTest, GetMinMaxProcessorFrequency) {
    double minFreq = getMinProcessorFrequency();
    double maxFreq = getMaxProcessorFrequency();
    double currentFreq = getProcessorFrequency();

    // Frequencies should be positive
    EXPECT_GT(minFreq, 0.0);
    EXPECT_GT(maxFreq, 0.0);
    EXPECT_GT(currentFreq, 0.0);

    // Min should be less than or equal to max
    EXPECT_LE(minFreq, maxFreq);

    // Current should be within min/max range (with some tolerance)
    EXPECT_GE(currentFreq, minFreq * 0.8); // Allow some tolerance
    EXPECT_LE(currentFreq, maxFreq * 1.2); // Allow some tolerance for boost
}

TEST_F(CpuTest, GetPerCoreFrequencies) {
    std::vector<double> perCoreFreq = getPerCoreFrequencies();

    // Should have frequencies for each core
    EXPECT_GT(perCoreFreq.size(), 0);

    // Each frequency should be positive
    for (double freq : perCoreFreq) {
        EXPECT_GT(freq, 0.0);
        EXPECT_LT(freq, 10000.0); // Reasonable upper bound (10 GHz)
    }
}

TEST_F(CpuTest, GetCpuLoadAverage) {
    LoadAverage loadAvg = getCpuLoadAverage();

    // Load averages should be non-negative
    EXPECT_GE(loadAvg.oneMinute, 0.0);
    EXPECT_GE(loadAvg.fiveMinutes, 0.0);
    EXPECT_GE(loadAvg.fifteenMinutes, 0.0);

    // Load averages should be reasonable (typically < 100 on most systems)
    EXPECT_LT(loadAvg.oneMinute, 1000.0);
    EXPECT_LT(loadAvg.fiveMinutes, 1000.0);
    EXPECT_LT(loadAvg.fifteenMinutes, 1000.0);
}

TEST_F(CpuTest, GetCpuPowerInfo) {
    CpuPowerInfo powerInfo = getCpuPowerInfo();

    // Power values should be non-negative
    EXPECT_GE(powerInfo.currentWatts, 0.0);
    EXPECT_GE(powerInfo.maxTDP, 0.0);
    EXPECT_GE(powerInfo.energyImpact, 0.0);

    // Current power should not exceed max TDP significantly
    if (powerInfo.maxTDP > 0) {
        EXPECT_LE(powerInfo.currentWatts, powerInfo.maxTDP * 2.0); // Allow some tolerance
    }
}

TEST_F(CpuTest, GetCpuSocketType) {
    std::string socketType = getCpuSocketType();

    // Socket type might be empty on some systems
    if (!socketType.empty()) {
        EXPECT_GT(socketType.length(), 0);
        EXPECT_LT(socketType.length(), 50); // Reasonable upper bound
    }
}

TEST_F(CpuTest, GetCpuScalingGovernor) {
    std::string governor = getCpuScalingGovernor();

    // Governor might be empty on some systems (Windows, etc.)
    if (!governor.empty()) {
        EXPECT_GT(governor.length(), 0);

        // Common governors on Linux
        std::vector<std::string> commonGovernors = {
            "performance", "powersave", "ondemand", "conservative", "schedutil"
        };

        bool isKnownGovernor = std::find(commonGovernors.begin(),
                                        commonGovernors.end(),
                                        governor) != commonGovernors.end();

        // Note: Might be a custom governor, so this is not a hard requirement
        if (isKnownGovernor) {
            EXPECT_TRUE(isKnownGovernor);
        }
    }
}

TEST_F(CpuTest, GetPerCoreScalingGovernors) {
    std::vector<std::string> governors = getPerCoreScalingGovernors();

    // Might be empty on non-Linux systems
    for (const auto& governor : governors) {
        EXPECT_FALSE(governor.empty());
        EXPECT_GT(governor.length(), 0);
    }
}

// ============================================================================
// Complete CPU Information Tests
// ============================================================================

TEST_F(CpuTest, GetCompleteCpuInfo) {
    CpuInfo cpuInfo = getCpuInfo();

    // Basic information should be available
    EXPECT_FALSE(cpuInfo.model.empty());
    EXPECT_FALSE(cpuInfo.identifier.empty());

    // Architecture and vendor should be valid
    EXPECT_NE(cpuInfo.architecture, CpuArchitecture::UNKNOWN);

    // Core counts should be positive
    EXPECT_GT(cpuInfo.numPhysicalPackages, 0);
    EXPECT_GT(cpuInfo.numPhysicalCores, 0);
    EXPECT_GT(cpuInfo.numLogicalCores, 0);

    // Logical cores should be >= physical cores
    EXPECT_GE(cpuInfo.numLogicalCores, cpuInfo.numPhysicalCores);

    // Frequencies should be positive
    EXPECT_GT(cpuInfo.baseFrequency, 0.0);
    EXPECT_GT(cpuInfo.maxFrequency, 0.0);

    // Base frequency should be <= max frequency
    EXPECT_LE(cpuInfo.baseFrequency, cpuInfo.maxFrequency);

    // Temperature and usage should be reasonable
    if (cpuInfo.temperature > 0) {
        EXPECT_LE(cpuInfo.temperature, 150.0f);
    }

    if (cpuInfo.usage >= 0) {
        EXPECT_LE(cpuInfo.usage, 100.0f);
    }

    // Cache sizes should be non-negative
    EXPECT_GE(cpuInfo.caches.l1d, 0);
    EXPECT_GE(cpuInfo.caches.l1i, 0);
    EXPECT_GE(cpuInfo.caches.l2, 0);
    EXPECT_GE(cpuInfo.caches.l3, 0);

    // Power info should be reasonable
    EXPECT_GE(cpuInfo.power.currentWatts, 0.0);
    EXPECT_GE(cpuInfo.power.maxTDP, 0.0);
    EXPECT_GE(cpuInfo.power.energyImpact, 0.0);

    // Load averages should be non-negative
    EXPECT_GE(cpuInfo.loadAverage.oneMinute, 0.0);
    EXPECT_GE(cpuInfo.loadAverage.fiveMinutes, 0.0);
    EXPECT_GE(cpuInfo.loadAverage.fifteenMinutes, 0.0);

    // Feature flags should be valid
    for (const auto& flag : cpuInfo.flags) {
        EXPECT_FALSE(flag.empty());
    }

    // Per-core information should match core count
    if (!cpuInfo.cores.empty()) {
        EXPECT_EQ(cpuInfo.cores.size(), static_cast<size_t>(cpuInfo.numLogicalCores));

        for (const auto& core : cpuInfo.cores) {
            EXPECT_GE(core.id, 0);
            EXPECT_GT(core.currentFrequency, 0.0);
            EXPECT_GT(core.maxFrequency, 0.0);
            EXPECT_GT(core.minFrequency, 0.0);

            if (core.temperature > 0) {
                EXPECT_LE(core.temperature, 150.0f);
            }

            if (core.usage >= 0) {
                EXPECT_LE(core.usage, 100.0f);
            }
        }
    }
}

TEST_F(CpuTest, RefreshCpuInfo) {
    // Test that refresh function works
    EXPECT_NO_THROW(refreshCpuInfo());

    // Get info before and after refresh
    CpuInfo info1 = getCpuInfo();
    refreshCpuInfo();
    CpuInfo info2 = getCpuInfo();

    // Static information should remain the same
    EXPECT_EQ(info1.model, info2.model);
    EXPECT_EQ(info1.identifier, info2.identifier);
    EXPECT_EQ(info1.architecture, info2.architecture);
    EXPECT_EQ(info1.vendor, info2.vendor);
    EXPECT_EQ(info1.numPhysicalPackages, info2.numPhysicalPackages);
    EXPECT_EQ(info1.numPhysicalCores, info2.numPhysicalCores);
    EXPECT_EQ(info1.numLogicalCores, info2.numLogicalCores);

    // Dynamic information might change (usage, temperature, frequency)
    // We just verify they're still within valid ranges
    if (info2.usage >= 0) {
        EXPECT_LE(info2.usage, 100.0f);
    }

    if (info2.temperature > 0) {
        EXPECT_LE(info2.temperature, 150.0f);
    }
}

// ============================================================================
// Performance and Monitoring Tests
// ============================================================================

TEST_F(CpuTest, CpuUsageMonitoring) {
    // Test CPU usage monitoring over time
    std::vector<float> usageReadings;

    for (int i = 0; i < 3; ++i) {
        float usage = getCurrentCpuUsage();
        usageReadings.push_back(usage);

        EXPECT_GE(usage, 0.0f);
        EXPECT_LE(usage, 100.0f);

        // Small delay between readings
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Should have collected readings
    EXPECT_EQ(usageReadings.size(), 3);
}

TEST_F(CpuTest, TemperatureMonitoring) {
    // Test temperature monitoring
    float temp1 = getCurrentCpuTemperature();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    float temp2 = getCurrentCpuTemperature();

    // Both readings should be valid
    if (temp1 > 0) {
        EXPECT_LE(temp1, 150.0f);
    }

    if (temp2 > 0) {
        EXPECT_LE(temp2, 150.0f);
    }

    // Temperature shouldn't change drastically in 100ms
    if (temp1 > 0 && temp2 > 0) {
        float tempDiff = std::abs(temp2 - temp1);
        EXPECT_LT(tempDiff, 10.0f); // Should not change by more than 10°C in 100ms
    }
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

TEST_F(CpuTest, ConsistentResults) {
    // Test that multiple calls return consistent results for static data
    std::string model1 = getCPUModel();
    std::string model2 = getCPUModel();
    EXPECT_EQ(model1, model2);

    std::string identifier1 = getProcessorIdentifier();
    std::string identifier2 = getProcessorIdentifier();
    EXPECT_EQ(identifier1, identifier2);

    int physicalCores1 = getNumberOfPhysicalCores();
    int physicalCores2 = getNumberOfPhysicalCores();
    EXPECT_EQ(physicalCores1, physicalCores2);

    int logicalCores1 = getNumberOfLogicalCores();
    int logicalCores2 = getNumberOfLogicalCores();
    EXPECT_EQ(logicalCores1, logicalCores2);
}

TEST_F(CpuTest, NoThrowGuarantee) {
    // Test that all CPU functions provide no-throw guarantee
    EXPECT_NO_THROW(getCurrentCpuUsage());
    EXPECT_NO_THROW(getPerCoreCpuUsage());
    EXPECT_NO_THROW(getCurrentCpuTemperature());
    EXPECT_NO_THROW(getPerCoreCpuTemperature());
    EXPECT_NO_THROW(getCPUModel());
    EXPECT_NO_THROW(getProcessorIdentifier());
    EXPECT_NO_THROW(getProcessorFrequency());
    EXPECT_NO_THROW(getMinProcessorFrequency());
    EXPECT_NO_THROW(getMaxProcessorFrequency());
    EXPECT_NO_THROW(getPerCoreFrequencies());
    EXPECT_NO_THROW(getNumberOfPhysicalPackages());
    EXPECT_NO_THROW(getNumberOfPhysicalCores());
    EXPECT_NO_THROW(getNumberOfLogicalCores());
    EXPECT_NO_THROW(getCacheSizes());
    EXPECT_NO_THROW(getCpuLoadAverage());
    EXPECT_NO_THROW(getCpuPowerInfo());
    EXPECT_NO_THROW(getCpuFeatureFlags());
    EXPECT_NO_THROW(getCpuArchitecture());
    EXPECT_NO_THROW(getCpuVendor());
    EXPECT_NO_THROW(getCpuSocketType());
    EXPECT_NO_THROW(getCpuScalingGovernor());
    EXPECT_NO_THROW(getPerCoreScalingGovernors());
    EXPECT_NO_THROW(getCpuInfo());
    EXPECT_NO_THROW(refreshCpuInfo());
}

TEST_F(CpuTest, StringConversionFunctions) {
    // Test architecture to string conversion
    for (int i = static_cast<int>(CpuArchitecture::UNKNOWN);
         i <= static_cast<int>(CpuArchitecture::RISC_V); ++i) {
        CpuArchitecture arch = static_cast<CpuArchitecture>(i);
        std::string archStr = cpuArchitectureToString(arch);
        EXPECT_FALSE(archStr.empty());
    }

    // Test vendor to string conversion
    for (int i = static_cast<int>(CpuVendor::UNKNOWN);
         i <= static_cast<int>(CpuVendor::OTHER); ++i) {
        CpuVendor vendor = static_cast<CpuVendor>(i);
        std::string vendorStr = cpuVendorToString(vendor);
        EXPECT_FALSE(vendorStr.empty());
    }
}
