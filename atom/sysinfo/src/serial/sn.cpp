#include "sn.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <filesystem>

#ifdef _WIN32
#include "platform/windows.hpp"
#else
#include "platform/linux.hpp"
#endif

namespace atom::system {

class SystemInfo::Impl {
public:
    SystemInfoConfig config_;
    std::chrono::system_clock::time_point lastCacheUpdate_;
    std::string lastError_;
    mutable std::mutex cacheMutex_;
    
    // Cached data
    std::optional<HardwareSerialData> cachedHardwareSerials_;
    std::optional<SystemIdentificationData> cachedSystemId_;
    std::optional<std::vector<MemoryModuleInfo>> cachedMemoryModules_;
    std::optional<std::vector<NetworkInterfaceInfo>> cachedNetworkInterfaces_;
    std::optional<ComprehensiveSystemInfo> cachedComprehensiveInfo_;
    
    // Platform-specific implementation
#ifdef _WIN32
    std::unique_ptr<WindowsSystemInfo> platformImpl_;
#else
    std::unique_ptr<LinuxSystemInfo> platformImpl_;
#endif

    Impl(const SystemInfoConfig& config) : config_(config) {
        initializePlatformImpl();
    }

    void initializePlatformImpl() {
#ifdef _WIN32
        platformImpl_ = createWindowsSystemInfo();
#else
        platformImpl_ = createLinuxSystemInfo();
#endif
        spdlog::debug("Platform-specific implementation initialized");
    }

    bool shouldRefreshCache(bool forceRefresh) const {
        if (forceRefresh || !config_.cacheResults) {
            return true;
        }
        
        auto now = std::chrono::system_clock::now();
        auto cacheAge = std::chrono::duration_cast<std::chrono::seconds>(now - lastCacheUpdate_);
        return cacheAge > config_.cacheTimeout;
    }

    void updateCacheTimestamp() {
        lastCacheUpdate_ = std::chrono::system_clock::now();
    }

    template<typename T>
    SystemInfoResult<T> getCachedOrFetch(
        std::optional<T>& cache,
        std::function<SystemInfoResult<T>()> fetchFunc,
        bool forceRefresh) {
        
        std::lock_guard<std::mutex> lock(cacheMutex_);
        
        if (!shouldRefreshCache(forceRefresh) && cache.has_value()) {
            SystemInfoResult<T> result;
            result.data = cache.value();
            result.success = true;
            return result;
        }
        
        auto result = fetchFunc();
        if (result.success && config_.cacheResults) {
            cache = result.data;
            updateCacheTimestamp();
        }
        
        if (!result.success) {
            lastError_ = result.errorMessage;
        }
        
        return result;
    }
};

SystemInfo::SystemInfo() : SystemInfo(SystemInfoConfig{}) {}

SystemInfo::SystemInfo(const SystemInfoConfig& config) 
    : impl_(std::make_unique<Impl>(config)) {
    spdlog::debug("SystemInfo instance created with config");
}

SystemInfo::~SystemInfo() {
    spdlog::debug("SystemInfo instance destroyed");
}

SystemInfo::SystemInfo(SystemInfo&& other) noexcept 
    : impl_(std::move(other.impl_)) {
    spdlog::debug("SystemInfo move constructor called");
}

SystemInfo& SystemInfo::operator=(SystemInfo&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
        spdlog::debug("SystemInfo move assignment performed");
    }
    return *this;
}

SystemInfoResult<HardwareSerialData> SystemInfo::getHardwareSerials(bool forceRefresh) {
    return impl_->getCachedOrFetch<HardwareSerialData>(
        impl_->cachedHardwareSerials_,
        [this]() { return impl_->platformImpl_->getHardwareSerials(); },
        forceRefresh
    );
}

SystemInfoResult<SystemIdentificationData> SystemInfo::getSystemIdentification(bool forceRefresh) {
    return impl_->getCachedOrFetch<SystemIdentificationData>(
        impl_->cachedSystemId_,
        [this]() { return impl_->platformImpl_->getSystemIdentification(); },
        forceRefresh
    );
}

SystemInfoResult<std::vector<MemoryModuleInfo>> SystemInfo::getMemoryModules(bool forceRefresh) {
    return impl_->getCachedOrFetch<std::vector<MemoryModuleInfo>>(
        impl_->cachedMemoryModules_,
        [this]() { return impl_->platformImpl_->getMemoryModules(); },
        forceRefresh
    );
}

