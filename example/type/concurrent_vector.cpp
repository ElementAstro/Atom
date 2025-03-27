#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "atom/type/concurrent_vector.hpp"

// A sample class to demonstrate object handling
class Person {
private:
    int id;
    std::string name;
    int age;

public:
    Person() : id(0), name(""), age(0) {}

    Person(int id, std::string name, int age)
        : id(id), name(std::move(name)), age(age) {}

    // Copy constructor
    Person(const Person& other) = default;

    // Move constructor
    Person(Person&& other) noexcept
        : id(other.id), name(std::move(other.name)), age(other.age) {}

    // Copy assignment
    Person& operator=(const Person& other) = default;

    // Move assignment
    Person& operator=(Person&& other) noexcept {
        if (this != &other) {
            id = other.id;
            name = std::move(other.name);
            age = other.age;
        }
        return *this;
    }

    // Equality operator
    bool operator==(const Person& other) const {
        return id == other.id && name == other.name && age == other.age;
    }

    // Getters
    int getId() const { return id; }
    const std::string& getName() const { return name; }
    int getAge() const { return age; }

    // Setters
    void setName(const std::string& newName) { name = newName; }
    void setAge(int newAge) { age = newAge; }

    // For debugging
    friend std::ostream& operator<<(std::ostream& os, const Person& p) {
        os << "Person(id=" << p.id << ", name='" << p.name << "', age=" << p.age
           << ")";
        return os;
    }
};

// Helper function to print a section header
void printHeader(const std::string& title) {
    std::cout << "\n==============================================="
              << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "===============================================" << std::endl;
}

