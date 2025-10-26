/**
 * @file high_performance_containers_example.cpp
 * @brief Comprehensive example demonstrating the Atom Containers module's
 * high-performance container capabilities
 *
 * This example shows how to:
 * - Use high-performance containers (flat_map, flat_set, small_vector, etc.)
 * - Work with lock-free containers for concurrent programming
 * - Utilize intrusive containers for memory-efficient data structures
 * - Compare performance characteristics of different container types
 * - Handle boost containers and fallback to standard library
 *
 * @author Max Qian
 * @date 2024-12-19
 */

#include <algorithm>
#include <chrono>
#include <iostream>
#include <mutex>
#include <queue>
#include <random>
#include <string>
#include <thread>
#include <vector>

// Atom Containers module headers
#include "atom/containers/boost_containers.hpp"
#include "atom/containers/high_performance.hpp"
#include "atom/containers/intrusive.hpp"
#include "atom/containers/lockfree.hpp"

using namespace atom::containers;

/**
 * @brief Demonstrates high-performance flat containers
 */
void flatContainersExample() {
    std::cout << "\n=== High-Performance Flat Containers Example ===\n";

    try {
        // Flat map - better cache locality than std::map
        hp::flat_map<std::string, int> scores;

        std::cout << "Adding scores to flat_map...\n";
        scores["Alice"] = 95;
        scores["Bob"] = 87;
        scores["Charlie"] = 92;
        scores["Diana"] = 98;
        scores["Eve"] = 89;

        std::cout << "Scores in flat_map (sorted by key):\n";
        for (const auto& [name, score] : scores) {
            std::cout << "  " << name << ": " << score << "\n";
        }

        // Flat set - better cache locality than std::set
        hp::flat_set<std::string> unique_names;
        unique_names.insert("Alice");
        unique_names.insert("Bob");
        unique_names.insert("Alice");  // Duplicate, won't be added
        unique_names.insert("Charlie");

        std::cout << "\nUnique names in flat_set:\n";
        for (const auto& name : unique_names) {
            std::cout << "  " << name << "\n";
        }

        // Performance comparison with lookup
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 10000; ++i) {
            auto it = scores.find("Charlie");
            if (it != scores.end()) {
                volatile int temp = it->second;  // Prevent optimization
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "\nFlat map lookup performance: " << duration.count()
                  << " microseconds for 10,000 lookups\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in flat containers example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates small and static vectors for optimized memory usage
 */
void smallVectorExample() {
    std::cout << "\n=== Small Vector and Static Vector Example ===\n";

    try {
        // Small vector - stack allocation for small sizes, heap for larger
        hp::small_vector<int, 8> small_vec;  // 8 elements on stack

        std::cout << "Adding elements to small_vector...\n";
        for (int i = 1; i <= 12; ++i) {
            small_vec.push_back(i * i);
        }

        std::cout
            << "Small vector contents (first 8 on stack, rest on heap):\n";
        for (size_t i = 0; i < small_vec.size(); ++i) {
            std::cout << "  [" << i << "] = " << small_vec[i] << "\n";
        }

        // Static vector - fixed capacity, all on stack
        hp::static_vector<std::string, 5> static_vec;

        std::cout
            << "\nDemonstrating static_vector (fixed capacity container)...\n";

#ifdef ATOM_HAS_BOOST_CONTAINER
        // With Boost, static_vector has push_back
        std::cout << "Using Boost static_vector with push_back:\n";
        static_vec.push_back("First");
        static_vec.push_back("Second");
        static_vec.push_back("Third");
        static_vec.push_back("Fourth");
        static_vec.push_back("Fifth");

        std::cout << "Static vector contents:\n";
        for (size_t i = 0; i < static_vec.size(); ++i) {
            std::cout << "  [" << i << "] = " << static_vec[i] << "\n";
        }

        std::cout << "Static vector capacity: " << static_vec.capacity()
                  << "\n";
        std::cout << "Static vector size: " << static_vec.size() << "\n";
#else
        // Fallback to std::array - initialize directly
        std::cout << "Using std::array fallback (initialized directly):\n";
        static_vec = {"First", "Second", "Third", "Fourth", "Fifth"};

        std::cout << "Static vector contents:\n";
        for (size_t i = 0; i < static_vec.size(); ++i) {
            std::cout << "  [" << i << "] = " << static_vec[i] << "\n";
        }

        std::cout << "Static vector capacity: " << static_vec.size()
                  << " (std::array)\n";
        std::cout << "Static vector size: " << static_vec.size() << "\n";
#endif

    } catch (const std::exception& e) {
        std::cerr << "Error in small vector example: " << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates lock-free containers for concurrent programming
 */
void lockFreeContainersExample() {
    std::cout << "\n=== Lock-Free Containers Example ===\n";

    try {
#ifdef ATOM_HAS_BOOST_LOCKFREE
        // Multi-producer multi-consumer queue
        hp::lockfree::queue<int, 256> mpmc_queue;

        std::cout << "Testing multi-producer multi-consumer queue...\n";

        // Producer thread
        std::thread producer([&mpmc_queue]() {
            for (int i = 1; i <= 100; ++i) {
                while (!mpmc_queue.push(i)) {
                    std::this_thread::yield();  // Wait if queue is full
                }
                if (i % 20 == 0) {
                    std::cout << "Produced: " << i << " items\n";
                }
            }
        });

        // Consumer thread
        std::vector<int> consumed;
        std::thread consumer([&mpmc_queue, &consumed]() {
            int item;
            int count = 0;
            while (count < 100) {
                if (mpmc_queue.pop(item)) {
                    consumed.push_back(item);
                    count++;
                    if (count % 20 == 0) {
                        std::cout << "Consumed: " << count << " items\n";
                    }
                } else {
                    std::this_thread::yield();
                }
            }
        });

        producer.join();
        consumer.join();

        std::cout << "Successfully processed " << consumed.size()
                  << " items through lock-free queue\n";

        // Single-producer single-consumer queue (more efficient)
        hp::lockfree::spsc_queue<std::string, 64> spsc_queue;

        std::cout << "\nTesting single-producer single-consumer queue...\n";

        std::thread spsc_producer([&spsc_queue]() {
            for (int i = 1; i <= 10; ++i) {
                std::string msg = "Message " + std::to_string(i);
                while (!spsc_queue.push(msg)) {
                    std::this_thread::yield();
                }
            }
        });

        std::vector<std::string> messages;
        std::thread spsc_consumer([&spsc_queue, &messages]() {
            std::string msg;
            int count = 0;
            while (count < 10) {
                if (spsc_queue.pop(msg)) {
                    messages.push_back(msg);
                    count++;
                } else {
                    std::this_thread::yield();
                }
            }
        });

        spsc_producer.join();
        spsc_consumer.join();

        std::cout << "SPSC queue processed messages:\n";
        for (const auto& msg : messages) {
            std::cout << "  " << msg << "\n";
        }

        // Lock-free stack
        hp::lockfree::stack<int, 128> lf_stack;

        std::cout << "\nTesting lock-free stack...\n";

        // Push items
        for (int i = 1; i <= 10; ++i) {
            while (!lf_stack.push(i)) {
                std::this_thread::yield();
            }
        }

        // Pop items
        std::cout << "Popping from lock-free stack (LIFO order):\n";
        int item;
        while (lf_stack.pop(item)) {
            std::cout << "  Popped: " << item << "\n";
        }

#else
        std::cout << "Lock-free containers require Boost.Lockfree library\n";
        std::cout << "Using fallback implementations...\n";

        // Fallback to standard containers with manual synchronization
        std::queue<int> fallback_queue;
        std::mutex queue_mutex;

        std::cout << "Using standard queue with mutex for thread safety\n";

        std::thread producer([&fallback_queue, &queue_mutex]() {
            for (int i = 1; i <= 10; ++i) {
                {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    fallback_queue.push(i);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

        std::thread consumer([&fallback_queue, &queue_mutex]() {
            int count = 0;
            while (count < 10) {
                int item = 0;
                bool got_item = false;
                {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    if (!fallback_queue.empty()) {
                        item = fallback_queue.front();
                        fallback_queue.pop();
                        got_item = true;
                    }
                }
                if (got_item) {
                    std::cout << "  Consumed: " << item << "\n";
                    count++;
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
        });

        producer.join();
        consumer.join();
#endif

    } catch (const std::exception& e) {
        std::cerr << "Error in lock-free containers example: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates performance comparison between different container types
 */
void performanceComparisonExample() {
    std::cout << "\n=== Performance Comparison Example ===\n";

    try {
        const int num_operations = 100000;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 1000);

        // Generate test data
        std::vector<int> test_data;
        test_data.reserve(num_operations);
        for (int i = 0; i < num_operations; ++i) {
            test_data.push_back(dis(gen));
        }

        std::cout << "Comparing insertion performance for " << num_operations
                  << " elements...\n";

        // Test standard vector
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<int> std_vec;
        std_vec.reserve(num_operations);
        for (int val : test_data) {
            std_vec.push_back(val);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto std_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // Test small vector
        start = std::chrono::high_resolution_clock::now();
        hp::small_vector<int, 16> small_vec;
        for (int val : test_data) {
            small_vec.push_back(val);
        }
        end = std::chrono::high_resolution_clock::now();
        auto small_duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Results:\n";
        std::cout << "  std::vector: " << std_duration.count()
                  << " microseconds\n";
        std::cout << "  small_vector: " << small_duration.count()
                  << " microseconds\n";

        if (small_duration < std_duration) {
            std::cout << "  small_vector is "
                      << (double)std_duration.count() / small_duration.count()
                      << "x faster\n";
        } else {
            std::cout << "  std::vector is "
                      << (double)small_duration.count() / std_duration.count()
                      << "x faster\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error in performance comparison example: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Main function demonstrating all container capabilities
 */
int main() {
    std::cout << "=== Atom High-Performance Containers Module Example ===\n";
    std::cout << "Demonstrating various high-performance container types...\n";

    try {
        // Run all examples
        flatContainersExample();
        smallVectorExample();
        lockFreeContainersExample();
        performanceComparisonExample();

        std::cout << "\n=== All Examples Completed Successfully ===\n";
        std::cout << "The containers module provides:\n";
        std::cout
            << "  ✓ High-performance flat containers (flat_map, flat_set)\n";
        std::cout
            << "  ✓ Memory-optimized vectors (small_vector, static_vector)\n";
        std::cout << "  ✓ Lock-free containers for concurrent programming\n";
        std::cout
            << "  ✓ Intrusive containers for zero-allocation data structures\n";
        std::cout << "  ✓ Boost container integration with standard library "
                     "fallbacks\n";
        std::cout << "  ✓ Performance optimizations for cache locality\n";
        std::cout << "  ✓ Thread-safe concurrent data structures\n";

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