SystemInfoResult<std::vector<NetworkInterfaceInfo>> SystemInfo::getNetworkInterfaces(bool forceRefresh) {
    return impl_->getCachedOrFetch<std::vector<NetworkInterfaceInfo>>(
        impl_->cachedNetworkInterfaces_,
        [this]() { return impl_->platformImpl_->getNetworkInterfaces(); },
        forceRefresh
    );
}

SystemInfoResult<ComprehensiveSystemInfo> SystemInfo::getComprehensiveInfo(bool forceRefresh) {
    return impl_->getCachedOrFetch<ComprehensiveSystemInfo>(
        impl_->cachedComprehensiveInfo_,
        [this]() { return impl_->platformImpl_->getComprehensiveInfo(impl_->config_); },
        forceRefresh
    );
}

SystemInfoResult<SystemIdQuery> SystemInfo::querySystemId(SystemIdType type, bool forceRefresh) {
    // System ID queries are not cached as they are typically one-off requests
    auto result = impl_->platformImpl_->querySystemId(type);
    if (!result.success) {
        impl_->lastError_ = result.errorMessage;
    }
    return result;
}

std::string SystemInfo::getSystemFingerprint(bool forceRefresh) const {
    // For const method, we need to cast away const to call non-const methods
    auto* nonConstThis = const_cast<SystemInfo*>(this);
    auto result = nonConstThis->getComprehensiveInfo(forceRefresh);
    if (result.success) {
        return result.data.getSystemFingerprint();
    }
    return "";
}

bool SystemInfo::isSupported() const {
#ifdef _WIN32
    return impl_->platformImpl_->isWmiAvailable();
#else
    return impl_->platformImpl_->isDmiAvailable();
#endif
}

std::string SystemInfo::getPlatformName() const {
#ifdef _WIN32
    return "Windows";
#else
    return "Linux";
#endif
}

std::string SystemInfo::getLastError() const {
    return impl_->lastError_;
}

void SystemInfo::updateConfig(const SystemInfoConfig& config) {
    std::lock_guard<std::mutex> lock(impl_->cacheMutex_);
    impl_->config_ = config;
    
    // Clear cache if caching is disabled
    if (!config.cacheResults) {
        clearCache();
    }
}

const SystemInfoConfig& SystemInfo::getConfig() const {
    return impl_->config_;
}

void SystemInfo::clearCache() {
    std::lock_guard<std::mutex> lock(impl_->cacheMutex_);
    impl_->cachedHardwareSerials_.reset();
    impl_->cachedSystemId_.reset();
    impl_->cachedMemoryModules_.reset();
    impl_->cachedNetworkInterfaces_.reset();
    impl_->cachedComprehensiveInfo_.reset();
    spdlog::debug("System info cache cleared");
}

bool SystemInfo::isCacheValid() const {
    std::lock_guard<std::mutex> lock(impl_->cacheMutex_);
    return !impl_->shouldRefreshCache(false);
}

std::chrono::seconds SystemInfo::getCacheAge() const {
    std::lock_guard<std::mutex> lock(impl_->cacheMutex_);
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - impl_->lastCacheUpdate_);
}

bool SystemInfo::refreshAll() {
    try {
        clearCache();
        
        auto hardwareResult = getHardwareSerials(true);
        auto systemIdResult = getSystemIdentification(true);
        auto memoryResult = getMemoryModules(true);
        auto networkResult = getNetworkInterfaces(true);
        auto comprehensiveResult = getComprehensiveInfo(true);
        
        return hardwareResult.success || systemIdResult.success || 
               memoryResult.success || networkResult.success || 
               comprehensiveResult.success;
    } catch (const std::exception& e) {
        impl_->lastError_ = e.what();
        return false;
    }
}

std::string SystemInfo::getSummary() const {
    std::ostringstream oss;
    oss << "=== System Information Summary ===\n";
    oss << "Platform: " << getPlatformName() << "\n";
    oss << "Supported: " << (isSupported() ? "Yes" : "No") << "\n";
    oss << "Cache Valid: " << (isCacheValid() ? "Yes" : "No") << "\n";
    oss << "Cache Age: " << getCacheAge().count() << " seconds\n";
    
    auto fingerprint = getSystemFingerprint(false);
    if (!fingerprint.empty()) {
        oss << "System Fingerprint: " << fingerprint.substr(0, 16) << "...\n";
    }
    
    return oss.str();
}

bool SystemInfo::validateIntegrity() const {
    try {
        auto* nonConstThis = const_cast<SystemInfo*>(this);
        auto result = nonConstThis->getComprehensiveInfo(false);
        return result.success && result.data.isValid();
    } catch (...) {
        return false;
    }
}

