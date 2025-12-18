/*
 * validator_example.cpp
 *
 * Demonstrates password validation using the Atom Secret module.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <iostream>
#include <string>
#include <vector>

#include "atom/secret/password/entry.hpp"
#include "atom/secret/password/validator.hpp"

using namespace atom::secret;

void printValidationResult(const std::string& password,
                           const ValidationResult& result) {
    std::cout << "Password: \"" << password << "\"" << std::endl;
    std::cout << "  Valid: " << (result.isValid ? "Yes" : "No") << std::endl;
    std::cout << "  Score: " << result.score << "/100" << std::endl;
    std::cout << "  Strength: " << strengthToString(result.strength)
              << std::endl;
    std::cout << "  Entropy: " << result.entropy << " bits" << std::endl;

    if (!result.issues.empty()) {
        std::cout << "  Issues:" << std::endl;
        for (const auto& issue : result.issues) {
            std::cout << "    - " << issue << std::endl;
        }
    }

    if (!result.suggestions.empty()) {
        std::cout << "  Suggestions:" << std::endl;
        for (const auto& suggestion : result.suggestions) {
            std::cout << "    - " << suggestion << std::endl;
        }
    }
    std::cout << std::endl;
}

int main() {
    std::cout << "=== Atom Secret Password Validator Example ===" << std::endl
              << std::endl;

    // ========================================================================
    // Basic Validation
    // ========================================================================
    std::cout << "--- Basic Password Validation ---" << std::endl;

    std::vector<std::string> testPasswords = {
        "password",           "Password1", "P@ssw0rd", "MySecureP@ssw0rd!",
        "xK9#mP2$vL7@nQ4wR8", "123456",    "qwerty",   "abc123"};

    for (const auto& password : testPasswords) {
        auto result = PasswordValidator::validate(password);
        if (result.isSuccess()) {
            printValidationResult(password, result.value());
        }
    }

    // ========================================================================
    // Custom Policy
    // ========================================================================
    std::cout << "--- Custom Password Policy ---" << std::endl;

    PasswordPolicy strictPolicy;
    strictPolicy.minLength = 12;
    strictPolicy.maxLength = 128;
    strictPolicy.requireUppercase = true;
    strictPolicy.requireLowercase = true;
    strictPolicy.requireDigit = true;
    strictPolicy.requireSpecial = true;
    strictPolicy.minUniqueChars = 8;

    std::cout << "Strict policy requirements:" << std::endl;
    std::cout << "  - Minimum length: " << strictPolicy.minLength << std::endl;
    std::cout << "  - Require uppercase: Yes" << std::endl;
    std::cout << "  - Require lowercase: Yes" << std::endl;
    std::cout << "  - Require digit: Yes" << std::endl;
    std::cout << "  - Require special: Yes" << std::endl;
    std::cout << "  - Minimum unique chars: " << strictPolicy.minUniqueChars
              << std::endl;
    std::cout << std::endl;

    std::vector<std::string> policyTestPasswords = {
        "short", "LongerPassword", "LongerPassword1", "LongerP@ssword1!"};

    for (const auto& password : policyTestPasswords) {
        auto result = PasswordValidator::validate(password, strictPolicy);
        if (result.isSuccess()) {
            printValidationResult(password, result.value());
        }
    }

    // ========================================================================
    // Password History Check
    // ========================================================================
    std::cout << "--- Password History Check ---" << std::endl;

    std::vector<std::string> history = {"OldPassword1!", "OldPassword2!",
                                        "OldPassword3!"};

    PasswordPolicy historyPolicy;
    historyPolicy.checkHistory = true;

    std::cout << "Password history: OldPassword1!, OldPassword2!, OldPassword3!"
              << std::endl;
    std::cout << std::endl;

    auto historyResult1 =
        PasswordValidator::validate("OldPassword1!", historyPolicy, history);
    if (historyResult1.isSuccess()) {
        std::cout << "Trying to reuse 'OldPassword1!':" << std::endl;
        std::cout << "  Valid: "
                  << (historyResult1.value().isValid ? "Yes" : "No")
                  << std::endl;
        if (!historyResult1.value().issues.empty()) {
            std::cout << "  Issue: " << historyResult1.value().issues[0]
                      << std::endl;
        }
    }

    auto historyResult2 =
        PasswordValidator::validate("NewPassword1!", historyPolicy, history);
    if (historyResult2.isSuccess()) {
        std::cout << "Using new password 'NewPassword1!':" << std::endl;
        std::cout << "  Valid: "
                  << (historyResult2.value().isValid ? "Yes" : "No")
                  << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Entropy Calculation
    // ========================================================================
    std::cout << "--- Entropy Comparison ---" << std::endl;

    std::vector<std::string> entropyPasswords = {
        "aaaaaaaaaa", "abcdefghij", "AbCdEfGhIj", "AbCd1234!@", "xK9#mP2$vL"};

    for (const auto& password : entropyPasswords) {
        double entropy = PasswordValidator::calculateEntropy(password);
        std::cout << "\"" << password << "\" - Entropy: " << entropy << " bits"
                  << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Pattern Detection
    // ========================================================================
    std::cout << "--- Pattern Detection ---" << std::endl;

    std::cout << "Repeating characters (3+):" << std::endl;
    std::cout << "  'aaa': "
              << (PasswordValidator::hasRepeatingCharacters("aaa", 3) ? "Yes"
                                                                      : "No")
              << std::endl;
    std::cout << "  'abc': "
              << (PasswordValidator::hasRepeatingCharacters("abc", 3) ? "Yes"
                                                                      : "No")
              << std::endl;
    std::cout << "  'password111': "
              << (PasswordValidator::hasRepeatingCharacters("password111", 3)
                      ? "Yes"
                      : "No")
              << std::endl;
    std::cout << std::endl;

    std::cout << "Sequential characters (3+):" << std::endl;
    std::cout << "  'abc': "
              << (PasswordValidator::hasSequentialCharacters("abc", 3) ? "Yes"
                                                                       : "No")
              << std::endl;
    std::cout << "  '123': "
              << (PasswordValidator::hasSequentialCharacters("123", 3) ? "Yes"
                                                                       : "No")
              << std::endl;
    std::cout << "  'xyz': "
              << (PasswordValidator::hasSequentialCharacters("xyz", 3) ? "Yes"
                                                                       : "No")
              << std::endl;
    std::cout << "  'adf': "
              << (PasswordValidator::hasSequentialCharacters("adf", 3) ? "Yes"
                                                                       : "No")
              << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Common Password Check
    // ========================================================================
    std::cout << "--- Common Password Check ---" << std::endl;

    std::vector<std::string> commonCheck = {"password", "123456", "qwerty",
                                            "xK9mP2vL7nQ4"};
    for (const auto& pw : commonCheck) {
        bool isCommon = PasswordValidator::isCommonPassword(pw);
        std::cout << "\"" << pw
                  << "\": " << (isCommon ? "Common" : "Not common")
                  << std::endl;
    }

    std::cout << std::endl << "=== Example Complete ===" << std::endl;
    return 0;
}
