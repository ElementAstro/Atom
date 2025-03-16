#include <gtest/gtest.h>

#include <zlib.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <thread>
#include <vector>

#include "atom/io/compress.hpp"

namespace fs = std::filesystem;

class CompressSlicesTest : public ::testing::Test {
protected:
    fs::path test_dir = fs::temp_directory_path() / "atom_compress_test";
    std::vector<std::string> slice_files;
    fs::path output_file;

    void SetUp() override {
        // Create test directory
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
        fs::create_directories(test_dir);
        output_file = test_dir / "merged_output.dat";
    }

    void TearDown() override {
        // Clean up test files
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }

    // Utility to create test data
    std::vector<char> createTestData(size_t size) {
        std::vector<char> data(size);
        std::mt19937 rng(static_cast<unsigned int>(std::time(nullptr)));
        std::uniform_int_distribution<int> dist(0, 255);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<char>(dist(rng));
        }
        return data;
    }

    // Create a compressed test file
    std::string createCompressedSlice(const std::vector<char>& data,
                                      int slice_num) {
        fs::path slice_path = test_dir / (std::string("test_slice_") +
                                          std::to_string(slice_num) + ".gz");
        gzFile out = gzopen(slice_path.string().c_str(), "wb");
        EXPECT_TRUE(out != nullptr);

        if (out) {
            gzwrite(out, data.data(), static_cast<unsigned int>(data.size()));
            gzclose(out);
        }

        return slice_path.string();
    }

    // Verify merged file content
    bool verifyMergedContent(
        const std::vector<std::vector<char>>& original_slices) {
        if (!fs::exists(output_file)) {
            return false;
        }

        std::ifstream file(output_file, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        // Check each slice's content
        for (const auto& slice_data : original_slices) {
            std::vector<char> read_data(slice_data.size());
            if (!file.read(read_data.data(), read_data.size())) {
                return false;
            }

            if (std::memcmp(read_data.data(), slice_data.data(),
                            slice_data.size()) != 0) {
                return false;
            }
        }

        // Check that we've reached EOF
        char extra;
        file.read(&extra, 1);
        return file.eof();
    }
};

// Test merging a small number of slices
TEST_F(CompressSlicesTest, MergeSimpleSlices) {
    // Create test data
    std::vector<std::vector<char>> original_data;
    original_data.push_back(createTestData(1000));
    original_data.push_back(createTestData(2000));
    original_data.push_back(createTestData(1500));

    // Create compressed slices
    for (size_t i = 0; i < original_data.size(); i++) {
        slice_files.push_back(
            createCompressedSlice(original_data[i], static_cast<int>(i)));
    }

    // Merge slices
    atom::io::DecompressionOptions options;
    options.use_parallel = false;
    auto result = atom::io::mergeCompressedSlices(
        slice_files, output_file.string(), options);

    // Verify results
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.original_size, 1000 + 2000 + 1500);
    EXPECT_TRUE(verifyMergedContent(original_data));
}

// Test merging with parallel processing
TEST_F(CompressSlicesTest, MergeSlicesParallel) {
    // Create test data
    std::vector<std::vector<char>> original_data;
    original_data.push_back(createTestData(10000));
    original_data.push_back(createTestData(15000));
    original_data.push_back(createTestData(12000));
    original_data.push_back(createTestData(8000));

    // Create compressed slices
    for (size_t i = 0; i < original_data.size(); i++) {
        slice_files.push_back(
            createCompressedSlice(original_data[i], static_cast<int>(i)));
    }

    // Merge slices with parallel processing
    atom::io::DecompressionOptions options;
    options.use_parallel = true;
    auto result = atom::io::mergeCompressedSlices(
        slice_files, output_file.string(), options);

    // Verify results
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.original_size, 10000 + 15000 + 12000 + 8000);
    EXPECT_TRUE(verifyMergedContent(original_data));
}

