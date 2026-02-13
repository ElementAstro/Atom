#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include "../virtual.hpp"

using namespace atom::system::virtual_env;
using namespace std::chrono;

class Timer {
public:
    Timer() : start_time(high_resolution_clock::now()) {}

    void reset() {
        start_time = high_resolution_clock::now();
    }

    double elapsed_ms() const {
        auto end_time = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end_time - start_time);
        return duration.count() / 1000.0;
    }

private:
    high_resolution_clock::time_point start_time;
};

void benchmarkBasicDetection() {
    std::cout << "Benchmarking Basic Detection Methods\n";
    std::cout << std::string(50, '-') << "\n";

    const int iterations = 100;
    Timer timer;

    // Benchmark CPUID detection
    timer.reset();
    for (int i = 0; i < iterations; ++i) {
        detection::cpuid::isHypervisorPresent();
    }
    double cpuid_time = timer.elapsed_ms();

    // Benchmark BIOS detection
    timer.reset();
    for (int i = 0; i < iterations; ++i) {
        detection::bios::checkBIOSInfo();
    }
    double bios_time = timer.elapsed_ms();

    // Benchmark network detection
    timer.reset();
    for (int i = 0; i < iterations; ++i) {
        detection::hardware::checkNetworkAdapters();
    }
    double network_time = timer.elapsed_ms();

    // Benchmark process detection
    timer.reset();
    for (int i = 0; i < iterations; ++i) {
        detection::processes::checkVirtualizationProcesses();
    }
    double process_time = timer.elapsed_ms();

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Method                 | Avg Time (ms) | Total Time (ms)\n";
    std::cout << std::string(50, '-') << "\n";
    std::cout << "CPUID Detection        | " << std::setw(10) << cpuid_time / iterations
              << "    | " << std::setw(12) << cpuid_time << "\n";
    std::cout << "BIOS Detection         | " << std::setw(10) << bios_time / iterations
              << "    | " << std::setw(12) << bios_time << "\n";
    std::cout << "Network Detection      | " << std::setw(10) << network_time / iterations
              << "    | " << std::setw(12) << network_time << "\n";
    std::cout << "Process Detection      | " << std::setw(10) << process_time / iterations
              << "    | " << std::setw(12) << process_time << "\n";
}

void benchmarkComprehensiveDetection() {
    std::cout << "\nBenchmarking Comprehensive Detection\n";
    std::cout << std::string(50, '-') << "\n";

    const int iterations = 10;
    Timer timer;
    VirtualizationDetector detector;

    // Benchmark full detection
    timer.reset();
    for (int i = 0; i < iterations; ++i) {
        auto info = detector.detect();
    }
    double full_detection_time = timer.elapsed_ms();

    // Benchmark quick checks
    timer.reset();
    for (int i = 0; i < iterations; ++i) {
        detector.isVirtual();
    }
    double quick_virtual_time = timer.elapsed_ms();

    timer.reset();
    for (int i = 0; i < iterations; ++i) {
        detector.isContainer();
    }
    double quick_container_time = timer.elapsed_ms();

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Detection Type         | Avg Time (ms) | Total Time (ms)\n";
    std::cout << std::string(50, '-') << "\n";
    std::cout << "Full Detection         | " << std::setw(10) << full_detection_time / iterations
              << "    | " << std::setw(12) << full_detection_time << "\n";
    std::cout << "Quick Virtual Check    | " << std::setw(10) << quick_virtual_time / iterations
              << "    | " << std::setw(12) << quick_virtual_time << "\n";
    std::cout << "Quick Container Check  | " << std::setw(10) << quick_container_time / iterations
              << "    | " << std::setw(12) << quick_container_time << "\n";
}

void benchmarkHypervisorDetection() {
    std::cout << "\nBenchmarking Hypervisor Detection\n";
    std::cout << std::string(50, '-') << "\n";

    const int iterations = 50;
    Timer timer;

    // Benchmark hypervisor vendor detection
    timer.reset();
    for (int i = 0; i < iterations; ++i) {
        hypervisor::getHypervisorVendor();
    }
    double vendor_time = timer.elapsed_ms();

    // Benchmark hypervisor type detection
    timer.reset();
    for (int i = 0; i < iterations; ++i) {
        hypervisor::detectHypervisorType();
    }
    double type_time = timer.elapsed_ms();

    // Benchmark full hypervisor info
    timer.reset();
    for (int i = 0; i < iterations; ++i) {
        hypervisor::getHypervisorInfo();
    }
    double info_time = timer.elapsed_ms();

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Hypervisor Detection   | Avg Time (ms) | Total Time (ms)\n";
    std::cout << std::string(50, '-') << "\n";
    std::cout << "Vendor Detection       | " << std::setw(10) << vendor_time / iterations
              << "    | " << std::setw(12) << vendor_time << "\n";
    std::cout << "Type Detection         | " << std::setw(10) << type_time / iterations
              << "    | " << std::setw(12) << type_time << "\n";
    std::cout << "Full Info              | " << std::setw(10) << info_time / iterations
              << "    | " << std::setw(12) << info_time << "\n";
}

