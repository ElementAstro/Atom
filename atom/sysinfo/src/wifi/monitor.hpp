/*
 * monitor.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Network Monitoring and Real-time Statistics

**************************************************/

#ifndef ATOM_SYSTEM_MODULE_WIFI_MONITOR_HPP
#define ATOM_SYSTEM_MODULE_WIFI_MONITOR_HPP

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>

#include "wifi.hpp"
#include "common.hpp"

namespace atom::system {

/**
 * @brief Network event types for monitoring
 */
enum class NetworkEvent {
    CONNECTION_ESTABLISHED,
    CONNECTION_LOST,
    SIGNAL_STRENGTH_CHANGED,
    BANDWIDTH_CHANGED,
    NEW_DEVICE_CONNECTED,
    DEVICE_DISCONNECTED,
    NETWORK_QUALITY_CHANGED
};

/**
 * @brief Network event data structure
 */
struct NetworkEventData {
    NetworkEvent event_type;
    std::chrono::steady_clock::time_point timestamp;
    std::string description;
    std::unordered_map<std::string, std::string> metadata;
} ATOM_ALIGNAS(32);

/**
 * @brief Network statistics with timestamp for historical tracking
 */
struct TimestampedNetworkStats {
    NetworkStats stats;
    std::chrono::steady_clock::time_point timestamp;

    TimestampedNetworkStats() : timestamp(std::chrono::steady_clock::now()) {}
    explicit TimestampedNetworkStats(const NetworkStats& s)
        : stats(s), timestamp(std::chrono::steady_clock::now()) {}
} ATOM_ALIGNAS(32);

/**
 * @brief Network monitoring configuration
 */
struct MonitorConfig {
    std::chrono::milliseconds polling_interval{5000};
    size_t max_history_size{1000};
    bool enable_event_callbacks{true};
    bool enable_auto_quality_analysis{true};
    double signal_threshold_change{5.0};  // dBm
    double bandwidth_threshold_change{1.0};  // Mbps
} ATOM_ALIGNAS(32);

/**
 * @brief Callback function type for network events
 */
using NetworkEventCallback = std::function<void(const NetworkEventData&)>;

/**
 * @brief Real-time network monitor class
 */
class NetworkMonitor {
public:
    explicit NetworkMonitor(const MonitorConfig& config = MonitorConfig{});
    ~NetworkMonitor();

    // Disable copy constructor and assignment
    NetworkMonitor(const NetworkMonitor&) = delete;
    NetworkMonitor& operator=(const NetworkMonitor&) = delete;

    // Enable move constructor and assignment
    NetworkMonitor(NetworkMonitor&&) noexcept = default;
    NetworkMonitor& operator=(NetworkMonitor&&) noexcept = default;

    /**
     * @brief Start monitoring network statistics
     * @return True if monitoring started successfully
     */
    ATOM_NODISCARD auto start() -> bool;

    /**
     * @brief Stop monitoring network statistics
     */
    void stop();

    /**
     * @brief Check if monitoring is currently active
     * @return True if monitoring is active
     */
    ATOM_NODISCARD auto isRunning() const -> bool;

    /**
     * @brief Register a callback for network events
     * @param callback Function to call when events occur
     */
    void registerEventCallback(NetworkEventCallback callback);

    /**
     * @brief Get current network statistics
     * @return Current network statistics
     */
    ATOM_NODISCARD auto getCurrentStats() const -> NetworkStats;

    /**
     * @brief Get network statistics history
     * @param duration How far back to retrieve history
     * @return Vector of timestamped network statistics
     */
    ATOM_NODISCARD auto getStatsHistory(std::chrono::minutes duration) const -> std::vector<TimestampedNetworkStats>;

    /**
     * @brief Get recent network events
     * @param duration How far back to retrieve events
     * @return Vector of network events
     */
    ATOM_NODISCARD auto getRecentEvents(std::chrono::minutes duration) const -> std::vector<NetworkEventData>;

    /**
     * @brief Get average statistics over a time period
     * @param duration Time period to average over
     * @return Averaged network statistics
     */
    ATOM_NODISCARD auto getAverageStats(std::chrono::minutes duration) const -> NetworkStats;

    /**
     * @brief Get peak statistics over a time period
     * @param duration Time period to analyze
     * @return Peak network statistics
     */
    ATOM_NODISCARD auto getPeakStats(std::chrono::minutes duration) const -> NetworkStats;

    /**
     * @brief Clear all historical data
     */
    void clearHistory();

    /**
     * @brief Update monitoring configuration
     * @param config New configuration
     */
    void updateConfig(const MonitorConfig& config);

    /**
     * @brief Get current monitoring configuration
     * @return Current configuration
     */
    ATOM_NODISCARD auto getConfig() const -> MonitorConfig;

private:
    void monitoringLoop();
    void processStats(const NetworkStats& current_stats);
    void detectAndEmitEvents(const NetworkStats& current_stats, const NetworkStats& previous_stats);
    void emitEvent(NetworkEvent event_type, const std::string& description,
                   const std::unordered_map<std::string, std::string>& metadata = {});

    MonitorConfig config_;
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> monitor_thread_;

    mutable std::mutex stats_mutex_;
    std::queue<TimestampedNetworkStats> stats_history_;
    NetworkStats current_stats_;
    NetworkStats previous_stats_;

    mutable std::mutex events_mutex_;
    std::queue<NetworkEventData> recent_events_;

    mutable std::mutex callbacks_mutex_;
    std::vector<NetworkEventCallback> event_callbacks_;
};

/**
 * @brief Network performance analyzer
 */
class NetworkPerformanceAnalyzer {
public:
    /**
     * @brief Analyze network performance trends
     * @param history Historical network statistics
     * @return Performance analysis report
     */
    ATOM_NODISCARD static auto analyzePerformanceTrends(const std::vector<TimestampedNetworkStats>& history) -> std::string;

    /**
     * @brief Detect network anomalies
     * @param history Historical network statistics
     * @return List of detected anomalies
     */
    ATOM_NODISCARD static auto detectAnomalies(const std::vector<TimestampedNetworkStats>& history) -> std::vector<std::string>;

    /**
     * @brief Calculate network stability score
     * @param history Historical network statistics
     * @return Stability score (0.0 to 1.0, higher is more stable)
     */
    ATOM_NODISCARD static auto calculateStabilityScore(const std::vector<TimestampedNetworkStats>& history) -> double;

    /**
     * @brief Generate network quality report
     * @param history Historical network statistics
     * @return Detailed quality report
     */
    ATOM_NODISCARD static auto generateQualityReport(const std::vector<TimestampedNetworkStats>& history) -> std::string;
};

/**
 * @brief Global network monitor instance
 * @return Reference to the global network monitor
 */
ATOM_NODISCARD auto getGlobalNetworkMonitor() -> NetworkMonitor&;

/**
 * @brief Initialize global network monitoring with custom configuration
 * @param config Monitoring configuration
 * @return True if initialization was successful
 */
ATOM_NODISCARD auto initializeNetworkMonitoring(const MonitorConfig& config = MonitorConfig{}) -> bool;

/**
 * @brief Shutdown global network monitoring
 */
void shutdownNetworkMonitoring();

} // namespace atom::system

#endif // ATOM_SYSTEM_MODULE_WIFI_MONITOR_HPP
