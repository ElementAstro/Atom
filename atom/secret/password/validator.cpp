#include "validator.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace atom::secret {

std::string PasswordAnalysis::getSummary() const {
    std::ostringstream ss;
    ss << "Strength: " << strengthToString(strength) << " (Score: " << score
       << "/100)\n";
    ss << "Entropy: " << entropy << " bits\n";
    ss << "Crack time: " << crackTimeDisplay << "\n";
    ss << "Length: " << length << " characters\n";

    if (!warnings.empty()) {
        ss << "Warnings:\n";
        for (const auto& w : warnings) {
            ss << "  - " << w << "\n";
        }
    }

    if (!suggestions.empty()) {
        ss << "Suggestions:\n";
        for (const auto& s : suggestions) {
            ss << "  - " << s << "\n";
        }
    }

    return ss.str();
}

PasswordAnalysis PasswordValidator::analyze(std::string_view password) {
    PasswordAnalysis result{};

    result.length = static_cast<int>(password.length());

    // Count character types
    for (char c : password) {
        if (std::isupper(static_cast<unsigned char>(c))) {
            result.uppercaseCount++;
            result.hasUppercase = true;
        } else if (std::islower(static_cast<unsigned char>(c))) {
            result.lowercaseCount++;
            result.hasLowercase = true;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            result.digitCount++;
            result.hasDigits = true;
        } else if (!std::isspace(static_cast<unsigned char>(c))) {
            result.specialCount++;
            result.hasSpecial = true;
        }
    }

    // Count unique characters
    std::string sorted(password);
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    result.uniqueChars = static_cast<int>(sorted.length());

    // Pattern detection
    result.hasRepeatedChars = hasExcessiveRepeats(password, 3);
    result.hasSequentialChars = hasSequentialPatterns(password);
    result.hasKeyboardPattern = hasKeyboardPatterns(password);
    result.isCommonPassword = isCommonPassword(password);
    result.hasCommonSubstitutions = hasCommonSubstitutions(password);

    // Calculate entropy and crack time
    result.entropy = calculateEntropy(password);
    result.crackTimeSeconds = estimateCrackTime(password);
    result.crackTimeDisplay = formatCrackTime(result.crackTimeSeconds);

    // Calculate score
    int score = 0;

    // Length contribution (up to 25 points)
    score += std::min(25, result.length * 2);

    // Character variety (up to 25 points)
    int variety = 0;
    if (result.hasUppercase)
        variety++;
    if (result.hasLowercase)
        variety++;
    if (result.hasDigits)
        variety++;
    if (result.hasSpecial)
        variety++;
    score += variety * 6;

    // Unique characters (up to 20 points)
    double uniqueRatio =
        result.length > 0
            ? static_cast<double>(result.uniqueChars) / result.length
            : 0;
    score += static_cast<int>(uniqueRatio * 20);

    // Entropy bonus (up to 20 points)
    score += std::min(20, static_cast<int>(result.entropy / 4));

    // Penalties
    if (result.hasRepeatedChars) {
        score -= 10;
        result.warnings.push_back("Contains repeated characters");
    }
    if (result.hasSequentialChars) {
        score -= 10;
        result.warnings.push_back("Contains sequential characters");
    }
    if (result.hasKeyboardPattern) {
        score -= 15;
        result.warnings.push_back("Contains keyboard pattern");
    }
    if (result.isCommonPassword) {
        score -= 40;
        result.warnings.push_back("This is a commonly used password");
    }
    if (result.hasCommonSubstitutions) {
        score -= 5;
        result.warnings.push_back("Uses predictable character substitutions");
    }

    // Clamp score
    result.score = std::max(0, std::min(100, score));
    result.strength = scoreToStrength(result.score);

    // Generate suggestions
    if (!result.hasUppercase) {
        result.suggestions.push_back("Add uppercase letters");
    }
    if (!result.hasLowercase) {
        result.suggestions.push_back("Add lowercase letters");
    }
    if (!result.hasDigits) {
        result.suggestions.push_back("Add numbers");
    }
    if (!result.hasSpecial) {
        result.suggestions.push_back("Add special characters");
    }
    if (result.length < 12) {
        result.suggestions.push_back("Use at least 12 characters");
    }
    if (result.uniqueChars < result.length / 2) {
        result.suggestions.push_back("Use more unique characters");
    }

    return result;
}

Result<bool> PasswordValidator::validate(std::string_view password,
                                         const PasswordPolicy& policy) {
    auto errors = getValidationErrors(password, policy);
    if (errors.empty()) {
        return Result<bool>::success(true);
    }

    std::string errorMsg;
    for (size_t i = 0; i < errors.size(); ++i) {
        if (i > 0)
            errorMsg += "; ";
        errorMsg += errors[i];
    }

    return Result<bool>::error(ErrorCode::PasswordTooWeak, errorMsg);
}

