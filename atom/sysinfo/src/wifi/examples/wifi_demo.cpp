/*
 * wifi_demo.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-2-21

Description: WiFi Module Demonstration Program

**************************************************/

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

#include "../wifi.hpp"
#include "../monitor.hpp"
#include "../quality.hpp"
#include "../config.hpp"
#include "../error_handler.hpp"

using namespace atom::system;

void printSeparator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

void demonstrateBasicFunctionality() {
    printSeparator("Basic WiFi Functionality");
    
    // Test internet connectivity
    std::cout << "Internet connectivity: " << (isConnectedToInternet() ? "Connected" : "Not connected") << "\n";
    
    // Get current WiFi
    std::string current_wifi = getCurrentWifi();
    std::cout << "Current WiFi: " << (current_wifi.empty() ? "None" : current_wifi) << "\n";
    
    // Get current wired network
    std::string wired_network = getCurrentWiredNetwork();
    std::cout << "Current wired network: " << (wired_network.empty() ? "None" : wired_network) << "\n";
    
    // Check hotspot status
    std::cout << "Hotspot connected: " << (isHotspotConnected() ? "Yes" : "No") << "\n";
    
    // Get host IPs
    auto host_ips = getHostIPs();
    std::cout << "Host IP addresses (" << host_ips.size() << "):\n";
    for (const auto& ip : host_ips) {
        std::cout << "  - " << ip << "\n";
    }
    
    // Get IPv4 addresses
    auto ipv4_addresses = getIPv4Addresses();
    std::cout << "IPv4 addresses (" << ipv4_addresses.size() << "):\n";
    for (const auto& ip : ipv4_addresses) {
        std::cout << "  - " << ip << "\n";
    }
    
    // Get interface names
    auto interfaces = getInterfaceNames();
    std::cout << "Network interfaces (" << interfaces.size() << "):\n";
    for (const auto& interface : interfaces) {
        std::cout << "  - " << interface << "\n";
    }
}

void demonstrateNetworkStats() {
    printSeparator("Network Statistics");
    
    auto stats = getNetworkStats();
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Download speed: " << stats.downloadSpeed << " MB/s\n";
    std::cout << "Upload speed: " << stats.uploadSpeed << " MB/s\n";
    std::cout << "Latency: " << stats.latency << " ms\n";
    std::cout << "Packet loss: " << stats.packetLoss << "%\n";
    std::cout << "Signal strength: " << stats.signalStrength << " dBm\n";
}

void demonstrateAdvancedFeatures() {
    printSeparator("Advanced Network Features");
    
    // Scan available networks
    std::cout << "Scanning available networks...\n";
    auto networks = scanAvailableNetworks();
    std::cout << "Available networks (" << networks.size() << "):\n";
    for (size_t i = 0; i < std::min(networks.size(), size_t(5)); ++i) {
        std::cout << "  - " << networks[i] << "\n";
    }
    if (networks.size() > 5) {
        std::cout << "  ... and " << (networks.size() - 5) << " more\n";
    }
    
    // Measure bandwidth
    std::cout << "\nMeasuring bandwidth...\n";
    auto [download, upload] = measureBandwidth();
    std::cout << "Measured bandwidth - Download: " << download << " Mbps, Upload: " << upload << " Mbps\n";
    
    // Analyze network quality
    std::cout << "\nAnalyzing network quality...\n";
    std::string quality = analyzeNetworkQuality();
    std::cout << "Network quality: " << quality << "\n";
    
    // Get network security
    std::string security = getNetworkSecurity();
    std::cout << "Network security: " << security << "\n";
    
    // Get connected devices
    auto devices = getConnectedDevices();
    std::cout << "Connected devices (" << devices.size() << "):\n";
    for (size_t i = 0; i < std::min(devices.size(), size_t(5)); ++i) {
        std::cout << "  - " << devices[i] << "\n";
    }
    if (devices.size() > 5) {
        std::cout << "  ... and " << (devices.size() - 5) << " more\n";
    }
}