// Helper function to print a subsection header
void printSubheader(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

// Helper to measure function execution time
template <typename F, typename... Args>
auto measureExecutionTime(F func, Args&&... args) {
    auto start = std::chrono::high_resolution_clock::now();
    func(std::forward<Args>(args)...);
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
        .count();
}

int main() {
    std::cout << "======================================================="
              << std::endl;
    std::cout << "     COMPREHENSIVE CONCURRENT_VECTOR USAGE EXAMPLE     "
              << std::endl;
    std::cout << "======================================================="
              << std::endl;

    // Random number generator setup for demonstration
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<int> dist(1, 1000);
    std::uniform_int_distribution<int> age_dist(18, 80);

    // Define some common names for our Person objects
    std::vector<std::string> firstNames = {
        "John",  "Alice", "Bob",    "Carol",  "David", "Emma",
        "Frank", "Grace", "Henry",  "Isabel", "Jack",  "Karen",
        "Leo",   "Maria", "Nathan", "Olivia"};

    // ============================================================
    // 1. Basic Construction and Initialization
    // ============================================================
    printHeader("1. BASIC CONSTRUCTION AND INITIALIZATION");

    // Create vector with default settings
    atom::type::concurrent_vector<int> vec1;
    std::cout << "Default constructed vector:" << std::endl;
    std::cout << "  Size: " << vec1.size() << std::endl;
    std::cout << "  Capacity: " << vec1.capacity() << std::endl;
    std::cout << "  Empty: " << (vec1.empty() ? "Yes" : "No") << std::endl;
    std::cout << "  Thread count: " << vec1.thread_count() << std::endl;

    // Create vector with initial capacity and custom thread count
    size_t initial_capacity = 100;
    size_t thread_count = 4;
    atom::type::concurrent_vector<int> vec2(initial_capacity, thread_count);
    std::cout << "\nCustom constructed vector:" << std::endl;
    std::cout << "  Initial capacity: " << vec2.capacity() << std::endl;
    std::cout << "  Thread count: " << vec2.thread_count() << std::endl;

    // Demonstrate move construction
    printSubheader("Move Construction");

    atom::type::concurrent_vector<int> source_vec(50, 2);
    for (int i = 1; i <= 10; i++) {
        source_vec.push_back(i);
    }

    std::cout << "Source vector before move:" << std::endl;
    std::cout << "  Size: " << source_vec.size() << std::endl;
    std::cout << "  First element: " << source_vec.front() << std::endl;
    std::cout << "  Last element: " << source_vec.back() << std::endl;

    // Move construct a new vector
    atom::type::concurrent_vector<int> moved_vec(std::move(source_vec));

    std::cout << "\nAfter move construction:" << std::endl;
    std::cout << "  Moved vector size: " << moved_vec.size() << std::endl;
    std::cout << "  Moved vector first element: " << moved_vec.front()
              << std::endl;
    std::cout << "  Moved vector last element: " << moved_vec.back()
              << std::endl;
    std::cout << "  Source vector size: " << source_vec.size() << std::endl;

    // ============================================================
    // 2. Basic Operations: push_back, pop_back, at, operator[]
    // ============================================================
    printHeader("2. BASIC OPERATIONS");

    atom::type::concurrent_vector<int> basic_vec;

    printSubheader("Adding Elements");

    // Add elements with push_back
    for (int i = 1; i <= 10; i++) {
        basic_vec.push_back(i * 10);
    }

    std::cout << "Vector after push_back:" << std::endl;
    std::cout << "  Size: " << basic_vec.size() << std::endl;
    std::cout << "  Capacity: " << basic_vec.capacity() << std::endl;
    std::cout << "  Content: ";
    for (size_t i = 0; i < basic_vec.size(); i++) {
        std::cout << basic_vec[i] << " ";
    }
    std::cout << std::endl;

    // Demonstrate at() with bounds checking
    printSubheader("Element Access");

    try {
        std::cout << "Element at index 5: " << basic_vec.at(5) << std::endl;
        std::cout << "Trying to access element at index 20..." << std::endl;
        std::cout << basic_vec.at(20) << std::endl;
    } catch (const atom::type::concurrent_vector_error& e) {
        std::cout << "  Caught exception: " << e.what() << std::endl;
    }

    // Demonstrate operator[] access
    std::cout << "\nAccessing elements with operator[]:" << std::endl;
    std::cout << "  First element: " << basic_vec[0] << std::endl;
    std::cout << "  Last element: " << basic_vec[basic_vec.size() - 1]
              << std::endl;

    // Demonstrate front() and back()
    printSubheader("Front and Back Access");

    std::cout << "Front element: " << basic_vec.front() << std::endl;
    std::cout << "Back element: " << basic_vec.back() << std::endl;

    // Demonstrate pop_back
    printSubheader("Removing Elements");

    std::cout << "Popping elements:" << std::endl;
    for (int i = 1; i <= 3; i++) {
        auto popped = basic_vec.pop_back();
        if (popped) {
            std::cout << "  Popped: " << *popped << std::endl;
        }
    }

    std::cout << "Vector after pop_back:" << std::endl;
    std::cout << "  Size: " << basic_vec.size() << std::endl;
    std::cout << "  Content: ";
    for (size_t i = 0; i < basic_vec.size(); i++) {
        std::cout << basic_vec[i] << " ";
    }
    std::cout << std::endl;

    // ============================================================
    // 3. Advanced Element Management: emplace_back, reserve, shrink_to_fit
    // ============================================================
    printHeader("3. ADVANCED ELEMENT MANAGEMENT");

    atom::type::concurrent_vector<Person> person_vec;

    printSubheader("Emplacing Elements");

    // Demonstrate emplace_back
    for (int i = 1; i <= 5; i++) {
        std::string name = firstNames[i % firstNames.size()];
        person_vec.emplace_back(i, name, age_dist(rng));
    }

    std::cout << "Person vector after emplace_back:" << std::endl;
    std::cout << "  Size: " << person_vec.size() << std::endl;
    std::cout << "  Content:" << std::endl;
    for (size_t i = 0; i < person_vec.size(); i++) {
        std::cout << "  " << person_vec[i] << std::endl;
    }

    printSubheader("Memory Management");

    // Demonstrate reserve
    size_t new_capacity = 20;
    std::cout << "Before reserve(" << new_capacity << "):" << std::endl;
    std::cout << "  Capacity: " << person_vec.capacity() << std::endl;

    person_vec.reserve(new_capacity);

    std::cout << "After reserve:" << std::endl;
    std::cout << "  Capacity: " << person_vec.capacity() << std::endl;

    // Demonstrate shrink_to_fit
    std::cout << "\nBefore shrink_to_fit:" << std::endl;
    std::cout << "  Size: " << person_vec.size() << std::endl;
    std::cout << "  Capacity: " << person_vec.capacity() << std::endl;

    person_vec.shrink_to_fit();

    std::cout << "After shrink_to_fit:" << std::endl;
    std::cout << "  Size: " << person_vec.size() << std::endl;
    std::cout << "  Capacity: " << person_vec.capacity() << std::endl;

    // ============================================================
    // 4. Batch Operations: batch_insert, clear_range
    // ============================================================
    printHeader("4. BATCH OPERATIONS");

    atom::type::concurrent_vector<int> batch_vec;

    printSubheader("Batch Insert");

    // Prepare a batch of integers
    std::vector<int> batch;
    for (int i = 1; i <= 100; i++) {
        batch.push_back(dist(rng));
    }

    // Perform batch insert
    batch_vec.batch_insert(batch);

    std::cout << "Vector after batch_insert:" << std::endl;
    std::cout << "  Size: " << batch_vec.size() << std::endl;
    std::cout << "  First few elements: ";
    for (size_t i = 0; i < std::min(batch_vec.size(), size_t(10)); i++) {
        std::cout << batch_vec[i] << " ";
    }
    std::cout << "..." << std::endl;

    // Demonstrate batch insert with move semantics
    printSubheader("Batch Insert with Move Semantics");

    std::vector<int> move_batch;
    for (int i = 1; i <= 50; i++) {
        move_batch.push_back(1000 + i);  // Higher numbers to distinguish
    }

    size_t original_size = batch_vec.size();
    batch_vec.batch_insert(std::move(move_batch));

    std::cout << "Vector after move batch_insert:" << std::endl;
    std::cout << "  New size: " << batch_vec.size() << std::endl;
    std::cout << "  Newly added elements: ";
    for (size_t i = original_size;
         i < std::min(batch_vec.size(), original_size + 10); i++) {
        std::cout << batch_vec[i] << " ";
    }
    std::cout << "..." << std::endl;

    printSubheader("Clear Range");

    // Demonstrate clear_range
    size_t start_idx = 10;
    size_t end_idx = 30;

    std::cout << "Before clear_range(" << start_idx << ", " << end_idx
              << "):" << std::endl;
    std::cout << "  Size: " << batch_vec.size() << std::endl;

    batch_vec.clear_range(start_idx, end_idx);

    std::cout << "After clear_range:" << std::endl;
    std::cout << "  Size: " << batch_vec.size() << std::endl;
    std::cout << "  Elements around cleared range: ";
    for (size_t i = std::max(start_idx, size_t(5)) - 5;
         i < std::min(start_idx + 5, batch_vec.size()); i++) {
        std::cout << batch_vec[i] << " ";
    }
    std::cout << std::endl;

    // ============================================================
    // 5. Parallel Operations: parallel_for_each, parallel_find,
    // parallel_transform
    // ============================================================
    printHeader("5. PARALLEL OPERATIONS");

    // Create a vector with more data for parallel operations
    atom::type::concurrent_vector<int> parallel_vec;
    for (int i = 0; i < 10000; i++) {
        parallel_vec.push_back(dist(rng));
    }

    printSubheader("Parallel ForEach");

    // Calculate sum using parallel_for_each
    std::atomic<int64_t> sum(0);

    auto parallel_time = measureExecutionTime([&]() {
        parallel_vec.parallel_for_each([&sum](int value) { sum += value; });
    });

    // Calculate sum sequentially for comparison
    int64_t sequential_sum = 0;
    auto sequential_time = measureExecutionTime([&]() {
        for (size_t i = 0; i < parallel_vec.size(); i++) {
            sequential_sum += parallel_vec[i];
        }
    });

    std::cout << "Parallel sum: " << sum << std::endl;
    std::cout << "Sequential sum: " << sequential_sum << std::endl;
    std::cout << "Parallel execution time: " << parallel_time << " ms"
              << std::endl;
    std::cout << "Sequential execution time: " << sequential_time << " ms"
              << std::endl;
    std::cout << "Speedup: " << std::fixed << std::setprecision(2)
              << (sequential_time > 0
                      ? static_cast<double>(sequential_time) / parallel_time
                      : 0)
              << "x" << std::endl;

    printSubheader("Parallel Find");

    // Insert a specific value to find
    int target_value = 12345;
    size_t target_index = parallel_vec.size() / 2;
    parallel_vec.at(target_index) = target_value;

    // Find the value in parallel
    auto find_time = measureExecutionTime([&]() {
        auto found_index = parallel_vec.parallel_find(target_value);

        std::cout << "Target value " << target_value << ": "
                  << (found_index
                          ? "Found at index " + std::to_string(*found_index)
                          : "Not found")
                  << std::endl;
    });

    // Find a non-existent value
    auto not_found_time = measureExecutionTime([&]() {
        auto found_index = parallel_vec.parallel_find(999999);

        std::cout << "Non-existent value: "
                  << (found_index
                          ? "Found at index " + std::to_string(*found_index)
                          : "Not found")
                  << std::endl;
    });

    std::cout << "Find execution time: " << find_time << " ms" << std::endl;
    std::cout << "Not-found execution time: " << not_found_time << " ms"
              << std::endl;

    printSubheader("Parallel Transform");

    // Create a smaller vector for transformation demonstration
    atom::type::concurrent_vector<int> transform_vec;
    for (int i = 1; i <= 100; i++) {
        transform_vec.push_back(i);
    }

    std::cout << "Before transformation (first 10 elements): ";
    for (size_t i = 0; i < std::min(transform_vec.size(), size_t(10)); i++) {
        std::cout << transform_vec[i] << " ";
    }
    std::cout << std::endl;

    // Apply parallel transformation (square each value)
    transform_vec.parallel_transform([](int& value) { value = value * value; });

    std::cout << "After transformation (first 10 elements): ";
    for (size_t i = 0; i < std::min(transform_vec.size(), size_t(10)); i++) {
        std::cout << transform_vec[i] << " ";
    }
    std::cout << std::endl;

    // ============================================================
    // 6. Parallel Batch Operations: parallel_batch_insert
    // ============================================================
    printHeader("6. PARALLEL BATCH OPERATIONS");

    atom::type::concurrent_vector<int> parallel_batch_vec;

    // Prepare a large batch of data
    std::vector<int> large_batch;
    for (int i = 0; i < 100000; i++) {
        large_batch.push_back(dist(rng));
    }

    // Insert batch in parallel
    std::cout << "Inserting 100,000 elements in parallel..." << std::endl;
    auto parallel_batch_time = measureExecutionTime(
        [&]() { parallel_batch_vec.parallel_batch_insert(large_batch); });

    // For comparison, insert batch sequentially
    atom::type::concurrent_vector<int> sequential_batch_vec;
    auto sequential_batch_time = measureExecutionTime(
        [&]() { sequential_batch_vec.batch_insert(large_batch); });

    std::cout << "Parallel batch insert: " << parallel_batch_time << " ms"
              << std::endl;
    std::cout << "Sequential batch insert: " << sequential_batch_time << " ms"
              << std::endl;
    std::cout << "Speedup: " << std::fixed << std::setprecision(2)
              << (sequential_batch_time > 0
                      ? static_cast<double>(sequential_batch_time) /
                            parallel_batch_time
                      : 0)
              << "x" << std::endl;

    std::cout << "Final vector size: " << parallel_batch_vec.size()
              << std::endl;

    // ============================================================
    // 7. Task Submission and Waiting: submit_task, wait_for_tasks
    // ============================================================
    printHeader("7. TASK SUBMISSION AND WAITING");

    atom::type::concurrent_vector<int> task_vec;
    task_vec.reserve(100);

    std::cout << "Submitting 10 tasks..." << std::endl;

    // Submit multiple tasks
    std::vector<std::future<int>> task_results;
    for (int i = 0; i < 10; i++) {
        std::promise<int> promise;
        task_results.push_back(promise.get_future());

        task_vec.submit_task([i, promise = std::move(promise)]() mutable {
            // Simulate work
            std::this_thread::sleep_for(
                std::chrono::milliseconds(100 + i * 20));

            // Set result
            promise.set_value(i * 100);
        });
    }

    std::cout << "Waiting for all tasks to complete..." << std::endl;
    task_vec.wait_for_tasks();

    std::cout << "All tasks completed!" << std::endl;
    std::cout << "Task results: ";
    for (auto& future : task_results) {
        if (future.valid()) {
            try {
                std::cout << future.get() << " ";
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << " ";
            }
        } else {
            std::cout << "Invalid ";
        }
    }
    std::cout << std::endl;

    // ============================================================
    // 8. Error Handling
    // ============================================================
    printHeader("8. ERROR HANDLING");

    atom::type::concurrent_vector<int> error_vec;

    printSubheader("Out-of-bounds Access");

    try {
        std::cout << "Trying to access element at index 5 in empty vector..."
                  << std::endl;
        error_vec.at(5);
    } catch (const atom::type::concurrent_vector_error& e) {
        std::cout << "  Caught expected exception: " << e.what() << std::endl;
    }

    printSubheader("Invalid Clear Range");

    // Add some elements
    for (int i = 0; i < 10; i++) {
        error_vec.push_back(i);
    }

    try {
        std::cout << "Trying to clear invalid range (15, 20)..." << std::endl;
        error_vec.clear_range(15, 20);
    } catch (const atom::type::concurrent_vector_error& e) {
        std::cout << "  Caught expected exception: " << e.what() << std::endl;
    }

    try {
        std::cout << "Trying to clear invalid range (5, 3)..." << std::endl;
        error_vec.clear_range(5, 3);
    } catch (const atom::type::concurrent_vector_error& e) {
        std::cout << "  Caught expected exception: " << e.what() << std::endl;
    }

    printSubheader("Pop from Empty Vector");

    // Clear the vector
    error_vec.clear();

    try {
        std::cout << "Trying to pop from empty vector..." << std::endl;
        error_vec.pop_back();
    } catch (const atom::type::concurrent_vector_error& e) {
        std::cout << "  Caught expected exception: " << e.what() << std::endl;
    }

    // ============================================================
    // 9. Data Access and Clear
    // ============================================================
    printHeader("9. DATA ACCESS AND CLEAR");

    atom::type::concurrent_vector<int> final_vec;
    for (int i = 1; i <= 10; i++) {
        final_vec.push_back(i * 10);
    }

    printSubheader("Getting Data Copy");

    // Get a const reference to the data
    const std::vector<int>& data_ref = final_vec.get_data();

    std::cout << "Data retrieved with get_data():" << std::endl;
    std::cout << "  Size: " << data_ref.size() << std::endl;
    std::cout << "  Content: ";
    for (size_t i = 0; i < data_ref.size(); i++) {
        std::cout << data_ref[i] << " ";
    }
    std::cout << std::endl;

    printSubheader("Clearing Vector");

    std::cout << "Before clear():" << std::endl;
    std::cout << "  Size: " << final_vec.size() << std::endl;
    std::cout << "  Empty: " << (final_vec.empty() ? "Yes" : "No") << std::endl;

    final_vec.clear();

    std::cout << "After clear():" << std::endl;
    std::cout << "  Size: " << final_vec.size() << std::endl;
    std::cout << "  Empty: " << (final_vec.empty() ? "Yes" : "No") << std::endl;

    // ============================================================
    // 10. Performance Benchmarks and Comparison
    // ============================================================
    printHeader("10. PERFORMANCE BENCHMARKS");

    const int benchmark_size = 1000000;

    printSubheader("Standard Vector vs. Concurrent Vector");

    // Benchmark standard vector push_back
    std::vector<int> std_vec;
    std_vec.reserve(benchmark_size);

    auto std_push_time = measureExecutionTime([&]() {
        for (int i = 0; i < benchmark_size; i++) {
            std_vec.push_back(i);
        }
    });

    // Benchmark concurrent_vector push_back
    atom::type::concurrent_vector<int> conc_vec(benchmark_size);

    auto conc_push_time = measureExecutionTime([&]() {
        for (int i = 0; i < benchmark_size; i++) {
            conc_vec.push_back(i);
        }
    });

    std::cout << "Standard vector push_back: " << std_push_time << " ms"
              << std::endl;
    std::cout << "Concurrent vector push_back: " << conc_push_time << " ms"
              << std::endl;

    // Benchmark find operations
    int find_target = benchmark_size / 2;

    auto std_find_time = measureExecutionTime([&]() {
        auto it = std::find(std_vec.begin(), std_vec.end(), find_target);
        std::cout << "Standard vector find: "
                  << (it != std_vec.end()
                          ? "Found at index " +
                                std::to_string(it - std_vec.begin())
                          : "Not found")
                  << std::endl;
    });

    auto conc_find_time = measureExecutionTime([&]() {
        auto index = conc_vec.parallel_find(find_target);
        std::cout << "Concurrent vector find: "
                  << (index ? "Found at index " + std::to_string(*index)
                            : "Not found")
                  << std::endl;
    });

    std::cout << "Standard vector find: " << std_find_time << " ms"
              << std::endl;
    std::cout << "Concurrent vector parallel_find: " << conc_find_time << " ms"
              << std::endl;
    std::cout << "Speedup: " << std::fixed << std::setprecision(2)
              << (std_find_time > 0
                      ? static_cast<double>(std_find_time) / conc_find_time
                      : 0)
              << "x" << std::endl;

    std::cout << "\n======================================================="
              << std::endl;
    std::cout << "     CONCURRENT_VECTOR EXAMPLE COMPLETED SUCCESSFULLY     "
              << std::endl;
    std::cout << "======================================================="
              << std::endl;

    return 0;
}