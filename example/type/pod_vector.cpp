#include "../atom/type/pod_vector.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>  // 添加这个头文件用于 std::memset 和 std::strcpy
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

// Simple POD type for testing
struct Point {
    float x;
    float y;

    // For easy printing
    friend std::ostream& operator<<(std::ostream& os, const Point& p) {
        return os << "(" << p.x << ", " << p.y << ")";
    }

    // For comparison
    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

// Helper function to print a vector
template <typename T>
void printVector(const atom::type::PodVector<T>& vec, const std::string& name) {
    std::cout << name << " (size " << vec.size() << "): [";
    for (int i = 0; i < vec.size(); ++i) {
        if (i > 0)
            std::cout << ", ";
        std::cout << vec[i];
    }
    std::cout << "]" << std::endl;
}

// Example 1: Basic Usage
void basicUsageExample() {
    std::cout << "\n=== Example 1: Basic Usage ===\n";

    // Create an empty vector
    atom::type::PodVector<int> emptyVec;
    std::cout << "Empty vector size: " << emptyVec.size() << std::endl;
    std::cout << "Empty vector capacity: " << emptyVec.capacity() << std::endl;
    std::cout << "Empty vector is empty: " << (emptyVec.empty() ? "yes" : "no")
              << std::endl;

    // Create a vector with initializer list
    atom::type::PodVector<int> vec = {1, 2, 3, 4, 5};
    printVector(vec, "Initialized vector");

    // Create a vector with specified size
    atom::type::PodVector<float> floatVec(10);
    std::cout << "Vector with specified size - size: " << floatVec.size()
              << std::endl;
    std::cout << "Vector with specified size - capacity: "
              << floatVec.capacity() << std::endl;

    // Access elements
    std::cout << "vec[0]: " << vec[0] << std::endl;
    std::cout << "vec[4]: " << vec[4] << std::endl;

    // Modify elements
    vec[0] = 10;
    vec[4] = 50;
    printVector(vec, "After modification");
}

// Example 2: Adding Elements
void addingElementsExample() {
    std::cout << "\n=== Example 2: Adding Elements ===\n";

    atom::type::PodVector<int> vec;

    // Push back values
    std::cout << "Adding elements with push_back:" << std::endl;
    for (int i = 0; i < 5; ++i) {
        vec.pushBack(i * 10);
        std::cout << "  Added " << i * 10 << ", size: " << vec.size()
                  << ", capacity: " << vec.capacity() << std::endl;
    }

    printVector(vec, "After pushBack");

    // Test emplace_back with Point struct
    atom::type::PodVector<Point> points;

    // Using emplaceBack
    std::cout << "\nAdding elements with emplaceBack:" << std::endl;
    points.emplaceBack(1.0f, 2.0f);
    points.emplaceBack(3.0f, 4.0f);
    points.emplaceBack(5.0f, 6.0f);

    std::cout << "Points vector:" << std::endl;
    for (int i = 0; i < points.size(); ++i) {
        std::cout << "  Point " << i << ": " << points[i] << std::endl;
    }

    // Insert element at position
    vec.insert(2, 25);
    printVector(vec, "After inserting 25 at position 2");

    // Extend with another vector
    atom::type::PodVector<int> vec2 = {100, 200, 300};
    vec.extend(vec2);
    printVector(vec, "After extending with {100, 200, 300}");

    // Extend with pointers
    int arr[] = {400, 500};
    vec.extend(arr, arr + 2);
    printVector(vec, "After extending with array {400, 500}");
}

// Example 3: Removing Elements
void removingElementsExample() {
    std::cout << "\n=== Example 3: Removing Elements ===\n";

    atom::type::PodVector<int> vec = {10, 20, 30, 40, 50};
    printVector(vec, "Original vector");

    // Pop back
    vec.popBack();
    printVector(vec, "After popBack");

    // Pop back with return value
    int value = vec.back();  // Get the last element before removing it
    vec.popBack();           // Then remove the element
    std::cout << "Popped value: " << value << std::endl;
    printVector(vec, "After popBack");  // 修复拼写错误：popxBack -> popBack

    // Erase element at position
    vec.erase(1);  // Remove 20
    printVector(vec, "After erasing element at position 1");

    // Clear
    vec.clear();
    std::cout << "After clear - size: " << vec.size()
              << ", empty: " << (vec.empty() ? "yes" : "no") << std::endl;
}

// Example 4: Memory Management
void memoryManagementExample() {
    std::cout << "\n=== Example 4: Memory Management ===\n";

    atom::type::PodVector<int> vec;

    // Initial state
    std::cout << "Initial - size: " << vec.size()
              << ", capacity: " << vec.capacity() << std::endl;

    // Reserve memory
    vec.reserve(100);
    std::cout << "After reserve(100) - size: " << vec.size()
              << ", capacity: " << vec.capacity() << std::endl;

    // Add some elements
    for (int i = 0; i < 10; ++i) {
        vec.pushBack(i);
    }
    std::cout << "After adding 10 elements - size: " << vec.size()
              << ", capacity: " << vec.capacity() << std::endl;

    // Resize
    vec.resize(20);
    std::cout << "After resize(20) - size: " << vec.size()
              << ", capacity: " << vec.capacity() << std::endl;

    // Resize smaller
    vec.resize(5);
    std::cout << "After resize(5) - size: " << vec.size()
              << ", capacity: " << vec.capacity() << std::endl;

    // Detach data
    auto [data_ptr, size] = vec.detach();
    std::cout << "After detach - original size: " << vec.size()
              << ", detached size: " << size << std::endl;

    // Clean up the detached memory manually since we own it now
    std::allocator<int> alloc;
    alloc.deallocate(data_ptr, size);
}

// Example 5: Iteration
void iterationExample() {
    std::cout << "\n=== Example 5: Iteration ===\n";

    atom::type::PodVector<int> vec = {10, 20, 30, 40, 50};

    // Range-based for loop
    std::cout << "Range-based for loop:" << std::endl;
    for (const auto& value : vec) {
        std::cout << "  " << value << std::endl;
    }

    // Iterator-based loop
    std::cout << "\nIterator-based loop:" << std::endl;
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        std::cout << "  " << *it << std::endl;
    }

