#include "atom/search/cache.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace atom::search;

// Define a simple Cacheable type
struct MyResource {
    int data;
};

// Define a function to load data
MyResource loadData() { return MyResource{42}; }

// Define a function to serialize MyResource to a string
std::string serialize(const MyResource &resource) {
    return std::to_string(resource.data);
}

// Define a function to deserialize MyResource from a string
MyResource deserialize(const std::string &str) {
    return MyResource{std::stoi(str)};
}

// Define a function to convert MyResource to JSON
json toJson(const MyResource &resource) {
    return json{{"data", resource.data}};
}

// Define a function to convert JSON to MyResource
MyResource fromJson(const json &j) {
    return MyResource{j.at("data").get<int>()};
}

int main() {
    // Create a ResourceCache with a maximum size of 5
    ResourceCache<MyResource> cache(5);

    // Insert a resource into the cache with a 10-second expiration time
    cache.insert("key1", MyResource{1}, std::chrono::seconds(10));

    // Check if the cache contains a resource with the specified key
    if (cache.contains("key1")) {
        std::cout << "Cache contains key1" << std::endl;
    }

    // Retrieve a resource from the cache
    auto resource = cache.get("key1");
    if (resource) {
        std::cout << "Retrieved resource with data: " << resource->data
                  << std::endl;
    }

    // Remove a resource from the cache
    cache.remove("key1");

    // Asynchronously insert a resource into the cache with a 10-second
    // expiration time
    auto insertFuture =
        cache.asyncInsert("key2", MyResource{2}, std::chrono::seconds(10));
    insertFuture.get();  // Wait for the insertion to complete

    // Asynchronously retrieve a resource from the cache
    auto getFuture = cache.asyncGet("key2");
    auto asyncResource = getFuture.get();
    if (asyncResource) {
        std::cout << "Asynchronously retrieved resource with data: "
                  << asyncResource->data << std::endl;
    }

    // Clear all resources from the cache
    cache.clear();

    // Get the number of resources in the cache
    std::cout << "Cache size: " << cache.size() << std::endl;

    // Check if the cache is empty
    if (cache.empty()) {
        std::cout << "Cache is empty" << std::endl;
    }

    // Insert multiple resources into the cache with a 10-second expiration time
    cache.insertBatch({{"key3", MyResource{3}}, {"key4", MyResource{4}}},
                      std::chrono::seconds(10));

    // Remove multiple resources from the cache
    cache.removeBatch({"key3", "key4"});

    // Register a callback to be called on insertion
    cache.onInsert([](const std::string &key) {
        std::cout << "Inserted key: " << key << std::endl;
    });

    // Register a callback to be called on removal
    cache.onRemove([](const std::string &key) {
        std::cout << "Removed key: " << key << std::endl;
    });

    // Retrieve cache statistics
    auto [hitCount, missCount] = cache.getStatistics();
    std::cout << "Cache hits: " << hitCount << ", misses: " << missCount
              << std::endl;

    // Asynchronously load a resource into the cache
    auto loadFuture = cache.asyncLoad("key5", loadData);
    loadFuture.get();  // Wait for the load to complete

    // Set the maximum size of the cache
    cache.setMaxSize(10);

    // Set the expiration time for a resource in the cache
    cache.setExpirationTime("key5", std::chrono::seconds(20));

    // Write the resources in the cache to a file
    cache.writeToFile("cache.txt", serialize);

    // Read resources from a file and insert them into the cache
    cache.readFromFile("cache.txt", deserialize);

    // Write the resources in the cache to a JSON file
    cache.writeToJsonFile("cache.json", toJson);

    // Read resources from a JSON file and insert them into the cache
    cache.readFromJsonFile("cache.json", fromJson);

    // Remove expired resources from the cache
    cache.removeExpired();

    return 0;
}