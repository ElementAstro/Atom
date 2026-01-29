#ifndef ATOM_SECRET_PASSWORD_VALIDATOR_HPP
#define ATOM_SECRET_PASSWORD_VALIDATOR_HPP

#include <string>
#include <string_view>
#include <vector>

#include "../core/result.hpp"
#include "entry.hpp"

namespace atom::secret {

/**
 * @brief Password policy configuration.
 */
struct PasswordPolicy {
    int minLength = 8;             ///< Minimum password length
    int maxLength = 128;           ///< Maximum password length
    bool requireUppercase = true;  ///< Require at least one uppercase letter
    bool requireLowercase = true;  ///< Require at least one lowercase letter
    bool requireDigit = true;      ///< Require at least one digit
    bool requireSpecial = false;   ///< Require at least one special character
    int minUppercase = 1;          ///< Minimum uppercase letters
    int minLowercase = 1;          ///< Minimum lowercase letters
    int minDigits = 1;             ///< Minimum digits
    int minSpecial = 0;            ///< Minimum special characters
    bool disallowCommonPasswords =
        true;  ///< Check against common password list
    bool disallowRepeatingChars = false;  ///< Disallow 3+ repeating characters
    bool disallowSequentialChars =
        false;  ///< Disallow sequential characters (abc, 123)
    int maxConsecutiveRepeats =
        3;  ///< Maximum consecutive repeating characters
    PasswordStrength minStrength =
        PasswordStrength::Medium;  ///< Minimum required strength
    std::vector<std::string>
        disallowedWords;  ///< Words that cannot appear in password

    /**
     * @brief Creates a default policy.
     * @return Default policy.
     */
    static PasswordPolicy defaults() { return PasswordPolicy{}; }

    /**
     * @brief Creates a strict policy.
     * @return Strict policy.
     */
    static PasswordPolicy strict() {
        PasswordPolicy policy;
        policy.minLength = 12;
        policy.requireSpecial = true;
        policy.minSpecial = 1;
        policy.disallowRepeatingChars = true;
        policy.disallowSequentialChars = true;
        policy.minStrength = PasswordStrength::Strong;
        return policy;
    }

    /**
     * @brief Creates a relaxed policy.
     * @return Relaxed policy.
     */
    static PasswordPolicy relaxed() {
        PasswordPolicy policy;
        policy.minLength = 6;
        policy.requireUppercase = false;
        policy.requireSpecial = false;
        policy.minUppercase = 0;
        policy.minStrength = PasswordStrength::Weak;
        return policy;
    }
};

/**
 * @brief Detailed password analysis results.
 */
struct PasswordAnalysis {
    PasswordStrength strength;     ///< Overall password strength
    int score;                     ///< Numerical score (0-100)
    double entropy;                ///< Password entropy in bits
    double crackTimeSeconds;       ///< Estimated crack time in seconds
    std::string crackTimeDisplay;  ///< Human-readable crack time

    // Character composition
    int length;          ///< Password length
    int uppercaseCount;  ///< Number of uppercase letters
    int lowercaseCount;  ///< Number of lowercase letters
    int digitCount;      ///< Number of digits
    int specialCount;    ///< Number of special characters
    int uniqueChars;     ///< Number of unique characters

    // Pattern detection
    bool hasUppercase;            ///< Contains uppercase letters
    bool hasLowercase;            ///< Contains lowercase letters
    bool hasDigits;               ///< Contains digits
    bool hasSpecial;              ///< Contains special characters
    bool hasRepeatedChars;        ///< Contains repeated characters (aaa)
    bool hasSequentialChars;      ///< Contains sequential characters (abc, 123)
    bool hasKeyboardPattern;      ///< Contains keyboard patterns (qwerty)
    bool isCommonPassword;        ///< Is a commonly used password
    bool hasCommonSubstitutions;  ///< Uses common substitutions (@ for a)

    std::vector<std::string> warnings;     ///< Warning messages
    std::vector<std::string> suggestions;  ///< Improvement suggestions

