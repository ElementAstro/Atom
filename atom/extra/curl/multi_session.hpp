#ifndef ATOM_EXTRA_CURL_MULTI_SESSION_HPP
#define ATOM_EXTRA_CURL_MULTI_SESSION_HPP

#include <curl/curl.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "error.hpp"
#include "request.hpp"
#include "response.hpp"

namespace atom::extra::curl {
/**
 * @brief A class for performing multiple HTTP requests concurrently using
 * libcurl's multi interface.
 *
 * This class allows you to add multiple HTTP requests and execute them
 * concurrently, improving performance when dealing with multiple requests.
 */
class MultiSession {
public:
    /**
     * @brief Constructor for the MultiSession class.
     *
     * Initializes the curl multi handle.
     * @throws Error if curl_multi_init fails.
     */
    MultiSession();

    /**
     * @brief Destructor for the MultiSession class.
     *
     * Cleans up the curl multi handle.
     */
    ~MultiSession();

    /**
     * @brief Adds an HTTP request to the multi session.
     *
     * @param request The HTTP request to add.
     * @param callback A function to be called when the request is successfully
     * completed. It takes a Response object as input.
     * @param error_callback A function to be called when an error occurs during
     * the request. It takes an Error object as input.
     * @throws Error if curl_easy_init or curl_multi_add_handle fails.
     */
    void add_request(const Request& request,
                     std::function<void(Response)> callback = nullptr,
                     std::function<void(const Error&)> error_callback =
                         nullptr);

    /**
     * @brief Performs all added requests and waits for them to complete.
     *
     * This method executes all the HTTP requests that have been added to the
     * multi session and waits for them to complete. It also checks for any
     * errors that may occur during the execution of the requests.
     * @throws Error if curl_multi_perform or curl_multi_wait fails.
     */
    void perform();

private:
    /**
     * @brief A struct to hold the context for each request.
     */
    struct RequestContext {
        /** @brief The HTTP request. */
        Request request;
        /** @brief The callback function for successful requests. */
        std::function<void(Response)> callback;
        /** @brief The callback function for error requests. */
        std::function<void(const Error&)> error_callback;
        /** @brief The curl easy handle. */
        CURL* handle;
        /** @brief The response body. */
        std::vector<char> response_body;
        /** @brief The response headers. */
        std::map<std::string, std::string> response_headers;
        /** @brief The error buffer. */
        char error_buffer[CURL_ERROR_SIZE] = {0};
        /** @brief The headers list. */
        struct curl_slist* headers = nullptr;
    };

    /** @brief The curl multi handle. */
    CURLM* multi_handle_;
    /** @brief A map of curl easy handles to request contexts. */
    std::map<CURL*, std::shared_ptr<RequestContext>> handles_;

    /**
     * @brief Sets up the curl easy handle with the given request.
     *
     * @param request The HTTP request.
     * @param context A pointer to the RequestContext struct.
     */
    void setup_request(const Request& request, RequestContext* context);

    /**
     * @brief Checks for completed transfers and processes the results.
     */
    void check_multi_info();

    /**
     * @brief A callback function to write the response body to a vector of
     * characters.
     *
     * @param ptr A pointer to the data.
     * @param size The size of each data element.
     * @param nmemb The number of data elements.
     * @param userdata A pointer to the user data (a vector of characters).
     * @return The number of bytes written.
     */
    static size_t write_callback(char* ptr, size_t size, size_t nmemb,
                                 void* userdata);

    /**
     * @brief A callback function to write the response headers to a map of
     * strings.
     *
     * @param buffer A pointer to the header data.
     * @param size The size of each data element.
     * @param nitems The number of data elements.
     * @param userdata A pointer to the user data (a map of strings).
     * @return The number of bytes written.
     */
    static size_t header_callback(char* buffer, size_t size, size_t nitems,
                                  void* userdata);
};
}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_MULTI_SESSION_HPP
