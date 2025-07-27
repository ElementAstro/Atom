/**
 * @file test_common.cpp
 * @brief Common test utilities and basic functionality tests
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

// Test utility functions
bool testErrorToString() {
    std::string result = errorToString(WMError::NONE);
    if (result.empty()) return false;

    result = errorToString(WMError::PLATFORM_NOT_SUPPORTED);
    if (result.empty()) return false;

    result = errorToString(WMError::WINDOW_NOT_FOUND);
    if (result.empty()) return false;

    return true;
}

bool testWindowStateToString() {
    std::string result = windowStateToString(WindowState::NORMAL);
    if (result.empty()) return false;

    result = windowStateToString(WindowState::MINIMIZED);
    if (result.empty()) return false;

    result = windowStateToString(WindowState::MAXIMIZED);
    if (result.empty()) return false;

    return true;
}

bool testWindowTypeToString() {
    std::string result = windowTypeToString(WindowType::NORMAL);
    if (result.empty()) return false;

    result = windowTypeToString(WindowType::DIALOG);
    if (result.empty()) return false;

    result = windowTypeToString(WindowType::UTILITY);
    if (result.empty()) return false;

    return true;
}

bool testThemeTypeToString() {
    std::string result = themeTypeToString(ThemeType::LIGHT);
    if (result.empty()) return false;

    result = themeTypeToString(ThemeType::DARK);
    if (result.empty()) return false;

    result = themeTypeToString(ThemeType::AUTO);
    if (result.empty()) return false;

    return true;
}

bool testResultHelpers() {
    // Test with successful result
    WMResult<int> successResult = 42;
    if (isError(successResult)) return false;
    if (getValue(successResult) != 42) return false;

    // Test with error result
    WMResult<int> errorResult = WMError::OPERATION_FAILED;
    if (!isError(errorResult)) return false;
    if (getError(errorResult) != WMError::OPERATION_FAILED) return false;

    return true;
}

bool testWindowInfoStructure() {
    WindowInfo window;
    window.id = 12345;
    window.title = "Test Window";
    window.className = "TestClass";
    window.processName = "test.exe";
    window.processId = 1000;
    window.state = WindowState::NORMAL;
    window.type = WindowType::NORMAL;
    window.x = 100;
    window.y = 200;
    window.width = 800;
    window.height = 600;
    window.isVisible = true;
    window.isActive = false;
    window.workspaceId = 1;

    // Basic validation
    return window.id == 12345 &&
           window.title == "Test Window" &&
           window.state == WindowState::NORMAL &&
           window.workspaceId.has_value() &&
           window.workspaceId.value() == 1;
}

bool testThemeInfoStructure() {
    ThemeInfo theme;
    theme.type = ThemeType::DARK;
    theme.name = "Dark Theme";
    theme.variant = "dark";
    theme.accentColor = "#0078D4";
    theme.backgroundColor = "#2E2E2E";
    theme.foregroundColor = "#FFFFFF";
    theme.followsSystemTheme = true;

    // Basic validation
    return theme.type == ThemeType::DARK &&
           theme.name == "Dark Theme" &&
           theme.followsSystemTheme;
}

bool testMonitorInfoStructure() {
    MonitorInfo monitor;
    monitor.id = 0;
    monitor.name = "Primary Monitor";
    monitor.isPrimary = true;
    monitor.x = 0;
    monitor.y = 0;
    monitor.width = 1920;
    monitor.height = 1080;
    monitor.refreshRate = 60;
    monitor.scaleFactor = 1.0f;

    // Basic validation
    return monitor.isPrimary &&
           monitor.width == 1920 &&
           monitor.height == 1080 &&
           monitor.scaleFactor == 1.0f;
}

bool testWorkspaceInfoStructure() {
    WorkspaceInfo workspace;
    workspace.id = 0;
    workspace.name = "Desktop 1";
    workspace.isActive = true;
    workspace.windowIds = {1, 2, 3};
    workspace.x = 0;
    workspace.y = 0;
    workspace.width = 1920;
    workspace.height = 1080;

    // Basic validation
    return workspace.isActive &&
           workspace.windowIds.size() == 3 &&
           workspace.name == "Desktop 1";
}

bool testSystemInfoStructure() {
    SystemInfo info;
    info.desktopEnvironment = "Test DE";
    info.windowManager = "Test WM";
    info.wmTheme = "Test Theme";
    info.icons = "Test Icons";
    info.font = "Test Font";
    info.cursor = "Test Cursor";
    info.supportsWorkspaces = true;
    info.supportsVirtualDesktops = true;
    info.wmVersion = "1.0";
    info.wmFeatures = {"Feature1", "Feature2"};

    // Basic validation
    return info.supportsWorkspaces &&
           info.supportsVirtualDesktops &&
           info.wmFeatures.size() == 2;
}

// Forward declarations for other test functions
extern int runSystemInfoTests();
extern int runWindowManagementTests();
extern int runThemeDetectionTests();

int runCommonTests() {
    std::cout << "Running Common Tests" << std::endl;
    std::cout << "===================" << std::endl;

    // Test utility functions
    TestRunner::run("errorToString", testErrorToString);
    TestRunner::run("windowStateToString", testWindowStateToString);
    TestRunner::run("windowTypeToString", testWindowTypeToString);
    TestRunner::run("themeTypeToString", testThemeTypeToString);
    TestRunner::run("resultHelpers", testResultHelpers);

    // Test data structures
    TestRunner::run("WindowInfo structure", testWindowInfoStructure);
    TestRunner::run("ThemeInfo structure", testThemeInfoStructure);
    TestRunner::run("MonitorInfo structure", testMonitorInfoStructure);
    TestRunner::run("WorkspaceInfo structure", testWorkspaceInfoStructure);
    TestRunner::run("SystemInfo structure", testSystemInfoStructure);

    TestRunner::printSummary();
    return TestRunner::getFailedCount();
}

int main() {
    std::cout << "Window Manager Module - Comprehensive Test Suite" << std::endl;
    std::cout << "================================================" << std::endl;

    int totalFailures = 0;

    // Run all test suites
    totalFailures += runCommonTests();
    std::cout << std::endl;

    totalFailures += runSystemInfoTests();
    std::cout << std::endl;

    totalFailures += runWindowManagementTests();
    std::cout << std::endl;

    totalFailures += runThemeDetectionTests();
    std::cout << std::endl;

    // Final summary
    std::cout << "=== FINAL TEST SUMMARY ===" << std::endl;
    std::cout << "Total failures across all test suites: " << totalFailures << std::endl;

    if (totalFailures == 0) {
        std::cout << "🎉 All tests passed!" << std::endl;
    } else {
        std::cout << "❌ " << totalFailures << " test(s) failed" << std::endl;
    }

    return totalFailures > 0 ? 1 : 0;
}
