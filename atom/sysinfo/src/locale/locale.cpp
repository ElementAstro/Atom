#include "locale.hpp"
#include "common.hpp"

// Platform-specific includes
#ifdef _WIN32
#include "windows.hpp"
#elif defined(__APPLE__)
#include "macos.hpp"
#include "linux.hpp" // For fallback functionality
#else
#include "linux.hpp"
#endif

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <spdlog/spdlog.h>

namespace atom::system {

namespace {
    // Global cache for locale information
    std::mutex cacheMutex;
    std::optional<LocaleInfo> cachedInfo;
    std::chrono::system_clock::time_point lastCacheUpdate;
    std::vector<std::string> cachedAvailableLocales;
    std::chrono::system_clock::time_point lastLocaleListUpdate;
    std::chrono::seconds cacheTimeout{300}; // 5 minutes default
}

// LocaleManager implementation
class LocaleManager::Impl {
public:
    LocalePreferences preferences;
    std::unordered_map<size_t, std::function<void(const std::string&, const std::string&)>> callbacks;
    std::atomic<size_t> nextCallbackId{1};
    std::mutex callbackMutex;
    std::chrono::seconds cacheTimeout{300};

    auto getCurrentLocaleImpl() -> LocaleInfo {
#ifdef _WIN32
        return locale::windows::getSystemLanguageInfo();
#elif defined(__APPLE__)
        return locale::macos::getSystemLanguageInfo();
#else
        return locale::linux_impl::getSystemLanguageInfo();
#endif
    }

    auto getAvailableLocalesImpl() -> std::vector<std::string> {
#ifdef _WIN32
        return locale::windows::getAvailableLocales();
#elif defined(__APPLE__)
        return locale::macos::getAvailableLocales();
#else
        return locale::linux_impl::getAvailableLocales();
#endif
    }

    auto validateLocaleImpl(const std::string& locale) -> bool {
#ifdef _WIN32
        return locale::windows::validateLocale(locale);
#elif defined(__APPLE__)
        return locale::macos::validateLocale(locale);
#else
        return locale::linux_impl::validateLocale(locale);
#endif
    }

    auto setLocaleImpl(const std::string& locale) -> LocaleError {
        std::string oldLocale = getCurrentLocaleImpl().localeName;

#ifdef _WIN32
        auto result = locale::windows::setSystemLocale(locale);
#elif defined(__APPLE__)
        auto result = locale::macos::setSystemLocale(locale);
#else
        auto result = locale::linux_impl::setSystemLocale(locale);
#endif

        if (result == LocaleError::None) {
            notifyCallbacks(oldLocale, locale);
        }
        return result;
    }

    auto getDefaultLocaleImpl() -> std::string {
#ifdef _WIN32
        return locale::windows::getDefaultLocale();
#elif defined(__APPLE__)
        return locale::macos::getDefaultLocale();
#else
        return locale::linux_impl::getDefaultLocale();
#endif
    }

    auto getPreferredLanguagesImpl() -> std::vector<std::string> {
#ifdef _WIN32
        return locale::windows::getPreferredUILanguages();
#elif defined(__APPLE__)
        return locale::macos::getPreferredLanguages();
#else
        return locale::linux_impl::getPreferredLanguages();
#endif
    }

    void notifyCallbacks(const std::string& oldLocale, const std::string& newLocale) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        for (const auto& [id, callback] : callbacks) {
            try {
                callback(oldLocale, newLocale);
            } catch (...) {
                // Ignore callback exceptions
            }
        }
    }
};

LocaleManager::LocaleManager() : pImpl(std::make_unique<Impl>()) {}
LocaleManager::~LocaleManager() = default;

auto LocaleManager::getCurrentLocale(bool useCache) -> LocaleInfo {
    if (useCache) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto now = std::chrono::system_clock::now();

        if (!cachedInfo || (now - lastCacheUpdate) > cacheTimeout) {
            spdlog::debug("Refreshing locale cache");
            cachedInfo = pImpl->getCurrentLocaleImpl();
            lastCacheUpdate = now;
        }

        return *cachedInfo;
    }

    return pImpl->getCurrentLocaleImpl();
}

