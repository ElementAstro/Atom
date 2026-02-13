#include "linux.hpp"

#ifdef __linux__

#include <array>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <sstream>
#include <fstream>

#include <spdlog/spdlog.h>

namespace atom::system::wm::linux {

namespace {

using PipeCloser = int (*)(FILE*);

}  // anonymous namespace

auto executeCommand(std::string_view command) -> std::string {
    std::array<char, 256> buffer{};
    std::string result;

    std::unique_ptr<FILE, PipeCloser> pipe(popen(command.data(), "r"), pclose);

    if (!pipe) {
        spdlog::error("Failed to execute command: {}", command);
        return "Unknown";
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // Remove trailing newline
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    // Remove quotes if present
    if (result.length() >= 2 && result.front() == '\'' &&
        result.back() == '\'') {
        result = result.substr(1, result.length() - 2);
    }

    return result.empty() ? "Unknown" : result;
}

auto getDesktopEnvironment() -> std::string {
    if (const char* de = std::getenv("XDG_CURRENT_DESKTOP")) {
        return de;
    }
    if (const char* de = std::getenv("DESKTOP_SESSION")) {
        return de;
    }
    if (const char* de = std::getenv("GDMSESSION")) {
        return de;
    }
    return "Unknown";
}

auto isWayland() -> bool {
    const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");
    const char* xdgSessionType = std::getenv("XDG_SESSION_TYPE");

    return (waylandDisplay && strlen(waylandDisplay) > 0) ||
           (xdgSessionType && strcmp(xdgSessionType, "wayland") == 0);
}

auto isX11() -> bool {
    const char* display = std::getenv("DISPLAY");
    const char* xdgSessionType = std::getenv("XDG_SESSION_TYPE");

    return (display && strlen(display) > 0) ||
           (xdgSessionType && strcmp(xdgSessionType, "x11") == 0);
}

auto getSystemInfo() -> WMResult<SystemInfo> {
    spdlog::debug("Detecting Linux desktop environment");

    SystemInfo info;
    info.desktopEnvironment = getDesktopEnvironment();

    // Try multiple methods to get window manager
    std::string wm =
        executeCommand("wmctrl -m 2>/dev/null | grep 'Name:' | cut -d' ' -f2");
    if (wm == "Unknown") {
        wm = executeCommand("echo $WINDOW_MANAGER");
    }
    if (wm == "Unknown") {
        wm = executeCommand(
            "pgrep -o 'i3|bspwm|openbox|xfwm4|kwin|mutter|awesome|dwm' | head "
            "-1");
    }
    info.windowManager = wm;

    // Detect display server
    if (isWayland()) {
        info.wmFeatures.push_back("Wayland");
    } else if (isX11()) {
        info.wmFeatures.push_back("X11");
    }

    // Get theme based on desktop environment
    if (info.desktopEnvironment.find("GNOME") != std::string::npos) {
        info.wmTheme = executeCommand(
            "gsettings get org.gnome.desktop.interface gtk-theme 2>/dev/null");
        info.font = executeCommand(
            "gsettings get org.gnome.desktop.interface font-name 2>/dev/null");
        info.cursor = executeCommand(
            "gsettings get org.gnome.desktop.interface cursor-theme "
            "2>/dev/null");
        info.icons = executeCommand(
            "gsettings get org.gnome.desktop.interface icon-theme 2>/dev/null");
        info.wmFeatures.push_back("GNOME Extensions");
        info.wmFeatures.push_back("Activities Overview");
    } else if (info.desktopEnvironment.find("KDE") != std::string::npos) {
        info.wmTheme = executeCommand(
            "kreadconfig5 --group General --key ColorScheme 2>/dev/null");
        info.font = executeCommand(
            "kreadconfig5 --group General --key font 2>/dev/null");
        info.cursor = executeCommand(
            "kreadconfig5 --group Icons --key Theme 2>/dev/null");
        info.icons = executeCommand(
            "kreadconfig5 --group Icons --key Theme 2>/dev/null");
        info.wmFeatures.push_back("KDE Activities");
        info.wmFeatures.push_back("Virtual Desktops");
    } else {
        info.wmTheme = "Unknown Theme";
        info.font = "Unknown Font";
        info.cursor = "Unknown Cursor";
        info.icons = "Unknown Icons";
    }

    // Check for workspace support
    std::string wmctrlOutput = executeCommand("wmctrl -d 2>/dev/null");
    info.supportsWorkspaces = (wmctrlOutput != "Unknown" && !wmctrlOutput.empty());
    info.supportsVirtualDesktops = info.supportsWorkspaces;

    // Get theme info
    auto themeResult = getThemeInfo();
    if (!isError(themeResult)) {
        info.themeInfo = getValue(themeResult);
    }

    // Get monitors
    auto monitorsResult = getMonitors();
    if (!isError(monitorsResult)) {
        info.monitors = getValue(monitorsResult);
    }

    // Get workspaces
    auto workspacesResult = getWorkspaces();
    if (!isError(workspacesResult)) {
        info.workspaces = getValue(workspacesResult);
    }

    spdlog::debug("Linux system info - DE: {}, WM: {}", info.desktopEnvironment,
                  info.windowManager);

    return info;
}

auto getThemeInfo() -> WMResult<ThemeInfo> {
    ThemeInfo theme;
    std::string de = getDesktopEnvironment();

    if (de.find("GNOME") != std::string::npos) {
        theme.name = executeCommand(
            "gsettings get org.gnome.desktop.interface gtk-theme 2>/dev/null");

        // Check for dark theme preference
        std::string colorScheme = executeCommand(
            "gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null");

        if (colorScheme.find("dark") != std::string::npos) {
            theme.type = ThemeType::DARK;
            theme.variant = "dark";
        } else if (colorScheme.find("light") != std::string::npos) {
            theme.type = ThemeType::LIGHT;
            theme.variant = "light";
        } else {
            theme.type = ThemeType::AUTO;
            theme.variant = "auto";
        }

        theme.followsSystemTheme = true;
    } else if (de.find("KDE") != std::string::npos) {
        theme.name = executeCommand(
            "kreadconfig5 --group General --key ColorScheme 2>/dev/null");

        if (theme.name.find("Dark") != std::string::npos ||
            theme.name.find("dark") != std::string::npos) {
            theme.type = ThemeType::DARK;
            theme.variant = "dark";
        } else {
            theme.type = ThemeType::LIGHT;
            theme.variant = "light";
        }

        theme.followsSystemTheme = true;
    } else {
        theme.type = ThemeType::UNKNOWN;
        theme.name = "Unknown";
        theme.variant = "unknown";
        theme.followsSystemTheme = false;
    }

    // Set default colors based on theme type
    if (theme.type == ThemeType::DARK) {
        theme.backgroundColor = "#2E2E2E";
        theme.foregroundColor = "#FFFFFF";
        theme.accentColor = "#3584E4";
    } else if (theme.type == ThemeType::LIGHT) {
        theme.backgroundColor = "#FFFFFF";
        theme.foregroundColor = "#000000";
        theme.accentColor = "#3584E4";
    }

    return theme;
}

auto enumerateWindows() -> WMResult<std::vector<WindowInfo>> {
    std::vector<WindowInfo> windows;

    // Use wmctrl to get window list
    std::string output = executeCommand("wmctrl -l -p -G 2>/dev/null");
    if (output == "Unknown" || output.empty()) {
        return WMError::OPERATION_FAILED;
    }

    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        std::istringstream lineStream(line);
        std::string windowIdStr, desktop, pid, x, y, width, height, hostname;

        if (!(lineStream >> windowIdStr >> desktop >> pid >> x >> y >> width >> height >> hostname)) {
            continue;
        }

        // Get the rest as window title
        std::string title;
        std::getline(lineStream, title);
        if (!title.empty() && title[0] == ' ') {
            title = title.substr(1); // Remove leading space
        }

        WindowInfo info;
        try {
            info.id = std::stoull(windowIdStr, nullptr, 16); // Window ID is in hex
            info.processId = std::stoul(pid);
            info.x = std::stoi(x);
            info.y = std::stoi(y);
            info.width = std::stoi(width);
            info.height = std::stoi(height);
            info.title = title;
            info.isVisible = true; // wmctrl only shows visible windows

            // Try to get process name
            std::string procPath = "/proc/" + pid + "/comm";
            std::ifstream procFile(procPath);
            if (procFile.is_open()) {
                std::getline(procFile, info.processName);
            } else {
                info.processName = "Unknown";
            }

            if (desktop != "-1") {
                info.workspaceId = std::stoul(desktop);
            }

            windows.push_back(info);
        } catch (const std::exception& e) {
            spdlog::warn("Failed to parse window info: {}", e.what());
            continue;
        }
    }

    return windows;
}

auto getWindowInfo(uint64_t windowId) -> WMResult<WindowInfo> {
    // Get all windows and find the one with matching ID
    auto windowsResult = enumerateWindows();
    if (isError(windowsResult)) {
        return getError(windowsResult);
    }

    const auto& windows = getValue(windowsResult);
    for (const auto& window : windows) {
        if (window.id == windowId) {
            return window;
        }
    }

    return WMError::WINDOW_NOT_FOUND;
}

auto getMonitors() -> WMResult<std::vector<MonitorInfo>> {
    std::vector<MonitorInfo> monitors;

    // Try xrandr first
    std::string output = executeCommand("xrandr --query 2>/dev/null | grep ' connected'");
    if (output != "Unknown" && !output.empty()) {
        std::istringstream stream(output);
        std::string line;
        uint32_t id = 0;

        while (std::getline(stream, line)) {
            MonitorInfo monitor;
            monitor.id = id++;

            // Parse xrandr output (format: "HDMI-1 connected 1920x1080+0+0 ...")
            std::istringstream lineStream(line);
            std::string name, status, geometry;

            if (lineStream >> name >> status >> geometry) {
                monitor.name = name;
                monitor.isPrimary = (line.find("primary") != std::string::npos);

                // Parse geometry (1920x1080+0+0)
                size_t xPos = geometry.find('x');
                size_t plusPos1 = geometry.find('+', xPos);
                size_t plusPos2 = geometry.find('+', plusPos1 + 1);

                if (xPos != std::string::npos && plusPos1 != std::string::npos) {
                    try {
                        monitor.width = std::stoi(geometry.substr(0, xPos));
                        monitor.height = std::stoi(geometry.substr(xPos + 1, plusPos1 - xPos - 1));
                        if (plusPos2 != std::string::npos) {
                            monitor.x = std::stoi(geometry.substr(plusPos1 + 1, plusPos2 - plusPos1 - 1));
                            monitor.y = std::stoi(geometry.substr(plusPos2 + 1));
                        }
                    } catch (const std::exception&) {
                        // Use defaults if parsing fails
                        monitor.width = 1920;
                        monitor.height = 1080;
                    }
                }

                monitor.refreshRate = 60; // Default, could be parsed from xrandr
                monitor.scaleFactor = 1.0f; // Could be detected

                monitors.push_back(monitor);
            }
        }
    }

    // Fallback: create a default monitor
    if (monitors.empty()) {
        MonitorInfo monitor;
        monitor.id = 0;
        monitor.name = "Default Monitor";
        monitor.isPrimary = true;
        monitor.width = 1920;
        monitor.height = 1080;
        monitor.refreshRate = 60;
        monitor.scaleFactor = 1.0f;
        monitors.push_back(monitor);
    }

    return monitors;
}

auto getWorkspaces() -> WMResult<std::vector<WorkspaceInfo>> {
    std::vector<WorkspaceInfo> workspaces;

    // Use wmctrl to get desktop list
    std::string output = executeCommand("wmctrl -d 2>/dev/null");
    if (output == "Unknown" || output.empty()) {
        return WMError::OPERATION_FAILED;
    }

    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        std::istringstream lineStream(line);
        std::string idStr, status, geometry, viewport, workarea;

        if (!(lineStream >> idStr >> status)) {
            continue;
        }

        WorkspaceInfo workspace;
        try {
            workspace.id = std::stoul(idStr);
            workspace.isActive = (status == "*");

            // Get the rest as workspace name
            std::string name;
            std::getline(lineStream, name);
            if (!name.empty()) {
                // Skip geometry, viewport, workarea fields and get the name
                size_t nameStart = name.find_last_of(' ');
                if (nameStart != std::string::npos) {
                    workspace.name = name.substr(nameStart + 1);
                } else {
                    workspace.name = "Desktop " + std::to_string(workspace.id + 1);
                }
            } else {
                workspace.name = "Desktop " + std::to_string(workspace.id + 1);
            }

            // Set default dimensions (could be parsed from geometry)
            workspace.width = 1920;
            workspace.height = 1080;

            workspaces.push_back(workspace);
        } catch (const std::exception& e) {
            spdlog::warn("Failed to parse workspace info: {}", e.what());
            continue;
        }
    }

