#ifndef ATOM_EXTRA_CURL_RESPONSE_HPP
#define ATOM_EXTRA_CURL_RESPONSE_HPP

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace atom::extra::curl {
/**
 * @brief Represents an HTTP response.
 *
 * This class encapsulates the data associated with an HTTP response,
 * including the status code, body, and headers.
 */
class Response {
public:
    /**
     * @brief Default constructor for the Response class.
     *
     * Initializes a new Response object with default values.
     */
    Response();

    /**
     * @brief Constructor for the Response class.
     *
     * @param status_code The HTTP status code of the response.
     * @param body The body of the response as a vector of characters.
     * @param headers A map of header names to header values.
     */
    Response(int status_code, std::vector<char> body,
             std::map<std::string, std::string> headers);

    /**
     * @brief Gets the HTTP status code of the response.
     *
     * @return The HTTP status code of the response.
     */
    int status_code() const noexcept;

    /**
     * @brief Gets the body of the response as a vector of characters.
     *
     * @return The body of the response as a vector of characters.
     */
    const std::vector<char>& body() const noexcept;

    /**
     * @brief Gets the body of the response as a string.
     *
     * @return The body of the response as a string.
     */
    std::string body_string() const;

    /**
     * @brief Parses the response body as JSON and returns it as a string.
     *
     * @return The JSON representation of the response body as a string.
     */
    std::string json() const;

    /**
     * @brief Gets the headers of the response.
     *
     * @return A map of header names to header values.
     */
    const std::map<std::string, std::string>& headers() const noexcept;

    /**
     * @brief Checks if the response is successful (200-299 status code).
     *
     * @return True if the response is successful, false otherwise.
     */
    bool ok() const noexcept;

    /**
     * @brief Checks if the response is a redirect (300-399 status code).
     *
     * @return True if the response is a redirect, false otherwise.
     */
    bool redirect() const noexcept;

    /**
     * @brief Checks if the response is a client error (400-499 status code).
     *
     * @return True if the response is a client error, false otherwise.
     */
    bool client_error() const noexcept;

    /**
     * @brief Checks if the response is a server error (500-599 status code).
     *
     * @return True if the response is a server error, false otherwise.
     */
    bool server_error() const noexcept;

    /**
     * @brief Checks if the response has a header with the given name.
     *
     * @param name The name of the header to check for.
     * @return True if the response has a header with the given name, false
     * otherwise.
     */
    bool has_header(const std::string& name) const;

    /**
     * @brief Gets the value of the header with the given name.
     *
     * @param name The name of the header to get the value of.
     * @return The value of the header with the given name, or an empty string
     * if the header is not found.
     */
    std::string get_header(const std::string& name) const;

    /**
     * @brief Gets the content type of the response.
     *
     * @return An optional string containing the content type of the response,
     * or std::nullopt if the content type is not set.
     */
    std::optional<std::string> content_type() const;

    /**
     * @brief Gets the content length of the response.
     *
     * @return An optional size_t containing the content length of the response,
     * or std::nullopt if the content length is not set or is invalid.
     */
    std::optional<size_t> content_length() const;

private:
    /** @brief The HTTP status code of the response. */
    int status_code_;
    /** @brief The body of the response as a vector of characters. */
    std::vector<char> body_;
    /** @brief A map of header names to header values. */
    std::map<std::string, std::string> headers_;
};
}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_RESPONSE_HPP