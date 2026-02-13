#include "common.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>
#include <functional>

namespace atom::system {

// HardwareSerialData implementations
bool HardwareSerialData::isValid() const {
    return !biosSerial.empty() || !motherboardSerial.empty() ||
           !cpuSerial.empty() || !diskSerials.empty();
}

std::string HardwareSerialData::toString() const {
    std::ostringstream oss;
    oss << "Hardware Serial Information:\n";
    oss << "  BIOS Serial: " << (biosSerial.empty() ? "N/A" : biosSerial) << "\n";
    oss << "  Motherboard Serial: " << (motherboardSerial.empty() ? "N/A" : motherboardSerial) << "\n";
    oss << "  CPU Serial: " << (cpuSerial.empty() ? "N/A" : cpuSerial) << "\n";
    oss << "  Disk Serials: ";
    if (diskSerials.empty()) {
        oss << "N/A";
    } else {
        for (size_t i = 0; i < diskSerials.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << diskSerials[i];
        }
    }
    oss << "\n";
    return oss.str();
}

// SystemIdentificationData implementations
bool SystemIdentificationData::isValid() const {
    return !systemUuid.empty() || !machineId.empty() || !macAddresses.empty();
}

std::string SystemIdentificationData::toString() const {
    std::ostringstream oss;
    oss << "System Identification Information:\n";
    oss << "  System UUID: " << (systemUuid.empty() ? "N/A" : systemUuid) << "\n";
    oss << "  Machine ID: " << (machineId.empty() ? "N/A" : machineId) << "\n";
    oss << "  Boot ID: " << (bootId.empty() ? "N/A" : bootId) << "\n";
    oss << "  Hostname: " << (hostname.empty() ? "N/A" : hostname) << "\n";
    oss << "  Domain: " << (domainName.empty() ? "N/A" : domainName) << "\n";
    oss << "  MAC Addresses: ";
    if (macAddresses.empty()) {
        oss << "N/A";
    } else {
        for (size_t i = 0; i < macAddresses.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << macAddresses[i];
        }
    }
    oss << "\n";
    return oss.str();
}

// MemoryModuleInfo implementations
bool MemoryModuleInfo::isValid() const {
    return !serialNumber.empty() && sizeBytes > 0;
}

std::string MemoryModuleInfo::toString() const {
    std::ostringstream oss;
    oss << "Memory Module (Slot " << static_cast<int>(slot) << "):\n";
    oss << "  Serial: " << (serialNumber.empty() ? "N/A" : serialNumber) << "\n";
    oss << "  Manufacturer: " << (manufacturer.empty() ? "N/A" : manufacturer) << "\n";
    oss << "  Part Number: " << (partNumber.empty() ? "N/A" : partNumber) << "\n";
    oss << "  Type: " << (type.empty() ? "N/A" : type) << "\n";
    oss << "  Size: " << SystemInfoUtils::formatBytes(sizeBytes) << "\n";
    oss << "  Speed: " << speedMHz << " MHz\n";
    oss << "  Form Factor: " << (formFactor.empty() ? "N/A" : formFactor) << "\n";
    return oss.str();
}

// NetworkInterfaceInfo implementations
bool NetworkInterfaceInfo::isValid() const {
    return !name.empty() && !macAddress.empty();
}

std::string NetworkInterfaceInfo::toString() const {
    std::ostringstream oss;
    oss << "Network Interface (" << name << "):\n";
    oss << "  MAC Address: " << macAddress << "\n";
    oss << "  Type: " << (type.empty() ? "N/A" : type) << "\n";
    oss << "  Manufacturer: " << (manufacturer.empty() ? "N/A" : manufacturer) << "\n";
    oss << "  Driver: " << (driver.empty() ? "N/A" : driver) << "\n";
    oss << "  Active: " << (isActive ? "Yes" : "No") << "\n";
    oss << "  IP Addresses: ";
    if (ipAddresses.empty()) {
        oss << "N/A";
    } else {
        for (size_t i = 0; i < ipAddresses.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << ipAddresses[i];
        }
    }
    oss << "\n";
    return oss.str();
}

// ComprehensiveSystemInfo implementations
bool ComprehensiveSystemInfo::isValid() const {
    return hardwareSerials.isValid() || systemId.isValid() ||
           !memoryModules.empty() || !networkInterfaces.empty();
}

std::string ComprehensiveSystemInfo::toString() const {
    std::ostringstream oss;
    oss << "=== Comprehensive System Information ===\n\n";

    oss << hardwareSerials.toString() << "\n";
    oss << systemId.toString() << "\n";

    if (!memoryModules.empty()) {
        oss << "Memory Modules:\n";
        for (const auto& module : memoryModules) {
            oss << module.toString() << "\n";
        }
    }

    if (!networkInterfaces.empty()) {
        oss << "Network Interfaces:\n";
        for (const auto& interface : networkInterfaces) {
            oss << interface.toString() << "\n";
        }
    }

    if (!additionalProperties.empty()) {
        oss << "Additional Properties:\n";
        for (const auto& [key, value] : additionalProperties) {
            oss << "  " << key << ": " << value << "\n";
        }
    }

    return oss.str();
}

std::string ComprehensiveSystemInfo::getSystemFingerprint() const {
    std::ostringstream oss;

    // Combine key identifiers for fingerprint
    oss << hardwareSerials.biosSerial << "|";
    oss << hardwareSerials.motherboardSerial << "|";
    oss << hardwareSerials.cpuSerial << "|";
    oss << systemId.systemUuid << "|";
    oss << systemId.machineId << "|";

    // Add first MAC address if available
    if (!systemId.macAddresses.empty()) {
        oss << systemId.macAddresses[0] << "|";
    }

    // Add first disk serial if available
    if (!hardwareSerials.diskSerials.empty()) {
        oss << hardwareSerials.diskSerials[0] << "|";
    }

    return SystemInfoUtils::generateHash(oss.str());
}

// SystemInfoResult template specialization for error descriptions
template<>
std::string SystemInfoResult<HardwareSerialData>::getErrorDescription() const {
    switch (error) {
        case SystemInfoError::SUCCESS: return "Operation successful";
        case SystemInfoError::ACCESS_DENIED: return "Access denied to hardware information";
        case SystemInfoError::NOT_SUPPORTED: return "Hardware serial retrieval not supported on this platform";
        case SystemInfoError::HARDWARE_NOT_FOUND: return "Hardware components not found";
        case SystemInfoError::INVALID_DATA: return "Invalid or corrupted hardware data";
        case SystemInfoError::TIMEOUT: return "Operation timed out";
        case SystemInfoError::UNKNOWN_ERROR: return "Unknown error occurred";
        default: return "Unspecified error";
    }
}

// SystemInfoUtils implementations
namespace SystemInfoUtils {

std::string generateHash(const std::string& input) {
    // Simple hash implementation (in production, use a proper cryptographic hash)
    std::hash<std::string> hasher;
    auto hashValue = hasher(input);

    std::ostringstream oss;
    oss << std::hex << hashValue;
    return oss.str();
}

bool isValidSerial(const std::string& serial) {
    if (serial.empty() || serial.length() < 3) {
        return false;
    }

    // Check for common invalid patterns
    std::vector<std::string> invalidPatterns = {
        "N/A", "n/a", "Not Available", "Unknown", "Default", "0000000000",
        "FFFFFFFFFF", "...........", "----------"
    };

    for (const auto& pattern : invalidPatterns) {
        if (serial.find(pattern) != std::string::npos) {
            return false;
        }
    }

    return true;
}

bool isValidMacAddress(const std::string& macAddress) {
    // MAC address regex pattern (supports both : and - separators)
    std::regex macPattern(R"(^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$)");
    return std::regex_match(macAddress, macPattern);
}

bool isValidUuid(const std::string& uuid) {
    // UUID regex pattern
    std::regex uuidPattern(R"(^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$)");
    return std::regex_match(uuid, uuidPattern);
}

std::string formatBytes(uint64_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unitIndex];
    return oss.str();
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace SystemInfoUtils

} // namespace atom::system
