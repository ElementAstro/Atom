#include "breach_checker.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

#include <openssl/sha.h>

#include "atom/algorithm/encoding/base.hpp"

namespace atom::secret {

// Common breached passwords (top 100 most common)
static const std::vector<std::string> DEFAULT_COMMON_PASSWORDS = {
    "123456",   "password", "12345678",   "qwerty",   "123456789", "12345",
    "1234",     "111111",   "1234567",    "dragon",   "123123",    "baseball",
    "abc123",   "football", "monkey",     "letmein",  "696969",    "shadow",
    "master",   "666666",   "qwertyuiop", "123321",   "mustang",   "1234567890",
    "michael",  "654321",   "pussy",      "superman", "1qaz2wsx",  "7777777",
    "fuckyou",  "121212",   "000000",     "qazwsx",   "123qwe",    "killer",
    "trustno1", "jordan",   "jennifer",   "zxcvbnm",  "asdfgh",    "hunter",
    "buster",   "soccer",   "harley",     "batman",   "andrew",    "tigger",
    "sunshine", "iloveyou", "fuckme",     "2000",     "charlie",   "robert",
    "thomas",   "hockey",   "ranger",     "daniel",   "starwars",  "klaster",
    "112233",   "george",   "asshole",    "computer", "michelle",  "jessica",
    "pepper",   "1111",     "zxcvbn",     "555555",   "11111111",  "131313",
    "freedom",  "777777",   "pass",       "fuck",     "maggie",    "159753",
    "aaaaaa",   "ginger",   "princess",   "joshua",   "cheese",    "amanda",
    "summer",   "love",     "ashley",     "6969",     "nicole",    "chelsea",
    "biteme",   "matthew",  "access",     "yankees",  "987654321", "dallas",
    "austin",   "thunder",  "taylor",     "matrix"};

std::vector<std::string>& BreachChecker::getCommonPasswords() {
    static std::vector<std::string> passwords(DEFAULT_COMMON_PASSWORDS.begin(),
                                              DEFAULT_COMMON_PASSWORDS.end());
    return passwords;
}

bool BreachChecker::isCommonBreachedPassword(std::string_view password) {
    if (password.empty()) {
        return false;
    }

    // Convert to lowercase for comparison
    std::string lowerPassword;
    lowerPassword.reserve(password.length());
    for (char c : password) {
        lowerPassword +=
            static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    const auto& commonPasswords = getCommonPasswords();
    for (const auto& common : commonPasswords) {
        std::string lowerCommon;
        lowerCommon.reserve(common.length());
        for (char c : common) {
            lowerCommon +=
                static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (lowerPassword == lowerCommon) {
            return true;
        }
    }

    return false;
}

Result<std::string> BreachChecker::getHashPrefix(std::string_view password) {
    if (password.empty()) {
        return Result<std::string>::error(ErrorCode::InvalidArgument,
                                          "Password cannot be empty");
    }

    // Compute SHA-1 hash
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(password.data()),
         password.length(), hash);

    // Convert to uppercase hex and return first 5 characters
    std::string hexHash = atom::algorithm::encodeHex(
        std::span<const uint8_t>(hash, SHA_DIGEST_LENGTH), true);

    if (hexHash.length() < 5) {
        return Result<std::string>::error(ErrorCode::HashFailed,
                                          "Failed to compute hash");
    }

    return Result<std::string>::success(hexHash.substr(0, 5));
}

Result<std::string> BreachChecker::getHashSuffix(std::string_view password) {
    if (password.empty()) {
        return Result<std::string>::error(ErrorCode::InvalidArgument,
                                          "Password cannot be empty");
    }

    // Compute SHA-1 hash
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(password.data()),
         password.length(), hash);

    // Convert to uppercase hex and return characters 6-40
    std::string hexHash = atom::algorithm::encodeHex(
        std::span<const uint8_t>(hash, SHA_DIGEST_LENGTH), true);

    if (hexHash.length() < 40) {
        return Result<std::string>::error(ErrorCode::HashFailed,
                                          "Failed to compute hash");
    }

    return Result<std::string>::success(hexHash.substr(5));
}

BreachCheckResult BreachChecker::checkSuffixInResponse(
    std::string_view suffix, std::string_view apiResponse) {
    BreachCheckResult result;
    result.source = "HaveIBeenPwned";

    if (suffix.empty() || apiResponse.empty()) {
        return result;
    }

    // Convert suffix to uppercase for comparison
    std::string upperSuffix;
    upperSuffix.reserve(suffix.length());
    for (char c : suffix) {
        upperSuffix +=
            static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    // Parse response (format: SUFFIX:COUNT\r\n)
    std::istringstream stream{std::string(apiResponse)};
    std::string line;

    while (std::getline(stream, line)) {
        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) {
            continue;
        }

        std::string lineSuffix = line.substr(0, colonPos);

        // Convert to uppercase for comparison
        for (char& c : lineSuffix) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }

        if (lineSuffix == upperSuffix) {
            result.isBreached = true;
            try {
                result.occurrences = std::stoi(line.substr(colonPos + 1));
            } catch (...) {
                result.occurrences = 1;
            }
            break;
        }
    }

    return result;
}

void BreachChecker::addToCommonList(std::string_view password) {
    if (password.empty()) {
        return;
    }

    auto& passwords = getCommonPasswords();

    // Check if already exists
    std::string pwdStr(password);
    for (const auto& existing : passwords) {
        if (existing == pwdStr) {
            return;
        }
    }

    passwords.push_back(pwdStr);
}

size_t BreachChecker::getCommonListSize() {
    return getCommonPasswords().size();
}

void BreachChecker::clearCommonList() { getCommonPasswords().clear(); }

}  // namespace atom::secret
