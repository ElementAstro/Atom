#include "atom/extra/curl/connection_pool.hpp"
#include "atom/extra/curl/error.hpp"
#include "atom/extra/curl/session.hpp"

#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

using namespace atom::extra::curl;
using namespace std::chrono_literals;

// Function to perform HTTP requests using a connection pool
void perform_requests_with_pool(ConnectionPool& pool, int thread_id,
                                int num_requests) {
    std::cout << "Thread " << thread_id << " starting " << num_requests
              << " requests" << std::endl;

    for (int i = 0; i < num_requests; ++i) {
        try {
            // Create session with connection pool
            Session session(&pool);

            // Perform request
            auto start = std::chrono::high_resolution_clock::now();
            auto response = session.get(
                "https://httpbin.org/get?thread=" + std::to_string(thread_id) +
                "&request=" + std::to_string(i));
            auto end = std::chrono::high_resolution_clock::now();

            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                      start);

            std::cout << "Thread " << thread_id << ", Request " << i
                      << " - Status: " << response.status_code()
                      << ", Duration: " << duration.count() << "ms"
                      << std::endl;

        } catch (const Error& e) {
            std::cerr << "Thread " << thread_id << ", Request " << i
                      << " failed: " << e.what() << std::endl;
        }

        // Small delay between requests
        std::this_thread::sleep_for(100ms);
    }

    std::cout << "Thread " << thread_id << " completed all requests"
              << std::endl;
}

// Function to perform requests without connection pool (for comparison)
void perform_requests_without_pool(int thread_id, int num_requests) {
    std::cout << "Thread " << thread_id << " (no pool) starting "
              << num_requests << " requests" << std::endl;

    for (int i = 0; i < num_requests; ++i) {
        try {
            // Create new session for each request
            Session session;

            auto start = std::chrono::high_resolution_clock::now();
            auto response = session.get(
                "https://httpbin.org/get?thread=" + std::to_string(thread_id) +
                "&request=" + std::to_string(i));
            auto end = std::chrono::high_resolution_clock::now();

            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                      start);

            std::cout << "Thread " << thread_id << " (no pool), Request " << i
                      << " - Status: " << response.status_code()
                      << ", Duration: " << duration.count() << "ms"
                      << std::endl;

        } catch (const Error& e) {
            std::cerr << "Thread " << thread_id << " (no pool), Request " << i
                      << " failed: " << e.what() << std::endl;
        }

        // Small delay between requests
        std::this_thread::sleep_for(100ms);
    }

    std::cout << "Thread " << thread_id << " (no pool) completed all requests"
              << std::endl;
}

int main() {
    try {
        std::cout << "=== CURL Connection Pool Example ===" << std::endl;

        // 1. Basic connection pool usage
        std::cout << "\n1. Basic Connection Pool Usage:" << std::endl;
        {
            ConnectionPool pool(5);  // Pool with 5 connections

            // Perform a few requests using the pool
            for (int i = 0; i < 3; ++i) {
                Session session(&pool);
                auto response = session.get("https://httpbin.org/get?request=" +
                                            std::to_string(i));
                std::cout << "Request " << i
                          << " - Status: " << response.status_code()
                          << std::endl;
            }
        }

        // 2. Multi-threaded requests with connection pool
        std::cout << "\n2. Multi-threaded Requests WITH Connection Pool:"
                  << std::endl;
        {
            ConnectionPool pool(10);  // Pool with 10 connections
            const int num_threads = 4;
            const int requests_per_thread = 3;

            std::vector<std::future<void>> futures;
            auto start_time = std::chrono::high_resolution_clock::now();

            // Launch threads
            for (int i = 0; i < num_threads; ++i) {
                futures.push_back(
                    std::async(std::launch::async, perform_requests_with_pool,
                               std::ref(pool), i, requests_per_thread));
            }

            // Wait for all threads to complete
            for (auto& future : futures) {
                future.wait();
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            auto total_duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time);

            std::cout << "WITH pool - Total time: " << total_duration.count()
                      << "ms" << std::endl;
        }

        // 3. Multi-threaded requests without connection pool (for comparison)
        std::cout << "\n3. Multi-threaded Requests WITHOUT Connection Pool:"
                  << std::endl;
        {
            const int num_threads = 4;
            const int requests_per_thread = 3;

            std::vector<std::future<void>> futures;
            auto start_time = std::chrono::high_resolution_clock::now();

            // Launch threads
            for (int i = 0; i < num_threads; ++i) {
                futures.push_back(std::async(std::launch::async,
                                             perform_requests_without_pool, i,
                                             requests_per_thread));
            }

            // Wait for all threads to complete
            for (auto& future : futures) {
                future.wait();
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            auto total_duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time);

            std::cout << "WITHOUT pool - Total time: " << total_duration.count()
                      << "ms" << std::endl;
        }

        // 4. Connection pool with different sizes
        std::cout << "\n4. Testing Different Pool Sizes:" << std::endl;
        {
            std::vector<size_t> pool_sizes = {1, 3, 5, 10};

            for (size_t pool_size : pool_sizes) {
                std::cout << "\nTesting pool size: " << pool_size << std::endl;
                ConnectionPool pool(pool_size);

                auto start_time = std::chrono::high_resolution_clock::now();

                // Perform 5 requests
                for (int i = 0; i < 5; ++i) {
                    Session session(&pool);
                    auto response =
                        session.get("https://httpbin.org/get?pool_size=" +
                                    std::to_string(pool_size) +
                                    "&request=" + std::to_string(i));
                    std::cout << "  Request " << i
                              << " - Status: " << response.status_code()
                              << std::endl;
                }

                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        end_time - start_time);

                std::cout << "  Pool size " << pool_size
                          << " - Total time: " << duration.count() << "ms"
                          << std::endl;
            }
        }

        // 5. Connection pool resource management
        std::cout << "\n5. Connection Pool Resource Management:" << std::endl;
        {
            ConnectionPool pool(
                3);  // Small pool to demonstrate resource management

            std::cout << "Creating multiple sessions..." << std::endl;

            // Create sessions that will acquire connections
            std::vector<std::unique_ptr<Session>> sessions;
            for (int i = 0; i < 5; ++i) {  // More sessions than pool size
                try {
                    sessions.push_back(std::make_unique<Session>(&pool));
                    std::cout << "Created session " << i << std::endl;
                } catch (const Error& e) {
                    std::cerr << "Failed to create session " << i << ": "
                              << e.what() << std::endl;
                }
            }

            std::cout << "Performing requests with existing sessions..."
                      << std::endl;
            for (size_t i = 0; i < sessions.size(); ++i) {
                try {
                    auto response = sessions[i]->get(
                        "https://httpbin.org/get?session=" + std::to_string(i));
                    std::cout << "Session " << i
                              << " - Status: " << response.status_code()
                              << std::endl;
                } catch (const Error& e) {
                    std::cerr << "Session " << i
                              << " request failed: " << e.what() << std::endl;
                }
            }

            std::cout << "Destroying sessions to release connections..."
                      << std::endl;
            sessions.clear();

            std::cout << "Creating new session after cleanup..." << std::endl;
            Session new_session(&pool);
            auto response =
                new_session.get("https://httpbin.org/get?cleanup=true");
            std::cout << "New session - Status: " << response.status_code()
                      << std::endl;
        }

        std::cout << "\n=== Connection Pool Example Completed ===" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
