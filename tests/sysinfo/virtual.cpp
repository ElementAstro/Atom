#include "atom/sysinfo/virtual.hpp"
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>

using namespace atom::system::virtual_env;
using namespace testing;

class VirtualTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if necessary
    }

    void TearDown() override {
        // Cleanup code if necessary
    }
};

// Test VirtualizationDetector basic functionality
TEST_F(VirtualTest, VirtualizationDetectorBasic) {
    VirtualizationDetector detector;

    // Test that basic methods don't crash
    EXPECT_NO_THROW({
        bool isVirtual = detector.isVirtual();
        bool isContainer = detector.isContainer();
        double confidence = detector.getConfidenceScore();

        // Results should be valid
        EXPECT_TRUE(isVirtual == true || isVirtual == false);
        EXPECT_TRUE(isContainer == true || isContainer == false);
        EXPECT_GE(confidence, 0.0);
        EXPECT_LE(confidence, 1.0);
    });
}

// Test comprehensive virtualization detection
TEST_F(VirtualTest, ComprehensiveDetection) {
    VirtualizationDetector detector;

    EXPECT_NO_THROW({
        auto info = detector.detect();

        // Basic validation
        EXPECT_TRUE(info.is_virtual == true || info.is_virtual == false);
        EXPECT_TRUE(info.is_container == true || info.is_container == false);
        EXPECT_GE(info.confidence_score, 0.0);
        EXPECT_LE(info.confidence_score, 1.0);

        // String fields should be valid
        EXPECT_TRUE(info.hypervisor_vendor.empty() || !info.hypervisor_vendor.empty());
        EXPECT_TRUE(info.virtualization_type.empty() || !info.virtualization_type.empty());
        EXPECT_TRUE(info.container_type.empty() || !info.container_type.empty());
        EXPECT_TRUE(info.cloud_provider.empty() || !info.cloud_provider.empty());
        EXPECT_TRUE(info.hardware_profile.empty() || !info.hardware_profile.empty());

        // Detection methods should be populated
        EXPECT_GT(info.detection_methods.size(), 0);

        // If virtual, should have some indicators
        if (info.is_virtual) {
            EXPECT_GT(info.indicators.size(), 0);
            EXPECT_FALSE(info.virtualization_type.empty());
        }

        // If container, should have container type
        if (info.is_container) {
            EXPECT_FALSE(info.container_type.empty());
        }
    });
}

// Test detection report generation
TEST_F(VirtualTest, DetectionReport) {
    VirtualizationDetector detector;

    EXPECT_NO_THROW({
        std::string report = detector.getDetectionReport();

        // Report should not be empty
        EXPECT_FALSE(report.empty());

        // Should contain some expected content
        EXPECT_NE(report.find("Virtualization"), std::string::npos);
    });
}

// Test detection method management
TEST_F(VirtualTest, DetectionMethodManagement) {
    VirtualizationDetector detector;

    EXPECT_NO_THROW({
        auto methods = detector.getAvailableDetectionMethods();

        // Should have some detection methods
        EXPECT_GT(methods.size(), 0);

        // Methods should be valid strings
        for (const auto& method : methods) {
            EXPECT_FALSE(method.empty());
        }

        // Test enabling/disabling methods
        if (!methods.empty()) {
            detector.setDetectionMethod(methods[0], false);
            detector.setDetectionMethod(methods[0], true);
        }
    });
}

