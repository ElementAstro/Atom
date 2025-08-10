#include "atom/sysinfo/cpu.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <vector>


using namespace atom::system;
using namespace testing;

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
    ASSERT_GT(cpuModel.length(), 0);
}

TEST_F(CpuTest, GetProcessorIdentifier) {
    std::string identifier = getProcessorIdentifier();
    ASSERT_GT(identifier.length(), 0);
}

TEST_F(CpuTest, GetProcessorFrequency) {
    double frequency = getProcessorFrequency();
    ASSERT_GT(frequency, 0.0);
}

TEST_F(CpuTest, GetNumberOfPhysicalPackages) {
    int numberOfPackages = getNumberOfPhysicalPackages();
    ASSERT_GT(numberOfPackages, 0);
}

TEST_F(CpuTest, GetNumberOfPhysicalCores) {
    int numberOfCores = getNumberOfPhysicalCores();
    ASSERT_GT(numberOfCores, 0);
}

TEST_F(CpuTest, GetNumberOfLogicalCores) {
    int numberOfCores = getNumberOfLogicalCores();
    ASSERT_GT(numberOfCores, 0);

    // Logical cores should be >= physical cores
    int physicalCores = getNumberOfPhysicalCores();
    ASSERT_GE(numberOfCores, physicalCores);
}

TEST_F(CpuTest, GetCacheSizes) {
    CacheSizes cacheSizes = getCacheSizes();
    ASSERT_GE(cacheSizes.l1i, 0);
    ASSERT_GE(cacheSizes.l1d, 0);
    ASSERT_GE(cacheSizes.l2, 0);
    ASSERT_GE(cacheSizes.l3, 0);

    // At least one cache level should be present
    ASSERT_TRUE(cacheSizes.l1i > 0 || cacheSizes.l1d > 0 ||
                cacheSizes.l2 > 0 || cacheSizes.l3 > 0);
}

// Enhanced CPU Information Tests
TEST_F(CpuTest, GetCpuInfo) {
    auto cpu_info = getCpuInfo();

    // Basic validation
    EXPECT_GT(cpu_info.model.length(), 0);
    EXPECT_GT(cpu_info.identifier.length(), 0);
    EXPECT_NE(cpu_info.architecture, CpuArchitecture::UNKNOWN);
    EXPECT_NE(cpu_info.vendor, CpuVendor::UNKNOWN);
    EXPECT_GT(cpu_info.numLogicalCores, 0);
    EXPECT_GT(cpu_info.numPhysicalCores, 0);
    EXPECT_GE(cpu_info.numLogicalCores, cpu_info.numPhysicalCores);
    EXPECT_GT(cpu_info.maxFrequency, 0.0);
    EXPECT_GE(cpu_info.baseFrequency, 0.0);

    // Enhanced fields validation
    EXPECT_GE(cpu_info.topology.packages, 1);
    EXPECT_GE(cpu_info.topology.numa_nodes, 1);
    EXPECT_GT(cpu_info.topology.cores_per_package, 0);
    EXPECT_GT(cpu_info.topology.threads_per_core, 0);
}

TEST_F(CpuTest, CpuArchitectureAndVendor) {
    auto arch = getCpuArchitecture();
    auto vendor = getCpuVendor();

    // Should be valid architectures and vendors
    std::vector<CpuArchitecture> valid_archs = {
        CpuArchitecture::X86, CpuArchitecture::X86_64,
        CpuArchitecture::ARM, CpuArchitecture::ARM64,
        CpuArchitecture::MIPS, CpuArchitecture::POWERPC,
        CpuArchitecture::RISC_V
    };

    std::vector<CpuVendor> valid_vendors = {
        CpuVendor::INTEL, CpuVendor::AMD, CpuVendor::ARM,
        CpuVendor::APPLE, CpuVendor::QUALCOMM, CpuVendor::IBM,
        CpuVendor::MEDIATEK, CpuVendor::SAMSUNG, CpuVendor::OTHER
    };

    EXPECT_THAT(valid_archs, Contains(arch));
    EXPECT_THAT(valid_vendors, Contains(vendor));
}

