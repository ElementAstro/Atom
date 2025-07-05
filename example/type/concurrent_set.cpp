#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <future>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "atom/type/concurrent_set.hpp"

// Custom type for demonstrating complex key handling
class ComplexKey {
private:
    int id;
    std::string name;

public:
    ComplexKey() : id(0), name("") {}
    ComplexKey(int id, std::string name) : id(id), name(std::move(name)) {}

    int getId() const { return id; }
    const std::string& getName() const { return name; }

    bool operator==(const ComplexKey& other) const {
        return id == other.id && name == other.name;
    }

    // Required for unordered_set
    friend struct std::hash<ComplexKey>;

    // Serialization support
    friend std::vector<char> serialize(const ComplexKey& key) {
        std::vector<char> result;
        // Serialize ID
        result.resize(sizeof(int));
        memcpy(result.data(), &key.id, sizeof(int));

        // Serialize name length and data
        size_t name_length = key.name.length();
        size_t offset = result.size();
        result.resize(offset + sizeof(size_t));
        memcpy(result.data() + offset, &name_length, sizeof(size_t));

        // Append name data
        offset = result.size();
        result.resize(offset + name_length);
        memcpy(result.data() + offset, key.name.c_str(), name_length);

        return result;
    }

    // Deserialization support
    template <typename T>
    friend T deserialize(const std::vector<char>& data);

    friend std::ostream& operator<<(std::ostream& os, const ComplexKey& key) {
        return os << "ComplexKey{id=" << key.id << ", name='" << key.name
                  << "'}";
    }
};

// Implement hash function for ComplexKey
namespace std {
template <>
struct hash<ComplexKey> {
    size_t operator()(const ComplexKey& k) const {
        return hash<int>()(k.getId()) ^ hash<string>()(k.getName());
    }
};
}  // namespace std

// Implement deserialization for ComplexKey
template <>
ComplexKey deserialize<ComplexKey>(const std::vector<char>& data) {
    if (data.size() < sizeof(int) + sizeof(size_t)) {
        throw std::runtime_error("Insufficient data for deserialization");
    }

    // Extract ID
    int id;
    memcpy(&id, data.data(), sizeof(int));

    // Extract name length
    size_t name_length;
    memcpy(&name_length, data.data() + sizeof(int), sizeof(size_t));

    // Extract name
    if (data.size() < sizeof(int) + sizeof(size_t) + name_length) {
        throw std::runtime_error("Corrupted serialized data");
    }

    std::string name(data.data() + sizeof(int) + sizeof(size_t), name_length);
    return ComplexKey(id, name);
}

// Helper function to print a section header
void print_header(const std::string& title) {
    std::cout << "\n==================================================="
              << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==================================================="
              << std::endl;
}