std::map<std::string, std::string> SystemInfo::getCollectionStats() const {
    std::map<std::string, std::string> stats;
    
    stats["platform"] = getPlatformName();
    stats["supported"] = isSupported() ? "true" : "false";
    stats["cache_valid"] = isCacheValid() ? "true" : "false";
    stats["cache_age_seconds"] = std::to_string(getCacheAge().count());
    stats["cache_timeout_seconds"] = std::to_string(impl_->config_.cacheTimeout.count());
    stats["caching_enabled"] = impl_->config_.cacheResults ? "true" : "false";
    stats["logging_enabled"] = impl_->config_.enableLogging ? "true" : "false";
    
    return stats;
}

// Factory functions
std::unique_ptr<SystemInfo> createSystemInfo(const SystemInfoConfig& config) {
    return std::make_unique<SystemInfo>(config);
}

std::string getQuickSystemFingerprint() {
    try {
        SystemInfoConfig config;
        config.cacheResults = false; // Don't cache for quick access
        auto sysInfo = createSystemInfo(config);
        return sysInfo->getSystemFingerprint(true);
    } catch (...) {
        return "";
    }
}

bool isSystemInfoAvailable() {
    try {
        SystemInfoConfig config;
        config.cacheResults = false;
        auto sysInfo = createSystemInfo(config);
        return sysInfo->isSupported();
    } catch (...) {
        return false;
    }
}

std::map<std::string, bool> getPlatformCapabilities() {
    std::map<std::string, bool> capabilities;

#ifdef _WIN32
    capabilities["wmi_access"] = true;
    capabilities["registry_access"] = true;
    capabilities["hardware_serials"] = true;
    capabilities["memory_modules"] = true;
    capabilities["network_interfaces"] = true;
    capabilities["system_uuid"] = true;
    capabilities["machine_guid"] = true;
#else
    capabilities["dmi_access"] = std::filesystem::exists("/sys/class/dmi/id");
    capabilities["proc_access"] = std::filesystem::exists("/proc");
    capabilities["sys_access"] = std::filesystem::exists("/sys");
    capabilities["hardware_serials"] = capabilities["dmi_access"];
    capabilities["memory_modules"] = capabilities["dmi_access"];
    capabilities["network_interfaces"] = capabilities["sys_access"];
    capabilities["system_uuid"] = capabilities["dmi_access"];
    capabilities["machine_id"] = std::filesystem::exists("/etc/machine-id");
#endif

    return capabilities;
}

std::string SystemInfo::exportToJson(bool includeAll) const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"platform\": \"" << getPlatformName() << "\",\n";
    json << "  \"timestamp\": \"" << SystemInfoUtils::getCurrentTimestamp() << "\",\n";
    json << "  \"fingerprint\": \"" << getSystemFingerprint(false) << "\",\n";

    if (includeAll) {
        auto* nonConstThis = const_cast<SystemInfo*>(this);
        auto result = nonConstThis->getComprehensiveInfo(false);
        if (result.success) {
            json << "  \"hardware_serials\": {\n";
            json << "    \"bios\": \"" << result.data.hardwareSerials.biosSerial << "\",\n";
            json << "    \"motherboard\": \"" << result.data.hardwareSerials.motherboardSerial << "\",\n";
            json << "    \"cpu\": \"" << result.data.hardwareSerials.cpuSerial << "\"\n";
            json << "  },\n";

            json << "  \"system_id\": {\n";
            json << "    \"uuid\": \"" << result.data.systemId.systemUuid << "\",\n";
            json << "    \"machine_id\": \"" << result.data.systemId.machineId << "\",\n";
            json << "    \"hostname\": \"" << result.data.systemId.hostname << "\"\n";
            json << "  }\n";
        }
    }

    json << "}";
    return json.str();
}

std::string SystemInfo::exportToXml(bool includeAll) const {
    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<system_info>\n";
    xml << "  <platform>" << getPlatformName() << "</platform>\n";
    xml << "  <timestamp>" << SystemInfoUtils::getCurrentTimestamp() << "</timestamp>\n";
    xml << "  <fingerprint>" << getSystemFingerprint(false) << "</fingerprint>\n";

    if (includeAll) {
        auto* nonConstThis = const_cast<SystemInfo*>(this);
        auto result = nonConstThis->getComprehensiveInfo(false);
        if (result.success) {
            xml << "  <hardware_serials>\n";
            xml << "    <bios>" << result.data.hardwareSerials.biosSerial << "</bios>\n";
            xml << "    <motherboard>" << result.data.hardwareSerials.motherboardSerial << "</motherboard>\n";
            xml << "    <cpu>" << result.data.hardwareSerials.cpuSerial << "</cpu>\n";
            xml << "  </hardware_serials>\n";

            xml << "  <system_id>\n";
            xml << "    <uuid>" << result.data.systemId.systemUuid << "</uuid>\n";
            xml << "    <machine_id>" << result.data.systemId.machineId << "</machine_id>\n";
            xml << "    <hostname>" << result.data.systemId.hostname << "</hostname>\n";
            xml << "  </system_id>\n";
        }
    }

    xml << "</system_info>";
    return xml.str();
}

