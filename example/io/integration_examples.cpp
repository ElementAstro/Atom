/**
 * @file integration_examples.cpp
 * @brief Advanced integration examples showing real-world use cases
 *
 * This example demonstrates:
 * - Integration between different atom/io modules
 * - Real-world file processing workflows
 * - Error handling patterns and recovery
 * - Performance optimization strategies
 * - Batch processing with progress monitoring
 * - Comprehensive logging and reporting
 */

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <vector>

// Include all major atom/io modules
#include "atom/io/compression/compress.hpp"
#include "atom/io/core/glob.hpp"
#include "atom/io/core/io.hpp"
#include "atom/io/filesystem/file_info.hpp"
#include "atom/io/filesystem/file_permission.hpp"
#include "atom/io/filesystem/pushd.hpp"

namespace fs = std::filesystem;

/**
 * @brief Comprehensive file processing workflow
 *
 * This demonstrates a real-world scenario where we:
 * 1. Find files using glob patterns
 * 2. Analyze file information
 * 3. Process files based on criteria
 * 4. Create backups with compression
 * 5. Generate reports
 */
class FileProcessingWorkflow {
private:
    std::string workingDir_;
    std::vector<std::string> processedFiles_;
    std::map<std::string, size_t> statistics_;

public:
    explicit FileProcessingWorkflow(const std::string& workingDir)
        : workingDir_(workingDir) {}

    /**
     * @brief Main workflow execution
     */
    void execute() {
        std::cout << "\n🔄 Starting comprehensive file processing workflow..."
                  << std::endl;

        try {
            // Step 1: Setup and validation
            setupWorkspace();

            // Step 2: Discovery phase
            auto sourceFiles = discoverSourceFiles();

            // Step 3: Analysis phase
            auto analysisResults = analyzeFiles(sourceFiles);

            // Step 4: Processing phase
            processFiles(analysisResults);

            // Step 5: Backup and archival
            createBackups();

            // Step 6: Cleanup and reporting
            generateReport();

        } catch (const std::exception& e) {
            std::cerr << "❌ Workflow failed: " << e.what() << std::endl;
            throw;
        }
    }

private:
    void setupWorkspace() {
        std::cout << "📁 Setting up workspace..." << std::endl;

        // Create directory structure
        fs::create_directories(workingDir_ + "/source");
        fs::create_directories(workingDir_ + "/processed");
        fs::create_directories(workingDir_ + "/backup");
        fs::create_directories(workingDir_ + "/reports");

        // Create sample source files
        createSampleFiles();

        std::cout << "✅ Workspace setup completed" << std::endl;
    }

    void createSampleFiles() {
        std::vector<std::pair<std::string, std::string>> sampleFiles = {
            {"source/data1.csv", generateCSVContent(1000)},
            {"source/data2.csv", generateCSVContent(2000)},
            {"source/config.json", generateJSONContent()},
            {"source/readme.txt",
             "This is a readme file\nWith multiple lines\nFor testing "
             "purposes"},
            {"source/script.py",
             "#!/usr/bin/env python3\nprint('Hello World')\n"},
            {"source/large_data.txt",
             std::string(50000, 'A') + "\nLarge file content"}};

        for (const auto& [path, content] : sampleFiles) {
            std::string fullPath = workingDir_ + "/" + path;
            std::ofstream file(fullPath);
            file << content;
            file.close();
        }
    }

    std::string generateCSVContent(int rows) {
        std::string content = "id,name,value,timestamp\n";
        for (int i = 0; i < rows; ++i) {
            content += std::to_string(i) + ",Item" + std::to_string(i) + "," +
                       std::to_string(i * 1.5) + ",2024-01-01T00:00:00Z\n";
        }
        return content;
    }

