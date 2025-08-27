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

// Additional Edge Cases and Error Conditions
TEST_F(MimeTypesTest, MalformedJsonFile) {
    fs::path malformedJsonFile = tempDir / "malformed.json";
    std::ofstream file(malformedJsonFile);
    file << R"({
        "text/plain": [".txt", ".text"
        "invalid json structure
    })";
    file.close();

    auto mime = createMimeTypes();
    EXPECT_THROW(mime->readJson(malformedJsonFile.string()), MimeTypeException);
}

TEST_F(MimeTypesTest, MalformedXmlFile) {
    fs::path malformedXmlFile = tempDir / "malformed.xml";
    std::ofstream file(malformedXmlFile);
    file << R"(<?xml version="1.0" encoding="UTF-8"?>
        <mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
            <mime-type type="text/plain">
                <glob pattern="*.txt"/>
            <!-- Missing closing tag
        </mime-info>)";
    file.close();

    auto mime = createMimeTypes();
    EXPECT_THROW(mime->readXml(malformedXmlFile.string()), MimeTypeException);
}

TEST_F(MimeTypesTest, EmptyJsonFile) {
    fs::path emptyJsonFile = tempDir / "empty.json";
    std::ofstream file(emptyJsonFile);
    file << "{}";
    file.close();

    auto mime = createMimeTypes();
    ASSERT_NO_THROW(mime->readJson(emptyJsonFile.string()));

    // Should have no MIME types
    auto result = mime->guessType("test.txt");
    EXPECT_FALSE(result.first.has_value());
}

TEST_F(MimeTypesTest, EmptyXmlFile) {
    fs::path emptyXmlFile = tempDir / "empty.xml";
    std::ofstream file(emptyXmlFile);
    file << R"(<?xml version="1.0" encoding="UTF-8"?>
        <mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
        </mime-info>)";
    file.close();

    auto mime = createMimeTypes();
    ASSERT_NO_THROW(mime->readXml(emptyXmlFile.string()));

    // Should have no MIME types
    auto result = mime->guessType("test.txt");
    EXPECT_FALSE(result.first.has_value());
}

TEST_F(MimeTypesTest, VeryLongFilenames) {
    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    // Test with very long filename
    std::string longFilename(1000, 'a');
    longFilename += ".txt";

    auto result = mime->guessType(longFilename);
    auto type = result.first;
    EXPECT_TRUE(type.has_value());
    EXPECT_EQ(*type, "text/plain");
}

TEST_F(MimeTypesTest, FilenamesWithSpecialCharacters) {
    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    // Test with special characters
    std::vector<std::string> specialFilenames = {
        "file with spaces.txt",
        "file-with-dashes.txt",
        "file_with_underscores.txt",
        "file.with.dots.txt",
        "file@with#symbols$.txt",
        "файл.txt",  // Cyrillic
        "文件.txt"   // Chinese
    };

    for (const auto& filename : specialFilenames) {
        auto result = mime->guessType(filename);
        auto type = result.first;
        EXPECT_TRUE(type.has_value()) << "Failed for filename: " << filename;
        EXPECT_EQ(*type, "text/plain") << "Wrong type for filename: " << filename;
    }
}

TEST_F(MimeTypesTest, CaseInsensitiveExtensions) {
    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    // Test case insensitive extension matching
    std::vector<std::string> caseVariations = {
        "file.txt", "file.TXT", "file.Txt", "file.tXt"
    };

    for (const auto& filename : caseVariations) {
        auto result = mime->guessType(filename);
        auto type = result.first;
        EXPECT_TRUE(type.has_value()) << "Failed for filename: " << filename;
        EXPECT_EQ(*type, "text/plain") << "Wrong type for filename: " << filename;
    }
}

TEST_F(MimeTypesTest, MultipleExtensions) {
    auto mime = createMimeTypes();

    // Add type with multiple extensions
    mime->addType("application/test", ".test1");
    mime->addType("application/test", ".test2");
    mime->addType("application/test", ".test3");

    auto extensions = mime->guessAllExtensions("application/test");
    EXPECT_GE(extensions.size(), 1);
    EXPECT_THAT(extensions, ::testing::Contains(".test1"));
}

TEST_F(MimeTypesTest, DuplicateTypeAddition) {
    auto mime = createMimeTypes();

    // Add same type multiple times
    ASSERT_NO_THROW(mime->addType("application/test", ".test"));
    ASSERT_NO_THROW(mime->addType("application/test", ".test"));  // Duplicate

    EXPECT_TRUE(mime->hasMimeType("application/test"));
    EXPECT_TRUE(mime->hasExtension(".test"));
}

TEST_F(MimeTypesTest, InvalidMimeTypeFormats) {
    auto mime = createMimeTypes();

    // Test invalid MIME type formats
    EXPECT_THROW(mime->addType("invalid-mime-type", ".test"), MimeTypeException);
    EXPECT_THROW(mime->addType("invalid/mime/type/too/many/slashes", ".test"), MimeTypeException);
    EXPECT_THROW(mime->addType("", ".test"), MimeTypeException);
}

