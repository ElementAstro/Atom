/**
 * @file locale.hpp
 * @brief System locale information functionality (Compatibility Header)
 *
 * This file provides backward compatibility for the original locale API.
 * The actual implementation has been moved to the locale/ folder with
 * enhanced features and platform-specific implementations.
 *
 * For new code, consider using the enhanced API in locale/locale.hpp
 * which provides additional features like LocaleManager, LocaleFormatter,
 * and LocaleDetector classes.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_LOCALE_HPP
#define ATOM_SYSINFO_LOCALE_HPP

// Include the new enhanced locale implementation
#include "src/locale/locale.hpp"

// Re-export all symbols for backward compatibility
namespace atom::system {

// All types and functions are now available through the include above
// The following are already available:
// - LocaleError enum
// - LocaleInfo struct
// - getSystemLanguageInfo()
// - printLocaleInfo()
// - validateLocale()
// - setSystemLocale()
// - getAvailableLocales()
// - getDefaultLocale()
// - getCachedLocaleInfo()
// - clearLocaleCache()

// Additional enhanced features available in locale/locale.hpp:
// - LocaleManager class for advanced locale management
// - LocaleFormatter class for locale-aware formatting
// - LocaleDetector class for automatic locale detection
// - LocalePreferences struct for user preferences
// - Enhanced error handling and caching
// - Platform-specific optimizations

}  // namespace atom::system

#endif  // ATOM_SYSINFO_LOCALE_HPP
