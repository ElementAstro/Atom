/*
 * generator_example.cpp
 *
 * Demonstrates password generation using the Atom Secret module.
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 * License: GPL3
 */

#include <iostream>

#include "atom/secret/password/generator.hpp"

using namespace atom::secret;

int main() {
    std::cout << "=== Atom Secret Password Generator Example ===" << std::endl
              << std::endl;

    // ========================================================================
    // Default Password Generation
    // ========================================================================
    std::cout << "--- Default Password Generation ---" << std::endl;

    for (int i = 0; i < 5; ++i) {
        auto result = PasswordGenerator::generate();
        if (result.isSuccess()) {
            std::cout << "Generated: " << result.value() << std::endl;
        }
    }
    std::cout << std::endl;

    // ========================================================================
    // Custom Length
    // ========================================================================
    std::cout << "--- Custom Length Passwords ---" << std::endl;

    PasswordGeneratorOptions config8;
    config8.length = 8;
    auto pw8 = PasswordGenerator::generate(config8);
    if (pw8.isSuccess()) {
        std::cout << "8 characters: " << pw8.value() << std::endl;
    }

    PasswordGeneratorOptions config16;
    config16.length = 16;
    auto pw16 = PasswordGenerator::generate(config16);
    if (pw16.isSuccess()) {
        std::cout << "16 characters: " << pw16.value() << std::endl;
    }

    PasswordGeneratorOptions config32;
    config32.length = 32;
    auto pw32 = PasswordGenerator::generate(config32);
    if (pw32.isSuccess()) {
        std::cout << "32 characters: " << pw32.value() << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Character Set Options
    // ========================================================================
    std::cout << "--- Character Set Options ---" << std::endl;

    // Lowercase only
    PasswordGeneratorOptions lowerConfig;
    lowerConfig.length = 16;
    lowerConfig.includeLowercase = true;
    lowerConfig.includeUppercase = false;
    lowerConfig.includeDigits = false;
    lowerConfig.includeSpecial = false;
    lowerConfig.minLowercase = 0;
    lowerConfig.minUppercase = 0;
    lowerConfig.minDigits = 0;
    auto lowerPw = PasswordGenerator::generate(lowerConfig);
    if (lowerPw.isSuccess()) {
        std::cout << "Lowercase only: " << lowerPw.value() << std::endl;
    }

    // Uppercase only
    PasswordGeneratorOptions upperConfig;
    upperConfig.length = 16;
    upperConfig.includeLowercase = false;
    upperConfig.includeUppercase = true;
    upperConfig.includeDigits = false;
    upperConfig.includeSpecial = false;
    upperConfig.minLowercase = 0;
    upperConfig.minUppercase = 0;
    upperConfig.minDigits = 0;
    auto upperPw = PasswordGenerator::generate(upperConfig);
    if (upperPw.isSuccess()) {
        std::cout << "Uppercase only: " << upperPw.value() << std::endl;
    }

    // Digits only
    PasswordGeneratorOptions digitConfig;
    digitConfig.length = 16;
    digitConfig.includeLowercase = false;
    digitConfig.includeUppercase = false;
    digitConfig.includeDigits = true;
    digitConfig.includeSpecial = false;
    digitConfig.minLowercase = 0;
    digitConfig.minUppercase = 0;
    digitConfig.minDigits = 0;
    auto digitPw = PasswordGenerator::generate(digitConfig);
    if (digitPw.isSuccess()) {
        std::cout << "Digits only: " << digitPw.value() << std::endl;
    }

    // Alphanumeric (no special)
    PasswordGeneratorOptions alphanumConfig;
    alphanumConfig.length = 16;
    alphanumConfig.includeLowercase = true;
    alphanumConfig.includeUppercase = true;
    alphanumConfig.includeDigits = true;
    alphanumConfig.includeSpecial = false;
    auto alphanumPw = PasswordGenerator::generate(alphanumConfig);
    if (alphanumPw.isSuccess()) {
        std::cout << "Alphanumeric: " << alphanumPw.value() << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Exclude Ambiguous Characters
    // ========================================================================
    std::cout << "--- Exclude Ambiguous Characters ---" << std::endl;

    PasswordGeneratorOptions noAmbigConfig;
    noAmbigConfig.length = 20;
    noAmbigConfig.excludeAmbiguous = true;

    std::cout << "Without ambiguous chars (0O1lI):" << std::endl;
    for (int i = 0; i < 3; ++i) {
        auto result = PasswordGenerator::generate(noAmbigConfig);
        if (result.isSuccess()) {
            std::cout << "  " << result.value() << std::endl;
        }
    }
    std::cout << std::endl;

    // ========================================================================
    // Preset Options
    // ========================================================================
    std::cout << "--- Preset Options ---" << std::endl;

    auto strongPw =
        PasswordGenerator::generate(PasswordGeneratorOptions::strong());
    if (strongPw.isSuccess()) {
        std::cout << "Strong preset: " << strongPw.value() << std::endl;
    }

    auto readablePw =
        PasswordGenerator::generate(PasswordGeneratorOptions::readable());
    if (readablePw.isSuccess()) {
        std::cout << "Readable preset: " << readablePw.value() << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // Passphrase Generation
    // ========================================================================
    std::cout << "--- Passphrase Generation ---" << std::endl;

    auto passphrase4 = PasswordGenerator::generatePassphrase(4);
    if (passphrase4.isSuccess()) {
        std::cout << "4 words: " << passphrase4.value() << std::endl;
    }

    auto passphrase6 = PasswordGenerator::generatePassphrase(6);
    if (passphrase6.isSuccess()) {
        std::cout << "6 words: " << passphrase6.value() << std::endl;
    }

    auto passphraseCustom = PasswordGenerator::generatePassphrase(4, "_");
    if (passphraseCustom.isSuccess()) {
        std::cout << "4 words (underscore): " << passphraseCustom.value()
                  << std::endl;
    }
    std::cout << std::endl;

    // ========================================================================
    // PIN Generation
    // ========================================================================
    std::cout << "--- PIN Generation ---" << std::endl;

    auto pin4 = PasswordGenerator::generatePin(4);
    if (pin4.isSuccess()) {
        std::cout << "4-digit PIN: " << pin4.value() << std::endl;
    }

    auto pin6 = PasswordGenerator::generatePin(6);
    if (pin6.isSuccess()) {
        std::cout << "6-digit PIN: " << pin6.value() << std::endl;
    }

    auto pin8 = PasswordGenerator::generatePin(8);
    if (pin8.isSuccess()) {
        std::cout << "8-digit PIN: " << pin8.value() << std::endl;
    }

    std::cout << std::endl << "=== Example Complete ===" << std::endl;
    return 0;
}