    // Reverse iteration using std::reverse_iterator
    std::cout << "\nReverse iteration:" << std::endl;
    for (auto it = std::make_reverse_iterator(vec.end());
         it != std::make_reverse_iterator(vec.begin()); ++it) {
        std::cout << "  " << *it << std::endl;
    }

    // Const iteration
    const auto& const_vec = vec;
    std::cout << "\nConst iteration:" << std::endl;
    for (auto it = const_vec.begin(); it != const_vec.end(); ++it) {
        std::cout << "  " << *it << std::endl;
    }
}

// Example 6: Algorithms and Operations
void algorithmsExample() {
    std::cout << "\n=== Example 6: Algorithms and Operations ===\n";

    atom::type::PodVector<int> vec = {5, 2, 8, 1, 9, 3};
    printVector(vec, "Original vector");

    // Sort using STL algorithm
    std::sort(vec.begin(), vec.end());
    printVector(vec, "After sorting");

    // Get sum using std::accumulate
    int sum = std::accumulate(vec.begin(), vec.end(), 0);
    std::cout << "Sum of elements: " << sum << std::endl;

    // Find element
    auto it = std::find(vec.begin(), vec.end(), 8);
    if (it != vec.end()) {
        std::cout << "Found element 8 at position: " << (it - vec.begin())
                  << std::endl;
    }

    // Reverse the vector
    vec.reverse();
    printVector(vec, "After reversing");

    // Access first/last element
    std::cout << "First element: " << vec[0] << std::endl;
    std::cout << "Last element (using back()): " << vec.back() << std::endl;

    // Use data pointer for direct memory access
    int* data = vec.data();
    std::cout << "Direct access using data(): ";
    for (int i = 0; i < vec.size(); ++i) {
        std::cout << data[i] << " ";
    }
    std::cout << std::endl;
}

// Example 7: Move Semantics
void moveAndCopyExample() {
    std::cout << "\n=== Example 7: Move Semantics ===\n";

    // Copy constructor
    atom::type::PodVector<int> vec1 = {1, 2, 3, 4, 5};
    atom::type::PodVector<int> vec2(vec1);  // Copy constructor

    printVector(vec1, "Original vector (vec1)");
    printVector(vec2, "Copied vector (vec2)");

    // Modify the copy to show they're independent
    vec2[0] = 100;
    printVector(vec1, "vec1 after modifying vec2");
    printVector(vec2, "vec2 after modification");

    // Move constructor
    atom::type::PodVector<int> vec3(std::move(vec1));
    std::cout << "vec1 size after move: " << vec1.size() << std::endl;
    printVector(vec3, "Moved vector (vec3)");

    // Move assignment
    atom::type::PodVector<int> vec4;
    vec4 = std::move(vec3);
    std::cout << "vec3 size after move assignment: " << vec3.size()
              << std::endl;
    printVector(vec4, "Target of move assignment (vec4)");
}