auto LocaleManager::setLocale(const std::string& locale) -> LocaleError {
    auto result = pImpl->setLocaleImpl(locale);
    if (result == LocaleError::None) {
        clearCache(); // Clear cache after successful locale change
    }
    return result;
}

auto LocaleManager::getAvailableLocales(bool useCache) -> std::vector<std::string> {
    if (useCache) {
        std::lock_guard<std::mutex> lock(cacheMutex);
        auto now = std::chrono::system_clock::now();

        if (cachedAvailableLocales.empty() || (now - lastLocaleListUpdate) > cacheTimeout) {
            spdlog::debug("Refreshing available locales cache");
            cachedAvailableLocales = pImpl->getAvailableLocalesImpl();
            lastLocaleListUpdate = now;
        }

        return cachedAvailableLocales;
    }

    return pImpl->getAvailableLocalesImpl();
}

auto LocaleManager::validateLocale(const std::string& locale) -> bool {
    return pImpl->validateLocaleImpl(locale);
}

auto LocaleManager::getDefaultLocale() -> std::string {
    return pImpl->getDefaultLocaleImpl();
}

auto LocaleManager::getPreferredLanguages() -> std::vector<std::string> {
    return pImpl->getPreferredLanguagesImpl();
}

void LocaleManager::clearCache() {
    std::lock_guard<std::mutex> lock(cacheMutex);
    cachedInfo.reset();
    cachedAvailableLocales.clear();
    spdlog::debug("Locale cache cleared");
}

void LocaleManager::setCacheTimeout(std::chrono::seconds timeout) {
    pImpl->cacheTimeout = timeout;
}

auto LocaleManager::registerChangeCallback(std::function<void(const std::string&, const std::string&)> callback) -> size_t {
    std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
    size_t id = pImpl->nextCallbackId++;
    pImpl->callbacks[id] = std::move(callback);
    return id;
}

void LocaleManager::unregisterChangeCallback(size_t id) {
    std::lock_guard<std::mutex> lock(pImpl->callbackMutex);
    pImpl->callbacks.erase(id);
}

auto LocaleManager::getPreferences() -> LocalePreferences {
    return pImpl->preferences;
}

void LocaleManager::setPreferences(const LocalePreferences& preferences) {
    pImpl->preferences = preferences;
}

// LocaleFormatter implementation
class LocaleFormatter::Impl {
public:
    std::string currentLocale;

    explicit Impl(const std::string& locale) : currentLocale(locale) {
        if (currentLocale.empty()) {
            currentLocale = getSystemLanguageInfo().localeName;
        }
    }

    auto formatNumberImpl(double number, int precision) -> std::string {
        std::string result;
#ifdef _WIN32
        result = locale::windows::formatNumber(number, currentLocale);
#elif defined(__APPLE__)
        result = locale::macos::formatNumber(number, currentLocale);
#else
        result = locale::linux_impl::formatNumber(number, currentLocale);
#endif

        // Apply precision if specified
        if (precision >= 0) {
            size_t dotPos = result.find('.');
            if (dotPos != std::string::npos) {
                if (precision == 0) {
                    result = result.substr(0, dotPos);
                } else {
                    size_t endPos = std::min(dotPos + 1 + precision, result.length());
                    result = result.substr(0, endPos);
                    // Pad with zeros if needed
                    while (result.length() < dotPos + 1 + precision) {
                        result += '0';
                    }
                }
            } else if (precision > 0) {
                result += '.';
                for (int i = 0; i < precision; ++i) {
                    result += '0';
                }
            }
        }

        return result;
    }

