/*
 * integration_example.cpp
 *
 * Copyright (C) 2025 Developers <example.com>
 *
 * A comprehensive example demonstrating how different Atom Web components work
 * together
 */

#include "atom/log/loguru.hpp"
#include "atom/web/address.hpp"
#include "atom/web/curl.hpp"
#include "atom/web/downloader.hpp"
#include "atom/web/httpparser.hpp"
#include "atom/web/minetype.hpp"
#include "atom/web/utils.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <thread>
#include <vector>

using namespace atom::web;

// Global variables for tracking operations
std::atomic<size_t> completedDownloads{0};
std::atomic<bool> allDownloadsComplete{false};

void demonstrateWebCrawlerIntegration() {
    std::cout << "\n=== Web Crawler Integration Example ===\n";

    try {
        // Step 1: Use network utilities to check connectivity
        std::cout << "1. Checking internet connectivity...\n";
        if (!checkInternetConnectivity()) {
            std::cout << "No internet connection available. Skipping web "
                         "crawler example.\n";
            return;
        }
        std::cout << "Internet connectivity confirmed.\n";

        // Step 2: Resolve target domain
        std::string domain = "httpbin.org";
        std::cout << "2. Resolving domain: " << domain << "\n";
        auto ipAddresses = getIPAddresses(domain);

        if (!ipAddresses.empty()) {
            std::cout << "Resolved IP addresses:\n";
            for (const auto& ip : ipAddresses) {
                std::cout << "  - " << ip << "\n";

                // Create address objects to validate
                auto addr = Address::createFromString(ip);
                if (addr) {
                    std::cout << "    Type: " << addr->getType() << "\n";
                }
            }
        }

        // Step 3: Use CURL to fetch a web page
        std::cout << "\n3. Fetching web page with CURL...\n";
        CurlWrapper curl;
        curl.setUrl("https://httpbin.org/html");
        curl.setRequestMethod("GET");
        curl.addHeader("User-Agent", "AtomWebCrawler/1.0");
        curl.setTimeout(10L);

        std::string htmlContent = curl.perform();
        std::cout << "Fetched HTML content (" << htmlContent.length()
                  << " bytes)\n";

        // Step 4: Parse HTTP headers from response
        std::cout << "\n4. Analyzing response headers...\n";
        // Note: In a real scenario, you'd get headers from the CURL response
        // For demonstration, we'll simulate typical headers
        HttpHeaderParser parser;
        std::string simulatedHeaders =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: " +
            std::to_string(htmlContent.length()) +
            "\r\n"
            "Server: nginx/1.18.0\r\n"
            "Date: Wed, 21 Oct 2025 07:28:00 GMT\r\n"
            "\r\n";

        parser.parseHeaders(simulatedHeaders);

        if (parser.hasHeader("Content-Type")) {
            auto contentType = parser.getHeaderValues("Content-Type");
            std::cout << "Content-Type: " << (*contentType)[0] << "\n";
        }

        // Step 5: Determine MIME type and save appropriately
        std::cout << "\n5. Determining MIME type and saving file...\n";
        MimeTypes mimeTypes({}, true);

        auto [mimeType, charset] = mimeTypes.guessType("page.html");
        std::cout << "Detected MIME type: "
                  << (mimeType ? *mimeType : "unknown") << "\n";

        // Save the content
        std::filesystem::create_directories("crawler_output");
        std::ofstream outputFile("crawler_output/fetched_page.html");
        outputFile << htmlContent;
        outputFile.close();

        std::cout << "Saved content to: crawler_output/fetched_page.html\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in web crawler integration: " << e.what() << "\n";
    }
}

