/**
 * @file streaming_io_demo.cpp
 * @brief Streaming I/O operations demonstration
 *
 * This example demonstrates:
 * - Streaming large image files
 * - Progressive loading and processing
 * - Memory-efficient I/O operations
 * - Tiled/chunked processing
 * - Streaming compression and decompression
 * - Network streaming capabilities
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/io/progressive_loader.hpp"
#include "atom/image/io/streaming_reader.hpp"
#include "atom/image/io/streaming_writer.hpp"
#include "atom/image/io/tiled_processor.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Create a large test image for streaming demonstrations
 */
void createLargeTestImage(const std::string& filename, int width, int height) {
    std::cout << "Creating large test image: " << filename << " (" << width
              << "x" << height << ")\n";

    try {
        StreamingWriter writer(filename);
        writer.setImageProperties(width, height, 3, ImageFormat::TIFF);
        writer.setCompressionLevel(5);  // Moderate compression

        const int tileHeight = 64;  // Process in strips
        std::vector<uint8_t> tileData(width * tileHeight * 3);

        auto start = steady_clock::now();

        for (int y = 0; y < height; y += tileHeight) {
            int currentTileHeight = std::min(tileHeight, height - y);

            // Generate test pattern for this tile
            for (int ty = 0; ty < currentTileHeight; ++ty) {
                for (int x = 0; x < width; ++x) {
                    int globalY = y + ty;
                    int index = (ty * width + x) * 3;

                    // Create a complex pattern
                    uint8_t r = static_cast<uint8_t>((x + globalY) % 256);
                    uint8_t g = static_cast<uint8_t>((x * 2 + globalY) % 256);
                    uint8_t b = static_cast<uint8_t>((x + globalY * 2) % 256);

                    tileData[index] = r;
                    tileData[index + 1] = g;
                    tileData[index + 2] = b;
                }
            }

            // Write tile
            writer.writeTile(tileData.data(), width * currentTileHeight * 3, 0,
                             y, width, currentTileHeight);

            if ((y / tileHeight + 1) % 50 == 0) {
                std::cout << "  Written " << (y / tileHeight + 1) << " tiles\n";
            }
        }

        writer.finalize();

        auto duration =
            duration_cast<milliseconds>(steady_clock::now() - start);

        // Get file size
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        size_t fileSize = file.tellg();
        file.close();

        std::cout << "  Creation time: " << duration.count() << "ms\n";
        std::cout << "  File size: " << (fileSize / (1024 * 1024)) << " MB\n";
        std::cout << "  Write speed: "
                  << (fileSize / (1024.0 * 1024.0)) /
                         (duration.count() / 1000.0)
                  << " MB/s\n";

    } catch (const std::exception& e) {
        std::cerr << "Error creating test image: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate streaming read operations
 */
void demonstrateStreamingRead() {
    std::cout << "\n=== Streaming Read Operations ===\n";

    const std::string filename = "large_test_image.tiff";

    try {
        StreamingReader reader(filename);

        if (!reader.isValid()) {
            std::cout << "Test image not found, creating it first...\n";
            createLargeTestImage(filename, 2048, 1536);
            reader.open(filename);
        }

        auto properties = reader.getImageProperties();
        std::cout << "Image properties:\n";
        std::cout << "  Dimensions: " << properties.width << "x"
                  << properties.height << "\n";
        std::cout << "  Channels: " << properties.channels << "\n";
        std::cout << "  Format: " << static_cast<int>(properties.format)
                  << "\n";
        std::cout << "  Compressed: "
                  << (properties.isCompressed ? "Yes" : "No") << "\n";
        std::cout << "  Tile size: " << properties.tileWidth << "x"
                  << properties.tileHeight << "\n";

        // Test 1: Sequential tile reading
        std::cout << "\nTest 1: Sequential tile reading\n";

        const int tilesX = (properties.width + properties.tileWidth - 1) /
                           properties.tileWidth;
        const int tilesY = (properties.height + properties.tileHeight - 1) /
                           properties.tileHeight;

        auto start = steady_clock::now();
        size_t totalBytesRead = 0;

        for (int ty = 0; ty < tilesY; ++ty) {
            for (int tx = 0; tx < tilesX; ++tx) {
                auto tileData = reader.readTile(tx, ty);
                if (tileData) {
                    totalBytesRead += tileData->size();
                }
            }
        }

        auto duration =
            duration_cast<milliseconds>(steady_clock::now() - start);

        std::cout << "  Read " << (tilesX * tilesY) << " tiles in "
                  << duration.count() << "ms\n";
        std::cout << "  Total data: " << (totalBytesRead / (1024 * 1024))
                  << " MB\n";
        std::cout << "  Read speed: "
                  << (totalBytesRead / (1024.0 * 1024.0)) /
                         (duration.count() / 1000.0)
                  << " MB/s\n";

        // Test 2: Random access reading
        std::cout << "\nTest 2: Random access reading\n";

        std::vector<std::pair<int, int>> randomTiles;
        for (int i = 0; i < 20; ++i) {
            int tx = rand() % tilesX;
            int ty = rand() % tilesY;
            randomTiles.push_back({tx, ty});
        }

        start = steady_clock::now();

        for (const auto& [tx, ty] : randomTiles) {
            auto tileData = reader.readTile(tx, ty);
            if (tileData) {
                // Simulate some processing
                uint8_t checksum = 0;
                for (size_t i = 0; i < std::min(size_t(1000), tileData->size());
                     ++i) {
                    checksum ^= (*tileData)[i];
                }
            }
        }

        duration = duration_cast<milliseconds>(steady_clock::now() - start);

        std::cout << "  Random access to " << randomTiles.size() << " tiles in "
                  << duration.count() << "ms\n";
        std::cout << "  Average time per tile: "
                  << (duration.count() / randomTiles.size()) << "ms\n";

        // Test 3: Region of interest reading
        std::cout << "\nTest 3: Region of interest reading\n";

        int roiX = properties.width / 4;
        int roiY = properties.height / 4;
        int roiWidth = properties.width / 2;
        int roiHeight = properties.height / 2;

        start = steady_clock::now();

        auto roiData = reader.readRegion(roiX, roiY, roiWidth, roiHeight);

        duration = duration_cast<milliseconds>(steady_clock::now() - start);

        if (roiData) {
            std::cout << "  ROI (" << roiX << "," << roiY << " " << roiWidth
                      << "x" << roiHeight << ") read in " << duration.count()
                      << "ms\n";
            std::cout << "  ROI data size: "
                      << (roiData->size() / (1024 * 1024)) << " MB\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in streaming read: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate progressive loading
 */
void demonstrateProgressiveLoading() {
    std::cout << "\n=== Progressive Loading ===\n";

    try {
        const std::string filename = "large_test_image.tiff";

        ProgressiveLoader loader(filename);

        if (!loader.isValid()) {
            std::cout << "Test image not available for progressive loading\n";
            return;
        }

        auto properties = loader.getImageProperties();
        std::cout << "Progressive loading of " << properties.width << "x"
                  << properties.height << " image\n";

        // Set up progressive loading parameters
        loader.setProgressiveLevels(4);  // 4 levels of detail
        loader.setTileSize(128, 128);

        // Simulate progressive loading for different quality levels
        std::vector<std::string> qualityNames = {"Thumbnail", "Low", "Medium",
                                                 "High"};

        for (int level = 0; level < 4; ++level) {
            std::cout << "\nLoading " << qualityNames[level]
                      << " quality (level " << level << "):\n";

            auto start = steady_clock::now();

            auto progressiveImage = loader.loadLevel(level);

            auto duration =
                duration_cast<milliseconds>(steady_clock::now() - start);

            if (progressiveImage) {
                auto levelProps = loader.getLevelProperties(level);
                std::cout << "  Dimensions: " << levelProps.width << "x"
                          << levelProps.height << "\n";
                std::cout << "  Data size: "
                          << (progressiveImage->size() / 1024) << " KB\n";
                std::cout << "  Load time: " << duration.count() << "ms\n";

                // Calculate compression ratio
                size_t fullSize =
                    properties.width * properties.height * properties.channels;
                double compressionRatio =
                    static_cast<double>(fullSize) / progressiveImage->size();
                std::cout << "  Compression ratio: " << std::fixed
                          << std::setprecision(1) << compressionRatio << ":1\n";
            }
        }

        // Test incremental loading
        std::cout << "\nTesting incremental loading:\n";

        loader.reset();

        auto start = steady_clock::now();

        // Load progressively with callbacks
        loader.setProgressCallback([](int level, double progress) {
            std::cout << "  Level " << level << ": " << std::fixed
                      << std::setprecision(1) << (progress * 100) << "%\n";
        });

        auto finalImage = loader.loadProgressively();

        auto duration =
            duration_cast<milliseconds>(steady_clock::now() - start);

        if (finalImage) {
            std::cout << "  Total progressive loading time: "
                      << duration.count() << "ms\n";
            std::cout << "  Final image size: "
                      << (finalImage->size() / (1024 * 1024)) << " MB\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in progressive loading: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate tiled processing
 */
void demonstrateTiledProcessing() {
    std::cout << "\n=== Tiled Processing ===\n";

    try {
        const std::string inputFile = "large_test_image.tiff";
        const std::string outputFile = "processed_large_image.tiff";

        TiledProcessor processor;
        processor.setTileSize(256, 256);
        processor.setOverlapSize(
            16);  // 16-pixel overlap for seamless processing
        processor.setMaxMemoryUsage(512 * 1024 * 1024);  // 512 MB memory limit

        // Set up processing function (simple blur filter)
        processor.setProcessingFunction(
            [](const std::vector<uint8_t>& input, int width, int height,
               int channels) -> std::vector<uint8_t> {
                std::vector<uint8_t> output = input;

                // Simple 3x3 blur kernel
                std::vector<float> kernel = {1 / 9.0f, 1 / 9.0f, 1 / 9.0f,
                                             1 / 9.0f, 1 / 9.0f, 1 / 9.0f,
                                             1 / 9.0f, 1 / 9.0f, 1 / 9.0f};

                // Apply convolution (simplified)
                for (int y = 1; y < height - 1; ++y) {
                    for (int x = 1; x < width - 1; ++x) {
                        for (int c = 0; c < channels; ++c) {
                            float sum = 0.0f;
                            for (int ky = -1; ky <= 1; ++ky) {
                                for (int kx = -1; kx <= 1; ++kx) {
                                    int idx = ((y + ky) * width + (x + kx)) *
                                                  channels +
                                              c;
                                    sum += input[idx] *
                                           kernel[(ky + 1) * 3 + (kx + 1)];
                                }
                            }
                            int outIdx = (y * width + x) * channels + c;
                            output[outIdx] = static_cast<uint8_t>(
                                std::clamp(sum, 0.0f, 255.0f));
                        }
                    }
                }

                return output;
            });

        std::cout << "Processing large image with tiled approach...\n";

        // Set up progress tracking
        std::atomic<int> tilesProcessed{0};
        std::atomic<int> totalTiles{0};

        processor.setProgressCallback([&](int processed, int total) {
            tilesProcessed = processed;
            totalTiles = total;
        });

        // Start processing in background thread
        std::thread processingThread([&]() {
            try {
                processor.processFile(inputFile, outputFile);
            } catch (const std::exception& e) {
                std::cerr << "Processing error: " << e.what() << "\n";
            }
        });

        // Monitor progress
        auto start = steady_clock::now();

        while (processingThread.joinable()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            int processed = tilesProcessed.load();
            int total = totalTiles.load();

            if (total > 0) {
                double progress =
                    static_cast<double>(processed) / total * 100.0;
                auto elapsed =
                    duration_cast<seconds>(steady_clock::now() - start);

                std::cout << "  Progress: " << std::fixed
                          << std::setprecision(1) << progress << "% "
                          << "(" << processed << "/" << total << " tiles) "
                          << "Elapsed: " << elapsed.count() << "s\n";
            }

            // Check if thread is still running
            if (processingThread.joinable()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (processingThread.joinable()) {
                    continue;
                } else {
                    break;
                }
            }
        }

        if (processingThread.joinable()) {
            processingThread.join();
        }

        auto totalDuration =
            duration_cast<seconds>(steady_clock::now() - start);

        std::cout << "  Tiled processing completed in " << totalDuration.count()
                  << "s\n";

        // Verify output file
        std::ifstream outputCheck(outputFile, std::ios::binary | std::ios::ate);
        if (outputCheck) {
            size_t outputSize = outputCheck.tellg();
            std::cout << "  Output file size: " << (outputSize / (1024 * 1024))
                      << " MB\n";
            outputCheck.close();
        }

        // Get memory usage statistics
        auto memStats = processor.getMemoryStatistics();
        std::cout << "  Peak memory usage: "
                  << (memStats.peakMemoryUsage / (1024 * 1024)) << " MB\n";
        std::cout << "  Average tile processing time: "
                  << memStats.averageTileProcessingTime << "ms\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in tiled processing: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate streaming compression
 */
void demonstrateStreamingCompression() {
    std::cout << "\n=== Streaming Compression ===\n";

    try {
        const std::string inputFile = "large_test_image.tiff";
        const std::string compressedFile = "compressed_stream.atom";
        const std::string decompressedFile = "decompressed_stream.tiff";

        // Test different compression methods
        std::vector<std::pair<CompressionMethod, std::string>> methods = {
            {CompressionMethod::LZ4, "LZ4 (Fast)"},
            {CompressionMethod::ZSTD, "ZSTD (Balanced)"},
            {CompressionMethod::LZMA, "LZMA (High Compression)"}};

        for (const auto& [method, name] : methods) {
            std::cout << "\nTesting " << name << " compression:\n";

            try {
                // Compression
                StreamingCompressor compressor(method);
                compressor.setCompressionLevel(5);   // Medium compression
                compressor.setChunkSize(64 * 1024);  // 64KB chunks

                auto start = steady_clock::now();

                bool compressed =
                    compressor.compressFile(inputFile, compressedFile);

                auto compressionTime =
                    duration_cast<milliseconds>(steady_clock::now() - start);

                if (compressed) {
                    // Get file sizes
                    std::ifstream input(inputFile,
                                        std::ios::binary | std::ios::ate);
                    std::ifstream output(compressedFile,
                                         std::ios::binary | std::ios::ate);

                    size_t inputSize = input.tellg();
                    size_t outputSize = output.tellg();

                    input.close();
                    output.close();

                    double compressionRatio =
                        static_cast<double>(inputSize) / outputSize;
                    double compressionSpeed =
                        (inputSize / (1024.0 * 1024.0)) /
                        (compressionTime.count() / 1000.0);

                    std::cout
                        << "  Compression time: " << compressionTime.count()
                        << "ms\n";
                    std::cout
                        << "  Original size: " << (inputSize / (1024 * 1024))
                        << " MB\n";
                    std::cout
                        << "  Compressed size: " << (outputSize / (1024 * 1024))
                        << " MB\n";
                    std::cout << "  Compression ratio: " << std::fixed
                              << std::setprecision(2) << compressionRatio
                              << ":1\n";
                    std::cout << "  Compression speed: " << std::setprecision(1)
                              << compressionSpeed << " MB/s\n";

                    // Decompression
                    StreamingDecompressor decompressor;

                    start = steady_clock::now();

                    bool decompressed = decompressor.decompressFile(
                        compressedFile, decompressedFile);

                    auto decompressionTime = duration_cast<milliseconds>(
                        steady_clock::now() - start);

                    if (decompressed) {
                        double decompressionSpeed =
                            (inputSize / (1024.0 * 1024.0)) /
                            (decompressionTime.count() / 1000.0);

                        std::cout << "  Decompression time: "
                                  << decompressionTime.count() << "ms\n";
                        std::cout
                            << "  Decompression speed: " << decompressionSpeed
                            << " MB/s\n";

                        // Verify integrity
                        std::ifstream original(inputFile, std::ios::binary);
                        std::ifstream restored(decompressedFile,
                                               std::ios::binary);

                        bool identical = true;
                        char byte1, byte2;
                        while (original.get(byte1) && restored.get(byte2)) {
                            if (byte1 != byte2) {
                                identical = false;
                                break;
                            }
                        }

                        std::cout << "  Data integrity: "
                                  << (identical ? "VERIFIED" : "FAILED")
                                  << "\n";

                        original.close();
                        restored.close();
                    } else {
                        std::cout << "  Decompression failed\n";
                    }
                } else {
                    std::cout << "  Compression failed\n";
                }

            } catch (const std::exception& e) {
                std::cout << "  Error: " << e.what() << "\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in streaming compression: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate memory usage optimization
 */
void demonstrateMemoryOptimization() {
    std::cout << "\n=== Memory Usage Optimization ===\n";

    try {
        const std::string filename = "large_test_image.tiff";

        // Test different memory strategies
        std::vector<std::pair<MemoryStrategy, std::string>> strategies = {
            {MemoryStrategy::MinimalMemory, "Minimal Memory"},
            {MemoryStrategy::Balanced, "Balanced"},
            {MemoryStrategy::MaxPerformance, "Max Performance"}};

        for (const auto& [strategy, name] : strategies) {
            std::cout << "\nTesting " << name << " strategy:\n";

            try {
                StreamingReader reader(filename);
                reader.setMemoryStrategy(strategy);

                auto properties = reader.getImageProperties();

                // Measure memory usage during processing
                auto start = steady_clock::now();
                size_t peakMemory = 0;
                size_t currentMemory = 0;

                // Process image in chunks
                const int numChunks = 10;
                const int chunkHeight = properties.height / numChunks;

                for (int i = 0; i < numChunks; ++i) {
                    int y = i * chunkHeight;
                    int height = (i == numChunks - 1) ? properties.height - y
                                                      : chunkHeight;

                    auto chunkData =
                        reader.readRegion(0, y, properties.width, height);

                    if (chunkData) {
                        currentMemory = chunkData->size();
                        peakMemory = std::max(peakMemory, currentMemory);

                        // Simulate processing
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(10));
                    }
                }

                auto duration =
                    duration_cast<milliseconds>(steady_clock::now() - start);

                std::cout << "  Processing time: " << duration.count()
                          << "ms\n";
                std::cout << "  Peak memory usage: "
                          << (peakMemory / (1024 * 1024)) << " MB\n";
                std::cout << "  Memory efficiency: "
                          << (static_cast<double>(peakMemory) /
                              (properties.width * properties.height *
                               properties.channels))
                          << "x\n";

                // Get detailed memory statistics
                auto memStats = reader.getMemoryStatistics();
                std::cout << "  Cache hits: " << memStats.cacheHits << "\n";
                std::cout << "  Cache misses: " << memStats.cacheMisses << "\n";
                std::cout << "  Cache efficiency: "
                          << (static_cast<double>(memStats.cacheHits) /
                              (memStats.cacheHits + memStats.cacheMisses) * 100)
                          << "%\n";

            } catch (const std::exception& e) {
                std::cout << "  Error: " << e.what() << "\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in memory optimization: " << e.what() << "\n";
    }
}

int main() {
    std::cout << "=== Atom Image Streaming I/O Demo ===\n";
    std::cout << "This example demonstrates streaming I/O operations for large "
                 "images\n";

    // Run all demonstrations
    demonstrateStreamingRead();
    demonstrateProgressiveLoading();
    demonstrateTiledProcessing();
    demonstrateStreamingCompression();
    demonstrateMemoryOptimization();

    std::cout << "\n=== Streaming I/O demo completed ===\n";
    std::cout << "\nKey capabilities demonstrated:\n";
    std::cout << "- Streaming read/write for large images\n";
    std::cout << "- Progressive loading with multiple quality levels\n";
    std::cout << "- Tiled processing for memory-efficient operations\n";
    std::cout << "- Streaming compression with multiple algorithms\n";
    std::cout << "- Memory usage optimization strategies\n";
    std::cout << "- Performance monitoring and statistics\n";

    // Cleanup test files
    std::cout << "\nCleaning up test files...\n";
    std::remove("large_test_image.tiff");
    std::remove("processed_large_image.tiff");
    std::remove("compressed_stream.atom");
    std::remove("decompressed_stream.tiff");

    return 0;
}
