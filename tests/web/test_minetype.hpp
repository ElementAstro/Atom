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
#include <spdlog/spdlog.h>
#include "atom/web/minetype.hpp"

namespace fs = std::filesystem;

class MimeTypesTest : public ::testing::Test {
protected:
    void SetUp() override {
        tempDir = fs::temp_directory_path() / "atom_mime_test";
        fs::create_directory(tempDir);

        jsonFile = tempDir / "mime.json";
        createSampleJsonFile(jsonFile);

        xmlFile = tempDir / "mime.xml";
        createSampleXmlFile(xmlFile);

        knownFiles = {jsonFile.string(), xmlFile.string()};

        testFile = tempDir / "test.txt";
        createTestFile(testFile);
    }

    void TearDown() override {
        try {
            fs::remove_all(tempDir);
        } catch (const std::exception& e) {
            spdlog::error("Cleanup error: {}", e.what());
        }
    }

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

    void createTestFile(const fs::path& path) {
        std::ofstream file(path);
        file << "This is a test file content.";
        file.close();
    }

    std::unique_ptr<MimeTypes> createMimeTypes(bool lenient = false) {
        return std::make_unique<MimeTypes>(knownFiles, lenient);
    }

    fs::path tempDir;
    fs::path jsonFile;
    fs::path xmlFile;
    fs::path testFile;
    std::vector<std::string> knownFiles;
};

TEST_F(MimeTypesTest, BasicConstructor) {
    ASSERT_NO_THROW({ auto mime = createMimeTypes(); });
    ASSERT_NO_THROW({ auto mime = createMimeTypes(true); });
}

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

TEST_F(MimeTypesTest, ReadJson) {
    auto mime = createMimeTypes();
    ASSERT_NO_THROW(mime->readJson(jsonFile.string()));

    EXPECT_TRUE(mime->hasMimeType("text/plain"));
    EXPECT_TRUE(mime->hasMimeType("image/jpeg"));
    EXPECT_TRUE(mime->hasExtension(".txt"));
    EXPECT_TRUE(mime->hasExtension(".jpg"));
}

TEST_F(MimeTypesTest, ReadXml) {
    auto mime = createMimeTypes();
    ASSERT_NO_THROW(mime->readXml(xmlFile.string()));

    EXPECT_TRUE(mime->hasMimeType("text/html"));
    EXPECT_TRUE(mime->hasMimeType("application/pdf"));
    EXPECT_TRUE(mime->hasExtension(".html"));
    EXPECT_TRUE(mime->hasExtension(".pdf"));
}

TEST_F(MimeTypesTest, GuessType) {
    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    auto result1 = mime->guessType("file.txt");
    auto type1 = result1.first;
    EXPECT_TRUE(type1.has_value());
    EXPECT_EQ(*type1, "text/plain");

    auto result2 = mime->guessType("http://example.com/document.pdf");
    auto type2 = result2.first;
    EXPECT_TRUE(type2.has_value());
    EXPECT_EQ(*type2, "application/pdf");

    auto result3 = mime->guessType("image.jpg");
    auto type3 = result3.first;
    EXPECT_TRUE(type3.has_value());
    EXPECT_EQ(*type3, "image/jpeg");

    auto result4 = mime->guessType("unknown.xyz");
    auto type4 = result4.first;
    EXPECT_FALSE(type4.has_value());
}

TEST_F(MimeTypesTest, GuessExtensions) {
    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    auto txtExts = mime->guessAllExtensions("text/plain");
    EXPECT_THAT(txtExts, ::testing::UnorderedElementsAre(".txt", ".text", ".log"));

    auto htmlExt = mime->guessExtension("text/html");
    EXPECT_TRUE(htmlExt.has_value());
    EXPECT_TRUE(*htmlExt == ".html" || *htmlExt == ".htm");

    auto unknownExts = mime->guessAllExtensions("application/unknown");
    EXPECT_TRUE(unknownExts.empty());

    auto unknownExt = mime->guessExtension("application/unknown");
    EXPECT_FALSE(unknownExt.has_value());
}

TEST_F(MimeTypesTest, AddType) {
    auto mime = createMimeTypes();

    ASSERT_NO_THROW(mime->addType("application/custom", ".cst"));

    EXPECT_TRUE(mime->hasMimeType("application/custom"));
    EXPECT_TRUE(mime->hasExtension(".cst"));

    EXPECT_THROW(mime->addType("", ".ext"), MimeTypeException);
    EXPECT_THROW(mime->addType("type/subtype", ""), MimeTypeException);
}

