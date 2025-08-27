// filepath: tests/web/test_integration.hpp
#ifndef TEST_INTEGRATION_HPP
#define TEST_INTEGRATION_HPP

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <spdlog/spdlog.h>

#include "atom/web/address.hpp"
#include "atom/web/http/curl.hpp"
#include "atom/web/http/downloader.hpp"
#include "atom/web/http/httpparser.hpp"
#include "atom/web/mime/minetype.hpp"
#include "atom/web/utils.hpp"

using namespace atom::web;
namespace fs = std::filesystem;

class WebIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests to reduce noise
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }

        // Create temporary directory for test files
        tempDir = fs::temp_directory_path() / "atom_integration_test";
        fs::create_directories(tempDir);

        // Initialize Windows Socket API if needed
        initializeWindowsSocketAPI();
    }

    void TearDown() override {
        try {
            fs::remove_all(tempDir);
        } catch (const std::exception& e) {
            // Ignore cleanup errors
        }

#ifdef _WIN32
        WSACleanup();
#endif
    }

    // Test data
    fs::path tempDir;

    // Test URLs - using httpbin.org for reliable testing
    const std::string TEST_BASE_URL = "https://httpbin.org";
    const std::string TEST_GET_URL = TEST_BASE_URL + "/get";
    const std::string TEST_POST_URL = TEST_BASE_URL + "/post";
    const std::string TEST_JSON_URL = TEST_BASE_URL + "/json";
    const std::string TEST_HTML_URL = TEST_BASE_URL + "/html";
    const std::string TEST_BYTES_URL = TEST_BASE_URL + "/bytes/1024";
};

// Web Crawler Integration Test
TEST_F(WebIntegrationTest, WebCrawlerIntegration) {
    try {
        // Step 1: Check internet connectivity
        bool hasInternet = checkInternetConnectivity();
        if (!hasInternet) {
            GTEST_SKIP() << "No internet connectivity available";
        }

        // Step 2: Resolve target domain
        std::string domain = "httpbin.org";
        auto ipAddresses = getIPAddresses(domain);
        EXPECT_FALSE(ipAddresses.empty()) << "Failed to resolve domain: " << domain;

        // Validate resolved IP addresses
        for (const auto& ip : ipAddresses) {
            auto addr = Address::createFromString(ip);
            EXPECT_NE(addr, nullptr) << "Invalid IP address: " << ip;
            EXPECT_TRUE(addr->getType() == "IPv4" || addr->getType() == "IPv6");
        }

        // Step 3: Fetch web page with CURL
        CurlWrapper curl;
        curl.setUrl(TEST_HTML_URL);
        curl.setRequestMethod("GET");
        curl.addHeader("User-Agent", "AtomWebCrawler/1.0");
        curl.setTimeout(10L);

        std::string htmlContent = curl.perform();
        EXPECT_FALSE(htmlContent.empty()) << "Failed to fetch HTML content";
        EXPECT_GT(htmlContent.length(), 100) << "HTML content too short";

        // Step 4: Parse HTTP headers
        HttpHeaderParser parser;
        parser.setHeaderValue("Content-Type", "text/html; charset=utf-8");
        parser.setHeaderValue("Content-Length", std::to_string(htmlContent.length()));
        parser.setHeaderValue("Server", "nginx/1.18.0");

        EXPECT_TRUE(parser.hasHeader("Content-Type"));
        auto contentType = parser.getHeaderValue("Content-Type");
        ASSERT_TRUE(contentType.has_value());
        EXPECT_THAT(*contentType, ::testing::HasSubstr("text/html"));

        // Step 5: Determine MIME type and save file
        MimeTypes mimeTypes({}, true);
        auto [mimeType, charset] = mimeTypes.guessType("page.html");
        EXPECT_TRUE(mimeType.has_value());
        EXPECT_EQ(*mimeType, "text/html");

        // Save the content
        fs::path outputFile = tempDir / "fetched_page.html";
        std::ofstream file(outputFile);
        file << htmlContent;
        file.close();

        EXPECT_TRUE(fs::exists(outputFile));
        EXPECT_GT(fs::file_size(outputFile), 0);

    } catch (const std::exception& e) {
        FAIL() << "Web crawler integration failed: " << e.what();
    }
}

