#ifndef ATOM_SECRET_PASSWORD_UTILS_HPP
#define ATOM_SECRET_PASSWORD_UTILS_HPP

#include <string>
#include <string_view>
#include <vector>

#include "common.hpp"
#include "result.hpp"

namespace atom::secret {

/**
 * @brief Password generation utilities.
 */
class PasswordGenerator {
public:
    /**
     * @brief Character sets for password generation.
     */
    struct CharacterSets {
        static constexpr const char* LOWERCASE = "abcdefghijklmnopqrstuvwxyz";
        static constexpr const char* UPPERCASE = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        static constexpr const char* DIGITS = "0123456789";
        static constexpr const char* SPECIAL = "!@#$%^&*()_+-=[]{}|;:,.<>?";
        static constexpr const char* AMBIGUOUS = "0O1lI";
    };

    /**
     * @brief Options for password generation.
     */
    struct GenerationOptions {
        int length = 16;                    ///< Password length.
        bool includeLowercase = true;       ///< Include lowercase letters.
        bool includeUppercase = true;       ///< Include uppercase letters.
        bool includeDigits = true;          ///< Include digits.
        bool includeSpecial = true;         ///< Include special characters.
        bool excludeAmbiguous = false;      ///< Exclude ambiguous characters.
        std::string customCharacters;       ///< Custom character set.
        int minLowercase = 1;              ///< Minimum lowercase letters.
        int minUppercase = 1;              ///< Minimum uppercase letters.
        int minDigits = 1;                 ///< Minimum digits.
        int minSpecial = 1;                ///< Minimum special characters.
    };

    /**
     * @brief Generates a secure password with the specified options.
     * @param options Password generation options.
     * @return Result containing the generated password or error message.
     */
    static Result<std::string> generatePassword(const GenerationOptions& options = {});

    /**
     * @brief Generates a password based on PasswordManagerSettings.
     * @param settings Password manager settings.
     * @param length Desired password length (overrides settings if specified).
     * @return Result containing the generated password or error message.
     */
    static Result<std::string> generatePassword(
        const PasswordManagerSettings& settings,
        int length = 0);

    /**
     * @brief Generates a memorable password using word lists.
     * @param wordCount Number of words to use.
     * @param separator Separator between words.
     * @param includeNumbers Whether to include numbers.
     * @return Result containing the generated password or error message.
     */
    static Result<std::string> generateMemorablePassword(
        int wordCount = 4,
        const std::string& separator = "-",
        bool includeNumbers = true);

    /**
     * @brief Generates a PIN code.
     * @param length PIN length.
     * @return Result containing the generated PIN or error message.
     */
    static Result<std::string> generatePin(int length = 6);

private:
    /**
     * @brief Builds character set based on options.
     * @param options Generation options.
     * @return Character set string.
     */
    static std::string buildCharacterSet(const GenerationOptions& options);

    /**
     * @brief Validates generation options.
     * @param options Generation options to validate.
     * @return Error message if invalid, empty string if valid.
     */
    static std::string validateOptions(const GenerationOptions& options);

    /**
     * @brief Ensures minimum character requirements are met.
     * @param password Password to modify.
     * @param options Generation options.
     * @return Modified password.
     */
    static std::string ensureMinimumRequirements(
        std::string password,
        const GenerationOptions& options);
};

/**
 * @brief Password validation and strength assessment utilities.
 */
class PasswordValidator {
public:
    /**
     * @brief Detailed password analysis results.
     */
    struct AnalysisResult {
        PasswordStrength strength;          ///< Overall password strength.
        int score;                         ///< Numerical score (0-100).
        bool hasLowercase;                 ///< Contains lowercase letters.
        bool hasUppercase;                 ///< Contains uppercase letters.
        bool hasDigits;                    ///< Contains digits.
        bool hasSpecial;                   ///< Contains special characters.
        bool hasRepeatedChars;             ///< Contains repeated characters.
        bool hasSequentialChars;           ///< Contains sequential characters.
        bool isCommonPassword;             ///< Is a commonly used password.
        std::vector<std::string> suggestions; ///< Improvement suggestions.
        double entropy;                    ///< Password entropy in bits.
    };

    /**
     * @brief Analyzes password strength and characteristics.
     * @param password Password to analyze.
     * @return Detailed analysis results.
     */
    static AnalysisResult analyzePassword(std::string_view password);

    /**
     * @brief Validates password against settings requirements.
     * @param password Password to validate.
     * @param settings Password manager settings.
     * @return Result containing true if valid or error message.
     */
    static Result<bool> validatePassword(
        std::string_view password,
        const PasswordManagerSettings& settings);

    /**
     * @brief Calculates password entropy.
     * @param password Password to analyze.
     * @return Entropy in bits.
     */
    static double calculateEntropy(std::string_view password);

    /**
     * @brief Checks if password is in common password list.
     * @param password Password to check.
     * @return True if password is common.
     */
    static bool isCommonPassword(std::string_view password);

    /**
     * @brief Estimates time to crack password.
     * @param password Password to analyze.
     * @param guessesPerSecond Guesses per second (default: 1 billion).
     * @return Estimated crack time in seconds.
     */
    static double estimateCrackTime(
        std::string_view password,
        double guessesPerSecond = 1e9);

private:
    /**
     * @brief Checks for repeated character patterns.
     * @param password Password to check.
     * @return True if repeated patterns found.
     */
    static bool hasRepeatedPatterns(std::string_view password);

    /**
     * @brief Checks for sequential character patterns.
     * @param password Password to check.
     * @return True if sequential patterns found.
     */
    static bool hasSequentialPatterns(std::string_view password);

    /**
     * @brief Calculates character set size for entropy calculation.
     * @param password Password to analyze.
     * @return Character set size.
     */
    static int calculateCharacterSetSize(std::string_view password);

    /**
     * @brief Gets common passwords list.
     * @return Vector of common passwords.
     */
    static const std::vector<std::string>& getCommonPasswords();
};

/**
 * @brief Secure string comparison utilities.
 */
class SecureComparison {
public:
    /**
     * @brief Performs constant-time string comparison.
     * @param a First string.
     * @param b Second string.
     * @return True if strings are equal.
     */
    static bool constantTimeEquals(std::string_view a, std::string_view b) noexcept;

    /**
     * @brief Performs constant-time memory comparison.
     * @param a First memory block.
     * @param b Second memory block.
     * @param size Size of memory blocks.
     * @return True if memory blocks are equal.
     */
    static bool constantTimeEquals(const void* a, const void* b, size_t size) noexcept;
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_PASSWORD_UTILS_HPP
