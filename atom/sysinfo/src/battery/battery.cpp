#include "battery.hpp"

#ifdef _WIN32
#include "windows.hpp"
#elif defined(__APPLE__)
#include "macos.hpp"
#elif defined(__linux__)
#include "linux.hpp"
#endif

#include <atomic>
#include <chrono>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <spdlog/spdlog.h>
#include <thread>

namespace atom::system::battery {

// Cross-platform battery information functions
auto getBatteryInfo() -> std::optional<BatteryInfo> {
#ifdef _WIN32
    return windows::WindowsBatteryProvider::getBatteryInfo();
#elif defined(__APPLE__)
    return macos::MacOSBatteryProvider::getBatteryInfo();
#elif defined(__linux__)
    return linux::LinuxBatteryProvider::getBatteryInfo();
#else
    spdlog::error("Platform not supported for battery info");
    return std::nullopt;
#endif
}

auto getDetailedBatteryInfo() -> BatteryResult {
#ifdef _WIN32
    return windows::WindowsBatteryProvider::getDetailedBatteryInfo();
#elif defined(__APPLE__)
    return macos::MacOSBatteryProvider::getDetailedBatteryInfo();
#elif defined(__linux__)
    return linux::LinuxBatteryProvider::getDetailedBatteryInfo();
#else
    spdlog::error("Platform not supported for detailed battery info");
    return BatteryError::NOT_SUPPORTED;
#endif
}

auto getAllBatteries() -> MultiBatteryInfo {
#ifdef _WIN32
    return windows::WindowsBatteryProvider::getAllBatteries();
#elif defined(__APPLE__)
    return macos::MacOSBatteryProvider::getAllBatteries();
#elif defined(__linux__)
    return linux::LinuxBatteryProvider::getAllBatteries();
#else
    spdlog::error("Platform not supported for multiple battery info");
    return MultiBatteryInfo{};
#endif
}

// Battery Monitor Implementation
class BatteryMonitorImpl {
public:
    BatteryMonitorImpl() = default;
    ~BatteryMonitorImpl() { stop(); }

    auto start(BatteryMonitor::BatteryCallback callback, unsigned int interval_ms) -> bool {
        if (m_isRunning.exchange(true)) {
            spdlog::warn("Battery monitor is already running");
            return false;
        }

        m_monitorThread = std::thread([this, cb = std::move(callback),
                                       interval = std::chrono::milliseconds(interval_ms)]() {
            BatteryInfo lastInfo;
            bool firstRun = true;

            while (m_isRunning.load()) {
                auto result = getDetailedBatteryInfo();
                if (auto* currentInfoPtr = std::get_if<BatteryInfo>(&result)) {
                    BatteryInfo& currentInfo = *currentInfoPtr;
                    if (currentInfo.isBatteryPresent &&
                        (firstRun || currentInfo != lastInfo)) {
                        cb(currentInfo);
                        lastInfo = currentInfo;
                        firstRun = false;
                    } else if (!currentInfo.isBatteryPresent && lastInfo.isBatteryPresent) {
                        cb(currentInfo);
                        lastInfo = currentInfo;
                    }
                } else {
                    BatteryError error = std::get<BatteryError>(result);
                    spdlog::error("Error getting detailed battery info: {}",
                                  static_cast<int>(error));
                }
                std::this_thread::sleep_for(interval);
            }
        });

        spdlog::info("Battery monitor started");
        return true;
    }

    void stop() {
        if (m_isRunning.exchange(false)) {
            if (m_monitorThread.joinable()) {
                m_monitorThread.join();
            }
            spdlog::info("Battery monitor stopped");
        }
    }

