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
    slice_files.push_back((test_dir / "non_existent_file.gz").string());

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
// Test for the compressData and decompressData template functions
TEST(CompressTest, DataCompression) {
    // Test data with different patterns
    std::vector<std::string> test_data = {
        "Simple test string to compress",
        std::string(1000, 'A'),  // Highly compressible
        // Random data with lower compression ratio
        []() {
            std::string random_data(1000, 0);
            std::mt19937 rng(42);  // Fixed seed for reproducibility
            std::uniform_int_distribution<int> dist(0, 255);
            for (auto& c : random_data) {
                c = static_cast<char>(dist(rng));
            }
            return random_data;
        }()};

    for (const auto& data : test_data) {
        // Test with different compression levels
        for (int level : {1, 6, 9}) {
            atom::io::CompressionOptions options;
            options.level = level;

            // Compress data
            auto [compress_result, compressed] =
                atom::io::compressData(data, options);

            EXPECT_TRUE(compress_result.success);
            EXPECT_EQ(compress_result.original_size, data.size());
            EXPECT_GT(compress_result.original_size,
                      compress_result.compressed_size);

            // Decompress data
            atom::io::DecompressionOptions decomp_options;
            auto [decompress_result, decompressed] = atom::io::decompressData(
                compressed, data.size(), decomp_options);

            EXPECT_TRUE(decompress_result.success);
            EXPECT_EQ(decompress_result.original_size, data.size());

            // Convert unsigned char vector to string for comparison
            std::string decompressed_str(decompressed.begin(),
                                         decompressed.end());
            EXPECT_EQ(decompressed_str, data);
        }
    }
}

// Test compression and decompression of different data types
TEST(CompressTest, DifferentDataTypes) {
    const std::string original_text =
        "Test data with some repetitions: " + std::string(50, 'a');

    // Test with std::vector<char>
    {
        std::vector<char> data(original_text.begin(), original_text.end());
        auto [compress_result, compressed] = atom::io::compressData(data, {});
        EXPECT_TRUE(compress_result.success);

        auto [decompress_result, decompressed] =
            atom::io::decompressData(compressed, data.size(), {});
        EXPECT_TRUE(decompress_result.success);

        std::string decompressed_str(decompressed.begin(), decompressed.end());
        EXPECT_EQ(decompressed_str, original_text);
    }

    // Test with std::span
    {
        std::vector<unsigned char> data(original_text.begin(),
                                        original_text.end());
        std::span<const unsigned char> data_span(data);

        auto [compress_result, compressed] =
            atom::io::compressData(data_span, {});
        EXPECT_TRUE(compress_result.success);

        auto [decompress_result, decompressed] =
            atom::io::decompressData(compressed, data.size(), {});
        EXPECT_TRUE(decompress_result.success);

        std::string decompressed_str(decompressed.begin(), decompressed.end());
        EXPECT_EQ(decompressed_str, original_text);
    }
}

class FileCompressionTest : public ::testing::Test {
protected:
    fs::path test_dir;
    fs::path test_file;
    fs::path output_dir;
    std::string test_content;

    void SetUp() override {
        test_dir = fs::temp_directory_path() / "atom_file_compress_test";
        output_dir = test_dir / "output";

        // Create test directory
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
        fs::create_directories(test_dir);
        fs::create_directories(output_dir);

        // Create test file with content
        test_file = test_dir / "test_file.txt";
        test_content = "This is a test file.\n" + std::string(1000, 'A');
        std::ofstream file(test_file);
        file << test_content;
        file.close();
    }

    void TearDown() override {
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }
};

// Test file compression and decompression
TEST_F(FileCompressionTest, CompressAndDecompressFile) {
    // Compress file
    atom::io::CompressionOptions options;
    options.level = 6;
    auto compress_result = atom::io::compressFile(test_file.string(),
                                                  output_dir.string(), options);

    EXPECT_TRUE(compress_result.success);
    EXPECT_EQ(compress_result.original_size, test_content.size());

    // Check if compressed file exists
    fs::path compressed_file =
        output_dir / (test_file.filename().string() + ".gz");
    EXPECT_TRUE(fs::exists(compressed_file));

    // Decompress file
    atom::io::DecompressionOptions decomp_options;
    fs::path decomp_dir = test_dir / "decompressed";
    fs::create_directories(decomp_dir);

    auto decompress_result = atom::io::decompressFile(
        compressed_file.string(), decomp_dir.string(), decomp_options);

    EXPECT_TRUE(decompress_result.success);
    EXPECT_EQ(decompress_result.original_size, test_content.size());

    // Check if decompressed file exists and has correct content
    fs::path decompressed_file = decomp_dir / test_file.filename();
    EXPECT_TRUE(fs::exists(decompressed_file));

    std::ifstream file(decompressed_file);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    EXPECT_EQ(content, test_content);
}

