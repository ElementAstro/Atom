/*
 * curl.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-4

Description: Simple HTTP client using libcurl.

**************************************************/

#ifndef ATOM_WEB_CURL_HPP
#define ATOM_WEB_CURL_HPP

#include <curl/curl.h>
#include <functional>
#include <memory>
#include <string>

namespace atom::web {

/**
 * @brief A comprehensive wrapper class for performing HTTP requests using
 * libcurl.
 */
class CurlWrapper {
public:
    /**
     * @brief Constructor for CurlWrapper.
     */
    CurlWrapper();

    /**
     * @brief Destructor for CurlWrapper.
     */
    ~CurlWrapper();

    CurlWrapper(const CurlWrapper &other) = delete;
    auto operator=(const CurlWrapper &other) -> CurlWrapper & = delete;
    CurlWrapper(CurlWrapper &&other) noexcept = delete;
    auto operator=(CurlWrapper &&other) noexcept -> CurlWrapper & = delete;

    /**
     * @brief Sets the URL for the HTTP request.
     * @param url The URL to set.
     * @return Reference to the CurlWrapper object.
     */
    auto setUrl(const std::string &url) -> CurlWrapper &;

    /**
     * @brief Sets the HTTP request method (e.g., GET, POST).
     * @param method The HTTP request method to set.
     * @return Reference to the CurlWrapper object.
     */
    auto setRequestMethod(const std::string &method) -> CurlWrapper &;

    /**
     * @brief Adds a custom header to the HTTP request.
     * @param key The header key.
     * @param value The header value.
     * @return Reference to the CurlWrapper object.
     */
    auto addHeader(const std::string &key,
                   const std::string &value) -> CurlWrapper &;

    /**
     * @brief Sets the callback function to be called on error.
     * @param callback The callback function to set.
     * @return Reference to the CurlWrapper object.
     */
    auto onError(std::function<void(CURLcode)> callback) -> CurlWrapper &;

    /**
     * @brief Sets the callback function to be called on error.
     * @param callback The callback function to set.
     * @return Reference to the CurlWrapper object.
     */
    auto setOnErrorCallback(std::function<void(CURLcode)> callback)
        -> CurlWrapper &;

    /**
     * @brief Sets the callback function to be called on response.
     * @param callback The callback function to set.
     * @return Reference to the CurlWrapper object.
     */
    auto setOnResponseCallback(
        std::function<void(const std::string &)> callback) -> CurlWrapper &;

    /**
     * @brief Sets the callback function to be called on response.
     * @param callback The callback function to set.
     * @return Reference to the CurlWrapper object.
     */
    auto onResponse(std::function<void(const std::string &)> callback)
        -> CurlWrapper &;

    /**
     * @brief Sets the timeout for the HTTP request.
     * @param timeout The timeout value in seconds.
     * @return Reference to the CurlWrapper object.
     */
    auto setTimeout(long timeout) -> CurlWrapper &;

    /**
     * @brief Sets whether to follow redirects.
     * @param follow Boolean value indicating whether to follow redirects.
     * @return Reference to the CurlWrapper object.
     */
    auto setFollowLocation(bool follow) -> CurlWrapper &;

    /**
     * @brief Sets the request body for POST requests.
     * @param data The request body data.
     * @return Reference to the CurlWrapper object.
     */
    auto setRequestBody(const std::string &data) -> CurlWrapper &;

    /**
     * @brief Sets the file path for uploading a file.
     * @param filePath The file path to set.
     * @return Reference to the CurlWrapper object.
     */
    auto setUploadFile(const std::string &filePath) -> CurlWrapper &;

    /**
     * @brief Sets the proxy for the HTTP request.
     * @param proxy The proxy URL to set.
     * @return Reference to the CurlWrapper object.
     */
    auto setProxy(const std::string &proxy) -> CurlWrapper &;

    /**
     * @brief Sets SSL options for the HTTP request.
     * @param verifyPeer Boolean value indicating whether to verify the peer's
     * SSL certificate.
     * @param verifyHost Boolean value indicating whether to verify the host's
     * SSL certificate.
     * @return Reference to the CurlWrapper object.
     */
    auto setSSLOptions(bool verifyPeer, bool verifyHost) -> CurlWrapper &;

    /**
     * @brief Performs the HTTP request synchronously.
     * @return The response as a string.
     */
    auto perform() -> std::string;

    /**
     * @brief Performs the HTTP request asynchronously.
     * @return Reference to the CurlWrapper object.
     */
    auto performAsync() -> CurlWrapper &;

    /**
     * @brief Waits for all asynchronous requests to complete.
     */
    void waitAll();

    /**
     * @brief Sets the maximum download speed.
     * @param speed The maximum download speed in bytes per second.
     * @return Reference to the CurlWrapper object.
     */
    auto setMaxDownloadSpeed(size_t speed) -> CurlWrapper &;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace atom::web

#endif  // ATOM_WEB_CURL_HPP