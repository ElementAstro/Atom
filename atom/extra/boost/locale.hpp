#ifndef ATOM_EXTRA_BOOST_LOCALE_HPP
#define ATOM_EXTRA_BOOST_LOCALE_HPP

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/locale.hpp>
#include <boost/locale/encoding.hpp>
#include <boost/locale/generator.hpp>
#include <boost/regex.hpp>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace atom::extra::boost {

/**
 * @brief A wrapper class for Boost.Locale functionalities
 *
 * This class provides various utilities for string conversion, Unicode
 * normalization, tokenization, translation, case conversion, collation, date
 * and time formatting, number formatting, currency formatting, and regex
 * replacement using Boost.Locale.
 */
class LocaleWrapper {
public:
    /**
     * @brief Constructs a LocaleWrapper object with the specified locale
     * @param localeName The name of the locale to use. If empty, the global
     * locale is used
     */
    explicit LocaleWrapper(std::string_view localeName = "") {
        ::boost::locale::generator gen;
        std::locale::global(gen(std::string(localeName)));
        locale_ = std::locale();
    }

    /**
     * @brief Converts a string to UTF-8 encoding
     * @param str The string to convert
     * @param fromCharset The original character set of the string
     * @return The UTF-8 encoded string
     */
    [[nodiscard]] static std::string toUtf8(std::string_view str,
                                            std::string_view fromCharset) {
        return ::boost::locale::conv::to_utf<char>(std::string(str),
                                                   std::string(fromCharset));
    }

    /**
     * @brief Converts a UTF-8 encoded string to another character set
     * @param str The UTF-8 encoded string to convert
     * @param toCharset The target character set
     * @return The converted string
     */
    [[nodiscard]] static std::string fromUtf8(std::string_view str,
                                              std::string_view toCharset) {
        return ::boost::locale::conv::from_utf<char>(std::string(str),
                                                     std::string(toCharset));
    }

    /**
     * @brief Normalizes a Unicode string
     * @param str The string to normalize
     * @param norm The normalization form to use (default is NFC)
     * @return The normalized string
     */
    [[nodiscard]] static std::string normalize(
        std::string_view str,
        ::boost::locale::norm_type norm = ::boost::locale::norm_default) {
        return ::boost::locale::normalize(std::string(str), norm);
    }

    /**
     * @brief Tokenizes a string into words
     * @param str The string to tokenize
     * @param localeName The name of the locale to use for tokenization
     * @return A vector of tokens
     */
    [[nodiscard]] static std::vector<std::string> tokenize(
        std::string_view str, std::string_view localeName = "") {
        ::boost::locale::generator gen;
        std::locale loc = gen(std::string(localeName));
        std::string s(str);
        ::boost::locale::boundary::ssegment_index map(
            ::boost::locale::boundary::word, s.begin(), s.end(), loc);

        std::vector<std::string> tokens;
        tokens.reserve(32);  // Reserve space for common cases

        for (const auto& token : map) {
            if ((!token.str().empty())) [[likely]] {
                tokens.emplace_back(token.str());
            }
        }
        return tokens;
    }

    /**
     * @brief Translates a string to the specified locale
     * @param str The string to translate
     * @param domain The domain for the translation (not used in this
     * implementation)
     * @param localeName The name of the locale to use for translation
     * @return The translated string
     */
    [[nodiscard]] static std::string translate(
        std::string_view str, std::string_view /*domain*/,
        std::string_view localeName = "") {
        ::boost::locale::generator gen;
        std::locale loc = gen(std::string(localeName));
        return ::boost::locale::translate(std::string(str)).str(loc);
    }

    /**
     * @brief Converts a string to uppercase
     * @param str The string to convert
     * @return The uppercase string
     */
    [[nodiscard]] std::string toUpper(std::string_view str) const {
        return ::boost::locale::to_upper(std::string(str), locale_);
    }

    /**
     * @brief Converts a string to lowercase
     * @param str The string to convert
     * @return The lowercase string
     */
    [[nodiscard]] std::string toLower(std::string_view str) const {
        return ::boost::locale::to_lower(std::string(str), locale_);
    }

    /**
     * @brief Converts a string to title case
     * @param str The string to convert
     * @return The title case string
     */
    [[nodiscard]] std::string toTitle(std::string_view str) const {
        return ::boost::locale::to_title(std::string(str), locale_);
    }

    /**
     * @brief Compares two strings using locale-specific collation rules
     * @param str1 The first string to compare
     * @param str2 The second string to compare
     * @return An integer less than, equal to, or greater than zero if str1 is
     * found, respectively, to be less than, to match, or be greater than str2
     */
    [[nodiscard]] int compare(std::string_view str1,
                              std::string_view str2) const {
        return static_cast<int>(::boost::locale::comparator<
                                char, ::boost::locale::collate_level::primary>(
            locale_)(std::string(str1), std::string(str2)));
    }

    /**
     * @brief Formats a date and time according to the specified format
     * @param dateTime The date and time to format
     * @param format The format string
     * @return The formatted date and time string
     */
    [[nodiscard]] static std::string formatDate(
        const ::boost::posix_time::ptime& dateTime, std::string_view format) {
        std::ostringstream oss;
        oss.imbue(std::locale());
        oss << ::boost::locale::format(std::string(format)) % dateTime;
        return oss.str();
    }

    /**
     * @brief Formats a number with the specified precision
     * @param number The number to format
     * @param precision The number of decimal places
     * @return The formatted number string
     */
    [[nodiscard]] static std::string formatNumber(double number,
                                                  int precision = 2) {
        std::ostringstream oss;
        oss.imbue(std::locale());
        oss << std::fixed << std::setprecision(precision) << number;
        return oss.str();
    }

    /**
     * @brief Formats a currency amount
     * @param amount The amount to format
     * @param currency The currency code
     * @return The formatted currency string
     */
    [[nodiscard]] static std::string formatCurrency(double amount,
                                                    std::string_view currency) {
        std::ostringstream oss;
        oss.imbue(std::locale());
        oss << ::boost::locale::as::currency << std::string(currency) << amount;
        return oss.str();
    }

    /**
     * @brief Replaces occurrences of a regex pattern in a string with a format
     * string
     * @param str The string to search
     * @param regex The regex pattern to search for
     * @param format The format string to replace with
     * @return The resulting string after replacements
     */
    [[nodiscard]] static std::string regexReplace(std::string_view str,
                                                  const ::boost::regex& regex,
                                                  std::string_view format) {
        return ::boost::regex_replace(
            std::string(str), regex, std::string(format),
            ::boost::match_default | ::boost::format_all);
    }

    /**
     * @brief Formats a string with named arguments
     * @tparam Args The types of the arguments
     * @param formatString The format string
     * @param args The arguments to format
     * @return The formatted string
     */
    template <typename... Args>
    [[nodiscard]] std::string format(std::string_view formatString,
                                     Args&&... args) const {
        return (::boost::locale::format(std::string(formatString)) % ... %
                std::forward<Args>(args))
            .str(locale_);
    }

    /**
     * @brief Gets the current locale
     * @return The current locale
     */
    [[nodiscard]] const std::locale& getLocale() const noexcept {
        return locale_;
    }

    /**
     * @brief Sets a new locale
     * @param localeName The name of the new locale
     */
    void setLocale(std::string_view localeName) {
        ::boost::locale::generator gen;
        locale_ = gen(std::string(localeName));
    }

private:
    std::locale locale_;
    static constexpr std::size_t BUFFER_SIZE = 4096;
};

}  // namespace atom::extra::boost

#endif  // ATOM_EXTRA_BOOST_LOCALE_HPP