    auto formatCurrencyImpl(double amount, const std::string& currencyCode) -> std::string {
        std::string result;
#ifdef _WIN32
        result = locale::windows::formatCurrency(amount, currentLocale);
#elif defined(__APPLE__)
        result = locale::macos::formatCurrency(amount, currentLocale);
#else
        result = locale::linux_impl::formatCurrency(amount, currentLocale);
#endif

        // Override currency code if specified
        if (!currencyCode.empty()) {
            auto [symbol, code] = getCurrencyInfoForLocale(currentLocale);
            // Replace the currency symbol/code in the result
            // This is a simplified implementation
            if (!symbol.empty() && result.find(symbol) != std::string::npos) {
                size_t pos = result.find(symbol);
                result.replace(pos, symbol.length(), currencyCode);
            }
        }

        return result;
    }

    auto getCurrencyInfoForLocale(const std::string& locale) -> std::pair<std::string, std::string> {
#ifdef _WIN32
        return locale::windows::getCurrencyInfo(locale);
#elif defined(__APPLE__)
        return locale::macos::getCurrencyInfo(locale);
#else
        return locale::linux_impl::getCurrencyInfo(locale);
#endif
    }

    auto formatDateImpl(const std::chrono::system_clock::time_point& timestamp, const std::string& style) -> std::string {
#ifdef _WIN32
        return locale::windows::formatDate(timestamp, currentLocale, style);
#elif defined(__APPLE__)
        return locale::macos::formatDate(timestamp, currentLocale, style);
#else
        return locale::linux_impl::formatDate(timestamp, currentLocale, style);
#endif
    }

    auto formatTimeImpl(const std::chrono::system_clock::time_point& timestamp, const std::string& style) -> std::string {
#ifdef _WIN32
        return locale::windows::formatTime(timestamp, currentLocale, style);
#elif defined(__APPLE__)
        return locale::macos::formatTime(timestamp, currentLocale, style);
#else
        return locale::linux_impl::formatTime(timestamp, currentLocale, style);
#endif
    }
};

LocaleFormatter::LocaleFormatter(const std::string& locale) : pImpl(std::make_unique<Impl>(locale)) {}
LocaleFormatter::~LocaleFormatter() = default;

void LocaleFormatter::setLocale(const std::string& locale) {
    pImpl->currentLocale = locale;
}

auto LocaleFormatter::getLocale() const -> std::string {
    return pImpl->currentLocale;
}

auto LocaleFormatter::formatNumber(double number, int precision) -> std::string {
    return pImpl->formatNumberImpl(number, precision);
}

auto LocaleFormatter::formatCurrency(double amount, const std::string& currencyCode) -> std::string {
    return pImpl->formatCurrencyImpl(amount, currencyCode);
}

auto LocaleFormatter::formatPercentage(double value, int precision) -> std::string {
    return formatNumber(value * 100, precision) + "%";
}

auto LocaleFormatter::formatDate(const std::chrono::system_clock::time_point& timestamp, const std::string& style) -> std::string {
    return pImpl->formatDateImpl(timestamp, style);
}

auto LocaleFormatter::formatTime(const std::chrono::system_clock::time_point& timestamp, const std::string& style) -> std::string {
    return pImpl->formatTimeImpl(timestamp, style);
}

auto LocaleFormatter::formatDateTime(const std::chrono::system_clock::time_point& timestamp,
                                     const std::string& dateStyle, const std::string& timeStyle) -> std::string {
    return formatDate(timestamp, dateStyle) + " " + formatTime(timestamp, timeStyle);
}

auto LocaleFormatter::formatList(const std::vector<std::string>& items) -> std::string {
    if (items.empty()) return "";
    if (items.size() == 1) return items[0];

    // Get locale-specific list separator
    std::string separator = ", ";
    std::string finalSeparator = " and ";

    // Customize separators based on locale
    auto localeInfo = locale::parseLocaleString(pImpl->currentLocale);
    if (localeInfo.languageCode == "de") {
        finalSeparator = " und ";
    } else if (localeInfo.languageCode == "fr") {
        finalSeparator = " et ";
    } else if (localeInfo.languageCode == "es") {
        finalSeparator = " y ";
    } else if (localeInfo.languageCode == "it") {
        finalSeparator = " e ";
    } else if (localeInfo.languageCode == "pt") {
        finalSeparator = " e ";
    } else if (localeInfo.languageCode == "ru") {
        finalSeparator = " и ";
    } else if (localeInfo.languageCode == "ja") {
        separator = "、";
        finalSeparator = "と";
    } else if (localeInfo.languageCode == "zh") {
        separator = "、";
        finalSeparator = "和";
    }

    std::string result;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) {
            if (i == items.size() - 1) {
                result += finalSeparator;
            } else {
                result += separator;
            }
        }
        result += items[i];
    }
    return result;
}

