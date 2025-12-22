/**
 * @file resource_cache_comprehensive.cpp
 * @brief Comprehensive example demonstrating all features of ResourceCache
 * @author Atom Search Examples
 * @date 2025-01-25
 *
 * This example demonstrates:
 * - Basic cache operations (insert, get, remove)
 * - Resource expiration handling
 * - LRU (Least Recently Used) eviction policy
 * - Asynchronous operations
 * - Batch operations for efficient handling of multiple resources
 * - Serialization and persistence to files (text and JSON)
 * - Event callbacks for monitoring cache operations
 * - Cache statistics for performance monitoring
 * - Advanced configuration options
 * - Cleanup and clear operations
 */

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "atom/search/cache.hpp"

// Helper function to print section titlesvoid printSection(const std::string&
// title) {
std::cout << "\n" << std::string(80, '=') << "\n";
std::cout << "  " << title << "\n";
std::cout << std::string(80, '=') << "\n";
}

// A simple resource class that can be cachedclass Resource {
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
}
;

// Implement to/from JSON conversion for Resourcenlohmann::json
// resourceToJson(const Resource& resource) {
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

// Implement string serialization/deserialization for Resourcestd::string
// resourceToString(const Resource& resource) {
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

// Helper function to create sample resourcesResource createSampleResource(int
// index) {
return Resource(index, "Resource-" + std::to_string(index),
                "Sample data for resource " + std::to_string(index));
}

// Long-running operation for async examplesResource loadResourceSlowly(int id)
// { Simulate a time-consuming operation
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

    //------------------------------------------------------------------------------
    // 4. Asynchronous Operations (asyncInsert, asyncGet, asyncLoad)
    //------------------------------------------------------------------------------
    printSection("4. Asynchronous Operations");
    {
        auto fIns = cache.asyncInsert("async-1", createSampleResource(100),
                                      std::chrono::seconds(30));
        fIns.get();
        auto fGet = cache.asyncGet("async-1");
        auto got = fGet.get();
        if (got) {
            std::cout << "asyncGet('async-1'): " << got->toString()
                      << std::endl;
        }
        auto fLoad =
            cache.asyncLoad("async-2", [] { return loadResourceSlowly(101); });
        fLoad.get();
        auto got2 = cache.get("async-2");
        if (got2) {
            std::cout << "asyncLoad('async-2'): " << got2->toString()
                      << std::endl;
        }
    }

    //------------------------------------------------------------------------------
    // 5. Batch Operations (insertBatch, removeBatch)
    //------------------------------------------------------------------------------
    printSection("5. Batch Operations");
    {
        Vector<std::pair<String, Resource>> batchItems = {
            {"b1", createSampleResource(201)},
            {"b2", createSampleResource(202)},
            {"b3", createSampleResource(203)}};
        cache.insertBatch(batchItems, std::chrono::seconds(120));
        std::cout << "Inserted batch of " << batchItems.size() << " items"
                  << std::endl;

        Vector<String> toRemove = {"b2"};
        cache.removeBatch(toRemove);
        std::cout << "Removed keys: b2" << std::endl;
    }

    //------------------------------------------------------------------------------
    // 6. Persistence to Text (writeToFile / readFromFile)
    //------------------------------------------------------------------------------
    printSection("6. Persistence to Text");
    {
        const String txtFile = "resource_cache.txt";
        cache.writeToFile(txtFile, resourceToString);
        cache.clear();
        cache.readFromFile(txtFile, resourceFromString);
        std::cout << "Reloaded from text file. Cache size: " << cache.size()
                  << std::endl;
    }

    //------------------------------------------------------------------------------
    // 7. Persistence to JSON (writeToJsonFile / readFromJsonFile)
    //------------------------------------------------------------------------------
    printSection("7. Persistence to JSON");
    {
        const String jsonFile = "resource_cache.json";
        cache.writeToJsonFile(jsonFile, resourceToJson);
        cache.clear();
        cache.readFromJsonFile(jsonFile, resourceFromJson);
        std::cout << "Reloaded from JSON file. Cache size: " << cache.size()
                  << std::endl;
    }

    //------------------------------------------------------------------------------
    // 8. Callbacks (onInsert, onRemove)
    //------------------------------------------------------------------------------
    printSection("8. Callbacks");
    {
        cache.onInsert([](const String& key, const Resource& r) {
            std::cout << "onInsert: " << std::string(key) << " -> "
                      << r.toString() << std::endl;
        });
        cache.onRemove([](const String& key, const Resource&) {
            std::cout << "onRemove: " << std::string(key) << std::endl;
        });
        cache.insert("cb-1", createSampleResource(301),
                     std::chrono::seconds(60));
        cache.remove("cb-1");
    }

    //------------------------------------------------------------------------------
    // 9. Statistics and Monitoring
    //------------------------------------------------------------------------------
    printSection("9. Statistics");
    {
        auto s = cache.getStatistics();
        std::cout << "Hits: " << s.first << ", Misses: " << s.second
                  << std::endl;
    }

    //------------------------------------------------------------------------------
    // 10. Configuration and Cleanup (setMaxSize, setExpirationTime,
    // removeExpired)
    //------------------------------------------------------------------------------
    printSection("10. Configuration and Cleanup");
    {
        cache.setMaxSize(20);
        cache.setExpirationTime("async-1", std::chrono::seconds(1));
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        cache.removeExpired();
        std::cout << "After removeExpired, contains 'async-1': "
                  << (cache.contains("async-1") ? "Yes" : "No") << std::endl;
    }

    return 0;
}