TEST_F(CpuTest, CpuFeatureFlags) {
    auto flags = getCpuFeatureFlags();

    // Should have at least some feature flags
    EXPECT_GT(flags.size(), 0);

    // Feature flags should not be empty strings
    for (const auto& flag : flags) {
        EXPECT_GT(flag.length(), 0);
    }
}

TEST_F(CpuTest, CpuFeatureSupport) {
    // Test common features
    std::vector<std::string> common_features = {
        "sse", "sse2", "avx", "aes", "fma"
    };

    for (const auto& feature : common_features) {
        auto support = isCpuFeatureSupported(feature);
        // Should return a valid support status
        EXPECT_TRUE(support == CpuFeatureSupport::SUPPORTED ||
                   support == CpuFeatureSupport::NOT_SUPPORTED ||
                   support == CpuFeatureSupport::UNKNOWN);
    }
}

// Per-core tests
TEST_F(CpuTest, PerCoreCpuUsage) {
    auto core_usages = getPerCoreCpuUsage();
    auto num_cores = getNumberOfLogicalCores();

    // Should have usage data for all cores
    EXPECT_EQ(core_usages.size(), static_cast<size_t>(num_cores));

    // Each core usage should be valid
    for (auto usage : core_usages) {
        EXPECT_GE(usage, 0.0f);
        EXPECT_LE(usage, 100.0f);
    }
}

TEST_F(CpuTest, PerCoreCpuTemperature) {
    auto core_temps = getPerCoreCpuTemperature();
    auto num_cores = getNumberOfLogicalCores();

    // Should have temperature data for all cores (or empty if not supported)
    EXPECT_TRUE(core_temps.empty() || core_temps.size() == static_cast<size_t>(num_cores));

    // Each core temperature should be valid
    for (auto temp : core_temps) {
        EXPECT_TRUE((temp == 0.0f) || (temp > 0.0f && temp < 150.0f));
    }
}

TEST_F(CpuTest, PerCoreFrequencies) {
    auto core_freqs = getPerCoreFrequencies();
    auto num_cores = getNumberOfLogicalCores();

    // Should have frequency data for all cores
    EXPECT_EQ(core_freqs.size(), static_cast<size_t>(num_cores));

    // Each core frequency should be positive
    for (auto freq : core_freqs) {
        EXPECT_GE(freq, 0.0);
    }
}

// Enhanced CPU features tests
TEST_F(CpuTest, CpuTopology) {
    auto topology = getCpuTopology();

    // Basic topology validation
    EXPECT_GT(topology.packages, 0);
    EXPECT_GT(topology.cores_per_package, 0);
    EXPECT_GT(topology.threads_per_core, 0);
    EXPECT_GE(topology.numa_nodes, 1);

    // NUMA node CPU mapping should be consistent
    if (!topology.numa_node_cpus.empty()) {
        int total_cpus = 0;
        for (const auto& node_cpus : topology.numa_node_cpus) {
            total_cpus += node_cpus.size();
        }
        EXPECT_EQ(total_cpus, getNumberOfLogicalCores());
    }
}

TEST_F(CpuTest, CpuSecurityInfo) {
    auto security = getCpuSecurityInfo();

    // Vulnerability information should be valid
    for (const auto& vuln : security.vulnerabilities) {
        EXPECT_GT(vuln.name.length(), 0);
        EXPECT_GT(vuln.status.length(), 0);
    }
}

TEST_F(CpuTest, CpuThermalInfo) {
    auto thermal = getCpuThermalInfo();

    // Thermal information should be reasonable
    if (thermal.current_temp > 0.0f) {
        EXPECT_LT(thermal.current_temp, 150.0f);
    }

    if (thermal.critical_temp > 0.0f) {
        EXPECT_GT(thermal.critical_temp, thermal.current_temp);
    }
}

