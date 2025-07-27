/**
 * @file basic_usage.cpp
 * @brief Basic usage example for the atom_sysinfo_sn library
 * 
 * This example demonstrates the basic functionality of the enhanced
 * system information library, showing how to collect hardware serial
 * numbers and system identification information.
 */

#include "sn.hpp"
#include <iostream>
#include <iomanip>

using namespace atom::system;

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

void printHardwareSerials(SystemInfo& sysInfo) {
    printSeparator("Hardware Serial Numbers");
    
    auto result = sysInfo.getHardwareSerials();
    
    if (result.success) {
        const auto& data = result.data;
        
        std::cout << "BIOS Serial:        " 
                  << (data.biosSerial.empty() ? "Not available" : data.biosSerial) << "\n";
        std::cout << "Motherboard Serial: " 
                  << (data.motherboardSerial.empty() ? "Not available" : data.motherboardSerial) << "\n";
        std::cout << "CPU Serial:         " 
                  << (data.cpuSerial.empty() ? "Not available" : data.cpuSerial) << "\n";
        
        std::cout << "Disk Serials:       ";
        if (data.diskSerials.empty()) {
            std::cout << "Not available";
        } else {
            for (size_t i = 0; i < data.diskSerials.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << data.diskSerials[i];
            }
        }
        std::cout << "\n";
        
        std::cout << "\nData collected at: " 
                  << std::chrono::duration_cast<std::chrono::seconds>(
                         data.lastUpdate.time_since_epoch()).count() 
                  << " (Unix timestamp)\n";
    } else {
        std::cout << "Failed to retrieve hardware serials: " << result.errorMessage << "\n";
        std::cout << "Error code: " << static_cast<int>(result.error) << "\n";
    }
}

void printSystemIdentification(SystemInfo& sysInfo) {
    printSeparator("System Identification");
    
    auto result = sysInfo.getSystemIdentification();
    
    if (result.success) {
        const auto& data = result.data;
        
        std::cout << "System UUID:    " 
                  << (data.systemUuid.empty() ? "Not available" : data.systemUuid) << "\n";
        std::cout << "Machine ID:     " 
                  << (data.machineId.empty() ? "Not available" : data.machineId) << "\n";
        std::cout << "Boot ID:        " 
                  << (data.bootId.empty() ? "Not available" : data.bootId) << "\n";
        std::cout << "Hostname:       " 
                  << (data.hostname.empty() ? "Not available" : data.hostname) << "\n";
        std::cout << "Domain:         " 
                  << (data.domainName.empty() ? "Not available" : data.domainName) << "\n";
        
        std::cout << "MAC Addresses:  ";
        if (data.macAddresses.empty()) {
            std::cout << "Not available";
        } else {
            for (size_t i = 0; i < data.macAddresses.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << data.macAddresses[i];
            }
        }
        std::cout << "\n";
    } else {
        std::cout << "Failed to retrieve system identification: " << result.errorMessage << "\n";
        std::cout << "Error code: " << static_cast<int>(result.error) << "\n";
    }
}

void printSystemFingerprint(SystemInfo& sysInfo) {
    printSeparator("System Fingerprint");
    
    std::string fingerprint = sysInfo.getSystemFingerprint();
    
    if (!fingerprint.empty()) {
        std::cout << "System Fingerprint: " << fingerprint << "\n";
        std::cout << "Fingerprint Length: " << fingerprint.length() << " characters\n";
        
        // Show first and last 8 characters for privacy
        if (fingerprint.length() > 16) {
            std::cout << "Preview:            " 
                      << fingerprint.substr(0, 8) << "..." 
                      << fingerprint.substr(fingerprint.length() - 8) << "\n";
        }
    } else {
        std::cout << "Failed to generate system fingerprint\n";
    }
}

void printSystemInfo(SystemInfo& sysInfo) {
    printSeparator("System Information");
    
    std::cout << "Platform:       " << sysInfo.getPlatformName() << "\n";
    std::cout << "Supported:      " << (sysInfo.isSupported() ? "Yes" : "No") << "\n";
    std::cout << "Cache Valid:    " << (sysInfo.isCacheValid() ? "Yes" : "No") << "\n";
    std::cout << "Cache Age:      " << sysInfo.getCacheAge().count() << " seconds\n";
    
    // Get configuration
    const auto& config = sysInfo.getConfig();
    std::cout << "Caching:        " << (config.cacheResults ? "Enabled" : "Disabled") << "\n";
    std::cout << "Cache Timeout:  " << config.cacheTimeout.count() << " seconds\n";
    std::cout << "Logging:        " << (config.enableLogging ? "Enabled" : "Disabled") << "\n";
}

void demonstrateQuickAccess() {
    printSeparator("Quick Access Functions");
    
    // Quick fingerprint without creating SystemInfo instance
    std::string quickFingerprint = getQuickSystemFingerprint();
    std::cout << "Quick Fingerprint: " << quickFingerprint << "\n";
    
    // Check availability
    bool available = isSystemInfoAvailable();
    std::cout << "System Info Available: " << (available ? "Yes" : "No") << "\n";
    
    // Get platform capabilities
    auto capabilities = getPlatformCapabilities();
    std::cout << "\nPlatform Capabilities:\n";
    for (const auto& [capability, supported] : capabilities) {
        std::cout << "  " << std::setw(20) << std::left << capability 
                  << ": " << (supported ? "Supported" : "Not supported") << "\n";
    }
}

int main() {
    std::cout << "Atom System Information - Basic Usage Example\n";
    std::cout << "=============================================\n";
    
    try {
        // Create system info instance
        auto sysInfo = createSystemInfo();
        
        if (!sysInfo) {
            std::cerr << "Failed to create SystemInfo instance\n";
            return 1;
        }
        
        // Print basic system information
        printSystemInfo(*sysInfo);
        
        // Get and print hardware serial numbers
        printHardwareSerials(*sysInfo);
        
        // Get and print system identification
        printSystemIdentification(*sysInfo);
        
        // Get and print system fingerprint
        printSystemFingerprint(*sysInfo);
        
        // Demonstrate quick access functions
        demonstrateQuickAccess();
        
        // Show summary
        printSeparator("Summary");
        std::cout << sysInfo->getSummary();
        
        std::cout << "\nExample completed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred\n";
        return 1;
    }
    
    return 0;
}
