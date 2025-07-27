#pragma once

#include "thread_safe_xml.hpp"
#include "lock_free_pool.hpp"
#include <spdlog/spdlog.h>

#include <atomic>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <concepts>
#include <algorithm>
#include <execution>
#include <ranges>
#include <source_location>

namespace atom::extra::pugixml::concurrent {

/**
 * @brief Work-stealing deque for efficient task distribution
 */
template<typename T>
class WorkStealingDeque {
private:
    static constexpr size_t INITIAL_CAPACITY = 256;
    
    struct CircularArray {
        std::atomic<size_t> capacity;
        std::unique_ptr<std::atomic<T>[]> data;
        
        explicit CircularArray(size_t cap) : capacity(cap) {
            data = std::make_unique<std::atomic<T>[]>(cap);
        }
        
        [[nodiscard]] T load(size_t index) const noexcept {
            return data[index % capacity.load(std::memory_order_acquire)]
                .load(std::memory_order_acquire);
        }
        
        void store(size_t index, T value) noexcept {
            data[index % capacity.load(std::memory_order_acquire)]
                .store(value, std::memory_order_release);
        }
    };
    
    std::atomic<size_t> top_{0};
    std::atomic<size_t> bottom_{0};
    std::atomic<CircularArray*> array_;
    std::mutex resize_mutex_;
    
    void resize() {
        std::lock_guard lock(resize_mutex_);
        auto old_array = array_.load(std::memory_order_acquire);
        auto new_capacity = old_array->capacity.load() * 2;
        auto new_array = new CircularArray(new_capacity);
        
        auto current_bottom = bottom_.load(std::memory_order_acquire);
        auto current_top = top_.load(std::memory_order_acquire);
        
        for (size_t i = current_top; i < current_bottom; ++i) {
            new_array->store(i, old_array->load(i));
        }
        
        array_.store(new_array.release(), std::memory_order_release);
        delete old_array;
    }
    
public:
    WorkStealingDeque() {
        array_.store(new CircularArray(INITIAL_CAPACITY), std::memory_order_relaxed);
    }
    
    ~WorkStealingDeque() {
        delete array_.load();
    }
    
    void push_bottom(T item) {
        auto current_bottom = bottom_.load(std::memory_order_relaxed);
        auto current_top = top_.load(std::memory_order_acquire);
        auto current_array = array_.load(std::memory_order_acquire);
        
        if (current_bottom - current_top >= current_array->capacity.load() - 1) {
            resize();
            current_array = array_.load(std::memory_order_acquire);
        }
        
        current_array->store(current_bottom, item);
        std::atomic_thread_fence(std::memory_order_release);
        bottom_.store(current_bottom + 1, std::memory_order_relaxed);
    }
    
    [[nodiscard]] std::optional<T> pop_bottom() {
        auto current_bottom = bottom_.load(std::memory_order_relaxed);
        auto current_array = array_.load(std::memory_order_acquire);
        
        if (current_bottom == 0) {
            return std::nullopt;
        }
        
        current_bottom--;
        bottom_.store(current_bottom, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        
        auto current_top = top_.load(std::memory_order_relaxed);
        
        if (current_top <= current_bottom) {
            auto item = current_array->load(current_bottom);
            
            if (current_top == current_bottom) {
                if (!top_.compare_exchange_strong(current_top, current_top + 1,
                                                std::memory_order_seq_cst,
                                                std::memory_order_relaxed)) {
                    bottom_.store(current_bottom + 1, std::memory_order_relaxed);
                    return std::nullopt;
                }
                bottom_.store(current_bottom + 1, std::memory_order_relaxed);
            }
            return item;
        } else {
            bottom_.store(current_bottom + 1, std::memory_order_relaxed);
            return std::nullopt;
        }
    }
    
    [[nodiscard]] std::optional<T> steal() {
        auto current_top = top_.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        auto current_bottom = bottom_.load(std::memory_order_acquire);
        
        if (current_top < current_bottom) {
            auto current_array = array_.load(std::memory_order_acquire);
            auto item = current_array->load(current_top);
            
            if (!top_.compare_exchange_strong(current_top, current_top + 1,
                                            std::memory_order_seq_cst,
                                            std::memory_order_relaxed)) {
                return std::nullopt;
            }
            return item;
        }
        return std::nullopt;
    }
    
    [[nodiscard]] bool empty() const noexcept {
        auto current_bottom = bottom_.load(std::memory_order_relaxed);
        auto current_top = top_.load(std::memory_order_relaxed);
        return current_top >= current_bottom;
    }
};

/**
 * @brief Task concept for parallel processing
 */
template<typename T>
concept Task = requires(T t) {
    { t() } -> std::same_as<void>;
};

/**
 * @brief High-performance thread pool with work stealing
 */
class ThreadPool {
private:
    using TaskType = std::function<void()>;
    
    std::vector<std::thread> workers_;
    std::vector<std::unique_ptr<WorkStealingDeque<TaskType>>> queues_;
    std::atomic<bool> shutdown_{false};
    std::shared_ptr<spdlog::logger> logger_;
    
    static thread_local size_t worker_id_;
    static thread_local ThreadPool* current_pool_;
    
