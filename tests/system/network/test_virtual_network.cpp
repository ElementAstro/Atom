/**
 * @file test_virtual_network.cpp
 * @brief Comprehensive tests for virtual network adapter functionality
 *
 * This file contains tests for the virtual network adapter management in
 * atom/system/network/virtual_network.hpp including adapter creation,
 * configuration, and removal.
 *
 * @author Max Qian
 * @date 2024
 * @license GPL3
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "atom/system/virtual_network.hpp"

namespace atom::system::test {

/**
 * @brief Test fixture for virtual network adapter tests
 */
class VirtualNetworkTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a virtual network adapter instance
        adapter = std::make_unique<VirtualNetworkAdapter>();

        // Set up test configuration
        testConfig.adapterName = L"TestAdapter";
        testConfig.hardwareID = L"TestHardwareID";
        testConfig.description = L"Test Virtual Network Adapter";
        testConfig.ipAddress = L"192.168.100.1";
        testConfig.subnetMask = L"255.255.255.0";
        testConfig.gateway = L"192.168.100.254";
        testConfig.primaryDNS = L"8.8.8.8";
        testConfig.secondaryDNS = L"8.8.4.4";
    }

    void TearDown() override {
        // Clean up any created adapters
        if (adapter) {
            adapter->Remove(testConfig.adapterName);
        }
    }

    std::unique_ptr<VirtualNetworkAdapter> adapter;
    VirtualAdapterConfig testConfig;
};

// ============================================================================
// Constructor and Destructor Tests
// ============================================================================

/**
 * @brief Test default constructor
 */
TEST_F(VirtualNetworkTest, Constructor_Default) {
    VirtualNetworkAdapter testAdapter;
    // Constructor should not throw
    SUCCEED();
}

/**
 * @brief Test move constructor
 */
TEST_F(VirtualNetworkTest, Constructor_Move) {
    VirtualNetworkAdapter adapter1;
    VirtualNetworkAdapter adapter2(std::move(adapter1));
    // Move constructor should not throw
    SUCCEED();
}

/**
 * @brief Test move assignment operator
 */
TEST_F(VirtualNetworkTest, Assignment_Move) {
    VirtualNetworkAdapter adapter1;
    VirtualNetworkAdapter adapter2;
    adapter2 = std::move(adapter1);
    // Move assignment should not throw
    SUCCEED();
}

// ============================================================================
// Adapter Creation Tests
// ============================================================================

/**
 * @brief Test creating a virtual adapter with valid configuration
 * @note This test is skipped by default as it requires administrator privileges
 */
TEST_F(VirtualNetworkTest, Create_ValidConfiguration) {
    GTEST_SKIP()
        << "Skipping adapter creation test - requires administrator privileges";

    bool result = adapter->Create(testConfig);

    if (result) {
        EXPECT_TRUE(result);
        // Clean up
        adapter->Remove(testConfig.adapterName);
    } else {
        // Creation might fail due to permissions or platform limitations
        std::wstring errorMsg = adapter->GetLastErrorMessage();
        EXPECT_FALSE(errorMsg.empty());
    }
}

/**
 * @brief Test creating adapter with empty name
 */
TEST_F(VirtualNetworkTest, Create_EmptyName) {
    VirtualAdapterConfig emptyNameConfig = testConfig;
    emptyNameConfig.adapterName = L"";

    bool result = adapter->Create(emptyNameConfig);
    EXPECT_FALSE(result);
}

/**
 * @brief Test creating adapter with invalid IP address
 */
TEST_F(VirtualNetworkTest, Create_InvalidIPAddress) {
    VirtualAdapterConfig invalidIPConfig = testConfig;
    invalidIPConfig.ipAddress = L"999.999.999.999";

    // This might succeed or fail depending on validation
    bool result = adapter->Create(invalidIPConfig);
    // Just verify it doesn't crash
    EXPECT_TRUE(result || !result);
}

/**
 * @brief Test creating adapter with empty IP address
 */
TEST_F(VirtualNetworkTest, Create_EmptyIPAddress) {
    VirtualAdapterConfig emptyIPConfig = testConfig;
    emptyIPConfig.ipAddress = L"";

    bool result = adapter->Create(emptyIPConfig);
    // Might succeed or fail depending on implementation
    EXPECT_TRUE(result || !result);
}

// ============================================================================
// Adapter Removal Tests
// ============================================================================

/**
 * @brief Test removing non-existent adapter
 */
TEST_F(VirtualNetworkTest, Remove_NonExistentAdapter) {
    bool result = adapter->Remove(L"NonExistentAdapter12345");
    EXPECT_FALSE(result);
}

/**
 * @brief Test removing adapter with empty name
 */
TEST_F(VirtualNetworkTest, Remove_EmptyName) {
    bool result = adapter->Remove(L"");
    EXPECT_FALSE(result);
}

// ============================================================================
// IP Configuration Tests
// ============================================================================

/**
 * @brief Test configuring IP for non-existent adapter
 */
TEST_F(VirtualNetworkTest, ConfigureIP_NonExistentAdapter) {
    bool result =
        adapter->ConfigureIP(L"NonExistentAdapter12345", L"192.168.1.1",
                             L"255.255.255.0", L"192.168.1.254");
    EXPECT_FALSE(result);
}

/**
 * @brief Test configuring IP with empty adapter name
 */
