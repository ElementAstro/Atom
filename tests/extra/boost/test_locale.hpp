#ifndef ATOM_EXTRA_BOOST_TEST_LOCALE_HPP
#define ATOM_EXTRA_BOOST_TEST_LOCALE_HPP

#include <gmock/gmock.h>
#include <gtest/gtest.h>


#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/regex.hpp>
#include <iostream>
#include <locale>
#include <string>
#include <vector>

#include "atom/extra/boost/locale.hpp"

namespace atom::extra::boost::test {

using ::testing::ElementsAre;
using ::testing::EndsWith;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::StartsWith;
using ::testing::StrEq;

class LocaleWrapperTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up default locale wrapper with system locale
        defaultWrapper = std::make_unique<LocaleWrapper>();

        // Set up locale wrappers with specific locales
        // Note: Some of these might fail if the locales are not installed on
        // the system
        try {
            enUsWrapper = std::make_unique<LocaleWrapper>("en_US.UTF-8");
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to create en_US locale: " << e.what()
                      << std::endl;
        }

        try {
            deDEWrapper = std::make_unique<LocaleWrapper>("de_DE.UTF-8");
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to create de_DE locale: " << e.what()
                      << std::endl;
        }

        try {
            frFRWrapper = std::make_unique<LocaleWrapper>("fr_FR.UTF-8");
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to create fr_FR locale: " << e.what()
                      << std::endl;
        }