    return workspaces;
}

auto setWindowState(uint64_t windowId, WindowState state) -> WMResult<bool> {
    std::string windowIdHex = std::to_string(windowId);
    std::string command;

    switch (state) {
        case WindowState::MINIMIZED:
            command = "wmctrl -i -r " + windowIdHex + " -b add,hidden";
            break;
        case WindowState::MAXIMIZED:
            command = "wmctrl -i -r " + windowIdHex + " -b add,maximized_vert,maximized_horz";
            break;
        case WindowState::NORMAL:
            command = "wmctrl -i -r " + windowIdHex + " -b remove,maximized_vert,maximized_horz,hidden";
            break;
        default:
            return WMError::INVALID_PARAMETER;
    }

    std::string result = executeCommand(command);
    return result != "Unknown";
}

auto moveWindow(uint64_t windowId, int x, int y) -> WMResult<bool> {
    std::string command = "wmctrl -i -r " + std::to_string(windowId) +
                         " -e 0," + std::to_string(x) + "," + std::to_string(y) + ",-1,-1";

    std::string result = executeCommand(command);
    return result != "Unknown";
}

auto resizeWindow(uint64_t windowId, int width, int height) -> WMResult<bool> {
    std::string command = "wmctrl -i -r " + std::to_string(windowId) +
                         " -e 0,-1,-1," + std::to_string(width) + "," + std::to_string(height);

    std::string result = executeCommand(command);
    return result != "Unknown";
}

auto focusWindow(uint64_t windowId) -> WMResult<bool> {
    std::string command = "wmctrl -i -a " + std::to_string(windowId);

    std::string result = executeCommand(command);
    return result != "Unknown";
}

auto closeWindow(uint64_t windowId) -> WMResult<bool> {
    std::string command = "wmctrl -i -c " + std::to_string(windowId);

    std::string result = executeCommand(command);
    return result != "Unknown";
}

auto switchToWorkspace(uint32_t workspaceId) -> WMResult<bool> {
    std::string command = "wmctrl -s " + std::to_string(workspaceId);

    std::string result = executeCommand(command);
    return result != "Unknown";
}

auto moveWindowToWorkspace(uint64_t windowId, uint32_t workspaceId) -> WMResult<bool> {
    std::string command = "wmctrl -i -r " + std::to_string(windowId) +
                         " -t " + std::to_string(workspaceId);

    std::string result = executeCommand(command);
    return result != "Unknown";
}

}  // namespace atom::system::wm::linux

#endif  // __linux__
