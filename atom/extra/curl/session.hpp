#ifndef ATOM_EXTRA_CURL_SESSION_HPP
#define ATOM_EXTRA_CURL_SESSION_HPP

#include <curl/curl.h>
#include <cstddef>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cache.hpp"
#include "interceptor.hpp"
#include "multipart.hpp"
#include "rate_limiter.hpp"
#include "request.hpp"
#include "response.hpp"
#include "websocket.hpp"

namespace atom::extra::curl {
class ConnectionPool;

/**
 * @brief A class for performing HTTP requests using libcurl.
 *
 * This class provides a high-level interface for making HTTP requests,
 * handling cookies, caching, rate limiting, and more.
 */
class Session {
public:
    /**
     * @brief Default constructor for the Session class.
     *
     * Initializes a new Session object with default values.
     * @throws Error if curl_easy_init fails.
     */
    Session();

    /**
     * @brief Constructor for the Session class with a connection pool.
     *
     * Initializes a new Session object with a connection pool.
     * @param pool The connection pool to use.
     * @throws Error if curl_easy_init fails.
     */
    explicit Session(ConnectionPool* pool);

    /**
     * @brief Destructor for the Session class.
     *
     * Cleans up the curl handle.
     */
    ~Session();

    // 禁止拷贝
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // 允许移动
    Session(Session&& other) noexcept;
    Session& operator=(Session&& other) noexcept;

    /**
     * @brief Adds an interceptor to the session.
     *
     * @param interceptor A shared pointer to the interceptor to add.
     */
    void add_interceptor(std::shared_ptr<Interceptor> interceptor);

    /**
     * @brief Sets the cache for the session.
     *
     * @param cache A pointer to the cache to use.
     */
    void set_cache(Cache* cache);

    /**
     * @brief Sets the rate limiter for the session.
     *
     * @param limiter A pointer to the rate limiter to use.
     */
    void set_rate_limiter(RateLimiter* limiter);

    /**
     * @brief Executes an HTTP request.
     *
     * @param request The HTTP request to execute.
     * @return The HTTP response.
     * @throws Error if the request fails.
     */
    Response execute(const Request& request);

    /**
     * @brief Executes an HTTP request asynchronously.
     *
     * @param request The HTTP request to execute.
     * @return A future that will contain the HTTP response.
     */
    std::future<Response> execute_async(const Request& request);

    /**
     * @brief Performs a GET request.
     *
     * @param url The URL to request.
     * @return The HTTP response.
     * @throws Error if the request fails.
     */
    Response get(std::string_view url);

    /**
     * @brief Performs a GET request with query parameters.
     *
     * @param url The URL to request.
     * @param params A map of query parameters to add to the URL.
     * @return The HTTP response.
     * @throws Error if the request fails.
     */
    Response get(std::string_view url,
                 const std::map<std::string, std::string>& params);

    /**
     * @brief Performs a POST request.
     *
     * @param url The URL to request.
     * @param body The body of the request.
     * @param content_type The content type of the request.
     * @return The HTTP response.
     * @throws Error if the request fails.
     */
    Response post(std::string_view url, std::string_view body,
                  std::string_view content_type = "application/json");

    /**
     * @brief Performs a Form URL encoded POST request.
     *
     * @param url The URL to request.
     * @param params A map of form parameters to add to the body.
     * @return The HTTP response.
     * @throws Error if the request fails.
     */
    Response post_form(std::string_view url,
                       const std::map<std::string, std::string>& params);

    /**
     * @brief Performs a JSON POST request.
     *
     * @param url The URL to request.
     * @param json The JSON body of the request.
     * @return The HTTP response.
     * @throws Error if the request fails.
     */
    Response post_json(std::string_view url, std::string_view json);

    /**
     * @brief Performs a PUT request.
     *
     * @param url The URL to request.
     * @param body The body of the request.
     * @param content_type The content type of the request.
     * @return The HTTP response.
     * @throws Error if the request fails.
     */
    Response put(std::string_view url, std::string_view body,
                 std::string_view content_type = "application/json");

    /**
     * @brief Performs a DELETE request.
     *
     * @param url The URL to request.
     * @return The HTTP response.
     * @throws Error if the request fails.
     */
    Response del(std::string_view url);

    /**
     * @brief Performs a PATCH request.
     *
     * @param url The URL to request.
     * @param body The body of the request.
     * @param content_type The content type of the request.
     * @return The HTTP response.
     * @throws Error if the request fails.
     */
    Response patch(std::string_view url, std::string_view body,
                   std::string_view content_type = "application/json");

