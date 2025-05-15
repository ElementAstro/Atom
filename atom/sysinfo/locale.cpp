#include "locale.hpp"

#include <mutex>

#ifdef _WIN32
#include <windows.h>
#else
#include <langinfo.h>
#endif

#ifdef ATOM_ENABLE_DEBUG
#include <iostream>
#endif

#include "atom/log/loguru.hpp"

namespace atom::system {

static std::mutex cacheMutex;
static std::optional<LocaleInfo> cachedInfo;
static std::chrono::system_clock::time_point lastCacheUpdate;

#ifdef _WIN32
// Windows-specific helper function to convert wstring to string
auto wstringToString(const std::wstring& wstr) -> std::string {
    LOG_F(INFO, "Converting wstring to string");
    return std::string(wstr.begin(), wstr.end());
}

// Function to get locale info on Windows
std::string getLocaleInfo(LCTYPE type) {
    LOG_F(INFO, "Getting locale info for type: %d", type);
    WCHAR buffer[LOCALE_NAME_MAX_LENGTH];
    int result = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, type, buffer,
                                 LOCALE_NAME_MAX_LENGTH);
    if (result != 0) {
        LOG_F(INFO, "Successfully retrieved locale info");
        return wstringToString(buffer);
    }
    LOG_F(WARNING, "Failed to retrieve locale info");
    return "Unknown";
}

// Function to get available locales on Windows
auto getAvailableLocales() -> std::vector<std::string> {
    std::vector<std::string> locales;
    EnumSystemLocalesEx(
        [](LPWSTR localeName, DWORD, LPARAM param) -> BOOL {
            auto* locales = reinterpret_cast<std::vector<std::string>*>(param);
            locales->push_back(wstringToString(localeName));
            return TRUE;
        },
        LOCALE_ALL, reinterpret_cast<LPARAM>(&locales), nullptr);
    return locales;
}
#else
// Unix-like systems implementation
auto getAvailableLocales() -> std::vector<std::string> {
    std::vector<std::string> locales;
    // Use system command to get available locales
    FILE* pipe = popen("locale -a", "r");
    if (pipe) {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            locales.emplace_back(buffer);
        }
        pclose(pipe);
    }
    return locales;
}
#endif

auto validateLocale(const std::string& locale) -> bool {
    auto locales = getAvailableLocales();
    return std::find(locales.begin(), locales.end(), locale) != locales.end();
}

auto getCachedLocaleInfo() -> const LocaleInfo& {
    std::lock_guard<std::mutex> lock(cacheMutex);
    auto now = std::chrono::system_clock::now();

    if (!cachedInfo || (now - lastCacheUpdate) > cachedInfo->cacheTimeout) {
        cachedInfo = getSystemLanguageInfo();
        lastCacheUpdate = now;
    }

    return *cachedInfo;
}

void clearLocaleCache() {
    std::lock_guard<std::mutex> lock(cacheMutex);
    cachedInfo.reset();
}

auto setSystemLocale(const std::string& locale) -> LocaleError {
    if (!validateLocale(locale)) {
        return LocaleError::InvalidLocale;
    }

    try {
#ifdef _WIN32
        // Convert narrow string to wide string
        std::wstring wlocale(locale.begin(), locale.end());
        if (SetThreadLocale(LocaleNameToLCID(wlocale.c_str(), 0)) == 0) {
            return LocaleError::SystemError;
        }
#else
        if (setlocale(LC_ALL, locale.c_str()) == nullptr) {
            return LocaleError::SystemError;
        }
#endif
        clearLocaleCache();
        return LocaleError::None;
    } catch (...) {
        return LocaleError::SystemError;
    }
}

