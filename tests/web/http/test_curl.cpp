#include <curl/curl.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include <spdlog/spdlog.h>
#include <vector>

#include "atom/web/http/curl.hpp"

using namespace atom::web;
namespace fs = std::filesystem;

class CurlWrapperTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable spdlog output during tests to reduce noise
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }

        // Create temporary directory for test files
        tempDir = fs::temp_directory_path() / "atom_curl_test";
        fs::create_directories(tempDir);

        // Create test files
        testFile = tempDir / "test.txt";
        createTestFile(
            testFile,
            "This is a test file for upload testing.\nLine 2\nLine 3");

        largeTestFile = tempDir / "large_test.txt";
        createLargeTestFile(largeTestFile, 1024);  // 1KB file

        // Reset callback flags
        errorCallbackCalled = false;
        responseCallbackCalled = false;
        lastErrorCode = CURLE_OK;
        lastResponse.clear();
    }

    void TearDown() override {
        try {
            fs::remove_all(tempDir);
        } catch (const std::exception& e) {
            // Ignore cleanup errors
        }
    }

    void createTestFile(const fs::path& path, const std::string& content) {
        std::ofstream file(path);
        file << content;
        file.close();
    }

    void createLargeTestFile(const fs::path& path, size_t sizeKB) {
        std::ofstream file(path);
        std::string chunk(1024, 'A');  // 1KB of 'A' characters
        for (size_t i = 0; i < sizeKB; ++i) {
            file << chunk;
        }
        file.close();
    }

    // Test helper methods
    void setupErrorCallback(CurlWrapper& curl) {
        curl.setOnErrorCallback([this](CURLcode code) {
            errorCallbackCalled = true;
            lastErrorCode = code;
        });
    }

    void setupResponseCallback(CurlWrapper& curl) {
        curl.setOnResponseCallback([this](const std::string& response) {
            responseCallbackCalled = true;
            lastResponse = response;
        });
    }

    // Test data
    fs::path tempDir;
    fs::path testFile;
    fs::path largeTestFile;

    // Callback tracking
    std::atomic<bool> errorCallbackCalled{false};
    std::atomic<bool> responseCallbackCalled{false};
    std::atomic<CURLcode> lastErrorCode{CURLE_OK};
    std::string lastResponse;

    // Test URLs - using httpbin.org for reliable testing
    const std::string TEST_GET_URL = "https://httpbin.org/get";
    const std::string TEST_POST_URL = "https://httpbin.org/post";
    const std::string TEST_PUT_URL = "https://httpbin.org/put";
    const std::string TEST_DELETE_URL = "https://httpbin.org/delete";
    const std::string TEST_REDIRECT_URL = "https://httpbin.org/redirect/3";
    const std::string TEST_DELAY_URL = "https://httpbin.org/delay/2";
    const std::string TEST_STATUS_URL = "https://httpbin.org/status/404";
};

// Basic Constructor/Destructor Tests
TEST_F(CurlWrapperTest, ConstructorDestructor) {
    ASSERT_NO_THROW({ CurlWrapper curl; });
}

// URL Setting Tests
TEST_F(CurlWrapperTest, SetUrl) {
    CurlWrapper curl;

    ASSERT_NO_THROW(curl.setUrl(TEST_GET_URL));
    ASSERT_NO_THROW(curl.setUrl("http://example.com"));
    ASSERT_NO_THROW(curl.setUrl("https://secure.example.com"));
}

TEST_F(CurlWrapperTest, SetUrlEmpty) {
    CurlWrapper curl;

    // Empty URL should not crash but may cause issues during perform
    ASSERT_NO_THROW(curl.setUrl(""));
}