TEST_F(MimeTypesTest, InvalidExtensionFormats) {
    auto mime = createMimeTypes();

    // Test invalid extension formats
    EXPECT_THROW(mime->addType("application/test", ""), MimeTypeException);
    EXPECT_THROW(mime->addType("application/test", "no-dot"), MimeTypeException);
}

// Performance and Stress Tests
TEST_F(MimeTypesTest, LargeMimeDatabase) {
    auto mime = createMimeTypes();

    // Add many MIME types
    for (int i = 0; i < 1000; ++i) {
        std::string mimeType = "application/test" + std::to_string(i);
        std::string extension = ".test" + std::to_string(i);
        mime->addType(mimeType, extension);
    }

    // Test lookup performance
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; ++i) {
        std::string filename = "file.test" + std::to_string(i % 1000);
        auto result = mime->guessType(filename);
        EXPECT_TRUE(result.first.has_value());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time (adjust threshold as needed)
    EXPECT_LT(duration.count(), 1000);  // Less than 1 second
}

TEST_F(MimeTypesTest, ConcurrentAccess) {
    auto mime = createMimeTypes();
    mime->readJson(jsonFile.string());

    constexpr int numThreads = 10;
    constexpr int operationsPerThread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&mime, &successCount]() {
            for (int i = 0; i < operationsPerThread; ++i) {
                try {
                    // Mix of different operations
                    if (i % 4 == 0) {
                        auto result = mime->guessType("test.txt");
                        if (result.first.has_value()) successCount++;
                    } else if (i % 4 == 1) {
                        auto extensions = mime->guessAllExtensions("text/plain");
                        if (!extensions.empty()) successCount++;
                    } else if (i % 4 == 2) {
                        bool hasType = mime->hasMimeType("text/plain");
                        if (hasType) successCount++;
                    } else {
                        bool hasExt = mime->hasExtension(".txt");
                        if (hasExt) successCount++;
                    }
                } catch (const std::exception&) {
                    // Ignore exceptions in stress test
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Should have many successful operations
    EXPECT_GT(successCount.load(), numThreads * operationsPerThread / 2);
}

TEST_F(MimeTypesTest, MemoryUsageWithLargeFiles) {
    // Create a large JSON file
    fs::path largeJsonFile = tempDir / "large.json";
    std::ofstream file(largeJsonFile);
    file << "{\n";

    for (int i = 0; i < 10000; ++i) {
        file << "  \"application/test" << i << "\": [\".test" << i << "\"]";
        if (i < 9999) file << ",";
        file << "\n";
    }

    file << "}";
    file.close();

    auto mime = createMimeTypes();

    // Should be able to load large file without issues
    ASSERT_NO_THROW(mime->readJson(largeJsonFile.string()));

    // Verify some entries were loaded
    EXPECT_TRUE(mime->hasMimeType("application/test0"));
    EXPECT_TRUE(mime->hasMimeType("application/test9999"));
}

TEST_F(MimeTypesTest, CacheEffectiveness) {
    MimeTypeConfig config;
    config.useCache = true;
    config.cacheSize = 100;

    MimeTypes mime(knownFiles, config);
    mime.readJson(jsonFile.string());

    // Perform repeated lookups
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        mime.guessType("test.txt");  // Same file repeatedly
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto cachedDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Clear cache and repeat
    mime.clearCache();

    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        mime.guessType("test" + std::to_string(i) + ".txt");  // Different files
    }

    end = std::chrono::high_resolution_clock::now();
    auto uncachedDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Cached lookups should be faster (though this is not guaranteed in all cases)
    // This is more of a performance observation than a strict requirement
    spdlog::info("Cached duration: {} μs, Uncached duration: {} μs",
                 cachedDuration.count(), uncachedDuration.count());
}

TEST_F(MimeTypesTest, DeepScanningPerformance) {
    MimeTypeConfig config;
    config.enableDeepScanning = true;

    MimeTypes mime(knownFiles, config);
    mime.readJson(jsonFile.string());

    // Test deep scanning performance
    auto start = std::chrono::high_resolution_clock::now();

    ASSERT_NO_THROW(mime.guessTypeByContent(testFile.string()));

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time
    EXPECT_LT(duration.count(), 5000);  // Less than 5 seconds
}

TEST_F(MimeTypesTest, ConfigurationPersistence) {
    MimeTypeConfig originalConfig;
    originalConfig.lenient = true;
    originalConfig.useCache = false;
    originalConfig.cacheSize = 500;
    originalConfig.enableDeepScanning = true;
    originalConfig.defaultType = "application/binary";

    MimeTypes mime(knownFiles, originalConfig);

    auto retrievedConfig = mime.getConfig();

    EXPECT_EQ(retrievedConfig.lenient, originalConfig.lenient);
    EXPECT_EQ(retrievedConfig.useCache, originalConfig.useCache);
    EXPECT_EQ(retrievedConfig.cacheSize, originalConfig.cacheSize);
    EXPECT_EQ(retrievedConfig.enableDeepScanning, originalConfig.enableDeepScanning);
    EXPECT_EQ(retrievedConfig.defaultType, originalConfig.defaultType);
}

#endif  // TEST_MINETYPE_HPP