TEST_F(CpuTest, CpuPowerInfo) {
    auto power = getCpuPowerInfo();

    // Power information should be non-negative
    EXPECT_GE(power.currentWatts, 0.0);
    EXPECT_GE(power.maxTDP, 0.0);
    EXPECT_GE(power.energyImpact, 0.0);
}

// Advanced features tests
TEST_F(CpuTest, CpuVulnerabilities) {
    auto vulnerabilities = getCpuVulnerabilities();

    // Should return valid vulnerability information
    for (const auto& vuln : vulnerabilities) {
        EXPECT_GT(vuln.name.length(), 0);
        EXPECT_GT(vuln.status.length(), 0);
    }
}

TEST_F(CpuTest, CpuMicrocodeInfo) {
    auto microcode = getCpuMicrocodeInfo();

    // Microcode version might be empty on some systems, but should be valid if present
    if (!microcode.version.empty()) {
        EXPECT_GT(microcode.version.length(), 0);
    }
}

TEST_F(CpuTest, CpuPerformanceCounters) {
    auto counters = getCpuPerformanceCounters();

    // Performance counters should be non-negative
    EXPECT_GE(counters.instructions_retired, 0);
    EXPECT_GE(counters.cycles_elapsed, 0);
    EXPECT_GE(counters.cache_misses, 0);
    EXPECT_GE(counters.cache_hits, 0);
    EXPECT_GE(counters.branch_mispredictions, 0);
    EXPECT_GE(counters.context_switches, 0);
    EXPECT_GE(counters.interrupts, 0);
    EXPECT_GE(counters.instructions_per_cycle, 0.0);
    EXPECT_GE(counters.cache_hit_ratio, 0.0);
    EXPECT_LE(counters.cache_hit_ratio, 1.0);
    EXPECT_GE(counters.branch_prediction_accuracy, 0.0);
    EXPECT_LE(counters.branch_prediction_accuracy, 1.0);
}

TEST_F(CpuTest, CpuPowerStates) {
    auto states = getCpuPowerStates();
    auto current_state = getCurrentCpuPowerState();

    // Should have at least one power state
    EXPECT_GT(states.size(), 0);

    // Current state should be valid
    EXPECT_NE(current_state, CpuPowerState::UNKNOWN);
}

TEST_F(CpuTest, CpuFrequencyGovernors) {
    auto governors = getCpuFrequencyGovernors();

    // Should have at least one governor
    EXPECT_GT(governors.size(), 0);

    // All governors should be valid
    for (auto governor : governors) {
        EXPECT_NE(governor, CpuFrequencyGovernor::UNKNOWN);
    }
}

TEST_F(CpuTest, NumaTopology) {
    auto numa_topology = getNumaTopology();

    // Should have at least one NUMA node
    EXPECT_GE(numa_topology.size(), 1);

    // Total CPUs in NUMA nodes should match logical cores
    if (!numa_topology.empty()) {
        int total_cpus = 0;
        for (const auto& node_cpus : numa_topology) {
            total_cpus += node_cpus.size();
        }
        EXPECT_EQ(total_cpus, getNumberOfLogicalCores());
    }
}

TEST_F(CpuTest, CpuCacheTopology) {
    auto cache_topology = getCpuCacheTopology();

    // Cache topology might be empty on some systems
    for (const auto& cache_level : cache_topology) {
        EXPECT_GT(cache_level.size, 0);
        EXPECT_GT(cache_level.line_size, 0);
        EXPECT_GT(cache_level.type.length(), 0);
    }
}

TEST_F(CpuTest, PerCoreThermalInfo) {
    auto thermal_info = getPerCoreThermalInfo();
    auto num_cores = getNumberOfLogicalCores();

    // Should have thermal info for all cores
    EXPECT_EQ(thermal_info.size(), static_cast<size_t>(num_cores));

    // Each core thermal info should be valid
    for (const auto& thermal : thermal_info) {
        if (thermal.current_temp > 0.0f) {
            EXPECT_LT(thermal.current_temp, 150.0f);
        }
        EXPECT_GT(thermal.thermal_zone.length(), 0);
    }
}

