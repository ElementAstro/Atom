/**
 * @file snowflake.cpp
 * @brief Comprehensive example demonstrating Snowflake ID generation
 *
 * This example shows how to:
 * - Generate unique distributed IDs using Snowflake algorithm
 * - Parse Snowflake IDs to extract components
 * - Handle multiple workers and datacenters
 * - Demonstrate ID uniqueness and ordering properties
 * - Show performance characteristics and throughput
 * - Handle edge cases and error conditions
 *
 * @author Atom Framework
 * @date 2024-12-19
 */

#include "atom/algorithm/snowflake.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <set>
#include <thread>
#include <vector>

using namespace atom::algorithm;

/**
 * @brief Helper function to print section headers
 */
void printHeader(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

/**
 * @brief Demonstrates basic Snowflake ID generation and parsing
 */
void demonstrateBasicSnowflakeGeneration() {
    printHeader("Basic Snowflake ID Generation");

    try {
        // Define custom epoch (January 1, 2021)
        constexpr uint64_t customEpoch = 1609459200000;

        std::cout << "Custom epoch: " << customEpoch << " (January 1, 2021)\n";
        std::cout << "Worker ID: 1, Datacenter ID: 1\n\n";

        // Create Snowflake generator
        Snowflake<customEpoch> snowflake(1, 1);

        // Generate and analyze several IDs
        std::cout << "Generating and parsing Snowflake IDs:\n";
        for (int i = 0; i < 5; ++i) {
            uint64_t id = snowflake.nextid()[0];

            // Parse the ID components
            uint64_t timestamp, datacenterId, workerId, sequence;
            snowflake.parseId(id, timestamp, datacenterId, workerId, sequence);

            std::cout << "ID " << (i + 1) << ": " << id << "\n";
            std::cout << "  Timestamp: " << timestamp << " (+"
                      << (timestamp - customEpoch) << "ms from epoch)\n";
            std::cout << "  Datacenter ID: " << datacenterId << "\n";
            std::cout << "  Worker ID: " << workerId << "\n";
            std::cout << "  Sequence: " << sequence << "\n\n";

            // Small delay to show timestamp progression
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Demonstrate reset functionality
        std::cout << "Resetting generator...\n";
        snowflake.reset();

        uint64_t resetId = snowflake.nextid()[0];
        std::cout << "ID after reset: " << resetId << "\n";

        // Show current configuration
        std::cout << "\nCurrent configuration:\n";
        std::cout << "  Worker ID: " << snowflake.getWorkerId() << "\n";
        std::cout << "  Datacenter ID: " << snowflake.getDatacenterId() << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in basic Snowflake demonstration: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Demonstrates ID uniqueness and ordering properties
 */
void demonstrateUniquenessAndOrdering() {
    printHeader("ID Uniqueness and Ordering Properties");

    try {
        constexpr uint64_t customEpoch = 1609459200000;
        Snowflake<customEpoch> snowflake(5, 3);

        std::cout << "Testing ID uniqueness and ordering (Worker 5, Datacenter "
                     "3):\n\n";

        // Generate a batch of IDs quickly
        const int batchSize = 1000;
        std::vector<uint64_t> ids;
        ids.reserve(batchSize);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < batchSize; ++i) {
            ids.push_back(snowflake.nextid()[0]);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Generated " << batchSize << " IDs in " << duration.count()
                  << " μs\n";
        std::cout << "Rate: " << std::fixed << std::setprecision(2)
                  << (static_cast<double>(batchSize) / duration.count() *
                      1000000)
                  << " IDs/second\n\n";

        // Check uniqueness
        std::set<uint64_t> uniqueIds(ids.begin(), ids.end());
        std::cout << "Uniqueness test:\n";
        std::cout << "  Generated: " << ids.size() << " IDs\n";
        std::cout << "  Unique: " << uniqueIds.size() << " IDs\n";
        std::cout << "  Result: "
                  << (ids.size() == uniqueIds.size() ? "✓ All unique"
                                                     : "✗ Duplicates found")
                  << "\n\n";

        // Check ordering (IDs should be monotonically increasing)
        bool isOrdered = true;
        for (size_t i = 1; i < ids.size(); ++i) {
            if (ids[i] <= ids[i - 1]) {
                isOrdered = false;
                break;
            }
        }
        std::cout << "Ordering test:\n";
        std::cout << "  Result: "
                  << (isOrdered ? "✓ Monotonically increasing"
                                : "✗ Not ordered")
                  << "\n";

        // Show first and last few IDs
        std::cout << "\nFirst 5 IDs:\n";
        for (int i = 0; i < 5; ++i) {
            std::cout << "  " << (i + 1) << ": " << ids[i] << "\n";
        }

        std::cout << "\nLast 5 IDs:\n";
        for (int i = batchSize - 5; i < batchSize; ++i) {
            std::cout << "  " << (i + 1) << ": " << ids[i] << "\n";
        }

        // Analyze sequence numbers
        uint64_t minSeq = UINT64_MAX, maxSeq = 0;
        for (const auto& id : ids) {
            uint64_t timestamp, datacenterId, workerId, sequence;
            snowflake.parseId(id, timestamp, datacenterId, workerId, sequence);
            minSeq = std::min(minSeq, sequence);
            maxSeq = std::max(maxSeq, sequence);
        }

        std::cout << "\nSequence number analysis:\n";
        std::cout << "  Minimum sequence: " << minSeq << "\n";
        std::cout << "  Maximum sequence: " << maxSeq << "\n";
        std::cout << "  Range: " << (maxSeq - minSeq + 1) << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in uniqueness and ordering demonstration: "
                  << e.what() << "\n";
    }
}

/**
 * @brief Demonstrates multiple workers and datacenters
 */
void demonstrateMultipleWorkersAndDatacenters() {
    printHeader("Multiple Workers and Datacenters");

    try {
        constexpr uint64_t customEpoch = 1609459200000;

        std::cout << "Testing multiple Snowflake generators:\n\n";

        // Create multiple generators
        std::vector<std::unique_ptr<Snowflake<customEpoch>>> generators;
        generators.push_back(std::make_unique<Snowflake<customEpoch>>(1, 1));
        generators.push_back(std::make_unique<Snowflake<customEpoch>>(2, 1));
        generators.push_back(std::make_unique<Snowflake<customEpoch>>(1, 2));
        generators.push_back(std::make_unique<Snowflake<customEpoch>>(3, 2));

        // Generate IDs from each generator
        std::vector<uint64_t> allIds;
        for (size_t i = 0; i < generators.size(); ++i) {
            std::cout << "Generator " << (i + 1) << " (Worker "
                      << generators[i]->getWorkerId() << ", Datacenter "
                      << generators[i]->getDatacenterId() << "):\n";

            for (int j = 0; j < 3; ++j) {
                uint64_t id = generators[i]->nextid()[0];
                allIds.push_back(id);

                uint64_t timestamp, datacenterId, workerId, sequence;
                generators[i]->parseId(id, timestamp, datacenterId, workerId,
                                       sequence);

                std::cout << "  ID: " << id << " (Worker: " << workerId
                          << ", DC: " << datacenterId << ", Seq: " << sequence
                          << ")\n";
            }
            std::cout << "\n";
        }

        // Verify all IDs are unique across generators
        std::set<uint64_t> uniqueIds(allIds.begin(), allIds.end());
        std::cout << "Cross-generator uniqueness test:\n";
        std::cout << "  Total IDs generated: " << allIds.size() << "\n";
        std::cout << "  Unique IDs: " << uniqueIds.size() << "\n";
        std::cout << "  Result: "
                  << (allIds.size() == uniqueIds.size() ? "✓ All unique"
                                                        : "✗ Duplicates found")
                  << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error in multiple workers demonstration: " << e.what()
                  << "\n";
    }
}

/**
 * @brief Main function demonstrating comprehensive Snowflake capabilities
 */
int main() {
    std::cout << "=== Atom Snowflake ID Generator Comprehensive Example ===\n";
    std::cout << "Demonstrating distributed unique ID generation...\n";

    try {
        // Run all demonstration functions
        demonstrateBasicSnowflakeGeneration();
        demonstrateUniquenessAndOrdering();
        demonstrateMultipleWorkersAndDatacenters();

        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "All Snowflake Examples Completed Successfully\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "The Snowflake ID generator provides:\n";
        std::cout << "  ✓ Distributed unique ID generation\n";
        std::cout << "  ✓ Monotonically increasing IDs within a worker\n";
        std::cout << "  ✓ High throughput ID generation\n";
        std::cout << "  ✓ Support for multiple workers and datacenters\n";
        std::cout << "  ✓ Timestamp-based ordering and parsing\n";
        std::cout << "  ✓ Configurable epoch for different applications\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in Snowflake example: " << e.what()
                  << "\n";
        return 1;
    }
}
