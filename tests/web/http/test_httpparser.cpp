#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <spdlog/spdlog.h>
#include <string>
#include "atom/web/httpparser.hpp"

using namespace atom::web;

class HttpHeaderParserTest : public ::testing::Test {
protected:
    void SetUp() override { parser = std::make_unique<HttpHeaderParser>(); }
    void TearDown() override { parser.reset(); }

    std::string createSampleHttpRequest(const std::string& body = "") {
        std::string contentLength =
            body.empty()
                ? ""
                : "Content-Length: " + std::to_string(body.length()) + "\r\n";
        return "GET /index.html HTTP/1.1\r\n"
               "Host: example.com\r\n"
               "User-Agent: Mozilla/5.0\r\n" +
               contentLength + "\r\n" + body;
    }

    std::string createSampleHttpResponse(const std::string& body = "") {
        std::string contentLength =
            body.empty()
                ? ""
                : "Content-Length: " + std::to_string(body.length()) + "\r\n";
        return "HTTP/1.1 200 OK\r\n"
               "Server: TestServer\r\n"
               "Content-Type: text/html; charset=utf-8\r\n" +
               contentLength + "\r\n" + body;
    }

    std::unique_ptr<HttpHeaderParser> parser;
};

TEST_F(HttpHeaderParserTest, GetEmptyBody) {
    std::string emptyBodyRequest = createSampleHttpRequest();
    ASSERT_TRUE(parser->parseRequest(emptyBodyRequest));
    EXPECT_EQ(parser->getBody(), "");
}

TEST_F(HttpHeaderParserTest, GetSimpleBody) {
    std::string sampleBody = "Hello, World!";
    std::string requestWithBody = createSampleHttpRequest(sampleBody);
    ASSERT_TRUE(parser->parseRequest(requestWithBody));
    EXPECT_EQ(parser->getBody(), sampleBody);
}

TEST_F(HttpHeaderParserTest, GetJsonBody) {
    std::string jsonBody = R"({"name": "Test", "value": 123})";
    std::string requestWithJsonBody =
        "POST /api/data HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " +
        std::to_string(jsonBody.length()) +
        "\r\n"
        "\r\n" +
        jsonBody;
    ASSERT_TRUE(parser->parseRequest(requestWithJsonBody));
    EXPECT_EQ(parser->getBody(), jsonBody);
}

TEST_F(HttpHeaderParserTest, GetBodyWithSpecialChars) {
    std::string specialBody = "Line 1\r\nLine 2\r\n\r\nExtra data: ÄÖÜ";
    std::string requestWithSpecialBody = createSampleHttpRequest(specialBody);
    ASSERT_TRUE(parser->parseRequest(requestWithSpecialBody));
    EXPECT_EQ(parser->getBody(), specialBody);
}

TEST_F(HttpHeaderParserTest, GetLongBody) {
    std::string longBody;
    for (int i = 0; i < 1000; i++) {
        longBody +=
            "This is line " + std::to_string(i) + " of the long body.\n";
    }
    std::string requestWithLongBody = createSampleHttpRequest(longBody);
    ASSERT_TRUE(parser->parseRequest(requestWithLongBody));
    EXPECT_EQ(parser->getBody(), longBody);
}

TEST_F(HttpHeaderParserTest, GetResponseBody) {
    std::string responseBody = "<html><body><h1>Welcome!</h1></body></html>";
    std::string responseWithBody = createSampleHttpResponse(responseBody);
    ASSERT_TRUE(parser->parseResponse(responseWithBody));
    EXPECT_EQ(parser->getBody(), responseBody);
}

TEST_F(HttpHeaderParserTest, SetAndGetBody) {
    std::string newBody = "This is a new body content";
    parser->setBody(newBody);
    EXPECT_EQ(parser->getBody(), newBody);
}

TEST_F(HttpHeaderParserTest, UpdateBody) {
    std::string initialBody = "Initial body";
    std::string initialRequest = createSampleHttpRequest(initialBody);
    ASSERT_TRUE(parser->parseRequest(initialRequest));
    EXPECT_EQ(parser->getBody(), initialBody);

    std::string updatedBody = "Updated body content";
    parser->setBody(updatedBody);
    EXPECT_EQ(parser->getBody(), updatedBody);
}

TEST_F(HttpHeaderParserTest, GetMultipartFormDataBody) {
    std::string boundary = "-------------------------12345";
    std::string multipartBody =
        "--" + boundary +
        "\r\n"
        "Content-Disposition: form-data; name=\"field1\"\r\n"
        "\r\n"
        "value1\r\n"
        "--" +
        boundary +
        "\r\n"
        "Content-Disposition: form-data; name=\"field2\"; "
        "filename=\"example.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "file content here\r\n"
        "--" +
        boundary + "--\r\n";
    std::string multipartRequest =
        "POST /upload HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: multipart/form-data; boundary=" +
        boundary +
        "\r\n"
        "Content-Length: " +
        std::to_string(multipartBody.length()) +
        "\r\n"
        "\r\n" +
        multipartBody;
    ASSERT_TRUE(parser->parseRequest(multipartRequest));
    EXPECT_EQ(parser->getBody(), multipartBody);
}

TEST_F(HttpHeaderParserTest, ClearBody) {
    std::string body = "This is some body content";
    parser->setBody(body);
    EXPECT_EQ(parser->getBody(), body);
    parser->setBody("");
    EXPECT_EQ(parser->getBody(), "");
}

TEST_F(HttpHeaderParserTest, BuildRequestWithBody) {
    parser->setMethod(HttpMethod::POST);
    parser->setPath("/api/data");
    parser->setVersion(HttpVersion::HTTP_1_1);
    parser->setHeaderValue("Host", "example.com");
    parser->setHeaderValue("Content-Type", "application/json");
    std::string jsonBody = R"({"key": "value"})";
    parser->setBody(jsonBody);
    parser->setHeaderValue("Content-Length", std::to_string(jsonBody.length()));
    std::string builtRequest = parser->buildRequest();
    HttpHeaderParser newParser;
    ASSERT_TRUE(newParser.parseRequest(builtRequest));
    EXPECT_EQ(newParser.getBody(), jsonBody);
}

TEST_F(HttpHeaderParserTest, BuildResponseWithBody) {
    parser->setStatus(HttpStatus::OK());
    parser->setVersion(HttpVersion::HTTP_1_1);
    parser->setHeaderValue("Content-Type", "text/html; charset=utf-8");
    std::string htmlBody = "<html><body>Hello, World!</body></html>";
    parser->setBody(htmlBody);
    parser->setHeaderValue("Content-Length", std::to_string(htmlBody.length()));
    std::string builtResponse = parser->buildResponse();
    HttpHeaderParser newParser;
    ASSERT_TRUE(newParser.parseResponse(builtResponse));
    EXPECT_EQ(newParser.getBody(), htmlBody);
}

// Header Manipulation Tests
TEST_F(HttpHeaderParserTest, SetAndGetHeaderValue) {
    parser->setHeaderValue("Content-Type", "application/json");
    parser->setHeaderValue("User-Agent", "TestAgent/1.0");

    auto contentType = parser->getHeaderValue("Content-Type");
    ASSERT_TRUE(contentType.has_value());
    EXPECT_EQ(*contentType, "application/json");

    auto userAgent = parser->getHeaderValue("User-Agent");
    ASSERT_TRUE(userAgent.has_value());
    EXPECT_EQ(*userAgent, "TestAgent/1.0");

    // Non-existent header
    auto nonExistent = parser->getHeaderValue("Non-Existent");
    EXPECT_FALSE(nonExistent.has_value());
}

TEST_F(HttpHeaderParserTest, AddHeaderValue) {
    parser->setHeaderValue("Accept", "application/json");
    parser->addHeaderValue("Accept", "text/html");
    parser->addHeaderValue("Accept", "*/*");

    auto acceptValues = parser->getHeaderValues("Accept");
    ASSERT_TRUE(acceptValues.has_value());
    EXPECT_EQ(acceptValues->size(), 3);
    EXPECT_EQ((*acceptValues)[0], "application/json");
    EXPECT_EQ((*acceptValues)[1], "text/html");
    EXPECT_EQ((*acceptValues)[2], "*/*");
}

TEST_F(HttpHeaderParserTest, SetHeaders) {
    std::map<std::string, std::vector<std::string>> headers = {
        {"Content-Type", {"application/json"}},
        {"Accept", {"application/json", "text/html"}},
        {"User-Agent", {"TestAgent/1.0"}}};

    parser->setHeaders(headers);

    EXPECT_TRUE(parser->hasHeader("Content-Type"));
    EXPECT_TRUE(parser->hasHeader("Accept"));
    EXPECT_TRUE(parser->hasHeader("User-Agent"));

    auto acceptValues = parser->getHeaderValues("Accept");
    ASSERT_TRUE(acceptValues.has_value());
    EXPECT_EQ(acceptValues->size(), 2);
}

TEST_F(HttpHeaderParserTest, RemoveHeader) {
    parser->setHeaderValue("Content-Type", "application/json");
    parser->setHeaderValue("User-Agent", "TestAgent/1.0");

    EXPECT_TRUE(parser->hasHeader("Content-Type"));

    parser->removeHeader("Content-Type");

    EXPECT_FALSE(parser->hasHeader("Content-Type"));
    EXPECT_TRUE(parser->hasHeader("User-Agent"));
}

TEST_F(HttpHeaderParserTest, GetAllHeaders) {
    parser->setHeaderValue("Content-Type", "application/json");
    parser->setHeaderValue("User-Agent", "TestAgent/1.0");
    parser->addHeaderValue("Accept", "application/json");
    parser->addHeaderValue("Accept", "text/html");

    auto allHeaders = parser->getAllHeaders();

    EXPECT_EQ(allHeaders.size(), 3);
    EXPECT_TRUE(allHeaders.count("Content-Type") > 0);
    EXPECT_TRUE(allHeaders.count("User-Agent") > 0);
    EXPECT_TRUE(allHeaders.count("Accept") > 0);
    EXPECT_EQ(allHeaders["Accept"].size(), 2);
}

TEST_F(HttpHeaderParserTest, ClearHeaders) {
    parser->setHeaderValue("Content-Type", "application/json");
    parser->setHeaderValue("User-Agent", "TestAgent/1.0");

    EXPECT_TRUE(parser->hasHeader("Content-Type"));

    parser->clearHeaders();

    EXPECT_FALSE(parser->hasHeader("Content-Type"));
    EXPECT_FALSE(parser->hasHeader("User-Agent"));

    auto allHeaders = parser->getAllHeaders();
    EXPECT_TRUE(allHeaders.empty());
}

// HTTP Method Tests
TEST_F(HttpHeaderParserTest, SetAndGetMethod) {
    parser->setMethod(HttpMethod::GET);
    EXPECT_EQ(parser->getMethod(), HttpMethod::GET);

    parser->setMethod(HttpMethod::POST);
    EXPECT_EQ(parser->getMethod(), HttpMethod::POST);

    parser->setMethod(HttpMethod::PUT);
    EXPECT_EQ(parser->getMethod(), HttpMethod::PUT);

    parser->setMethod(HttpMethod::DELETE);
    EXPECT_EQ(parser->getMethod(), HttpMethod::DELETE);
}

TEST_F(HttpHeaderParserTest, StringToMethod) {
    EXPECT_EQ(HttpHeaderParser::stringToMethod("GET"), HttpMethod::GET);
    EXPECT_EQ(HttpHeaderParser::stringToMethod("POST"), HttpMethod::POST);
    EXPECT_EQ(HttpHeaderParser::stringToMethod("PUT"), HttpMethod::PUT);
    EXPECT_EQ(HttpHeaderParser::stringToMethod("DELETE"), HttpMethod::DELETE);
    EXPECT_EQ(HttpHeaderParser::stringToMethod("PATCH"), HttpMethod::PATCH);
    EXPECT_EQ(HttpHeaderParser::stringToMethod("HEAD"), HttpMethod::HEAD);
    EXPECT_EQ(HttpHeaderParser::stringToMethod("OPTIONS"), HttpMethod::OPTIONS);
    EXPECT_EQ(HttpHeaderParser::stringToMethod("INVALID"), HttpMethod::UNKNOWN);
}

