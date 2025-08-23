#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <thread>

#include "atom/type/auto_table.hpp"

// Custom data type for demonstration
struct UserData {
    int id;
    std::string name;
    double score;

    // Required for JSON serialization
    friend void to_json(atom::type::json& j, const UserData& data) {
        j = atom::type::json{
            {"id", data.id}, {"name", data.name}, {"score", data.score}};
    }

    // Required for JSON deserialization
    friend void from_json(const atom::type::json& j, UserData& data) {
        j.at("id").get_to(data.id);
        j.at("name").get_to(data.name);
        j.at("score").get_to(data.score);
    }

    // For pretty-printing
    friend std::ostream& operator<<(std::ostream& os, const UserData& data) {
        return os << "User(id=" << data.id << ", name=\"" << data.name
                  << "\", score=" << data.score << ")";
    }
};

// Helper function to print query results
template <typename T>
void print_results(const std::string& operation,
                   const std::vector<std::optional<T>>& results) {
    std::cout << operation << " results:" << std::endl;
    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << "  [" << i << "]: ";
        if (results[i].has_value()) {
            std::cout << results[i].value();
        } else {
            std::cout << "not found";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

// Helper function to print entries
template <typename EntryType>
void print_entries(const std::string& title,
                   const std::vector<EntryType>& entries) {
    std::cout << title << ":" << std::endl;
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& [key, data] = entries[i];
        std::cout << "  [" << i << "] Key: " << key << ", Count: " << data.count
                  << ", Value: " << data.value << std::endl;
    }
    std::cout << std::endl;
}

// Simulate access patterns
void simulate_accesses(
    atom::type::CountingHashTable<std::string, std::string>& table,
    const std::vector<std::string>& keys, int num_accesses, std::mt19937& rng) {
    std::uniform_int_distribution<> dist(0, keys.size() - 1);

    for (int i = 0; i < num_accesses; ++i) {
        size_t idx = dist(rng);
        // Access with higher probability for lower indices (creates skewed
        // access pattern)
        if (idx < keys.size() / 4 || std::bernoulli_distribution(0.7)(rng)) {
            table.get(keys[idx]);
        }
    }
}

int main() {
    std::cout << "=== CountingHashTable Usage Examples ===" << std::endl
              << std::endl;

    // 1. Basic Usage with String Keys and Values
    std::cout << "1. BASIC USAGE WITH STRING KEYS AND VALUES" << std::endl;
    std::cout << "==========================================" << std::endl;

    // Create a table with 8 mutexes and initial capacity of 100
    atom::type::CountingHashTable<std::string, std::string> string_table(8,
                                                                         100);

    // Insert single entries
    string_table.insert("apple", "A fruit");
    string_table.insert("banana", "Yellow fruit");
    string_table.insert("cherry", "Small red fruit");

    // Retrieve values and demonstrate counting
    std::cout << "Retrieving 'apple' multiple times to increment counter..."
              << std::endl;
    for (int i = 0; i < 5; i++) {
        auto value = string_table.get("apple");
        std::cout << "  Access #" << i + 1 << ": "
                  << (value ? *value : "not found") << std::endl;
    }

    std::cout << "Retrieving 'banana' twice..." << std::endl;
    string_table.get("banana");
    string_table.get("banana");

    std::cout << "Retrieving 'cherry' once..." << std::endl;
    string_table.get("cherry");

    // Get access counts
    std::cout << "\nAccess counts:" << std::endl;
    std::cout << "  apple: " << string_table.getAccessCount("apple").value_or(0)
              << std::endl;
    std::cout << "  banana: "
              << string_table.getAccessCount("banana").value_or(0) << std::endl;
    std::cout << "  cherry: "
              << string_table.getAccessCount("cherry").value_or(0) << std::endl;
    std::cout << "  nonexistent: "
              << string_table.getAccessCount("nonexistent").value_or(0)
              << std::endl;

    // Batch operations
    std::cout << "\nPerforming batch operations:" << std::endl;

    // Batch insertion
    std::vector<std::pair<std::string, std::string>> batch_items = {
        {"grape", "Small purple fruit"},
        {"orange", "Citrus fruit"},
        {"apple",
         "Updated apple description"}  // This will update existing entry
    };

    string_table.insertBatch(batch_items);
    std::cout << "Inserted batch of 3 items (including 1 update)" << std::endl;

    // Batch retrieval
    std::vector<std::string> batch_keys = {"apple", "nonexistent", "banana",
                                           "grape"};
    auto batch_results = string_table.getBatch(batch_keys);
    print_results("Batch retrieval", batch_results);

    // Access counts after batch retrieval
    std::cout << "Access counts after batch retrieval:" << std::endl;
    std::cout << "  apple: " << string_table.getAccessCount("apple").value_or(0)
              << std::endl;
    std::cout << "  banana: "
              << string_table.getAccessCount("banana").value_or(0) << std::endl;

    // Get all entries
    auto all_entries = string_table.getAllEntries();
    print_entries("All entries", all_entries);

    // Get top entries
    auto top_entries = string_table.getTopNEntries(3);
    print_entries("Top 3 entries by access count", top_entries);

    // Erase an entry
    bool erased = string_table.erase("banana");
    std::cout << "Erased 'banana': " << (erased ? "yes" : "no") << std::endl;

    // Try to retrieve the erased entry
    auto erased_value = string_table.get("banana");
    std::cout << "Retrieving 'banana' after erasure: "
              << (erased_value.has_value() ? *erased_value : "not found")
              << std::endl
              << std::endl;

    // 2. Custom Data Types
    std::cout << "2. CUSTOM DATA TYPES" << std::endl;
    std::cout << "====================" << std::endl;

    atom::type::CountingHashTable<int, UserData> user_table(4, 50);

    // Insert some users
    user_table.insert(1001, UserData{1001, "Alice", 95.5});
    user_table.insert(1002, UserData{1002, "Bob", 87.0});
    user_table.insert(1003, UserData{1003, "Charlie", 92.3});
    user_table.insert(1004, UserData{1004, "Diana", 88.7});

    std::cout << "Inserted 4 users" << std::endl;

    // Access some users multiple times to create a usage pattern
    for (int i = 0; i < 10; i++)
        user_table.get(1001);  // Alice: 10 accesses
    for (int i = 0; i < 5; i++)
        user_table.get(1002);  // Bob: 5 accesses
    for (int i = 0; i < 3; i++)
        user_table.get(1003);  // Charlie: 3 accesses
    for (int i = 0; i < 7; i++)
        user_table.get(1004);  // Diana: 7 accesses

    // Get top users by access count
    auto top_users = user_table.getTopNEntries(4);
    std::cout << "Users sorted by popularity:" << std::endl;
    for (size_t i = 0; i < top_users.size(); ++i) {
        const auto& [id, user_data] = top_users[i];
        std::cout << "  " << i + 1 << ". " << user_data.value << " (accessed "
                  << user_data.count << " times)" << std::endl;
    }
    std::cout << std::endl;

    // 3. JSON Serialization and Deserialization
    std::cout << "3. JSON SERIALIZATION AND DESERIALIZATION" << std::endl;
    std::cout << "=========================================" << std::endl;

    // Serialize the user table to JSON
    atom::type::json json_data = user_table.serializeToJson();
    std::string json_str =
        json_data.dump(2);  // Pretty print with 2-space indent

    std::cout << "Serialized JSON data:" << std::endl;
    std::cout << json_str.substr(0, 300) << "..." << std::endl << std::endl;

    // Save to file
    std::string filename = "user_table.json";
    std::ofstream out_file(filename);
    out_file << json_str;
    out_file.close();
    std::cout << "Saved JSON data to " << filename << std::endl;

    // Create a new table and deserialize from JSON
    atom::type::CountingHashTable<int, UserData> restored_table(4, 50);
    restored_table.deserializeFromJson(json_data);

    std::cout << "Deserialized table data:" << std::endl;
    auto restored_entries = restored_table.getAllEntries();
    for (const auto& [id, user_data] : restored_entries) {
        std::cout << "  User ID: " << id << ", Count: " << user_data.count
                  << ", Data: " << user_data.value << std::endl;
    }
    std::cout << std::endl;

    // 4. Automatic Sorting
    std::cout << "4. AUTOMATIC SORTING" << std::endl;
    std::cout << "====================" << std::endl;

    // Create a new table for this example
    atom::type::CountingHashTable<std::string, std::string> auto_sort_table(
        4, 100);

    // Insert items
    std::vector<std::string> words = {
        "the",     "quick",   "brown",     "fox",   "jumps",
        "over",    "lazy",    "dog",       "hello", "world",
        "example", "sorting", "algorithm", "data",  "structure"};

    for (const auto& word : words) {
        auto_sort_table.insert(word, "Word: " + word);
    }

    // Set up random engine for access simulation
    std::random_device rd;
    std::mt19937 gen(rd());

    std::cout << "Starting auto-sorting with 500ms interval..." << std::endl;
    auto_sort_table.startAutoSorting(std::chrono::milliseconds(500));

    // Simulate access patterns in background
    std::cout << "Simulating random access pattern for 2 seconds..."
              << std::endl;
    simulate_accesses(auto_sort_table, words, 1000, gen);

    // Show the current state
    auto current_entries = auto_sort_table.getTopNEntries(5);
    print_entries("Top 5 entries after first simulation", current_entries);

    // Sleep to let the auto-sorting happen
    std::cout << "Waiting for auto-sorting to run..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Simulate different access pattern
    std::cout << "Simulating different access pattern..." << std::endl;
    for (int i = 0; i < 20; i++) {
        auto_sort_table.get("algorithm");
        auto_sort_table.get("structure");
        auto_sort_table.get("data");
    }

    // Sleep again
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Show updated state
    auto updated_entries = auto_sort_table.getTopNEntries(5);
    print_entries("Top 5 entries after focused access", updated_entries);

    // Stop auto-sorting
    auto_sort_table.stopAutoSorting();
    std::cout << "Auto-sorting stopped" << std::endl << std::endl;

    // 5. Performance Benchmarking
    std::cout << "5. PERFORMANCE BENCHMARKING" << std::endl;
    std::cout << "==========================" << std::endl;

    const int NUM_ITEMS = 10000;
    const int NUM_QUERIES = 100000;
    const int NUM_THREADS = 4;

    // Create a table for benchmarking
    atom::type::CountingHashTable<int, int> bench_table(16, NUM_ITEMS);

    // Insert test data
    std::cout << "Inserting " << NUM_ITEMS << " items..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ITEMS; ++i) {
        bench_table.insert(i, i * i);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto insert_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                              start_time)
            .count();

    std::cout << "Insertion time: " << insert_duration << " ms" << std::endl;

    // Test concurrent reads
    std::cout << "Testing " << NUM_QUERIES << " concurrent reads with "
              << NUM_THREADS << " threads..." << std::endl;

    start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    std::atomic<int> counter(0);
    std::atomic<int> hits(0);

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(
                0, NUM_ITEMS * 2 - 1);  // Some hits, some misses

            const int queries_per_thread = NUM_QUERIES / NUM_THREADS;
            for (int i = 0; i < queries_per_thread; ++i) {
                int key = dis(gen);
                auto result = bench_table.get(key);
                if (result.has_value()) {
                    hits.fetch_add(1);
                }
                counter.fetch_add(1);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    end_time = std::chrono::high_resolution_clock::now();
    auto query_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                              end_time - start_time)
                              .count();

    double queries_per_second = static_cast<double>(counter.load()) /
                                (static_cast<double>(query_duration) / 1000.0);

    std::cout << "Completed " << counter.load() << " queries in "
              << query_duration << " ms" << std::endl;
    std::cout << "Hit rate: "
              << (static_cast<double>(hits.load()) / counter.load() * 100.0)
              << "%" << std::endl;
    std::cout << "Throughput: " << std::fixed << std::setprecision(2)
              << queries_per_second << " queries/second" << std::endl;

    // Get most accessed items
    auto most_accessed = bench_table.getTopNEntries(5);
    std::cout << "\nMost accessed items:" << std::endl;
    for (const auto& [key, data] : most_accessed) {
        std::cout << "  Key: " << key << ", Count: " << data.count
                  << ", Value: " << data.value << std::endl;
    }

    // 6. Clear the table
    std::cout << "\n6. CLEARING THE TABLE" << std::endl;
    std::cout << "======================" << std::endl;

    bench_table.clear();
    std::cout << "Table cleared" << std::endl;

    auto after_clear = bench_table.getAllEntries();
    std::cout << "Entries after clearing: " << after_clear.size() << std::endl;

    std::cout << "\nAll examples completed successfully!" << std::endl;

    return 0;
}