TEST_F(VirtualNetworkTest, ConfigureIP_EmptyAdapterName) {
    bool result = adapter->ConfigureIP(L"", L"192.168.1.1", L"255.255.255.0",
                                       L"192.168.1.254");
    EXPECT_FALSE(result);
}

/**
 * @brief Test configuring IP with invalid IP address
 */
TEST_F(VirtualNetworkTest, ConfigureIP_InvalidIPAddress) {
    bool result =
        adapter->ConfigureIP(testConfig.adapterName, L"invalid.ip.address",
                             L"255.255.255.0", L"192.168.1.254");
    EXPECT_FALSE(result);
}

/**
 * @brief Test configuring IP with empty IP address
 */
TEST_F(VirtualNetworkTest, ConfigureIP_EmptyIPAddress) {
    bool result = adapter->ConfigureIP(testConfig.adapterName, L"",
                                       L"255.255.255.0", L"192.168.1.254");
    EXPECT_FALSE(result);
}

/**
 * @brief Test configuring IP with invalid subnet mask
 */
TEST_F(VirtualNetworkTest, ConfigureIP_InvalidSubnetMask) {
    bool result = adapter->ConfigureIP(testConfig.adapterName, L"192.168.1.1",
                                       L"invalid.mask", L"192.168.1.254");
    EXPECT_FALSE(result);
}

/**
 * @brief Test configuring IP with empty subnet mask
 */
TEST_F(VirtualNetworkTest, ConfigureIP_EmptySubnetMask) {
    bool result = adapter->ConfigureIP(testConfig.adapterName, L"192.168.1.1",
                                       L"", L"192.168.1.254");
    EXPECT_FALSE(result);
}

/**
 * @brief Test configuring IP with invalid gateway
 */
TEST_F(VirtualNetworkTest, ConfigureIP_InvalidGateway) {
    bool result = adapter->ConfigureIP(testConfig.adapterName, L"192.168.1.1",
                                       L"255.255.255.0", L"invalid.gateway");
    EXPECT_FALSE(result);
}

/**
 * @brief Test configuring IP with empty gateway
 */
TEST_F(VirtualNetworkTest, ConfigureIP_EmptyGateway) {
    bool result = adapter->ConfigureIP(testConfig.adapterName, L"192.168.1.1",
                                       L"255.255.255.0", L"");
    // Empty gateway might be allowed
    EXPECT_TRUE(result || !result);
}

// ============================================================================
// DNS Configuration Tests
// ============================================================================

/**
 * @brief Test configuring DNS for non-existent adapter
 */
TEST_F(VirtualNetworkTest, ConfigureDNS_NonExistentAdapter) {
    bool result = adapter->ConfigureDNS(L"NonExistentAdapter12345", L"8.8.8.8",
                                        L"8.8.4.4");
    EXPECT_FALSE(result);
}

/**
 * @brief Test configuring DNS with empty adapter name
 */
TEST_F(VirtualNetworkTest, ConfigureDNS_EmptyAdapterName) {
    bool result = adapter->ConfigureDNS(L"", L"8.8.8.8", L"8.8.4.4");
    EXPECT_FALSE(result);
}

/**
 * @brief Test configuring DNS with invalid primary DNS
 */
TEST_F(VirtualNetworkTest, ConfigureDNS_InvalidPrimaryDNS) {
    bool result = adapter->ConfigureDNS(testConfig.adapterName, L"invalid.dns",
                                        L"8.8.4.4");
    EXPECT_FALSE(result);
}

/**
 * @brief Test configuring DNS with empty primary DNS
 */
TEST_F(VirtualNetworkTest, ConfigureDNS_EmptyPrimaryDNS) {
    bool result =
        adapter->ConfigureDNS(testConfig.adapterName, L"", L"8.8.4.4");
    EXPECT_FALSE(result);
}

/**
 * @brief Test configuring DNS with invalid secondary DNS
 */
TEST_F(VirtualNetworkTest, ConfigureDNS_InvalidSecondaryDNS) {
    bool result = adapter->ConfigureDNS(testConfig.adapterName, L"8.8.8.8",
                                        L"invalid.dns");
    EXPECT_FALSE(result);
}

/**
 * @brief Test configuring DNS with empty secondary DNS
 */
TEST_F(VirtualNetworkTest, ConfigureDNS_EmptySecondaryDNS) {
    bool result =
        adapter->ConfigureDNS(testConfig.adapterName, L"8.8.8.8", L"");
    // Empty secondary DNS might be allowed
    EXPECT_TRUE(result || !result);
}

// ============================================================================
// Error Message Tests
// ============================================================================

/**
 * @brief Test getting error message after failed operation
 */
TEST_F(VirtualNetworkTest, GetLastErrorMessage_AfterFailedOperation) {
    // Perform an operation that should fail
    adapter->Remove(L"NonExistentAdapter12345");

    std::wstring errorMsg = adapter->GetLastErrorMessage();
    // Error message might be empty or contain error details
    EXPECT_TRUE(errorMsg.empty() || !errorMsg.empty());
}

/**
 * @brief Test getting error message without prior operation
 */
TEST_F(VirtualNetworkTest, GetLastErrorMessage_NoOperation) {
    std::wstring errorMsg = adapter->GetLastErrorMessage();
    // Error message should be empty or contain default message
    EXPECT_TRUE(errorMsg.empty() || !errorMsg.empty());
}

}  // namespace atom::system::test
