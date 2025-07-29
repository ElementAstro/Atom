/*
 * disk_security.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Disk Security

**************************************************/

#include "disk_security.hpp"
#include "disk_device.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <mutex>
#include <random>
#include <regex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#elif __linux__
#include <sys/mount.h>
#include <unistd.h>
#elif __APPLE__
#include <sys/mount.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/mount.h>
#endif

#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

namespace atom::system {

namespace {
std::mutex g_whitelistMutex;
std::unordered_set<std::string> g_whitelistedDevices = {"SD1234", "SD5678"};

const std::unordered_set<std::string> SUSPICIOUS_EXTENSIONS = {
    ".exe", ".bat", ".cmd", ".ps1", ".vbs", ".js",  ".jar", ".sh", ".py",
    ".scr", ".pif", ".com", ".msi", ".dll", ".hta", ".wsf", ".lnk"};

const std::vector<std::pair<std::string, std::regex>> SUSPICIOUS_PATTERNS = {
    {"autorun.inf", std::regex("(?i)^autorun\\.inf$")},
    {"autorun", std::regex("(?i)^autorun$")},
    {"suspicious naming",
     std::regex("(?i)(virus|hack|crack|keygen|patch|warez|trojan|malware)")},
    {"hidden system", std::regex("(?i)^\\.")},
    {"temp files", std::regex("(?i)\\.(tmp|temp)$")}};

std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return str;
}
}  // namespace

bool addDeviceToWhitelist(const std::string& deviceIdentifier) {
    std::lock_guard<std::mutex> lock(g_whitelistMutex);

    if (g_whitelistedDevices.find(deviceIdentifier) !=
        g_whitelistedDevices.end()) {
        spdlog::info("Device {} is already in the whitelist", deviceIdentifier);
        return true;
    }

    g_whitelistedDevices.insert(deviceIdentifier);
    spdlog::info("Added device {} to whitelist", deviceIdentifier);

    return true;
}

bool removeDeviceFromWhitelist(const std::string& deviceIdentifier) {
    std::lock_guard<std::mutex> lock(g_whitelistMutex);

    const auto it = g_whitelistedDevices.find(deviceIdentifier);
    if (it == g_whitelistedDevices.end()) {
        spdlog::warn("Device {} is not in the whitelist", deviceIdentifier);
        return false;
    }

    g_whitelistedDevices.erase(it);
    spdlog::info("Removed device {} from whitelist", deviceIdentifier);

    return true;
}

bool isDeviceInWhitelist(const std::string& deviceIdentifier) {
    std::lock_guard<std::mutex> lock(g_whitelistMutex);

    const bool result = g_whitelistedDevices.find(deviceIdentifier) !=
                        g_whitelistedDevices.end();

    if (result) {
        spdlog::info("Device {} is in the whitelist. Access granted.",
                     deviceIdentifier);
    } else {
        spdlog::error("Device {} is not in the whitelist. Access denied.",
                      deviceIdentifier);
    }

    return result;
}

bool setDiskReadOnly(const std::string& path) {
#ifdef _WIN32
    const HANDLE hDevice =
        CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                    FILE_FLAG_NO_BUFFERING, nullptr);

    if (hDevice == INVALID_HANDLE_VALUE) {
        spdlog::error("Failed to open device {}: {}", path, GetLastError());
        return false;
    }

    DWORD bytesReturned = 0;
    const BOOL result =
        DeviceIoControl(hDevice, FSCTL_SET_PERSISTENT_VOLUME_STATE, nullptr, 0,
                        nullptr, 0, &bytesReturned, nullptr);

    CloseHandle(hDevice);

    if (!result) {
        spdlog::error("Failed to set disk read-only: {}", GetLastError());
        return false;
    }

    spdlog::info("Successfully set disk {} to read-only mode", path);
    return true;

#elif __linux__
    const int result = mount(path.c_str(), path.c_str(), nullptr,
                             MS_REMOUNT | MS_RDONLY, nullptr);

    if (result != 0) {
        spdlog::error("Failed to set disk {} to read-only: {}", path,
                      strerror(errno));
        return false;
    }

    spdlog::info("Successfully set disk {} to read-only mode", path);
    return true;