// SystemInfoHelpers implementations
namespace SystemInfoHelpers {

double compareFingerprints(const std::string& fingerprint1, const std::string& fingerprint2) {
    if (fingerprint1.empty() || fingerprint2.empty()) {
        return 0.0;
    }

    if (fingerprint1 == fingerprint2) {
        return 1.0;
    }

    // Simple similarity based on common characters
    size_t commonChars = 0;
    size_t maxLength = std::max(fingerprint1.length(), fingerprint2.length());
    size_t minLength = std::min(fingerprint1.length(), fingerprint2.length());

    for (size_t i = 0; i < minLength; ++i) {
        if (fingerprint1[i] == fingerprint2[i]) {
            commonChars++;
        }
    }

    return static_cast<double>(commonChars) / static_cast<double>(maxLength);
}

ComprehensiveSystemInfo anonymizeSystemInfo(const ComprehensiveSystemInfo& info) {
    ComprehensiveSystemInfo anonymized = info;

    // Anonymize serial numbers
    if (!anonymized.hardwareSerials.biosSerial.empty()) {
        anonymized.hardwareSerials.biosSerial = "BIOS_" + SystemInfoUtils::generateHash(anonymized.hardwareSerials.biosSerial).substr(0, 8);
    }
    if (!anonymized.hardwareSerials.motherboardSerial.empty()) {
        anonymized.hardwareSerials.motherboardSerial = "MB_" + SystemInfoUtils::generateHash(anonymized.hardwareSerials.motherboardSerial).substr(0, 8);
    }
    if (!anonymized.hardwareSerials.cpuSerial.empty()) {
        anonymized.hardwareSerials.cpuSerial = "CPU_" + SystemInfoUtils::generateHash(anonymized.hardwareSerials.cpuSerial).substr(0, 8);
    }

    // Anonymize system identification
    if (!anonymized.systemId.hostname.empty()) {
        anonymized.systemId.hostname = "HOST_" + SystemInfoUtils::generateHash(anonymized.systemId.hostname).substr(0, 8);
    }
    if (!anonymized.systemId.domainName.empty()) {
        anonymized.systemId.domainName = "DOMAIN_" + SystemInfoUtils::generateHash(anonymized.systemId.domainName).substr(0, 8);
    }

    // Anonymize MAC addresses
    for (auto& mac : anonymized.systemId.macAddresses) {
        if (!mac.empty()) {
            mac = "MAC_" + SystemInfoUtils::generateHash(mac).substr(0, 12);
        }
    }

    return anonymized;
}

std::string generateHardwareLicenseKey(const ComprehensiveSystemInfo& info, const std::string& salt) {
    std::ostringstream keyData;
    keyData << info.hardwareSerials.biosSerial << "|";
    keyData << info.hardwareSerials.motherboardSerial << "|";
    keyData << info.hardwareSerials.cpuSerial << "|";
    keyData << info.systemId.systemUuid << "|";
    keyData << salt;

    return SystemInfoUtils::generateHash(keyData.str());
}

bool validateHardwareLicenseKey(const std::string& licenseKey,
                                const ComprehensiveSystemInfo& info,
                                const std::string& salt) {
    std::string expectedKey = generateHardwareLicenseKey(info, salt);
    return licenseKey == expectedKey;
}

std::string getSystemChangeHash(const ComprehensiveSystemInfo& info) {
    std::ostringstream changeData;
    changeData << info.hardwareSerials.biosSerial << "|";
    changeData << info.hardwareSerials.motherboardSerial << "|";
    changeData << info.hardwareSerials.cpuSerial << "|";
    changeData << info.systemId.systemUuid << "|";
    changeData << info.memoryModules.size() << "|";
    changeData << info.networkInterfaces.size();

    return SystemInfoUtils::generateHash(changeData.str());
}

bool detectSystemChanges(const std::string& oldHash, const std::string& newHash) {
    return oldHash != newHash;
}

} // namespace SystemInfoHelpers

} // namespace atom::system
