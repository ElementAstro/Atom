/*
 * monitor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: System Information Module - Network Monitoring Implementation

**************************************************/

#include "monitor.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace atom::system {

NetworkMonitor::NetworkMonitor(const MonitorConfig& config) : config_(config) {
    LOG_F(INFO, "NetworkMonitor created with polling interval: {}ms", config_.polling_interval.count());
}

NetworkMonitor::~NetworkMonitor() {
    stop();
}

auto NetworkMonitor::start() -> bool {
    if (running_.load()) {
        LOG_F(WARNING, "NetworkMonitor is already running");
        return true;
    }

    LOG_F(INFO, "Starting network monitoring");
    running_.store(true);

    try {
        monitor_thread_ = std::make_unique<std::thread>(&NetworkMonitor::monitoringLoop, this);
        LOG_F(INFO, "Network monitoring started successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to start network monitoring: {}", e.what());
        running_.store(false);
        return false;
    }
}

void NetworkMonitor::stop() {
    if (!running_.load()) {
        return;
    }

    LOG_F(INFO, "Stopping network monitoring");
    running_.store(false);

    if (monitor_thread_ && monitor_thread_->joinable()) {
        monitor_thread_->join();
    }
    monitor_thread_.reset();

    LOG_F(INFO, "Network monitoring stopped");
}

auto NetworkMonitor::isRunning() const -> bool {
    return running_.load();
}

void NetworkMonitor::registerEventCallback(NetworkEventCallback callback) {
    std::lock_guard lock(callbacks_mutex_);
    event_callbacks_.push_back(std::move(callback));
    LOG_F(INFO, "Registered network event callback. Total callbacks: {}", event_callbacks_.size());
}

auto NetworkMonitor::getCurrentStats() const -> NetworkStats {
    std::lock_guard lock(stats_mutex_);
    return current_stats_;
}

auto NetworkMonitor::getStatsHistory(std::chrono::minutes duration) const -> std::vector<TimestampedNetworkStats> {
    std::lock_guard lock(stats_mutex_);
    std::vector<TimestampedNetworkStats> result;

    auto cutoff_time = std::chrono::steady_clock::now() - duration;
    auto temp_queue = stats_history_;

    while (!temp_queue.empty()) {
        const auto& stats = temp_queue.front();
        if (stats.timestamp >= cutoff_time) {
            result.push_back(stats);
        }
        temp_queue.pop();
    }

    return result;
}

auto NetworkMonitor::getRecentEvents(std::chrono::minutes duration) const -> std::vector<NetworkEventData> {
    std::lock_guard lock(events_mutex_);
    std::vector<NetworkEventData> result;

    auto cutoff_time = std::chrono::steady_clock::now() - duration;
    auto temp_queue = recent_events_;

    while (!temp_queue.empty()) {
        const auto& event = temp_queue.front();
        if (event.timestamp >= cutoff_time) {
            result.push_back(event);
        }
        temp_queue.pop();
    }

    return result;
}

auto NetworkMonitor::getAverageStats(std::chrono::minutes duration) const -> NetworkStats {
    auto history = getStatsHistory(duration);
    if (history.empty()) {
        return NetworkStats{};
    }

    NetworkStats avg{};
    for (const auto& timestamped_stats : history) {
        const auto& stats = timestamped_stats.stats;
        avg.downloadSpeed += stats.downloadSpeed;
        avg.uploadSpeed += stats.uploadSpeed;
        avg.latency += stats.latency;
        avg.packetLoss += stats.packetLoss;
        avg.signalStrength += stats.signalStrength;
    }

    size_t count = history.size();
    avg.downloadSpeed /= count;
    avg.uploadSpeed /= count;
    avg.latency /= count;
    avg.packetLoss /= count;
    avg.signalStrength /= count;

    return avg;
}

auto NetworkMonitor::getPeakStats(std::chrono::minutes duration) const -> NetworkStats {
    auto history = getStatsHistory(duration);
    if (history.empty()) {
        return NetworkStats{};
    }

    NetworkStats peak{};
    bool first = true;

    for (const auto& timestamped_stats : history) {
        const auto& stats = timestamped_stats.stats;
        if (first) {
            peak = stats;
            first = false;
        } else {
            peak.downloadSpeed = std::max(peak.downloadSpeed, stats.downloadSpeed);
            peak.uploadSpeed = std::max(peak.uploadSpeed, stats.uploadSpeed);
            peak.latency = std::max(peak.latency, stats.latency);
            peak.packetLoss = std::max(peak.packetLoss, stats.packetLoss);
            peak.signalStrength = std::max(peak.signalStrength, stats.signalStrength);
        }
    }

    return peak;
}

