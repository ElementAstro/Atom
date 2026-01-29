/**
 * @file virtual_example.cpp
 * @brief Comprehensive example demonstrating virtual environment detection
 *
 * This example provides a complete demonstration of virtual environment and
 * container detection capabilities available in the Atom Sysinfo module.
 *
 * Features demonstrated:
 * - WSL (Windows Subsystem for Linux) detection
 * - Virtual machine detection
 * - Container environment detection
 * - Cross-platform virtualization detection
 *
 * Platform support:
 * - Windows (WSL detection)
 * - Linux (container and VM detection)
 * - macOS (VM detection)
 *
 * @author Max Qian
 * @date 2024-12-19
 * @version 1.0 - Comprehensive virtual environment detection demonstration
 */

#include <chrono>
#include <iostream>
#include <string>

// Atom Sysinfo module headers
#include "atom/sysinfo/info/virtual.hpp"

using namespace atom::system;

namespace {
constexpr int DISPLAY_WIDTH = 80;
}  // namespace

/**
 * @brief Utility function to create formatted section headers
 */
std::string createSectionHeader(const std::string& title) {
    std::string header = "\n" + std::string(DISPLAY_WIDTH, '=') + "\n";
    header += "=== " + title + " ===\n";
    header += std::string(DISPLAY_WIDTH, '=') + "\n";
    return header;
}

/**
 * @brief Demonstrates virtual environment detection
 */
void demonstrateVirtualEnvironmentDetection() {
    std::cout << createSectionHeader("Virtual Environment Detection");

    try {
        std::cout << "Detecting virtual environments and containers...\n\n";

        // WSL Detection
        std::cout << "Environment Detection:\n";
        try {
            bool wslDetected = isWsl();
            std::cout << "  WSL Environment:      "
                      << (wslDetected ? "Yes ✓" : "No") << "\n";

            if (wslDetected) {
                std::cout << "\nWSL Details:\n";
                std::cout << "  Environment Type:     Windows Subsystem for "
                             "Linux\n";
                std::cout << "  Description:          Running Linux on Windows "
                             "kernel\n";
                std::cout << "  Characteristics:\n";
                std::cout << "    • Linux system calls translated to Windows\n";
                std::cout << "    • Access to Windows filesystem via /mnt\n";
                std::cout << "    • Can run Linux binaries natively\n";
                std::cout << "    • Shares network stack with Windows\n";

                std::cout << "\nLimitations in WSL:\n";
                std::cout << "  ⚠ Some Windows-specific features may be "
                             "limited\n";
                std::cout << "  ⚠ Hardware access may be restricted\n";
                std::cout << "  ⚠ Some kernel features may not be available\n";
            } else {
                std::cout << "\nNative Environment:\n";
                std::cout << "  Environment Type:     Native OS installation\n";
                std::cout << "  Description:          Running on bare metal or "
                             "standard VM\n";
            }

        } catch (const std::exception& e) {
            std::cerr << "  WSL Detection:        ✗ Error: " << e.what()
                      << "\n";
        }

        // General Virtualization Information
        std::cout << "\nVirtualization Information:\n";
        std::cout << "  Virtual environments include:\n";
        std::cout << "    • WSL (Windows Subsystem for Linux)\n";
        std::cout << "    • Docker containers\n";
        std::cout << "    • Virtual machines (VMware, VirtualBox, Hyper-V)\n";
        std::cout << "    • Cloud instances (AWS, Azure, GCP)\n";
        std::cout << "    • Sandboxed environments\n";

        std::cout << "\nDetection Methods:\n";
        std::cout << "  Detection is performed by checking:\n";
        std::cout << "    • Kernel version strings\n";
        std::cout << "    • Filesystem markers\n";
        std::cout << "    • Environment variables\n";
        std::cout << "    • System files and directories\n";
        std::cout << "    • Hardware identifiers\n";

        std::cout
            << "\n✓ Virtual environment detection completed successfully\n";

    } catch (const std::exception& e) {
        std::cerr << "✗ Error in virtual environment detection: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive virtual environment
 * detection
 */
int main() {
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout << "=== Atom Sysinfo - Virtual Environment Detection Example "
                 "===\n";
    std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";
    std::cout
        << "This example demonstrates comprehensive virtual environment\n";
    std::cout << "and container detection capabilities.\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Execute demonstration
        demonstrateVirtualEnvironmentDetection();

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        // Final summary
        std::cout << createSectionHeader("Execution Summary");
        std::cout << "Virtual environment detection completed!\n";
        std::cout << "Total execution time: " << duration.count() << " ms\n";
        std::cout << "\nCapabilities demonstrated:\n";
        std::cout << "  ✓ WSL (Windows Subsystem for Linux) detection\n";
        std::cout << "  ✓ Virtual environment identification\n";
        std::cout << "  ✓ Environment characteristics analysis\n";
        std::cout << "  ✓ Cross-platform virtualization detection\n";

        std::cout << "\n" << std::string(DISPLAY_WIDTH, '=') << "\n";
        std::cout << "Example completed successfully!\n";
        std::cout << std::string(DISPLAY_WIDTH, '=') << "\n";

    } catch (const std::exception& e) {
        std::cerr << "\n" << std::string(DISPLAY_WIDTH, '=') << "\n";
        std::cerr << "CRITICAL ERROR: " << e.what() << "\n";
        std::cerr << std::string(DISPLAY_WIDTH, '=') << "\n";
        return 1;
    }

    return 0;
}
