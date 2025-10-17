/*
 * httpparser_comprehensive_example.cpp
 *
 * Copyright (C) 2025 Developers <example.com>
 *
 * A comprehensive example demonstrating advanced features of the Atom
 * HttpHeaderParser class
 */

#include "atom/log/loguru.hpp"
#include "atom/web/httpparser.hpp"

#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace atom::web;

void demonstrateBasicHeaderParsing() {
    std::cout << "\n=== Basic Header Parsing ===\n";

    try {
        HttpHeaderParser parser;

        // Parse a realistic HTTP response header
        std::string httpResponse =
            "HTTP/1.1 200 OK\r\n"
            "Date: Wed, 21 Oct 2025 07:28:00 GMT\r\n"
            "Server: Apache/2.4.41 (Ubuntu)\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Content-Length: 1234\r\n"
            "Connection: keep-alive\r\n"
            "Cache-Control: max-age=3600, public\r\n"
            "ETag: \"abc123def456\"\r\n"
            "Last-Modified: Tue, 20 Oct 2025 15:30:00 GMT\r\n"
            "\r\n";

        std::cout << "Parsing HTTP response headers:\n";
        std::cout << "Raw headers:\n" << httpResponse << "\n";

        parser.parseHeaders(httpResponse);

        // Display all parsed headers
        auto allHeaders = parser.getAllHeaders();
        std::cout << "Parsed headers:\n";
        for (const auto& [name, values] : allHeaders) {
            std::cout << "  " << name << ": ";
            for (size_t i = 0; i < values.size(); ++i) {
                if (i > 0)
                    std::cout << ", ";
                std::cout << values[i];
            }
            std::cout << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in basic header parsing: " << e.what() << "\n";
    }
}

void demonstrateRequestHeaderParsing() {
    std::cout << "\n=== Request Header Parsing ===\n";

    try {
        HttpHeaderParser parser;

        // Parse HTTP request headers
        std::string httpRequest =
            "GET /api/users/123 HTTP/1.1\r\n"
            "Host: api.example.com\r\n"
            "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
            "AppleWebKit/537.36\r\n"
            "Accept: application/json, text/plain, */*\r\n"
            "Accept-Language: en-US,en;q=0.9,es;q=0.8\r\n"
            "Accept-Encoding: gzip, deflate, br\r\n"
            "Authorization: Bearer eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: 156\r\n"
            "Origin: https://example.com\r\n"
            "Referer: https://example.com/users\r\n"
            "Cookie: session_id=abc123; user_pref=dark_mode\r\n"
            "X-Requested-With: XMLHttpRequest\r\n"
            "X-CSRF-Token: def456ghi789\r\n"
            "\r\n";

        std::cout << "Parsing HTTP request headers:\n";
        parser.parseHeaders(httpRequest);

        // Check for specific headers
        std::vector<std::string> importantHeaders = {
            "Host",         "User-Agent", "Accept",      "Authorization",
            "Content-Type", "Cookie",     "X-CSRF-Token"};

        std::cout << "Important request headers:\n";
        for (const auto& headerName : importantHeaders) {
            if (parser.hasHeader(headerName)) {
                auto values = parser.getHeaderValues(headerName);
                if (values) {
                    std::cout << "  " << headerName << ": " << (*values)[0]
                              << "\n";
                }
            } else {
                std::cout << "  " << headerName << ": (not present)\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in request header parsing: " << e.what() << "\n";
    }
}

void demonstrateMultiValueHeaders() {
    std::cout << "\n=== Multi-Value Headers ===\n";

    try {
        HttpHeaderParser parser;

        // Headers with multiple values
        std::string multiValueHeaders =
            "Accept: text/html, application/xhtml+xml, application/xml;q=0.9, "
            "*/*;q=0.8\r\n"
            "Accept-Language: en-US, en;q=0.9, es;q=0.8, fr;q=0.7\r\n"
            "Accept-Encoding: gzip, deflate, br\r\n"
            "Cache-Control: no-cache, no-store, must-revalidate\r\n"
            "Vary: Accept-Encoding, User-Agent, Origin\r\n"
            "Set-Cookie: session_id=abc123; Path=/; HttpOnly\r\n"
            "Set-Cookie: user_pref=dark_mode; Path=/; Expires=Thu, 01 Jan 2026 "
            "00:00:00 GMT\r\n"
            "Set-Cookie: csrf_token=xyz789; Path=/; Secure; SameSite=Strict\r\n"
            "\r\n";

        std::cout << "Parsing multi-value headers:\n";
        parser.parseHeaders(multiValueHeaders);

        // Demonstrate adding values to existing headers
        parser.addHeaderValue("Accept", "application/json");
        parser.addHeaderValue("Cache-Control", "max-age=0");

        // Display multi-value headers
        std::vector<std::string> multiHeaders = {
            "Accept", "Accept-Language", "Accept-Encoding", "Cache-Control",
            "Vary",   "Set-Cookie"};

        for (const auto& headerName : multiHeaders) {
            auto values = parser.getHeaderValues(headerName);
            if (values) {
                std::cout << "  " << headerName << " (" << values->size()
                          << " values):\n";
                for (size_t i = 0; i < values->size(); ++i) {
                    std::cout << "    [" << i << "] " << (*values)[i] << "\n";
                }
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in multi-value header parsing: " << e.what()
                  << "\n";
    }
}

void demonstrateHeaderManipulation() {
    std::cout << "\n=== Header Manipulation ===\n";

    try {
        HttpHeaderParser parser;

        // Start with basic headers
        std::cout << "1. Setting initial headers:\n";
        parser.setHeaderValue("Content-Type", "application/json");
        parser.setHeaderValue("Accept", "application/json");
        parser.setHeaderValue("User-Agent", "AtomHttpParser/1.0");

        // Add multiple values to Accept header
        parser.addHeaderValue("Accept", "text/plain");
        parser.addHeaderValue("Accept", "*/*");

        std::cout << "Initial headers set.\n";

        // Bulk set headers
        std::cout << "\n2. Bulk setting headers:\n";
        std::map<std::string, std::vector<std::string>> bulkHeaders = {
            {"Authorization", {"Bearer token123"}},
            {"X-API-Key", {"secret-key-456"}},
            {"X-Request-ID", {"req-789"}},
            {"Cache-Control", {"no-cache", "no-store"}},
            {"Accept-Language", {"en-US", "en;q=0.9"}}};

        parser.setHeaders(bulkHeaders);

        // Display current state
        std::cout << "Headers after bulk setting:\n";
        auto allHeaders = parser.getAllHeaders();
        for (const auto& [name, values] : allHeaders) {
            std::cout << "  " << name << ": ";
            for (size_t i = 0; i < values.size(); ++i) {
                if (i > 0)
                    std::cout << ", ";
                std::cout << values[i];
            }
            std::cout << "\n";
        }

        // Remove a header
        std::cout << "\n3. Removing headers:\n";
        parser.removeHeader("X-API-Key");
        std::cout << "Removed X-API-Key header.\n";

        // Check if header exists
        std::cout << "X-API-Key exists: "
                  << (parser.hasHeader("X-API-Key") ? "Yes" : "No") << "\n";
        std::cout << "Authorization exists: "
                  << (parser.hasHeader("Authorization") ? "Yes" : "No") << "\n";

        // Clear all headers
        std::cout << "\n4. Clearing all headers:\n";
        parser.clearHeaders();
        std::cout << "All headers cleared.\n";
        std::cout << "Header count: " << parser.getAllHeaders().size() << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in header manipulation: " << e.what() << "\n";
    }
}

void demonstrateRealWorldScenarios() {
    std::cout << "\n=== Real-World Scenarios ===\n";

    try {
        // Scenario 1: API Response with CORS headers
        std::cout << "1. API Response with CORS headers:\n";
        HttpHeaderParser apiParser;

        std::string apiResponse =
            "HTTP/1.1 200 OK\r\n"
            "Access-Control-Allow-Origin: https://example.com\r\n"
            "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type, Authorization, "
            "X-Requested-With\r\n"
            "Access-Control-Allow-Credentials: true\r\n"
            "Access-Control-Max-Age: 86400\r\n"
            "Content-Type: application/json; charset=utf-8\r\n"
            "X-RateLimit-Limit: 1000\r\n"
            "X-RateLimit-Remaining: 999\r\n"
            "X-RateLimit-Reset: 1640995200\r\n"
            "\r\n";

        apiParser.parseHeaders(apiResponse);

        // Check CORS headers
        std::vector<std::string> corsHeaders = {
            "Access-Control-Allow-Origin", "Access-Control-Allow-Methods",
            "Access-Control-Allow-Headers", "Access-Control-Allow-Credentials"};

        std::cout << "CORS headers:\n";
        for (const auto& header : corsHeaders) {
            if (apiParser.hasHeader(header)) {
                auto values = apiParser.getHeaderValues(header);
                std::cout << "  " << header << ": " << (*values)[0] << "\n";
            }
        }

        // Scenario 2: File download response
        std::cout << "\n2. File download response:\n";
        HttpHeaderParser downloadParser;

        std::string downloadResponse =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Length: 1048576\r\n"
            "Content-Disposition: attachment; filename=\"document.pdf\"\r\n"
            "Content-Encoding: gzip\r\n"
            "Accept-Ranges: bytes\r\n"
            "ETag: \"abc123-def456\"\r\n"
            "Last-Modified: Mon, 19 Oct 2025 10:15:30 GMT\r\n"
            "\r\n";

        downloadParser.parseHeaders(downloadResponse);

        // Extract download information
        if (downloadParser.hasHeader("Content-Disposition")) {
            auto disposition =
                downloadParser.getHeaderValues("Content-Disposition");
            std::cout << "File disposition: " << (*disposition)[0] << "\n";
        }

        if (downloadParser.hasHeader("Content-Length")) {
            auto length = downloadParser.getHeaderValues("Content-Length");
            std::cout << "File size: " << (*length)[0] << " bytes\n";
        }

        // Scenario 3: Redirect response
        std::cout << "\n3. Redirect response:\n";
        HttpHeaderParser redirectParser;

        std::string redirectResponse =
            "HTTP/1.1 302 Found\r\n"
            "Location: https://www.example.com/new-location\r\n"
            "Cache-Control: no-cache\r\n"
            "Expires: Thu, 01 Jan 1970 00:00:00 GMT\r\n"
            "\r\n";

        redirectParser.parseHeaders(redirectResponse);

        if (redirectParser.hasHeader("Location")) {
            auto location = redirectParser.getHeaderValues("Location");
            std::cout << "Redirect location: " << (*location)[0] << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in real-world scenarios: " << e.what() << "\n";
    }
}

void demonstrateEdgeCases() {
    std::cout << "\n=== Edge Cases and Error Handling ===\n";

    try {
        HttpHeaderParser parser;

        // Test with malformed headers
        std::cout << "1. Testing malformed headers:\n";
        std::vector<std::string> malformedHeaders = {
            "Invalid-Header-Without-Colon\r\n", ": Value-Without-Name\r\n",
            "Valid-Header: Valid Value\r\n", "Header-With-Empty-Value:\r\n",
            "Multiple:Colons:In:Header: Value\r\n"};

        for (const auto& header : malformedHeaders) {
            try {
                parser.parseHeaders(header);
                std::cout << "  Parsed: " << header.substr(0, header.find('\r'))
                          << "\n";
            } catch (const std::exception& e) {
                std::cout << "  Failed: " << header.substr(0, header.find('\r'))
                          << " (" << e.what() << ")\n";
            }
        }

        // Test with empty and whitespace
        std::cout << "\n2. Testing empty and whitespace:\n";
        parser.clearHeaders();

        parser.parseHeaders("");          // Empty string
        parser.parseHeaders("   \r\n");   // Whitespace only
        parser.parseHeaders("\r\n\r\n");  // Just line endings

        std::cout << "Empty/whitespace parsing completed without errors.\n";

        // Test case sensitivity
        std::cout << "\n3. Testing case sensitivity:\n";
        parser.clearHeaders();
        parser.setHeaderValue("Content-Type", "application/json");
        parser.setHeaderValue("content-type", "text/html");  // Different case

        std::cout << "Headers with different cases:\n";
        auto headers = parser.getAllHeaders();
        for (const auto& [name, values] : headers) {
            std::cout << "  " << name << ": " << values[0] << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in edge cases: " << e.what() << "\n";
    }
}

int main(int argc, char** argv) {
    // Initialize logging
    loguru::init(argc, argv);
    loguru::add_file("httpparser_comprehensive_example.log", loguru::Append,
                     loguru::Verbosity_MAX);

    std::cout << "============================================\n";
    std::cout << "   ATOM HTTP PARSER COMPREHENSIVE DEMO     \n";
    std::cout << "============================================\n";

    try {
        demonstrateBasicHeaderParsing();
        demonstrateRequestHeaderParsing();
        demonstrateMultiValueHeaders();
        demonstrateHeaderManipulation();
        demonstrateRealWorldScenarios();
        demonstrateEdgeCases();

        std::cout << "\n============================================\n";
        std::cout << "   HTTP PARSER COMPREHENSIVE DEMO COMPLETED\n";
        std::cout << "============================================\n";

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