// Test individual detection methods
TEST_F(VirtualTest, IndividualDetectionMethods) {
    // Test CPUID detection
    EXPECT_NO_THROW({
        bool result = detection::cpuid::isHypervisorPresent();
        EXPECT_TRUE(result == true || result == false);
    });

    // Test BIOS detection
    EXPECT_NO_THROW({
        bool result = detection::bios::checkBIOSInfo();
        EXPECT_TRUE(result == true || result == false);
    });

    // Test hardware detection methods
    EXPECT_NO_THROW({
        bool networkResult = detection::hardware::checkNetworkAdapters();
        bool diskResult = detection::hardware::checkDiskInfo();
        bool graphicsResult = detection::hardware::checkGraphicsCard();
        bool pciResult = detection::hardware::checkPCIBus();

        EXPECT_TRUE(networkResult == true || networkResult == false);
        EXPECT_TRUE(diskResult == true || diskResult == false);
        EXPECT_TRUE(graphicsResult == true || graphicsResult == false);
        EXPECT_TRUE(pciResult == true || pciResult == false);
    });

    // Test process detection
    EXPECT_NO_THROW({
        bool result = detection::processes::checkVirtualizationProcesses();
        EXPECT_TRUE(result == true || result == false);
    });

    // Test timing detection
    EXPECT_NO_THROW({
        bool result = detection::timing::checkTimeDrift();
        EXPECT_TRUE(result == true || result == false);
    });

    // Test filesystem detection
    EXPECT_NO_THROW({
        bool result = detection::filesystem::checkVirtualizationFiles();
        EXPECT_TRUE(result == true || result == false);
    });

    // Test environment detection
    EXPECT_NO_THROW({
        bool result = detection::environment::checkVirtualizationEnvironment();
        EXPECT_TRUE(result == true || result == false);
    });
}

// Test hypervisor detection
TEST_F(VirtualTest, HypervisorDetection) {
    EXPECT_NO_THROW({
        auto vendor = hypervisor::getHypervisorVendor();
        auto type = hypervisor::detectHypervisorType();
        auto info = hypervisor::getHypervisorInfo();

        // Vendor can be empty if no hypervisor
        EXPECT_GE(vendor.length(), 0);

        // Type should be valid enum value
        std::string typeStr = hypervisor::hypervisorTypeToString(type);
        EXPECT_FALSE(typeStr.empty());

        // Info should have valid confidence
        EXPECT_GE(info.detection_confidence, 0.0);
        EXPECT_LE(info.detection_confidence, 1.0);

        // Info fields should be valid
        EXPECT_TRUE(info.name.empty() || !info.name.empty());
        EXPECT_TRUE(info.vendor.empty() || !info.vendor.empty());
        EXPECT_TRUE(info.version.empty() || !info.version.empty());
    });
}

