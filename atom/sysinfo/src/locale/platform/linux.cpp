#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)

#include "linux.hpp"
#include <langinfo.h>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_set>
#include <spdlog/spdlog.h>

namespace atom::system::locale::linux_impl {

namespace {
    // RTL language codes
    const std::unordered_set<std::string> RTL_LANGUAGES = {
        "ar", "he", "fa", "ur", "yi", "ji", "iw", "ku", "ps", "sd"
    };
    
    // Countries that typically use imperial measurements
    const std::unordered_set<std::string> IMPERIAL_COUNTRIES = {
        "US", "LR", "MM"
    };
    
    // Countries that typically use Letter paper size
    const std::unordered_set<std::string> LETTER_COUNTRIES = {
        "US", "CA", "MX", "GT", "BZ", "SV", "HN", "NI", "CR", "PA", "CO", "VE", "CL", "PH"
    };
}

auto getLanguageInfo(int item, const std::string& locale) -> std::string {
    std::string result;
    
    if (!locale.empty()) {
        // Temporarily set locale
        char* oldLocale = setlocale(LC_ALL, nullptr);
        std::string savedLocale = oldLocale ? oldLocale : "C";
        
        if (setlocale(LC_ALL, locale.c_str()) != nullptr) {
            char* info = nl_langinfo(item);
            if (info) {
                result = info;
            }
            setlocale(LC_ALL, savedLocale.c_str());
        } else {
            spdlog::warn("Failed to set temporary locale: {}", locale);
        }
    } else {
        char* info = nl_langinfo(item);
        if (info) {
            result = info;
        }
    }
    
    return result.empty() ? "Unknown" : result;
}

auto getSystemLanguageInfo() -> LocaleInfo {
    spdlog::debug("Retrieving Linux system language information");
    LocaleInfo localeInfo;

    // Set locale to user's default
    setlocale(LC_ALL, "");

    // Get basic locale information
    const char* currentLocale = setlocale(LC_ALL, nullptr);
    if (currentLocale) {
        localeInfo = parseLocaleString(currentLocale);
    } else {
        localeInfo.localeName = "C";
        localeInfo.languageCode = "en";
        localeInfo.countryCode = "US";
    }

    // Get detailed information using nl_langinfo
    localeInfo.characterEncoding = getLanguageInfo(CODESET);
    localeInfo.currencySymbol = getLanguageInfo(CRNCYSTR);
    localeInfo.decimalSymbol = getLanguageInfo(RADIXCHAR);
    localeInfo.thousandSeparator = getLanguageInfo(THOUSEP);
    localeInfo.dateFormat = getLanguageInfo(D_FMT);
    localeInfo.timeFormat = getLanguageInfo(T_FMT);
    
    // Set additional properties
    localeInfo.isRTL = isRTLLocale(localeInfo.languageCode);
    localeInfo.measurementSystem = getMeasurementSystem(localeInfo.countryCode);
    localeInfo.paperSize = getPaperSize(localeInfo.countryCode);
    localeInfo.timeZone = getSystemTimeZone();
    localeInfo.firstDayOfWeek = getFirstDayOfWeek(localeInfo.localeName);
    localeInfo.weekendDays = getWeekendDays(localeInfo.localeName);
    
    // Get AM/PM strings
    std::string amString = getLanguageInfo(AM_STR);
    std::string pmString = getLanguageInfo(PM_STR);
    localeInfo.amPmFormat = amString + "/" + pmString;
    
    // Get currency code (extract from currency symbol if available)
    auto [symbol, code] = getCurrencyInfo(localeInfo.localeName);
    localeInfo.currencySymbol = symbol;
    localeInfo.currencyCode = code;

    spdlog::info("Successfully retrieved Linux locale information for: {}", localeInfo.localeName);
    return localeInfo;
}

auto getAvailableLocales() -> std::vector<std::string> {
    std::vector<std::string> locales;
    FILE* pipe = popen("locale -a 2>/dev/null", "r");
    if (pipe) {
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string locale(buffer);
            if (!locale.empty() && locale.back() == '\n') {
                locale.pop_back();
            }
            if (!locale.empty()) {
                locales.emplace_back(std::move(locale));
            }
        }
        pclose(pipe);
    } else {
        spdlog::warn("Failed to execute 'locale -a' command");
        // Fallback to common locales
        locales = {"C", "POSIX", "en_US.UTF-8", "en_GB.UTF-8"};
    }
    spdlog::info("Found {} available locales on Linux", locales.size());
    return locales;
}