    void worker_loop(size_t id) {
        worker_id_ = id;
        current_pool_ = this;
        
        if (logger_) {
            logger_->debug("Worker {} started", id);
        }
        
        while (!shutdown_.load(std::memory_order_acquire)) {
            TaskType task;
            
            // Try to get task from own queue
            if (auto opt_task = queues_[id]->pop_bottom()) {
                task = std::move(*opt_task);
            } else {
                // Try to steal from other queues
                bool found = false;
                for (size_t i = 0; i < queues_.size(); ++i) {
                    if (i != id) {
                        if (auto stolen_task = queues_[i]->steal()) {
                            task = std::move(*stolen_task);
                            found = true;
                            break;
                        }
                    }
                }
                
                if (!found) {
                    std::this_thread::yield();
                    continue;
                }
            }
            
            try {
                task();
            } catch (const std::exception& e) {
                if (logger_) {
                    logger_->error("Task execution failed in worker {}: {}", id, e.what());
                }
            }
        }
        
        if (logger_) {
            logger_->debug("Worker {} stopped", id);
        }
    }
    
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency(),
                       std::shared_ptr<spdlog::logger> logger = nullptr)
        : logger_(logger) {
        
        if (num_threads == 0) {
            num_threads = 1;
        }
        
        queues_.reserve(num_threads);
        workers_.reserve(num_threads);
        
        for (size_t i = 0; i < num_threads; ++i) {
            queues_.emplace_back(std::make_unique<WorkStealingDeque<TaskType>>());
        }
        
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back(&ThreadPool::worker_loop, this, i);
        }
        
        if (logger_) {
            logger_->info("ThreadPool created with {} workers", num_threads);
        }
    }
    
    ~ThreadPool() {
        shutdown_.store(true, std::memory_order_release);
        
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        
        if (logger_) {
            logger_->info("ThreadPool destroyed");
        }
    }
    
    template<Task T>
    void submit(T&& task) {
        if (shutdown_.load(std::memory_order_acquire)) {
            throw std::runtime_error("ThreadPool is shutting down");
        }
        
        // If called from worker thread, use its queue
        if (current_pool_ == this && worker_id_ < queues_.size()) {
            queues_[worker_id_]->push_bottom(std::forward<T>(task));
        } else {
            // Round-robin assignment for external submissions
            static std::atomic<size_t> next_queue{0};
            auto queue_id = next_queue.fetch_add(1, std::memory_order_relaxed) % queues_.size();
            queues_[queue_id]->push_bottom(std::forward<T>(task));
        }
    }
    
    template<typename F, typename... Args>
    [[nodiscard]] auto submit_with_future(F&& f, Args&&... args) 
        -> std::future<std::invoke_result_t<F, Args...>> {
        
        using ReturnType = std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        
        auto future = task->get_future();
        submit([task]() { (*task)(); });
        
        return future;
    }
    
    [[nodiscard]] size_t size() const noexcept {
        return workers_.size();
    }
    
    [[nodiscard]] bool is_shutdown() const noexcept {
        return shutdown_.load(std::memory_order_acquire);
    }
};

thread_local size_t ThreadPool::worker_id_{SIZE_MAX};
thread_local ThreadPool* ThreadPool::current_pool_{nullptr};

/**
 * @brief Parallel XML processor with advanced concurrency features
 */
class ParallelXmlProcessor {
private:
    ThreadPool thread_pool_;
    LockFreePool<ThreadSafeNode> node_pool_;
    std::shared_ptr<spdlog::logger> logger_;
    
public:
    explicit ParallelXmlProcessor(size_t num_threads = std::thread::hardware_concurrency(),
                                 std::shared_ptr<spdlog::logger> logger = nullptr)
        : thread_pool_(num_threads, logger), node_pool_(logger), logger_(logger) {
        
        if (logger_) {
            logger_->info("ParallelXmlProcessor created with {} threads", num_threads);
        }
    }
    
    /**
     * @brief Process XML nodes in parallel using std::execution
     */
    template<typename Range, typename UnaryFunction>
    void parallel_for_each(Range&& range, UnaryFunction&& func) {
        if (logger_) {
            logger_->debug("Starting parallel_for_each with {} elements", 
                          std::ranges::distance(range));
        }
        
        std::for_each(std::execution::par_unseq, 
                     std::ranges::begin(range), 
                     std::ranges::end(range),
                     std::forward<UnaryFunction>(func));
    }
    
    /**
     * @brief Parallel transformation of XML nodes
     */
    template<typename InputRange, typename OutputIterator, typename UnaryOperation>
    void parallel_transform(InputRange&& input, OutputIterator output, 
                           UnaryOperation&& op) {
        if (logger_) {
            logger_->debug("Starting parallel_transform");
        }
        
        std::transform(std::execution::par_unseq,
                      std::ranges::begin(input),
                      std::ranges::end(input),
                      output,
                      std::forward<UnaryOperation>(op));
    }
    
    /**
     * @brief Parallel reduction of XML data
     */
    template<typename Range, typename T, typename BinaryOperation>
    [[nodiscard]] T parallel_reduce(Range&& range, T init, BinaryOperation&& op) {
        if (logger_) {
            logger_->debug("Starting parallel_reduce");
        }
        
        return std::reduce(std::execution::par_unseq,
                          std::ranges::begin(range),
                          std::ranges::end(range),
                          init,
                          std::forward<BinaryOperation>(op));
    }
    
    /**
     * @brief Submit asynchronous XML processing task
     */
    template<typename F, typename... Args>
    [[nodiscard]] auto submit_async(F&& f, Args&&... args) {
        return thread_pool_.submit_with_future(std::forward<F>(f), 
                                              std::forward<Args>(args)...);
    }
    
    /**
     * @brief Get thread pool statistics
     */
    [[nodiscard]] auto get_pool_statistics() const {
        return node_pool_.get_statistics();
    }
    
    /**
     * @brief Get number of worker threads
     */
    [[nodiscard]] size_t thread_count() const noexcept {
        return thread_pool_.size();
    }
};

}  // namespace atom::extra::pugixml::concurrent