// HTTP Method Tests
TEST_F(CurlWrapperTest, SetRequestMethod) {
    CurlWrapper curl;

    ASSERT_NO_THROW(curl.setRequestMethod("GET"));
    ASSERT_NO_THROW(curl.setRequestMethod("POST"));
    ASSERT_NO_THROW(curl.setRequestMethod("PUT"));
    ASSERT_NO_THROW(curl.setRequestMethod("DELETE"));
    ASSERT_NO_THROW(curl.setRequestMethod("PATCH"));
    ASSERT_NO_THROW(curl.setRequestMethod("HEAD"));
    ASSERT_NO_THROW(curl.setRequestMethod("OPTIONS"));
}

TEST_F(CurlWrapperTest, SetRequestMethodInvalid) {
    CurlWrapper curl;

    // Invalid methods should not crash
    ASSERT_NO_THROW(curl.setRequestMethod("INVALID"));
    ASSERT_NO_THROW(curl.setRequestMethod(""));
}

// Header Tests
TEST_F(CurlWrapperTest, AddHeader) {
    CurlWrapper curl;

    ASSERT_NO_THROW(curl.addHeader("User-Agent", "TestAgent/1.0"));
    ASSERT_NO_THROW(curl.addHeader("Content-Type", "application/json"));
    ASSERT_NO_THROW(curl.addHeader("Authorization", "Bearer token123"));
    ASSERT_NO_THROW(curl.addHeader("Accept", "application/json"));
}

TEST_F(CurlWrapperTest, AddHeaderEmpty) {
    CurlWrapper curl;

    ASSERT_NO_THROW(curl.addHeader("", ""));
    ASSERT_NO_THROW(curl.addHeader("EmptyValue", ""));
    ASSERT_NO_THROW(curl.addHeader("", "EmptyKey"));
}

// Callback Tests
TEST_F(CurlWrapperTest, SetCallbacks) {
    CurlWrapper curl;

    ASSERT_NO_THROW(setupErrorCallback(curl));
    ASSERT_NO_THROW(setupResponseCallback(curl));
}

// Timeout Tests
TEST_F(CurlWrapperTest, SetTimeout) {
    CurlWrapper curl;

    ASSERT_NO_THROW(curl.setTimeout(30L));
    ASSERT_NO_THROW(curl.setTimeout(0L));     // No timeout
    ASSERT_NO_THROW(curl.setTimeout(1L));     // Very short timeout
    ASSERT_NO_THROW(curl.setTimeout(3600L));  // Long timeout
}

// Follow Location Tests
TEST_F(CurlWrapperTest, SetFollowLocation) {
    CurlWrapper curl;

    ASSERT_NO_THROW(curl.setFollowLocation(true));
    ASSERT_NO_THROW(curl.setFollowLocation(false));
}

// Request Body Tests
TEST_F(CurlWrapperTest, SetRequestBody) {
    CurlWrapper curl;

    ASSERT_NO_THROW(curl.setRequestBody("param1=value1&param2=value2"));
    ASSERT_NO_THROW(curl.setRequestBody(R"({"key": "value", "number": 123})"));
    ASSERT_NO_THROW(curl.setRequestBody(""));

    // Large body
    std::string largeBody(10000, 'X');
    ASSERT_NO_THROW(curl.setRequestBody(largeBody));
}

// File Upload Tests
TEST_F(CurlWrapperTest, SetUploadFile) {
    CurlWrapper curl;

    ASSERT_NO_THROW(curl.setUploadFile(testFile.string()));
}

TEST_F(CurlWrapperTest, SetUploadFileNonexistent) {
    CurlWrapper curl;

    // Should not crash, but will fail during perform
    ASSERT_NO_THROW(curl.setUploadFile("nonexistent_file.txt"));
}

// Proxy Tests
TEST_F(CurlWrapperTest, SetProxy) {
    CurlWrapper curl;

    ASSERT_NO_THROW(curl.setProxy("http://proxy.example.com:8080"));
    ASSERT_NO_THROW(curl.setProxy("socks5://proxy.example.com:1080"));
    ASSERT_NO_THROW(curl.setProxy(""));
}