// Example 8: Performance Comparison
void performanceExample() {
    std::cout << "\n=== Example 8: Performance Comparison ===\n";

    constexpr int NUM_ELEMENTS = 1000000;

    // Test PodVector
    auto start_pod = std::chrono::high_resolution_clock::now();
    atom::type::PodVector<int> pod_vec;
    pod_vec.reserve(NUM_ELEMENTS);  // Pre-allocate for fair comparison

    for (int i = 0; i < NUM_ELEMENTS; ++i) {
        pod_vec.pushBack(i);
    }

    auto end_pod = std::chrono::high_resolution_clock::now();
    auto duration_pod = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_pod - start_pod);

    // Test standard vector
    auto start_std = std::chrono::high_resolution_clock::now();
    std::vector<int> std_vec;
    std_vec.reserve(NUM_ELEMENTS);  // Pre-allocate for fair comparison

    for (int i = 0; i < NUM_ELEMENTS; ++i) {
        std_vec.push_back(i);
    }

    auto end_std = std::chrono::high_resolution_clock::now();
    auto duration_std = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_std - start_std);

    std::cout << "Time to add " << NUM_ELEMENTS << " elements:" << std::endl;
    std::cout << "  PodVector: " << duration_pod.count() << " ms" << std::endl;
    std::cout << "  std::vector: " << duration_std.count() << " ms"
              << std::endl;

    // Test iteration performance
    auto start_pod_iter = std::chrono::high_resolution_clock::now();
    int sum_pod = 0;
    for (const auto& val : pod_vec) {
        sum_pod += val;
    }
    auto end_pod_iter = std::chrono::high_resolution_clock::now();
    auto duration_pod_iter =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_pod_iter -
                                                              start_pod_iter);

    auto start_std_iter = std::chrono::high_resolution_clock::now();
    int sum_std = 0;
    for (const auto& val : std_vec) {
        sum_std += val;
    }
    auto end_std_iter = std::chrono::high_resolution_clock::now();
    auto duration_std_iter =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_std_iter -
                                                              start_std_iter);

    std::cout << "\nTime to iterate through " << NUM_ELEMENTS
              << " elements:" << std::endl;
    std::cout << "  PodVector: " << duration_pod_iter.count() << " ms"
              << std::endl;
    std::cout << "  std::vector: " << duration_std_iter.count() << " ms"
              << std::endl;
    std::cout << "  Sums: " << sum_pod << " vs " << sum_std << std::endl;
}

// Example 9: Working with Complex POD Types
void complexPodExample() {
    std::cout << "\n=== Example 9: Working with Complex POD Types ===\n";

    // Define a more complex POD type
    struct Particle {
        float x, y, z;     // Position
        float vx, vy, vz;  // Velocity
        float mass;        // Mass
        int type;          // Type identifier
        bool active;       // Active flag
    };

    // 修复：添加独立的打印函数，而不是作为类的成员函数
    auto printParticle = [](const Particle& p) {
        std::cout << "Particle(" << p.x << "," << p.y << "," << p.z
                  << ", mass=" << p.mass << ", type=" << p.type << ")";
    };

    // Create a PodVector for particles
    atom::type::PodVector<Particle> particles;

    // Add some particles
    particles.pushBack(
        Particle{1.0f, 2.0f, 3.0f, 0.1f, 0.2f, 0.3f, 5.0f, 1, true});
    particles.pushBack(
        Particle{4.0f, 5.0f, 6.0f, 0.4f, 0.5f, 0.6f, 10.0f, 2, true});
    particles.pushBack(
        Particle{7.0f, 8.0f, 9.0f, 0.7f, 0.8f, 0.9f, 15.0f, 1, false});

    // Display particles
    std::cout << "Particles:" << std::endl;
    for (int i = 0; i < particles.size(); ++i) {
        std::cout << "  ";
        printParticle(particles[i]);
        std::cout << std::endl;
    }

    // Calculate total mass
    float total_mass = 0.0f;
    for (const auto& p : particles) {
        total_mass += p.mass;
    }
    std::cout << "Total mass: " << total_mass << std::endl;

    // Filter active particles
    int active_count = 0;
    for (const auto& p : particles) {
        if (p.active)
            active_count++;
    }
    std::cout << "Active particles: " << active_count << " out of "
              << particles.size() << std::endl;

    // Update particle positions based on velocity (simplified physics)
    float dt = 0.1f;  // time step
    for (auto& p : particles) {
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.z += p.vz * dt;
    }

    std::cout << "\nAfter position update:" << std::endl;
    for (int i = 0; i < particles.size(); ++i) {
        std::cout << "  ";
        printParticle(particles[i]);
        std::cout << std::endl;
    }
}

