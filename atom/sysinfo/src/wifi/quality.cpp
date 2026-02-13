/*
 * quality.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Network Quality Analysis Implementation

**************************************************/

#include "quality.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <thread>

namespace atom::system {

auto NetworkQualityAnalyzer::measureJitter(const JitterConfig& config) -> double {
    LOG_F(INFO, "Measuring network jitter with {} packets to {}", config.packet_count, config.target_host);

    std::vector<double> latencies;
    latencies.reserve(config.packet_count);

    for (int i = 0; i < config.packet_count; ++i) {
        auto [latency, error] = measurePing(config.target_host, config.timeout_ms);
        if (error == WiFiError::SUCCESS && latency >= 0) {
            latencies.push_back(latency);
        }

        if (i < config.packet_count - 1) {
            std::this_thread::sleep_for(config.interval);
        }
    }

    if (latencies.size() < 2) {
        LOG_F(ERROR, "Insufficient data for jitter calculation (got {} valid measurements)", latencies.size());
        return -1.0;
    }

    // Calculate jitter as the average absolute difference between consecutive latencies
    double total_jitter = 0.0;
    for (size_t i = 1; i < latencies.size(); ++i) {
        total_jitter += std::abs(latencies[i] - latencies[i-1]);
    }

    double jitter = total_jitter / (latencies.size() - 1);
    LOG_F(INFO, "Measured jitter: {:.2f}ms from {} samples", jitter, latencies.size());

    return jitter;
}

auto NetworkQualityAnalyzer::measurePacketLoss(const PacketLossConfig& config) -> double {
    LOG_F(INFO, "Measuring packet loss with {} packets to {}", config.packet_count, config.target_host);

    int successful_pings = 0;

    for (int i = 0; i < config.packet_count; ++i) {
        auto [latency, error] = measurePing(config.target_host, config.timeout.count());
        if (error == WiFiError::SUCCESS && latency >= 0) {
            successful_pings++;
        }
    }

    double packet_loss = ((config.packet_count - successful_pings) * 100.0) / config.packet_count;
    LOG_F(INFO, "Measured packet loss: {:.2f}% ({}/{} packets lost)",
          packet_loss, config.packet_count - successful_pings, config.packet_count);

    return packet_loss;
}

auto NetworkQualityAnalyzer::measureThroughput(const ThroughputConfig& config) -> double {
    LOG_F(INFO, "Measuring network throughput for {} seconds", config.test_duration_seconds);

    // For a real implementation, you would:
    // 1. Download/upload test data from/to a speed test server
    // 2. Measure actual bytes transferred over time
    // 3. Calculate throughput in Mbps

    // Placeholder implementation using current network stats
    auto stats = getNetworkStats();
    double estimated_throughput = std::max(stats.downloadSpeed, stats.uploadSpeed) * 8.0; // Convert MB/s to Mbps

    LOG_F(INFO, "Estimated throughput: {:.2f} Mbps", estimated_throughput);
    return estimated_throughput;
}

auto NetworkQualityAnalyzer::assessConnectionStability(std::chrono::minutes duration) -> double {
    LOG_F(INFO, "Assessing connection stability over {} minutes", duration.count());

    std::vector<double> latencies;
    auto start_time = std::chrono::steady_clock::now();
    auto end_time = start_time + duration;

    // Sample latency every 10 seconds
    while (std::chrono::steady_clock::now() < end_time) {
        auto [latency, error] = measurePing("8.8.8.8", 3000);
        if (error == WiFiError::SUCCESS && latency >= 0) {
            latencies.push_back(latency);
        }

        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    if (latencies.size() < 2) {
        LOG_F(ERROR, "Insufficient data for stability assessment");
        return -1.0;
    }

    // Calculate coefficient of variation (lower is more stable)
    double mean = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
    double variance = 0.0;
    for (double latency : latencies) {
        variance += (latency - mean) * (latency - mean);
    }
    variance /= latencies.size();
    double stddev = std::sqrt(variance);

    double cv = (mean > 0) ? (stddev / mean) : 1.0;
    double stability = std::max(0.0, 1.0 - std::min(1.0, cv));

    LOG_F(INFO, "Connection stability score: {:.2f} (CV: {:.3f})", stability, cv);
    return stability;
}

auto NetworkQualityAnalyzer::calculateSignalQuality(double signal_strength) -> double {
    // Convert signal strength (dBm) to quality score
    // Typical WiFi signal ranges: -30 dBm (excellent) to -90 dBm (unusable)

    if (signal_strength >= -30) {
        return 1.0; // Excellent
    } else if (signal_strength >= -50) {
        return 0.8 + 0.2 * (signal_strength + 50) / 20; // Good to Excellent
    } else if (signal_strength >= -70) {
        return 0.5 + 0.3 * (signal_strength + 70) / 20; // Fair to Good
    } else if (signal_strength >= -80) {
        return 0.2 + 0.3 * (signal_strength + 80) / 10; // Poor to Fair
    } else if (signal_strength >= -90) {
        return 0.1 * (signal_strength + 90) / 10; // Very Poor
    } else {
        return 0.0; // Unusable
    }
}

auto NetworkQualityAnalyzer::performComprehensiveAnalysis(
    const JitterConfig& jitter_config,
    const ThroughputConfig& throughput_config,
    const PacketLossConfig& packet_loss_config) -> QualityMetrics {

    LOG_F(INFO, "Performing comprehensive network quality analysis");

    QualityMetrics metrics{};
    metrics.measurement_time = std::chrono::steady_clock::now();

    // Measure jitter
    metrics.jitter_ms = measureJitter(jitter_config);

    // Measure packet loss
    metrics.packet_loss_percentage = measurePacketLoss(packet_loss_config);

    // Measure throughput
    metrics.throughput_mbps = measureThroughput(throughput_config);

    // Assess connection stability (shorter duration for comprehensive test)
    metrics.connection_stability = assessConnectionStability(std::chrono::minutes{2});

    // Get signal quality
    auto stats = getNetworkStats();
    metrics.signal_quality = calculateSignalQuality(stats.signalStrength);

    // Calculate overall quality
    metrics.overall_quality = calculateOverallQuality(
        metrics.jitter_ms,
        metrics.packet_loss_percentage,
        metrics.throughput_mbps,
        metrics.connection_stability,
        metrics.signal_quality
    );

    LOG_F(INFO, "Comprehensive analysis completed - Overall quality: {:.2f}", metrics.overall_quality);
    return metrics;
}

auto NetworkQualityAnalyzer::getQualityLevel(const QualityMetrics& metrics) -> QualityLevel {
    if (metrics.overall_quality >= 0.8) {
        return QualityLevel::EXCELLENT;
    } else if (metrics.overall_quality >= 0.6) {
        return QualityLevel::GOOD;
    } else if (metrics.overall_quality >= 0.4) {
        return QualityLevel::FAIR;
    } else if (metrics.overall_quality >= 0.2) {
        return QualityLevel::POOR;
    } else {
        return QualityLevel::CRITICAL;
    }
}

auto NetworkQualityAnalyzer::generateQualityReport(const QualityMetrics& metrics) -> std::string {
    std::ostringstream report;
    report << std::fixed << std::setprecision(2);

    report << "Network Quality Analysis Report\n";
    report << "===============================\n\n";

    // Overall assessment
    auto level = getQualityLevel(metrics);
    report << "Overall Quality: " << qualityLevelToString(level)
           << " (" << (metrics.overall_quality * 100) << "%)\n\n";

    // Detailed metrics
    report << "Detailed Metrics:\n";
    report << "- Jitter: " << metrics.jitter_ms << " ms\n";
    report << "- Packet Loss: " << metrics.packet_loss_percentage << "%\n";
    report << "- Throughput: " << metrics.throughput_mbps << " Mbps\n";
    report << "- Connection Stability: " << (metrics.connection_stability * 100) << "%\n";
    report << "- Signal Quality: " << (metrics.signal_quality * 100) << "%\n\n";

    // Quality assessment for each metric
    report << "Quality Assessment:\n";
    report << "- Jitter: " << (metrics.jitter_ms < 5 ? "Excellent" :
                               metrics.jitter_ms < 15 ? "Good" :
                               metrics.jitter_ms < 30 ? "Fair" : "Poor") << "\n";
    report << "- Packet Loss: " << (metrics.packet_loss_percentage < 0.1 ? "Excellent" :
                                   metrics.packet_loss_percentage < 1.0 ? "Good" :
                                   metrics.packet_loss_percentage < 3.0 ? "Fair" : "Poor") << "\n";
    report << "- Throughput: " << (metrics.throughput_mbps > 100 ? "Excellent" :
                                  metrics.throughput_mbps > 25 ? "Good" :
                                  metrics.throughput_mbps > 5 ? "Fair" : "Poor") << "\n";

    return report.str();
}

auto NetworkQualityAnalyzer::getQualityRecommendations(const QualityMetrics& metrics) -> std::vector<std::string> {
    std::vector<std::string> recommendations;

    if (metrics.jitter_ms > 30) {
        recommendations.push_back("High jitter detected. Consider using a wired connection or moving closer to the router.");
    }

    if (metrics.packet_loss_percentage > 3.0) {
        recommendations.push_back("Significant packet loss detected. Check network congestion and signal strength.");
    }

    if (metrics.throughput_mbps < 5) {
        recommendations.push_back("Low throughput detected. Consider upgrading your internet plan or checking for network issues.");
    }

    if (metrics.connection_stability < 0.5) {
        recommendations.push_back("Unstable connection detected. Check for interference and consider repositioning your router.");
    }

    if (metrics.signal_quality < 0.3) {
        recommendations.push_back("Poor signal quality. Move closer to the router or consider using a WiFi extender.");
    }

    if (recommendations.empty()) {
        recommendations.push_back("Network quality is good. No immediate action required.");
    }

    return recommendations;
}

auto NetworkQualityAnalyzer::calculateOverallQuality(double jitter, double packet_loss,
                                                     double throughput, double stability,
                                                     double signal_quality) -> double {
    // Normalize individual metrics to 0-1 scale
    double jitter_score = normalizeJitter(jitter);
    double packet_loss_score = normalizePacketLoss(packet_loss);
    double throughput_score = normalizeThroughput(throughput);

    // Weighted average of all quality factors
    double weights[] = {0.2, 0.25, 0.25, 0.15, 0.15}; // jitter, packet_loss, throughput, stability, signal
    double scores[] = {jitter_score, packet_loss_score, throughput_score, stability, signal_quality};

    double weighted_sum = 0.0;
    double total_weight = 0.0;

    for (int i = 0; i < 5; ++i) {
        if (scores[i] >= 0) { // Only include valid scores
            weighted_sum += weights[i] * scores[i];
            total_weight += weights[i];
        }
    }

    return (total_weight > 0) ? (weighted_sum / total_weight) : 0.0;
}

auto NetworkQualityAnalyzer::normalizeJitter(double jitter_ms) -> double {
    if (jitter_ms < 0) return -1.0; // Invalid measurement

    // Jitter normalization: 0ms = 1.0, 50ms+ = 0.0
    return std::max(0.0, 1.0 - (jitter_ms / 50.0));
}

auto NetworkQualityAnalyzer::normalizePacketLoss(double packet_loss_percent) -> double {
    if (packet_loss_percent < 0) return -1.0; // Invalid measurement

    // Packet loss normalization: 0% = 1.0, 10%+ = 0.0
    return std::max(0.0, 1.0 - (packet_loss_percent / 10.0));
}

auto NetworkQualityAnalyzer::normalizeThroughput(double throughput_mbps) -> double {
    if (throughput_mbps < 0) return -1.0; // Invalid measurement

    // Throughput normalization: 100+ Mbps = 1.0, 0 Mbps = 0.0
    return std::min(1.0, throughput_mbps / 100.0);
}

// NetworkQualityMonitor implementation
NetworkQualityMonitor::NetworkQualityMonitor(std::chrono::seconds assessment_interval)
    : assessment_interval_(assessment_interval) {
    LOG_F(INFO, "NetworkQualityMonitor created with {}s assessment interval", assessment_interval.count());
}

NetworkQualityMonitor::~NetworkQualityMonitor() {
    stop();
}

auto NetworkQualityMonitor::start() -> bool {
    if (running_.load()) {
        LOG_F(WARNING, "NetworkQualityMonitor is already running");
        return true;
    }

    LOG_F(INFO, "Starting network quality monitoring");
    running_.store(true);

    try {
        monitor_thread_ = std::make_unique<std::thread>(&NetworkQualityMonitor::monitoringLoop, this);
        LOG_F(INFO, "Network quality monitoring started successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to start network quality monitoring: {}", e.what());
        running_.store(false);
        return false;
    }
}

void NetworkQualityMonitor::stop() {
    if (!running_.load()) {
        return;
    }

    LOG_F(INFO, "Stopping network quality monitoring");
    running_.store(false);

    if (monitor_thread_ && monitor_thread_->joinable()) {
        monitor_thread_->join();
    }
    monitor_thread_.reset();

    LOG_F(INFO, "Network quality monitoring stopped");
}

auto NetworkQualityMonitor::isRunning() const -> bool {
    return running_.load();
}

auto NetworkQualityMonitor::getLatestMetrics() const -> std::optional<QualityMetrics> {
    std::lock_guard lock(metrics_mutex_);
    return latest_metrics_;
}

auto NetworkQualityMonitor::getQualityHistory(std::chrono::hours duration) const -> std::vector<QualityMetrics> {
    std::lock_guard lock(metrics_mutex_);
    std::vector<QualityMetrics> result;

    auto cutoff_time = std::chrono::steady_clock::now() - duration;

    for (const auto& metrics : quality_history_) {
        if (metrics.measurement_time >= cutoff_time) {
            result.push_back(metrics);
        }
    }

    return result;
}

void NetworkQualityMonitor::setQualityAlertCallback(std::function<void(const QualityMetrics&, QualityLevel)> callback) {
    alert_callback_ = std::move(callback);
    LOG_F(INFO, "Quality alert callback registered");
}

void NetworkQualityMonitor::setAlertThreshold(QualityLevel level) {
    alert_threshold_ = level;
    LOG_F(INFO, "Quality alert threshold set to: {}", qualityLevelToString(level));
}

void NetworkQualityMonitor::monitoringLoop() {
    LOG_F(INFO, "Network quality monitoring loop started");

    while (running_.load()) {
        try {
            // Perform lightweight quality assessment
            JitterConfig jitter_config;
            jitter_config.packet_count = 5; // Reduced for continuous monitoring

            PacketLossConfig packet_loss_config;
            packet_loss_config.packet_count = 20; // Reduced for continuous monitoring

            ThroughputConfig throughput_config;
            throughput_config.test_duration_seconds = 3; // Reduced for continuous monitoring

            auto metrics = analyzer_.performComprehensiveAnalysis(jitter_config, throughput_config, packet_loss_config);
            processQualityMetrics(metrics);

            std::this_thread::sleep_for(assessment_interval_);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error in quality monitoring loop: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    LOG_F(INFO, "Network quality monitoring loop ended");
}

void NetworkQualityMonitor::processQualityMetrics(const QualityMetrics& metrics) {
    {
        std::lock_guard lock(metrics_mutex_);
        latest_metrics_ = metrics;
        quality_history_.push_back(metrics);

        // Maintain history size (keep last 24 hours worth of data)
        auto cutoff_time = std::chrono::steady_clock::now() - std::chrono::hours(24);
        quality_history_.erase(
            std::remove_if(quality_history_.begin(), quality_history_.end(),
                          [cutoff_time](const QualityMetrics& m) {
                              return m.measurement_time < cutoff_time;
                          }),
            quality_history_.end());
    }

    // Check for quality alerts
    if (alert_callback_) {
        auto level = NetworkQualityAnalyzer::getQualityLevel(metrics);
        if (level >= alert_threshold_) {
            try {
                alert_callback_(metrics, level);
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Error in quality alert callback: {}", e.what());
            }
        }
    }
}

auto qualityLevelToString(QualityLevel level) -> std::string {
    switch (level) {
        case QualityLevel::EXCELLENT: return "Excellent";
        case QualityLevel::GOOD: return "Good";
        case QualityLevel::FAIR: return "Fair";
        case QualityLevel::POOR: return "Poor";
        case QualityLevel::CRITICAL: return "Critical";
        default: return "Unknown";
    }
}

// Global quality monitor instance
static std::unique_ptr<NetworkQualityMonitor> global_quality_monitor;
static std::mutex global_quality_monitor_mutex;

auto getGlobalQualityMonitor() -> NetworkQualityMonitor& {
    std::lock_guard lock(global_quality_monitor_mutex);
    if (!global_quality_monitor) {
        global_quality_monitor = std::make_unique<NetworkQualityMonitor>();
    }
    return *global_quality_monitor;
}

} // namespace atom::system
