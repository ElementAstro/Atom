// filepath: atom/extra/curl/test_rest_client.cpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <future>
#include <sstream>
#include <string>
#include <thread>

#include "atom/extra/curl/mock_server.hpp"  // Assume we have a mock server implementation
#include "atom/extra/curl/rest_client.hpp"


using namespace atom::extra::curl;
using ::testing::HasSubstr;
using ::testing::StartsWith;

class RestClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Start a mock server on a local port
        mock_server_.start();
        base_url_ = "http://localhost:" + std::to_string(mock_server_.port());

        // Configure mock server responses
        mock_server_.add_route("GET", "/test", 200, "Test response");
        mock_server_.add_route("POST", "/test", 201, "Created resource");
        mock_server_.add_route("PUT", "/test", 200, "Updated resource");
        mock_server_.add_route("DELETE", "/test", 204, "");
        mock_server_.add_route("GET", "/error", 500, "Internal server error");
        mock_server_.add_route("GET", "/not-found", 404, "Not found");

        // Add JSON response
        mock_server_.add_route("GET", "/json", 200, "{\"key\": \"value\"}",
                               {{"Content-Type", "application/json"}});
    }

    void TearDown() override { mock_server_.stop(); }

    MockServer mock_server_;
    std::string base_url_;
};

// Test global GET function
TEST_F(RestClientTest, GlobalGetFunction) {
    auto response = get(base_url_ + "/test");

    EXPECT_EQ(response.status_code(), 200);
    EXPECT_EQ(response.body(), "Test response");
}

// Test global POST function
TEST_F(RestClientTest, GlobalPostFunction) {
    auto response = post(base_url_ + "/test", "{\"data\":\"test\"}");

    EXPECT_EQ(response.status_code(), 201);
    EXPECT_EQ(response.body(), "Created resource");
}

// Test global PUT function
TEST_F(RestClientTest, GlobalPutFunction) {
    auto response = put(base_url_ + "/test", "{\"data\":\"updated\"}");

    EXPECT_EQ(response.status_code(), 200);
    EXPECT_EQ(response.body(), "Updated resource");
}

// Test global DELETE function
TEST_F(RestClientTest, GlobalDeleteFunction) {
    auto response = del(base_url_ + "/test");

    EXPECT_EQ(response.status_code(), 204);
    EXPECT_TRUE(response.body().empty());
}

// Test error handling with fetch function
TEST_F(RestClientTest, FetchFunctionErrorHandling) {
    Request request(Request::Method::GET, base_url_ + "/error");
    bool success_called = false;
    bool error_called = false;
    std::string error_message;

    fetch(
        request,
        [&success_called](const Response& response) { success_called = true; },
        [&error_called, &error_message](const Error& error) {
            error_called = true;
            error_message = error.what();
        });

    EXPECT_FALSE(success_called);
    EXPECT_TRUE(error_called);
    EXPECT_THAT(error_message, HasSubstr("500"));
}

// Test successful response with fetch function
TEST_F(RestClientTest, FetchFunctionSuccessHandling) {
    Request request(Request::Method::GET, base_url_ + "/test");
    bool success_called = false;
    bool error_called = false;
    std::string response_body;

    fetch(
        request,
        [&success_called, &response_body](const Response& response) {
            success_called = true;
            response_body = response.body();
        },
        [&error_called](const Error& error) { error_called = true; });

    EXPECT_TRUE(success_called);
    EXPECT_FALSE(error_called);
    EXPECT_EQ(response_body, "Test response");
}

// Test coroutine fetch function
TEST_F(RestClientTest, CoroutineFetchFunction) {
    auto task = [this]() -> Task<Response> {
        Request request(Request::Method::GET, base_url_ + "/test");
        try {
            Response response = co_await fetch(std::move(request));
            co_return response;
        } catch (const Error& e) {
            throw e;
        }
    }();

    Response response = task.result();
    EXPECT_EQ(response.status_code(), 200);
    EXPECT_EQ(response.body(), "Test response");
}

// Test fetch_async function
TEST_F(RestClientTest, FetchAsyncFunction) {
    Request request(Request::Method::GET, base_url_ + "/test");
    auto task = fetch_async(std::move(request));

    Response response = task.result();
    EXPECT_EQ(response.status_code(), 200);
    EXPECT_EQ(response.body(), "Test response");
}

// Test fetch_async with error
TEST_F(RestClientTest, FetchAsyncWithError) {
    Request request(Request::Method::GET, base_url_ + "/error");
    auto task = fetch_async(std::move(request));

    EXPECT_THROW({ Response response = task.result(); }, Error);
}

// Test RestClient GET method
TEST_F(RestClientTest, RestClientGet) {
    RestClient client(base_url_);
    auto response = client.get("/test");

    EXPECT_EQ(response.status_code(), 200);
    EXPECT_EQ(response.body(), "Test response");
}

// Test RestClient GET with query parameters
TEST_F(RestClientTest, RestClientGetWithQueryParams) {
    // Add a route that echoes query parameters
    mock_server_.add_route(
        "GET", "/query", 200,
        [](const std::map<std::string, std::string>& params) {
            return "param1=" + params.at("param1") +
                   "&param2=" + params.at("param2");
        });

    RestClient client(base_url_);
    std::map<std::string, std::string> params = {{"param1", "value1"},
                                                 {"param2", "value2"}};

    auto response = client.get("/query", params);

    EXPECT_EQ(response.status_code(), 200);
    EXPECT_EQ(response.body(), "param1=value1&param2=value2");
}