void demonstrateFileDownloadWithAnalysis() {
    std::cout << "\n=== File Download with Analysis Integration ===\n";

    try {
        // Create download manager
        DownloadManager dm("integration_downloads.json");
        MimeTypes mimeTypes({}, true);

        // Define files to download with different types
        std::vector<std::pair<std::string, std::string>> downloads = {
            {"https://httpbin.org/json", "downloads/data.json"},
            {"https://httpbin.org/html", "downloads/page.html"},
            {"https://httpbin.org/bytes/1024", "downloads/binary.bin"}};

        std::cout << "Setting up downloads with analysis...\n";
        std::filesystem::create_directories("downloads");

        // Add download tasks
        for (size_t i = 0; i < downloads.size(); ++i) {
            const auto& [url, filepath] = downloads[i];
            dm.addTask(url, filepath, static_cast<int>(downloads.size() - i));
            std::cout << "  Added: " << url << " -> " << filepath << "\n";
        }

        // Set up completion callback with file analysis
        dm.onDownloadComplete([&downloads, &mimeTypes](size_t index,
                                                       bool success) {
            if (success && index < downloads.size()) {
                const auto& [url, filepath] = downloads[index];

                std::cout << "\nAnalyzing downloaded file: " << filepath
                          << "\n";

                // Check file size
                if (std::filesystem::exists(filepath)) {
                    auto fileSize = std::filesystem::file_size(filepath);
                    std::cout << "  File size: " << fileSize << " bytes\n";

                    // Detect MIME type by extension
                    auto [extMimeType, charset] = mimeTypes.guessType(filepath);
                    std::cout << "  MIME type (by extension): "
                              << (extMimeType ? *extMimeType : "unknown")
                              << "\n";

                    // Detect MIME type by content
                    auto contentMimeType =
                        mimeTypes.guessTypeByContent(filepath);
                    std::cout
                        << "  MIME type (by content): "
                        << (contentMimeType ? *contentMimeType : "unknown")
                        << "\n";

                    // Suggest appropriate file extension
                    if (extMimeType) {
                        auto suggestedExt =
                            mimeTypes.guessExtension(*extMimeType);
                        if (suggestedExt) {
                            std::cout
                                << "  Suggested extension: " << *suggestedExt
                                << "\n";
                        }
                    }
                }
            }

            completedDownloads++;
            if (completedDownloads >= downloads.size()) {
                allDownloadsComplete = true;
            }
        });

        dm.onProgressUpdate([&downloads](size_t index, double progress) {
            if (index < downloads.size()) {
                const auto& [url, filepath] = downloads[index];
                std::cout << "  "
                          << std::filesystem::path(filepath).filename().string()
                          << ": " << std::fixed << std::setprecision(1)
                          << (progress * 100) << "%\n";
            }
        });

        // Start downloads
        std::cout << "\nStarting downloads...\n";
        dm.start(2);  // Use 2 threads

        // Wait for completion
        auto startTime = std::chrono::steady_clock::now();
        while (!allDownloadsComplete &&
               std::chrono::steady_clock::now() - startTime <
                   std::chrono::seconds(30)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        dm.stop();

        std::cout << "\nDownload and analysis completed!\n";
        std::cout << "Files downloaded and analyzed: " << completedDownloads
                  << "/" << downloads.size() << "\n";

        // Reset for next example
        completedDownloads = 0;
        allDownloadsComplete = false;

    } catch (const std::exception& e) {
        std::cerr << "Error in file download with analysis: " << e.what()
                  << "\n";
    }
}

void demonstrateNetworkServiceDiscovery() {
    std::cout << "\n=== Network Service Discovery Integration ===\n";

    try {
        // Step 1: Get local network information
        std::cout << "1. Discovering local network information...\n";
        auto localIPs = getLocalIPAddresses();

        std::cout << "Local IP addresses:\n";
        for (const auto& ip : localIPs) {
            auto addr = Address::createFromString(ip);
            if (addr) {
                std::cout << "  " << ip << " (" << addr->getType() << ")\n";

                // For IPv4 addresses, show network information
                if (addr->getType() == "IPv4") {
                    auto ipv4Addr = dynamic_cast<IPv4*>(addr.get());
                    if (ipv4Addr) {
                        std::string mask =
                            "255.255.255.0";  // Assume /24 network
                        std::string network = ipv4Addr->getNetworkAddress(mask);
                        std::string broadcast =
                            ipv4Addr->getBroadcastAddress(mask);

                        std::cout << "    Network: " << network << "\n";
                        std::cout << "    Broadcast: " << broadcast << "\n";
                    }
                }
            }
        }

        // Step 2: Scan for common services on localhost
        std::cout << "\n2. Scanning for common services on localhost...\n";
        std::vector<uint16_t> commonPorts = {22,   80,   443,  3306,
                                             5432, 6379, 8080, 9200};

        std::cout << "Scanning ports: ";
        for (size_t i = 0; i < commonPorts.size(); ++i) {
            if (i > 0)
                std::cout << ", ";
            std::cout << commonPorts[i];
        }
        std::cout << "\n";

        for (auto port : commonPorts) {
            bool isOpen = scanPort("127.0.0.1", port);
            std::cout << "  Port " << port << ": "
                      << (isOpen ? "OPEN" : "closed") << "\n";

            if (isOpen) {
                // Try to get process information
                auto pid = getProcessIDOnPort(port);
                if (pid) {
                    std::cout << "    Process ID: " << *pid << "\n";
                }
            }
        }

        // Step 3: Test connectivity to external services
        std::cout << "\n3. Testing connectivity to external services...\n";
        std::vector<std::pair<std::string, uint16_t>> externalServices = {
            {"google.com", 80}, {"github.com", 443}, {"httpbin.org", 80}};

        for (const auto& [host, port] : externalServices) {
            std::cout << "Testing " << host << ":" << port << "... ";
            bool isReachable = scanPort(host, port);
            std::cout << (isReachable ? "REACHABLE" : "unreachable") << "\n";

            if (isReachable) {
                // Get IP addresses for the host
                auto ips = getIPAddresses(host);
                if (!ips.empty()) {
                    std::cout << "  IP: " << ips[0];
                    if (ips.size() > 1) {
                        std::cout << " (+" << (ips.size() - 1) << " more)";
                    }
                    std::cout << "\n";
                }
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in network service discovery: " << e.what() << "\n";
    }
}

void demonstrateAPIClientIntegration() {
    std::cout << "\n=== API Client Integration Example ===\n";

    try {
        // Step 1: Create an API client using CURL
        std::cout << "1. Creating API client...\n";
        CurlWrapper apiClient;
        apiClient.addHeader("User-Agent", "AtomAPIClient/1.0");
        apiClient.addHeader("Accept", "application/json");
        apiClient.setTimeout(10L);

        // Step 2: Make API requests and parse responses
        std::cout << "\n2. Making API requests...\n";

        // GET request
        apiClient.setUrl("https://httpbin.org/get?param1=value1&param2=value2");
        apiClient.setRequestMethod("GET");

        std::string getResponse = apiClient.perform();
        std::cout << "GET response received (" << getResponse.length()
                  << " bytes)\n";

        // POST request with JSON
        apiClient.setUrl("https://httpbin.org/post");
        apiClient.setRequestMethod("POST");
        apiClient.addHeader("Content-Type", "application/json");
        apiClient.setRequestBody(R"({
            "user": "testuser",
            "action": "create_resource",
            "data": {
                "name": "Test Resource",
                "value": 42
            }
        })");

        std::string postResponse = apiClient.perform();
        std::cout << "POST response received (" << postResponse.length()
                  << " bytes)\n";

        // Step 3: Analyze response headers
        std::cout << "\n3. Analyzing API response characteristics...\n";

        // Simulate response headers analysis
        HttpHeaderParser headerParser;
        std::string apiHeaders =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json; charset=utf-8\r\n"
            "X-RateLimit-Limit: 1000\r\n"
            "X-RateLimit-Remaining: 999\r\n"
            "X-RateLimit-Reset: 1640995200\r\n"
            "Cache-Control: no-cache\r\n"
            "\r\n";

        headerParser.parseHeaders(apiHeaders);

        // Check rate limiting headers
        if (headerParser.hasHeader("X-RateLimit-Limit")) {
            auto limit = headerParser.getHeaderValues("X-RateLimit-Limit");
            auto remaining =
                headerParser.getHeaderValues("X-RateLimit-Remaining");

            std::cout << "Rate limiting info:\n";
            std::cout << "  Limit: " << (*limit)[0] << "\n";
            if (remaining) {
                std::cout << "  Remaining: " << (*remaining)[0] << "\n";
            }
        }

        // Step 4: Save responses with appropriate file types
        std::cout << "\n4. Saving API responses...\n";
        std::filesystem::create_directories("api_responses");

        MimeTypes mimeTypes({}, true);

        // Save GET response
        std::ofstream getFile("api_responses/get_response.json");
        getFile << getResponse;
        getFile.close();

        // Save POST response
        std::ofstream postFile("api_responses/post_response.json");
        postFile << postResponse;
        postFile.close();

        // Verify MIME types
        auto [getMime, getCharset] =
            mimeTypes.guessType("api_responses/get_response.json");
        auto [postMime, postCharset] =
            mimeTypes.guessType("api_responses/post_response.json");

        std::cout << "Saved responses:\n";
        std::cout << "  GET response: " << (getMime ? *getMime : "unknown")
                  << "\n";
        std::cout << "  POST response: " << (postMime ? *postMime : "unknown")
                  << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in API client integration: " << e.what() << "\n";
    }
}

void cleanupIntegrationFiles() {
    std::cout << "\n=== Cleanup ===\n";

    try {
        std::vector<std::string> pathsToClean = {"crawler_output", "downloads",
                                                 "api_responses",
                                                 "integration_downloads.json"};

        for (const auto& path : pathsToClean) {
            if (std::filesystem::exists(path)) {
                if (std::filesystem::is_directory(path)) {
                    std::filesystem::remove_all(path);
                } else {
                    std::filesystem::remove(path);
                }
                std::cout << "Cleaned up: " << path << "\n";
            }
        }

    } catch (const std::exception& e) {
        std::cout << "Cleanup warning: " << e.what() << "\n";
    }
}

int main(int argc, char** argv) {
    // Initialize logging
    loguru::init(argc, argv);
    loguru::add_file("integration_example.log", loguru::Append,
                     loguru::Verbosity_MAX);

    std::cout << "============================================\n";
    std::cout << "      ATOM WEB INTEGRATION DEMO            \n";
    std::cout << "============================================\n";

    try {
        demonstrateWebCrawlerIntegration();
        demonstrateFileDownloadWithAnalysis();
        demonstrateNetworkServiceDiscovery();
        demonstrateAPIClientIntegration();

        std::cout << "\n============================================\n";
        std::cout << "     WEB INTEGRATION DEMO COMPLETED        \n";
        std::cout << "============================================\n";

        cleanupIntegrationFiles();

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
