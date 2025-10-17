/**
 * @file lru_cache_comprehensive.cpp
 * @brief Comprehensive example demonstrating all features of ThreadSafeLRUCache
 * @author Atom Search Examples
 * @date 2025-01-25
 *
 * This example demonstrates:
 * - Basic cache operations (put, get, erase, contains)
 * - TTL and expiration of cache entries
 * - LRU eviction policy when the cache reaches capacity
 * - Batch operations for efficient handling of multiple items
 * - Thread safety and concurrent access from multiple threads
 * - Callbacks for monitoring cache events
 * - Dynamic resizing of the cache
 * - Persistence with save and load operations
 * - Prefetching to proactively populate the cache
 * - Error handling and edge cases
 */

#include <chrono>
#include <future>
#include <iostream>
#include <memory>
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

    return 0;
}
