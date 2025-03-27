/**
 * @file object_pool_example.cpp
 * @brief Comprehensive examples of using the ObjectPool class
 * @author Example Author
 * @date 2025-03-23
 */

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "atom/memory/object.hpp"

// Helper function to print section titles
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

// A simple example class that satisfies the Resettable concept
class Connection {
private:
    std::string host_;
    int port_;
    bool connected_;
    std::string last_query_;
    int query_count_;
    int connection_id_;
    static std::atomic<int> next_id_;

public:
    Connection()
        : host_("localhost"),
          port_(8080),
          connected_(false),
          query_count_(0),
          connection_id_(next_id_++) {
        std::cout << "Creating Connection #" << connection_id_ << std::endl;
    }

    ~Connection() {
        std::cout << "Destroying Connection #" << connection_id_ << std::endl;
    }

    bool connect(const std::string& host, int port) {
        // Simulate connection establishment
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        host_ = host;
        port_ = port;
        connected_ = true;
        std::cout << "Connection #" << connection_id_ << " established to "
                  << host << ":" << port << std::endl;
        return true;
    }

    bool disconnect() {
        if (connected_) {
            // Simulate disconnection
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            connected_ = false;
            std::cout << "Connection #" << connection_id_
                      << " disconnected from " << host_ << ":" << port_
                      << std::endl;
            return true;
        }
        return false;
    }

    bool executeQuery(const std::string& query) {
        if (!connected_) {
            std::cout << "Error: Connection #" << connection_id_
                      << " not connected" << std::endl;
            return false;
        }
        // Simulate query execution
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        last_query_ = query;
        query_count_++;
        return true;
    }

    // Required by Resettable concept
    void reset() {
        disconnect();
        last_query_.clear();
        query_count_ = 0;
        host_ = "localhost";
        port_ = 8080;
    }

    bool isValid() const {
        // Example validity check - could check actual connection status
        return query_count_ < 100;  // Consider invalid after too many queries
    }

    int getId() const { return connection_id_; }
    bool isConnected() const { return connected_; }
    int getQueryCount() const { return query_count_; }
    std::string getHost() const { return host_; }
    int getPort() const { return port_; }
};

std::atomic<int> Connection::next_id_(1);

// A heavy resource class to demonstrate resource pooling benefits
class HeavyResource {
private:
    std::vector<double> data_;
    bool initialized_;
    int resource_id_;
    static std::atomic<int> next_id_;

public:
    HeavyResource()
        : data_(1000000, 0.0),  // 1M doubles - intentionally large
          initialized_(false),
          resource_id_(next_id_++) {
        std::cout << "Creating HeavyResource #" << resource_id_
                  << " (expensive!)" << std::endl;
    }

    ~HeavyResource() {
        std::cout << "Destroying HeavyResource #" << resource_id_ << std::endl;
    }

    bool initialize() {
        if (!initialized_) {
            // Simulate heavy initialization
            std::cout << "Initializing HeavyResource #" << resource_id_ << "..."
                      << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            // Fill with some values
            std::mt19937 gen(resource_id_);
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            for (auto& val : data_) {
                val = dist(gen);
            }
            initialized_ = true;
            std::cout << "HeavyResource #" << resource_id_ << " initialized"
                      << std::endl;
            return true;
        }
        return false;
    }

    double compute() {
        if (!initialized_) {
            std::cout << "Error: HeavyResource #" << resource_id_
                      << " not initialized" << std::endl;
            return -1.0;
        }

        // Simulate computation
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        double sum = 0.0;
        for (const auto& val : data_) {
            sum += val;
        }
        return sum / data_.size();
    }

    // Required by Resettable concept
    void reset() {
        // Just mark as uninitialized rather than clearing all data
        initialized_ = false;
    }

    bool isValid() const { return initialized_; }

    int getId() const { return resource_id_; }
};

std::atomic<int> HeavyResource::next_id_(1);

// Benchmark utility function
template <typename FuncType>
double measureExecutionTime(FuncType&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    return duration.count();
}