// SSL Options Tests
TEST_F(CurlWrapperTest, SetSSLOptions) {
    CurlWrapper curl;

    ASSERT_NO_THROW(curl.setSSLOptions(true, true));    // Verify both
    ASSERT_NO_THROW(curl.setSSLOptions(false, false));  // Verify neither
    ASSERT_NO_THROW(curl.setSSLOptions(true, false));   // Verify peer only
    ASSERT_NO_THROW(curl.setSSLOptions(false, true));   // Verify host only
}

// Download Speed Tests
TEST_F(CurlWrapperTest, SetMaxDownloadSpeed) {
    CurlWrapper curl;

    ASSERT_NO_THROW(curl.setMaxDownloadSpeed(1024));     // 1KB/s
    ASSERT_NO_THROW(curl.setMaxDownloadSpeed(1048576));  // 1MB/s
    ASSERT_NO_THROW(curl.setMaxDownloadSpeed(0));        // No limit
}

// Basic HTTP Request Tests
TEST_F(CurlWrapperTest, BasicGETRequest) {
    CurlWrapper curl;
    curl.setUrl(TEST_GET_URL);
    curl.setRequestMethod("GET");
    curl.setTimeout(10L);

    setupResponseCallback(curl);

    std::string response;
    ASSERT_NO_THROW(response = curl.perform());

    EXPECT_FALSE(response.empty());
    EXPECT_TRUE(responseCallbackCalled);
    EXPECT_FALSE(lastResponse.empty());
}

TEST_F(CurlWrapperTest, BasicPOSTRequest) {
    CurlWrapper curl;
    curl.setUrl(TEST_POST_URL);
    curl.setRequestMethod("POST");
    curl.addHeader("Content-Type", "application/json");
    curl.setRequestBody(R"({"test": "data", "number": 42})");
    curl.setTimeout(10L);

    std::string response;
    ASSERT_NO_THROW(response = curl.perform());

    EXPECT_FALSE(response.empty());
    // Response should contain our posted data
    EXPECT_THAT(response, ::testing::HasSubstr("test"));
    EXPECT_THAT(response, ::testing::HasSubstr("data"));
}

TEST_F(CurlWrapperTest, BasicPUTRequest) {
    CurlWrapper curl;
    curl.setUrl(TEST_PUT_URL);
    curl.setRequestMethod("PUT");
    curl.addHeader("Content-Type", "text/plain");
    curl.setRequestBody("Updated content");
    curl.setTimeout(10L);

    std::string response;
    ASSERT_NO_THROW(response = curl.perform());

    EXPECT_FALSE(response.empty());
}

TEST_F(CurlWrapperTest, BasicDELETERequest) {
    CurlWrapper curl;
    curl.setUrl(TEST_DELETE_URL);
    curl.setRequestMethod("DELETE");
    curl.setTimeout(10L);

    std::string response;
    ASSERT_NO_THROW(response = curl.perform());

    EXPECT_FALSE(response.empty());
}

// Header Tests with Real Requests
TEST_F(CurlWrapperTest, CustomHeaders) {
    CurlWrapper curl;
    curl.setUrl(TEST_GET_URL);
    curl.setRequestMethod("GET");
    curl.addHeader("User-Agent", "CurlWrapperTest/1.0");
    curl.addHeader("X-Test-Header", "TestValue");
    curl.addHeader("Accept", "application/json");
    curl.setTimeout(10L);

    std::string response;
    ASSERT_NO_THROW(response = curl.perform());

    EXPECT_FALSE(response.empty());
    // httpbin.org echoes headers back, so we should see our custom headers
    EXPECT_THAT(response, ::testing::HasSubstr("CurlWrapperTest/1.0"));
    EXPECT_THAT(response, ::testing::HasSubstr("X-Test-Header"));
}

