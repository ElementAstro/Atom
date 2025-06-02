/*
 * battery.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "atom/sysinfo/battery.hpp"

#include <atomic>
#include <chrono>
#include <format>
#include <fstream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <conio.h>
#include <devguid.h>
#include <poclass.h>
#include <powersetting.h>
#include <setupapi.h>
// clang-format on
#include <winnt.h>
#elif defined(__APPLE__)
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#elif defined(__linux__)
#include <csignal>
#include <cstdio>
#endif

#include <spdlog/spdlog.h>

namespace atom::system {

auto BatteryInfo::operator==(const BatteryInfo& other) const -> bool {
    return isBatteryPresent == other.isBatteryPresent &&
           isCharging == other.isCharging &&
           batteryLifePercent == other.batteryLifePercent &&
           batteryLifeTime == other.batteryLifeTime &&
           batteryFullLifeTime == other.batteryFullLifeTime &&
           energyNow == other.energyNow && energyFull == other.energyFull &&
           energyDesign == other.energyDesign &&
           voltageNow == other.voltageNow && currentNow == other.currentNow &&
           temperature == other.temperature &&
           cycleCounts == other.cycleCounts &&
           manufacturer == other.manufacturer && model == other.model &&
           serialNumber == other.serialNumber;
}

auto BatteryInfo::operator!=(const BatteryInfo& other) const -> bool {
    return !(*this == other);
}

auto BatteryInfo::getBatteryHealth() const -> float {
    if (energyDesign > 0) {
        return (energyFull / energyDesign) * 100.0f;
    }
    return 0.0f;
}

auto BatteryInfo::getEstimatedTimeRemaining() const -> float {
    if (currentNow > 0 && !isCharging) {
        return (energyNow / (voltageNow * currentNow));
    }
    return batteryLifeTime / 60.0f;
}

auto getBatteryInfo() -> std::optional<BatteryInfo> {
    spdlog::debug("Starting battery info retrieval");
    BatteryInfo info;

#ifdef _WIN32
    SYSTEM_POWER_STATUS powerStatus{};
    if (GetSystemPowerStatus(&powerStatus) != 0) {
        info.isBatteryPresent = powerStatus.BatteryFlag != 128;
        info.isCharging =
            powerStatus.BatteryFlag == 8 || powerStatus.ACLineStatus == 1;
        info.batteryLifePercent =
            static_cast<float>(powerStatus.BatteryLifePercent);
        info.batteryLifeTime =
            powerStatus.BatteryLifeTime == 0xFFFFFFFF
                ? 0.0f
                : static_cast<float>(powerStatus.BatteryLifeTime);
        info.batteryFullLifeTime =
            powerStatus.BatteryFullLifeTime == 0xFFFFFFFF
                ? 0.0f
                : static_cast<float>(powerStatus.BatteryFullLifeTime);

        spdlog::debug("Battery present: {}, charging: {}, level: {:.2f}%",
                      info.isBatteryPresent, info.isCharging,
                      info.batteryLifePercent);
        return info;
    } else {
        spdlog::error("Failed to get system power status");
        return std::nullopt;
    }

#elif defined(__APPLE__)
    template <typename CFType>
    struct CFDeleter {
        void operator()(CFType ref) {
            if (ref)
                CFRelease(ref);
        }
    };

    template <typename CFType>
    using CFUniquePtr =
        std::unique_ptr<std::remove_pointer_t<CFType>, CFDeleter<CFType>>;

    CFUniquePtr<CFTypeRef> powerSourcesInfo(IOPSCopyPowerSourcesInfo());
    if (!powerSourcesInfo) {
        spdlog::error("Failed to copy power sources info");
        return std::nullopt;
    }

    CFUniquePtr<CFArrayRef> powerSources(
        IOPSCopyPowerSourcesList(powerSourcesInfo.get()));
    if (!powerSources) {
        spdlog::error("Failed to copy power sources list");
        return std::nullopt;
    }

    CFIndex count = CFArrayGetCount(powerSources.get());
    if (count > 0) {
        CFDictionaryRef powerSource = static_cast<CFDictionaryRef>(
            CFArrayGetValueAtIndex(powerSources.get(), 0));

        if (auto isCharging = static_cast<CFBooleanRef>(
                CFDictionaryGetValue(powerSource, kIOPSIsChargingKey))) {
            info.isCharging = CFBooleanGetValue(isCharging);
        }

        if (auto capacity = static_cast<CFNumberRef>(
                CFDictionaryGetValue(powerSource, kIOPSCurrentCapacityKey))) {
            SInt32 value;
            CFNumberGetValue(capacity, kCFNumberSInt32Type, &value);
            info.batteryLifePercent = static_cast<float>(value);
        }

        if (auto timeToEmpty = static_cast<CFNumberRef>(
                CFDictionaryGetValue(powerSource, kIOPSTimeToEmptyKey))) {
            SInt32 value;
            CFNumberGetValue(timeToEmpty, kCFNumberSInt32Type, &value);
            info.batteryLifeTime = static_cast<float>(value);
        }

        if (auto isPresent = static_cast<CFBooleanRef>(
                CFDictionaryGetValue(powerSource, kIOPSIsPresentKey))) {
            info.isBatteryPresent = CFBooleanGetValue(isPresent);
        }

        spdlog::debug(
            "Battery info - charging: {}, level: {:.2f}%, time: {:.2f}min",
            info.isCharging, info.batteryLifePercent, info.batteryLifeTime);
        return info;
    } else {
        spdlog::warn("No power sources found");
        info.isBatteryPresent = false;
        return info;
    }

#elif defined(__linux__)
    std::ifstream batteryInfo("/sys/class/power_supply/BAT0/uevent");
    if (batteryInfo.is_open()) {
        std::string line;
        info.isBatteryPresent = true;

        while (std::getline(batteryInfo, line)) {
            const auto pos = line.find('=');
            if (pos == std::string::npos)
                continue;

            const std::string_view key = std::string_view(line).substr(0, pos);
            const std::string value = line.substr(pos + 1);

            if (key == "POWER_SUPPLY_PRESENT") {
                info.isBatteryPresent = (value == "1");
            } else if (key == "POWER_SUPPLY_STATUS") {
                info.isCharging = (value == "Charging" || value == "Full");
            } else if (key == "POWER_SUPPLY_CAPACITY") {
                info.batteryLifePercent = std::stof(value);
            } else if (key == "POWER_SUPPLY_TIME_TO_EMPTY_NOW") {
                info.batteryLifeTime = std::stof(value) / 60.0f;
            } else if (key == "POWER_SUPPLY_ENERGY_NOW") {
                info.energyNow = std::stof(value);
            } else if (key == "POWER_SUPPLY_ENERGY_FULL") {
                info.energyFull = std::stof(value);
            } else if (key == "POWER_SUPPLY_ENERGY_FULL_DESIGN") {
                info.energyDesign = std::stof(value);
            } else if (key == "POWER_SUPPLY_VOLTAGE_NOW") {
                info.voltageNow = std::stof(value) / 1000000.0f;
            } else if (key == "POWER_SUPPLY_CURRENT_NOW") {
                info.currentNow = std::stof(value) / 1000000.0f;
            }
        }

        if (!info.isBatteryPresent) {
            spdlog::debug("Battery marked as not present");
            return std::nullopt;
        }
        return info;
    } else {
        spdlog::error("Failed to open battery info file");
        return std::nullopt;
    }
#else
    spdlog::error("Platform not supported for battery info");
    return std::nullopt;
#endif
}

auto getDetailedBatteryInfo() -> BatteryResult {
    auto batteryInfoOpt = getBatteryInfo();
    if (!batteryInfoOpt) {
        return BatteryError::READ_ERROR;
    }

    BatteryInfo info = std::move(*batteryInfoOpt);

#ifdef _WIN32
    HDEVINFO hdev = SetupDiGetClassDevs(&GUID_DEVCLASS_BATTERY, 0, 0,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hdev != INVALID_HANDLE_VALUE) {
        struct DevInfoSetCloser {
            HDEVINFO handle;
            ~DevInfoSetCloser() {
                if (handle != INVALID_HANDLE_VALUE) {
                    SetupDiDestroyDeviceInfoList(handle);
                }
            }
        } deviceInfoSetCloser{hdev};

        SP_DEVICE_INTERFACE_DATA did{};
        did.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        if (SetupDiEnumDeviceInterfaces(hdev, NULL, &GUID_DEVCLASS_BATTERY, 0,
                                        &did)) {
            DWORD cbRequired = 0;
            SetupDiGetDeviceInterfaceDetail(hdev, &did, NULL, 0, &cbRequired,
                                            NULL);

            if (cbRequired > 0) {
                auto pspdidd = std::make_unique<BYTE[]>(cbRequired);
                auto detailData =
                    reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(
                        pspdidd.get());
                detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

                if (SetupDiGetDeviceInterfaceDetail(hdev, &did, detailData,
                                                    cbRequired, NULL, NULL)) {
                    HANDLE hBattery = CreateFile(
                        detailData->DevicePath, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                        0, NULL);

                    if (hBattery != INVALID_HANDLE_VALUE) {
                        struct FileHandleCloser {
                            HANDLE handle;
                            ~FileHandleCloser() {
                                if (handle != INVALID_HANDLE_VALUE)
                                    CloseHandle(handle);
                            }
                        } batteryFileHandleCloser{hBattery};

                        BATTERY_QUERY_INFORMATION bqi{};
                        DWORD bytesReturned;

                        bqi.InformationLevel = BatteryManufactureName;
                        std::wstring manufacturerName(128, L'\0');
                        if (DeviceIoControl(
                                hBattery, IOCTL_BATTERY_QUERY_INFORMATION, &bqi,
                                sizeof(bqi), manufacturerName.data(),
                                manufacturerName.size() * sizeof(wchar_t),
                                &bytesReturned, NULL) &&
                            bytesReturned > 0) {
                            manufacturerName.resize(
                                bytesReturned / sizeof(wchar_t) > 0
                                    ? (bytesReturned / sizeof(wchar_t)) - 1
                                    : 0);
                            info.manufacturer.assign(manufacturerName.begin(),
                                                     manufacturerName.end());
                        }

                        bqi.InformationLevel = BatteryDeviceName;
                        std::wstring modelName(128, L'\0');
                        if (DeviceIoControl(hBattery,
                                            IOCTL_BATTERY_QUERY_INFORMATION,
                                            &bqi, sizeof(bqi), modelName.data(),
                                            modelName.size() * sizeof(wchar_t),
                                            &bytesReturned, NULL) &&
                            bytesReturned > 0) {
                            modelName.resize(
                                bytesReturned / sizeof(wchar_t) > 0
                                    ? (bytesReturned / sizeof(wchar_t)) - 1
                                    : 0);
                            info.model.assign(modelName.begin(),
                                              modelName.end());
                        }

                        bqi.InformationLevel = BatterySerialNumber;
                        std::wstring serialNumberStr(128, L'\0');
                        if (DeviceIoControl(
                                hBattery, IOCTL_BATTERY_QUERY_INFORMATION, &bqi,
                                sizeof(bqi), serialNumberStr.data(),
                                serialNumberStr.size() * sizeof(wchar_t),
                                &bytesReturned, NULL) &&
                            bytesReturned > 0) {
                            serialNumberStr.resize(
                                bytesReturned / sizeof(wchar_t) > 0
                                    ? (bytesReturned / sizeof(wchar_t)) - 1
                                    : 0);
                            info.serialNumber.assign(serialNumberStr.begin(),
                                                     serialNumberStr.end());
                        }
                    }
                }
            }
        }
        return info;
    }
    spdlog::error("SetupDiGetClassDevs failed: {}", GetLastError());
    return BatteryError::ACCESS_DENIED;

#elif defined(__APPLE__)
    template <typename CFType>
    struct CFDeleter {
        void operator()(CFType ref) {
            if (ref)
                CFRelease(ref);
        }
    };

    template <typename CFType>
    using CFUniquePtr =
        std::unique_ptr<std::remove_pointer_t<CFType>, CFDeleter<CFType>>;

    CFUniquePtr<CFTypeRef> powerSourcesInfo(IOPSCopyPowerSourcesInfo());
    if (!powerSourcesInfo)
        return BatteryError::READ_ERROR;

    CFUniquePtr<CFArrayRef> powerSources(
        IOPSCopyPowerSourcesList(powerSourcesInfo.get()));
    if (!powerSources || CFArrayGetCount(powerSources.get()) == 0) {
        return BatteryError::NOT_PRESENT;
    }

    CFDictionaryRef psDesc = static_cast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(powerSources.get(), 0));

    auto getStringValue = [&](CFStringRef key) -> std::string {
        if (auto value =
                static_cast<CFStringRef>(CFDictionaryGetValue(psDesc, key))) {
            char buffer[256];
            if (CFStringGetCString(value, buffer, sizeof(buffer),
                                   kCFStringEncodingUTF8)) {
                return buffer;
            }
        }
        return "";
    };

    auto getIntValue = [&](CFStringRef key) -> int {
        if (auto value =
                static_cast<CFNumberRef>(CFDictionaryGetValue(psDesc, key))) {
            int intVal;
            if (CFNumberGetValue(value, kCFNumberIntType, &intVal)) {
                return intVal;
            }
        }
        return 0;
    };

    auto getFloatValue = [&](CFStringRef key, float scale = 1.0f) -> float {
        if (auto value =
                static_cast<CFNumberRef>(CFDictionaryGetValue(psDesc, key))) {
            double doubleVal;
            if (CFNumberGetValue(value, kCFNumberDoubleType, &doubleVal)) {
                return static_cast<float>(doubleVal * scale);
            }
        }
        return 0.0f;
    };

    info.manufacturer = getStringValue(CFSTR(kIOPSManufacturerKey));
    info.model = getStringValue(CFSTR(kIOPSDeviceNameKey));
    info.serialNumber = getStringValue(CFSTR(kIOPSSerialNumberKey));
    info.cycleCounts = getIntValue(CFSTR(kIOPSCycleCountKey));
    info.temperature = getFloatValue(CFSTR(kIOPSTemperatureKey), 0.01f);
    info.voltageNow = getFloatValue(CFSTR(kIOPSVoltageKey), 0.001f);
    info.currentNow = getFloatValue(CFSTR(kIOPSAmperageKey), 0.001f);

    return info;

#elif defined(__linux__)
    std::ifstream batteryUevent("/sys/class/power_supply/BAT0/uevent");
    if (batteryUevent.is_open()) {
        std::string line;
        while (std::getline(batteryUevent, line)) {
            const auto pos = line.find('=');
            if (pos == std::string::npos)
                continue;

            const std::string_view key = std::string_view(line).substr(0, pos);
            const std::string value = line.substr(pos + 1);

            if (key == "POWER_SUPPLY_CYCLE_COUNT") {
                info.cycleCounts = std::stoi(value);
            } else if (key == "POWER_SUPPLY_TEMP") {
                info.temperature = std::stof(value) / 10.0f;
            } else if (key == "POWER_SUPPLY_MANUFACTURER") {
                info.manufacturer = value;
            } else if (key == "POWER_SUPPLY_MODEL_NAME") {
                info.model = value;
            } else if (key == "POWER_SUPPLY_SERIAL_NUMBER") {
                info.serialNumber = value;
            }
        }
        return info;
    }
    spdlog::error("Failed to open battery uevent file for detailed info");
    return BatteryError::READ_ERROR;
#else
    spdlog::warn("Detailed battery info not supported for this platform");
    return BatteryError::NOT_SUPPORTED;
#endif
}

class BatteryMonitorImpl {
public:
    BatteryMonitorImpl() = default;
    ~BatteryMonitorImpl() { stop(); }

    auto start(BatteryMonitor::BatteryCallback callback,
               unsigned int interval_ms) -> bool {
        if (m_isRunning.exchange(true)) {
            spdlog::warn("Battery monitor is already running");
            return false;
        }

        m_monitorThread =
            std::thread([this, cb = std::move(callback),
                         interval = std::chrono::milliseconds(interval_ms)]() {
                BatteryInfo lastInfo;
                bool firstRun = true;

                while (m_isRunning.load()) {
                    auto result = getDetailedBatteryInfo();
                    if (auto* currentInfoPtr =
                            std::get_if<BatteryInfo>(&result)) {
                        BatteryInfo& currentInfo = *currentInfoPtr;
                        if (currentInfo.isBatteryPresent &&
                            (firstRun || currentInfo != lastInfo)) {
                            cb(currentInfo);
                            lastInfo = currentInfo;
                            firstRun = false;
                        } else if (!currentInfo.isBatteryPresent &&
                                   lastInfo.isBatteryPresent) {
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

    [[nodiscard]] auto isRunning() const noexcept -> bool {
        return m_isRunning.load();
    }

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

auto BatteryMonitor::startMonitoring(BatteryCallback callback,
                                     unsigned int interval_ms) -> bool {
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
            m_logFile.open(std::string(logFilePath),
                           std::ios::out | std::ios::app);
            if (!m_logFile.is_open()) {
                spdlog::error("Failed to open log file: {}", logFilePath);
                return false;
            }
            spdlog::info("Recording battery data to log file: {}", logFilePath);
            m_logFile << "timestamp,datetime,battery_level_percent,temperature_"
                         "celsius,"
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
            [this](const BatteryInfo& info) {
                this->handleBatteryUpdate(info);
            },
            interval_ms);
    }

    void stopMonitoring() {
        spdlog::info("BatteryManager stopping internal monitoring");
        BatteryMonitor::stopMonitoring();
    }

    [[nodiscard]] auto getHistory(unsigned int maxEntries) const -> std::vector<
        std::pair<std::chrono::system_clock::time_point, BatteryInfo>> {
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

        if (info.batteryLifePercent <=
            currentAlertSettings.criticalBatteryThreshold) {
            spdlog::warn("Critical battery alert: {:.2f}%",
                         info.batteryLifePercent);
            currentAlertCallback(AlertType::CRITICAL_BATTERY, info);
        } else if (info.batteryLifePercent <=
                   currentAlertSettings.lowBatteryThreshold) {
            spdlog::warn("Low battery alert: {:.2f}%", info.batteryLifePercent);
            currentAlertCallback(AlertType::LOW_BATTERY, info);
        }

        if (info.temperature >= currentAlertSettings.highTempThreshold) {
            spdlog::warn("High temperature alert: {:.2f}°C", info.temperature);
            currentAlertCallback(AlertType::HIGH_TEMPERATURE, info);
        }

        if (info.getBatteryHealth() <=
            currentAlertSettings.lowHealthThreshold) {
            spdlog::warn("Low battery health alert: {:.2f}%",
                         info.getBatteryHealth());
            currentAlertCallback(AlertType::LOW_BATTERY_HEALTH, info);
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
            m_currentStats.minVoltage =
                std::min(m_currentStats.minVoltage, info.voltageNow);
            m_currentStats.maxVoltage =
                std::max(m_currentStats.maxVoltage, info.voltageNow);
        }

        if (!m_historyData.empty() && !info.isCharging &&
            m_historyData.back().second.batteryLifePercent >
                info.batteryLifePercent) {
            const auto& lastRecord = m_historyData.back();
            auto timeDiffSeconds =
                std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now() - lastRecord.first)
                    .count();

            if (timeDiffSeconds > 0) {
                float dischargePercent = lastRecord.second.batteryLifePercent -
                                         info.batteryLifePercent;
                float currentRatePerHour =
                    (dischargePercent / static_cast<float>(timeDiffSeconds)) *
                    3600.0f;

                if (m_currentStats.avgDischargeRate < 0) {
                    m_currentStats.avgDischargeRate = currentRatePerHour;
                } else {
                    m_currentStats.avgDischargeRate =
                        (m_currentStats.avgDischargeRate * 0.9f) +
                        (currentRatePerHour * 0.1f);
                }
            }
        }

        m_currentStats.cycleCount = info.cycleCounts;
        m_currentStats.batteryHealth = info.getBatteryHealth();
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
                                m_historyData.begin() +
                                    (m_historyData.size() - MAX_HISTORY_SIZE));
        }

        if (m_logFile.is_open()) {
            std::time_t tt = std::chrono::system_clock::to_time_t(now);
            std::tm tm_local = *std::localtime(&tt);

            std::stringstream time_ss;
            time_ss << std::put_time(&tm_local, "%Y-%m-%d %H:%M:%S");

            m_logFile << std::format(
                "{},{},{:.2f},{:.2f},{:.3f},{:.3f},{:.2f},{}\n", tt,
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
    std::vector<std::pair<std::chrono::system_clock::time_point, BatteryInfo>>
        m_historyData;
    BatteryStats m_currentStats;
};

auto BatteryManager::getInstance() -> BatteryManager& {
    static BatteryManager instance;
    return instance;
}

BatteryManager::BatteryManager()
    : impl(std::make_unique<BatteryManagerImpl>()) {}

BatteryManager::~BatteryManager() = default;

void BatteryManager::setAlertCallback(AlertCallback callback) {
    impl->setAlertCallback(std::move(callback));
}

void BatteryManager::setAlertSettings(const BatteryAlertSettings& settings) {
    impl->setAlertSettings(settings);
}

auto BatteryManager::getStats() const -> const BatteryStats& {
    return impl->getStats();
}

auto BatteryManager::startRecording(std::string_view logFile) -> bool {
    return impl->startRecording(logFile);
}

void BatteryManager::stopRecording() { impl->stopRecording(); }

auto BatteryManager::startMonitoring(unsigned int interval_ms) -> bool {
    return impl->startMonitoring(interval_ms);
}

void BatteryManager::stopMonitoring() { impl->stopMonitoring(); }

auto BatteryManager::getHistory(unsigned int maxEntries) const -> std::vector<
    std::pair<std::chrono::system_clock::time_point, BatteryInfo>> {
    return impl->getHistory(maxEntries);
}

auto PowerPlanManager::setPowerPlan(PowerPlan plan) -> std::optional<bool> {
#ifdef _WIN32
    GUID planGuid{};
    switch (plan) {
        case PowerPlan::BALANCED:
            planGuid = GUID_TYPICAL_POWER_SAVINGS;
            break;
        case PowerPlan::PERFORMANCE:
            planGuid = GUID_MIN_POWER_SAVINGS;
            break;
        case PowerPlan::POWER_SAVER:
            planGuid = GUID_MAX_POWER_SAVINGS;
            break;
        case PowerPlan::CUSTOM:
            spdlog::error(
                "Setting custom power plans by enum not supported without "
                "GUID");
            return std::nullopt;
        default:
            spdlog::error("Invalid power plan specified for Windows");
            return std::nullopt;
    }

    spdlog::info("Setting Windows power plan");
    DWORD result = PowerSetActiveScheme(NULL, &planGuid);
    if (result != ERROR_SUCCESS) {
        spdlog::error("Failed to set power plan: error {}", result);
        return false;
    }
    spdlog::info("Windows power plan successfully changed");
    return true;

#elif defined(__linux__)
    std::string cmd;
    switch (plan) {
        case PowerPlan::BALANCED:
            cmd = "powerprofilesctl set balanced";
            break;
        case PowerPlan::PERFORMANCE:
            cmd = "powerprofilesctl set performance";
            break;
        case PowerPlan::POWER_SAVER:
            cmd = "powerprofilesctl set power-saver";
            break;
        case PowerPlan::CUSTOM:
            spdlog::error(
                "Custom power plans not settable via powerprofilesctl enum");
            return std::nullopt;
        default:
            spdlog::error("Invalid power plan specified for Linux");
            return std::nullopt;
    }

    spdlog::info("Setting Linux power profile: {}", cmd);
    int result = std::system(cmd.c_str());
    if (WIFEXITED(result) && WEXITSTATUS(result) == 0) {
        spdlog::info("Linux power profile successfully changed");
        return true;
    } else {
        spdlog::error("Failed to set Linux power profile: exit status {}",
                      WEXITSTATUS(result));
        return false;
    }

#elif defined(__APPLE__)
    spdlog::warn("Direct power plan setting not standard on macOS");
    return std::nullopt;
#else
    spdlog::warn("Power plan management not implemented for this platform");
    return std::nullopt;
#endif
}

auto PowerPlanManager::getCurrentPowerPlan() -> std::optional<PowerPlan> {
#ifdef _WIN32
    struct LibraryHandle {
        HMODULE handle;
        explicit LibraryHandle(const wchar_t* name)
            : handle(LoadLibraryW(name)) {}
        ~LibraryHandle() {
            if (handle)
                FreeLibrary(handle);
        }
        operator HMODULE() const { return handle; }
    } hPowrProf{L"powrprof.dll"};

    if (!hPowrProf) {
        spdlog::error("Failed to load powrprof.dll: {}", GetLastError());
        return std::nullopt;
    }

    using PFN_PowerGetActiveScheme = DWORD(WINAPI*)(HKEY, GUID**);
    auto pGetActiveScheme = reinterpret_cast<PFN_PowerGetActiveScheme>(
        GetProcAddress(hPowrProf, "PowerGetActiveScheme"));

    if (!pGetActiveScheme) {
        spdlog::error("Failed to get PowerGetActiveScheme address: {}",
                      GetLastError());
        return std::nullopt;
    }

    GUID* pActiveSchemeGuid = nullptr;
    if (pGetActiveScheme(NULL, &pActiveSchemeGuid) == ERROR_SUCCESS &&
        pActiveSchemeGuid) {
        struct GuidDeleter {
            void operator()(GUID* ptr) {
                if (ptr)
                    LocalFree(ptr);
            }
        };
        std::unique_ptr<GUID, GuidDeleter> activeGuidPtr(pActiveSchemeGuid);

        if (IsEqualGUID(*activeGuidPtr, GUID_MAX_POWER_SAVINGS)) {
            spdlog::debug("Current Windows power plan: Power Saver");
            return PowerPlan::POWER_SAVER;
        } else if (IsEqualGUID(*activeGuidPtr, GUID_TYPICAL_POWER_SAVINGS)) {
            spdlog::debug("Current Windows power plan: Balanced");
            return PowerPlan::BALANCED;
        } else if (IsEqualGUID(*activeGuidPtr, GUID_MIN_POWER_SAVINGS)) {
            spdlog::debug("Current Windows power plan: Performance");
            return PowerPlan::PERFORMANCE;
        } else {
            spdlog::debug("Current Windows power plan: Custom");
            return PowerPlan::CUSTOM;
        }
    } else {
        spdlog::error("Failed to get active power scheme: {}", GetLastError());
        return std::nullopt;
    }

#elif defined(__linux__)
    std::string cmd = "powerprofilesctl get";
    std::string currentProfile;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        spdlog::error("Failed to run 'powerprofilesctl get'");
        return std::nullopt;
    }

    char buffer[128];
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        currentProfile = buffer;
        currentProfile.erase(currentProfile.find_last_not_of(" \n\r\t") + 1);
    }
    pclose(pipe);

    if (currentProfile.empty()) {
        spdlog::warn("Could not determine current power profile");
        return std::nullopt;
    }

    spdlog::debug("Current Linux power profile: {}", currentProfile);
    if (currentProfile == "power-saver") {
        return PowerPlan::POWER_SAVER;
    } else if (currentProfile == "balanced") {
        return PowerPlan::BALANCED;
    } else if (currentProfile == "performance") {
        return PowerPlan::PERFORMANCE;
    } else {
        return PowerPlan::CUSTOM;
    }

#elif defined(__APPLE__)
    spdlog::debug("macOS power management is adaptive; reporting as Balanced");
    return PowerPlan::BALANCED;
#else
    spdlog::warn(
        "Getting current power plan not implemented for this platform");
    return std::nullopt;
#endif
}

auto PowerPlanManager::getAvailablePowerPlans() -> std::vector<std::string> {
    std::vector<std::string> plans;

#ifdef _WIN32
    plans = {"Balanced", "High performance", "Power saver"};
    spdlog::debug("Reporting standard Windows power plans");

#elif defined(__linux__)
    std::string cmd = "powerprofilesctl list";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        spdlog::error("Failed to run 'powerprofilesctl list'");
        plans = {"balanced", "performance", "power-saver"};
        return plans;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line = buffer;
        size_t nameStart = line.find_first_not_of(" *");
        size_t nameEnd = line.find_last_of(":");
        if (nameStart != std::string::npos && nameEnd != std::string::npos &&
            nameStart < nameEnd) {
            plans.push_back(line.substr(nameStart, nameEnd - nameStart));
        }
    }
    pclose(pipe);

    if (plans.empty()) {
        spdlog::warn("Failed to parse powerprofilesctl output, using defaults");
        plans = {"balanced", "performance", "power-saver"};
    }

#elif defined(__APPLE__)
    plans.push_back("Default");
    spdlog::debug("macOS uses automatic power management");
#else
    spdlog::warn(
        "Getting available power plans not implemented for this platform");
    plans.push_back("Default");
#endif

    return plans;
}

}  // namespace atom::system
