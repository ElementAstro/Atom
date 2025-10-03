/**
 * @file advanced_memory_management.cpp
 * @brief Advanced memory management techniques for image processing
 *
 * This example demonstrates:
 * - Memory pooling strategies
 * - Cache-friendly memory layouts
 * - Memory alignment optimization
 * - Large image handling techniques
 * - Memory usage monitoring
 * - Custom memory allocators
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <chrono>
#include <iostream>
#include <memory>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iomanip>
#include <cstdlib>

#include "atom/image/core/image_blob.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Simple memory pool for blob allocation
 */
class BlobMemoryPool {
public:
    BlobMemoryPool(size_t poolSize = 100 * 1024 * 1024) : poolSize_(poolSize) {
        pool_ = std::aligned_alloc(64, poolSize_);
        if (!pool_) {
            throw std::bad_alloc();
        }
        freeBlocks_.push_back({pool_, poolSize_});
        std::cout << "Memory pool created: " << (poolSize_ / (1024 * 1024)) << "MB\n";
    }
    
    ~BlobMemoryPool() {
        if (pool_) {
            std::free(pool_);
        }
    }
    
    void* allocate(size_t size, size_t alignment = 64) {
        // Align size to alignment boundary
        size = (size + alignment - 1) & ~(alignment - 1);
        
        for (auto it = freeBlocks_.begin(); it != freeBlocks_.end(); ++it) {
            if (it->size >= size) {
                void* ptr = it->ptr;
                
                // Split block if necessary
                if (it->size > size) {
                    FreeBlock remaining;
                    remaining.ptr = static_cast<char*>(it->ptr) + size;
                    remaining.size = it->size - size;
                    freeBlocks_.insert(it + 1, remaining);
                }
                
                // Track allocation
                allocatedBlocks_[ptr] = size;
                freeBlocks_.erase(it);
                
                return ptr;
            }
        }
        
        return nullptr; // No suitable block found
    }
    
    void deallocate(void* ptr) {
        auto it = allocatedBlocks_.find(ptr);
        if (it != allocatedBlocks_.end()) {
            FreeBlock block{ptr, it->second};
            freeBlocks_.push_back(block);
            allocatedBlocks_.erase(it);
            
            // Merge adjacent free blocks
            mergeAdjacentBlocks();
        }
    }
    
    size_t getTotalAllocated() const {
        size_t total = 0;
        for (const auto& [ptr, size] : allocatedBlocks_) {
            total += size;
        }
        return total;
    }
    
    size_t getTotalFree() const {
        size_t total = 0;
        for (const auto& block : freeBlocks_) {
            total += block.size;
        }
        return total;
    }
    
    void printStats() const {
        std::cout << "Memory Pool Stats:\n";
        std::cout << "  Total size: " << (poolSize_ / (1024 * 1024)) << "MB\n";
        std::cout << "  Allocated: " << (getTotalAllocated() / (1024 * 1024)) << "MB\n";
        std::cout << "  Free: " << (getTotalFree() / (1024 * 1024)) << "MB\n";
        std::cout << "  Free blocks: " << freeBlocks_.size() << "\n";
        std::cout << "  Allocated blocks: " << allocatedBlocks_.size() << "\n";
    }

private:
    struct FreeBlock {
        void* ptr;
        size_t size;
    };
    
    void mergeAdjacentBlocks() {
        std::sort(freeBlocks_.begin(), freeBlocks_.end(), 
                  [](const FreeBlock& a, const FreeBlock& b) {
                      return a.ptr < b.ptr;
                  });
        
        for (size_t i = 0; i < freeBlocks_.size() - 1; ) {
            char* currentEnd = static_cast<char*>(freeBlocks_[i].ptr) + freeBlocks_[i].size;
            if (currentEnd == freeBlocks_[i + 1].ptr) {
                // Merge blocks
                freeBlocks_[i].size += freeBlocks_[i + 1].size;
                freeBlocks_.erase(freeBlocks_.begin() + i + 1);
            } else {
                ++i;
            }
        }
    }
    
    void* pool_;
    size_t poolSize_;
    std::vector<FreeBlock> freeBlocks_;
    std::unordered_map<void*, size_t> allocatedBlocks_;
};

/**
 * @brief Demonstrate memory pooling for blob operations
 */