TEST_F(MimeTypesTest, AddTypesBatch) {
    auto mime = createMimeTypes();

    std::vector<std::pair<std::string, std::string>> types = {
        {"application/custom1", ".cst1"},
        {"application/custom2", ".cst2"},
        {"application/custom3", ".cst3"}};

    ASSERT_NO_THROW(mime->addTypesBatch(types));

    EXPECT_TRUE(mime->hasMimeType("application/custom1"));
    EXPECT_TRUE(mime->hasMimeType("application/custom2"));
    EXPECT_TRUE(mime->hasMimeType("application/custom3"));

    EXPECT_TRUE(mime->hasExtension(".cst1"));
    EXPECT_TRUE(mime->hasExtension(".cst2"));
    EXPECT_TRUE(mime->hasExtension(".cst3"));
}

TEST_F(MimeTypesTest, CacheBehavior) {
    MimeTypeConfig config;
    config.useCache = true;
    config.cacheSize = 10;

    MimeTypes mime(knownFiles, config);
    mime.readJson(jsonFile.string());

    for (int i = 0; i < 15; i++) {
        mime.guessType("file.txt");
        mime.guessType("image.jpg");
    }

    ASSERT_NO_THROW(mime.clearCache());
}

TEST_F(MimeTypesTest, ExportToJson) {
    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    fs::path exportPath = tempDir / "export.json";

    ASSERT_NO_THROW(mime->exportToJson(exportPath.string()));
    EXPECT_TRUE(fs::exists(exportPath));

    std::vector<std::string> exportedFilePaths = {exportPath.string()};
    ASSERT_NO_THROW({
        MimeTypes newMime(exportedFilePaths, false);
        EXPECT_TRUE(newMime.hasMimeType("text/plain"));
    });
}

TEST_F(MimeTypesTest, ExportToXml) {
    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    fs::path exportPath = tempDir / "export.xml";

    ASSERT_NO_THROW(mime->exportToXml(exportPath.string()));
    EXPECT_TRUE(fs::exists(exportPath));

    std::vector<std::string> exportedFilePaths = {exportPath.string()};
    ASSERT_NO_THROW({
        MimeTypes newMime(exportedFilePaths, false);
        EXPECT_TRUE(newMime.hasMimeType("text/plain"));
    });
}

TEST_F(MimeTypesTest, InvalidFiles) {
    auto mime = createMimeTypes();

    EXPECT_THROW(mime->readJson("nonexistent.json"), MimeTypeException);
    EXPECT_THROW(mime->readXml("nonexistent.xml"), MimeTypeException);

    EXPECT_THROW(mime->exportToJson("/invalid/path/file.json"), MimeTypeException);
    EXPECT_THROW(mime->exportToXml("/invalid/path/file.xml"), MimeTypeException);
}

TEST_F(MimeTypesTest, GuessTypeByContent) {
    MimeTypeConfig config;
    config.enableDeepScanning = true;

    MimeTypes mime(knownFiles, config);
    mime.readJson(jsonFile.string());

    ASSERT_NO_THROW({ auto type = mime.guessTypeByContent(testFile.string()); });

    EXPECT_THROW(mime.guessTypeByContent("nonexistent.file"), MimeTypeException);
}

TEST_F(MimeTypesTest, ThreadSafety) {
    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    constexpr int numThreads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back([&mime, i]() {
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

    for (auto& thread : threads) {
        thread.join();
    }

    ASSERT_NO_THROW({
        auto result = mime->guessType("file.txt");
        EXPECT_TRUE(result.first.has_value());
    });
}

TEST_F(MimeTypesTest, PathLikeConcept) {
    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    std::string pathString = testFile.string();
    ASSERT_NO_THROW(mime->guessTypeByContent(pathString));

    std::string tempString = testFile.string();
    const char* pathCStr = tempString.c_str();
    ASSERT_NO_THROW(mime->guessTypeByContent(pathCStr));

    ASSERT_NO_THROW(mime->guessTypeByContent(testFile.string()));
}

TEST_F(MimeTypesTest, EdgeCases) {
    std::vector<std::string> emptyFiles;
    ASSERT_NO_THROW({ MimeTypes mime(emptyFiles, false); });

    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    EXPECT_FALSE(mime->hasMimeType(""));
    EXPECT_FALSE(mime->hasExtension(""));

    auto result = mime->guessType("");
    auto type = result.first;
    EXPECT_FALSE(type.has_value());

    auto exts = mime->guessAllExtensions("");
    EXPECT_TRUE(exts.empty());

    auto ext = mime->guessExtension("");
    EXPECT_FALSE(ext.has_value());
}

TEST_F(MimeTypesTest, LenientMode) {
    auto strictMime = createMimeTypes(false);
    strictMime->readJson(jsonFile.string());

    auto lenientMime = createMimeTypes(true);
    lenientMime->readJson(jsonFile.string());

    ASSERT_NO_THROW({
        auto strictResult = strictMime->guessType("unknown.xyz");
        auto strictType = strictResult.first;

        auto lenientResult = lenientMime->guessType("unknown.xyz");
        auto lenientType = lenientResult.first;
    });
}

#endif  // TEST_MINETYPE_HPP
