// http.cpp
#include "http.hpp"
#include <spdlog/spdlog.h>

HttpClient::HttpClient(net::io_context& ioc,
                       bool enable_connection_pool,
                       bool enable_performance_monitoring)
    : resolver_(net::make_strand(ioc))
    , stream_(net::make_strand(ioc))
    , connection_pool_enabled_(enable_connection_pool)
    , performance_monitoring_enabled_(enable_performance_monitoring) {

    setDefaultHeader("User-Agent", BOOST_BEAST_VERSION_STRING);
    setDefaultHeader("Accept", "*/*");
    setDefaultHeader("Connection", "keep-alive"); // Enable keep-alive for pooling

    // Initialize connection pool if enabled
    if (connection_pool_enabled_) {
        connection_pool_ = std::make_unique<atom::beast::pool::LockFreeConnectionPool>(ioc);
        spdlog::info("Lock-free connection pool initialized");
    }

    // Initialize performance monitoring if enabled
    if (performance_monitoring_enabled_) {
        performance_monitor_ = &atom::beast::monitoring::get_global_performance_monitor();
        spdlog::info("Performance monitoring enabled");
    }

    // Initialize work-stealing queue for batch operations
    work_queue_ = std::make_unique<atom::beast::concurrency::WorkStealingDeque<std::function<void()>>>();

    spdlog::info("HttpClient initialized with advanced concurrency features");
}

void HttpClient::setDefaultHeader(std::string_view key,
                                  std::string_view value) {
    if (key.empty()) {
        throw std::invalid_argument("Header key must not be empty");
    }
    default_headers_[std::string(key)] = std::string(value);
}

void HttpClient::setTimeout(std::chrono::seconds timeout) {
    if (timeout <= std::chrono::seconds(0)) {
        throw std::invalid_argument("Timeout must be positive");
    }
    timeout_ = timeout;
}

void HttpClient::validateHostPort(std::string_view host,
                                  std::string_view port) const {
    if (host.empty() || port.empty()) {
        throw std::invalid_argument("Host and port must not be empty");
    }
}

void HttpClient::setupRequest(
    http::request<http::string_body>& req, http::verb method,
    std::string_view host, std::string_view target, int version,
    std::string_view content_type, std::string_view body,
    const std::unordered_map<std::string, std::string>& headers) const {
    req.method(method);
    req.target(std::string(target));
    req.version(version);
    req.set(http::field::host, std::string(host));

    for (const auto& [key, value] : default_headers_) {
        req.set(key, value);
    }

    for (const auto& [key, value] : headers) {
        req.set(key, value);
    }

    if (!content_type.empty()) {
        req.set(http::field::content_type, std::string(content_type));
    }

    if (!body.empty()) {
        req.body() = std::string(body);
        req.prepare_payload();
    }
}

void HttpClient::gracefulClose() {
    beast::error_code ec;
    auto result = stream_.socket().shutdown(tcp::socket::shutdown_both, ec);
    if (ec && ec != beast::errc::not_connected) {
        spdlog::debug("Socket shutdown warning: {} (result: {})", ec.message(),
                      result.message());
    }
}

auto HttpClient::request(
    http::verb method, std::string_view host, std::string_view port,
    std::string_view target, int version, std::string_view content_type,
    std::string_view body,
    const std::unordered_map<std::string, std::string>& headers)
    -> http::response<http::string_body> {
    validateHostPort(host, port);

    // Start performance monitoring
    auto start_time = std::chrono::steady_clock::now();
    if (performance_monitoring_enabled_ && performance_monitor_) {
        performance_monitor_->record_http_request_start();
    }

    http::request<http::string_body> req;
    setupRequest(req, method, host, target, version, content_type, body, headers);

    spdlog::debug("Sending {} request to {}:{}{}",
                  std::string(http::to_string(method)), host, port, target);

    http::response<http::string_body> res;

    try {
        // Try to use connection pool if enabled
        if (connection_pool_enabled_ && connection_pool_) {
            auto conn = connection_pool_->acquire_connection(host, port);

            // Set timeout and send request
            conn->stream().expires_after(timeout_);
            http::write(conn->stream(), req);

            // Read response
            beast::flat_buffer buffer;
            http::read(conn->stream(), buffer, res);

            // Return connection to pool
            connection_pool_->release_connection(std::move(conn));
        } else {
            // Fallback to traditional connection
            auto const results = resolver_.resolve(std::string(host), std::string(port));
            stream_.connect(results);
            stream_.expires_after(timeout_);

            http::write(stream_, req);

            beast::flat_buffer buffer;
            http::read(stream_, buffer, res);

            gracefulClose();
        }

        spdlog::debug("Received response: {} {}", static_cast<int>(res.result()), res.reason());

        // Record successful request
        if (performance_monitoring_enabled_ && performance_monitor_) {
            performance_monitor_->record_http_request_success(
                start_time, body.size(), res.body().size());
        }

    } catch (const std::exception& e) {
        spdlog::error("Request failed: {}", e.what());

        // Record failed request
        if (performance_monitoring_enabled_ && performance_monitor_) {
            performance_monitor_->record_http_request_error();
        }

        throw;
    }

    return res;
}