    /**
     * @brief Gets a summary of the analysis.
     * @return Summary string.
     */
    std::string getSummary() const;
};

/**
 * @brief Password validation and strength assessment.
 */
class PasswordValidator {
public:
    /**
     * @brief Analyzes password strength and characteristics.
     * @param password Password to analyze.
     * @return Detailed analysis results.
     */
    static PasswordAnalysis analyze(std::string_view password);

    /**
     * @brief Validates password against a policy.
     * @param password Password to validate.
     * @param policy Password policy.
     * @return Result containing true if valid, or error with details.
     */
    static Result<bool> validate(
        std::string_view password,
        const PasswordPolicy& policy = PasswordPolicy::defaults());

    /**
     * @brief Gets all validation errors for a password.
     * @param password Password to validate.
     * @param policy Password policy.
     * @return Vector of error messages (empty if valid).
     */
    static std::vector<std::string> getValidationErrors(
        std::string_view password,
        const PasswordPolicy& policy = PasswordPolicy::defaults());

    /**
     * @brief Calculates password entropy.
     * @param password Password to analyze.
     * @return Entropy in bits.
     */
    static double calculateEntropy(std::string_view password);

    /**
     * @brief Estimates time to crack password.
     * @param password Password to analyze.
     * @param guessesPerSecond Guesses per second (default: 10 billion).
     * @return Estimated crack time in seconds.
     */
    static double estimateCrackTime(std::string_view password,
                                    double guessesPerSecond = 1e10);

    /**
     * @brief Formats crack time as human-readable string.
     * @param seconds Crack time in seconds.
     * @return Human-readable string.
     */
    static std::string formatCrackTime(double seconds);

    /**
     * @brief Checks if password is in common password list.
     * @param password Password to check.
     * @return True if password is common.
     */
    static bool isCommonPassword(std::string_view password);

    /**
     * @brief Checks if password contains a word from a list.
     * @param password Password to check.
     * @param words Words to check for.
     * @return True if any word is found.
     */
    static bool containsWord(std::string_view password,
                             const std::vector<std::string>& words);

    /**
     * @brief Determines password strength from score.
     * @param score Score (0-100).
     * @return Password strength level.
     */
    static PasswordStrength scoreToStrength(int score);

private:
    /**
     * @brief Calculates character set size for entropy.
     * @param password Password to analyze.
     * @return Character set size.
     */
    static int calculateCharsetSize(std::string_view password);

    /**
     * @brief Checks for repeated character patterns.
     * @param password Password to check.
     * @param maxRepeats Maximum allowed repeats.
     * @return True if excessive repeats found.
     */
    static bool hasExcessiveRepeats(std::string_view password, int maxRepeats);

    /**
     * @brief Checks for sequential character patterns.
     * @param password Password to check.
     * @return True if sequential patterns found.
     */
    static bool hasSequentialPatterns(std::string_view password);

    /**
     * @brief Checks for keyboard patterns.
     * @param password Password to check.
     * @return True if keyboard patterns found.
     */
    static bool hasKeyboardPatterns(std::string_view password);

    /**
     * @brief Checks for common character substitutions.
     * @param password Password to check.
     * @return True if common substitutions found.
     */
    static bool hasCommonSubstitutions(std::string_view password);

    /**
     * @brief Gets the common passwords list.
     * @return Vector of common passwords.
     */
    static const std::vector<std::string>& getCommonPasswords();

    /**
     * @brief Gets keyboard pattern sequences.
     * @return Vector of keyboard patterns.
     */
    static const std::vector<std::string>& getKeyboardPatterns();
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
    static bool constantTimeEquals(std::string_view a,
                                   std::string_view b) noexcept;

    /**
     * @brief Performs constant-time memory comparison.
     * @param a First memory block.
     * @param b Second memory block.
     * @param size Size of memory blocks.
     * @return True if memory blocks are equal.
     */
    static bool constantTimeEquals(const void* a, const void* b,
                                   size_t size) noexcept;
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_PASSWORD_VALIDATOR_HPP