// Helper function to print a subsection header
void print_subheader(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

// Helper function to print timing information
void print_timing(const std::string& operation,
                  std::chrono::milliseconds duration) {
    std::cout << "  Time for " << std::left << std::setw(25) << operation
              << ": " << duration.count() << " ms" << std::endl;
}

// Helper function to measure execution time
template <typename Func>
std::chrono::milliseconds time_execution(Func func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
}

int main() {
    std::cout << "==============================================" << std::endl;
    std::cout << "     CONCURRENT SET COMPREHENSIVE EXAMPLE     " << std::endl;
    std::cout << "==============================================" << std::endl;

    // Random number generator setup
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> int_dist(1, 1000000);
    std::uniform_int_distribution<> small_int_dist(1, 100);
    std::uniform_real_distribution<> real_dist(0.0, 1.0);

    // ===============================================================
    // 1. Basic Usage with Integer Keys
    // ===============================================================
    print_header("1. BASIC USAGE WITH INTEGER KEYS");

    // Create a concurrent set with default settings
    atom::type::concurrent_set<int> int_set;

    print_subheader("Insert and Find Operations");

    // Insert some elements
    for (int i = 1; i <= 10; i++) {
        int_set.insert(i * 10);
    }

    std::cout << "Set size after insertion: " << int_set.size() << std::endl;

    // Find elements
    for (int i = 1; i <= 12; i++) {
        auto result = int_set.find(i * 10);
        std::cout << "Find " << i * 10 << ": "
                  << (result && *result ? "Found" : "Not found") << std::endl;
    }

    print_subheader("Erase Operations");

    // Erase some elements
    bool erased = int_set.erase(30);
    std::cout << "Erase 30: " << (erased ? "Success" : "Failure") << std::endl;
    erased = int_set.erase(110);
    std::cout << "Erase 110: " << (erased ? "Success" : "Failure") << std::endl;

    std::cout << "Set size after erasure: " << int_set.size() << std::endl;

    // ===============================================================
    // 2. Asynchronous Operations
    // ===============================================================
    print_header("2. ASYNCHRONOUS OPERATIONS");

    atom::type::concurrent_set<int> async_set;

    print_subheader("Async Insert");

    // Queue some async inserts
    for (int i = 1; i <= 10; i++) {
        async_set.async_insert(i * 5);
    }

    // Wait for operations to complete
    std::cout << "Waiting for async operations to complete..." << std::endl;
    bool completed = async_set.wait_for_tasks(1000);
    std::cout << "All tasks completed: " << (completed ? "Yes" : "No")
              << std::endl;
    std::cout << "Set size after async insertion: " << async_set.size()
              << std::endl;

    print_subheader("Async Find");

    // Perform async find operations
    std::vector<std::future<void>> find_futures;

    for (int i = 1; i <= 10; i++) {
        std::promise<void> promise;
        find_futures.push_back(promise.get_future());

        async_set.async_find(
            i * 5, [i, promise = std::move(promise)](auto result) mutable {
                std::cout << "  Async find " << i * 5 << ": "
                          << (result && *result ? "Found" : "Not found")
                          << std::endl;
                promise.set_value();
            });
    }

    // Wait for all find operations to complete
    for (auto& future : find_futures) {
        future.wait();
    }

    print_subheader("Async Erase");

    // Perform async erase operations
    std::vector<std::future<void>> erase_futures;

    for (int i = 1; i <= 5; i++) {
        std::promise<void> promise;
        erase_futures.push_back(promise.get_future());

        async_set.async_erase(
            i * 5, [i, promise = std::move(promise)](bool success) mutable {
                std::cout << "  Async erase " << i * 5 << ": "
                          << (success ? "Success" : "Failure") << std::endl;
                promise.set_value();
            });
    }

    // Wait for all erase operations to complete
    for (auto& future : erase_futures) {
        future.wait();
    }

    std::cout << "Set size after async erasure: " << async_set.size()
              << std::endl;

    // ===============================================================
    // 3. Batch Operations
    // ===============================================================
    print_header("3. BATCH OPERATIONS");

    atom::type::concurrent_set<int> batch_set;

    print_subheader("Batch Insert");

    // Create a batch of values
    std::vector<int> batch_values;
    for (int i = 1; i <= 1000; i++) {
        batch_values.push_back(int_dist(gen));
    }

    // Insert in batch
    auto batch_insert_time =
        time_execution([&]() { batch_set.batch_insert(batch_values); });

    std::cout << "Set size after batch insertion: " << batch_set.size()
              << std::endl;
    print_timing("batch insert (1000 items)", batch_insert_time);

    print_subheader("Async Batch Insert");

    // Create another batch
    std::vector<int> async_batch_values;
    for (int i = 1; i <= 1000; i++) {
        async_batch_values.push_back(int_dist(gen));
    }

    // Async batch insert with completion callback
    std::promise<void> batch_promise;
    auto batch_future = batch_promise.get_future();

    auto async_batch_start = std::chrono::high_resolution_clock::now();

    batch_set.async_batch_insert(async_batch_values, [&](bool success) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - async_batch_start);
        std::cout << "  Async batch insert completed: "
                  << (success ? "Success" : "Failure") << std::endl;
        std::cout << "  Time for async batch insert: " << duration.count()
                  << " ms" << std::endl;
        batch_promise.set_value();
    });

    batch_future.wait();
    std::cout << "Set size after async batch insertion: " << batch_set.size()
              << std::endl;

    print_subheader("Batch Erase");

    // Create a subset of values to erase
    std::vector<int> erase_values(batch_values.begin(),
                                  batch_values.begin() + 200);

    // Batch erase
    size_t erased_count = batch_set.batch_erase(erase_values);

    std::cout << "Items erased in batch: " << erased_count << " out of "
              << erase_values.size() << std::endl;
    std::cout << "Set size after batch erasure: " << batch_set.size()
              << std::endl;

    // ===============================================================
    // 4. Cache Performance
    // ===============================================================
    print_header("4. CACHE PERFORMANCE");

    // Create sets with different cache sizes
    atom::type::concurrent_set<int> set_no_cache(4, 0);         // No cache
    atom::type::concurrent_set<int> set_small_cache(4, 100);    // Small cache
    atom::type::concurrent_set<int> set_large_cache(4, 10000);  // Large cache

    // Insert same data into all sets
    std::vector<int> cache_test_data;
    for (int i = 0; i < 5000; i++) {
        cache_test_data.push_back(int_dist(gen));
    }

    set_no_cache.batch_insert(cache_test_data);
    set_small_cache.batch_insert(cache_test_data);
    set_large_cache.batch_insert(cache_test_data);

    // Prepare access pattern (with repetition to test cache)
    std::vector<int> access_pattern;
    for (int i = 0; i < 100; i++) {
        // Add some frequently accessed items
        access_pattern.push_back(cache_test_data[i % 200]);

        // Add some random items
        access_pattern.push_back(
            cache_test_data[int_dist(gen) % cache_test_data.size()]);
    }

    print_subheader("Find Performance Comparison");

    // Test find performance with no cache
    auto time_no_cache = time_execution([&]() {
        for (int value : access_pattern) {
            set_no_cache.find(value);
        }
    });

    // Test find performance with small cache
    auto time_small_cache = time_execution([&]() {
        for (int value : access_pattern) {
            set_small_cache.find(value);
        }
    });

    // Test find performance with large cache
    auto time_large_cache = time_execution([&]() {
        for (int value : access_pattern) {
            set_large_cache.find(value);
        }
    });

    print_timing("find with no cache", time_no_cache);
    print_timing("find with small cache", time_small_cache);
    print_timing("find with large cache", time_large_cache);

    print_subheader("Cache Statistics");

    // Get cache statistics
    auto no_cache_stats = set_no_cache.get_cache_stats();
    auto small_cache_stats = set_small_cache.get_cache_stats();
    auto large_cache_stats = set_large_cache.get_cache_stats();

    std::cout << "No cache stats: size=" << std::get<0>(no_cache_stats)
              << ", hits=" << std::get<1>(no_cache_stats)
              << ", misses=" << std::get<2>(no_cache_stats)
              << ", hit rate=" << std::fixed << std::setprecision(2)
              << std::get<3>(no_cache_stats) << "%" << std::endl;

    std::cout << "Small cache stats: size=" << std::get<0>(small_cache_stats)
              << ", hits=" << std::get<1>(small_cache_stats)
              << ", misses=" << std::get<2>(small_cache_stats)
              << ", hit rate=" << std::fixed << std::setprecision(2)
              << std::get<3>(small_cache_stats) << "%" << std::endl;

    std::cout << "Large cache stats: size=" << std::get<0>(large_cache_stats)
              << ", hits=" << std::get<1>(large_cache_stats)
              << ", misses=" << std::get<2>(large_cache_stats)
              << ", hit rate=" << std::fixed << std::setprecision(2)
              << std::get<3>(large_cache_stats) << "%" << std::endl;

    print_subheader("Cache Resizing");

    // Resize the cache
    set_small_cache.resize_cache(500);
    std::cout << "Cache resized from 100 to 500" << std::endl;

    // Test performance after resize
    auto time_after_resize = time_execution([&]() {
        for (int value : access_pattern) {
            set_small_cache.find(value);
        }
    });

    print_timing("find after cache resize", time_after_resize);

    auto resized_cache_stats = set_small_cache.get_cache_stats();
    std::cout << "Resized cache stats: size="
              << std::get<0>(resized_cache_stats)
              << ", hits=" << std::get<1>(resized_cache_stats)
              << ", misses=" << std::get<2>(resized_cache_stats)
              << ", hit rate=" << std::fixed << std::setprecision(2)
              << std::get<3>(resized_cache_stats) << "%" << std::endl;

    // ===============================================================
    // 5. Thread Pool Adjustment
    // ===============================================================
    print_header("5. THREAD POOL ADJUSTMENT");

    // Create a set with a specific thread count
    atom::type::concurrent_set<int> pool_set(2);  // 2 threads

    std::cout << "Initial thread count: " << pool_set.get_thread_count()
              << std::endl;

    // Prepare a large batch of async operations
    std::vector<int> large_batch;
    for (int i = 0; i < 10000; i++) {
        large_batch.push_back(int_dist(gen));
    }

    print_subheader("Performance with Different Thread Pool Sizes");

    // Test with initial thread count
    auto start_2threads = std::chrono::high_resolution_clock::now();
    std::promise<void> promise_2threads;
    auto future_2threads = promise_2threads.get_future();

    pool_set.async_batch_insert(
        large_batch, [&](bool success) { promise_2threads.set_value(); });

    future_2threads.wait();
    auto end_2threads = std::chrono::high_resolution_clock::now();
    auto duration_2threads =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_2threads -
                                                              start_2threads);

    std::cout << "Time with 2 threads: " << duration_2threads.count() << " ms"
              << std::endl;

    // Adjust thread pool size
    pool_set.adjust_thread_pool_size(8);  // 8 threads
    std::cout << "Thread pool adjusted to: " << pool_set.get_thread_count()
              << " threads" << std::endl;

    // Clear the set
    pool_set.clear();

    // Test with adjusted thread count
    auto start_8threads = std::chrono::high_resolution_clock::now();
    std::promise<void> promise_8threads;
    auto future_8threads = promise_8threads.get_future();

    pool_set.async_batch_insert(
        large_batch, [&](bool success) { promise_8threads.set_value(); });

    future_8threads.wait();
    auto end_8threads = std::chrono::high_resolution_clock::now();
    auto duration_8threads =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_8threads -
                                                              start_8threads);

    std::cout << "Time with 8 threads: " << duration_8threads.count() << " ms"
              << std::endl;
    std::cout << "Speedup factor: " << std::fixed << std::setprecision(2)
              << (static_cast<double>(duration_2threads.count()) /
                  duration_8threads.count())
              << "x" << std::endl;

    // ===============================================================
    // 6. Error Handling
    // ===============================================================
    print_header("6. ERROR HANDLING");

    atom::type::concurrent_set<int> error_set;

    print_subheader("Custom Error Callback");

    // Set a custom error callback
    error_set.set_error_callback([](std::string_view message,
                                    std::exception_ptr eptr) {
        std::cout << "Custom error handler called: " << message << std::endl;
        if (eptr) {
            try {
                std::rethrow_exception(eptr);
            } catch (const std::exception& e) {
                std::cout << "  Exception details: " << e.what() << std::endl;
            } catch (...) {
                std::cout << "  Unknown exception" << std::endl;
            }
        }
    });

    // Try operations that might cause errors
    try {
        // Try to resize cache to 0 (should trigger error)
        error_set.resize_cache(0);
    } catch (const atom::type::cache_exception& e) {
        std::cout << "Caught cache exception: " << e.what() << std::endl;
    }

    // Get error count
    std::cout << "Error count: " << error_set.get_error_count() << std::endl;

    // ===============================================================
    // 7. Complex Key Types
    // ===============================================================
    print_header("7. COMPLEX KEY TYPES");

    atom::type::concurrent_set<ComplexKey> complex_set;

    print_subheader("Operations with Complex Keys");

    // Insert complex keys
    complex_set.insert(ComplexKey(1, "Alice"));
    complex_set.insert(ComplexKey(2, "Bob"));
    complex_set.insert(ComplexKey(3, "Charlie"));
    complex_set.insert(ComplexKey(4, "David"));

    std::cout << "Complex set size: " << complex_set.size() << std::endl;

    // Find complex keys
    auto find_alice = complex_set.find(ComplexKey(1, "Alice"));
    auto find_eve = complex_set.find(ComplexKey(5, "Eve"));

    std::cout << "Find Alice: "
              << (find_alice && *find_alice ? "Found" : "Not found")
              << std::endl;
    std::cout << "Find Eve: " << (find_eve && *find_eve ? "Found" : "Not found")
              << std::endl;

    // Erase complex key
    bool erased_bob = complex_set.erase(ComplexKey(2, "Bob"));
    std::cout << "Erase Bob: " << (erased_bob ? "Success" : "Failure")
              << std::endl;
    std::cout << "Complex set size after erase: " << complex_set.size()
              << std::endl;

    // ===============================================================
    // 8. File I/O Operations
    // ===============================================================
    print_header("8. FILE I/O OPERATIONS");

    // Create a set with some data
    atom::type::concurrent_set<int> file_set;
    for (int i = 1; i <= 1000; i++) {
        file_set.insert(i);
    }

    print_subheader("Save to File");

    // Save to file
    std::string filename = "concurrent_set_data.bin";
    bool save_success = false;

    try {
        save_success = file_set.save_to_file(filename);
        std::cout << "Save to file: " << (save_success ? "Success" : "Failure")
                  << std::endl;
    } catch (const atom::type::io_exception& e) {
        std::cout << "I/O exception during save: " << e.what() << std::endl;
    }

    print_subheader("Load from File");

    // Create a new set and load from file
    atom::type::concurrent_set<int> loaded_set;
    bool load_success = false;

    try {
        load_success = loaded_set.load_from_file(filename);
        std::cout << "Load from file: "
                  << (load_success ? "Success" : "Failure") << std::endl;
        std::cout << "Loaded set size: " << loaded_set.size() << std::endl;
    } catch (const atom::type::io_exception& e) {
        std::cout << "I/O exception during load: " << e.what() << std::endl;
    }

    // Verify some values
    for (int i = 1; i <= 10; i++) {
        int value = i * 100;
        auto result = loaded_set.find(value);
        std::cout << "Find " << value << " in loaded set: "
                  << (result && *result ? "Found" : "Not found") << std::endl;
    }

    print_subheader("Async File Operations");

    // Perform async save
    std::promise<void> save_promise;
    auto save_future = save_promise.get_future();

    file_set.async_save_to_file(filename + ".async", [&](bool success) {
        std::cout << "Async save completed: "
                  << (success ? "Success" : "Failure") << std::endl;
        save_promise.set_value();
    });

    save_future.wait();

    // ===============================================================
    // 9. Conditional Find and Parallel ForEach
    // ===============================================================
    print_header("9. CONDITIONAL FIND AND PARALLEL FOREACH");

    // Create a set with data for searching
    atom::type::concurrent_set<int> search_set;
    for (int i = 1; i <= 10000; i++) {
        search_set.insert(i);
    }

    print_subheader("Conditional Find");

    // Find even numbers between 100 and 200
    auto even_numbers = search_set.conditional_find([](int value) {
        return value >= 100 && value <= 200 && value % 2 == 0;
    });

    std::cout << "Found " << even_numbers.size()
              << " even numbers between 100 and 200" << std::endl;
    std::cout << "First 5 values: ";
    for (size_t i = 0; i < std::min(size_t(5), even_numbers.size()); i++) {
        std::cout << even_numbers[i] << " ";
    }
    std::cout << std::endl;

    print_subheader("Async Conditional Find");

    // Async conditional find
    std::promise<void> find_promise;
    auto find_future = find_promise.get_future();

    search_set.async_conditional_find(
        [](int value) { return value >= 9900 && value <= 10000; },
        [&](std::vector<int> results) {
            std::cout << "Async conditional find complete: found "
                      << results.size() << " values" << std::endl;
            if (!results.empty()) {
                std::cout << "First result: " << results[0] << std::endl;
            }
            find_promise.set_value();
        });

    find_future.wait();

    print_subheader("Parallel ForEach");

    // Use parallel_for_each to compute sum
    std::atomic<int> sum{0};

    auto foreach_time = time_execution([&]() {
        search_set.parallel_for_each([&sum](int value) { sum += value; });
    });

    std::cout << "Parallel sum of all elements: " << sum << std::endl;
    print_timing("parallel_for_each over 10000 items", foreach_time);

    // ===============================================================
    // 10. Transaction Support
    // ===============================================================
    print_header("10. TRANSACTION SUPPORT");

    atom::type::concurrent_set<int> tx_set;
    for (int i = 1; i <= 10; i++) {
        tx_set.insert(i);
    }

    std::cout << "Initial set size: " << tx_set.size() << std::endl;

    print_subheader("Successful Transaction");

    // Create a transaction that will succeed
    std::vector<std::function<void()>> successful_tx = {
        [&]() { tx_set.insert(100); }, [&]() { tx_set.insert(200); },
        [&]() { tx_set.erase(5); }};

    bool tx_success = tx_set.transaction(successful_tx);

    std::cout << "Transaction success: " << (tx_success ? "Yes" : "No")
              << std::endl;
    std::cout << "Set size after transaction: " << tx_set.size() << std::endl;

    // Check the values
    auto find_100 = tx_set.find(100);
    auto find_5 = tx_set.find(5);

    std::cout << "Find 100: " << (find_100 && *find_100 ? "Found" : "Not found")
              << std::endl;
    std::cout << "Find 5: " << (find_5 && *find_5 ? "Found" : "Not found")
              << std::endl;

    print_subheader("Failed Transaction");

    // Create a transaction that will fail (throws an exception)
    std::vector<std::function<void()>> failing_tx = {
        [&]() { tx_set.insert(300); }, [&]() { tx_set.insert(400); },
        [&]() { throw std::runtime_error("Intentional error in transaction"); },
        [&]() { tx_set.insert(500); }};

    bool tx_failed = false;
    try {
        tx_failed = !tx_set.transaction(failing_tx);
    } catch (const atom::type::transaction_exception& e) {
        std::cout << "Transaction exception: " << e.what() << std::endl;
        tx_failed = true;
    }

    std::cout << "Transaction failed: " << (tx_failed ? "Yes" : "No")
              << std::endl;
    std::cout << "Set size after failed transaction: " << tx_set.size()
              << std::endl;

    // Check that none of the new values were inserted
    auto find_300 = tx_set.find(300);
    auto find_400 = tx_set.find(400);
    auto find_500 = tx_set.find(500);

    std::cout << "Find 300: " << (find_300 && *find_300 ? "Found" : "Not found")
              << std::endl;
    std::cout << "Find 400: " << (find_400 && *find_400 ? "Found" : "Not found")
              << std::endl;
    std::cout << "Find 500: " << (find_500 && *find_500 ? "Found" : "Not found")
              << std::endl;

    // ===============================================================
    // 11. Performance Metrics
    // ===============================================================
    print_header("11. PERFORMANCE METRICS");

    // Create a set specifically for metrics testing
    atom::type::concurrent_set<int> metric_set;

    // Perform various operations
    for (int i = 0; i < 1000; i++) {
        metric_set.insert(i);
    }

    for (int i = 0; i < 10000; i++) {
        metric_set.find(i % 2000);
    }

    for (int i = 0; i < 500; i++) {
        metric_set.erase(i);
    }

    // Get metrics
    size_t insertion_count = metric_set.get_insertion_count();
    size_t deletion_count = metric_set.get_deletion_count();
    size_t find_count = metric_set.get_find_count();
    size_t error_count = metric_set.get_error_count();

    std::cout << "Operation counts:" << std::endl;
    std::cout << "  Insertions: " << insertion_count << std::endl;
    std::cout << "  Deletions: " << deletion_count << std::endl;
    std::cout << "  Finds: " << find_count << std::endl;
    std::cout << "  Errors: " << error_count << std::endl;

    // Get pending task count
    size_t pending_tasks = metric_set.get_pending_task_count();
    std::cout << "Pending tasks: " << pending_tasks << std::endl;

    // ===============================================================
    // 12. Cleanup and Final Statistics
    // ===============================================================
    print_header("12. CLEANUP AND FINAL STATISTICS");

    // Clean up file
    std::remove(filename.c_str());
    std::remove((filename + ".async").c_str());

    // Print counts for all sets used in the example
    std::cout << "Final set sizes:" << std::endl;
    std::cout << "  int_set: " << int_set.size() << std::endl;
    std::cout << "  async_set: " << async_set.size() << std::endl;
    std::cout << "  batch_set: " << batch_set.size() << std::endl;
    std::cout << "  complex_set: " << complex_set.size() << std::endl;
    std::cout << "  search_set: " << search_set.size() << std::endl;

    std::cout << "\nExample completed successfully!" << std::endl;

    return 0;
}
