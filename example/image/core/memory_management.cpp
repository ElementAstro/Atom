/**
 * @file memory_management.cpp
 * @brief Advanced example demonstrating memory management techniques with image
 * blobs
 *
 * This example covers:
 * - Memory alignment for SIMD operations
 * - Memory pooling and reuse strategies
 * - Large image handling techniques
 * - Memory usage monitoring and optimization
 * - Cache-friendly access patterns
 * - Memory leak prevention
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "atom/image/core/image_blob.hpp"

using namespace atom::image;
using namespace std::chrono;

/**
 * @brief Simple memory pool for blob reuse
 */
template <typename T>
class BlobMemoryPool {
private:
    std::vector<std::unique_ptr<blob<T>>> available_blobs_;
    size_t max_pool_size_;

public:
    explicit BlobMemoryPool(size_t max_size = 10) : max_pool_size_(max_size) {}

    std::unique_ptr<blob<T>> acquire(int rows, int cols, int channels) {
        // Try to find a suitable blob in the pool
        for (auto it = available_blobs_.begin(); it != available_blobs_.end();
             ++it) {
            if ((*it)->rows() >= rows && (*it)->cols() >= cols &&
                (*it)->channels() >= channels) {
                auto blob_ptr = std::move(*it);
                available_blobs_.erase(it);

                // Resize to exact dimensions if needed
                if (blob_ptr->rows() != rows || blob_ptr->cols() != cols) {
                    blob_ptr->resize(
                        cols, rows);  // Note: resize takes (width, height)
                }

                return blob_ptr;
            }
        }

        // No suitable blob found, create new one
        return std::make_unique<blob<T>>(rows, cols, channels);
    }

    void release(std::unique_ptr<blob<T>> blob_ptr) {
        if (available_blobs_.size() < max_pool_size_) {
            available_blobs_.push_back(std::move(blob_ptr));
        }
        // If pool is full, blob will be automatically destroyed
    }

    size_t pool_size() const { return available_blobs_.size(); }

    void clear() { available_blobs_.clear(); }
};

/**
 * @brief Demonstrate memory pool usage
 */
void demonstrateMemoryPool() {
    std::cout << "\n=== Memory Pool Demonstration ===\n";

    try {
        BlobMemoryPool<uint8_t> pool(5);
        std::cout << "Created memory pool with max size: 5\n";

        // Simulate multiple image processing operations
        std::vector<std::unique_ptr<blob<uint8_t>>> active_blobs;

        for (int i = 0; i < 8; ++i) {
            auto start = high_resolution_clock::now();

            // Acquire blob from pool
            auto img = pool.acquire(100 + i * 10, 100 + i * 10, 3);

            auto end = high_resolution_clock::now();
            auto duration = duration_cast<microseconds>(end - start);

            std::cout << "Iteration " << i << ": Acquired " << img->cols()
                      << "x" << img->rows() << " blob in " << duration.count()
                      << " μs (pool size: " << pool.pool_size() << ")\n";

            // Simulate some processing
            for (int y = 0; y < img->rows(); ++y) {
                for (int x = 0; x < img->cols(); ++x) {
                    img->at(y, x, 0) = static_cast<uint8_t>((x + y + i) % 256);
                    img->at(y, x, 1) = static_cast<uint8_t>((x * y + i) % 256);
                    img->at(y, x, 2) = static_cast<uint8_t>((x ^ y ^ i) % 256);
                }
            }

            active_blobs.push_back(std::move(img));
        }

        // Release blobs back to pool
        std::cout << "\nReleasing blobs back to pool...\n";
        for (auto& blob_ptr : active_blobs) {
            pool.release(std::move(blob_ptr));
        }
        active_blobs.clear();

        std::cout << "Final pool size: " << pool.pool_size() << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in memory pool demonstration: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate cache-friendly access patterns
 */
void demonstrateCacheOptimization() {
    std::cout << "\n=== Cache Optimization Demonstration ===\n";

    try {
        const int width = 1000;
        const int height = 1000;
        const int channels = 3;

        blob<uint8_t> img(height, width, channels);

        // Fill image with test data
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < channels; ++c) {
                    img.at(y, x, c) = static_cast<uint8_t>((x + y + c) % 256);
                }
            }
        }

        // Test row-major access (cache-friendly)
        auto start = high_resolution_clock::now();
        uint64_t sum1 = 0;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < channels; ++c) {
                    sum1 += img.at(y, x, c);
                }
            }
        }
        auto end = high_resolution_clock::now();
        auto row_major_time = duration_cast<microseconds>(end - start);

        // Test column-major access (cache-unfriendly)
        start = high_resolution_clock::now();
        uint64_t sum2 = 0;
        for (int x = 0; x < width; ++x) {
            for (int y = 0; y < height; ++y) {
                for (int c = 0; c < channels; ++c) {
                    sum2 += img.at(y, x, c);
                }
            }
        }
        end = high_resolution_clock::now();
        auto col_major_time = duration_cast<microseconds>(end - start);

        std::cout << "Cache performance comparison (" << width << "x" << height
                  << " image):\n";
        std::cout << "Row-major access (cache-friendly): "
                  << row_major_time.count() << " μs\n";
        std::cout << "Column-major access (cache-unfriendly): "
                  << col_major_time.count() << " μs\n";
        std::cout << "Performance ratio: "
                  << static_cast<double>(col_major_time.count()) /
                         row_major_time.count()
                  << "x\n";
        std::cout << "Sums (verification): " << sum1 << " vs " << sum2
                  << " (should be equal)\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in cache optimization demonstration: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrate memory usage monitoring
 */
