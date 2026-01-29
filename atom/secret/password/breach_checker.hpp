#ifndef ATOM_SECRET_PASSWORD_BREACH_CHECKER_HPP
#define ATOM_SECRET_PASSWORD_BREACH_CHECKER_HPP

#include <string>
#include <string_view>
#include <vector>

#include "../core/result.hpp"

namespace atom::secret {

/**
 * @brief Result of a breach check.
 */
struct BreachCheckResult {
    bool isBreached;     ///< Whether the password was found in breaches
    int occurrences;     ///< Number of times found (if available)
    std::string source;  ///< Source of the check (e.g., "HaveIBeenPwned")

    BreachCheckResult() : isBreached(false), occurrences(0) {}
    BreachCheckResult(bool breached, int count = 0, const std::string& src = "")
        : isBreached(breached), occurrences(count), source(src) {}
};

/**
 * @brief Password breach checking utilities.
 *
 * Provides methods to check if passwords have been exposed in known data
 * breaches. Uses k-anonymity to protect the password being checked.
 */
class BreachChecker {
public:
    /**
     * @brief Checks a password against a local list of common breached
     * passwords.
     *
     * This is a fast, offline check against a small list of the most common
     * breached passwords.
     *
     * @param password Password to check.
     * @return True if password is in the common breached list.
     */
    static bool isCommonBreachedPassword(std::string_view password);

    /**
     * @brief Computes the SHA-1 hash prefix for k-anonymity API calls.
     *
     * The HaveIBeenPwned API uses k-anonymity where only the first 5 characters
     * of the SHA-1 hash are sent to the server.
     *
     * @param password Password to hash.
     * @return Result containing the 5-character hash prefix or error.
     */
    static Result<std::string> getHashPrefix(std::string_view password);

    /**
     * @brief Computes the full SHA-1 hash suffix for local comparison.
     *
     * @param password Password to hash.
     * @return Result containing the hash suffix (characters 6-40) or error.
     */
    static Result<std::string> getHashSuffix(std::string_view password);

    /**
     * @brief Checks if a hash suffix appears in an API response.
     *
     * @param suffix The hash suffix to search for.
     * @param apiResponse The API response containing suffix:count pairs.
     * @return BreachCheckResult with occurrence count if found.
     */
    static BreachCheckResult checkSuffixInResponse(
        std::string_view suffix, std::string_view apiResponse);

    /**
     * @brief Adds a password to the local common breached list.
     *
     * @param password Password to add.
     */
    static void addToCommonList(std::string_view password);

    /**
     * @brief Gets the count of passwords in the common breached list.
     *
     * @return Number of passwords in the list.
     */
    static size_t getCommonListSize();

    /**
     * @brief Clears the common breached password list.
     */
    static void clearCommonList();

private:
    static std::vector<std::string>& getCommonPasswords();
};

}  // namespace atom::secret

#endif  // ATOM_SECRET_PASSWORD_BREACH_CHECKER_HPP
