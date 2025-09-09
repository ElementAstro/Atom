/*
 * caching_prefetching.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file caching_prefetching.cpp
 * @brief Image caching and prefetching example for Atom Image library
 * 
 * This example demonstrates:
 * - Image caching strategies
 * - Prefetching mechanisms
 * - Memory management
 * - Cache eviction policies
 * - Performance optimization
 */

#include <iostream>
#include <unordered_map>
#include <list>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <future>
#include <mutex>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

/**
 * @brief Simple image data structure
 */
struct ImageData {
    string path;
    size_t width;
    size_t height;
    size_t channels;
    vector<uint8_t> data;
    chrono::steady_clock::time_point load_time;
    size_t access_count;
    
    ImageData(const string& p, size_t w, size_t h, size_t c) 
        : path(p), width(w), height(h), channels(c), 
          load_time(chrono::steady_clock::now()), access_count(0) {
        data.resize(w * h * c);
        // Simulate image data
        fill(data.begin(), data.end(), 128);
    }
    
    size_t getMemorySize() const {
        return data.size() + path.size() + sizeof(*this);
    }
};

/**
 * @brief LRU Cache for images
 */
class ImageCache {
private:
    struct CacheNode {
        string key;
        shared_ptr<ImageData> image;
        list<CacheNode>::iterator list_iter;
    };
    
    size_t max_size_;
    size_t max_memory_;
    size_t current_memory_;
    unordered_map<string, CacheNode> cache_map_;
    list<CacheNode> lru_list_;
    mutable mutex cache_mutex_;
    
public:
    ImageCache(size_t max_size = 100, size_t max_memory_mb = 512) 
        : max_size_(max_size), max_memory_(max_memory_mb * 1024 * 1024), 
          current_memory_(0) {}
    
    /**
     * @brief Get image from cache or load if not present
     */
    shared_ptr<ImageData> getImage(const string& path) {
        lock_guard<mutex> lock(cache_mutex_);
        
        auto it = cache_map_.find(path);
        if (it != cache_map_.end()) {
            // Move to front (most recently used)
            lru_list_.splice(lru_list_.begin(), lru_list_, it->second.list_iter);
            it->second.image->access_count++;
            
            cout << "Cache HIT: " << path << " (access count: " 
                 << it->second.image->access_count << ")" << endl;
            return it->second.image;
        }
        
        cout << "Cache MISS: " << path << " - Loading..." << endl;
        
        // Load image (simulate)
        auto image = loadImageFromDisk(path);
        if (!image) {
            return nullptr;
        }
        
        // Add to cache
        addToCache(path, image);
        
        return image;
    }
    
    /**
     * @brief Prefetch images in background
     */
    void prefetchImages(const vector<string>& paths) {
        cout << "Prefetching " << paths.size() << " images..." << endl;
        
        vector<future<void>> futures;
        
        for (const auto& path : paths) {
            futures.push_back(async(launch::async, [this, path]() {
                this->getImage(path);
            }));
        }
        
        // Wait for all prefetch operations to complete
        for (auto& future : futures) {
            future.wait();
        }
        
        cout << "Prefetching completed." << endl;
    }
    
    /**
     * @brief Get cache statistics
     */
    void printCacheStats() const {
        lock_guard<mutex> lock(cache_mutex_);
        
        cout << "\n=== Cache Statistics ===" << endl;
        cout << "Cache size: " << cache_map_.size() << "/" << max_size_ << endl;
        cout << "Memory usage: " << (current_memory_ / 1024 / 1024) 
             << "/" << (max_memory_ / 1024 / 1024) << " MB" << endl;
        
        cout << "Cached images:" << endl;
        for (const auto& node : lru_list_) {
            cout << "  " << fs::path(node.image->path).filename().string() 
                 << " (" << node.image->access_count << " accesses, "
                 << (node.image->getMemorySize() / 1024) << " KB)" << endl;
        }
    }
    
    /**
     * @brief Clear cache
     */
    void clearCache() {
        lock_guard<mutex> lock(cache_mutex_);
        cache_map_.clear();
        lru_list_.clear();
        current_memory_ = 0;
        cout << "Cache cleared." << endl;
    }
    
private:
    shared_ptr<ImageData> loadImageFromDisk(const string& path) {
        // Simulate loading time
        this_thread::sleep_for(chrono::milliseconds(50 + rand() % 100));
        
        // Create mock image data
        size_t width = 800 + rand() % 400;
        size_t height = 600 + rand() % 300;
        size_t channels = 3;
        
        return make_shared<ImageData>(path, width, height, channels);
    }
    