    std::string generateJSONContent() {
        return R"({
  "name": "test_config",
  "version": "1.0.0",
  "settings": {
    "debug": true,
    "max_connections": 100,
    "timeout": 30
  },
  "features": ["logging", "compression", "async"]
})";
    }

    std::vector<fs::path> discoverSourceFiles() {
        std::cout << "🔍 Discovering source files..." << std::endl;

        // Use glob to find all files in source directory
        std::string pattern = workingDir_ + "/source/**/*";
        auto allFiles = atom::io::glob(pattern, true, false);

        // Filter for regular files only
        std::vector<fs::path> sourceFiles;
        for (const auto& file : allFiles) {
            if (fs::is_regular_file(file)) {
                sourceFiles.push_back(file);
            }
        }

        std::cout << "✅ Found " << sourceFiles.size() << " source files"
                  << std::endl;
        return sourceFiles;
    }

    struct FileAnalysis {
        fs::path filePath;
        atom::io::FileInfo info;
        std::string category;
        bool needsProcessing;
        std::string processingReason;
    };

    std::vector<FileAnalysis> analyzeFiles(const std::vector<fs::path>& files) {
        std::cout << "📊 Analyzing files..." << std::endl;

        std::vector<FileAnalysis> results;

        for (const auto& file : files) {
            try {
                FileAnalysis analysis;
                analysis.filePath = file;
                analysis.info = atom::io::getFileInfo(file);

                // Categorize file
                std::string ext = file.extension().string();
                if (ext == ".csv") {
                    analysis.category = "data";
                } else if (ext == ".json") {
                    analysis.category = "config";
                } else if (ext == ".py") {
                    analysis.category = "script";
                } else if (ext == ".txt") {
                    analysis.category = "text";
                } else {
                    analysis.category = "other";
                }

                // Determine if processing is needed
                analysis.needsProcessing = false;
                if (analysis.info.fileSize > 10000) {
                    analysis.needsProcessing = true;
                    analysis.processingReason =
                        "Large file - needs compression";
                } else if (analysis.category == "data") {
                    analysis.needsProcessing = true;
                    analysis.processingReason = "Data file - needs validation";
                } else if (analysis.category == "script") {
                    analysis.needsProcessing = true;
                    analysis.processingReason =
                        "Script file - needs permission check";
                }

                results.push_back(analysis);

            } catch (const std::exception& e) {
                std::cerr << "⚠️  Failed to analyze " << file << ": " << e.what()
                          << std::endl;
            }
        }

        std::cout << "✅ Analyzed " << results.size() << " files" << std::endl;
        return results;
    }

    void processFiles(const std::vector<FileAnalysis>& analyses) {
        std::cout << "⚙️  Processing files..." << std::endl;

        for (const auto& analysis : analyses) {
            if (!analysis.needsProcessing) {
                continue;
            }

            std::cout << "Processing " << analysis.filePath.filename() << " ("
                      << analysis.processingReason << ")" << std::endl;

            try {
                if (analysis.category == "data") {
                    processDataFile(analysis);
                } else if (analysis.category == "script") {
                    processScriptFile(analysis);
                } else if (analysis.info.fileSize > 10000) {
                    processLargeFile(analysis);
                }

                processedFiles_.push_back(analysis.filePath.string());
                statistics_[analysis.category]++;

            } catch (const std::exception& e) {
                std::cerr << "❌ Failed to process " << analysis.filePath
                          << ": " << e.what() << std::endl;
            }
        }

        std::cout << "✅ Processing completed" << std::endl;
    }

    void processDataFile(const FileAnalysis& analysis) {
        // Validate data file and create processed version
        std::string outputPath =
            workingDir_ + "/processed/" + analysis.filePath.filename().string();

        // Read, validate, and write processed data
        std::ifstream input(analysis.filePath);
        std::ofstream output(outputPath);

        std::string line;
        int lineCount = 0;
        while (std::getline(input, line)) {
            // Simple validation - ensure line is not empty
            if (!line.empty()) {
                output << line << std::endl;
                lineCount++;
            }
        }

        std::cout << "  ✅ Validated " << lineCount << " lines" << std::endl;
    }

    void processScriptFile(const FileAnalysis& analysis) {
        // Check and set appropriate permissions for script files
        try {
            auto currentPerms =
                atom::io::getFilePermissions(analysis.filePath.string());
            std::cout << "  Current permissions: " << currentPerms << std::endl;

            // Set executable permissions
            atom::io::changeFilePermissions(analysis.filePath.string(),
                                            "rwxr-xr-x");
            std::cout << "  ✅ Set executable permissions" << std::endl;

        } catch (const std::exception& e) {
            std::cout << "  ⚠️  Could not modify permissions: " << e.what()
                      << std::endl;
        }
    }

    void processLargeFile(const FileAnalysis& analysis) {
        // Compress large files
        std::string outputPath = workingDir_ + "/processed/" +
                                 analysis.filePath.filename().string() + ".gz";

        atom::io::CompressionOptions options;
        options.level = 6;

        auto result = atom::io::compressFile(
            analysis.filePath.string(), workingDir_ + "/processed", options);

        if (result.success) {
            double ratio = result.compression_ratio * 100;
            std::cout << "  ✅ Compressed to " << ratio << "% of original size"
                      << std::endl;
        } else {
            std::cout << "  ❌ Compression failed: " << result.error_message
                      << std::endl;
        }
    }

    void createBackups() {
        std::cout << "💾 Creating backups..." << std::endl;

        // Create timestamped backup
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        std::string timestamp = std::to_string(time_t);
        std::string backupPath =
            workingDir_ + "/backup/backup_" + timestamp + ".zip";

        // Compress entire processed directory
        auto result =
            atom::io::compressFolder(workingDir_ + "/processed", backupPath);

        if (result.success) {
            auto backupSize = fs::file_size(backupPath);
            std::cout << "✅ Backup created: " << backupPath << " ("
                      << (backupSize / 1024) << " KB)" << std::endl;
        } else {
            std::cout << "❌ Backup failed: " << result.error_message
                      << std::endl;
        }
    }

    void generateReport() {
        std::cout << "📋 Generating report..." << std::endl;

        std::string reportPath = workingDir_ + "/reports/processing_report.txt";
        std::ofstream report(reportPath);

        report << "File Processing Report\n";
        report << "=====================\n\n";

        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        report << "Generated: " << std::ctime(&time_t) << "\n";

        report << "Processing Statistics:\n";
        report << "---------------------\n";
        for (const auto& [category, count] : statistics_) {
            report << category << ": " << count << " files\n";
        }

        report << "\nProcessed Files:\n";
        report << "---------------\n";
        for (const auto& file : processedFiles_) {
            report << "- " << file << "\n";
        }

        report.close();

        std::cout << "✅ Report generated: " << reportPath << std::endl;
    }
};

