/**
 * @file common.cpp
 * @brief Common GPU functionality implementation
 *
 * This file contains the implementation of common GPU utilities and helper
 * functions shared across different platform implementations.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "common.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace atom::system {

auto gpuVendorToString(GPUVendor vendor) -> std::string {
    switch (vendor) {
        case GPUVendor::NVIDIA:
            return "NVIDIA";
        case GPUVendor::AMD:
            return "AMD";
        case GPUVendor::INTEL:
            return "Intel";
        case GPUVendor::APPLE:
            return "Apple";
        case GPUVendor::QUALCOMM:
            return "Qualcomm";
        case GPUVendor::ARM:
            return "ARM";
        case GPUVendor::IMAGINATION:
            return "Imagination Technologies";
        case GPUVendor::UNKNOWN:
        default:
            return "Unknown";
    }
}

auto gpuTypeToString(GPUType type) -> std::string {
    switch (type) {
        case GPUType::DISCRETE:
            return "Discrete";
        case GPUType::INTEGRATED:
            return "Integrated";
        case GPUType::VIRTUAL:
            return "Virtual";
        case GPUType::EXTERNAL:
            return "External";
        case GPUType::UNKNOWN:
        default:
            return "Unknown";
    }
}

auto gpuArchitectureToString(GPUArchitecture arch) -> std::string {
    switch (arch) {
        // NVIDIA architectures
        case GPUArchitecture::TESLA:
            return "Tesla";
        case GPUArchitecture::FERMI:
            return "Fermi";
        case GPUArchitecture::KEPLER:
            return "Kepler";
        case GPUArchitecture::MAXWELL:
            return "Maxwell";
        case GPUArchitecture::PASCAL:
            return "Pascal";
        case GPUArchitecture::VOLTA:
            return "Volta";
        case GPUArchitecture::TURING:
            return "Turing";
        case GPUArchitecture::AMPERE:
            return "Ampere";
        case GPUArchitecture::ADA_LOVELACE:
            return "Ada Lovelace";
        case GPUArchitecture::HOPPER:
            return "Hopper";
        // AMD architectures
        case GPUArchitecture::TERASCALE:
            return "TeraScale";
        case GPUArchitecture::GCN:
            return "GCN";
        case GPUArchitecture::RDNA:
            return "RDNA";
        case GPUArchitecture::RDNA2:
            return "RDNA2";
        case GPUArchitecture::RDNA3:
            return "RDNA3";
        case GPUArchitecture::CDNA:
            return "CDNA";
        // Intel architectures
        case GPUArchitecture::GEN7:
            return "Gen7";
        case GPUArchitecture::GEN8:
            return "Gen8";
        case GPUArchitecture::GEN9:
            return "Gen9";
        case GPUArchitecture::GEN11:
            return "Gen11";
        case GPUArchitecture::GEN12:
            return "Gen12";
        case GPUArchitecture::XE:
            return "Xe";
        case GPUArchitecture::ARC:
            return "Arc";
        case GPUArchitecture::UNKNOWN:
        default:
            return "Unknown";
    }
}

auto parseGPUVendor(const std::string& vendorId) -> GPUVendor {
    std::string lowerVendorId = vendorId;
    std::transform(lowerVendorId.begin(), lowerVendorId.end(), lowerVendorId.begin(), ::tolower);
    
    if (lowerVendorId.find("nvidia") != std::string::npos || 
        lowerVendorId.find("10de") != std::string::npos) {
        return GPUVendor::NVIDIA;
    } else if (lowerVendorId.find("amd") != std::string::npos || 
               lowerVendorId.find("ati") != std::string::npos ||
               lowerVendorId.find("1002") != std::string::npos) {
        return GPUVendor::AMD;
    } else if (lowerVendorId.find("intel") != std::string::npos ||
               lowerVendorId.find("8086") != std::string::npos) {
        return GPUVendor::INTEL;
    } else if (lowerVendorId.find("apple") != std::string::npos) {
        return GPUVendor::APPLE;
    } else if (lowerVendorId.find("qualcomm") != std::string::npos) {
        return GPUVendor::QUALCOMM;
    } else if (lowerVendorId.find("arm") != std::string::npos) {
        return GPUVendor::ARM;
    } else if (lowerVendorId.find("imagination") != std::string::npos) {
        return GPUVendor::IMAGINATION;
    }
    
    return GPUVendor::UNKNOWN;
}

auto parseGPUArchitecture(const std::string& deviceName, GPUVendor vendor) -> GPUArchitecture {
    std::string lowerName = deviceName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
    
    switch (vendor) {
        case GPUVendor::NVIDIA:
            if (lowerName.find("rtx 40") != std::string::npos || 
                lowerName.find("rtx 4090") != std::string::npos ||
                lowerName.find("rtx 4080") != std::string::npos ||
                lowerName.find("rtx 4070") != std::string::npos ||
                lowerName.find("rtx 4060") != std::string::npos) {
                return GPUArchitecture::ADA_LOVELACE;
            } else if (lowerName.find("rtx 30") != std::string::npos ||
                       lowerName.find("rtx 3090") != std::string::npos ||
                       lowerName.find("rtx 3080") != std::string::npos ||
                       lowerName.find("rtx 3070") != std::string::npos ||
                       lowerName.find("rtx 3060") != std::string::npos) {
                return GPUArchitecture::AMPERE;
            } else if (lowerName.find("rtx 20") != std::string::npos ||
                       lowerName.find("gtx 16") != std::string::npos) {
                return GPUArchitecture::TURING;
            } else if (lowerName.find("gtx 10") != std::string::npos ||
                       lowerName.find("titan x") != std::string::npos) {
                return GPUArchitecture::PASCAL;
            } else if (lowerName.find("gtx 9") != std::string::npos ||
                       lowerName.find("gtx 7") != std::string::npos) {
                return GPUArchitecture::MAXWELL;
            } else if (lowerName.find("gtx 6") != std::string::npos) {
                return GPUArchitecture::KEPLER;
            } else if (lowerName.find("h100") != std::string::npos ||
                       lowerName.find("h800") != std::string::npos) {
                return GPUArchitecture::HOPPER;
            } else if (lowerName.find("v100") != std::string::npos ||
                       lowerName.find("titan v") != std::string::npos) {
                return GPUArchitecture::VOLTA;
            }
            break;
            
        case GPUVendor::AMD:
            if (lowerName.find("rx 7") != std::string::npos ||
                lowerName.find("rx 79") != std::string::npos ||
                lowerName.find("rx 78") != std::string::npos ||
                lowerName.find("rx 77") != std::string::npos) {
                return GPUArchitecture::RDNA3;
            } else if (lowerName.find("rx 6") != std::string::npos ||
                       lowerName.find("rx 69") != std::string::npos ||
                       lowerName.find("rx 68") != std::string::npos ||
                       lowerName.find("rx 67") != std::string::npos ||
                       lowerName.find("rx 66") != std::string::npos) {
                return GPUArchitecture::RDNA2;
            } else if (lowerName.find("rx 5") != std::string::npos) {
                return GPUArchitecture::RDNA;
            } else if (lowerName.find("rx 4") != std::string::npos ||
                       lowerName.find("rx 3") != std::string::npos ||
                       lowerName.find("rx 2") != std::string::npos ||
                       lowerName.find("r9") != std::string::npos ||
                       lowerName.find("r7") != std::string::npos ||
                       lowerName.find("r5") != std::string::npos) {
                return GPUArchitecture::GCN;
            } else if (lowerName.find("mi") != std::string::npos) {
                return GPUArchitecture::CDNA;
            }
            break;
            
        case GPUVendor::INTEL:
            if (lowerName.find("arc") != std::string::npos) {
                return GPUArchitecture::ARC;
            } else if (lowerName.find("xe") != std::string::npos ||
                       lowerName.find("iris xe") != std::string::npos) {
                return GPUArchitecture::XE;
            } else if (lowerName.find("gen12") != std::string::npos) {
                return GPUArchitecture::GEN12;
            } else if (lowerName.find("gen11") != std::string::npos) {
                return GPUArchitecture::GEN11;
            } else if (lowerName.find("gen9") != std::string::npos) {
                return GPUArchitecture::GEN9;
            } else if (lowerName.find("gen8") != std::string::npos) {
                return GPUArchitecture::GEN8;
            } else if (lowerName.find("gen7") != std::string::npos) {
                return GPUArchitecture::GEN7;
            }
            break;
            
        default:
            break;
    }
    
    return GPUArchitecture::UNKNOWN;
}

auto formatMemorySize(size_t bytes) -> std::string {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unitIndex];
    return oss.str();
}

auto formatClockSpeed(double mhz) -> std::string {
    std::ostringstream oss;
    if (mhz >= 1000.0) {
        oss << std::fixed << std::setprecision(2) << (mhz / 1000.0) << " GHz";
    } else {
        oss << std::fixed << std::setprecision(0) << mhz << " MHz";
    }
    return oss.str();
}

auto formatTemperature(double celsius) -> std::string {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << celsius << "°C";
    return oss.str();
}

auto formatPower(double watts) -> std::string {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << watts << "W";
    return oss.str();
}

auto calculateGPUScore(const GPUInfo& info) -> double {
    double score = 0.0;
    
    // Base score from compute capability
    score += info.compute.peakFP32Performance * 10.0;  // TFLOPS * 10
    
    // Memory score
    score += (info.memoryInfo.totalMemory / (1024.0 * 1024.0 * 1024.0)) * 5.0;  // GB * 5
    
    // Architecture bonus
    switch (info.architecture) {
        case GPUArchitecture::ADA_LOVELACE:
        case GPUArchitecture::HOPPER:
        case GPUArchitecture::RDNA3:
            score *= 1.2;
            break;
        case GPUArchitecture::AMPERE:
        case GPUArchitecture::RDNA2:
        case GPUArchitecture::ARC:
            score *= 1.1;
            break;
        case GPUArchitecture::TURING:
        case GPUArchitecture::RDNA:
        case GPUArchitecture::XE:
            score *= 1.0;
            break;
        default:
            score *= 0.9;
            break;
    }
    
    // Vendor-specific adjustments
    switch (info.vendor) {
        case GPUVendor::NVIDIA:
            score *= 1.05;  // Slight bonus for CUDA ecosystem
            break;
        case GPUVendor::AMD:
        case GPUVendor::INTEL:
            score *= 1.0;
            break;
        default:
            score *= 0.95;
            break;
    }
    
    return std::min(score, 100.0);  // Cap at 100
}

auto validateGPUInfo(const GPUInfo& info) -> bool {
    // Basic validation checks
    if (info.name.empty()) return false;
    if (info.vendor == GPUVendor::UNKNOWN) return false;
    if (info.memoryInfo.totalMemory == 0) return false;
    
    // Performance metrics validation
    if (info.performance.gpuUtilization < 0.0 || info.performance.gpuUtilization > 100.0) return false;
    if (info.performance.temperature < -50.0 || info.performance.temperature > 150.0) return false;
    
    return true;
}

auto getGPUSummary(const GPUInfo& info) -> std::string {
    std::ostringstream oss;
    oss << info.name << " (" << gpuVendorToString(info.vendor) << ")\n";
    oss << "Type: " << gpuTypeToString(info.type) << "\n";
    oss << "Architecture: " << gpuArchitectureToString(info.architecture) << "\n";
    oss << "Memory: " << formatMemorySize(info.memoryInfo.totalMemory) << "\n";
    oss << "Temperature: " << formatTemperature(info.performance.temperature) << "\n";
    oss << "Utilization: " << std::fixed << std::setprecision(1) << info.performance.gpuUtilization << "%\n";
    oss << "Driver: " << info.driver.driverVersion;
    return oss.str();
}

auto compareGPUPerformance(const GPUPerformanceMetrics& a, const GPUPerformanceMetrics& b) -> int {
    double scoreA = a.gpuUtilization + (a.coreClockSpeed / 1000.0) + (a.memoryClockSpeed / 1000.0);
    double scoreB = b.gpuUtilization + (b.coreClockSpeed / 1000.0) + (b.memoryClockSpeed / 1000.0);
    
    if (scoreA > scoreB) return 1;
    if (scoreA < scoreB) return -1;
    return 0;
}

auto supportsFeature(const GPUInfo& info, const std::string& feature) -> bool {
    // Check in supported APIs
    for (const auto& api : info.compute.supportedAPIs) {
        if (api.find(feature) != std::string::npos) return true;
    }
    
    for (const auto& api : info.compute.supportedComputeAPIs) {
        if (api.find(feature) != std::string::npos) return true;
    }
    
    // Check in driver features
    for (const auto& driverFeature : info.driver.supportedFeatures) {
        if (driverFeature.find(feature) != std::string::npos) return true;
    }
    
    return false;
}

auto getRecommendedSettings(const GPUInfo& info) -> std::map<std::string, std::string> {
    std::map<std::string, std::string> settings;
    
    // Power management recommendations
    if (info.performance.powerDraw > info.performance.maxPowerLimit * 0.9) {
        settings["power_management"] = "Consider reducing power limit";
    }
    
    // Temperature recommendations
    if (info.performance.temperature > 80.0) {
        settings["cooling"] = "Improve cooling or reduce clock speeds";
    }
    
    // Memory recommendations
    if (info.memoryInfo.memoryUsagePercent > 90.0) {
        settings["memory"] = "Reduce memory usage or upgrade GPU";
    }
    
    // Performance recommendations based on architecture
    switch (info.architecture) {
        case GPUArchitecture::AMPERE:
        case GPUArchitecture::ADA_LOVELACE:
            settings["dlss"] = "Enable DLSS for better performance";
            break;
        case GPUArchitecture::RDNA2:
        case GPUArchitecture::RDNA3:
            settings["fsr"] = "Enable FSR for better performance";
            break;
        default:
            break;
    }
    
    return settings;
}

auto detectThermalThrottling(const GPUPerformanceMetrics& metrics) -> bool {
    // Check if current clock is significantly below boost clock
    if (metrics.boostClock > 0 && metrics.coreClockSpeed > 0) {
        double clockRatio = metrics.coreClockSpeed / metrics.boostClock;
        if (clockRatio < 0.8 && metrics.temperature > 75.0) {
            return true;
        }
    }
    
    // Check temperature threshold
    if (metrics.temperature > 85.0) {
        return true;
    }
    
    return false;
}

auto calculatePowerEfficiency(const GPUPerformanceMetrics& metrics) -> double {
    if (metrics.powerDraw <= 0.0) return 0.0;

    // Simple efficiency calculation: utilization per watt
    return metrics.gpuUtilization / metrics.powerDraw;
}

}  // namespace atom::system
