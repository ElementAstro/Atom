#include "common.hpp"

#include <random>
#include <set>
#include <cmath>
#include <algorithm>

namespace atom::secret::utils {

std::string generatePassword(const PasswordGenerationOptions& options) {
    if (options.length <= 0) {
        return "";
    }

    std::string charset;

    // Use custom charset if provided
    if (!options.customCharset.empty()) {
        charset = options.customCharset;
    } else {
        // Build charset based on options
        if (options.includeLowercase) {
            charset += "abcdefghijklmnopqrstuvwxyz";
        }
        if (options.includeUppercase) {
            charset += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        }
        if (options.includeNumbers) {
            charset += "0123456789";
        }
        if (options.includeSpecialChars) {
            charset += "!@#$%^&*()_+-=[]{}|;:,.<>?";
        }
    }

    // Remove excluded characters
    if (!options.excludeChars.empty()) {
        for (char c : options.excludeChars) {
            charset.erase(std::remove(charset.begin(), charset.end(), c), charset.end());
        }
    }

    // Remove similar/ambiguous characters if requested
    if (options.excludeSimilarChars) {
        std::string similarChars = "0O1lI";
        for (char c : similarChars) {
            charset.erase(std::remove(charset.begin(), charset.end(), c), charset.end());
        }
    }

    if (options.excludeAmbiguousChars) {
        std::string ambiguousChars = "{}[]()\\~,;.<>\"'";
        for (char c : ambiguousChars) {
            charset.erase(std::remove(charset.begin(), charset.end(), c), charset.end());
        }
    }

    if (charset.empty()) {
        return "";
    }

    // Initialize random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, charset.size() - 1);

    std::string password;
    password.reserve(options.length);

    // If requireFromEachSet is true, ensure at least one character from each enabled set
    if (options.requireFromEachSet && options.customCharset.empty()) {
        std::vector<std::string> charSets;

        if (options.includeLowercase) {
            charSets.push_back("abcdefghijklmnopqrstuvwxyz");
        }
        if (options.includeUppercase) {
            charSets.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        }
        if (options.includeNumbers) {
            charSets.push_back("0123456789");
        }
        if (options.includeSpecialChars) {
            charSets.push_back("!@#$%^&*()_+-=[]{}|;:,.<>?");
        }

        // Add one character from each set
        for (const auto& set : charSets) {
            if (password.length() < static_cast<size_t>(options.length)) {
                std::string filteredSet = set;

                // Apply exclusions to each set
                if (!options.excludeChars.empty()) {
                    for (char c : options.excludeChars) {
                        filteredSet.erase(std::remove(filteredSet.begin(), filteredSet.end(), c), filteredSet.end());
                    }
                }

                if (options.excludeSimilarChars) {
                    std::string similarChars = "0O1lI";
                    for (char c : similarChars) {
                        filteredSet.erase(std::remove(filteredSet.begin(), filteredSet.end(), c), filteredSet.end());
                    }
                }

                if (options.excludeAmbiguousChars) {
                    std::string ambiguousChars = "{}[]()\\~,;.<>\"'";
                    for (char c : ambiguousChars) {
                        filteredSet.erase(std::remove(filteredSet.begin(), filteredSet.end(), c), filteredSet.end());
                    }
                }

                if (!filteredSet.empty()) {
                    std::uniform_int_distribution<> setDis(0, filteredSet.size() - 1);
                    password += filteredSet[setDis(gen)];
                }
            }
        }
    }

    // Fill the rest with random characters from the full charset
    while (password.length() < static_cast<size_t>(options.length)) {
        password += charset[dis(gen)];
    }

    // Shuffle the password to avoid predictable patterns
    std::shuffle(password.begin(), password.end(), gen);

    return password;
}

double calculatePasswordEntropy(std::string_view password) {
    if (password.empty()) {
        return 0.0;
    }

    // Determine character set size
    bool hasLower = false, hasUpper = false, hasDigit = false, hasSpecial = false;
    std::set<char> uniqueChars;

    for (char c : password) {
        uniqueChars.insert(c);
        if (std::islower(c)) hasLower = true;
        else if (std::isupper(c)) hasUpper = true;
        else if (std::isdigit(c)) hasDigit = true;
        else hasSpecial = true;
    }

    // Calculate character space size
    int charSpaceSize = 0;
    if (hasLower) charSpaceSize += 26;
    if (hasUpper) charSpaceSize += 26;
    if (hasDigit) charSpaceSize += 10;
    if (hasSpecial) charSpaceSize += 32; // Approximate number of common special characters

    // Use the larger of actual unique characters or estimated character space
    int effectiveCharSpace = std::max(static_cast<int>(uniqueChars.size()), charSpaceSize);

    // Calculate entropy: log2(character_space^length)
    double entropy = password.length() * std::log2(effectiveCharSpace);

    return entropy;
}

} // namespace atom::secret::utils