// Test merging with custom chunk size
TEST_F(CompressSlicesTest, MergeWithCustomChunkSize) {
    // Create test data
    std::vector<std::vector<char>> original_data;
    original_data.push_back(createTestData(5000));
    original_data.push_back(createTestData(7000));

    // Create compressed slices
    for (size_t i = 0; i < original_data.size(); i++) {
        slice_files.push_back(
            createCompressedSlice(original_data[i], static_cast<int>(i)));
    }

    // Merge slices with custom chunk size
    atom::io::DecompressionOptions options;
    options.chunk_size = 1024;  // Small chunk size to test multiple reads
    options.use_parallel = false;
    auto result = atom::io::mergeCompressedSlices(
        slice_files, output_file.string(), options);

    // Verify results
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.original_size, 5000 + 7000);
    EXPECT_TRUE(verifyMergedContent(original_data));
}

// Test error handling: empty slice list
TEST_F(CompressSlicesTest, EmptySliceList) {
    std::vector<std::string> empty_slices;

    atom::io::DecompressionOptions options;
    auto result = atom::io::mergeCompressedSlices(
        empty_slices, output_file.string(), options);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
    EXPECT_EQ(result.original_size, 0);
    EXPECT_EQ(result.compressed_size, 0);
    EXPECT_FALSE(fs::exists(output_file));
}

