#include "locale.hpp"

#include <algorithm>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#else
#include <langinfo.h>
#include <clocale>
#include <cstdio>
#endif

#ifdef ATOM_ENABLE_DEBUG
#include <iostream>
#endif

#include <spdlog/spdlog.h>

namespace atom::system {

static std::mutex cacheMutex;
static std::optional<LocaleInfo> cachedInfo;
static std::chrono::system_clock::time_point lastCacheUpdate;

#ifdef _WIN32
/**
 * @brief Converts a wide string to a UTF-8 string
 * @param wstr The wide string to convert
 * @return The converted UTF-8 string
 */
auto wstringToString(const std::wstring& wstr) -> std::string {
    spdlog::debug("Converting wstring to string");
    if (wstr.empty()) {
        return std::string();
    }

    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0,
                                   nullptr, nullptr);
    if (size <= 0) {
        spdlog::warn("Failed to get required buffer size for conversion");
        return std::string(wstr.begin(), wstr.end());
    }

    std::string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr,
                        nullptr);
    return result;
}

/**
 * @brief Retrieves locale information from Windows API
 * @param type The type of locale information to retrieve
 * @return The locale information as a string
 */
std::string getLocaleInfo(LCTYPE type) {
    spdlog::debug("Getting locale info for type: {}", type);
    WCHAR buffer[LOCALE_NAME_MAX_LENGTH];
    int result = GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, type, buffer,
                                 LOCALE_NAME_MAX_LENGTH);
    if (result != 0) {
        spdlog::debug("Successfully retrieved locale info");
        return wstringToString(buffer);
    }
    spdlog::warn("Failed to retrieve locale info for type: {}", type);
    return "Unknown";
}

/**
 * @brief Retrieves all available locales on Windows
 * @return Vector of locale names
 */
auto getAvailableLocales() -> std::vector<std::string> {
    std::vector<std::string> locales;
    EnumSystemLocalesEx(
        [](LPWSTR localeName, DWORD, LPARAM param) -> BOOL {
            auto* locales = reinterpret_cast<std::vector<std::string>*>(param);
            locales->push_back(wstringToString(localeName));
            return TRUE;
        },
        LOCALE_ALL, reinterpret_cast<LPARAM>(&locales), nullptr);
    spdlog::info("Found {} available locales", locales.size());
    return locales;
}
#else
/**
 * @brief Retrieves all available locales on Unix-like systems
 * @return Vector of locale names
 */
auto getAvailableLocales() -> std::vector<std::string> {
    std::vector<std::string> locales;
    FILE* pipe = popen("locale -a", "r");
    if (pipe) {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string locale(buffer);
            if (!locale.empty() && locale.back() == '\n') {
                locale.pop_back();
            }
            locales.emplace_back(std::move(locale));
        }
        pclose(pipe);
    } else {
        spdlog::warn("Failed to execute 'locale -a' command");
    }
    spdlog::info("Found {} available locales", locales.size());
    return locales;
}
#endif

auto validateLocale(const std::string& locale) -> bool {
    if (locale.empty()) {
        return false;
    }

    auto locales = getAvailableLocales();
    return std::find(locales.begin(), locales.end(), locale) != locales.end();
}

auto getCachedLocaleInfo() -> const LocaleInfo& {
    std::lock_guard<std::mutex> lock(cacheMutex);
    auto now = std::chrono::system_clock::now();

    if (!cachedInfo || (now - lastCacheUpdate) > cachedInfo->cacheTimeout) {
        spdlog::debug("Refreshing locale cache");
        cachedInfo = getSystemLanguageInfo();
        lastCacheUpdate = now;
    }

    return *cachedInfo;
}

void clearLocaleCache() {
    std::lock_guard<std::mutex> lock(cacheMutex);
    cachedInfo.reset();
    spdlog::debug("Locale cache cleared");
}

