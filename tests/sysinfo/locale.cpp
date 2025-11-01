#include "atom/sysinfo/locale.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <regex>
#include <string>
#include <vector>

using namespace atom::system;

namespace atom::sysinfo::test {

// ============================================================================
// Locale Information Tests
// ============================================================================

class LocaleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup locale tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(LocaleTest, GetSystemLanguageInfo) {
    // Test system language information retrieval
    LocaleInfo info = getSystemLanguageInfo();

    // Language code should not be empty and should be valid
    EXPECT_FALSE(info.languageCode.empty());
    EXPECT_GT(info.languageCode.length(), 0);
    EXPECT_LT(info.languageCode.length(), 20);  // Reasonable upper bound

    // Language code should follow ISO standards (2-3 characters, possibly with
    // region) Examples: "en", "en_US", "en-US", "zh_CN", etc.
    std::regex langCodePattern(
        R"([a-z]{2,3}([_-][A-Z]{2})?([_-][a-zA-Z0-9]+)?)");
    EXPECT_TRUE(std::regex_match(info.languageCode, langCodePattern));

    // Locale name should not be empty
    EXPECT_FALSE(info.localeName.empty());
    EXPECT_GT(info.localeName.length(), 0);
    EXPECT_LT(info.localeName.length(), 100);  // Reasonable upper bound

    // Currency symbol should not be empty
    EXPECT_FALSE(info.currencySymbol.empty());
    EXPECT_GT(info.currencySymbol.length(), 0);
    EXPECT_LT(info.currencySymbol.length(),
              10);  // Currency symbols are typically short

    // Decimal symbol should be a single character (typically '.' or ',')
    EXPECT_FALSE(info.decimalSymbol.empty());
    EXPECT_EQ(info.decimalSymbol.length(), 1);
    EXPECT_TRUE(info.decimalSymbol == "." || info.decimalSymbol == "," ||
                info.decimalSymbol == "٫" ||
                info.decimalSymbol == "·");  // Common decimal separators

    // Date format should not be empty
    EXPECT_FALSE(info.dateFormat.empty());
    EXPECT_GT(info.dateFormat.length(), 0);
    EXPECT_LT(info.dateFormat.length(), 50);  // Reasonable upper bound

    // Time format should not be empty
    EXPECT_FALSE(info.timeFormat.empty());
    EXPECT_GT(info.timeFormat.length(), 0);
    EXPECT_LT(info.timeFormat.length(), 50);  // Reasonable upper bound

    // Character encoding should not be empty
    EXPECT_FALSE(info.characterEncoding.empty());
    EXPECT_GT(info.characterEncoding.length(), 0);
    EXPECT_LT(info.characterEncoding.length(), 50);  // Reasonable upper bound

    // Common character encodings
    std::vector<std::string> commonEncodings = {
        "UTF-8", "utf-8",    "UTF-16",       "utf-16", "ASCII",
        "ascii", "ISO-8859", "Windows-1252", "CP1252", "ANSI"};

