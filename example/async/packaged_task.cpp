#include "atom/async/packaged_task.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <exception>
// #include <iomanip>  // Removed as it's not used directly
#include <iostream>
#include <mutex>
#include <optional>  // Added for optional future
#include <sstream>   // Added for std::stringstream
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Helper class: Print thread ID
std::string get_thread_id() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

// Print mutex
std::mutex print_mutex;

// Thread-safe print function
template <typename... Args>
void print_safe(Args&&... args) {
    std::lock_guard<std::mutex> lock(print_mutex);
    (std::cout << ... << args) << std::endl;
}

// Print separator line
void print_section(const std::string& title) {
    std::lock_guard<std::mutex> lock(print_mutex);
    std::cout << "\n========== " << title << " ==========\n" << std::endl;
}

// Simple counter to observe callback execution
class CallbackCounter {
public:
    CallbackCounter() : count_(0) {}

    void increment() {
        ++count_;
        print_safe("Callback executed #", count_);
    }

    void increment_with_value(int value) {
        ++count_;
        print_safe("Callback executed #", count_, ", with value: ", value);
    }

    int get_count() const { return count_; }

private:
    std::atomic<int> count_;
};

// 1. Basic Usage Examples
void basic_usage_examples() {
    print_section("Basic Usage Examples");

    // Example 1: Create and execute a task returning an integer
    print_safe("Example 1: Create a task returning an integer");

    atom::async::EnhancedPackagedTask<int> task1([]() {
        print_safe("Thread [", get_thread_id(),
                   "] Task 1 calculation in progress...");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return 42;
    });

    // Get the Future object for the task
    auto future1 = task1.getEnhancedFuture();

    // Execute the task in a new thread
    std::thread t1([&task1]() {
        print_safe("Thread [", get_thread_id(), "] Starting Task 1");
        task1();
    });

    // Wait for and get the result
    print_safe("Main thread [", get_thread_id(),
               "] Waiting for Task 1 result...");
    int result1 = future1.get();
    print_safe("Task 1 result: ", result1);

    t1.join();

    // Example 2: Create and execute a task with parameters
    print_safe("\nExample 2: Create a task with parameters");

    atom::async::EnhancedPackagedTask<std::string, int, double> task2(
        [](int a, double b) {
            print_safe("Thread [", get_thread_id(), "] Calculating ", a, " + ",
                       b);
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            return "Result: " + std::to_string(a + b);
        });

    auto future2 = task2.getEnhancedFuture();

    std::thread t2([&task2]() {
        print_safe("Thread [", get_thread_id(), "] Starting Task 2");
        task2(10, 3.14);
    });

    print_safe("Main thread waiting for Task 2 result...");
    std::string result2 = future2.get();
    print_safe("Task 2 result: ", result2);

    t2.join();

    // Example 3: Create and execute a task with no return value
    print_safe("\nExample 3: Create a task with no return value");

    atom::async::EnhancedPackagedTask<void, std::string> task3(
        [](const std::string& message) {
            print_safe("Thread [", get_thread_id(),
                       "] Executing Task 3: ", message);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            print_safe("Thread [", get_thread_id(), "] Task 3 finished");
        });

    auto future3 = task3.getEnhancedFuture();

    std::thread t3([&task3]() { task3("Processing message"); });

    print_safe("Main thread waiting for Task 3 to complete...");
    future3.wait();
    print_safe("Task 3 completed");

    t3.join();

    // Example 4: Create a task using make_enhanced_task (Commented out as it
    // was in the original)
    print_safe("\nExample 4: Create a task using make_enhanced_task");

    /*
    // Note: The original code had this section commented out.
    // Assuming make_enhanced_task exists and works as intended.
    // Need to deduce the signature correctly or provide it explicitly.
    // The commented-out code below might need adjustment based on the actual
    // implementation of make_enhanced_task.

    // Attempting to deduce signature (might require helper traits or specific
    make_enhanced_task overload) auto lambda_func = [](int x) {
        print_safe("Thread [", get_thread_id()], Calculating square of ", x);
        return x * x;
    };
    // Explicitly providing signature might be safer if deduction isn't
    supported as shown atom::async::EnhancedPackagedTask<int, int>
    task4(lambda_func);

    // Or if make_enhanced_task deduces correctly:
    // auto task4 = atom::async::make_enhanced_task(lambda_func);

    auto future4 = task4.getEnhancedFuture();

    std::thread t4([&task4]() { task4(7); });

    print_safe("Main thread waiting for Task 4 result...");
    int result4 = future4.get();
    print_safe("Task 4 result: Square of 7 = ", result4);

    t4.join();
    */
    print_safe("Example 4 was commented out in the original code.");
}

