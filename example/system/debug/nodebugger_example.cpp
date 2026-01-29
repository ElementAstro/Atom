/**
 * @file nodebugger_example.cpp
 * @brief Example demonstrating anti-debugging and debugger detection
 *
 * This example shows how to use the anti-debugging functionality to:
 * - Detect if a debugger is attached
 * - Handle debugger detection with various actions
 * - Monitor for debugger attachment
 * - Protect against tampering
 */

#include "atom/system/nodebugger.hpp"

#include <chrono>
#include <iostream>
#include <thread>

using namespace atom::system;

void demonstrateBasicDebuggerDetection() {
    std::cout << "\n=== Basic Debugger Detection ===" << std::endl;

    bool debuggerPresent =
        isDebuggerAttached(DebuggerDetectionMethod::BASIC_CHECK);

    if (debuggerPresent) {
        std::cout << "⚠ Debugger detected!" << std::endl;
    } else {
        std::cout << "✓ No debugger detected" << std::endl;
    }
}

void demonstrateAdvancedDetection() {
    std::cout << "\n=== Advanced Detection Methods ===" << std::endl;

    DebuggerDetectionMethod methods[] = {
        DebuggerDetectionMethod::BASIC_CHECK,
        DebuggerDetectionMethod::TIMING_CHECK,
        DebuggerDetectionMethod::EXCEPTION_BASED,
        DebuggerDetectionMethod::HARDWARE_BREAKPOINTS,
        DebuggerDetectionMethod::PROCESS_ENVIRONMENT};

    const char* methodNames[] = {"Basic Check", "Timing Check",
                                 "Exception Based", "Hardware Breakpoints",
                                 "Process Environment"};

    for (size_t i = 0; i < sizeof(methods) / sizeof(methods[0]); ++i) {
        bool detected = isDebuggerAttached(methods[i]);
        std::cout << "  " << methodNames[i] << ": "
                  << (detected ? "⚠ DETECTED" : "✓ Clear") << std::endl;
    }
}

void demonstrateCustomAction() {
    std::cout << "\n=== Custom Debugger Detection Action ===" << std::endl;

    AntiDebugConfig config;
    config.enabled = true;
    config.method = DebuggerDetectionMethod::BASIC_CHECK;
    config.action = AntiDebugAction::CUSTOM;
    config.customAction = []() {
        std::cout << "⚠ Custom action triggered: Debugger detected!"
                  << std::endl;
        std::cout << "  This is where you would implement your custom response"
                  << std::endl;
    };

    handleDebuggerDetection(config);
    std::cout << "✓ Custom action handler configured" << std::endl;
}

void demonstrateContinuousMonitoring() {
    std::cout << "\n=== Continuous Debugger Monitoring ===" << std::endl;

    AntiDebugConfig config;
    config.enabled = true;
    config.method = DebuggerDetectionMethod::BASIC_CHECK;
    config.action = AntiDebugAction::CUSTOM;
    config.customAction = []() {
        std::cout << "⚠ Debugger detected during monitoring!" << std::endl;
    };
    config.continuousMonitoring = true;
    config.checkInterval = 1000;  // Check every second

    std::cout << "Starting continuous monitoring for 5 seconds..." << std::endl;
    startAntiDebugMonitoring(config);

    // Monitor for 5 seconds
    std::this_thread::sleep_for(std::chrono::seconds(5));

    stopAntiDebugMonitoring();
    std::cout << "✓ Monitoring stopped" << std::endl;
}

void demonstrateTimingBasedDetection() {
    std::cout << "\n=== Timing-Based Detection ===" << std::endl;

    AntiDebugConfig config;
    config.enabled = true;
    config.method = DebuggerDetectionMethod::TIMING_CHECK;
    config.timingThreshold = 10000;  // 10ms threshold
    config.action = AntiDebugAction::CUSTOM;
    config.customAction = []() {
        std::cout << "⚠ Timing anomaly detected (possible debugger)"
                  << std::endl;
    };

    handleDebuggerDetection(config);
    std::cout << "✓ Timing-based detection completed" << std::endl;
}

void demonstrateMemoryProtection() {
    std::cout << "\n=== Memory Protection ===" << std::endl;

    try {
        // Example: Protect a small memory region
        char protectedData[256] = "This is protected data";

        std::cout << "Protecting memory region..." << std::endl;
        protectMemoryRegion(protectedData, sizeof(protectedData));
        std::cout << "✓ Memory region protected" << std::endl;

        // Note: Attempting to modify this region from a debugger would trigger
        // protection
    } catch (const std::exception& e) {
        std::cerr << "✗ Memory protection failed: " << e.what() << std::endl;
    }
}

#ifdef _WIN32
void demonstrateWindowsSpecific() {
    std::cout << "\n=== Windows-Specific Anti-Debug Features ===" << std::endl;

    try {
        std::cout << "Hiding PEB debugging flags..." << std::endl;
        hidePEBDebuggingFlags();
        std::cout << "✓ PEB flags hidden" << std::endl;

        std::cout << "Detecting remote threads..." << std::endl;
        detectRemoteThreads();
        std::cout << "✓ Remote thread detection completed" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ Windows-specific feature failed: " << e.what()
                  << std::endl;
    }
}
#endif

int main() {
    std::cout << "Anti-Debugging and Debugger Detection Example" << std::endl;
    std::cout << "==============================================" << std::endl;

    try {
        demonstrateBasicDebuggerDetection();
        demonstrateAdvancedDetection();
        demonstrateCustomAction();
        demonstrateContinuousMonitoring();
        demonstrateTimingBasedDetection();
        demonstrateMemoryProtection();

#ifdef _WIN32
        demonstrateWindowsSpecific();
#endif

        std::cout << "\n✓ All anti-debugging demonstrations completed!"
                  << std::endl;
        std::cout
            << "\nNote: These features are designed to protect applications"
            << std::endl;
        std::cout
            << "      from reverse engineering and unauthorized debugging."
            << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
