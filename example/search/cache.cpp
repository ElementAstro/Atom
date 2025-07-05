/**
 * @file cache_example.cpp
 * @brief Comprehensive example demonstrating all features of ResourceCache
 * @author Example Author
 * @date 2025-03-23
 */

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "atom/search/cache.hpp"

// Helper function to print section titles
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

// A simple resource class that can be cached
class Resource {
public:
    Resource() : id_(-1), name_(""), data_("") {}

    Resource(int id, const std::string& name, const std::string& data)
        : id_(id), name_(name), data_(data) {}

    // Getters
    int getId() const { return id_; }
    std::string getName() const { return name_; }
    std::string getData() const { return data_; }

    // Setters
    void setId(int id) { id_ = id; }
    void setName(const std::string& name) { name_ = name; }
    void setData(const std::string& data) { data_ = data; }

    // Operators for comparison
    bool operator==(const Resource& other) const {
        return id_ == other.id_ && name_ == other.name_ && data_ == other.data_;
    }

    // ToString method for display
    std::string toString() const {
        std::stringstream ss;
        ss << "Resource[id=" << id_ << ", name=" << name_ << ", data=" << data_
           << "]";
        return ss.str();
    }

private:
    int id_;
    std::string name_;
    std::string data_;
};

// Implement to/from JSON conversion for Resource
nlohmann::json resourceToJson(const Resource& resource) {
    nlohmann::json j;
    j["id"] = resource.getId();
    j["name"] = resource.getName();
    j["data"] = resource.getData();
    return j;
}

Resource resourceFromJson(const nlohmann::json& j) {
    int id = j.at("id").get<int>();
    std::string name = j.at("name").get<std::string>();
    std::string data = j.at("data").get<std::string>();
    return Resource(id, name, data);
}

// Implement string serialization/deserialization for Resource
std::string resourceToString(const Resource& resource) {
    return std::to_string(resource.getId()) + "|" + resource.getName() + "|" +
           resource.getData();
}

Resource resourceFromString(const std::string& str) {
    std::istringstream iss(str);
    std::string idStr, name, data;

    std::getline(iss, idStr, '|');
    std::getline(iss, name, '|');
    std::getline(iss, data);

    int id = std::stoi(idStr);
    return Resource(id, name, data);
}

// Helper function to create sample resources
Resource createSampleResource(int index) {
    return Resource(index, "Resource-" + std::to_string(index),
                    "Sample data for resource " + std::to_string(index));
}

// Long-running operation for async examples
Resource loadResourceSlowly(int id) {
    // Simulate a time-consuming operation
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return Resource(id, "Async-Resource-" + std::to_string(id),
                    "Data loaded asynchronously for " + std::to_string(id));
}