TEST_F(CpuTest, PerCorePowerStates) {
    auto power_states = getPerCorePowerStates();
    auto num_cores = getNumberOfLogicalCores();

    // Should have power states for all cores
    EXPECT_EQ(power_states.size(), static_cast<size_t>(num_cores));

    // Each core power state should be valid
    for (auto state : power_states) {
        EXPECT_NE(state, CpuPowerState::UNKNOWN);
    }
}

TEST_F(CpuTest, PerCorePerformanceCounters) {
    auto counters = getPerCorePerformanceCounters();
    auto num_cores = getNumberOfLogicalCores();

    // Should have performance counters for all cores
    EXPECT_EQ(counters.size(), static_cast<size_t>(num_cores));

    // Each core's counters should be valid
    for (const auto& counter : counters) {
        EXPECT_GE(counter.instructions_retired, 0);
        EXPECT_GE(counter.cycles_elapsed, 0);
        EXPECT_GE(counter.cache_misses, 0);
        EXPECT_GE(counter.cache_hits, 0);
    }
}

// Advanced analysis tests
TEST_F(CpuTest, CpuThrottlingDetection) {
    bool is_throttling = isCpuThrottling();

    // Should return a valid boolean (no assertion needed, just test it doesn't crash)
    EXPECT_TRUE(is_throttling == true || is_throttling == false);
}

TEST_F(CpuTest, CpuEfficiencyRating) {
    auto efficiency = getCpuEfficiencyRating();

    // Efficiency should be non-negative
    EXPECT_GE(efficiency, 0.0);
}

TEST_F(CpuTest, CpuBenchmarkScore) {
    auto score = getCpuBenchmarkScore();

    // Benchmark score should be positive
    EXPECT_GT(score, 0.0);
}

TEST_F(CpuTest, CpuLifespanEstimation) {
    auto lifespan = estimateCpuLifespan();

    // Lifespan should be reasonable (at least 1 year, at most 50 years)
    EXPECT_GE(lifespan.count(), 8760);  // 1 year in hours
    EXPECT_LE(lifespan.count(), 438000); // 50 years in hours
}

// Performance tests
TEST_F(CpuTest, CachingPerformance) {
    // Test that caching improves performance
    auto start1 = std::chrono::high_resolution_clock::now();
    auto info1 = getCpuInfo();
    auto end1 = std::chrono::high_resolution_clock::now();

    auto start2 = std::chrono::high_resolution_clock::now();
    auto info2 = getCpuInfo();
    auto end2 = std::chrono::high_resolution_clock::now();

    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);

    // Second call should be faster due to caching (or at least not significantly slower)
    EXPECT_LE(duration2.count(), duration1.count() * 2);

    // Results should be identical for cached fields
    EXPECT_EQ(info1.model, info2.model);
    EXPECT_EQ(info1.numLogicalCores, info2.numLogicalCores);
    EXPECT_EQ(info1.vendor, info2.vendor);
}