void demonstrateQualityAnalysis() {
    printSeparator("Network Quality Analysis");
    
    NetworkQualityAnalyzer analyzer;
    
    std::cout << "Performing comprehensive quality analysis...\n";
    
    // Configure for faster demo
    JitterConfig jitter_config;
    jitter_config.packet_count = 5;
    
    PacketLossConfig packet_loss_config;
    packet_loss_config.packet_count = 10;
    
    ThroughputConfig throughput_config;
    throughput_config.test_duration_seconds = 3;
    
    auto metrics = analyzer.performComprehensiveAnalysis(jitter_config, throughput_config, packet_loss_config);
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Quality Metrics:\n";
    std::cout << "  - Jitter: " << metrics.jitter_ms << " ms\n";
    std::cout << "  - Packet Loss: " << metrics.packet_loss_percentage << "%\n";
    std::cout << "  - Throughput: " << metrics.throughput_mbps << " Mbps\n";
    std::cout << "  - Connection Stability: " << (metrics.connection_stability * 100) << "%\n";
    std::cout << "  - Signal Quality: " << (metrics.signal_quality * 100) << "%\n";
    std::cout << "  - Overall Quality: " << (metrics.overall_quality * 100) << "%\n";
    
    auto quality_level = NetworkQualityAnalyzer::getQualityLevel(metrics);
    std::cout << "Quality Level: " << qualityLevelToString(quality_level) << "\n";
    
    auto recommendations = NetworkQualityAnalyzer::getQualityRecommendations(metrics);
    std::cout << "Recommendations:\n";
    for (const auto& recommendation : recommendations) {
        std::cout << "  - " << recommendation << "\n";
    }
}

void demonstrateMonitoring() {
    printSeparator("Network Monitoring");
    
    std::cout << "Starting network monitoring for 10 seconds...\n";
    
    auto& monitor = getGlobalNetworkMonitor();
    
    // Register event callback
    monitor.registerEventCallback([](const NetworkEventData& event) {
        std::cout << "Network Event: " << event.description << "\n";
    });
    
    if (monitor.start()) {
        std::cout << "Monitoring started successfully\n";
        
        // Monitor for 10 seconds
        for (int i = 0; i < 10; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "." << std::flush;
        }
        std::cout << "\n";
        
        // Get monitoring results
        auto current_stats = monitor.getCurrentStats();
        std::cout << "Current monitored stats:\n";
        std::cout << "  - Download: " << current_stats.downloadSpeed << " MB/s\n";
        std::cout << "  - Upload: " << current_stats.uploadSpeed << " MB/s\n";
        std::cout << "  - Latency: " << current_stats.latency << " ms\n";
        
        auto history = monitor.getStatsHistory(std::chrono::minutes(1));
        std::cout << "History entries collected: " << history.size() << "\n";
        
        auto avg_stats = monitor.getAverageStats(std::chrono::minutes(1));
        std::cout << "Average stats:\n";
        std::cout << "  - Avg Download: " << avg_stats.downloadSpeed << " MB/s\n";
        std::cout << "  - Avg Latency: " << avg_stats.latency << " ms\n";
        
        monitor.stop();
        std::cout << "Monitoring stopped\n";
    } else {
        std::cout << "Failed to start monitoring\n";
    }
}

void demonstrateConfiguration() {
    printSeparator("Configuration Management");
    
    auto& config_manager = getGlobalConfigManager();
    
    std::cout << "Current configuration:\n";
    std::cout << config_manager.toString() << "\n";
    
    // Test parameter modification
    std::cout << "Modifying ping timeout...\n";
    config_manager.setParameter("ping_timeout", "4000");
    std::cout << "New ping timeout: " << config_manager.getParameter("ping_timeout") << " ms\n";
    
    // Test configuration validation
    auto errors = config_manager.validateConfiguration();
    if (errors.empty()) {
        std::cout << "Configuration is valid\n";
    } else {
        std::cout << "Configuration errors:\n";
        for (const auto& error : errors) {
            std::cout << "  - " << error << "\n";
        }
    }
    
    // Test auto-tuning
    std::cout << "Auto-tuning configuration...\n";
    config_manager.autoTune();
    std::cout << "Configuration auto-tuned\n";
}