int main() {
    std::cout << "RESOURCE CACHE COMPREHENSIVE EXAMPLES\n";
    std::cout << "====================================\n";

    //--------------------------------------------------------------------------
    // 1. Basic Resource Cache Usage
    //--------------------------------------------------------------------------
    printSection("1. Basic Resource Cache Usage");

    // Create a cache with a maximum size of 10 items
    atom::search::ResourceCache<Resource> cache(10);
    std::cout << "Created a ResourceCache with maximum size: 10" << std::endl;

    // Insert some resources
    std::cout << "\nInserting resources into the cache..." << std::endl;
    for (int i = 1; i <= 5; ++i) {
        Resource resource = createSampleResource(i);
        cache.insert("resource-" + std::to_string(i), resource,
                     std::chrono::seconds(60));
        std::cout << "Inserted: " << resource.toString() << std::endl;
    }

    // Check if resources exist
    std::cout << "\nChecking if resources exist..." << std::endl;
    std::cout << "Contains 'resource-1': "
              << (cache.contains("resource-1") ? "Yes" : "No") << std::endl;
    std::cout << "Contains 'resource-10': "
              << (cache.contains("resource-10") ? "Yes" : "No") << std::endl;

    // Retrieve resources
    std::cout << "\nRetrieving resources from the cache..." << std::endl;
    auto resource1 = cache.get("resource-1");
    if (resource1) {
        std::cout << "Retrieved: " << resource1->toString() << std::endl;
    } else {
        std::cout << "Resource not found!" << std::endl;
    }

    auto nonExistentResource = cache.get("non-existent-key");
    if (nonExistentResource) {
        std::cout << "Retrieved: " << nonExistentResource->toString()
                  << std::endl;
    } else {
        std::cout << "Resource 'non-existent-key' not found!" << std::endl;
    }

    // Cache size and emptiness check
    std::cout << "\nCache statistics:" << std::endl;
    std::cout << "Cache size: " << cache.size() << std::endl;
    std::cout << "Cache is empty: " << (cache.empty() ? "Yes" : "No")
              << std::endl;

    // Remove a resource
    std::cout << "\nRemoving resource-3 from the cache..." << std::endl;
    cache.remove("resource-3");
    std::cout << "Contains 'resource-3' after removal: "
              << (cache.contains("resource-3") ? "Yes" : "No") << std::endl;
    std::cout << "Cache size after removal: " << cache.size() << std::endl;

    //--------------------------------------------------------------------------
    // 2. Resource Expiration
    //--------------------------------------------------------------------------
    printSection("2. Resource Expiration");

    // Insert a resource with a short expiration time
    std::cout << "Inserting a resource with a 2-second expiration time..."
              << std::endl;
    Resource shortLivedResource(100, "Short-lived",
                                "This resource will expire quickly");
    cache.insert("short-lived", shortLivedResource, std::chrono::seconds(2));

    // Check if the resource exists
    std::cout << "Contains 'short-lived' immediately after insertion: "
              << (cache.contains("short-lived") ? "Yes" : "No") << std::endl;

    // Wait for the resource to expire
    std::cout << "Waiting for the resource to expire (3 seconds)..."
              << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Resource should have expired by now
    bool isExpired = cache.isExpired("short-lived");
    std::cout << "Is 'short-lived' resource expired: "
              << (isExpired ? "Yes" : "No") << std::endl;

    // Try to get the expired resource
    auto expiredResource = cache.get("short-lived");
    if (expiredResource) {
        std::cout << "Retrieved expired resource: "
                  << expiredResource->toString() << std::endl;
    } else {
        std::cout << "Expired resource 'short-lived' was automatically removed "
                     "from the cache"
                  << std::endl;
    }

    // Manually remove expired resources
    std::cout << "\nManually removing any other expired resources..."
              << std::endl;
    cache.removeExpired();
    std::cout << "Cache size after removing expired: " << cache.size()
              << std::endl;

    //--------------------------------------------------------------------------
    // 3. LRU Eviction Policy
    //--------------------------------------------------------------------------
    printSection("3. LRU Eviction Policy");

    // Create a small cache to demonstrate eviction
    atom::search::ResourceCache<Resource> smallCache(3);
    std::cout << "Created a small cache with maximum size: 3" << std::endl;

    // Insert resources up to capacity
    std::cout << "\nInserting resources up to capacity..." << std::endl;
    for (int i = 1; i <= 3; ++i) {
        Resource resource = createSampleResource(i);
        smallCache.insert("small-" + std::to_string(i), resource,
                          std::chrono::seconds(60));
        std::cout << "Inserted: " << resource.toString() << std::endl;
    }

    // Access one of the resources to update its LRU position
    std::cout << "\nAccessing 'small-1' to update its LRU position..."
              << std::endl;
    auto small1 = smallCache.get("small-1");
    if (small1) {
        std::cout << "Accessed: " << small1->toString() << std::endl;
    }

    // Insert a new resource, which should evict the least recently used
    std::cout
        << "\nInserting a new resource, which should evict the LRU item..."
        << std::endl;
    Resource newResource = createSampleResource(4);
    smallCache.insert("small-4", newResource, std::chrono::seconds(60));

    // Check which resource was evicted (should be small-2 since small-1 was
    // recently accessed)
    std::cout << "Contains 'small-1': "
              << (smallCache.contains("small-1") ? "Yes" : "No") << std::endl;
    std::cout << "Contains 'small-2': "
              << (smallCache.contains("small-2") ? "Yes" : "No") << std::endl;
    std::cout << "Contains 'small-3': "
              << (smallCache.contains("small-3") ? "Yes" : "No") << std::endl;
    std::cout << "Contains 'small-4': "
              << (smallCache.contains("small-4") ? "Yes" : "No") << std::endl;

    // Manually evict the oldest resource
    std::cout << "\nManually evicting the oldest resource..." << std::endl;
    smallCache.evictOldest();
    std::cout << "Cache size after eviction: " << smallCache.size()
              << std::endl;

    //--------------------------------------------------------------------------
    // 4. Asynchronous Operations
    //--------------------------------------------------------------------------
    printSection("4. Asynchronous Operations");

    // Asynchronous insertion
    std::cout << "Asynchronously inserting a resource..." << std::endl;
    Resource asyncResource(200, "Async-Resource",
                           "This resource is inserted asynchronously");
    auto insertFuture = cache.asyncInsert("async-resource", asyncResource,
                                          std::chrono::seconds(60));

    // Do other work while insertion is happening
    std::cout << "Doing other work while insertion is in progress..."
              << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Wait for insertion to complete
    insertFuture.wait();
    std::cout << "Async insertion completed" << std::endl;

    // Asynchronous retrieval
    std::cout << "\nAsynchronously retrieving a resource..." << std::endl;
    auto getFuture = cache.asyncGet("async-resource");

    // Do other work while retrieval is happening
    std::cout << "Doing other work while retrieval is in progress..."
              << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Get the result
    auto asyncResult = getFuture.get();
    if (asyncResult) {
        std::cout << "Async retrieval returned: " << asyncResult->toString()
                  << std::endl;
    } else {
        std::cout << "Async retrieval failed to find the resource" << std::endl;
    }

    // Asynchronous loading with a provider function
    std::cout
        << "\nAsynchronously loading a resource using a provider function..."
        << std::endl;
    auto loadFuture = cache.asyncLoad("computed-resource", []() -> Resource {
        // Simulate a time-consuming computation
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return Resource(300, "Computed-Resource",
                        "This resource was computed asynchronously");
    });

    std::cout << "Resource is being computed and loaded in the background..."
              << std::endl;

    // Wait for loading to complete
    loadFuture.wait();
    std::cout << "Async loading completed" << std::endl;

    // Verify the resource was loaded
    auto computedResource = cache.get("computed-resource");
    if (computedResource) {
        std::cout << "Retrieved computed resource: "
                  << computedResource->toString() << std::endl;
    } else {
        std::cout << "Failed to retrieve computed resource" << std::endl;
    }

    //--------------------------------------------------------------------------
    // 5. Batch Operations
    //--------------------------------------------------------------------------
    printSection("5. Batch Operations");

    // Prepare a batch of resources
    std::cout << "Preparing a batch of resources..." << std::endl;
    std::vector<std::pair<std::string, Resource>> batch;
    for (int i = 1; i <= 5; ++i) {
        batch.emplace_back("batch-" + std::to_string(i),
                           createSampleResource(i + 100));
    }

    // Insert the batch
    std::cout << "Inserting batch of " << batch.size() << " resources..."
              << std::endl;
    cache.insertBatch(batch, std::chrono::seconds(60));

    // Check if batch resources exist
    std::cout << "\nVerifying batch insertion..." << std::endl;
    for (int i = 1; i <= 5; ++i) {
        std::string key = "batch-" + std::to_string(i);
        std::cout << "Contains '" << key
                  << "': " << (cache.contains(key) ? "Yes" : "No") << std::endl;
    }

    // Remove a batch of resources
    std::cout << "\nRemoving a batch of resources..." << std::endl;
    std::vector<std::string> keysToRemove = {"batch-1", "batch-3", "batch-5"};
    cache.removeBatch(keysToRemove);

    // Verify removal
    std::cout << "Verifying batch removal..." << std::endl;
    for (int i = 1; i <= 5; ++i) {
        std::string key = "batch-" + std::to_string(i);
        std::cout << "Contains '" << key
                  << "': " << (cache.contains(key) ? "Yes" : "No") << std::endl;
    }

    //--------------------------------------------------------------------------
    // 6. Serialization and Persistence
    //--------------------------------------------------------------------------
    printSection("6. Serialization and Persistence");

    // Create some resources for serialization
    atom::search::ResourceCache<Resource> serializationCache(10);
    for (int i = 1; i <= 5; ++i) {
        Resource resource = createSampleResource(i + 200);
        serializationCache.insert("serial-" + std::to_string(i), resource,
                                  std::chrono::seconds(3600));
    }

    // Write to a text file
    std::cout << "Writing cache to a text file..." << std::endl;
    serializationCache.writeToFile("cache_data.txt", resourceToString);
    std::cout << "Cache saved to 'cache_data.txt'" << std::endl;

    // Create a new cache and read from the file
    atom::search::ResourceCache<Resource> loadedCache(10);
    std::cout << "\nLoading cache from text file..." << std::endl;
    loadedCache.readFromFile("cache_data.txt", resourceFromString);

    // Verify loaded resources
    std::cout << "Loaded cache size: " << loadedCache.size() << std::endl;
    for (int i = 1; i <= 5; ++i) {
        std::string key = "serial-" + std::to_string(i);
        auto resource = loadedCache.get(key);
        if (resource) {
            std::cout << "Loaded " << key << ": " << resource->toString()
                      << std::endl;
        } else {
            std::cout << "Failed to load " << key << std::endl;
        }
    }

    // Write to a JSON file
    std::cout << "\nWriting cache to a JSON file..." << std::endl;
    serializationCache.writeToJsonFile("cache_data.json", resourceToJson);
    std::cout << "Cache saved to 'cache_data.json'" << std::endl;

    // Create another new cache and read from the JSON file
    atom::search::ResourceCache<Resource> jsonLoadedCache(10);
    std::cout << "\nLoading cache from JSON file..." << std::endl;
    jsonLoadedCache.readFromJsonFile("cache_data.json", resourceFromJson);

    // Verify loaded resources
    std::cout << "JSON loaded cache size: " << jsonLoadedCache.size()
              << std::endl;
    for (int i = 1; i <= 5; ++i) {
        std::string key = "serial-" + std::to_string(i);
        auto resource = jsonLoadedCache.get(key);
        if (resource) {
            std::cout << "Loaded " << key << ": " << resource->toString()
                      << std::endl;
        } else {
            std::cout << "Failed to load " << key << std::endl;
        }
    }

    //--------------------------------------------------------------------------
    // 7. Event Callbacks
    //--------------------------------------------------------------------------
    printSection("7. Event Callbacks");

    // Create a cache with callbacks
    atom::search::ResourceCache<Resource> callbackCache(10);

    // Register callbacks
    std::cout << "Registering callbacks for insert and remove events..."
              << std::endl;

    callbackCache.onInsert([](const std::string& key) {
        std::cout << "Insert callback: Resource '" << key << "' was inserted"
                  << std::endl;
    });

    callbackCache.onRemove([](const std::string& key) {
        std::cout << "Remove callback: Resource '" << key << "' was removed"
                  << std::endl;
    });

    // Insert resources to trigger callbacks
    std::cout << "\nInserting resources to trigger callbacks..." << std::endl;
    callbackCache.insert("callback-1", createSampleResource(401),
                         std::chrono::seconds(60));
    callbackCache.insert("callback-2", createSampleResource(402),
                         std::chrono::seconds(60));

    // Remove a resource to trigger callback
    std::cout << "\nRemoving a resource to trigger callback..." << std::endl;
    callbackCache.remove("callback-1");

    // Insert a batch to trigger callbacks
    std::cout << "\nInserting a batch to trigger callbacks..." << std::endl;
    std::vector<std::pair<std::string, Resource>> callbackBatch;
    callbackBatch.emplace_back("callback-batch-1", createSampleResource(403));
    callbackBatch.emplace_back("callback-batch-2", createSampleResource(404));
    callbackCache.insertBatch(callbackBatch, std::chrono::seconds(60));

    // Remove a batch to trigger callbacks
    std::cout << "\nRemoving a batch to trigger callbacks..." << std::endl;
    callbackCache.removeBatch({"callback-batch-1", "callback-2"});

    //--------------------------------------------------------------------------
    // 8. Cache Statistics
    //--------------------------------------------------------------------------
    printSection("8. Cache Statistics");

    // Create a cache for statistics
    atom::search::ResourceCache<Resource> statsCache(10);

    // Insert some resources
    for (int i = 1; i <= 5; ++i) {
        statsCache.insert("stats-" + std::to_string(i),
                          createSampleResource(i + 500),
                          std::chrono::seconds(60));
    }

    // Generate cache hits
    std::cout << "Generating cache hits..." << std::endl;
    for (int i = 1; i <= 5; ++i) {
        auto resource = statsCache.get("stats-" + std::to_string(i));
        if (resource) {
            std::cout << "Cache hit for 'stats-" << i << "'" << std::endl;
        }
    }

    // Generate cache misses
    std::cout << "\nGenerating cache misses..." << std::endl;
    for (int i = 6; i <= 8; ++i) {
        auto resource = statsCache.get("stats-" + std::to_string(i));
        if (!resource) {
            std::cout << "Cache miss for 'stats-" << i << "'" << std::endl;
        }
    }

    // Get statistics
    auto [hits, misses] = statsCache.getStatistics();
    std::cout << "\nCache statistics:" << std::endl;
    std::cout << "Hits: " << hits << std::endl;
    std::cout << "Misses: " << misses << std::endl;
    std::cout << "Hit ratio: "
              << (static_cast<double>(hits) / (hits + misses)) * 100 << "%"
              << std::endl;

    //--------------------------------------------------------------------------
    // 9. Advanced Configuration
    //--------------------------------------------------------------------------
    printSection("9. Advanced Configuration");

    // Create a cache with an initial size
    atom::search::ResourceCache<Resource> configCache(5);
    std::cout << "Created cache with initial size limit: 5" << std::endl;

    // Insert resources up to limit
    for (int i = 1; i <= 5; ++i) {
        configCache.insert("config-" + std::to_string(i),
                           createSampleResource(i + 600),
                           std::chrono::seconds(60));
    }

    std::cout << "Cache size after initial insertion: " << configCache.size()
              << std::endl;

    // Change the maximum size
    std::cout << "\nChanging maximum cache size to 10..." << std::endl;
    configCache.setMaxSize(10);

    // Insert more resources
    for (int i = 6; i <= 10; ++i) {
        configCache.insert("config-" + std::to_string(i),
                           createSampleResource(i + 600),
                           std::chrono::seconds(60));
    }

    std::cout << "Cache size after additional insertion: " << configCache.size()
              << std::endl;

    // Modify expiration time for a resource
    std::cout << "\nChanging expiration time for 'config-1'..." << std::endl;
    configCache.setExpirationTime("config-1", std::chrono::seconds(1));

    // Wait for the resource to expire
    std::cout << "Waiting for the resource to expire (2 seconds)..."
              << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Try to get the resource with updated expiration
    auto expiredConfigResource = configCache.get("config-1");
    if (expiredConfigResource) {
        std::cout << "Retrieved resource with updated expiration: "
                  << expiredConfigResource->toString() << std::endl;
    } else {
        std::cout << "Resource with updated expiration was automatically "
                     "removed from the cache"
                  << std::endl;
    }

    //--------------------------------------------------------------------------
    // 10. Cleanup and Clear Operations
    //--------------------------------------------------------------------------
    printSection("10. Cleanup and Clear Operations");

    // Check the current state of the main cache
    std::cout << "Current cache size: " << cache.size() << std::endl;

    // Clear the cache
    std::cout << "\nClearing the entire cache..." << std::endl;
    cache.clear();
    std::cout << "Cache size after clearing: " << cache.size() << std::endl;
    std::cout << "Cache is empty: " << (cache.empty() ? "Yes" : "No")
              << std::endl;

    // Insert a few resources with different expiration times
    std::cout << "\nInserting resources with different expiration times..."
              << std::endl;
    cache.insert("cleanup-1", createSampleResource(701),
                 std::chrono::seconds(1));
    cache.insert("cleanup-2", createSampleResource(702),
                 std::chrono::seconds(3));
    cache.insert("cleanup-3", createSampleResource(703),
                 std::chrono::seconds(5));

    // Wait for some resources to expire
    std::cout << "Waiting for some resources to expire (2 seconds)..."
              << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Remove expired resources
    std::cout << "Manually removing expired resources..." << std::endl;
    cache.removeExpired();

    // Check which resources remain
    std::cout << "\nChecking remaining resources:" << std::endl;
    std::cout << "Contains 'cleanup-1': "
              << (cache.contains("cleanup-1") ? "Yes" : "No") << std::endl;
    std::cout << "Contains 'cleanup-2': "
              << (cache.contains("cleanup-2") ? "Yes" : "No") << std::endl;
    std::cout << "Contains 'cleanup-3': "
              << (cache.contains("cleanup-3") ? "Yes" : "No") << std::endl;

    // Wait for more resources to expire
    std::cout << "\nWaiting for more resources to expire (2 seconds)..."
              << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Automatic cleanup should have removed more resources
    std::cout << "Checking resources after automatic cleanup:" << std::endl;
    std::cout << "Contains 'cleanup-2': "
              << (cache.contains("cleanup-2") ? "Yes" : "No") << std::endl;
    std::cout << "Contains 'cleanup-3': "
              << (cache.contains("cleanup-3") ? "Yes" : "No") << std::endl;

    //--------------------------------------------------------------------------
    // Summary
    //--------------------------------------------------------------------------
    printSection("Summary");

    std::cout
        << "This example demonstrated the following ResourceCache features:"
        << std::endl;
    std::cout << "  1. Basic cache operations (insert, get, remove)"
              << std::endl;
    std::cout << "  2. Resource expiration handling" << std::endl;
    std::cout << "  3. LRU (Least Recently Used) eviction policy" << std::endl;
    std::cout << "  4. Asynchronous operations" << std::endl;
    std::cout
        << "  5. Batch operations for efficient handling of multiple resources"
        << std::endl;
    std::cout << "  6. Serialization and persistence to files (text and JSON)"
              << std::endl;
    std::cout << "  7. Event callbacks for monitoring cache operations"
              << std::endl;
    std::cout << "  8. Cache statistics for performance monitoring"
              << std::endl;
    std::cout << "  9. Advanced configuration options" << std::endl;
    std::cout << "  10. Cleanup and clear operations" << std::endl;

    // Clean up temporary files
    std::cout << "\nCleaning up temporary files..." << std::endl;
    std::remove("cache_data.txt");
    std::remove("cache_data.json");
    std::cout << "Example completed successfully!" << std::endl;

    return 0;
}
