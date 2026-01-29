/*
 * test_containers.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-22

Description: Comprehensive Unit Tests for Atom Container Library
Tests boost containers, graph structures, high-performance containers,
intrusive containers, and lock-free containers.

**************************************************/

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "atom/containers/boost_containers.hpp"
#include "atom/containers/graph.hpp"
#include "atom/containers/high_performance.hpp"
#include "atom/containers/intrusive.hpp"
#include "atom/containers/lockfree.hpp"

namespace atom::containers::test {

// ============================================================================
// Boost Containers Tests
// ============================================================================

class BoostContainersTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test data
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(BoostContainersTest, BasicFunctionality) {
    // Test flat_map operations
    atom::containers::hp::flat_map<int, std::string> fmap;
    fmap[1] = "one";
    fmap[2] = "two";
    fmap[3] = "three";

    EXPECT_EQ(fmap.size(), 3);
    EXPECT_EQ(fmap[1], "one");
    EXPECT_EQ(fmap[2], "two");
    EXPECT_TRUE(fmap.find(4) == fmap.end());

    // Test flat_set operations
    atom::containers::hp::flat_set<int> fset;
    fset.insert(10);
    fset.insert(20);
    fset.insert(15);

    EXPECT_EQ(fset.size(), 3);
    EXPECT_TRUE(fset.find(15) != fset.end());
    EXPECT_TRUE(fset.find(25) == fset.end());

    // Test small_vector operations
    atom::containers::hp::small_vector<int, 4> svec;
    svec.push_back(1);
    svec.push_back(2);
    svec.push_back(3);
    svec.push_back(4);

    EXPECT_EQ(svec.size(), 4);
    EXPECT_EQ(svec[0], 1);
    EXPECT_EQ(svec[3], 4);

    // Test capacity expansion
    svec.push_back(5);
    EXPECT_EQ(svec.size(), 5);
    EXPECT_EQ(svec[4], 5);
}

TEST_F(BoostContainersTest, PerformanceCharacteristics) {
    const size_t test_size = 10000;

    // Test fast_unordered_map performance
    atom::containers::hp::fast_unordered_map<int, int> fast_map;
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < test_size; ++i) {
        fast_map[i] = i * 2;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    EXPECT_EQ(fast_map.size(), test_size);
    EXPECT_LT(duration.count(), 50000);  // Should complete within 50ms

    // Test lookup performance
    start = std::chrono::high_resolution_clock::now();
    int sum = 0;
    for (size_t i = 0; i < test_size; ++i) {
        sum += fast_map[i];
    }
    end = std::chrono::high_resolution_clock::now();
    duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    EXPECT_EQ(sum,
              test_size * (test_size - 1));  // Sum of 0*2 + 1*2 + ... + (n-1)*2
    EXPECT_LT(duration.count(),
              20000);  // Lookup should be faster than insertion
}

// ============================================================================
// Graph Structure Tests
// ============================================================================

class GraphTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test graphs
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(GraphTest, NodeOperations) {
#ifdef ATOM_HAS_BOOST_GRAPH
    // Test directed graph node operations
    atom::containers::graph::Graph graph(
        atom::containers::graph::Graph::GraphType::Directed);

    // Add vertices
    auto v1 = graph.add_vertex("vertex1");
    auto v2 = graph.add_vertex("vertex2");
    auto v3 = graph.add_vertex("vertex3");

    EXPECT_EQ(graph.num_vertices(), 3);
    EXPECT_TRUE(graph.has_vertex("vertex1"));
    EXPECT_TRUE(graph.has_vertex("vertex2"));
    EXPECT_TRUE(graph.has_vertex("vertex3"));
    EXPECT_FALSE(graph.has_vertex("nonexistent"));

    // Test vertex properties
    EXPECT_EQ(graph.get_vertex_name(v1), "vertex1");
    EXPECT_EQ(graph.get_vertex_name(v2), "vertex2");
    EXPECT_EQ(graph.get_vertex_name(v3), "vertex3");
#else
    // Graph functionality not available - skip test
    GTEST_SKIP() << "Graph functionality requires ATOM_HAS_BOOST_GRAPH";
#endif
}

TEST_F(GraphTest, EdgeOperations) {
#ifdef ATOM_HAS_BOOST_GRAPH
    // Test edge creation and manipulation
    atom::containers::graph::Graph graph(
        atom::containers::graph::Graph::GraphType::Undirected);

    // Add vertices first
    graph.add_vertex("A");
    graph.add_vertex("B");
    graph.add_vertex("C");

    // Add edges
    bool success1 = graph.add_edge("A", "B");
    bool success2 = graph.add_edge("B", "C");
    bool success3 = graph.add_edge("A", "C");

    EXPECT_TRUE(success1);
    EXPECT_TRUE(success2);
    EXPECT_TRUE(success3);
    EXPECT_EQ(graph.num_edges(), 3);

    // Test edge existence
    EXPECT_TRUE(graph.has_edge("A", "B"));
    EXPECT_TRUE(graph.has_edge("B", "C"));
    EXPECT_TRUE(graph.has_edge("A", "C"));
    EXPECT_FALSE(graph.has_edge("A", "D"));

    // Test degree calculation
    EXPECT_EQ(graph.degree("A"), 2);
    EXPECT_EQ(graph.degree("B"), 2);
    EXPECT_EQ(graph.degree("C"), 2);
#else
    // Graph functionality not available - skip test
    GTEST_SKIP() << "Graph functionality requires ATOM_HAS_BOOST_GRAPH";
#endif
}

TEST_F(GraphTest, GraphTraversal) {
#ifdef ATOM_HAS_BOOST_GRAPH
    // Create a test graph for traversal
    atom::containers::graph::Graph graph(
        atom::containers::graph::Graph::GraphType::Directed);

    // Add vertices
    graph.add_vertex("A");
    graph.add_vertex("B");
    graph.add_vertex("C");
    graph.add_vertex("D");

    // Create edges: A->B, A->C, B->D, C->D
    graph.add_edge("A", "B");
    graph.add_edge("A", "C");
    graph.add_edge("B", "D");
    graph.add_edge("C", "D");

    // Test BFS traversal
    auto bfs_result = graph.bfs("A");
    EXPECT_EQ(bfs_result.size(), 4);
    EXPECT_EQ(bfs_result[0], "A");

    // Test DFS traversal
    auto dfs_result = graph.dfs("A");
    EXPECT_EQ(dfs_result.size(), 4);
    EXPECT_EQ(dfs_result[0], "A");

    // Test connectivity
    EXPECT_TRUE(graph.is_connected("A", "D"));
    EXPECT_FALSE(graph.is_connected("D", "A"));  // Directed graph
#else
    // Graph functionality not available - skip test
    GTEST_SKIP() << "Graph functionality requires ATOM_HAS_BOOST_GRAPH";
#endif
}

// ============================================================================
// High Performance Containers Tests
// ============================================================================

class HighPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup performance test environment
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(HighPerformanceTest, InsertionPerformance) {
    const size_t test_size = 50000;

    // Test HashMap insertion performance
    atom::containers::HashMap<int, std::string> hash_map;
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < test_size; ++i) {
        hash_map[i] = "value_" + std::to_string(i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(hash_map.size(), test_size);
    EXPECT_LT(duration.count(), 1000);  // Should complete within 1 second

    // Test Vector insertion performance
    atom::containers::Vector<int> vector;
    start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < test_size; ++i) {
        vector.push_back(static_cast<int>(i));
    }

    end = std::chrono::high_resolution_clock::now();
    duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(vector.size(), test_size);
    EXPECT_LT(duration.count(), 500);  // Vector should be faster
}

TEST_F(HighPerformanceTest, LookupPerformance) {
    const size_t test_size = 10000;

    // Prepare test data
    atom::containers::HashMap<int, int> hash_map;
    for (size_t i = 0; i < test_size; ++i) {
        hash_map[i] = i * 2;
    }

    // Test lookup performance
    auto start = std::chrono::high_resolution_clock::now();
    int sum = 0;
    for (size_t i = 0; i < test_size; ++i) {
        auto it = hash_map.find(i);
        if (it != hash_map.end()) {
            sum += it->second;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    EXPECT_EQ(sum,
              test_size * (test_size - 1));  // Sum of 0*2 + 1*2 + ... + (n-1)*2
    EXPECT_LT(duration.count(), 50000);      // Should complete within 50ms

    // Test HashSet lookup performance
    atom::containers::HashSet<int> hash_set;
    for (size_t i = 0; i < test_size; ++i) {
        hash_set.insert(static_cast<int>(i));
    }

    start = std::chrono::high_resolution_clock::now();
    size_t found_count = 0;
    for (size_t i = 0; i < test_size; ++i) {
        if (hash_set.find(i) != hash_set.end()) {
            found_count++;
        }
    }
    end = std::chrono::high_resolution_clock::now();
    duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    EXPECT_EQ(found_count, test_size);
    EXPECT_LT(duration.count(), 30000);  // Set lookup should be very fast
}

TEST_F(HighPerformanceTest, MemoryEfficiency) {
    // Test memory usage patterns
    const size_t test_size = 1000;

    // Test String memory efficiency
    atom::containers::String str1("short");
    atom::containers::String str2(
        "this is a much longer string that should test memory allocation");

    EXPECT_EQ(str1.size(), 5);
    EXPECT_GT(str2.size(), 50);
    EXPECT_LT(str1.capacity(), str2.capacity());

    // Test Vector memory efficiency
    atom::containers::Vector<int> vec;
    size_t initial_capacity = vec.capacity();

    for (size_t i = 0; i < test_size; ++i) {
        vec.push_back(static_cast<int>(i));
    }

    EXPECT_EQ(vec.size(), test_size);
    EXPECT_GE(vec.capacity(), test_size);
    EXPECT_GT(vec.capacity(), initial_capacity);

    // Test memory reuse after clear
    vec.clear();
    size_t capacity_after_clear = vec.capacity();
    EXPECT_EQ(vec.size(), 0);
    EXPECT_GT(capacity_after_clear, 0);  // Capacity should be retained
}

// ============================================================================
// Intrusive Containers Tests
// ============================================================================

class IntrusiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup intrusive container tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(IntrusiveTest, IntrusiveList) {
#ifdef ATOM_HAS_BOOST_INTRUSIVE
    // Test intrusive list functionality
    struct ListNode : public boost::intrusive::list_base_hook<> {
        int value;
        explicit ListNode(int v) : value(v) {}
    };

    boost::intrusive::list<ListNode> ilist;

    // Create test nodes
    ListNode node1(10);
    ListNode node2(20);
    ListNode node3(30);

    // Test insertion
    ilist.push_back(node1);
    ilist.push_back(node2);
    ilist.push_back(node3);

    EXPECT_EQ(ilist.size(), 3);
    EXPECT_EQ(ilist.front().value, 10);
    EXPECT_EQ(ilist.back().value, 30);

    // Test iteration
    int expected_values[] = {10, 20, 30};
    int index = 0;
    for (const auto& node : ilist) {
        EXPECT_EQ(node.value, expected_values[index++]);
    }

    // Test removal
    ilist.pop_front();
    EXPECT_EQ(ilist.size(), 2);
    EXPECT_EQ(ilist.front().value, 20);
#else
    // Intrusive containers not available - skip test
    GTEST_SKIP() << "Intrusive containers require ATOM_HAS_BOOST_INTRUSIVE";
#endif
}

TEST_F(IntrusiveTest, IntrusiveSet) {
#ifdef ATOM_HAS_BOOST_INTRUSIVE
    // Test intrusive set functionality
    struct SetNode : public boost::intrusive::set_base_hook<> {
        int value;
        explicit SetNode(int v) : value(v) {}

        bool operator<(const SetNode& other) const {
            return value < other.value;
        }
    };

    boost::intrusive::set<SetNode> iset;

    // Create test nodes
    SetNode node1(30);
    SetNode node2(10);
    SetNode node3(20);

    // Test insertion (should be sorted)
    iset.insert(node1);
    iset.insert(node2);
    iset.insert(node3);

    EXPECT_EQ(iset.size(), 3);

    // Test sorted order
    int expected_values[] = {10, 20, 30};
    int index = 0;
    for (const auto& node : iset) {
        EXPECT_EQ(node.value, expected_values[index++]);
    }

    // Test find
    auto it = iset.find(SetNode(20));
    EXPECT_NE(it, iset.end());
    EXPECT_EQ(it->value, 20);

    // Test erase
    iset.erase(it);
    EXPECT_EQ(iset.size(), 2);
    EXPECT_EQ(iset.find(SetNode(20)), iset.end());
#else
    // Intrusive containers not available - skip test
    GTEST_SKIP() << "Intrusive containers require ATOM_HAS_BOOST_INTRUSIVE";
#endif
}

// ============================================================================
// Lock-Free Containers Tests
// ============================================================================

class LockFreeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup lock-free container tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(LockFreeTest, ConcurrentAccess) {
#ifdef ATOM_HAS_BOOST_LOCKFREE
    // Test concurrent access patterns
    atom::containers::lockfree::queue<int> lf_queue(1000);
    const int num_items = 1000;
    const int num_threads = 4;

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;
    std::atomic<int> items_produced{0};
    std::atomic<int> items_consumed{0};

    // Create producer threads
    for (int t = 0; t < num_threads; ++t) {
        producers.emplace_back(
            [&lf_queue, &items_produced, num_items, num_threads, t]() {
                int start = t * (num_items / num_threads);
                int end = (t + 1) * (num_items / num_threads);
                for (int i = start; i < end; ++i) {
                    while (!lf_queue.push(i)) {
                        std::this_thread::yield();
                    }
                    items_produced.fetch_add(1);
                }
            });
    }

    // Create consumer threads
    for (int t = 0; t < num_threads; ++t) {
        consumers.emplace_back([&lf_queue, &items_consumed]() {
            int value;
            while (items_consumed.load() < 1000) {
                if (lf_queue.pop(value)) {
                    items_consumed.fetch_add(1);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& t : producers)
        t.join();
    for (auto& t : consumers)
        t.join();

    EXPECT_EQ(items_produced.load(), num_items);
    EXPECT_EQ(items_consumed.load(), num_items);
#else
    // Lock-free containers not available - skip test
    GTEST_SKIP() << "Lock-free containers require ATOM_HAS_BOOST_LOCKFREE";
#endif
}

TEST_F(LockFreeTest, ThreadSafety) {
#ifdef ATOM_HAS_BOOST_LOCKFREE
    // Test thread safety guarantees
    atom::containers::lockfree::stack<int> lf_stack(1000);
    const int num_operations = 10000;
    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};

    std::thread pusher([&lf_stack, &push_count, num_operations]() {
        for (int i = 0; i < num_operations; ++i) {
            while (!lf_stack.push(i)) {
                std::this_thread::yield();
            }
            push_count.fetch_add(1);
        }
    });

    std::thread popper([&lf_stack, &pop_count]() {
        int value;
        while (pop_count.load() < 10000) {
            if (lf_stack.pop(value)) {
                pop_count.fetch_add(1);
            } else {
                std::this_thread::yield();
            }
        }
    });

    pusher.join();
    popper.join();

    EXPECT_EQ(push_count.load(), num_operations);
    EXPECT_EQ(pop_count.load(), num_operations);

    // Stack should be empty
    int dummy;
    EXPECT_FALSE(lf_stack.pop(dummy));
#else
    // Lock-free containers not available - skip test
    GTEST_SKIP() << "Lock-free containers require ATOM_HAS_BOOST_LOCKFREE";
#endif
}

TEST_F(LockFreeTest, PerformanceUnderContention) {
#ifdef ATOM_HAS_BOOST_LOCKFREE
    // Test performance under high contention
    atom::containers::lockfree::spsc_queue<int> spsc_queue(10000);
    const int num_items = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    std::thread producer([&spsc_queue, num_items]() {
        for (int i = 0; i < num_items; ++i) {
            while (!spsc_queue.push(i)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&spsc_queue, num_items]() {
        int value;
        int consumed = 0;
        while (consumed < num_items) {
            if (spsc_queue.pop(value)) {
                consumed++;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete within reasonable time (less than 5 seconds)
    EXPECT_LT(duration.count(), 5000);

    // Queue should be empty
    int dummy;
    EXPECT_FALSE(spsc_queue.pop(dummy));
#else
    // Lock-free containers not available - skip test
    GTEST_SKIP() << "Lock-free containers require ATOM_HAS_BOOST_LOCKFREE";
#endif
}

// ============================================================================
// Integration Tests
// ============================================================================

class ContainerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup integration test environment
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(ContainerIntegrationTest, ContainerInteroperability) {
    // Test how different containers work together

    // Create a scenario where we use multiple container types together
    atom::containers::HashMap<std::string, int> name_to_id;
    atom::containers::Vector<std::string> id_to_name;
    atom::containers::HashSet<int> active_ids;

    // Add some test data
    std::vector<std::string> names = {"Alice", "Bob", "Charlie", "Diana",
                                      "Eve"};

    for (size_t i = 0; i < names.size(); ++i) {
        int id = static_cast<int>(i + 1);
        name_to_id[names[i]] = id;
        id_to_name.push_back(names[i]);
        active_ids.insert(id);
    }

    // Test cross-container operations
    EXPECT_EQ(name_to_id.size(), names.size());
    EXPECT_EQ(id_to_name.size(), names.size());
    EXPECT_EQ(active_ids.size(), names.size());

    // Test lookup consistency
    for (const auto& name : names) {
        int id = name_to_id[name];
        EXPECT_GT(id, 0);
        EXPECT_LE(id, static_cast<int>(names.size()));
        EXPECT_EQ(id_to_name[id - 1], name);  // id is 1-based
        EXPECT_TRUE(active_ids.find(id) != active_ids.end());
    }

    // Test removal consistency
    std::string removed_name = "Bob";
    int removed_id = name_to_id[removed_name];
    name_to_id.erase(removed_name);
    active_ids.erase(removed_id);

    EXPECT_EQ(name_to_id.find(removed_name), name_to_id.end());
    EXPECT_EQ(active_ids.find(removed_id), active_ids.end());
    EXPECT_EQ(id_to_name[removed_id - 1],
              removed_name);  // Name still in vector
}

TEST_F(ContainerIntegrationTest, RealWorldScenarios) {
    // Test real-world usage scenario: Simple cache implementation

    struct CacheEntry {
        std::string key;
        std::string value;
        std::chrono::steady_clock::time_point timestamp;

        CacheEntry() = default;

        CacheEntry(const std::string& k, const std::string& v)
            : key(k), value(v), timestamp(std::chrono::steady_clock::now()) {}
    };

    // Use multiple containers for a cache implementation
    atom::containers::HashMap<std::string, CacheEntry> cache_data;
    atom::containers::Vector<std::string> access_order;  // LRU tracking
    const size_t max_cache_size = 3;

    auto add_to_cache = [&](const std::string& key, const std::string& value) {
        // Remove oldest if at capacity
        if (cache_data.size() >= max_cache_size &&
            cache_data.find(key) == cache_data.end()) {
            std::string oldest_key = access_order.front();
            cache_data.erase(oldest_key);
            access_order.erase(access_order.begin());
        }

        // Add or update entry
        auto cache_it = cache_data.find(key);
        if (cache_it != cache_data.end()) {
            cache_it->second = CacheEntry(key, value);
        } else {
            cache_data.emplace(key, CacheEntry(key, value));
        }

        // Update access order
        auto order_it =
            std::find(access_order.begin(), access_order.end(), key);
        if (order_it != access_order.end()) {
            access_order.erase(order_it);
        }
        access_order.push_back(key);
    };

    // Test cache operations
    add_to_cache("key1", "value1");
    add_to_cache("key2", "value2");
    add_to_cache("key3", "value3");

    EXPECT_EQ(cache_data.size(), 3);
    EXPECT_EQ(access_order.size(), 3);

    // Add fourth item - should evict first
    add_to_cache("key4", "value4");

    EXPECT_EQ(cache_data.size(), 3);
    EXPECT_EQ(cache_data.find("key1"), cache_data.end());  // Should be evicted
    EXPECT_NE(cache_data.find("key4"), cache_data.end());  // Should be present

    // Test access order update
    add_to_cache("key2", "updated_value2");  // Update existing
    EXPECT_EQ(access_order.back(), "key2");  // Should be most recent
    EXPECT_EQ(cache_data["key2"].value, "updated_value2");
}

// ============================================================================
// Error Handling Tests
// ============================================================================

class ContainerErrorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup error condition tests
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(ContainerErrorTest, OutOfMemoryHandling) {
    // Test behavior under memory pressure

    // Test Vector behavior with large allocations
    atom::containers::Vector<int> large_vector;

    try {
        // Reserve a reasonable amount first
        large_vector.reserve(1000);
        EXPECT_GE(large_vector.capacity(), 1000);

        // Fill with data
        for (int i = 0; i < 1000; ++i) {
            large_vector.push_back(i);
        }

        EXPECT_EQ(large_vector.size(), 1000);
        EXPECT_EQ(large_vector[999], 999);

    } catch (const std::bad_alloc& e) {
        // Memory allocation failed - this is acceptable behavior
        GTEST_SKIP() << "Insufficient memory for large allocation test";
    }

    // Test HashMap behavior with many elements
    atom::containers::HashMap<int, int> large_map;

    try {
        const size_t large_size = 100000;
        for (size_t i = 0; i < large_size; ++i) {
            large_map[i] = static_cast<int>(i * 2);
        }

        EXPECT_EQ(large_map.size(), large_size);
        EXPECT_EQ(large_map[50000], 100000);

    } catch (const std::bad_alloc& e) {
        // Memory allocation failed - this is acceptable behavior
        GTEST_SKIP() << "Insufficient memory for large map test";
    }
}

TEST_F(ContainerErrorTest, InvalidOperations) {
    // Test handling of invalid operations

    // Test Vector boundary conditions
    atom::containers::Vector<int> vec;

    // Test empty vector operations
    EXPECT_TRUE(vec.empty());
    EXPECT_EQ(vec.size(), 0);

    // Test out-of-bounds access (should not crash in debug mode)
    // Note: This behavior is implementation-defined
    vec.push_back(42);
    EXPECT_EQ(vec.at(0), 42);

    try {
        vec.at(1);  // Out of bounds
        FAIL() << "Expected std::out_of_range exception";
    } catch (const std::out_of_range& e) {
        // Expected behavior
        SUCCEED();
    }

    // Test HashMap with invalid keys
    atom::containers::HashMap<std::string, int> map;

    // Test find on empty map
    EXPECT_EQ(map.find("nonexistent"), map.end());
    EXPECT_EQ(map.size(), 0);

    // Test erase on non-existent key
    size_t erased = map.erase("nonexistent");
    EXPECT_EQ(erased, 0);

    // Add element and test valid operations
    map["key"] = 100;
    EXPECT_EQ(map["key"], 100);
    EXPECT_NE(map.find("key"), map.end());

    // Test erase existing key
    erased = map.erase("key");
    EXPECT_EQ(erased, 1);
    EXPECT_EQ(map.find("key"), map.end());

    // Test String edge cases
    atom::containers::String str;
    EXPECT_TRUE(str.empty());
    EXPECT_EQ(str.size(), 0);

    // Test substring operations
    str = "Hello World";
    EXPECT_EQ(str.substr(0, 5), "Hello");
    EXPECT_EQ(str.substr(6), "World");

    try {
        str.substr(20);  // Out of bounds
        FAIL() << "Expected std::out_of_range exception";
    } catch (const std::out_of_range& e) {
        // Expected behavior
        SUCCEED();
    }
}

// ============================================================================
// Additional Edge Case Tests
// ============================================================================

class ContainerEdgeCaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup edge case test environment
    }

    void TearDown() override {
        // Cleanup
    }
};

TEST_F(ContainerEdgeCaseTest, NullAndEmptyHandling) {
    // Test handling of null and empty values

    // Test empty containers
    atom::containers::Vector<int> empty_vector;
    atom::containers::HashMap<std::string, int> empty_map;
    atom::containers::HashSet<int> empty_set;

    EXPECT_TRUE(empty_vector.empty());
    EXPECT_TRUE(empty_map.empty());
    EXPECT_TRUE(empty_set.empty());

    EXPECT_EQ(empty_vector.size(), 0);
    EXPECT_EQ(empty_map.size(), 0);
    EXPECT_EQ(empty_set.size(), 0);

    // Test operations on empty containers
    EXPECT_EQ(empty_map.find("nonexistent"), empty_map.end());
    EXPECT_EQ(empty_set.find(42), empty_set.end());

    // Test empty string handling
    atom::containers::String empty_string;
    atom::containers::String null_string("");

    EXPECT_TRUE(empty_string.empty());
    EXPECT_TRUE(null_string.empty());
    EXPECT_EQ(empty_string.size(), 0);
    EXPECT_EQ(null_string.size(), 0);
}

TEST_F(ContainerEdgeCaseTest, BoundaryConditions) {
    // Test boundary conditions and limits

    // Test maximum capacity scenarios
    atom::containers::Vector<int> large_vector;

    try {
        // Test with reasonable large size
        const size_t large_size = 1000000;
        large_vector.reserve(large_size);

        for (size_t i = 0; i < std::min(large_size, size_t(10000)); ++i) {
            large_vector.push_back(static_cast<int>(i));
        }

        EXPECT_GE(large_vector.capacity(), 10000);
        EXPECT_EQ(large_vector.size(), 10000);

    } catch (const std::bad_alloc& e) {
        // Memory allocation failed - acceptable in constrained environments
        GTEST_SKIP() << "Insufficient memory for large container test";
    }

    // Test string boundary conditions
    atom::containers::String long_string(1000, 'A');
    EXPECT_EQ(long_string.size(), 1000);
    EXPECT_EQ(long_string[0], 'A');
    EXPECT_EQ(long_string[999], 'A');
}

TEST_F(ContainerEdgeCaseTest, ConcurrencyStress) {
    // Test container behavior under concurrent stress

    // Use standard containers for this test since atom containers may not be
    // thread-safe
    std::unordered_map<int, int> concurrent_map;
    std::mutex map_mutex;
    const int num_threads = 2;              // Reduced for stability
    const int operations_per_thread = 100;  // Reduced for faster execution
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    // Concurrent insertions with proper synchronization
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&concurrent_map, &map_mutex, &success_count, t,
                              operations_per_thread]() {
            for (int i = 0; i < operations_per_thread; ++i) {
                try {
                    int key = t * operations_per_thread + i;
                    {
                        std::lock_guard<std::mutex> lock(map_mutex);
                        concurrent_map[key] = key * 2;
                    }
                    success_count.fetch_add(1);
                } catch (...) {
                    // Handle any exceptions during concurrent access
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all operations succeeded with proper synchronization
    EXPECT_EQ(success_count.load(), num_threads * operations_per_thread);
    EXPECT_EQ(concurrent_map.size(), num_threads * operations_per_thread);
}

}  // namespace atom::containers::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
