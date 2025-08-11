// atom/extra/spdlog/utils/test_archiver.cpp

#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include "archiver.h"


using modern_log::LogArchiver;
namespace fs = std::filesystem;

class LogArchiverTest : public ::testing::Test {
protected:
    fs::path temp_dir;

    void SetUp() override {
        temp_dir = fs::temp_directory_path() / fs::path("logarchiver_test_dir");
        fs::remove_all(temp_dir);
        fs::create_directories(temp_dir);
    }

    void TearDown() override { fs::remove_all(temp_dir); }

    void create_file(const std::string& name, size_t size = 10) {
        std::ofstream ofs(temp_dir / name, std::ios::binary);
        for (size_t i = 0; i < size; ++i)
            ofs.put('A');
    }

    void touch_file(const std::string& name,
                    std::chrono::system_clock::time_point tp) {
        auto p = temp_dir / name;
        std::ofstream ofs(p, std::ios::binary | std::ios::app);
        ofs << "X";
        ofs.close();
        auto ftime = fs::file_time_type::clock::now() +
                     (tp - std::chrono::system_clock::now());
        fs::last_write_time(p, ftime);
    }
};

TEST_F(LogArchiverTest, ConstructorCreatesDirectory) {
    fs::remove_all(temp_dir);
    EXPECT_FALSE(fs::exists(temp_dir));
    LogArchiver archiver(temp_dir);
    EXPECT_TRUE(fs::exists(temp_dir));
}

TEST_F(LogArchiverTest, GetArchivableFilesReturnsCorrectExtensions) {
    create_file("a.log");
    create_file("b.txt");
    create_file("c.gz");
    create_file("d.tmp");
    LogArchiver archiver(temp_dir);
    auto files = archiver.get_archivable_files();
    std::vector<std::string> names;
    for (const auto& f : files)
        names.push_back(f.filename().string());
    EXPECT_NE(std::find(names.begin(), names.end(), "a.log"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "b.txt"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "c.gz"), names.end());
    EXPECT_EQ(std::find(names.begin(), names.end(), "d.tmp"), names.end());
}

TEST_F(LogArchiverTest, GetDirectorySizeSumsFileSizes) {
    create_file("a.log", 5);
    create_file("b.txt", 7);
    LogArchiver archiver(temp_dir);
    size_t sz = archiver.get_directory_size();
    EXPECT_EQ(sz, 12u);
}

TEST_F(LogArchiverTest, CleanupExcessFilesRemovesOldest) {
    create_file("a.log");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    create_file("b.log");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    create_file("c.log");
    LogArchiver::ArchiveConfig cfg;
    cfg.max_total_size =
        15;  // Each file is 10 bytes, so only one file should remain
    LogArchiver archiver(temp_dir, cfg);
    archiver.cleanup_excess_files();
    auto files = archiver.get_archivable_files();
    // Only one file should remain (the newest)
    EXPECT_EQ(files.size(), 1u);
}

TEST_F(LogArchiverTest, SetConfigUpdatesConfig) {
    LogArchiver archiver(temp_dir);
    LogArchiver::ArchiveConfig cfg;
    cfg.max_files = 1;
    archiver.set_config(cfg);
    // Should only keep 1 file after archiving
    create_file("a.log");
    create_file("b.log");
    archiver.archive_old_files();
    auto files = archiver.get_archivable_files();
    EXPECT_EQ(files.size(), 1u);
}

TEST_F(LogArchiverTest, GetStatsReturnsCorrectValues) {
    create_file("a.log");
    create_file("b.gz");
    // Make a.log old
    auto old_time =
        std::chrono::system_clock::now() - std::chrono::hours(24 * 8);
    touch_file("a.log", old_time);
    LogArchiver::ArchiveConfig cfg;
    cfg.max_age = std::chrono::hours(24 * 7);
    LogArchiver archiver(temp_dir, cfg);
    auto stats = archiver.get_stats();
    EXPECT_EQ(stats.total_files, 2u);
    EXPECT_EQ(stats.compressed_files, 1u);
    EXPECT_EQ(stats.archived_files, 1u);
    EXPECT_EQ(stats.total_size, fs::file_size(temp_dir / "a.log") +
                                    fs::file_size(temp_dir / "b.gz"));
}

TEST_F(LogArchiverTest, ArchiveOldFilesRemovesExtraFilesAndCompressesOld) {
    create_file("a.log");
    create_file("b.log");
    // Make a.log old
    auto old_time =
        std::chrono::system_clock::now() - std::chrono::hours(24 * 8);
    touch_file("a.log", old_time);
    LogArchiver::ArchiveConfig cfg;
    cfg.max_files = 1;
    cfg.max_age = std::chrono::hours(24 * 7);
    cfg.compress = false;  // Don't actually compress in this test
    LogArchiver archiver(temp_dir, cfg);
    archiver.archive_old_files();
    auto files = archiver.get_archivable_files();
    EXPECT_EQ(files.size(), 1u);
    // Only the newest file should remain
    EXPECT_EQ(files[0].filename().string(), "b.log");
}

TEST_F(LogArchiverTest, CompressFileHandlesNonexistentFileGracefully) {
    LogArchiver archiver(temp_dir);
    EXPECT_FALSE(archiver.compress_file(temp_dir / "no_such_file.log"));
}

TEST_F(LogArchiverTest, DecompressFileHandlesNonexistentFileGracefully) {
    LogArchiver archiver(temp_dir);
    EXPECT_FALSE(archiver.decompress_file(temp_dir / "no_such_file.gz"));
}
