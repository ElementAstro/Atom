/**
 * @file window_management.cpp
 * @brief Window enumeration and management example
 *
 * This example demonstrates how to enumerate windows, filter them,
 * and perform basic window management operations.
 */

#include "atom/sysinfo/wm/wm.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <algorithm>

using namespace atom::system::wm;

void printWindowInfo(const WindowInfo& window) {
    std::cout << std::left;
    std::cout << "  ID: " << std::hex << window.id << std::dec << std::endl;
    std::cout << "  Title: " << window.title << std::endl;
    std::cout << "  Process: " << window.processName << " (PID: " << window.processId << ")" << std::endl;
    std::cout << "  Class: " << window.className << std::endl;
    std::cout << "  State: " << windowStateToString(window.state) << std::endl;
    std::cout << "  Type: " << windowTypeToString(window.type) << std::endl;
    std::cout << "  Position: (" << window.x << ", " << window.y << ")" << std::endl;
    std::cout << "  Size: " << window.width << "x" << window.height << std::endl;
    std::cout << "  Visible: " << (window.isVisible ? "Yes" : "No") << std::endl;
    std::cout << "  Active: " << (window.isActive ? "Yes" : "No") << std::endl;
    if (window.workspaceId.has_value()) {
        std::cout << "  Workspace: " << window.workspaceId.value() << std::endl;
    }
    std::cout << std::endl;
}

void enumerateAllWindows() {
    std::cout << "=== All Windows ===" << std::endl;

    auto result = enumerateWindows();
    if (isError(result)) {
        std::cout << "Error enumerating windows: " << errorToString(getError(result)) << std::endl;
        return;
    }

    const auto& windows = getValue(result);
    std::cout << "Found " << windows.size() << " windows:" << std::endl << std::endl;

    for (const auto& window : windows) {
        printWindowInfo(window);
    }
}

void findWindowsByProcess(const std::string& processName) {
    std::cout << "=== Windows for Process: " << processName << " ===" << std::endl;

    WindowManager wm;
    auto result = wm.getWindowsByProcess(processName);

    if (isError(result)) {
        std::cout << "Error finding windows: " << errorToString(getError(result)) << std::endl;
        return;
    }

    const auto& windows = getValue(result);
    std::cout << "Found " << windows.size() << " windows for " << processName << ":" << std::endl << std::endl;

    for (const auto& window : windows) {
        printWindowInfo(window);
    }
}

void findWindowsByTitle(const std::string& titleSubstring) {
    std::cout << "=== Windows with Title containing: " << titleSubstring << " ===" << std::endl;

    WindowManager wm;
    auto result = wm.getFilteredWindows([&titleSubstring](const WindowInfo& window) {
        return window.title.find(titleSubstring) != std::string::npos;
    });

    if (isError(result)) {
        std::cout << "Error filtering windows: " << errorToString(getError(result)) << std::endl;
        return;
    }

    const auto& windows = getValue(result);
    std::cout << "Found " << windows.size() << " windows with title containing '" << titleSubstring << "':" << std::endl << std::endl;

    for (const auto& window : windows) {
        printWindowInfo(window);
    }
}

void demonstrateWindowControl() {
    std::cout << "=== Window Control Demo ===" << std::endl;

    auto result = enumerateWindows();
    if (isError(result)) {
        std::cout << "Error enumerating windows: " << errorToString(getError(result)) << std::endl;
        return;
    }

    const auto& windows = getValue(result);
    if (windows.empty()) {
        std::cout << "No windows found for demonstration." << std::endl;
        return;
    }

    // Find a suitable window for demonstration (not minimized, has a title)
    auto it = std::find_if(windows.begin(), windows.end(), [](const WindowInfo& window) {
        return !window.title.empty() &&
               window.state != WindowState::MINIMIZED &&
               window.isVisible;
    });

    if (it == windows.end()) {
        std::cout << "No suitable window found for demonstration." << std::endl;
        return;
    }

    const auto& targetWindow = *it;
    std::cout << "Using window: " << targetWindow.title << std::endl;

    // Demonstrate window operations
    std::cout << "1. Focusing window..." << std::endl;
    auto focusResult = focusWindow(targetWindow.id);
    if (isError(focusResult)) {
        std::cout << "   Failed: " << errorToString(getError(focusResult)) << std::endl;
    } else {
        std::cout << "   Success!" << std::endl;
    }

    std::cout << "2. Moving window to (200, 200)..." << std::endl;
    auto moveResult = moveWindow(targetWindow.id, 200, 200);
    if (isError(moveResult)) {
        std::cout << "   Failed: " << errorToString(getError(moveResult)) << std::endl;
    } else {
        std::cout << "   Success!" << std::endl;
    }

    std::cout << "3. Resizing window to 800x600..." << std::endl;
    auto resizeResult = resizeWindow(targetWindow.id, 800, 600);
    if (isError(resizeResult)) {
        std::cout << "   Failed: " << errorToString(getError(resizeResult)) << std::endl;
    } else {
        std::cout << "   Success!" << std::endl;
    }

    // Note: We don't demonstrate minimize/maximize to avoid disrupting the user's workflow
    std::cout << "Window control demonstration completed." << std::endl;
}

void showWindowStatistics() {
    std::cout << "=== Window Statistics ===" << std::endl;

    auto result = enumerateWindows();
    if (isError(result)) {
        std::cout << "Error enumerating windows: " << errorToString(getError(result)) << std::endl;
        return;
    }

    const auto& windows = getValue(result);

    // Count by state
    int normal = 0, minimized = 0, maximized = 0, fullscreen = 0, hidden = 0;
    for (const auto& window : windows) {
        switch (window.state) {
            case WindowState::NORMAL: normal++; break;
            case WindowState::MINIMIZED: minimized++; break;
            case WindowState::MAXIMIZED: maximized++; break;
            case WindowState::FULLSCREEN: fullscreen++; break;
            case WindowState::HIDDEN: hidden++; break;
            default: break;
        }
    }

    std::cout << "Total windows: " << windows.size() << std::endl;
    std::cout << "  Normal: " << normal << std::endl;
    std::cout << "  Minimized: " << minimized << std::endl;
    std::cout << "  Maximized: " << maximized << std::endl;
    std::cout << "  Fullscreen: " << fullscreen << std::endl;
    std::cout << "  Hidden: " << hidden << std::endl;

    // Count visible vs invisible
    int visible = std::count_if(windows.begin(), windows.end(),
        [](const WindowInfo& w) { return w.isVisible; });
    std::cout << "  Visible: " << visible << std::endl;
    std::cout << "  Invisible: " << (windows.size() - visible) << std::endl;

    // Find active window
    auto activeIt = std::find_if(windows.begin(), windows.end(),
        [](const WindowInfo& w) { return w.isActive; });
    if (activeIt != windows.end()) {
        std::cout << "Active window: " << activeIt->title << std::endl;
    }
}

int main() {
    std::cout << "Window Management Example" << std::endl;
    std::cout << "=========================" << std::endl;

    try {
        // Show statistics first
        showWindowStatistics();
        std::cout << std::endl;

        // Demonstrate filtering
        findWindowsByTitle("Chrome");
        findWindowsByTitle("Firefox");
        findWindowsByProcess("notepad.exe");

        // Demonstrate window control (commented out to avoid disruption)
        // demonstrateWindowControl();

        // Show all windows (commented out as it can be very long)
        // enumerateAllWindows();

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
