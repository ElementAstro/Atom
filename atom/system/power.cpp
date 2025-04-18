#include "power.hpp"

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <winuser.h>
// clang-format on
#elif defined(__APPLE__)
#include <cstdlib>
#else  // Unix-like systems
#include <cstdlib>
#include <fstream>
#endif

namespace atom::system {

auto shutdown() -> bool {
#ifdef _WIN32
    return ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, 0) != 0;
#elif defined(__APPLE__)
    return std::system(
               "osascript -e 'tell app \"System Events\" to shut down'") == 0;
#else  // Unix-like systems
    return std::system("shutdown -h now") == 0;
#endif
}

auto reboot() -> bool {
#ifdef _WIN32
    return ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0) != 0;
#elif defined(__APPLE__)
    return std::system(
               "osascript -e 'tell app \"System Events\" to restart'") == 0;
#else  // Unix-like systems
    return std::system("reboot") == 0;
#endif
}

auto hibernate() -> bool {
#ifdef _WIN32
    return SetSystemPowerState(TRUE, FALSE) != 0;
#elif defined(__APPLE__)
    return std::system("pmset sleepnow") == 0;
#else  // Unix-like systems
    return std::system("systemctl hibernate") == 0;
#endif
}

auto logout() -> bool {
#ifdef _WIN32
    return ExitWindowsEx(EWX_LOGOFF | EWX_FORCE, 0) != 0;
#elif defined(__APPLE__)
    return std::system(
               "osascript -e 'tell app \"System Events\" to log out'") == 0;
#else  // Unix-like systems
    return std::system("pkill -KILL -u $(whoami)") == 0;
#endif
}

auto lockScreen() -> bool {
#ifdef _WIN32
    return LockWorkStation() != 0;
#elif defined(__APPLE__)
    return std::system(
               "/System/Library/CoreServices/Menu\\ "
               "Extras/User.menu/Contents/Resources/CGSession -suspend") == 0;
#else  // Unix-like systems
    // Try different methods for various desktop environments
    if (std::system("gnome-screensaver-command -l") == 0) {
        return true;
    } else if (std::system(
                   "qdbus org.freedesktop.ScreenSaver /ScreenSaver Lock") ==
               0) {
        return true;
    } else if (std::system("xdg-screensaver lock") == 0) {
        return true;
    }
    return false;
#endif
}

auto setScreenBrightness(int level) -> bool {
#ifdef _WIN32
    // Windows implementation for setting screen brightness
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) {
        return false;
    }

    CONSOLE_SCREEN_BUFFER_INFOEX csbi;
    csbi.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    if (!GetConsoleScreenBufferInfoEx(hConsole, &csbi)) {
        return false;
    }

    csbi.wAttributes = (csbi.wAttributes & 0xFFF0) | (level & 0x000F);
    if (!SetConsoleScreenBufferInfoEx(hConsole, &csbi)) {
        return false;
    }

    return true;
#elif defined(__APPLE__)
    // macOS implementation for setting screen brightness
    std::string command = "brightness " + std::to_string(level / 100.0);
    return std::system(command.c_str()) == 0;
#else  // Unix-like systems
    // Unix-like systems implementation for setting screen brightness
    std::ofstream brightnessFile(
        "/sys/class/backlight/intel_backlight/brightness");
    if (brightnessFile.is_open()) {
        brightnessFile << level;
        return true;
    }
    return false;
#endif
}
}  // namespace atom::system