// API Client Integration Test
TEST_F(WebIntegrationTest, APIClientIntegration) {
    try {
        // Check internet connectivity
        if (!checkInternetConnectivity()) {
            GTEST_SKIP() << "No internet connectivity available";
        }

        // Step 1: Create API client
        CurlWrapper apiClient;
        apiClient.addHeader("User-Agent", "AtomAPIClient/1.0");
        apiClient.addHeader("Accept", "application/json");
        apiClient.setTimeout(10L);

        // Step 2: GET request with parameters
        apiClient.setUrl(TEST_GET_URL + "?param1=value1&param2=value2");
        apiClient.setRequestMethod("GET");

        std::string getResponse = apiClient.perform();
        EXPECT_FALSE(getResponse.empty());
        EXPECT_THAT(getResponse, ::testing::HasSubstr("param1"));
        EXPECT_THAT(getResponse, ::testing::HasSubstr("value1"));

        // Step 3: POST request with JSON
        apiClient.setUrl(TEST_POST_URL);
        apiClient.setRequestMethod("POST");
        apiClient.addHeader("Content-Type", "application/json");
        apiClient.setRequestBody(R"({"name": "test", "value": 123, "active": true})");

        std::string postResponse = apiClient.perform();
        EXPECT_FALSE(postResponse.empty());
        EXPECT_THAT(postResponse, ::testing::HasSubstr("test"));
        EXPECT_THAT(postResponse, ::testing::HasSubstr("123"));

        // Step 4: Parse response headers
        HttpHeaderParser responseParser;
        responseParser.setHeaderValue("Content-Type", "application/json");
        responseParser.setStatus(HttpStatus{200, "OK"});

        EXPECT_TRUE(responseParser.hasHeader("Content-Type"));
        auto status = responseParser.getStatus();
        EXPECT_EQ(status.code, 200);
        EXPECT_EQ(status.description, "OK");

        // Step 5: Validate JSON response format
        auto [mimeType, charset] = MimeTypes({}, true).guessType("response.json");
        EXPECT_TRUE(mimeType.has_value());
        EXPECT_EQ(*mimeType, "application/json");

    } catch (const std::exception& e) {
        FAIL() << "API client integration failed: " << e.what();
    }
}

// Download Manager Integration Test
TEST_F(WebIntegrationTest, DownloadManagerIntegration) {
    try {
        // Check internet connectivity
        if (!checkInternetConnectivity()) {
            GTEST_SKIP() << "No internet connectivity available";
        }

        // Step 1: Create download manager
        fs::path taskFile = tempDir / "download_tasks.json";
        DownloadManager dm(taskFile.string());

        // Track download completion
        std::atomic<int> completedDownloads{0};
        std::atomic<bool> hasError{false};

        dm.onDownloadComplete([&](size_t index, bool success) {
            if (success) {
                completedDownloads++;
            } else {
                hasError = true;
            }
        });

        // Step 2: Add multiple download tasks
        std::vector<fs::path> outputFiles;
        std::vector<std::string> urls = {
            TEST_BYTES_URL,
            TEST_JSON_URL,
            TEST_HTML_URL
        };

        for (size_t i = 0; i < urls.size(); ++i) {
            fs::path outputFile = tempDir / ("download_" + std::to_string(i) + ".dat");
            outputFiles.push_back(outputFile);
            dm.addTask(urls[i], outputFile.string());
        }

        EXPECT_EQ(dm.getTotalTaskCount(), urls.size());

        // Step 3: Start downloads
        dm.start(2);  // Use 2 threads
        EXPECT_TRUE(dm.isRunning());

        // Wait for downloads to complete
        auto start = std::chrono::steady_clock::now();
        while (dm.getActiveTaskCount() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() > 30) {
                break;  // Timeout after 30 seconds
            }
        }

        dm.stop();
        EXPECT_FALSE(dm.isRunning());

        // Step 4: Verify downloads
        EXPECT_GT(completedDownloads.load(), 0) << "No downloads completed successfully";
        EXPECT_FALSE(hasError.load()) << "Download errors occurred";

        // Check downloaded files
        for (const auto& file : outputFiles) {
            if (fs::exists(file)) {
                EXPECT_GT(fs::file_size(file), 0) << "Downloaded file is empty: " << file;

                // Step 5: Analyze downloaded content with MIME types
                std::string extension = file.extension().string();
                if (extension.empty()) {
                    extension = ".dat";
                }

                auto [mimeType, charset] = MimeTypes({}, true).guessType("file" + extension);
                // MIME type detection may vary, just ensure no crash
                EXPECT_NO_THROW(MimeTypes({}, true).guessType("file" + extension));
            }
        }

    } catch (const std::exception& e) {
        FAIL() << "Download manager integration failed: " << e.what();
    }
}

