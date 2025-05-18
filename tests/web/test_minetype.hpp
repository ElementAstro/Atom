// filepath: atom/web/test_minetype.hpp
#ifndef TEST_MINETYPE_HPP
#define TEST_MINETYPE_HPP

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "atom/log/loguru.hpp"
#include "atom/web/minetype.hpp"

namespace fs = std::filesystem;

class MimeTypesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建临时文件用于测试
        tempDir = fs::temp_directory_path() / "atom_mime_test";
        fs::create_directory(tempDir);

        // 创建示例 JSON 文件
        jsonFile = tempDir / "mime.json";
        createSampleJsonFile(jsonFile);

        // 创建示例 XML 文件
        xmlFile = tempDir / "mime.xml";
        createSampleXmlFile(xmlFile);

        // 常用文件扩展名用于测试
        knownFiles = {jsonFile.string(), xmlFile.string()};

        // 创建测试文件
        testFile = tempDir / "test.txt";
        createTestFile(testFile);
    }

    void TearDown() override {
        // 清理临时文件
        try {
            fs::remove_all(tempDir);
        } catch (const std::exception& e) {
            std::cerr << "清理错误: " << e.what() << std::endl;
        }
    }

    // 创建示例 JSON MIME 类型文件的辅助函数
    void createSampleJsonFile(const fs::path& path) {
        std::ofstream file(path);
        file << R"({
            "text/plain": [".txt", ".text", ".log"],
            "text/html": [".html", ".htm"],
            "image/jpeg": [".jpg", ".jpeg"],
            "application/pdf": [".pdf"],
            "application/json": [".json"]
        })";
        file.close();
    }

    // 创建示例 XML MIME 类型文件的辅助函数
    void createSampleXmlFile(const fs::path& path) {
        std::ofstream file(path);
        file << R"(<?xml version="1.0" encoding="UTF-8"?>
        <mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
            <mime-type type="text/plain">
                <glob pattern="*.txt"/>
                <glob pattern="*.text"/>
                <glob pattern="*.log"/>
            </mime-type>
            <mime-type type="text/html">
                <glob pattern="*.html"/>
                <glob pattern="*.htm"/>
            </mime-type>
            <mime-type type="image/jpeg">
                <glob pattern="*.jpg"/>
                <glob pattern="*.jpeg"/>
            </mime-type>
            <mime-type type="application/pdf">
                <glob pattern="*.pdf"/>
            </mime-type>
            <mime-type type="application/json">
                <glob pattern="*.json"/>
            </mime-type>
        </mime-info>)";
        file.close();
    }

    // 创建带内容的测试文件的辅助函数
    void createTestFile(const fs::path& path) {
        std::ofstream file(path);
        file << "This is a test file content.";
        file.close();
    }

    // 创建用于测试的 MimeTypes 实例
    std::unique_ptr<MimeTypes> createMimeTypes(bool lenient = false) {
        return std::make_unique<MimeTypes>(knownFiles, lenient);
    }

    fs::path tempDir;
    fs::path jsonFile;
    fs::path xmlFile;
    fs::path testFile;
    std::vector<std::string> knownFiles;
};

// 测试基本构造函数
TEST_F(MimeTypesTest, BasicConstructor) {
    ASSERT_NO_THROW({ auto mime = createMimeTypes(); });

    ASSERT_NO_THROW({
        auto mime = createMimeTypes(true);  // 带 lenient 标志
    });
}

// 测试带配置的构造函数
TEST_F(MimeTypesTest, ConfigConstructor) {
    MimeTypeConfig config;
    config.lenient = true;
    config.useCache = false;
    config.cacheSize = 500;
    config.enableDeepScanning = true;
    config.defaultType = "application/binary";

    ASSERT_NO_THROW({
        MimeTypes mime(knownFiles, config);
        EXPECT_EQ(mime.getConfig().defaultType, "application/binary");
    });
}

// 测试更新配置
TEST_F(MimeTypesTest, UpdateConfig) {
    auto mime = createMimeTypes();

    MimeTypeConfig newConfig;
    newConfig.lenient = true;
    newConfig.cacheSize = 2000;

    mime->updateConfig(newConfig);

    auto config = mime->getConfig();
    EXPECT_TRUE(config.lenient);
    EXPECT_EQ(config.cacheSize, 2000);
}

// 测试从 JSON 读取
TEST_F(MimeTypesTest, ReadJson) {
    auto mime = createMimeTypes();
    ASSERT_NO_THROW(mime->readJson(jsonFile.string()));

    // 验证一些加载的 MIME 类型
    EXPECT_TRUE(mime->hasMimeType("text/plain"));
    EXPECT_TRUE(mime->hasMimeType("image/jpeg"));

    // 验证一些文件扩展名
    EXPECT_TRUE(mime->hasExtension(".txt"));
    EXPECT_TRUE(mime->hasExtension(".jpg"));
}

