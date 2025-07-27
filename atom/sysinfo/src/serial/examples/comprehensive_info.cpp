/**
 * @file comprehensive_info.cpp
 * @brief Comprehensive information example for the atom_sysinfo_sn library
 * 
 * This example demonstrates how to collect and display all available
 * system information including memory modules, network interfaces,
 * and export functionality.
 */

#include "sn.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>

using namespace atom::system;

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

void printMemoryModules(const std::vector<MemoryModuleInfo>& modules) {
    if (modules.empty()) {
        std::cout << "No memory module information available.\n";
        return;
    }
    
    std::cout << "Found " << modules.size() << " memory module(s):\n\n";
    
    for (size_t i = 0; i < modules.size(); ++i) {
        const auto& module = modules[i];
        std::cout << "Module " << (i + 1) << ":\n";
        std::cout << "  Serial Number: " 
                  << (module.serialNumber.empty() ? "Not available" : module.serialNumber) << "\n";
        std::cout << "  Manufacturer:  " 
                  << (module.manufacturer.empty() ? "Not available" : module.manufacturer) << "\n";
        std::cout << "  Part Number:   " 
                  << (module.partNumber.empty() ? "Not available" : module.partNumber) << "\n";
        std::cout << "  Type:          " 
                  << (module.type.empty() ? "Not available" : module.type) << "\n";
        
        if (module.sizeBytes > 0) {
            double sizeGB = static_cast<double>(module.sizeBytes) / (1024.0 * 1024.0 * 1024.0);
            std::cout << "  Size:          " << std::fixed << std::setprecision(1) 
                      << sizeGB << " GB\n";
        } else {
            std::cout << "  Size:          Not available\n";
        }
        
        if (module.speedMHz > 0) {
            std::cout << "  Speed:         " << module.speedMHz << " MHz\n";
        } else {
            std::cout << "  Speed:         Not available\n";
        }
        
        std::cout << "  Form Factor:   " 
                  << (module.formFactor.empty() ? "Not available" : module.formFactor) << "\n";
        std::cout << "  Slot:          " << static_cast<int>(module.slot) << "\n";
        
        if (i < modules.size() - 1) {
            std::cout << "\n";
        }
    }
}

void printNetworkInterfaces(const std::vector<NetworkInterfaceInfo>& interfaces) {
    if (interfaces.empty()) {
        std::cout << "No network interface information available.\n";
        return;
    }
    
    std::cout << "Found " << interfaces.size() << " network interface(s):\n\n";
    
    for (size_t i = 0; i < interfaces.size(); ++i) {
        const auto& interface = interfaces[i];
        std::cout << "Interface " << (i + 1) << ":\n";
        std::cout << "  Name:         " << interface.name << "\n";
        std::cout << "  MAC Address:  " << interface.macAddress << "\n";
        std::cout << "  Type:         " 
                  << (interface.type.empty() ? "Not available" : interface.type) << "\n";
        std::cout << "  Manufacturer: " 
                  << (interface.manufacturer.empty() ? "Not available" : interface.manufacturer) << "\n";
        std::cout << "  Driver:       " 
                  << (interface.driver.empty() ? "Not available" : interface.driver) << "\n";
        std::cout << "  Active:       " << (interface.isActive ? "Yes" : "No") << "\n";
        
        if (!interface.ipAddresses.empty()) {
            std::cout << "  IP Addresses: ";
            for (size_t j = 0; j < interface.ipAddresses.size(); ++j) {
                if (j > 0) std::cout << ", ";
                std::cout << interface.ipAddresses[j];
            }
            std::cout << "\n";
        } else {
            std::cout << "  IP Addresses: Not available\n";
        }
        
        if (i < interfaces.size() - 1) {
            std::cout << "\n";
        }
    }
}

void demonstrateComprehensiveInfo() {
    printSeparator("Comprehensive System Information");
    
    // Create system info with full configuration
    SystemInfoConfig config;
    config.includeMemoryModules = true;
    config.includeNetworkInterfaces = true;
    config.includeDiskSerials = true;
    config.includeSystemUuid = true;
    config.includeMacAddresses = true;
    config.cacheResults = true;
    config.enableLogging = true;
    
    auto sysInfo = createSystemInfo(config);
    
    // Get comprehensive information
    auto result = sysInfo->getComprehensiveInfo();
    
    if (result.success) {
        const auto& data = result.data;
        
        std::cout << "System information collected successfully!\n";
        std::cout << "Data validity: " << (data.isValid() ? "Valid" : "Invalid") << "\n";
        
        // Print using built-in toString method
        std::cout << "\n" << data.toString();
        
    } else {
        std::cout << "Failed to collect comprehensive information:\n";
        std::cout << "Error: " << result.errorMessage << "\n";
        std::cout << "Error Code: " << static_cast<int>(result.error) << "\n";
    }
}

void demonstrateMemoryModules() {
    printSeparator("Memory Modules");
    
    auto sysInfo = createSystemInfo();
    auto result = sysInfo->getMemoryModules();
    
    if (result.success) {
        printMemoryModules(result.data);
    } else {
        std::cout << "Failed to retrieve memory module information:\n";
        std::cout << "Error: " << result.errorMessage << "\n";
        std::cout << "This is normal on some systems where memory information\n";
        std::cout << "is not accessible or requires elevated privileges.\n";
    }
}

