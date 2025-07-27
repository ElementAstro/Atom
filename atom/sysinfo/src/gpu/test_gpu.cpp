/**
 * @file test_gpu.cpp
 * @brief Simple test for the GPU module
 */

#include "gpu.hpp"
#include "common.hpp"
#include <iostream>

int main() {
    try {
        std::cout << "Testing GPU Module" << std::endl;
        std::cout << "==================" << std::endl;
        
        // Test basic GPU info
        std::cout << "Basic GPU Info:" << std::endl;
        std::string basicInfo = atom::system::getGPUInfo();
        std::cout << basicInfo << std::endl << std::endl;
        
        // Test GPU count
        int gpuCount = atom::system::getGPUCount();
        std::cout << "GPU Count: " << gpuCount << std::endl << std::endl;
        
        // Test detailed GPU info
        auto gpus = atom::system::getDetailedGPUInfo();
        std::cout << "Detailed GPU Info (" << gpus.size() << " GPUs):" << std::endl;
        
        for (const auto& gpu : gpus) {
            std::cout << "GPU " << gpu.gpuIndex << ":" << std::endl;
            std::cout << "  Name: " << gpu.name << std::endl;
            std::cout << "  Vendor: " << atom::system::gpuVendorToString(gpu.vendor) << std::endl;
            std::cout << "  Type: " << atom::system::gpuTypeToString(gpu.type) << std::endl;
            std::cout << "  Architecture: " << atom::system::gpuArchitectureToString(gpu.architecture) << std::endl;
            std::cout << "  Total Memory: " << atom::system::formatMemorySize(gpu.memoryInfo.totalMemory) << std::endl;
            std::cout << std::endl;
        }
        
        // Test monitor info
        auto monitors = atom::system::getAllMonitorsInfo();
        std::cout << "Monitor Info (" << monitors.size() << " monitors):" << std::endl;
        
        for (size_t i = 0; i < monitors.size(); ++i) {
            const auto& monitor = monitors[i];
            std::cout << "Monitor " << i << ":" << std::endl;
            std::cout << "  Model: " << monitor.model << std::endl;
            std::cout << "  Resolution: " << monitor.width << "x" << monitor.height << std::endl;
            std::cout << "  Refresh Rate: " << monitor.refreshRate << " Hz" << std::endl;
            std::cout << std::endl;
        }
        
        std::cout << "GPU Module test completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
