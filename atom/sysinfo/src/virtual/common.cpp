#include "common.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

#ifdef _WIN32
#include <intrin.h>
#include <windows.h>
#else
#include <cpuid.h>
#include <unistd.h>
#endif

#include <spdlog/spdlog.h>

namespace atom::system::virtual_env {

auto executeCommand(std::string_view command) -> std::string {
    std::array<char, 128> buffer;
    std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.data(), "r"), pclose);

    if (!pipe) {
        spdlog::error("Failed to execute command: {}", command);
        return {};
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

template<size_t N>
auto containsKeywords(std::string_view text,
                     const std::array<std::string_view, N>& keywords) -> bool {
    std::string lowerText = toLowercase(text);

    return std::any_of(keywords.begin(), keywords.end(),
        [&lowerText](std::string_view keyword) {
            std::string lowerKeyword = toLowercase(keyword);
            return lowerText.find(lowerKeyword) != std::string::npos;
        });
}

auto containsVMKeywords(std::string_view text) -> bool {
    return containsKeywords(text, keywords::VM_KEYWORDS);
}

auto containsContainerKeywords(std::string_view text) -> bool {
    return containsKeywords(text, keywords::CONTAINER_KEYWORDS);
}

auto containsCloudKeywords(std::string_view text) -> bool {
    return containsKeywords(text, keywords::CLOUD_KEYWORDS);
}

auto toLowercase(std::string_view str) -> std::string {
    std::string result;
    result.reserve(str.size());
    std::transform(str.begin(), str.end(), std::back_inserter(result),
                   [](char c) { return std::tolower(c); });
    return result;
}

auto readFileContent(std::string_view filepath) -> std::string {
    std::ifstream file(filepath.data());
    if (!file.is_open()) {
        return {};
    }

    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

auto fileExists(std::string_view filepath) -> bool {
    return std::filesystem::exists(filepath);
}

auto getCPUInfo(unsigned int leaf) -> std::array<unsigned int, 4> {
    std::array<unsigned int, 4> cpuInfo = {0};

#ifdef _WIN32
    __cpuid(reinterpret_cast<int*>(cpuInfo.data()), leaf);
#else
    __get_cpuid(leaf, &cpuInfo[0], &cpuInfo[1], &cpuInfo[2], &cpuInfo[3]);
#endif

    return cpuInfo;
}

auto formatDetectionReport(const std::vector<DetectionResult>& results) -> std::string {
    std::ostringstream report;
    report << "Virtualization Detection Report\n";
    report << "================================\n\n";

    double totalConfidence = 0.0;
    int detectedCount = 0;

    for (const auto& result : results) {
        report << "Method: " << result.method_name << "\n";
        report << "  Detected: " << (result.detected ? "Yes" : "No") << "\n";
        report << "  Confidence: " << (result.confidence * 100) << "%\n";

        if (!result.details.empty()) {
            report << "  Details: " << result.details << "\n";
        }

        if (!result.indicators.empty()) {
            report << "  Indicators:\n";
            for (const auto& indicator : result.indicators) {
                report << "    - " << indicator << "\n";
            }
        }

        report << "\n";

        if (result.detected) {
            totalConfidence += result.confidence;
            detectedCount++;
        }
    }

    if (detectedCount > 0) {
        report << "Overall Confidence: " << (totalConfidence / detectedCount * 100) << "%\n";
    } else {
        report << "Overall Confidence: 0%\n";
    }

    return report.str();
}

auto calculateConfidenceScore(const std::vector<DetectionResult>& results) -> double {
    double totalWeight = 0.0;
    double evidenceWeight = 0.0;

    // Define weights for different detection methods
    std::unordered_map<std::string, double> methodWeights = {
        {"CPUID", constants::CPUID_WEIGHT},
        {"BIOS", constants::BIOS_WEIGHT},
        {"Network", constants::NETWORK_WEIGHT},
        {"Disk", constants::DISK_WEIGHT},
        {"Graphics", constants::GRAPHICS_WEIGHT},
        {"Processes", constants::PROCESS_WEIGHT},
        {"PCI Bus", constants::PCI_WEIGHT},
        {"Time Drift", constants::TIME_DRIFT_WEIGHT}
    };

    for (const auto& result : results) {
        double weight = 0.1; // Default weight
        auto it = methodWeights.find(result.method_name);
        if (it != methodWeights.end()) {
            weight = it->second;
        }

        totalWeight += weight;
        if (result.detected) {
            evidenceWeight += weight * result.confidence;
        }
    }

    return totalWeight > 0.0 ? evidenceWeight / totalWeight : 0.0;
}

namespace platform {
    auto isWindows() -> bool {
#ifdef _WIN32
        return true;
#else
        return false;
#endif
    }

    auto isLinux() -> bool {
#ifdef __linux__
        return true;
#else
        return false;
#endif
    }

    auto isMacOS() -> bool {
#ifdef __APPLE__
        return true;
#else
        return false;
#endif
    }

    auto getPlatformName() -> std::string {
        if (isWindows()) return "Windows";
        if (isLinux()) return "Linux";
        if (isMacOS()) return "macOS";
        return "Unknown";
    }
}

} // namespace atom::system::virtual_env
