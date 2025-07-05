#include "atom/extra/boost/locale.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace atom::extra::boost;

int main() {
    // Create a LocaleWrapper instance with a specific locale
    LocaleWrapper localeWrapper("en_US.UTF-8");

    // Convert a string to UTF-8 encoding
    std::string originalStr = "Hello, World!";
    std::string utf8Str = LocaleWrapper::toUtf8(originalStr, "ISO-8859-1");
    std::cout << "UTF-8 encoded string: " << utf8Str << std::endl;

    // Convert a UTF-8 encoded string to another character set
    std::string convertedStr = LocaleWrapper::fromUtf8(utf8Str, "ISO-8859-1");
    std::cout << "Converted string: " << convertedStr << std::endl;

    // Normalize a Unicode string
    std::string unicodeStr = "\u00E9";  // é in NFC
    std::string normalizedStr =
        LocaleWrapper::normalize(unicodeStr, boost::locale::norm_nfd);
    std::cout << "Normalized string: " << normalizedStr << std::endl;

    // Tokenize a string into words
    std::string text = "Boost.Locale is a great library!";
    std::vector<std::string> tokens = LocaleWrapper::tokenize(text);
    std::cout << "Tokens: ";
    for (const auto& token : tokens) {
        std::cout << token << " ";
    }
    std::cout << std::endl;

    // Translate a string to the specified locale
    std::string translatedStr =
        LocaleWrapper::translate("Hello, World!", "", "fr_FR.UTF-8");
    std::cout << "Translated string: " << translatedStr << std::endl;

    // Convert a string to uppercase
    std::string upperStr = localeWrapper.toUpper("hello");
    std::cout << "Uppercase string: " << upperStr << std::endl;

    // Convert a string to lowercase
    std::string lowerStr = localeWrapper.toLower("HELLO");
    std::cout << "Lowercase string: " << lowerStr << std::endl;

    // Convert a string to title case
    std::string titleStr = localeWrapper.toTitle("hello world");
    std::cout << "Title case string: " << titleStr << std::endl;

    // Compare two strings using locale-specific collation rules
    int comparisonResult = localeWrapper.compare("apple", "banana");
    std::cout << "Comparison result: " << comparisonResult << std::endl;

    // Format a date and time according to the specified format
    boost::posix_time::ptime dateTime(
        boost::posix_time::second_clock::local_time());
    std::string formattedDate =
        LocaleWrapper::formatDate(dateTime, "%Y-%m-%d %H:%M:%S");
    std::cout << "Formatted date: " << formattedDate << std::endl;

    // Format a number with the specified precision
    double number = 12345.6789;
    std::string formattedNumber = LocaleWrapper::formatNumber(number, 2);
    std::cout << "Formatted number: " << formattedNumber << std::endl;

    // Format a currency amount
    double amount = 12345.67;
    std::string formattedCurrency =
        LocaleWrapper::formatCurrency(amount, "USD");
    std::cout << "Formatted currency: " << formattedCurrency << std::endl;

    // Replace occurrences of a regex pattern in a string with a format string
    std::string regexStr = "Boost.Locale is great!";
    boost::regex regexPattern("great");
    std::string replacedStr =
        LocaleWrapper::regexReplace(regexStr, regexPattern, "awesome");
    std::cout << "Replaced string: " << replacedStr << std::endl;

    // Format a string with named arguments
    std::string formatStr = "Hello, {1}! Today is {2}.";
    std::string formattedStr =
        localeWrapper.format(formatStr, "Alice", formattedDate);
    std::cout << "Formatted string: " << formattedStr << std::endl;

    return 0;
}
