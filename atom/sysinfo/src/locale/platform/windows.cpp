#ifdef _WIN32

#include "windows.hpp"
#include <windows.h>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace atom::system::locale::windows {

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

auto stringToWstring(const std::string& str) -> std::wstring {
    if (str.empty()) {
        return std::wstring();
    }

    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        spdlog::warn("Failed to get required buffer size for wide conversion");
        return std::wstring(str.begin(), str.end());
    }

    std::wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

auto getLocaleInfo(unsigned long type, const std::string& localeName) -> std::string {
    spdlog::debug("Getting locale info for type: {}", type);
    WCHAR buffer[LOCALE_NAME_MAX_LENGTH];
    
    LPCWSTR localeNamePtr = LOCALE_NAME_USER_DEFAULT;
    std::wstring wLocaleName;
    if (!localeName.empty()) {
        wLocaleName = stringToWstring(localeName);
        localeNamePtr = wLocaleName.c_str();
    }
    
    int result = GetLocaleInfoEx(localeNamePtr, type, buffer, LOCALE_NAME_MAX_LENGTH);
    if (result != 0) {
        spdlog::debug("Successfully retrieved locale info");
        return wstringToString(buffer);
    }
    spdlog::warn("Failed to retrieve locale info for type: {}", type);
    return "Unknown";
}

auto getLocaleInfoNumeric(unsigned long type, const std::string& localeName) -> int {
    std::string strValue = getLocaleInfo(type, localeName);
    try {
        return std::stoi(strValue);
    } catch (const std::exception& e) {
        spdlog::warn("Failed to convert locale info to numeric: {}", e.what());
        return 0;
    }
}

auto getSystemLanguageInfo() -> LocaleInfo {
    spdlog::debug("Retrieving Windows system language information");
    LocaleInfo localeInfo;

    localeInfo.languageCode = getLocaleInfo(LOCALE_SISO639LANGNAME);
    localeInfo.countryCode = getLocaleInfo(LOCALE_SISO3166CTRYNAME);
    localeInfo.localeName = getLocaleInfo(LOCALE_SNAME);
    localeInfo.languageDisplayName = getLocaleInfo(LOCALE_SNATIVELANGNAME);
    localeInfo.countryDisplayName = getLocaleInfo(LOCALE_SNATIVECTRYNAME);
    localeInfo.currencySymbol = getLocaleInfo(LOCALE_SCURRENCY);
    localeInfo.currencyCode = getLocaleInfo(LOCALE_SINTLSYMBOL);
    localeInfo.decimalSymbol = getLocaleInfo(LOCALE_SDECIMAL);
    localeInfo.thousandSeparator = getLocaleInfo(LOCALE_STHOUSAND);
    localeInfo.dateFormat = getLocaleInfo(LOCALE_SSHORTDATE);
    localeInfo.timeFormat = getLocaleInfo(LOCALE_STIMEFORMAT);
    localeInfo.characterEncoding = "UTF-8"; // Windows uses UTF-16 internally, but we report UTF-8
    localeInfo.isRTL = isRTLLocale(localeInfo.localeName);
    localeInfo.numberFormat = getLocaleInfo(LOCALE_SNATIVEDIGITS);
    localeInfo.measurementSystem = getMeasurementSystem(localeInfo.localeName);
    localeInfo.paperSize = getPaperSize(localeInfo.localeName);
    localeInfo.scriptCode = getLocaleInfo(LOCALE_SSCRIPTS);
    localeInfo.timeZone = getSystemTimeZone();
    localeInfo.firstDayOfWeek = getFirstDayOfWeek(localeInfo.localeName);
    localeInfo.weekendDays = getWeekendDays(localeInfo.localeName);
    localeInfo.amPmFormat = getLocaleInfo(LOCALE_S1159) + "/" + getLocaleInfo(LOCALE_S2359);
    localeInfo.listSeparator = getLocaleInfo(LOCALE_SLIST);

    spdlog::info("Successfully retrieved Windows locale information for: {}", localeInfo.localeName);
    return localeInfo;
}

auto getAvailableLocales() -> std::vector<std::string> {
    std::vector<std::string> locales;
    EnumSystemLocalesEx(
        [](LPWSTR localeName, DWORD, LPARAM param) -> BOOL {
            auto* locales = reinterpret_cast<std::vector<std::string>*>(param);
            locales->push_back(wstringToString(localeName));
            return TRUE;
        },
        LOCALE_ALL, reinterpret_cast<LPARAM>(&locales), nullptr);
    spdlog::info("Found {} available locales on Windows", locales.size());
    return locales;
}

auto validateLocale(const std::string& locale) -> bool {
    if (locale.empty()) {
        return false;
    }

    std::wstring wlocale = stringToWstring(locale);
    LCID lcid = LocaleNameToLCID(wlocale.c_str(), 0);
    return lcid != 0;
}

auto setSystemLocale(const std::string& locale) -> LocaleError {
    if (!validateLocale(locale)) {
        spdlog::error("Invalid locale: {}", locale);
        return LocaleError::InvalidLocale;
    }

    try {
        std::wstring wlocale = stringToWstring(locale);
        LCID lcid = LocaleNameToLCID(wlocale.c_str(), 0);
        if (lcid == 0) {
            spdlog::error("Failed to convert locale name to LCID: {}", locale);
            return LocaleError::SystemError;
        }
        if (SetThreadLocale(lcid) == 0) {
            spdlog::error("Failed to set thread locale: {}", locale);
            return LocaleError::SystemError;
        }
        spdlog::info("Successfully set Windows locale to: {}", locale);
        return LocaleError::None;
    } catch (const std::exception& e) {
        spdlog::error("Exception while setting Windows locale {}: {}", locale, e.what());
        return LocaleError::SystemError;
    }
}