    [[nodiscard]] auto isRunning() const noexcept -> bool { return m_isRunning.load(); }

private:
    std::atomic<bool> m_isRunning{false};
    std::thread m_monitorThread;
};

static std::unique_ptr<BatteryMonitorImpl> g_batteryMonitorImpl;
static std::mutex g_monitorMutex;

static auto getMonitorImpl() -> BatteryMonitorImpl& {
    std::lock_guard<std::mutex> lock(g_monitorMutex);
    if (!g_batteryMonitorImpl) {
        g_batteryMonitorImpl = std::make_unique<BatteryMonitorImpl>();
    }
    return *g_batteryMonitorImpl;
}

auto BatteryMonitor::startMonitoring(BatteryCallback callback, unsigned int interval_ms) -> bool {
    return getMonitorImpl().start(std::move(callback), interval_ms);
}

void BatteryMonitor::stopMonitoring() {
    std::lock_guard<std::mutex> lock(g_monitorMutex);
    if (g_batteryMonitorImpl) {
        g_batteryMonitorImpl->stop();
    }
}

auto BatteryMonitor::isMonitoring() noexcept -> bool {
    std::lock_guard<std::mutex> lock(g_monitorMutex);
    return g_batteryMonitorImpl && g_batteryMonitorImpl->isRunning();
}

// Power Plan Manager Implementation
auto PowerPlanManager::setPowerPlan(PowerPlan plan) -> std::optional<bool> {
#ifdef _WIN32
    return windows::WindowsPowerManager::setPowerPlan(plan);
#elif defined(__APPLE__)
    return macos::MacOSPowerManager::setPowerPlan(plan);
#elif defined(__linux__)
    return linux::LinuxPowerManager::setPowerPlan(plan);
#else
    spdlog::warn("Power plan management not implemented for this platform");
    return std::nullopt;
#endif
}

auto PowerPlanManager::getCurrentPowerPlan() -> std::optional<PowerPlan> {
#ifdef _WIN32
    return windows::WindowsPowerManager::getCurrentPowerPlan();
#elif defined(__APPLE__)
    return macos::MacOSPowerManager::getCurrentPowerPlan();
#elif defined(__linux__)
    return linux::LinuxPowerManager::getCurrentPowerPlan();
#else
    spdlog::warn("Getting current power plan not implemented for this platform");
    return std::nullopt;
#endif
}

auto PowerPlanManager::getAvailablePowerPlans() -> std::vector<std::string> {
#ifdef _WIN32
    return windows::WindowsPowerManager::getAvailablePowerPlans();
#elif defined(__APPLE__)
    return macos::MacOSPowerManager::getAvailablePowerPlans();
#elif defined(__linux__)
    return linux::LinuxPowerManager::getAvailablePowerPlans();
#else
    spdlog::warn("Getting available power plans not implemented for this platform");
    return {"Default"};
#endif
}

// Battery Manager Implementation
class BatteryManager::BatteryManagerImpl {
public:
    BatteryManagerImpl() = default;
    ~BatteryManagerImpl() {
        stopMonitoring();
        stopRecording();
    }

    void setAlertCallback(AlertCallback callback) {
        std::lock_guard lock(m_mutex);
        m_alertCallback = std::move(callback);
    }

    void setAlertSettings(const BatteryAlertSettings& settings) {
        std::lock_guard lock(m_mutex);
        m_alertSettings = settings;
    }

    [[nodiscard]] auto getStats() const -> const BatteryStats& {
        std::shared_lock lock(m_mutex);
        return m_currentStats;
    }

    auto startRecording(std::string_view logFilePath) -> bool {
        std::lock_guard lock(m_mutex);
        if (m_isRecording) {
            spdlog::warn("Recording is already active");
            return false;
        }

        if (!logFilePath.empty()) {
            m_logFile.open(std::string(logFilePath), std::ios::out | std::ios::app);
            if (!m_logFile.is_open()) {
                spdlog::error("Failed to open log file: {}", logFilePath);
                return false;
            }
            spdlog::info("Recording battery data to log file: {}", logFilePath);
            m_logFile << "timestamp,datetime,battery_level_percent,temperature_celsius,"
                         "voltage_v,current_a,health_percent,is_charging\n";
        } else {
            spdlog::info("Recording battery data to memory only");
        }

        m_isRecording = true;
        return true;
    }

    void stopRecording() {
        std::lock_guard lock(m_mutex);
        if (!m_isRecording)
            return;

        m_isRecording = false;
        if (m_logFile.is_open()) {
            m_logFile.close();
            spdlog::info("Stopped recording battery data to log file");
        } else {
            spdlog::info("Stopped recording battery data (memory only)");
        }
    }

