#include "wm.hpp"

#include <array>
#include <format>
#include <string_view>

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <dwmapi.h>
// clang-format on
#if _MSC_VER
#pragma comment(lib, "dwmapi.lib")
#endif
#elif __linux__
#include <cstdlib>
#include <memory>
#endif

#include <spdlog/spdlog.h>

namespace atom::system {

namespace {

#ifdef __linux__
/**
 * @brief Executes a shell command and returns the output.
 * @param command The command to execute
 * @return The command output as a string
 */
auto executeCommand(std::string_view command) -> std::string {
    std::array<char, 256> buffer{};
    std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.data(), "r"),
                                                  pclose);

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

/**
 * @brief Gets the desktop environment from environment variables.
 * @return The desktop environment name
 */
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
#endif

#ifdef _WIN32
/**
 * @brief Gets Windows theme information from registry.
 * @return Theme information string
 */
auto getWindowsTheme() -> std::string {
    DWORD appsUseLightTheme = 1;
    DWORD systemUsesLightTheme = 1;

    HKEY hKey;
    constexpr auto regPath =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

    if (RegOpenKeyExW(HKEY_CURRENT_USER, regPath, 0, KEY_READ, &hKey) ==
        ERROR_SUCCESS) {
        DWORD dataSize = sizeof(DWORD);
        RegQueryValueExW(hKey, L"AppsUseLightTheme", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&appsUseLightTheme),
                         &dataSize);
        RegQueryValueExW(hKey, L"SystemUsesLightTheme", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&systemUsesLightTheme),
                         &dataSize);
        RegCloseKey(hKey);
    }

    return std::format("Windows Theme (System: {}, Apps: {})",
                       systemUsesLightTheme ? "Light" : "Dark",
                       appsUseLightTheme ? "Light" : "Dark");
}

/**
 * @brief Gets Windows system font information.
 * @return Font information string
 */
auto getWindowsFont() -> std::string {
    NONCLIENTMETRICS metrics{};
    metrics.cbSize = sizeof(NONCLIENTMETRICS);

    if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics,
                             0)) {
        return std::format(
            "{} ({}pt)",
            reinterpret_cast<char*>(metrics.lfMessageFont.lfFaceName),
            std::abs(metrics.lfMessageFont.lfHeight));
    }

    return "Unknown Font";
}

/**
 * @brief Checks if Desktop Window Manager is enabled.
 * @return Window manager name
 */
auto getWindowsWM() -> std::string {
    BOOL isDWMEnabled = FALSE;
    HRESULT result = DwmIsCompositionEnabled(&isDWMEnabled);

    if (SUCCEEDED(result) && isDWMEnabled) {
        return "Desktop Window Manager (DWM)";
    }

    return "Classic Windows";
}
#endif

}  // anonymous namespace

auto getSystemInfo() -> SystemInfo {
    spdlog::debug("Retrieving system information");
    SystemInfo info;

#ifdef _WIN32
    spdlog::debug("Detecting Windows desktop environment");

    info.desktopEnvironment = "Windows Fluent Design";
    info.windowManager = getWindowsWM();
    info.wmTheme = getWindowsTheme();
    info.icons = "Windows Shell Icons";
    info.font = getWindowsFont();
    info.cursor = "Windows Default";

    spdlog::debug("Windows system info - DE: {}, WM: {}",
                  info.desktopEnvironment, info.windowManager);

#elif __linux__
    spdlog::debug("Detecting Linux desktop environment");

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
    } else if (info.desktopEnvironment.find("KDE") != std::string::npos) {
        info.wmTheme = executeCommand(
            "kreadconfig5 --group General --key ColorScheme 2>/dev/null");
        info.font = executeCommand(
            "kreadconfig5 --group General --key font 2>/dev/null");
        info.cursor = executeCommand(
            "kreadconfig5 --group Icons --key Theme 2>/dev/null");
        info.icons = executeCommand(
            "kreadconfig5 --group Icons --key Theme 2>/dev/null");
    } else {
        info.wmTheme = "Unknown Theme";
        info.font = "Unknown Font";
        info.cursor = "Unknown Cursor";
        info.icons = "Unknown Icons";
    }

    spdlog::debug("Linux system info - DE: {}, WM: {}", info.desktopEnvironment,
                  info.windowManager);

#else
    spdlog::warn("Unsupported platform for system info detection");

    constexpr auto unknown = "Unsupported Platform";
    info.desktopEnvironment = unknown;
    info.windowManager = unknown;
    info.wmTheme = unknown;
    info.icons = unknown;
    info.font = unknown;
    info.cursor = unknown;
#endif

    spdlog::debug("System information retrieval completed");
    return info;
}

}  // namespace atom::system