// Function to get system language info, cross-platform
LocaleInfo getSystemLanguageInfo() {
    LOG_F(INFO, "Starting getSystemLanguageInfo function");
    LocaleInfo localeInfo;

#ifdef _WIN32
    // On Windows, use GetLocaleInfoEx to retrieve locale info
    LOG_F(INFO, "Retrieving locale info on Windows");
    localeInfo.languageCode = getLocaleInfo(LOCALE_SISO639LANGNAME);
    localeInfo.countryCode = getLocaleInfo(LOCALE_SISO3166CTRYNAME);
    localeInfo.localeName = getLocaleInfo(LOCALE_SNAME);
    localeInfo.languageDisplayName = getLocaleInfo(LOCALE_SNATIVELANGNAME);
    localeInfo.countryDisplayName = getLocaleInfo(LOCALE_SNATIVECTRYNAME);
    localeInfo.currencySymbol = getLocaleInfo(LOCALE_SCURRENCY);
    localeInfo.decimalSymbol = getLocaleInfo(LOCALE_SDECIMAL);
    localeInfo.thousandSeparator = getLocaleInfo(LOCALE_STHOUSAND);
    localeInfo.dateFormat = getLocaleInfo(LOCALE_SSHORTDATE);
    localeInfo.timeFormat = getLocaleInfo(LOCALE_STIMEFORMAT);
    localeInfo.characterEncoding = getLocaleInfo(LOCALE_IDEFAULTANSICODEPAGE);
    localeInfo.isRTL = GetSystemDefaultUILanguage() & 0x1;
    localeInfo.numberFormat = getLocaleInfo(LOCALE_SNATIVEDIGITS);
    localeInfo.measurementSystem = getLocaleInfo(LOCALE_IMEASURE);
    localeInfo.paperSize = getLocaleInfo(LOCALE_IPAPERSIZE);
#else
    // On Unix-like systems, use setlocale and nl_langinfo
    LOG_F(INFO, "Retrieving locale info on Unix-like system");
    std::setlocale(LC_ALL, "");
    localeInfo.languageCode =
        std::string(nl_langinfo(CODESET));  // Language code as encoding
    localeInfo.countryCode = "N/A";  // No direct equivalent for country code
    localeInfo.localeName =
        std::string(setlocale(LC_ALL, NULL));  // Full locale name
    localeInfo.languageDisplayName = "N/A";    // Not directly available
    localeInfo.countryDisplayName = "N/A";     // Not directly available
    localeInfo.currencySymbol = std::string(nl_langinfo(CRNCYSTR));
    localeInfo.decimalSymbol = std::string(nl_langinfo(RADIXCHAR));
    localeInfo.thousandSeparator =
        "N/A";  // Not directly available from nl_langinfo
    localeInfo.dateFormat = std::string(nl_langinfo(D_FMT));  // Date format
    localeInfo.timeFormat = std::string(nl_langinfo(T_FMT));  // Time format
    localeInfo.characterEncoding =
        std::string(nl_langinfo(CODESET));  // Character encoding
    localeInfo.isRTL = false;               // Needs specific language check
    // TODO: Fix this problem, NUMERIC_FMT is not existed
    // localeInfo.numberFormat = std::string(nl_langinfo(NUMERIC_FMT));
    localeInfo.measurementSystem = "metric";  // Most Unix systems use metric
    localeInfo.paperSize = "A4";              // Default A4
#endif

    LOG_F(INFO, "Finished getSystemLanguageInfo function");
    return localeInfo;
}

// Function to display locale information
void printLocaleInfo([[maybe_unused]] const LocaleInfo& info) {
#if ATOM_ENABLE_DEBUG
    LOG_F(INFO, "Printing locale information");
    std::cout << "Language code (ISO 639): " << info.languageCode << "\n";
    std::cout << "Country code (ISO 3166): " << info.countryCode << "\n";
    std::cout << "Full locale name: " << info.localeName << "\n";
    std::cout << "Language display name: " << info.languageDisplayName << "\n";
    std::cout << "Country display name: " << info.countryDisplayName << "\n";
    std::cout << "Currency symbol: " << info.currencySymbol << "\n";
    std::cout << "Decimal symbol: " << info.decimalSymbol << "\n";
    std::cout << "Thousand separator: " << info.thousandSeparator << "\n";
    std::cout << "Date format: " << info.dateFormat << "\n";
    std::cout << "Time format: " << info.timeFormat << "\n";
    std::cout << "Character encoding: " << info.characterEncoding << "\n";
    std::cout << "Is RTL: " << info.isRTL << "\n";
    std::cout << "Number format: " << info.numberFormat << "\n";
    std::cout << "Measurement system: " << info.measurementSystem << "\n";
    std::cout << "Paper size: " << info.paperSize << "\n";
#endif
}

auto getDefaultLocale() -> std::string {
    LOG_F(INFO, "Getting default locale");
#ifdef _WIN32
    WCHAR localeName[LOCALE_NAME_MAX_LENGTH];
    if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) == 0) {
        LOG_F(WARNING, "Failed to get default locale, returning en-US");
        return "en-US";
    }
    return wstringToString(localeName);
#else
    const char* locale = setlocale(LC_ALL, nullptr);
    if (!locale) {
        LOG_F(WARNING, "Failed to get default locale, returning en_US.UTF-8");
        return "en_US.UTF-8";
    }
    return std::string(locale);
#endif
}

bool LocaleInfo::operator==(const LocaleInfo& other) const {
    return languageCode == other.languageCode &&
           countryCode == other.countryCode && localeName == other.localeName;
}

}  // namespace atom::system
