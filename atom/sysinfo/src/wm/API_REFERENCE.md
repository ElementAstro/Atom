# Window Manager API Reference

## Namespace: `atom::system::wm`

### Core Types

#### Enumerations

##### `WMError`
Error codes for window manager operations.
- `NONE` - No error
- `PLATFORM_NOT_SUPPORTED` - Platform not supported
- `PERMISSION_DENIED` - Permission denied
- `WINDOW_NOT_FOUND` - Window not found
- `WORKSPACE_NOT_FOUND` - Workspace not found
- `OPERATION_FAILED` - Operation failed
- `INVALID_PARAMETER` - Invalid parameter
- `TIMEOUT` - Operation timed out
- `UNKNOWN_ERROR` - Unknown error

##### `WindowState`
Window state enumeration.
- `NORMAL` - Normal window state
- `MINIMIZED` - Window is minimized
- `MAXIMIZED` - Window is maximized
- `FULLSCREEN` - Window is in fullscreen mode
- `HIDDEN` - Window is hidden
- `UNKNOWN` - Unknown state

##### `WindowType`
Window type enumeration.
- `NORMAL` - Normal application window
- `DIALOG` - Dialog window
- `UTILITY` - Utility window
- `TOOLBAR` - Toolbar window
- `SPLASH` - Splash screen
- `MENU` - Menu window
- `DROPDOWN_MENU` - Dropdown menu
- `POPUP_MENU` - Popup menu
- `TOOLTIP` - Tooltip window
- `NOTIFICATION` - Notification window
- `COMBO` - Combo box window
- `DND` - Drag and drop window
- `DESKTOP` - Desktop window
- `DOCK` - Dock window
- `UNKNOWN` - Unknown type

##### `ThemeType`
Theme type enumeration.
- `LIGHT` - Light theme
- `DARK` - Dark theme
- `AUTO` - Automatic theme (follows system)
- `HIGH_CONTRAST` - High contrast theme
- `CUSTOM` - Custom theme
- `UNKNOWN` - Unknown theme

#### Structures

##### `WindowInfo`
Contains information about a window.
```cpp
struct WindowInfo {
    uint64_t id;                    // Window ID/handle
    std::string title;              // Window title
    std::string className;          // Window class name
    std::string processName;        // Process name owning the window
    uint32_t processId;             // Process ID
    WindowState state;              // Current window state
    WindowType type;                // Window type
    int x, y;                       // Window position
    int width, height;              // Window dimensions
    bool isVisible;                 // Whether window is visible
    bool isActive;                  // Whether window is active/focused
    std::optional<uint32_t> workspaceId; // Workspace/desktop ID
};
```

##### `WorkspaceInfo`
Contains information about a workspace/virtual desktop.
```cpp
struct WorkspaceInfo {
    uint32_t id;                    // Workspace ID
    std::string name;               // Workspace name
    bool isActive;                  // Whether this is the current workspace
    std::vector<uint64_t> windowIds; // Windows in this workspace
    int x, y;                       // Workspace position
    int width, height;              // Workspace dimensions
};
```

##### `MonitorInfo`
Contains information about a monitor/display.
```cpp
struct MonitorInfo {
    uint32_t id;                    // Monitor ID
    std::string name;               // Monitor name/model
    bool isPrimary;                 // Whether this is the primary monitor
    int x, y;                       // Monitor position
    int width, height;              // Monitor resolution
    int refreshRate;                // Refresh rate in Hz
    float scaleFactor;              // DPI scale factor
};
```

##### `ThemeInfo`
Contains theme information.
```cpp
struct ThemeInfo {
    ThemeType type;                 // Theme type
    std::string name;               // Theme name
    std::string variant;            // Theme variant
    std::string accentColor;        // Accent color (hex format)
    std::string backgroundColor;    // Background color
    std::string foregroundColor;    // Foreground/text color
    bool followsSystemTheme;        // Whether theme follows system setting
};
```

##### `SystemInfo`
Enhanced system information structure.
```cpp
struct SystemInfo {
    std::string desktopEnvironment;      // Desktop environment
    std::string windowManager;           // Window manager
    std::string wmTheme;                 // Window manager theme
    std::string icons;                   // Icon theme
    std::string font;                    // System font
    std::string cursor;                  // Cursor theme
    ThemeInfo themeInfo;                 // Detailed theme information
    std::vector<MonitorInfo> monitors;   // Connected monitors
    std::vector<WorkspaceInfo> workspaces; // Available workspaces
    bool supportsWorkspaces;             // Whether WM supports workspaces
    bool supportsVirtualDesktops;        // Whether system supports virtual desktops
    std::string wmVersion;               // Window manager version
    std::vector<std::string> wmFeatures; // Supported WM features
};
```

#### Template Types

##### `WMResult<T>`
Result type that contains either a value of type T or an error.
```cpp
template<typename T>
using WMResult = std::variant<T, WMError>;
```

#### Callback Types
```cpp
using WindowCallback = std::function<void(const WindowInfo&, bool /* added */)>;
using WorkspaceCallback = std::function<void(const WorkspaceInfo&)>;
using ThemeCallback = std::function<void(const ThemeInfo&)>;
```

### Core Functions

#### System Information
```cpp
[[nodiscard]] auto getSystemInfo() -> WMResult<SystemInfo>;
```
Get comprehensive system information including window manager details.

```cpp
[[nodiscard]] auto getThemeInfo() -> WMResult<ThemeInfo>;
```
Get current theme information.

#### Window Operations
```cpp
[[nodiscard]] auto enumerateWindows() -> WMResult<std::vector<WindowInfo>>;
```
Enumerate all visible windows in the system.