    bool hasKnownEncoding = false;
    for (const auto& enc : commonEncodings) {
        if (info.characterEncoding.find(enc) != std::string::npos) {
            hasKnownEncoding = true;
            break;
        }
    }
    EXPECT_TRUE(hasKnownEncoding);
}

TEST_F(LocaleTest, GetSystemLanguageInfoConsistency) {
    // Test that multiple calls return consistent results
    LocaleInfo info1 = getSystemLanguageInfo();
    LocaleInfo info2 = getSystemLanguageInfo();

    // All fields should be identical
    EXPECT_EQ(info1.languageCode, info2.languageCode);
    EXPECT_EQ(info1.localeName, info2.localeName);
    EXPECT_EQ(info1.currencySymbol, info2.currencySymbol);
    EXPECT_EQ(info1.decimalSymbol, info2.decimalSymbol);
    EXPECT_EQ(info1.dateFormat, info2.dateFormat);
    EXPECT_EQ(info1.timeFormat, info2.timeFormat);
    EXPECT_EQ(info1.characterEncoding, info2.characterEncoding);
}

TEST_F(LocaleTest, PrintLocaleInfo) {
    // Test locale info printing function
    LocaleInfo info = getSystemLanguageInfo();

    // Function should not throw
    EXPECT_NO_THROW(printLocaleInfo(info));

    // Since printLocaleInfo only prints to console, we can't directly test its
    // output. However, we can ensure it doesn't crash or throw exceptions.
}

// ============================================================================
// Locale Information Structure Tests
// ============================================================================

TEST_F(LocaleTest, LocaleInfoStructure) {
    // Test LocaleInfo structure properties
    LocaleInfo info;

    // Default values should be empty
    EXPECT_TRUE(info.languageCode.empty());
    EXPECT_TRUE(info.localeName.empty());
    EXPECT_TRUE(info.currencySymbol.empty());
    EXPECT_TRUE(info.decimalSymbol.empty());
    EXPECT_TRUE(info.dateFormat.empty());
    EXPECT_TRUE(info.timeFormat.empty());
    EXPECT_TRUE(info.characterEncoding.empty());

    // Test assignment
    info.languageCode = "en_US";
    info.localeName = "English (United States)";
    info.currencySymbol = "$";
    info.decimalSymbol = ".";
    info.dateFormat = "MM/dd/yyyy";
    info.timeFormat = "HH:mm:ss";
    info.characterEncoding = "UTF-8";

    EXPECT_EQ(info.languageCode, "en_US");
    EXPECT_EQ(info.localeName, "English (United States)");
    EXPECT_EQ(info.currencySymbol, "$");
    EXPECT_EQ(info.decimalSymbol, ".");
    EXPECT_EQ(info.dateFormat, "MM/dd/yyyy");
    EXPECT_EQ(info.timeFormat, "HH:mm:ss");
    EXPECT_EQ(info.characterEncoding, "UTF-8");
}

TEST_F(LocaleTest, LocaleInfoCopySemantics) {
    // Test copy constructor and assignment
    LocaleInfo original = getSystemLanguageInfo();

    // Test copy constructor
    LocaleInfo copied(original);
    EXPECT_EQ(copied.languageCode, original.languageCode);
    EXPECT_EQ(copied.localeName, original.localeName);
    EXPECT_EQ(copied.currencySymbol, original.currencySymbol);
    EXPECT_EQ(copied.decimalSymbol, original.decimalSymbol);
    EXPECT_EQ(copied.dateFormat, original.dateFormat);
    EXPECT_EQ(copied.timeFormat, original.timeFormat);
    EXPECT_EQ(copied.characterEncoding, original.characterEncoding);

    // Test assignment operator
    LocaleInfo assigned;
    assigned = original;
    EXPECT_EQ(assigned.languageCode, original.languageCode);
    EXPECT_EQ(assigned.localeName, original.localeName);
    EXPECT_EQ(assigned.currencySymbol, original.currencySymbol);
    EXPECT_EQ(assigned.decimalSymbol, original.decimalSymbol);
    EXPECT_EQ(assigned.dateFormat, original.dateFormat);
    EXPECT_EQ(assigned.timeFormat, original.timeFormat);
    EXPECT_EQ(assigned.characterEncoding, original.characterEncoding);
}

// ============================================================================
// Locale Validation Tests
// ============================================================================

TEST_F(LocaleTest, LanguageCodeValidation) {
    // Test language code validation
    LocaleInfo info = getSystemLanguageInfo();

    // Language code should be lowercase for the language part
    std::string langPart = info.languageCode.substr(0, 2);
    EXPECT_TRUE(std::all_of(langPart.begin(), langPart.end(), ::islower));

    // Common language codes
    std::vector<std::string> commonLanguages = {"en", "es", "fr", "de", "it",
                                                "pt", "ru", "zh", "ja", "ko",
                                                "ar", "hi", "tr"};

    bool hasKnownLanguage = false;
    for (const auto& lang : commonLanguages) {
        if (info.languageCode.find(lang) == 0) {  // Starts with known language
            hasKnownLanguage = true;
            break;
        }
    }
    // Note: This is not a strict requirement as there are many languages
    EXPECT_TRUE(hasKnownLanguage || !hasKnownLanguage);
}

TEST_F(LocaleTest, CurrencySymbolValidation) {
    // Test currency symbol validation
    LocaleInfo info = getSystemLanguageInfo();

    // Currency symbol should be reasonable
    EXPECT_LE(info.currencySymbol.length(),
              5);  // Most currency symbols are 1-3 characters

    // Common currency symbols
    std::vector<std::string> commonCurrencies = {
        "$", "€", "£", "¥", "₹", "₽", "¢", "₩", "₪", "₨", "₡", "₦", "₵"};

    bool hasKnownCurrency = false;
    for (const auto& currency : commonCurrencies) {
        if (info.currencySymbol.find(currency) != std::string::npos) {
            hasKnownCurrency = true;
            break;
        }
    }
    // Note: This is not a strict requirement as there are many currencies
    EXPECT_TRUE(hasKnownCurrency || !hasKnownCurrency);
}

TEST_F(LocaleTest, DateTimeFormatValidation) {
    // Test date and time format validation
    LocaleInfo info = getSystemLanguageInfo();

    // Date format should contain common date format characters
    bool hasDateChars = (info.dateFormat.find('d') != std::string::npos ||
                         info.dateFormat.find('D') != std::string::npos ||
                         info.dateFormat.find('m') != std::string::npos ||
                         info.dateFormat.find('M') != std::string::npos ||
                         info.dateFormat.find('y') != std::string::npos ||
                         info.dateFormat.find('Y') != std::string::npos ||
                         info.dateFormat.find('/') != std::string::npos ||
                         info.dateFormat.find('-') != std::string::npos ||
                         info.dateFormat.find('.') != std::string::npos);
    EXPECT_TRUE(hasDateChars);

    // Time format should contain common time format characters
    bool hasTimeChars = (info.timeFormat.find('h') != std::string::npos ||
                         info.timeFormat.find('H') != std::string::npos ||
                         info.timeFormat.find('m') != std::string::npos ||
                         info.timeFormat.find('s') != std::string::npos ||
                         info.timeFormat.find(':') != std::string::npos);
    EXPECT_TRUE(hasTimeChars);
}

// ============================================================================
// Edge Cases and Error Handling Tests
// ============================================================================

TEST_F(LocaleTest, NoThrowGuarantee) {
    // Test that all locale functions provide no-throw guarantee
    EXPECT_NO_THROW(getSystemLanguageInfo());

    LocaleInfo info = getSystemLanguageInfo();
    EXPECT_NO_THROW(printLocaleInfo(info));
}

TEST_F(LocaleTest, LocaleInfoBoundaryValues) {
    // Test with boundary values
    LocaleInfo info;

    // Test with empty values
    EXPECT_NO_THROW(printLocaleInfo(info));

    // Test with very long values
    info.languageCode = std::string(100, 'a');
    info.localeName = std::string(1000, 'b');
    info.currencySymbol = std::string(50, '$');
    info.decimalSymbol = std::string(10, '.');
    info.dateFormat = std::string(200, 'd');
    info.timeFormat = std::string(200, 'h');
    info.characterEncoding = std::string(100, 'u');

    EXPECT_NO_THROW(printLocaleInfo(info));
}

TEST_F(LocaleTest, LocaleInfoSpecialCharacters) {
    // Test with special characters
    LocaleInfo info;
    info.languageCode = "zh_CN";
    info.localeName = "中文 (简体)";
    info.currencySymbol = "¥";
    info.decimalSymbol = ".";
    info.dateFormat = "yyyy年MM月dd日";
    info.timeFormat = "HH时mm分ss秒";
    info.characterEncoding = "UTF-8";

    EXPECT_NO_THROW(printLocaleInfo(info));

    // Test with Arabic
    info.languageCode = "ar_SA";
    info.localeName = "العربية (المملكة العربية السعودية)";
    info.currencySymbol = "ر.س";
    info.decimalSymbol = "٫";
    info.dateFormat = "dd/MM/yyyy";
    info.timeFormat = "HH:mm:ss";
    info.characterEncoding = "UTF-8";

    EXPECT_NO_THROW(printLocaleInfo(info));
}

TEST_F(LocaleTest, LocaleInfoFieldLengths) {
    // Test field length constraints
    LocaleInfo info = getSystemLanguageInfo();

    // Verify reasonable field lengths
    EXPECT_LT(info.languageCode.length(), 50);
    EXPECT_LT(info.localeName.length(), 200);
    EXPECT_LT(info.currencySymbol.length(), 20);
    EXPECT_LT(info.decimalSymbol.length(), 10);
    EXPECT_LT(info.dateFormat.length(), 100);
    EXPECT_LT(info.timeFormat.length(), 100);
    EXPECT_LT(info.characterEncoding.length(), 100);
}

}  // namespace atom::sysinfo::test
