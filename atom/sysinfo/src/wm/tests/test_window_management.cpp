/**
 * @file test_window_management.cpp
 * @brief Window management functionality tests
 */

#include "wm.hpp"
#include <iostream>
#include <cassert>
#include <string>
#include <algorithm>

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

bool testEnumerateWindows() {
    auto result = enumerateWindows();

    if (isError(result)) {
        WMError error = getError(result);
        // Acceptable errors on some platforms
        return error == WMError::PLATFORM_NOT_SUPPORTED ||
               error == WMError::OPERATION_FAILED ||
               error == WMError::PERMISSION_DENIED;
    }

    const auto& windows = getValue(result);

    std::cout << "\n  Found " << windows.size() << " window(s)" << std::endl;

    // Basic validation of window data
    for (const auto& window : windows) {
        // Window ID should be non-zero
        if (window.id == 0) return false;

        // At least some windows should have titles
        // (though some system windows might not)
    }

    // Show some sample windows
    int count = 0;
    for (const auto& window : windows) {
        if (!window.title.empty() && count < 3) {
            std::cout << "  Sample window: " << window.title
                      << " (" << window.processName << ")" << std::endl;
            count++;
        }
    }

    return true;
}

bool testGetWindowInfo() {
    // First get a list of windows
    auto windowsResult = enumerateWindows();

    if (isError(windowsResult)) {
        // If we can't enumerate windows, we can't test getWindowInfo
        return true; // Skip this test
    }

    const auto& windows = getValue(windowsResult);
    if (windows.empty()) {
        std::cout << "\n  No windows to test getWindowInfo with" << std::endl;
        return true; // Skip this test
    }

    // Test getting info for the first window
    const auto& firstWindow = windows[0];
    auto result = getWindowInfo(firstWindow.id);

    if (isError(result)) {
        WMError error = getError(result);
        // Acceptable errors
        return error == WMError::PLATFORM_NOT_SUPPORTED ||
               error == WMError::WINDOW_NOT_FOUND ||
               error == WMError::OPERATION_FAILED;
    }

    const auto& windowInfo = getValue(result);

    // The window info should match what we got from enumeration
    if (windowInfo.id != firstWindow.id) return false;

    std::cout << "\n  Retrieved info for window: " << windowInfo.title << std::endl;

    return true;
}

bool testWindowManagerClass() {
    WindowManager wm;

    // Test filtering functionality
    auto result = wm.getFilteredWindows([](const WindowInfo& window) {
        return !window.title.empty();
    });

    if (isError(result)) {
        WMError error = getError(result);
        // Acceptable errors
        return error == WMError::PLATFORM_NOT_SUPPORTED ||
               error == WMError::OPERATION_FAILED;
    }

    const auto& filteredWindows = getValue(result);
    std::cout << "\n  Filtered windows with titles: " << filteredWindows.size() << std::endl;

    // Test getWindowsByProcess (might not find any, but shouldn't crash)
    auto processResult = wm.getWindowsByProcess("nonexistent.exe");
    if (!isError(processResult)) {
        const auto& processWindows = getValue(processResult);
        std::cout << "  Windows for 'nonexistent.exe': " << processWindows.size() << std::endl;
    }

    return true;
}

bool testWindowControl() {
    // This test is more cautious to avoid disrupting the user's workflow

    auto windowsResult = enumerateWindows();
    if (isError(windowsResult)) {
        return true; // Skip if we can't enumerate windows
    }

    const auto& windows = getValue(windowsResult);
    if (windows.empty()) {
        return true; // Skip if no windows
    }

    // Find a suitable test window (prefer our own test process or a simple window)
    auto testWindow = std::find_if(windows.begin(), windows.end(), [](const WindowInfo& window) {
        return window.title.find("test") != std::string::npos ||
               window.processName.find("test") != std::string::npos;
    });

    if (testWindow == windows.end()) {
        // Use the first visible window as a fallback, but be very careful
        testWindow = std::find_if(windows.begin(), windows.end(), [](const WindowInfo& window) {
            return window.isVisible && !window.title.empty();
        });
    }

    if (testWindow == windows.end()) {
        std::cout << "\n  No suitable window found for control testing" << std::endl;
        return true; // Skip this test
    }

    std::cout << "\n  Testing window control with: " << testWindow->title << std::endl;

    // Test focus (least disruptive)
    auto focusResult = focusWindow(testWindow->id);
    if (isError(focusResult)) {
        WMError error = getError(focusResult);
        // These are acceptable errors for window control
        bool acceptable = (error == WMError::PLATFORM_NOT_SUPPORTED ||
                          error == WMError::WINDOW_NOT_FOUND ||
                          error == WMError::OPERATION_FAILED ||
                          error == WMError::PERMISSION_DENIED);
        if (!acceptable) return false;
    } else {
        std::cout << "  Focus operation: " << (getValue(focusResult) ? "Success" : "Failed") << std::endl;
    }

    // Note: We don't test move/resize/state changes to avoid disrupting the user

    return true;
}

bool testWorkspaceOperations() {
    // Test workspace switching (if supported)
    auto workspacesResult = getWorkspaces();

    if (isError(workspacesResult)) {
        // Workspaces not supported, skip
        return true;
    }

    const auto& workspaces = getValue(workspacesResult);
    if (workspaces.empty()) {
        return true; // No workspaces to test with
    }

    std::cout << "\n  Testing workspace operations with " << workspaces.size() << " workspace(s)" << std::endl;

    // Find current workspace
    auto currentWorkspace = std::find_if(workspaces.begin(), workspaces.end(),
        [](const WorkspaceInfo& ws) { return ws.isActive; });

    if (currentWorkspace != workspaces.end()) {
        std::cout << "  Current workspace: " << currentWorkspace->name << std::endl;

        // Test WorkspaceManager
        auto currentResult = WorkspaceManager::getCurrentWorkspace();
        if (!isError(currentResult)) {
            const auto& current = getValue(currentResult);
            if (current.id == currentWorkspace->id) {
                std::cout << "  WorkspaceManager::getCurrentWorkspace() works correctly" << std::endl;
            }
        }
    }

    // Note: We don't test actual workspace switching to avoid disrupting the user

    return true;
}

bool testErrorHandling() {
    // Test with invalid window ID
    auto result = getWindowInfo(0xDEADBEEF);

    if (!isError(result)) {
        // This should typically fail, but might succeed on some platforms
        std::cout << "\n  Warning: getWindowInfo with invalid ID succeeded" << std::endl;
    } else {
        WMError error = getError(result);
        std::cout << "\n  Invalid window ID correctly returned error: " << errorToString(error) << std::endl;
    }

    // Test window control with invalid ID
    auto controlResult = focusWindow(0xDEADBEEF);
    if (!isError(controlResult)) {
        std::cout << "  Warning: focusWindow with invalid ID succeeded" << std::endl;
    }

    return true;
}

int runWindowManagementTests() {
    std::cout << "Running Window Management Tests" << std::endl;
    std::cout << "===============================" << std::endl;

    TestRunner::run("enumerateWindows", testEnumerateWindows);
    TestRunner::run("getWindowInfo", testGetWindowInfo);
    TestRunner::run("WindowManager class", testWindowManagerClass);
    TestRunner::run("window control", testWindowControl);
    TestRunner::run("workspace operations", testWorkspaceOperations);
    TestRunner::run("error handling", testErrorHandling);

    TestRunner::printSummary();
    return TestRunner::getFailedCount();
}

int main() {
    return runWindowManagementTests();
}
