#include "atom/extra/curl/rate_limiter.hpp"
#include "atom/extra/curl/error.hpp"
#include "atom/extra/curl/session.hpp"

#include <chrono>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

using namespace atom::extra::curl;
using namespace std::chrono_literals;

// Function to perform requests with rate limitingvoid
// perform_rate_limited_requests(RateLimiter& limiter, int thread_id,
                                   int num_requests) {
                                       std::cout << "Thread " << thread_id
                                                 << " starting " << num_requests
                                                 << " rate-limited requests"
                                                 << std::endl;

                                       Session session;
                                       session.set_rate_limiter(&limiter);

                                       for (int i = 0; i < num_requests; ++i) {
                                           try {
                                               auto start = std::chrono::
                                                   high_resolution_clock::now();

                                               // The rate limiter will
                                               // automatically control the
                                               // request rate
                                               auto response = session.get(
                                                   "https://httpbin.org/"
                                                   "get?thread=" +
                                                   std::to_string(thread_id) +
                                                   "&request=" +
                                                   std::to_string(i));

                                               auto end = std::chrono::
                                                   high_resolution_clock::now();
                                               auto duration =
                                                   std::chrono::duration_cast<
                                                       std::chrono::
                                                           milliseconds>(end -
                                                                         start);

                                               std::cout
                                                   << "Thread " << thread_id
                                                   << ", Request " << i
                                                   << " - Status: "
                                                   << response.status_code()
                                                   << ", Duration: "
                                                   << duration.count() << "ms"
                                                   << ", Time: "
                                                   << std::chrono::duration_cast<
                                                          std::chrono::
                                                              milliseconds>(
                                                          std::chrono::
                                                              steady_clock::now()
                                                                  .time_since_epoch())
                                                          .count()
                                                   << "ms" << std::endl;

                                           } catch (const Error& e) {
                                               std::cerr
                                                   << "Thread " << thread_id
                                                   << ", Request " << i
                                                   << " failed: " << e.what()
                                                   << std::endl;
                                           }
                                       }

                                       std::cout << "Thread " << thread_id
                                                 << " completed all "
                                                    "rate-limited requests"
                                                 << std::endl;
                                   }

                                   // Function to perform requests without rate
                                   // limiting (for comparison)
                                   void perform_unlimited_requests(
                                       int thread_id, int num_requests) {
                                       std::cout << "Thread " << thread_id
                                                 << " starting " << num_requests
                                                 << " unlimited requests"
                                                 << std::endl;

                                       Session session;

                                       for (int i = 0; i < num_requests; ++i) {
                                           try {
                                               auto start = std::chrono::
                                                   high_resolution_clock::now();
                                               auto response = session.get(
                                                   "https://httpbin.org/"
                                                   "get?thread=" +
                                                   std::to_string(thread_id) +
                                                   "&request=" +
                                                   std::to_string(i));
                                               auto end = std::chrono::
                                                   high_resolution_clock::now();

                                               auto duration =
                                                   std::chrono::duration_cast<
                                                       std::chrono::
                                                           milliseconds>(end -
                                                                         start);

                                               std::cout
                                                   << "Thread " << thread_id
                                                   << " (unlimited), Request "
                                                   << i << " - Status: "
                                                   << response.status_code()
                                                   << ", Duration: "
                                                   << duration.count() << "ms"
                                                   << ", Time: "
                                                   << std::chrono::duration_cast<
                                                          std::chrono::
                                                              milliseconds>(
                                                          std::chrono::
                                                              steady_clock::now()
                                                                  .time_since_epoch())
                                                          .count()
                                                   << "ms" << std::endl;

                                           } catch (const Error& e) {
                                               std::cerr
                                                   << "Thread " << thread_id
                                                   << " (unlimited), Request "
                                                   << i
                                                   << " failed: " << e.what()
                                                   << std::endl;
                                           }
                                       }

                                       std::cout << "Thread " << thread_id
                                                 << " completed all unlimited "
                                                    "requests"
                                                 << std::endl;
                                   }

                                   int main() {
                                       try {
                                           std::cout << "=== CURL Rate Limiter "
                                                        "Example ==="
                                                     << std::endl;

                                           // 1. Basic rate limiting
                                           std::cout
                                               << "\n1. Basic Rate Limiting (2 "
                                                  "requests per second):"
                                               << std::endl;
                                           {
                                               RateLimiter limiter(
                                                   2.0);  // 2 requests per
                                                          // second
                                               Session session;
                                               session.set_rate_limiter(
                                                   &limiter);

                                               auto start_time = std::chrono::
                                                   high_resolution_clock::now();

                                               // Perform 5 requests - should
                                               // take at least 2 seconds due to
                                               // rate limiting
                                               for (int i = 0; i < 5; ++i) {
                                                   auto request_start =
                                                       std::chrono::
                                                           high_resolution_clock::
                                                               now();
                                                   auto response = session.get(
                                                       "https://httpbin.org/"
                                                       "get?request=" +
                                                       std::to_string(i));
                                                   auto request_end =
                                                       std::chrono::
                                                           high_resolution_clock::
                                                               now();

                                                   auto request_duration = std::
                                                       chrono::duration_cast<
                                                           std::chrono::
                                                               milliseconds>(
                                                           request_end -
                                                           request_start);
                                                   auto total_elapsed = std::
                                                       chrono::duration_cast<
                                                           std::chrono::
                                                               milliseconds>(
                                                           request_end -
                                                           start_time);

                                                   std::cout
                                                       << "Request " << i
                                                       << " - Status: "
                                                       << response.status_code()
                                                       << ", Request time: "
                                                       << request_duration
                                                              .count()
                                                       << "ms"
                                                       << ", Total elapsed: "
                                                       << total_elapsed.count()
                                                       << "ms" << std::endl;
                                               }

                                               auto end_time = std::chrono::
                                                   high_resolution_clock::now();
                                               auto total_duration =
                                                   std::chrono::duration_cast<
                                                       std::chrono::
                                                           milliseconds>(
                                                       end_time - start_time);
                                               std::cout
                                                   << "Total time for 5 "
                                                      "requests: "
                                                   << total_duration.count()
                                                   << "ms" << std::endl;
                                           }

                                           // 2. Different rate limits
                                           // comparison
                                           std::cout << "\n2. Different Rate "
                                                        "Limits Comparison:"
                                                     << std::endl;
                                           {
                                               std::vector<double> rates = {
                                                   1.0, 2.0,
                                                   5.0};  // requests per second

                                               for (double rate : rates) {
                                                   std::cout
                                                       << "\nTesting rate: "
                                                       << rate
                                                       << " requests/second"
                                                       << std::endl;
                                                   RateLimiter limiter(rate);
                                                   Session session;
                                                   session.set_rate_limiter(
                                                       &limiter);

                                                   auto start_time = std::chrono::
                                                       high_resolution_clock::
                                                           now();

                                                   // Perform 3 requests
                                                   for (int i = 0; i < 3; ++i) {
                                                       auto response =
                                                           session.get(
                                                               "https://"
                                                               "httpbin.org/"
                                                               "get?rate=" +
                                                               std::to_string(
                                                                   rate) +
                                                               "&request=" +
                                                               std::to_string(
                                                                   i));
                                                       auto elapsed = std::
                                                           chrono::duration_cast<
                                                               std::chrono::
                                                                   milliseconds>(
                                                               std::chrono::
                                                                   high_resolution_clock::
                                                                       now() -
                                                               start_time);

                                                       std::cout
                                                           << "  Request " << i
                                                           << " - Status: "
                                                           << response
                                                                  .status_code()
                                                           << ", Elapsed: "
                                                           << elapsed.count()
                                                           << "ms" << std::endl;
                                                   }

                                                   auto end_time = std::chrono::
                                                       high_resolution_clock::
                                                           now();
                                                   auto total_duration = std::
                                                       chrono::duration_cast<
                                                           std::chrono::
                                                               milliseconds>(
                                                           end_time -
                                                           start_time);
                                                   std::cout
                                                       << "  Total time: "
                                                       << total_duration.count()
                                                       << "ms" << std::endl;
                                               }
                                           }

                                           // 3. Multi-threaded rate limiting
                                           std::cout << "\n3. Multi-threaded "
                                                        "Rate Limiting:"
                                                     << std::endl;
                                           {
                                               RateLimiter limiter(
                                                   3.0);  // 3 requests per
                                                          // second across all
                                                          // threads
                                               const int num_threads = 3;
                                               const int requests_per_thread =
                                                   2;

                                               std::vector<std::future<void>>
                                                   futures;
                                               auto start_time = std::chrono::
                                                   high_resolution_clock::now();

                                               std::cout
                                                   << "Starting " << num_threads
                                                   << " threads with shared "
                                                      "rate limiter..."
                                                   << std::endl;

                                               // Launch threads
                                               for (int i = 0; i < num_threads;
                                                    ++i) {
                                                   futures.push_back(std::async(
                                                       std::launch::async,
                                                       perform_rate_limited_requests,
                                                       std::ref(limiter), i,
                                                       requests_per_thread));
                                               }

                                               // Wait for all threads to
                                               // complete
                                               for (auto& future : futures) {
                                                   future.wait();
                                               }

                                               auto end_time = std::chrono::
                                                   high_resolution_clock::now();
                                               auto total_duration =
                                                   std::chrono::duration_cast<
                                                       std::chrono::
                                                           milliseconds>(
                                                       end_time - start_time);

                                               std::cout
                                                   << "Multi-threaded "
                                                      "rate-limited - Total "
                                                      "time: "
                                                   << total_duration.count()
                                                   << "ms" << std::endl;
                                           }

                                           // 4. Comparison with unlimited
                                           // requests
                                           std::cout << "\n4. Comparison: Rate "
                                                        "Limited vs Unlimited:"
                                                     << std::endl;
                                           {
                                               const int num_threads = 2;
                                               const int requests_per_thread =
                                                   3;

                                               // Rate limited test
                                               std::cout << "\nRate Limited "
                                                            "Test (2 req/sec):"
                                                         << std::endl;
                                               {
                                                   RateLimiter limiter(2.0);
                                                   std::vector<
                                                       std::future<void>>
                                                       futures;
                                                   auto start_time = std::chrono::
                                                       high_resolution_clock::
                                                           now();

                                                   for (int i = 0;
                                                        i < num_threads; ++i) {
                                                       futures.push_back(std::async(
                                                           std::launch::async,
                                                           perform_rate_limited_requests,
                                                           std::ref(limiter), i,
                                                           requests_per_thread));
                                                   }

                                                   for (auto& future :
                                                        futures) {
                                                       future.wait();
                                                   }

                                                   auto end_time = std::chrono::
                                                       high_resolution_clock::
                                                           now();
                                                   auto duration = std::chrono::
                                                       duration_cast<
                                                           std::chrono::
                                                               milliseconds>(
                                                           end_time -
                                                           start_time);
                                                   std::cout << "Rate limited "
                                                                "total time: "
                                                             << duration.count()
                                                             << "ms"
                                                             << std::endl;
                                               }

                                               // Unlimited test
                                               std::cout << "\nUnlimited Test:"
                                                         << std::endl;
                                               {
                                                   std::vector<
                                                       std::future<void>>
                                                       futures;
                                                   auto start_time = std::chrono::
                                                       high_resolution_clock::
                                                           now();

                                                   for (int i = 0;
                                                        i < num_threads; ++i) {
                                                       futures.push_back(std::async(
                                                           std::launch::async,
                                                           perform_unlimited_requests,
                                                           i,
                                                           requests_per_thread));
                                                   }

                                                   for (auto& future :
                                                        futures) {
                                                       future.wait();
                                                   }

                                                   auto end_time = std::chrono::
                                                       high_resolution_clock::
                                                           now();
                                                   auto duration = std::chrono::
                                                       duration_cast<
                                                           std::chrono::
                                                               milliseconds>(
                                                           end_time -
                                                           start_time);
                                                   std::cout << "Unlimited "
                                                                "total time: "
                                                             << duration.count()
                                                             << "ms"
                                                             << std::endl;
                                               }
                                           }

                                           // 5. Rate limiter with burst
                                           // capability
                                           std::cout << "\n5. Rate Limiter "
                                                        "Behavior Over Time:"
                                                     << std::endl;
                                           {
                                               RateLimiter limiter(
                                                   1.0);  // 1 request per
                                                          // second
                                               Session session;
                                               session.set_rate_limiter(
                                                   &limiter);

                                               std::cout
                                                   << "Performing requests "
                                                      "with 1 req/sec limit..."
                                                   << std::endl;

                                               for (int i = 0; i < 4; ++i) {
                                                   auto start = std::chrono::
                                                       steady_clock::now();
                                                   auto response = session.get(
                                                       "https://httpbin.org/"
                                                       "get?burst_test=" +
                                                       std::to_string(i));
                                                   auto end = std::chrono::
                                                       steady_clock::now();

                                                   auto wait_time = std::
                                                       chrono::duration_cast<
                                                           std::chrono::
                                                               milliseconds>(
                                                           end - start);
                                                   std::cout
                                                       << "Request " << i
                                                       << " - Status: "
                                                       << response.status_code()
                                                       << ", Wait time: "
                                                       << wait_time.count()
                                                       << "ms" << std::endl;
                                               }
                                           }

                                           std::cout << "\n=== Rate Limiter "
                                                        "Example Completed ==="
                                                     << std::endl;

                                       } catch (const std::exception& e) {
                                           std::cerr << "Unexpected error: "
                                                     << e.what() << std::endl;
                                           return 1;
                                       }

                                       return 0;
                                   }
