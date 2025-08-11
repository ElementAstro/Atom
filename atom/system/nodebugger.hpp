#ifndef ATOM_SYSTEM_NODEBUGGER_HPP
#define ATOM_SYSTEM_NODEBUGGER_HPP

#include <cstdint>
#include <functional>

namespace atom::system {

enum class DebuggerDetectionMethod {
    BASIC_CHECK,
    TIMING_CHECK,
    EXCEPTION_BASED,
    HARDWARE_BREAKPOINTS,
    MEMORY_BREAKPOINTS,
    PROCESS_ENVIRONMENT,
    PARENT_PROCESS,
    THREAD_CONTEXT,
    ALL_METHODS
};

enum class AntiDebugAction { EXIT, CRASH, MISLEAD, CORRUPT_MEMORY, CUSTOM };

struct AntiDebugConfig {
    bool enabled = true;
    DebuggerDetectionMethod method = DebuggerDetectionMethod::BASIC_CHECK;
    AntiDebugAction action = AntiDebugAction::EXIT;
    std::function<void()> customAction = nullptr;
    uint32_t timingThreshold = 10000;  // Microseconds for timing checks
    bool continuousMonitoring = false;
    uint32_t checkInterval = 500;  // Milliseconds between checks if continuous
};

// Basic API (backward compatible)
void checkDebuggerAndExit();

// Enhanced API
bool isDebuggerAttached(
    DebuggerDetectionMethod method = DebuggerDetectionMethod::BASIC_CHECK);
void handleDebuggerDetection(const AntiDebugConfig& config = AntiDebugConfig{});
void startAntiDebugMonitoring(
    const AntiDebugConfig& config = AntiDebugConfig{});
void stopAntiDebugMonitoring();

// Anti-tampering functions
void protectMemoryRegion(void* address, size_t size);
void installIntegrityChecks(const void* codeStart, size_t codeSize,
                            const uint8_t* hash);
void preventDumping();

// Advanced windows-specific functionality (will be no-op on other platforms)
#ifdef _WIN32
void hidePEBDebuggingFlags();
void detectRemoteThreads();
void enableSelfModifyingCode(void* codeAddress, size_t codeSize);
#endif

}  // namespace atom::system

#endif  // ATOM_SYSTEM_NODEBUGGER_HPP