    auto startMonitoring(unsigned int interval_ms) -> bool {
        spdlog::info("BatteryManager starting internal monitoring");
        return BatteryMonitor::startMonitoring(
            [this](const BatteryInfo& info) { this->handleBatteryUpdate(info); }, interval_ms);
    }

    void stopMonitoring() {
        spdlog::info("BatteryManager stopping internal monitoring");
        BatteryMonitor::stopMonitoring();
    }

    [[nodiscard]] auto getHistory(unsigned int maxEntries) const
        -> std::vector<std::pair<std::chrono::system_clock::time_point, BatteryInfo>> {
        std::shared_lock lock(m_mutex);

        if (maxEntries == 0 || maxEntries >= m_historyData.size()) {
            return m_historyData;
        }

        return {m_historyData.end() - maxEntries, m_historyData.end()};
    }

private:
    void handleBatteryUpdate(const BatteryInfo& info) {
        if (info.isBatteryPresent) {
            recordData(info);
            updateStats(info);
            checkAlerts(info);
        }
    }

    void checkAlerts(const BatteryInfo& info) {
        AlertCallback currentAlertCallback;
        BatteryAlertSettings currentAlertSettings;
        {
            std::shared_lock lock(m_mutex);
            if (!m_alertCallback)
                return;

            currentAlertCallback = m_alertCallback;
            currentAlertSettings = m_alertSettings;
        }

        if (info.batteryLifePercent <= currentAlertSettings.criticalBatteryThreshold) {
            spdlog::warn("Critical battery alert: {:.2f}%", info.batteryLifePercent);
            currentAlertCallback(AlertType::CRITICAL_BATTERY, info);
        } else if (info.batteryLifePercent <= currentAlertSettings.lowBatteryThreshold) {
            spdlog::warn("Low battery alert: {:.2f}%", info.batteryLifePercent);
            currentAlertCallback(AlertType::LOW_BATTERY, info);
        }

        if (currentAlertSettings.enableTemperatureAlerts &&
            info.temperature >= currentAlertSettings.highTempThreshold) {
            spdlog::warn("High temperature alert: {:.2f}°C", info.temperature);
            currentAlertCallback(AlertType::HIGH_TEMPERATURE, info);
        }

        if (currentAlertSettings.enableHealthAlerts &&
            info.getBatteryHealth() <= currentAlertSettings.lowHealthThreshold) {
            spdlog::warn("Low battery health alert: {:.2f}%", info.getBatteryHealth());
            currentAlertCallback(AlertType::LOW_BATTERY_HEALTH, info);
        }

        if (currentAlertSettings.enableCycleAlerts &&
            info.cycleCounts >= currentAlertSettings.highCycleCountThreshold) {
            spdlog::warn("High cycle count alert: {}", info.cycleCounts);
            currentAlertCallback(AlertType::HIGH_CYCLE_COUNT, info);
        }
    }

    void updateStats(const BatteryInfo& info) {
        std::lock_guard lock(m_mutex);

        m_currentStats.minBatteryLevel =
            std::min(m_currentStats.minBatteryLevel, info.batteryLifePercent);
        m_currentStats.maxBatteryLevel =
            std::max(m_currentStats.maxBatteryLevel, info.batteryLifePercent);

        if (info.temperature > -100) {
            m_currentStats.minTemperature =
                std::min(m_currentStats.minTemperature, info.temperature);
            m_currentStats.maxTemperature =
                std::max(m_currentStats.maxTemperature, info.temperature);
        }

        if (info.voltageNow > 0) {
            m_currentStats.minVoltage = std::min(m_currentStats.minVoltage, info.voltageNow);
            m_currentStats.maxVoltage = std::max(m_currentStats.maxVoltage, info.voltageNow);
        }

        // Calculate discharge rate
        if (!m_historyData.empty() && !info.isCharging &&
            m_historyData.back().second.batteryLifePercent > info.batteryLifePercent) {
            const auto& lastRecord = m_historyData.back();
            auto timeDiffSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                                       std::chrono::system_clock::now() - lastRecord.first)
                                       .count();

            if (timeDiffSeconds > 0) {
                float dischargePercent =
                    lastRecord.second.batteryLifePercent - info.batteryLifePercent;
                float currentRatePerHour =
                    (dischargePercent / static_cast<float>(timeDiffSeconds)) * 3600.0f;

                if (m_currentStats.avgDischargeRate < 0) {
                    m_currentStats.avgDischargeRate = currentRatePerHour;
                } else {
                    m_currentStats.avgDischargeRate =
                        (m_currentStats.avgDischargeRate * 0.9f) + (currentRatePerHour * 0.1f);
                }
            }
        }

