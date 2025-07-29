/*
 * quality.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Network Quality Analysis

**************************************************/

#ifndef ATOM_SYSTEM_MODULE_WIFI_QUALITY_HPP
#define ATOM_SYSTEM_MODULE_WIFI_QUALITY_HPP

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "wifi.hpp"
#include "common.hpp"

namespace atom::system {

/**
 * @brief Detailed network quality metrics
 */
struct QualityMetrics {
    double jitter_ms;                    // Network jitter in milliseconds
    double packet_loss_percentage;       // Packet loss percentage
    double throughput_mbps;              // Actual throughput in Mbps
    double connection_stability;         // Stability score (0.0 to 1.0)
    double signal_quality;               // Signal quality score (0.0 to 1.0)
    double overall_quality;              // Overall quality score (0.0 to 1.0)
    std::chrono::steady_clock::time_point measurement_time;
} ATOM_ALIGNAS(32);

/**
 * @brief Network quality assessment levels
 */
enum class QualityLevel {
    EXCELLENT,
    GOOD,
    FAIR,
    POOR,
    CRITICAL
};

/**
 * @brief Jitter measurement configuration
 */
struct JitterConfig {
    int packet_count{10};                // Number of packets to send
    std::chrono::milliseconds interval{100};  // Interval between packets
    std::string target_host{"8.8.8.8"};  // Target host for measurement
    int timeout_ms{5000};                // Timeout for each packet
} ATOM_ALIGNAS(16);

/**
 * @brief Throughput test configuration
 */
struct ThroughputConfig {
    size_t test_duration_seconds{10};    // Duration of throughput test
    size_t concurrent_connections{4};    // Number of concurrent connections
    std::string test_server{"speedtest.net"};  // Test server URL
    size_t buffer_size{65536};           // Buffer size for data transfer
} ATOM_ALIGNAS(16);

/**
 * @brief Packet loss test configuration
 */
struct PacketLossConfig {
    int packet_count{100};               // Number of packets to send
    std::chrono::milliseconds timeout{1000};  // Timeout for each packet
    std::string target_host{"8.8.8.8"};  // Target host for testing
} ATOM_ALIGNAS(16);

/**
 * @brief Network quality analyzer class
 */
class NetworkQualityAnalyzer {
public:
    NetworkQualityAnalyzer() = default;
    ~NetworkQualityAnalyzer() = default;

    // Disable copy constructor and assignment
    NetworkQualityAnalyzer(const NetworkQualityAnalyzer&) = delete;
    NetworkQualityAnalyzer& operator=(const NetworkQualityAnalyzer&) = delete;

    // Enable move constructor and assignment
    NetworkQualityAnalyzer(NetworkQualityAnalyzer&&) noexcept = default;
    NetworkQualityAnalyzer& operator=(NetworkQualityAnalyzer&&) noexcept = default;

    /**
     * @brief Measure network jitter
     * @param config Jitter measurement configuration
     * @return Jitter in milliseconds, or -1.0 on error
     */
    ATOM_NODISCARD auto measureJitter(const JitterConfig& config = JitterConfig{}) -> double;

    /**
     * @brief Measure packet loss percentage
     * @param config Packet loss test configuration
     * @return Packet loss percentage (0.0 to 100.0), or -1.0 on error
     */
    ATOM_NODISCARD auto measurePacketLoss(const PacketLossConfig& config = PacketLossConfig{}) -> double;

    /**
     * @brief Measure actual network throughput
     * @param config Throughput test configuration
     * @return Throughput in Mbps, or -1.0 on error
     */
    ATOM_NODISCARD auto measureThroughput(const ThroughputConfig& config = ThroughputConfig{}) -> double;

    /**
     * @brief Assess connection stability over time
     * @param duration Duration to monitor stability
     * @return Stability score (0.0 to 1.0), or -1.0 on error
     */
    ATOM_NODISCARD auto assessConnectionStability(std::chrono::minutes duration = std::chrono::minutes{5}) -> double;

    /**
     * @brief Calculate signal quality score
     * @param signal_strength Signal strength in dBm
     * @return Signal quality score (0.0 to 1.0)
     */
    ATOM_NODISCARD static auto calculateSignalQuality(double signal_strength) -> double;

    /**
     * @brief Perform comprehensive quality analysis
     * @param jitter_config Jitter measurement configuration
     * @param throughput_config Throughput test configuration
     * @param packet_loss_config Packet loss test configuration
     * @return Complete quality metrics
     */
    ATOM_NODISCARD auto performComprehensiveAnalysis(
        const JitterConfig& jitter_config = JitterConfig{},
        const ThroughputConfig& throughput_config = ThroughputConfig{},
        const PacketLossConfig& packet_loss_config = PacketLossConfig{}) -> QualityMetrics;