auto setSystemLocale(const std::string& locale) -> LocaleError {
    if (!validateLocale(locale)) {
        spdlog::error("Invalid locale: {}", locale);
        return LocaleError::InvalidLocale;
    }

    try {
#ifdef _WIN32
        std::wstring wlocale(locale.begin(), locale.end());
        LCID lcid = LocaleNameToLCID(wlocale.c_str(), 0);
        if (lcid == 0) {
            spdlog::error("Failed to convert locale name to LCID: {}", locale);
            return LocaleError::SystemError;
        }
        if (SetThreadLocale(lcid) == 0) {
            spdlog::error("Failed to set thread locale: {}", locale);
            return LocaleError::SystemError;
        }
#else
        if (setlocale(LC_ALL, locale.c_str()) == nullptr) {
            spdlog::error("Failed to set locale: {}", locale);
            return LocaleError::SystemError;
        }
#endif
        clearLocaleCache();
        spdlog::info("Successfully set locale to: {}", locale);
        return LocaleError::None;
    } catch (const std::exception& e) {
        spdlog::error("Exception while setting locale {}: {}", locale,
                      e.what());
        return LocaleError::SystemError;
    }
}

LocaleInfo getSystemLanguageInfo() {
    spdlog::debug("Retrieving system language information");
    LocaleInfo localeInfo;

#ifdef _WIN32
    spdlog::debug("Using Windows API for locale information");
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
    localeInfo.isRTL = (GetSystemDefaultUILanguage() & 0x1) != 0;
    localeInfo.numberFormat = getLocaleInfo(LOCALE_SNATIVEDIGITS);
    localeInfo.measurementSystem = getLocaleInfo(LOCALE_IMEASURE);
    localeInfo.paperSize = getLocaleInfo(LOCALE_IPAPERSIZE);
#else
    spdlog::debug("Using POSIX API for locale information");
    std::setlocale(LC_ALL, "");

    localeInfo.languageCode = std::string(nl_langinfo(CODESET));
    localeInfo.countryCode = "N/A";
    localeInfo.localeName = std::string(setlocale(LC_ALL, nullptr));
    localeInfo.languageDisplayName = "N/A";
    localeInfo.countryDisplayName = "N/A";
    localeInfo.currencySymbol = std::string(nl_langinfo(CRNCYSTR));
    localeInfo.decimalSymbol = std::string(nl_langinfo(RADIXCHAR));
    localeInfo.thousandSeparator = std::string(nl_langinfo(THOUSEP));
    localeInfo.dateFormat = std::string(nl_langinfo(D_FMT));
    localeInfo.timeFormat = std::string(nl_langinfo(T_FMT));
    localeInfo.characterEncoding = std::string(nl_langinfo(CODESET));
    localeInfo.isRTL = false;
    localeInfo.numberFormat = "N/A";
    localeInfo.measurementSystem = "metric";
    localeInfo.paperSize = "A4";
#endif

    spdlog::info("Successfully retrieved locale information for: {}",
                 localeInfo.localeName);
    return localeInfo;
}

void printLocaleInfo([[maybe_unused]] const LocaleInfo& info) {
#ifdef ATOM_ENABLE_DEBUG
    spdlog::info("Printing locale information");
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
    std::cout << "Is RTL: " << std::boolalpha << info.isRTL << "\n";
    std::cout << "Number format: " << info.numberFormat << "\n";
    std::cout << "Measurement system: " << info.measurementSystem << "\n";
    std::cout << "Paper size: " << info.paperSize << "\n";
#endif
}

auto getDefaultLocale() -> std::string {
    spdlog::debug("Getting default locale");
#ifdef _WIN32
    WCHAR localeName[LOCALE_NAME_MAX_LENGTH];
    if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) == 0) {
        spdlog::warn("Failed to get default locale, returning en-US");
        return "en-US";
    }
    return wstringToString(localeName);
#else
    const char* locale = setlocale(LC_ALL, nullptr);
    if (!locale) {
        spdlog::warn("Failed to get default locale, returning en_US.UTF-8");
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