auto validateLocale(const std::string& locale) -> bool {
    if (locale.empty()) {
        return false;
    }

    // Try to set the locale temporarily
    char* oldLocale = setlocale(LC_ALL, nullptr);
    std::string savedLocale = oldLocale ? oldLocale : "C";
    
    bool isValid = (setlocale(LC_ALL, locale.c_str()) != nullptr);
    
    // Restore original locale
    setlocale(LC_ALL, savedLocale.c_str());
    
    return isValid;
}

auto setSystemLocale(const std::string& locale) -> LocaleError {
    if (!validateLocale(locale)) {
        spdlog::error("Invalid locale: {}", locale);
        return LocaleError::InvalidLocale;
    }

    try {
        if (setlocale(LC_ALL, locale.c_str()) == nullptr) {
            spdlog::error("Failed to set locale: {}", locale);
            return LocaleError::SystemError;
        }
        spdlog::info("Successfully set Linux locale to: {}", locale);
        return LocaleError::None;
    } catch (const std::exception& e) {
        spdlog::error("Exception while setting Linux locale {}: {}", locale, e.what());
        return LocaleError::SystemError;
    }
}

auto getDefaultLocale() -> std::string {
    spdlog::debug("Getting Linux default locale");
    
    // Try environment variables in order of preference
    const char* locale = getenv("LC_ALL");
    if (!locale || strlen(locale) == 0) {
        locale = getenv("LC_MESSAGES");
    }
    if (!locale || strlen(locale) == 0) {
        locale = getenv("LANG");
    }
    if (!locale || strlen(locale) == 0) {
        locale = setlocale(LC_ALL, nullptr);
    }
    
    if (!locale) {
        spdlog::warn("Failed to get default locale, returning en_US.UTF-8");
        return "en_US.UTF-8";
    }
    
    return std::string(locale);
}

auto getLocaleFromEnvironment(const std::string& category) -> std::string {
    const char* locale = getenv(category.c_str());
    return locale ? std::string(locale) : "";
}

auto getPreferredLanguages() -> std::vector<std::string> {
    std::vector<std::string> languages;
    
    // Check LANGUAGE environment variable (colon-separated list)
    const char* languageEnv = getenv("LANGUAGE");
    if (languageEnv) {
        std::string languageStr(languageEnv);
        std::istringstream iss(languageStr);
        std::string lang;
        while (std::getline(iss, lang, ':')) {
            if (!lang.empty()) {
                languages.push_back(lang);
            }
        }
    }
    
    // Fallback to LANG if LANGUAGE is not set
    if (languages.empty()) {
        std::string defaultLang = getDefaultLocale();
        if (!defaultLang.empty()) {
            languages.push_back(defaultLang);
        }
    }
    
    return languages;
}

auto isRTLLocale(const std::string& locale) -> bool {
    std::string langCode = locale.substr(0, 2);
    std::transform(langCode.begin(), langCode.end(), langCode.begin(), ::tolower);
    return RTL_LANGUAGES.find(langCode) != RTL_LANGUAGES.end();
}

auto getCurrencyInfo(const std::string& locale) -> std::pair<std::string, std::string> {
    std::string symbol = getLanguageInfo(CRNCYSTR, locale);
    std::string code;
    
    // Extract currency code from symbol if it follows the format "-USD$" or "+USD$"
    if (symbol.length() > 3) {
        if (symbol[0] == '-' || symbol[0] == '+') {
            code = symbol.substr(1, 3);
            symbol = symbol.substr(4);
        }
    }
    
    // Common currency mappings based on country
    if (code.empty()) {
        LocaleInfo info = parseLocaleString(locale);
        if (info.countryCode == "US") code = "USD";
        else if (info.countryCode == "GB") code = "GBP";
        else if (info.countryCode == "JP") code = "JPY";
        else if (info.countryCode == "CN") code = "CNY";
        else if (info.countryCode == "IN") code = "INR";
        else if (info.countryCode == "RU") code = "RUB";
        else if (info.countryCode == "BR") code = "BRL";
        else if (info.countryCode == "CA") code = "CAD";
        else if (info.countryCode == "AU") code = "AUD";
        else code = "USD"; // Default fallback
    }
    
    return {symbol, code};
}

