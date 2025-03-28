/*
 * leak_detection_example.cpp
 *
 * This example demonstrates how to use the leak detection utility in Atom.
 * The utility helps identify memory leaks in your application by tracking
 * allocations and deallocations.
 *
 * Copyright (C) 2024 Example User
 */

 #include <iostream>
 #include <vector>
 #include <string>
 #include <memory>
 #include <cstdlib>
 #include <thread>
 #include <chrono>
 
 // Include leak detection header first to ensure proper initialization
 #include "atom/utils/leak.hpp"
 
 // Additional headers
 #include "atom/log/loguru.hpp"
 
 // A class with deliberate memory leak for demonstration
 class LeakyClass {
 private:
     int* data;
     char* buffer;
     std::vector<double>* vector_data;
 
 public:
     LeakyClass(int size) {
         // Allocate memory without proper cleanup in some paths
         data = new int[size];
         std::cout << "Allocated int array with " << size << " elements at " << data << std::endl;
         
         buffer = new char[1024];
         std::cout << "Allocated char buffer of 1024 bytes at " << static_cast<void*>(buffer) << std::endl;
         
         vector_data = new std::vector<double>(size, 0.0);
         std::cout << "Allocated vector with " << size << " elements at " << vector_data << std::endl;
     }
     
     // Proper cleanup path
     void cleanupProperly() {
         std::cout << "Properly cleaning up all allocations" << std::endl;
         delete[] data;
         delete[] buffer;
         delete vector_data;
         
         // Set to nullptr to prevent double-free
         data = nullptr;
         buffer = nullptr;
         vector_data = nullptr;
     }
     
     // Incomplete cleanup - will cause leak
     void cleanupIncomplete() {
         std::cout << "Performing incomplete cleanup (will cause leaks)" << std::endl;
         // Only delete the int array, leaving the buffer and vector leaked
         delete[] data;
         data = nullptr;
     }
     
     // No cleanup - will cause all resources to leak
     void noCleanup() {
         std::cout << "No cleanup performed (will cause all resources to leak)" << std::endl;
         // Intentionally do nothing
     }
     
     ~LeakyClass() {
         // In real code, we should clean up here
         // But for the example, we'll leave it empty to demonstrate leaks
         std::cout << "~LeakyClass destructor called (without proper cleanup)" << std::endl;
     }
 };
 
 // Function that demonstrates a memory leak
 void demonstrateSimpleLeak() {
     std::cout << "\n=== Demonstrating Simple Memory Leak ===" << std::endl;
     
     // Allocate memory without freeing it
     int* leakedArray = new int[100];
     for (int i = 0; i < 100; i++) {
         leakedArray[i] = i;
     }
     
     std::cout << "Allocated array at " << leakedArray << " but didn't free it" << std::endl;
     
     // Note: Deliberately not deleting leakedArray to demonstrate leak detection
 }
 
 // Function that demonstrates proper memory management
 void demonstrateProperMemoryManagement() {
     std::cout << "\n=== Demonstrating Proper Memory Management ===" << std::endl;
     
     // Allocate memory and properly free it
     int* properArray = new int[100];
     for (int i = 0; i < 100; i++) {
         properArray[i] = i;
     }
     
     std::cout << "Allocated array at " << properArray << std::endl;
     
     // Proper cleanup
     delete[] properArray;
     std::cout << "Properly freed the array" << std::endl;
 }
 
 // Function that demonstrates smart pointers to prevent leaks
 void demonstrateSmartPointers() {
     std::cout << "\n=== Demonstrating Smart Pointers ===" << std::endl;
     
     // Using unique_ptr for automatic cleanup
     {
         std::unique_ptr<int[]> uniqueArray = std::make_unique<int[]>(100);
         std::cout << "Created array with unique_ptr at " << uniqueArray.get() << std::endl;
         
         // Fill with data
         for (int i = 0; i < 100; i++) {
             uniqueArray[i] = i;
         }
         
         std::cout << "unique_ptr will automatically free memory when going out of scope" << std::endl;
     } // uniqueArray is automatically deleted here
     
     // Using shared_ptr for shared ownership
     {
         auto sharedVector = std::make_shared<std::vector<double>>(1000, 0.5);
         std::cout << "Created vector with shared_ptr at " << sharedVector.get() << std::endl;
         
         // Create another shared pointer to the same data
         std::shared_ptr<std::vector<double>> anotherReference = sharedVector;
         std::cout << "Created second reference, use count: " << sharedVector.use_count() << std::endl;
         
         // The data will be freed when all references are gone
     } // Both shared pointers are automatically deleted here
 }
 
 // Function to demonstrate complex leaking scenario across threads
 void demonstrateThreadedLeaks() {
     std::cout << "\n=== Demonstrating Threaded Memory Leaks ===" << std::endl;
     
     // Create a vector to store thread objects
     std::vector<std::thread> threads;
     
     // Launch multiple threads that may leak memory
     for (int i = 0; i < 3; i++) {
         threads.emplace_back([i]() {
             std::cout << "Thread " << i << " starting" << std::endl;
             
             // Allocate memory in thread
             char* threadBuffer = new char[512 * (i + 1)];
             std::memset(threadBuffer, 'A' + i, 512 * (i + 1));
             
             std::cout << "Thread " << i << " allocated " << 512 * (i + 1) 
                       << " bytes at " << static_cast<void*>(threadBuffer) << std::endl;
             
             // Sleep to simulate work
             std::this_thread::sleep_for(std::chrono::milliseconds(100));
             
             // Even threads leak differently
             if (i % 2 == 0) {
                 // Even-numbered threads free their memory
                 delete[] threadBuffer;
                 std::cout << "Thread " << i << " freed its memory" << std::endl;
             } else {
                 // Odd-numbered threads leak their memory
                 std::cout << "Thread " << i << " is leaking its memory" << std::endl;
             }
             
             std::cout << "Thread " << i << " ending" << std::endl;
         });
     }
     
     // Join all threads
     for (auto& thread : threads) {
         thread.join();
     }
     
     std::cout << "All threads completed" << std::endl;
 }
 
 // Function to demonstrate leak detection with container classes
 void demonstrateContainerLeaks() {
     std::cout << "\n=== Demonstrating Container Leaks ===" << std::endl;
     
     // Create a vector of raw pointers (not recommended in real code)
     std::vector<int*> pointerVector;
     
     // Add multiple allocations
     for (int i = 0; i < 5; i++) {
         int* ptr = new int(i * 100);
         pointerVector.push_back(ptr);
         std::cout << "Added pointer to value " << *ptr << " at " << ptr << std::endl;
     }
     
     // Only delete some of them (creating leaks)
     for (size_t i = 0; i < pointerVector.size(); i++) {
         if (i % 2 == 0) {
             std::cout << "Deleting pointer at index " << i << std::endl;
             delete pointerVector[i];
         } else {
             std::cout << "Leaking pointer at index " << i << std::endl;
         }
     }
     
     // Clear the vector (but the odd-indexed pointers are still leaked)
     pointerVector.clear();
     std::cout << "Vector cleared, but some pointers were leaked" << std::endl;
 }
 
 // Class to demonstrate RAII pattern to prevent leaks
 class RAIIExample {
 private:
     int* resource;
 
 public:
     RAIIExample(int size) : resource(new int[size]) {
         std::cout << "RAII class allocated resource at " << resource << std::endl;
     }
     
     ~RAIIExample() {
         std::cout << "RAII class automatically freeing resource at " << resource << std::endl;
         delete[] resource;
     }
 };
 
 // Function to demonstrate proper RAII usage
 void demonstrateRAII() {
     std::cout << "\n=== Demonstrating RAII (Resource Acquisition Is Initialization) ===" << std::endl;
     
     // Create an instance of the RAII class
     {
         RAIIExample raii(200);
         std::cout << "Using RAII object..." << std::endl;
         
         // No need to manually call cleanup methods
     } // Resource is automatically freed here
     
     std::cout << "RAII object went out of scope, resource was freed" << std::endl;
 }
 
 int main() {
     // Initialize loguru
     loguru::g_stderr_verbosity = 1;
     loguru::init(0, nullptr);
     
     std::cout << "===============================================" << std::endl;
     std::cout << "Memory Leak Detection Example" << std::endl;
     std::cout << "===============================================" << std::endl;
     std::cout << "This example demonstrates how to use the leak detection utility" << std::endl;
     std::cout << "Note: Visual Leak Detector will report leaks at program exit" << std::endl;
     std::cout << "===============================================\n" << std::endl;
     
     // Demonstrate memory leaks with different scenarios
     demonstrateSimpleLeak();
     
     demonstrateProperMemoryManagement();
     
     demonstrateSmartPointers();
     
     // Create leaky class instances with different cleanup approaches
     {
         std::cout << "\n=== Demonstrating Different Cleanup Strategies ===" << std::endl;
         
         LeakyClass* properCleanup = new LeakyClass(50);
         LeakyClass* incompleteCleanup = new LeakyClass(100);
         LeakyClass* noCleanup = new LeakyClass(150);
         
         // Demonstrate different cleanup strategies
         properCleanup->cleanupProperly();
         incompleteCleanup->cleanupIncomplete();
         noCleanup->noCleanup();
         
         // Free the class instances
         delete properCleanup;
         delete incompleteCleanup;
         delete noCleanup;
     }
     
     demonstrateThreadedLeaks();
     
     demonstrateContainerLeaks();
     
     demonstrateRAII();
     
     std::cout << "\n=== Additional Memory Leak Detection Tips ===" << std::endl;
     std::cout << "1. Always use smart pointers (std::unique_ptr, std::shared_ptr) when possible" << std::endl;
     std::cout << "2. Implement RAII pattern in your classes" << std::endl;
     std::cout << "3. Avoid manual memory management with new/delete" << std::endl;
     std::cout << "4. Use containers and algorithms from the standard library" << std::endl;
     std::cout << "5. Set clear ownership rules for resources" << std::endl;
     std::cout << "6. Run with memory leak detection tools regularly" << std::endl;
     
     std::cout << "\n===============================================" << std::endl;
     std::cout << "Program completed. Check leak detector output." << std::endl;
     std::cout << "===============================================" << std::endl;
     
     return 0;
 }