auto HttpClient::uploadFile(std::string_view host, std::string_view port,
                            std::string_view target, std::string_view filepath,
                            std::string_view field_name)
    -> http::response<http::string_body> {
    validateHostPort(host, port);
    if (filepath.empty()) {
        throw std::invalid_argument("Filepath must not be empty");
    }

    std::filesystem::path file_path(filepath);
    if (!std::filesystem::exists(file_path)) {
        throw std::runtime_error("File does not exist: " + file_path.string());
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + file_path.string());
    }

    std::string file_content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());

    auto boundary =
        "----WebKitFormBoundary" +
        std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());

    std::string body;
    body.reserve(file_content.size() + 512);

    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"";
    body += field_name.empty() ? "file" : field_name;
    body += "\"; filename=\"" + file_path.filename().string() + "\"\r\n";
    body += "Content-Type: application/octet-stream\r\n\r\n";
    body += file_content + "\r\n";
    body += "--" + boundary + "--\r\n";

    return request(http::verb::post, host, port, target, 11,
                   "multipart/form-data; boundary=" + boundary, body);
}

void HttpClient::downloadFile(std::string_view host, std::string_view port,
                              std::string_view target,
                              std::string_view filepath) {
    if (filepath.empty()) {
        throw std::invalid_argument("Filepath must not be empty");
    }

    auto response = request(http::verb::get, host, port, target);

    if (response.result() != http::status::ok) {
        throw beast::system_error(
            beast::error_code(static_cast<int>(response.result()),
                              boost::system::generic_category()));
    }

    std::filesystem::path file_path(filepath);
    if (auto parent = file_path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream outFile(file_path, std::ios::binary);
    if (!outFile) {
        throw std::runtime_error("Failed to open file for writing: " +
                                 file_path.string());
    }

    outFile << response.body();
    if (!outFile) {
        throw std::runtime_error("Failed to write to file: " +
                                 file_path.string());
    }

    spdlog::info("File downloaded successfully to {}", file_path.string());
}

auto HttpClient::requestWithRetry(
    http::verb method, std::string_view host, std::string_view port,
    std::string_view target, int retry_count, int version,
    std::string_view content_type, std::string_view body,
    const std::unordered_map<std::string, std::string>& headers)
    -> http::response<http::string_body> {
    validateHostPort(host, port);

    for (int attempt = 0; attempt < retry_count; ++attempt) {
        try {
            spdlog::debug("Request attempt {} of {}", attempt + 1, retry_count);
            return request(method, host, port, target, version, content_type,
                           body, headers);
        } catch (const beast::system_error& e) {
            spdlog::warn("Request attempt {} failed: {}", attempt + 1,
                         e.what());

            if (attempt + 1 == retry_count) {
                spdlog::error("All retry attempts failed");
                throw;
            }

            auto delay = std::chrono::milliseconds(100 << attempt);
            std::this_thread::sleep_for(delay);
        }
    }

    throw std::runtime_error("All retry attempts failed");
}

auto HttpClient::batchRequest(
    const std::vector<std::tuple<http::verb, std::string, std::string,
                                 std::string>>& requests,
    const std::unordered_map<std::string, std::string>& headers)
    -> std::vector<http::response<http::string_body>> {
    std::vector<http::response<http::string_body>> responses;
    responses.reserve(requests.size());

    for (const auto& [method, host, port, target] : requests) {
        try {
            validateHostPort(host, port);
            spdlog::debug("Executing batch request to {}:{}{}", host, port,
                          target);
            responses.emplace_back(
                request(method, host, port, target, 11, "", "", headers));
        } catch (const std::exception& e) {
            spdlog::error("Batch request failed for {}: {}", target, e.what());
            responses.emplace_back();
        }
    }

    return responses;
}

auto HttpClient::batchRequestWorkStealing(
    const std::vector<std::tuple<http::verb, std::string, std::string, std::string>>& requests,
    const std::unordered_map<std::string, std::string>& headers,
    std::size_t num_worker_threads) -> std::vector<http::response<http::string_body>> {

    if (requests.empty()) {
        return {};
    }

    if (num_worker_threads == 0) {
        num_worker_threads = std::thread::hardware_concurrency();
    }

    spdlog::info("Starting work-stealing batch request with {} requests on {} threads",
                requests.size(), num_worker_threads);

    // Prepare result storage
    std::vector<http::response<http::string_body>> responses(requests.size());
    std::vector<std::atomic<bool>> completed(requests.size());
    std::vector<std::exception_ptr> exceptions(requests.size());

    // Initialize completion flags
    for (auto& flag : completed) {
        flag.store(false, std::memory_order_relaxed);
    }

    // Create work-stealing deques for each worker
    std::vector<std::unique_ptr<atom::beast::concurrency::WorkStealingDeque<std::size_t>>> worker_queues;
    for (std::size_t i = 0; i < num_worker_threads; ++i) {
        worker_queues.emplace_back(
            std::make_unique<atom::beast::concurrency::WorkStealingDeque<std::size_t>>());
    }

    // Distribute work across queues
    for (std::size_t i = 0; i < requests.size(); ++i) {
        worker_queues[i % num_worker_threads]->push_bottom(std::move(i));
    }

    // Launch worker threads
    std::vector<std::thread> workers;
    std::atomic<std::size_t> completed_count{0};

    for (std::size_t worker_id = 0; worker_id < num_worker_threads; ++worker_id) {
        workers.emplace_back([&, worker_id]() {
            auto& my_queue = *worker_queues[worker_id];

            while (completed_count.load(std::memory_order_acquire) < requests.size()) {
                std::size_t task_index;
                bool found_work = false;

                // Try to get work from own queue first
                if (my_queue.pop_bottom(task_index)) {
                    found_work = true;
                } else {
                    // Try to steal work from other queues
                    for (std::size_t steal_from = 0; steal_from < num_worker_threads; ++steal_from) {
                        if (steal_from != worker_id && worker_queues[steal_from]->steal(task_index)) {
                            found_work = true;
                            break;
                        }
                    }
                }

                if (found_work) {
                    try {
                        const auto& [method, host, port, target] = requests[task_index];

                        // Create a new HttpClient instance for this thread
                        net::io_context local_ioc;
                        HttpClient local_client(local_ioc, false, false); // Disable pooling for workers

                        // Copy headers and execute request
                        responses[task_index] = local_client.request(
                            method, host, port, target, 11, "", "", headers);

                        completed[task_index].store(true, std::memory_order_release);
                        completed_count.fetch_add(1, std::memory_order_acq_rel);

                        spdlog::debug("Worker {} completed task {} ({}:{}{})",
                                    worker_id, task_index, host, port, target);

                    } catch (...) {
                        exceptions[task_index] = std::current_exception();
                        completed[task_index].store(true, std::memory_order_release);
                        completed_count.fetch_add(1, std::memory_order_acq_rel);

                        spdlog::error("Worker {} failed task {}", worker_id, task_index);
                    }
                } else {
                    // No work available, yield briefly
                    std::this_thread::yield();
                }
            }

            spdlog::debug("Worker {} finished", worker_id);
        });
    }

    // Wait for all workers to complete
    for (auto& worker : workers) {
        worker.join();
    }

    // Check for exceptions and rethrow the first one found
    for (std::size_t i = 0; i < exceptions.size(); ++i) {
        if (exceptions[i]) {
            spdlog::error("Request {} failed, rethrowing exception", i);
            std::rethrow_exception(exceptions[i]);
        }
    }

    spdlog::info("Work-stealing batch request completed: {}/{} successful",
                completed_count.load(), requests.size());

    return responses;
}

void HttpClient::runWithThreadPool(size_t num_threads) {
    if (num_threads == 0) {
        throw std::invalid_argument("Thread count must be positive");
    }

    net::thread_pool pool(num_threads);

    // Set thread affinity for NUMA awareness if possible
    for (size_t i = 0; i < num_threads; ++i) {
        net::post(pool, [i, num_threads]() {
            spdlog::debug("NUMA-aware worker thread {} started (total: {})", i, num_threads);

            // Initialize thread-local allocators
            atom::beast::concurrency::NUMAAwareThreadLocal<int>::initialize();
        });
    }

    pool.join();
    spdlog::info("NUMA-aware thread pool completed with {} threads", num_threads);
}

void HttpClient::configureConnectionPool(std::size_t max_connections_per_host,
                                        std::chrono::seconds max_idle_time,
                                        std::chrono::seconds connection_timeout) {
    if (connection_pool_) {
        connection_pool_->set_max_connections_per_host(max_connections_per_host);
        connection_pool_->set_max_idle_time(max_idle_time);
        connection_pool_->set_connection_timeout(connection_timeout);

        spdlog::info("Connection pool configured: max_conn={}, idle_time={}s, timeout={}s",
                    max_connections_per_host, max_idle_time.count(), connection_timeout.count());
    } else {
        spdlog::warn("Connection pool not enabled, configuration ignored");
    }
}

atom::beast::monitoring::PerformanceMonitor::PerformanceStats
HttpClient::getPerformanceStatistics() const {
    if (performance_monitor_) {
        return performance_monitor_->get_statistics();
    }
    return {};
}

void HttpClient::resetPerformanceStatistics() {
    if (performance_monitor_) {
        performance_monitor_->reset_statistics();
        spdlog::info("Performance statistics reset");
    }
}

void HttpClient::logPerformanceSummary() const {
    if (performance_monitor_) {
        performance_monitor_->log_performance_summary();
    } else {
        spdlog::warn("Performance monitoring not enabled");
    }
}