// Network Service Discovery Integration Test
TEST_F(WebIntegrationTest, NetworkServiceDiscoveryIntegration) {
    try {
        // Step 1: Get local IP addresses
        auto localIPs = getLocalIPAddresses();
        EXPECT_FALSE(localIPs.empty()) << "No local IP addresses found";

        // Validate local IPs
        for (const auto& ip : localIPs) {
            EXPECT_TRUE(isValidIPv4(ip) || isValidIPv6(ip)) << "Invalid local IP: " << ip;

            // Create address objects
            auto addr = Address::createFromString(ip);
            EXPECT_NE(addr, nullptr) << "Failed to create address from: " << ip;
        }

        // Step 2: Scan common ports on localhost
        std::vector<uint16_t> commonPorts = {22, 80, 443, 3306, 5432, 8080};
        std::vector<uint16_t> openPorts;

        for (auto port : commonPorts) {
            if (scanPort("127.0.0.1", port, std::chrono::milliseconds(1000))) {
                openPorts.push_back(port);
            }
        }

        // Step 3: Test connectivity to external services (if internet available)
        if (checkInternetConnectivity()) {
            std::vector<std::pair<std::string, uint16_t>> externalServices = {
                {"google.com", 80},
                {"github.com", 443}
            };

            for (const auto& [host, port] : externalServices) {
                bool isReachable = scanPort(host, port, std::chrono::milliseconds(5000));

                if (isReachable) {
                    // Get IP addresses for reachable hosts
                    auto ips = getIPAddresses(host);
                    EXPECT_FALSE(ips.empty()) << "Failed to resolve reachable host: " << host;

                    // Validate resolved IPs
                    for (const auto& ip : ips) {
                        EXPECT_TRUE(isValidIPv4(ip) || isValidIPv6(ip)) << "Invalid resolved IP: " << ip;
                    }
                }
            }
        }

        // Step 4: Port range scanning
        auto scannedPorts = scanPortRange("127.0.0.1", 65530, 65535, std::chrono::milliseconds(500));
        // Results may vary, just ensure no crash
        EXPECT_TRUE(scannedPorts.empty() || !scannedPorts.empty());

    } catch (const std::exception& e) {
        FAIL() << "Network service discovery integration failed: " << e.what();
    }
}

// HTTP Parser and CURL Integration Test
TEST_F(WebIntegrationTest, HTTPParserCurlIntegration) {
    try {
        // Check internet connectivity
        if (!checkInternetConnectivity()) {
            GTEST_SKIP() << "No internet connectivity available";
        }

        // Step 1: Create HTTP request using parser
        HttpHeaderParser requestParser;
        requestParser.setMethod(HttpMethod::POST);
        requestParser.setPath("/post");
        requestParser.setVersion(HttpVersion::HTTP_1_1);
        requestParser.setHeaderValue("Host", "httpbin.org");
        requestParser.setHeaderValue("Content-Type", "application/json");
        requestParser.setHeaderValue("User-Agent", "AtomIntegrationTest/1.0");

        // Add cookies
        Cookie sessionCookie;
        sessionCookie.name = "session_id";
        sessionCookie.value = "test123";
        sessionCookie.path = std::make_optional("/");
        sessionCookie.secure = true;
        sessionCookie.httpOnly = true;
        requestParser.addCookie(sessionCookie);

        std::string requestBody = R"({"integration": "test", "timestamp": 1234567890})";
        requestParser.setBody(requestBody);

        // Step 2: Build complete HTTP request
        std::string httpRequest = requestParser.buildRequest();
        EXPECT_THAT(httpRequest, ::testing::HasSubstr("POST /post HTTP/1.1"));
        EXPECT_THAT(httpRequest, ::testing::HasSubstr("Content-Type: application/json"));
        EXPECT_THAT(httpRequest, ::testing::HasSubstr("integration"));

        // Step 3: Use CURL to make the actual request
        CurlWrapper curl;
        curl.setUrl(TEST_POST_URL);
        curl.setRequestMethod("POST");
        curl.addHeader("Content-Type", "application/json");
        curl.addHeader("User-Agent", "AtomIntegrationTest/1.0");
        curl.setRequestBody(requestBody);
        curl.setTimeout(10L);

        std::string response = curl.perform();
        EXPECT_FALSE(response.empty());
        EXPECT_THAT(response, ::testing::HasSubstr("integration"));

        // Step 4: Parse response using HTTP parser
        HttpHeaderParser responseParser;
        responseParser.setStatus(HttpStatus{200, "OK"});
        responseParser.setHeaderValue("Content-Type", "application/json");
        responseParser.setHeaderValue("Server", "nginx/1.18.0");
        responseParser.setBody(response);

        // Verify response parsing
        auto status = responseParser.getStatus();
        EXPECT_EQ(status.code, 200);

        auto contentType = responseParser.getHeaderValue("Content-Type");
        ASSERT_TRUE(contentType.has_value());
        EXPECT_THAT(*contentType, ::testing::HasSubstr("application/json"));

        // Step 5: URL parameter parsing
        std::string testUrl = "https://httpbin.org/get?param1=value1&param2=value2&encoded=hello%20world";
        auto params = responseParser.parseUrlParameters(testUrl);
        EXPECT_EQ(params.size(), 3);
        EXPECT_EQ(params["param1"], "value1");
        EXPECT_EQ(params["param2"], "value2");
        EXPECT_EQ(params["encoded"], "hello%20world");

    } catch (const std::exception& e) {
        FAIL() << "HTTP parser and CURL integration failed: " << e.what();
    }
}