// Redirect Tests
TEST_F(CurlWrapperTest, FollowRedirects) {
    CurlWrapper curl;
    curl.setUrl(TEST_REDIRECT_URL);
    curl.setRequestMethod("GET");
    curl.setFollowLocation(true);
    curl.setTimeout(15L);

    std::string response;
    ASSERT_NO_THROW(response = curl.perform());

    EXPECT_FALSE(response.empty());
}

TEST_F(CurlWrapperTest, NoFollowRedirects) {
    CurlWrapper curl;
    curl.setUrl(TEST_REDIRECT_URL);
    curl.setRequestMethod("GET");
    curl.setFollowLocation(false);
    curl.setTimeout(10L);

    setupErrorCallback(curl);

    // This might throw or return empty response depending on implementation
    ASSERT_NO_THROW(curl.perform());
}

// SSL Tests
TEST_F(CurlWrapperTest, SSLVerification) {
    CurlWrapper curl;
    curl.setUrl("https://httpbin.org/get");
    curl.setRequestMethod("GET");
    curl.setSSLOptions(true, true);  // Verify peer and host
    curl.setTimeout(10L);

    std::string response;
    ASSERT_NO_THROW(response = curl.perform());

    EXPECT_FALSE(response.empty());
}

TEST_F(CurlWrapperTest, SSLNoVerification) {
    CurlWrapper curl;
    curl.setUrl("https://httpbin.org/get");
    curl.setRequestMethod("GET");
    curl.setSSLOptions(false, false);  // Don't verify
    curl.setTimeout(10L);

    std::string response;
    ASSERT_NO_THROW(response = curl.perform());

    EXPECT_FALSE(response.empty());
}

// File Upload Tests
TEST_F(CurlWrapperTest, FileUpload) {
    CurlWrapper curl;
    curl.setUrl(TEST_POST_URL);
    curl.setRequestMethod("POST");
    curl.setUploadFile(testFile.string());
    curl.setTimeout(15L);

    std::string response;
    ASSERT_NO_THROW(response = curl.perform());

    EXPECT_FALSE(response.empty());
}

// Error Handling Tests
TEST_F(CurlWrapperTest, InvalidURL) {
    CurlWrapper curl;
    curl.setUrl("invalid://not.a.real.url");
    curl.setRequestMethod("GET");
    curl.setTimeout(5L);

    setupErrorCallback(curl);

    EXPECT_THROW(curl.perform(), std::exception);
    // Error callback should be called
    EXPECT_TRUE(errorCallbackCalled);
}

TEST_F(CurlWrapperTest, TimeoutError) {
    CurlWrapper curl;
    curl.setUrl(TEST_DELAY_URL);  // This URL delays for 2 seconds
    curl.setRequestMethod("GET");
    curl.setTimeout(1L);  // Timeout after 1 second

    setupErrorCallback(curl);

    EXPECT_THROW(curl.perform(), std::exception);
    EXPECT_TRUE(errorCallbackCalled);
    EXPECT_EQ(lastErrorCode, CURLE_OPERATION_TIMEDOUT);
}

// Async Operation Tests
TEST_F(CurlWrapperTest, AsyncRequest) {
    CurlWrapper curl;
    curl.setUrl(TEST_GET_URL);
    curl.setRequestMethod("GET");
    curl.setTimeout(10L);

    setupResponseCallback(curl);

    ASSERT_NO_THROW(curl.performAsync());
    ASSERT_NO_THROW(curl.waitAll());

    EXPECT_TRUE(responseCallbackCalled);
    EXPECT_FALSE(lastResponse.empty());
}

TEST_F(CurlWrapperTest, MultipleAsyncRequests) {
    std::vector<std::unique_ptr<CurlWrapper>> curls;

    for (int i = 0; i < 3; ++i) {
        auto curl = std::make_unique<CurlWrapper>();
        curl->setUrl(TEST_GET_URL);
        curl->setRequestMethod("GET");
        curl->setTimeout(10L);
        curl->addHeader("X-Request-ID", std::to_string(i));

        ASSERT_NO_THROW(curl->performAsync());
        curls.push_back(std::move(curl));
    }

    // Wait for all requests to complete
    for (auto& curl : curls) {
        ASSERT_NO_THROW(curl->waitAll());
    }
}