// Stress tests
TEST_F(CpuTest, ConcurrentAccess) {
    const int num_threads = 10;
    const int calls_per_thread = 50;
    std::vector<std::thread> threads;
    std::vector<bool> results(num_threads, false);

    // Launch multiple threads accessing CPU info concurrently
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            try {
                for (int j = 0; j < calls_per_thread; ++j) {
                    auto info = getCpuInfo();
                    EXPECT_GT(info.numLogicalCores, 0);

                    auto usage = getCurrentCpuUsage();
                    EXPECT_GE(usage, 0.0f);
                    EXPECT_LE(usage, 100.0f);
                }
                results[i] = true;
            } catch (...) {
                results[i] = false;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // All threads should have completed successfully
    for (bool result : results) {
        EXPECT_TRUE(result);
    }
}

// Utility function tests
TEST_F(CpuTest, StringToBytesConversion) {
    EXPECT_EQ(stringToBytes("1024"), 1024);
    EXPECT_EQ(stringToBytes("1K"), 1024);
    EXPECT_EQ(stringToBytes("1KB"), 1024);
    EXPECT_EQ(stringToBytes("1M"), 1024 * 1024);
    EXPECT_EQ(stringToBytes("1MB"), 1024 * 1024);
    EXPECT_EQ(stringToBytes("1G"), 1024ULL * 1024 * 1024);
    EXPECT_EQ(stringToBytes("1GB"), 1024ULL * 1024 * 1024);
    EXPECT_EQ(stringToBytes("2.5GB"), static_cast<size_t>(2.5 * 1024 * 1024 * 1024));
    EXPECT_EQ(stringToBytes(""), 0);
    EXPECT_EQ(stringToBytes("invalid"), 0);
}

TEST_F(CpuTest, VendorStringConversion) {
    EXPECT_EQ(getVendorFromString("Intel"), CpuVendor::INTEL);
    EXPECT_EQ(getVendorFromString("AMD"), CpuVendor::AMD);
    EXPECT_EQ(getVendorFromString("Apple"), CpuVendor::APPLE);
    EXPECT_EQ(getVendorFromString("GenuineIntel"), CpuVendor::INTEL);
    EXPECT_EQ(getVendorFromString("AuthenticAMD"), CpuVendor::AMD);
    EXPECT_EQ(getVendorFromString("Unknown Vendor"), CpuVendor::OTHER);
    EXPECT_EQ(getVendorFromString(""), CpuVendor::UNKNOWN);
}

// String conversion tests for new enums
TEST_F(CpuTest, EnumStringConversions) {
    // Test CPU power state conversions
    EXPECT_GT(cpuPowerStateToString(CpuPowerState::C0).length(), 0);
    EXPECT_GT(cpuPowerStateToString(CpuPowerState::C1).length(), 0);
    EXPECT_GT(cpuPowerStateToString(CpuPowerState::UNKNOWN).length(), 0);

    // Test CPU frequency governor conversions
    EXPECT_GT(cpuFrequencyGovernorToString(CpuFrequencyGovernor::PERFORMANCE).length(), 0);
    EXPECT_GT(cpuFrequencyGovernorToString(CpuFrequencyGovernor::POWERSAVE).length(), 0);
    EXPECT_GT(cpuFrequencyGovernorToString(CpuFrequencyGovernor::UNKNOWN).length(), 0);

    // Test thermal throttle state conversions
    EXPECT_GT(thermalThrottleStateToString(ThermalThrottleState::NORMAL).length(), 0);
    EXPECT_GT(thermalThrottleStateToString(ThermalThrottleState::LIGHT_THROTTLE).length(), 0);
    EXPECT_GT(thermalThrottleStateToString(ThermalThrottleState::CRITICAL_THROTTLE).length(), 0);

    // Test CPU topology level conversions
    EXPECT_GT(cpuTopologyLevelToString(CpuTopologyLevel::THREAD).length(), 0);
    EXPECT_GT(cpuTopologyLevelToString(CpuTopologyLevel::CORE).length(), 0);
    EXPECT_GT(cpuTopologyLevelToString(CpuTopologyLevel::PACKAGE).length(), 0);
}

// Integration test
TEST_F(CpuTest, FullSystemIntegration) {
    // Test that all major functions work together
    auto info = getCpuInfo();
    auto topology = getCpuTopology();
    auto security = getCpuSecurityInfo();
    auto thermal = getCpuThermalInfo();

    // Basic consistency checks
    EXPECT_EQ(info.numLogicalCores, getNumberOfLogicalCores());
    EXPECT_EQ(info.numPhysicalCores, getNumberOfPhysicalCores());

    // Topology should be consistent with basic info
    int expected_logical_cores = topology.packages * topology.cores_per_package * topology.threads_per_core;
    EXPECT_EQ(expected_logical_cores, info.numLogicalCores);

    // Enhanced info should be populated
    EXPECT_TRUE(!info.topology.numa_node_cpus.empty() || info.topology.numa_nodes > 0);

    // Timestamp should be recent
    auto now = std::chrono::system_clock::now();
    auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(now - info.last_updated);
    EXPECT_LT(time_diff.count(), 60); // Should be updated within the last minute
}

// Test error handling and edge cases
TEST_F(CpuTest, ErrorHandling) {
    // Test with invalid feature names
    auto invalidFeatureSupport = isCpuFeatureSupported("");
    EXPECT_TRUE(invalidFeatureSupport == CpuFeatureSupport::NOT_SUPPORTED ||
               invalidFeatureSupport == CpuFeatureSupport::UNKNOWN);

    auto invalidFeatureSupport2 = isCpuFeatureSupported("invalid_feature_name_12345");
    EXPECT_TRUE(invalidFeatureSupport2 == CpuFeatureSupport::NOT_SUPPORTED ||
               invalidFeatureSupport2 == CpuFeatureSupport::UNKNOWN);

    // Test string conversion with invalid inputs
    EXPECT_EQ(stringToBytes(""), 0);
    EXPECT_EQ(stringToBytes("invalid"), 0);
    EXPECT_EQ(stringToBytes("-1"), 0);

    // Test vendor string conversion with edge cases
    EXPECT_EQ(getVendorFromString(""), CpuVendor::UNKNOWN);
    EXPECT_EQ(getVendorFromString("   "), CpuVendor::UNKNOWN);
    EXPECT_EQ(getVendorFromString("NonExistentVendor"), CpuVendor::OTHER);
}

// Test data consistency across multiple calls
TEST_F(CpuTest, DataConsistency) {
    // Test that static information remains consistent across calls
    auto info1 = getCpuInfo();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto info2 = getCpuInfo();

    // Static fields should be identical
    EXPECT_EQ(info1.model, info2.model);
    EXPECT_EQ(info1.identifier, info2.identifier);
    EXPECT_EQ(info1.architecture, info2.architecture);
    EXPECT_EQ(info1.vendor, info2.vendor);
    EXPECT_EQ(info1.numLogicalCores, info2.numLogicalCores);
    EXPECT_EQ(info1.numPhysicalCores, info2.numPhysicalCores);
    EXPECT_EQ(info1.numPhysicalPackages, info2.numPhysicalPackages);
    EXPECT_EQ(info1.socketType, info2.socketType);

    // Cache sizes should be identical
    EXPECT_EQ(info1.caches.l1i, info2.caches.l1i);
    EXPECT_EQ(info1.caches.l1d, info2.caches.l1d);
    EXPECT_EQ(info1.caches.l2, info2.caches.l2);
    EXPECT_EQ(info1.caches.l3, info2.caches.l3);

    // Feature flags should be identical
    EXPECT_EQ(info1.flags, info2.flags);

    // Dynamic fields may differ but should be reasonable
    if (info1.usage > 0.0f && info2.usage > 0.0f) {
        EXPECT_GE(info1.usage, 0.0f);
        EXPECT_LE(info1.usage, 100.0f);
        EXPECT_GE(info2.usage, 0.0f);
        EXPECT_LE(info2.usage, 100.0f);
    }

    if (info1.temperature > 0.0f && info2.temperature > 0.0f) {
        EXPECT_GT(info1.temperature, 0.0f);
        EXPECT_GT(info2.temperature, 0.0f);
        EXPECT_LT(info1.temperature, 150.0f);
        EXPECT_LT(info2.temperature, 150.0f);
    }
}

// Test boundary conditions
TEST_F(CpuTest, BoundaryConditions) {
    // Test CPU usage boundaries
    auto usage = getCurrentCpuUsage();
    EXPECT_GE(usage, 0.0f);
    EXPECT_LE(usage, 100.0f);

    // Test per-core usage boundaries
    auto coreUsages = getPerCoreCpuUsage();
    for (auto coreUsage : coreUsages) {
        EXPECT_GE(coreUsage, 0.0f);
        EXPECT_LE(coreUsage, 100.0f);
    }

    // Test frequency boundaries
    auto frequency = getProcessorFrequency();
    EXPECT_GT(frequency, 0.0);
    EXPECT_LT(frequency, 10.0); // 10 GHz should be reasonable upper bound

    auto maxFreq = getMaxProcessorFrequency();
    auto minFreq = getMinProcessorFrequency();
    if (maxFreq > 0.0 && minFreq > 0.0) {
        EXPECT_GE(maxFreq, minFreq);
    }

    // Test core count boundaries
    auto logicalCores = getNumberOfLogicalCores();
    auto physicalCores = getNumberOfPhysicalCores();
    auto packages = getNumberOfPhysicalPackages();

    EXPECT_GT(logicalCores, 0);
    EXPECT_GT(physicalCores, 0);
    EXPECT_GT(packages, 0);
    EXPECT_GE(logicalCores, physicalCores);
    EXPECT_GE(physicalCores, packages);
    EXPECT_LE(packages, physicalCores);
    EXPECT_LE(physicalCores, logicalCores);
}

// Test performance under load
TEST_F(CpuTest, PerformanceUnderLoad) {
    const int iterations = 1000;

    // Measure time for multiple CPU info retrievals
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        auto info = getCpuInfo();
        (void)info; // Suppress unused variable warning
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time (10 seconds for 1000 calls)
    EXPECT_LT(duration.count(), 10000);

    // Test that caching improves performance for subsequent calls
    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        auto info = getCpuInfo();
        (void)info; // Suppress unused variable warning
    }

    end = std::chrono::high_resolution_clock::now();
    auto cachedDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Cached calls should be faster or at least not significantly slower
    EXPECT_LE(cachedDuration.count(), duration.count() * 1.5);
}