/**
 * @brief Demonstrates error handling and recovery patterns
 */
void demonstrateErrorHandling() {
    std::cout << "\n🛡️  Error Handling and Recovery Patterns" << std::endl;
    std::cout << "=========================================" << std::endl;

    // Test various error scenarios and recovery strategies
    std::vector<std::function<void()>> errorTests = {
        []() {
            std::cout << "\n1. Testing file not found error..." << std::endl;
            try {
                auto info = atom::io::getFileInfo("non_existent_file.txt");
            } catch (const std::runtime_error& e) {
                std::cout << "✅ Caught expected error: " << e.what()
                          << std::endl;
            }
        },

        []() {
            std::cout << "\n2. Testing permission error..." << std::endl;
            try {
                atom::io::changeFilePermissions("non_existent_file.txt",
                                                "rwxrwxrwx");
            } catch (const std::exception& e) {
                std::cout << "✅ Caught permission error: " << e.what()
                          << std::endl;
            }
        },

        []() {
            std::cout << "\n3. Testing compression error..." << std::endl;
            auto result = atom::io::compressFile("non_existent_file.txt", ".");
            if (!result.success) {
                std::cout << "✅ Graceful compression failure: "
                          << result.error_message << std::endl;
            }
        }};

    for (auto& test : errorTests) {
        try {
            test();
        } catch (const std::exception& e) {
            std::cout << "✅ Caught unexpected error: " << e.what()
                      << std::endl;
        }
    }
}

/**
 * @brief Demonstrates performance optimization strategies
 */
void demonstratePerformanceOptimization() {
    std::cout << "\n⚡ Performance Optimization Strategies" << std::endl;
    std::cout << "=====================================" << std::endl;

    const std::string testDir = "perf_optimization_test";
    fs::create_directories(testDir);

    // Create test files
    for (int i = 0; i < 100; ++i) {
        std::string filename = testDir + "/file" + std::to_string(i) + ".txt";
        std::ofstream file(filename);
        file << "Test content for file " << i << std::endl;
        file.close();
    }

    // Strategy 1: Batch operations
    std::cout << "\n1. Batch vs Individual Operations:" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    auto allFiles = atom::io::glob(testDir + "/*.txt", false, false);
    auto end = std::chrono::high_resolution_clock::now();
    auto batchTime =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Batch glob: " << allFiles.size() << " files in "
              << batchTime.count() << "μs" << std::endl;

    // Strategy 2: Parallel processing
    std::cout << "\n2. Parallel File Processing:" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    std::vector<std::future<size_t>> futures;

    for (const auto& file : allFiles) {
        futures.push_back(std::async(std::launch::async, [file]() {
            return atom::io::getFileSize(file);
        }));
    }

    size_t totalSize = 0;
    for (auto& future : futures) {
        totalSize += future.get();
    }

    end = std::chrono::high_resolution_clock::now();
    auto parallelTime =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Parallel processing: " << totalSize << " total bytes in "
              << parallelTime.count() << "μs" << std::endl;

    // Cleanup
    fs::remove_all(testDir);
}

/**
 * @brief Cleanup function
 */
void cleanup() {
    std::cout << "\n🧹 Cleaning up integration test files..." << std::endl;

    if (fs::exists("integration_workflow")) {
        fs::remove_all("integration_workflow");
    }

    std::cout << "✅ Cleanup completed" << std::endl;
}

int main() {
    try {
        std::cout << "🔗 Atom I/O Integration Examples" << std::endl;
        std::cout << "================================" << std::endl;

        // Run comprehensive workflow
        FileProcessingWorkflow workflow("integration_workflow");
        workflow.execute();

        // Demonstrate error handling patterns
        demonstrateErrorHandling();

        // Demonstrate performance optimization
        demonstratePerformanceOptimization();

        // Cleanup
        cleanup();

        std::cout << "\n🎉 All integration examples completed successfully!"
                  << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal exception: " << e.what() << std::endl;
        cleanup();
        return 1;
    }
}