void demonstrateMemoryPooling() {
    std::cout << "\n=== Memory Pooling Demonstration ===\n";
    
    BlobMemoryPool pool(50 * 1024 * 1024); // 50MB pool
    
    // Allocate multiple blobs from pool
    std::vector<void*> allocations;
    std::vector<size_t> sizes = {
        1024 * 1024,    // 1MB
        2 * 1024 * 1024, // 2MB
        5 * 1024 * 1024, // 5MB
        1024 * 1024,    // 1MB
        3 * 1024 * 1024  // 3MB
    };
    
    std::cout << "Allocating from memory pool:\n";
    for (size_t size : sizes) {
        void* ptr = pool.allocate(size);
        if (ptr) {
            allocations.push_back(ptr);
            std::cout << "  Allocated " << (size / (1024 * 1024)) << "MB at " << ptr << "\n";
        } else {
            std::cout << "  Failed to allocate " << (size / (1024 * 1024)) << "MB\n";
        }
    }
    
    pool.printStats();
    
    // Deallocate some blocks
    std::cout << "\nDeallocating some blocks:\n";
    if (allocations.size() >= 2) {
        pool.deallocate(allocations[1]);
        std::cout << "  Deallocated block at " << allocations[1] << "\n";
        allocations.erase(allocations.begin() + 1);
    }
    
    pool.printStats();
    
    // Try to allocate again (should reuse freed space)
    void* reused = pool.allocate(1024 * 1024);
    if (reused) {
        std::cout << "  Reused memory at " << reused << "\n";
        allocations.push_back(reused);
    }
    
    pool.printStats();
    
    // Clean up remaining allocations
    for (void* ptr : allocations) {
        pool.deallocate(ptr);
    }
    
    std::cout << "\nAfter cleanup:\n";
    pool.printStats();
}

/**
 * @brief Demonstrate memory alignment optimization
 */
void demonstrateMemoryAlignment() {
    std::cout << "\n=== Memory Alignment Optimization ===\n";
    
    // Test different alignment values
    std::vector<size_t> alignments = {1, 4, 8, 16, 32, 64, 128, 256};
    size_t dataSize = 1024 * 1024; // 1MB
    
    for (size_t alignment : alignments) {
        std::cout << "Testing " << alignment << "-byte alignment:\n";
        
        // Allocate aligned memory
        void* alignedPtr = std::aligned_alloc(alignment, dataSize);
        if (alignedPtr) {
            // Check alignment
            uintptr_t addr = reinterpret_cast<uintptr_t>(alignedPtr);
            bool isAligned = (addr % alignment) == 0;
            
            std::cout << "  Address: " << alignedPtr << "\n";
            std::cout << "  Aligned: " << (isAligned ? "YES" : "NO") << "\n";
            
            // Performance test with aligned memory
            auto start = high_resolution_clock::now();
            
            // Simulate memory-intensive operation
            uint8_t* data = static_cast<uint8_t*>(alignedPtr);
            for (size_t i = 0; i < dataSize; i += alignment) {
                data[i] = static_cast<uint8_t>(i % 256);
            }
            
            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);
            
            std::cout << "  Write time: " << duration.count() << " μs\n";
            
            std::free(alignedPtr);
        } else {
            std::cout << "  Failed to allocate aligned memory\n";
        }
    }
}

/**
 * @brief Demonstrate cache-friendly memory layouts
 */
