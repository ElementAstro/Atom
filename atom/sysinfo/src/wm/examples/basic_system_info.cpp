/**
 * @file basic_system_info.cpp
 * @brief Basic system information retrieval example
 *
 * This example demonstrates how to retrieve basic system information
 * including desktop environment, window manager, and theme details.
 */

#include "atom/sysinfo/wm/wm.hpp"
#include <iostream>
#include <iomanip>

using namespace atom::system::wm;

void printSystemInfo() {
    std::cout << "=== System Information ===" << std::endl;

    auto result = getSystemInfo();
    if (isError(result)) {
        std::cout << "Error getting system info: " << errorToString(getError(result)) << std::endl;
        return;
    }

    const auto& info = getValue(result);

    std::cout << std::left;
    std::cout << std::setw(25) << "Desktop Environment:" << info.desktopEnvironment << std::endl;
    std::cout << std::setw(25) << "Window Manager:" << info.windowManager << std::endl;
    std::cout << std::setw(25) << "WM Theme:" << info.wmTheme << std::endl;
    std::cout << std::setw(25) << "Icons:" << info.icons << std::endl;
    std::cout << std::setw(25) << "Font:" << info.font << std::endl;
    std::cout << std::setw(25) << "Cursor:" << info.cursor << std::endl;
    std::cout << std::setw(25) << "WM Version:" << info.wmVersion << std::endl;
    std::cout << std::setw(25) << "Supports Workspaces:" << (info.supportsWorkspaces ? "Yes" : "No") << std::endl;
    std::cout << std::setw(25) << "Supports Virtual Desktops:" << (info.supportsVirtualDesktops ? "Yes" : "No") << std::endl;

    if (!info.wmFeatures.empty()) {
        std::cout << std::setw(25) << "WM Features:";
        for (size_t i = 0; i < info.wmFeatures.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << info.wmFeatures[i];
        }
        std::cout << std::endl;
    }
}

void printThemeInfo() {
    std::cout << "\n=== Theme Information ===" << std::endl;

    auto result = getThemeInfo();
    if (isError(result)) {
        std::cout << "Error getting theme info: " << errorToString(getError(result)) << std::endl;
        return;
    }

    const auto& theme = getValue(result);

    std::cout << std::left;
    std::cout << std::setw(20) << "Theme Type:" << themeTypeToString(theme.type) << std::endl;
    std::cout << std::setw(20) << "Theme Name:" << theme.name << std::endl;
    std::cout << std::setw(20) << "Variant:" << theme.variant << std::endl;
    std::cout << std::setw(20) << "Follows System:" << (theme.followsSystemTheme ? "Yes" : "No") << std::endl;

    if (!theme.backgroundColor.empty()) {
        std::cout << std::setw(20) << "Background Color:" << theme.backgroundColor << std::endl;
    }
    if (!theme.foregroundColor.empty()) {
        std::cout << std::setw(20) << "Foreground Color:" << theme.foregroundColor << std::endl;
    }
    if (!theme.accentColor.empty()) {
        std::cout << std::setw(20) << "Accent Color:" << theme.accentColor << std::endl;
    }
}

void printMonitorInfo() {
    std::cout << "\n=== Monitor Information ===" << std::endl;

    auto result = getMonitors();
    if (isError(result)) {
        std::cout << "Error getting monitor info: " << errorToString(getError(result)) << std::endl;
        return;
    }

    const auto& monitors = getValue(result);

    for (size_t i = 0; i < monitors.size(); ++i) {
        const auto& monitor = monitors[i];
        std::cout << "Monitor " << (i + 1) << ":" << std::endl;
        std::cout << "  Name: " << monitor.name << std::endl;
        std::cout << "  Primary: " << (monitor.isPrimary ? "Yes" : "No") << std::endl;
        std::cout << "  Resolution: " << monitor.width << "x" << monitor.height << std::endl;
        std::cout << "  Position: (" << monitor.x << ", " << monitor.y << ")" << std::endl;
        std::cout << "  Refresh Rate: " << monitor.refreshRate << " Hz" << std::endl;
        std::cout << "  Scale Factor: " << monitor.scaleFactor << "x" << std::endl;
        std::cout << std::endl;
    }
}

void printWorkspaceInfo() {
    std::cout << "=== Workspace Information ===" << std::endl;

    auto result = getWorkspaces();
    if (isError(result)) {
        std::cout << "Error getting workspace info: " << errorToString(getError(result)) << std::endl;
        return;
    }

    const auto& workspaces = getValue(result);

    if (workspaces.empty()) {
        std::cout << "No workspaces found or not supported on this platform." << std::endl;
        return;
    }

    for (const auto& workspace : workspaces) {
        std::cout << "Workspace " << workspace.id << ":" << std::endl;
        std::cout << "  Name: " << workspace.name << std::endl;
        std::cout << "  Active: " << (workspace.isActive ? "Yes" : "No") << std::endl;
        std::cout << "  Dimensions: " << workspace.width << "x" << workspace.height << std::endl;
        std::cout << "  Position: (" << workspace.x << ", " << workspace.y << ")" << std::endl;
        std::cout << "  Windows: " << workspace.windowIds.size() << std::endl;
        std::cout << std::endl;
    }
}

int main() {
    std::cout << "Window Manager System Information Example" << std::endl;
    std::cout << "=========================================" << std::endl;

    try {
        printSystemInfo();
        printThemeInfo();
        printMonitorInfo();
        printWorkspaceInfo();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