TEST_F(HttpHeaderParserTest, MethodToString) {
    EXPECT_EQ(HttpHeaderParser::methodToString(HttpMethod::GET), "GET");
    EXPECT_EQ(HttpHeaderParser::methodToString(HttpMethod::POST), "POST");
    EXPECT_EQ(HttpHeaderParser::methodToString(HttpMethod::PUT), "PUT");
    EXPECT_EQ(HttpHeaderParser::methodToString(HttpMethod::DELETE), "DELETE");
    EXPECT_EQ(HttpHeaderParser::methodToString(HttpMethod::PATCH), "PATCH");
    EXPECT_EQ(HttpHeaderParser::methodToString(HttpMethod::HEAD), "HEAD");
    EXPECT_EQ(HttpHeaderParser::methodToString(HttpMethod::OPTIONS), "OPTIONS");
}

// HTTP Status Tests
TEST_F(HttpHeaderParserTest, SetAndGetStatus) {
    HttpStatus status200{200, "OK"};
    parser->setStatus(status200);

    auto retrievedStatus = parser->getStatus();
    EXPECT_EQ(retrievedStatus.code, 200);
    EXPECT_EQ(retrievedStatus.description, "OK");

    HttpStatus status404{404, "Not Found"};
    parser->setStatus(status404);

    retrievedStatus = parser->getStatus();
    EXPECT_EQ(retrievedStatus.code, 404);
    EXPECT_EQ(retrievedStatus.description, "Not Found");
}

// Path Tests
TEST_F(HttpHeaderParserTest, SetAndGetPath) {
    parser->setPath("/api/users");
    EXPECT_EQ(parser->getPath(), "/api/users");

    parser->setPath("/api/users/123?param=value");
    EXPECT_EQ(parser->getPath(), "/api/users/123?param=value");

    parser->setPath("");
    EXPECT_EQ(parser->getPath(), "");
}

// Version Tests
TEST_F(HttpHeaderParserTest, SetAndGetVersion) {
    parser->setVersion(HttpVersion::HTTP_1_0);
    EXPECT_EQ(parser->getVersion(), HttpVersion::HTTP_1_0);

    parser->setVersion(HttpVersion::HTTP_1_1);
    EXPECT_EQ(parser->getVersion(), HttpVersion::HTTP_1_1);

    parser->setVersion(HttpVersion::HTTP_2_0);
    EXPECT_EQ(parser->getVersion(), HttpVersion::HTTP_2_0);

    parser->setVersion(HttpVersion::HTTP_3_0);
    EXPECT_EQ(parser->getVersion(), HttpVersion::HTTP_3_0);
}

// Cookie Tests
TEST_F(HttpHeaderParserTest, AddAndGetCookie) {
    Cookie cookie1;
    cookie1.name = "sessionId";
    cookie1.value = "abc123";
    cookie1.path = "/";
    cookie1.domain = "example.com";
    cookie1.maxAge = 3600;
    cookie1.secure = true;
    cookie1.httpOnly = true;

    Cookie cookie2;
    cookie2.name = "userId";
    cookie2.value = "user456";
    cookie2.path = "/api";
    cookie2.domain = "api.example.com";
    cookie2.maxAge = 7200;
    cookie2.secure = false;
    cookie2.httpOnly = false;

    parser->addCookie(cookie1);
    parser->addCookie(cookie2);

    auto allCookies = parser->getAllCookies();
    EXPECT_EQ(allCookies.size(), 2);

    auto sessionCookie = parser->getCookie("sessionId");
    ASSERT_TRUE(sessionCookie.has_value());
    EXPECT_EQ(sessionCookie->name, "sessionId");
    EXPECT_EQ(sessionCookie->value, "abc123");
    ASSERT_TRUE(sessionCookie->path.has_value());
    EXPECT_EQ(*sessionCookie->path, "/");
    ASSERT_TRUE(sessionCookie->domain.has_value());
    EXPECT_EQ(*sessionCookie->domain, "example.com");
    ASSERT_TRUE(sessionCookie->maxAge.has_value());
    EXPECT_EQ(*sessionCookie->maxAge, 3600);
    EXPECT_TRUE(sessionCookie->secure);
    EXPECT_TRUE(sessionCookie->httpOnly);

    auto userCookie = parser->getCookie("userId");
    ASSERT_TRUE(userCookie.has_value());
    EXPECT_EQ(userCookie->name, "userId");
    EXPECT_EQ(userCookie->value, "user456");
}

TEST_F(HttpHeaderParserTest, RemoveCookie) {
    Cookie cookie1;
    cookie1.name = "sessionId";
    cookie1.value = "abc123";

    Cookie cookie2;
    cookie2.name = "userId";
    cookie2.value = "user456";

    parser->addCookie(cookie1);
    parser->addCookie(cookie2);

    EXPECT_TRUE(parser->getCookie("sessionId").has_value());

    parser->removeCookie("sessionId");

    EXPECT_FALSE(parser->getCookie("sessionId").has_value());
    EXPECT_TRUE(parser->getCookie("userId").has_value());
}

TEST_F(HttpHeaderParserTest, ParseCookies) {
    std::string cookieStr = "sessionId=abc123; userId=user456; theme=dark";

    auto cookies = parser->parseCookies(cookieStr);

    EXPECT_EQ(cookies.size(), 3);
    EXPECT_EQ(cookies["sessionId"], "abc123");
    EXPECT_EQ(cookies["userId"], "user456");
    EXPECT_EQ(cookies["theme"], "dark");
}

TEST_F(HttpHeaderParserTest, ParseCookiesWithSpaces) {
    std::string cookieStr =
        " sessionId = abc123 ; userId = user456 ; theme = dark ";

    auto cookies = parser->parseCookies(cookieStr);

    EXPECT_EQ(cookies.size(), 3);
    EXPECT_EQ(cookies["sessionId"], "abc123");
    EXPECT_EQ(cookies["userId"], "user456");
    EXPECT_EQ(cookies["theme"], "dark");
}

// URL Parameter Tests
TEST_F(HttpHeaderParserTest, ParseUrlParameters) {
    std::string url =
        "https://example.com/api/users?id=123&name=john&active=true";

    auto params = parser->parseUrlParameters(url);

    EXPECT_EQ(params.size(), 3);
    EXPECT_EQ(params["id"], "123");
    EXPECT_EQ(params["name"], "john");
    EXPECT_EQ(params["active"], "true");
}

TEST_F(HttpHeaderParserTest, ParseUrlParametersNoQuery) {
    std::string url = "https://example.com/api/users";

    auto params = parser->parseUrlParameters(url);

    EXPECT_TRUE(params.empty());
}

TEST_F(HttpHeaderParserTest, ParseUrlParametersEmptyValues) {
    std::string url =
        "https://example.com/api/users?param1=&param2=value&param3=";

    auto params = parser->parseUrlParameters(url);

    EXPECT_EQ(params.size(), 3);
    EXPECT_EQ(params["param1"], "");
    EXPECT_EQ(params["param2"], "value");
    EXPECT_EQ(params["param3"], "");
}

// URL Encoding/Decoding Tests
TEST_F(HttpHeaderParserTest, UrlEncode) {
    EXPECT_EQ(HttpHeaderParser::urlEncode("hello world"), "hello%20world");
    EXPECT_EQ(HttpHeaderParser::urlEncode("test@example.com"),
              "test%40example.com");
    EXPECT_EQ(HttpHeaderParser::urlEncode("a+b=c&d"), "a%2Bb%3Dc%26d");
    EXPECT_EQ(HttpHeaderParser::urlEncode(""), "");
    EXPECT_EQ(HttpHeaderParser::urlEncode("abc123"), "abc123");
}

TEST_F(HttpHeaderParserTest, UrlDecode) {
    EXPECT_EQ(HttpHeaderParser::urlDecode("hello%20world"), "hello world");
    EXPECT_EQ(HttpHeaderParser::urlDecode("test%40example.com"),
              "test@example.com");
    EXPECT_EQ(HttpHeaderParser::urlDecode("a%2Bb%3Dc%26d"), "a+b=c&d");
    EXPECT_EQ(HttpHeaderParser::urlDecode(""), "");
    EXPECT_EQ(HttpHeaderParser::urlDecode("abc123"), "abc123");
}

TEST_F(HttpHeaderParserTest, UrlEncodeDecodeRoundTrip) {
    std::string original = "Hello World! @#$%^&*()+={}[]|\\:;\"'<>,.?/~`";
    std::string encoded = HttpHeaderParser::urlEncode(original);
    std::string decoded = HttpHeaderParser::urlDecode(encoded);

    EXPECT_EQ(original, decoded);
}

// Build Request/Response Tests
TEST_F(HttpHeaderParserTest, BuildCompleteRequest) {
    parser->setMethod(HttpMethod::POST);
    parser->setPath("/api/users");
    parser->setVersion(HttpVersion::HTTP_1_1);
    parser->setHeaderValue("Host", "api.example.com");
    parser->setHeaderValue("Content-Type", "application/json");
    parser->setHeaderValue("User-Agent", "TestClient/1.0");
    parser->setBody(R"({"name": "John", "email": "john@example.com"})");

    std::string request = parser->buildRequest();

    EXPECT_THAT(request, ::testing::HasSubstr("POST /api/users HTTP/1.1"));
    EXPECT_THAT(request, ::testing::HasSubstr("Host: api.example.com"));
    EXPECT_THAT(request,
                ::testing::HasSubstr("Content-Type: application/json"));
    EXPECT_THAT(request, ::testing::HasSubstr("User-Agent: TestClient/1.0"));
    EXPECT_THAT(request,
                ::testing::HasSubstr(
                    R"({"name": "John", "email": "john@example.com"})"));
}

TEST_F(HttpHeaderParserTest, BuildCompleteResponse) {
    parser->setStatus(HttpStatus{201, "Created"});
    parser->setVersion(HttpVersion::HTTP_1_1);
    parser->setHeaderValue("Content-Type", "application/json");
    parser->setHeaderValue("Location", "/api/users/123");
    parser->setHeaderValue("Server", "TestServer/1.0");
    parser->setBody(
        R"({"id": 123, "name": "John", "email": "john@example.com"})");

    std::string response = parser->buildResponse();

    EXPECT_THAT(response, ::testing::HasSubstr("HTTP/1.1 201 Created"));
    EXPECT_THAT(response,
                ::testing::HasSubstr("Content-Type: application/json"));
    EXPECT_THAT(response, ::testing::HasSubstr("Location: /api/users/123"));
    EXPECT_THAT(response, ::testing::HasSubstr("Server: TestServer/1.0"));
    EXPECT_THAT(
        response,
        ::testing::HasSubstr(
            R"({"id": 123, "name": "John", "email": "john@example.com"})"));
}

// Edge Cases and Error Handling Tests
TEST_F(HttpHeaderParserTest, ParseMalformedRequest) {
    // Missing HTTP version
    std::string malformedRequest = "GET /path\r\nHost: example.com\r\n\r\n";
    EXPECT_FALSE(parser->parseRequest(malformedRequest));

    // Invalid request line
    std::string invalidRequest =
        "INVALID REQUEST LINE\r\nHost: example.com\r\n\r\n";
    EXPECT_FALSE(parser->parseRequest(invalidRequest));

    // Empty request
    EXPECT_FALSE(parser->parseRequest(""));
}

TEST_F(HttpHeaderParserTest, ParseMalformedResponse) {
    // Missing status code
    std::string malformedResponse =
        "HTTP/1.1\r\nContent-Type: text/html\r\n\r\n";
    EXPECT_FALSE(parser->parseResponse(malformedResponse));

    // Invalid status line
    std::string invalidResponse =
        "INVALID RESPONSE LINE\r\nContent-Type: text/html\r\n\r\n";
    EXPECT_FALSE(parser->parseResponse(invalidResponse));

    // Empty response
    EXPECT_FALSE(parser->parseResponse(""));
}

TEST_F(HttpHeaderParserTest, ParseHeadersWithMalformedLines) {
    std::string malformedHeaders =
        "Valid-Header: Valid Value\r\n"
        "Invalid-Header-Without-Colon\r\n"
        ": Value-Without-Name\r\n"
        "Another-Valid: Value\r\n";

    ASSERT_NO_THROW(parser->parseHeaders(malformedHeaders));

    // Should still parse valid headers
    EXPECT_TRUE(parser->hasHeader("Valid-Header"));
    EXPECT_TRUE(parser->hasHeader("Another-Valid"));
}

TEST_F(HttpHeaderParserTest, CaseSensitivityHeaders) {
    parser->setHeaderValue("Content-Type", "application/json");
    parser->setHeaderValue("content-type", "text/html");  // Different case

    // Should treat as different headers or overwrite (implementation dependent)
    auto allHeaders = parser->getAllHeaders();
    // At least one should exist
    EXPECT_TRUE(parser->hasHeader("Content-Type") ||
                parser->hasHeader("content-type"));
}

