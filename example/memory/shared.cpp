/**
 * @file shared_memory_example.cpp
 * @brief Comprehensive examples of using SharedMemory class
 * @author Example Author
 * @date 2025-03-23
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "atom/memory/shared.hpp"

// Helper function to print section titles
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

// Sample struct for shared memory usage
struct SensorData {
    int id;
    double temperature;
    double humidity;
    double pressure;
    char location[64];
    uint64_t timestamp;
    bool active;

    // Helper method to print the sensor data
    void print() const {
        std::cout << "Sensor ID: " << id << std::endl;
        std::cout << "Location: " << location << std::endl;
        std::cout << "Temperature: " << std::fixed << std::setprecision(2)
                  << temperature << " °C" << std::endl;
        std::cout << "Humidity: " << std::fixed << std::setprecision(2)
                  << humidity << " %" << std::endl;
        std::cout << "Pressure: " << std::fixed << std::setprecision(2)
                  << pressure << " hPa" << std::endl;
        std::cout << "Timestamp: " << timestamp << std::endl;
        std::cout << "Active: " << (active ? "Yes" : "No") << std::endl;
    }

    // Helper method to get a formatted timestamp string
    std::string formatTimestamp() const {
        auto time = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(timestamp));
        auto time_t = std::chrono::system_clock::to_time_t(time);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    // Helper method to set current timestamp
    void setCurrentTimestamp() {
        timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
    }
};

// Ensure the struct is trivially copyable
static_assert(std::is_trivially_copyable_v<SensorData>,
              "SensorData must be trivially copyable");

// Smaller struct for partial read/write examples
struct SensorStatus {
    bool active;
    uint64_t last_update;
};

// Ensure the struct is trivially copyable
static_assert(std::is_trivially_copyable_v<SensorStatus>,
              "SensorStatus must be trivially copyable");

// Generate random sensor data
SensorData generateRandomSensorData(int id) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> temp_dist(15.0, 35.0);
    static std::uniform_real_distribution<> humidity_dist(30.0, 90.0);
    static std::uniform_real_distribution<> pressure_dist(990.0, 1030.0);

    static const std::vector<std::string> locations = {
        "Living Room", "Kitchen", "Bedroom",  "Office",  "Garage",
        "Basement",    "Attic",   "Bathroom", "Hallway", "Garden"};

    SensorData data;
    data.id = id;
    data.temperature = temp_dist(gen);
    data.humidity = humidity_dist(gen);
    data.pressure = pressure_dist(gen);

    // Choose a random location
    std::string loc = locations[gen() % locations.size()];
    std::strncpy(data.location, loc.c_str(), sizeof(data.location) - 1);
    data.location[sizeof(data.location) - 1] = '\0';  // Ensure null termination

    data.setCurrentTimestamp();
    data.active = true;

    return data;
}

int main() {
    std::cout << "SHARED MEMORY COMPREHENSIVE EXAMPLES\n";
    std::cout << "===================================\n";

    // Define shared memory names
    const std::string shm_name = "example_sensor_data";
    const std::string producer_name = "producer_sensor_data";
    const std::string consumer_name = "consumer_sensor_data";

    //--------------------------------------------------------------------------
    // 1. Basic Creation and Usage
    //--------------------------------------------------------------------------
    printSection("1. Basic Creation and Usage");

    // Check if shared memory exists and remove it if it does (cleanup)
    if (atom::connection::SharedMemory<SensorData>::exists(shm_name)) {
        std::cout << "Shared memory '" << shm_name
                  << "' already exists from a previous run.\n";
        std::cout << "Please note: You might need to manually remove it using "
                     "system commands\n";
        std::cout
            << "if the previous process crashed or didn't clean up properly.\n";

#ifndef _WIN32
        std::cout << "On Linux, try: 'rm /dev/shm/" << shm_name << "*'\n";
#endif
    }

    // Create a shared memory segment with initial data
    std::cout << "\nCreating shared memory with initial data...\n";
    SensorData initialData = generateRandomSensorData(1);
    std::optional<SensorData> initialDataOpt = initialData;

    try {
        atom::connection::SharedMemory<SensorData> sharedMemory(shm_name, true,
                                                                initialDataOpt);

        std::cout << "Shared memory created successfully.\n";
        std::cout << "Name: " << sharedMemory.getName() << std::endl;
        std::cout << "Size: " << sharedMemory.getSize() << " bytes\n";
        std::cout << "Version: " << sharedMemory.getVersion() << std::endl;
        std::cout << "Is creator: " << (sharedMemory.isCreator() ? "Yes" : "No")
                  << std::endl;
        std::cout << "Is initialized: "
                  << (sharedMemory.isInitialized() ? "Yes" : "No") << std::endl;

        // Read data from shared memory
        SensorData readData = sharedMemory.read();
        std::cout << "\nRead data from shared memory:\n";
        readData.print();

        // Update data in shared memory
        std::cout << "\nUpdating data in shared memory...\n";
        SensorData newData = generateRandomSensorData(2);
        sharedMemory.write(newData);

        // Read updated data
        SensorData updatedData = sharedMemory.read();
        std::cout << "\nRead updated data from shared memory:\n";
        updatedData.print();

        // Clear shared memory
        std::cout << "\nClearing shared memory...\n";
        sharedMemory.clear();

        // Try reading cleared memory (should throw exception)
        try {
            SensorData clearedData = sharedMemory.read();
            std::cout << "Cleared data was unexpectedly read:\n";
            clearedData.print();
        } catch (const atom::connection::SharedMemoryException& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
            std::cout << "Error code: " << e.getErrorCodeString() << std::endl;
        }

        // Try reading safely with tryRead
        std::cout << "\nTrying to read safely with tryRead()...\n";
        auto optData = sharedMemory.tryRead();
        if (optData) {
            std::cout << "Data read successfully (unexpected):\n";
            optData->print();
        } else {
            std::cout << "No data available (expected after clear)\n";
        }

    } catch (const atom::connection::SharedMemoryException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Error code: " << e.getErrorCodeString() << std::endl;
        return 1;
    }

    //--------------------------------------------------------------------------
    // 2. Opening Existing Shared Memory
    //--------------------------------------------------------------------------
    printSection("2. Opening Existing Shared Memory");

    try {
        // Create shared memory as one process
        std::cout << "Creating shared memory as 'creator' process...\n";
        atom::connection::SharedMemory<SensorData> creator(shm_name, true);

        // Write some data
        SensorData data = generateRandomSensorData(3);
        creator.write(data);
        std::cout << "Data written by creator:\n";
        data.print();

        // Open the same shared memory as another process
        std::cout
            << "\nOpening the same shared memory as 'consumer' process...\n";
        atom::connection::SharedMemory<SensorData> consumer(shm_name, false);

        std::cout << "Consumer shared memory info:\n";
        std::cout << "Name: " << consumer.getName() << std::endl;
        std::cout << "Size: " << consumer.getSize() << " bytes\n";
        std::cout << "Version: " << consumer.getVersion() << std::endl;
        std::cout << "Is creator: " << (consumer.isCreator() ? "Yes" : "No")
                  << std::endl;
        std::cout << "Is initialized: "
                  << (consumer.isInitialized() ? "Yes" : "No") << std::endl;

        // Read data from consumer
        SensorData readData = consumer.read();
        std::cout << "\nData read by consumer:\n";
        readData.print();

        // Modify data through consumer
        std::cout << "\nModifying data through consumer...\n";
        readData.temperature += 5.0;
        readData.setCurrentTimestamp();
        consumer.write(readData);

        // Read modified data from creator
        SensorData modifiedData = creator.read();
        std::cout << "\nModified data read by creator:\n";
        modifiedData.print();

    } catch (const atom::connection::SharedMemoryException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Error code: " << e.getErrorCodeString() << std::endl;
    }

    //--------------------------------------------------------------------------
    // 3. Error Handling and Timeouts
    //--------------------------------------------------------------------------
    printSection("3. Error Handling and Timeouts");

    try {
        // Try to open non-existent shared memory
        std::cout << "Trying to open non-existent shared memory...\n";
        atom::connection::SharedMemory<SensorData> nonExistent(
            "non_existent_memory", false);

        std::cout << "This line should not be reached\n";

    } catch (const atom::connection::SharedMemoryException& e) {
        std::cout << "Expected exception caught: " << e.what() << std::endl;
        std::cout << "Error code: " << e.getErrorCodeString() << std::endl;
    }

    try {
        // Create shared memory with timeout
        atom::connection::SharedMemory<SensorData> sharedMemory(shm_name, true);

        // Simulate a scenario where the memory might be locked
        std::cout << "\nDemonstrating timeout functionality...\n";
        std::cout << "Reading with a 500ms timeout...\n";

        auto start = std::chrono::high_resolution_clock::now();
        try {
            SensorData data = sharedMemory.read(std::chrono::milliseconds(500));
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = end - start;

            std::cout << "Read successful, took " << elapsed.count() << " ms\n";
            std::cout << "Data read:\n";
            data.print();
        } catch (const atom::connection::SharedMemoryException& e) {
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = end - start;

            std::cout << "Exception after " << elapsed.count()
                      << " ms: " << e.what() << std::endl;
            std::cout << "Error code: " << e.getErrorCodeString() << std::endl;
        }

    } catch (const atom::connection::SharedMemoryException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Error code: " << e.getErrorCodeString() << std::endl;
    }

    //--------------------------------------------------------------------------
    // 4. Partial Read/Write
    //--------------------------------------------------------------------------
    printSection("4. Partial Read/Write");

    try {
        // Create shared memory
        atom::connection::SharedMemory<SensorData> sharedMemory(shm_name, true);

        // Write initial data
        SensorData fullData = generateRandomSensorData(4);
        sharedMemory.write(fullData);

        std::cout << "Initial full data written:\n";
        fullData.print();

        // Calculate offset for the active flag within SensorData
        constexpr size_t active_offset = offsetof(SensorData, active);
        std::cout << "\nOffset of 'active' field: " << active_offset
                  << " bytes\n";

        // Read just the active flag using partial read
        bool isActive = sharedMemory.readPartial<bool>(active_offset);
        std::cout << "Partial read of active flag: "
                  << (isActive ? "Active" : "Inactive") << std::endl;

        // Update just the active flag using partial write
        bool newActiveState = false;
        std::cout << "\nUpdating active flag to: "
                  << (newActiveState ? "Active" : "Inactive") << std::endl;
        sharedMemory.writePartial(newActiveState, active_offset);

        // Read the full data to verify the partial update
        SensorData updatedData = sharedMemory.read();
        std::cout << "\nFull data after partial update:\n";
        updatedData.print();

        // Demonstrate partial read/write with a nested struct
        SensorStatus status;
        status.active = true;
        status.last_update =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();

        std::cout << "\nWriting SensorStatus struct partially...\n";
        std::cout << "Status active: " << (status.active ? "Yes" : "No")
                  << std::endl;
        std::cout << "Status last update: " << status.last_update << std::endl;

        // Write partial status (this will write to active and timestamp fields)
        sharedMemory.writePartial(status, active_offset);

        // Read the full data to verify the complex partial update
        SensorData complexUpdatedData = sharedMemory.read();
        std::cout << "\nFull data after complex partial update:\n";
        complexUpdatedData.print();

        // Read back just the status part
        SensorStatus readStatus =
            sharedMemory.readPartial<SensorStatus>(active_offset);
        std::cout << "\nPartially read status:\n";
        std::cout << "Status active: " << (readStatus.active ? "Yes" : "No")
                  << std::endl;
        std::cout << "Status last update: " << readStatus.last_update
                  << std::endl;

        // Demonstrate out-of-bounds handling
        std::cout << "\nTrying to read beyond the bounds of shared memory...\n";
        try {
            double value =
                sharedMemory.readPartial<double>(sizeof(SensorData) - 4);
            std::cout << "Read value: " << value << " (unexpected)\n";
        } catch (const atom::connection::SharedMemoryException& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
            std::cout << "Error code: " << e.getErrorCodeString() << std::endl;
        }

    } catch (const atom::connection::SharedMemoryException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Error code: " << e.getErrorCodeString() << std::endl;
    }

    //--------------------------------------------------------------------------
    // 5. Binary Data with Span
    //--------------------------------------------------------------------------
    printSection("5. Binary Data with Span");

    try {
        // Create shared memory
        atom::connection::SharedMemory<SensorData> sharedMemory(shm_name, true);

        // Create binary data
        std::vector<std::byte> binaryData(sizeof(SensorData));

        // Fill with pattern
        for (size_t i = 0; i < binaryData.size(); ++i) {
            binaryData[i] = static_cast<std::byte>(i % 256);
        }

        std::cout << "Writing " << binaryData.size()
                  << " bytes of binary data...\n";
        std::cout << "First few bytes: ";
        for (size_t i = 0; i < std::min(size_t(10), binaryData.size()); ++i) {
            std::cout << std::setw(2) << std::setfill('0') << std::hex
                      << static_cast<int>(binaryData[i]) << " ";
        }
        std::cout << std::dec << std::endl;

        // Write binary data using span
        sharedMemory.writeSpan(std::span<const std::byte>(binaryData));

        // Read binary data using span
        std::vector<std::byte> readData(sizeof(SensorData));
        size_t bytesRead =
            sharedMemory.readSpan(std::span<std::byte>(readData));

        std::cout << "\nRead " << bytesRead << " bytes of binary data\n";
        std::cout << "First few bytes: ";
        for (size_t i = 0; i < std::min(size_t(10), readData.size()); ++i) {
            std::cout << std::setw(2) << std::setfill('0') << std::hex
                      << static_cast<int>(readData[i]) << " ";
        }
        std::cout << std::dec << std::endl;

        // Verify data integrity
        bool dataMatches =
            std::equal(binaryData.begin(), binaryData.end(), readData.begin());
        std::cout << "\nData integrity check: "
                  << (dataMatches ? "PASSED" : "FAILED") << std::endl;

        // Read binary data into a smaller buffer
        std::vector<std::byte> smallBuffer(64);  // Only read first 64 bytes
        size_t bytesReadPartial =
            sharedMemory.readSpan(std::span<std::byte>(smallBuffer));

        std::cout << "\nRead " << bytesReadPartial
                  << " bytes into smaller buffer\n";
        std::cout << "First few bytes: ";
        for (size_t i = 0; i < std::min(size_t(10), smallBuffer.size()); ++i) {
            std::cout << std::setw(2) << std::setfill('0') << std::hex
                      << static_cast<int>(smallBuffer[i]) << " ";
        }
        std::cout << std::dec << std::endl;

        // Try to write a too large buffer
        std::cout << "\nTrying to write too much data...\n";
        std::vector<std::byte> tooLargeBuffer(sizeof(SensorData) + 100);
        try {
            sharedMemory.writeSpan(std::span<const std::byte>(tooLargeBuffer));
            std::cout << "Write succeeded (unexpected)\n";
        } catch (const atom::connection::SharedMemoryException& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
            std::cout << "Error code: " << e.getErrorCodeString() << std::endl;
        }

    } catch (const atom::connection::SharedMemoryException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Error code: " << e.getErrorCodeString() << std::endl;
    }

    //--------------------------------------------------------------------------
    // 6. Resize Functionality
    //--------------------------------------------------------------------------
    printSection("6. Resize Functionality");

    // Define a larger struct
    struct LargeData {
        SensorData sensor;
        std::array<double, 100> historical_data;
        char notes[256];
    };

    // Ensure the struct is trivially copyable
    static_assert(std::is_trivially_copyable_v<LargeData>,
                  "LargeData must be trivially copyable");

    try {
        // Create a shared memory for the SensorData
        std::cout << "Creating shared memory with SensorData size...\n";
        atom::connection::SharedMemory<SensorData> originalMemory(
            "resizable_memory", true);

        // Write initial sensor data
        SensorData sensorData = generateRandomSensorData(5);
        originalMemory.write(sensorData);

        std::cout << "Original shared memory size: " << originalMemory.getSize()
                  << " bytes\n";
        std::cout << "Initial data written:\n";
        sensorData.print();

        // Resize the shared memory to hold the larger struct
        std::cout << "\nResizing shared memory to hold LargeData...\n";
        constexpr size_t newSize = sizeof(LargeData);
        std::cout << "New size will be: " << newSize << " bytes\n";

        originalMemory.resize(newSize);

        std::cout << "Shared memory after resize: " << originalMemory.getSize()
                  << " bytes\n";

        // Read the original data to verify it was preserved
        SensorData preservedData = originalMemory.read();
        std::cout << "\nVerifying original data was preserved:\n";
        preservedData.print();

        // Now we'll create a new shared memory object for the large data
        std::cout << "\nCreating new handle for larger data type...\n";
        atom::connection::SharedMemory<LargeData> largeMemory(
            "resizable_memory", false);

        // Read current data and expand it
        LargeData largeData;

        // Copy the sensor data from the original shared memory
        largeData.sensor = preservedData;

        // Fill historical data
        for (size_t i = 0; i < largeData.historical_data.size(); ++i) {
            largeData.historical_data[i] = 20.0 + 0.1 * static_cast<double>(i);
        }

        // Add some notes
        std::string notes = "Temperature history for sensor #" +
                            std::to_string(largeData.sensor.id) +
                            " located in " + largeData.sensor.location;
        std::strncpy(largeData.notes, notes.c_str(),
                     sizeof(largeData.notes) - 1);
        largeData.notes[sizeof(largeData.notes) - 1] =
            '\0';  // Ensure null termination

        // Write the large data
        largeMemory.write(largeData);

        std::cout << "Large data written successfully\n";
        std::cout << "First few historical values: ";
        for (size_t i = 0; i < 5; ++i) {
            std::cout << largeData.historical_data[i] << " ";
        }
        std::cout << "...\n";
        std::cout << "Notes: " << largeData.notes << std::endl;

        // Read back the large data to verify
        LargeData readLargeData = largeMemory.read();

        std::cout << "\nLarge data read successfully\n";
        std::cout << "Sensor info:\n";
        readLargeData.sensor.print();
        std::cout << "First few historical values: ";
        for (size_t i = 0; i < 5; ++i) {
            std::cout << readLargeData.historical_data[i] << " ";
        }
        std::cout << "...\n";
        std::cout << "Notes: " << readLargeData.notes << std::endl;

        // Try to resize as non-creator
        std::cout << "\nTrying to resize as non-creator...\n";
        try {
            largeMemory.resize(sizeof(SensorData));
            std::cout << "Resize succeeded (unexpected)\n";
        } catch (const atom::connection::SharedMemoryException& e) {
            std::cout << "Expected exception caught: " << e.what() << std::endl;
            std::cout << "Error code: " << e.getErrorCodeString() << std::endl;
        }

    } catch (const atom::connection::SharedMemoryException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Error code: " << e.getErrorCodeString() << std::endl;
    }

    //--------------------------------------------------------------------------
    // 7. Asynchronous Operations
    //--------------------------------------------------------------------------
    printSection("7. Asynchronous Operations");

    try {
        // Create shared memory
        atom::connection::SharedMemory<SensorData> sharedMemory(shm_name, true);

        // Write some initial data
        SensorData initialData = generateRandomSensorData(6);
        sharedMemory.write(initialData);

        std::cout << "Initial data written synchronously:\n";
        initialData.print();

        // Asynchronous read
        std::cout << "\nPerforming asynchronous read...\n";
        auto readFuture = sharedMemory.readAsync();

        // Do some other work while reading
        std::cout << "Doing other work while reading asynchronously...\n";
        for (int i = 0; i < 3; ++i) {
            std::cout << "Work item " << (i + 1) << " completed\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Get the result
        SensorData asyncReadData = readFuture.get();
        std::cout << "\nAsync read completed. Data:\n";
        asyncReadData.print();

        // Asynchronous write
        std::cout << "\nPerforming asynchronous write...\n";
        SensorData newData = generateRandomSensorData(7);
        auto writeFuture = sharedMemory.writeAsync(newData);

        // Do some other work while writing
        std::cout << "Doing other work while writing asynchronously...\n";
        for (int i = 0; i < 3; ++i) {
            std::cout << "Work item " << (i + 1) << " completed\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Wait for write to complete
        writeFuture.wait();
        std::cout << "\nAsync write completed.\n";

        // Read back the data to verify
        SensorData verifyData = sharedMemory.read();
        std::cout << "Verifying data after async write:\n";
        verifyData.print();

    } catch (const atom::connection::SharedMemoryException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Error code: " << e.getErrorCodeString() << std::endl;
    }

    //--------------------------------------------------------------------------
    // 8. Change Notifications and Callbacks
    //--------------------------------------------------------------------------
    printSection("8. Change Notifications and Callbacks");

    try {
        // Create shared memory
        atom::connection::SharedMemory<SensorData> producer(producer_name,
                                                            true);

        // Create a second shared memory object to simulate another process
        atom::connection::SharedMemory<SensorData> consumer(producer_name,
                                                            false);

        // Register a change callback
        int callbackCount = 0;
        std::size_t callbackId = consumer.registerChangeCallback(
            [&callbackCount](const SensorData& data) {
                std::cout << "Callback triggered! Count: " << ++callbackCount
                          << std::endl;
                std::cout << "Received sensor data:\n";
                data.print();
                std::cout << std::endl;
            });

        std::cout << "Registered change callback with ID: " << callbackId
                  << std::endl;

        // Write data from producer and observe callbacks
        std::cout << "\nWriting data from producer...\n";
        for (int i = 0; i < 3; ++i) {
            SensorData data = generateRandomSensorData(10 + i);
            std::cout << "Producer writing data #" << (i + 1) << ":\n";
            data.print();
            producer.write(data);

            // Small delay to allow callback processing
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        std::cout << "\nCallback was triggered " << callbackCount << " times\n";

        // Unregister callback
        std::cout << "\nUnregistering callback...\n";
        bool unregistered = consumer.unregisterChangeCallback(callbackId);
        std::cout << "Callback unregistered: " << (unregistered ? "Yes" : "No")
                  << std::endl;

        // Write more data to verify no more callbacks
        std::cout << "\nWriting more data after unregistering callback...\n";
        SensorData finalData = generateRandomSensorData(20);
        producer.write(finalData);

        // Small delay to verify no callback
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::cout << "Final callback count: " << callbackCount
                  << " (should be unchanged)\n";

        // Demonstrate waitForChange
        std::cout << "\nDemonstrating waitForChange()...\n";

        // Start a thread that will write data after a delay
        std::thread writerThread([&producer]() {
            std::cout
                << "Writer thread: waiting 1 second before updating data...\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));

            SensorData newData = generateRandomSensorData(30);
            std::cout << "Writer thread: writing new data now\n";
            producer.write(newData);
        });

        // Wait for the change with timeout
        std::cout << "Main thread: waiting for data to change (2 second "
                     "timeout)...\n";
        bool changed = consumer.waitForChange(std::chrono::milliseconds(2000));

        if (changed) {
            std::cout << "Data changed detected!\n";
            SensorData newData = consumer.read();
            std::cout << "New data:\n";
            newData.print();
        } else {
            std::cout << "Timeout waiting for change\n";
        }

        // Ensure the writer thread completes
        if (writerThread.joinable()) {
            writerThread.join();
        }

    } catch (const atom::connection::SharedMemoryException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Error code: " << e.getErrorCodeString() << std::endl;
    }

    //--------------------------------------------------------------------------
    // 9. Multi-threaded Producer-Consumer Pattern
    //--------------------------------------------------------------------------
    printSection("9. Multi-threaded Producer-Consumer Pattern");

    try {
        // Create shared memory
        atom::connection::SharedMemory<SensorData> sharedMemory(shm_name, true);

        // Flag to signal threads to stop
        std::atomic<bool> stopThreads(false);

        // Statistics counters
        std::atomic<int> producerCount(0);
        std::atomic<int> consumerCount(0);

        // Producer thread
        std::thread producerThread([&]() {
            try {
                while (!stopThreads) {
                    // Generate and write new sensor data
                    SensorData data =
                        generateRandomSensorData(producerCount + 100);
                    sharedMemory.write(data);

                    // Increment the counter
                    producerCount++;

                    // Simulate some processing time
                    std::this_thread::sleep_for(std::chrono::milliseconds(300));
                }
            } catch (const std::exception& e) {
                std::cerr << "Producer thread exception: " << e.what()
                          << std::endl;
            }
        });

        // Consumer thread
        std::thread consumerThread([&]() {
            try {
                while (!stopThreads) {
                    try {
                        // Wait for data to change
                        bool changed = sharedMemory.waitForChange(
                            std::chrono::milliseconds(500));

                        if (changed) {
                            // Read the new data
                            SensorData data = sharedMemory.read();

                            // Process the data (just print a summary in this
                            // example)
                            std::cout << "Consumer: Processing sensor #"
                                      << data.id << " from " << data.location
                                      << ", temp: " << data.temperature
                                      << "°C\n";

                            // Increment the counter
                            consumerCount++;
                        }
                    } catch (const atom::connection::SharedMemoryException& e) {
                        // Ignore timeouts, report other errors
                        if (e.getErrorCode() !=
                            atom::connection::SharedMemoryException::ErrorCode::
                                TIMEOUT) {
                            std::cerr << "Consumer exception: " << e.what()
                                      << std::endl;
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Consumer thread exception: " << e.what()
                          << std::endl;
            }
        });

        // Status reporting thread
        std::thread reportThread([&]() {
            while (!stopThreads) {
                std::cout << "Status: produced=" << producerCount
                          << ", consumed=" << consumerCount << std::endl;

                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });

        // Run the example for a few seconds
        std::cout << "Running producer-consumer pattern for 5 seconds...\n";
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Signal threads to stop and wait for them
        stopThreads = true;

        if (producerThread.joinable())
            producerThread.join();
        if (consumerThread.joinable())
            consumerThread.join();
        if (reportThread.joinable())
            reportThread.join();

        std::cout << "\nProducer-consumer test complete!\n";
        std::cout << "Final stats: produced=" << producerCount
                  << ", consumed=" << consumerCount << std::endl;

    } catch (const atom::connection::SharedMemoryException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Error code: " << e.getErrorCodeString() << std::endl;
    }

    //--------------------------------------------------------------------------
    // 10. Advanced Function with withLock
    //--------------------------------------------------------------------------
    printSection("10. Advanced Functions with withLock");

    try {
        // Create shared memory
        atom::connection::SharedMemory<SensorData> sharedMemory(shm_name, true);

        // Initialize data
        SensorData initialData = generateRandomSensorData(40);
        sharedMemory.write(initialData);

        std::cout << "Initial data written:\n";
        initialData.print();

        // Use withLock to perform an atomic read-modify-write operation
        std::cout << "\nPerforming atomic read-modify-write operation...\n";

        sharedMemory.withLock(
            [&]() {
                // Read current data
                SensorData data;
                std::memcpy(&data, sharedMemory.getDataPtr(),
                            sizeof(SensorData));

                // Modify the data
                std::cout << "Current temperature: " << data.temperature
                          << "°C\n";
                data.temperature += 1.5;
                std::cout << "Updated temperature: " << data.temperature
                          << "°C\n";

                // Update timestamp
                data.setCurrentTimestamp();

                // Write back
                std::memcpy(sharedMemory.getDataPtr(), &data,
                            sizeof(SensorData));

                // Update version and other metadata
                return true;  // Operation successful
            },
            std::chrono::milliseconds(100));

        // Verify the modification
        SensorData modifiedData = sharedMemory.read();
        std::cout << "\nVerifying data after atomic operation:\n";
        modifiedData.print();

        // Get native handle (for advanced use cases)
        void* nativeHandle = sharedMemory.getNativeHandle();
        std::cout << "\nNative handle: " << nativeHandle << std::endl;

    } catch (const atom::connection::SharedMemoryException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << "Error code: " << e.getErrorCodeString() << std::endl;
    }

    //--------------------------------------------------------------------------
    // Cleanup
    //--------------------------------------------------------------------------
    printSection("Cleanup");

    std::cout << "Completed all examples. Resources will be cleaned up "
                 "automatically\n";
    std::cout << "when shared memory objects go out of scope.\n";

    // The destructors for the SharedMemory objects will handle resource cleanup

    return 0;
}