// Speed Limit Tests
TEST_F(CurlWrapperTest, DownloadSpeedLimit) {
    CurlWrapper curl;
    curl.setUrl(TEST_GET_URL);
    curl.setRequestMethod("GET");
    curl.setMaxDownloadSpeed(1024);  // 1KB/s limit
    curl.setTimeout(30L);            // Longer timeout for speed-limited request

    auto start = std::chrono::high_resolution_clock::now();
    std::string response;
    ASSERT_NO_THROW(response = curl.perform());
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_FALSE(response.empty());

    // The request should take some time due to speed limiting
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    // Note: This is a rough test, actual timing may vary
    EXPECT_GT(duration.count(), 0);  // Should take some time
}

// Edge Cases and Stress Tests
TEST_F(CurlWrapperTest, EmptyResponse) {
    CurlWrapper curl;
    curl.setUrl("https://httpbin.org/status/204");  // No Content
    curl.setRequestMethod("GET");
    curl.setTimeout(10L);

    std::string response;
    ASSERT_NO_THROW(response = curl.perform());

    // 204 No Content should return empty response
    EXPECT_TRUE(response.empty());
}

TEST_F(CurlWrapperTest, LargeResponse) {
    CurlWrapper curl;
    curl.setUrl("https://httpbin.org/bytes/10240");  // 10KB response
    curl.setRequestMethod("GET");
    curl.setTimeout(30L);

    std::string response;
    ASSERT_NO_THROW(response = curl.perform());

    EXPECT_EQ(response.size(), 10240);
}

TEST_F(CurlWrapperTest, HTTPErrorStatus) {
    CurlWrapper curl;
    curl.setUrl(TEST_STATUS_URL);  // Returns 404
    curl.setRequestMethod("GET");
    curl.setTimeout(10L);

    setupErrorCallback(curl);

    std::string response;
    // HTTP error status codes don't necessarily throw exceptions
    ASSERT_NO_THROW(response = curl.perform());

    // Response might be empty or contain error page
}

// Method Chaining Tests
TEST_F(CurlWrapperTest, MethodChaining) {
    CurlWrapper curl;

    ASSERT_NO_THROW({
        curl.setUrl(TEST_GET_URL)
            .setRequestMethod("GET")
            .addHeader("User-Agent", "ChainTest/1.0")
            .setTimeout(10L)
            .setFollowLocation(true)
            .setSSLOptions(true, true);
    });

    std::string response;
    ASSERT_NO_THROW(response = curl.perform());
    EXPECT_FALSE(response.empty());
}

// Callback Edge Cases
TEST_F(CurlWrapperTest, NullCallbacks) {
    CurlWrapper curl;
    curl.setUrl(TEST_GET_URL);
    curl.setRequestMethod("GET");
    curl.setTimeout(10L);

    // Set null callbacks
    ASSERT_NO_THROW(curl.setOnErrorCallback(nullptr));
    ASSERT_NO_THROW(curl.setOnResponseCallback(nullptr));

    std::string response;
    ASSERT_NO_THROW(response = curl.perform());
    EXPECT_FALSE(response.empty());
}

TEST_F(CurlWrapperTest, CallbackExceptions) {
    CurlWrapper curl;
    curl.setUrl(TEST_GET_URL);
    curl.setRequestMethod("GET");
    curl.setTimeout(10L);

    // Set callbacks that throw exceptions
    curl.setOnErrorCallback(
        [](CURLcode) { throw std::runtime_error("Error callback exception"); });

    curl.setOnResponseCallback([](const std::string&) {
        throw std::runtime_error("Response callback exception");
    });

    // The perform should still work despite callback exceptions
    std::string response;
    ASSERT_NO_THROW(response = curl.perform());
    EXPECT_FALSE(response.empty());
}