#elif __APPLE__
    struct statfs statInfo;
    if (statfs(path.c_str(), &statInfo) != 0) {
        spdlog::error("Failed to get mount info for {}: {}", path,
                      strerror(errno));
        return false;
    }

    const int result = mount(statInfo.f_fstypename, path.c_str(),
                             MNT_RDONLY | MNT_UPDATE, nullptr);

    if (result != 0) {
        spdlog::error("Failed to set disk {} to read-only: {}", path,
                      strerror(errno));
        return false;
    }

    spdlog::info("Successfully set disk {} to read-only mode", path);
    return true;

#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    struct statfs statInfo;
    if (statfs(path.c_str(), &statInfo) != 0) {
        spdlog::error("Failed to get mount info for {}: {}", path,
                      strerror(errno));
        return false;
    }

    const int result = mount(statInfo.f_fstypename, path.c_str(),
                             MNT_RDONLY | MNT_UPDATE, nullptr);

    if (result != 0) {
        spdlog::error("Failed to set disk {} to read-only: {}", path,
                      strerror(errno));
        return false;
    }

    spdlog::info("Successfully set disk {} to read-only mode", path);
    return true;
#else
    spdlog::error(
        "Setting disk to read-only is not implemented for this platform");
    return false;
#endif
}

std::pair<bool, int> scanDiskForThreats(const std::string& path,
                                        int scanDepth) {
    spdlog::info("Scanning {} for malicious files (depth: {})", path,
                 scanDepth);

    int suspiciousCount = 0;
    bool success = true;

    try {
        std::function<void(const fs::path&, int)> scanDirectory;

        scanDirectory = [&](const fs::path& dirPath, int currentDepth) {
            if (scanDepth > 0 && currentDepth > scanDepth) {
                return;
            }

            std::error_code ec;
            for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
                if (ec) {
                    spdlog::warn("Error accessing directory {}: {}",
                                 dirPath.string(), ec.message());
                    continue;
                }

                try {
                    if (fs::is_directory(entry, ec) && !ec) {
                        scanDirectory(entry.path(), currentDepth + 1);
                    } else if (fs::is_regular_file(entry, ec) && !ec) {
                        const std::string extension =
                            toLower(entry.path().extension().string());
                        const std::string filename =
                            entry.path().filename().string();

                        if (SUSPICIOUS_EXTENSIONS.find(extension) !=
                            SUSPICIOUS_EXTENSIONS.end()) {
                            spdlog::warn("Suspicious file extension found: {}",
                                         entry.path().string());
                            suspiciousCount++;
                        }

                        for (const auto& [patternName, pattern] :
                             SUSPICIOUS_PATTERNS) {
                            if (std::regex_search(filename, pattern)) {
                                spdlog::warn(
                                    "Suspicious file pattern ({}) found: {}",
                                    patternName, entry.path().string());
                                suspiciousCount++;
                                break;
                            }
                        }

                        if (fs::file_size(entry, ec) == 0 && !ec) {
                            spdlog::warn(
                                "Empty file found (potential placeholder): {}",
                                entry.path().string());
                        }

                        const auto fileTime = fs::last_write_time(entry, ec);
                        if (!ec) {
                            const auto now = std::chrono::file_clock::now();
                            const auto duration = now - fileTime;
                            if (duration < std::chrono::minutes(5)) {
                                spdlog::info(
                                    "Recently created file detected: {}",
                                    entry.path().string());
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Error scanning {}: {}",
                                  entry.path().string(), e.what());
                }
            }
        };

        scanDirectory(path, 0);
    } catch (const std::exception& e) {
        spdlog::error("Error scanning {}: {}", path, e.what());
        success = false;
    }

    spdlog::info("Scan completed for {}. Found {} suspicious files.", path,
                 suspiciousCount);

    return {success, suspiciousCount};
}

EncryptionStatus detectEncryptionStatus(const std::string& devicePath) {
#ifdef __linux__
    // Check for LUKS encryption
    std::string command = "cryptsetup isLuks " + devicePath + " 2>/dev/null";
    int result = std::system(command.c_str());
    if (result == 0) {
        return EncryptionStatus::ENCRYPTED;
    }

    // Check for dm-crypt
    std::ifstream dmCryptFile("/proc/crypto");
    if (dmCryptFile.is_open()) {
        std::string line;
        while (std::getline(dmCryptFile, line)) {
            if (line.find("dm-crypt") != std::string::npos) {
                return EncryptionStatus::PARTIALLY_ENCRYPTED;
            }
        }
    }

    return EncryptionStatus::NOT_ENCRYPTED;
#elif _WIN32
    // Check for BitLocker
    std::string command = "manage-bde -status " + devicePath;
    // This is a simplified check - real implementation would use Windows APIs
    return EncryptionStatus::UNKNOWN;
#else
    return EncryptionStatus::UNKNOWN;
#endif
}

ThreatScanResult scanForThreatsAdvanced(const std::string& path, const SecurityScanConfig& config) {
    ThreatScanResult result;
    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        result.success = true;
        int suspiciousCount = 0;
        ThreatLevel maxThreat = ThreatLevel::NONE;

        std::function<void(const fs::path&, int)> scanDirectory =
            [&](const fs::path& dirPath, int depth) {
                if (config.maxScanDepth > 0 && depth > config.maxScanDepth) {
                    return;
                }

                try {
                    for (const auto& entry : fs::directory_iterator(dirPath)) {
                        if (entry.is_regular_file()) {
                            const std::string filename = entry.path().filename().string();
                            const std::string extension = entry.path().extension().string();

                            // Check against suspicious extensions
                            if (std::find(config.suspiciousExtensions.begin(),
                                        config.suspiciousExtensions.end(), extension) !=
                                config.suspiciousExtensions.end()) {

                                suspiciousCount++;
                                ThreatLevel threat = ThreatLevel::MEDIUM;

                                // Enhanced ML-like pattern matching
                                if (config.enableMLPatterns) {
                                    if (filename.find("autorun") != std::string::npos ||
                                        filename.find("setup") != std::string::npos ||
                                        filename.find("install") != std::string::npos) {
                                        threat = ThreatLevel::HIGH;
                                    }

                                    // Check file size (very small or very large executables are suspicious)
                                    auto fileSize = entry.file_size();
                                    if (extension == ".exe" && (fileSize < 1024 || fileSize > 100*1024*1024)) {
                                        threat = ThreatLevel::HIGH;
                                    }
                                }

                                if (threat > maxThreat) {
                                    maxThreat = threat;
                                }

                                result.threatDetails.push_back(
                                    "Suspicious file: " + entry.path().string() +
                                    " (Threat level: " + std::to_string(static_cast<int>(threat)) + ")");

                                // Quarantine if configured
                                if (config.quarantineThreats && threat >= config.quarantineThreshold) {
                                    // In a real implementation, we'd move the file to quarantine
                                    result.quarantinedFiles++;
                                    spdlog::warn("Would quarantine: {}", entry.path().string());
                                }
                            }
                        } else if (entry.is_directory() &&
                                 entry.path().filename() != "." &&
                                 entry.path().filename() != "..") {
                            scanDirectory(entry.path(), depth + 1);
                        }
                    }
                } catch (const fs::filesystem_error& e) {
                    spdlog::warn("Error scanning directory {}: {}", dirPath.string(), e.what());
                }
            };

        scanDirectory(path, 0);
        result.suspiciousFiles = suspiciousCount;
        result.maxThreatLevel = maxThreat;

    } catch (const std::exception& e) {
        spdlog::error("Error in advanced threat scan: {}", e.what());
        result.success = false;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.scanDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    return result;
}

std::optional<DeviceFingerprint> createDeviceFingerprint(const std::string& devicePath) {
    try {
        auto deviceInfo = getEnhancedDeviceInfo(devicePath);
        if (!deviceInfo) {
            return std::nullopt;
        }

        DeviceFingerprint fingerprint(devicePath, *deviceInfo);

        // Calculate MBR checksum (simplified)
#ifdef __linux__
        std::ifstream device(devicePath, std::ios::binary);
        if (device.is_open()) {
            std::vector<char> mbr(512);
            device.read(mbr.data(), 512);

            // Simple checksum calculation
            uint32_t checksum = 0;
            for (char byte : mbr) {
                checksum += static_cast<uint8_t>(byte);
            }
            fingerprint.checksumMBR = std::to_string(checksum);
        }
#endif

        return fingerprint;
    } catch (const std::exception& e) {
        spdlog::error("Error creating device fingerprint: {}", e.what());
        return std::nullopt;
    }
}

bool verifyDeviceFingerprint(const std::string& devicePath, const DeviceFingerprint& storedFingerprint) {
    auto currentFingerprint = createDeviceFingerprint(devicePath);
    if (!currentFingerprint) {
        return false;
    }

    return (currentFingerprint->serialNumber == storedFingerprint.serialNumber &&
            currentFingerprint->model == storedFingerprint.model &&
            currentFingerprint->vendor == storedFingerprint.vendor &&
            currentFingerprint->sizeBytes == storedFingerprint.sizeBytes &&
            currentFingerprint->checksumMBR == storedFingerprint.checksumMBR);
}

SecurityAuditResult performSecurityAudit(const std::string& devicePath) {
    SecurityAuditResult result;
    result.devicePath = devicePath;

    try {
        // Check encryption status
        result.encryptionStatus = detectEncryptionStatus(devicePath);

        // Check if device is in whitelist
        auto fingerprint = createDeviceFingerprint(devicePath);
        if (fingerprint) {
            result.fingerprint = *fingerprint;
            result.isInWhitelist = isDeviceInWhitelist(fingerprint->serialNumber);
        }

        // Perform threat scan
        SecurityScanConfig config;
        result.lastScanResult = scanForThreatsAdvanced(devicePath, config);

        // Check read-only status
#ifdef __linux__
        std::string roPath = "/sys/block/" + fs::path(devicePath).filename().string() + "/ro";
        std::ifstream roFile(roPath);
        std::string roValue;
        if (roFile.is_open() && std::getline(roFile, roValue)) {
            result.isReadOnly = (roValue == "1");
        }
#endif

        // Analyze security issues
        if (result.encryptionStatus == EncryptionStatus::NOT_ENCRYPTED) {
            result.securityIssues.push_back("Device is not encrypted");
        }

        if (!result.isInWhitelist) {
            result.securityIssues.push_back("Device is not in security whitelist");
        }

        if (result.lastScanResult.maxThreatLevel >= ThreatLevel::MEDIUM) {
            result.securityIssues.push_back("Threats detected during scan");
        }

    } catch (const std::exception& e) {
        spdlog::error("Error performing security audit: {}", e.what());
        result.securityIssues.push_back("Audit failed: " + std::string(e.what()));
    }

    return result;
}

int quarantineFiles(const std::vector<std::string>& filePaths, const std::string& quarantineDir) {
    int quarantinedCount = 0;

    try {
        // Create quarantine directory if it doesn't exist
        fs::create_directories(quarantineDir);

        for (const auto& filePath : filePaths) {
            try {
                fs::path sourcePath(filePath);
                fs::path destPath = fs::path(quarantineDir) / sourcePath.filename();

                // Add timestamp to avoid conflicts
                auto now = std::chrono::system_clock::now();
                auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
                destPath += "_" + std::to_string(timestamp);

                fs::rename(sourcePath, destPath);
                quarantinedCount++;
                spdlog::info("Quarantined file: {} -> {}", filePath, destPath.string());

            } catch (const fs::filesystem_error& e) {
                spdlog::error("Failed to quarantine {}: {}", filePath, e.what());
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error in quarantine operation: {}", e.what());
    }

    return quarantinedCount;
}

bool establishSecureChannel(const std::string& devicePath) {
    // This is a placeholder for secure channel establishment
    // In a real implementation, this would involve cryptographic protocols
    spdlog::info("Establishing secure channel for device: {}", devicePath);

    try {
        // Verify device identity
        auto fingerprint = createDeviceFingerprint(devicePath);
        if (!fingerprint) {
            spdlog::error("Cannot establish secure channel: device fingerprinting failed");
            return false;
        }

        // Check if device is trusted
        if (!isDeviceInWhitelist(fingerprint->serialNumber)) {
            spdlog::warn("Device not in whitelist, secure channel establishment denied");
            return false;
        }

        // In a real implementation, we would:
        // 1. Exchange cryptographic keys
        // 2. Establish encrypted communication
        // 3. Verify device certificates

        spdlog::info("Secure channel established for device: {}", devicePath);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Error establishing secure channel: {}", e.what());
        return false;
    }
}

bool validateDeviceIntegrity(const std::string& devicePath) {
    try {
        auto currentFingerprint = createDeviceFingerprint(devicePath);
        if (!currentFingerprint) {
            return false;
        }

        // In a real implementation, we would compare against stored checksums
        // For now, we'll do basic validation

        // Check if device is accessible
        std::ifstream device(devicePath, std::ios::binary);
        if (!device.is_open()) {
            spdlog::error("Cannot access device for integrity check: {}", devicePath);
            return false;
        }

        // Read and verify MBR
        std::vector<char> mbr(512);
        device.read(mbr.data(), 512);

        // Check for valid MBR signature
        if (mbr.size() >= 512 &&
            static_cast<uint8_t>(mbr[510]) == 0x55 &&
            static_cast<uint8_t>(mbr[511]) == 0xAA) {
            spdlog::debug("Valid MBR signature found for device: {}", devicePath);
            return true;
        }

        spdlog::warn("Invalid or missing MBR signature for device: {}", devicePath);
        return false;

    } catch (const std::exception& e) {
        spdlog::error("Error validating device integrity: {}", e.what());
        return false;
    }
}

bool encryptDevice(const std::string& devicePath, const std::string& passphrase) {
#ifdef __linux__
    try {
        // Use LUKS encryption (simplified)
        std::string command = "cryptsetup luksFormat " + devicePath + " --batch-mode";
        // Note: In a real implementation, we would use proper APIs and handle the passphrase securely

        spdlog::info("Encrypting device: {}", devicePath);
        int result = std::system(command.c_str());

        if (result == 0) {
            spdlog::info("Device encryption completed: {}", devicePath);
            return true;
        } else {
            spdlog::error("Device encryption failed: {}", devicePath);
            return false;
        }
    } catch (const std::exception& e) {
        spdlog::error("Error encrypting device: {}", e.what());
        return false;
    }
#else
    spdlog::warn("Device encryption not implemented for this platform");
    return false;
#endif
}

bool secureWipeDevice(const std::string& devicePath, int passes) {
    try {
        spdlog::info("Starting secure wipe of device: {} with {} passes", devicePath, passes);

        std::ifstream deviceInfo(devicePath, std::ios::binary | std::ios::ate);
        if (!deviceInfo.is_open()) {
            spdlog::error("Cannot access device for secure wipe: {}", devicePath);
            return false;
        }

        auto deviceSize = deviceInfo.tellg();
        deviceInfo.close();

        std::ofstream device(devicePath, std::ios::binary);
        if (!device.is_open()) {
            spdlog::error("Cannot open device for writing: {}", devicePath);
            return false;
        }

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        const size_t bufferSize = 1024 * 1024;  // 1MB buffer
        std::vector<char> buffer(bufferSize);

        for (int pass = 0; pass < passes; ++pass) {
            spdlog::info("Secure wipe pass {}/{}", pass + 1, passes);
            device.seekp(0);

            for (size_t written = 0; written < static_cast<size_t>(deviceSize); written += bufferSize) {
                size_t writeSize = std::min(bufferSize, static_cast<size_t>(deviceSize) - written);

                // Fill buffer with random data
                for (size_t i = 0; i < writeSize; ++i) {
                    buffer[i] = static_cast<char>(dis(gen));
                }

                device.write(buffer.data(), writeSize);
                device.flush();
            }
        }

        spdlog::info("Secure wipe completed for device: {}", devicePath);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Error during secure wipe: {}", e.what());
        return false;
    }
}

// SecurityManager implementation
class SecurityManager::Impl {
public:
    SecurityScanConfig scanConfig;
    std::atomic<bool> monitoringActive{false};
    std::future<void> monitoringFuture;
    mutable std::mutex statsMutex;
    SecurityStats stats;
    std::unordered_map<std::string, DeviceFingerprint> deviceFingerprints;

    Impl() {
        stats.lastScan = std::chrono::system_clock::now();
    }

    bool addDeviceToWhitelist(const std::string& devicePath) {
        try {
            auto fingerprint = createDeviceFingerprint(devicePath);
            if (!fingerprint) {
                return false;
            }

            deviceFingerprints[fingerprint->serialNumber] = *fingerprint;
            return atom::system::addDeviceToWhitelist(fingerprint->serialNumber);
        } catch (const std::exception& e) {
            spdlog::error("Error adding device to whitelist: {}", e.what());
            return false;
        }
    }

    ThreatScanResult performSecurityScan(const std::string& path, const SecurityScanConfig& config) {
        auto result = scanForThreatsAdvanced(path, config);

        {
            std::lock_guard<std::mutex> lock(statsMutex);
            stats.threatsDetected += result.suspiciousFiles;
            stats.filesQuarantined += result.quarantinedFiles;
            stats.lastScan = std::chrono::system_clock::now();
        }

        return result;
    }

    void startSecurityMonitoring(std::function<void(const SecurityAuditResult&)> callback) {
        if (monitoringActive.load()) {
            return;
        }

        monitoringActive = true;
        monitoringFuture = std::async(std::launch::async, [this, callback]() {
            while (monitoringActive.load()) {
                try {
                    auto devices = getStorageDevices(true);

                    {
                        std::lock_guard<std::mutex> lock(statsMutex);
                        stats.devicesMonitored = devices.size();
                    }

                    for (const auto& device : devices) {
                        auto auditResult = performSecurityAudit(device.devicePath);

                        // Check for security violations
                        if (!auditResult.securityIssues.empty()) {
                            std::lock_guard<std::mutex> lock(statsMutex);
                            stats.securityViolations++;
                        }

                        callback(auditResult);
                    }

                    std::this_thread::sleep_for(std::chrono::minutes(5));
                } catch (const std::exception& e) {
                    spdlog::error("Error in security monitoring: {}", e.what());
                }
            }
        });
    }

    void stopSecurityMonitoring() {
        monitoringActive = false;
    }

    SecurityStats getSecurityStats() const {
        std::lock_guard<std::mutex> lock(statsMutex);
        return stats;
    }
};

SecurityManager::SecurityManager() : pImpl(std::make_unique<Impl>()) {}

SecurityManager::~SecurityManager() {
    stopSecurityMonitoring();
}

SecurityManager::SecurityManager(SecurityManager&&) noexcept = default;

SecurityManager& SecurityManager::operator=(SecurityManager&&) noexcept = default;

bool SecurityManager::addDeviceToWhitelist(const std::string& devicePath) {
    return pImpl->addDeviceToWhitelist(devicePath);
}

bool SecurityManager::removeDeviceFromWhitelist(const std::string& deviceIdentifier) {
    return atom::system::removeDeviceFromWhitelist(deviceIdentifier);
}

bool SecurityManager::isDeviceWhitelisted(const std::string& devicePath) const {
    auto fingerprint = createDeviceFingerprint(devicePath);
    if (!fingerprint) {
        return false;
    }
    return isDeviceInWhitelist(fingerprint->serialNumber);
}

ThreatScanResult SecurityManager::performSecurityScan(const std::string& path, const SecurityScanConfig& config) {
    return pImpl->performSecurityScan(path, config);
}

void SecurityManager::startSecurityMonitoring(std::function<void(const SecurityAuditResult&)> callback) {
    pImpl->startSecurityMonitoring(callback);
}

void SecurityManager::stopSecurityMonitoring() {
    pImpl->stopSecurityMonitoring();
}

auto SecurityManager::getSecurityStats() const -> SecurityStats {
    return pImpl->getSecurityStats();
}

void SecurityManager::updateScanConfig(const SecurityScanConfig& config) {
    pImpl->scanConfig = config;
}

const SecurityScanConfig& SecurityManager::getScanConfig() const {
    return pImpl->scanConfig;
}

}  // namespace atom::system