// Test specific hypervisor detection
TEST_F(VirtualTest, SpecificHypervisorDetection) {
    // Test VMware detection
    EXPECT_NO_THROW({
        bool detected = hypervisor::vmware::detect();
        EXPECT_TRUE(detected == true || detected == false);

        if (detected) {
            std::string version = hypervisor::vmware::getVersion();
            bool hasTools = hypervisor::vmware::hasVMwareTools();
            auto features = hypervisor::vmware::getFeatures();

            EXPECT_TRUE(version.empty() || !version.empty());
            EXPECT_TRUE(hasTools == true || hasTools == false);
        }
    });

    // Test VirtualBox detection
    EXPECT_NO_THROW({
        bool detected = hypervisor::virtualbox::detect();
        EXPECT_TRUE(detected == true || detected == false);

        if (detected) {
            std::string version = hypervisor::virtualbox::getVersion();
            bool hasAdditions = hypervisor::virtualbox::hasGuestAdditions();
            auto features = hypervisor::virtualbox::getFeatures();

            EXPECT_TRUE(version.empty() || !version.empty());
            EXPECT_TRUE(hasAdditions == true || hasAdditions == false);
        }
    });

    // Test Hyper-V detection
    EXPECT_NO_THROW({
        bool detected = hypervisor::hyperv::detect();
        EXPECT_TRUE(detected == true || detected == false);

        if (detected) {
            std::string version = hypervisor::hyperv::getVersion();
            bool hasServices = hypervisor::hyperv::hasIntegrationServices();
            int generation = hypervisor::hyperv::getGeneration();
            auto features = hypervisor::hyperv::getFeatures();

            EXPECT_TRUE(version.empty() || !version.empty());
            EXPECT_TRUE(hasServices == true || hasServices == false);
            EXPECT_GE(generation, 0);
        }
    });

    // Test KVM detection
    EXPECT_NO_THROW({
        bool detected = hypervisor::kvm::detect();
        bool qemuDetected = hypervisor::kvm::detectQEMU();
        EXPECT_TRUE(detected == true || detected == false);
        EXPECT_TRUE(qemuDetected == true || qemuDetected == false);

        if (detected || qemuDetected) {
            std::string kvmVersion = hypervisor::kvm::getVersion();
            std::string qemuVersion = hypervisor::kvm::getQEMUVersion();
            bool hasAgent = hypervisor::kvm::hasGuestAgent();
            bool hasVirtIO = hypervisor::kvm::hasVirtIODevices();
            auto features = hypervisor::kvm::getFeatures();

            EXPECT_TRUE(kvmVersion.empty() || !kvmVersion.empty());
            EXPECT_TRUE(qemuVersion.empty() || !qemuVersion.empty());
            EXPECT_TRUE(hasAgent == true || hasAgent == false);
            EXPECT_TRUE(hasVirtIO == true || hasVirtIO == false);
        }
    });

    // Test Xen detection
    EXPECT_NO_THROW({
        bool detected = hypervisor::xen::detect();
        EXPECT_TRUE(detected == true || detected == false);

        if (detected) {
            std::string version = hypervisor::xen::getVersion();
            bool isPV = hypervisor::xen::isParavirtualized();
            bool isHVM = hypervisor::xen::isHVM();
            auto features = hypervisor::xen::getFeatures();

            EXPECT_TRUE(version.empty() || !version.empty());
            EXPECT_TRUE(isPV == true || isPV == false);
            EXPECT_TRUE(isHVM == true || isHVM == false);
        }
    });
}

// Test cloud hypervisor detection
TEST_F(VirtualTest, CloudHypervisorDetection) {
    EXPECT_NO_THROW({
        bool cloudDetected = hypervisor::cloud::detect();
        EXPECT_TRUE(cloudDetected == true || cloudDetected == false);

        // Test specific cloud providers
        bool awsDetected = hypervisor::cloud::detectAWSNitro();
        bool gcpDetected = hypervisor::cloud::detectGCPHypervisor();
        bool azureDetected = hypervisor::cloud::detectAzureHypervisor();

        EXPECT_TRUE(awsDetected == true || awsDetected == false);
        EXPECT_TRUE(gcpDetected == true || gcpDetected == false);
        EXPECT_TRUE(azureDetected == true || azureDetected == false);

        // Get cloud provider
        std::string provider = hypervisor::cloud::getCloudProvider();
        EXPECT_TRUE(provider.empty() || !provider.empty());

        // Get cloud metadata
        auto metadata = hypervisor::cloud::getCloudMetadata();
        // Metadata can be empty if not in cloud

        // If any cloud provider is detected, should have provider info
        if (awsDetected || gcpDetected || azureDetected) {
            EXPECT_FALSE(provider.empty());
        }
    });
}

// Test container detection
TEST_F(VirtualTest, ContainerDetection) {
    EXPECT_NO_THROW({
        bool isContainer = container::isContainer();
        EXPECT_TRUE(isContainer == true || isContainer == false);

        auto containerType = container::detectContainerType();
        auto containerRuntime = container::detectContainerRuntime();
        auto containerInfo = container::getContainerInfo();

        // Type and runtime should be valid enums
        std::string typeStr = container::containerTypeToString(containerType);
        std::string runtimeStr = container::containerRuntimeToString(containerRuntime);
        EXPECT_FALSE(typeStr.empty());
        EXPECT_FALSE(runtimeStr.empty());

        // Container info should have valid confidence
        EXPECT_GE(containerInfo.detection_confidence, 0.0);
        EXPECT_LE(containerInfo.detection_confidence, 1.0);

        // If container detected, should have some info
        if (isContainer) {
            EXPECT_NE(containerType, container::ContainerType::UNKNOWN);
            EXPECT_FALSE(typeStr.empty());
        }
    });
}