void demonstrateCacheFriendlyLayouts() {
    std::cout << "\n=== Cache-Friendly Memory Layouts ===\n";
    
    const size_t width = 1024;
    const size_t height = 1024;
    const size_t channels = 3;
    const size_t totalSize = width * height * channels;
    
    // Test different memory layouts
    std::cout << "Comparing memory access patterns:\n";
    
    // Row-major layout (cache-friendly for row access)
    {
        std::vector<uint8_t> rowMajorData(totalSize);
        
        auto start = high_resolution_clock::now();
        
        // Sequential row access (cache-friendly)
        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                for (size_t c = 0; c < channels; ++c) {
                    size_t index = (y * width + x) * channels + c;
                    rowMajorData[index] = static_cast<uint8_t>((x + y + c) % 256);
                }
            }
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        std::cout << "  Row-major sequential access: " << duration.count() << " μs\n";
    }
    
    // Column access on row-major data (cache-unfriendly)
    {
        std::vector<uint8_t> rowMajorData(totalSize);
        
        auto start = high_resolution_clock::now();
        
        // Column access (cache-unfriendly)
        for (size_t x = 0; x < width; ++x) {
            for (size_t y = 0; y < height; ++y) {
                for (size_t c = 0; c < channels; ++c) {
                    size_t index = (y * width + x) * channels + c;
                    rowMajorData[index] = static_cast<uint8_t>((x + y + c) % 256);
                }
            }
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        std::cout << "  Row-major column access: " << duration.count() << " μs\n";
    }
    
    // Blocked/tiled access (cache-friendly for both)
    {
        std::vector<uint8_t> tiledData(totalSize);
        const size_t blockSize = 64; // Cache line friendly block size
        
        auto start = high_resolution_clock::now();
        
        // Tiled access pattern
        for (size_t by = 0; by < height; by += blockSize) {
            for (size_t bx = 0; bx < width; bx += blockSize) {
                for (size_t y = by; y < std::min(by + blockSize, height); ++y) {
                    for (size_t x = bx; x < std::min(bx + blockSize, width); ++x) {
                        for (size_t c = 0; c < channels; ++c) {
                            size_t index = (y * width + x) * channels + c;
                            tiledData[index] = static_cast<uint8_t>((x + y + c) % 256);
                        }
                    }
                }
            }
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        std::cout << "  Tiled access pattern: " << duration.count() << " μs\n";
    }
}

/**
 * @brief Demonstrate large image handling strategies
 */
void demonstrateLargeImageHandling() {
    std::cout << "\n=== Large Image Handling Strategies ===\n";
    
    // Simulate very large image dimensions
    const size_t largeWidth = 8192;
    const size_t largeHeight = 8192;
    const size_t channels = 3;
    const size_t totalSize = largeWidth * largeHeight * channels;
    
    std::cout << "Simulating large image: " << largeWidth << "x" << largeHeight << "x" << channels << "\n";
    std::cout << "Total size: " << (totalSize / (1024 * 1024)) << "MB\n";
    
    // Strategy 1: Streaming processing
    std::cout << "\n1. Streaming Processing:\n";
    {
        const size_t stripHeight = 256; // Process in strips
        const size_t stripSize = largeWidth * stripHeight * channels;
        
        std::cout << "  Processing in strips of " << stripHeight << " rows\n";
        std::cout << "  Strip size: " << (stripSize / (1024 * 1024)) << "MB\n";
        
        auto start = high_resolution_clock::now();
        
        for (size_t y = 0; y < largeHeight; y += stripHeight) {
            size_t currentStripHeight = std::min(stripHeight, largeHeight - y);
            size_t currentStripSize = largeWidth * currentStripHeight * channels;
            
            // Simulate processing one strip
            std::vector<uint8_t> strip(currentStripSize);
            
            // Fill strip with data
            for (size_t i = 0; i < currentStripSize; ++i) {
                strip[i] = static_cast<uint8_t>((i + y) % 256);
            }
            
            // Simulate processing (e.g., filtering)
            for (size_t i = 0; i < currentStripSize; i += channels) {
                // Simple processing: average RGB values
                if (i + 2 < currentStripSize) {
                    uint8_t avg = (strip[i] + strip[i+1] + strip[i+2]) / 3;
                    strip[i] = strip[i+1] = strip[i+2] = avg;
                }
            }
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start);
        
        std::cout << "  Streaming processing time: " << duration.count() << "ms\n";
        std::cout << "  Peak memory usage: ~" << (stripSize / (1024 * 1024)) << "MB\n";
    }
    
    // Strategy 2: Tiled processing
    std::cout << "\n2. Tiled Processing:\n";
    {
        const size_t tileSize = 512;
        const size_t tileSizeBytes = tileSize * tileSize * channels;
        
        std::cout << "  Processing in " << tileSize << "x" << tileSize << " tiles\n";
        std::cout << "  Tile size: " << (tileSizeBytes / 1024) << "KB\n";
        
        auto start = high_resolution_clock::now();
        
        for (size_t ty = 0; ty < largeHeight; ty += tileSize) {
            for (size_t tx = 0; tx < largeWidth; tx += tileSize) {
                size_t currentTileWidth = std::min(tileSize, largeWidth - tx);
                size_t currentTileHeight = std::min(tileSize, largeHeight - ty);
                size_t currentTileSize = currentTileWidth * currentTileHeight * channels;
                
                // Simulate processing one tile
                std::vector<uint8_t> tile(currentTileSize);
                
                // Fill and process tile
                for (size_t i = 0; i < currentTileSize; ++i) {
                    tile[i] = static_cast<uint8_t>((i + tx + ty) % 256);
                }
            }
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start);
        
        std::cout << "  Tiled processing time: " << duration.count() << "ms\n";
        std::cout << "  Peak memory usage: ~" << (tileSizeBytes / 1024) << "KB\n";
    }
}

int main() {
    std::cout << "=== Atom Image Advanced Memory Management Example ===\n";
    std::cout << "This example demonstrates advanced memory management techniques\n";

    // Run all demonstrations
    demonstrateMemoryPooling();
    demonstrateMemoryAlignment();
    demonstrateCacheFriendlyLayouts();
    demonstrateLargeImageHandling();

    std::cout << "\n=== Advanced memory management example completed ===\n";
    std::cout << "\nKey techniques demonstrated:\n";
    std::cout << "- Memory pooling for reduced allocation overhead\n";
    std::cout << "- Memory alignment for performance optimization\n";
    std::cout << "- Cache-friendly memory access patterns\n";
    std::cout << "- Streaming and tiled processing for large images\n";
    
    return 0;
}
