#include <gtest/gtest.h>
#include "async_compress.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <future>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <minizip-ng/mz_compat.h>

namespace fs = std::filesystem;

class AsyncCompressTest : public ::testing::Test {
protected:
    asio::io_context io_context;
    std::unique_ptr<asio::io_context::work> work;
    std::thread io_thread;
    fs::path test_dir;

    AsyncCompressTest() {
        // Keep the io_context running
        work = std::make_unique<asio::io_context::work>(io_context);
        io_thread = std::thread([this]() { io_context.run(); });
    }

    void SetUp() override {
        // Create temporary test directory
        test_dir = fs::temp_directory_path() / "atom_async_compress_test";
        if (fs::exists(test_dir)) {
            fs::remove_all(test_dir);
        }
        fs::create_directories(test_dir);
    }

    void TearDown() override {
        // Clean up test directory
        try {
            if (fs::exists(test_dir)) {
                fs::remove_all(test_dir);
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to clean up test directory: " << e.what()
                      << std::endl;
        }
    }

    ~AsyncCompressTest() override {
        // Stop the io_context and join the thread
        work.reset();
        if (io_thread.joinable()) {
            io_thread.join();
        }
    }

    // Helper function to create a test file with given content
    fs::path createTestFile(const std::string& name,
                            const std::string& content) {
        fs::path file_path = test_dir / name;
        std::ofstream file(file_path);
        file << content;
        file.close();
        return file_path;
    }

    // Helper function to create a directory structure with files
    void createTestDirectory(const fs::path& dir_path, int num_files,
                             int num_subdirs = 0, int files_per_subdir = 0) {
        fs::create_directories(dir_path);

        // Create files in the main directory
        for (int i = 0; i < num_files; i++) {
            fs::path file_path =
                dir_path / ("file_" + std::to_string(i) + ".txt");
            std::ofstream file(file_path);
            file << "Content for file " << i << " in main directory";
            file.close();
        }

        // Create subdirectories with files if requested
        for (int i = 0; i < num_subdirs; i++) {
            fs::path subdir_path = dir_path / ("subdir_" + std::to_string(i));
            fs::create_directories(subdir_path);

            for (int j = 0; j < files_per_subdir; j++) {
                fs::path file_path =
                    subdir_path / ("subfile_" + std::to_string(j) + ".txt");
                std::ofstream file(file_path);
                file << "Content for file " << j << " in subdirectory " << i;
                file.close();
            }
        }
    }

    // Helper function to read a file synchronously
    std::string readFileSync(const fs::path& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return "";
        }

        auto size = file.tellg();
        file.seekg(0);
        std::string content(static_cast<size_t>(size), '\0');
        file.read(content.data(), size);
        return content;
    }

    // Generate random content
    std::string generateRandomContent(size_t size) {
        static std::mt19937 rng(std::random_device{}());
        static std::uniform_int_distribution<char> dist(
            32, 126);  // ASCII printable chars

        std::string content(size, 0);
        std::generate(content.begin(), content.end(),
                      [&]() { return dist(rng); });
        return content;
    }

    // Helper to run asynchronous operations and wait for completion
    template <typename Func>
    void runAsyncOperation(Func&& func) {
        std::promise<void> done_promise;
        std::future<void> done_future = done_promise.get_future();

        // Run the operation with a completion callback
        std::invoke(std::forward<Func>(func),
                    [&done_promise]() { done_promise.set_value(); });

        // Wait for completion or timeout
        if (done_future.wait_for(std::chrono::seconds(10)) ==
            std::future_status::timeout) {
            FAIL() << "Async operation timed out";
        }
    }

    // Helper to decompress a gzip file
    std::string decompressGzFile(const fs::path& compressed_file) {
        gzFile file = gzopen(compressed_file.string().c_str(), "rb");
        if (!file) {
            return "";
        }

        std::string result;
        std::array<char, 4096> buffer;
        int bytes_read;

        while ((bytes_read = gzread(file, buffer.data(), buffer.size())) > 0) {
            result.append(buffer.data(), bytes_read);
        }

        gzclose(file);
        return result;
    }

    // Helper to create a ZIP file with test files
    fs::path createTestZipFile(
        const std::string& zip_name,
        const std::vector<std::pair<std::string, std::string>>& files) {
        fs::path zip_path = test_dir / zip_name;

        zipFile zf = zipOpen(zip_path.string().c_str(), APPEND_STATUS_CREATE);
        if (!zf) {
            return "";
        }

        for (const auto& [filename, content] : files) {
            zip_fileinfo zi = {};

            if (zipOpenNewFileInZip(zf, filename.c_str(), &zi, nullptr, 0,
                                    nullptr, 0, nullptr, Z_DEFLATED,
                                    Z_DEFAULT_COMPRESSION) != ZIP_OK) {
                zipClose(zf, nullptr);
                return "";
            }

            if (zipWriteInFileInZip(zf, content.c_str(), content.size()) !=
                ZIP_OK) {
                zipCloseFileInZip(zf);
                zipClose(zf, nullptr);
                return "";
            }

            zipCloseFileInZip(zf);
        }

        zipClose(zf, nullptr);
        return zip_path;
    }
};

