/**
 * @file ring_buffer_example.cpp
 * @brief Comprehensive examples of using the RingBuffer class
 * @author Example Author
 * @date 2025-03-23
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "atom/memory/ring.hpp"

// Helper function to print section titles
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

// Helper function to print buffer contents
template <typename T>
void printBuffer(const atom::memory::RingBuffer<T>& buffer,
                 const std::string& label = "Buffer contents") {
    std::cout << label << " (size " << buffer.size() << "/" << buffer.capacity()
              << "): ";
    auto contents = buffer.view();

    if (contents.empty()) {
        std::cout << "[empty]";
    } else {
        std::cout << "[ ";
        for (const auto& item : contents) {
            std::cout << item << " ";
        }
        std::cout << "]";
    }
    std::cout << std::endl;
}

// A sample class to demonstrate using RingBuffer with complex types
class SensorReading {
private:
    int id_;
    double value_;
    std::chrono::system_clock::time_point timestamp_;

public:
    SensorReading()
        : id_(0), value_(0.0), timestamp_(std::chrono::system_clock::now()) {}

    SensorReading(int id, double value)
        : id_(id),
          value_(value),
          timestamp_(std::chrono::system_clock::now()) {}

    int getId() const { return id_; }
    double getValue() const { return value_; }
    std::chrono::system_clock::time_point getTimestamp() const {
        return timestamp_;
    }

    std::string getTimeString() const {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp_);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
        return ss.str();
    }

    // Comparison operator for contains() and other operations
    bool operator==(const SensorReading& other) const {
        return id_ == other.id_ &&
               std::abs(value_ - other.value_) <
                   0.001;  // Handle floating point comparison
    }

    // String representation for printing
    friend std::ostream& operator<<(std::ostream& os,
                                    const SensorReading& reading) {
        os << "Reading{id=" << reading.id_ << ", value=" << std::fixed
           << std::setprecision(2) << reading.value_
           << ", time=" << reading.getTimeString() << "}";
        return os;
    }
};

// Function to simulate sensor data collection
std::vector<SensorReading> collectSensorData(int count, int start_id = 0) {
    std::vector<SensorReading> readings;
    readings.reserve(count);

    // Random number generation for simulated sensor values
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(10.0, 30.0);

    for (int i = 0; i < count; ++i) {
        readings.emplace_back(start_id + i, dis(gen));
        std::this_thread::sleep_for(
            std::chrono::milliseconds(5));  // Small delay for unique timestamps
    }

    return readings;
}

// Define a simple log entry class
class LogEntry {
public:
    enum class Level { DEBUG, INFO, WARNING, ERROR, CRITICAL };

    LogEntry(Level level, std::string message)
        : level_(level),
          message_(std::move(message)),
          timestamp_(std::chrono::system_clock::now()) {}

    std::string getLevelString() const {
        switch (level_) {
            case Level::DEBUG:
                return "DEBUG";
            case Level::INFO:
                return "INFO";
            case Level::WARNING:
                return "WARNING";
            case Level::ERROR:
                return "ERROR";
            case Level::CRITICAL:
                return "CRITICAL";
            default:
                return "UNKNOWN";
        }
    }

    std::string getTimeString() const {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp_);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    Level getLevel() const { return level_; }
    const std::string& getMessage() const { return message_; }

    friend std::ostream& operator<<(std::ostream& os, const LogEntry& entry) {
        os << "[" << entry.getTimeString() << "] " << std::setw(8) << std::left
           << entry.getLevelString() << " " << entry.getMessage();
        return os;
    }

private:
    Level level_;
    std::string message_;
    std::chrono::system_clock::time_point timestamp_;
};

int main() {
    std::cout << "RING BUFFER COMPREHENSIVE EXAMPLES\n";
    std::cout << "==================================\n";

    //--------------------------------------------------------------------------
    // 1. Basic Operations
    //--------------------------------------------------------------------------
    printSection("1. Basic Operations");

    // Create a ring buffer for integers with capacity 5
    atom::memory::RingBuffer<int> intBuffer(5);
    std::cout << "Created integer buffer with capacity: "
              << intBuffer.capacity() << std::endl;

    // Push items to the buffer
    std::cout << "\nPushing items to buffer..." << std::endl;
    for (int i = 1; i <= 5; ++i) {
        bool success = intBuffer.push(i);
        std::cout << "Pushed " << i << ": " << (success ? "success" : "failed")
                  << std::endl;
    }

    printBuffer(intBuffer);

    // Try to push to a full buffer
    std::cout << "\nTrying to push to full buffer..." << std::endl;
    bool success = intBuffer.push(6);
    std::cout << "Pushed 6: " << (success ? "success" : "failed") << std::endl;

    // Pop items from the buffer
    std::cout << "\nPopping items from buffer..." << std::endl;
    for (int i = 0; i < 3; ++i) {
        auto item = intBuffer.pop();
        if (item) {
            std::cout << "Popped: " << *item << std::endl;
        } else {
            std::cout << "Buffer empty, couldn't pop" << std::endl;
        }
    }

    printBuffer(intBuffer);

    // Check if buffer is full or empty
    std::cout << "\nBuffer status:" << std::endl;
    std::cout << "Is full: " << (intBuffer.full() ? "yes" : "no") << std::endl;
    std::cout << "Is empty: " << (intBuffer.empty() ? "yes" : "no")
              << std::endl;
    std::cout << "Current size: " << intBuffer.size() << std::endl;

    // Push some more items
    std::cout << "\nPushing more items..." << std::endl;
    for (int i = 6; i <= 8; ++i) {
        bool success = intBuffer.push(i);
        std::cout << "Pushed " << i << ": " << (success ? "success" : "failed")
                  << std::endl;
    }

    printBuffer(intBuffer);

    //--------------------------------------------------------------------------
    // 2. Push Overwrite Functionality
    //--------------------------------------------------------------------------
    printSection("2. Push Overwrite Functionality");

    // Create a new buffer for demonstration
    atom::memory::RingBuffer<int> overwriteBuffer(3);

    // Fill the buffer
    std::cout << "Filling buffer with [1,2,3]..." << std::endl;
    for (int i = 1; i <= 3; ++i) {
        overwriteBuffer.push(i);
    }

    printBuffer(overwriteBuffer, "Initial buffer");

    // Demonstrate pushOverwrite
    std::cout << "\nPushing 4, 5, 6 with overwrite..." << std::endl;
    for (int i = 4; i <= 6; ++i) {
        overwriteBuffer.pushOverwrite(i);
        printBuffer(overwriteBuffer, "After pushing " + std::to_string(i));
    }

    //--------------------------------------------------------------------------
    // 3. Advanced Inspection (front, back, at)
    //--------------------------------------------------------------------------
    printSection("3. Advanced Inspection (front, back, at)");

    // Create and fill a buffer for testing
    atom::memory::RingBuffer<std::string> stringBuffer(5);
    std::vector<std::string> fruits = {"Apple", "Banana", "Cherry", "Date",
                                       "Elderberry"};

    std::cout << "Filling string buffer..." << std::endl;
    for (const auto& fruit : fruits) {
        stringBuffer.push(fruit);
    }

    printBuffer(stringBuffer);

    // Demonstrate front(), back(), at()
    auto frontItem = stringBuffer.front();
    auto backItem = stringBuffer.back();

    std::cout << "\nInspecting buffer:" << std::endl;
    std::cout << "Front item: " << (frontItem ? *frontItem : "none")
              << std::endl;
    std::cout << "Back item: " << (backItem ? *backItem : "none") << std::endl;

    std::cout << "\nAccessing items by index:" << std::endl;
    for (size_t i = 0; i < stringBuffer.size() + 1; ++i) {
        auto item = stringBuffer.at(i);
        std::cout << "Item at index " << i << ": ";
        if (item) {
            std::cout << *item << std::endl;
        } else {
            std::cout << "out of bounds" << std::endl;
        }
    }

    // Check if buffer contains specific items
    std::cout << "\nChecking for items:" << std::endl;
    std::cout << "Contains 'Cherry': "
              << (stringBuffer.contains("Cherry") ? "yes" : "no") << std::endl;
    std::cout << "Contains 'Fig': "
              << (stringBuffer.contains("Fig") ? "yes" : "no") << std::endl;

    //--------------------------------------------------------------------------
    // 4. Iterators and Range-based For Loops
    //--------------------------------------------------------------------------
    printSection("4. Iterators and Range-based For Loops");

    // Create a buffer with numeric sequence
    atom::memory::RingBuffer<int> sequenceBuffer(10);
    for (int i = 1; i <= 5; ++i) {
        sequenceBuffer.push(i * 10);
    }

    printBuffer(sequenceBuffer, "Sequence buffer");

    // Demonstrate iterator usage
    std::cout << "\nIterating using explicit iterators:" << std::endl;
    for (auto it = sequenceBuffer.begin(); it != sequenceBuffer.end(); ++it) {
        std::cout << *it << " ";
    }
    std::cout << std::endl;

    // Demonstrate range-based for loop
    std::cout << "\nIterating using range-based for loop:" << std::endl;
    for (const auto& value : sequenceBuffer) {
        std::cout << value << " ";
    }
    std::cout << std::endl;

    // Calculate sum using iterators
    int sum = std::accumulate(sequenceBuffer.begin(), sequenceBuffer.end(), 0);
    std::cout << "\nSum of all elements: " << sum << std::endl;

    //--------------------------------------------------------------------------
    // 5. Buffer Manipulation (clear, resize, rotate)
    //--------------------------------------------------------------------------
    printSection("5. Buffer Manipulation (clear, resize, rotate)");

    // Start with a partially filled buffer
    atom::memory::RingBuffer<char> charBuffer(5);
    for (char c = 'A'; c <= 'C'; ++c) {
        charBuffer.push(c);
    }

    printBuffer(charBuffer, "Initial char buffer");

    // Demonstrate clear
    std::cout << "\nClearing buffer..." << std::endl;
    charBuffer.clear();
    printBuffer(charBuffer, "After clear");

    // Refill the buffer
    std::cout << "\nRefilling buffer with A through E..." << std::endl;
    for (char c = 'A'; c <= 'E'; ++c) {
        charBuffer.push(c);
    }
    printBuffer(charBuffer, "Refilled buffer");

    // Demonstrate resize
    std::cout << "\nResizing buffer to capacity 8..." << std::endl;
    charBuffer.resize(8);
    std::cout << "New capacity: " << charBuffer.capacity() << std::endl;
    printBuffer(charBuffer, "After resize");

    // Add more items to show the increased capacity
    std::cout << "\nAdding more items to show increased capacity..."
              << std::endl;
    for (char c = 'F'; c <= 'H'; ++c) {
        charBuffer.push(c);
    }
    printBuffer(charBuffer, "After adding more items");

    // Demonstrate rotate
    std::cout << "\nRotating buffer by 2 positions (left)..." << std::endl;
    charBuffer.rotate(2);
    printBuffer(charBuffer, "After rotating left");

    std::cout << "\nRotating buffer by -3 positions (right)..." << std::endl;
    charBuffer.rotate(-3);
    printBuffer(charBuffer, "After rotating right");

    //--------------------------------------------------------------------------
    // 6. Higher-order Functions (forEach, removeIf)
    //--------------------------------------------------------------------------
    printSection("6. Higher-order Functions (forEach, removeIf)");

    // Create a buffer of integers
    atom::memory::RingBuffer<int> numberBuffer(10);
    for (int i = 1; i <= 10; ++i) {
        numberBuffer.push(i);
    }

    printBuffer(numberBuffer, "Initial number buffer");

    // Demonstrate forEach
    std::cout << "\nDoubling each value with forEach..." << std::endl;
    numberBuffer.forEach([](int& value) { value *= 2; });

    printBuffer(numberBuffer, "After doubling");

    // Demonstrate removeIf
    std::cout << "\nRemoving odd numbers with removeIf..." << std::endl;
    numberBuffer.removeIf([](int value) { return value % 2 != 0; });

    printBuffer(numberBuffer, "After removing odds");

    // Demonstrate chaining operations
    std::cout << "\nAdding 5 to each value and removing values > 15..."
              << std::endl;
    numberBuffer.forEach([](int& value) { value += 5; });

    numberBuffer.removeIf([](int value) { return value > 15; });

    printBuffer(numberBuffer, "After chained operations");

    //--------------------------------------------------------------------------
    // 7. Complex Types and Custom Classes
    //--------------------------------------------------------------------------
    printSection("7. Complex Types and Custom Classes");

    // Create a buffer for sensor readings
    atom::memory::RingBuffer<SensorReading> sensorBuffer(10);

    // Collect some simulated sensor data
    std::cout << "Collecting sensor readings..." << std::endl;
    auto sensorData = collectSensorData(5);

    // Push readings to the buffer
    for (const auto& reading : sensorData) {
        sensorBuffer.push(reading);
        std::cout << "Added: " << reading << std::endl;
    }

    std::cout << "\nBuffer size: " << sensorBuffer.size() << "/"
              << sensorBuffer.capacity() << std::endl;

    // Check for a specific reading
    SensorReading targetReading(2, sensorData[2].getValue());
    bool contains = sensorBuffer.contains(targetReading);
    std::cout << "Buffer contains reading with ID 2: "
              << (contains ? "yes" : "no") << std::endl;

    // Process readings with forEach
    std::cout << "\nProcessing readings (calculating average)..." << std::endl;
    double sum_values = 0.0;
    size_t count = 0;

    sensorBuffer.forEach([&sum_values, &count](SensorReading& reading) {
        sum_values += reading.getValue();
        count++;
    });

    double average = count > 0 ? sum_values / count : 0.0;
    std::cout << "Average sensor value: " << std::fixed << std::setprecision(2)
              << average << std::endl;

    // Filter out readings with values below average
    std::cout << "\nFiltering out readings below average..." << std::endl;
    sensorBuffer.removeIf([average](const SensorReading& reading) {
        return reading.getValue() < average;
    });

    std::cout << "Remaining readings:" << std::endl;
    for (const auto& reading : sensorBuffer) {
        std::cout << "  " << reading << std::endl;
    }

    //--------------------------------------------------------------------------
    // 8. Thread Safety and Concurrent Access
    //--------------------------------------------------------------------------
    printSection("8. Thread Safety and Concurrent Access");

    // Create a shared buffer for testing concurrency
    atom::memory::RingBuffer<int> sharedBuffer(100);
    std::atomic<bool> done(false);
    std::atomic<int> producedCount(0);
    std::atomic<int> consumedCount(0);

    std::cout << "Starting producer-consumer test with 3 threads..."
              << std::endl;

    // Producer thread
    std::thread producer([&sharedBuffer, &done, &producedCount]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> delay_dist(1, 10);

        for (int i = 1; i <= 500; ++i) {
            bool success = sharedBuffer.push(i);
            if (success) {
                producedCount++;
            }

            // Small random delay to allow consumer to catch up sometimes
            std::this_thread::sleep_for(
                std::chrono::milliseconds(delay_dist(gen)));
        }

        // Signal that production is complete
        done = true;
    });

    // Two consumer threads
    std::thread consumer1([&sharedBuffer, &done, &consumedCount]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> delay_dist(5, 15);

        while (!done || !sharedBuffer.empty()) {
            auto item = sharedBuffer.pop();
            if (item) {
                consumedCount++;
            }

            // Slightly longer delay for consumers
            std::this_thread::sleep_for(
                std::chrono::milliseconds(delay_dist(gen)));
        }
    });

    std::thread consumer2([&sharedBuffer, &done, &consumedCount]() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> delay_dist(5, 15);

        while (!done || !sharedBuffer.empty()) {
            auto item = sharedBuffer.pop();
            if (item) {
                consumedCount++;
            }

            // Slightly longer delay for consumers
            std::this_thread::sleep_for(
                std::chrono::milliseconds(delay_dist(gen)));
        }
    });

    // Progress reporting thread
    std::thread reporter(
        [&sharedBuffer, &done, &producedCount, &consumedCount]() {
            while (!done || !sharedBuffer.empty()) {
                std::cout << "Status: produced=" << producedCount
                          << ", consumed=" << consumedCount
                          << ", buffer size=" << sharedBuffer.size() << "/"
                          << sharedBuffer.capacity() << std::endl;

                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        });

    // Wait for all threads to complete
    producer.join();
    consumer1.join();
    consumer2.join();
    reporter.join();

    std::cout << "\nConcurrent test complete!" << std::endl;
    std::cout << "Final stats: produced=" << producedCount
              << ", consumed=" << consumedCount
              << ", remaining=" << sharedBuffer.size() << std::endl;

    //--------------------------------------------------------------------------
    // 9. Performance Demonstration
    //--------------------------------------------------------------------------
    printSection("9. Performance Demonstration");

    constexpr size_t BUFFER_SIZE = 10000;
    constexpr int NUM_OPERATIONS = 100000;

    // Create a large buffer
    std::cout << "Creating buffer with capacity " << BUFFER_SIZE << "..."
              << std::endl;
    atom::memory::RingBuffer<int> perfBuffer(BUFFER_SIZE);

    // Measure push performance
    std::cout << "\nMeasuring push performance..." << std::endl;
    auto push_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        perfBuffer.push(i);

        // Keep buffer at half capacity by popping occasionally
        if (i % 2 == 0 && !perfBuffer.empty()) {
            perfBuffer.pop();
        }
    }

    auto push_end = std::chrono::high_resolution_clock::now();
    auto push_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        push_end - push_start);

    std::cout << "Completed " << NUM_OPERATIONS << " push operations in "
              << push_duration.count() << " microseconds" << std::endl;
    std::cout << "Average time per operation: "
              << static_cast<double>(push_duration.count()) / NUM_OPERATIONS
              << " microseconds" << std::endl;

    // Clear the buffer for the next test
    perfBuffer.clear();

    // Measure pushOverwrite performance
    std::cout << "\nMeasuring pushOverwrite performance..." << std::endl;
    auto overwrite_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_OPERATIONS; ++i) {
        perfBuffer.pushOverwrite(i);
    }

    auto overwrite_end = std::chrono::high_resolution_clock::now();
    auto overwrite_duration =
        std::chrono::duration_cast<std::chrono::microseconds>(overwrite_end -
                                                              overwrite_start);

    std::cout << "Completed " << NUM_OPERATIONS
              << " pushOverwrite operations in " << overwrite_duration.count()
              << " microseconds" << std::endl;
    std::cout << "Average time per operation: "
              << static_cast<double>(overwrite_duration.count()) /
                     NUM_OPERATIONS
              << " microseconds" << std::endl;

    //--------------------------------------------------------------------------
    // 10. Real-World Application: Logging System
    //--------------------------------------------------------------------------
    printSection("10. Real-World Application: Logging System");

    // Create a ring buffer as a circular log
    atom::memory::RingBuffer<LogEntry> logBuffer(
        100);  // Keep last 100 log entries

    // Simulate logging from multiple sources
    std::cout << "Simulating logging activity..." << std::endl;

    // Log messages from system startup
    logBuffer.push(LogEntry(LogEntry::Level::INFO, "System initializing"));
    logBuffer.push(LogEntry(LogEntry::Level::INFO, "Loading configuration"));
    logBuffer.push(LogEntry(LogEntry::Level::DEBUG,
                            "Config loaded from /etc/app/config.json"));
    logBuffer.push(
        LogEntry(LogEntry::Level::INFO, "Starting network services"));
    logBuffer.push(LogEntry(LogEntry::Level::WARNING,
                            "Firewall rules not optimally configured"));
    logBuffer.push(
        LogEntry(LogEntry::Level::INFO, "Database connection established"));

    // Simulate an error condition
    logBuffer.push(
        LogEntry(LogEntry::Level::WARNING, "High memory usage detected (85%)"));
    logBuffer.push(LogEntry(LogEntry::Level::ERROR,
                            "Failed to connect to backup service"));
    logBuffer.push(LogEntry(LogEntry::Level::DEBUG,
                            "Connection attempt timed out after 30s"));
    logBuffer.push(LogEntry(LogEntry::Level::CRITICAL,
                            "Primary storage cluster unreachable"));
    logBuffer.push(
        LogEntry(LogEntry::Level::INFO, "Switching to backup storage"));
    logBuffer.push(
        LogEntry(LogEntry::Level::INFO, "Recovery procedure initiated"));

    // Display all logs
    std::cout << "\nComplete log history:" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (const auto& entry : logBuffer) {
        std::cout << entry << std::endl;
    }

    // Filter to show only warnings and above
    std::cout << "\nFiltering for WARNING level and above:" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (const auto& entry : logBuffer) {
        if (static_cast<int>(entry.getLevel()) >=
            static_cast<int>(LogEntry::Level::WARNING)) {
            std::cout << entry << std::endl;
        }
    }

    // Search for specific text in logs
    std::string searchTerm = "connection";
    std::cout << "\nSearching logs for term: '" << searchTerm << "'"
              << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (const auto& entry : logBuffer) {
        if (entry.getMessage().find(searchTerm) != std::string::npos) {
            std::cout << entry << std::endl;
        }
    }

    std::cout << "\nAll RingBuffer examples completed successfully!\n";
    return 0;
}