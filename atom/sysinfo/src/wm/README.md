# Window Manager Module

A comprehensive, cross-platform window manager and system information library for C++.

## Overview

The Window Manager module provides a unified API for interacting with window managers and desktop environments across Windows, Linux, and macOS. It offers both basic system information retrieval and advanced window management capabilities.

## Features

### System Information

- Desktop environment detection
- Window manager identification
- Theme information (light/dark mode)
- Monitor/display enumeration
- Workspace/virtual desktop support
- Font and cursor information

### Window Management

- Window enumeration and filtering
- Window state manipulation (minimize, maximize, etc.)
- Window positioning and resizing
- Window focus and activation
- Cross-platform window operations

### Advanced Features

- Real-time window monitoring
- Theme change detection
- Workspace management
- Multi-monitor support
- Platform-specific optimizations

## Supported Platforms

| Platform | System Info | Window Enum | Window Control | Workspaces | Theme Detection |
|----------|-------------|-------------|----------------|------------|-----------------|
| Windows  | ✅          | ✅          | ✅             | ⚠️         | ✅              |
| Linux    | ✅          | ✅          | ✅             | ✅         | ✅              |
| macOS    | ✅          | ⚠️          | ⚠️             | ⚠️         | ✅              |

✅ Fully supported
⚠️ Basic support / Work in progress

## Quick Start

### Basic Usage

```cpp
#include "atom/sysinfo/wm/wm.hpp"

using namespace atom::system::wm;

// Get system information
auto sysInfoResult = getSystemInfo();
if (!isError(sysInfoResult)) {
    const auto& info = getValue(sysInfoResult);
    std::cout << "Desktop Environment: " << info.desktopEnvironment << std::endl;
    std::cout << "Window Manager: " << info.windowManager << std::endl;
}

// Enumerate windows
auto windowsResult = enumerateWindows();
if (!isError(windowsResult)) {
    const auto& windows = getValue(windowsResult);
    for (const auto& window : windows) {
        std::cout << "Window: " << window.title << " (" << window.processName << ")" << std::endl;
    }
}
```

### Window Management

```cpp
// Find a specific window
auto windowsResult = enumerateWindows();
if (!isError(windowsResult)) {
    const auto& windows = getValue(windowsResult);
    for (const auto& window : windows) {
        if (window.title.find("Notepad") != std::string::npos) {
            // Focus the window
            focusWindow(window.id);

            // Move and resize
            moveWindow(window.id, 100, 100);
            resizeWindow(window.id, 800, 600);
            break;
        }
    }
}
```

### Advanced Window Manager

```cpp
WindowManager wm;

// Start monitoring window changes
wm.startMonitoring([](const WindowInfo& window, bool added) {
    if (added) {
        std::cout << "Window opened: " << window.title << std::endl;
    } else {
        std::cout << "Window closed: " << window.title << std::endl;
    }
});

// Filter windows by process
auto chromeWindows = wm.getWindowsByProcess("chrome.exe");
if (!isError(chromeWindows)) {
    std::cout << "Found " << getValue(chromeWindows).size() << " Chrome windows" << std::endl;
}
```

### Theme Management

```cpp
ThemeManager tm;

// Get current theme
auto themeResult = tm.getCurrentTheme();
if (!isError(themeResult)) {
    const auto& theme = getValue(themeResult);
    std::cout << "Current theme: " << theme.name << " (" << themeTypeToString(theme.type) << ")" << std::endl;
}

// Monitor theme changes
tm.startMonitoring([](const ThemeInfo& theme) {
    std::cout << "Theme changed to: " << theme.name << std::endl;
});
```

## API Reference

### Core Functions

- `getSystemInfo()` - Get comprehensive system information
- `getThemeInfo()` - Get current theme information
- `enumerateWindows()` - List all visible windows
- `getWindowInfo(windowId)` - Get detailed window information
- `getMonitors()` - Get monitor/display information
- `getWorkspaces()` - Get workspace/virtual desktop information

### Window Control

- `setWindowState(windowId, state)` - Change window state
- `moveWindow(windowId, x, y)` - Move window to position
- `resizeWindow(windowId, width, height)` - Resize window
- `focusWindow(windowId)` - Focus/activate window
- `closeWindow(windowId)` - Close window

### Workspace Management

- `switchToWorkspace(workspaceId)` - Switch to workspace
- `moveWindowToWorkspace(windowId, workspaceId)` - Move window to workspace

### Classes

- `WindowManager` - Advanced window management and monitoring
- `ThemeManager` - Theme detection and monitoring
- `WorkspaceManager` - Workspace operations

## Error Handling

All functions return `WMResult<T>` which is a variant containing either the result or an error:

```cpp
auto result = getSystemInfo();
if (isError(result)) {
    WMError error = getError(result);
    std::cout << "Error: " << errorToString(error) << std::endl;
} else {
    const auto& info = getValue(result);
    // Use the result
}
```

## Platform-Specific Notes

### Windows

- Uses Win32 API and Desktop Window Manager (DWM)
- Virtual desktop support requires Windows 10+
- Some operations require elevated privileges

### Linux

- Requires X11 or Wayland
- Uses wmctrl for window management
- Supports GNOME, KDE, and other desktop environments
- Some features require specific tools (wmctrl, xrandr)

### macOS

- Uses Core Graphics and Accessibility APIs
- Requires accessibility permissions for window control
- Spaces (workspaces) support through private APIs

## Dependencies

- spdlog (logging)
- Platform-specific libraries:
  - Windows: dwmapi.lib, shell32.lib, psapi.lib
  - Linux: X11 libraries (optional)
  - macOS: Core Graphics, Accessibility frameworks

## Building

The module integrates with the existing Atom build system. Platform-specific implementations are automatically selected based on the target platform.

## Examples

See the `examples/` directory for complete usage examples:

- `basic_system_info.cpp` - Basic system information retrieval
- `window_management.cpp` - Window enumeration and control
- `theme_monitoring.cpp` - Theme change detection
- `workspace_demo.cpp` - Workspace management

## Migration from Legacy API

The module maintains backward compatibility with the original `atom::system::getSystemInfo()` function. New code should use the enhanced `atom::system::wm` namespace for full functionality.

```cpp
// Legacy (still supported)
auto info = atom::system::getSystemInfo();

// New enhanced API
auto result = atom::system::wm::getSystemInfo();
```

## Contributing

When adding new features:

1. Implement in platform-specific files (windows.cpp, linux.cpp, macos.cpp)
2. Add cross-platform wrapper in wm.cpp
3. Update the header files with appropriate declarations
4. Add tests and examples
5. Update documentation

## License

Part of the Atom project. See the main project license for details.