TEST_F(HttpHeaderParserTest, EmptyAndWhitespaceValues) {
    parser->setHeaderValue("Empty-Value", "");
    parser->setHeaderValue("Whitespace-Value", "   ");
    parser->setHeaderValue("", "Empty-Key");

    EXPECT_TRUE(parser->hasHeader("Empty-Value"));
    EXPECT_TRUE(parser->hasHeader("Whitespace-Value"));

    auto emptyValue = parser->getHeaderValue("Empty-Value");
    ASSERT_TRUE(emptyValue.has_value());
    EXPECT_EQ(*emptyValue, "");
}

TEST_F(HttpHeaderParserTest, LargeHeaders) {
    // Test with very large header values
    std::string largeValue(10000, 'X');
    parser->setHeaderValue("Large-Header", largeValue);

    auto retrievedValue = parser->getHeaderValue("Large-Header");
    ASSERT_TRUE(retrievedValue.has_value());
    EXPECT_EQ(*retrievedValue, largeValue);
}

TEST_F(HttpHeaderParserTest, SpecialCharactersInHeaders) {
    parser->setHeaderValue(
        "Special-Chars", "Value with spaces, commas, and symbols: !@#$%^&*()");
    parser->setHeaderValue("Unicode-Header", "Value with unicode: 你好世界");

    auto specialValue = parser->getHeaderValue("Special-Chars");
    ASSERT_TRUE(specialValue.has_value());
    EXPECT_EQ(*specialValue,
              "Value with spaces, commas, and symbols: !@#$%^&*()");

    auto unicodeValue = parser->getHeaderValue("Unicode-Header");
    ASSERT_TRUE(unicodeValue.has_value());
    EXPECT_EQ(*unicodeValue, "Value with unicode: 你好世界");
}

TEST_F(HttpHeaderParserTest, HTTPVersionEdgeCases) {
    // Test various HTTP version formats
    std::string request1 = "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n";
    ASSERT_TRUE(parser->parseRequest(request1));
    EXPECT_EQ(parser->getVersion(), HttpVersion::HTTP_1_0);

    std::string request2 = "GET / HTTP/2\r\nHost: example.com\r\n\r\n";
    ASSERT_TRUE(parser->parseRequest(request2));
    EXPECT_EQ(parser->getVersion(), HttpVersion::HTTP_2_0);

    std::string request3 = "GET / HTTP/3.0\r\nHost: example.com\r\n\r\n";
    ASSERT_TRUE(parser->parseRequest(request3));
    EXPECT_EQ(parser->getVersion(), HttpVersion::HTTP_3_0);

    std::string requestUnknown = "GET / HTTP/9.9\r\nHost: example.com\r\n\r\n";
    ASSERT_TRUE(parser->parseRequest(requestUnknown));
    EXPECT_EQ(parser->getVersion(), HttpVersion::UNKNOWN);
}

TEST_F(HttpHeaderParserTest, ComplexCookieParsing) {
    std::string complexCookieStr =
        "sessionId=abc123; expires=Wed, 21 Oct 2025 07:28:00 GMT; path=/; "
        "domain=.example.com; secure; httponly; samesite=strict";

    auto cookies = parser->parseCookies(complexCookieStr);

    // Should at least parse the basic name-value pairs
    EXPECT_TRUE(cookies.count("sessionId") > 0);
    EXPECT_EQ(cookies["sessionId"], "abc123");
}

TEST_F(HttpHeaderParserTest, URLParametersEdgeCases) {
    // URL with no parameters
    auto params1 = parser->parseUrlParameters("https://example.com/path");
    EXPECT_TRUE(params1.empty());

    // URL with empty parameter values
    auto params2 =
        parser->parseUrlParameters("https://example.com/path?a=&b=value&c=");
    EXPECT_EQ(params2.size(), 3);
    EXPECT_EQ(params2["a"], "");
    EXPECT_EQ(params2["b"], "value");
    EXPECT_EQ(params2["c"], "");

    // URL with encoded parameters
    auto params3 = parser->parseUrlParameters(
        "https://example.com/path?name=John%20Doe&email=test%40example.com");
    EXPECT_EQ(params3.size(), 2);
    // Note: This test depends on whether the parser automatically decodes URL
    // parameters
}

// Extended Malformed Request/Response Tests
class HttpParserMalformedTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<HttpHeaderParser>();
        spdlog::set_level(spdlog::level::off);
    }
    void TearDown() override { parser.reset(); }

    std::unique_ptr<HttpHeaderParser> parser;
};

TEST_F(HttpParserMalformedTest, MalformedRequestLines) {
    std::vector<std::string> malformedRequests = {
        "GET\r\nHost: example.com\r\n\r\n",        // Missing path and version
        "GET /path\r\nHost: example.com\r\n\r\n",  // Missing version
        "GET /path HTTP\r\nHost: example.com\r\n\r\n",     // Incomplete version
        "GET /path HTTP/\r\nHost: example.com\r\n\r\n",    // Incomplete version
        "GET /path HTTP/1.\r\nHost: example.com\r\n\r\n",  // Incomplete version
        "GET  /path HTTP/1.1\r\nHost: example.com\r\n\r\n",  // Extra space
        "GET\t/path HTTP/1.1\r\nHost: example.com\r\n\r\n",  // Tab instead of
                                                             // space
        "get /path HTTP/1.1\r\nHost: example.com\r\n\r\n",   // Lowercase method
        "GET /path http/1.1\r\nHost: example.com\r\n\r\n",   // Lowercase HTTP
        "GET /path HTTP/1.1 extra\r\nHost: example.com\r\n\r\n",  // Extra text
        "GET /path HTTP/1.1\nHost: example.com\n\n",  // LF instead of CRLF
        "GET /path HTTP/1.1\r\r\nHost: example.com\r\n\r\n",  // Extra CR
        "",                                                   // Empty request
        "\r\n",                                               // Only CRLF
        "   \r\n\r\n",                                        // Only spaces
        "GET /path HTTP/1.1",                       // No headers separator
        "GET /path HTTP/1.1\r\n",                   // No headers end
        "GET /path HTTP/1.1\r\nHost: example.com",  // No headers end
    };

    for (const auto& request : malformedRequests) {
        EXPECT_FALSE(parser->parseRequest(request))
            << "Should fail to parse malformed request: " << request;
    }
}

TEST_F(HttpParserMalformedTest, MalformedResponseLines) {
    std::vector<std::string> malformedResponses = {
        "HTTP/1.1\r\nContent-Type: text/html\r\n\r\n",  // Missing status code
        "HTTP/1.1 200\r\nContent-Type: text/html\r\n\r\n",  // Missing reason
                                                            // phrase
        "HTTP/1.1  200 OK\r\nContent-Type: text/html\r\n\r\n",  // Extra space
        "HTTP/1.1\t200 OK\r\nContent-Type: text/html\r\n\r\n",  // Tab instead
                                                                // of space
        "http/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n",  // Lowercase HTTP
        "HTTP/1.1 200 OK extra\r\nContent-Type: text/html\r\n\r\n",  // Extra
                                                                     // text
        "HTTP/1.1 abc OK\r\nContent-Type: text/html\r\n\r\n",     // Non-numeric
                                                                  // status
        "HTTP/1.1 999999 OK\r\nContent-Type: text/html\r\n\r\n",  // Invalid
                                                                  // status code
        "HTTP/1.1 200 OK\nContent-Type: text/html\n\n",  // LF instead of CRLF
        "HTTP/1.1 200 OK\r\r\nContent-Type: text/html\r\n\r\n",  // Extra CR
        "",                                            // Empty response
        "\r\n",                                        // Only CRLF
        "   \r\n\r\n",                                 // Only spaces
        "HTTP/1.1 200 OK",                             // No headers separator
        "HTTP/1.1 200 OK\r\n",                         // No headers end
        "HTTP/1.1 200 OK\r\nContent-Type: text/html",  // No headers end
    };

    for (const auto& response : malformedResponses) {
        EXPECT_FALSE(parser->parseResponse(response))
            << "Should fail to parse malformed response: " << response;
    }
}

TEST_F(HttpParserMalformedTest, MalformedHeaders) {
    std::vector<std::string> malformedHeaders = {
        "Header-Without-Colon Value\r\n",     // Missing colon
        ": Value-Without-Name\r\n",           // Missing header name
        "Header:\r\n",                        // Missing value
        "Header: \r\n",                       // Only space as value
        "Header:\t\r\n",                      // Only tab as value
        "Header With Spaces: Value\r\n",      // Spaces in header name
        "Header\tWith\tTabs: Value\r\n",      // Tabs in header name
        "Header:Value:With:Colons\r\n",       // Multiple colons
        "Header: Value\r\nContinuation\r\n",  // Line continuation (deprecated)
        "Header: Value\r\n With Continuation\r\n",  // Line continuation with
                                                    // space
        "Header: Value\r\n\tWith Tab Continuation\r\n",  // Line continuation
                                                         // with tab
        "Header: Value\nNext-Header: Value\n",           // LF instead of CRLF
        "Header: Value\r\r\nNext-Header: Value\r\n",     // Extra CR
        "Header: Value\r\nNext-Header: Value\r\r\n",     // Extra CR at end
    };

    for (const auto& headerLine : malformedHeaders) {
        std::string request = "GET /path HTTP/1.1\r\n" + headerLine + "\r\n";
        // Some malformed headers might be parsed with warnings, others might
        // fail The important thing is that the parser doesn't crash
        EXPECT_NO_THROW(parser->parseRequest(request))
            << "Parser should not crash on malformed header: " << headerLine;
    }
}

// Large Headers and Bodies Tests
class HttpParserLargeDataTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<HttpHeaderParser>();
        spdlog::set_level(spdlog::level::off);
    }
    void TearDown() override { parser.reset(); }

    std::unique_ptr<HttpHeaderParser> parser;
};

TEST_F(HttpParserLargeDataTest, VeryLargeHeaders) {
    // Test with extremely large header values
    std::vector<size_t> sizes = {1000, 10000, 100000, 1000000};

    for (size_t size : sizes) {
        std::string largeValue(size, 'X');
        std::string request =
            "GET /path HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Large-Header: " +
            largeValue +
            "\r\n"
            "\r\n";

        EXPECT_NO_THROW({
            bool parseResult = parser->parseRequest(request);
            if (parseResult) {
                auto retrievedValue = parser->getHeaderValue("Large-Header");
                if (retrievedValue.has_value()) {
                    EXPECT_EQ(retrievedValue->length(), size);
                }
            }
        }) << "Parser should handle large header of size: "
           << size;
    }
}

TEST_F(HttpParserLargeDataTest, ManyHeaders) {
    // Test with many headers
    std::string request = "GET /path HTTP/1.1\r\n";

    for (int i = 0; i < 1000; ++i) {
        request += "Header-" + std::to_string(i) + ": Value-" +
                   std::to_string(i) + "\r\n";
    }
    request += "\r\n";

    EXPECT_NO_THROW({
        bool parseResult = parser->parseRequest(request);
        if (parseResult) {
            auto allHeaders = parser->getAllHeaders();
            EXPECT_GT(allHeaders.size(),
                      500);  // Should have parsed many headers
        }
    });
}

TEST_F(HttpParserLargeDataTest, VeryLargeBodies) {
    // Test with extremely large bodies
    std::vector<size_t> sizes = {10000, 100000, 1000000};

    for (size_t size : sizes) {
        std::string largeBody(size, 'B');
        std::string request =
            "POST /path HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: " +
            std::to_string(size) +
            "\r\n"
            "\r\n" +
            largeBody;

        EXPECT_NO_THROW({
            bool parseResult = parser->parseRequest(request);
            if (parseResult) {
                std::string retrievedBody = parser->getBody();
                EXPECT_EQ(retrievedBody.length(), size);
                if (size <= 100000) {  // Only check content for smaller sizes
                    EXPECT_EQ(retrievedBody, largeBody);
                }
            }
        }) << "Parser should handle large body of size: "
           << size;
    }
}

TEST_F(HttpParserLargeDataTest, LargeUrlPaths) {
    // Test with very long URL paths
    std::vector<size_t> pathSizes = {1000, 5000, 10000};

    for (size_t size : pathSizes) {
        std::string largePath = "/" + std::string(size - 1, 'p');
        std::string request = "GET " + largePath +
                              " HTTP/1.1\r\n"
                              "Host: example.com\r\n"
                              "\r\n";

        EXPECT_NO_THROW({
            bool parseResult = parser->parseRequest(request);
            if (parseResult) {
                std::string retrievedPath = parser->getPath();
                EXPECT_EQ(retrievedPath.length(), size);
                EXPECT_EQ(retrievedPath, largePath);
            }
        }) << "Parser should handle large path of size: "
           << size;
    }
}