    void addToCache(const string& path, shared_ptr<ImageData> image) {
        // Check if we need to evict items
        while ((cache_map_.size() >= max_size_ || 
                current_memory_ + image->getMemorySize() > max_memory_) &&
               !lru_list_.empty()) {
            evictLeastRecentlyUsed();
        }
        
        // Add new item to front of list
        lru_list_.emplace_front();
        auto& new_node = lru_list_.front();
        new_node.key = path;
        new_node.image = image;
        new_node.list_iter = lru_list_.begin();
        
        // Add to map
        cache_map_[path] = new_node;
        current_memory_ += image->getMemorySize();
        
        cout << "Added to cache: " << fs::path(path).filename().string() 
             << " (" << (image->getMemorySize() / 1024) << " KB)" << endl;
    }
    
    void evictLeastRecentlyUsed() {
        if (lru_list_.empty()) return;
        
        auto& last_node = lru_list_.back();
        current_memory_ -= last_node.image->getMemorySize();
        
        cout << "Evicted from cache: " << fs::path(last_node.key).filename().string() 
             << " (accessed " << last_node.image->access_count << " times)" << endl;
        
        cache_map_.erase(last_node.key);
        lru_list_.pop_back();
    }
};

/**
 * @brief Prefetching strategy manager
 */
class PrefetchManager {
private:
    ImageCache& cache_;
    
public:
    explicit PrefetchManager(ImageCache& cache) : cache_(cache) {}
    
    /**
     * @brief Sequential prefetching strategy
     */
    void sequentialPrefetch(const vector<string>& paths, size_t lookahead = 3) {
        cout << "\nSequential prefetching (lookahead: " << lookahead << "):" << endl;
        
        for (size_t i = 0; i < paths.size(); ++i) {
            // Process current image
            cout << "Processing: " << fs::path(paths[i]).filename().string() << endl;
            auto current = cache_.getImage(paths[i]);
            
            // Prefetch next images
            vector<string> prefetch_paths;
            for (size_t j = i + 1; j < min(i + 1 + lookahead, paths.size()); ++j) {
                prefetch_paths.push_back(paths[j]);
            }
            
            if (!prefetch_paths.empty()) {
                thread prefetch_thread([this, prefetch_paths]() {
                    this->cache_.prefetchImages(prefetch_paths);
                });
                prefetch_thread.detach();
            }
            
            // Simulate processing time
            this_thread::sleep_for(chrono::milliseconds(200));
        }
    }
    
    /**
     * @brief Predictive prefetching based on access patterns
     */
    void predictivePrefetch(const vector<string>& frequently_accessed) {
        cout << "\nPredictive prefetching for frequently accessed images:" << endl;
        cache_.prefetchImages(frequently_accessed);
    }
};

/**
 * @brief Demonstrate caching and prefetching
 */
void demonstrateCachingPrefetching() {
    cout << "=== Image Caching and Prefetching Demo ===" << endl;
    
    // Create image cache
    ImageCache cache(10, 64); // 10 images max, 64MB max memory
    
    // Sample image paths
    vector<string> image_paths = {
        "image001.jpg", "image002.jpg", "image003.jpg", "image004.jpg",
        "image005.jpg", "image006.jpg", "image007.jpg", "image008.jpg",
        "image009.jpg", "image010.jpg", "image011.jpg", "image012.jpg"
    };
    
    // 1. Basic caching demonstration
    cout << "\n1. Basic Caching:" << endl;
    for (int i = 0; i < 3; ++i) {
        cout << "\nRound " << (i + 1) << ":" << endl;
        for (const auto& path : vector<string>(image_paths.begin(), image_paths.begin() + 5)) {
            cache.getImage(path);
        }
    }
    
    cache.printCacheStats();
    
    // 2. Prefetching demonstration
    cout << "\n2. Prefetching:" << endl;
    PrefetchManager prefetch_manager(cache);
    
    // Sequential prefetching
    prefetch_manager.sequentialPrefetch(
        vector<string>(image_paths.begin() + 5, image_paths.end()), 2);
    
    cache.printCacheStats();
    
    // 3. Cache overflow demonstration
    cout << "\n3. Cache Overflow (LRU eviction):" << endl;
    for (const auto& path : image_paths) {
        cache.getImage(path);
    }
    
    cache.printCacheStats();
    
    // 4. Predictive prefetching
    cout << "\n4. Predictive Prefetching:" << endl;
    vector<string> frequently_accessed = {"image001.jpg", "image005.jpg", "image010.jpg"};
    prefetch_manager.predictivePrefetch(frequently_accessed);
    
    cache.printCacheStats();
}

/**
 * @brief Main function
 */
int main() {
    try {
        cout << "Image Caching and Prefetching Example" << endl;
        cout << "====================================" << endl;
        
        srand(static_cast<unsigned>(time(nullptr)));
        
        demonstrateCachingPrefetching();
        
        cout << "\nCaching and prefetching demonstration completed!" << endl;
        return 0;
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
}