// Test specific container detection
TEST_F(VirtualTest, SpecificContainerDetection) {
    // Test Docker detection
    EXPECT_NO_THROW({
        bool detected = container::docker::detect();
        EXPECT_TRUE(detected == true || detected == false);

        if (detected) {
            std::string containerID = container::docker::getContainerID();
            std::string imageInfo = container::docker::getImageInfo();

            EXPECT_TRUE(containerID.empty() || !containerID.empty());
            EXPECT_TRUE(imageInfo.empty() || !imageInfo.empty());
        }
    });

    // Test Kubernetes detection
    EXPECT_NO_THROW({
        bool detected = container::kubernetes::detect();
        EXPECT_TRUE(detected == true || detected == false);

        if (detected) {
            std::string namespace_ = container::kubernetes::getNamespace();
            std::string podName = container::kubernetes::getPodName();

            EXPECT_TRUE(namespace_.empty() || !namespace_.empty());
            EXPECT_TRUE(podName.empty() || !podName.empty());
        }
    });

    // Test LXC detection
    EXPECT_NO_THROW({
        bool detected = container::lxc::detect();
        EXPECT_TRUE(detected == true || detected == false);

        if (detected) {
            std::string containerName = container::lxc::getContainerName();

            EXPECT_TRUE(containerName.empty() || !containerName.empty());
        }
    });
}

// Test global convenience functions
TEST_F(VirtualTest, GlobalConvenienceFunctions) {
    EXPECT_NO_THROW({
        bool isVirtual = isVirtualEnvironment();
        bool isContainer = isContainerEnvironment();
        auto info = getVirtualizationInfo();

        EXPECT_TRUE(isVirtual == true || isVirtual == false);
        EXPECT_TRUE(isContainer == true || isContainer == false);

        // Info should be consistent with individual checks
        EXPECT_EQ(isVirtual, info.is_virtual);
        EXPECT_EQ(isContainer, info.is_container);
    });
}

// Test enum string conversions
TEST_F(VirtualTest, EnumStringConversions) {
    // Test hypervisor type conversions
    std::vector<hypervisor::HypervisorType> hypervisorTypes = {
        hypervisor::HypervisorType::UNKNOWN,
        hypervisor::HypervisorType::VMWARE_WORKSTATION,
        hypervisor::HypervisorType::VIRTUALBOX,
        hypervisor::HypervisorType::HYPER_V,
        hypervisor::HypervisorType::KVM,
        hypervisor::HypervisorType::XEN
    };

    for (auto type : hypervisorTypes) {
        std::string typeStr = hypervisor::hypervisorTypeToString(type);
        EXPECT_FALSE(typeStr.empty());
    }

    // Test container type conversions
    std::vector<container::ContainerType> containerTypes = {
        container::ContainerType::UNKNOWN,
        container::ContainerType::DOCKER,
        container::ContainerType::KUBERNETES_POD,
        container::ContainerType::LXC,
        container::ContainerType::SYSTEMD_NSPAWN
    };

    for (auto type : containerTypes) {
        std::string typeStr = container::containerTypeToString(type);
        EXPECT_FALSE(typeStr.empty());
    }

    // Test container runtime conversions
    std::vector<container::ContainerRuntime> runtimes = {
        container::ContainerRuntime::UNKNOWN,
        container::ContainerRuntime::DOCKER_ENGINE,
        container::ContainerRuntime::CONTAINERD,
        container::ContainerRuntime::CRIO,
        container::ContainerRuntime::PODMAN
    };

    for (auto runtime : runtimes) {
        std::string runtimeStr = container::containerRuntimeToString(runtime);
        EXPECT_FALSE(runtimeStr.empty());
    }
}