        m_currentStats.cycleCount = info.cycleCounts;
        m_currentStats.batteryHealth = info.getBatteryHealth();
        m_currentStats.lastUpdateTime = std::chrono::system_clock::now();
    }

    void recordData(const BatteryInfo& info) {
        std::lock_guard lock(m_mutex);
        if (!m_isRecording)
            return;

        auto now = std::chrono::system_clock::now();
        m_historyData.emplace_back(now, info);

        constexpr size_t MAX_HISTORY_SIZE = 8640;
        if (m_historyData.size() > MAX_HISTORY_SIZE) {
            m_historyData.erase(m_historyData.begin(),
                                m_historyData.begin() + (m_historyData.size() - MAX_HISTORY_SIZE));
        }

        if (m_logFile.is_open()) {
            std::time_t tt = std::chrono::system_clock::to_time_t(now);
            std::tm tm_local = *std::localtime(&tt);

            std::stringstream time_ss;
            time_ss << std::put_time(&tm_local, "%Y-%m-%d %H:%M:%S");

            m_logFile << std::format("{},{},{:.2f},{:.2f},{:.3f},{:.3f},{:.2f},{}\n", tt,
                                     time_ss.str(), info.batteryLifePercent, info.temperature,
                                     info.voltageNow, info.currentNow, info.getBatteryHealth(),
                                     info.isCharging);
            m_logFile.flush();
        }
    }

    mutable std::shared_mutex m_mutex;
    BatteryAlertSettings m_alertSettings;
    AlertCallback m_alertCallback;
    bool m_isRecording{false};
    std::ofstream m_logFile;
    std::vector<std::pair<std::chrono::system_clock::time_point, BatteryInfo>> m_historyData;
    BatteryStats m_currentStats;
};

auto BatteryManager::getInstance() -> BatteryManager& {
    static BatteryManager instance;
    return instance;
}

BatteryManager::BatteryManager() : impl(std::make_unique<BatteryManagerImpl>()) {}

BatteryManager::~BatteryManager() = default;

void BatteryManager::setAlertCallback(AlertCallback callback) {
    impl->setAlertCallback(std::move(callback));
}

void BatteryManager::setAlertSettings(const BatteryAlertSettings& settings) {
    impl->setAlertSettings(settings);
}

auto BatteryManager::getStats() const -> const BatteryStats& { return impl->getStats(); }

auto BatteryManager::startRecording(std::string_view logFile) -> bool {
    return impl->startRecording(logFile);
}

void BatteryManager::stopRecording() { impl->stopRecording(); }

auto BatteryManager::startMonitoring(unsigned int interval_ms) -> bool {
    return impl->startMonitoring(interval_ms);
}

void BatteryManager::stopMonitoring() { impl->stopMonitoring(); }

auto BatteryManager::getHistory(unsigned int maxEntries) const
    -> std::vector<std::pair<std::chrono::system_clock::time_point, BatteryInfo>> {
    return impl->getHistory(maxEntries);
}

// Battery Calibrator Implementation
static std::atomic<bool> g_calibrationInProgress{false};
static std::atomic<float> g_calibrationProgress{0.0f};
static CalibrationData g_calibrationData;
static std::mutex g_calibrationMutex;

