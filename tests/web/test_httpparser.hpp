// filepath: d:\msys64\home\qwdma\Atom\tests\web\test_httpparser.hpp
#ifndef TEST_HTTPPARSER_HPP
#define TEST_HTTPPARSER_HPP

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
        {"User-Agent", {"TestAgent/1.0"}}
    };

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
    std::string cookieStr = " sessionId = abc123 ; userId = user456 ; theme = dark ";

    auto cookies = parser->parseCookies(cookieStr);

    EXPECT_EQ(cookies.size(), 3);
    EXPECT_EQ(cookies["sessionId"], "abc123");
    EXPECT_EQ(cookies["userId"], "user456");
    EXPECT_EQ(cookies["theme"], "dark");
}

// URL Parameter Tests
TEST_F(HttpHeaderParserTest, ParseUrlParameters) {
    std::string url = "https://example.com/api/users?id=123&name=john&active=true";

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
    std::string url = "https://example.com/api/users?param1=&param2=value&param3=";

    auto params = parser->parseUrlParameters(url);

    EXPECT_EQ(params.size(), 3);
    EXPECT_EQ(params["param1"], "");
    EXPECT_EQ(params["param2"], "value");
    EXPECT_EQ(params["param3"], "");
}

// URL Encoding/Decoding Tests
TEST_F(HttpHeaderParserTest, UrlEncode) {
    EXPECT_EQ(HttpHeaderParser::urlEncode("hello world"), "hello%20world");
    EXPECT_EQ(HttpHeaderParser::urlEncode("test@example.com"), "test%40example.com");
    EXPECT_EQ(HttpHeaderParser::urlEncode("a+b=c&d"), "a%2Bb%3Dc%26d");
    EXPECT_EQ(HttpHeaderParser::urlEncode(""), "");
    EXPECT_EQ(HttpHeaderParser::urlEncode("abc123"), "abc123");
}

TEST_F(HttpHeaderParserTest, UrlDecode) {
    EXPECT_EQ(HttpHeaderParser::urlDecode("hello%20world"), "hello world");
    EXPECT_EQ(HttpHeaderParser::urlDecode("test%40example.com"), "test@example.com");
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
    EXPECT_THAT(request, ::testing::HasSubstr("Content-Type: application/json"));
    EXPECT_THAT(request, ::testing::HasSubstr("User-Agent: TestClient/1.0"));
    EXPECT_THAT(request, ::testing::HasSubstr(R"({"name": "John", "email": "john@example.com"})"));
}

TEST_F(HttpHeaderParserTest, BuildCompleteResponse) {
    parser->setStatus(HttpStatus{201, "Created"});
    parser->setVersion(HttpVersion::HTTP_1_1);
    parser->setHeaderValue("Content-Type", "application/json");
    parser->setHeaderValue("Location", "/api/users/123");
    parser->setHeaderValue("Server", "TestServer/1.0");
    parser->setBody(R"({"id": 123, "name": "John", "email": "john@example.com"})");

    std::string response = parser->buildResponse();

    EXPECT_THAT(response, ::testing::HasSubstr("HTTP/1.1 201 Created"));
    EXPECT_THAT(response, ::testing::HasSubstr("Content-Type: application/json"));
    EXPECT_THAT(response, ::testing::HasSubstr("Location: /api/users/123"));
    EXPECT_THAT(response, ::testing::HasSubstr("Server: TestServer/1.0"));
    EXPECT_THAT(response, ::testing::HasSubstr(R"({"id": 123, "name": "John", "email": "john@example.com"})"));
}

// Edge Cases and Error Handling Tests
TEST_F(HttpHeaderParserTest, ParseMalformedRequest) {
    // Missing HTTP version
    std::string malformedRequest = "GET /path\r\nHost: example.com\r\n\r\n";
    EXPECT_FALSE(parser->parseRequest(malformedRequest));

    // Invalid request line
    std::string invalidRequest = "INVALID REQUEST LINE\r\nHost: example.com\r\n\r\n";
    EXPECT_FALSE(parser->parseRequest(invalidRequest));

    // Empty request
    EXPECT_FALSE(parser->parseRequest(""));
}

TEST_F(HttpHeaderParserTest, ParseMalformedResponse) {
    // Missing status code
    std::string malformedResponse = "HTTP/1.1\r\nContent-Type: text/html\r\n\r\n";
    EXPECT_FALSE(parser->parseResponse(malformedResponse));

    // Invalid status line
    std::string invalidResponse = "INVALID RESPONSE LINE\r\nContent-Type: text/html\r\n\r\n";
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
    EXPECT_TRUE(parser->hasHeader("Content-Type") || parser->hasHeader("content-type"));
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
    parser->setHeaderValue("Special-Chars", "Value with spaces, commas, and symbols: !@#$%^&*()");
    parser->setHeaderValue("Unicode-Header", "Value with unicode: 你好世界");

    auto specialValue = parser->getHeaderValue("Special-Chars");
    ASSERT_TRUE(specialValue.has_value());
    EXPECT_EQ(*specialValue, "Value with spaces, commas, and symbols: !@#$%^&*()");

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
    std::string complexCookieStr = "sessionId=abc123; expires=Wed, 21 Oct 2025 07:28:00 GMT; path=/; domain=.example.com; secure; httponly; samesite=strict";

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
    auto params2 = parser->parseUrlParameters("https://example.com/path?a=&b=value&c=");
    EXPECT_EQ(params2.size(), 3);
    EXPECT_EQ(params2["a"], "");
    EXPECT_EQ(params2["b"], "value");
    EXPECT_EQ(params2["c"], "");

    // URL with encoded parameters
    auto params3 = parser->parseUrlParameters("https://example.com/path?name=John%20Doe&email=test%40example.com");
    EXPECT_EQ(params3.size(), 2);
    // Note: This test depends on whether the parser automatically decodes URL parameters
}

#endif  // TEST_HTTPPARSER_HPP