// LocaleDetector implementation
auto LocaleDetector::detectSystemLocale(const std::string& fallback) -> std::string {
    try {
        LocaleManager manager;
        return manager.getDefaultLocale();
    } catch (...) {
        return fallback;
    }
}

auto LocaleDetector::detectFromEnvironment() -> std::string {
#ifdef _WIN32
    return locale::windows::getDefaultLocale();
#elif defined(__APPLE__)
    return locale::macos::getDefaultLocale();
#else
    return locale::linux_impl::getLocaleFromEnvironment();
#endif
}

auto LocaleDetector::detectFromSystemConfig() -> std::string {
#ifdef __linux__
    return locale::linux_impl::getSystemLocaleFromConfig();
#else
    return detectFromEnvironment();
#endif
}

auto LocaleDetector::findBestMatch(const std::string& preferred, const std::vector<std::string>& available) -> std::string {
    if (available.empty()) return preferred;

    // Exact match
    if (std::find(available.begin(), available.end(), preferred) != available.end()) {
        return preferred;
    }

    auto preferredInfo = locale::parseLocaleString(preferred);

    // Try to find exact language and country match
    for (const auto& locale : available) {
        auto info = locale::parseLocaleString(locale);
        if (info.languageCode == preferredInfo.languageCode &&
            info.countryCode == preferredInfo.countryCode) {
            return locale;
        }
    }

    // Try to find language match with UTF-8 encoding
    for (const auto& locale : available) {
        auto info = locale::parseLocaleString(locale);
        if (info.languageCode == preferredInfo.languageCode &&
            info.characterEncoding == "UTF-8") {
            return locale;
        }
    }

    // Try to find any language match
    for (const auto& locale : available) {
        auto info = locale::parseLocaleString(locale);
        if (info.languageCode == preferredInfo.languageCode) {
            return locale;
        }
    }

    // Try to find compatible language (e.g., en_GB for en_US)
    std::vector<std::string> compatibleLanguages;
    if (preferredInfo.languageCode == "en") {
        compatibleLanguages = {"en_US", "en_GB", "en_CA", "en_AU"};
    } else if (preferredInfo.languageCode == "es") {
        compatibleLanguages = {"es_ES", "es_MX", "es_AR"};
    } else if (preferredInfo.languageCode == "pt") {
        compatibleLanguages = {"pt_PT", "pt_BR"};
    } else if (preferredInfo.languageCode == "zh") {
        compatibleLanguages = {"zh_CN", "zh_TW", "zh_HK"};
    }

    for (const auto& compatibleLang : compatibleLanguages) {
        if (std::find(available.begin(), available.end(), compatibleLang) != available.end()) {
            return compatibleLang;
        }
        // Try with UTF-8 encoding
        std::string withEncoding = compatibleLang + ".UTF-8";
        if (std::find(available.begin(), available.end(), withEncoding) != available.end()) {
            return withEncoding;
        }
    }

    // Fallback to English if available
    std::vector<std::string> englishFallbacks = {"en_US.UTF-8", "en_US", "en_GB.UTF-8", "en_GB", "C.UTF-8", "C", "POSIX"};
    for (const auto& fallback : englishFallbacks) {
        if (std::find(available.begin(), available.end(), fallback) != available.end()) {
            return fallback;
        }
    }

    // Last resort: return first available
    return available[0];
}