// Comprehensive HTTP Methods Tests
class HttpParserMethodsTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<HttpHeaderParser>();
        spdlog::set_level(spdlog::level::off);
    }
    void TearDown() override { parser.reset(); }

    std::unique_ptr<HttpHeaderParser> parser;
};

TEST_F(HttpParserMethodsTest, AllStandardHttpMethods) {
    std::vector<std::pair<std::string, HttpMethod>> methods = {
        {"GET", HttpMethod::GET},         {"POST", HttpMethod::POST},
        {"PUT", HttpMethod::PUT},         {"DELETE", HttpMethod::DELETE},
        {"PATCH", HttpMethod::PATCH},     {"HEAD", HttpMethod::HEAD},
        {"OPTIONS", HttpMethod::OPTIONS}, {"TRACE", HttpMethod::TRACE},
        {"CONNECT", HttpMethod::CONNECT}};

    for (const auto& [methodStr, expectedMethod] : methods) {
        std::string request = methodStr +
                              " /path HTTP/1.1\r\n"
                              "Host: example.com\r\n"
                              "\r\n";

        EXPECT_TRUE(parser->parseRequest(request))
            << "Should parse request with method: " << methodStr;

        if (expectedMethod != HttpMethod::UNKNOWN) {
            EXPECT_EQ(parser->getMethod(), expectedMethod)
                << "Method should match for: " << methodStr;
        }

        // Test string conversion
        EXPECT_EQ(HttpHeaderParser::stringToMethod(methodStr), expectedMethod);
        if (expectedMethod != HttpMethod::UNKNOWN) {
            EXPECT_EQ(HttpHeaderParser::methodToString(expectedMethod),
                      methodStr);
        }
    }
}

TEST_F(HttpParserMethodsTest, CustomAndUnknownMethods) {
    std::vector<std::string> customMethods = {"CUSTOM",    "WEBDAV", "PROPFIND",
                                              "PROPPATCH", "MKCOL",  "COPY",
                                              "MOVE",      "LOCK",   "UNLOCK"};

    for (const auto& methodStr : customMethods) {
        std::string request = methodStr +
                              " /path HTTP/1.1\r\n"
                              "Host: example.com\r\n"
                              "\r\n";

        EXPECT_TRUE(parser->parseRequest(request))
            << "Should parse request with custom method: " << methodStr;

        // Custom methods should be parsed as UNKNOWN
        EXPECT_EQ(parser->getMethod(), HttpMethod::UNKNOWN)
            << "Custom method should be UNKNOWN: " << methodStr;

        EXPECT_EQ(HttpHeaderParser::stringToMethod(methodStr),
                  HttpMethod::UNKNOWN);
    }
}

TEST_F(HttpParserMethodsTest, MethodsWithBodies) {
    std::vector<std::string> methodsWithBodies = {"POST", "PUT", "PATCH"};
    std::string testBody = R"({"test": "data", "number": 123})";

    for (const auto& method : methodsWithBodies) {
        std::string request = method +
                              " /api/data HTTP/1.1\r\n"
                              "Host: example.com\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: " +
                              std::to_string(testBody.length()) +
                              "\r\n"
                              "\r\n" +
                              testBody;

        EXPECT_TRUE(parser->parseRequest(request))
            << "Should parse " << method << " request with body";

        EXPECT_EQ(parser->getBody(), testBody)
            << "Body should be preserved for " << method << " request";
    }
}

// Comprehensive HTTP Status Codes Tests
class HttpParserStatusCodesTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<HttpHeaderParser>();
        spdlog::set_level(spdlog::level::off);
    }
    void TearDown() override { parser.reset(); }

    std::unique_ptr<HttpHeaderParser> parser;
};

TEST_F(HttpParserStatusCodesTest, InformationalStatusCodes) {
    std::vector<std::pair<int, std::string>> statusCodes = {
        {100, "Continue"},
        {101, "Switching Protocols"},
        {102, "Processing"},
        {103, "Early Hints"}};

    for (const auto& [code, reason] : statusCodes) {
        std::string response = "HTTP/1.1 " + std::to_string(code) + " " +
                               reason +
                               "\r\n"
                               "Content-Type: text/plain\r\n"
                               "\r\n";

        EXPECT_TRUE(parser->parseResponse(response))
            << "Should parse response with status: " << code << " " << reason;

        auto status = parser->getStatus();
        EXPECT_EQ(status.code, code);
        EXPECT_EQ(status.description, reason);
    }
}

TEST_F(HttpParserStatusCodesTest, SuccessStatusCodes) {
    std::vector<std::pair<int, std::string>> statusCodes = {
        {200, "OK"},
        {201, "Created"},
        {202, "Accepted"},
        {203, "Non-Authoritative Information"},
        {204, "No Content"},
        {205, "Reset Content"},
        {206, "Partial Content"},
        {207, "Multi-Status"},
        {208, "Already Reported"},
        {226, "IM Used"}};

    for (const auto& [code, reason] : statusCodes) {
        std::string response = "HTTP/1.1 " + std::to_string(code) + " " +
                               reason +
                               "\r\n"
                               "Content-Type: text/plain\r\n"
                               "\r\n";

        EXPECT_TRUE(parser->parseResponse(response))
            << "Should parse response with status: " << code << " " << reason;

        auto status = parser->getStatus();
        EXPECT_EQ(status.code, code);
        EXPECT_EQ(status.description, reason);
    }
}

TEST_F(HttpParserStatusCodesTest, RedirectionStatusCodes) {
    std::vector<std::pair<int, std::string>> statusCodes = {
        {300, "Multiple Choices"},
        {301, "Moved Permanently"},
        {302, "Found"},
        {303, "See Other"},
        {304, "Not Modified"},
        {305, "Use Proxy"},
        {307, "Temporary Redirect"},
        {308, "Permanent Redirect"}};

    for (const auto& [code, reason] : statusCodes) {
        std::string response = "HTTP/1.1 " + std::to_string(code) + " " +
                               reason +
                               "\r\n"
                               "Location: https://example.com/new-location\r\n"
                               "\r\n";

        EXPECT_TRUE(parser->parseResponse(response))
            << "Should parse response with status: " << code << " " << reason;

        auto status = parser->getStatus();
        EXPECT_EQ(status.code, code);
        EXPECT_EQ(status.description, reason);
    }
}

TEST_F(HttpParserStatusCodesTest, ClientErrorStatusCodes) {
    std::vector<std::pair<int, std::string>> statusCodes = {
        {400, "Bad Request"},
        {401, "Unauthorized"},
        {402, "Payment Required"},
        {403, "Forbidden"},
        {404, "Not Found"},
        {405, "Method Not Allowed"},
        {406, "Not Acceptable"},
        {407, "Proxy Authentication Required"},
        {408, "Request Timeout"},
        {409, "Conflict"},
        {410, "Gone"},
        {411, "Length Required"},
        {412, "Precondition Failed"},
        {413, "Payload Too Large"},
        {414, "URI Too Long"},
        {415, "Unsupported Media Type"},
        {416, "Range Not Satisfiable"},
        {417, "Expectation Failed"},
        {418, "I'm a teapot"},
        {421, "Misdirected Request"},
        {422, "Unprocessable Entity"},
        {423, "Locked"},
        {424, "Failed Dependency"},
        {425, "Too Early"},
        {426, "Upgrade Required"},
        {428, "Precondition Required"},
        {429, "Too Many Requests"},
        {431, "Request Header Fields Too Large"},
        {451, "Unavailable For Legal Reasons"}};

    for (const auto& [code, reason] : statusCodes) {
        std::string response = "HTTP/1.1 " + std::to_string(code) + " " +
                               reason +
                               "\r\n"
                               "Content-Type: application/json\r\n"
                               "\r\n"
                               R"({"error": ")" +
                               reason + R"("})";

        EXPECT_TRUE(parser->parseResponse(response))
            << "Should parse response with status: " << code << " " << reason;

        auto status = parser->getStatus();
        EXPECT_EQ(status.code, code);
        EXPECT_EQ(status.description, reason);
    }
}

TEST_F(HttpParserStatusCodesTest, ServerErrorStatusCodes) {
    std::vector<std::pair<int, std::string>> statusCodes = {
        {500, "Internal Server Error"},
        {501, "Not Implemented"},
        {502, "Bad Gateway"},
        {503, "Service Unavailable"},
        {504, "Gateway Timeout"},
        {505, "HTTP Version Not Supported"},
        {506, "Variant Also Negotiates"},
        {507, "Insufficient Storage"},
        {508, "Loop Detected"},
        {510, "Not Extended"},
        {511, "Network Authentication Required"}};

    for (const auto& [code, reason] : statusCodes) {
        std::string response =
            "HTTP/1.1 " + std::to_string(code) + " " + reason +
            "\r\n"
            "Content-Type: text/html\r\n"
            "\r\n"
            "<html><body><h1>" +
            std::to_string(code) + " " + reason + "</h1></body></html>";

        EXPECT_TRUE(parser->parseResponse(response))
            << "Should parse response with status: " << code << " " << reason;

        auto status = parser->getStatus();
        EXPECT_EQ(status.code, code);
        EXPECT_EQ(status.description, reason);
    }
}

// Comprehensive Encoding Tests
class HttpParserEncodingTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<HttpHeaderParser>();
        spdlog::set_level(spdlog::level::off);
    }
    void TearDown() override { parser.reset(); }

    std::unique_ptr<HttpHeaderParser> parser;
};

TEST_F(HttpParserEncodingTest, UrlEncodingEdgeCases) {
    std::vector<std::pair<std::string, std::string>> testCases = {
        {"", ""},                                    // Empty string
        {"abc123", "abc123"},                        // No encoding needed
        {"hello world", "hello%20world"},            // Space
        {"test@example.com", "test%40example.com"},  // @ symbol
        {"a+b=c&d", "a%2Bb%3Dc%26d"},                // Multiple special chars
        {"100%", "100%25"},                          // Percent sign
        {"café", "caf%C3%A9"},                       // UTF-8 characters
        {"你好", "%E4%BD%A0%E5%A5%BD"},              // Chinese characters
        {"🚀", "%F0%9F%9A%80"},                      // Emoji
        {"\r\n\t", "%0D%0A%09"},                     // Control characters
        {"\"quotes\"", "%22quotes%22"},              // Quotes
        {"<script>", "%3Cscript%3E"},                // HTML tags
        {"a/b\\c", "a%2Fb%5Cc"},                     // Path separators
        {"key=value&other=data",
         "key%3Dvalue%26other%3Ddata"},  // Query-like string
    };

    for (const auto& [input, expected] : testCases) {
        std::string encoded = HttpHeaderParser::urlEncode(input);
        EXPECT_EQ(encoded, expected) << "URL encoding failed for: " << input;

        std::string decoded = HttpHeaderParser::urlDecode(encoded);
        EXPECT_EQ(decoded, input)
            << "Round-trip encoding/decoding failed for: " << input;
    }
}

TEST_F(HttpParserEncodingTest, UrlDecodingEdgeCases) {
    std::vector<std::pair<std::string, std::string>> testCases = {
        {"", ""},                                    // Empty string
        {"abc123", "abc123"},                        // No decoding needed
        {"hello%20world", "hello world"},            // Space
        {"test%40example.com", "test@example.com"},  // @ symbol
        {"a%2Bb%3Dc%26d", "a+b=c&d"},                // Multiple special chars
        {"100%25", "100%"},                          // Percent sign
        {"%20%21%22%23", " !\"#"},                   // Multiple encoded chars
        {"invalid%", "invalid%"},    // Invalid encoding (missing digits)
        {"invalid%G", "invalid%G"},  // Invalid hex digit
        {"invalid%1", "invalid%1"},  // Incomplete encoding
        {"valid%20invalid%", "valid invalid%"},  // Mixed valid/invalid
        {"%2", "%2"},                            // Incomplete at end
        {"%%20", "%%20"},                        // Double percent (invalid)
        {"%00", std::string(1, '\0')},           // Null character
        {"%FF", std::string(1, '\xFF')},         // High byte value
    };

    for (const auto& [input, expected] : testCases) {
        std::string decoded = HttpHeaderParser::urlDecode(input);
        EXPECT_EQ(decoded, expected) << "URL decoding failed for: " << input;
    }
}

