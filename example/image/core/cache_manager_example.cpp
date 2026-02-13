/**
 * @file cache_manager_example.cpp
 * @brief Example demonstrating cache manager usage
 *
 * This example covers:
 * - Creating and configuring cache managers
 * - Storing and retrieving cached data
 * - Cache eviction policies (LRU, LFU, FIFO)
 * - Cache statistics and monitoring
 * - Memory management
 *
 * @author Atom Image Module
 * @date 2025
 */

#include <iostream>
#include <memory>
#include <opencv2/core.hpp>
#include <string>

#include "atom/image/core/cache_manager.hpp"
#include "atom/image/core/image_blob.hpp"

using namespace atom::image;
using namespace std;

/**
 * @brief Demonstrate basic cache operations
 */
void demonstrateBasicCaching() {
    cout << "\n=== Basic Cache Operations ===\n";

    // Create cache manager for cv::Mat with 10MB limit
    CacheManager<cv::Mat> cache(10 * 1024 * 1024, CachePolicy::LRU);

    // Create and cache some images
    for (int i = 0; i < 5; ++i) {
        auto img = make_shared<cv::Mat>(480, 640, CV_8UC3,
                                        cv::Scalar(i * 50, 100, 150));
        string key = "image_" + to_string(i);
        size_t size = img->total() * img->elemSize();

        cache.put(key, img, size);
        cout << "Cached: " << key << " (" << size << " bytes)\n";
    }

    // Retrieve from cache
    auto retrieved = cache.get("image_2");
    if (retrieved) {
        cout << "Retrieved image_2 from cache\n";
        cout << "Size: " << (*retrieved)->cols << "x" << (*retrieved)->rows
             << "\n";
    }

    // Check cache statistics
    auto stats = cache.getStats();
    cout << "\nCache Statistics:\n";
    cout << "  Hits: " << stats.hitCount << "\n";
    cout << "  Misses: " << stats.missCount << "\n";
    cout << "  Evictions: " << stats.evictionCount << "\n";
    cout << "  Current size: " << stats.currentSize << " bytes\n";
}

/**
 * @brief Demonstrate cache eviction policies
 */
void demonstrateCacheEviction() {
    cout << "\n=== Cache Eviction Policies ===\n";

    // Small cache to trigger eviction
    CacheManager<string> cache(100, CachePolicy::LRU);

    // Add items until eviction occurs
    for (int i = 0; i < 10; ++i) {
        auto data = make_shared<string>("Data item " + to_string(i));
        cache.put("key_" + to_string(i), data, 20);
    }

    auto stats = cache.getStats();
    cout << "After adding 10 items (20 bytes each) to 100-byte cache:\n";
    cout << "  Evictions: " << stats.evictionCount << "\n";
    cout << "  Current size: " << stats.currentSize << " bytes\n";

    // Check which items remain
    cout << "\nRemaining items:\n";
    for (int i = 0; i < 10; ++i) {
        string key = "key_" + to_string(i);
        if (cache.contains(key)) {
            cout << "  " << key << " is in cache\n";
        }
    }
}

/**
 * @brief Demonstrate cache with image blobs
 */
void demonstrateBlobCaching() {
    cout << "\n=== Blob Caching ===\n";

    CacheManager<blob> cache(50 * 1024 * 1024, CachePolicy::LRU);

    // Create and cache blobs
    for (int i = 0; i < 3; ++i) {
        auto img = make_shared<blob>(480, 640, 3);
        string key = "blob_" + to_string(i);
        size_t size = img->size();

        cache.put(key, img, size);
        cout << "Cached blob: " << key << " (" << size << " bytes)\n";
    }

    // Access pattern that affects LRU
    cache.get("blob_0");
    cache.get("blob_1");
    cache.get("blob_0");  // blob_0 accessed twice

    auto stats = cache.getStats();
    cout << "\nAfter access pattern:\n";
    cout << "  Cache hits: " << stats.hitCount << "\n";
    cout << "  Hit rate: "
         << (stats.hitCount * 100.0 / (stats.hitCount + stats.missCount))
         << "%\n";
}

/**
 * @brief Demonstrate cache management operations
 */
void demonstrateCacheManagement() {
    cout << "\n=== Cache Management ===\n";

    CacheManager<int> cache(1000, CachePolicy::LRU);

    // Add items
    for (int i = 0; i < 5; ++i) {
        cache.put("item_" + to_string(i), make_shared<int>(i * 10),
                  sizeof(int));
    }

    cout << "Initial cache size: " << cache.getStats().currentSize
         << " bytes\n";

    // Remove specific item
    cache.remove("item_2");
    cout << "After removing item_2: " << cache.getStats().currentSize
         << " bytes\n";

    // Change max size
    cache.setMaxSize(50);
    cout << "After reducing max size to 50 bytes: "
         << cache.getStats().currentSize << " bytes\n";

    // Clear cache
    cache.clear();
    cout << "After clearing: " << cache.getStats().currentSize << " bytes\n";
}

/**
 * @brief Demonstrate cache hit rate optimization
 */
void demonstrateHitRateOptimization() {
    cout << "\n=== Hit Rate Optimization ===\n";

    CacheManager<string> cache(500, CachePolicy::LFU);

    // Add items
    for (int i = 0; i < 10; ++i) {
        cache.put("item_" + to_string(i), make_shared<string>("data"), 10);
    }

    // Access pattern: frequently access some items
    for (int i = 0; i < 20; ++i) {
        cache.get("item_0");  // Very frequent
        cache.get("item_1");  // Very frequent
        if (i % 2 == 0)
            cache.get("item_2");  // Moderate
        if (i % 5 == 0)
            cache.get("item_3");  // Infrequent
    }

    auto stats = cache.getStats();
    cout << "Access pattern results:\n";
    cout << "  Total accesses: " << (stats.hitCount + stats.missCount) << "\n";
    cout << "  Hit rate: "
         << (stats.hitCount * 100.0 / (stats.hitCount + stats.missCount))
         << "%\n";
}

int main() {
    demonstrateBasicCaching();
    demonstrateCacheEviction();
    demonstrateBlobCaching();
    demonstrateCacheManagement();
    demonstrateHitRateOptimization();

    return 0;
}
