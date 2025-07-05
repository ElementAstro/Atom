#include "atom/utils/uuid.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>

// Helper function to measure timing
template <typename Func>
double measure_time(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;
    return duration.count();
}

// Benchmark function
template <typename Func>
void benchmark(const std::string& name, Func&& func, int iterations = 1000) {
    double total_time = measure_time([&]() {
        for (int i = 0; i < iterations; ++i) {
            func();
        }
    });
    std::cout << name << ": " << (total_time / iterations)
              << " ms per operation, " << (iterations * 1000.0 / total_time)
              << " ops/s" << std::endl;
}

int main() {
    std::cout << "=== UUID Usage Examples ===" << std::endl;

    // Basic generation and conversion
    std::cout << "\n--- Basic Operations ---" << std::endl;

    // Create a random UUID
    atom::utils::UUID uuid;
    std::cout << "Random UUID: " << uuid.toString() << std::endl;

    // Create a UUID from a string
    std::string uuid_str = "123e4567-e89b-12d3-a456-426614174000";
    auto parsed_uuid = atom::utils::UUID::fromString(uuid_str);
    if (parsed_uuid) {
        std::cout << "Parsed UUID: " << parsed_uuid.value().toString()
                  << std::endl;
    } else {
        std::cout << "Failed to parse UUID string" << std::endl;
    }

    // Create a UUID from raw bytes
    std::array<uint8_t, 16> raw_bytes = {0x12, 0x3e, 0x45, 0x67, 0xe8, 0x9b,
                                         0x12, 0xd3, 0xa4, 0x56, 0x42, 0x66,
                                         0x14, 0x17, 0x40, 0x00};
    atom::utils::UUID byte_uuid(raw_bytes);
    std::cout << "UUID from bytes: " << byte_uuid.toString() << std::endl;

    // UUID comparison
    std::cout << "\n--- UUID Comparisons ---" << std::endl;
    atom::utils::UUID uuid1;
    atom::utils::UUID uuid2;
    std::cout << "UUID 1: " << uuid1.toString() << std::endl;
    std::cout << "UUID 2: " << uuid2.toString() << std::endl;
    std::cout << "Equal? " << (uuid1 == uuid2 ? "Yes" : "No") << std::endl;
    std::cout << "Less than? " << (uuid1 < uuid2 ? "Yes" : "No") << std::endl;

    // Validate UUID format
    std::cout << "\n--- UUID Validation ---" << std::endl;
    std::string valid_uuid = "123e4567-e89b-12d3-a456-426614174000";
    std::string invalid_uuid = "not-a-valid-uuid";
    std::cout << valid_uuid << " is "
              << (atom::utils::UUID::isValidUUID(valid_uuid) ? "valid"
                                                             : "invalid")
              << std::endl;
    std::cout << invalid_uuid << " is "
              << (atom::utils::UUID::isValidUUID(invalid_uuid) ? "valid"
                                                               : "invalid")
              << std::endl;

    // Version-specific generation
    std::cout << "\n--- UUID Version Generation ---" << std::endl;

    // Version 1 (time-based)
    auto v1_uuid = atom::utils::UUID::generateV1();
    std::cout << "Version 1 UUID: " << v1_uuid.toString()
              << " (Version: " << static_cast<int>(v1_uuid.version()) << ")"
              << std::endl;

    // Version 4 (random)
    auto v4_uuid = atom::utils::UUID::generateV4();
    std::cout << "Version 4 UUID: " << v4_uuid.toString()
              << " (Version: " << static_cast<int>(v4_uuid.version()) << ")"
              << std::endl;

    // Version 3 (name-based, MD5)
    atom::utils::UUID namespace_uuid;  // Using a random UUID as namespace
    auto v3_uuid = atom::utils::UUID::generateV3(namespace_uuid, "test-name");
    std::cout << "Version 3 UUID: " << v3_uuid.toString()
              << " (Version: " << static_cast<int>(v3_uuid.version()) << ")"
              << std::endl;

    // Version 5 (name-based, SHA-1)
    auto v5_uuid = atom::utils::UUID::generateV5(namespace_uuid, "test-name");
    std::cout << "Version 5 UUID: " << v5_uuid.toString()
              << " (Version: " << static_cast<int>(v5_uuid.version()) << ")"
              << std::endl;

    // System info utility functions
    std::cout << "\n--- System Info Functions ---" << std::endl;
    std::string mac = atom::utils::getMAC();
    std::cout << "MAC Address: " << (mac.empty() ? "Not available" : mac)
              << std::endl;

    std::string cpu_serial = atom::utils::getCPUSerial();
    std::cout << "CPU Serial: "
              << (cpu_serial.empty() ? "Not available" : cpu_serial)
              << std::endl;

    // Format UUID string
    std::cout << "\n--- UUID Formatting ---" << std::endl;
    std::string raw_uuid = "123e4567e89b12d3a456426614174000";  // No dashes
    std::string formatted = atom::utils::formatUUID(raw_uuid);
    std::cout << "Raw: " << raw_uuid << std::endl;
    std::cout << "Formatted: " << formatted << std::endl;

    // Generate unique UUID
    std::cout << "\n--- Unique UUID Generation ---" << std::endl;
    std::string unique_uuid = atom::utils::generateUniqueUUID();
    std::cout << "Unique UUID: " << unique_uuid << std::endl;

#if ATOM_USE_SIMD
    // SIMD-accelerated UUID operations
    std::cout << "\n--- SIMD-accelerated UUID ---" << std::endl;
    atom::utils::FastUUID fast_uuid;
    std::cout << "Fast UUID: " << fast_uuid.str() << std::endl;

    // Create from string
    atom::utils::FastUUID fast_uuid_from_str(unique_uuid);
    std::cout << "Fast UUID from string: " << fast_uuid_from_str.str()
              << std::endl;

    // Compare FastUUIDs
    std::cout << "Fast UUIDs equal? "
              << (fast_uuid == fast_uuid_from_str ? "Yes" : "No") << std::endl;

    // Fast generator
    atom::utils::FastUUIDGenerator<std::mt19937_64> generator;
    auto gen_uuid = generator.getUUID();
    std::cout << "Generated Fast UUID: " << gen_uuid.str() << std::endl;

    // Batch generation example
    std::cout << "\n--- Batch UUID Generation ---" << std::endl;
    size_t batch_size = 1000;
    std::vector<atom::utils::FastUUID> batch =
        atom::utils::generateUUIDBatch(batch_size);
    std::cout << "Generated batch of " << batch.size() << " UUIDs" << std::endl;
    std::cout << "First: " << batch.front().str() << std::endl;
    std::cout << "Last: " << batch.back().str() << std::endl;

    // Parallel batch generation
    std::vector<atom::utils::FastUUID> parallel_batch =
        atom::utils::generateUUIDBatchParallel(batch_size);
    std::cout << "Generated parallel batch of " << parallel_batch.size()
              << " UUIDs" << std::endl;
    std::cout << "First: " << parallel_batch.front().str() << std::endl;
    std::cout << "Last: " << parallel_batch.back().str() << std::endl;

    // Performance benchmarks
    std::cout << "\n--- Performance Benchmarks ---" << std::endl;
    benchmark("Standard UUID generation", []() {
        atom::utils::UUID uuid;
        return uuid;
    });

    benchmark("Fast UUID generation", []() {
        atom::utils::FastUUID uuid;
        return uuid;
    });

    benchmark("UUID to string", []() {
        static atom::utils::UUID uuid;
        return uuid.toString();
    });

    benchmark("FastUUID to string", []() {
        static atom::utils::FastUUID uuid;
        return uuid.str();
    });

    // Batch generation benchmark
    int batch_benchmark_size = 10000;
    double standard_time = measure_time([&]() {
        for (int i = 0; i < batch_benchmark_size; i++) {
            atom::utils::UUID uuid;
        }
    });

    double fast_time = measure_time([&]() {
        auto batch = atom::utils::generateUUIDBatch(batch_benchmark_size);
    });

    double parallel_time = measure_time([&]() {
        auto batch =
            atom::utils::generateUUIDBatchParallel(batch_benchmark_size);
    });

    std::cout << "Generating " << batch_benchmark_size
              << " UUIDs:" << std::endl;
    std::cout << "  Standard: " << standard_time << " ms ("
              << (batch_benchmark_size * 1000.0 / standard_time) << " UUIDs/s)"
              << std::endl;
    std::cout << "  Batch: " << fast_time << " ms ("
              << (batch_benchmark_size * 1000.0 / fast_time) << " UUIDs/s)"
              << std::endl;
    std::cout << "  Parallel: " << parallel_time << " ms ("
              << (batch_benchmark_size * 1000.0 / parallel_time) << " UUIDs/s)"
              << std::endl;
    std::cout << "  Speed improvement (batch vs standard): "
              << (standard_time / fast_time) << "x" << std::endl;
    std::cout << "  Speed improvement (parallel vs standard): "
              << (standard_time / parallel_time) << "x" << std::endl;
#endif

    return 0;
}