    /**
     * @brief Convert quality metrics to quality level
     * @param metrics Quality metrics to evaluate
     * @return Quality level assessment
     */
    ATOM_NODISCARD static auto getQualityLevel(const QualityMetrics& metrics) -> QualityLevel;

    /**
     * @brief Generate detailed quality report
     * @param metrics Quality metrics to report on
     * @return Formatted quality report string
     */
    ATOM_NODISCARD static auto generateQualityReport(const QualityMetrics& metrics) -> std::string;

    /**
     * @brief Get quality recommendations based on metrics
     * @param metrics Quality metrics to analyze
     * @return Vector of recommendation strings
     */
    ATOM_NODISCARD static auto getQualityRecommendations(const QualityMetrics& metrics) -> std::vector<std::string>;

private:
    /**
     * @brief Calculate overall quality score from individual metrics
     * @param jitter Jitter in milliseconds
     * @param packet_loss Packet loss percentage
     * @param throughput Throughput in Mbps
     * @param stability Connection stability score
     * @param signal_quality Signal quality score
     * @return Overall quality score (0.0 to 1.0)
     */
    ATOM_NODISCARD static auto calculateOverallQuality(double jitter, double packet_loss,
                                                       double throughput, double stability,
                                                       double signal_quality) -> double;

    /**
     * @brief Normalize jitter to quality score
     * @param jitter_ms Jitter in milliseconds
     * @return Quality score (0.0 to 1.0)
     */
    ATOM_NODISCARD static auto normalizeJitter(double jitter_ms) -> double;

    /**
     * @brief Normalize packet loss to quality score
     * @param packet_loss_percent Packet loss percentage
     * @return Quality score (0.0 to 1.0)
     */
    ATOM_NODISCARD static auto normalizePacketLoss(double packet_loss_percent) -> double;

    /**
     * @brief Normalize throughput to quality score
     * @param throughput_mbps Throughput in Mbps
     * @return Quality score (0.0 to 1.0)
     */
    ATOM_NODISCARD static auto normalizeThroughput(double throughput_mbps) -> double;
};

/**
 * @brief Network quality monitor for continuous assessment
 */
class NetworkQualityMonitor {
public:
    explicit NetworkQualityMonitor(std::chrono::seconds assessment_interval = std::chrono::seconds{60});
    ~NetworkQualityMonitor();

    // Disable copy constructor and assignment
    NetworkQualityMonitor(const NetworkQualityMonitor&) = delete;
    NetworkQualityMonitor& operator=(const NetworkQualityMonitor&) = delete;

    // Move constructor and assignment are implicitly deleted due to unique_ptr and atomic members

    /**
     * @brief Start continuous quality monitoring
     * @return True if monitoring started successfully
     */
    ATOM_NODISCARD auto start() -> bool;

    /**
     * @brief Stop quality monitoring
     */
    void stop();

    /**
     * @brief Check if monitoring is active
     * @return True if monitoring is running
     */
    ATOM_NODISCARD auto isRunning() const -> bool;

    /**
     * @brief Get the latest quality metrics
     * @return Latest quality metrics, or nullopt if not available
     */
    ATOM_NODISCARD auto getLatestMetrics() const -> std::optional<QualityMetrics>;

    /**
     * @brief Get quality metrics history
     * @param duration How far back to retrieve history
     * @return Vector of quality metrics
     */
    ATOM_NODISCARD auto getQualityHistory(std::chrono::hours duration = std::chrono::hours{24}) const -> std::vector<QualityMetrics>;

    /**
     * @brief Set callback for quality alerts
     * @param callback Function to call when quality drops below threshold
     */
    void setQualityAlertCallback(std::function<void(const QualityMetrics&, QualityLevel)> callback);

    /**
     * @brief Set quality alert threshold
     * @param level Quality level that triggers alerts
     */
    void setAlertThreshold(QualityLevel level);

private:
    void monitoringLoop();
    void processQualityMetrics(const QualityMetrics& metrics);

    NetworkQualityAnalyzer analyzer_;
    std::chrono::seconds assessment_interval_;
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> monitor_thread_;

    mutable std::mutex metrics_mutex_;
    std::vector<QualityMetrics> quality_history_;
    std::optional<QualityMetrics> latest_metrics_;

    std::function<void(const QualityMetrics&, QualityLevel)> alert_callback_;
    QualityLevel alert_threshold_{QualityLevel::POOR};
};

/**
 * @brief Convert quality level to string
 * @param level Quality level to convert
 * @return String representation of quality level
 */
ATOM_NODISCARD auto qualityLevelToString(QualityLevel level) -> std::string;

/**
 * @brief Get global network quality monitor instance
 * @return Reference to global quality monitor
 */
ATOM_NODISCARD auto getGlobalQualityMonitor() -> NetworkQualityMonitor&;

} // namespace atom::system

#endif // ATOM_SYSTEM_MODULE_WIFI_QUALITY_HPP