std::vector<std::string> PasswordValidator::getValidationErrors(
    std::string_view password, const PasswordPolicy& policy) {
    std::vector<std::string> errors;

    int len = static_cast<int>(password.length());

    if (len < policy.minLength) {
        errors.push_back("Password must be at least " +
                         std::to_string(policy.minLength) + " characters");
    }

    if (len > policy.maxLength) {
        errors.push_back("Password must not exceed " +
                         std::to_string(policy.maxLength) + " characters");
    }

    int uppercase = 0, lowercase = 0, digits = 0, special = 0;
    for (char c : password) {
        if (std::isupper(static_cast<unsigned char>(c)))
            uppercase++;
        else if (std::islower(static_cast<unsigned char>(c)))
            lowercase++;
        else if (std::isdigit(static_cast<unsigned char>(c)))
            digits++;
        else if (!std::isspace(static_cast<unsigned char>(c)))
            special++;
    }

    if (policy.requireUppercase && uppercase < policy.minUppercase) {
        errors.push_back("Password must contain at least " +
                         std::to_string(policy.minUppercase) +
                         " uppercase letter(s)");
    }

    if (policy.requireLowercase && lowercase < policy.minLowercase) {
        errors.push_back("Password must contain at least " +
                         std::to_string(policy.minLowercase) +
                         " lowercase letter(s)");
    }

    if (policy.requireDigit && digits < policy.minDigits) {
        errors.push_back("Password must contain at least " +
                         std::to_string(policy.minDigits) + " digit(s)");
    }

    if (policy.requireSpecial && special < policy.minSpecial) {
        errors.push_back("Password must contain at least " +
                         std::to_string(policy.minSpecial) +
                         " special character(s)");
    }

    if (policy.disallowCommonPasswords && isCommonPassword(password)) {
        errors.push_back("This password is too common");
    }

    if (policy.disallowRepeatingChars &&
        hasExcessiveRepeats(password, policy.maxConsecutiveRepeats)) {
        errors.push_back("Password contains too many repeating characters");
    }

    if (policy.disallowSequentialChars && hasSequentialPatterns(password)) {
        errors.push_back("Password contains sequential characters");
    }

    if (!policy.disallowedWords.empty() &&
        containsWord(password, policy.disallowedWords)) {
        errors.push_back("Password contains a disallowed word");
    }

    // Check minimum strength
    auto analysis = analyze(password);
    if (analysis.strength < policy.minStrength) {
        errors.push_back("Password does not meet minimum strength requirement");
    }

    return errors;
}

double PasswordValidator::calculateEntropy(std::string_view password) {
    if (password.empty())
        return 0.0;

    int charsetSize = calculateCharsetSize(password);
    if (charsetSize == 0)
        return 0.0;

    return password.length() * std::log2(charsetSize);
}

double PasswordValidator::estimateCrackTime(std::string_view password,
                                            double guessesPerSecond) {
    double entropy = calculateEntropy(password);
    double combinations = std::pow(2.0, entropy);
    return combinations / (2.0 * guessesPerSecond);  // Average case
}

std::string PasswordValidator::formatCrackTime(double seconds) {
    if (seconds < 1)
        return "instant";
    if (seconds < 60)
        return std::to_string(static_cast<int>(seconds)) + " seconds";
    if (seconds < 3600)
        return std::to_string(static_cast<int>(seconds / 60)) + " minutes";
    if (seconds < 86400)
        return std::to_string(static_cast<int>(seconds / 3600)) + " hours";
    if (seconds < 31536000)
        return std::to_string(static_cast<int>(seconds / 86400)) + " days";
    if (seconds < 31536000 * 100)
        return std::to_string(static_cast<int>(seconds / 31536000)) + " years";
    if (seconds < 31536000 * 1000000)
        return std::to_string(static_cast<int>(seconds / 31536000)) + " years";
    return "centuries";
}

bool PasswordValidator::isCommonPassword(std::string_view password) {
    std::string lower(password);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    const auto& common = getCommonPasswords();
    return std::find(common.begin(), common.end(), lower) != common.end();
}

bool PasswordValidator::containsWord(std::string_view password,
                                     const std::vector<std::string>& words) {
    std::string lower(password);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto& word : words) {
        std::string lowerWord = word;
        std::transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (lower.find(lowerWord) != std::string::npos) {
            return true;
        }
    }
    return false;
}

PasswordStrength PasswordValidator::scoreToStrength(int score) {
    if (score >= 80)
        return PasswordStrength::VeryStrong;
    if (score >= 60)
        return PasswordStrength::Strong;
    if (score >= 40)
        return PasswordStrength::Medium;
    if (score >= 20)
        return PasswordStrength::Weak;
    return PasswordStrength::VeryWeak;
}