void demonstratePerformanceMonitoring() {
    printSeparator("Performance Monitoring");
    
    auto& perf_monitor = getGlobalPerformanceMonitor();
    
    std::cout << "Recording some performance data...\n";
    
    // Simulate some operations with timing
    for (int i = 0; i < 5; ++i) {
        WIFI_PERF_TIMER("demo_operation");
        std::this_thread::sleep_for(std::chrono::milliseconds(10 + i * 5));
    }
    
    // Record some cache accesses
    for (int i = 0; i < 10; ++i) {
        if (i < 7) {
            WIFI_RECORD_CACHE_HIT("demo_cache");
        } else {
            WIFI_RECORD_CACHE_MISS("demo_cache");
        }
    }
    
    // Get performance statistics
    std::string stats = perf_monitor.getPerformanceStats(std::chrono::hours(1));
    std::cout << "Performance Statistics:\n" << stats << "\n";
    
    double hit_ratio = perf_monitor.getCacheHitRatio("demo_cache", std::chrono::hours(1));
    std::cout << "Demo cache hit ratio: " << (hit_ratio * 100) << "%\n";
}

void demonstrateErrorHandling() {
    printSeparator("Error Handling");
    
    auto& error_handler = getGlobalErrorHandler();
    
    // Register error callback
    error_handler.registerErrorCallback([](const ErrorEvent& event) {
        std::cout << "Error callback triggered: " << event.error_message << "\n";
    });
    
    std::cout << "Logging some test errors...\n";
    
    // Log some test errors
    WIFI_LOG_ERROR(WiFiError::TIMEOUT, "Demo timeout error");
    WIFI_LOG_ERROR(WiFiError::NETWORK_INTERFACE_ERROR, "Demo interface error");
    WIFI_LOG_ERROR(WiFiError::TIMEOUT, "Another demo timeout error");
    
    // Get error statistics
    auto error_stats = error_handler.getErrorStatistics(std::chrono::hours(1));
    std::cout << "Error statistics:\n";
    for (const auto& [error_code, count] : error_stats) {
        std::cout << "  - " << wifiErrorToString(error_code) << ": " << count << " occurrences\n";
    }
    
    // Generate error report
    std::string report = error_handler.generateErrorReport(std::chrono::hours(1));
    std::cout << "Error Report:\n" << report << "\n";
}

int main() {
    std::cout << "WiFi Module Comprehensive Demonstration\n";
    std::cout << "========================================\n";
    
    // Initialize all systems
    if (!initializeErrorHandling()) {
        std::cerr << "Failed to initialize error handling\n";
        return 1;
    }
    
    if (!initializeConfiguration()) {
        std::cerr << "Failed to initialize configuration\n";
        return 1;
    }
    
    if (!initializeNetworkMonitoring()) {
        std::cerr << "Failed to initialize network monitoring\n";
        return 1;
    }
    
    try {
        // Run demonstrations
        demonstrateBasicFunctionality();
        demonstrateNetworkStats();
        demonstrateAdvancedFeatures();
        demonstrateQualityAnalysis();
        demonstrateMonitoring();
        demonstrateConfiguration();
        demonstratePerformanceMonitoring();
        demonstrateErrorHandling();
        
        printSeparator("Demonstration Complete");
        std::cout << "All WiFi module features have been demonstrated successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error during demonstration: " << e.what() << "\n";
        return 1;
    }
    
    // Cleanup
    shutdownNetworkMonitoring();
    shutdownConfiguration();
    shutdownErrorHandling();
    
    return 0;
}