void demonstrateMemoryMonitoring() {
    std::cout << "\n=== Memory Usage Monitoring ===\n";

    try {
        std::vector<std::unique_ptr<blob<uint8_t>>> images;
        size_t total_memory = 0;

        std::cout << "Creating images and monitoring memory usage:\n";

        for (int i = 1; i <= 10; ++i) {
            int size = i * 100;
            auto img = std::make_unique<blob<uint8_t>>(size, size, 3);

            size_t image_memory = img->size();
            total_memory += image_memory;

            std::cout << "Image " << i << ": " << size << "x" << size << " = "
                      << image_memory / 1024 << " KB"
                      << " (Total: " << total_memory / 1024 << " KB)\n";

            images.push_back(std::move(img));

            // Simulate memory pressure check
            if (total_memory > 5 * 1024 * 1024) {  // 5MB threshold
                std::cout << "Memory threshold exceeded, consider cleanup\n";
            }
        }

        std::cout << "\nMemory usage summary:\n";
        std::cout << "Total images: " << images.size() << "\n";
        std::cout << "Total memory: " << total_memory / 1024 << " KB ("
                  << total_memory / (1024 * 1024) << " MB)\n";
        std::cout << "Average per image: "
                  << (total_memory / images.size()) / 1024 << " KB\n";

        // Demonstrate selective cleanup
        std::cout << "\nPerforming selective cleanup (removing every other "
                     "image)...\n";
        for (auto it = images.begin(); it != images.end();) {
            if (std::distance(images.begin(), it) % 2 == 0) {
                total_memory -= (*it)->size();
                it = images.erase(it);
            } else {
                ++it;
            }
        }

        std::cout << "After cleanup: " << images.size() << " images, "
                  << total_memory / 1024 << " KB total\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in memory monitoring: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate large image handling strategies
 */
void demonstrateLargeImageHandling() {
    std::cout << "\n=== Large Image Handling Strategies ===\n";

    try {
        // Simulate processing a very large image using tiling
        const int full_width = 4000;
        const int full_height = 3000;
        const int tile_size = 512;
        const int channels = 3;

        std::cout << "Simulating processing of " << full_width << "x"
                  << full_height << " image using " << tile_size << "x"
                  << tile_size << " tiles\n";

        int tiles_x = (full_width + tile_size - 1) / tile_size;
        int tiles_y = (full_height + tile_size - 1) / tile_size;

        std::cout << "Total tiles: " << tiles_x << "x" << tiles_y << " = "
                  << tiles_x * tiles_y << "\n";

        // Process tiles
        auto start = high_resolution_clock::now();

        for (int ty = 0; ty < tiles_y; ++ty) {
            for (int tx = 0; tx < tiles_x; ++tx) {
                // Calculate tile dimensions
                int tile_width =
                    std::min(tile_size, full_width - tx * tile_size);
                int tile_height =
                    std::min(tile_size, full_height - ty * tile_size);

                // Create tile
                blob<uint8_t> tile(tile_height, tile_width, channels);

                // Simulate processing (fill with pattern)
                for (int y = 0; y < tile_height; ++y) {
                    for (int x = 0; x < tile_width; ++x) {
                        int global_x = tx * tile_size + x;
                        int global_y = ty * tile_size + y;

                        tile.at(y, x, 0) = static_cast<uint8_t>(global_x % 256);
                        tile.at(y, x, 1) = static_cast<uint8_t>(global_y % 256);
                        tile.at(y, x, 2) =
                            static_cast<uint8_t>((global_x + global_y) % 256);
                    }
                }

                // Simulate some processing operation
                tile.flip(0);  // Vertical flip

                // In a real application, you would save or accumulate results
                // here
            }

            if ((ty + 1) % 2 == 0) {
                std::cout << "Processed " << (ty + 1) << "/" << tiles_y
                          << " tile rows\n";
            }
        }

        auto end = high_resolution_clock::now();
        auto processing_time = duration_cast<milliseconds>(end - start);

        std::cout << "Tile processing completed in " << processing_time.count()
                  << " ms\n";

        // Calculate memory efficiency
        size_t tile_memory = tile_size * tile_size * channels;
        size_t full_memory =
            static_cast<size_t>(full_width) * full_height * channels;

        std::cout << "Memory efficiency:\n";
        std::cout << "  Tile memory: " << tile_memory / 1024 << " KB\n";
        std::cout << "  Full image memory: " << full_memory / (1024 * 1024)
                  << " MB\n";
        std::cout << "  Memory reduction: "
                  << static_cast<double>(full_memory) / tile_memory << "x\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in large image handling: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrate multithreaded memory management
 */
void demonstrateMultithreadedMemory() {
    std::cout << "\n=== Multithreaded Memory Management ===\n";

    try {
        const int num_threads = std::thread::hardware_concurrency();
        const int images_per_thread = 5;

        std::cout << "Using " << num_threads << " threads, "
                  << images_per_thread << " images per thread\n";

        std::vector<std::thread> threads;
        std::vector<size_t> thread_memory(num_threads, 0);

        auto start = high_resolution_clock::now();

        // Launch threads
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([t, images_per_thread, &thread_memory]() {
                std::vector<std::unique_ptr<blob<uint8_t>>> local_images;

                for (int i = 0; i < images_per_thread; ++i) {
                    int size = 200 + (t * images_per_thread + i) * 50;
                    auto img = std::make_unique<blob<uint8_t>>(size, size, 3);

                    // Fill with thread-specific pattern
                    for (int y = 0; y < img->rows(); ++y) {
                        for (int x = 0; x < img->cols(); ++x) {
                            img->at(y, x, 0) =
                                static_cast<uint8_t>((x + t) % 256);
                            img->at(y, x, 1) =
                                static_cast<uint8_t>((y + t) % 256);
                            img->at(y, x, 2) =
                                static_cast<uint8_t>((x + y + t) % 256);
                        }
                    }

                    thread_memory[t] += img->size();
                    local_images.push_back(std::move(img));
                }

                // Simulate some processing time
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            });
        }

        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }

        auto end = high_resolution_clock::now();
        auto total_time = duration_cast<milliseconds>(end - start);

        // Calculate statistics
        size_t total_memory = 0;
        for (size_t mem : thread_memory) {
            total_memory += mem;
        }

        std::cout << "Multithreaded processing completed in "
                  << total_time.count() << " ms\n";
        std::cout << "Memory usage per thread:\n";
        for (int t = 0; t < num_threads; ++t) {
            std::cout << "  Thread " << t << ": " << thread_memory[t] / 1024
                      << " KB\n";
        }
        std::cout << "Total memory used: " << total_memory / 1024 << " KB\n";
        std::cout << "Average per thread: "
                  << (total_memory / num_threads) / 1024 << " KB\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in multithreaded memory management: " << e.what()
                  << "\n";
    }
}

int main() {
    std::cout << "=== Atom Image Memory Management Example ===\n";
    std::cout
        << "This example demonstrates advanced memory management techniques\n";

    // Run all demonstrations
    demonstrateMemoryPool();
    demonstrateCacheOptimization();
    demonstrateMemoryMonitoring();
    demonstrateLargeImageHandling();
    demonstrateMultithreadedMemory();

    std::cout << "\n=== Memory management example completed ===\n";
    return 0;
}
