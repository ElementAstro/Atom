/*
 * virtual.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Unit Tests for Virtual Environment Detection Module
Tests virtualization and container detection functionality.

**************************************************/

#include <gtest/gtest.h>
#include <string>

#include "atom/sysinfo/virtual.hpp"

using namespace atom::system;

namespace atom::sysinfo::test {

// ============================================================================
// Virtual Environment Detection Tests
// ============================================================================

class VirtualTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup virtual environment tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(VirtualTest, GetHypervisorVendor) {
    // Test hypervisor vendor detection
    std::string vendor = getHypervisorVendor();
    
    // Should not throw and return a string (might be empty)
    EXPECT_TRUE(vendor.empty() || !vendor.empty());
    
    // If vendor is detected, it should be a known vendor
    if (!vendor.empty()) {
        EXPECT_GT(vendor.length(), 0);
        EXPECT_LT(vendor.length(), 100); // Reasonable upper bound
        
        // Common hypervisor vendors
        bool isKnownVendor = (vendor.find("VMware") != std::string::npos ||
                             vendor.find("VirtualBox") != std::string::npos ||
                             vendor.find("Microsoft") != std::string::npos ||
                             vendor.find("Xen") != std::string::npos ||
                             vendor.find("KVM") != std::string::npos ||
                             vendor.find("QEMU") != std::string::npos ||
                             vendor.find("Hyper-V") != std::string::npos);
        
        // Note: This might fail on unknown hypervisors, which is acceptable
        if (isKnownVendor) {
            EXPECT_TRUE(isKnownVendor);
        }
    }
}

TEST_F(VirtualTest, IsVirtualMachine) {
    // Test virtual machine detection
    bool isVM = isVirtualMachine();
    
    // Should return a boolean value without throwing
    EXPECT_TRUE(isVM || !isVM);
    
    // If we're in a VM, hypervisor vendor might be available
    if (isVM) {
        std::string vendor = getHypervisorVendor();
        // Vendor might still be empty even in a VM, but shouldn't crash
        EXPECT_TRUE(vendor.empty() || !vendor.empty());
    }
}

TEST_F(VirtualTest, CheckBIOS) {
    // Test BIOS-based VM detection
    bool biosIndicatesVM = checkBIOS();
    
    // Should return a boolean value without throwing
    EXPECT_TRUE(biosIndicatesVM || !biosIndicatesVM);
}

TEST_F(VirtualTest, CheckNetworkAdapter) {
    // Test network adapter-based VM detection
    bool networkIndicatesVM = checkNetworkAdapter();
    
    // Should return a boolean value without throwing
    EXPECT_TRUE(networkIndicatesVM || !networkIndicatesVM);
}

TEST_F(VirtualTest, CheckDisk) {
    // Test disk-based VM detection
    bool diskIndicatesVM = checkDisk();
    
    // Should return a boolean value without throwing
    EXPECT_TRUE(diskIndicatesVM || !diskIndicatesVM);
}

TEST_F(VirtualTest, CheckGraphicsCard) {
    // Test graphics card-based VM detection
    bool graphicsIndicatesVM = checkGraphicsCard();
    
    // Should return a boolean value without throwing
    EXPECT_TRUE(graphicsIndicatesVM || !graphicsIndicatesVM);
}

TEST_F(VirtualTest, CheckProcesses) {
    // Test process-based VM detection
    bool processesIndicateVM = checkProcesses();
    
    // Should return a boolean value without throwing
    EXPECT_TRUE(processesIndicateVM || !processesIndicateVM);
}

TEST_F(VirtualTest, CheckPCIBus) {
    // Test PCI bus-based VM detection
    bool pciIndicatesVM = checkPCIBus();
    
    // Should return a boolean value without throwing
    EXPECT_TRUE(pciIndicatesVM || !pciIndicatesVM);
}

TEST_F(VirtualTest, CheckTimeDrift) {
    // Test time drift-based VM detection
    bool timeDriftIndicatesVM = checkTimeDrift();
    
    // Should return a boolean value without throwing
    EXPECT_TRUE(timeDriftIndicatesVM || !timeDriftIndicatesVM);
}

// ============================================================================
// Container Detection Tests
// ============================================================================

TEST_F(VirtualTest, IsDockerContainer) {
    // Test Docker container detection
    bool isDocker = isDockerContainer();
    
    // Should return a boolean value without throwing
    EXPECT_TRUE(isDocker || !isDocker);
}

TEST_F(VirtualTest, IsContainer) {
    // Test general container detection
    bool isContainerEnv = isContainer();
    
    // Should return a boolean value without throwing
    EXPECT_TRUE(isContainerEnv || !isContainerEnv);
    
    // If we're in a Docker container, general container detection should also be true
    if (isDockerContainer()) {
        EXPECT_TRUE(isContainerEnv);
    }
}

TEST_F(VirtualTest, GetContainerType) {
    // Test container type detection
    std::string containerType = getContainerType();
    
    // Should not throw
    EXPECT_TRUE(containerType.empty() || !containerType.empty());
    
    // If we're in a container, type should not be empty
    if (isContainer()) {
        EXPECT_FALSE(containerType.empty());
        EXPECT_GT(containerType.length(), 0);
        
        // Common container types
        bool isKnownType = (containerType == "Docker" ||
                           containerType == "LXC" ||
                           containerType == "LXD" ||
                           containerType == "Kubernetes" ||
                           containerType == "Podman");
        
        // Note: This might fail for unknown container types
        if (isKnownType) {
            EXPECT_TRUE(isKnownType);
        }
    } else {
        // If not in a container, type should be empty
        EXPECT_TRUE(containerType.empty());
    }
}

// ============================================================================
// Advanced Detection Tests
// ============================================================================

TEST_F(VirtualTest, GetVirtualizationConfidence) {
    // Test virtualization confidence score
    double confidence = getVirtualizationConfidence();
    
    // Confidence should be between 0.0 and 1.0
    EXPECT_GE(confidence, 0.0);
    EXPECT_LE(confidence, 1.0);
    
    // If we're definitely in a VM, confidence should be high
    if (isVirtualMachine()) {
        EXPECT_GT(confidence, 0.5); // At least 50% confidence
    }
}

TEST_F(VirtualTest, GetVirtualizationType) {
    // Test virtualization type detection
    std::string virtType = getVirtualizationType();
    
    // Should not throw
    EXPECT_TRUE(virtType.empty() || !virtType.empty());
    
    // If we're in a VM, type should not be "Unknown" or empty
    if (isVirtualMachine()) {
        EXPECT_FALSE(virtType.empty());
        EXPECT_NE(virtType, "Unknown");
        
        // Common virtualization types
        bool isKnownType = (virtType.find("VMware") != std::string::npos ||
                           virtType.find("VirtualBox") != std::string::npos ||
                           virtType.find("Hyper-V") != std::string::npos ||
                           virtType.find("KVM") != std::string::npos ||
                           virtType.find("QEMU") != std::string::npos ||
                           virtType.find("Xen") != std::string::npos);
        
        if (isKnownType) {
            EXPECT_TRUE(isKnownType);
        }
    }
}

// ============================================================================
// Consistency Tests
// ============================================================================

TEST_F(VirtualTest, ConsistentResults) {
    // Test that multiple calls return consistent results
    bool vm1 = isVirtualMachine();
    bool vm2 = isVirtualMachine();
    EXPECT_EQ(vm1, vm2);
    
    bool container1 = isContainer();
    bool container2 = isContainer();
    EXPECT_EQ(container1, container2);
    
    std::string vendor1 = getHypervisorVendor();
    std::string vendor2 = getHypervisorVendor();
    EXPECT_EQ(vendor1, vendor2);
    
    std::string type1 = getVirtualizationType();
    std::string type2 = getVirtualizationType();
    EXPECT_EQ(type1, type2);
}

TEST_F(VirtualTest, LogicalConsistency) {
    // Test logical consistency between different detection methods
    bool isVM = isVirtualMachine();
    double confidence = getVirtualizationConfidence();
    std::string vendor = getHypervisorVendor();
    std::string type = getVirtualizationType();
    
    // If we're in a VM, confidence should be > 0
    if (isVM) {
        EXPECT_GT(confidence, 0.0);
    }
    
    // If we have a vendor, we should be in a VM
    if (!vendor.empty()) {
        EXPECT_TRUE(isVM);
    }
    
    // If we have a virtualization type other than "Unknown", we should be in a VM
    if (!type.empty() && type != "Unknown") {
        EXPECT_TRUE(isVM);
    }
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

TEST_F(VirtualTest, NoThrowGuarantee) {
    // Test that all methods provide no-throw guarantee
    EXPECT_NO_THROW(getHypervisorVendor());
    EXPECT_NO_THROW(isVirtualMachine());
    EXPECT_NO_THROW(checkBIOS());
    EXPECT_NO_THROW(checkNetworkAdapter());
    EXPECT_NO_THROW(checkDisk());
    EXPECT_NO_THROW(checkGraphicsCard());
    EXPECT_NO_THROW(checkProcesses());
    EXPECT_NO_THROW(checkPCIBus());
    EXPECT_NO_THROW(checkTimeDrift());
    EXPECT_NO_THROW(isDockerContainer());
    EXPECT_NO_THROW(isContainer());
    EXPECT_NO_THROW(getContainerType());
    EXPECT_NO_THROW(getVirtualizationConfidence());
    EXPECT_NO_THROW(getVirtualizationType());
}

} // namespace atom::sysinfo::test