auto getMeasurementSystem(const std::string& locale) -> MeasurementSystem {
    LocaleInfo info = parseLocaleString(locale);
    return (IMPERIAL_COUNTRIES.find(info.countryCode) != IMPERIAL_COUNTRIES.end()) 
           ? MeasurementSystem::Imperial : MeasurementSystem::Metric;
}

auto getPaperSize(const std::string& locale) -> PaperSize {
    LocaleInfo info = parseLocaleString(locale);
    return (LETTER_COUNTRIES.find(info.countryCode) != LETTER_COUNTRIES.end()) 
           ? PaperSize::Letter : PaperSize::A4;
}

auto getFirstDayOfWeek(const std::string& locale) -> std::string {
    // Most countries use Monday as first day, US and some others use Sunday
    LocaleInfo info = parseLocaleString(locale);
    if (info.countryCode == "US" || info.countryCode == "CA" || info.countryCode == "IL") {
        return "Sunday";
    }
    return "Monday";
}

auto getWeekendDays(const std::string& locale) -> std::vector<std::string> {
    // Most countries have Saturday and Sunday as weekend
    // Some Middle Eastern countries have Friday and Saturday
    LocaleInfo info = parseLocaleString(locale);
    if (info.countryCode == "SA" || info.countryCode == "AE" || info.countryCode == "BH" ||
        info.countryCode == "KW" || info.countryCode == "QA" || info.countryCode == "OM") {
        return {"Friday", "Saturday"};
    }
    return {"Saturday", "Sunday"};
}

auto getSystemTimeZone() -> std::string {
    // Try to read from /etc/timezone
    std::ifstream timezoneFile("/etc/timezone");
    if (timezoneFile.is_open()) {
        std::string timezone;
        std::getline(timezoneFile, timezone);
        if (!timezone.empty()) {
            return timezone;
        }
    }
    
    // Try TZ environment variable
    const char* tz = getenv("TZ");
    if (tz) {
        return std::string(tz);
    }
    
    // Try to read symlink /etc/localtime
    char buffer[256];
    ssize_t len = readlink("/etc/localtime", buffer, sizeof(buffer) - 1);
    if (len > 0) {
        buffer[len] = '\0';
        std::string path(buffer);
        size_t pos = path.find("/zoneinfo/");
        if (pos != std::string::npos) {
            return path.substr(pos + 10);
        }
    }
    
    return "UTC";
}

auto formatNumber(double number, const std::string& locale) -> std::string {
    // Save current locale
    char* oldLocale = setlocale(LC_NUMERIC, nullptr);
    std::string savedLocale = oldLocale ? oldLocale : "C";
    
    // Set target locale
    if (setlocale(LC_NUMERIC, locale.c_str()) != nullptr) {
        // Use locale-specific formatting
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%.2f", number);
        std::string result(buffer);
        
        // Restore locale
        setlocale(LC_NUMERIC, savedLocale.c_str());
        return result;
    }
    
    // Fallback to default formatting
    setlocale(LC_NUMERIC, savedLocale.c_str());
    return std::to_string(number);
}

auto formatCurrency(double amount, const std::string& locale) -> std::string {
    auto [symbol, code] = getCurrencyInfo(locale);
    std::string formattedNumber = formatNumber(amount, locale);
    return symbol + formattedNumber;
}

auto formatDate(const std::chrono::system_clock::time_point& timestamp, 
                const std::string& locale, 
                const std::string& format) -> std::string {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    struct tm* tm_info = localtime(&time_t);
    
    char buffer[256];
    std::string fmt = format.empty() ? getLanguageInfo(D_FMT, locale) : format;
    if (fmt.empty()) fmt = "%Y-%m-%d";
    
    strftime(buffer, sizeof(buffer), fmt.c_str(), tm_info);
    return std::string(buffer);
}

