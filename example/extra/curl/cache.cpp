#include "atom/extra/curl/cache.hpp"
#include "atom/extra/curl/error.hpp"
#include "atom/extra/curl/response.hpp"
#include "atom/extra/curl/session.hpp"

#include <chrono>
#include <iostream>
#include <thread>

using namespace atom::extra::curl;
using namespace std::chrono_literals;

int main() {
    try {
        std::cout << "=== CURL Cache Example ===" << std::endl;

        // 1. Basic cache usage
        std::cout << "\n1. Basic Cache Usage:" << std::endl;
        {
            Cache cache;
            Session session;
            session.set_cache(&cache);

            std::cout << "First request (should hit server):" << std::endl;
            auto start1 = std::chrono::high_resolution_clock::now();
            auto response1 = session.get(
                "https://httpbin.org/cache/60");  // Cache for 60 seconds
            auto end1 = std::chrono::high_resolution_clock::now();
            auto duration1 =
                std::chrono::duration_cast<std::chrono::milliseconds>(end1 -
                                                                      start1);

            std::cout << "Status: " << response1.status_code()
                      << ", Duration: " << duration1.count() << "ms"
                      << std::endl;
            std::cout << "Cache-Control: " << response1.header("Cache-Control")
                      << std::endl;

            std::cout << "\nSecond request (should be cached):" << std::endl;
            auto start2 = std::chrono::high_resolution_clock::now();
            auto response2 = session.get("https://httpbin.org/cache/60");
            auto end2 = std::chrono::high_resolution_clock::now();
            auto duration2 =
                std::chrono::duration_cast<std::chrono::milliseconds>(end2 -
                                                                      start2);

            std::cout << "Status: " << response2.status_code()
                      << ", Duration: " << duration2.count() << "ms"
                      << std::endl;

            if (duration2 < duration1) {
                std::cout << "Second request was faster (likely cached)!"
                          << std::endl;
            }
        }

        // 2. Cache with different URLs
        std::cout << "\n2. Cache with Different URLs:" << std::endl;
        {
            Cache cache;
            Session session;
            session.set_cache(&cache);

            std::vector<std::string> urls = {
                "https://httpbin.org/cache/30",
                "https://httpbin.org/get?param=1",
                "https://httpbin.org/get?param=2",
                "https://httpbin.org/cache/30"  // Same as first, should be
                                                // cached
            };

            for (size_t i = 0; i < urls.size(); ++i) {
                auto start = std::chrono::high_resolution_clock::now();
                auto response = session.get(urls[i]);
                auto end = std::chrono::high_resolution_clock::now();
                auto duration =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        end - start);

                std::cout << "URL " << i << " (" << urls[i] << ")" << std::endl;
                std::cout << "  Status: " << response.status_code()
                          << ", Duration: " << duration.count() << "ms"
                          << std::endl;
            }
        }

        // 3. Cache expiration
        std::cout << "\n3. Cache Expiration:" << std::endl;
        {
            Cache cache;
            Session session;
            session.set_cache(&cache);

            // Request with short cache time
            std::cout << "Request with 2-second cache:" << std::endl;
            auto response1 = session.get("https://httpbin.org/cache/2");
            std::cout << "First request - Status: " << response1.status_code()
                      << std::endl;

            // Immediate second request (should be cached)
            auto start2 = std::chrono::high_resolution_clock::now();
            auto response2 = session.get("https://httpbin.org/cache/2");
            auto end2 = std::chrono::high_resolution_clock::now();
            auto duration2 =
                std::chrono::duration_cast<std::chrono::milliseconds>(end2 -
                                                                      start2);

            std::cout << "Second request (immediate) - Status: "
                      << response2.status_code()
                      << ", Duration: " << duration2.count() << "ms"
                      << std::endl;

            // Wait for cache to expire
            std::cout << "Waiting 3 seconds for cache to expire..."
                      << std::endl;
            std::this_thread::sleep_for(3s);

            // Third request (cache should be expired)
            auto start3 = std::chrono::high_resolution_clock::now();
            auto response3 = session.get("https://httpbin.org/cache/2");
            auto end3 = std::chrono::high_resolution_clock::now();
            auto duration3 =
                std::chrono::duration_cast<std::chrono::milliseconds>(end3 -
                                                                      start3);

            std::cout << "Third request (after expiration) - Status: "
                      << response3.status_code()
                      << ", Duration: " << duration3.count() << "ms"
                      << std::endl;

            if (duration3 > duration2) {
                std::cout
                    << "Third request was slower (cache expired, hit server)"
                    << std::endl;
            }
        }

        // 4. Cache validation (ETag/Last-Modified)
        std::cout << "\n4. Cache Validation:" << std::endl;
        {
            Cache cache;
            Session session;
            session.set_cache(&cache);

            // First request
            std::cout << "First request:" << std::endl;
            auto response1 = session.get("https://httpbin.org/etag/test-etag");
            std::cout << "Status: " << response1.status_code() << std::endl;
            std::cout << "ETag: " << response1.header("ETag") << std::endl;

            // Second request (should send If-None-Match header)
            std::cout << "\nSecond request (with validation):" << std::endl;
            auto response2 = session.get("https://httpbin.org/etag/test-etag");
            std::cout << "Status: " << response2.status_code() << std::endl;

            if (response2.status_code() == 304) {
                std::cout << "Received 304 Not Modified (validation successful)"
                          << std::endl;
            }
        }

        // 5. Cache statistics and management
        std::cout << "\n5. Cache Statistics and Management:" << std::endl;
        {
            Cache cache;
            Session session;
            session.set_cache(&cache);

            // Perform several requests
            std::vector<std::string> test_urls = {
                "https://httpbin.org/cache/60",
                "https://httpbin.org/get?test=1",
                "https://httpbin.org/get?test=2",
                "https://httpbin.org/cache/60",   // Duplicate
                "https://httpbin.org/get?test=1"  // Duplicate
            };

            std::cout << "Performing " << test_urls.size() << " requests..."
                      << std::endl;
            for (size_t i = 0; i < test_urls.size(); ++i) {
                auto response = session.get(test_urls[i]);
                std::cout << "Request " << i
                          << " - Status: " << response.status_code()
                          << std::endl;
            }

            // Check cache statistics (if available)
            std::cout << "\nCache operations completed" << std::endl;

            // Clear cache
            std::cout << "Clearing cache..." << std::endl;
            cache.clear();

            // Request after clearing cache
            std::cout << "Request after cache clear:" << std::endl;
            auto start = std::chrono::high_resolution_clock::now();
            auto response = session.get("https://httpbin.org/cache/60");
            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                      start);

            std::cout << "Status: " << response.status_code()
                      << ", Duration: " << duration.count() << "ms"
                      << std::endl;
        }

        // 6. Cache with POST requests (should not be cached)
        std::cout << "\n6. Cache with POST Requests (should not cache):"
                  << std::endl;
        {
            Cache cache;
            Session session;
            session.set_cache(&cache);

            std::map<std::string, std::string> data = {{"test", "data"}};

            std::cout << "First POST request:" << std::endl;
            auto start1 = std::chrono::high_resolution_clock::now();
            auto response1 = session.post("https://httpbin.org/post", data);
            auto end1 = std::chrono::high_resolution_clock::now();
            auto duration1 =
                std::chrono::duration_cast<std::chrono::milliseconds>(end1 -
                                                                      start1);

            std::cout << "Status: " << response1.status_code()
                      << ", Duration: " << duration1.count() << "ms"
                      << std::endl;

            std::cout << "Second POST request (should not be cached):"
                      << std::endl;
            auto start2 = std::chrono::high_resolution_clock::now();
            auto response2 = session.post("https://httpbin.org/post", data);
            auto end2 = std::chrono::high_resolution_clock::now();
            auto duration2 =
                std::chrono::duration_cast<std::chrono::milliseconds>(end2 -
                                                                      start2);

            std::cout << "Status: " << response2.status_code()
                      << ", Duration: " << duration2.count() << "ms"
                      << std::endl;

            std::cout << "POST requests are not cached (as expected)"
                      << std::endl;
        }

        // 7. Cache with custom headers
        std::cout << "\n7. Cache with Custom Headers:" << std::endl;
        {
            Cache cache;
            Session session;
            session.set_cache(&cache);

            // Set custom headers
            session.set_header("User-Agent", "Cache-Test-Agent/1.0");
            session.set_header("Accept", "application/json");

            std::cout << "Request with custom headers:" << std::endl;
            auto response1 = session.get("https://httpbin.org/headers");
            std::cout << "Status: " << response1.status_code() << std::endl;

            std::cout << "Second request with same headers (should be cached):"
                      << std::endl;
            auto start = std::chrono::high_resolution_clock::now();
            auto response2 = session.get("https://httpbin.org/headers");
            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                      start);

            std::cout << "Status: " << response2.status_code()
                      << ", Duration: " << duration.count() << "ms"
                      << std::endl;
        }

        std::cout << "\n=== Cache Example Completed ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