int main() {
    std::cout << "OBJECT POOL COMPREHENSIVE EXAMPLES\n";
    std::cout << "==================================\n";

    //--------------------------------------------------------------------------
    // 1. Basic Usage
    //--------------------------------------------------------------------------
    printSection("1. Basic Usage");

    // Create an object pool with max size 5 and 2 prefilled objects
    atom::memory::ObjectPool<Connection> connectionPool(5, 2);

    std::cout << "Created pool with capacity 5, prefilled with 2 connections\n";
    std::cout << "Available connections: " << connectionPool.available()
              << "\n";
    std::cout << "In-use connections: " << connectionPool.inUseCount() << "\n";

    // Acquire an object from the pool
    {
        std::cout << "\nAcquiring connection from pool..." << std::endl;
        auto conn = connectionPool.acquire();

        // Use the connection
        conn->connect("database.example.com", 5432);
        conn->executeQuery("SELECT * FROM users");

        std::cout << "Connection #" << conn->getId()
                  << " in use, query count: " << conn->getQueryCount()
                  << std::endl;

        // Connection will be automatically returned to the pool when conn goes
        // out of scope
        std::cout << "Releasing connection back to pool..." << std::endl;
    }

    std::cout << "\nAfter release:" << std::endl;
    std::cout << "Available connections: " << connectionPool.available()
              << "\n";

    // Acquire multiple connections
    {
        std::cout << "\nAcquiring 3 connections..." << std::endl;
        auto conn1 = connectionPool.acquire();
        auto conn2 = connectionPool.acquire();
        auto conn3 = connectionPool.acquire();

        conn1->connect("server1.example.com", 8080);
        conn2->connect("server2.example.com", 8080);
        conn3->connect("server3.example.com", 8080);

        std::cout << "All 3 connections acquired and connected" << std::endl;
        std::cout << "Available connections: " << connectionPool.available()
                  << "\n";
        std::cout << "In-use connections: " << connectionPool.inUseCount()
                  << "\n";

        // Connections automatically returned to pool
    }

    //--------------------------------------------------------------------------
    // 2. Timeouts and Priority
    //--------------------------------------------------------------------------
    printSection("2. Timeouts and Priority");

    // Create a small pool with just 2 slots
    atom::memory::ObjectPool<Connection> smallPool(2, 0);

    // Acquire both available connections
    auto highPriorityConn =
        smallPool.acquire(atom::memory::ObjectPool<Connection>::Priority::High);
    auto normalPriorityConn = smallPool.acquire(
        atom::memory::ObjectPool<Connection>::Priority::Normal);

    std::cout << "Acquired all available connections from small pool"
              << std::endl;
    std::cout << "Available connections: " << smallPool.available() << "\n";

    // Demonstrate timeout
    std::cout << "\nTrying to acquire with timeout of 500ms..." << std::endl;
    auto startTime = std::chrono::steady_clock::now();
    auto optionalConn = smallPool.tryAcquireFor(std::chrono::milliseconds(500));
    auto endTime = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         endTime - startTime)
                         .count();

    std::cout << "Acquisition attempt completed after " << elapsedMs << "ms"
              << std::endl;
    if (optionalConn) {
        std::cout << "Unexpectedly acquired a connection!" << std::endl;
    } else {
        std::cout << "Timeout occurred as expected" << std::endl;
    }

    // Create background thread that will release a connection after delay
    std::thread releaseThread([&]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout
            << "\nBackground thread releasing normal priority connection..."
            << std::endl;
        normalPriorityConn.reset();  // Release the connection
    });

    // Try to acquire with different priorities
    std::cout << "\nMain thread: trying to acquire with critical priority..."
              << std::endl;
    auto criticalConn = smallPool.acquire(
        atom::memory::ObjectPool<Connection>::Priority::Critical);
    std::cout << "Acquired connection with critical priority!" << std::endl;

    // Release connections
    highPriorityConn.reset();
    criticalConn.reset();

    releaseThread.join();

    //--------------------------------------------------------------------------
    // 3. Validation
    //--------------------------------------------------------------------------
    printSection("3. Validation");

    // Create pool with validation
    atom::memory::ObjectPool<Connection>::PoolConfig validateConfig;
    validateConfig.validate_on_acquire = true;
    validateConfig.validate_on_release = true;
    validateConfig.validator = [](const Connection& conn) {
        return conn.getQueryCount() <
               5;  // Connection valid if fewer than 5 queries
    };

    atom::memory::ObjectPool<Connection> validatingPool(5, 2, nullptr,
                                                        validateConfig);

    // Use connections multiple times to trigger validation
    {
        std::cout << "Acquiring and using connections multiple times..."
                  << std::endl;

        for (int i = 0; i < 3; ++i) {
            auto conn = validatingPool.acquire();
            conn->connect("validation-test.example.com", 9000);

            // Execute enough queries to invalidate the connection
            for (int j = 0; j < 3; ++j) {
                conn->executeQuery("Query #" + std::to_string(j));
            }

            std::cout << "Connection #" << conn->getId() << " used for "
                      << conn->getQueryCount() << " queries" << std::endl;
            // Connection returned to pool, validation occurs
        }

        std::cout << "\nAcquiring validated connection..." << std::endl;
        // Use custom validator to get a connection with specific properties
        auto validatedConn =
            validatingPool.acquireValidated([](const Connection& conn) {
                // Only consider connections with even IDs valid
                return conn.getId() % 2 == 0;
            });

        std::cout << "Acquired connection #" << validatedConn->getId()
                  << " (passes custom validation)" << std::endl;
    }

    //--------------------------------------------------------------------------
    // 4. Batch Operations
    //--------------------------------------------------------------------------
    printSection("4. Batch Operations");

    // Create pool for batch operations
    atom::memory::ObjectPool<Connection> batchPool(10, 5);

    // Acquire a batch of connections
    std::cout << "Acquiring batch of 4 connections..." << std::endl;
    auto connectionBatch = batchPool.acquireBatch(4);

    std::cout << "Acquired " << connectionBatch.size()
              << " connections in batch" << std::endl;
    std::cout << "Available connections: " << batchPool.available() << "\n";

    // Use connections in batch
    for (size_t i = 0; i < connectionBatch.size(); ++i) {
        auto& conn = connectionBatch[i];
        conn->connect("batch-server-" + std::to_string(i) + ".example.com",
                      8080);
        conn->executeQuery("Batch query from connection " + std::to_string(i));
    }

    std::cout << "\nReleasing connections one by one..." << std::endl;

    // Release one connection
    connectionBatch[0].reset();
    std::cout << "After releasing one: available = " << batchPool.available()
              << std::endl;

    // Release the rest
    connectionBatch.clear();
    std::cout << "After releasing all: available = " << batchPool.available()
              << std::endl;

    //--------------------------------------------------------------------------
    // 5. Auto-Cleanup and Pool Management
    //--------------------------------------------------------------------------
    printSection("5. Auto-Cleanup and Pool Management");

    // Create pool with fast auto-cleanup
    atom::memory::ObjectPool<Connection>::PoolConfig cleanupConfig;
    cleanupConfig.enable_auto_cleanup = true;
    cleanupConfig.cleanup_interval =
        std::chrono::minutes(1);  // Cleanup every minute
    cleanupConfig.max_idle_time =
        std::chrono::minutes(2);  // Remove after 2 minutes idle

    atom::memory::ObjectPool<Connection> cleanupPool(10, 3, nullptr,
                                                     cleanupConfig);

    std::cout << "Created pool with auto-cleanup, prefilled with 3 connections"
              << std::endl;
    std::cout << "Available connections: " << cleanupPool.available() << "\n";

    // Use some connections
    {
        auto conn1 = cleanupPool.acquire();
        auto conn2 = cleanupPool.acquire();

        conn1->connect("cleanup-test1.example.com", 8080);
        conn2->connect("cleanup-test2.example.com", 8080);

        std::cout << "Used 2 connections from pool" << std::endl;
        // Connections returned to pool automatically
    }

    // Force a cleanup
    std::cout << "\nForcing manual cleanup..." << std::endl;
    size_t cleaned = cleanupPool.runCleanup(true);
    std::cout << "Cleaned " << cleaned << " connections" << std::endl;

    // Resize the pool
    std::cout << "\nResizing pool from 10 to 15 slots..." << std::endl;
    cleanupPool.resize(15);
    std::cout << "New pool size: " << cleanupPool.size() << std::endl;
    std::cout << "Available connections: " << cleanupPool.available() << "\n";

    // Clear the pool
    std::cout << "\nClearing pool..." << std::endl;
    cleanupPool.clear();
    std::cout << "Available connections after clear: "
              << cleanupPool.available() << "\n";

    //--------------------------------------------------------------------------
    // 6. Statistics and Monitoring
    //--------------------------------------------------------------------------
    printSection("6. Statistics and Monitoring");

    // Create pool with statistics enabled
    atom::memory::ObjectPool<Connection>::PoolConfig statsConfig;
    statsConfig.enable_stats = true;

    atom::memory::ObjectPool<Connection> statsPool(5, 2, nullptr, statsConfig);

    std::cout << "Created pool with statistics tracking" << std::endl;

    // Perform various operations to generate statistics
    for (int i = 0; i < 10; ++i) {
        auto conn = statsPool.acquire();
        conn->connect("stats-test.example.com", 8080);
        conn->executeQuery("SELECT * FROM table_" + std::to_string(i));
        // Connection returned to pool
    }

    // Try to acquire with timeout to generate timeout stats
    for (int i = 0; i < 3; ++i) {
        // Acquire all connections
        std::vector<std::shared_ptr<Connection>> allConns;
        for (int j = 0; j < 5; ++j) {
            allConns.push_back(statsPool.acquire());
        }

        // Try to acquire one more (will timeout)
        auto optConn = statsPool.tryAcquireFor(std::chrono::milliseconds(50));

        // Release all connections
        allConns.clear();
    }

    // Get and display statistics
    auto stats = statsPool.getStats();

    std::cout << "\nPool Statistics:" << std::endl;
    std::cout << "Hits (reused objects): " << stats.hits << std::endl;
    std::cout << "Misses (created objects): " << stats.misses << std::endl;
    std::cout << "Peak usage: " << stats.peak_usage << std::endl;
    std::cout << "Cleanup count: " << stats.cleanups << std::endl;
    std::cout << "Wait count: " << stats.wait_count << std::endl;
    std::cout << "Timeout count: " << stats.timeout_count << std::endl;

    double avgWaitMs =
        stats.wait_count > 0
            ? std::chrono::duration_cast<std::chrono::milliseconds>(
                  stats.total_wait_time)
                      .count() /
                  static_cast<double>(stats.wait_count)
            : 0.0;

    double maxWaitMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           stats.max_wait_time)
                           .count();

    std::cout << "Average wait time: " << avgWaitMs << " ms" << std::endl;
    std::cout << "Maximum wait time: " << maxWaitMs << " ms" << std::endl;

    // Reset statistics
    std::cout << "\nResetting statistics..." << std::endl;
    statsPool.resetStats();
    auto resetStats = statsPool.getStats();
    std::cout << "Hits after reset: " << resetStats.hits << std::endl;

    //--------------------------------------------------------------------------
    // 7. Performance Comparison
    //--------------------------------------------------------------------------
    printSection("7. Performance Comparison");

    constexpr int NUM_ITERATIONS = 1000;

    // Measure performance with object pool
    std::cout << "Testing performance with object pool..." << std::endl;
    atom::memory::ObjectPool<HeavyResource> resourcePool(10, 5);

    double poolTime = measureExecutionTime([&]() {
        for (int i = 0; i < NUM_ITERATIONS; ++i) {
            auto resource = resourcePool.acquire();
            if (!resource->isValid()) {
                resource->initialize();
            }
            double result = resource->compute();
            if (i % 100 == 0) {
                std::cout << "Iteration " << i << ", result: " << result
                          << std::endl;
            }
            // Resource returned to pool automatically
        }
    });

    // Measure performance without object pool
    std::cout << "\nTesting performance without object pool..." << std::endl;
    double noPoolTime = measureExecutionTime([&]() {
        for (int i = 0; i < NUM_ITERATIONS; ++i) {
            auto resource = std::make_shared<HeavyResource>();
            resource->initialize();
            double result = resource->compute();
            if (i % 100 == 0) {
                std::cout << "Iteration " << i << ", result: " << result
                          << std::endl;
            }
            // Resource destroyed
        }
    });

    // Report performance comparison
    std::cout << "\nPerformance Comparison:" << std::endl;
    std::cout << "With object pool:    " << std::fixed << std::setprecision(2)
              << poolTime << " ms" << std::endl;
    std::cout << "Without object pool: " << std::fixed << std::setprecision(2)
              << noPoolTime << " ms" << std::endl;
    std::cout << "Speedup factor:      " << std::fixed << std::setprecision(2)
              << (noPoolTime / poolTime) << "x" << std::endl;

    //--------------------------------------------------------------------------
    // 8. Concurrency Testing
    //--------------------------------------------------------------------------
    printSection("8. Concurrency Testing");

    // Create shared pool for concurrent testing
    atom::memory::ObjectPool<Connection> concurrentPool(10, 5);

    std::cout << "Running concurrent test with 5 threads..." << std::endl;

    std::atomic<int> total_queries{0};
    std::vector<std::thread> threads;

    auto threadFunc = [&](int thread_id) {
        const int OPS_PER_THREAD = 50;
        std::mt19937 rng(thread_id);  // Deterministic random for this thread
        std::uniform_int_distribution<> wait_dist(5, 30);

        for (int i = 0; i < OPS_PER_THREAD; ++i) {
            auto priority =
                i % 3 == 0
                    ? atom::memory::ObjectPool<Connection>::Priority::High
                    : atom::memory::ObjectPool<Connection>::Priority::Normal;

            // Sometimes try with timeout
            std::shared_ptr<Connection> conn;
            if (i % 5 == 0) {
                auto optConn = concurrentPool.tryAcquireFor(
                    std::chrono::milliseconds(100), priority);
                if (optConn) {
                    conn = *optConn;
                } else {
                    continue;  // Skip this iteration if timeout
                }
            } else {
                conn = concurrentPool.acquire(priority);
            }

            // Use the connection
            conn->connect(
                "thread-" + std::to_string(thread_id) + ".example.com", 8080);
            conn->executeQuery("Thread " + std::to_string(thread_id) +
                               " query " + std::to_string(i));

            total_queries.fetch_add(1);

            // Simulate varying processing time
            std::this_thread::sleep_for(
                std::chrono::milliseconds(wait_dist(rng)));

            // Connection automatically returned to pool
        }
    };

    // Start threads
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back(threadFunc, i);
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Concurrent test completed" << std::endl;
    std::cout << "Total queries executed: " << total_queries << std::endl;

    // Check pool state after concurrent usage
    std::cout << "Available connections: " << concurrentPool.available()
              << "\n";

    // Get statistics from concurrent usage
    auto concurrentStats = concurrentPool.getStats();
    std::cout << "\nPool Statistics After Concurrent Usage:" << std::endl;
    std::cout << "Hits: " << concurrentStats.hits << std::endl;
    std::cout << "Misses: " << concurrentStats.misses << std::endl;
    std::cout << "Peak usage: " << concurrentStats.peak_usage << std::endl;
    std::cout << "Wait count: " << concurrentStats.wait_count << std::endl;

    //--------------------------------------------------------------------------
    // 9. Custom Object Creation
    //--------------------------------------------------------------------------
    printSection("9. Custom Object Creation");

    // Define custom creation function
    auto customCreator = []() {
        std::cout << "Custom creator called" << std::endl;
        auto conn = std::make_shared<Connection>();
        // Pre-configure the connection
        conn->connect("custom-default.example.com", 9090);
        return conn;
    };

    // Create pool with custom creator
    atom::memory::ObjectPool<Connection> customPool(5, 2, customCreator);

    std::cout << "Created pool with custom object creator function"
              << std::endl;

    // Acquire an object and verify it's pre-configured
    auto customConn = customPool.acquire();
    std::cout << "Acquired connection #" << customConn->getId() << std::endl;
    std::cout << "Connection is connected: "
              << (customConn->isConnected() ? "yes" : "no") << std::endl;
    std::cout << "Connection host: " << customConn->getHost() << std::endl;
    std::cout << "Connection port: " << customConn->getPort() << std::endl;

    // Release connection
    customConn.reset();

    //--------------------------------------------------------------------------
    // 10. Apply Actions to All Objects
    //--------------------------------------------------------------------------
    printSection("10. Apply Actions to All Objects");

    // Create pool and prefill
    atom::memory::ObjectPool<Connection> batchActionPool(5, 5);

    // Apply action to all objects in the pool
    std::cout << "Applying action to all connections in pool..." << std::endl;
    batchActionPool.applyToAll([](Connection& conn) {
        // Pre-connect all connections to a specific server
        conn.connect("batch-action.example.com", 8888);
        std::cout << "Connection #" << conn.getId() << " prepared" << std::endl;
    });

    // Acquire a connection and verify it's already prepared
    auto preparedConn = batchActionPool.acquire();
    std::cout << "\nAcquired prepared connection #" << preparedConn->getId()
              << std::endl;
    std::cout << "Is connected: "
              << (preparedConn->isConnected() ? "yes" : "no") << std::endl;
    std::cout << "Connected to: " << preparedConn->getHost() << ":"
              << preparedConn->getPort() << std::endl;

    // Clean up
    preparedConn.reset();

    std::cout << "\nAll ObjectPool examples completed successfully!\n";
    return 0;
}