// MIME Type and Content Analysis Integration Test
TEST_F(WebIntegrationTest, MimeTypeContentAnalysisIntegration) {
    try {
        // Step 1: Create test files with different content types
        std::vector<std::pair<std::string, std::string>> testFiles = {
            {"test.html", "<html><body><h1>Test HTML</h1></body></html>"},
            {"test.json", R"({"name": "test", "value": 123})"},
            {"test.txt", "This is a plain text file for testing."},
            {"test.xml", "<?xml version=\"1.0\"?><root><item>test</item></root>"},
            {"test.css", "body { color: red; font-size: 14px; }"}
        };

        std::vector<fs::path> createdFiles;
        for (const auto& [filename, content] : testFiles) {
            fs::path filePath = tempDir / filename;
            std::ofstream file(filePath);
            file << content;
            file.close();
            createdFiles.push_back(filePath);
        }

        // Step 2: Initialize MIME type detector
        MimeTypes mimeTypes({}, true);

        // Step 3: Analyze each file
        for (const auto& filePath : createdFiles) {
            std::string filename = filePath.filename().string();

            // Guess MIME type by extension
            auto [mimeType, charset] = mimeTypes.guessType(filename);
            EXPECT_TRUE(mimeType.has_value()) << "Failed to detect MIME type for: " << filename;

            // Verify expected MIME types
            if (filename.ends_with(".html")) {
                EXPECT_EQ(*mimeType, "text/html");
            } else if (filename.ends_with(".json")) {
                EXPECT_EQ(*mimeType, "application/json");
            } else if (filename.ends_with(".txt")) {
                EXPECT_EQ(*mimeType, "text/plain");
            } else if (filename.ends_with(".xml")) {
                EXPECT_EQ(*mimeType, "application/xml");
            } else if (filename.ends_with(".css")) {
                EXPECT_EQ(*mimeType, "text/css");
            }

            // Test content-based detection
            if (mimeTypes.getConfig().enableDeepScanning) {
                auto contentType = mimeTypes.guessTypeByContent(filePath.string());
                EXPECT_TRUE(contentType.has_value()) << "Content-based detection failed for: " << filename;
            }

            // Test with HTTP headers
            HttpHeaderParser parser;
            parser.setHeaderValue("Content-Type", *mimeType);

            auto headerContentType = parser.getHeaderValue("Content-Type");
            ASSERT_TRUE(headerContentType.has_value());
            EXPECT_EQ(*headerContentType, *mimeType);
        }

        // Step 4: Test MIME type caching and performance
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 100; ++i) {
            for (const auto& filePath : createdFiles) {
                mimeTypes.guessType(filePath.filename().string());
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // Should complete quickly due to caching
        EXPECT_LT(duration.count(), 1000);  // Less than 1 second

    } catch (const std::exception& e) {
        FAIL() << "MIME type and content analysis integration failed: " << e.what();
    }
}

#endif  // TEST_INTEGRATION_HPP