void demonstrateNetworkInterfaces() {
    printSeparator("Network Interfaces");
    
    auto sysInfo = createSystemInfo();
    auto result = sysInfo->getNetworkInterfaces();
    
    if (result.success) {
        printNetworkInterfaces(result.data);
    } else {
        std::cout << "Failed to retrieve network interface information:\n";
        std::cout << "Error: " << result.errorMessage << "\n";
    }
}

void demonstrateExportFunctionality() {
    printSeparator("Export Functionality");
    
    auto sysInfo = createSystemInfo();
    
    // Export to JSON
    std::cout << "Exporting to JSON format:\n";
    std::string jsonData = sysInfo->exportToJson(true);
    std::cout << jsonData << "\n\n";
    
    // Save JSON to file
    std::ofstream jsonFile("system_info.json");
    if (jsonFile.is_open()) {
        jsonFile << jsonData;
        jsonFile.close();
        std::cout << "JSON data saved to 'system_info.json'\n";
    }
    
    // Export to XML
    std::cout << "\nExporting to XML format:\n";
    std::string xmlData = sysInfo->exportToXml(true);
    std::cout << xmlData << "\n\n";
    
    // Save XML to file
    std::ofstream xmlFile("system_info.xml");
    if (xmlFile.is_open()) {
        xmlFile << xmlData;
        xmlFile.close();
        std::cout << "XML data saved to 'system_info.xml'\n";
    }
}

void demonstrateSystemQueries() {
    printSeparator("System ID Queries");
    
    auto sysInfo = createSystemInfo();
    
    // Query different types of system identifiers
    std::vector<std::pair<SystemIdType, std::string>> queries = {
        {SystemIdType::BIOS_SERIAL, "BIOS Serial"},
        {SystemIdType::MOTHERBOARD_SERIAL, "Motherboard Serial"},
        {SystemIdType::CPU_SERIAL, "CPU Serial"},
        {SystemIdType::SYSTEM_UUID, "System UUID"},
        {SystemIdType::MACHINE_ID, "Machine ID"},
        {SystemIdType::MAC_ADDRESS, "MAC Address"},
        {SystemIdType::DISK_SERIAL, "Disk Serial"},
        {SystemIdType::MEMORY_SERIAL, "Memory Serial"}
    };
    
    for (const auto& [idType, description] : queries) {
        auto result = sysInfo->querySystemId(idType);
        
        std::cout << std::setw(18) << std::left << description << ": ";
        
        if (result.success && result.data.isAvailable) {
            std::cout << result.data.value;
        } else {
            std::cout << "Not available";
            if (!result.data.errorMessage.empty()) {
                std::cout << " (" << result.data.errorMessage << ")";
            }
        }
        std::cout << "\n";
    }
}

void demonstrateSystemFingerprinting() {
    printSeparator("System Fingerprinting");
    
    auto sysInfo = createSystemInfo();
    
    // Get system fingerprint
    std::string fingerprint = sysInfo->getSystemFingerprint();
    std::cout << "System Fingerprint: " << fingerprint << "\n";
    std::cout << "Fingerprint Length: " << fingerprint.length() << " characters\n";
    
    // Get comprehensive info for fingerprint analysis
    auto result = sysInfo->getComprehensiveInfo();
    if (result.success) {
        std::string compFingerprint = result.data.getSystemFingerprint();
        std::cout << "Comprehensive Fingerprint: " << compFingerprint << "\n";
        std::cout << "Fingerprints Match: " << (fingerprint == compFingerprint ? "Yes" : "No") << "\n";
    }
    
    // Demonstrate fingerprint comparison
    std::cout << "\nFingerprint Comparison:\n";
    double similarity = SystemInfoHelpers::compareFingerprints(fingerprint, fingerprint);
    std::cout << "Self-similarity: " << std::fixed << std::setprecision(2) << similarity << "\n";
    
    // Compare with a slightly different fingerprint
    std::string modifiedFingerprint = fingerprint;
    if (!modifiedFingerprint.empty()) {
        modifiedFingerprint[0] = (modifiedFingerprint[0] == 'a') ? 'b' : 'a';
        similarity = SystemInfoHelpers::compareFingerprints(fingerprint, modifiedFingerprint);
        std::cout << "Modified similarity: " << std::fixed << std::setprecision(2) << similarity << "\n";
    }
}

int main() {
    std::cout << "Atom System Information - Comprehensive Information Example\n";
    std::cout << "==========================================================\n";
    
    try {
        // Demonstrate comprehensive information collection
        demonstrateComprehensiveInfo();
        
        // Demonstrate memory module information
        demonstrateMemoryModules();
        
        // Demonstrate network interface information
        demonstrateNetworkInterfaces();
        
        // Demonstrate system ID queries
        demonstrateSystemQueries();
        
        // Demonstrate system fingerprinting
        demonstrateSystemFingerprinting();
        
        // Demonstrate export functionality
        demonstrateExportFunctionality();
        
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "Comprehensive information example completed successfully!\n";
        std::cout << "Check the generated JSON and XML files for exported data.\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred\n";
        return 1;
    }
    
    return 0;
}