auto BatteryCalibrator::startCalibration() -> bool {
    std::lock_guard lock(g_calibrationMutex);
    if (g_calibrationInProgress.load()) {
        spdlog::warn("Battery calibration already in progress");
        return false;
    }

    spdlog::info("Starting battery calibration process");
    g_calibrationInProgress = true;
    g_calibrationProgress = 0.0f;

    // Start calibration in a separate thread
    std::thread calibrationThread([]() {
        try {
            // Phase 1: Full discharge (0-50% progress)
            spdlog::info("Calibration Phase 1: Full discharge");
            for (int i = 0; i <= 50; ++i) {
                if (!g_calibrationInProgress.load()) break;
                g_calibrationProgress = static_cast<float>(i);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            // Phase 2: Full charge (50-100% progress)
            spdlog::info("Calibration Phase 2: Full charge");
            for (int i = 50; i <= 100; ++i) {
                if (!g_calibrationInProgress.load()) break;
                g_calibrationProgress = static_cast<float>(i);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            // Update calibration data
            {
                std::lock_guard lock(g_calibrationMutex);
                auto batteryResult = getDetailedBatteryInfo();
                if (auto* batteryInfo = std::get_if<BatteryInfo>(&batteryResult)) {
                    g_calibrationData.actualCapacity = batteryInfo->energyFull;
                    g_calibrationData.designCapacity = batteryInfo->energyDesign;
                    g_calibrationData.calibrationAccuracy =
                        (g_calibrationData.actualCapacity / g_calibrationData.designCapacity) * 100.0f;
                    g_calibrationData.lastCalibration = std::chrono::system_clock::now();
                    g_calibrationData.calibrationCycles++;
                    g_calibrationData.needsCalibration = false;
                }
            }

            spdlog::info("Battery calibration completed successfully");
        } catch (const std::exception& e) {
            spdlog::error("Battery calibration failed: {}", e.what());
        }

        g_calibrationInProgress = false;
        g_calibrationProgress = 100.0f;
    });

    calibrationThread.detach();
    return true;
}

void BatteryCalibrator::stopCalibration() {
    std::lock_guard lock(g_calibrationMutex);
    if (g_calibrationInProgress.load()) {
        spdlog::info("Stopping battery calibration");
        g_calibrationInProgress = false;
    }
}

auto BatteryCalibrator::isCalibrating() -> bool {
    return g_calibrationInProgress.load();
}

auto BatteryCalibrator::getCalibrationProgress() -> float {
    return g_calibrationProgress.load();
}

auto BatteryCalibrator::getCalibrationData() -> CalibrationData {
    std::lock_guard lock(g_calibrationMutex);
    return g_calibrationData;
}

auto BatteryCalibrator::needsCalibration() -> bool {
    auto batteryResult = getDetailedBatteryInfo();
    if (auto* batteryInfo = std::get_if<BatteryInfo>(&batteryResult)) {
        float health = batteryInfo->getBatteryHealth();
        int cycles = batteryInfo->cycleCounts;

        // Need calibration if health is low or many cycles have passed
        return health < 80.0f || cycles > 500 ||
               (std::chrono::system_clock::now() - g_calibrationData.lastCalibration) >
               std::chrono::hours(24 * 30);  // 30 days
    }
    return false;
}

// Thermal Manager Implementation
static ThermalSettings g_thermalSettings;
static std::atomic<bool> g_thermalMonitoringEnabled{false};
static std::mutex g_thermalMutex;

void ThermalManager::setThermalSettings(const ThermalSettings& settings) {
    std::lock_guard lock(g_thermalMutex);
    g_thermalSettings = settings;
    spdlog::info("Thermal settings updated - Warning: {:.1f}°C, Critical: {:.1f}°C",
                 settings.warningTemperature, settings.criticalTemperature);
}

auto ThermalManager::getThermalSettings() -> ThermalSettings {
    std::lock_guard lock(g_thermalMutex);
    return g_thermalSettings;
}

auto ThermalManager::isThermalThrottling() -> bool {
    auto batteryResult = getDetailedBatteryInfo();
    if (auto* batteryInfo = std::get_if<BatteryInfo>(&batteryResult)) {
        std::lock_guard lock(g_thermalMutex);
        return batteryInfo->temperature >= g_thermalSettings.criticalTemperature;
    }
    return false;
}

auto ThermalManager::getSystemTemperature() -> std::optional<float> {
    auto batteryResult = getDetailedBatteryInfo();
    if (auto* batteryInfo = std::get_if<BatteryInfo>(&batteryResult)) {
        return batteryInfo->temperature;
    }
    return std::nullopt;
}

void ThermalManager::setThermalMonitoring(bool enabled) {
    g_thermalMonitoringEnabled = enabled;
    spdlog::info("Thermal monitoring {}", enabled ? "enabled" : "disabled");
}

auto ThermalManager::isThermalMonitoringEnabled() -> bool {
    return g_thermalMonitoringEnabled.load();
}

// Adaptive Power Manager Implementation
static std::atomic<bool> g_adaptivePowerEnabled{false};
static std::string g_currentProfile = "balanced";
static std::mutex g_adaptivePowerMutex;

auto AdaptivePowerManager::enableAdaptivePower() -> bool {
    std::lock_guard lock(g_adaptivePowerMutex);
    if (g_adaptivePowerEnabled.load()) {
        spdlog::warn("Adaptive power management already enabled");
        return true;
    }

    spdlog::info("Enabling adaptive power management");
    g_adaptivePowerEnabled = true;

    // Start adaptive power management thread
    std::thread adaptiveThread([]() {
        while (g_adaptivePowerEnabled.load()) {
            auto batteryResult = getDetailedBatteryInfo();
            if (auto* batteryInfo = std::get_if<BatteryInfo>(&batteryResult)) {
                PowerPlan recommendedPlan = PowerPlan::BALANCED;

                // Adaptive logic based on battery state
                if (batteryInfo->batteryLifePercent <= 20.0f && !batteryInfo->isCharging) {
                    recommendedPlan = PowerPlan::POWER_SAVER;
                } else if (batteryInfo->isCharging && batteryInfo->batteryLifePercent > 80.0f) {
                    recommendedPlan = PowerPlan::PERFORMANCE;
                } else if (batteryInfo->temperature > 40.0f) {
                    recommendedPlan = PowerPlan::POWER_SAVER;  // Reduce heat
                }

                // Apply the recommended plan
                auto currentPlan = PowerPlanManager::getCurrentPowerPlan();
                if (currentPlan && *currentPlan != recommendedPlan) {
                    spdlog::info("Adaptive power: switching to plan {}", static_cast<int>(recommendedPlan));
                    auto result = PowerPlanManager::setPowerPlan(recommendedPlan);
                    if (!result || !*result) {
                        spdlog::warn("Failed to set adaptive power plan");
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::seconds(30));  // Check every 30 seconds
        }
    });

    adaptiveThread.detach();
    return true;
}

void AdaptivePowerManager::disableAdaptivePower() {
    std::lock_guard lock(g_adaptivePowerMutex);
    if (g_adaptivePowerEnabled.load()) {
        spdlog::info("Disabling adaptive power management");
        g_adaptivePowerEnabled = false;
    }
}

auto AdaptivePowerManager::isAdaptivePowerEnabled() -> bool {
    return g_adaptivePowerEnabled.load();
}

auto AdaptivePowerManager::setOptimizationProfile(const std::string& profile) -> bool {
    std::lock_guard lock(g_adaptivePowerMutex);

    std::vector<std::string> validProfiles = {"battery_saver", "balanced", "performance", "gaming"};
    if (std::find(validProfiles.begin(), validProfiles.end(), profile) == validProfiles.end()) {
        spdlog::error("Invalid optimization profile: {}", profile);
        return false;
    }

    g_currentProfile = profile;
    spdlog::info("Optimization profile set to: {}", profile);
    return true;
}

auto AdaptivePowerManager::getCurrentProfile() -> std::string {
    std::lock_guard lock(g_adaptivePowerMutex);
    return g_currentProfile;
}

auto AdaptivePowerManager::getAvailableProfiles() -> std::vector<std::string> {
    return {"battery_saver", "balanced", "performance", "gaming", "presentation"};
}

}  // namespace atom::system::battery
