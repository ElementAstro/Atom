/**
 * @file locale_formatter.hpp
 * @brief Locale information formatter
 *
 * This file contains the LocaleFormatter class for formatting locale information
 * into human-readable text with various styling and output format options.
 *
 * @copyright Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef ATOM_SYSINFO_PRINTER_FORMATTERS_LOCALE_FORMATTER_HPP
#define ATOM_SYSINFO_PRINTER_FORMATTERS_LOCALE_FORMATTER_HPP

#include "base_formatter.hpp"
#include "../../locale.hpp"

namespace atom::system {

/**
 * @class LocaleFormatter
 * @brief Formatter for locale information
 *
 * This class provides specialized formatting for locale information including
 * language, country, character encoding, time/date formats, and currency.
 */
class LocaleFormatter : public BaseFormatter {
public:
    /**
     * @brief Default constructor
     */
    LocaleFormatter() = default;

    /**
     * @brief Constructor with options
     * @param options Initial formatting options
     */
    explicit LocaleFormatter(const FormatterOptions& options);

    /**
     * @brief Format locale information as a string
     * @param info The locale information to format
     * @return A formatted string containing locale details
     */
    [[nodiscard]] auto format(const LocaleInfo& info) const -> std::string;

    /**
     * @brief Format locale information with custom title
     * @param info The locale information to format
     * @param title Custom title for the section
     * @return A formatted string containing locale details
     */
    [[nodiscard]] auto format(const LocaleInfo& info, const std::string& title) const -> std::string;

    /**
     * @brief Format only basic locale information
     * @param info The locale information to format
     * @return A formatted string with basic locale details
     */
    [[nodiscard]] auto formatBasic(const LocaleInfo& info) const -> std::string;

    /**
     * @brief Format regional settings
     * @param info The locale information to format
     * @return A formatted string with regional settings
     */
    [[nodiscard]] auto formatRegional(const LocaleInfo& info) const -> std::string;

    /**
     * @brief Format formatting preferences
     * @param info The locale information to format
     * @return A formatted string with formatting preferences
     */
    [[nodiscard]] auto formatPreferences(const LocaleInfo& info) const -> std::string;

private:
    /**
     * @brief Format language information with flag emoji
     * @param languageCode The language code
     * @param displayName The display name
     * @return Formatted language string with emoji if available
     */
    [[nodiscard]] auto formatLanguageWithFlag(const std::string& languageCode,
                                             const std::string& displayName) const -> std::string;

    /**
     * @brief Format country information with flag emoji
     * @param countryCode The country code
     * @param displayName The display name
     * @return Formatted country string with emoji if available
     */
    [[nodiscard]] auto formatCountryWithFlag(const std::string& countryCode,
                                            const std::string& displayName) const -> std::string;

    /**
     * @brief Get flag emoji for country code
     * @param countryCode The ISO country code
     * @return Unicode flag emoji or empty string
     */
    [[nodiscard]] auto getFlagEmoji(const std::string& countryCode) const -> std::string;

    /**
     * @brief Format character encoding with description
     * @param encoding The character encoding
     * @return Formatted encoding string with description
     */
    [[nodiscard]] auto formatEncodingWithDescription(const std::string& encoding) const -> std::string;
};

} // namespace atom::system

#endif // ATOM_SYSINFO_PRINTER_FORMATTERS_LOCALE_FORMATTER_HPP