// Test error handling in file compression
TEST_F(FileCompressionTest, CompressFileErrors) {
    // Test with non-existent file
    auto result1 = atom::io::compressFile("non_existent_file.txt",
                                          output_dir.string(), {});
    EXPECT_FALSE(result1.success);
    EXPECT_FALSE(result1.error_message.empty());

    // Test with empty path
    auto result2 = atom::io::compressFile("", output_dir.string(), {});
    EXPECT_FALSE(result2.success);
    EXPECT_FALSE(result2.error_message.empty());

    // Test with invalid output directory
    auto result3 = atom::io::compressFile(test_file.string(),
                                          "/invalid/directory/path", {});
    EXPECT_FALSE(result3.success);
    EXPECT_FALSE(result3.error_message.empty());
}

// Test folder compression and extraction
class FolderCompressionTest : public ::testing::Test {
protected:
    fs::path test_dir;
    fs::path source_dir;
    fs::path output_dir;
    fs::path zip_file;
    std::map<std::string, std::string> test_files;

    void SetUp() override {
        test_dir = fs::temp_directory_path() / "atom_folder_compress_test";
        source_dir = test_dir / "source";
        output_dir = test_dir / "output";
        zip_file = test_dir / "compressed_folder.zip";

        // Create test directories
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
        fs::create_directories(test_dir);
        fs::create_directories(source_dir);
        fs::create_directories(output_dir);

        // Create test files with content
        test_files = {
            {"file1.txt", "Content of file 1\n" + std::string(100, 'A')},
            {"file2.txt", "Content of file 2\n" + std::string(200, 'B')},
            {"subfolder/file3.txt",
             "Content of file 3\n" + std::string(300, 'C')}};

        for (const auto& [path, content] : test_files) {
            fs::path file_path = source_dir / path;
            fs::create_directories(file_path.parent_path());
            std::ofstream file(file_path);
            file << content;
            file.close();
        }
    }

    void TearDown() override {
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
    }