void benchmarkContainerDetection() {
    std::cout << "\nBenchmarking Container Detection\n";
    std::cout << std::string(50, '-') << "\n";

    const int iterations = 50;
    Timer timer;

    // Benchmark container detection
    timer.reset();
    for (int i = 0; i < iterations; ++i) {
        container::isContainer();
    }
    double container_check_time = timer.elapsed_ms();

    // Benchmark container type detection
    timer.reset();
    for (int i = 0; i < iterations; ++i) {
        container::detectContainerType();
    }
    double type_time = timer.elapsed_ms();

    // Benchmark full container info
    timer.reset();
    for (int i = 0; i < iterations; ++i) {
        container::getContainerInfo();
    }
    double info_time = timer.elapsed_ms();

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Container Detection    | Avg Time (ms) | Total Time (ms)\n";
    std::cout << std::string(50, '-') << "\n";
    std::cout << "Container Check        | " << std::setw(10) << container_check_time / iterations
              << "    | " << std::setw(12) << container_check_time << "\n";
    std::cout << "Type Detection         | " << std::setw(10) << type_time / iterations
              << "    | " << std::setw(12) << type_time << "\n";
    std::cout << "Full Info              | " << std::setw(10) << info_time / iterations
              << "    | " << std::setw(12) << info_time << "\n";
}

void benchmarkMemoryUsage() {
    std::cout << "\nMemory Usage Analysis\n";
    std::cout << std::string(50, '-') << "\n";

    // Create multiple detector instances to test memory usage
    std::vector<std::unique_ptr<VirtualizationDetector>> detectors;

    const int num_detectors = 100;
    Timer timer;

    timer.reset();
    for (int i = 0; i < num_detectors; ++i) {
        detectors.push_back(std::make_unique<VirtualizationDetector>());
    }
    double creation_time = timer.elapsed_ms();

    timer.reset();
    for (auto& detector : detectors) {
        detector->detect();
    }
    double detection_time = timer.elapsed_ms();

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Memory Test            | Time (ms)\n";
    std::cout << std::string(35, '-') << "\n";
    std::cout << "Create " << num_detectors << " detectors     | " << creation_time << "\n";
    std::cout << "Run " << num_detectors << " detections      | " << detection_time << "\n";
    std::cout << "Avg per detection      | " << detection_time / num_detectors << "\n";
}

void stressTest() {
    std::cout << "\nStress Test\n";
    std::cout << std::string(50, '-') << "\n";

    const int stress_iterations = 1000;
    Timer timer;
    VirtualizationDetector detector;

    std::cout << "Running " << stress_iterations << " detection cycles...\n";

    timer.reset();
    for (int i = 0; i < stress_iterations; ++i) {
        auto info = detector.detect();

        // Verify consistency
        if (i > 0 && i % 100 == 0) {
            std::cout << "Completed " << i << " iterations...\n";
        }
    }
    double total_time = timer.elapsed_ms();

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "\nStress Test Results:\n";
    std::cout << "Total iterations: " << stress_iterations << "\n";
    std::cout << "Total time: " << total_time << " ms\n";
    std::cout << "Average time per detection: " << total_time / stress_iterations << " ms\n";
    std::cout << "Detections per second: " << (stress_iterations * 1000.0) / total_time << "\n";
}

int main() {
    std::cout << "Virtual Environment Detection Benchmark\n";
    std::cout << "========================================\n";

    try {
        benchmarkBasicDetection();
        benchmarkComprehensiveDetection();
        benchmarkHypervisorDetection();
        benchmarkContainerDetection();
        benchmarkMemoryUsage();
        stressTest();

        std::cout << "\n" << std::string(50, '=') << "\n";
        std::cout << "Benchmark completed successfully!\n";
        std::cout << std::string(50, '=') << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error during benchmark: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