void NetworkMonitor::clearHistory() {
    {
        std::lock_guard lock(stats_mutex_);
        while (!stats_history_.empty()) {
            stats_history_.pop();
        }
    }

    {
        std::lock_guard lock(events_mutex_);
        while (!recent_events_.empty()) {
            recent_events_.pop();
        }
    }

    LOG_F(INFO, "Network monitoring history cleared");
}

void NetworkMonitor::updateConfig(const MonitorConfig& config) {
    config_ = config;
    LOG_F(INFO, "Network monitoring configuration updated");
}

auto NetworkMonitor::getConfig() const -> MonitorConfig {
    return config_;
}

void NetworkMonitor::monitoringLoop() {
    LOG_F(INFO, "Network monitoring loop started");

    while (running_.load()) {
        try {
            auto current_stats = getNetworkStats();
            processStats(current_stats);

            std::this_thread::sleep_for(config_.polling_interval);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error in monitoring loop: {}", e.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    LOG_F(INFO, "Network monitoring loop ended");
}

void NetworkMonitor::processStats(const NetworkStats& current_stats) {
    {
        std::lock_guard lock(stats_mutex_);
        previous_stats_ = current_stats_;
        current_stats_ = current_stats;

        // Add to history
        stats_history_.emplace(current_stats);

        // Maintain history size limit
        while (stats_history_.size() > config_.max_history_size) {
            stats_history_.pop();
        }
    }

    // Detect and emit events if enabled
    if (config_.enable_event_callbacks) {
        detectAndEmitEvents(current_stats, previous_stats_);
    }
}

void NetworkMonitor::detectAndEmitEvents(const NetworkStats& current_stats, const NetworkStats& previous_stats) {
    // Check for significant signal strength changes
    if (std::abs(current_stats.signalStrength - previous_stats.signalStrength) >= config_.signal_threshold_change) {
        std::unordered_map<std::string, std::string> metadata;
        metadata["previous_signal"] = std::to_string(previous_stats.signalStrength);
        metadata["current_signal"] = std::to_string(current_stats.signalStrength);

        emitEvent(NetworkEvent::SIGNAL_STRENGTH_CHANGED,
                 "Signal strength changed significantly", metadata);
    }

    // Check for bandwidth changes
    double bandwidth_change = std::abs(current_stats.downloadSpeed - previous_stats.downloadSpeed);
    if (bandwidth_change >= config_.bandwidth_threshold_change) {
        std::unordered_map<std::string, std::string> metadata;
        metadata["previous_download"] = std::to_string(previous_stats.downloadSpeed);
        metadata["current_download"] = std::to_string(current_stats.downloadSpeed);

        emitEvent(NetworkEvent::BANDWIDTH_CHANGED,
                 "Bandwidth changed significantly", metadata);
    }

    // Check for connection loss (high latency or no signal)
    if (current_stats.latency < 0 && previous_stats.latency >= 0) {
        emitEvent(NetworkEvent::CONNECTION_LOST, "Network connection lost");
    } else if (current_stats.latency >= 0 && previous_stats.latency < 0) {
        emitEvent(NetworkEvent::CONNECTION_ESTABLISHED, "Network connection established");
    }
}

void NetworkMonitor::emitEvent(NetworkEvent event_type, const std::string& description,
                              const std::unordered_map<std::string, std::string>& metadata) {
    NetworkEventData event_data;
    event_data.event_type = event_type;
    event_data.timestamp = std::chrono::steady_clock::now();
    event_data.description = description;
    event_data.metadata = metadata;

    // Add to recent events
    {
        std::lock_guard lock(events_mutex_);
        recent_events_.push(event_data);

        // Maintain events queue size (keep last 1000 events)
        while (recent_events_.size() > 1000) {
            recent_events_.pop();
        }
    }

    // Call registered callbacks
    {
        std::lock_guard lock(callbacks_mutex_);
        for (const auto& callback : event_callbacks_) {
            try {
                callback(event_data);
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Error in event callback: {}", e.what());
            }
        }
    }

    LOG_F(INFO, "Network event emitted: {}", description);
}

// NetworkPerformanceAnalyzer implementation
auto NetworkPerformanceAnalyzer::analyzePerformanceTrends(const std::vector<TimestampedNetworkStats>& history) -> std::string {
    if (history.empty()) {
        return "No data available for trend analysis";
    }

    if (history.size() < 2) {
        return "Insufficient data for trend analysis (need at least 2 data points)";
    }

    std::ostringstream analysis;
    analysis << "Network Performance Trend Analysis:\n";

    // Calculate trends for key metrics
    double latency_trend = 0.0;
    double download_trend = 0.0;
    double signal_trend = 0.0;

    size_t valid_points = 0;
    for (size_t i = 1; i < history.size(); ++i) {
        const auto& current = history[i].stats;
        const auto& previous = history[i-1].stats;

        if (current.latency >= 0 && previous.latency >= 0) {
            latency_trend += (current.latency - previous.latency);
            valid_points++;
        }

        download_trend += (current.downloadSpeed - previous.downloadSpeed);
        signal_trend += (current.signalStrength - previous.signalStrength);
    }

    if (valid_points > 0) {
        latency_trend /= valid_points;
        download_trend /= (history.size() - 1);
        signal_trend /= (history.size() - 1);

        analysis << "- Latency trend: " << (latency_trend > 0 ? "Increasing" : "Decreasing")
                 << " (" << std::fixed << std::setprecision(2) << latency_trend << "ms/sample)\n";
        analysis << "- Download speed trend: " << (download_trend > 0 ? "Increasing" : "Decreasing")
                 << " (" << std::fixed << std::setprecision(2) << download_trend << "MB/s/sample)\n";
        analysis << "- Signal strength trend: " << (signal_trend > 0 ? "Improving" : "Degrading")
                 << " (" << std::fixed << std::setprecision(2) << signal_trend << "dBm/sample)\n";
    }

    return analysis.str();
}

auto NetworkPerformanceAnalyzer::detectAnomalies(const std::vector<TimestampedNetworkStats>& history) -> std::vector<std::string> {
    std::vector<std::string> anomalies;

    if (history.size() < 10) {
        return anomalies; // Need sufficient data for anomaly detection
    }

    // Calculate statistics for anomaly detection
    std::vector<double> latencies, download_speeds, signal_strengths;

    for (const auto& timestamped_stats : history) {
        const auto& stats = timestamped_stats.stats;
        if (stats.latency >= 0) {
            latencies.push_back(stats.latency);
        }
        download_speeds.push_back(stats.downloadSpeed);
        signal_strengths.push_back(stats.signalStrength);
    }

    // Calculate mean and standard deviation for each metric
    auto calculate_stats = [](const std::vector<double>& values) -> std::pair<double, double> {
        if (values.empty()) return {0.0, 0.0};

        double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
        double variance = 0.0;
        for (double value : values) {
            variance += (value - mean) * (value - mean);
        }
        variance /= values.size();
        double stddev = std::sqrt(variance);
        return {mean, stddev};
    };

    auto [latency_mean, latency_stddev] = calculate_stats(latencies);
    auto [download_mean, download_stddev] = calculate_stats(download_speeds);
    auto [signal_mean, signal_stddev] = calculate_stats(signal_strengths);

    // Detect anomalies (values beyond 2 standard deviations)
    for (const auto& timestamped_stats : history) {
        const auto& stats = timestamped_stats.stats;

        if (!latencies.empty() && stats.latency >= 0) {
            if (std::abs(stats.latency - latency_mean) > 2 * latency_stddev) {
                anomalies.push_back("Unusual latency detected: " + std::to_string(stats.latency) + "ms");
            }
        }

        if (std::abs(stats.downloadSpeed - download_mean) > 2 * download_stddev) {
            anomalies.push_back("Unusual download speed detected: " + std::to_string(stats.downloadSpeed) + "MB/s");
        }

        if (std::abs(stats.signalStrength - signal_mean) > 2 * signal_stddev) {
            anomalies.push_back("Unusual signal strength detected: " + std::to_string(stats.signalStrength) + "dBm");
        }
    }

    return anomalies;
}

auto NetworkPerformanceAnalyzer::calculateStabilityScore(const std::vector<TimestampedNetworkStats>& history) -> double {
    if (history.size() < 2) {
        return 0.0; // Cannot calculate stability with insufficient data
    }

    // Calculate coefficient of variation for key metrics
    std::vector<double> latencies, download_speeds, signal_strengths;

    for (const auto& timestamped_stats : history) {
        const auto& stats = timestamped_stats.stats;
        if (stats.latency >= 0) {
            latencies.push_back(stats.latency);
        }
        download_speeds.push_back(stats.downloadSpeed);
        signal_strengths.push_back(stats.signalStrength);
    }

    auto calculate_cv = [](const std::vector<double>& values) -> double {
        if (values.size() < 2) return 1.0; // High variability for insufficient data

        double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
        if (mean == 0.0) return 1.0;

        double variance = 0.0;
        for (double value : values) {
            variance += (value - mean) * (value - mean);
        }
        variance /= (values.size() - 1);
        double stddev = std::sqrt(variance);

        return stddev / mean; // Coefficient of variation
    };

    double latency_cv = latencies.empty() ? 0.0 : calculate_cv(latencies);
    double download_cv = calculate_cv(download_speeds);
    double signal_cv = calculate_cv(signal_strengths);

    // Calculate overall stability score (lower CV = higher stability)
    // Convert CV to stability score (0-1 scale, where 1 is most stable)
    double avg_cv = (latency_cv + download_cv + std::abs(signal_cv)) / 3.0;
    double stability_score = std::max(0.0, 1.0 - std::min(1.0, avg_cv));

    return stability_score;
}

auto NetworkPerformanceAnalyzer::generateQualityReport(const std::vector<TimestampedNetworkStats>& history) -> std::string {
    if (history.empty()) {
        return "No data available for quality report";
    }

    std::ostringstream report;
    report << "Network Quality Report\n";
    report << "=====================\n\n";

    // Basic statistics
    auto latest_stats = history.back().stats;
    report << "Current Status:\n";
    report << "- Latency: " << (latest_stats.latency >= 0 ? std::to_string(static_cast<int>(latest_stats.latency)) + "ms" : "Unknown") << "\n";
    report << "- Download Speed: " << std::fixed << std::setprecision(2) << latest_stats.downloadSpeed << " MB/s\n";
    report << "- Upload Speed: " << std::fixed << std::setprecision(2) << latest_stats.uploadSpeed << " MB/s\n";
    report << "- Signal Strength: " << std::fixed << std::setprecision(1) << latest_stats.signalStrength << " dBm\n";
    report << "- Packet Loss: " << std::fixed << std::setprecision(1) << latest_stats.packetLoss << "%\n\n";

    // Stability analysis
    double stability_score = calculateStabilityScore(history);
    report << "Stability Score: " << std::fixed << std::setprecision(2) << (stability_score * 100) << "%\n";

    if (stability_score >= 0.8) {
        report << "Network stability: Excellent\n";
    } else if (stability_score >= 0.6) {
        report << "Network stability: Good\n";
    } else if (stability_score >= 0.4) {
        report << "Network stability: Fair\n";
    } else {
        report << "Network stability: Poor\n";
    }

    // Trend analysis
    report << "\n" << analyzePerformanceTrends(history);

    // Anomaly detection
    auto anomalies = detectAnomalies(history);
    if (!anomalies.empty()) {
        report << "\nDetected Anomalies:\n";
        for (const auto& anomaly : anomalies) {
            report << "- " << anomaly << "\n";
        }
    } else {
        report << "\nNo anomalies detected in recent data.\n";
    }

    return report.str();
}

// Global monitor instance
static std::unique_ptr<NetworkMonitor> global_monitor;
static std::mutex global_monitor_mutex;

auto getGlobalNetworkMonitor() -> NetworkMonitor& {
    std::lock_guard lock(global_monitor_mutex);
    if (!global_monitor) {
        global_monitor = std::make_unique<NetworkMonitor>();
    }
    return *global_monitor;
}

auto initializeNetworkMonitoring(const MonitorConfig& config) -> bool {
    std::lock_guard lock(global_monitor_mutex);

    if (global_monitor && global_monitor->isRunning()) {
        LOG_F(WARNING, "Network monitoring is already initialized and running");
        return true;
    }

    global_monitor = std::make_unique<NetworkMonitor>(config);
    bool success = global_monitor->start();

    if (success) {
        LOG_F(INFO, "Global network monitoring initialized successfully");
    } else {
        LOG_F(ERROR, "Failed to initialize global network monitoring");
        global_monitor.reset();
    }

    return success;
}

void shutdownNetworkMonitoring() {
    std::lock_guard lock(global_monitor_mutex);

    if (global_monitor) {
        global_monitor->stop();
        global_monitor.reset();
        LOG_F(INFO, "Global network monitoring shutdown completed");
    }
}

} // namespace atom::system