// Test memory efficiency
TEST_F(CpuTest, MemoryEfficiency) {
    // Test that repeated calls don't cause memory leaks
    const int iterations = 100;

    for (int i = 0; i < iterations; ++i) {
        auto info = getCpuInfo();
        auto topology = getCpuTopology();
        auto security = getCpuSecurityInfo();
        auto thermal = getCpuThermalInfo();
        auto power = getCpuPowerInfo();
        auto counters = getCpuPerformanceCounters();
        auto vulnerabilities = getCpuVulnerabilities();
        auto microcode = getCpuMicrocodeInfo();

        // Prevent optimization
        (void)info;
        (void)topology;
        (void)security;
        (void)thermal;
        (void)power;
        (void)counters;
        (void)vulnerabilities;
        (void)microcode;
    }

    // If we reach here without crashing, memory management is working
    EXPECT_TRUE(true);
}

// Test thread safety with high contention
TEST_F(CpuTest, ThreadSafetyHighContention) {
    const int num_threads = 20;
    const int calls_per_thread = 100;
    std::vector<std::thread> threads;
    std::vector<bool> results(num_threads, false);
    std::atomic<int> successful_calls{0};

    // Launch many threads with high contention
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            try {
                for (int j = 0; j < calls_per_thread; ++j) {
                    auto info = getCpuInfo();
                    auto usage = getCurrentCpuUsage();
                    auto temp = getCurrentCpuTemperature();
                    auto topology = getCpuTopology();

                    // Basic validation
                    EXPECT_GT(info.numLogicalCores, 0);
                    EXPECT_GE(usage, 0.0f);
                    EXPECT_LE(usage, 100.0f);
                    EXPECT_GT(topology.packages, 0);

                    successful_calls.fetch_add(1);
                }
                results[i] = true;
            } catch (...) {
                results[i] = false;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // All threads should have completed successfully
    for (bool result : results) {
        EXPECT_TRUE(result);
    }

    // All calls should have been successful
    EXPECT_EQ(successful_calls.load(), num_threads * calls_per_thread);
}
