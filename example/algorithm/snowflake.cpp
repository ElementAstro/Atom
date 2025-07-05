#include "atom/algorithm/snowflake.hpp"

#include <iostream>

int main() {
    using namespace atom::algorithm;

    // Define the epoch (custom epoch for Snowflake IDs)
    constexpr uint64_t customEpoch = 1609459200000;  // January 1, 2021

    // Create a Snowflake generator with worker ID 1 and datacenter ID 1
    Snowflake<customEpoch> snowflake(1, 1);

    // Generate a unique ID
    uint64_t id = snowflake.nextid()[0];
    std::cout << "Generated ID: " << id << std::endl;

    // Parse the generated ID
    uint64_t timestamp, datacenterId, workerId, sequence;
    snowflake.parseId(id, timestamp, datacenterId, workerId, sequence);
    std::cout << "Parsed ID:" << std::endl;
    std::cout << "  Timestamp: " << timestamp << std::endl;
    std::cout << "  Datacenter ID: " << datacenterId << std::endl;
    std::cout << "  Worker ID: " << workerId << std::endl;
    std::cout << "  Sequence: " << sequence << std::endl;

    // Reset the Snowflake generator
    snowflake.reset();
    std::cout << "Snowflake generator reset." << std::endl;

    // Generate another unique ID after reset
    uint64_t newId = snowflake.nextid()[0];
    std::cout << "Generated new ID after reset: " << newId << std::endl;

    // Retrieve current worker ID and datacenter ID
    uint64_t currentWorkerId = snowflake.getWorkerId();
    uint64_t currentDatacenterId = snowflake.getDatacenterId();
    std::cout << "Current Worker ID: " << currentWorkerId << std::endl;
    std::cout << "Current Datacenter ID: " << currentDatacenterId << std::endl;

    return 0;
}
