/**
 * @file gpu_monitoring_example.cpp
 * @brief Example demonstrating GPU monitoring and benchmarking
 *
 * This example shows how to use the GPU module for continuous monitoring,
 * benchmarking, and real-time performance tracking.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "atom/sysinfo/gpu/gpu.hpp"
#include "atom/sysinfo/gpu/common.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <atomic>
#include <signal.h>

std::atomic<bool> running{true};

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived interrupt signal. Stopping monitoring..." << std::endl;
        running = false;
    }
}

void monitoringCallback(const atom::system::GPUPerformanceMetrics& metrics) {
    static int updateCount = 0;
    updateCount++;
    
    // Clear screen and move cursor to top
    std::cout << "\033[2J\033[H";
    
    std::cout << "GPU Real-time Monitoring (Update #" << updateCount << ")" << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << "Press Ctrl+C to stop monitoring" << std::endl << std::endl;
    
    // Performance metrics
    std::cout << "Performance Metrics:" << std::endl;
    std::cout << "  GPU Utilization:    " << std::setw(6) << std::fixed << std::setprecision(1) 
              << metrics.gpuUtilization << "%" << std::endl;
    std::cout << "  Memory Utilization: " << std::setw(6) << std::fixed << std::setprecision(1) 
              << metrics.memoryUtilization << "%" << std::endl;
    std::cout << "  Encoder Utilization:" << std::setw(6) << std::fixed << std::setprecision(1) 
              << metrics.encoderUtilization << "%" << std::endl;
    std::cout << "  Decoder Utilization:" << std::setw(6) << std::fixed << std::setprecision(1) 
              << metrics.decoderUtilization << "%" << std::endl;
    
    std::cout << std::endl;
    
    // Thermal information
    std::cout << "Thermal Information:" << std::endl;
    std::cout << "  GPU Temperature:    " << std::setw(6) << std::fixed << std::setprecision(1) 
              << metrics.temperature << "°C" << std::endl;
    std::cout << "  Hotspot Temperature:" << std::setw(6) << std::fixed << std::setprecision(1) 
              << metrics.hotspotTemperature << "°C" << std::endl;
    std::cout << "  Memory Temperature: " << std::setw(6) << std::fixed << std::setprecision(1) 
              << metrics.memoryTemperature << "°C" << std::endl;
    std::cout << "  Max Temperature:    " << std::setw(6) << std::fixed << std::setprecision(1) 
              << metrics.maxTemperature << "°C" << std::endl;
    
    std::cout << std::endl;
    
    // Power information
    std::cout << "Power Information:" << std::endl;
    std::cout << "  Power Draw:         " << std::setw(6) << std::fixed << std::setprecision(1) 
              << metrics.powerDraw << "W" << std::endl;
    std::cout << "  Power Limit:        " << std::setw(6) << std::fixed << std::setprecision(1) 
              << metrics.powerLimit << "W" << std::endl;
    std::cout << "  Max Power Limit:    " << std::setw(6) << std::fixed << std::setprecision(1) 
              << metrics.maxPowerLimit << "W" << std::endl;
    std::cout << "  Power Efficiency:   " << std::setw(6) << std::fixed << std::setprecision(2) 
              << metrics.powerEfficiency << " perf/W" << std::endl;
    
    std::cout << std::endl;
    
    // Clock speeds
    std::cout << "Clock Speeds:" << std::endl;
    std::cout << "  Core Clock:         " << std::setw(6) << std::fixed << std::setprecision(0) 
              << metrics.coreClockSpeed << " MHz" << std::endl;
    std::cout << "  Memory Clock:       " << std::setw(6) << std::fixed << std::setprecision(0) 
              << metrics.memoryClockSpeed << " MHz" << std::endl;
    std::cout << "  Base Clock:         " << std::setw(6) << std::fixed << std::setprecision(0) 
              << metrics.baseClock << " MHz" << std::endl;
    std::cout << "  Boost Clock:        " << std::setw(6) << std::fixed << std::setprecision(0) 
              << metrics.boostClock << " MHz" << std::endl;
    
    std::cout << std::endl;
    
    // Fan information
    std::cout << "Fan Information:" << std::endl;
    std::cout << "  Fan Speed:          " << std::setw(6) << std::fixed << std::setprecision(1) 
              << metrics.fanSpeed << "%" << std::endl;
    std::cout << "  Fan RPM:            " << std::setw(6) << metrics.fanRPM << " RPM" << std::endl;
    
    std::cout << std::endl;
    
    // Alerts and warnings
    std::cout << "Status:" << std::endl;
    
    if (metrics.temperature > 85.0) {
        std::cout << "  ⚠️  HIGH TEMPERATURE WARNING!" << std::endl;
    }
    
    if (metrics.powerDraw > metrics.maxPowerLimit * 0.95) {
        std::cout << "  ⚠️  HIGH POWER CONSUMPTION!" << std::endl;
    }
    
    if (atom::system::detectThermalThrottling(metrics)) {
        std::cout << "  🔥 THERMAL THROTTLING DETECTED!" << std::endl;
    }
    
    if (metrics.temperature < 85.0 && metrics.powerDraw < metrics.maxPowerLimit * 0.8) {
        std::cout << "  ✅ GPU operating normally" << std::endl;
    }
}

void runBenchmark(int gpuIndex) {
    std::cout << "=== GPU Benchmark ===" << std::endl;
    std::cout << "Running comprehensive GPU benchmark..." << std::endl;
    
    // Configure benchmark
    atom::system::GPUBenchmarkConfig config;
    config.duration = std::chrono::seconds(30);
    config.testCompute = true;
    config.testMemory = true;
    config.testGraphics = true;
    config.memoryTestSize = 256 * 1024 * 1024;  // 256MB
    config.renderWidth = 1920;
    config.renderHeight = 1080;
    
    std::cout << "Benchmark configuration:" << std::endl;
    std::cout << "  Duration: " << config.duration.count() << " seconds" << std::endl;
    std::cout << "  Memory test size: " << config.memoryTestSize / (1024*1024) << " MB" << std::endl;
    std::cout << "  Render resolution: " << config.renderWidth << "x" << config.renderHeight << std::endl;
    std::cout << std::endl;
    
    // Run benchmark
    auto results = atom::system::benchmarkGPU(gpuIndex, config);
    
    // Display results
    std::cout << "Benchmark Results:" << std::endl;
    std::cout << "  Overall Score:      " << std::fixed << std::setprecision(1) 
              << results.overallScore << "/100" << std::endl;
    std::cout << "  Compute Score:      " << results.computeScore << "/100" << std::endl;
    std::cout << "  Memory Score:       " << results.memoryScore << "/100" << std::endl;
    std::cout << "  Graphics Score:     " << results.graphicsScore << "/100" << std::endl;
    
    std::cout << std::endl;
    
    std::cout << "Detailed Metrics:" << std::endl;
    std::cout << "  Memory Bandwidth:   " << std::fixed << std::setprecision(1) 
              << results.memoryBandwidth << " GB/s" << std::endl;
    std::cout << "  Compute Throughput: " << results.computeThroughput << " GFLOPS" << std::endl;
    std::cout << "  Fill Rate:          " << results.fillRate << " Mpixels/s" << std::endl;
    std::cout << "  Triangle Rate:      " << results.triangleRate << " Mtriangles/s" << std::endl;
    
    std::cout << std::endl;
    
    std::cout << "Performance Characteristics:" << std::endl;
    std::cout << "  Average Frame Time: " << std::fixed << std::setprecision(2) 
              << results.averageFrameTime << " ms" << std::endl;
    std::cout << "  Min Frame Time:     " << results.minFrameTime << " ms" << std::endl;
    std::cout << "  Max Frame Time:     " << results.maxFrameTime << " ms" << std::endl;
    std::cout << "  Frame Time Variance:" << results.frameTimeVariance << " ms²" << std::endl;
    
    std::cout << std::endl;
    
    std::cout << "Thermal/Power During Test:" << std::endl;
    std::cout << "  Peak Temperature:   " << std::fixed << std::setprecision(1) 
              << results.peakTemperature << "°C" << std::endl;
    std::cout << "  Average Power Draw: " << results.averagePowerDraw << "W" << std::endl;
    std::cout << "  Peak Power Draw:    " << results.peakPowerDraw << "W" << std::endl;
    
    std::cout << std::endl;
}

void runMonitoring(int gpuIndex) {
    std::cout << "=== GPU Monitoring ===" << std::endl;
    
    // Configure monitoring
    atom::system::GPUMonitoringConfig config;
    config.updateInterval = std::chrono::milliseconds(1000);
    config.monitorPerformance = true;
    config.monitorTemperature = true;
    config.monitorPower = true;
    config.monitorMemory = true;
    config.monitorFanSpeed = true;
    config.temperatureThreshold = 80.0;
    config.memoryThreshold = 90.0;
    config.powerThreshold = 95.0;
    
    std::cout << "Starting real-time monitoring for GPU " << gpuIndex << "..." << std::endl;
    std::cout << "Update interval: " << config.updateInterval.count() << "ms" << std::endl;
    std::cout << "Temperature threshold: " << config.temperatureThreshold << "°C" << std::endl;
    std::cout << "Memory threshold: " << config.memoryThreshold << "%" << std::endl;
    std::cout << "Power threshold: " << config.powerThreshold << "%" << std::endl;
    std::cout << std::endl;
    
    // Start monitoring
    bool success = atom::system::startGPUMonitoring(gpuIndex, config, monitoringCallback);
    
    if (!success) {
        std::cerr << "Failed to start GPU monitoring!" << std::endl;
        return;
    }
    
    // Wait for interrupt signal
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Stop monitoring
    atom::system::stopGPUMonitoring(gpuIndex);
    std::cout << "Monitoring stopped." << std::endl;
}

int main(int argc, char* argv[]) {
    // Set up signal handler
    signal(SIGINT, signalHandler);
    
    try {
        std::cout << "GPU Monitoring and Benchmarking Example" << std::endl;
        std::cout << "=======================================" << std::endl << std::endl;
        
        // Check for GPUs
        int gpuCount = atom::system::getGPUCount();
        if (gpuCount == 0) {
            std::cout << "No GPUs detected!" << std::endl;
            return 1;
        }
        
        std::cout << "Detected " << gpuCount << " GPU(s)" << std::endl;
        
        // Select GPU to monitor (default to 0, or from command line)
        int gpuIndex = 0;
        if (argc > 1) {
            gpuIndex = std::atoi(argv[1]);
            if (gpuIndex < 0 || gpuIndex >= gpuCount) {
                std::cerr << "Invalid GPU index: " << gpuIndex << std::endl;
                return 1;
            }
        }
        
        auto gpu = atom::system::getGPUInfo(gpuIndex);
        std::cout << "Selected GPU " << gpuIndex << ": " << gpu.name << std::endl << std::endl;
        
        // Ask user what to do
        std::cout << "Select operation:" << std::endl;
        std::cout << "1. Run benchmark" << std::endl;
        std::cout << "2. Start monitoring" << std::endl;
        std::cout << "3. Both (benchmark then monitoring)" << std::endl;
        std::cout << "Enter choice (1-3): ";
        
        int choice;
        std::cin >> choice;
        std::cout << std::endl;
        
        switch (choice) {
            case 1:
                runBenchmark(gpuIndex);
                break;
            case 2:
                runMonitoring(gpuIndex);
                break;
            case 3:
                runBenchmark(gpuIndex);
                std::cout << "Press Enter to start monitoring...";
                std::cin.ignore();
                std::cin.get();
                runMonitoring(gpuIndex);
                break;
            default:
                std::cout << "Invalid choice. Starting monitoring..." << std::endl;
                runMonitoring(gpuIndex);
                break;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
