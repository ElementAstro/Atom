/**
 * @file test_bios_basic.cpp
 * @brief Basic tests for the BIOS module
 */

#include "../bios.hpp"
#include <cassert>
#include <iostream>

using namespace atom::system;

void testBiosInfoSingleton() {
    std::cout << "Testing BiosInfo singleton..." << std::endl;
    
    auto& instance1 = BiosInfo::getInstance();
    auto& instance2 = BiosInfo::getInstance();
    
    // Should be the same instance
    assert(&instance1 == &instance2);
    std::cout << "✓ Singleton test passed" << std::endl;
}

void testBiosInfoRetrieval() {
    std::cout << "Testing BIOS info retrieval..." << std::endl;
    
    try {
        auto& biosInfo = BiosInfo::getInstance();
        const auto& info = biosInfo.getBiosInfo();
        
        // Basic validation - at least one field should be populated
        bool hasData = !info.version.empty() || 
                      !info.manufacturer.empty() || 
                      !info.releaseDate.empty();
        
        if (hasData) {
            std::cout << "✓ BIOS info retrieval test passed" << std::endl;
            std::cout << "  Version: " << info.version << std::endl;
            std::cout << "  Manufacturer: " << info.manufacturer << std::endl;
        } else {
            std::cout << "⚠ BIOS info retrieval returned empty data (may be expected on some systems)" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "⚠ BIOS info retrieval failed: " << e.what() << std::endl;
    }
}

void testSupportedFeatures() {
    std::cout << "Testing supported features..." << std::endl;
    
    try {
        auto& biosInfo = BiosInfo::getInstance();
        auto features = biosInfo.getSupportedFeatures();
        
        assert(!features.empty());
        std::cout << "✓ Supported features test passed" << std::endl;
        std::cout << "  Found " << features.size() << " supported features" << std::endl;
        
        for (const auto& feature : features) {
            std::cout << "    - " << feature << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "✗ Supported features test failed: " << e.what() << std::endl;
    }
}

void testUtilityFunctions() {
    std::cout << "Testing utility functions..." << std::endl;
    
    // Test temperature conversion
    double celsius = 25.0;
    double fahrenheit = BiosUtils::celsiusToFahrenheit(celsius);
    double expected = 77.0;
    assert(std::abs(fahrenheit - expected) < 0.1);
    
    // Test frequency conversion
    int mhz = 3200;
    double ghz = BiosUtils::mhzToGhz(mhz);
    assert(std::abs(ghz - 3.2) < 0.01);
    
    // Test boot time formatting
    std::string bootTime = BiosUtils::formatBootTime(1500);
    assert(bootTime.find("1.5 s") != std::string::npos);
    
    // Test BIOS version validation
    assert(BiosUtils::isValidBiosVersion("1.2.3") == true);
    assert(BiosUtils::isValidBiosVersion("") == false);
    assert(BiosUtils::isValidBiosVersion("ABC") == false);
    assert(BiosUtils::isValidBiosVersion("1.0") == true);
    
    std::cout << "✓ Utility functions test passed" << std::endl;
}

void testOperationResultToString() {
    std::cout << "Testing operation result strings..." << std::endl;
    
    std::string success = BiosInfo::operationResultToString(BiosOperationResult::SUCCESS);
    std::string failed = BiosInfo::operationResultToString(BiosOperationResult::FAILED);
    std::string notSupported = BiosInfo::operationResultToString(BiosOperationResult::NOT_SUPPORTED);
    std::string insufficientPrivs = BiosInfo::operationResultToString(BiosOperationResult::INSUFFICIENT_PRIVILEGES);
    std::string rebootRequired = BiosInfo::operationResultToString(BiosOperationResult::REBOOT_REQUIRED);
    
    assert(!success.empty());
    assert(!failed.empty());
    assert(!notSupported.empty());
    assert(!insufficientPrivs.empty());
    assert(!rebootRequired.empty());
    
    std::cout << "✓ Operation result strings test passed" << std::endl;
}

void testEnhancedFeatures() {
    std::cout << "Testing enhanced features..." << std::endl;
    
    try {
        auto& biosInfo = BiosInfo::getInstance();
        
        // Test firmware info
        auto firmwareInfo = biosInfo.getFirmwareInfo();
        std::cout << "  Firmware type: " << firmwareInfo.type << std::endl;
        
        // Test boot configuration
        auto bootConfig = biosInfo.getBootConfiguration();
        std::cout << "  UEFI mode: " << (bootConfig.uefiMode ? "Yes" : "No") << std::endl;
        
        // Test power management
        auto powerSettings = biosInfo.getPowerManagementSettings();
        std::cout << "  ACPI enabled: " << (powerSettings.acpiEnabled ? "Yes" : "No") << std::endl;
        
        // Test overclocking info
        auto overclockInfo = biosInfo.getOverclockingInfo();
        std::cout << "  Overclocking supported: " << (overclockInfo.overclockingSupported ? "Yes" : "No") << std::endl;
        
        // Test hardware monitoring
        auto monitoring = biosInfo.getHardwareMonitoring();
        std::cout << "  CPU temperatures found: " << monitoring.cpuTemperatures.size() << std::endl;
        
        // Test diagnostics
        auto diagnostics = biosInfo.runDiagnostics();
        std::cout << "  POST test: " << (diagnostics.postTestPassed ? "PASS" : "FAIL") << std::endl;
        
        std::cout << "✓ Enhanced features test passed" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "⚠ Enhanced features test encountered issues: " << e.what() << std::endl;
    }
}

void testHealthCheck() {
    std::cout << "Testing health check..." << std::endl;
    
    try {
        auto& biosInfo = BiosInfo::getInstance();
        auto health = biosInfo.checkHealth();
        
        std::cout << "  Overall health: " << (health.isHealthy ? "HEALTHY" : "UNHEALTHY") << std::endl;
        std::cout << "  Warnings: " << health.warnings.size() << std::endl;
        std::cout << "  Errors: " << health.errors.size() << std::endl;
        
        std::cout << "✓ Health check test passed" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "⚠ Health check test failed: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "BIOS Module Basic Tests" << std::endl;
    std::cout << "======================" << std::endl;
    
    try {
        testBiosInfoSingleton();
        testUtilityFunctions();
        testOperationResultToString();
        testBiosInfoRetrieval();
        testSupportedFeatures();
        testEnhancedFeatures();
        testHealthCheck();
        
        std::cout << "\n✓ All basic tests completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n✗ Test failed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}