auto LocaleDetector::getConfidenceScore(const std::string& locale) -> double {
    if (locale.empty()) return 0.0;

    double score = 0.0;

    // Check if locale format is valid
    if (locale::isValidLocaleFormat(locale)) {
        score += 0.3;
    }

    // Check if locale is in our known aliases
    auto aliases = locale::getLocaleAliases();
    if (aliases.find(locale) != aliases.end()) {
        score += 0.2;
    }

    // Try to validate the locale
    try {
        LocaleManager manager;
        if (manager.validateLocale(locale)) {
            score += 0.5; // High confidence for validated locales
        } else {
            // Check if it's a partial match
            auto info = locale::parseLocaleString(locale);
            if (!info.languageCode.empty()) {
                score += 0.1; // Some confidence for parseable locales
            }
        }
    } catch (...) {
        // Ignore validation errors, but don't add to score
    }

    // Bonus for UTF-8 encoding
    if (locale.find("UTF-8") != std::string::npos || locale.find("utf8") != std::string::npos) {
        score += 0.1;
    }

    // Bonus for common locales
    std::vector<std::string> commonLocales = {
        "en_US", "en_GB", "de_DE", "fr_FR", "es_ES", "it_IT", "ja_JP", "ko_KR",
        "zh_CN", "zh_TW", "ru_RU", "pt_PT", "pt_BR", "ar_SA", "hi_IN"
    };

    auto info = locale::parseLocaleString(locale);
    std::string baseLocale = info.languageCode;
    if (!info.countryCode.empty()) {
        baseLocale += "_" + info.countryCode;
    }

    if (std::find(commonLocales.begin(), commonLocales.end(), baseLocale) != commonLocales.end()) {
        score += 0.1;
    }

    return std::min(1.0, score); // Cap at 1.0
}

// Backward compatibility functions
auto getSystemLanguageInfo() -> LocaleInfo {
    LocaleManager manager;
    return manager.getCurrentLocale();
}

void printLocaleInfo([[maybe_unused]] const LocaleInfo& info) {
#ifdef ATOM_ENABLE_DEBUG
    spdlog::info("Printing locale information");
    spdlog::info("Language code (ISO 639): {}", info.languageCode);
    spdlog::info("Country code (ISO 3166): {}", info.countryCode);
    spdlog::info("Full locale name: {}", info.localeName);
    spdlog::info("Language display name: {}", info.languageDisplayName);
    spdlog::info("Country display name: {}", info.countryDisplayName);
    spdlog::info("Currency symbol: {}", info.currencySymbol);
    spdlog::info("Currency code: {}", info.currencyCode);
    spdlog::info("Decimal symbol: {}", info.decimalSymbol);
    spdlog::info("Thousand separator: {}", info.thousandSeparator);
    spdlog::info("Date format: {}", info.dateFormat);
    spdlog::info("Time format: {}", info.timeFormat);
    spdlog::info("Character encoding: {}", info.characterEncoding);
    spdlog::info("Is RTL: {}", info.isRTL);
    spdlog::info("Number format: {}", info.numberFormat);
    spdlog::info("Measurement system: {}", locale::measurementSystemToString(info.measurementSystem));
    spdlog::info("Paper size: {}", locale::paperSizeToString(info.paperSize));
    spdlog::info("Time zone: {}", info.timeZone);
    spdlog::info("First day of week: {}", info.firstDayOfWeek);
#endif
}

auto validateLocale(const std::string& locale) -> bool {
    LocaleManager manager;
    return manager.validateLocale(locale);
}

auto setSystemLocale(const std::string& locale) -> LocaleError {
    LocaleManager manager;
    return manager.setLocale(locale);
}

auto getAvailableLocales(bool useCache) -> std::vector<std::string> {
    LocaleManager manager;
    return manager.getAvailableLocales(useCache);
}

auto getDefaultLocale() -> std::string {
    LocaleManager manager;
    return manager.getDefaultLocale();
}

auto getCachedLocaleInfo() -> const LocaleInfo& {
    static LocaleManager manager;
    static LocaleInfo cached = manager.getCurrentLocale(true);
    return cached;
}

void clearLocaleCache() {
    LocaleManager manager;
    manager.clearCache();
}

} // namespace atom::system