    bool verifyExtractedFiles() {
        for (const auto& [path, content] : test_files) {
            fs::path file_path = output_dir / path;
            if (!fs::exists(file_path)) {
                return false;
            }

            std::ifstream file(file_path);
            std::string extracted_content(
                (std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>());
            if (extracted_content != content) {
                return false;
            }
        }
        return true;
    }
};

// Test compressing and extracting folders
TEST_F(FolderCompressionTest, CompressAndExtractFolder) {
    // Compress folder
    atom::io::CompressionOptions comp_options;
    auto compress_result = atom::io::compressFolder(
        source_dir.string(), zip_file.string(), comp_options);

    EXPECT_TRUE(compress_result.success);
    EXPECT_TRUE(fs::exists(zip_file));

    // Extract ZIP file
    atom::io::DecompressionOptions decomp_options;
    auto extract_result = atom::io::extractZip(
        zip_file.string(), output_dir.string(), decomp_options);

    EXPECT_TRUE(extract_result.success);
    EXPECT_TRUE(verifyExtractedFiles());
}

// Test parallel folder compression
TEST_F(FolderCompressionTest, ParallelCompression) {
    // Create more test files for parallel processing
    for (int i = 0; i < 10; i++) {
        std::string filename = "parallel_file_" + std::to_string(i) + ".txt";
        std::string content = "Content of parallel file " + std::to_string(i) +
                              "\n" + std::string(1000 + i * 100, 'X');
        fs::path file_path = source_dir / filename;
        std::ofstream file(file_path);
        file << content;
        file.close();
        test_files[filename] = content;
    }

    // Test parallel compression
    atom::io::CompressionOptions parallel_options;
    parallel_options.use_parallel = true;
    parallel_options.num_threads = 4;

    auto compress_result = atom::io::compressFolder(
        source_dir.string(), zip_file.string(), parallel_options);

    EXPECT_TRUE(compress_result.success);
    EXPECT_TRUE(fs::exists(zip_file));

    // Extract and verify
    atom::io::DecompressionOptions decomp_options;
    auto extract_result = atom::io::extractZip(
        zip_file.string(), output_dir.string(), decomp_options);

    EXPECT_TRUE(extract_result.success);
    EXPECT_TRUE(verifyExtractedFiles());
}

// Test ZIP operations (list, exists, remove)
TEST_F(FolderCompressionTest, ZipOperations) {
    // First compress the folder
    atom::io::compressFolder(source_dir.string(), zip_file.string(), {});

    // Test listing ZIP contents
    auto contents = atom::io::listZipContents(zip_file.string());
    EXPECT_FALSE(contents.empty());

    // Verify that all files are in the list
    std::set<std::string> expected_files;
    for (const auto& [path, _] : test_files) {
        expected_files.insert(path);
    }

    std::set<std::string> actual_files;
    for (const auto& info : contents) {
        actual_files.insert(info.name);
    }

    for (const auto& file : expected_files) {
        EXPECT_TRUE(actual_files.find(file) != actual_files.end())
            << "File not found in ZIP: " << file;
    }

    // Test file exists in ZIP
    EXPECT_TRUE(atom::io::fileExistsInZip(zip_file.string(), "file1.txt"));
    EXPECT_FALSE(
        atom::io::fileExistsInZip(zip_file.string(), "non_existent_file.txt"));

    // Test removing a file from ZIP
    auto remove_result =
        atom::io::removeFromZip(zip_file.string(), "file1.txt");
    EXPECT_TRUE(remove_result.success);

    // Verify file was removed
    EXPECT_FALSE(atom::io::fileExistsInZip(zip_file.string(), "file1.txt"));
    EXPECT_TRUE(atom::io::fileExistsInZip(zip_file.string(), "file2.txt"));

    // Test getZipSize
    auto size = atom::io::getZipSize(zip_file.string());
    EXPECT_TRUE(size.has_value());
    EXPECT_GT(*size, 0);
}

// Test edge cases for data compression and decompression
TEST(CompressTest, EdgeCases) {
    // Test empty data
    {
        std::vector<char> empty_data;
        auto [compress_result, compressed] =
            atom::io::compressData(empty_data, {});
        EXPECT_FALSE(compress_result.success);
        EXPECT_TRUE(compressed.empty());
    }

    // Test very large data
    {
        const size_t large_size = 10 * 1024 * 1024;  // 10MB
        std::vector<char> large_data(large_size, 'X');

        atom::io::CompressionOptions options;
        auto [compress_result, compressed] =
            atom::io::compressData(large_data, options);

        EXPECT_TRUE(compress_result.success);
        EXPECT_LT(compressed.size(), large_size);  // Should compress well

        auto [decompress_result, decompressed] =
            atom::io::decompressData(compressed, large_size, {});

        EXPECT_TRUE(decompress_result.success);
        EXPECT_EQ(decompressed.size(), large_size);
        EXPECT_EQ(decompressed[0], 'X');
        EXPECT_EQ(decompressed[large_size - 1], 'X');
    }

    // Test corrupted compressed data
    {
        std::string original = "Test data for corruption test";
        auto [compress_result, compressed] =
            atom::io::compressData(original, {});

        // Corrupt the data
        if (!compressed.empty()) {
            compressed[compressed.size() / 2] ^= 0xFF;  // Flip some bits
        }

        auto [decompress_result, decompressed] =
            atom::io::decompressData(compressed, original.size(), {});

        EXPECT_FALSE(decompress_result.success);
    }
}

// Test compression/decompression with different chunk sizes
TEST_F(FileCompressionTest, DifferentChunkSizes) {
    std::vector<size_t> chunk_sizes = {512, 4096, 65536};

    for (auto chunk_size : chunk_sizes) {
        // Compress with specific chunk size
        atom::io::CompressionOptions comp_options;
        comp_options.chunk_size = chunk_size;

        auto compress_result = atom::io::compressFile(
            test_file.string(), output_dir.string(), comp_options);

        EXPECT_TRUE(compress_result.success);

        // Decompress with same chunk size
        fs::path compressed_file =
            output_dir / (test_file.filename().string() + ".gz");
        fs::path decomp_dir =
            test_dir / ("decompressed_" + std::to_string(chunk_size));
        fs::create_directories(decomp_dir);

        atom::io::DecompressionOptions decomp_options;
        decomp_options.chunk_size = chunk_size;

        auto decompress_result = atom::io::decompressFile(
            compressed_file.string(), decomp_dir.string(), decomp_options);

        EXPECT_TRUE(decompress_result.success);

        // Verify content
        fs::path decompressed_file = decomp_dir / test_file.filename();
        std::ifstream file(decompressed_file);
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        EXPECT_EQ(content, test_content);
    }
}

// Test compressing with backup option
TEST_F(FileCompressionTest, CompressWithBackup) {
    // First compress normally
    atom::io::CompressionOptions options;
    options.create_backup = false;

    auto result1 = atom::io::compressFile(test_file.string(),
                                          output_dir.string(), options);
    EXPECT_TRUE(result1.success);

    fs::path compressed_file =
        output_dir / (test_file.filename().string() + ".gz");
    EXPECT_TRUE(fs::exists(compressed_file));

    // Change test file content
    std::string new_content = "Modified content\n" + std::string(500, 'B');
    {
        std::ofstream file(test_file);
        file << new_content;
    }

    // Compress again with backup option
    options.create_backup = true;
    auto result2 = atom::io::compressFile(test_file.string(),
                                          output_dir.string(), options);
    EXPECT_TRUE(result2.success);

    // Check if backup was created
    fs::path backup_file =
        output_dir / (test_file.filename().string() + ".gz.bak");
    EXPECT_TRUE(fs::exists(backup_file));
    EXPECT_TRUE(fs::exists(compressed_file));

    // Original compressed file should now be the backup
    atom::io::DecompressionOptions decomp_options;
    fs::path decomp_dir = test_dir / "backup_check";
    fs::create_directories(decomp_dir);

    auto decompress_result = atom::io::decompressFile(
        backup_file.string(), decomp_dir.string(), decomp_options);
    EXPECT_TRUE(decompress_result.success);

    fs::path decompressed_file = decomp_dir / test_file.filename();
    std::ifstream file(decompressed_file);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Backup should contain the original content, not the modified content
    EXPECT_EQ(content, test_content);
}

// Performance test comparing parallel vs sequential processing
TEST_F(FolderCompressionTest, DISABLED_CompressionPerformance) {
    // Create a larger test dataset
    for (int i = 0; i < 50; i++) {
        std::string filename = "perf_file_" + std::to_string(i) + ".txt";
        std::string content = "Performance test file " + std::to_string(i) +
                              "\n" +
                              std::string(1000 * 1024, 'X');  // 1MB per file
        fs::path file_path = source_dir / filename;
        std::ofstream file(file_path);
        file << content;
        file.close();
    }

    // Sequential compression
    fs::path seq_zip = test_dir / "sequential.zip";
    atom::io::CompressionOptions seq_options;
    seq_options.use_parallel = false;

    auto start_seq = std::chrono::high_resolution_clock::now();
    auto seq_result = atom::io::compressFolder(source_dir.string(),
                                               seq_zip.string(), seq_options);
    auto end_seq = std::chrono::high_resolution_clock::now();
    auto duration_seq = std::chrono::duration_cast<std::chrono::milliseconds>(
                            end_seq - start_seq)
                            .count();

    EXPECT_TRUE(seq_result.success);

    // Parallel compression
    fs::path par_zip = test_dir / "parallel.zip";
    atom::io::CompressionOptions par_options;
    par_options.use_parallel = true;

    auto start_par = std::chrono::high_resolution_clock::now();
    auto par_result = atom::io::compressFolder(source_dir.string(),
                                               par_zip.string(), par_options);
    auto end_par = std::chrono::high_resolution_clock::now();
    auto duration_par = std::chrono::duration_cast<std::chrono::milliseconds>(
                            end_par - start_par)
                            .count();

    EXPECT_TRUE(par_result.success);

    std::cout << "Sequential compression time: " << duration_seq << "ms"
              << std::endl;
    std::cout << "Parallel compression time: " << duration_par << "ms"
              << std::endl;
    std::cout << "Speedup: " << static_cast<double>(duration_seq) / duration_par
              << "x" << std::endl;

    // Verify both ZIP files have similar size
    auto seq_size = fs::file_size(seq_zip);
    auto par_size = fs::file_size(par_zip);

    double size_ratio = static_cast<double>(par_size) / seq_size;
    EXPECT_NEAR(size_ratio, 1.0, 0.05);  // Allow 5% difference
}