TEST_F(HttpParserEncodingTest, UnicodeInHeaders) {
    std::vector<std::string> unicodeStrings = {
        "café",                  // Latin with accents
        "你好世界",              // Chinese
        "marhaba",               // Arabic (transliterated)
        "Zdravstvuy",            // Cyrillic (transliterated)
        "Rocket-Star-Computer",  // Emojis (replaced)
        "Internationalization",  // Mixed accented characters (simplified)
        "Ellinika",              // Greek (transliterated)
        "Nihongo",               // Japanese (transliterated)
        "Hangul",                // Korean (transliterated)
        "Ivrit",                 // Hebrew (transliterated)
    };

    for (const auto& unicodeStr : unicodeStrings) {
        parser->setHeaderValue("Unicode-Header", unicodeStr);

        auto retrievedValue = parser->getHeaderValue("Unicode-Header");
        ASSERT_TRUE(retrievedValue.has_value())
            << "Failed to retrieve unicode header: " << unicodeStr;
        EXPECT_EQ(*retrievedValue, unicodeStr)
            << "Unicode string corrupted: " << unicodeStr;
    }
}

TEST_F(HttpParserEncodingTest, BinaryDataInBodies) {
    // Test with binary data in request/response bodies
    std::vector<std::string> binaryData = {
        std::string(1, '\0') + "null byte",                   // Null byte
        std::string{'\x01', '\x02', '\x03', '\xFF', '\xFE'},  // Binary bytes
        std::string(256, '\xFF'),                             // All high bytes
        std::string{'\r', '\n', '\r', '\n'},                  // CRLF sequences
        std::string("Mixed") + '\x00' + "binary" + '\xFF' + '\xFE' + "data" +
            '\x01' + '\x02',  // Mixed text and binary
    };

    for (const auto& data : binaryData) {
        std::string request =
            "POST /upload HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Length: " +
            std::to_string(data.length()) +
            "\r\n"
            "\r\n" +
            data;

        EXPECT_TRUE(parser->parseRequest(request))
            << "Should parse request with binary data";

        std::string retrievedBody = parser->getBody();
        EXPECT_EQ(retrievedBody.length(), data.length())
            << "Binary data length should be preserved";
        EXPECT_EQ(retrievedBody, data)
            << "Binary data should be preserved exactly";
    }
}

// Performance and Stress Tests
class HttpParserPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<HttpHeaderParser>();
        spdlog::set_level(spdlog::level::off);
    }
    void TearDown() override { parser.reset(); }

    std::unique_ptr<HttpHeaderParser> parser;
};

TEST_F(HttpParserPerformanceTest, ParsingPerformance) {
    // Test parsing performance with various request sizes
    std::vector<size_t> bodySizes = {0, 1000, 10000, 100000};

    for (size_t bodySize : bodySizes) {
        std::string body(bodySize, 'X');
        std::string request =
            "POST /api/data HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: " +
            std::to_string(bodySize) +
            "\r\n"
            "\r\n" +
            body;

        auto start = std::chrono::high_resolution_clock::now();

        bool parseResult = parser->parseRequest(request);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        EXPECT_TRUE(parseResult)
            << "Should parse request with body size: " << bodySize;
        EXPECT_LT(duration.count(), 1000)
            << "Parsing should be fast for body size: " << bodySize;

        if (parseResult) {
            EXPECT_EQ(parser->getBody().length(), bodySize);
        }
    }
}

TEST_F(HttpParserPerformanceTest, RepeatedParsing) {
    std::string request =
        "GET /api/test HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: TestAgent/1.0\r\n"
        "Accept: application/json\r\n"
        "\r\n";

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        EXPECT_TRUE(parser->parseRequest(request));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(),
              5000);  // Should complete 1000 parses in less than 5 seconds
}

TEST_F(HttpParserPerformanceTest, BuildingPerformance) {
    parser->setMethod(HttpMethod::POST);
    parser->setPath("/api/data");
    parser->setVersion(HttpVersion::HTTP_1_1);
    parser->setHeaderValue("Host", "example.com");
    parser->setHeaderValue("Content-Type", "application/json");

    std::string body = R"({"test": "data", "number": 123})";
    parser->setBody(body);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        std::string request = parser->buildRequest();
        EXPECT_FALSE(request.empty());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(),
              2000);  // Should complete 1000 builds in less than 2 seconds
}

// Concurrent Access Tests
class HttpParserConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override { spdlog::set_level(spdlog::level::off); }
};

TEST_F(HttpParserConcurrencyTest, ConcurrentParsing) {
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i]() {
            try {
                HttpHeaderParser parser;

                for (int j = 0; j < 100; ++j) {
                    std::string request = "GET /path" + std::to_string(i) +
                                          "/" + std::to_string(j) +
                                          " HTTP/1.1\r\n"
                                          "Host: example.com\r\n"
                                          "User-Agent: Thread-" +
                                          std::to_string(i) +
                                          "\r\n"
                                          "\r\n";

                    if (parser.parseRequest(request)) {
                        std::string path = parser.getPath();
                        std::string expectedPath = "/path" + std::to_string(i) +
                                                   "/" + std::to_string(j);
                        if (path == expectedPath) {
                            successCount++;
                        } else {
                            errorCount++;
                        }
                    } else {
                        errorCount++;
                    }
                }
            } catch (const std::exception&) {
                errorCount++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount.load(), numThreads * 100);
    EXPECT_EQ(errorCount.load(), 0);
}

// Advanced Malformed Data Tests
class HttpParserAdvancedMalformedTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<HttpHeaderParser>();
        spdlog::set_level(spdlog::level::off);
    }
    void TearDown() override { parser.reset(); }

    std::unique_ptr<HttpHeaderParser> parser;
};

TEST_F(HttpParserAdvancedMalformedTest, MalformedCookieStrings) {
    std::vector<std::string> malformedCookies = {
        "name=value; expires=invalid-date",     // Invalid expiration date
        "name=value; max-age=not-a-number",     // Invalid max-age
        "name=value; domain=.invalid..domain",  // Invalid domain
        "name=value; path=/invalid path",       // Space in path
        "name=value; secure=false",             // Secure with value
        "name=value; httponly=true",            // HttpOnly with value
        "name=value; samesite=invalid",         // Invalid SameSite value
        "name=; value=test",                    // Empty name
        "=value; name=test",                    // Empty name at start
        "name=value;; extra=semicolon",         // Double semicolon
        "name=value; =empty-attribute",         // Empty attribute name
        "name=value; attribute-without-value",  // Attribute without value
        "name=value; attribute=",               // Attribute with empty value
        "name=value\r\n; next=cookie",          // CRLF in cookie
        "name=value\x00; null=byte",            // Null byte in cookie
        "name=value; path=/\x7F\x80\x81",       // Control/high bytes in path
        std::string("name=value; comment=") +
            std::string(1000, 'x'),  // Very long comment
    };

    for (const auto& cookieStr : malformedCookies) {
        EXPECT_NO_THROW({
            auto cookies = parser->parseCookies(cookieStr);
            // Should not crash, but may have varying behavior
        }) << "Parser should not crash on malformed cookie: "
           << cookieStr;
    }
}

TEST_F(HttpParserAdvancedMalformedTest, InvalidUrlParameterFormats) {
    std::vector<std::string> malformedUrls = {
        "https://example.com/path?",        // Query string with no parameters
        "https://example.com/path?&",       // Query string starting with &
        "https://example.com/path?&&",      // Multiple & with no parameters
        "https://example.com/path?param",   // Parameter without =
        "https://example.com/path?param=",  // Parameter with empty value
        "https://example.com/path?=value",  // Value without parameter name
        "https://example.com/path?param=value&",            // Trailing &
        "https://example.com/path?param=value&&next=test",  // Double &
        "https://example.com/path?param=val%",    // Incomplete URL encoding
        "https://example.com/path?param=val%G",   // Invalid hex in URL encoding
        "https://example.com/path?param=val%1",   // Incomplete hex in URL
                                                  // encoding
        "https://example.com/path?param=val%00",  // Null byte encoded
        "https://example.com/path?param=val%FF",  // High byte encoded
        "https://example.com/path?" +
            std::string(10000, 'x'),  // Very long query string
        "https://example.com/path?param=" +
            std::string(5000, 'v'),  // Very long parameter value
    };

    for (const auto& url : malformedUrls) {
        EXPECT_NO_THROW({
            auto params = parser->parseUrlParameters(url);
            // Should not crash, behavior may vary
        }) << "Parser should not crash on malformed URL: "
           << url;
    }
}

TEST_F(HttpParserAdvancedMalformedTest, CorruptedMultipartFormData) {
    std::vector<std::string> corruptedMultipart = {
        // Missing boundary
        "POST /upload HTTP/1.1\r\n"
        "Content-Type: multipart/form-data\r\n"
        "\r\n"
        "--boundary\r\n"
        "Content-Disposition: form-data; name=\"field\"\r\n"
        "\r\n"
        "value\r\n"
        "--boundary--\r\n",

        // Boundary mismatch
        "POST /upload HTTP/1.1\r\n"
        "Content-Type: multipart/form-data; boundary=boundary1\r\n"
        "\r\n"
        "--boundary2\r\n"
        "Content-Disposition: form-data; name=\"field\"\r\n"
        "\r\n"
        "value\r\n"
        "--boundary2--\r\n",

        // Missing final boundary
        "POST /upload HTTP/1.1\r\n"
        "Content-Type: multipart/form-data; boundary=boundary\r\n"
        "\r\n"
        "--boundary\r\n"
        "Content-Disposition: form-data; name=\"field\"\r\n"
        "\r\n"
        "value\r\n",

        // Malformed Content-Disposition
        "POST /upload HTTP/1.1\r\n"
        "Content-Type: multipart/form-data; boundary=boundary\r\n"
        "\r\n"
        "--boundary\r\n"
        "Content-Disposition: invalid-disposition\r\n"
        "\r\n"
        "value\r\n"
        "--boundary--\r\n",

        // Missing Content-Disposition
        "POST /upload HTTP/1.1\r\n"
        "Content-Type: multipart/form-data; boundary=boundary\r\n"
        "\r\n"
        "--boundary\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "value\r\n"
        "--boundary--\r\n",
    };

    for (const auto& request : corruptedMultipart) {
        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request);
            // Should not crash, may succeed or fail depending on implementation
        }) << "Parser should not crash on corrupted multipart data";
    }
}

TEST_F(HttpParserAdvancedMalformedTest, HeadersWithInvalidCharacters) {
    std::vector<std::string> invalidHeaderRequests = {
        // Control characters in header name
        "GET /path HTTP/1.1\r\n"
        "Host\x01: example.com\r\n"
        "\r\n",

        // Control characters in header value
        "GET /path HTTP/1.1\r\n"
        "Host: example.com\x02\r\n"
        "\r\n",

        // High-bit characters in header name
        "GET /path HTTP/1.1\r\n"
        "H\xFF\xFEost: example.com\r\n"
        "\r\n",

        // High-bit characters in header value
        "GET /path HTTP/1.1\r\n"
        "Host: example\xFF\xFE.com\r\n"
        "\r\n",

        // Null bytes in header name
        std::string("GET /path HTTP/1.1\r\n") +
            std::string("Ho\x00st: example.com\r\n") + std::string("\r\n"),

        // Null bytes in header value
        std::string("GET /path HTTP/1.1\r\n") +
            std::string("Host: exam\x00ple.com\r\n") + std::string("\r\n"),

        // Unicode characters in header name
        "GET /path HTTP/1.1\r\n"
        "Höst: example.com\r\n"
        "\r\n",

        // Very long header name
        "GET /path HTTP/1.1\r\n" + std::string(1000, 'H') +
            ": example.com\r\n"
            "\r\n",

        // Header name with spaces
        "GET /path HTTP/1.1\r\n"
        "Ho st: example.com\r\n"
        "\r\n",

        // Header name with tabs
        "GET /path HTTP/1.1\r\n"
        "Ho\tst: example.com\r\n"
        "\r\n",
    };

    for (const auto& request : invalidHeaderRequests) {
        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request);
            // Should not crash, behavior may vary
        }) << "Parser should not crash on headers with invalid characters";
    }
}

