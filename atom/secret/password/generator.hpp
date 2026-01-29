#ifndef ATOM_SECRET_PASSWORD_GENERATOR_HPP
#define ATOM_SECRET_PASSWORD_GENERATOR_HPP

#include <string>
#include <vector>

#include "../core/result.hpp"

namespace atom::secret {

/**
 * @brief Character sets for password generation.
 */
struct CharacterSets {
    static constexpr const char* LOWERCASE = "abcdefghijklmnopqrstuvwxyz";
    static constexpr const char* UPPERCASE = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static constexpr const char* DIGITS = "0123456789";
    static constexpr const char* SPECIAL = "!@#$%^&*()_+-=[]{}|;:,.<>?";
    static constexpr const char* SPECIAL_SAFE =
        "!@#$%^&*_+-=";  // URL-safe special chars
    static constexpr const char* AMBIGUOUS = "0O1lI";
    static constexpr const char* BRACKETS = "()[]{}";
    static constexpr const char* PUNCTUATION = ".,;:!?";
};

/**
 * @brief Options for password generation.
 */
struct PasswordGeneratorOptions {
    int length = 16;                ///< Password length
    bool includeLowercase = true;   ///< Include lowercase letters
    bool includeUppercase = true;   ///< Include uppercase letters
    bool includeDigits = true;      ///< Include digits
    bool includeSpecial = true;     ///< Include special characters
    bool excludeAmbiguous = false;  ///< Exclude ambiguous characters (0O1lI)
    bool excludeBrackets = false;   ///< Exclude brackets
    std::string customCharacters;   ///< Custom character set to include
    std::string excludeCharacters;  ///< Characters to exclude
    int minLowercase = 1;           ///< Minimum lowercase letters
    int minUppercase = 1;           ///< Minimum uppercase letters
    int minDigits = 1;              ///< Minimum digits
    int minSpecial = 0;             ///< Minimum special characters
    bool startWithLetter = false;   ///< Password must start with a letter
    bool noRepeatingChars = false;  ///< No repeating adjacent characters

    /**
     * @brief Creates default options.
     * @return Default options.
     */
    static PasswordGeneratorOptions defaults() {
        return PasswordGeneratorOptions{};
    }

    /**
     * @brief Creates options for a strong password.
     * @return Strong password options.
     */
    static PasswordGeneratorOptions strong() {
        PasswordGeneratorOptions opts;
        opts.length = 20;
        opts.minSpecial = 2;
        opts.excludeAmbiguous = true;
        return opts;
    }

    /**
     * @brief Creates options for a PIN.
     * @param length PIN length.
     * @return PIN options.
     */
    static PasswordGeneratorOptions pin(int length = 6) {
        PasswordGeneratorOptions opts;
        opts.length = length;
        opts.includeLowercase = false;
        opts.includeUppercase = false;
        opts.includeDigits = true;
        opts.includeSpecial = false;
        opts.minLowercase = 0;
        opts.minUppercase = 0;
        opts.minDigits = 0;
        opts.minSpecial = 0;
        return opts;
    }

    /**
     * @brief Creates options for a passphrase-friendly password.
     * @return Passphrase options.
     */
    static PasswordGeneratorOptions readable() {
        PasswordGeneratorOptions opts;
        opts.length = 16;
        opts.includeSpecial = false;
        opts.excludeAmbiguous = true;
        opts.minSpecial = 0;
        return opts;
    }
};

/**
 * @brief Password generation utilities.
 */
class PasswordGenerator {
public:
    /**
     * @brief Generates a secure password with default options.
     * @return Result containing the generated password or error.
     */
    static Result<std::string> generate();

    /**
     * @brief Generates a secure password with the specified options.
     * @param options Password generation options.
     * @return Result containing the generated password or error.
     */
    static Result<std::string> generate(
        const PasswordGeneratorOptions& options);

    /**
     * @brief Generates a PIN code.
     * @param length PIN length (default 6).
     * @return Result containing the generated PIN or error.
     */
    static Result<std::string> generatePin(int length = 6);

    /**
     * @brief Generates a memorable password using word lists.
     * @param wordCount Number of words to use.
     * @param separator Separator between words.
     * @param capitalize Capitalize first letter of each word.
     * @param includeNumber Include a number.
     * @return Result containing the generated password or error.
     */
    static Result<std::string> generatePassphrase(
        int wordCount = 4, const std::string& separator = "-",
        bool capitalize = true, bool includeNumber = true);

    /**
     * @brief Generates a pronounceable password.
     * @param length Approximate length.
     * @return Result containing the generated password or error.
     */
    static Result<std::string> generatePronounceable(int length = 12);

    /**
     * @brief Generates multiple passwords.
     * @param count Number of passwords to generate.
     * @param options Generation options.
     * @return Result containing vector of passwords or error.
     */
    static Result<std::vector<std::string>> generateMultiple(
        int count, const PasswordGeneratorOptions& options =
                       PasswordGeneratorOptions::defaults());

    /**
     * @brief Validates generation options.
     * @param options Options to validate.
     * @return Error message if invalid, empty string if valid.
     */
    static std::string validateOptions(const PasswordGeneratorOptions& options);

private:
    /**
     * @brief Builds character set based on options.
     * @param options Generation options.
     * @return Character set string.
     */
    static std::string buildCharacterSet(
        const PasswordGeneratorOptions& options);

    /**
     * @brief Ensures minimum character requirements are met.
     * @param password Password to modify.
     * @param options Generation options.
     * @return Modified password.
     */
    static std::string ensureMinimumRequirements(
        std::string password, const PasswordGeneratorOptions& options);

    /**
     * @brief Gets a random character from a string.
     * @param chars Character set.
     * @return Random character or '\0' on error.
     */
    static char getRandomChar(const std::string& chars);

    /**
     * @brief Gets a random integer in range [0, max).
     * @param max Upper bound (exclusive).
     * @return Random integer.
     */
    static size_t getRandomIndex(size_t max);

    /**
     * @brief Shuffles a string randomly.
     * @param str String to shuffle.
     */
    static void shuffleString(std::string& str);

    /**
     * @brief Gets the word list for passphrase generation.
     * @return Vector of words.
     */
    static const std::vector<std::string>& getWordList();
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_PASSWORD_GENERATOR_HPP