// Test RestClient POST method
TEST_F(RestClientTest, RestClientPost) {
    RestClient client(base_url_);
    auto response = client.post("/test", "{\"data\":\"test\"}");

    EXPECT_EQ(response.status_code(), 201);
    EXPECT_EQ(response.body(), "Created resource");
}

// Test RestClient PUT method
TEST_F(RestClientTest, RestClientPut) {
    RestClient client(base_url_);
    auto response = client.put("/test", "{\"data\":\"updated\"}");

    EXPECT_EQ(response.status_code(), 200);
    EXPECT_EQ(response.body(), "Updated resource");
}

// Test RestClient DELETE method
TEST_F(RestClientTest, RestClientDelete) {
    RestClient client(base_url_);
    auto response = client.del("/test");

    EXPECT_EQ(response.status_code(), 204);
    EXPECT_TRUE(response.body().empty());
}

// Test RestClient with headers
TEST_F(RestClientTest, RestClientWithHeaders) {
    // Add a route that echoes headers
    mock_server_.add_route(
        "GET", "/headers", 200,
        [](const std::map<std::string, std::string>& headers) {
            return headers.at("X-Test-Header");
        });

    RestClient client(base_url_);
    client.set_header("X-Test-Header", "test-value");

    auto response = client.get("/headers");

    EXPECT_EQ(response.status_code(), 200);
    EXPECT_EQ(response.body(), "test-value");
}

// Test RestClient with authorization
TEST_F(RestClientTest, RestClientWithAuthorization) {
    // Add a route that echoes authorization header
    mock_server_.add_route(
        "GET", "/auth", 200,
        [](const std::map<std::string, std::string>& headers) {
            return headers.at("Authorization");
        });

    RestClient client(base_url_);
    client.set_auth_token("my-token");

    auto response = client.get("/auth");

    EXPECT_EQ(response.status_code(), 200);
    EXPECT_EQ(response.body(), "Bearer my-token");
}

// Test RestClient URL construction
TEST_F(RestClientTest, RestClientUrlConstruction) {
    // Test with trailing slash in base URL
    {
        RestClient client(base_url_ + "/");
        auto response = client.get("test");
        EXPECT_EQ(response.status_code(), 200);
    }

    // Test with path starting with slash
    {
        RestClient client(base_url_);
        auto response = client.get("/test");
        EXPECT_EQ(response.status_code(), 200);
    }

    // Test with empty path
    {
        // Add a route that responds to the base URL
        mock_server_.add_route("GET", "/", 200, "Base URL");

        RestClient client(base_url_);
        auto response = client.get("");
        EXPECT_EQ(response.status_code(), 200);
        EXPECT_EQ(response.body(), "Base URL");
    }
}

// Test LoggingInterceptor
TEST_F(RestClientTest, LoggingInterceptor) {
    std::stringstream log_stream;
    LoggingInterceptor interceptor(log_stream);

    Session session;
    session.add_interceptor(std::make_shared<LoggingInterceptor>(log_stream));

    auto response = session.get(base_url_ + "/test");

    std::string log_content = log_stream.str();
    EXPECT_THAT(log_content, HasSubstr("Request: GET"));
    EXPECT_THAT(log_content, HasSubstr(base_url_ + "/test"));
    EXPECT_THAT(log_content, HasSubstr("Response: 200"));
    EXPECT_THAT(log_content, HasSubstr("Test response"));
}

// Test LoggingInterceptor with large body
TEST_F(RestClientTest, LoggingInterceptorWithLargeBody) {
    // Create a large response
    std::string large_body(1000, 'X');
    mock_server_.add_route("GET", "/large", 200, large_body);

    std::stringstream log_stream;

    Session session;
    session.add_interceptor(std::make_shared<LoggingInterceptor>(log_stream));

    auto response = session.get(base_url_ + "/large");

    std::string log_content = log_stream.str();
    EXPECT_THAT(log_content, HasSubstr("Response: 200"));
    EXPECT_THAT(log_content, HasSubstr("..."));  // Should truncate the body
}

// Test multiple concurrent requests
TEST_F(RestClientTest, ConcurrentRequests) {
    constexpr int num_threads = 10;
    std::vector<std::thread> threads;
    std::vector<Response> responses(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i, &responses]() {
            static thread_local Session session;
            responses[i] = session.get(base_url_ + "/test");
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    for (const auto& response : responses) {
        EXPECT_EQ(response.status_code(), 200);
        EXPECT_EQ(response.body(), "Test response");
    }
}

// Test ResponseHandler concept
TEST_F(RestClientTest, ResponseHandlerConcept) {
    // Create a lambda that satisfies ResponseHandler
    auto handler = [](const Response& response) {
        return response.status_code() == 200;
    };

    // This should compile if the concept is satisfied
    static_assert(ResponseHandler<decltype(handler)>,
                  "Handler does not satisfy ResponseHandler concept");

    // Create a lambda that does not satisfy ResponseHandler
    auto non_handler = []() { return true; };

    // This should not compile
    static_assert(!ResponseHandler<decltype(non_handler)>,
                  "Non-handler incorrectly satisfies ResponseHandler concept");
}

// Test ErrorHandler concept
TEST_F(RestClientTest, ErrorHandlerConcept) {
    // Create a lambda that satisfies ErrorHandler
    auto handler = [](const Error& error) { return error.what(); };

    // This should compile if the concept is satisfied
    static_assert(ErrorHandler<decltype(handler)>,
                  "Handler does not satisfy ErrorHandler concept");

    // Create a lambda that does not satisfy ErrorHandler
    auto non_handler = []() { return "error"; };

    // This should not compile
    static_assert(!ErrorHandler<decltype(non_handler)>,
                  "Non-handler incorrectly satisfies ErrorHandler concept");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