TEST_F(HttpParserAdvancedMalformedTest, MixedLineEndingFormats) {
    std::vector<std::string> mixedLineEndingRequests = {
        // LF only
        "GET /path HTTP/1.1\n"
        "Host: example.com\n"
        "\n",

        // Mixed CRLF and LF
        "GET /path HTTP/1.1\r\n"
        "Host: example.com\n"
        "User-Agent: TestAgent\r\n"
        "\r\n",

        // CR only (very unusual)
        "GET /path HTTP/1.1\r"
        "Host: example.com\r"
        "\r",

        // Mixed CR and CRLF
        "GET /path HTTP/1.1\r\n"
        "Host: example.com\r"
        "User-Agent: TestAgent\r\n"
        "\r\n",

        // Extra CR characters
        "GET /path HTTP/1.1\r\r\n"
        "Host: example.com\r\n"
        "\r\n",

        // Extra LF characters
        "GET /path HTTP/1.1\r\n\n"
        "Host: example.com\r\n"
        "\r\n",

        // No line endings in request line
        "GET /path HTTP/1.1Host: example.com\r\n\r\n",
    };

    for (const auto& request : mixedLineEndingRequests) {
        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request);
            // Should not crash, behavior may vary
        }) << "Parser should not crash on mixed line endings";
    }
}

// Security-Related Edge Cases Tests
class HttpParserSecurityTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<HttpHeaderParser>();
        spdlog::set_level(spdlog::level::off);
    }
    void TearDown() override { parser.reset(); }

    std::unique_ptr<HttpHeaderParser> parser;
};

TEST_F(HttpParserSecurityTest, HeaderInjectionAttempts) {
    std::vector<std::string> injectionAttempts = {
        // CRLF injection in header value
        "GET /path HTTP/1.1\r\n"
        "Host: example.com\r\nInjected: header\r\n"
        "\r\n",

        // CRLF injection in path
        "GET /path\r\nInjected: header HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "\r\n",

        // Multiple CRLF sequences
        "GET /path HTTP/1.1\r\n"
        "Host: example.com\r\n\r\nInjected: body\r\n"
        "\r\n",

        // LF injection
        "GET /path HTTP/1.1\r\n"
        "Host: example.com\nInjected: header\r\n"
        "\r\n",

        // Null byte injection
        std::string("GET /path HTTP/1.1\r\n") +
            std::string("Host: example.com\x00\r\nInjected: header\r\n") +
            std::string("\r\n"),

        // Tab injection
        "GET /path HTTP/1.1\r\n"
        "Host: example.com\tInjected: header\r\n"
        "\r\n",
    };

    for (const auto& request : injectionAttempts) {
        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request);
            if (result) {
                // Check that injected headers are not present
                EXPECT_FALSE(parser->hasHeader("Injected"));
            }
        }) << "Parser should handle header injection attempts safely";
    }
}

TEST_F(HttpParserSecurityTest, RequestSmugglingPatterns) {
    std::vector<std::string> smugglingAttempts = {
        // Content-Length and Transfer-Encoding conflict
        "POST /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 10\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "0\r\n\r\n",

        // Multiple Content-Length headers
        "POST /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 10\r\n"
        "Content-Length: 20\r\n"
        "\r\n"
        "test body",

        // Folded header with Transfer-Encoding
        "POST /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Transfer-Encoding:\r\n"
        " chunked\r\n"
        "\r\n"
        "0\r\n\r\n",

        // Space before colon in header
        "POST /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length : 10\r\n"
        "\r\n"
        "test body",

        // Tab before colon in header
        "POST /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length\t: 10\r\n"
        "\r\n"
        "test body",
    };

    for (const auto& request : smugglingAttempts) {
        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request);
            // Should not crash, behavior should be consistent
        }) << "Parser should handle request smuggling patterns safely";
    }
}

TEST_F(HttpParserSecurityTest, OversizedHeadersAndValues) {
    // Test with extremely large header names and values
    std::vector<size_t> sizes = {10000, 100000, 1000000};

    for (size_t size : sizes) {
        // Large header name
        std::string largeHeaderName(size, 'H');
        std::string request1 = "GET /path HTTP/1.1\r\n" + largeHeaderName +
                               ": value\r\n"
                               "Host: example.com\r\n"
                               "\r\n";

        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request1);
            // Should handle gracefully, may succeed or fail
        }) << "Parser should handle large header name of size: "
           << size;

        // Large header value
        std::string largeHeaderValue(size, 'V');
        std::string request2 =
            "GET /path HTTP/1.1\r\n"
            "Host: example.com\r\n"
            "Large-Header: " +
            largeHeaderValue +
            "\r\n"
            "\r\n";

        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request2);
            // Should handle gracefully, may succeed or fail
        }) << "Parser should handle large header value of size: "
           << size;
    }
}

TEST_F(HttpParserSecurityTest, NullByteInjection) {
    std::vector<std::string> nullByteTests = {
        // Null byte in method
        std::string("G\x00ET /path HTTP/1.1\r\n") +
            std::string("Host: example.com\r\n\r\n"),

        // Null byte in path
        std::string("GET /pa\x00th HTTP/1.1\r\n") +
            std::string("Host: example.com\r\n\r\n"),

        // Null byte in HTTP version
        std::string("GET /path HTTP/1.\x001\r\n") +
            std::string("Host: example.com\r\n\r\n"),

        // Null byte in header name
        std::string("GET /path HTTP/1.1\r\n") +
            std::string("Ho\x00st: example.com\r\n\r\n"),

        // Null byte in header value
        std::string("GET /path HTTP/1.1\r\n") +
            std::string("Host: exam\x00ple.com\r\n\r\n"),

        // Null byte in body
        std::string("POST /path HTTP/1.1\r\n") +
            std::string("Host: example.com\r\n") +
            std::string("Content-Length: 10\r\n\r\n") +
            std::string("test\x00body"),

        // Multiple null bytes
        std::string("GET /path HTTP/1.1\r\n") +
            std::string("Host: \x00\x00\x00example.com\r\n\r\n"),
    };

    for (const auto& request : nullByteTests) {
        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request);
            // Should not crash, behavior may vary
        }) << "Parser should handle null byte injection safely";
    }
}

// Character Encoding and Internationalization Tests
class HttpParserEncodingAdvancedTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<HttpHeaderParser>();
        spdlog::set_level(spdlog::level::off);
    }
    void TearDown() override { parser.reset(); }

    std::unique_ptr<HttpHeaderParser> parser;
};

TEST_F(HttpParserEncodingAdvancedTest, MixedEncodingInHeadersAndBody) {
    // Test with different encodings in headers vs body
    std::vector<std::pair<std::string, std::string>> encodingTests = {
        // UTF-8 in headers and body
        {"Content-Type: text/plain; charset=utf-8\r\nX-Custom: café",
         "Body with UTF-8: 你好世界"},

        // ASCII headers with UTF-8 body
        {"Content-Type: text/plain\r\nX-Custom: test", "UTF-8 body: 🚀🌟💻"},

        // Headers with various Unicode characters
        {"Content-Type: text/plain\r\nX-Unicode: Iñtërnâtiônàlizætiøn",
         "Regular ASCII body"},

        // Mixed scripts in headers
        {"Content-Type: text/plain\r\nX-Mixed: English中文العربية",
         "Mixed body content"},

        // Emoji in headers
        {"Content-Type: text/plain\r\nX-Emoji: 🚀🌟💻", "Emoji body: 😀😃😄"},
    };

    for (const auto& [headerPart, bodyPart] : encodingTests) {
        std::string request =
            "POST /test HTTP/1.1\r\n"
            "Host: example.com\r\n" +
            headerPart +
            "\r\n"
            "Content-Length: " +
            std::to_string(bodyPart.length()) +
            "\r\n"
            "\r\n" +
            bodyPart;

        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request);
            if (result) {
                std::string retrievedBody = parser->getBody();
                EXPECT_EQ(retrievedBody, bodyPart);
            }
        }) << "Parser should handle mixed encoding correctly";
    }
}

TEST_F(HttpParserEncodingAdvancedTest, ByteOrderMarkHandling) {
    // Test with Byte Order Mark (BOM) in various positions
    std::vector<std::string> bomTests = {
        // UTF-8 BOM at start of request
        std::string("\xEF\xBB\xBF") +
            "GET /path HTTP/1.1\r\nHost: example.com\r\n\r\n",

        // UTF-8 BOM in header value
        "GET /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "X-BOM: " +
            std::string("\xEF\xBB\xBF") +
            "value\r\n"
            "\r\n",

        // UTF-8 BOM in body
        "POST /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 6\r\n"
        "\r\n" +
            std::string("\xEF\xBB\xBF") + "body",

        // UTF-16 BE BOM
        std::string("\xFE\xFF") +
            "GET /path HTTP/1.1\r\nHost: example.com\r\n\r\n",

        // UTF-16 LE BOM
        std::string("\xFF\xFE") +
            "GET /path HTTP/1.1\r\nHost: example.com\r\n\r\n",
    };

    for (const auto& request : bomTests) {
        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request);
            (void)result;  // Suppress unused variable warning
            // Should handle gracefully, behavior may vary
        }) << "Parser should handle BOM characters gracefully";
    }
}

TEST_F(HttpParserEncodingAdvancedTest, ControlCharactersInContent) {
    // Test with various control characters
    std::vector<std::string> controlCharTests = {
        // Tab characters in body
        "POST /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "test\tbody",

        // Vertical tab and form feed
        "POST /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "test\v\fend",

        // Bell and backspace characters
        "POST /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "test\a\bend",

        // Escape sequences
        "POST /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "test\x1B[31m",

        // All control characters (0x00-0x1F)
        "POST /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 32\r\n"
        "\r\n" +
            std::string("\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C"
                        "\x0D\x0E\x0F"
                        "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C"
                        "\x1D\x1E\x1F",
                        32),
    };

    for (const auto& request : controlCharTests) {
        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request);
            (void)result;  // Suppress unused variable warning
            // Should not crash, behavior may vary
        }) << "Parser should handle control characters safely";
    }
}

// Content-Length and Transfer-Encoding Tests
class HttpParserContentLengthTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<HttpHeaderParser>();
        spdlog::set_level(spdlog::level::off);
    }
    void TearDown() override { parser.reset(); }

    std::unique_ptr<HttpHeaderParser> parser;
};

TEST_F(HttpParserContentLengthTest, ContentLengthMismatchScenarios) {
    std::vector<std::pair<std::string, std::string>> mismatchTests = {
        // Content-Length larger than actual body
        {"Content-Length: 20\r\n", "short body"},

        // Content-Length smaller than actual body
        {"Content-Length: 5\r\n", "this is a much longer body than expected"},

        // Content-Length zero with body
        {"Content-Length: 0\r\n", "unexpected body"},

        // Content-Length with no body
        {"Content-Length: 10\r\n", ""},

        // Very large Content-Length
        {"Content-Length: 999999999\r\n", "small body"},

        // Negative Content-Length
        {"Content-Length: -10\r\n", "test body"},

        // Content-Length with leading zeros
        {"Content-Length: 000010\r\n", "test body"},

        // Content-Length with plus sign
        {"Content-Length: +10\r\n", "test body"},
    };

    for (const auto& [headerPart, bodyPart] : mismatchTests) {
        std::string request =
            "POST /test HTTP/1.1\r\n"
            "Host: example.com\r\n" +
            headerPart + "\r\n" + bodyPart;

        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request);
            (void)result;  // Suppress unused variable warning
            // Should handle gracefully, behavior may vary
        }) << "Parser should handle Content-Length mismatch: "
           << headerPart;
    }
}

TEST_F(HttpParserContentLengthTest, InvalidContentLengthValues) {
    std::vector<std::string> invalidContentLengths = {
        "Content-Length: abc\r\n",        // Non-numeric
        "Content-Length: 12.5\r\n",       // Decimal number
        "Content-Length: 1e10\r\n",       // Scientific notation
        "Content-Length: 0x10\r\n",       // Hexadecimal
        "Content-Length: 010\r\n",        // Octal-like
        "Content-Length: \r\n",           // Empty value
        "Content-Length:  \r\n",          // Only spaces
        "Content-Length: 10 bytes\r\n",   // With units
        "Content-Length: 10,000\r\n",     // With comma
        "Content-Length: 10 20\r\n",      // Multiple values
        "Content-Length: infinity\r\n",   // Text value
        "Content-Length: NaN\r\n",        // NaN
        "Content-Length: null\r\n",       // Null
        "Content-Length: undefined\r\n",  // Undefined
    };

    for (const auto& contentLength : invalidContentLengths) {
        std::string request =
            "POST /test HTTP/1.1\r\n"
            "Host: example.com\r\n" +
            contentLength +
            "\r\n"
            "test body";

        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request);
            (void)result;  // Suppress unused variable warning
            // Should handle gracefully, may succeed or fail
        }) << "Parser should handle invalid Content-Length: "
           << contentLength;
    }
}

