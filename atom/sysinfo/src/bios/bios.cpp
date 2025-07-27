#include "bios.hpp"
#include "common.hpp"
#include <spdlog/spdlog.h>
#include <chrono>

namespace atom::system {

BiosInfo::BiosInfo() {
    impl_ = createBiosImplementation();
    if (!impl_) {
        throw std::runtime_error("Failed to create platform-specific BIOS implementation");
    }
}

BiosInfo& BiosInfo::getInstance() {
    static BiosInfo instance;
    return instance;
}

const BiosInfoData& BiosInfo::getBiosInfo(bool forceUpdate) {
    auto now = std::chrono::system_clock::now();
    if (forceUpdate || (now - cacheTime_) > CACHE_DURATION) {
        refreshBiosInfo();
    }
    return cachedInfo_;
}

bool BiosInfo::refreshBiosInfo() {
    try {
        cachedInfo_ = impl_->fetchBiosInfo();
        cacheTime_ = std::chrono::system_clock::now();
        return cachedInfo_.isValid();
    } catch (const std::exception& e) {
        spdlog::error("Failed to refresh BIOS info: {}", e.what());
        return false;
    }
}

BiosHealthStatus BiosInfo::checkHealth() const {
    if (!impl_) {
        BiosHealthStatus status;
        status.isHealthy = false;
        status.errors.push_back("BIOS implementation not available");
        return status;
    }
    return impl_->checkHealth();
}

BiosUpdateInfo BiosInfo::checkForUpdates() const {
    if (!impl_) {
        BiosUpdateInfo info;
        info.updateAvailable = false;
        return info;
    }
    return impl_->checkForUpdates();
}

std::vector<std::string> BiosInfo::getSMBIOSData() const {
    if (!impl_) {
        return {};
    }
    return impl_->getSMBIOSData();
}

bool BiosInfo::setSecureBoot(bool enable) {
    if (!impl_) {
        spdlog::error("BIOS implementation not available");
        return false;
    }
    return impl_->setSecureBoot(enable);
}

bool BiosInfo::setUEFIBoot(bool enable) {
    if (!impl_) {
        spdlog::error("BIOS implementation not available");
        return false;
    }
    return impl_->setUEFIBoot(enable);
}

bool BiosInfo::backupBiosSettings(const std::string& filepath) {
    if (!impl_) {
        spdlog::error("BIOS implementation not available");
        return false;
    }
    return impl_->backupBiosSettings(filepath);
}

bool BiosInfo::restoreBiosSettings(const std::string& filepath) {
    if (!impl_) {
        spdlog::error("BIOS implementation not available");
        return false;
    }
    return impl_->restoreBiosSettings(filepath);
}

bool BiosInfo::isSecureBootSupported() const {
    if (!impl_) {
        return false;
    }
    return impl_->isSecureBootSupported();
}

bool BiosInfo::isUEFIBootSupported() const {
    if (!impl_) {
        return false;
    }
    return impl_->isUEFIBootSupported();
}

// Enhanced features implementation

FirmwareInfo BiosInfo::getFirmwareInfo() const {
    if (!impl_) {
        return FirmwareInfo{};
    }
    return impl_->getFirmwareInfo();
}

BootConfiguration BiosInfo::getBootConfiguration() const {
    if (!impl_) {
        return BootConfiguration{};
    }
    return impl_->getBootConfiguration();
}

BiosSecuritySettings BiosInfo::getSecuritySettings() const {
    if (!impl_) {
        return BiosSecuritySettings{};
    }
    return impl_->getSecuritySettings();
}

BiosOperationResult BiosInfo::setBootOrder(const std::vector<std::string>& order) {
    if (!impl_) {
        spdlog::error("BIOS implementation not available");
        return BiosOperationResult::FAILED;
    }
    return impl_->setBootOrder(order);
}

BiosOperationResult BiosInfo::enableVirtualization(bool enable) {
    if (!impl_) {
        spdlog::error("BIOS implementation not available");
        return BiosOperationResult::FAILED;
    }
    return impl_->enableVirtualization(enable);
}

BiosOperationResult BiosInfo::enableHyperThreading(bool enable) {
    if (!impl_) {
        spdlog::error("BIOS implementation not available");
        return BiosOperationResult::FAILED;
    }
    return impl_->enableHyperThreading(enable);
}

std::vector<std::string> BiosInfo::getAvailableBootDevices() const {
    if (!impl_) {
        return {};
    }
    return impl_->getAvailableBootDevices();
}

bool BiosInfo::validateBiosIntegrity() const {
    if (!impl_) {
        return false;
    }
    return impl_->validateBiosIntegrity();
}

std::string BiosInfo::operationResultToString(BiosOperationResult result) {
    switch (result) {
        case BiosOperationResult::SUCCESS:
            return "Operation completed successfully";
        case BiosOperationResult::FAILED:
            return "Operation failed";
        case BiosOperationResult::NOT_SUPPORTED:
            return "Operation not supported on this platform";
        case BiosOperationResult::INSUFFICIENT_PRIVILEGES:
            return "Insufficient privileges to perform operation";
        case BiosOperationResult::REBOOT_REQUIRED:
            return "Operation completed, system reboot required";
        default:
            return "Unknown operation result";
    }
}

// Additional enhanced features implementation

PowerManagementSettings BiosInfo::getPowerManagementSettings() const {
    if (!impl_) {
        return PowerManagementSettings{};
    }
    return impl_->getPowerManagementSettings();
}

BiosOperationResult BiosInfo::setPowerManagementSettings(const PowerManagementSettings& settings) {
    if (!impl_) {
        spdlog::error("BIOS implementation not available");
        return BiosOperationResult::FAILED;
    }
    return impl_->setPowerManagementSettings(settings);
}

OverclockingInfo BiosInfo::getOverclockingInfo() const {
    if (!impl_) {
        return OverclockingInfo{};
    }
    return impl_->getOverclockingInfo();
}

BiosOperationResult BiosInfo::setOverclockingProfile(const std::string& profile) {
    if (!impl_) {
        spdlog::error("BIOS implementation not available");
        return BiosOperationResult::FAILED;
    }
    return impl_->setOverclockingProfile(profile);
}

HardwareMonitoring BiosInfo::getHardwareMonitoring() const {
    if (!impl_) {
        return HardwareMonitoring{};
    }
    return impl_->getHardwareMonitoring();
}

BiosDiagnostics BiosInfo::runDiagnostics() const {
    if (!impl_) {
        BiosDiagnostics diag;
        diag.postTestPassed = false;
        diag.failedComponents.push_back("BIOS implementation not available");
        return diag;
    }
    return impl_->runDiagnostics();
}

BiosOperationResult BiosInfo::resetToDefaults() {
    if (!impl_) {
        spdlog::error("BIOS implementation not available");
        return BiosOperationResult::FAILED;
    }
    return impl_->resetToDefaults();
}

std::vector<std::string> BiosInfo::getSupportedFeatures() const {
    if (!impl_) {
        return {"Error: BIOS implementation not available"};
    }
    return impl_->getSupportedFeatures();
}

}  // namespace atom::system