// 测试从 XML 读取
TEST_F(MimeTypesTest, ReadXml) {
    auto mime = createMimeTypes();
    ASSERT_NO_THROW(mime->readXml(xmlFile.string()));

    // 验证一些加载的 MIME 类型
    EXPECT_TRUE(mime->hasMimeType("text/html"));
    EXPECT_TRUE(mime->hasMimeType("application/pdf"));

    // 验证一些文件扩展名
    EXPECT_TRUE(mime->hasExtension(".html"));
    EXPECT_TRUE(mime->hasExtension(".pdf"));
}

// 测试从 URL 猜测 MIME 类型
TEST_F(MimeTypesTest, GuessType) {
    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    // 测试各种 URL
    auto result1 = mime->guessType("file.txt");
    auto type1 = result1.first;
    auto charset1 = result1.second;
    EXPECT_TRUE(type1.has_value());
    EXPECT_EQ(*type1, "text/plain");

    auto result2 = mime->guessType("http://example.com/document.pdf");
    auto type2 = result2.first;
    auto charset2 = result2.second;
    EXPECT_TRUE(type2.has_value());
    EXPECT_EQ(*type2, "application/pdf");

    auto result3 = mime->guessType("image.jpg");
    auto type3 = result3.first;
    auto charset3 = result3.second;
    EXPECT_TRUE(type3.has_value());
    EXPECT_EQ(*type3, "image/jpeg");

    // 测试未知扩展名
    auto result4 = mime->guessType("unknown.xyz");
    auto type4 = result4.first;
    auto charset4 = result4.second;
    EXPECT_FALSE(type4.has_value());
}

// 测试猜测扩展名
TEST_F(MimeTypesTest, GuessExtensions) {
    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    // 测试猜测所有扩展名
    auto txtExts = mime->guessAllExtensions("text/plain");
    EXPECT_THAT(txtExts,
                ::testing::UnorderedElementsAre(".txt", ".text", ".log"));

    // 测试猜测单个扩展名
    auto htmlExt = mime->guessExtension("text/html");
    EXPECT_TRUE(htmlExt.has_value());
    EXPECT_TRUE(*htmlExt == ".html" || *htmlExt == ".htm");

    // 测试未知 MIME 类型
    auto unknownExts = mime->guessAllExtensions("application/unknown");
    EXPECT_TRUE(unknownExts.empty());

    auto unknownExt = mime->guessExtension("application/unknown");
    EXPECT_FALSE(unknownExt.has_value());
}

// 测试添加 MIME 类型
TEST_F(MimeTypesTest, AddType) {
    auto mime = createMimeTypes();

    // 添加新 MIME 类型
    ASSERT_NO_THROW(mime->addType("application/custom", ".cst"));

    // 验证已添加
    EXPECT_TRUE(mime->hasMimeType("application/custom"));
    EXPECT_TRUE(mime->hasExtension(".cst"));

    // 测试错误条件下的添加
    EXPECT_THROW(mime->addType("", ".ext"), MimeTypeException);
    EXPECT_THROW(mime->addType("type/subtype", ""), MimeTypeException);
}

// 测试批量添加 MIME 类型
TEST_F(MimeTypesTest, AddTypesBatch) {
    auto mime = createMimeTypes();

    std::vector<std::pair<std::string, std::string>> types = {
        {"application/custom1", ".cst1"},
        {"application/custom2", ".cst2"},
        {"application/custom3", ".cst3"}};

    ASSERT_NO_THROW(mime->addTypesBatch(types));

    // 验证全部已添加
    EXPECT_TRUE(mime->hasMimeType("application/custom1"));
    EXPECT_TRUE(mime->hasMimeType("application/custom2"));
    EXPECT_TRUE(mime->hasMimeType("application/custom3"));

    EXPECT_TRUE(mime->hasExtension(".cst1"));
    EXPECT_TRUE(mime->hasExtension(".cst2"));
    EXPECT_TRUE(mime->hasExtension(".cst3"));
}

// 测试缓存行为
TEST_F(MimeTypesTest, CacheBehavior) {
    MimeTypeConfig config;
    config.useCache = true;
    config.cacheSize = 10;

    MimeTypes mime(knownFiles, config);
    mime.readJson(jsonFile.string());

    // 执行一些查询以填充缓存
    for (int i = 0; i < 15; i++) {
        mime.guessType("file.txt");
        mime.guessType("image.jpg");
    }

    // 清除缓存并验证其工作
    ASSERT_NO_THROW(mime.clearCache());
}

// 测试导出到 JSON
TEST_F(MimeTypesTest, ExportToJson) {
    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    fs::path exportPath = tempDir / "export.json";

    ASSERT_NO_THROW(mime->exportToJson(exportPath.string()));
    EXPECT_TRUE(fs::exists(exportPath));

    // 尝试读取导出的文件
    std::vector<std::string> exportedFilePaths = {exportPath.string()};
    ASSERT_NO_THROW({
        MimeTypes newMime(exportedFilePaths, false);
        EXPECT_TRUE(newMime.hasMimeType("text/plain"));
    });
}