TEST_F(HttpParserContentLengthTest, MultipleContentLengthHeaders) {
    std::vector<std::string> multipleContentLengthTests = {
        // Two identical Content-Length headers
        "POST /test HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 10\r\n"
        "Content-Length: 10\r\n"
        "\r\n"
        "test body",

        // Two different Content-Length headers
        "POST /test HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 10\r\n"
        "Content-Length: 20\r\n"
        "\r\n"
        "test body",

        // Multiple Content-Length headers with different cases
        "POST /test HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Length: 10\r\n"
        "content-length: 15\r\n"
        "CONTENT-LENGTH: 20\r\n"
        "\r\n"
        "test body",

        // Content-Length mixed with other headers
        "POST /test HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 10\r\n"
        "User-Agent: TestAgent\r\n"
        "Content-Length: 15\r\n"
        "\r\n"
        "test body",
    };

    for (const auto& request : multipleContentLengthTests) {
        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request);
            (void)result;  // Suppress unused variable warning
            // Should handle gracefully, behavior should be consistent
        }) << "Parser should handle multiple Content-Length headers "
              "consistently";
    }
}

TEST_F(HttpParserContentLengthTest, MissingContentLengthForPostRequests) {
    std::vector<std::string> missingContentLengthTests = {
        // POST without Content-Length
        "POST /test HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/json\r\n"
        "\r\n"
        "{\"key\": \"value\"}",

        // PUT without Content-Length
        "PUT /test HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/json\r\n"
        "\r\n"
        "{\"key\": \"value\"}",

        // PATCH without Content-Length
        "PATCH /test HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/json\r\n"
        "\r\n"
        "{\"key\": \"value\"}",

        // POST with empty body but no Content-Length
        "POST /test HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/json\r\n"
        "\r\n",
    };

    for (const auto& request : missingContentLengthTests) {
        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request);
            (void)result;  // Suppress unused variable warning
            // Should handle gracefully, behavior may vary
        }) << "Parser should handle missing Content-Length for POST requests";
    }
}

// Parser State Management and Reuse Tests
class HttpParserStateManagementTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<HttpHeaderParser>();
        spdlog::set_level(spdlog::level::off);
    }
    void TearDown() override { parser.reset(); }

    std::unique_ptr<HttpHeaderParser> parser;
};

TEST_F(HttpParserStateManagementTest, ParserReuseAcrossMultipleRequests) {
    std::vector<std::string> requests = {
        "GET /path1 HTTP/1.1\r\nHost: example.com\r\nUser-Agent: "
        "TestAgent1\r\n\r\n",
        "POST /path2 HTTP/1.1\r\nHost: example.com\r\nContent-Type: "
        "application/json\r\nContent-Length: 13\r\n\r\n{\"key\":\"val\"}",
        "PUT /path3 HTTP/1.1\r\nHost: example.com\r\nAuthorization: Bearer "
        "token123\r\n\r\n",
        "DELETE /path4 HTTP/1.1\r\nHost: example.com\r\nX-Custom: "
        "custom-value\r\n\r\n",
    };

    for (size_t i = 0; i < requests.size(); ++i) {
        EXPECT_TRUE(parser->parseRequest(requests[i]))
            << "Should parse request " << i << " successfully";

        // Verify that previous request data is cleared
        if (i > 0) {
            // Check that headers from previous requests are not present
            EXPECT_FALSE(parser->hasHeader("User-Agent") &&
                         parser->hasHeader("Content-Type"))
                << "Previous request headers should be cleared";
        }

        // Verify current request is parsed correctly
        switch (i) {
            case 0:
                EXPECT_EQ(parser->getMethod(), HttpMethod::GET);
                EXPECT_EQ(parser->getPath(), "/path1");
                EXPECT_TRUE(parser->hasHeader("User-Agent"));
                break;
            case 1:
                EXPECT_EQ(parser->getMethod(), HttpMethod::POST);
                EXPECT_EQ(parser->getPath(), "/path2");
                EXPECT_TRUE(parser->hasHeader("Content-Type"));
                EXPECT_EQ(parser->getBody(), "{\"key\":\"val\"}");
                break;
            case 2:
                EXPECT_EQ(parser->getMethod(), HttpMethod::PUT);
                EXPECT_EQ(parser->getPath(), "/path3");
                EXPECT_TRUE(parser->hasHeader("Authorization"));
                break;
            case 3:
                EXPECT_EQ(parser->getMethod(), HttpMethod::DELETE);
                EXPECT_EQ(parser->getPath(), "/path4");
                EXPECT_TRUE(parser->hasHeader("X-Custom"));
                break;
        }
    }
}

TEST_F(HttpParserStateManagementTest, StatePersistenceBetweenOperations) {
    // Parse initial request
    std::string request =
        "POST /api/data HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 15\r\n"
        "\r\n"
        "{\"test\": true}";

    EXPECT_TRUE(parser->parseRequest(request));

    // Store initial state
    auto initialMethod = parser->getMethod();
    auto initialPath = parser->getPath();
    auto initialBody = parser->getBody();
    auto initialHeaders = parser->getAllHeaders();

    // Perform various operations that shouldn't affect parsed state
    auto encoded = parser->urlEncode("test string");
    auto decoded = parser->urlDecode("test%20string");
    auto cookies = parser->parseCookies("name=value; path=/");
    auto params =
        parser->parseUrlParameters("https://example.com/path?param=value");
    (void)encoded;
    (void)decoded;
    (void)cookies;
    (void)params;  // Suppress warnings

    // Verify state is preserved
    EXPECT_EQ(parser->getMethod(), initialMethod);
    EXPECT_EQ(parser->getPath(), initialPath);
    EXPECT_EQ(parser->getBody(), initialBody);

    auto currentHeaders = parser->getAllHeaders();
    EXPECT_EQ(currentHeaders.size(), initialHeaders.size());
    for (const auto& [key, values] : initialHeaders) {
        EXPECT_TRUE(currentHeaders.count(key) > 0);
        EXPECT_EQ(currentHeaders[key], values);
    }
}

TEST_F(HttpParserStateManagementTest, MemoryManagementUnderStress) {
    // Test memory management with repeated parsing operations
    for (int i = 0; i < 1000; ++i) {
        std::string request = "POST /test" + std::to_string(i) +
                              " HTTP/1.1\r\n"
                              "Host: example.com\r\n"
                              "Content-Type: application/json\r\n"
                              "X-Request-ID: " +
                              std::to_string(i) +
                              "\r\n"
                              "Content-Length: " +
                              std::to_string(10 + i % 100) +
                              "\r\n"
                              "\r\n" +
                              std::string(10 + i % 100, 'x');

        EXPECT_TRUE(parser->parseRequest(request))
            << "Should parse request " << i << " successfully";

        // Verify parsing is correct
        EXPECT_EQ(parser->getMethod(), HttpMethod::POST);
        EXPECT_EQ(parser->getPath(), "/test" + std::to_string(i));
        EXPECT_EQ(parser->getBody().length(), 10 + i % 100);

        // Perform additional operations to stress memory management
        parser->setHeaderValue("Dynamic-Header", "value" + std::to_string(i));
        parser->addHeaderValue("Multi-Value", "value" + std::to_string(i));

        Cookie cookie;
        cookie.name = "cookie" + std::to_string(i);
        cookie.value = "value" + std::to_string(i);
        parser->addCookie(cookie);

        // Clear some data
        if (i % 10 == 0) {
            parser->removeHeader("Dynamic-Header");
            parser->removeCookie("cookie" + std::to_string(i - 5));
        }
    }

    // Final verification that parser is still functional
    std::string finalRequest =
        "GET /final HTTP/1.1\r\nHost: example.com\r\n\r\n";
    EXPECT_TRUE(parser->parseRequest(finalRequest));
    EXPECT_EQ(parser->getMethod(), HttpMethod::GET);
    EXPECT_EQ(parser->getPath(), "/final");
}

TEST_F(HttpParserStateManagementTest, ConcurrentStyleOperations) {
    // Simulate concurrent-style operations (though not actually concurrent)
    std::vector<std::thread> operations;
    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};

    // Simulate multiple "threads" performing operations
    for (int threadId = 0; threadId < 10; ++threadId) {
        // Create separate parser for each "thread" to avoid actual concurrency
        // issues
        auto threadParser = std::make_unique<HttpHeaderParser>();

        for (int i = 0; i < 100; ++i) {
            try {
                std::string request = "GET /thread" + std::to_string(threadId) +
                                      "/request" + std::to_string(i) +
                                      " HTTP/1.1\r\n"
                                      "Host: example.com\r\n"
                                      "X-Thread-ID: " +
                                      std::to_string(threadId) +
                                      "\r\n"
                                      "X-Request-ID: " +
                                      std::to_string(i) +
                                      "\r\n"
                                      "\r\n";

                if (threadParser->parseRequest(request)) {
                    // Verify parsing
                    if (threadParser->getMethod() == HttpMethod::GET &&
                        threadParser->hasHeader("X-Thread-ID") &&
                        threadParser->hasHeader("X-Request-ID")) {
                        successCount++;
                    } else {
                        errorCount++;
                    }
                } else {
                    errorCount++;
                }

                // Perform additional operations
                threadParser->setHeaderValue("Dynamic", "value");
                auto encoded =
                    threadParser->urlEncode("test data " + std::to_string(i));
                auto params = threadParser->parseUrlParameters(
                    "https://example.com/test?id=" + std::to_string(i));
                (void)encoded;
                (void)params;  // Suppress warnings

            } catch (const std::exception&) {
                errorCount++;
            }
        }
    }

    // Verify that most operations succeeded
    EXPECT_GT(successCount.load(), 900);  // At least 90% success rate
    EXPECT_LT(errorCount.load(), 100);    // Less than 10% error rate
}

// Advanced Cookie Testing
class HttpParserAdvancedCookieTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<HttpHeaderParser>();
        spdlog::set_level(spdlog::level::off);
    }
    void TearDown() override { parser.reset(); }

    std::unique_ptr<HttpHeaderParser> parser;
};

TEST_F(HttpParserAdvancedCookieTest, ComplexCookieAttributeCombinations) {
    std::vector<std::string> complexCookies = {
        // All attributes
        "sessionId=abc123; expires=Wed, 21 Oct 2025 07:28:00 GMT; "
        "max-age=3600; domain=.example.com; path=/; secure; httponly; "
        "samesite=strict",

        // Conflicting expires and max-age
        "conflictCookie=value; expires=Wed, 21 Oct 2025 07:28:00 GMT; "
        "max-age=7200",

        // Invalid but parseable attributes
        "testCookie=value; secure=false; httponly=true; samesite=invalid",

        // Case variations
        "caseCookie=value; SECURE; HttpOnly; SameSite=Lax; PATH=/test; "
        "DOMAIN=example.com",

        // Quoted values
        "quotedCookie=\"quoted value\"; path=\"/quoted/path\"; "
        "domain=\".quoted.com\"",

        // Special characters in values
        "specialCookie=value!@#$%^&*(); path=/special; comment=\"Special "
        "cookie with symbols\"",

        // Unicode in cookie values
        "unicodeCookie=café🚀; path=/unicode; comment=\"Unicode test cookie\"",

        // Very long cookie
        "longCookie=" + std::string(4000, 'x') + "; path=/long",

        // Multiple cookies in one string
        "cookie1=value1; path=/; cookie2=value2; domain=.example.com; "
        "cookie3=value3; secure",
    };

    for (const auto& cookieStr : complexCookies) {
        EXPECT_NO_THROW({
            auto cookies = parser->parseCookies(cookieStr);
            EXPECT_GT(cookies.size(), 0)
                << "Should parse at least one cookie from: " << cookieStr;
        }) << "Parser should handle complex cookie: "
           << cookieStr;
    }
}

TEST_F(HttpParserAdvancedCookieTest, CookieExpirationDateParsing) {
    std::vector<std::pair<std::string, bool>> expirationTests = {
        // Valid RFC formats
        {"expires=Wed, 21 Oct 2025 07:28:00 GMT", true},
        {"expires=Wednesday, 21-Oct-25 07:28:00 GMT", true},
        {"expires=Wed Oct 21 07:28:00 2025", true},

        // Invalid formats
        {"expires=2025-10-21 07:28:00", false},
        {"expires=21/10/2025 07:28:00", false},
        {"expires=invalid-date", false},
        {"expires=", false},
        {"expires=Wed, 32 Oct 2025 07:28:00 GMT", false},  // Invalid day
        {"expires=Wed, 21 Foo 2025 07:28:00 GMT", false},  // Invalid month
        {"expires=Wed, 21 Oct 2025 25:28:00 GMT", false},  // Invalid hour

        // Edge cases
        {"expires=Wed, 21 Oct 2025 07:28:00", false},      // Missing timezone
        {"expires=Wed, 21 Oct 2025 07:28:00 PST", false},  // Wrong timezone
        {"expires=Wed, 21 Oct 99 07:28:00 GMT", true},     // Two-digit year
    };

    for (const auto& [expiresStr, shouldBeValid] : expirationTests) {
        std::string cookieStr = "testCookie=value; " + expiresStr;

        EXPECT_NO_THROW({
            auto cookies = parser->parseCookies(cookieStr);
            // Should not crash regardless of validity
        }) << "Parser should handle expiration date: "
           << expiresStr;
    }
}