auto formatTime(const std::chrono::system_clock::time_point& timestamp, 
                const std::string& locale, 
                const std::string& format) -> std::string {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    struct tm* tm_info = localtime(&time_t);
    
    char buffer[256];
    std::string fmt = format.empty() ? getLanguageInfo(T_FMT, locale) : format;
    if (fmt.empty()) fmt = "%H:%M:%S";
    
    strftime(buffer, sizeof(buffer), fmt.c_str(), tm_info);
    return std::string(buffer);
}

auto getSystemLocaleFromConfig() -> std::string {
    // Try /etc/locale.conf (systemd)
    std::ifstream localeConf("/etc/locale.conf");
    if (localeConf.is_open()) {
        std::string line;
        while (std::getline(localeConf, line)) {
            if (line.find("LANG=") == 0) {
                return line.substr(5);
            }
        }
    }
    
    // Try /etc/default/locale (Debian/Ubuntu)
    std::ifstream defaultLocale("/etc/default/locale");
    if (defaultLocale.is_open()) {
        std::string line;
        while (std::getline(defaultLocale, line)) {
            if (line.find("LANG=") == 0) {
                std::string locale = line.substr(5);
                // Remove quotes if present
                if (locale.front() == '"' && locale.back() == '"') {
                    locale = locale.substr(1, locale.length() - 2);
                }
                return locale;
            }
        }
    }
    
    return "";
}

auto getLocaleFromSystemd() -> std::string {
    FILE* pipe = popen("localectl status 2>/dev/null | grep 'System Locale' | cut -d: -f2 | cut -d= -f2", "r");
    if (pipe) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string locale(buffer);
            if (!locale.empty() && locale.back() == '\n') {
                locale.pop_back();
            }
            pclose(pipe);
            return locale;
        }
        pclose(pipe);
    }
    return "";
}

auto isLocaleInstalled(const std::string& locale) -> bool {
    auto availableLocales = getAvailableLocales();
    return std::find(availableLocales.begin(), availableLocales.end(), locale) != availableLocales.end();
}

auto getCharacterEncoding(const std::string& locale) -> std::string {
    return getLanguageInfo(CODESET, locale);
}

auto getCollationInfo(const std::string& locale) -> std::string {
    // Save current locale
    char* oldLocale = setlocale(LC_COLLATE, nullptr);
    std::string savedLocale = oldLocale ? oldLocale : "C";
    
    std::string result;
    if (setlocale(LC_COLLATE, locale.c_str()) != nullptr) {
        // Get collation information - this is implementation specific
        result = locale; // Simplified - actual implementation would get detailed collation rules
        setlocale(LC_COLLATE, savedLocale.c_str());
    } else {
        result = "C";
    }
    
    return result;
}

auto getNumericInfo(const std::string& locale) -> std::string {
    std::string decimal = getLanguageInfo(RADIXCHAR, locale);
    std::string thousands = getLanguageInfo(THOUSEP, locale);
    return "Decimal: " + decimal + ", Thousands: " + thousands;
}

auto getMonetaryInfo(const std::string& locale) -> std::string {
    auto [symbol, code] = getCurrencyInfo(locale);
    return "Symbol: " + symbol + ", Code: " + code;
}

auto getTimeInfo(const std::string& locale) -> std::string {
    std::string dateFormat = getLanguageInfo(D_FMT, locale);
    std::string timeFormat = getLanguageInfo(T_FMT, locale);
    return "Date: " + dateFormat + ", Time: " + timeFormat;
}

auto getMessageInfo(const std::string& locale) -> std::string {
    std::string yesExpr = getLanguageInfo(YESEXPR, locale);
    std::string noExpr = getLanguageInfo(NOEXPR, locale);
    return "Yes: " + yesExpr + ", No: " + noExpr;
}

} // namespace atom::system::locale::linux_impl

#endif // defined(__linux__) || defined(__unix__) || defined(__APPLE__)