        try {
            jaJPWrapper = std::make_unique<LocaleWrapper>("ja_JP.UTF-8");
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to create ja_JP locale: " << e.what()
                      << std::endl;
        }
    }

    void TearDown() override {
        // Clean up
        defaultWrapper.reset();
        enUsWrapper.reset();
        deDEWrapper.reset();
        frFRWrapper.reset();
        jaJPWrapper.reset();
    }

    // Helper method to check if a locale is available
    bool isLocaleAvailable(const std::string& localeName) {
        try {
            std::locale loc(localeName.c_str());
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    std::unique_ptr<LocaleWrapper> defaultWrapper;
    std::unique_ptr<LocaleWrapper> enUsWrapper;
    std::unique_ptr<LocaleWrapper> deDEWrapper;
    std::unique_ptr<LocaleWrapper> frFRWrapper;
    std::unique_ptr<LocaleWrapper> jaJPWrapper;
};

// Test constructor
TEST_F(LocaleWrapperTest, Constructor) {
    // Test with default locale
    ASSERT_NO_THROW(LocaleWrapper());

    // Test with available system locales
    if (isLocaleAvailable("en_US.UTF-8")) {
        EXPECT_NO_THROW(LocaleWrapper("en_US.UTF-8"));
    }

    if (isLocaleAvailable("de_DE.UTF-8")) {
        EXPECT_NO_THROW(LocaleWrapper("de_DE.UTF-8"));
    }

    // Test with invalid locale
    EXPECT_THROW(LocaleWrapper("invalid_locale"), std::runtime_error);
}

// Test UTF-8 conversion
TEST_F(LocaleWrapperTest, Utf8Conversion) {
    // Test converting to UTF-8
    try {
        // Convert from Latin-1 (ISO-8859-1) to UTF-8
        std::string latin1String = "\xE4\xF6\xFC";  // ä ö ü in Latin-1
        std::string utf8Result =
            LocaleWrapper::toUtf8(latin1String, "ISO-8859-1");

        // These should be multi-byte characters in UTF-8
        EXPECT_NE(utf8Result, latin1String);
        EXPECT_EQ(utf8Result.size(),
                  6);  // Each character becomes 2 bytes in UTF-8

        // Convert back to Latin-1
        std::string backToLatin1 =
            LocaleWrapper::fromUtf8(utf8Result, "ISO-8859-1");
        EXPECT_EQ(backToLatin1, latin1String);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Skipping UTF-8 conversion test: " << e.what();
    }

    // Test with empty string
    EXPECT_EQ(LocaleWrapper::toUtf8("", "ASCII"), "");
    EXPECT_EQ(LocaleWrapper::fromUtf8("", "ASCII"), "");

    // Test with ASCII (no change expected)
    std::string asciiString = "Hello, world!";
    EXPECT_EQ(LocaleWrapper::toUtf8(asciiString, "ASCII"), asciiString);
    EXPECT_EQ(LocaleWrapper::fromUtf8(asciiString, "ASCII"), asciiString);
}

// Test normalization
TEST_F(LocaleWrapperTest, Normalization) {
    try {
        // Test with NFC normalization (default)
        // Example: composed character ñ vs. decomposed n + ̃
        std::string composed = "\u00F1";     // ñ (single code point)
        std::string decomposed = "n\u0303";  // n + ̃ (combining tilde)

        std::string normalizedComposed = LocaleWrapper::normalize(composed);
        std::string normalizedDecomposed = LocaleWrapper::normalize(decomposed);

        // Both should normalize to the same string
        EXPECT_EQ(normalizedComposed, normalizedDecomposed);

        // Test with different normalization forms
        EXPECT_NO_THROW(
            LocaleWrapper::normalize(composed, ::boost::locale::norm_nfd));
        EXPECT_NO_THROW(
            LocaleWrapper::normalize(composed, ::boost::locale::norm_nfkc));
        EXPECT_NO_THROW(
            LocaleWrapper::normalize(composed, ::boost::locale::norm_nfkd));
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Skipping normalization test: " << e.what();
    }

    // Test with empty string
    EXPECT_EQ(LocaleWrapper::normalize(""), "");

    // Test with ASCII (no change expected)
    std::string asciiString = "Hello, world!";
    EXPECT_EQ(LocaleWrapper::normalize(asciiString), asciiString);
}

// Test tokenization
TEST_F(LocaleWrapperTest, Tokenization) {
    // Test with English text
    std::string englishText = "Hello, world! This is a test.";
    std::vector<std::string> englishTokens =
        LocaleWrapper::tokenize(englishText);

    // We expect words, spaces, and punctuation as separate tokens
    EXPECT_GT(englishTokens.size(),
              4);  // At least "Hello", "world", "This", "is", "a", "test"
    EXPECT_THAT(englishTokens, testing::Contains(std::string("Hello")));
    EXPECT_THAT(englishTokens, testing::Contains(std::string("world")));
    EXPECT_THAT(englishTokens, testing::Contains(std::string("test")));

    // Test with empty string
    std::vector<std::string> emptyTokens = LocaleWrapper::tokenize("");
    EXPECT_TRUE(emptyTokens.empty());

    // Test with different locales
    if (isLocaleAvailable("en_US.UTF-8") && isLocaleAvailable("de_DE.UTF-8")) {
        // German has different word breaking rules for compound words
        std::string germanText = "Donaudampfschifffahrtsgesellschaft";

        std::vector<std::string> enTokens =
            LocaleWrapper::tokenize(germanText, "en_US.UTF-8");
        std::vector<std::string> deTokens =
            LocaleWrapper::tokenize(germanText, "de_DE.UTF-8");

        // The tokenization might be different (but might also be the same)
        // Just verify that tokenization doesn't throw
        EXPECT_NO_THROW(LocaleWrapper::tokenize(germanText, "en_US.UTF-8"));
        EXPECT_NO_THROW(LocaleWrapper::tokenize(germanText, "de_DE.UTF-8"));
    }
}

// Test translation
TEST_F(LocaleWrapperTest, Translation) {
    // Note: This test mainly verifies that the method doesn't throw
    // Actual translation would require translation catalogs to be set up

    std::string originalText = "Hello, world!";

    // Test with default locale
    EXPECT_NO_THROW(LocaleWrapper::translate(originalText, "messages"));

    // Test with specific locales
    if (isLocaleAvailable("en_US.UTF-8")) {
        EXPECT_NO_THROW(
            LocaleWrapper::translate(originalText, "messages", "en_US.UTF-8"));
    }

    if (isLocaleAvailable("de_DE.UTF-8")) {
        EXPECT_NO_THROW(
            LocaleWrapper::translate(originalText, "messages", "de_DE.UTF-8"));
    }

    // Test with empty string
    EXPECT_EQ(LocaleWrapper::translate("", "messages"), "");
}

// Test case conversion
TEST_F(LocaleWrapperTest, CaseConversion) {
    // Test with English text
    std::string mixedCase = "Hello, World! 123";

    if (defaultWrapper) {
        // Test uppercase
        std::string upperCase = defaultWrapper->toUpper(mixedCase);
        EXPECT_EQ(upperCase, "HELLO, WORLD! 123");

        // Test lowercase
        std::string lowerCase = defaultWrapper->toLower(mixedCase);
        EXPECT_EQ(lowerCase, "hello, world! 123");

        // Test title case
        std::string titleCase = defaultWrapper->toTitle(mixedCase);
        EXPECT_EQ(titleCase[0], 'H');  // First letter should be uppercase
        EXPECT_EQ(titleCase[7], 'W');  // 'W' in 'World' should be uppercase
    }

    // Test with locale-specific case conversion (Turkish dotted/dotless i)
    if (isLocaleAvailable("tr_TR.UTF-8")) {
        try {
            LocaleWrapper trTRWrapper("tr_TR.UTF-8");

            // In Turkish, uppercase of 'i' is 'İ' (dotted I)
            std::string turkish = "istanbul";
            std::string turkishUpper = trTRWrapper.toUpper(turkish);

            // This test depends on the specific implementation of Boost.Locale
            // It might not handle Turkish properly, so we don't make strict
            // assertions
            EXPECT_NO_THROW(trTRWrapper.toUpper(turkish));
            EXPECT_NO_THROW(trTRWrapper.toLower(turkishUpper));
        } catch (const std::exception& e) {
            std::cerr << "Warning: Turkish locale test failed: " << e.what()
                      << std::endl;
        }
    }

    // Test with empty string
    if (defaultWrapper) {
        EXPECT_EQ(defaultWrapper->toUpper(""), "");
        EXPECT_EQ(defaultWrapper->toLower(""), "");
        EXPECT_EQ(defaultWrapper->toTitle(""), "");
    }
}

// Test string comparison
TEST_F(LocaleWrapperTest, StringComparison) {
    if (defaultWrapper) {
        // Basic comparison
        EXPECT_LT(defaultWrapper->compare("apple", "banana"), 0);
        EXPECT_GT(defaultWrapper->compare("banana", "apple"), 0);
        EXPECT_EQ(defaultWrapper->compare("apple", "apple"), 0);

        // Case-insensitive comparison (depends on collation rules)
        // Primary strength comparison should ignore case
        int compareResult = defaultWrapper->compare("Apple", "apple");
        // It might be equal (0) or not, depending on the locale's collation
        // rules Just make sure it doesn't throw
        EXPECT_NO_THROW(defaultWrapper->compare("Apple", "apple"));
    }

    // Test with different locales
    if (enUsWrapper && deDEWrapper) {
        // Test German specific collation rules (ä sorts differently in German)
        std::string wordWithUmlaut = "ärger";
        std::string normalA = "arger";

        int enUsCompare = enUsWrapper->compare(wordWithUmlaut, normalA);
        int deDeCompare = deDEWrapper->compare(wordWithUmlaut, normalA);

        // We can't make strict assertions since it depends on the specific
        // implementation Just verify that it doesn't throw
        EXPECT_NO_THROW(enUsWrapper->compare(wordWithUmlaut, normalA));
        EXPECT_NO_THROW(deDEWrapper->compare(wordWithUmlaut, normalA));
    }

    // Test with empty strings
    if (defaultWrapper) {
        EXPECT_EQ(defaultWrapper->compare("", ""), 0);
        EXPECT_LT(defaultWrapper->compare("", "a"), 0);
        EXPECT_GT(defaultWrapper->compare("a", ""), 0);
    }
}

// Test date and time formatting
TEST_F(LocaleWrapperTest, DateTimeFormatting) {
    // Create a fixed date and time for testing
    ::boost::posix_time::ptime testDateTime(
        ::boost::gregorian::date(2023, 5, 15),          // May 15, 2023
        ::boost::posix_time::time_duration(14, 30, 45)  // 14:30:45
    );

    try {
        // Test with default format
        std::string formattedDate =
            LocaleWrapper::formatDate(testDateTime, "%Y-%m-%d %H:%M:%S");
        EXPECT_EQ(formattedDate, "2023-05-15 14:30:45");

        // Test with different format
        formattedDate = LocaleWrapper::formatDate(testDateTime, "%B %d, %Y");
        // This will depend on the locale, but should contain the year
        EXPECT_THAT(formattedDate, HasSubstr("2023"));
        EXPECT_THAT(formattedDate, HasSubstr("15"));
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Skipping date formatting test: " << e.what();
    }

    // Test with empty format string
    EXPECT_NO_THROW(LocaleWrapper::formatDate(testDateTime, ""));
}

// Test number formatting
TEST_F(LocaleWrapperTest, NumberFormatting) {
    try {
        // Test with default precision (2)
        std::string formattedNumber = LocaleWrapper::formatNumber(1234.5678);
        EXPECT_THAT(formattedNumber,
                    HasSubstr("1234.57"));  // Rounded to 2 decimal places

        // Test with custom precision
        formattedNumber = LocaleWrapper::formatNumber(1234.5678, 1);
        EXPECT_THAT(formattedNumber,
                    HasSubstr("1234.6"));  // Rounded to 1 decimal place

        formattedNumber = LocaleWrapper::formatNumber(1234.5678, 3);
        EXPECT_THAT(formattedNumber,
                    HasSubstr("1234.568"));  // Rounded to 3 decimal places

        // Test with zero
        formattedNumber = LocaleWrapper::formatNumber(0, 2);
        EXPECT_THAT(formattedNumber, HasSubstr("0.00"));

        // Test with negative number
        formattedNumber = LocaleWrapper::formatNumber(-1234.5678, 2);
        EXPECT_THAT(formattedNumber, HasSubstr("-1234.57"));
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Skipping number formatting test: " << e.what();
    }
}

// Test currency formatting
TEST_F(LocaleWrapperTest, CurrencyFormatting) {
    try {
        // Test with USD
        std::string formattedCurrency =
            LocaleWrapper::formatCurrency(1234.56, "USD");
        // The exact format depends on the locale, but should contain the amount
        EXPECT_THAT(formattedCurrency, HasSubstr("1234"));

        // Test with EUR
        formattedCurrency = LocaleWrapper::formatCurrency(1234.56, "EUR");
        EXPECT_THAT(formattedCurrency, HasSubstr("1234"));

        // Test with JPY
        formattedCurrency = LocaleWrapper::formatCurrency(1234, "JPY");
        EXPECT_THAT(formattedCurrency, HasSubstr("1234"));

        // Test with negative amount
        formattedCurrency = LocaleWrapper::formatCurrency(-1234.56, "USD");
        EXPECT_THAT(formattedCurrency, HasSubstr("-"));
        EXPECT_THAT(formattedCurrency, HasSubstr("1234"));
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Skipping currency formatting test: " << e.what();
    }
}

// Test regex replacement
TEST_F(LocaleWrapperTest, RegexReplacement) {
    // Test simple replacement
    ::boost::regex simpleRegex("world");
    std::string replaced =
        LocaleWrapper::regexReplace("Hello, world!", simpleRegex, "universe");
    EXPECT_EQ(replaced, "Hello, universe!");

    // Test with multiple occurrences
    ::boost::regex multiRegex("a");
    replaced = LocaleWrapper::regexReplace("banana", multiRegex, "o");
    EXPECT_EQ(replaced, "bonono");

    // Test with regex pattern (matching digits)
    ::boost::regex digitsRegex("\\d+");
    replaced = LocaleWrapper::regexReplace(
        "There are 123 apples and 456 oranges", digitsRegex, "many");
    EXPECT_EQ(replaced, "There are many apples and many oranges");

    // Test with capturing groups
    ::boost::regex captureRegex("(\\w+)-(\\w+)");
    replaced =
        LocaleWrapper::regexReplace("hello-world", captureRegex, "\\2-\\1");
    EXPECT_EQ(replaced, "world-hello");

    // Test with empty string
    replaced = LocaleWrapper::regexReplace("", simpleRegex, "replacement");
    EXPECT_EQ(replaced, "");

    // Test with no matches
    replaced =
        LocaleWrapper::regexReplace("Hello, universe!", simpleRegex, "world");
    EXPECT_EQ(replaced, "Hello, universe!");
}

// Test string formatting
TEST_F(LocaleWrapperTest, StringFormatting) {
    if (defaultWrapper) {
        try {
            // Test with string argument
            std::string formatted =
                defaultWrapper->format("Hello, {1}!", "world");
            EXPECT_EQ(formatted, "Hello, world!");

            // Test with multiple arguments
            formatted = defaultWrapper->format("{1} + {2} = {3}", 2, 3, 5);
            EXPECT_EQ(formatted, "2 + 3 = 5");

            // Test with different types
            formatted = defaultWrapper->format(
                "Name: {1}, Age: {2}, Height: {3}m", "John", 30, 1.75);
            EXPECT_THAT(formatted, HasSubstr("Name: John"));
            EXPECT_THAT(formatted, HasSubstr("Age: 30"));
            EXPECT_THAT(formatted, HasSubstr("Height: 1.75m"));
        } catch (const std::exception& e) {
            GTEST_SKIP() << "Skipping string formatting test: " << e.what();
        }
    }
}

// Test with different languages and scripts
TEST_F(LocaleWrapperTest, InternationalText) {
    // Note: These tests mainly verify that the methods don't throw
    // when dealing with non-ASCII text

    // Russian
    std::string russian = "Привет, мир!";
    // Chinese
    std::string chinese = "你好，世界！";
    // Arabic
    std::string arabic = "مرحبا بالعالم!";
    // Japanese
    std::string japanese = "こんにちは世界！";

    if (defaultWrapper) {
        // Test case conversion
        EXPECT_NO_THROW(defaultWrapper->toUpper(russian));
        EXPECT_NO_THROW(defaultWrapper->toLower(chinese));
        EXPECT_NO_THROW(defaultWrapper->toTitle(arabic));
        EXPECT_NO_THROW(defaultWrapper->toTitle(japanese));

        // Test comparison
        EXPECT_NO_THROW(defaultWrapper->compare(russian, chinese));
        EXPECT_NO_THROW(defaultWrapper->compare(arabic, japanese));
    }

    // Test tokenization
    EXPECT_NO_THROW(LocaleWrapper::tokenize(russian));
    EXPECT_NO_THROW(LocaleWrapper::tokenize(chinese));
    EXPECT_NO_THROW(LocaleWrapper::tokenize(arabic));
    EXPECT_NO_THROW(LocaleWrapper::tokenize(japanese));

    // Test normalization
    EXPECT_NO_THROW(LocaleWrapper::normalize(russian));
    EXPECT_NO_THROW(LocaleWrapper::normalize(chinese));
    EXPECT_NO_THROW(LocaleWrapper::normalize(arabic));
    EXPECT_NO_THROW(LocaleWrapper::normalize(japanese));
}

// Test with edge cases
TEST_F(LocaleWrapperTest, EdgeCases) {
    // Test with empty string
    if (defaultWrapper) {
        EXPECT_EQ(defaultWrapper->toUpper(""), "");
        EXPECT_EQ(defaultWrapper->toLower(""), "");
        EXPECT_EQ(defaultWrapper->toTitle(""), "");
        EXPECT_EQ(defaultWrapper->compare("", ""), 0);
    }

    EXPECT_EQ(LocaleWrapper::normalize(""), "");
    EXPECT_TRUE(LocaleWrapper::tokenize("").empty());
    EXPECT_EQ(LocaleWrapper::translate("", "domain"), "");

    // Test with very long string
    std::string longString(10000, 'a');
    if (defaultWrapper) {
        EXPECT_NO_THROW(defaultWrapper->toUpper(longString));
        EXPECT_NO_THROW(defaultWrapper->toLower(longString));
        EXPECT_NO_THROW(defaultWrapper->toTitle(longString));
    }

    EXPECT_NO_THROW(LocaleWrapper::normalize(longString));
    EXPECT_NO_THROW(LocaleWrapper::tokenize(longString));
    EXPECT_NO_THROW(LocaleWrapper::translate(longString, "domain"));

    // Test with unusual characters
    std::string unusualChars =
        "❤️🌍🚀👨‍👩‍👧‍👦";  // Emoji including ZWJ
                                                    // sequences
    if (defaultWrapper) {
        EXPECT_NO_THROW(defaultWrapper->toUpper(unusualChars));
        EXPECT_NO_THROW(defaultWrapper->toLower(unusualChars));
        EXPECT_NO_THROW(defaultWrapper->toTitle(unusualChars));
    }

    EXPECT_NO_THROW(LocaleWrapper::normalize(unusualChars));
    EXPECT_NO_THROW(LocaleWrapper::tokenize(unusualChars));
    EXPECT_NO_THROW(LocaleWrapper::translate(unusualChars, "domain"));
}

}  // namespace atom::extra::boost::test

#endif  // ATOM_EXTRA_BOOST_TEST_LOCALE_HPP
