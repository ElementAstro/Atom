// filepath: d:\msys64\home\qwdma\Atom\tests\web\test_httpparser.hpp
#ifndef TEST_HTTPPARSER_HPP
#define TEST_HTTPPARSER_HPP

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "atom/log/loguru.hpp"
#include "atom/web/httpparser.hpp"

using namespace atom::web;

class HttpHeaderParserTest : public ::testing::Test {
protected:
    void SetUp() override { parser = std::make_unique<HttpHeaderParser>(); }

    void TearDown() override { parser.reset(); }

    // 辅助函数: 创建一个简单的HTTP请求字符串
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

    // 辅助函数: 创建一个简单的HTTP响应字符串
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

// 测试空请求体的获取
TEST_F(HttpHeaderParserTest, GetEmptyBody) {
    // 准备一个没有请求体的HTTP请求
    std::string emptyBodyRequest = createSampleHttpRequest();

    // 解析请求
    ASSERT_TRUE(parser->parseRequest(emptyBodyRequest));

    // 验证请求体为空
    EXPECT_EQ(parser->getBody(), "");
}

// 测试简单请求体的获取
TEST_F(HttpHeaderParserTest, GetSimpleBody) {
    // 定义一个简单的请求体内容
    std::string sampleBody = "Hello, World!";

    // 准备一个带有请求体的HTTP请求
    std::string requestWithBody = createSampleHttpRequest(sampleBody);

    // 解析请求
    ASSERT_TRUE(parser->parseRequest(requestWithBody));

    // 验证请求体内容
    EXPECT_EQ(parser->getBody(), sampleBody);
}

// 测试JSON请求体的获取
TEST_F(HttpHeaderParserTest, GetJsonBody) {
    // 定义一个JSON请求体
    std::string jsonBody = R"({"name": "Test", "value": 123})";

    // 准备一个带有JSON请求体的HTTP请求
    std::string requestWithJsonBody =
        "POST /api/data HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " +
        std::to_string(jsonBody.length()) +
        "\r\n"
        "\r\n" +
        jsonBody;

    // 解析请求
    ASSERT_TRUE(parser->parseRequest(requestWithJsonBody));

    // 验证请求体内容
    EXPECT_EQ(parser->getBody(), jsonBody);
}

// 测试带特殊字符请求体的获取
TEST_F(HttpHeaderParserTest, GetBodyWithSpecialChars) {
    // 定义一个包含特殊字符的请求体
    std::string specialBody = "Line 1\r\nLine 2\r\n\r\nExtra data: ÄÖÜ";

    // 准备一个带有特殊字符请求体的HTTP请求
    std::string requestWithSpecialBody = createSampleHttpRequest(specialBody);

    // 解析请求
    ASSERT_TRUE(parser->parseRequest(requestWithSpecialBody));

    // 验证请求体内容
    EXPECT_EQ(parser->getBody(), specialBody);
}

// 测试长请求体的获取
TEST_F(HttpHeaderParserTest, GetLongBody) {
    // 创建一个较长的请求体
    std::string longBody;
    for (int i = 0; i < 1000; i++) {
        longBody +=
            "This is line " + std::to_string(i) + " of the long body.\n";
    }

    // 准备一个带有长请求体的HTTP请求
    std::string requestWithLongBody = createSampleHttpRequest(longBody);

    // 解析请求
    ASSERT_TRUE(parser->parseRequest(requestWithLongBody));

    // 验证请求体内容
    EXPECT_EQ(parser->getBody(), longBody);
}

// 测试响应体的获取
TEST_F(HttpHeaderParserTest, GetResponseBody) {
    // 定义一个响应体
    std::string responseBody = "<html><body><h1>Welcome!</h1></body></html>";

    // 准备一个带有响应体的HTTP响应
    std::string responseWithBody = createSampleHttpResponse(responseBody);

    // 解析响应
    ASSERT_TRUE(parser->parseResponse(responseWithBody));

    // 验证响应体内容
    EXPECT_EQ(parser->getBody(), responseBody);
}

// 测试设置和获取请求体
TEST_F(HttpHeaderParserTest, SetAndGetBody) {
    // 设置一个新的请求体
    std::string newBody = "This is a new body content";
    parser->setBody(newBody);

    // 验证请求体内容
    EXPECT_EQ(parser->getBody(), newBody);
}

// 测试更新请求体
TEST_F(HttpHeaderParserTest, UpdateBody) {
    // 首先解析一个请求
    std::string initialBody = "Initial body";
    std::string initialRequest = createSampleHttpRequest(initialBody);
    ASSERT_TRUE(parser->parseRequest(initialRequest));
    EXPECT_EQ(parser->getBody(), initialBody);

    // 然后更新请求体
    std::string updatedBody = "Updated body content";
    parser->setBody(updatedBody);

    // 验证请求体已更新
    EXPECT_EQ(parser->getBody(), updatedBody);
}

// 测试多部分表单数据的请求体获取
TEST_F(HttpHeaderParserTest, GetMultipartFormDataBody) {
    // 定义一个多部分表单数据的请求体
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

    // 准备一个带有多部分表单数据的HTTP请求
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

    // 解析请求
    ASSERT_TRUE(parser->parseRequest(multipartRequest));

    // 验证请求体内容
    EXPECT_EQ(parser->getBody(), multipartBody);
}

// 测试清除请求体
TEST_F(HttpHeaderParserTest, ClearBody) {
    // 首先设置一个请求体
    std::string body = "This is some body content";
    parser->setBody(body);
    EXPECT_EQ(parser->getBody(), body);

    // 清除请求体
    parser->setBody("");

    // 验证请求体已清除
    EXPECT_EQ(parser->getBody(), "");
}

// 测试构建带有请求体的HTTP请求
TEST_F(HttpHeaderParserTest, BuildRequestWithBody) {
    // 设置HTTP请求的组成部分
    parser->setMethod(HttpMethod::POST);
    parser->setPath("/api/data");
    parser->setVersion(HttpVersion::HTTP_1_1);
    parser->setHeaderValue("Host", "example.com");
    parser->setHeaderValue("Content-Type", "application/json");

    // 设置请求体
    std::string jsonBody = R"({"key": "value"})";
    parser->setBody(jsonBody);
    parser->setHeaderValue("Content-Length", std::to_string(jsonBody.length()));

    // 构建HTTP请求
    std::string builtRequest = parser->buildRequest();

    // 解析构建的请求
    HttpHeaderParser newParser;
    ASSERT_TRUE(newParser.parseRequest(builtRequest));

    // 验证请求体被正确处理
    EXPECT_EQ(newParser.getBody(), jsonBody);
}

// 测试构建带有响应体的HTTP响应
TEST_F(HttpHeaderParserTest, BuildResponseWithBody) {
    // 设置HTTP响应的组成部分
    parser->setStatus(HttpStatus::OK());
    parser->setVersion(HttpVersion::HTTP_1_1);
    parser->setHeaderValue("Content-Type", "text/html; charset=utf-8");

    // 设置响应体
    std::string htmlBody = "<html><body>Hello, World!</body></html>";
    parser->setBody(htmlBody);
    parser->setHeaderValue("Content-Length", std::to_string(htmlBody.length()));

    // 构建HTTP响应
    std::string builtResponse = parser->buildResponse();

    // 解析构建的响应
    HttpHeaderParser newParser;
    ASSERT_TRUE(newParser.parseResponse(builtResponse));

    // 验证响应体被正确处理
    EXPECT_EQ(newParser.getBody(), htmlBody);
}

#endif  // TEST_HTTPPARSER_HPP