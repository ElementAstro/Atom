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

#endif  // TEST_HTTPPARSER_HPP
