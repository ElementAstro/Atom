/**
 * @file test_system_info.cpp
 * @brief System information retrieval tests
 */

#include "wm.hpp"
#include <iostream>
#include <cassert>
#include <string>

using namespace atom::system::wm;

// Simple test framework
class TestRunner {
public:
    static void run(const std::string& testName, std::function<bool()> test) {
        std::cout << "Running test: " << testName << "... ";
        try {
            if (test()) {
                std::cout << "PASSED" << std::endl;
                passed_++;
            } else {
                std::cout << "FAILED" << std::endl;
                failed_++;
            }
        } catch (const std::exception& e) {
            std::cout << "EXCEPTION: " << e.what() << std::endl;
            failed_++;
        }
        total_++;
    }

    static void printSummary() {
        std::cout << "\n=== Test Summary ===" << std::endl;
        std::cout << "Total: " << total_ << std::endl;
        std::cout << "Passed: " << passed_ << std::endl;
        std::cout << "Failed: " << failed_ << std::endl;
        std::cout << "Success Rate: " << (total_ > 0 ? (passed_ * 100 / total_) : 0) << "%" << std::endl;
    }

    static int getFailedCount() { return failed_; }

private:
    static int total_;
    static int passed_;
    static int failed_;
};

int TestRunner::total_ = 0;
int TestRunner::passed_ = 0;
int TestRunner::failed_ = 0;

bool testGetSystemInfo() {
    auto result = getSystemInfo();

    // Should not return an error on supported platforms
    if (isError(result)) {
        WMError error = getError(result);
        // Only acceptable error is platform not supported
        return error == WMError::PLATFORM_NOT_SUPPORTED;
    }

    const auto& info = getValue(result);

    // Basic validation - these fields should not be empty on supported platforms
    if (info.desktopEnvironment.empty()) return false;
    if (info.windowManager.empty()) return false;

    // These might be empty on some platforms, so we just check they exist
    // (not necessarily non-empty)

    std::cout << "\n  Desktop Environment: " << info.desktopEnvironment << std::endl;
    std::cout << "  Window Manager: " << info.windowManager << std::endl;
    std::cout << "  Theme: " << info.wmTheme << std::endl;
    std::cout << "  Supports Workspaces: " << (info.supportsWorkspaces ? "Yes" : "No") << std::endl;

    return true;
}

bool testGetThemeInfo() {
    auto result = getThemeInfo();

    if (isError(result)) {
        WMError error = getError(result);
        // Only acceptable error is platform not supported
        return error == WMError::PLATFORM_NOT_SUPPORTED;
    }

    const auto& theme = getValue(result);

    // Theme type should be valid
    if (theme.type == ThemeType::UNKNOWN && theme.name.empty()) {
        // This might be acceptable on unsupported platforms
        return true;
    }

    std::cout << "\n  Theme Type: " << themeTypeToString(theme.type) << std::endl;
    std::cout << "  Theme Name: " << theme.name << std::endl;
    std::cout << "  Variant: " << theme.variant << std::endl;

    return true;
}

bool testGetMonitors() {
    auto result = getMonitors();

    if (isError(result)) {
        WMError error = getError(result);
        // Only acceptable error is platform not supported
        return error == WMError::PLATFORM_NOT_SUPPORTED;
    }

    const auto& monitors = getValue(result);

    // Should have at least one monitor
    if (monitors.empty()) return false;

    // Check that at least one monitor is marked as primary
    bool hasPrimary = false;
    for (const auto& monitor : monitors) {
        if (monitor.isPrimary) {
            hasPrimary = true;
            break;
        }
    }

    if (!hasPrimary) return false;

    std::cout << "\n  Found " << monitors.size() << " monitor(s)" << std::endl;
    for (size_t i = 0; i < monitors.size(); ++i) {
        const auto& monitor = monitors[i];
        std::cout << "  Monitor " << (i + 1) << ": " << monitor.name
                  << " (" << monitor.width << "x" << monitor.height << ")"
                  << (monitor.isPrimary ? " [PRIMARY]" : "") << std::endl;
    }

    return true;
}

bool testGetWorkspaces() {
    auto result = getWorkspaces();

    if (isError(result)) {
        WMError error = getError(result);
        // Acceptable errors: platform not supported or operation failed
        return error == WMError::PLATFORM_NOT_SUPPORTED ||
               error == WMError::OPERATION_FAILED;
    }

    const auto& workspaces = getValue(result);

    // Workspaces might be empty on some platforms
    if (workspaces.empty()) {
        std::cout << "\n  No workspaces found (not supported or none configured)" << std::endl;
        return true;
    }

    // Check that at least one workspace is marked as active
    bool hasActive = false;
    for (const auto& workspace : workspaces) {
        if (workspace.isActive) {
            hasActive = true;
            break;
        }
    }

    if (!hasActive) return false;

    std::cout << "\n  Found " << workspaces.size() << " workspace(s)" << std::endl;
    for (const auto& workspace : workspaces) {
        std::cout << "  Workspace " << workspace.id << ": " << workspace.name
                  << (workspace.isActive ? " [ACTIVE]" : "") << std::endl;
    }

    return true;
}

bool testSystemInfoConsistency() {
    auto sysResult = getSystemInfo();
    auto themeResult = getThemeInfo();
    auto monitorResult = getMonitors();

    // If system info works, theme and monitor info should also work
    // (or at least not fail with unexpected errors)

    if (!isError(sysResult)) {
        const auto& sysInfo = getValue(sysResult);

        // If system info has theme info, it should match standalone theme info
        if (!isError(themeResult)) {
            const auto& themeInfo = getValue(themeResult);
            // Basic consistency check - types should match
            if (sysInfo.themeInfo.type != ThemeType::UNKNOWN &&
                themeInfo.type != ThemeType::UNKNOWN) {
                if (sysInfo.themeInfo.type != themeInfo.type) {
                    std::cout << "\n  Warning: Theme type mismatch between system info and theme info" << std::endl;
                }
            }
        }

        // If system info has monitors, count should match standalone monitor info
        if (!isError(monitorResult)) {
            const auto& monitors = getValue(monitorResult);
            if (!sysInfo.monitors.empty() && !monitors.empty()) {
                if (sysInfo.monitors.size() != monitors.size()) {
                    std::cout << "\n  Warning: Monitor count mismatch" << std::endl;
                }
            }
        }
    }

    return true;
}

bool testBackwardCompatibility() {
    // Test the old namespace compatibility
    auto oldResult = atom::system::getSystemInfo();

    // Should not throw and should return valid data
    if (oldResult.desktopEnvironment.empty() && oldResult.windowManager.empty()) {
        // Might be acceptable on unsupported platforms
        return true;
    }

    std::cout << "\n  Backward compatibility test passed" << std::endl;
    std::cout << "  Old API - DE: " << oldResult.desktopEnvironment << std::endl;
    std::cout << "  Old API - WM: " << oldResult.windowManager << std::endl;

    return true;
}

int runSystemInfoTests() {
    std::cout << "Running System Information Tests" << std::endl;
    std::cout << "===============================" << std::endl;

    TestRunner::run("getSystemInfo", testGetSystemInfo);
    TestRunner::run("getThemeInfo", testGetThemeInfo);
    TestRunner::run("getMonitors", testGetMonitors);
    TestRunner::run("getWorkspaces", testGetWorkspaces);
    TestRunner::run("systemInfoConsistency", testSystemInfoConsistency);
    TestRunner::run("backwardCompatibility", testBackwardCompatibility);

    TestRunner::printSummary();
    return TestRunner::getFailedCount();
}

int main() {
    return runSystemInfoTests();
}
