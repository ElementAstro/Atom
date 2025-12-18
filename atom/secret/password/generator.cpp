#include "generator.hpp"

#include <openssl/rand.h>
#include <algorithm>
#include <cstring>

namespace atom::secret {

Result<std::string> PasswordGenerator::generate() {
    return generate(PasswordGeneratorOptions::defaults());
}

Result<std::string> PasswordGenerator::generate(
    const PasswordGeneratorOptions& options) {
    std::string error = validateOptions(options);
    if (!error.empty()) {
        return Result<std::string>::error(ErrorCode::InvalidArgument, error);
    }

    std::string charset = buildCharacterSet(options);
    if (charset.empty()) {
        return Result<std::string>::error(
            ErrorCode::InvalidArgument,
            "No character set available with given options");
    }

    std::string password;
    password.reserve(options.length);

    // Generate initial password
    for (int i = 0; i < options.length; ++i) {
        char c = getRandomChar(charset);
        if (c == '\0') {
            return Result<std::string>::error(
                ErrorCode::RandomGenerationFailed,
                "Failed to generate random character");
        }

        // Check for repeating characters if required
        if (options.noRepeatingChars && !password.empty() &&
            password.back() == c) {
            --i;  // Try again
            continue;
        }

        password += c;
    }

    // Ensure minimum requirements are met
    password = ensureMinimumRequirements(password, options);

    // Ensure starts with letter if required
    if (options.startWithLetter) {
        std::string letters;
        if (options.includeLowercase)
            letters += CharacterSets::LOWERCASE;
        if (options.includeUppercase)
            letters += CharacterSets::UPPERCASE;

        if (!letters.empty() && !password.empty()) {
            bool startsWithLetter = false;
            for (char c : letters) {
                if (password[0] == c) {
                    startsWithLetter = true;
                    break;
                }
            }
            if (!startsWithLetter) {
                // Find a letter in the password and swap with first char
                for (size_t i = 1; i < password.size(); ++i) {
                    for (char c : letters) {
                        if (password[i] == c) {
                            std::swap(password[0], password[i]);
                            goto done;
                        }
                    }
                }
                // No letter found, replace first char
                password[0] = getRandomChar(letters);
            done:;
            }
        }
    }

    return Result<std::string>::success(std::move(password));
}

Result<std::string> PasswordGenerator::generatePin(int length) {
    return generate(PasswordGeneratorOptions::pin(length));
}

Result<std::string> PasswordGenerator::generatePassphrase(
    int wordCount, const std::string& separator, bool capitalize,
    bool includeNumber) {
    if (wordCount < 1 || wordCount > 20) {
        return Result<std::string>::error(
            ErrorCode::InvalidArgument, "Word count must be between 1 and 20");
    }

    const auto& words = getWordList();
    if (words.empty()) {
        return Result<std::string>::error(ErrorCode::InvalidArgument,
                                          "Word list not available");
    }

    std::string passphrase;

    for (int i = 0; i < wordCount; ++i) {
        if (i > 0) {
            passphrase += separator;
        }

        size_t idx = getRandomIndex(words.size());
        std::string word = words[idx];

        if (capitalize && !word.empty()) {
            word[0] = static_cast<char>(
                std::toupper(static_cast<unsigned char>(word[0])));
        }

        passphrase += word;
    }

    if (includeNumber) {
        // Add a random number at a random position
        size_t pos = getRandomIndex(passphrase.size() + 1);
        int num = static_cast<int>(getRandomIndex(100));
        passphrase.insert(pos, std::to_string(num));
    }

    return Result<std::string>::success(std::move(passphrase));
}

Result<std::string> PasswordGenerator::generatePronounceable(int length) {
    if (length < 4 || length > 64) {
        return Result<std::string>::error(ErrorCode::InvalidArgument,
                                          "Length must be between 4 and 64");
    }

    static const char* vowels = "aeiou";
    static const char* consonants = "bcdfghjklmnpqrstvwxyz";

    std::string password;
    password.reserve(length);

    bool useConsonant = getRandomIndex(2) == 0;

    while (static_cast<int>(password.size()) < length) {
        if (useConsonant) {
            password += consonants[getRandomIndex(21)];
        } else {
            password += vowels[getRandomIndex(5)];
        }
        useConsonant = !useConsonant;
    }

    // Add some uppercase and digits
    if (password.size() >= 2) {
        size_t upperPos = getRandomIndex(password.size());
        password[upperPos] = static_cast<char>(
            std::toupper(static_cast<unsigned char>(password[upperPos])));
    }

    if (password.size() >= 4) {
        password += static_cast<char>('0' + getRandomIndex(10));
    }

    return Result<std::string>::success(std::move(password));
}

Result<std::vector<std::string>> PasswordGenerator::generateMultiple(
    int count, const PasswordGeneratorOptions& options) {
    if (count < 1 || count > 100) {
        return Result<std::vector<std::string>>::error(
            ErrorCode::InvalidArgument, "Count must be between 1 and 100");
    }

    std::vector<std::string> passwords;
    passwords.reserve(count);

    for (int i = 0; i < count; ++i) {
        auto result = generate(options);
        if (result.isError()) {
            return Result<std::vector<std::string>>::error(
                result.errorCode(), result.errorMessage());
        }
        passwords.push_back(std::move(result.value()));
    }

    return Result<std::vector<std::string>>::success(std::move(passwords));
}

std::string PasswordGenerator::validateOptions(
    const PasswordGeneratorOptions& options) {
    if (options.length < 4) {
        return "Password length must be at least 4";
    }
    if (options.length > 256) {
        return "Password length must not exceed 256";
    }

    int minRequired = options.minLowercase + options.minUppercase +
                      options.minDigits + options.minSpecial;
    if (minRequired > options.length) {
        return "Minimum character requirements exceed password length";
    }

    if (!options.includeLowercase && !options.includeUppercase &&
        !options.includeDigits && !options.includeSpecial &&
        options.customCharacters.empty()) {
        return "At least one character type must be included";
    }

    return "";
}

std::string PasswordGenerator::buildCharacterSet(
    const PasswordGeneratorOptions& options) {
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

    charset += options.customCharacters;

    // Remove excluded characters
    if (options.excludeAmbiguous) {
        for (char c : std::string(CharacterSets::AMBIGUOUS)) {
            charset.erase(std::remove(charset.begin(), charset.end(), c),
                          charset.end());
        }
    }
    if (options.excludeBrackets) {
        for (char c : std::string(CharacterSets::BRACKETS)) {
            charset.erase(std::remove(charset.begin(), charset.end(), c),
                          charset.end());
        }
    }
    for (char c : options.excludeCharacters) {
        charset.erase(std::remove(charset.begin(), charset.end(), c),
                      charset.end());
    }

    // Remove duplicates
    std::sort(charset.begin(), charset.end());
    charset.erase(std::unique(charset.begin(), charset.end()), charset.end());

    return charset;
}

std::string PasswordGenerator::ensureMinimumRequirements(
    std::string password, const PasswordGeneratorOptions& options) {
    auto countChars = [&password](const char* set) {
        int count = 0;
        for (char c : password) {
            for (const char* s = set; *s; ++s) {
                if (c == *s) {
                    ++count;
                    break;
                }
            }
        }
        return count;
    };

    auto replaceWithChar = [&password](const char* set) {
        if (!set || !*set)
            return;
        size_t pos = getRandomIndex(password.size());
        size_t setLen = std::strlen(set);
        password[pos] = set[getRandomIndex(setLen)];
    };

    // Ensure minimum lowercase
    if (options.includeLowercase && options.minLowercase > 0) {
        while (countChars(CharacterSets::LOWERCASE) < options.minLowercase) {
            replaceWithChar(CharacterSets::LOWERCASE);
        }
    }

    // Ensure minimum uppercase
    if (options.includeUppercase && options.minUppercase > 0) {
        while (countChars(CharacterSets::UPPERCASE) < options.minUppercase) {
            replaceWithChar(CharacterSets::UPPERCASE);
        }
    }

    // Ensure minimum digits
    if (options.includeDigits && options.minDigits > 0) {
        while (countChars(CharacterSets::DIGITS) < options.minDigits) {
            replaceWithChar(CharacterSets::DIGITS);
        }
    }

    // Ensure minimum special
    if (options.includeSpecial && options.minSpecial > 0) {
        while (countChars(CharacterSets::SPECIAL) < options.minSpecial) {
            replaceWithChar(CharacterSets::SPECIAL);
        }
    }

    // Shuffle to randomize positions
    shuffleString(password);

    return password;
}

char PasswordGenerator::getRandomChar(const std::string& chars) {
    if (chars.empty())
        return '\0';
    size_t idx = getRandomIndex(chars.size());
    return chars[idx];
}

size_t PasswordGenerator::getRandomIndex(size_t max) {
    if (max == 0)
        return 0;

    unsigned char buf[4];
    if (RAND_bytes(buf, 4) != 1) {
        return 0;
    }

    uint32_t value = (static_cast<uint32_t>(buf[0]) << 24) |
                     (static_cast<uint32_t>(buf[1]) << 16) |
                     (static_cast<uint32_t>(buf[2]) << 8) |
                     static_cast<uint32_t>(buf[3]);

    return value % max;
}

void PasswordGenerator::shuffleString(std::string& str) {
    for (size_t i = str.size() - 1; i > 0; --i) {
        size_t j = getRandomIndex(i + 1);
        std::swap(str[i], str[j]);
    }
}

const std::vector<std::string>& PasswordGenerator::getWordList() {
    // A small word list for passphrase generation
    // In production, this would be loaded from a file (e.g., EFF word list)
    static const std::vector<std::string> words = {
        "apple",   "banana",  "cherry",   "dragon",  "eagle",    "falcon",
        "garden",  "hammer",  "island",   "jungle",  "kettle",   "lemon",
        "mango",   "needle",  "orange",   "pepper",  "quartz",   "rabbit",
        "salmon",  "tiger",   "umbrella", "violet",  "walnut",   "xenon",
        "yellow",  "zebra",   "anchor",   "bridge",  "castle",   "desert",
        "engine",  "forest",  "glacier",  "harbor",  "ivory",    "jacket",
        "kingdom", "lantern", "mountain", "network", "ocean",    "palace",
        "quarter", "river",   "sunset",   "thunder", "universe", "valley",
        "window",  "crystal", "diamond",  "emerald", "phoenix",  "silver",
        "golden",  "cosmic",  "stellar",  "lunar",   "solar",    "arctic",
        "tropic",  "meadow",  "canyon",   "summit",  "rapids",   "cavern",
        "beacon",  "voyage",  "quest",    "legend"};
    return words;
}

}  // namespace atom::secret