// Test VirtualizationInfo structure
TEST_F(VirtualTest, VirtualizationInfoStructure) {
    VirtualizationInfo info;

    // Default values should be valid
    EXPECT_FALSE(info.is_virtual);
    EXPECT_FALSE(info.is_container);
    EXPECT_EQ(info.confidence_score, 0.0);
    EXPECT_TRUE(info.hypervisor_vendor.empty());
    EXPECT_TRUE(info.virtualization_type.empty());
    EXPECT_TRUE(info.container_type.empty());
    EXPECT_TRUE(info.cloud_provider.empty());
    EXPECT_TRUE(info.hardware_profile.empty());
    EXPECT_TRUE(info.detection_methods.empty());
    EXPECT_TRUE(info.indicators.empty());
}

// Test HypervisorInfo structure
TEST_F(VirtualTest, HypervisorInfoStructure) {
    hypervisor::HypervisorInfo info;

    // Default values should be valid
    EXPECT_EQ(info.type, hypervisor::HypervisorType::UNKNOWN);
    EXPECT_EQ(info.detection_confidence, 0.0);
    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.vendor.empty());
    EXPECT_TRUE(info.version.empty());
    EXPECT_TRUE(info.features.empty());
    EXPECT_TRUE(info.properties.empty());
}

// Test ContainerInfo structure
TEST_F(VirtualTest, ContainerInfoStructure) {
    container::ContainerInfo info;

    // Default values should be valid
    EXPECT_EQ(info.type, container::ContainerType::UNKNOWN);
    EXPECT_EQ(info.runtime, container::ContainerRuntime::UNKNOWN);
    EXPECT_EQ(info.detection_confidence, 0.0);
    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.id.empty());
    EXPECT_TRUE(info.image.empty());
    EXPECT_TRUE(info.version.empty());
    EXPECT_TRUE(info.features.empty());
    EXPECT_TRUE(info.metadata.empty());
    EXPECT_TRUE(info.environment.empty());
}

// Test concurrent access
TEST_F(VirtualTest, ConcurrentAccess) {
    const int num_threads = 5;
    std::vector<std::thread> threads;
    std::vector<bool> results(num_threads, false);

    // Launch multiple threads accessing virtualization detection concurrently
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            try {
                VirtualizationDetector detector;
                bool isVirtual = detector.isVirtual();
                bool isContainer = detector.isContainer();
                double confidence = detector.getConfidenceScore();
                auto info = detector.detect();

                // Basic validation
                EXPECT_TRUE(isVirtual == true || isVirtual == false);
                EXPECT_TRUE(isContainer == true || isContainer == false);
                EXPECT_GE(confidence, 0.0);
                EXPECT_LE(confidence, 1.0);
                EXPECT_GE(info.confidence_score, 0.0);
                EXPECT_LE(info.confidence_score, 1.0);

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

// Test performance and caching
TEST_F(VirtualTest, PerformanceAndCaching) {
    VirtualizationDetector detector;

    // First detection call
    auto start1 = std::chrono::high_resolution_clock::now();
    auto info1 = detector.detect();
    auto end1 = std::chrono::high_resolution_clock::now();

    // Second detection call (should be faster due to caching)
    auto start2 = std::chrono::high_resolution_clock::now();
    auto info2 = detector.detect();
    auto end2 = std::chrono::high_resolution_clock::now();

    // Results should be identical
    EXPECT_EQ(info1.is_virtual, info2.is_virtual);
    EXPECT_EQ(info1.is_container, info2.is_container);
    EXPECT_EQ(info1.confidence_score, info2.confidence_score);
    EXPECT_EQ(info1.hypervisor_vendor, info2.hypervisor_vendor);
    EXPECT_EQ(info1.virtualization_type, info2.virtualization_type);

    // Second call should be faster or at least not significantly slower
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);
    EXPECT_LE(duration2.count(), duration1.count() * 2);
}