#ifndef ATOM_SYSINFO_WM_COMMON_HPP
#define ATOM_SYSINFO_WM_COMMON_HPP

#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <chrono>
#include <functional>

#include "atom/macro.hpp"

namespace atom::system::wm {

/**
 * @brief Error types for window manager operations
 */
enum class WMError {
    NONE,
    PLATFORM_NOT_SUPPORTED,
    PERMISSION_DENIED,
    WINDOW_NOT_FOUND,
    WORKSPACE_NOT_FOUND,
    OPERATION_FAILED,
    INVALID_PARAMETER,
    TIMEOUT,
    UNKNOWN_ERROR
};

/**
 * @brief Window state enumeration
 */
enum class WindowState {
    NORMAL,
    MINIMIZED,
    MAXIMIZED,
    FULLSCREEN,
    HIDDEN,
    UNKNOWN
};

/**
 * @brief Window type enumeration
 */
enum class WindowType {
    NORMAL,
    DIALOG,
    UTILITY,
    TOOLBAR,
    SPLASH,
    MENU,
    DROPDOWN_MENU,
    POPUP_MENU,
    TOOLTIP,
    NOTIFICATION,
    COMBO,
    DND,
    DESKTOP,
    DOCK,
    UNKNOWN
};

/**
 * @brief Theme type enumeration
 */
enum class ThemeType {
    LIGHT,
    DARK,
    AUTO,
    HIGH_CONTRAST,
    CUSTOM,
    UNKNOWN
};

/**
 * @brief Window information structure
 */
struct WindowInfo {
    uint64_t id{0};                    //!< Window ID/handle
    std::string title;                 //!< Window title
    std::string className;             //!< Window class name
    std::string processName;           //!< Process name owning the window
    uint32_t processId{0};             //!< Process ID
    WindowState state{WindowState::UNKNOWN};  //!< Current window state
    WindowType type{WindowType::UNKNOWN};     //!< Window type
    int x{0}, y{0};                    //!< Window position
    int width{0}, height{0};           //!< Window dimensions
    bool isVisible{false};             //!< Whether window is visible
    bool isActive{false};              //!< Whether window is active/focused
    std::optional<uint32_t> workspaceId;  //!< Workspace/desktop ID (if applicable)
} ATOM_ALIGNAS(64);

/**
 * @brief Workspace/Desktop information
 */
struct WorkspaceInfo {
    uint32_t id{0};                    //!< Workspace ID
    std::string name;                  //!< Workspace name
    bool isActive{false};              //!< Whether this is the current workspace
    std::vector<uint64_t> windowIds;   //!< Windows in this workspace
    int x{0}, y{0};                    //!< Workspace position (for multi-monitor)
    int width{0}, height{0};           //!< Workspace dimensions
} ATOM_ALIGNAS(64);

/**
 * @brief Monitor/Display information
 */
struct MonitorInfo {
    uint32_t id{0};                    //!< Monitor ID
    std::string name;                  //!< Monitor name/model
    bool isPrimary{false};             //!< Whether this is the primary monitor
    int x{0}, y{0};                    //!< Monitor position
    int width{0}, height{0};           //!< Monitor resolution
    int refreshRate{0};                //!< Refresh rate in Hz
    float scaleFactor{1.0f};           //!< DPI scale factor
} ATOM_ALIGNAS(64);

/**
 * @brief Theme information structure
 */
struct ThemeInfo {
    ThemeType type{ThemeType::UNKNOWN};  //!< Theme type
    std::string name;                    //!< Theme name
    std::string variant;                 //!< Theme variant (e.g., "dark", "light")
    std::string accentColor;             //!< Accent color (hex format)
    std::string backgroundColor;         //!< Background color
    std::string foregroundColor;         //!< Foreground/text color
    bool followsSystemTheme{false};      //!< Whether theme follows system setting
} ATOM_ALIGNAS(64);

/**
 * @brief Enhanced system information structure
 */
struct SystemInfo {
    std::string desktopEnvironment;      //!< Desktop environment (e.g., Fluent, GNOME, KDE)
    std::string windowManager;           //!< Window manager (e.g., Desktop Window Manager, i3, bspwm)
    std::string wmTheme;                 //!< Window manager theme information
    std::string icons;                   //!< Icon theme or icon information
    std::string font;                    //!< System font information
    std::string cursor;                  //!< Cursor theme information
    ThemeInfo themeInfo;                 //!< Detailed theme information
    std::vector<MonitorInfo> monitors;   //!< Connected monitors
    std::vector<WorkspaceInfo> workspaces; //!< Available workspaces
    bool supportsWorkspaces{false};      //!< Whether WM supports multiple workspaces
    bool supportsVirtualDesktops{false}; //!< Whether system supports virtual desktops
    std::string wmVersion;               //!< Window manager version
    std::vector<std::string> wmFeatures; //!< Supported WM features
} ATOM_ALIGNAS(128);

/**
 * @brief Result type for operations that can fail
 */
template<typename T>
using WMResult = std::variant<T, WMError>;

/**
 * @brief Callback function types
 */
using WindowCallback = std::function<void(const WindowInfo&, bool /* added */)>;
using WorkspaceCallback = std::function<void(const WorkspaceInfo&)>;
using ThemeCallback = std::function<void(const ThemeInfo&)>;

/**
 * @brief Convert error enum to string
 */
[[nodiscard]] auto errorToString(WMError error) -> std::string;

/**
 * @brief Convert window state enum to string
 */
[[nodiscard]] auto windowStateToString(WindowState state) -> std::string;

/**
 * @brief Convert window type enum to string
 */
[[nodiscard]] auto windowTypeToString(WindowType type) -> std::string;

/**
 * @brief Convert theme type enum to string
 */
[[nodiscard]] auto themeTypeToString(ThemeType type) -> std::string;

/**
 * @brief Check if a result contains an error
 */
template<typename T>
[[nodiscard]] constexpr auto isError(const WMResult<T>& result) -> bool {
    return std::holds_alternative<WMError>(result);
}

/**
 * @brief Get error from result (if it contains one)
 */
template<typename T>
[[nodiscard]] constexpr auto getError(const WMResult<T>& result) -> WMError {
    return std::get<WMError>(result);
}

/**
 * @brief Get value from result (if it contains one)
 */
template<typename T>
[[nodiscard]] constexpr auto getValue(const WMResult<T>& result) -> const T& {
    return std::get<T>(result);
}

}  // namespace atom::system::wm

#endif  // ATOM_SYSINFO_WM_COMMON_HPP