// 测试导出到 XML
TEST_F(MimeTypesTest, ExportToXml) {
    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    fs::path exportPath = tempDir / "export.xml";

    ASSERT_NO_THROW(mime->exportToXml(exportPath.string()));
    EXPECT_TRUE(fs::exists(exportPath));

    // 尝试读取导出的文件
    std::vector<std::string> exportedFilePaths = {exportPath.string()};
    ASSERT_NO_THROW({
        MimeTypes newMime(exportedFilePaths, false);
        EXPECT_TRUE(newMime.hasMimeType("text/plain"));
    });
}

// 测试无效文件的错误处理
TEST_F(MimeTypesTest, InvalidFiles) {
    auto mime = createMimeTypes();

    // 尝试读取不存在的文件
    EXPECT_THROW(mime->readJson("nonexistent.json"), MimeTypeException);
    EXPECT_THROW(mime->readXml("nonexistent.xml"), MimeTypeException);

    // 尝试导出到无效位置
    EXPECT_THROW(mime->exportToJson("/invalid/path/file.json"),
                 MimeTypeException);
    EXPECT_THROW(mime->exportToXml("/invalid/path/file.xml"),
                 MimeTypeException);
}

// 测试根据内容猜测类型
TEST_F(MimeTypesTest, GuessTypeByContent) {
    MimeTypeConfig config;
    config.enableDeepScanning = true;

    MimeTypes mime(knownFiles, config);
    mime.readJson(jsonFile.string());

    // 测试现有文件
    ASSERT_NO_THROW(
        { auto type = mime.guessTypeByContent(testFile.string()); });

    // 测试不存在的文件 - 应抛出异常
    EXPECT_THROW(mime.guessTypeByContent("nonexistent.file"),
                 MimeTypeException);
}

// 测试线程安全性
TEST_F(MimeTypesTest, ThreadSafety) {
    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    constexpr int numThreads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([&mime, i]() {
            // 并行执行各种操作
            if (i % 3 == 0) {
                mime->guessType("file.txt");
                mime->guessType("image.jpg");
            } else if (i % 3 == 1) {
                mime->guessAllExtensions("text/plain");
                mime->hasExtension(".pdf");
            } else {
                mime->addType("application/thread-" + std::to_string(i),
                              ".t" + std::to_string(i));
            }
        });
    }

    // 等待所有线程
    for (auto& thread : threads) {
        thread.join();
    }

    // 验证实例仍然可用
    ASSERT_NO_THROW({
        auto result = mime->guessType("file.txt");
        EXPECT_TRUE(result.first.has_value());
    });
}

// 测试不同路径类型的处理（测试 PathLike 概念）
TEST_F(MimeTypesTest, PathLikeConcept) {
    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    // 测试 std::string
    std::string pathString = testFile.string();
    ASSERT_NO_THROW(mime->guessTypeByContent(pathString));

    // 测试 const char*
    // 使用持久化的字符串，避免悬空指针
    std::string tempString = testFile.string();
    const char* pathCStr = tempString.c_str();
    ASSERT_NO_THROW(mime->guessTypeByContent(pathCStr));

    // 测试 std::filesystem::path 字符串形式
    // 取决于 PathLike 如何实现
    ASSERT_NO_THROW(mime->guessTypeByContent(testFile.string()));
}

// 测试空构造函数和边缘情况
TEST_F(MimeTypesTest, EdgeCases) {
    // 空已知文件
    std::vector<std::string> emptyFiles;
    ASSERT_NO_THROW({ MimeTypes mime(emptyFiles, false); });

    // 检查空字符串处理
    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    EXPECT_FALSE(mime->hasMimeType(""));
    EXPECT_FALSE(mime->hasExtension(""));

    auto result = mime->guessType("");
    auto type = result.first;
    auto charset = result.second;
    EXPECT_FALSE(type.has_value());

    auto exts = mime->guessAllExtensions("");
    EXPECT_TRUE(exts.empty());

    auto ext = mime->guessExtension("");
    EXPECT_FALSE(ext.has_value());
}

// 测试宽松模式
TEST_F(MimeTypesTest, LenientMode) {
    // 创建一个宽松模式关闭的
    auto strictMime = createMimeTypes(false);
    strictMime->readJson(jsonFile.string());

    // 创建一个宽松模式打开的
    auto lenientMime = createMimeTypes(true);
    lenientMime->readJson(jsonFile.string());

    // 测试可能在宽松和严格模式之间有所不同的各种情况
    // 实际行为取决于实现，但我们可以检查它们不会抛出异常
    ASSERT_NO_THROW({
        auto strictResult = strictMime->guessType("unknown.xyz");
        auto strictType = strictResult.first;
        auto strictCharset = strictResult.second;

        auto lenientResult = lenientMime->guessType("unknown.xyz");
        auto lenientType = lenientResult.first;
        auto lenientCharset = lenientResult.second;

        // 在适当的实现中，宽松模式可能返回默认类型
        // 而严格模式可能返回 nullopt
    });
}

#endif  // TEST_MINETYPE_HPP