// Test SingleFileCompressor
TEST_F(AsyncCompressTest, SingleFileCompression) {
    const std::string content = "This is test content for compression.";
    fs::path input_file = createTestFile("input.txt", content);
    fs::path output_file = test_dir / "output.gz";

    // Create compressor
    atom::async::io::SingleFileCompressor compressor(io_context, input_file,
                                                     output_file);

    // Run compression
    std::promise<void> done;
    std::future<void> future = done.get_future();

    compressor.start();

    // Wait for a reasonable time for the compression to finish
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Verify output file exists
    ASSERT_TRUE(fs::exists(output_file));
    ASSERT_GT(fs::file_size(output_file), 0);

    // Decompress the file and check content
    std::string decompressed = decompressGzFile(output_file);
    EXPECT_EQ(decompressed, content);
}

// Test SingleFileCompressor with larger file
TEST_F(AsyncCompressTest, LargeFileCompression) {
    const std::string content = generateRandomContent(1024 * 1024);  // 1MB
    fs::path input_file = createTestFile("large_input.txt", content);
    fs::path output_file = test_dir / "large_output.gz";

    // Create compressor
    atom::async::io::SingleFileCompressor compressor(io_context, input_file,
                                                     output_file);

    // Run compression
    compressor.start();

    // Wait for a reasonable time for the compression to finish
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Verify output file exists and is smaller than original (compression
    // worked)
    ASSERT_TRUE(fs::exists(output_file));
    ASSERT_GT(fs::file_size(output_file), 0);
    // Compression should generally reduce file size for random data
    EXPECT_LT(fs::file_size(output_file), content.size());

    // Decompress and compare with original
    std::string decompressed = decompressGzFile(output_file);
    EXPECT_EQ(decompressed, content);
}

// Test DirectoryCompressor
TEST_F(AsyncCompressTest, DirectoryCompression) {
    fs::path input_dir = test_dir / "test_dir";
    fs::path output_file = test_dir / "dir_output.gz";

    // Create test directory with files
    createTestDirectory(input_dir, 5, 2, 3);

    // Create directory compressor
    atom::async::io::DirectoryCompressor compressor(io_context, input_dir,
                                                    output_file);

    // Run compression
    compressor.start();

    // Wait for a reasonable time for the compression to finish
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Verify output file exists
    ASSERT_TRUE(fs::exists(output_file));
    ASSERT_GT(fs::file_size(output_file), 0);
}

// Test SingleFileDecompressor
TEST_F(AsyncCompressTest, SingleFileDecompression) {
    const std::string content = "This is test content for decompression.";
    fs::path input_file = test_dir / "input.txt";
    fs::path compressed_file = test_dir / "compressed.gz";
    fs::path output_dir = test_dir / "output_dir";

    // Create a file and compress it manually for testing
    {
        std::ofstream file(input_file);
        file << content;
        file.close();

        gzFile out = gzopen(compressed_file.string().c_str(), "wb");
        ASSERT_NE(out, nullptr);

        gzwrite(out, content.c_str(),
                static_cast<unsigned int>(content.size()));
        gzclose(out);
    }

    // Create decompressor
    atom::async::io::SingleFileDecompressor decompressor(
        io_context, compressed_file, output_dir);

    // Run decompression
    decompressor.start();

    // Wait for a reasonable time for the decompression to finish
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Verify output file exists
    fs::path expected_output = output_dir / "compressed.out";
    ASSERT_TRUE(fs::exists(expected_output));

    // Check content
    std::string decompressed = readFileSync(expected_output);
    EXPECT_EQ(decompressed, content);
}

// Test DirectoryDecompressor
TEST_F(AsyncCompressTest, DirectoryDecompression) {
    fs::path compressed_dir = test_dir / "compressed";
    fs::path output_dir = test_dir / "decompressed";
    fs::create_directories(compressed_dir);

    // Create a few compressed files
    const std::vector<std::pair<std::string, std::string>> test_files = {
        {"file1.txt", "Content for file 1"},
        {"file2.txt", "Content for file 2"}};

    for (const auto& [filename, content] : test_files) {
        fs::path compressed_file = compressed_dir / (filename + ".gz");
        gzFile out = gzopen(compressed_file.string().c_str(), "wb");
        ASSERT_NE(out, nullptr);

        gzwrite(out, content.c_str(),
                static_cast<unsigned int>(content.size()));
        gzclose(out);
    }

    // Create decompressor
    atom::async::io::DirectoryDecompressor decompressor(
        io_context, compressed_dir, output_dir);

    // Run decompression
    decompressor.start();

    // Wait for a reasonable time for the decompression to finish
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Verify output files exist
    for (const auto& [filename, content] : test_files) {
        fs::path output_file = output_dir / (filename + ".out");
        ASSERT_TRUE(fs::exists(output_file))
            << "Output file doesn't exist: " << output_file.string();

        // Check content
        std::string decompressed = readFileSync(output_file);
        EXPECT_EQ(decompressed, content);
    }
}

// Test ListFilesInZip
TEST_F(AsyncCompressTest, ListFilesInZip) {
    // Create test files for the ZIP
    const std::vector<std::pair<std::string, std::string>> test_files = {
        {"file1.txt", "Content for file 1"},
        {"file2.txt", "Content for file 2"},
        {"subdir/file3.txt", "Content in subdirectory"}};

    // Create the ZIP file
    fs::path zip_path = createTestZipFile("test.zip", test_files);
    ASSERT_FALSE(zip_path.empty());

    // Create list files operation
    atom::async::io::ListFilesInZip list_op(io_context, zip_path.string());

    // Run operation
    list_op.start();

    // Wait for a reasonable time for completion
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Get and verify file list
    std::vector<std::string> files = list_op.getFileList();
    ASSERT_EQ(files.size(), test_files.size());

    // Sort both lists for comparison
    std::vector<std::string> expected_files;
    for (const auto& [filename, _] : test_files) {
        expected_files.push_back(filename);
    }

    std::sort(files.begin(), files.end());
    std::sort(expected_files.begin(), expected_files.end());

    EXPECT_EQ(files, expected_files);
}

// Test FileExistsInZip
TEST_F(AsyncCompressTest, FileExistsInZip) {
    // Create test files for the ZIP
    const std::vector<std::pair<std::string, std::string>> test_files = {
        {"file1.txt", "Content for file 1"},
        {"file2.txt", "Content for file 2"}};

    // Create the ZIP file
    fs::path zip_path = createTestZipFile("test_exists.zip", test_files);
    ASSERT_FALSE(zip_path.empty());

    // Test with existing file
    {
        atom::async::io::FileExistsInZip exists_op(
            io_context, zip_path.string(), "file1.txt");
        exists_op.start();

        // Wait for completion
        std::this_thread::sleep_for(std::chrono::seconds(2));

        EXPECT_TRUE(exists_op.found());
    }

    // Test with non-existing file
    {
        atom::async::io::FileExistsInZip exists_op(
            io_context, zip_path.string(), "nonexistent.txt");
        exists_op.start();

        // Wait for completion
        std::this_thread::sleep_for(std::chrono::seconds(2));

        EXPECT_FALSE(exists_op.found());
    }
}

// Test RemoveFileFromZip
TEST_F(AsyncCompressTest, RemoveFileFromZip) {
    // Create test files for the ZIP
    const std::vector<std::pair<std::string, std::string>> test_files = {
        {"file1.txt", "Content for file 1"},
        {"file2.txt", "Content for file 2"},
        {"file3.txt", "Content for file 3"}};

    // Create the ZIP file
    fs::path zip_path = createTestZipFile("test_remove.zip", test_files);
    ASSERT_FALSE(zip_path.empty());

    // Create remove operation
    atom::async::io::RemoveFileFromZip remove_op(io_context, zip_path.string(),
                                                 "file2.txt");

    // Run operation
    remove_op.start();

    // Wait for completion
    std::this_thread::sleep_for(std::chrono::seconds(2));

    EXPECT_TRUE(remove_op.isSuccessful());

    // Verify the file was removed by checking the file list
    atom::async::io::ListFilesInZip list_op(io_context, zip_path.string());
    list_op.start();

    // Wait for completion
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::vector<std::string> files = list_op.getFileList();
    ASSERT_EQ(files.size(), 2);

    // Check that file2.txt is not in the list
    auto it = std::find(files.begin(), files.end(), "file2.txt");
    EXPECT_EQ(it, files.end());
}

// Test GetZipFileSize
TEST_F(AsyncCompressTest, GetZipFileSize) {
    // Create test files for the ZIP
    const std::vector<std::pair<std::string, std::string>> test_files = {
        {"file1.txt", "Content for file 1"},
        {"file2.txt", "Content for file 2"}};

    // Create the ZIP file
    fs::path zip_path = createTestZipFile("test_size.zip", test_files);
    ASSERT_FALSE(zip_path.empty());

    // Get file size using filesystem
    std::uintmax_t expected_size = fs::file_size(zip_path);

    // Create size operation
    atom::async::io::GetZipFileSize size_op(io_context, zip_path.string());

    // Run operation
    size_op.start();

    // Wait for completion
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Check size
    EXPECT_EQ(size_op.getSizeValue(), expected_size);
}

// Test error conditions for SingleFileCompressor
TEST_F(AsyncCompressTest, SingleFileCompressorErrors) {
    fs::path nonexistent_file = test_dir / "nonexistent.txt";
    fs::path output_file = test_dir / "output.gz";

    // Test with non-existent input file
    EXPECT_THROW(atom::async::io::SingleFileCompressor compressor(
                     io_context, nonexistent_file, output_file),
                 std::invalid_argument);

    // Test with empty output path
    fs::path input_file = createTestFile("input.txt", "Test content");
    EXPECT_THROW(atom::async::io::SingleFileCompressor compressor(
                     io_context, input_file, ""),
                 std::invalid_argument);
}

// Test error conditions for DirectoryCompressor
TEST_F(AsyncCompressTest, DirectoryCompressorErrors) {
    fs::path nonexistent_dir = test_dir / "nonexistent_dir";
    fs::path output_file = test_dir / "output.gz";

    // Test with non-existent input directory
    EXPECT_THROW(atom::async::io::DirectoryCompressor compressor(
                     io_context, nonexistent_dir, output_file),
                 std::invalid_argument);

    // Test with file as input directory
    fs::path input_file = createTestFile("input.txt", "Test content");
    EXPECT_THROW(atom::async::io::DirectoryCompressor compressor(
                     io_context, input_file, output_file),
                 std::invalid_argument);

    // Test with empty output path
    fs::path input_dir = test_dir / "input_dir";
    fs::create_directories(input_dir);
    EXPECT_THROW(atom::async::io::DirectoryCompressor compressor(io_context,
                                                                 input_dir, ""),
                 std::invalid_argument);
}

// Test SingleFileCompressor with empty file
TEST_F(AsyncCompressTest, EmptyFileCompression) {
    fs::path input_file = createTestFile("empty.txt", "");
    fs::path output_file = test_dir / "empty.gz";

    // Create compressor
    atom::async::io::SingleFileCompressor compressor(io_context, input_file,
                                                     output_file);

    // Run compression
    compressor.start();

    // Wait for a reasonable time for the compression to finish
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Verify output file exists
    ASSERT_TRUE(fs::exists(output_file));
}

// Test parallel compression capabilities
TEST_F(AsyncCompressTest, ParallelCompression) {
    // Create multiple compressors for different files
    const int num_files = 5;
    std::vector<fs::path> input_files;
    std::vector<fs::path> output_files;
    std::vector<std::unique_ptr<atom::async::io::SingleFileCompressor>>
        compressors;

    for (int i = 0; i < num_files; i++) {
        std::string content = "Content for file " + std::to_string(i);
        input_files.push_back(
            createTestFile("input" + std::to_string(i) + ".txt", content));
        output_files.push_back(test_dir /
                               ("output" + std::to_string(i) + ".gz"));

        compressors.push_back(
            std::make_unique<atom::async::io::SingleFileCompressor>(
                io_context, input_files.back(), output_files.back()));
    }

    // Start all compressors
    for (auto& compressor : compressors) {
        compressor->start();
    }

    // Wait for all to complete
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Verify all output files exist
    for (const auto& output : output_files) {
        ASSERT_TRUE(fs::exists(output));
        ASSERT_GT(fs::file_size(output), 0);
    }
}

// Test DirectoryCompressor with complex directory structure
TEST_F(AsyncCompressTest, ComplexDirectoryCompression) {
    fs::path input_dir = test_dir / "complex_dir";
    fs::path output_file = test_dir / "complex_dir.gz";

    // Create a complex directory structure
    createTestDirectory(input_dir, 10, 5, 5);

    // Add some empty subdirectories
    for (int i = 0; i < 3; i++) {
        fs::create_directories(input_dir / ("empty_dir_" + std::to_string(i)));
    }

    // Add a file with special characters in its name
    fs::path special_file = input_dir / "special!@#$%.txt";
    std::ofstream file(special_file);
    file << "File with special characters";
    file.close();

    // Create compressor
    atom::async::io::DirectoryCompressor compressor(io_context, input_dir,
                                                    output_file);

    // Run compression
    compressor.start();

    // Wait for a reasonable time for the compression to finish
    std::this_thread::sleep_for(std::chrono::seconds(5));

    // Verify output file exists
    ASSERT_TRUE(fs::exists(output_file));
    ASSERT_GT(fs::file_size(output_file), 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}