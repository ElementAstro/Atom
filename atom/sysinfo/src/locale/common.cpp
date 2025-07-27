#include "common.hpp"

#include <algorithm>
#include <regex>
#include <unordered_set>
#include <mutex>
#include <atomic>

namespace atom::system::locale {

namespace {
    // Static data for callbacks
    std::mutex callbackMutex;
    std::unordered_map<size_t, LocaleChangeCallback> callbacks;
    std::atomic<size_t> nextCallbackId{1};

    // Locale aliases mapping
    const std::unordered_map<std::string, std::string> LOCALE_ALIASES = {
        {"C", "en_US"},
        {"POSIX", "en_US"},
        {"en", "en_US"},
        {"en_GB", "en_GB.UTF-8"},
        {"en_US", "en_US.UTF-8"},
        {"de", "de_DE"},
        {"de_DE", "de_DE.UTF-8"},
        {"fr", "fr_FR"},
        {"fr_FR", "fr_FR.UTF-8"},
        {"es", "es_ES"},
        {"es_ES", "es_ES.UTF-8"},
        {"it", "it_IT"},
        {"it_IT", "it_IT.UTF-8"},
        {"ja", "ja_JP"},
        {"ja_JP", "ja_JP.UTF-8"},
        {"ko", "ko_KR"},
        {"ko_KR", "ko_KR.UTF-8"},
        {"zh", "zh_CN"},
        {"zh_CN", "zh_CN.UTF-8"},
        {"zh_TW", "zh_TW.UTF-8"},
        {"ru", "ru_RU"},
        {"ru_RU", "ru_RU.UTF-8"},
        {"pt", "pt_PT"},
        {"pt_PT", "pt_PT.UTF-8"},
        {"pt_BR", "pt_BR.UTF-8"},
        {"ar", "ar_SA"},
        {"ar_SA", "ar_SA.UTF-8"},
        {"hi", "hi_IN"},
        {"hi_IN", "hi_IN.UTF-8"}
    };
}

auto localeErrorToString(LocaleError error) -> std::string {
    switch (error) {
        case LocaleError::None:
            return "No error";
        case LocaleError::InvalidLocale:
            return "Invalid locale";
        case LocaleError::SystemError:
            return "System error";
        case LocaleError::UnsupportedPlatform:
            return "Unsupported platform";
        case LocaleError::CacheError:
            return "Cache error";
        case LocaleError::FormatError:
            return "Format error";
        case LocaleError::DetectionError:
            return "Detection error";
        default:
            return "Unknown error";
    }
}

auto measurementSystemToString(MeasurementSystem system) -> std::string {
    switch (system) {
        case MeasurementSystem::Metric:
            return "metric";
        case MeasurementSystem::Imperial:
            return "imperial";
        case MeasurementSystem::US:
            return "us";
        default:
            return "metric";
    }
}

auto stringToMeasurementSystem(const std::string& str) -> std::optional<MeasurementSystem> {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower == "metric") {
        return MeasurementSystem::Metric;
    } else if (lower == "imperial") {
        return MeasurementSystem::Imperial;
    } else if (lower == "us" || lower == "us_customary") {
        return MeasurementSystem::US;
    }
    return std::nullopt;
}

auto paperSizeToString(PaperSize size) -> std::string {
    switch (size) {
        case PaperSize::A4:
            return "A4";
        case PaperSize::Letter:
            return "Letter";
        case PaperSize::Legal:
            return "Legal";
        case PaperSize::A3:
            return "A3";
        case PaperSize::A5:
            return "A5";
        case PaperSize::Tabloid:
            return "Tabloid";
        default:
            return "A4";
    }
}

auto stringToPaperSize(const std::string& str) -> std::optional<PaperSize> {
    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    if (upper == "A4") {
        return PaperSize::A4;
    } else if (upper == "LETTER") {
        return PaperSize::Letter;
    } else if (upper == "LEGAL") {
        return PaperSize::Legal;
    } else if (upper == "A3") {
        return PaperSize::A3;
    } else if (upper == "A5") {
        return PaperSize::A5;
    } else if (upper == "TABLOID") {
        return PaperSize::Tabloid;
    }
    return std::nullopt;
}

auto parseLocaleString(const std::string& locale) -> LocaleInfo {
    LocaleInfo info;

    // Regular expression to parse locale string: language[_country][.encoding][@variant]
    std::regex localeRegex(R"(^([a-z]{2,3})(?:_([A-Z]{2}))?(?:\.([^@]+))?(?:@(.+))?$)");
    std::smatch matches;

    if (std::regex_match(locale, matches, localeRegex)) {
        info.languageCode = matches[1].str();
        info.countryCode = matches[2].matched ? matches[2].str() : "";
        info.characterEncoding = matches[3].matched ? matches[3].str() : "UTF-8";
        info.variantCode = matches[4].matched ? matches[4].str() : "";

        // Construct locale name
        info.localeName = info.languageCode;
        if (!info.countryCode.empty()) {
            info.localeName += "_" + info.countryCode;
        }
        if (!info.characterEncoding.empty() && info.characterEncoding != "UTF-8") {
            info.localeName += "." + info.characterEncoding;
        }
        if (!info.variantCode.empty()) {
            info.localeName += "@" + info.variantCode;
        }
    } else {
        // Fallback parsing
        info.localeName = locale;
        size_t underscorePos = locale.find('_');
        if (underscorePos != std::string::npos) {
            info.languageCode = locale.substr(0, underscorePos);
            size_t dotPos = locale.find('.', underscorePos);
            if (dotPos != std::string::npos) {
                info.countryCode = locale.substr(underscorePos + 1, dotPos - underscorePos - 1);
                info.characterEncoding = locale.substr(dotPos + 1);
            } else {
                info.countryCode = locale.substr(underscorePos + 1);
                info.characterEncoding = "UTF-8";
            }
        } else {
            info.languageCode = locale;
            info.characterEncoding = "UTF-8";
        }
    }

    return info;
}

