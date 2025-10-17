#include "password_utils.hpp"

#include <openssl/rand.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <random>
#include <set>
#include <unordered_set>

#include "spdlog/spdlog.h"

namespace atom::secret {

// ============================================================================
// PasswordGenerator Implementation
// ============================================================================

Result<std::string> PasswordGenerator::generatePassword() {
    return generatePassword(GenerationOptions{});
}

Result<std::string> PasswordGenerator::generatePassword(
    const GenerationOptions& options) {
    // Validate options
    std::string validationError = validateOptions(options);
    if (!validationError.empty()) {
        return Result<std::string>::error(validationError);
    }

    // Build character set
    std::string charset = buildCharacterSet(options);
    if (charset.empty()) {
        return Result<std::string>(
            "No characters available for password generation");
    }

    // Generate random password
    std::string password;
    password.reserve(options.length);

    // Use OpenSSL for cryptographically secure random numbers
    std::vector<unsigned char> randomBytes(options.length);
    if (RAND_bytes(randomBytes.data(), options.length) != 1) {
        return Result<std::string>("Failed to generate secure random numbers");
    }

    for (int i = 0; i < options.length; ++i) {
        size_t index = randomBytes[i] % charset.length();
        password += charset[index];
    }

    // Ensure minimum requirements are met
    password = ensureMinimumRequirements(std::move(password), options);

    return Result<std::string>(std::move(password));
}

Result<std::string> PasswordGenerator::generatePassword(
    const PasswordManagerSettings& settings, int length) {
    GenerationOptions options;
    options.length = (length > 0) ? length : settings.minPasswordLength;
    options.includeLowercase = true;
    options.includeUppercase = settings.requireMixedCase;
    options.includeDigits = settings.requireNumbers;
    options.includeSpecial = settings.requireSpecialChars;
    options.excludeAmbiguous = true;  // Default to excluding ambiguous chars

    return generatePassword(options);
}

Result<std::string> PasswordGenerator::generateMemorablePassword(
    int wordCount, const std::string& separator, bool includeNumbers) {
    if (wordCount < 2 || wordCount > 10) {
        return Result<std::string>("Word count must be between 2 and 10");
    }

    // Simple word list for memorable passwords
    static const std::vector<std::string> words = {
        "apple", "brave", "chair", "dance", "eagle",  "flame", "grace",
        "house", "image", "juice", "knife", "light",  "music", "night",
        "ocean", "peace", "quiet", "river", "stone",  "table", "unity",
        "voice", "water", "youth", "zebra", "beach",  "cloud", "dream",
        "earth", "field", "green", "happy", "island", "magic", "north",
        "power", "quick", "smile", "trust", "world"};

    std::string password;
    std::vector<unsigned char> randomBytes(wordCount * 2);

    if (RAND_bytes(randomBytes.data(), wordCount * 2) != 1) {
        return Result<std::string>("Failed to generate secure random numbers");
    }

    for (int i = 0; i < wordCount; ++i) {
        if (i > 0) {
            password += separator;
        }

        size_t wordIndex = randomBytes[i] % words.size();
        password += words[wordIndex];

        if (includeNumbers && i < wordCount - 1) {
            int number = randomBytes[wordCount + i] % 100;
            password += std::to_string(number);
        }
    }

    return Result<std::string>(std::move(password));
}

Result<std::string> PasswordGenerator::generatePin(int length) {
    if (length < 4 || length > 20) {
        return Result<std::string>("PIN length must be between 4 and 20");
    }

    std::string pin;
    pin.reserve(length);

    std::vector<unsigned char> randomBytes(length);
    if (RAND_bytes(randomBytes.data(), length) != 1) {
        return Result<std::string>("Failed to generate secure random numbers");
    }

    for (int i = 0; i < length; ++i) {
        pin += '0' + (randomBytes[i] % 10);
    }

    return Result<std::string>(std::move(pin));
}

std::string PasswordGenerator::buildCharacterSet(
    const GenerationOptions& options) {
    std::string charset;

    if (options.includeLowercase) {
        charset += CharacterSets::LOWERCASE;
    }
    if (options.includeUppercase) {
        charset += CharacterSets::UPPERCASE;
    }
    if (options.includeDigits) {
        charset += CharacterSets::DIGITS;
    }
    if (options.includeSpecial) {
        charset += CharacterSets::SPECIAL;
    }
    if (!options.customCharacters.empty()) {
        charset += options.customCharacters;
    }

    // Remove ambiguous characters if requested
    if (options.excludeAmbiguous) {
        std::string ambiguous = CharacterSets::AMBIGUOUS;
        for (char c : ambiguous) {
            charset.erase(std::remove(charset.begin(), charset.end(), c),
                          charset.end());
        }
    }

    // Remove duplicates
    std::sort(charset.begin(), charset.end());
    charset.erase(std::unique(charset.begin(), charset.end()), charset.end());

    return charset;
}

std::string PasswordGenerator::validateOptions(
    const GenerationOptions& options) {
    if (options.length < 1 || options.length > 1000) {
        return "Password length must be between 1 and 1000";
    }

    if (!options.includeLowercase && !options.includeUppercase &&
        !options.includeDigits && !options.includeSpecial &&
        options.customCharacters.empty()) {
        return "At least one character type must be enabled";
    }

    int minRequired = options.minLowercase + options.minUppercase +
                      options.minDigits + options.minSpecial;
    if (minRequired > options.length) {
        return "Minimum character requirements exceed password length";
    }

    return "";
}

std::string PasswordGenerator::ensureMinimumRequirements(
    std::string password, const GenerationOptions& options) {
    // Count existing character types
    int lowercase = 0, uppercase = 0, digits = 0, special = 0;

    for (char c : password) {
        if (std::islower(c))
            lowercase++;
        else if (std::isupper(c))
            uppercase++;
        else if (std::isdigit(c))
            digits++;
        else
            special++;
    }

    // Replace characters to meet minimum requirements
    std::vector<unsigned char> randomBytes(password.length());
    RAND_bytes(randomBytes.data(), password.length());

    size_t pos = 0;

    // Ensure minimum lowercase
    while (lowercase < options.minLowercase && pos < password.length()) {
        if (!std::islower(password[pos])) {
            password[pos] = CharacterSets::LOWERCASE[randomBytes[pos] % 26];
            lowercase++;
        }
        pos++;
    }

    // Ensure minimum uppercase
    while (uppercase < options.minUppercase && pos < password.length()) {
        if (!std::isupper(password[pos])) {
            password[pos] = CharacterSets::UPPERCASE[randomBytes[pos] % 26];
            uppercase++;
        }
        pos++;
    }

    // Ensure minimum digits
    while (digits < options.minDigits && pos < password.length()) {
        if (!std::isdigit(password[pos])) {
            password[pos] = CharacterSets::DIGITS[randomBytes[pos] % 10];
            digits++;
        }
        pos++;
    }

    // Ensure minimum special characters
    while (special < options.minSpecial && pos < password.length()) {
        if (!std::islower(password[pos]) && !std::isupper(password[pos]) &&
            !std::isdigit(password[pos])) {
            password[pos] = CharacterSets::SPECIAL[randomBytes[pos] % 26];
            special++;
        }
        pos++;
    }

    return password;
}

// ============================================================================
// PasswordValidator Implementation
// ============================================================================

PasswordValidator::AnalysisResult PasswordValidator::analyzePassword(
    std::string_view password) {
    AnalysisResult result = {};

    if (password.empty()) {
        result.strength = PasswordStrength::VeryWeak;
        result.score = 0;
        result.suggestions.push_back("Password cannot be empty");
        return result;
    }

    // Check character types
    for (char c : password) {
        if (std::islower(c))
            result.hasLowercase = true;
        else if (std::isupper(c))
            result.hasUppercase = true;
        else if (std::isdigit(c))
            result.hasDigits = true;
        else
            result.hasSpecial = true;
    }

    // Check for patterns
    result.hasRepeatedChars = hasRepeatedPatterns(password);
    result.hasSequentialChars = hasSequentialPatterns(password);
    result.isCommonPassword = isCommonPassword(password);

    // Calculate entropy
    result.entropy = calculateEntropy(password);

    // Calculate score based on various factors
    int score = 0;

    // Length scoring
    if (password.length() >= 8)
        score += 25;
    if (password.length() >= 12)
        score += 25;
    if (password.length() >= 16)
        score += 25;

    // Character type scoring
    if (result.hasLowercase)
        score += 5;
    if (result.hasUppercase)
        score += 5;
    if (result.hasDigits)
        score += 5;
    if (result.hasSpecial)
        score += 10;

    // Pattern penalties
    if (result.hasRepeatedChars)
        score -= 20;
    if (result.hasSequentialChars)
        score -= 15;
    if (result.isCommonPassword)
        score -= 30;

    // Entropy bonus
    if (result.entropy > 50)
        score += 15;

    result.score = std::max(0, std::min(100, score));

    // Determine strength
    if (result.score < 20)
        result.strength = PasswordStrength::VeryWeak;
    else if (result.score < 40)
        result.strength = PasswordStrength::Weak;
    else if (result.score < 60)
        result.strength = PasswordStrength::Medium;
    else if (result.score < 80)
        result.strength = PasswordStrength::Strong;
    else
        result.strength = PasswordStrength::VeryStrong;

    // Generate suggestions
    if (password.length() < 12) {
        result.suggestions.push_back("Use at least 12 characters");
    }
    if (!result.hasLowercase) {
        result.suggestions.push_back("Include lowercase letters");
    }
    if (!result.hasUppercase) {
        result.suggestions.push_back("Include uppercase letters");
    }
    if (!result.hasDigits) {
        result.suggestions.push_back("Include numbers");
    }
    if (!result.hasSpecial) {
        result.suggestions.push_back("Include special characters");
    }
    if (result.hasRepeatedChars) {
        result.suggestions.push_back("Avoid repeated character patterns");
    }
    if (result.hasSequentialChars) {
        result.suggestions.push_back("Avoid sequential character patterns");
    }
    if (result.isCommonPassword) {
        result.suggestions.push_back("Avoid common passwords");
    }

    return result;
}

Result<bool> PasswordValidator::validatePassword(
    std::string_view password, const PasswordManagerSettings& settings) {
    if (password.length() < static_cast<size_t>(settings.minPasswordLength)) {
        return Result<bool>::error("Password is too short (minimum " +
                                   std::to_string(settings.minPasswordLength) +
                                   " characters)");
    }

    bool hasLower = false, hasUpper = false, hasDigit = false,
         hasSpecial = false;

    for (char c : password) {
        if (std::islower(c))
            hasLower = true;
        else if (std::isupper(c))
            hasUpper = true;
        else if (std::isdigit(c))
            hasDigit = true;
        else
            hasSpecial = true;
    }

    if (settings.requireMixedCase && (!hasLower || !hasUpper)) {
        return Result<bool>(
            "Password must contain both uppercase and lowercase letters");
    }

    if (settings.requireNumbers && !hasDigit) {
        return Result<bool>("Password must contain at least one number");
    }

    if (settings.requireSpecialChars && !hasSpecial) {
        return Result<bool>(
            "Password must contain at least one special character");
    }

    return Result<bool>(true);
}

double PasswordValidator::calculateEntropy(std::string_view password) {
    if (password.empty()) {
        return 0.0;
    }

    int charsetSize = calculateCharacterSetSize(password);
    return password.length() * std::log2(charsetSize);
}

bool PasswordValidator::isCommonPassword(std::string_view password) {
    const auto& commonPasswords = getCommonPasswords();
    std::string lowerPassword;
    lowerPassword.reserve(password.length());

    for (char c : password) {
        lowerPassword += std::tolower(c);
    }

    return std::find(commonPasswords.begin(), commonPasswords.end(),
                     lowerPassword) != commonPasswords.end();
}

double PasswordValidator::estimateCrackTime(std::string_view password,
                                            double guessesPerSecond) {
    double entropy = calculateEntropy(password);
    double possibleCombinations = std::pow(2.0, entropy);
    return possibleCombinations / (2.0 * guessesPerSecond);  // Average case
}

bool PasswordValidator::hasRepeatedPatterns(std::string_view password) {
    if (password.length() < 3)
        return false;

    // Check for repeated characters (3 or more in a row)
    for (size_t i = 0; i < password.length() - 2; ++i) {
        if (password[i] == password[i + 1] && password[i] == password[i + 2]) {
            return true;
        }
    }

    // Check for repeated substrings
    for (size_t len = 2; len <= password.length() / 2; ++len) {
        for (size_t i = 0; i <= password.length() - 2 * len; ++i) {
            std::string_view pattern = password.substr(i, len);
            std::string_view next = password.substr(i + len, len);
            if (pattern == next) {
                return true;
            }
        }
    }

    return false;
}

bool PasswordValidator::hasSequentialPatterns(std::string_view password) {
    if (password.length() < 3)
        return false;

    // Check for sequential characters (ascending or descending)
    for (size_t i = 0; i < password.length() - 2; ++i) {
        char c1 = password[i], c2 = password[i + 1], c3 = password[i + 2];

        // Ascending sequence
        if (c2 == c1 + 1 && c3 == c2 + 1) {
            return true;
        }

        // Descending sequence
        if (c2 == c1 - 1 && c3 == c2 - 1) {
            return true;
        }
    }

    return false;
}

int PasswordValidator::calculateCharacterSetSize(std::string_view password) {
    bool hasLower = false, hasUpper = false, hasDigit = false,
         hasSpecial = false;

    for (char c : password) {
        if (std::islower(c))
            hasLower = true;
        else if (std::isupper(c))
            hasUpper = true;
        else if (std::isdigit(c))
            hasDigit = true;
        else
            hasSpecial = true;
    }

    int size = 0;
    if (hasLower)
        size += 26;
    if (hasUpper)
        size += 26;
    if (hasDigit)
        size += 10;
    if (hasSpecial)
        size += 32;  // Approximate number of common special characters

    return size;
}

const std::vector<std::string>& PasswordValidator::getCommonPasswords() {
    static const std::vector<std::string> commonPasswords = {
        "password", "123456",  "password123", "admin",      "qwerty",
        "letmein",  "welcome", "monkey",      "1234567890", "abc123",
        "111111",   "dragon",  "master",      "princess",   "login",
        "guest",    "solo",    "sunshine",    "shadow",     "football",
        "jesus",    "michael", "ninja",       "mustang",    "password1"};
    return commonPasswords;
}

// ============================================================================
// SecureComparison Implementation
// ============================================================================

bool SecureComparison::constantTimeEquals(std::string_view a,
                                          std::string_view b) noexcept {
    if (a.length() != b.length()) {
        return false;
    }

    return constantTimeEquals(a.data(), b.data(), a.length());
}

bool SecureComparison::constantTimeEquals(const void* a, const void* b,
                                          size_t size) noexcept {
    const unsigned char* pa = static_cast<const unsigned char*>(a);
    const unsigned char* pb = static_cast<const unsigned char*>(b);

    unsigned char result = 0;
    for (size_t i = 0; i < size; ++i) {
        result |= pa[i] ^ pb[i];
    }

    return result == 0;
}

}  // namespace atom::secret
