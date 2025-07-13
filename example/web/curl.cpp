#include "atom/web/curl.hpp"

#include <iostream>

using namespace atom::web;

int main() {
    // Create a CurlWrapper instance
    CurlWrapper curl;

    // Set the URL for the request
    curl.setUrl("https://example.com");

    // Set the request method to GET
    curl.setRequestMethod("GET");

    // Add a custom header to the request
    curl.addHeader("User-Agent", "CurlWrapper/1.0");

    // Set an error callback
    curl.onError([](CURLcode code) {
        std::cerr << "Error: " << curl_easy_strerror(code) << std::endl;
    });

    // Set a response callback
    curl.onResponse([](const std::string& response) {
        std::cout << "Response: " << response << std::endl;
    });

    // Set the timeout for the request
    curl.setTimeout(30L);

    // Set whether to follow redirects
    curl.setFollowLocation(true);

    // Set the request body for POST requests
    curl.setRequestBody("param1=value1&param2=value2");

    // Set the file path for uploading a file
    curl.setUploadFile("/path/to/file.txt");

    // Set the proxy for the request
    curl.setProxy("http://proxy.example.com:8080");

    // Set SSL options
    curl.setSSLOptions(true, true);

    // Perform the request synchronously
    std::string response = curl.perform();
    std::cout << "Synchronous response: " << response << std::endl;

    // Perform the request asynchronously
    curl.performAsync();

    // Wait for all asynchronous requests to complete
    curl.waitAll();

    // Set the maximum download speed
    curl.setMaxDownloadSpeed(1024 * 1024);  // 1 MB/s

    return 0;
}