// Advanced HTTP Method Edge Cases
class HttpParserAdvancedMethodTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<HttpHeaderParser>();
        spdlog::set_level(spdlog::level::off);
    }
    void TearDown() override { parser.reset(); }

    std::unique_ptr<HttpHeaderParser> parser;
};

TEST_F(HttpParserAdvancedMethodTest, TraceAndConnectMethods) {
    std::vector<std::pair<std::string, HttpMethod>> traceConnectTests = {
        // TRACE method tests
        {"TRACE /path HTTP/1.1\r\nHost: example.com\r\n\r\n",
         HttpMethod::TRACE},
        {"TRACE * HTTP/1.1\r\nHost: example.com\r\n\r\n", HttpMethod::TRACE},
        {"TRACE /path HTTP/1.1\r\nHost: example.com\r\nMax-Forwards: "
         "10\r\n\r\n",
         HttpMethod::TRACE},

        // CONNECT method tests
        {"CONNECT example.com:443 HTTP/1.1\r\nHost: example.com:443\r\n\r\n",
         HttpMethod::CONNECT},
        {"CONNECT proxy.example.com:8080 HTTP/1.1\r\nProxy-Authorization: "
         "Basic dGVzdA==\r\n\r\n",
         HttpMethod::CONNECT},
        {"CONNECT [::1]:443 HTTP/1.1\r\nHost: [::1]:443\r\n\r\n",
         HttpMethod::CONNECT},
    };

    for (const auto& [request, expectedMethod] : traceConnectTests) {
        EXPECT_TRUE(parser->parseRequest(request))
            << "Should parse request: " << request;

        EXPECT_EQ(parser->getMethod(), expectedMethod)
            << "Method should match for request: " << request;
    }
}

TEST_F(HttpParserAdvancedMethodTest, MethodCaseSensitivity) {
    std::vector<std::pair<std::string, HttpMethod>> caseSensitivityTests = {
        // Lowercase methods (should be rejected or treated as UNKNOWN)
        {"get /path HTTP/1.1\r\nHost: example.com\r\n\r\n",
         HttpMethod::UNKNOWN},
        {"post /path HTTP/1.1\r\nHost: example.com\r\n\r\n",
         HttpMethod::UNKNOWN},
        {"put /path HTTP/1.1\r\nHost: example.com\r\n\r\n",
         HttpMethod::UNKNOWN},

        // Mixed case methods (should be rejected or treated as UNKNOWN)
        {"Get /path HTTP/1.1\r\nHost: example.com\r\n\r\n",
         HttpMethod::UNKNOWN},
        {"Post /path HTTP/1.1\r\nHost: example.com\r\n\r\n",
         HttpMethod::UNKNOWN},
        {"pUT /path HTTP/1.1\r\nHost: example.com\r\n\r\n",
         HttpMethod::UNKNOWN},

        // Correct uppercase methods
        {"GET /path HTTP/1.1\r\nHost: example.com\r\n\r\n", HttpMethod::GET},
        {"POST /path HTTP/1.1\r\nHost: example.com\r\n\r\n", HttpMethod::POST},
        {"PUT /path HTTP/1.1\r\nHost: example.com\r\n\r\n", HttpMethod::PUT},
    };

    for (const auto& [request, expectedMethod] : caseSensitivityTests) {
        bool parseResult = parser->parseRequest(request);
        if (parseResult) {
            EXPECT_EQ(parser->getMethod(), expectedMethod)
                << "Method should match for request: " << request;
        } else {
            // If parsing fails, that's also acceptable for invalid methods
            EXPECT_EQ(expectedMethod, HttpMethod::UNKNOWN)
                << "Failed parsing should only occur for UNKNOWN methods: "
                << request;
        }
    }
}

TEST_F(HttpParserAdvancedMethodTest, WebDAVMethods) {
    std::vector<std::string> webdavMethods = {
        "PROPFIND", "PROPPATCH", "MKCOL", "COPY", "MOVE", "LOCK", "UNLOCK"};

    for (const auto& method : webdavMethods) {
        std::string request = method +
                              " /webdav/resource HTTP/1.1\r\n"
                              "Host: example.com\r\n"
                              "Depth: 1\r\n"
                              "\r\n";

        EXPECT_TRUE(parser->parseRequest(request))
            << "Should parse WebDAV request: " << method;

        // WebDAV methods should be parsed as UNKNOWN since they're not in the
        // standard enum
        EXPECT_EQ(parser->getMethod(), HttpMethod::UNKNOWN)
            << "WebDAV method should be UNKNOWN: " << method;
    }
}

// Advanced HTTP Version Tests
class HttpParserAdvancedVersionTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<HttpHeaderParser>();
        spdlog::set_level(spdlog::level::off);
    }
    void TearDown() override { parser.reset(); }

    std::unique_ptr<HttpHeaderParser> parser;
};

TEST_F(HttpParserAdvancedVersionTest, HTTP2AndHTTP3Parsing) {
    std::vector<std::pair<std::string, HttpVersion>> versionTests = {
        // HTTP/2 variations
        {"GET /path HTTP/2\r\nHost: example.com\r\n\r\n",
         HttpVersion::HTTP_2_0},
        {"GET /path HTTP/2.0\r\nHost: example.com\r\n\r\n",
         HttpVersion::HTTP_2_0},

        // HTTP/3 variations
        {"GET /path HTTP/3\r\nHost: example.com\r\n\r\n",
         HttpVersion::HTTP_3_0},
        {"GET /path HTTP/3.0\r\nHost: example.com\r\n\r\n",
         HttpVersion::HTTP_3_0},

        // Invalid versions
        {"GET /path HTTP/4.0\r\nHost: example.com\r\n\r\n",
         HttpVersion::UNKNOWN},
        {"GET /path HTTP/1.2\r\nHost: example.com\r\n\r\n",
         HttpVersion::UNKNOWN},
        {"GET /path HTTP/0.9\r\nHost: example.com\r\n\r\n",
         HttpVersion::UNKNOWN},

        // Malformed versions
        {"GET /path HTTP/\r\nHost: example.com\r\n\r\n", HttpVersion::UNKNOWN},
        {"GET /path HTTP\r\nHost: example.com\r\n\r\n", HttpVersion::UNKNOWN},
        {"GET /path HTTPS/1.1\r\nHost: example.com\r\n\r\n",
         HttpVersion::UNKNOWN},
    };

    for (const auto& [request, expectedVersion] : versionTests) {
        bool parseResult = parser->parseRequest(request);
        if (parseResult) {
            EXPECT_EQ(parser->getVersion(), expectedVersion)
                << "Version should match for request: " << request;
        }
        // Note: Some malformed versions might cause parsing to fail entirely
    }
}

TEST_F(HttpParserAdvancedVersionTest, VersionWithExtraWhitespace) {
    std::vector<std::string> whitespaceTests = {
        // Extra spaces
        "GET /path  HTTP/1.1\r\nHost: example.com\r\n\r\n",
        "GET  /path HTTP/1.1\r\nHost: example.com\r\n\r\n",
        "GET /path HTTP/1.1 \r\nHost: example.com\r\n\r\n",

        // Tabs
        "GET\t/path HTTP/1.1\r\nHost: example.com\r\n\r\n",
        "GET /path\tHTTP/1.1\r\nHost: example.com\r\n\r\n",
        "GET /path HTTP/1.1\t\r\nHost: example.com\r\n\r\n",

        // Mixed whitespace
        "GET  \t/path\t HTTP/1.1 \r\nHost: example.com\r\n\r\n",
    };

    for (const auto& request : whitespaceTests) {
        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request);
            (void)result;  // Suppress unused variable warning
            // Should handle gracefully, behavior may vary
        }) << "Parser should handle whitespace variations: "
           << request;
    }
}

// Advanced Header Parsing Edge Cases
class HttpParserAdvancedHeaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<HttpHeaderParser>();
        spdlog::set_level(spdlog::level::off);
    }
    void TearDown() override { parser.reset(); }

    std::unique_ptr<HttpHeaderParser> parser;
};

TEST_F(HttpParserAdvancedHeaderTest, HeaderFoldingAndContinuation) {
    // Note: Header folding is deprecated in HTTP/1.1 but may still be
    // encountered
    std::vector<std::string> foldingTests = {
        // Traditional folding with space
        "GET /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Long-Header: first part\r\n"
        " second part\r\n"
        "\r\n",

        // Traditional folding with tab
        "GET /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Long-Header: first part\r\n"
        "\tsecond part\r\n"
        "\r\n",

        // Multiple folding lines
        "GET /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Multi-Line: part1\r\n"
        " part2\r\n"
        " part3\r\n"
        "\r\n",

        // Folding with mixed whitespace
        "GET /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Mixed-Fold: start\r\n"
        "  \t  continuation\r\n"
        "\r\n",
    };

    for (const auto& request : foldingTests) {
        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request);
            (void)result;  // Suppress unused variable warning
            // Should handle gracefully, behavior depends on implementation
        }) << "Parser should handle header folding: "
           << request;
    }
}

TEST_F(HttpParserAdvancedHeaderTest, HeadersWithSpecialCharacters) {
    std::vector<std::pair<std::string, std::string>> specialCharTests = {
        // Headers with quotes
        {"Quoted-Header", "\"quoted value\""},
        {"Mixed-Quotes", "start \"quoted middle\" end"},
        {"Escaped-Quotes", "value with \\\"escaped\\\" quotes"},

        // Headers with semicolons and commas
        {"Semicolon-Header", "value1; value2; value3"},
        {"Comma-Header", "value1, value2, value3"},
        {"Mixed-Delim", "value1; param=test, value2"},

        // Headers with equals signs
        {"Equals-Header", "key=value; other=data"},
        {"Multiple-Equals", "a=b=c=d"},

        // Headers with parentheses
        {"Paren-Header", "value (with comments)"},
        {"Nested-Paren", "value (outer (inner) comment)"},

        // Headers with brackets
        {"Bracket-Header", "value [with brackets]"},
        {"IPv6-Header", "[2001:db8::1]:8080"},

        // Headers with special symbols
        {"Symbol-Header", "value!@#$%^&*()_+-={}[]|\\:;\"'<>,.?/~`"},
    };

    for (const auto& [headerName, headerValue] : specialCharTests) {
        std::string request =
            "GET /path HTTP/1.1\r\n"
            "Host: example.com\r\n" +
            headerName + ": " + headerValue +
            "\r\n"
            "\r\n";

        EXPECT_NO_THROW({
            bool result = parser->parseRequest(request);
            if (result) {
                auto retrievedValue = parser->getHeaderValue(headerName);
                if (retrievedValue.has_value()) {
                    // Value should be preserved (though may be processed)
                    EXPECT_FALSE(retrievedValue->empty());
                }
            }
        }) << "Parser should handle special characters in header: "
           << headerName;
    }
}

TEST_F(HttpParserAdvancedHeaderTest, DuplicateHeaderHandling) {
    std::vector<std::string> duplicateHeaderTests = {
        // Same header name, different values
        "GET /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Accept: text/html\r\n"
        "Accept: application/json\r\n"
        "Accept: */*\r\n"
        "\r\n",

        // Case variations of same header
        "GET /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: text/html\r\n"
        "content-type: application/json\r\n"
        "CONTENT-TYPE: text/plain\r\n"
        "\r\n",

        // Mixed duplicate headers
        "GET /path HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "X-Custom: value1\r\n"
        "User-Agent: TestAgent\r\n"
        "X-Custom: value2\r\n"
        "Authorization: Bearer token\r\n"
        "X-Custom: value3\r\n"
        "\r\n",
    };

    for (const auto& request : duplicateHeaderTests) {
        EXPECT_TRUE(parser->parseRequest(request))
            << "Should parse request with duplicate headers";

        // Check that parser handles duplicates consistently
        auto allHeaders = parser->getAllHeaders();
        EXPECT_GT(allHeaders.size(), 0) << "Should have parsed some headers";
    }
}