    /**
     * @brief Performs a HEAD request.
     *
     * @param url The URL to request.
     * @return The HTTP response.
     * @throws Error if the request fails.
     */
    Response head(std::string_view url);

    /**
     * @brief Performs an OPTIONS request.
     *
     * @param url The URL to request.
     * @return The HTTP response.
     * @throws Error if the request fails.
     */
    Response options(std::string_view url);

    /**
     * @brief Downloads a file from a URL.
     *
     * @param url The URL to download the file from.
     * @param filepath The path to save the downloaded file to.
     * @param resume_from An optional offset to resume the download from.
     * @return The HTTP response.
     * @throws Error if the request fails.
     */
    Response download(std::string_view url, std::string_view filepath,
                      std::optional<curl_off_t> resume_from = std::nullopt);

    /**
     * @brief Uploads a file to a URL.
     *
     * @param url The URL to upload the file to.
     * @param filepath The path to the file to upload.
     * @param field_name The name of the form field to use for the file.
     * @param resume_from An optional offset to resume the upload from.
     * @return The HTTP response.
     * @throws Error if the request fails.
     */
    Response upload(std::string_view url, std::string_view filepath,
                    std::string_view field_name = "file",
                    std::optional<curl_off_t> resume_from = std::nullopt);

    /**
     * @brief Sets the progress callback for the session.
     *
     * @param callback A function to be called during the request to report
     * progress.
     */
    void set_progress_callback(
        std::function<int(curl_off_t, curl_off_t, curl_off_t, curl_off_t)>
            callback);

    /**
     * @brief URL encodes a string.
     *
     * @param str The string to encode.
     * @return The encoded string.
     */
    static std::string url_encode(std::string_view str);

    /**
     * @brief URL decodes a string.
     *
     * @param str The string to decode.
     * @return The decoded string.
     */
    static std::string url_decode(std::string_view str);

    /**
     * @brief Creates a WebSocket connection.
     *
     * @param url The URL to connect to.
     * @param headers A map of headers to send with the connection request.
     * @return A shared pointer to the WebSocket object, or nullptr if the
     * connection fails.
     */
    std::shared_ptr<WebSocket> create_websocket(
        const std::string& url,
        const std::map<std::string, std::string>& headers = {});

private:
    /** @brief The curl handle. */
    CURL* handle_ = nullptr;
    /** @brief The connection pool. */
    ConnectionPool* connection_pool_ = nullptr;
    /** @brief The cache. */
    Cache* cache_ = nullptr;
    /** @brief The rate limiter. */
    RateLimiter* rate_limiter_ = nullptr;
    /** @brief The interceptors. */
    std::vector<std::shared_ptr<Interceptor>> interceptors_;
    /** @brief The response body. */
    std::vector<char> response_body_;
    /** @brief The response headers. */
    std::map<std::string, std::string> response_headers_;
    /** @brief The error buffer. */
    char error_buffer_[CURL_ERROR_SIZE] = {0};

    /**
     * @brief A struct to hold the progress callback.
     */
    struct ProgressCallback {
        /** @brief The callback function. */
        std::function<int(curl_off_t, curl_off_t, curl_off_t, curl_off_t)>
            callback;

        /**
         * @brief The callback function for libcurl.
         *
         * @param clientp A pointer to the user data.
         * @param dltotal The total number of bytes to download.
         * @param dlnow The number of bytes downloaded so far.
         * @param ultotal The total number of bytes to upload.
         * @param ulnow The number of bytes uploaded so far.
         * @return 0 to continue, or any other value to abort.
         */
        static int xferinfo(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                            curl_off_t ultotal, curl_off_t ulnow);
    } progress_callback_;

    /**
     * @brief Resets the curl handle.
     */
    void reset();

    /**
     * @brief Sets up the curl handle with the given request.
     *
     * @param request The HTTP request to execute.
     */
    void setup_request(const Request& request);

    /**
     * @brief Executes the request and returns the response.
     *
     * @param request The HTTP request to execute.
     * @return The HTTP response.
     * @throws Error if the request fails.
     */
    Response execute_internal(const Request& request);

    /**
     * @brief Performs the curl request.
     *
     * @return The HTTP response.
     * @throws Error if the request fails.
     */
    Response perform();

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

    /**
     * @brief A callback function to write the response body to a file.
     *
     * @param ptr A pointer to the data.
     * @param size The size of each data element.
     * @param nmemb The number of data elements.
     * @param userdata A pointer to the user data (a FILE*).
     * @return The number of bytes written.
     */
    static size_t file_write_callback(char* ptr, size_t size, size_t nmemb,
                                      void* userdata);
};
}  // namespace atom::extra::curl

#endif  // ATOM_EXTRA_CURL_SESSION_HPP