auto getDefaultLocale() -> std::string {
    spdlog::debug("Getting Windows default locale");
    WCHAR localeName[LOCALE_NAME_MAX_LENGTH];
    if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH) == 0) {
        spdlog::warn("Failed to get default locale, returning en-US");
        return "en-US";
    }
    return wstringToString(localeName);
}

auto getPreferredUILanguages() -> std::vector<std::string> {
    std::vector<std::string> languages;
    ULONG numLanguages = 0;
    ULONG bufferSize = 0;
    
    // Get buffer size
    if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &numLanguages, nullptr, &bufferSize)) {
        std::vector<WCHAR> buffer(bufferSize);
        if (GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &numLanguages, buffer.data(), &bufferSize)) {
            WCHAR* current = buffer.data();
            for (ULONG i = 0; i < numLanguages; ++i) {
                languages.push_back(wstringToString(current));
                current += wcslen(current) + 1;
            }
        }
    }
    
    if (languages.empty()) {
        languages.push_back(getDefaultLocale());
    }
    
    return languages;
}

auto getSystemDefaultUILanguage() -> std::string {
    LANGID langId = GetSystemDefaultUILanguage();
    WCHAR localeName[LOCALE_NAME_MAX_LENGTH];
    if (LCIDToLocaleName(MAKELCID(langId, SORT_DEFAULT), localeName, LOCALE_NAME_MAX_LENGTH, 0) != 0) {
        return wstringToString(localeName);
    }
    return "en-US";
}

auto getUserDefaultUILanguage() -> std::string {
    LANGID langId = GetUserDefaultUILanguage();
    WCHAR localeName[LOCALE_NAME_MAX_LENGTH];
    if (LCIDToLocaleName(MAKELCID(langId, SORT_DEFAULT), localeName, LOCALE_NAME_MAX_LENGTH, 0) != 0) {
        return wstringToString(localeName);
    }
    return "en-US";
}

auto isRTLLocale(const std::string& locale) -> bool {
    int readingLayout = getLocaleInfoNumeric(LOCALE_IREADINGLAYOUT, locale);
    return (readingLayout == 1); // 1 indicates right-to-left
}

auto getCurrencyInfo(const std::string& locale) -> std::pair<std::string, std::string> {
    std::string symbol = getLocaleInfo(LOCALE_SCURRENCY, locale);
    std::string code = getLocaleInfo(LOCALE_SINTLSYMBOL, locale);
    return {symbol, code};
}

auto getMeasurementSystem(const std::string& locale) -> MeasurementSystem {
    int measure = getLocaleInfoNumeric(LOCALE_IMEASURE, locale);
    return (measure == 0) ? MeasurementSystem::Metric : MeasurementSystem::US;
}

auto getPaperSize(const std::string& locale) -> PaperSize {
    int paperSize = getLocaleInfoNumeric(LOCALE_IPAPERSIZE, locale);
    switch (paperSize) {
        case 1: return PaperSize::Letter;
        case 5: return PaperSize::Legal;
        case 8: return PaperSize::A3;
        case 9: return PaperSize::A4;
        case 11: return PaperSize::A5;
        default: return PaperSize::A4;
    }
}

auto getFirstDayOfWeek(const std::string& locale) -> std::string {
    int firstDay = getLocaleInfoNumeric(LOCALE_IFIRSTDAYOFWEEK, locale);
    const std::vector<std::string> days = {
        "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"
    };
    return (firstDay >= 0 && firstDay < 7) ? days[firstDay] : "Monday";
}

auto getWeekendDays(const std::string& locale) -> std::vector<std::string> {
    // Windows doesn't provide direct API for weekend days, use common defaults
    int firstDay = getLocaleInfoNumeric(LOCALE_IFIRSTDAYOFWEEK, locale);
    if (firstDay == 6) { // Sunday is first day (US style)
        return {"Saturday", "Sunday"};
    } else { // Monday is first day (most other countries)
        return {"Saturday", "Sunday"};
    }
}

auto getSystemTimeZone() -> std::string {
    TIME_ZONE_INFORMATION tzi;
    DWORD result = GetTimeZoneInformation(&tzi);
    if (result != TIME_ZONE_ID_INVALID) {
        return wstringToString(tzi.StandardName);
    }
    return "UTC";
}

auto formatNumber(double number, const std::string& locale) -> std::string {
    // Implementation would use GetNumberFormat API
    // For now, return a basic format
    return std::to_string(number);
}

auto formatCurrency(double amount, const std::string& locale) -> std::string {
    // Implementation would use GetCurrencyFormat API
    // For now, return a basic format
    auto [symbol, code] = getCurrencyInfo(locale);
    return symbol + std::to_string(amount);
}

auto formatDate(const std::chrono::system_clock::time_point& timestamp, 
                const std::string& locale, 
                const std::string& format) -> std::string {
    // Implementation would use GetDateFormat API
    // For now, return a basic format
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    return std::to_string(time_t);
}

auto formatTime(const std::chrono::system_clock::time_point& timestamp, 
                const std::string& locale, 
                const std::string& format) -> std::string {
    // Implementation would use GetTimeFormat API
    // For now, return a basic format
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    return std::to_string(time_t);
}

} // namespace atom::system::locale::windows

#endif // _WIN32
