#include "atom/web/httpparser.hpp"

#include <iostream>
#include <vector>
#include <map>
#include <optional>

using namespace atom::web;

int main() {
    // Create an HttpHeaderParser instance
    HttpHeaderParser parser;

    // Parse raw HTTP headers
    std::string rawHeaders = "Content-Type: text/html\r\nContent-Length: 123\r\n";
    parser.parseHeaders(rawHeaders);

    // Set the value of a specific header field
    parser.setHeaderValue("User-Agent", "HttpHeaderParser/1.0");

    // Set multiple header fields at once
    std::map<std::string, std::vector<std::string>> headers = {
        {"Accept", {"text/html", "application/json"}},
        {"Cache-Control", {"no-cache"}}
    };
    parser.setHeaders(headers);

    // Add a new value to an existing header field
    parser.addHeaderValue("Accept", "text/plain");

    // Retrieve the values of a specific header field
    std::optional<std::vector<std::string>> acceptValues = parser.getHeaderValues("Accept");
    if (acceptValues) {
        std::cout << "Accept header values: ";
        for (const auto& value : *acceptValues) {
            std::cout << value << " ";
        }
        std::cout << std::endl;
    }

    // Remove a specific header field
    parser.removeHeader("Cache-Control");

    // Retrieve all the parsed headers
    std::map<std::string, std::vector<std::string>> allHeaders = parser.getAllHeaders();
    std::cout << "All headers: " << std::endl;
    for (const auto& [key, values] : allHeaders) {
        std::cout << key << ": ";
        for (const auto& value : values) {
            std::cout << value << " ";
        }
        std::cout << std::endl;
    }

    // Check if a specific header field exists
    bool hasContentType = parser.hasHeader("Content-Type");
    std::cout << "Has Content-Type header: " << std::boolalpha << hasContentType << std::endl;

    // Clear all the parsed headers
    parser.clearHeaders();
    std::cout << "Headers cleared." << std::endl;

    return 0;
}
