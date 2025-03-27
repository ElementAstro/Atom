/**
 * @file lru_example.cpp
 * @brief Comprehensive example demonstrating all features of ThreadSafeLRUCache
 * @author Example Author
 * @date 2025-03-23
 */

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "atom/search/lru.hpp"

// Helper function to print section titles
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

// Sample struct that will be stored in the cache
struct UserProfile {
    int id;
    std::string username;
    std::string email;
    int age;

    bool operator==(const UserProfile& other) const {
        return id == other.id && username == other.username &&
               email == other.email && age == other.age;
    }

    // Display method for easier debugging
    std::string toString() const {
        return "UserProfile{id=" + std::to_string(id) + ", username='" +
               username + "', email='" + email +
               "', age=" + std::to_string(age) + "}";
    }
};

// Create a sample user for testing
UserProfile createSampleUser(int id) {
    return {
        id, "user" + std::to_string(id),
        "user" + std::to_string(id) + "@example.com",
        20 + (id % 50)  // Age between 20-69
    };
}

// Function to simulate a slow database lookup
UserProfile simulateDatabaseLookup(const int& userId) {
    // Simulate database access delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return createSampleUser(userId);
}

// Function to run cache operations in separate threads for testing thread
// safety
void threadFunction(atom::search::ThreadSafeLRUCache<int, UserProfile>& cache,
                    int threadId, int operationsCount) {
    std::cout << "Thread " << threadId << " started" << std::endl;

    for (int i = 0; i < operationsCount; ++i) {
        int userId = threadId * 1000 + i;

        // Mix of operations to test thread safety
        switch (i % 5) {
            case 0: {
                // Insert new item
                cache.put(userId, createSampleUser(userId));
                std::cout << "Thread " << threadId << ": Added user " << userId
                          << std::endl;
                break;
            }
            case 1: {
                // Get an item
                auto user = cache.get(userId - 1);
                if (user) {
                    std::cout << "Thread " << threadId << ": Found user "
                              << userId - 1 << std::endl;
                }
                break;
            }
            case 2: {
                // Erase an item
                if (i > 0) {
                    cache.erase(userId - 2);
                    std::cout << "Thread " << threadId << ": Erased user "
                              << userId - 2 << std::endl;
                }
                break;
            }
            case 3: {
                // Check if contains
                bool contains = cache.contains(userId);
                std::cout << "Thread " << threadId << ": Cache "
                          << (contains ? "contains" : "does not contain")
                          << " user " << userId << std::endl;
                break;
            }
            case 4: {
                // Get shared pointer to item
                auto userPtr = cache.getShared(userId);
                if (userPtr) {
                    std::cout << "Thread " << threadId
                              << ": Got shared ptr to user " << userId
                              << std::endl;
                }
                break;
            }
        }

        // Short sleep to mix thread operations
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Thread " << threadId << " completed" << std::endl;
}

int main() {
    std::cout << "THREAD-SAFE LRU CACHE COMPREHENSIVE EXAMPLES\n";
    std::cout << "===========================================\n";

    //--------------------------------------------------------------------------
    // 1. Basic Cache Operations
    //--------------------------------------------------------------------------
    printSection("1. Basic Cache Operations");

    // Create a cache with maximum size of 10
    atom::search::ThreadSafeLRUCache<int, UserProfile> cache(10);
    std::cout << "Created a ThreadSafeLRUCache with maximum size: 10"
              << std::endl;

    // Insert some users
    std::cout << "\nInserting users into the cache..." << std::endl;
    for (int i = 1; i <= 5; ++i) {
        UserProfile user = createSampleUser(i);
        cache.put(i, user);
        std::cout << "Inserted: " << user.toString() << std::endl;
    }

    // Get a user
    std::cout << "\nRetrieving a user from the cache..." << std::endl;
    auto user = cache.get(3);
    if (user) {
        std::cout << "Retrieved user 3: " << user->toString() << std::endl;
    } else {
        std::cout << "User 3 not found in cache (unexpected)" << std::endl;
    }

    // Try to get a non-existent user
    std::cout << "\nAttempting to retrieve a non-existent user..." << std::endl;
    auto nonExistentUser = cache.get(999);
    if (nonExistentUser) {
        std::cout << "Retrieved user 999 (unexpected)" << std::endl;
    } else {
        std::cout << "User 999 not found in cache (expected)" << std::endl;
    }

    // Check if contains
    std::cout << "\nChecking if cache contains certain users..." << std::endl;
    std::cout << "Contains user 2: " << (cache.contains(2) ? "Yes" : "No")
              << std::endl;
    std::cout << "Contains user 999: " << (cache.contains(999) ? "Yes" : "No")
              << std::endl;

    // Erase a user
    std::cout << "\nErasing a user from the cache..." << std::endl;
    bool erased = cache.erase(4);
    std::cout << "User 4 was " << (erased ? "successfully erased" : "not found")
              << std::endl;
    std::cout << "Contains user 4 after erase: "
              << (cache.contains(4) ? "Yes" : "No") << std::endl;

    // Get cache size
    std::cout << "\nCache size: " << cache.size() << std::endl;
    std::cout << "Cache max size: " << cache.maxSize() << std::endl;
    std::cout << "Cache load factor: " << cache.loadFactor() << std::endl;

    //--------------------------------------------------------------------------
    // 2. Expiry and TTL (Time-To-Live)
    //--------------------------------------------------------------------------
    printSection("2. Expiry and TTL (Time-To-Live)");

    // Insert a user with a short TTL
    std::cout << "Inserting a user with a 2-second TTL..." << std::endl;
    UserProfile shortLivedUser = createSampleUser(100);
    cache.put(100, shortLivedUser, std::chrono::seconds(2));
    std::cout << "Inserted: " << shortLivedUser.toString() << std::endl;

    // Verify it exists
    std::cout << "\nVerifying user exists immediately after insertion..."
              << std::endl;
    std::cout << "Contains user 100: " << (cache.contains(100) ? "Yes" : "No")
              << std::endl;

    // Wait for the TTL to expire
    std::cout << "\nWaiting for user TTL to expire (3 seconds)..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Check if the user still exists
    std::cout << "Contains user 100 after TTL: "
              << (cache.contains(100) ? "Yes" : "No") << std::endl;

    // Try to get the expired user
    auto expiredUser = cache.get(100);
    if (expiredUser) {
        std::cout << "Retrieved user 100 after TTL (unexpected): "
                  << expiredUser->toString() << std::endl;
    } else {
        std::cout << "User 100 not found after TTL (expected)" << std::endl;
    }

    // Explicitly prune expired entries
    std::cout << "\nExplicitly pruning expired entries..." << std::endl;
    size_t prunedCount = cache.pruneExpired();
    std::cout << "Pruned " << prunedCount << " expired entries" << std::endl;

    //--------------------------------------------------------------------------
    // 3. LRU Eviction Policy
    //--------------------------------------------------------------------------
    printSection("3. LRU Eviction Policy");

    // Create a small cache to demonstrate LRU eviction
    atom::search::ThreadSafeLRUCache<int, UserProfile> smallCache(3);
    std::cout << "Created a small cache with maximum size: 3" << std::endl;

    // Insert users up to capacity
    std::cout << "\nInserting users up to capacity..." << std::endl;
    for (int i = 1; i <= 3; ++i) {
        UserProfile user = createSampleUser(i);
        smallCache.put(i, user);
        std::cout << "Inserted: " << user.toString() << std::endl;
    }

    // Get a user to update its position in the LRU list
    std::cout << "\nAccessing user 1 to update its LRU position..."
              << std::endl;
    auto user1 = smallCache.get(1);
    if (user1) {
        std::cout << "Accessed: " << user1->toString() << std::endl;
    }

    // Insert a new user, which should evict the least recently used
    std::cout << "\nInserting a new user, which should evict the LRU item..."
              << std::endl;
    UserProfile user4 = createSampleUser(4);
    smallCache.put(4, user4);
    std::cout << "Inserted: " << user4.toString() << std::endl;

    // Check which users remain in the cache
    std::cout << "\nChecking which users remain in the cache..." << std::endl;
    for (int i = 1; i <= 4; ++i) {
        std::cout << "Contains user " << i << ": "
                  << (smallCache.contains(i) ? "Yes" : "No") << std::endl;
    }

    // Manually pop the LRU item
    std::cout << "\nManually popping the LRU item..." << std::endl;
    auto poppedItem = smallCache.popLru();
    if (poppedItem) {
        std::cout << "Popped LRU item: User ID " << poppedItem->first << " - "
                  << poppedItem->second.toString() << std::endl;
    } else {
        std::cout << "No item to pop (unexpected)" << std::endl;
    }

    // Check cache size after popping
    std::cout << "Cache size after popping: " << smallCache.size() << std::endl;

    //--------------------------------------------------------------------------
    // 4. Batch Operations
    //--------------------------------------------------------------------------
    printSection("4. Batch Operations");

    // Create a new cache for batch operations
    atom::search::ThreadSafeLRUCache<int, UserProfile> batchCache(20);
    std::cout << "Created a cache for batch operations with size: 20"
              << std::endl;

    // Prepare batch of users
    std::cout << "\nPreparing a batch of users..." << std::endl;
    std::vector<std::pair<int, UserProfile>> userBatch;
    for (int i = 101; i <= 110; ++i) {
        userBatch.emplace_back(i, createSampleUser(i));
    }

    // Insert batch
    std::cout << "Inserting batch of " << userBatch.size() << " users..."
              << std::endl;
    batchCache.putBatch(userBatch, std::chrono::minutes(10));
    std::cout << "Batch inserted, cache size: " << batchCache.size()
              << std::endl;

    // Prepare batch of keys to retrieve
    std::cout << "\nPreparing a batch of keys to retrieve..." << std::endl;
    std::vector<int> keyBatch = {101, 103, 105,
                                 107, 109, 200};  // 200 doesn't exist

    // Get batch
    std::cout << "Retrieving batch of " << keyBatch.size() << " users..."
              << std::endl;
    auto resultBatch = batchCache.getBatch(keyBatch);

    // Display batch results
    std::cout << "Batch retrieval results:" << std::endl;
    for (size_t i = 0; i < keyBatch.size(); ++i) {
        if (resultBatch[i]) {
            std::cout << "  Key " << keyBatch[i] << ": Found - "
                      << resultBatch[i]->toString() << std::endl;
        } else {
            std::cout << "  Key " << keyBatch[i] << ": Not found" << std::endl;
        }
    }

    // Get all keys from the cache
    std::cout << "\nRetrieving all keys from the cache..." << std::endl;
    auto allKeys = batchCache.keys();
    std::cout << "Total keys: " << allKeys.size() << std::endl;
    std::cout << "First few keys: ";
    for (size_t i = 0; i < std::min(allKeys.size(), size_t(5)); ++i) {
        std::cout << allKeys[i] << " ";
    }
    std::cout << std::endl;

    //--------------------------------------------------------------------------
    // 5. Thread Safety and Concurrent Access
    //--------------------------------------------------------------------------
    printSection("5. Thread Safety and Concurrent Access");

    // Create a cache for concurrent access
    atom::search::ThreadSafeLRUCache<int, UserProfile> concurrentCache(50);
    std::cout << "Created a cache for concurrent access with size: 50"
              << std::endl;

    // Set up multiple threads to operate on the cache concurrently
    std::cout
        << "\nStarting multiple threads to access the cache concurrently..."
        << std::endl;
    std::vector<std::thread> threads;
    const int numThreads = 5;
    const int operationsPerThread = 20;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(threadFunction, std::ref(concurrentCache), i,
                             operationsPerThread);
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "\nAll threads completed" << std::endl;
    std::cout << "Final cache size: " << concurrentCache.size() << std::endl;
    std::cout << "Cache statistics: " << std::endl;
    auto stats = concurrentCache.getStatistics();
    std::cout << "  Hit count: " << stats.hitCount << std::endl;
    std::cout << "  Miss count: " << stats.missCount << std::endl;
    std::cout << "  Hit rate: " << (stats.hitRate * 100) << "%" << std::endl;
    std::cout << "  Current size: " << stats.size << std::endl;
    std::cout << "  Max size: " << stats.maxSize << std::endl;
    std::cout << "  Load factor: " << stats.loadFactor << std::endl;

    //--------------------------------------------------------------------------
    // 6. Callbacks and Event Handling
    //--------------------------------------------------------------------------
    printSection("6. Callbacks and Event Handling");

    // Create a cache with callbacks
    atom::search::ThreadSafeLRUCache<int, UserProfile> callbackCache(10);
    std::cout << "Created a cache with callbacks, size: 10" << std::endl;

    // Set insert callback
    callbackCache.setInsertCallback(
        [](const int& key, const UserProfile& value) {
            std::cout << "Insert callback: User " << key << " added - "
                      << value.toString() << std::endl;
        });

    // Set erase callback
    callbackCache.setEraseCallback([](const int& key) {
        std::cout << "Erase callback: User " << key << " removed" << std::endl;
    });

    // Set clear callback
    callbackCache.setClearCallback([]() {
        std::cout << "Clear callback: Cache has been cleared" << std::endl;
    });

    // Demonstrate callbacks with operations
    std::cout << "\nDemonstrating callbacks..." << std::endl;
    std::cout << "Inserting users:" << std::endl;
    for (int i = 201; i <= 205; ++i) {
        callbackCache.put(i, createSampleUser(i));
    }

    std::cout << "\nErasing a user:" << std::endl;
    callbackCache.erase(203);

    std::cout << "\nClearing the cache:" << std::endl;
    callbackCache.clear();

    //--------------------------------------------------------------------------
    // 7. Resizing the Cache
    //--------------------------------------------------------------------------
    printSection("7. Resizing the Cache");

    // Create a cache for resize demonstration
    atom::search::ThreadSafeLRUCache<int, UserProfile> resizeCache(5);
    std::cout << "Created a cache with initial size: 5" << std::endl;

    // Fill the cache
    std::cout << "\nFilling the cache..." << std::endl;
    for (int i = 301; i <= 305; ++i) {
        resizeCache.put(i, createSampleUser(i));
    }
    std::cout << "Cache size after filling: " << resizeCache.size()
              << std::endl;

    // Resize the cache to a smaller size
    std::cout << "\nResizing cache to a smaller size (3)..." << std::endl;
    resizeCache.resize(3);
    std::cout << "Cache size after resize: " << resizeCache.size() << std::endl;
    std::cout << "Cache max size after resize: " << resizeCache.maxSize()
              << std::endl;

    // Check which users remain
    std::cout << "Checking which users remain:" << std::endl;
    for (int i = 301; i <= 305; ++i) {
        std::cout << "  Contains user " << i << ": "
                  << (resizeCache.contains(i) ? "Yes" : "No") << std::endl;
    }

    // Resize to a larger size
    std::cout << "\nResizing cache to a larger size (10)..." << std::endl;
    resizeCache.resize(10);
    std::cout << "Cache size after resize: " << resizeCache.size() << std::endl;
    std::cout << "Cache max size after resize: " << resizeCache.maxSize()
              << std::endl;

    // Add more users to fill the expanded space
    std::cout << "\nAdding more users to fill expanded space..." << std::endl;
    for (int i = 306; i <= 310; ++i) {
        resizeCache.put(i, createSampleUser(i));
    }
    std::cout << "Cache size after adding more users: " << resizeCache.size()
              << std::endl;

    //--------------------------------------------------------------------------
    // 8. Persistence - Save and Load
    //--------------------------------------------------------------------------
    printSection("8. Persistence - Save and Load");

    // Create a cache with data to be saved
    atom::search::ThreadSafeLRUCache<int, UserProfile> persistentCache(10);
    std::cout << "Created a cache for persistence demonstration, size: 10"
              << std::endl;

    // Fill with some data
    std::cout << "\nFilling cache with data to be saved..." << std::endl;
    for (int i = 401; i <= 405; ++i) {
        // Add some users with TTL, some without
        if (i % 2 == 0) {
            persistentCache.put(i, createSampleUser(i),
                                std::chrono::minutes(30));
            std::cout << "  Added user " << i << " with 30-minute TTL"
                      << std::endl;
        } else {
            persistentCache.put(i, createSampleUser(i));
            std::cout << "  Added user " << i << " with no TTL" << std::endl;
        }
    }

    // Save the cache to a file
    const std::string cacheFile = "lru_cache_data.bin";
    std::cout << "\nSaving cache to file: " << cacheFile << std::endl;
    try {
        persistentCache.saveToFile(cacheFile);
        std::cout << "Cache successfully saved" << std::endl;
    } catch (const atom::search::LRUCacheException& e) {
        std::cout << "Failed to save cache: " << e.what() << std::endl;
    }

    // Create a new cache and load from file
    atom::search::ThreadSafeLRUCache<int, UserProfile> loadedCache(20);
    std::cout << "\nCreated a new cache with size 20" << std::endl;
    std::cout << "Loading cache from file: " << cacheFile << std::endl;

    try {
        loadedCache.loadFromFile(cacheFile);
        std::cout << "Cache successfully loaded" << std::endl;
    } catch (const atom::search::LRUCacheException& e) {
        std::cout << "Failed to load cache: " << e.what() << std::endl;
    }

    // Verify the loaded data
    std::cout << "\nVerifying loaded data..." << std::endl;
    std::cout << "Loaded cache size: " << loadedCache.size() << std::endl;
    for (int i = 401; i <= 405; ++i) {
        auto loadedUser = loadedCache.get(i);
        if (loadedUser) {
            std::cout << "  User " << i << " found: " << loadedUser->toString()
                      << std::endl;
        } else {
            std::cout << "  User " << i << " not found" << std::endl;
        }
    }

    // Clean up the cache file
    std::remove(cacheFile.c_str());

    //--------------------------------------------------------------------------
    // 9. Prefetching
    //--------------------------------------------------------------------------
    printSection("9. Prefetching");

    // Create a cache for prefetch demonstration
    atom::search::ThreadSafeLRUCache<int, UserProfile> prefetchCache(20);
    std::cout << "Created a cache for prefetch demonstration, size: 20"
              << std::endl;

    // Initialize with a few items
    std::cout << "\nInitializing with a few items..." << std::endl;
    for (int i = 501; i <= 503; ++i) {
        prefetchCache.put(i, createSampleUser(i));
    }

    // Create a list of keys to prefetch
    std::cout << "\nPrefetching a batch of users..." << std::endl;
    std::vector<int> prefetchKeys = {501, 504, 505, 506,
                                     507};  // Mix of existing and new

    // Prefetch the keys
    size_t prefetchedCount = prefetchCache.prefetch(
        prefetchKeys,
        simulateDatabaseLookup,  // Function to load missing items
        std::chrono::minutes(5)  // TTL for prefetched items
    );

    std::cout << "Successfully prefetched " << prefetchedCount << " users"
              << std::endl;

    // Verify the prefetched data
    std::cout << "\nVerifying prefetched data..." << std::endl;
    for (int key : prefetchKeys) {
        auto user = prefetchCache.get(key);
        if (user) {
            std::cout << "  User " << key << " found: " << user->toString()
                      << std::endl;
        } else {
            std::cout << "  User " << key << " not found" << std::endl;
        }
    }

    // Check cache statistics
    auto prefetchStats = prefetchCache.getStatistics();
    std::cout << "\nCache statistics after prefetching:" << std::endl;
    std::cout << "  Hit rate: " << (prefetchStats.hitRate * 100) << "%"
              << std::endl;
    std::cout << "  Cache size: " << prefetchStats.size << std::endl;

    //--------------------------------------------------------------------------
    // 10. Error Handling and Edge Cases
    //--------------------------------------------------------------------------
    printSection("10. Error Handling and Edge Cases");

    // Try to create a cache with invalid size
    std::cout << "Attempting to create a cache with size 0..." << std::endl;
    try {
        atom::search::ThreadSafeLRUCache<int, UserProfile> invalidCache(0);
        std::cout << "Created cache (unexpected)" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Exception caught (expected): " << e.what() << std::endl;
    }

    // Create a proper cache for edge cases
    atom::search::ThreadSafeLRUCache<int, UserProfile> edgeCache(2);
    std::cout << "\nCreated a tiny cache with size 2 for edge cases"
              << std::endl;

    // Test with empty cache
    std::cout << "\nTesting operations on empty cache:" << std::endl;

    auto emptyGet = edgeCache.get(999);
    std::cout << "  Get on empty cache: "
              << (emptyGet ? "Found (unexpected)" : "Not found (expected)")
              << std::endl;

    bool emptyErase = edgeCache.erase(999);
    std::cout << "  Erase on empty cache: "
              << (emptyErase ? "Succeeded (unexpected)" : "Failed (expected)")
              << std::endl;

    auto emptyPop = edgeCache.popLru();
    std::cout << "  PopLru on empty cache: "
              << (emptyPop ? "Succeeded (unexpected)" : "Failed (expected)")
              << std::endl;

    // Test with cache of exactly max size
    std::cout << "\nFilling cache to exact capacity..." << std::endl;
    edgeCache.put(601, createSampleUser(601));
    edgeCache.put(602, createSampleUser(602));
    std::cout << "Cache size: " << edgeCache.size() << " (expected 2)"
              << std::endl;

    // Add one more to force eviction
    std::cout << "\nAdding one more item to force eviction..." << std::endl;
    edgeCache.put(603, createSampleUser(603));
    std::cout << "Cache size after adding: " << edgeCache.size()
              << " (should still be 2)" << std::endl;

    // Check which items remain
    std::cout << "Items in cache:" << std::endl;
    std::cout << "  Contains 601: " << (edgeCache.contains(601) ? "Yes" : "No")
              << " (should be No - evicted)" << std::endl;
    std::cout << "  Contains 602: " << (edgeCache.contains(602) ? "Yes" : "No")
              << " (should be Yes)" << std::endl;
    std::cout << "  Contains 603: " << (edgeCache.contains(603) ? "Yes" : "No")
              << " (should be Yes - newly added)" << std::endl;

    // Test failure cases with file operations
    std::cout << "\nTesting file operation failures:" << std::endl;

    try {
        edgeCache.saveToFile("/nonexistent/directory/file.bin");
        std::cout << "  Save to invalid path succeeded (unexpected)"
                  << std::endl;
    } catch (const atom::search::LRUCacheIOException& e) {
        std::cout << "  Save to invalid path failed (expected): " << e.what()
                  << std::endl;
    }

    try {
        edgeCache.loadFromFile("nonexistent_file.bin");
        std::cout << "  Load from nonexistent file succeeded (unexpected)"
                  << std::endl;
    } catch (const atom::search::LRUCacheIOException& e) {
        std::cout << "  Load from nonexistent file failed (expected): "
                  << e.what() << std::endl;
    }

    //--------------------------------------------------------------------------
    // Summary
    //--------------------------------------------------------------------------
    printSection("Summary");

    std::cout << "This example demonstrated the following ThreadSafeLRUCache "
                 "features:"
              << std::endl;
    std::cout << "  1. Basic cache operations (put, get, erase, contains)"
              << std::endl;
    std::cout << "  2. TTL and expiration of cache entries" << std::endl;
    std::cout << "  3. LRU eviction policy when the cache reaches capacity"
              << std::endl;
    std::cout
        << "  4. Batch operations for efficient handling of multiple items"
        << std::endl;
    std::cout
        << "  5. Thread safety and concurrent access from multiple threads"
        << std::endl;
    std::cout << "  6. Callbacks for monitoring cache events" << std::endl;
    std::cout << "  7. Dynamic resizing of the cache" << std::endl;
    std::cout << "  8. Persistence with save and load operations" << std::endl;
    std::cout << "  9. Prefetching to proactively populate the cache"
              << std::endl;
    std::cout << "  10. Error handling and edge cases" << std::endl;

    return 0;
}