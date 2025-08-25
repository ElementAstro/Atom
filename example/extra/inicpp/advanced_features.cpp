#include "atom/extra/inicpp/inicpp.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace inicpp;
using namespace std::chrono_literals;

// Helper function to create test INI files
void create_test_ini_file(const std::string& filename,
                          const std::string& content) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << content;
        file.close();
        std::cout << "Created test file: " << filename << std::endl;
    }
}

int main() {
    try {
        std::cout << "=== inicpp Advanced Features Example ===" << std::endl;

        // 1. Nested sections (if supported)
        std::cout << "\n1. Nested Sections:" << std::endl;
        {
            create_test_ini_file("nested.ini", R"(
[database]
host = localhost
port = 5432

[database.connection]
timeout = 30
pool_size = 10
ssl_mode = require

[database.connection.retry]
max_attempts = 3
delay = 1000
exponential_backoff = true

[server]
port = 8080

[server.ssl]
enabled = true
cert_file = /path/to/cert.pem
key_file = /path/to/key.pem

[server.ssl.protocols]
tls_1_2 = true
tls_1_3 = true
)");

            IniFile ini;
            try {
                ini.load("nested.ini");
                std::cout << "Loaded nested sections INI file" << std::endl;

                // Access nested sections
                if (ini.has("database.connection")) {
                    auto timeout =
                        ini["database.connection"]["timeout"].as<int>();
                    auto pool_size =
                        ini["database.connection"]["pool_size"].as<int>();
                    std::cout << "DB connection timeout: " << timeout
                              << std::endl;
                    std::cout << "DB pool size: " << pool_size << std::endl;
                }

                if (ini.has("server.ssl")) {
                    auto ssl_enabled = ini["server.ssl"]["enabled"].as<bool>();
                    std::cout << "SSL enabled: " << (ssl_enabled ? "yes" : "no")
                              << std::endl;
                }
            } catch (const std::exception& e) {
                std::cout << "Nested sections not supported or error: "
                          << e.what() << std::endl;
            }
        }

        // 2. Path queries (if supported)
        std::cout << "\n2. Path Queries:" << std::endl;
        {
            create_test_ini_file("path_query.ini", R"(
[app]
name = MyApp
version = 1.0.0

[app.database]
host = localhost
port = 5432
name = myapp_db

[app.cache]
type = redis
host = localhost
port = 6379

[app.logging]
level = info
file = /var/log/app.log
)");

            IniFile ini;
            ini.load("path_query.ini");

            try {
                // Query using path notation
                auto app_name = ini.query("app.name");
                auto db_host = ini.query("app.database.host");
                auto cache_type = ini.query("app.cache.type");

                std::cout << "App name (via query): " << app_name << std::endl;
                std::cout << "DB host (via query): " << db_host << std::endl;
                std::cout << "Cache type (via query): " << cache_type
                          << std::endl;

                // Query with wildcards
                auto all_hosts = ini.queryAll("*.host");
                std::cout << "All hosts found: " << all_hosts.size()
                          << std::endl;
                for (const auto& host : all_hosts) {
                    std::cout << "  " << host << std::endl;
                }
            } catch (const std::exception& e) {
                std::cout << "Path queries not supported or error: " << e.what()
                          << std::endl;
            }
        }

        // 3. Event listeners (if supported)
        std::cout << "\n3. Event Listeners:" << std::endl;
        {
            create_test_ini_file("events.ini", R"(
[settings]
debug = false
timeout = 30
)");

            IniFile ini;

            try {
                // Set up event listeners
                ini.addEventListener([](const std::string& section,
                                        const std::string& key,
                                        const std::string& old_value,
                                        const std::string& new_value) {
                    std::cout << "Field changed: [" << section << "]." << key
                              << " from '" << old_value << "' to '" << new_value
                              << "'" << std::endl;
                });

                ini.addSectionListener(
                    [](const std::string& section, bool added) {
                        std::cout << "Section " << section << " "
                                  << (added ? "added" : "removed") << std::endl;
                    });

                ini.load("events.ini");

                // Make changes to trigger events
                ini["settings"]["debug"] = true;
                ini["settings"]["timeout"] = 60;
                ini["new_section"]["new_key"] = "new_value";

                std::cout << "Event listeners triggered for changes"
                          << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Event listeners not supported or error: "
                          << e.what() << std::endl;
            }
        }

        // 4. Format conversion (if supported)
        std::cout << "\n4. Format Conversion:" << std::endl;
        {
            create_test_ini_file("convert_source.ini", R"(
[application]
name = ConvertApp
version = 1.0.0
debug = true

[database]
host = localhost
port = 5432
ssl = true
)");

            IniFile ini;
            ini.load("convert_source.ini");

            try {
                // Convert to JSON
                auto json_output = ini.toJson();
                std::cout << "JSON conversion:" << std::endl;
                std::cout << json_output << std::endl;

                // Convert to YAML
                auto yaml_output = ini.toYaml();
                std::cout << "YAML conversion:" << std::endl;
                std::cout << yaml_output << std::endl;

                // Convert to XML
                auto xml_output = ini.toXml();
                std::cout << "XML conversion:" << std::endl;
                std::cout << xml_output << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Format conversion not supported or error: "
                          << e.what() << std::endl;
            }
        }

        // 5. Thread safety testing
        std::cout << "\n5. Thread Safety Testing:" << std::endl;
        {
            create_test_ini_file("thread_test.ini", R"(
[counters]
counter1 = 0
counter2 = 0
counter3 = 0
)");

            IniFile ini;
            ini.load("thread_test.ini");

            std::vector<std::future<void>> futures;
            const int num_threads = 4;
            const int operations_per_thread = 100;

            // Launch multiple threads to modify the INI file
            for (int t = 0; t < num_threads; ++t) {
                futures.push_back(std::async(
                    std::launch::async, [&ini, t, operations_per_thread]() {
                        for (int i = 0; i < operations_per_thread; ++i) {
                            // Read and write operations
                            auto counter_key =
                                "counter" + std::to_string((t % 3) + 1);
                            auto current_value =
                                ini["counters"][counter_key].as<int>();
                            ini["counters"][counter_key] = current_value + 1;

                            // Small delay to increase chance of race conditions
                            std::this_thread::sleep_for(1ms);
                        }
                    }));
            }

            // Wait for all threads to complete
            for (auto& future : futures) {
                future.wait();
            }

            std::cout << "Thread safety test completed:" << std::endl;
            std::cout << "  Counter1: " << ini["counters"]["counter1"].as<int>()
                      << std::endl;
            std::cout << "  Counter2: " << ini["counters"]["counter2"].as<int>()
                      << std::endl;
            std::cout << "  Counter3: " << ini["counters"]["counter3"].as<int>()
                      << std::endl;

            int total = ini["counters"]["counter1"].as<int>() +
                        ini["counters"]["counter2"].as<int>() +
                        ini["counters"]["counter3"].as<int>();
            std::cout << "  Total operations: " << total
                      << " (expected: " << num_threads * operations_per_thread
                      << ")" << std::endl;
        }

        // 6. Memory pool usage (if supported)
        std::cout << "\n6. Memory Pool Usage:" << std::endl;
        {
            try {
                // Create a large INI file to test memory efficiency
                std::ofstream large_file("large.ini");
                for (int section = 0; section < 100; ++section) {
                    large_file << "[section_" << section << "]" << std::endl;
                    for (int key = 0; key < 50; ++key) {
                        large_file << "key_" << key << " = value_" << key << "_"
                                   << section << std::endl;
                    }
                }
                large_file.close();

                auto start_time = std::chrono::high_resolution_clock::now();

                IniFile ini;
                ini.load("large.ini");

                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        end_time - start_time);

                std::cout << "Loaded large INI file (" << ini.size()
                          << " sections) in " << duration.count() << " ms"
                          << std::endl;

                // Count total fields
                int total_fields = 0;
                for (const auto& [section_name, section] : ini) {
                    total_fields += section.size();
                }
                std::cout << "Total fields: " << total_fields << std::endl;

            } catch (const std::exception& e) {
                std::cout << "Memory pool test error: " << e.what()
                          << std::endl;
            }
        }

        // 7. Custom field types and converters
        std::cout << "\n7. Custom Field Types:" << std::endl;
        {
            create_test_ini_file("custom_types.ini", R"(
[network]
ip_address = 192.168.1.100
mac_address = 00:11:22:33:44:55
ports = 80,443,8080,8443

[colors]
primary = #FF0000
secondary = #00FF00
background = #0000FF

[coordinates]
latitude = 40.7128
longitude = -74.0060
)");

            IniFile ini;
            ini.load("custom_types.ini");

            // Parse IP address
            auto ip_str = ini["network"]["ip_address"].as<std::string>();
            std::cout << "IP Address: " << ip_str << std::endl;

            // Parse port list
            auto ports_str = ini["network"]["ports"].as<std::string>();
            std::vector<int> ports;
            std::stringstream ss(ports_str);
            std::string port;
            while (std::getline(ss, port, ',')) {
                ports.push_back(std::stoi(port));
            }
            std::cout << "Ports: ";
            for (size_t i = 0; i < ports.size(); ++i) {
                std::cout << ports[i];
                if (i < ports.size() - 1)
                    std::cout << ", ";
            }
            std::cout << std::endl;

            // Parse color values
            auto primary_color = ini["colors"]["primary"].as<std::string>();
            std::cout << "Primary color: " << primary_color << std::endl;

            // Parse coordinates
            auto latitude = ini["coordinates"]["latitude"].as<double>();
            auto longitude = ini["coordinates"]["longitude"].as<double>();
            std::cout << "Coordinates: " << latitude << ", " << longitude
                      << std::endl;
        }

        // 8. Configuration validation
        std::cout << "\n8. Configuration Validation:" << std::endl;
        {
            create_test_ini_file("validate.ini", R"(
[server]
port = 8080
host = localhost
workers = 4

[database]
host = db.example.com
port = 5432
timeout = 30
)");

            IniFile ini;
            ini.load("validate.ini");

            // Validate configuration
            bool valid = true;
            std::vector<std::string> errors;

            // Check required sections
            if (!ini.has("server")) {
                errors.push_back("Missing required section: server");
                valid = false;
            }

            if (!ini.has("database")) {
                errors.push_back("Missing required section: database");
                valid = false;
            }

            // Validate server configuration
            if (ini.has("server")) {
                auto port = ini["server"]["port"].as<int>();
                if (port < 1 || port > 65535) {
                    errors.push_back("Invalid server port: " +
                                     std::to_string(port));
                    valid = false;
                }

                auto workers = ini["server"]["workers"].as<int>();
                if (workers < 1 || workers > 100) {
                    errors.push_back("Invalid worker count: " +
                                     std::to_string(workers));
                    valid = false;
                }
            }

            // Validate database configuration
            if (ini.has("database")) {
                auto db_port = ini["database"]["port"].as<int>();
                if (db_port < 1 || db_port > 65535) {
                    errors.push_back("Invalid database port: " +
                                     std::to_string(db_port));
                    valid = false;
                }

                auto timeout = ini["database"]["timeout"].as<int>();
                if (timeout < 1 || timeout > 300) {
                    errors.push_back("Invalid database timeout: " +
                                     std::to_string(timeout));
                    valid = false;
                }
            }

            std::cout << "Configuration validation: "
                      << (valid ? "PASSED" : "FAILED") << std::endl;
            if (!valid) {
                std::cout << "Validation errors:" << std::endl;
                for (const auto& error : errors) {
                    std::cout << "  - " << error << std::endl;
                }
            }
        }

        // 9. Performance benchmarking
        std::cout << "\n9. Performance Benchmarking:" << std::endl;
        {
            const int num_sections = 1000;
            const int fields_per_section = 20;

            // Create benchmark file
            std::ofstream bench_file("benchmark.ini");
            for (int s = 0; s < num_sections; ++s) {
                bench_file << "[section_" << s << "]" << std::endl;
                for (int f = 0; f < fields_per_section; ++f) {
                    bench_file << "field_" << f << " = value_" << s << "_" << f
                               << std::endl;
                }
            }
            bench_file.close();

            // Benchmark loading
            auto start = std::chrono::high_resolution_clock::now();
            IniFile ini;
            ini.load("benchmark.ini");
            auto load_end = std::chrono::high_resolution_clock::now();

            // Benchmark reading
            int read_count = 0;
            for (const auto& [section_name, section] : ini) {
                for (const auto& [field_name, field] : section) {
                    auto value = field.as<std::string>();
                    read_count++;
                }
            }
            auto read_end = std::chrono::high_resolution_clock::now();

            auto load_time =
                std::chrono::duration_cast<std::chrono::milliseconds>(load_end -
                                                                      start);
            auto read_time =
                std::chrono::duration_cast<std::chrono::milliseconds>(read_end -
                                                                      load_end);

            std::cout << "Performance benchmark:" << std::endl;
            std::cout << "  Sections: " << num_sections << std::endl;
            std::cout << "  Fields per section: " << fields_per_section
                      << std::endl;
            std::cout << "  Total fields: " << read_count << std::endl;
            std::cout << "  Load time: " << load_time.count() << " ms"
                      << std::endl;
            std::cout << "  Read time: " << read_time.count() << " ms"
                      << std::endl;
            std::cout << "  Load rate: "
                      << (read_count /
                          std::max(1, static_cast<int>(load_time.count())))
                      << " fields/ms" << std::endl;
        }

        // Cleanup test files
        std::cout << "\nCleaning up test files..." << std::endl;
        std::filesystem::remove("nested.ini");
        std::filesystem::remove("path_query.ini");
        std::filesystem::remove("events.ini");
        std::filesystem::remove("convert_source.ini");
        std::filesystem::remove("thread_test.ini");
        std::filesystem::remove("large.ini");
        std::filesystem::remove("custom_types.ini");
        std::filesystem::remove("validate.ini");
        std::filesystem::remove("benchmark.ini");

        std::cout << "\n=== inicpp Advanced Features Example Completed ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
