#ifndef ATOM_SYSTEM_MODULE_BATTERY_HPP
#define ATOM_SYSTEM_MODULE_BATTERY_HPP

#include <chrono>
#include <functional>
#include <string>
#include <vector>
#include "atom/macro.hpp"

namespace atom::system {
/**
 * @brief Battery information.
 */
struct BatteryInfo {
    bool isBatteryPresent = false;    // Whether the battery is present
    bool isCharging = false;          // Whether the battery is charging
    float batteryLifePercent = 0.0;   // Battery life percentage
    float batteryLifeTime = 0.0;      // Remaining battery life time (minutes)
    float batteryFullLifeTime = 0.0;  // Full battery life time (minutes)
    float energyNow = 0.0;            // Current remaining energy (microjoules)
    float energyFull = 0.0;           // Total battery capacity (microjoules)
    float energyDesign = 0.0;         // Designed battery capacity (microjoules)
    float voltageNow = 0.0;           // Current voltage (volts)
    float currentNow = 0.0;           // Current battery current (amperes)
    float temperature = 0.0;          // Battery temperature in Celsius
    int cycleCounts = 0;              // Battery charge cycle counts
    std::string manufacturer;         // Battery manufacturer
    std::string model;                // Battery model
    std::string serialNumber;         // Battery serial number

    /**
     * @brief Default constructor.
     */
    BatteryInfo() = default;
    BatteryInfo(const BatteryInfo&) = default;

    auto operator==(const BatteryInfo& other) const -> bool {
        return isBatteryPresent == other.isBatteryPresent &&
               isCharging == other.isCharging &&
               batteryLifePercent == other.batteryLifePercent &&
               batteryLifeTime == other.batteryLifeTime &&
               batteryFullLifeTime == other.batteryFullLifeTime &&
               energyNow == other.energyNow && energyFull == other.energyFull &&
               energyDesign == other.energyDesign &&
               voltageNow == other.voltageNow &&
               currentNow == other.currentNow &&
               temperature == other.temperature &&
               cycleCounts == other.cycleCounts &&
               manufacturer == other.manufacturer && model == other.model &&
               serialNumber == other.serialNumber;
    }

    auto operator!=(const BatteryInfo& other) const -> bool {
        return !(*this == other);
    }

    auto operator=(const BatteryInfo& other) -> BatteryInfo& {
        if (this != &other) {
            isBatteryPresent = other.isBatteryPresent;
            isCharging = other.isCharging;
            batteryLifePercent = other.batteryLifePercent;
            batteryLifeTime = other.batteryLifeTime;
            batteryFullLifeTime = other.batteryFullLifeTime;
            energyNow = other.energyNow;
            energyFull = other.energyFull;
            energyDesign = other.energyDesign;
            voltageNow = other.voltageNow;
            currentNow = other.currentNow;
            temperature = other.temperature;
            cycleCounts = other.cycleCounts;
            manufacturer = other.manufacturer;
            model = other.model;
            serialNumber = other.serialNumber;
        }
        return *this;
    }

    /**
     * @brief Calculate battery health (0-100%).
     * @return Battery health percentage.
     */
    [[nodiscard]] auto getBatteryHealth() const -> float {
        if (energyDesign > 0) {
            return (energyFull / energyDesign) * 100.0f;
        }
        return 0.0f;
    }

    /**
     * @brief Estimate remaining usage time (hours).
     * @return Estimated time remaining in hours.
     */
    [[nodiscard]] auto getEstimatedTimeRemaining() const -> float {
        if (currentNow > 0 && !isCharging) {
            return (energyNow / (voltageNow * currentNow));
        }
        return batteryLifeTime / 60.0f;  // Convert to hours
    }
} ATOM_ALIGNAS(64);

/**
 * @brief Get basic battery information.
 * @return BatteryInfo
 */
[[nodiscard("Result of getBatteryInfo is not used")]] BatteryInfo
getBatteryInfo();

/**
 * @brief Get detailed battery information (including manufacturer, model,
 * etc.).
 * @return BatteryInfo
 */
[[nodiscard]] BatteryInfo getDetailedBatteryInfo();

/**
 * @brief Monitor battery status changes.
 */
class BatteryMonitor {
public:
    using BatteryCallback = std::function<void(const BatteryInfo&)>;

    /**
     * @brief Start monitoring battery status.
     * @param callback Callback function to handle battery status updates.
     * @param interval_ms Monitoring interval in milliseconds.
     */
    static void startMonitoring(BatteryCallback callback,
                                unsigned int interval_ms = 1000);

    /**
     * @brief Stop monitoring battery status.
     */
    static void stopMonitoring();
};

/**
 * @brief 电池警报设置
 */
struct BatteryAlertSettings {
    float lowBatteryThreshold = 20.0f;      // 低电量警告阈值
    float criticalBatteryThreshold = 5.0f;  // 严重低电量警告阈值
    float highTempThreshold = 45.0f;        // 高温警告阈值
    float lowHealthThreshold = 60.0f;       // 低健康度警告阈值
};

/**
 * @brief 电池使用统计
 */
struct BatteryStats {
    float averagePowerConsumption = 0.0f;  // 平均功耗(W)
    float totalEnergyConsumed = 0.0f;      // 总消耗能量(Wh)
    float averageDischargeRate = 0.0f;     // 平均放电速率(%/h)
    std::chrono::seconds totalUptime{0};   // 总使用时间
    float minBatteryLevel = 100.0f;        // 最低电量水平
    float maxBatteryLevel = 0.0f;          // 最高电量水平
    float minTemperature = 100.0f;         // 最低温度
    float maxTemperature = 0.0f;           // 最高温度
    float minVoltage = 100.0f;             // 最低电压
    float maxVoltage = 0.0f;               // 最高电压
    float avgDischargeRate = -1.0f;        // 平均放电速率
    int cycleCount = 0;                    // 充电循环次数
    float batteryHealth = 100.0f;          // 电池健康度
};

/**
 * @brief 电池管理器
 */
class BatteryManager {
public:
    using AlertCallback =
        std::function<void(const std::string& alert, const BatteryInfo&)>;

    static BatteryManager& getInstance();

    // 设置警报回调
    void setAlertCallback(AlertCallback callback);

    // 配置警报阈值
    void setAlertSettings(const BatteryAlertSettings& settings);

    // 获取电池使用统计
    [[nodiscard]] BatteryStats getStats() const;

    // 开始记录电池历史数据
    void startRecording(const std::string& logFile = "");

    // 停止记录
    void stopRecording();

    // 开始监控
    void startMonitoring(unsigned int interval_ms = 10000);

    // 停止监控
    void stopMonitoring();

    // 获取历史数据
    [[nodiscard]] std::vector<
        std::pair<std::chrono::system_clock::time_point, BatteryInfo>>
    getHistory(unsigned int maxEntries = 0) const;

private:
    BatteryManager();
    ~BatteryManager();
    BatteryManager(const BatteryManager&) = delete;
    BatteryManager& operator=(const BatteryManager&) = delete;

    class BatteryManagerImpl;
    BatteryManagerImpl* impl;
};

// 电池电源计划管理
enum class PowerPlan { BALANCED, PERFORMANCE, POWER_SAVER, CUSTOM };

/**
 * @brief 电源计划管理器
 */
class PowerPlanManager {
public:
    static bool setPowerPlan(PowerPlan plan);
    static PowerPlan getCurrentPowerPlan();
    static std::vector<std::string> getAvailablePowerPlans();
};

}  // namespace atom::system
#endif
