#ifndef _WIN32

#include "linux.hpp"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <unistd.h>
#include <sys/utsname.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace atom::system {

LinuxSystemInfo::LinuxSystemInfo() {
    spdlog::debug("LinuxSystemInfo instance created");
}

SystemInfoResult<HardwareSerialData> LinuxSystemInfo::getHardwareSerials() {
    SystemInfoResult<HardwareSerialData> result;
    
    try {
        result.data.biosSerial = validateSerial(readFile("/sys/class/dmi/id/product_serial"));
        result.data.motherboardSerial = validateSerial(readFile("/sys/class/dmi/id/board_serial"));
        result.data.cpuSerial = validateSerial(readFile("/proc/cpuinfo", "Serial"));
        result.data.diskSerials = getDiskSerials();
        result.data.lastUpdate = std::chrono::system_clock::now();
        
        result.success = result.data.isValid();
        
        if (!result.success) {
            result.error = SystemInfoError::HARDWARE_NOT_FOUND;
            result.errorMessage = "No valid hardware serial numbers found";
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error = SystemInfoError::UNKNOWN_ERROR;
        result.errorMessage = e.what();
        lastError_ = e.what();
    }
    
    return result;
}

SystemInfoResult<SystemIdentificationData> LinuxSystemInfo::getSystemIdentification() {
    SystemInfoResult<SystemIdentificationData> result;
    
    try {
        result.data.systemUuid = getSystemUuid();
        result.data.machineId = getMachineId();
        result.data.bootId = getBootId();
        result.data.hostname = getHostname();
        result.data.domainName = getDomainName();
        result.data.macAddresses = getNetworkMacAddresses();
        result.data.lastUpdate = std::chrono::system_clock::now();
        
        result.success = result.data.isValid();
        
        if (!result.success) {
            result.error = SystemInfoError::HARDWARE_NOT_FOUND;
            result.errorMessage = "No valid system identification found";
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error = SystemInfoError::UNKNOWN_ERROR;
        result.errorMessage = e.what();
        lastError_ = e.what();
    }
    
    return result;
}

SystemInfoResult<std::vector<MemoryModuleInfo>> LinuxSystemInfo::getMemoryModules() {
    SystemInfoResult<std::vector<MemoryModuleInfo>> result;
    
    try {
        // Try to get detailed memory information from DMI
        result.data = getMemoryModulesFromDmi();
        
        // If DMI is not available, get basic info from /proc/meminfo
        if (result.data.empty()) {
            auto basicInfo = getMemoryInfoFromProc();
            if (basicInfo.isValid()) {
                result.data.push_back(basicInfo);
            }
        }
        
        result.success = !result.data.empty();
        
        if (!result.success) {
            result.error = SystemInfoError::HARDWARE_NOT_FOUND;
            result.errorMessage = "No memory module information found";
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error = SystemInfoError::UNKNOWN_ERROR;
        result.errorMessage = e.what();
        lastError_ = e.what();
    }
    
    return result;
}

SystemInfoResult<std::vector<NetworkInterfaceInfo>> LinuxSystemInfo::getNetworkInterfaces() {
    SystemInfoResult<std::vector<NetworkInterfaceInfo>> result;
    
    try {
        result.data = getNetworkInterfacesFromSys();
        result.success = !result.data.empty();
        
        if (!result.success) {
            result.error = SystemInfoError::HARDWARE_NOT_FOUND;
            result.errorMessage = "No network interfaces found";
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error = SystemInfoError::UNKNOWN_ERROR;
        result.errorMessage = e.what();
        lastError_ = e.what();
    }
    
    return result;
}

SystemInfoResult<ComprehensiveSystemInfo> LinuxSystemInfo::getComprehensiveInfo(const SystemInfoConfig& config) {
    SystemInfoResult<ComprehensiveSystemInfo> result;
    
    try {
        auto hardwareResult = getHardwareSerials();
        if (hardwareResult.success) {
            result.data.hardwareSerials = hardwareResult.data;
        }
        
        auto systemIdResult = getSystemIdentification();
        if (systemIdResult.success) {
            result.data.systemId = systemIdResult.data;
        }
        
        if (config.includeMemoryModules) {
            auto memoryResult = getMemoryModules();
            if (memoryResult.success) {
                result.data.memoryModules = memoryResult.data;
            }
        }
        
        if (config.includeNetworkInterfaces) {
            auto networkResult = getNetworkInterfaces();
            if (networkResult.success) {
                result.data.networkInterfaces = networkResult.data;
            }
        }
        
        result.data.lastUpdate = std::chrono::system_clock::now();
        result.success = result.data.isValid();
        
        if (!result.success) {
            result.error = SystemInfoError::HARDWARE_NOT_FOUND;
            result.errorMessage = "No valid system information found";
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error = SystemInfoError::UNKNOWN_ERROR;
        result.errorMessage = e.what();
        lastError_ = e.what();
    }
    
    return result;
}

SystemInfoResult<SystemIdQuery> LinuxSystemInfo::querySystemId(SystemIdType type) {
    SystemInfoResult<SystemIdQuery> result;
    result.data.type = type;
    result.data.timestamp = std::chrono::system_clock::now();
    
    try {
        switch (type) {
            case SystemIdType::BIOS_SERIAL:
                result.data.value = validateSerial(readFile("/sys/class/dmi/id/product_serial"));
                break;
            case SystemIdType::MOTHERBOARD_SERIAL:
                result.data.value = validateSerial(readFile("/sys/class/dmi/id/board_serial"));
                break;
            case SystemIdType::CPU_SERIAL:
                result.data.value = validateSerial(readFile("/proc/cpuinfo", "Serial"));
                break;
            case SystemIdType::SYSTEM_UUID:
                result.data.value = getSystemUuid();
                break;
            case SystemIdType::MACHINE_ID:
                result.data.value = getMachineId();
                break;
            case SystemIdType::MAC_ADDRESS: {
                auto macs = getNetworkMacAddresses();
                if (!macs.empty()) {
                    result.data.value = macs[0];
                }
                break;
            }
            case SystemIdType::DISK_SERIAL: {
                auto disks = getDiskSerials();
                if (!disks.empty()) {
                    result.data.value = disks[0];
                }
                break;
            }
            case SystemIdType::MEMORY_SERIAL: {
                auto modules = getMemoryModulesFromDmi();
                if (!modules.empty() && !modules[0].serialNumber.empty()) {
                    result.data.value = modules[0].serialNumber;
                }
                break;
            }
        }
        
        result.data.isAvailable = !result.data.value.empty();
        result.success = true;
        
        if (!result.data.isAvailable) {
            result.data.errorMessage = "System identifier not available";
        }
    } catch (const std::exception& e) {
        result.success = false;
        result.error = SystemInfoError::UNKNOWN_ERROR;
        result.errorMessage = e.what();
        result.data.errorMessage = e.what();
        lastError_ = e.what();
    }
    
    return result;
}

bool LinuxSystemInfo::isDmiAvailable() const {
    return isDirectoryExists("/sys/class/dmi/id");
}

std::string LinuxSystemInfo::getLastError() const {
    return lastError_;
}

std::string LinuxSystemInfo::readFile(const std::string& path, const std::string& key) const {
    spdlog::debug("Reading file: {}", path);

    if (!isFileReadable(path)) {
        spdlog::warn("File does not exist or is not readable: {}", path);
        return "";
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        spdlog::error("Failed to open file: {}", path);
        return "";
    }

    std::string content;
    if (key.empty()) {
        std::getline(file, content);
        content = trim(content);
        spdlog::debug("Read content from {}: {}", path, content);
    } else {
        std::string line;
        while (std::getline(file, line)) {
            if (line.find(key) != std::string::npos) {
                auto pos = line.find(":");
                if (pos != std::string::npos && pos + 2 < line.length()) {
                    content = trim(line.substr(pos + 2));
                    spdlog::debug("Found key '{}' with value: {}", key, content);
                }
                break;
            }
        }
    }

    return content;
}

std::vector<std::string> LinuxSystemInfo::readFileLines(const std::string& path) const {
    std::vector<std::string> lines;
    
    if (!isFileReadable(path)) {
        return lines;
    }
    
    std::ifstream file(path);
    if (!file.is_open()) {
        return lines;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(trim(line));
    }
    
    return lines;
}

std::string LinuxSystemInfo::executeCommand(const std::string& command) const {
    std::string result;
    char buffer[128];

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return result;
    }

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    pclose(pipe);
    return trim(result);
}

std::string LinuxSystemInfo::getSystemUuid() const {
    // Try multiple sources for system UUID
    std::string uuid = readFile("/sys/class/dmi/id/product_uuid");
    if (uuid.empty()) {
        uuid = readFile("/proc/sys/kernel/random/uuid");
    }
    return uuid;
}

std::string LinuxSystemInfo::getMachineId() const {
    return readFile("/etc/machine-id");
}

std::string LinuxSystemInfo::getBootId() const {
    return readFile("/proc/sys/kernel/random/boot_id");
}

std::string LinuxSystemInfo::getHostname() const {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        return std::string(hostname);
    }
    return "";
}

std::string LinuxSystemInfo::getDomainName() const {
    return readFile("/proc/sys/kernel/domainname");
}

std::vector<std::string> LinuxSystemInfo::getNetworkMacAddresses() const {
    std::vector<std::string> macAddresses;

    try {
        for (const auto& entry : std::filesystem::directory_iterator("/sys/class/net")) {
            if (entry.is_directory()) {
                std::string interfaceName = entry.path().filename().string();
                if (interfaceName != "lo") { // Skip loopback interface
                    std::string macPath = entry.path().string() + "/address";
                    std::string mac = readFile(macPath);
                    if (!mac.empty() && SystemInfoUtils::isValidMacAddress(mac)) {
                        macAddresses.push_back(mac);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error reading network interfaces: {}", e.what());
    }

    return macAddresses;
}

std::vector<std::string> LinuxSystemInfo::getDiskSerials() const {
    std::vector<std::string> serials;

    try {
        for (const auto& entry : std::filesystem::directory_iterator("/sys/block")) {
            if (entry.is_directory()) {
                std::string serialPath = entry.path().string() + "/device/serial";
                std::string serial = validateSerial(readFile(serialPath));
                if (!serial.empty()) {
                    serials.push_back(serial);
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error reading disk serials: {}", e.what());
    }

    return serials;
}

std::vector<MemoryModuleInfo> LinuxSystemInfo::getMemoryModulesFromDmi() const {
    std::vector<MemoryModuleInfo> modules;

    // Try to use dmidecode if available
    std::string dmiOutput = executeCommand("dmidecode -t memory 2>/dev/null");
    if (!dmiOutput.empty()) {
        // Parse dmidecode output (simplified parsing)
        std::istringstream iss(dmiOutput);
        std::string line;
        MemoryModuleInfo currentModule;
        bool inMemoryDevice = false;

        while (std::getline(iss, line)) {
            line = trim(line);

            if (line.find("Memory Device") != std::string::npos) {
                if (inMemoryDevice && currentModule.isValid()) {
                    modules.push_back(currentModule);
                }
                currentModule = MemoryModuleInfo{};
                inMemoryDevice = true;
            } else if (inMemoryDevice) {
                if (line.find("Serial Number:") != std::string::npos) {
                    auto pos = line.find(":");
                    if (pos != std::string::npos) {
                        currentModule.serialNumber = validateSerial(trim(line.substr(pos + 1)));
                    }
                } else if (line.find("Manufacturer:") != std::string::npos) {
                    auto pos = line.find(":");
                    if (pos != std::string::npos) {
                        currentModule.manufacturer = trim(line.substr(pos + 1));
                    }
                } else if (line.find("Size:") != std::string::npos) {
                    auto pos = line.find(":");
                    if (pos != std::string::npos) {
                        currentModule.sizeBytes = parseMemorySize(trim(line.substr(pos + 1)));
                    }
                }
            }
        }

        if (inMemoryDevice && currentModule.isValid()) {
            modules.push_back(currentModule);
        }
    }

    return modules;
}

MemoryModuleInfo LinuxSystemInfo::getMemoryInfoFromProc() const {
    MemoryModuleInfo module;

    auto memInfo = parseKeyValuePairs(readFile("/proc/meminfo"));
    auto it = memInfo.find("MemTotal");
    if (it != memInfo.end()) {
        module.sizeBytes = parseMemorySize(it->second);
        module.serialNumber = "proc-meminfo"; // Placeholder
        module.manufacturer = "Unknown";
        module.type = "Unknown";
    }

    return module;
}

std::string LinuxSystemInfo::validateSerial(const std::string& serial) const {
    if (!SystemInfoUtils::isValidSerial(serial)) {
        return "";
    }

    // Additional Linux-specific validation
    std::string cleaned = trim(serial);

    // Remove common Linux placeholder values
    if (cleaned == "To Be Filled By O.E.M." ||
        cleaned == "System Serial Number" ||
        cleaned == "Default string" ||
        cleaned == "Not Specified" ||
        cleaned.find("OEM") != std::string::npos) {
        return "";
    }

    return cleaned;
}

bool LinuxSystemInfo::isFileReadable(const std::string& path) const {
    return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

bool LinuxSystemInfo::isDirectoryExists(const std::string& path) const {
    return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

std::string LinuxSystemInfo::trim(const std::string& str) const {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";

    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

uint64_t LinuxSystemInfo::parseMemorySize(const std::string& sizeStr) const {
    std::string cleaned = toLower(trim(sizeStr));

    // Extract numeric part
    std::string numStr;
    for (char c : cleaned) {
        if (std::isdigit(c) || c == '.') {
            numStr += c;
        } else {
            break;
        }
    }

    if (numStr.empty()) return 0;

    double value = std::stod(numStr);

    // Convert based on unit
    if (cleaned.find("kb") != std::string::npos) {
        return static_cast<uint64_t>(value * 1024);
    } else if (cleaned.find("mb") != std::string::npos) {
        return static_cast<uint64_t>(value * 1024 * 1024);
    } else if (cleaned.find("gb") != std::string::npos) {
        return static_cast<uint64_t>(value * 1024 * 1024 * 1024);
    }

    return static_cast<uint64_t>(value);
}

std::map<std::string, std::string> LinuxSystemInfo::parseKeyValuePairs(const std::string& text,
                                                                        const std::string& delimiter) const {
    std::map<std::string, std::string> result;
    std::istringstream iss(text);
    std::string line;

    while (std::getline(iss, line)) {
        auto pos = line.find(delimiter);
        if (pos != std::string::npos) {
            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + delimiter.length()));
            result[key] = value;
        }
    }

    return result;
}

std::string LinuxSystemInfo::toLower(const std::string& str) const {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::vector<std::string> LinuxSystemInfo::split(const std::string& str, char delimiter) const {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        result.push_back(trim(item));
    }

    return result;
}

uint32_t LinuxSystemInfo::parseMemorySpeed(const std::string& speedStr) const {
    std::string cleaned = trim(speedStr);

    // Extract numeric part
    std::string numStr;
    for (char c : cleaned) {
        if (std::isdigit(c)) {
            numStr += c;
        } else {
            break;
        }
    }

    if (numStr.empty()) return 0;

    try {
        return static_cast<uint32_t>(std::stoul(numStr));
    } catch (...) {
        return 0;
    }
}

std::vector<NetworkInterfaceInfo> LinuxSystemInfo::getNetworkInterfacesFromSys() const {
    std::vector<NetworkInterfaceInfo> interfaces;

    try {
        for (const auto& entry : std::filesystem::directory_iterator("/sys/class/net")) {
            if (entry.is_directory()) {
                std::string interfaceName = entry.path().filename().string();
                if (interfaceName != "lo") { // Skip loopback interface
                    auto interfaceInfo = getNetworkInterfaceDetails(interfaceName);
                    if (interfaceInfo.isValid()) {
                        interfaces.push_back(interfaceInfo);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error reading network interfaces: {}", e.what());
    }

    return interfaces;
}

NetworkInterfaceInfo LinuxSystemInfo::getNetworkInterfaceDetails(const std::string& interfaceName) const {
    NetworkInterfaceInfo info;
    info.name = interfaceName;

    std::string basePath = "/sys/class/net/" + interfaceName;

    // Get MAC address
    info.macAddress = readFile(basePath + "/address");

    // Get interface type
    std::string type = readFile(basePath + "/type");
    if (type == "1") {
        info.type = "Ethernet";
    } else if (type == "24") {
        info.type = "IEEE 802.11";
    } else {
        info.type = "Unknown";
    }

    // Check if interface is active
    std::string operstate = readFile(basePath + "/operstate");
    info.isActive = (operstate == "up");

    // Try to get driver information
    std::string driverPath = basePath + "/device/driver";
    if (std::filesystem::exists(driverPath) && std::filesystem::is_symlink(driverPath)) {
        auto driverTarget = std::filesystem::read_symlink(driverPath);
        info.driver = driverTarget.filename().string();
    }

    return info;
}

SystemInfoError LinuxSystemInfo::handleSystemError(const std::string& operation, int errorCode) {
    std::ostringstream oss;
    oss << "System error in " << operation;
    if (errorCode != 0) {
        oss << " (errno: " << errorCode << ")";
    }
    lastError_ = oss.str();

    if (errorCode == EACCES || errorCode == EPERM) {
        return SystemInfoError::ACCESS_DENIED;
    } else if (errorCode == ENOENT || errorCode == ENOTDIR) {
        return SystemInfoError::HARDWARE_NOT_FOUND;
    } else if (errorCode == ETIMEDOUT) {
        return SystemInfoError::TIMEOUT;
    } else {
        return SystemInfoError::UNKNOWN_ERROR;
    }
}

std::unique_ptr<LinuxSystemInfo> createLinuxSystemInfo() {
    return std::make_unique<LinuxSystemInfo>();
}

} // namespace atom::system

#endif // !_WIN32