int PasswordValidator::calculateCharsetSize(std::string_view password) {
    bool hasLower = false, hasUpper = false, hasDigit = false,
         hasSpecial = false;

    for (char c : password) {
        if (std::islower(static_cast<unsigned char>(c)))
            hasLower = true;
        else if (std::isupper(static_cast<unsigned char>(c)))
            hasUpper = true;
        else if (std::isdigit(static_cast<unsigned char>(c)))
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
        size += 32;

    return size;
}

bool PasswordValidator::hasExcessiveRepeats(std::string_view password,
                                            int maxRepeats) {
    if (password.length() < static_cast<size_t>(maxRepeats))
        return false;

    int count = 1;
    for (size_t i = 1; i < password.length(); ++i) {
        if (password[i] == password[i - 1]) {
            count++;
            if (count >= maxRepeats)
                return true;
        } else {
            count = 1;
        }
    }
    return false;
}

bool PasswordValidator::hasSequentialPatterns(std::string_view password) {
    if (password.length() < 3)
        return false;

    for (size_t i = 0; i < password.length() - 2; ++i) {
        char c1 = password[i];
        char c2 = password[i + 1];
        char c3 = password[i + 2];

        // Check ascending sequence
        if (c2 == c1 + 1 && c3 == c2 + 1)
            return true;
        // Check descending sequence
        if (c2 == c1 - 1 && c3 == c2 - 1)
            return true;
    }
    return false;
}

bool PasswordValidator::hasKeyboardPatterns(std::string_view password) {
    std::string lower(password);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    const auto& patterns = getKeyboardPatterns();
    for (const auto& pattern : patterns) {
        if (lower.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool PasswordValidator::hasCommonSubstitutions(std::string_view password) {
    // Check for common l33t speak substitutions
    static const std::vector<std::pair<char, char>> substitutions = {
        {'@', 'a'}, {'4', 'a'}, {'3', 'e'}, {'1', 'i'}, {'1', 'l'},
        {'0', 'o'}, {'$', 's'}, {'5', 's'}, {'7', 't'}, {'!', 'i'}};

    int count = 0;
    for (char c : password) {
        for (const auto& sub : substitutions) {
            if (c == sub.first) {
                count++;
                break;
            }
        }
    }

    return count >= 2;  // Multiple substitutions suggest l33t speak
}

const std::vector<std::string>& PasswordValidator::getCommonPasswords() {
    static const std::vector<std::string> passwords = {
        "password",    "123456",   "12345678", "qwerty",   "abc123",
        "monkey",      "1234567",  "letmein",  "trustno1", "dragon",
        "baseball",    "iloveyou", "master",   "sunshine", "ashley",
        "bailey",      "passw0rd", "shadow",   "123123",   "654321",
        "superman",    "qazwsx",   "michael",  "football", "password1",
        "password123", "welcome",  "jesus",    "ninja",    "mustang",
        "password1!",  "admin",    "login",    "hello",    "charlie",
        "donald",      "loveme",   "hockey",   "ranger",   "thomas",
        "klaster",     "george",   "asshole",  "fuckyou",  "summer",
        "harley",      "ginger",   "joshua",   "pepper",   "hunter",
        "cheese",      "butter",   "killer",   "andrew"};
    return passwords;
}

const std::vector<std::string>& PasswordValidator::getKeyboardPatterns() {
    static const std::vector<std::string> patterns = {
        "qwerty", "qwertz", "azerty", "asdf",   "zxcv",  "qwer",
        "asdfgh", "zxcvbn", "1234",   "2345",   "3456",  "4567",
        "5678",   "6789",   "7890",   "0987",   "9876",  "8765",
        "7654",   "6543",   "5432",   "4321",   "3210",  "qazwsx",
        "wsxedc", "edcrfv", "rfvtgb", "tgbyhn", "yhnujm"};
    return patterns;
}

// ============================================================================
// SecureComparison Implementation
// ============================================================================

bool SecureComparison::constantTimeEquals(std::string_view a,
                                          std::string_view b) noexcept {
    if (a.size() != b.size()) {
        return false;
    }
    return constantTimeEquals(a.data(), b.data(), a.size());
}

bool SecureComparison::constantTimeEquals(const void* a, const void* b,
                                          size_t size) noexcept {
    const auto* pa = static_cast<const unsigned char*>(a);
    const auto* pb = static_cast<const unsigned char*>(b);

    unsigned char result = 0;
    for (size_t i = 0; i < size; ++i) {
        result |= pa[i] ^ pb[i];
    }
    return result == 0;
}

}  // namespace atom::secret
