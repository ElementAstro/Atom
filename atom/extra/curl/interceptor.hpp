#ifndef ATOM_EXTRA_CURL_INTERCEPTOR_HPP
#define ATOM_EXTRA_CURL_INTERCEPTOR_HPP

#include <curl/curl.h>

namespace atom::extra::curl {
/**
 * @brief Forward declaration of the Request class.
 */
class Request;

/**
 * @brief Forward declaration of the Response class.
 */
class Response;

/**
 * @brief Abstract base class for intercepting HTTP requests and responses.
 *
 * Interceptors allow you to modify or inspect HTTP requests before they are
 * sent and HTTP responses after they are received. You can use interceptors
 * for logging, authentication, caching, or other custom logic.
 */
class Interceptor {
public:
    /**
     * @brief Virtual destructor for the Interceptor class.
     *
     * Ensures proper cleanup of derived classes.
     */
    virtual ~Interceptor() = default;

    /**
     * @brief Intercepts the HTTP request before it is sent.
     *
     * This method is called before the HTTP request is sent to the server.
     * You can use this method to modify the request headers, body, or URL.
     *
     * @param handle A pointer to the CURL handle for the request.
     * @param request A reference to the Request object representing the HTTP
     * request.
     */
    virtual void before_request(CURL* handle, const Request& request) = 0;

    /**
     * @brief Intercepts the HTTP response after it is received.
     *
     * This method is called after the HTTP response is received from the
     * server. You can use this method to inspect the response headers, body,
     * or status code.
     *
     * @param handle A pointer to the CURL handle for the request.
     * @param request A reference to the Request object representing the HTTP
     * request.
     * @param response A reference to the Response object representing the HTTP
     * response.
     */
    virtual void after_response(CURL* handle, const Request& request,
                                const Response& response) = 0;
};
}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_INTERCEPTOR_HPP