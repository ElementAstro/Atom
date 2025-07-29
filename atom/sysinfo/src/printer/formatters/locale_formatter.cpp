#include "locale_formatter.hpp"
#include <spdlog/spdlog.h>
#include <format>
#include <sstream>
#include <unordered_map>

namespace atom::system {

LocaleInfoFormatter::LocaleInfoFormatter(const FormatterOptions& options) {
    setOptions(options);
}

auto LocaleInfoFormatter::format(const LocaleInfo& info) const -> std::string {
    return format(info, "Locale Information");
}

auto LocaleInfoFormatter::format(const LocaleInfo& info, const std::string& title) const -> std::string {
    std::stringstream ss;

    ss << createTableHeader(title);

    // Language information
    if (options_.style == FormatterStyle::VERBOSE) {
        ss << createTableRow("Language", formatLanguageWithFlag(info.languageCode, info.languageDisplayName));
    } else {
        ss << createTableRow("Language", info.languageDisplayName);
    }
    ss << createTableRow("Language Code", info.languageCode);

    // Country information
    if (options_.style == FormatterStyle::VERBOSE) {
        ss << createTableRow("Country", formatCountryWithFlag(info.countryCode, info.countryDisplayName));
    } else {
        ss << createTableRow("Country", info.countryDisplayName);
    }
    ss << createTableRow("Country Code", info.countryCode);

    // Character encoding
    if (options_.style == FormatterStyle::DETAILED || options_.style == FormatterStyle::VERBOSE) {
        ss << createTableRow("Character Encoding", formatEncodingWithDescription(info.characterEncoding));
    } else {
        ss << createTableRow("Character Encoding", info.characterEncoding);
    }

    // Time and date formats
    ss << createTableRow("Time Format", info.timeFormat);
    ss << createTableRow("Date Format", info.dateFormat);

    // Additional details for detailed/verbose styles
    if (options_.style == FormatterStyle::DETAILED || options_.style == FormatterStyle::VERBOSE) {
        if (!info.numberFormat.empty()) {
            ss << createTableRow("Number Format", info.numberFormat);
        }
        if (!info.currencySymbol.empty()) {
            ss << createTableRow("Currency Symbol", info.currencySymbol);
        }
        if (!info.decimalSymbol.empty()) {
            ss << createTableRow("Decimal Symbol", info.decimalSymbol);
        }
        if (!info.thousandSeparator.empty()) {
            ss << createTableRow("Thousands Separator", info.thousandSeparator);
        }
    }

    // Verbose mode additional information
    if (options_.style == FormatterStyle::VERBOSE) {
        ss << createTableRow("Measurement System", locale::measurementSystemToString(info.measurementSystem));
        ss << createTableRow("Paper Size", locale::paperSizeToString(info.paperSize));
        ss << createTableRow("Right-to-Left", info.isRTL ? "Yes" : "No");
    }

    ss << createTableFooter();

    return addTimestamp(ss.str());
}

auto LocaleInfoFormatter::formatBasic(const LocaleInfo& info) const -> std::string {
    return std::format("{} ({}), {} ({})",
                      info.languageDisplayName,
                      info.languageCode,
                      info.countryDisplayName,
                      info.countryCode);
}

auto LocaleInfoFormatter::formatRegional(const LocaleInfo& info) const -> std::string {
    std::stringstream ss;

    ss << createTableHeader("Regional Settings");
    ss << createTableRow("Language", info.languageDisplayName);
    ss << createTableRow("Country", info.countryDisplayName);
    ss << createTableRow("Character Encoding", info.characterEncoding);

    ss << createTableRow("Measurement System", locale::measurementSystemToString(info.measurementSystem));
    ss << createTableRow("Paper Size", locale::paperSizeToString(info.paperSize));

    ss << createTableFooter();

    return ss.str();
}

auto LocaleInfoFormatter::formatPreferences(const LocaleInfo& info) const -> std::string {
    std::stringstream ss;

    ss << createTableHeader("Formatting Preferences");
    ss << createTableRow("Time Format", info.timeFormat);
    ss << createTableRow("Date Format", info.dateFormat);

    if (!info.numberFormat.empty()) {
        ss << createTableRow("Number Format", info.numberFormat);
    }
    if (!info.currencySymbol.empty()) {
        ss << createTableRow("Currency Symbol", info.currencySymbol);
    }
    if (!info.decimalSymbol.empty()) {
        ss << createTableRow("Decimal Symbol", info.decimalSymbol);
    }
    if (!info.thousandSeparator.empty()) {
        ss << createTableRow("Thousands Separator", info.thousandSeparator);
    }

    ss << createTableFooter();

    return ss.str();
}

auto LocaleInfoFormatter::formatLanguageWithFlag(const std::string& languageCode,
                                            const std::string& displayName) const -> std::string {
    // For now, just return the display name
    // In a full implementation, you could add language-specific icons or flags
    return displayName;
}

auto LocaleInfoFormatter::formatCountryWithFlag(const std::string& countryCode,
                                           const std::string& displayName) const -> std::string {
    if (options_.style == FormatterStyle::VERBOSE) {
        auto flag = getFlagEmoji(countryCode);
        if (!flag.empty()) {
            return flag + " " + displayName;
        }
    }
    return displayName;
}

auto LocaleInfoFormatter::getFlagEmoji(const std::string& countryCode) const -> std::string {
    // Simple flag emoji mapping for common countries
    static const std::unordered_map<std::string, std::string> flagMap = {
        {"US", "🇺🇸"}, {"GB", "🇬🇧"}, {"CA", "🇨🇦"}, {"AU", "🇦🇺"},
        {"DE", "🇩🇪"}, {"FR", "🇫🇷"}, {"IT", "🇮🇹"}, {"ES", "🇪🇸"},
        {"JP", "🇯🇵"}, {"CN", "🇨🇳"}, {"KR", "🇰🇷"}, {"IN", "🇮🇳"},
        {"BR", "🇧🇷"}, {"MX", "🇲🇽"}, {"RU", "🇷🇺"}, {"NL", "🇳🇱"},
        {"SE", "🇸🇪"}, {"NO", "🇳🇴"}, {"DK", "🇩🇰"}, {"FI", "🇫🇮"}
    };

    auto it = flagMap.find(countryCode);
    return (it != flagMap.end()) ? it->second : "";
}

auto LocaleInfoFormatter::formatEncodingWithDescription(const std::string& encoding) const -> std::string {
    static const std::unordered_map<std::string, std::string> encodingDescriptions = {
        {"UTF-8", "UTF-8 (Unicode Transformation Format 8-bit)"},
        {"UTF-16", "UTF-16 (Unicode Transformation Format 16-bit)"},
        {"UTF-32", "UTF-32 (Unicode Transformation Format 32-bit)"},
        {"ASCII", "ASCII (American Standard Code for Information Interchange)"},
        {"ISO-8859-1", "ISO-8859-1 (Latin-1)"},
        {"ISO-8859-15", "ISO-8859-15 (Latin-9)"},
        {"Windows-1252", "Windows-1252 (Western European)"},
        {"CP1252", "CP1252 (Windows Western European)"}
    };

    auto it = encodingDescriptions.find(encoding);
    if (it != encodingDescriptions.end()) {
        return it->second;
    }

    return encoding;
}

} // namespace atom::system