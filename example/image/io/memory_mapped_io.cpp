/**
 * @file memory_mapped_io.cpp
 * @brief Memory-mapped I/O operations demonstration
 *
 * This example demonstrates:
 * - Memory-mapped file access for large images
 * - Efficient random access to image data
 * - Virtual memory management
 * - Copy-on-write semantics
 * - Cross-platform memory mapping
 * - Performance comparison with traditional I/O
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "atom/image/core/image_blob.hpp"
#include "atom/image/io/image_file_reader.hpp"
#include "atom/image/io/memory_mapped_file.hpp"
#include "atom/image/io/virtual_memory_manager.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Create a test image file for memory mapping
 */
void createTestImageFile(const std::string& filename, int width, int height) {
    std::cout << "Creating test image file: " << filename << " (" << width
              << "x" << height << ")\n";

    try {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Cannot create test file");
        }

        // Write simple header (width, height, channels)
        uint32_t header[3] = {static_cast<uint32_t>(width),
                              static_cast<uint32_t>(height), 3};
        file.write(reinterpret_cast<const char*>(header), sizeof(header));

        // Write image data
        const size_t dataSize = width * height * 3;
        std::vector<uint8_t> imageData(dataSize);

        // Generate test pattern
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int index = (y * width + x) * 3;
                imageData[index] = static_cast<uint8_t>((x + y) % 256);  // R
                imageData[index + 1] =
                    static_cast<uint8_t>((x * 2) % 256);  // G
                imageData[index + 2] =
                    static_cast<uint8_t>((y * 2) % 256);  // B
            }
        }

        file.write(reinterpret_cast<const char*>(imageData.data()), dataSize);
        file.close();

        std::cout << "  File size: "
                  << ((sizeof(header) + dataSize) / (1024 * 1024)) << " MB\n";

    } catch (const std::exception& e) {
        std::cerr << "Error creating test file: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate basic memory mapping operations
 */
void demonstrateBasicMemoryMapping() {
    std::cout << "\n=== Basic Memory Mapping Operations ===\n";

    const std::string filename = "test_image_mmap.raw";
    const int width = 2048;
    const int height = 1536;

    // Create test file
    createTestImageFile(filename, width, height);

    try {
        // Open file with memory mapping
        MemoryMappedFile mmFile(filename);

        if (!mmFile.isValid()) {
            std::cout << "Failed to memory map file\n";
            return;
        }

        std::cout << "Memory mapped file successfully\n";
        std::cout << "  File size: " << (mmFile.size() / (1024 * 1024))
                  << " MB\n";
        std::cout << "  Mapped address: " << mmFile.data() << "\n";

        // Read header
        const uint32_t* header =
            reinterpret_cast<const uint32_t*>(mmFile.data());
        uint32_t fileWidth = header[0];
        uint32_t fileHeight = header[1];
        uint32_t fileChannels = header[2];

        std::cout << "  Image dimensions: " << fileWidth << "x" << fileHeight
                  << "x" << fileChannels << "\n";

        // Access image data
        const uint8_t* imageData = reinterpret_cast<const uint8_t*>(
            mmFile.data() + sizeof(uint32_t) * 3);

        // Test 1: Sequential access
        std::cout << "\nTest 1: Sequential access\n";

        auto start = steady_clock::now();

        uint64_t checksum = 0;
        const size_t dataSize = fileWidth * fileHeight * fileChannels;

        for (size_t i = 0; i < dataSize; ++i) {
            checksum += imageData[i];
        }

        auto duration =
            duration_cast<microseconds>(steady_clock::now() - start);

        std::cout << "  Checksum: " << checksum << "\n";
        std::cout << "  Sequential access time: " << duration.count()
                  << " μs\n";
        std::cout << "  Throughput: "
                  << (dataSize / (1024.0 * 1024.0)) /
                         (duration.count() / 1000000.0)
                  << " MB/s\n";

        // Test 2: Random access
        std::cout << "\nTest 2: Random access\n";

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dis(0, dataSize - 1);

        const int numAccesses = 10000;
        std::vector<size_t> randomIndices;
        for (int i = 0; i < numAccesses; ++i) {
            randomIndices.push_back(dis(gen));
        }

        start = steady_clock::now();

        uint64_t randomChecksum = 0;
        for (size_t index : randomIndices) {
            randomChecksum += imageData[index];
        }

        duration = duration_cast<microseconds>(steady_clock::now() - start);

        std::cout << "  Random checksum: " << randomChecksum << "\n";
        std::cout << "  Random access time: " << duration.count() << " μs\n";
        std::cout << "  Average time per access: "
                  << (duration.count() / numAccesses) << " ns\n";

        // Test 3: Region access
        std::cout << "\nTest 3: Region access\n";

        const int regionX = fileWidth / 4;
        const int regionY = fileHeight / 4;
        const int regionWidth = fileWidth / 2;
        const int regionHeight = fileHeight / 2;

        start = steady_clock::now();

        uint64_t regionChecksum = 0;
        for (int y = regionY; y < regionY + regionHeight; ++y) {
            for (int x = regionX; x < regionX + regionWidth; ++x) {
                for (int c = 0; c < fileChannels; ++c) {
                    size_t index = (y * fileWidth + x) * fileChannels + c;
                    regionChecksum += imageData[index];
                }
            }
        }

        duration = duration_cast<microseconds>(steady_clock::now() - start);

        std::cout << "  Region (" << regionX << "," << regionY << " "
                  << regionWidth << "x" << regionHeight << ")\n";
        std::cout << "  Region checksum: " << regionChecksum << "\n";
        std::cout << "  Region access time: " << duration.count() << " μs\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in basic memory mapping: " << e.what() << "\n";
    }
}

/**
 * @brief Compare memory mapping vs traditional I/O
 */
void compareMemoryMappingVsTraditionalIO() {
    std::cout << "\n=== Memory Mapping vs Traditional I/O Comparison ===\n";

    const std::string filename = "test_image_mmap.raw";

    try {
        // Get file size
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        size_t fileSize = file.tellg();
        file.close();

        std::cout << "Comparing access methods for "
                  << (fileSize / (1024 * 1024)) << " MB file\n";

        // Test parameters
        const int numTests = 1000;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dis(
            12, fileSize - 1000);  // Skip header

        // Generate random positions
        std::vector<size_t> testPositions;
        for (int i = 0; i < numTests; ++i) {
            testPositions.push_back(dis(gen));
        }

        // Test 1: Traditional I/O
        std::cout << "\nTest 1: Traditional I/O (random seeks)\n";

        auto start = steady_clock::now();

        std::ifstream traditionalFile(filename, std::ios::binary);
        uint64_t traditionalChecksum = 0;

        for (size_t pos : testPositions) {
            traditionalFile.seekg(pos);
            uint8_t byte;
            traditionalFile.read(reinterpret_cast<char*>(&byte), 1);
            traditionalChecksum += byte;
        }

        traditionalFile.close();

        auto traditionalTime =
            duration_cast<microseconds>(steady_clock::now() - start);

        std::cout << "  Traditional I/O time: " << traditionalTime.count()
                  << " μs\n";
        std::cout << "  Average seek time: "
                  << (traditionalTime.count() / numTests) << " ns\n";
        std::cout << "  Checksum: " << traditionalChecksum << "\n";

        // Test 2: Memory mapping
        std::cout << "\nTest 2: Memory mapping (random access)\n";

        start = steady_clock::now();

        MemoryMappedFile mmFile(filename);
        const uint8_t* data = reinterpret_cast<const uint8_t*>(mmFile.data());
        uint64_t mmapChecksum = 0;

        for (size_t pos : testPositions) {
            mmapChecksum += data[pos];
        }

        auto mmapTime =
            duration_cast<microseconds>(steady_clock::now() - start);

        std::cout << "  Memory mapping time: " << mmapTime.count() << " μs\n";
        std::cout << "  Average access time: " << (mmapTime.count() / numTests)
                  << " ns\n";
        std::cout << "  Checksum: " << mmapChecksum << "\n";

        // Performance comparison
        std::cout << "\nPerformance Comparison:\n";
        double speedup =
            static_cast<double>(traditionalTime.count()) / mmapTime.count();
        std::cout << "  Memory mapping speedup: " << std::fixed
                  << std::setprecision(1) << speedup << "x\n";

        // Test 3: Sequential read comparison
        std::cout << "\nTest 3: Sequential read comparison\n";

        const size_t readSize = 1024 * 1024;  // 1 MB

        // Traditional sequential read
        start = steady_clock::now();

        std::ifstream seqFile(filename, std::ios::binary);
        seqFile.seekg(12);  // Skip header
        std::vector<uint8_t> buffer(readSize);
        seqFile.read(reinterpret_cast<char*>(buffer.data()), readSize);
        seqFile.close();

        uint64_t seqChecksum = 0;
        for (uint8_t byte : buffer) {
            seqChecksum += byte;
        }

        auto seqTime = duration_cast<microseconds>(steady_clock::now() - start);

        std::cout << "  Traditional sequential read: " << seqTime.count()
                  << " μs\n";
        std::cout << "  Sequential throughput: "
                  << (readSize / (1024.0 * 1024.0)) /
                         (seqTime.count() / 1000000.0)
                  << " MB/s\n";

        // Memory mapped sequential access
        start = steady_clock::now();

        const uint8_t* mmapData =
            reinterpret_cast<const uint8_t*>(mmFile.data()) + 12;
        uint64_t mmapSeqChecksum = 0;

        for (size_t i = 0; i < readSize; ++i) {
            mmapSeqChecksum += mmapData[i];
        }

        auto mmapSeqTime =
            duration_cast<microseconds>(steady_clock::now() - start);

        std::cout << "  Memory mapped sequential: " << mmapSeqTime.count()
                  << " μs\n";
        std::cout << "  Mmap sequential throughput: "
                  << (readSize / (1024.0 * 1024.0)) /
                         (mmapSeqTime.count() / 1000000.0)
                  << " MB/s\n";

        std::cout << "  Sequential checksums match: "
                  << (seqChecksum == mmapSeqChecksum ? "YES" : "NO") << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in I/O comparison: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate virtual memory management
 */
void demonstrateVirtualMemoryManagement() {
    std::cout << "\n=== Virtual Memory Management ===\n";

    try {
        const std::string filename = "test_image_mmap.raw";

        VirtualMemoryManager vmManager;
        vmManager.setPageSize(4096);         // 4KB pages
        vmManager.setMaxResidentPages(256);  // 1MB resident set

        std::cout << "Virtual memory manager configuration:\n";
        std::cout << "  Page size: " << vmManager.getPageSize() << " bytes\n";
        std::cout << "  Max resident pages: " << vmManager.getMaxResidentPages()
                  << "\n";
        std::cout << "  Max resident memory: "
                  << (vmManager.getMaxResidentPages() *
                      vmManager.getPageSize() / 1024)
                  << " KB\n";

        // Map file into virtual memory
        auto vmFile = vmManager.mapFile(filename);

        if (!vmFile) {
            std::cout << "Failed to map file into virtual memory\n";
            return;
        }

        std::cout << "File mapped into virtual memory\n";
        std::cout << "  Virtual size: "
                  << (vmFile->getVirtualSize() / (1024 * 1024)) << " MB\n";
        std::cout << "  Number of pages: " << vmFile->getPageCount() << "\n";

        // Test page fault handling
        std::cout << "\nTesting page fault handling:\n";

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dis(0,
                                                  vmFile->getVirtualSize() - 1);

        auto start = steady_clock::now();

        const int numAccesses = 5000;
        uint64_t checksum = 0;

        for (int i = 0; i < numAccesses; ++i) {
            size_t offset = dis(gen);
            uint8_t byte = vmFile->readByte(offset);
            checksum += byte;

            if ((i + 1) % 1000 == 0) {
                auto stats = vmManager.getStatistics();
                std::cout << "  Access " << (i + 1) << ": "
                          << "Page faults: " << stats.pageFaults
                          << ", Resident pages: " << stats.residentPages
                          << ", Cache hits: " << stats.cacheHits << "\n";
            }
        }

        auto duration =
            duration_cast<milliseconds>(steady_clock::now() - start);

        // Final statistics
        auto finalStats = vmManager.getStatistics();
        std::cout << "\nFinal virtual memory statistics:\n";
        std::cout << "  Total accesses: " << numAccesses << "\n";
        std::cout << "  Page faults: " << finalStats.pageFaults << "\n";
        std::cout << "  Cache hits: " << finalStats.cacheHits << "\n";
        std::cout << "  Cache hit rate: "
                  << (static_cast<double>(finalStats.cacheHits) / numAccesses *
                      100)
                  << "%\n";
        std::cout << "  Resident pages: " << finalStats.residentPages << "\n";
        std::cout << "  Memory usage: "
                  << (finalStats.residentPages * vmManager.getPageSize() / 1024)
                  << " KB\n";
        std::cout << "  Total time: " << duration.count() << " ms\n";
        std::cout << "  Checksum: " << checksum << "\n";

        // Test different access patterns
        std::cout << "\nTesting access patterns:\n";

        // Sequential access
        start = steady_clock::now();
        uint64_t seqChecksum = 0;
        const size_t seqSize = 100000;

        for (size_t i = 0; i < seqSize; ++i) {
            seqChecksum += vmFile->readByte(i + 12);  // Skip header
        }

        auto seqTime = duration_cast<microseconds>(steady_clock::now() - start);
        auto seqStats = vmManager.getStatistics();

        std::cout << "  Sequential access (" << seqSize << " bytes):\n";
        std::cout << "    Time: " << seqTime.count() << " μs\n";
        std::cout << "    Page faults: "
                  << (seqStats.pageFaults - finalStats.pageFaults) << "\n";
        std::cout << "    Throughput: "
                  << (seqSize / (1024.0 * 1024.0)) /
                         (seqTime.count() / 1000000.0)
                  << " MB/s\n";

        // Locality-based access
        vmManager.resetStatistics();
        start = steady_clock::now();
        uint64_t localityChecksum = 0;

        // Access data in small local regions
        for (int region = 0; region < 100; ++region) {
            size_t baseOffset = (region * 1000) + 12;
            for (int i = 0; i < 100; ++i) {
                localityChecksum += vmFile->readByte(baseOffset + i);
            }
        }

        auto localityTime =
            duration_cast<microseconds>(steady_clock::now() - start);
        auto localityStats = vmManager.getStatistics();

        std::cout << "  Locality-based access (10000 bytes in regions):\n";
        std::cout << "    Time: " << localityTime.count() << " μs\n";
        std::cout << "    Page faults: " << localityStats.pageFaults << "\n";
        std::cout << "    Cache hit rate: "
                  << (static_cast<double>(localityStats.cacheHits) / 10000 *
                      100)
                  << "%\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in virtual memory management: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate copy-on-write semantics
 */
void demonstrateCopyOnWrite() {
    std::cout << "\n=== Copy-on-Write Semantics ===\n";

    try {
        const std::string filename = "test_image_mmap.raw";

        // Create multiple memory mapped views
        std::cout << "Creating multiple memory mapped views:\n";

        MemoryMappedFile readOnlyView(filename,
                                      MemoryMappedFile::AccessMode::ReadOnly);
        MemoryMappedFile copyOnWriteView(
            filename, MemoryMappedFile::AccessMode::CopyOnWrite);

        if (!readOnlyView.isValid() || !copyOnWriteView.isValid()) {
            std::cout << "Failed to create memory mapped views\n";
            return;
        }

        std::cout << "  Read-only view: " << readOnlyView.data()
                  << " (size: " << readOnlyView.size() << ")\n";
        std::cout << "  Copy-on-write view: " << copyOnWriteView.data()
                  << " (size: " << copyOnWriteView.size() << ")\n";

        // Read original data
        const uint8_t* originalData =
            reinterpret_cast<const uint8_t*>(readOnlyView.data()) + 12;
        uint8_t* cowData =
            reinterpret_cast<uint8_t*>(copyOnWriteView.data()) + 12;

        // Verify initial data is the same
        bool initialMatch = true;
        for (size_t i = 0; i < 1000; ++i) {
            if (originalData[i] != cowData[i]) {
                initialMatch = false;
                break;
            }
        }

        std::cout << "  Initial data match: " << (initialMatch ? "YES" : "NO")
                  << "\n";

        // Modify copy-on-write view
        std::cout << "\nModifying copy-on-write view:\n";

        auto start = steady_clock::now();

        // Modify some data
        for (size_t i = 0; i < 1000; ++i) {
            cowData[i] = static_cast<uint8_t>((cowData[i] + 1) % 256);
        }

        auto modifyTime =
            duration_cast<microseconds>(steady_clock::now() - start);

        std::cout << "  Modification time: " << modifyTime.count() << " μs\n";

        // Verify data divergence
        int differences = 0;
        for (size_t i = 0; i < 1000; ++i) {
            if (originalData[i] != cowData[i]) {
                differences++;
            }
        }

        std::cout << "  Data differences: " << differences
                  << " out of 1000 bytes\n";
        std::cout << "  Copy-on-write working: "
                  << (differences > 0 ? "YES" : "NO") << "\n";

        // Test memory usage
        auto memInfo = copyOnWriteView.getMemoryInfo();
        std::cout << "  Private pages: " << memInfo.privatePages << "\n";
        std::cout << "  Shared pages: " << memInfo.sharedPages << "\n";
        std::cout << "  Memory overhead: "
                  << (memInfo.privatePages * 4096 / 1024) << " KB\n";

        // Test performance of COW vs regular allocation
        std::cout << "\nPerformance comparison:\n";

        const size_t testSize = 1024 * 1024;  // 1 MB

        // Regular allocation and copy
        start = steady_clock::now();

        std::vector<uint8_t> regularCopy(testSize);
        std::memcpy(regularCopy.data(), originalData, testSize);

        // Modify the copy
        for (size_t i = 0; i < testSize; ++i) {
            regularCopy[i] = static_cast<uint8_t>((regularCopy[i] + 1) % 256);
        }

        auto regularTime =
            duration_cast<microseconds>(steady_clock::now() - start);

        // COW approach
        MemoryMappedFile cowTest(filename,
                                 MemoryMappedFile::AccessMode::CopyOnWrite);
        uint8_t* cowTestData = reinterpret_cast<uint8_t*>(cowTest.data()) + 12;

        start = steady_clock::now();

        // Modify COW data
        for (size_t i = 0; i < testSize; ++i) {
            cowTestData[i] = static_cast<uint8_t>((cowTestData[i] + 1) % 256);
        }

        auto cowTime = duration_cast<microseconds>(steady_clock::now() - start);

        std::cout << "  Regular allocation + copy + modify: "
                  << regularTime.count() << " μs\n";
        std::cout << "  Copy-on-write modify: " << cowTime.count() << " μs\n";
        std::cout << "  COW performance ratio: "
                  << (static_cast<double>(cowTime.count()) /
                      regularTime.count())
                  << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in copy-on-write demonstration: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrate cross-platform compatibility
 */
void demonstrateCrossPlatformCompatibility() {
    std::cout << "\n=== Cross-Platform Compatibility ===\n";

    try {
        const std::string filename = "test_image_mmap.raw";

        // Test platform-specific features
        std::cout << "Platform information:\n";

#ifdef _WIN32
        std::cout << "  Platform: Windows\n";
        std::cout << "  Using: CreateFileMapping/MapViewOfFile\n";
#elif defined(__linux__)
        std::cout << "  Platform: Linux\n";
        std::cout << "  Using: mmap/munmap\n";
#elif defined(__APPLE__)
        std::cout << "  Platform: macOS\n";
        std::cout << "  Using: mmap/munmap\n";
#else
        std::cout << "  Platform: Unknown\n";
#endif

        // Test memory mapping capabilities
        MemoryMappedFile::Capabilities caps =
            MemoryMappedFile::getCapabilities();

        std::cout << "Memory mapping capabilities:\n";
        std::cout << "  Read-only mapping: "
                  << (caps.supportsReadOnly ? "YES" : "NO") << "\n";
        std::cout << "  Read-write mapping: "
                  << (caps.supportsReadWrite ? "YES" : "NO") << "\n";
        std::cout << "  Copy-on-write: "
                  << (caps.supportsCopyOnWrite ? "YES" : "NO") << "\n";
        std::cout << "  Large files (>4GB): "
                  << (caps.supportsLargeFiles ? "YES" : "NO") << "\n";
        std::cout << "  Sparse files: "
                  << (caps.supportsSparseFiles ? "YES" : "NO") << "\n";
        std::cout << "  Memory advice: "
                  << (caps.supportsMemoryAdvice ? "YES" : "NO") << "\n";

        // Test different access modes
        std::vector<std::pair<MemoryMappedFile::AccessMode, std::string>>
            modes = {
                {MemoryMappedFile::AccessMode::ReadOnly, "Read-Only"},
                {MemoryMappedFile::AccessMode::ReadWrite, "Read-Write"},
                {MemoryMappedFile::AccessMode::CopyOnWrite, "Copy-on-Write"}};

        for (const auto& [mode, name] : modes) {
            std::cout << "\nTesting " << name << " mode:\n";

            try {
                MemoryMappedFile mmFile(filename, mode);

                if (mmFile.isValid()) {
                    std::cout << "  Status: SUCCESS\n";
                    std::cout << "  Size: " << (mmFile.size() / 1024)
                              << " KB\n";
                    std::cout << "  Address: " << mmFile.data() << "\n";

                    // Test basic read
                    const uint8_t* data =
                        reinterpret_cast<const uint8_t*>(mmFile.data());
                    uint32_t checksum = 0;
                    for (size_t i = 0;
                         i < std::min(size_t(1000), mmFile.size()); ++i) {
                        checksum += data[i];
                    }
                    std::cout << "  Read test checksum: " << checksum << "\n";

                    // Test write (if supported)
                    if (mode != MemoryMappedFile::AccessMode::ReadOnly) {
                        try {
                            uint8_t* writeData =
                                reinterpret_cast<uint8_t*>(mmFile.data());
                            uint8_t originalByte = writeData[12];
                            writeData[12] =
                                static_cast<uint8_t>((originalByte + 1) % 256);
                            writeData[12] = originalByte;  // Restore
                            std::cout << "  Write test: SUCCESS\n";
                        } catch (const std::exception& e) {
                            std::cout << "  Write test: FAILED (" << e.what()
                                      << ")\n";
                        }
                    }

                } else {
                    std::cout << "  Status: FAILED\n";
                }

            } catch (const std::exception& e) {
                std::cout << "  Status: ERROR (" << e.what() << ")\n";
            }
        }

        // Test memory advice (if supported)
        if (caps.supportsMemoryAdvice) {
            std::cout << "\nTesting memory advice:\n";

            MemoryMappedFile mmFile(filename);
            if (mmFile.isValid()) {
                std::vector<
                    std::pair<MemoryMappedFile::MemoryAdvice, std::string>>
                    adviceTypes = {
                        {MemoryMappedFile::MemoryAdvice::Normal, "Normal"},
                        {MemoryMappedFile::MemoryAdvice::Sequential,
                         "Sequential"},
                        {MemoryMappedFile::MemoryAdvice::Random, "Random"},
                        {MemoryMappedFile::MemoryAdvice::WillNeed, "Will Need"},
                        {MemoryMappedFile::MemoryAdvice::DontNeed,
                         "Don't Need"}};

                for (const auto& [advice, name] : adviceTypes) {
                    try {
                        mmFile.advise(advice);
                        std::cout << "  " << name << " advice: SUCCESS\n";
                    } catch (const std::exception& e) {
                        std::cout << "  " << name << " advice: FAILED ("
                                  << e.what() << ")\n";
                    }
                }
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in cross-platform compatibility test: " << e.what()
                  << "\n";
    }
}

int main() {
    std::cout << "=== Atom Image Memory-Mapped I/O Demo ===\n";
    std::cout << "This example demonstrates memory-mapped I/O operations\n";

    // Run all demonstrations
    demonstrateBasicMemoryMapping();
    compareMemoryMappingVsTraditionalIO();
    demonstrateVirtualMemoryManagement();
    demonstrateCopyOnWrite();
    demonstrateCrossPlatformCompatibility();

    std::cout << "\n=== Memory-mapped I/O demo completed ===\n";
    std::cout << "\nKey capabilities demonstrated:\n";
    std::cout << "- Basic memory mapping operations and access patterns\n";
    std::cout << "- Performance comparison with traditional file I/O\n";
    std::cout << "- Virtual memory management with page fault handling\n";
    std::cout << "- Copy-on-write semantics for efficient data sharing\n";
    std::cout << "- Cross-platform compatibility and capability detection\n";
    std::cout << "- Memory advice and optimization techniques\n";

    // Cleanup
    std::cout << "\nCleaning up test files...\n";
    std::remove("test_image_mmap.raw");

    return 0;
}