// Example 10: Advanced Usage Patterns
void advancedUsageExample() {
    std::cout << "\n=== Example 10: Advanced Usage Patterns ===\n";

    // Create a PodVector with growth factor of 4 (faster growth)
    atom::type::PodVector<int, 4> fastGrowthVec;
    std::cout << "Fast growth vector - initial capacity: "
              << fastGrowthVec.capacity() << std::endl;

    // Add elements to trigger growth
    for (int i = 0; i < 100; ++i) {
        fastGrowthVec.pushBack(i);
        if (i % 20 == 0) {
            std::cout << "  After " << (i + 1)
                      << " elements: capacity = " << fastGrowthVec.capacity()
                      << std::endl;
        }
    }

    // 修复：使用纯POD类型而不是有构造函数的类型
    struct PodMemoryBlock {
        char data[64];
        bool used;
    };

    // 创建内存池并初始化所有内存块
    atom::type::PodVector<PodMemoryBlock> memoryPool(10);
    for (auto& block : memoryPool) {
        block.used = false;
        std::memset(block.data, 0, sizeof(block.data));
    }

    // 分配一个块
    auto allocateBlock = [&memoryPool]() -> int {
        for (int i = 0; i < memoryPool.size(); ++i) {
            if (!memoryPool[i].used) {
                memoryPool[i].used = true;
                return i;
            }
        }
        // 没有空闲的块，添加一个新块
        PodMemoryBlock newBlock;
        newBlock.used = true;
        std::memset(newBlock.data, 0, sizeof(newBlock.data));

        memoryPool.pushBack(newBlock);
        return memoryPool.size() - 1;
    };

    // 释放一个块
    auto freeBlock = [&memoryPool](int index) {
        if (index >= 0 && index < memoryPool.size()) {
            memoryPool[index].used = false;
            std::memset(memoryPool[index].data, 0,
                        sizeof(memoryPool[index].data));
        }
    };

    // 使用内存池
    std::cout << "\nMemory pool example:" << std::endl;
    int block1 = allocateBlock();
    std::cout << "Allocated block " << block1 << std::endl;

    // 写入块
    std::strcpy(memoryPool[block1].data, "Hello, Memory Pool!");
    std::cout << "Data in block " << block1 << ": " << memoryPool[block1].data
              << std::endl;

    // 分配更多块
    int block2 = allocateBlock();
    int block3 = allocateBlock();
    std::cout << "Allocated blocks " << block2 << " and " << block3
              << std::endl;
    std::cout << "Pool size: " << memoryPool.size() << std::endl;

    // 释放 block 2
    freeBlock(block2);
    std::cout << "Freed block " << block2 << std::endl;

    // 再次分配（应该重用 block 2）
    int block4 = allocateBlock();
    std::cout << "Allocated block " << block4 << " (should be " << block2 << ")"
              << std::endl;

    // 计算已使用的块
    int usedBlocks = 0;
    for (const auto& block : memoryPool) {
        if (block.used)
            usedBlocks++;
    }
    std::cout << "Used blocks: " << usedBlocks << " out of "
              << memoryPool.size() << std::endl;
}

int main() {
    std::cout << "===== PodVector<T> Usage Examples =====\n";

    // Run all examples
    basicUsageExample();
    addingElementsExample();
    removingElementsExample();
    memoryManagementExample();
    iterationExample();
    algorithmsExample();
    moveAndCopyExample();
    performanceExample();
    complexPodExample();
    advancedUsageExample();

    return 0;
}