auto isValidLocaleFormat(const std::string& locale) -> bool {
    if (locale.empty()) {
        return false;
    }

    // Check for basic locale format: language[_country][.encoding][@variant]
    std::regex localeRegex(R"(^[a-z]{2,3}(?:_[A-Z]{2})?(?:\.[^@]+)?(?:@.+)?$)");
    return std::regex_match(locale, localeRegex);
}

auto normalizeLocaleIdentifier(const std::string& locale) -> std::string {
    if (locale.empty()) {
        return "en_US.UTF-8";
    }

    // Remove any whitespace
    std::string normalized = locale;
    normalized.erase(std::remove_if(normalized.begin(), normalized.end(), ::isspace), normalized.end());

    // Convert to lowercase for language part, uppercase for country part
    size_t underscorePos = normalized.find('_');
    if (underscorePos != std::string::npos) {
        std::transform(normalized.begin(), normalized.begin() + underscorePos, normalized.begin(), ::tolower);
        size_t dotPos = normalized.find('.', underscorePos);
        size_t endPos = (dotPos != std::string::npos) ? dotPos : normalized.length();
        std::transform(normalized.begin() + underscorePos + 1, normalized.begin() + endPos,
                      normalized.begin() + underscorePos + 1, ::toupper);
    } else {
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    }

    return normalized;
}

auto getFallbackLocale(const std::string& locale) -> std::string {
    if (locale.empty()) {
        return "en_US.UTF-8";
    }

    // Extract language code
    size_t underscorePos = locale.find('_');
    if (underscorePos != std::string::npos) {
        std::string languageCode = locale.substr(0, underscorePos);

        // Common fallbacks for languages
        if (languageCode == "en") return "en_US.UTF-8";
        if (languageCode == "de") return "de_DE.UTF-8";
        if (languageCode == "fr") return "fr_FR.UTF-8";
        if (languageCode == "es") return "es_ES.UTF-8";
        if (languageCode == "it") return "it_IT.UTF-8";
        if (languageCode == "ja") return "ja_JP.UTF-8";
        if (languageCode == "ko") return "ko_KR.UTF-8";
        if (languageCode == "zh") return "zh_CN.UTF-8";
        if (languageCode == "ru") return "ru_RU.UTF-8";
        if (languageCode == "pt") return "pt_PT.UTF-8";
        if (languageCode == "ar") return "ar_SA.UTF-8";
        if (languageCode == "hi") return "hi_IN.UTF-8";

        // Default fallback with UTF-8 encoding
        return languageCode + "_" + languageCode + ".UTF-8";
    }

    return "en_US.UTF-8";
}

auto areLocalesCompatible(const std::string& locale1, const std::string& locale2) -> bool {
    if (locale1 == locale2) {
        return true;
    }

    // Extract language codes
    size_t pos1 = locale1.find('_');
    size_t pos2 = locale2.find('_');

    std::string lang1 = (pos1 != std::string::npos) ? locale1.substr(0, pos1) : locale1;
    std::string lang2 = (pos2 != std::string::npos) ? locale2.substr(0, pos2) : locale2;

    // Same language is considered compatible
    return lang1 == lang2;
}

auto getLocaleAliases() -> const std::unordered_map<std::string, std::string>& {
    return LOCALE_ALIASES;
}

auto resolveLocaleAlias(const std::string& alias) -> std::string {
    auto it = LOCALE_ALIASES.find(alias);
    return (it != LOCALE_ALIASES.end()) ? it->second : alias;
}

auto registerLocaleChangeCallback(LocaleChangeCallback callback) -> size_t {
    std::lock_guard<std::mutex> lock(callbackMutex);
    size_t id = nextCallbackId++;
    callbacks[id] = std::move(callback);
    return id;
}

void unregisterLocaleChangeCallback(size_t id) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    callbacks.erase(id);
}

void notifyLocaleChange(const std::string& oldLocale, const std::string& newLocale) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    for (const auto& [id, callback] : callbacks) {
        try {
            callback(oldLocale, newLocale);
        } catch (...) {
            // Ignore callback exceptions to prevent one bad callback from affecting others
        }
    }
}

bool LocaleInfo::operator==(const LocaleInfo& other) const {
    return languageCode == other.languageCode &&
           countryCode == other.countryCode &&
           localeName == other.localeName &&
           characterEncoding == other.characterEncoding;
}

bool LocaleInfo::operator!=(const LocaleInfo& other) const {
    return !(*this == other);
}

} // namespace atom::system::locale
