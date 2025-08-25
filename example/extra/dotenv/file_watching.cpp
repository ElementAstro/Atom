#include "atom/extra/dotenv/dotenv.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

using namespace dotenv;
using namespace std::chrono_literals;

// Helper function to create/update test .env files
void update_env_file(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << content;
        file.close();
        std::cout << "Updated file: " << filename << std::endl;
    }
}

int main() {
    try {
        std::cout << "=== Dotenv File Watching Example ===" << std::endl;

        // 1. Basic file watching
        std::cout << "\n1. Basic File Watching:" << std::endl;
        {
            const std::string watch_file = "watched.env";

            // Create initial .env file
            update_env_file(watch_file, R"(
# Initial configuration
APP_NAME=WatchedApp
VERSION=1.0.0
DEBUG=false
PORT=3000
)");

            Dotenv dotenv;
            std::atomic<int> reload_count{0};
            std::atomic<bool> keep_watching{true};

            // Set up file watcher
            dotenv.watch(watch_file, [&reload_count](const LoadResult& result) {
                reload_count++;
                std::cout << "\n[RELOAD #" << reload_count << "] File changed!"
                          << std::endl;

                if (result.success) {
                    std::cout << "  Successfully reloaded "
                              << result.variables.size()
                              << " variables:" << std::endl;
                    for (const auto& [key, value] : result.variables) {
                        std::cout << "    " << key << " = " << value
                                  << std::endl;
                    }
                } else {
                    std::cout << "  Reload failed with errors:" << std::endl;
                    for (const auto& error : result.errors) {
                        std::cout << "    " << error << std::endl;
                    }
                }
            });

            std::cout << "Started watching " << watch_file << std::endl;
            std::cout << "Making changes to the file..." << std::endl;

            // Simulate file changes
            std::this_thread::sleep_for(1s);

            // Change 1: Update existing values
            update_env_file(watch_file, R"(
# Updated configuration
APP_NAME=WatchedApp
VERSION=1.1.0
DEBUG=true
PORT=3001
LOG_LEVEL=debug
)");

            std::this_thread::sleep_for(2s);

            // Change 2: Add new variables
            update_env_file(watch_file, R"(
# Further updated configuration
APP_NAME=WatchedApp
VERSION=1.2.0
DEBUG=true
PORT=3002
LOG_LEVEL=info
DATABASE_URL=postgresql://localhost:5432/myapp
CACHE_ENABLED=true
)");

            std::this_thread::sleep_for(2s);

            // Change 3: Remove some variables
            update_env_file(watch_file, R"(
# Minimal configuration
APP_NAME=WatchedApp
VERSION=2.0.0
PORT=4000
)");

            std::this_thread::sleep_for(2s);

            std::cout << "\nStopping file watcher..." << std::endl;
            dotenv.stopWatching();

            std::cout << "Total reloads: " << reload_count.load() << std::endl;
        }

        // 2. Watching multiple files
        std::cout << "\n2. Watching Multiple Files:" << std::endl;
        {
            const std::vector<std::string> watch_files = {"app.env", "db.env",
                                                          "cache.env"};
            std::vector<std::unique_ptr<Dotenv>> watchers;
            std::atomic<int> total_changes{0};

            // Create initial files
            update_env_file("app.env", R"(
APP_NAME=MultiWatchApp
APP_VERSION=1.0.0
DEBUG=false
)");

            update_env_file("db.env", R"(
DB_HOST=localhost
DB_PORT=5432
DB_NAME=myapp
)");

            update_env_file("cache.env", R"(
CACHE_HOST=localhost
CACHE_PORT=6379
CACHE_TTL=3600
)");

            // Set up watchers for each file
            for (const auto& file : watch_files) {
                auto watcher = std::make_unique<Dotenv>();
                watcher->watch(file, [&total_changes,
                                      file](const LoadResult& result) {
                    total_changes++;
                    std::cout << "\n[MULTI-WATCH] " << file << " changed!"
                              << std::endl;
                    std::cout << "  Variables: " << result.variables.size()
                              << std::endl;
                    std::cout << "  Success: " << result.success << std::endl;
                });
                watchers.push_back(std::move(watcher));
                std::cout << "Started watching " << file << std::endl;
            }

            std::this_thread::sleep_for(1s);

            // Make changes to different files
            std::cout << "\nMaking changes to multiple files..." << std::endl;

            // Update app.env
            update_env_file("app.env", R"(
APP_NAME=MultiWatchApp
APP_VERSION=1.1.0
DEBUG=true
FEATURE_X=enabled
)");

            std::this_thread::sleep_for(1s);

            // Update db.env
            update_env_file("db.env", R"(
DB_HOST=db.example.com
DB_PORT=5432
DB_NAME=myapp
DB_USER=admin
DB_PASSWORD=secret
)");

            std::this_thread::sleep_for(1s);

            // Update cache.env
            update_env_file("cache.env", R"(
CACHE_HOST=cache.example.com
CACHE_PORT=6379
CACHE_TTL=7200
CACHE_MAX_MEMORY=256mb
)");

            std::this_thread::sleep_for(2s);

            // Stop all watchers
            for (auto& watcher : watchers) {
                watcher->stopWatching();
            }

            std::cout << "Total changes detected: " << total_changes.load()
                      << std::endl;
        }

        // 3. Error handling in file watching
        std::cout << "\n3. Error Handling in File Watching:" << std::endl;
        {
            const std::string error_file = "error_test.env";

            // Create initial valid file
            update_env_file(error_file, R"(
VALID_VAR=value1
ANOTHER_VAR=value2
)");

            Dotenv dotenv;
            std::atomic<int> success_count{0};
            std::atomic<int> error_count{0};

            dotenv.watch(error_file, [&success_count,
                                      &error_count](const LoadResult& result) {
                if (result.success) {
                    success_count++;
                    std::cout << "\n[SUCCESS] File loaded successfully"
                              << std::endl;
                } else {
                    error_count++;
                    std::cout << "\n[ERROR] File load failed:" << std::endl;
                    for (const auto& error : result.errors) {
                        std::cout << "  " << error << std::endl;
                    }
                }
            });

            std::cout << "Started error handling test..." << std::endl;
            std::this_thread::sleep_for(1s);

            // Create invalid content
            update_env_file(error_file, R"(
VALID_VAR=value1
INVALID LINE WITHOUT EQUALS
ANOTHER_VAR=value2
=INVALID_KEY_EMPTY
)");

            std::this_thread::sleep_for(2s);

            // Fix the file
            update_env_file(error_file, R"(
VALID_VAR=fixed_value1
FIXED_VAR=new_value
ANOTHER_VAR=fixed_value2
)");

            std::this_thread::sleep_for(2s);

            dotenv.stopWatching();

            std::cout << "Success reloads: " << success_count.load()
                      << std::endl;
            std::cout << "Error reloads: " << error_count.load() << std::endl;
        }

        // 4. Performance test with frequent changes
        std::cout << "\n4. Performance Test with Frequent Changes:"
                  << std::endl;
        {
            const std::string perf_file = "performance_test.env";

            // Create initial file
            update_env_file(perf_file, R"(
COUNTER=0
TIMESTAMP=0
)");

            Dotenv dotenv;
            std::atomic<int> change_count{0};
            auto start_time = std::chrono::steady_clock::now();

            dotenv.watch(perf_file, [&change_count,
                                     start_time](const LoadResult& result) {
                change_count++;
                auto elapsed =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start_time);

                if (change_count % 5 == 0) {
                    std::cout << "\n[PERF] Change #" << change_count << " at "
                              << elapsed.count() << "ms" << std::endl;
                }
            });

            std::cout << "Starting performance test with rapid changes..."
                      << std::endl;

            // Make rapid changes
            for (int i = 1; i <= 20; ++i) {
                std::string content = "COUNTER=" + std::to_string(i) + "\n";
                content +=
                    "TIMESTAMP=" + std::to_string(std::time(nullptr)) + "\n";
                content += "ITERATION=test_" + std::to_string(i) + "\n";

                update_env_file(perf_file, content);
                std::this_thread::sleep_for(200ms);  // 5 changes per second
            }

            std::this_thread::sleep_for(
                1s);  // Wait for final changes to process

            dotenv.stopWatching();

            auto total_time =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time);

            std::cout << "Performance test completed:" << std::endl;
            std::cout << "  Total changes: " << change_count.load()
                      << std::endl;
            std::cout << "  Total time: " << total_time.count() << "ms"
                      << std::endl;
            std::cout << "  Average time per change: "
                      << (change_count > 0
                              ? total_time.count() / change_count.load()
                              : 0)
                      << "ms" << std::endl;
        }

        // 5. Watching with custom options
        std::cout << "\n5. Watching with Custom Options:" << std::endl;
        {
            const std::string custom_file = "custom_watch.env";

            // Set up custom options
            DotenvOptions options;
            options.debug = true;
            options.logger = [](const std::string& message) {
                std::cout << "[CUSTOM WATCHER] " << message << std::endl;
            };
            options.parse_options.expand_variables = true;
            options.parse_options.trim_whitespace = true;

            // Create initial file with variable expansion
            update_env_file(custom_file, R"(
BASE_URL=https://api.example.com
API_VERSION=v1
FULL_URL=${BASE_URL}/${API_VERSION}
WHITESPACE_VAR=  trimmed value
)");

            Dotenv dotenv(options);
            std::atomic<int> custom_changes{0};

            dotenv.watch(
                custom_file, [&custom_changes](const LoadResult& result) {
                    custom_changes++;
                    std::cout << "\n[CUSTOM] File changed with custom options:"
                              << std::endl;
                    for (const auto& [key, value] : result.variables) {
                        std::cout << "  " << key << " = '" << value << "'"
                                  << std::endl;
                    }
                });

            std::this_thread::sleep_for(1s);

            // Update with more variable expansion
            update_env_file(custom_file, R"(
BASE_URL=https://prod-api.example.com
API_VERSION=v2
FULL_URL=${BASE_URL}/${API_VERSION}
BACKUP_URL=${BASE_URL}/backup
WHITESPACE_VAR=  another trimmed value
)");

            std::this_thread::sleep_for(2s);

            dotenv.stopWatching();

            std::cout << "Custom watcher changes: " << custom_changes.load()
                      << std::endl;
        }

        // Cleanup test files
        std::cout << "\nCleaning up test files..." << std::endl;
        std::filesystem::remove("watched.env");
        std::filesystem::remove("app.env");
        std::filesystem::remove("db.env");
        std::filesystem::remove("cache.env");
        std::filesystem::remove("error_test.env");
        std::filesystem::remove("performance_test.env");
        std::filesystem::remove("custom_watch.env");

        std::cout << "\n=== Dotenv File Watching Example Completed ==="
                  << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