// 2. Different Parameter Combination Examples
void parameter_combination_examples() {
    print_section("Different Parameter Combination Examples");

    // Example 1: Multiple parameters and complex return type
    print_safe("Example 1: Multiple parameters and complex return type");

    atom::async::EnhancedPackagedTask<std::vector<int>, int, int, int> task1(
        [](int start, int end, int step) {
            print_safe("Thread [", get_thread_id(),
                       "] Generating sequence in range [", start, ", ", end,
                       ") with step ", step);
            std::vector<int> result;
            for (int i = start; i < end; i += step) {
                result.push_back(i);
            }
            return result;
        });

    auto future1 = task1.getEnhancedFuture();

    std::thread t1([&task1]() { task1(5, 20, 3); });

    std::vector<int> result1 = future1.get();
    print_safe("Generated sequence: ");
    // Use print_safe for thread safety if needed, but outputting parts is
    // tricky. Locking for the whole loop is simpler here.
    {
        std::lock_guard<std::mutex> lock(print_mutex);
        for (int val : result1) {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }

    t1.join();

    // Example 2: Using reference parameters
    print_safe("\nExample 2: Using reference parameters");

    struct ResultAccumulator {
        int sum = 0;
        // Make add thread-safe if accumulator is accessed from multiple threads
        // concurrently For this example, it's accessed only by the task thread,
        // then main thread reads.
        void add(int value) { sum += value; }
    };

    ResultAccumulator accumulator;

    // Note: Passing references to tasks executed in other threads requires
    // careful lifetime management. Here, 'accumulator' outlives the thread
    // 't2'.
    atom::async::EnhancedPackagedTask<void, ResultAccumulator&, int> task2(
        [](ResultAccumulator& acc, int value) {
            print_safe("Thread [", get_thread_id(),
                       "] Adding value to accumulator: ", value);
            acc.add(value);
        });

    auto future2 = task2.getEnhancedFuture();

    std::thread t2([&task2, &accumulator]() { task2(accumulator, 42); });

    future2.wait();  // Wait for the task to finish modifying the accumulator
    print_safe("Accumulator result: ", accumulator.sum);

    t2.join();

    // Example 3: Using lambda capture
    print_safe("\nExample 3: Using lambda capture");

    int multiplier = 10;

    // Capturing 'multiplier' by value is safe here.
    atom::async::EnhancedPackagedTask<std::vector<int>, const std::vector<int>&>
        task3([multiplier](const std::vector<int>& input) {
            print_safe("Thread [", get_thread_id(),
                       "] Multiplying each element in the input vector by ",
                       multiplier);
            std::vector<int> result;
            result.reserve(input.size());  // Good practice: reserve space
            for (int val : input) {
                result.push_back(val * multiplier);
            }
            return result;
        });

    auto future3 = task3.getEnhancedFuture();

    std::vector<int> input = {1, 2, 3, 4, 5};

    // Pass 'input' by const reference or value. Here it's captured implicitly.
    // Ensure 'input' lifetime is valid if passed by reference.
    std::thread t3([&task3, &input]() { task3(input); });

    std::vector<int> result3 = future3.get();

    // Lock for printing vectors
    {
        std::lock_guard<std::mutex> lock(print_mutex);
        std::cout << "Input vector: ";
        for (int val : input) {
            std::cout << val << " ";
        }
        std::cout << std::endl;

        std::cout << "Result vector (multiplied by 10): ";
        for (int val : result3) {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }

    t3.join();
}

// 3. Edge Cases and Special Situations Examples
void edge_cases_examples() {
    print_section("Edge Cases and Special Situations Examples");

    // Example 1: Checking task validity
    print_safe("Example 1: Checking task validity");

    atom::async::EnhancedPackagedTask<int> task1([]() { return 42; });
    print_safe("Is Task 1 valid: ", static_cast<bool>(task1) ? "Yes" : "No");

    // Move the task
    auto task1_moved = std::move(task1);
    print_safe("After move, is Task 1 valid: ",
               static_cast<bool>(task1) ? "Yes" : "No");
    print_safe("After move, is the new task valid: ",
               static_cast<bool>(task1_moved) ? "Yes" : "No");

    // Create an invalid task
    try {
        print_safe("Attempting to create an invalid task (nullptr)...");
        // Assuming the constructor throws for nullptr, based on original
        // comments
        atom::async::EnhancedPackagedTask<int> invalid_task(nullptr);
        print_safe(
            "Error: Creation with nullptr did not throw.");  // Should not reach
                                                             // here
    } catch (const atom::async::InvalidPackagedTaskException&
                 e) {  // Catch specific type if possible
        print_safe("Caught expected exception: ", e.what());
    } catch (const std::exception& e) {
        print_safe("Caught other exception: ", e.what());  // Fallback
    }

    // Example 2: Cancelling a task
    print_safe("\nExample 2: Cancelling a task");

    atom::async::EnhancedPackagedTask<int> task2([]() {
        print_safe("Thread [", get_thread_id(),
                   "] Executing task that might be cancelled");
        // Simulate work
        for (int i = 0; i < 5; ++i) {
            // Check for cancellation periodically if the task is long-running
            // if (isCancelled()) { // Assuming isCancelled() is accessible or
            // passed in
            //    print_safe("Task cancelled during execution.");
            //    throw std::runtime_error("Task cancelled"); // Or return early
            // }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        print_safe("Thread [", get_thread_id(),
                   "] Task finished work (potentially too late if cancelled)");
        return 100;
    });

    auto future2 = task2.getEnhancedFuture();

    // Cancel the task before starting the thread (or shortly after)
    bool was_cancelled = task2.cancel();
    print_safe("Task 2 cancellation successful? ",
               was_cancelled ? "Yes" : "No");
    print_safe("Task 2 current state: ",
               task2.isCancelled() ? "Cancelled" : "Not Cancelled");

    // Try cancelling again (should return false)
    bool second_cancel = task2.cancel();
    print_safe("Second cancellation of Task 2 successful? ",
               second_cancel ? "Yes" : "No");

    // Execute the (already cancelled) task
    std::thread t2([&task2]() {
        print_safe("Thread [", get_thread_id(),
                   "] Attempting to execute the cancelled task");
        task2();  // The task's operator() should handle the cancelled state
    });

    try {
        print_safe("Waiting for the cancelled task's result (should throw)...");
        int result = future2.get();                     // This should throw
        print_safe("Cancelled task result: ", result);  // Should not reach here
    } catch (const atom::async::InvalidPackagedTaskException& e) {
        print_safe("Caught expected exception: ", e.what());
    } catch (const std::future_error& e) {
        // std::future_error might be thrown if the promise has an exception set
        print_safe("Caught future_error: ", e.what(),
                   " code: ", e.code().message());
    } catch (const std::exception& e) {
        print_safe("Caught other exception: ", e.what());
    }

    t2.join();

    // Example 3: No-operation task
    print_safe("\nExample 3: No-operation task");

    atom::async::EnhancedPackagedTask<void> noop_task([]() {
        print_safe("Thread [", get_thread_id(), "] Executing no-op task");
        // Does nothing
    });

    auto noop_future = noop_task.getEnhancedFuture();

    std::thread t3([&noop_task]() { noop_task(); });

    noop_future.wait();  // Wait for the void task to complete
    print_safe("No-op task completed");

    t3.join();

    // Example 4: Checking future validity after task destruction
    print_safe("\nExample 4: Checking future validity after task destruction");
    // Use std::optional to hold the future, as EnhancedFuture might not be
    // default constructible
    std::optional<atom::async::EnhancedFuture<int>> future_from_temp;
    {
        atom::async::EnhancedPackagedTask<int> temp_task([]() { return 42; });
        future_from_temp =
            temp_task.getEnhancedFuture();  // Get the shared_future wrapper
        // Check validity using has_value() for std::optional
        print_safe("Inside scope: Is future valid? ",
                   future_from_temp.has_value() ? "Yes" : "No");
        // temp_task is destroyed here, but the underlying shared state of the
        // future persists
    }
    // Check validity using has_value() for std::optional
    print_safe("Outside scope: Is future still valid? ",
               future_from_temp.has_value() ? "Yes" : "No");
    // Note: You cannot *execute* the destroyed task, but the future remains
    // valid until the result (or exception) is set and retrieved, or the last
    // future object is destroyed. Trying to get() a future whose promise was
    // destroyed *before* setting a value/exception might lead to
    // std::future_error with std::future_errc::broken_promise.

    print_safe("Creating a new task to demonstrate normal future usage");
    atom::async::EnhancedPackagedTask<int> new_task([]() {
        print_safe("Thread [", get_thread_id(), "] Executing new task");
        return 84;
    });

    auto new_future = new_task.getEnhancedFuture();

    std::thread t4([&new_task]() { new_task(); });

    int result = new_future.get();
    print_safe("New task result: ", result);

    t4.join();
}

// 4. Error Handling Examples
void error_handling_examples() {
    print_section("Error Handling Examples");

    // Example 1: Exception thrown inside the task
    print_safe("Example 1: Exception thrown inside the task");

    atom::async::EnhancedPackagedTask<int> task1(
        []() -> int {  // Explicit return type for clarity
            print_safe("Thread [", get_thread_id(),
                       "] Executing task that will throw");
            throw std::runtime_error("Intentional exception from task");
            // return 42; // Unreachable
        });

    auto future1 = task1.getEnhancedFuture();

    std::thread t1([&task1]() {
        try {
            task1();
        } catch (...) {
            // The exception is caught internally and set on the promise
            print_safe("Thread [", get_thread_id(),
                       "] Task execution completed (exception set on promise)");
        }
    });

    try {
        print_safe("Waiting for result of task that might throw...");
        int result = future1.get();  // This should re-throw the exception
        print_safe("Task result: ", result);  // Unreachable
    } catch (const std::exception& e) {
        print_safe("Correctly caught exception via future: ", e.what());
    }

    t1.join();

    // Example 2: Division by zero error
    print_safe("\nExample 2: Division by zero error");

    atom::async::EnhancedPackagedTask<double, double, double> division_task(
        [](double a, double b) {
            print_safe("Thread [", get_thread_id(), "] Calculating ", a, " / ",
                       b);
            if (b == 0.0) {
                throw std::invalid_argument("Divisor cannot be zero");
            }
            return a / b;
        });

    auto div_future = division_task.getEnhancedFuture();

    std::thread t2([&division_task]() {
        try {
            division_task(10.0, 0.0);  // Intentionally pass zero as divisor
        } catch (...) {
            print_safe("Thread [", get_thread_id(),
                       "] Division task execution completed (exception set on "
                       "promise)");
        }
    });

    try {
        print_safe("Waiting for division result...");
        double result = div_future.get();         // Should re-throw
        print_safe("Division result: ", result);  // Unreachable
    } catch (const std::invalid_argument& e) {    // Catch specific type
        print_safe("Correctly caught division by zero exception: ", e.what());
    } catch (const std::exception& e) {
        print_safe("Caught other exception: ", e.what());
    }

    t2.join();

    // Example 3: Accessing a moved-from task
    print_safe("\nExample 3: Accessing a moved-from task");

    atom::async::EnhancedPackagedTask<int> original_task([]() { return 42; });
    print_safe("Original task valid before move: ",
               static_cast<bool>(original_task));

    // Move the task
    auto moved_task = std::move(original_task);
    print_safe("Original task valid after move: ",
               static_cast<bool>(original_task));
    print_safe("Moved task valid after move: ", static_cast<bool>(moved_task));

    try {
        print_safe("Attempting to get future from the moved-from task...");
        // Accessing a moved-from object leads to undefined behavior generally,
        // but the class might define specific state (e.g., nullptr task_).
        // The getEnhancedFuture() likely checks validity.
        auto invalid_future = original_task.getEnhancedFuture();
        print_safe(
            "Error: Getting future from moved task did not throw.");  // Should
                                                                      // not
                                                                      // reach
                                                                      // here
    } catch (const atom::async::InvalidPackagedTaskException& e) {
        print_safe("Correctly caught exception: ", e.what());
    } catch (const std::exception& e) {
        print_safe("Caught other exception: ", e.what());
    }

    // Example 4: Getting result from a cancelled task (revisited)
    print_safe("\nExample 4: Getting result from a cancelled task (revisited)");

    atom::async::EnhancedPackagedTask<std::string> cancel_task([]() {
        print_safe("Thread [", get_thread_id(),
                   "] Cancelled task's function (might run briefly)");
        std::this_thread::sleep_for(
            std::chrono::milliseconds(50));  // Short delay
        return "Should not return successfully";
    });

    auto cancel_future = cancel_task.getEnhancedFuture();

    // Cancel the task and capture the nodiscard return value
    [[maybe_unused]] bool cancelled = cancel_task.cancel();
    print_safe("Task cancelled state: ", cancel_task.isCancelled());

    std::thread t4([&cancel_task]() {
        print_safe("Thread [", get_thread_id(),
                   "] Executing the cancelled task...");
        cancel_task();  // operator() should set exception on promise
    });

    try {
        print_safe("Waiting for the cancelled task's result...");
        std::string result = cancel_future.get();  // Should throw
        print_safe("Task result: ", result);       // Unreachable
    } catch (const atom::async::InvalidPackagedTaskException& e) {
        print_safe("Correctly caught cancellation exception via future: ",
                   e.what());
    } catch (const std::future_error& e) {
        print_safe("Caught future_error: ", e.what(),
                   " code: ", e.code().message());
    } catch (const std::exception& e) {
        print_safe("Caught other exception: ", e.what());
    }

    t4.join();
}

// 5. Callback Function Examples
void callback_examples() {
    print_section("Callback Function Examples");

    // Only compile this section if ATOM_USE_LOCKFREE_QUEUE is defined
#ifdef ATOM_USE_LOCKFREE_QUEUE
    print_safe("Example: Using callback functions");

    // Create a task with callbacks
    CallbackCounter counter;

    atom::async::EnhancedPackagedTask<int> task([]() {
        print_safe("Thread [", get_thread_id(),
                   "] Executing task with callbacks");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return 100;
    });

    // Register a callback
    task.onComplete([&counter](int result) {
        print_safe("Callback received result: ", result);
        counter.increment_with_value(result);
    });

    // Add another callback
    task.onComplete(
        [&counter](int /*result*/) {  // Parameter name optional if unused
            print_safe("Another callback executing.");
            counter.increment();
        });

    auto future = task.getEnhancedFuture();

    std::thread t([&task]() {
        task();  // Execute the task, which will trigger callbacks upon
                 // completion
    });

    int result = future.get();  // Wait for the task to complete
    print_safe("Task result obtained via future: ", result);

    // Wait a bit more to ensure callbacks (which run after promise is set) have
    // executed In a real scenario, better synchronization might be needed if
    // the main thread depends on callback side effects immediately after get().
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    print_safe("Number of callback executions: ", counter.get_count());
    assert(counter.get_count() >= 2);  // Check if callbacks ran

    t.join();
#else
    print_safe(
        "Callback functionality requires the ATOM_USE_LOCKFREE_QUEUE compile "
        "flag to be defined.");
#endif
}

// 6. Performance Test Example
void performance_examples() {
    print_section("Performance Test Example");

    print_safe("Performance test executing a large number of tasks");

    constexpr int NUM_TASKS = 1000;
    // Use unique_ptr if tasks are large or non-copyable/movable easily
    std::vector<atom::async::EnhancedPackagedTask<int>> tasks;
    std::vector<std::thread> threads;
    std::vector<atom::async::EnhancedFuture<int>> futures;
    tasks.reserve(NUM_TASKS);
    threads.reserve(NUM_TASKS);
    futures.reserve(NUM_TASKS);

    // Prepare tasks
    print_safe("Preparing ", NUM_TASKS, " tasks...");
    for (int i = 0; i < NUM_TASKS; ++i) {
        tasks.emplace_back([i]() {
            // Simple calculation
            long long sum = 0;  // Use larger type for sum
            for (int j = 0; j <= i % 100;
                 ++j) {  // Limit inner loop for faster test
                sum += j;
            }
            // Simulate some work without blocking
            if (sum == -1)
                throw std::runtime_error(
                    "Simulated error");           // Avoid optimizing out
            return static_cast<int>(sum % 1000);  // Return a smaller int
        });

        futures.push_back(tasks.back().getEnhancedFuture());
    }

    // Start timing
    print_safe("Starting threads and executing tasks...");
    auto start = std::chrono::high_resolution_clock::now();

    // Start all threads
    for (int i = 0; i < NUM_TASKS; ++i) {
        // Capture 'i' by value for the lambda
        threads.emplace_back([i, &tasks]() {
            try {
                tasks[i]();
            } catch (...) {
                // Handle potential exceptions during task execution if needed
            }
        });
    }

    // Wait for all tasks to complete and collect results
    print_safe("Waiting for tasks to complete...");
    long long total = 0;  // Use larger type for total
    int completed_count = 0;
    int failed_count = 0;
    for (auto& future : futures) {
        try {
            total += future.get();
            completed_count++;
        } catch (const std::exception& e) {
            // print_safe("Task failed with exception: ", e.what());
            failed_count++;
        }
    }

    // End timing
    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    print_safe("Executed ", completed_count, " tasks successfully (",
               failed_count, " failed) in: ", duration.count(), " ms");
    if (completed_count > 0) {
        print_safe("Sum of results: ", total);
    }

    // Wait for all threads to finish
    print_safe("Joining threads...");
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    print_safe("All threads joined.");
}

int main() {
    try {
        std::cout << "====== EnhancedPackagedTask Usage Examples ======"
                  << std::endl;

        basic_usage_examples();
        parameter_combination_examples();
        edge_cases_examples();
        error_handling_examples();
        callback_examples();
        performance_examples();

        std::cout << "\n====== All Examples Completed ======" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in main: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown unhandled exception in main." << std::endl;
        return 1;
    }

    return 0;
}
