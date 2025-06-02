#ifndef ATOM_SYSINFO_WM_HPP
#define ATOM_SYSINFO_WM_HPP

#include <string>

#include "atom/macro.hpp"

namespace atom::system {

/**
 * @brief Contains system desktop environment and window manager information.
 */
struct SystemInfo {
    std::string desktopEnvironment;  //!< Desktop environment (e.g., Fluent, GNOME, KDE)
    std::string windowManager;       //!< Window manager (e.g., Desktop Window Manager, i3, bspwm)
    std::string wmTheme;            //!< Window manager theme information
    std::string icons;              //!< Icon theme or icon information
    std::string font;               //!< System font information
    std::string cursor;             //!< Cursor theme information
} ATOM_ALIGNAS(128);

/**
 * @brief Retrieves system desktop environment and window manager information.
 * @return SystemInfo structure containing desktop environment details
 */
[[nodiscard]] auto getSystemInfo() -> SystemInfo;

}  // namespace atom::system

#endif
