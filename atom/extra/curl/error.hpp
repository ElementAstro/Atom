#ifndef ATOM_EXTRA_CURL_ERROR_HPP
#define ATOM_EXTRA_CURL_ERROR_HPP

#include <curl/curl.h>
#include <optional>
#include <stdexcept>
#include <string>

namespace atom::extra::curl {

/**
 * @brief Custom exception class for curl errors.
 *
 * This class inherits from std::runtime_error and provides additional
 * information about the curl error, such as the curl error code and message.
 */
class Error : public std::runtime_error {
public:
    /**
     * @brief Constructor for the Error class.
     *
     * @param code The curl error code.
     * @param message The error message.
     */
    explicit Error(CURLcode code, std::string message);

    /**
     * @brief Constructor for the Error class when a multi error occurs.
     *
     * @param code The curl multi error code.
     * @param message The error message.
     */
    explicit Error(CURLMcode code, std::string message);

    /**
     * @brief Gets the curl error code.
     *
     * @return The curl error code.
     */
    CURLcode code() const noexcept;

    /**
     * @brief Gets the curl multi error code, if available.
     *
     * @return The curl multi error code, or std::nullopt if not available.
     */
    std::optional<CURLMcode> multi_code() const noexcept;

private:
    /** @brief The curl error code. */
    CURLcode code_;
    /** @brief The curl multi error code (optional). */
    std::optional<CURLMcode> multi_code_;
};

}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_ERROR_HPP