```cpp
[[nodiscard]] auto getWindowInfo(uint64_t windowId) -> WMResult<WindowInfo>;
```
Get detailed information about a specific window.

#### Monitor and Workspace Information
```cpp
[[nodiscard]] auto getMonitors() -> WMResult<std::vector<MonitorInfo>>;
```
Get information about all connected monitors/displays.

```cpp
[[nodiscard]] auto getWorkspaces() -> WMResult<std::vector<WorkspaceInfo>>;
```
Get information about all available workspaces/virtual desktops.

#### Window Control
```cpp
[[nodiscard]] auto setWindowState(uint64_t windowId, WindowState state) -> WMResult<bool>;
```
Set the state of a window (minimize, maximize, etc.).

```cpp
[[nodiscard]] auto moveWindow(uint64_t windowId, int x, int y) -> WMResult<bool>;
```
Move a window to a specific position.

```cpp
[[nodiscard]] auto resizeWindow(uint64_t windowId, int width, int height) -> WMResult<bool>;
```
Resize a window to specific dimensions.

```cpp
[[nodiscard]] auto focusWindow(uint64_t windowId) -> WMResult<bool>;
```
Focus/activate a window.

```cpp
[[nodiscard]] auto closeWindow(uint64_t windowId) -> WMResult<bool>;
```
Close a window.

#### Workspace Control
```cpp
[[nodiscard]] auto switchToWorkspace(uint32_t workspaceId) -> WMResult<bool>;
```
Switch to a specific workspace/virtual desktop.

```cpp
[[nodiscard]] auto moveWindowToWorkspace(uint64_t windowId, uint32_t workspaceId) -> WMResult<bool>;
```
Move a window to a specific workspace/virtual desktop.

### Utility Functions

#### Error Handling
```cpp
template<typename T>
[[nodiscard]] constexpr auto isError(const WMResult<T>& result) -> bool;
```
Check if a result contains an error.

```cpp
template<typename T>
[[nodiscard]] constexpr auto getError(const WMResult<T>& result) -> WMError;
```
Get error from result (if it contains one).

```cpp
template<typename T>
[[nodiscard]] constexpr auto getValue(const WMResult<T>& result) -> const T&;
```
Get value from result (if it contains one).

#### String Conversion
```cpp
[[nodiscard]] auto errorToString(WMError error) -> std::string;
[[nodiscard]] auto windowStateToString(WindowState state) -> std::string;
[[nodiscard]] auto windowTypeToString(WindowType type) -> std::string;
[[nodiscard]] auto themeTypeToString(ThemeType type) -> std::string;
```
Convert enums to human-readable strings.

### Classes

#### `WindowManager`
Advanced window management and monitoring class.

##### Constructor/Destructor
```cpp
WindowManager();
~WindowManager();
```

##### Methods
```cpp
auto startMonitoring(WindowCallback callback,
                    std::chrono::milliseconds interval = std::chrono::milliseconds(1000)) -> bool;
```
Start monitoring window changes.

```cpp
void stopMonitoring();
```
Stop monitoring window changes.

```cpp
[[nodiscard]] auto isMonitoring() const -> bool;
```
Check if currently monitoring.

```cpp
template<typename FilterFunc>
[[nodiscard]] auto getFilteredWindows(FilterFunc filter) -> WMResult<std::vector<WindowInfo>>;
```
Get all windows matching a filter function.

```cpp
[[nodiscard]] auto getWindowsByProcess(const std::string& processName) -> WMResult<std::vector<WindowInfo>>;
```
Get windows by process name.

```cpp
[[nodiscard]] auto getWindowsInWorkspace(uint32_t workspaceId) -> WMResult<std::vector<WindowInfo>>;
```
Get windows in a specific workspace.

#### `ThemeManager`
Theme detection and monitoring class.

##### Constructor/Destructor
```cpp
ThemeManager();
~ThemeManager();
```

##### Methods
```cpp
[[nodiscard]] auto getCurrentTheme() -> WMResult<ThemeInfo>;
```
Get current theme information.

```cpp
auto startMonitoring(ThemeCallback callback,
                    std::chrono::milliseconds interval = std::chrono::milliseconds(5000)) -> bool;
```
Start monitoring theme changes.

```cpp
void stopMonitoring();
```
Stop monitoring theme changes.

```cpp
[[nodiscard]] auto isMonitoring() const -> bool;
```
Check if currently monitoring.

#### `WorkspaceManager`
Workspace operations class (static methods only).

##### Methods
```cpp
[[nodiscard]] static auto getCurrentWorkspace() -> WMResult<WorkspaceInfo>;
```
Get current active workspace.

```cpp
[[nodiscard]] static auto createWorkspace(const std::string& name) -> WMResult<uint32_t>;
```
Create a new workspace (if supported).

```cpp
[[nodiscard]] static auto deleteWorkspace(uint32_t workspaceId) -> WMResult<bool>;
```
Delete a workspace (if supported).

```cpp
[[nodiscard]] static auto renameWorkspace(uint32_t workspaceId, const std::string& newName) -> WMResult<bool>;
```
Rename a workspace (if supported).

### Platform Support

| Function | Windows | Linux | macOS |
|----------|---------|-------|-------|
| getSystemInfo | ✅ | ✅ | ✅ |
| getThemeInfo | ✅ | ✅ | ✅ |
| enumerateWindows | ✅ | ✅ | ⚠️ |
| Window Control | ✅ | ✅ | ⚠️ |
| getMonitors | ✅ | ✅ | ✅ |
| getWorkspaces | ⚠️ | ✅ | ⚠️ |
| Workspace Control | ❌ | ✅ | ⚠️ |

✅ Fully supported
⚠️ Basic support / Work in progress
❌ Not supported