// Test error handling: empty output path
TEST_F(CompressSlicesTest, EmptyOutputPath) {
    std::vector<std::vector<char>> original_data;
    original_data.push_back(createTestData(1000));

    for (size_t i = 0; i < original_data.size(); i++) {
        slice_files.push_back(
            createCompressedSlice(original_data[i], static_cast<int>(i)));
    }

    atom::io::DecompressionOptions options;
    auto result = atom::io::mergeCompressedSlices(slice_files, "", options);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

// Test with invalid slice file
TEST_F(CompressSlicesTest, InvalidSliceFile) {
    // Create one valid slice
    std::vector<char> valid_data = createTestData(1000);
    slice_files.push_back(createCompressedSlice(valid_data, 0));

    // Add an invalid slice path
    slice_files.push_back(test_dir / "non_existent_file.gz");

    atom::io::DecompressionOptions options;
    options.use_parallel =
        false;  // Ensure sequential processing to test error handling
    auto result = atom::io::mergeCompressedSlices(
        slice_files, output_file.string(), options);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

// Test with corrupted slice file
TEST_F(CompressSlicesTest, CorruptedSliceFile) {
    // Create valid slices
    std::vector<std::vector<char>> original_data;
    original_data.push_back(createTestData(1000));
    original_data.push_back(createTestData(2000));

    for (size_t i = 0; i < original_data.size(); i++) {
        slice_files.push_back(
            createCompressedSlice(original_data[i], static_cast<int>(i)));
    }

    // Corrupt one of the files
    std::ofstream corrupt(slice_files[1], std::ios::binary | std::ios::app);
    corrupt.write("CORRUPT", 7);
    corrupt.close();

    atom::io::DecompressionOptions options;
    options.use_parallel = false;
    auto result = atom::io::mergeCompressedSlices(
        slice_files, output_file.string(), options);

    // The result might be success=false (if corruption is detected early)
    // or incomplete data if the corruption is only detected during reading
    if (result.success) {
        EXPECT_FALSE(verifyMergedContent(original_data));
    } else {
        EXPECT_FALSE(result.error_message.empty());
    }
}

// Test merging a large number of slices
TEST_F(CompressSlicesTest, MergeManySlices) {
    // Create 10 slices with varying sizes
    std::vector<std::vector<char>> original_data;
    for (int i = 0; i < 10; i++) {
        // Size between 500 and 5000 bytes
        size_t size = 500 + i * 500;
        original_data.push_back(createTestData(size));
        slice_files.push_back(createCompressedSlice(original_data.back(), i));
    }

    // Test both parallel and sequential approaches
    for (bool use_parallel : {false, true}) {
        // Reset output file
        if (fs::exists(output_file)) {
            fs::remove(output_file);
        }

        atom::io::DecompressionOptions options;
        options.use_parallel = use_parallel;
        auto result = atom::io::mergeCompressedSlices(
            slice_files, output_file.string(), options);

        EXPECT_TRUE(result.success);
        size_t total_size = 0;
        for (const auto& data : original_data) {
            total_size += data.size();
        }
        EXPECT_EQ(result.original_size, total_size);
        EXPECT_TRUE(verifyMergedContent(original_data));

        // Add a small delay between tests to ensure files are properly closed
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Test merging slices with different compression levels
TEST_F(CompressSlicesTest, MergeSlicesWithDifferentCompressionLevels) {
    std::vector<std::vector<char>> original_data;
    original_data.push_back(createTestData(3000));
    original_data.push_back(createTestData(2000));

    // Create slice files with different compression levels
    for (int i = 0; i < 2; i++) {
        fs::path slice_path =
            test_dir / (std::string("test_slice_") + std::to_string(i) + ".gz");
        gzFile out = gzopen(slice_path.string().c_str(), "wb");
        EXPECT_TRUE(out != nullptr);

        if (out) {
            // Use different compression levels (1 for first file, 9 for second)
            gzsetparams(out, i == 0 ? 1 : 9, Z_DEFAULT_STRATEGY);
            gzwrite(out, original_data[i].data(),
                    static_cast<unsigned int>(original_data[i].size()));
            gzclose(out);
        }

        slice_files.push_back(slice_path.string());
    }

    atom::io::DecompressionOptions options;
    auto result = atom::io::mergeCompressedSlices(
        slice_files, output_file.string(), options);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.original_size, 3000 + 2000);
    EXPECT_TRUE(verifyMergedContent(original_data));
}

// Test merging slices when output file already exists
TEST_F(CompressSlicesTest, MergeToExistingFile) {
    // Create an existing output file
    std::ofstream existing_file(output_file, std::ios::binary);
    existing_file << "This file should be overwritten";
    existing_file.close();

    // Create test slices
    std::vector<std::vector<char>> original_data;
    original_data.push_back(createTestData(1000));
    original_data.push_back(createTestData(2000));

    for (size_t i = 0; i < original_data.size(); i++) {
        slice_files.push_back(
            createCompressedSlice(original_data[i], static_cast<int>(i)));
    }

    atom::io::DecompressionOptions options;
    auto result = atom::io::mergeCompressedSlices(
        slice_files, output_file.string(), options);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.original_size, 1000 + 2000);
    EXPECT_TRUE(verifyMergedContent(original_data));
}

// Test performance with large files
TEST_F(CompressSlicesTest, DISABLED_LargeFilesPerformance) {
    // This test creates larger slices to test performance - disabled by default
    std::vector<std::vector<char>> original_data;
    const size_t slice_size = 10 * 1024 * 1024;  // 10MB slices
    const int num_slices = 5;                    // 50MB total

    for (int i = 0; i < num_slices; i++) {
        original_data.push_back(createTestData(slice_size));
        slice_files.push_back(createCompressedSlice(original_data.back(), i));
    }

    // Test both sequential and parallel approaches and measure performance
    for (bool use_parallel : {false, true}) {
        // Reset output file
        if (fs::exists(output_file)) {
            fs::remove(output_file);
        }

        atom::io::DecompressionOptions options;
        options.use_parallel = use_parallel;

        auto start_time = std::chrono::high_resolution_clock::now();
        auto result = atom::io::mergeCompressedSlices(
            slice_files, output_file.string(), options);
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            end_time - start_time)
                            .count();

        EXPECT_TRUE(result.success);
        std::cout << "Merge performance ("
                  << (use_parallel ? "parallel" : "sequential")
                  << "): " << duration << "ms" << std::endl;
        EXPECT_TRUE(verifyMergedContent(original_data));

        // Add a small delay between tests
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}