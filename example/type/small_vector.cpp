/**
 * @file small_vector_example.cpp
 * @brief Comprehensive examples demonstrating the SmallVector class
 *
 * This file showcases all features of the SmallVector template class including
 * constructors, element access, modifiers, iterators, and more.
 */

#include "atom/type/small_vector.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==========================================" << std::endl;
}

// Helper function to print subsection headers
void printSubsection(const std::string& title) {
    std::cout << "\n--- " << title << " ---" << std::endl;
}

// Helper function to display SmallVector contents
template <typename T, std::size_t N, typename Allocator = std::allocator<T>>
void printVector(const SmallVector<T, N, Allocator>& vec,
                 const std::string& name) {
    std::cout << name << " (size=" << vec.size()
              << ", capacity=" << vec.capacity()
              << ", inline=" << (vec.isUsingInlineStorage() ? "true" : "false")
              << "): [";

    bool first = true;
    for (const auto& elem : vec) {
        if (!first) {
            std::cout << ", ";
        }
        std::cout << elem;
        first = false;
    }
    std::cout << "]" << std::endl;
}

// Helper function to measure performance
template <typename Func>
double measureTime(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::micro>(end - start).count();
}

// Custom allocator for testing
template <typename T>
class TrackingAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template <typename U>
    struct rebind {
        using other = TrackingAllocator<U>;
    };

    TrackingAllocator() noexcept = default;
    template <typename U>
    TrackingAllocator(const TrackingAllocator<U>&) noexcept {}

    pointer allocate(size_type n) {
        allocation_count++;
        total_bytes_allocated += n * sizeof(T);
        return static_cast<pointer>(::operator new(n * sizeof(T)));
    }

    void deallocate(pointer p, size_type n) noexcept {
        deallocation_count++;
        total_bytes_deallocated += n * sizeof(T);
        ::operator delete(p);
    }

    bool operator==(const TrackingAllocator&) const noexcept { return true; }
    bool operator!=(const TrackingAllocator&) const noexcept { return false; }

    // Static tracking counters
    static size_t allocation_count;
    static size_t deallocation_count;
    static size_t total_bytes_allocated;
    static size_t total_bytes_deallocated;

    static void resetCounters() {
        allocation_count = 0;
        deallocation_count = 0;
        total_bytes_allocated = 0;
        total_bytes_deallocated = 0;
    }
};

template <typename T>
size_t TrackingAllocator<T>::allocation_count = 0;

template <typename T>
size_t TrackingAllocator<T>::deallocation_count = 0;

template <typename T>
size_t TrackingAllocator<T>::total_bytes_allocated = 0;

template <typename T>
size_t TrackingAllocator<T>::total_bytes_deallocated = 0;

// Custom class for testing with non-trivial types
class TestObject {
public:
    TestObject() : value_(0) { constructor_calls++; }

    TestObject(int val) : value_(val) { constructor_calls++; }

    TestObject(const TestObject& other) : value_(other.value_) {
        copy_constructor_calls++;
    }

    TestObject(TestObject&& other) noexcept : value_(other.value_) {
        move_constructor_calls++;
        other.value_ = -1;
    }

    ~TestObject() { destructor_calls++; }

    TestObject& operator=(const TestObject& other) {
        if (this != &other) {
            value_ = other.value_;
            copy_assignment_calls++;
        }
        return *this;
    }

    TestObject& operator=(TestObject&& other) noexcept {
        if (this != &other) {
            value_ = other.value_;
            other.value_ = -1;
            move_assignment_calls++;
        }
        return *this;
    }

    bool operator==(const TestObject& other) const {
        return value_ == other.value_;
    }

    bool operator!=(const TestObject& other) const { return !(*this == other); }

    bool operator<(const TestObject& other) const {
        return value_ < other.value_;
    }

    friend std::ostream& operator<<(std::ostream& os, const TestObject& obj) {
        return os << obj.value_;
    }

    int getValue() const { return value_; }

    // Static counters for tracking operations
    static void resetCounters() {
        constructor_calls = 0;
        copy_constructor_calls = 0;
        move_constructor_calls = 0;
        destructor_calls = 0;
        copy_assignment_calls = 0;
        move_assignment_calls = 0;
    }

    static size_t constructor_calls;
    static size_t copy_constructor_calls;
    static size_t move_constructor_calls;
    static size_t destructor_calls;
    static size_t copy_assignment_calls;
    static size_t move_assignment_calls;

private:
    int value_;
};

size_t TestObject::constructor_calls = 0;
size_t TestObject::copy_constructor_calls = 0;
size_t TestObject::move_constructor_calls = 0;
size_t TestObject::destructor_calls = 0;
size_t TestObject::copy_assignment_calls = 0;
size_t TestObject::move_assignment_calls = 0;

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "  SmallVector Class Demonstration" << std::endl;
    std::cout << "==========================================" << std::endl;

    try {
        // Example 1: Constructors
        printSection("1. Constructors");

        // Default constructor
        printSubsection("Default Constructor");
        SmallVector<int, 8> v1;
        printVector(v1, "v1 (default)");

        // Constructor with count and default value
        printSubsection("Count Constructor");
        SmallVector<int, 8> v2(5);
        printVector(v2, "v2 (count)");

        // Constructor with count and specified value
        printSubsection("Count and Value Constructor");
        SmallVector<int, 8> v3(5, 42);
        printVector(v3, "v3 (count, value)");

        // Constructor with iterators
        printSubsection("Iterator Constructor");
        int arr[] = {1, 2, 3, 4, 5};
        SmallVector<int, 8> v4(std::begin(arr), std::end(arr));
        printVector(v4, "v4 (iterator range)");

        // Copy constructor
        printSubsection("Copy Constructor");
        SmallVector<int, 8> v5(v4);
        printVector(v5, "v5 (copy of v4)");

        // Move constructor
        printSubsection("Move Constructor");
        SmallVector<int, 8> v6(std::move(v5));
        printVector(v6, "v6 (moved from v5)");
        printVector(v5, "v5 (after move)");  // Should be empty

        // Initializer list constructor
        printSubsection("Initializer List Constructor");
        SmallVector<int, 8> v7 = {10, 20, 30, 40, 50};
        printVector(v7, "v7 (initializer list)");

        // Constructor with custom allocator
        printSubsection("Custom Allocator Constructor");
        TrackingAllocator<int>::resetCounters();
        SmallVector<int, 4, TrackingAllocator<int>> v8(10, 99);
        printVector(v8, "v8 (with tracking allocator)");
        std::cout << "Allocations: " << TrackingAllocator<int>::allocation_count
                  << ", Bytes: "
                  << TrackingAllocator<int>::total_bytes_allocated << std::endl;

        // Testing with larger vector forcing heap allocation
        printSubsection("Heap Allocation");
        SmallVector<int, 4> v9(10, 7);  // Exceeds inline capacity
        printVector(v9, "v9 (exceeds inline capacity)");
        std::cout << "Using inline storage: "
                  << (v9.isUsingInlineStorage() ? "true" : "false")
                  << std::endl;

        // Example 2: Assignment Operators
        printSection("2. Assignment Operators");

        // Copy assignment
        printSubsection("Copy Assignment");
        SmallVector<int, 8> v10;
        v10 = v7;
        printVector(v10, "v10 = v7 (copy)");

        // Move assignment
        printSubsection("Move Assignment");
        SmallVector<int, 8> v11;
        v11 = std::move(v10);
        printVector(v11, "v11 = std::move(v10)");
        printVector(v10, "v10 (after move)");  // Should be empty

        // Initializer list assignment
        printSubsection("Initializer List Assignment");
        SmallVector<int, 8> v12;
        v12 = {100, 200, 300};
        printVector(v12, "v12 = {100, 200, 300}");

        // Example 3: Assign Methods
        printSection("3. Assign Methods");

        // assign with count and value
        printSubsection("assign(count, value)");
        SmallVector<int, 8> v13;
        v13.assign(4, 25);
        printVector(v13, "v13.assign(4, 25)");

        // assign with iterators
        printSubsection("assign(first, last)");
        SmallVector<int, 8> v14;
        v14.assign(v7.begin(), v7.end());
        printVector(v14, "v14.assign(v7.begin(), v7.end())");

        // assign with initializer list
        printSubsection("assign(initializer_list)");
        SmallVector<int, 8> v15;
        v15.assign({5, 10, 15, 20});
        printVector(v15, "v15.assign({5, 10, 15, 20})");

        // Example 4: Element Access
        printSection("4. Element Access");

        SmallVector<int, 8> access_vec = {1, 2, 3, 4, 5};

        // operator[]
        printSubsection("operator[]");
        std::cout << "access_vec[0]: " << access_vec[0] << std::endl;
        std::cout << "access_vec[2]: " << access_vec[2] << std::endl;

        // at() method with bounds checking
        printSubsection("at() method");
        std::cout << "access_vec.at(1): " << access_vec.at(1) << std::endl;

        try {
            std::cout << "Attempting access_vec.at(10) (out of bounds): ";
            int val = access_vec.at(10);  // Should throw
            std::cout << val << std::endl;
        } catch (const std::out_of_range& e) {
            std::cout << "Exception caught: " << e.what() << std::endl;
        }

        // front() and back()
        printSubsection("front() and back()");
        std::cout << "access_vec.front(): " << access_vec.front() << std::endl;
        std::cout << "access_vec.back(): " << access_vec.back() << std::endl;

        // data() pointer
        printSubsection("data() method");
        const int* data_ptr = access_vec.data();
        std::cout << "First 3 elements via data(): ";
        for (size_t i = 0; i < 3; ++i) {
            std::cout << data_ptr[i] << " ";
        }
        std::cout << std::endl;

        // Example 5: Iterators
        printSection("5. Iterators");

        SmallVector<int, 8> iter_vec = {10, 20, 30, 40, 50};

        // begin/end
        printSubsection("Regular Iterators");
        std::cout << "Elements using begin()/end(): ";
        for (auto it = iter_vec.begin(); it != iter_vec.end(); ++it) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;

        // cbegin/cend
        printSubsection("Constant Iterators");
        std::cout << "Elements using cbegin()/cend(): ";
        for (auto it = iter_vec.cbegin(); it != iter_vec.cend(); ++it) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;

        // rbegin/rend
        printSubsection("Reverse Iterators");
        std::cout << "Elements using rbegin()/rend(): ";
        for (auto it = iter_vec.rbegin(); it != iter_vec.rend(); ++it) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;

        // crbegin/crend
        printSubsection("Constant Reverse Iterators");
        std::cout << "Elements using crbegin()/crend(): ";
        for (auto it = iter_vec.crbegin(); it != iter_vec.crend(); ++it) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;

        // Example 6: Capacity Methods
        printSection("6. Capacity Methods");

        SmallVector<int, 8> cap_vec;

        // empty, size, capacity
        printSubsection("empty(), size(), capacity()");
        std::cout << "Empty vector:" << std::endl;
        std::cout << "empty(): " << (cap_vec.empty() ? "true" : "false")
                  << std::endl;
        std::cout << "size(): " << cap_vec.size() << std::endl;
        std::cout << "capacity(): " << cap_vec.capacity() << std::endl;

        // After adding elements
        cap_vec = {1, 2, 3};
        std::cout << "\nAfter adding elements:" << std::endl;
        std::cout << "empty(): " << (cap_vec.empty() ? "true" : "false")
                  << std::endl;
        std::cout << "size(): " << cap_vec.size() << std::endl;
        std::cout << "capacity(): " << cap_vec.capacity() << std::endl;

        // reserve
        printSubsection("reserve()");
        std::cout << "Before reserve(20):" << std::endl;
        std::cout << "capacity(): " << cap_vec.capacity() << std::endl;
        std::cout << "isUsingInlineStorage(): "
                  << (cap_vec.isUsingInlineStorage() ? "true" : "false")
                  << std::endl;

        cap_vec.reserve(20);
        std::cout << "After reserve(20):" << std::endl;
        std::cout << "capacity(): " << cap_vec.capacity() << std::endl;
        std::cout << "isUsingInlineStorage(): "
                  << (cap_vec.isUsingInlineStorage() ? "true" : "false")
                  << std::endl;
        printVector(cap_vec, "cap_vec");

        // shrinkToFit
        printSubsection("shrinkToFit()");
        std::cout << "Before shrinkToFit():" << std::endl;
        std::cout << "size(): " << cap_vec.size() << std::endl;
        std::cout << "capacity(): " << cap_vec.capacity() << std::endl;

        cap_vec.shrinkToFit();
        std::cout << "After shrinkToFit():" << std::endl;
        std::cout << "size(): " << cap_vec.size() << std::endl;
        std::cout << "capacity(): " << cap_vec.capacity() << std::endl;
        std::cout << "isUsingInlineStorage(): "
                  << (cap_vec.isUsingInlineStorage() ? "true" : "false")
                  << std::endl;

        // maxSize
        printSubsection("maxSize()");
        std::cout << "maxSize(): " << cap_vec.maxSize() << std::endl;

        // Example 7: Modifiers
        printSection("7. Modifiers");

        // clear
        printSubsection("clear()");
        SmallVector<int, 8> mod_vec = {1, 2, 3, 4, 5};
        printVector(mod_vec, "Before clear()");
        mod_vec.clear();
        printVector(mod_vec, "After clear()");

        // insert single element
        printSubsection("insert() - single element");
        mod_vec = {10, 20, 40, 50};
        printVector(mod_vec, "Before insert");
        auto it_insert = mod_vec.insert(mod_vec.begin() + 2, 30);
        printVector(mod_vec, "After insert(pos, 30)");
        std::cout << "Return value points to: " << *it_insert << std::endl;

        // insert multiple copies
        printSubsection("insert() - multiple copies");
        printVector(mod_vec, "Before insert");
        it_insert = mod_vec.insert(mod_vec.end(), 3, 60);
        printVector(mod_vec, "After insert(end, 3, 60)");

        // insert range
        printSubsection("insert() - range");
        std::vector<int> source = {70, 80, 90};
        printVector(mod_vec, "Before insert");
        it_insert = mod_vec.insert(mod_vec.end(), source.begin(), source.end());
        printVector(mod_vec, "After insert(end, source.begin(), source.end())");

        // insert initializer list
        printSubsection("insert() - initializer list");
        printVector(mod_vec, "Before insert");
        it_insert = mod_vec.insert(mod_vec.begin(), {0, 5});
        printVector(mod_vec, "After insert(begin, {0, 5})");

        // emplace
        printSubsection("emplace()");
        SmallVector<std::string, 8> str_vec = {"hello", "world"};
        std::cout << "Before emplace: ";
        for (const auto& s : str_vec)
            std::cout << s << " ";
        std::cout << std::endl;

        str_vec.emplace(str_vec.begin() + 1, "beautiful");
        std::cout << "After emplace: ";
        for (const auto& s : str_vec)
            std::cout << s << " ";
        std::cout << std::endl;

        // erase - single element
        printSubsection("erase() - single element");
        printVector(mod_vec, "Before erase");
        auto it_erase = mod_vec.erase(mod_vec.begin() + 2);
        printVector(mod_vec, "After erase(begin+2)");
        std::cout << "Return value points to: " << *it_erase << std::endl;

        // erase - range
        printSubsection("erase() - range");
        printVector(mod_vec, "Before erase");
        it_erase = mod_vec.erase(mod_vec.begin() + 3, mod_vec.begin() + 6);
        printVector(mod_vec, "After erase(begin+3, begin+6)");

        // pushBack
        printSubsection("pushBack()");
        SmallVector<int, 4> push_vec;
        std::cout << "Initial: ";
        for (int i : push_vec)
            std::cout << i << " ";
        std::cout << std::endl;

        // Push elements with both lvalue and rvalue
        push_vec.pushBack(100);
        int val = 200;
        push_vec.pushBack(val);
        push_vec.pushBack(300);

        std::cout << "After pushBack: ";
        for (int i : push_vec)
            std::cout << i << " ";
        std::cout << std::endl;

        std::cout << "Size: " << push_vec.size()
                  << ", Capacity: " << push_vec.capacity() << ", Using inline: "
                  << (push_vec.isUsingInlineStorage() ? "yes" : "no")
                  << std::endl;

        // Push that forces reallocation
        push_vec.pushBack(400);
        push_vec.pushBack(500);

        std::cout << "After more pushBack: ";
        for (int i : push_vec)
            std::cout << i << " ";
        std::cout << std::endl;

        std::cout << "Size: " << push_vec.size()
                  << ", Capacity: " << push_vec.capacity() << ", Using inline: "
                  << (push_vec.isUsingInlineStorage() ? "yes" : "no")
                  << std::endl;

        // emplaceBack
        printSubsection("emplaceBack()");
        SmallVector<std::string, 4> emplace_vec;

        // Add elements with emplaceBack
        emplace_vec.emplaceBack("first");
        emplace_vec.emplaceBack(5, 'a');  // aaaaa
        emplace_vec.emplaceBack("third");

        std::cout << "After emplaceBack: ";
        for (const auto& s : emplace_vec)
            std::cout << "\"" << s << "\" ";
        std::cout << std::endl;

        // popBack
        printSubsection("popBack()");
        SmallVector<int, 8> pop_vec = {1, 2, 3, 4, 5};
        printVector(pop_vec, "Before popBack");
        pop_vec.popBack();
        printVector(pop_vec, "After popBack");
        pop_vec.popBack();
        printVector(pop_vec, "After another popBack");

        // resize - grow with default values
        printSubsection("resize() - grow with defaults");
        SmallVector<int, 8> resize_vec = {1, 2, 3};
        printVector(resize_vec, "Before resize(5)");
        resize_vec.resize(5);
        printVector(resize_vec, "After resize(5)");

        // resize - grow with specified value
        printSubsection("resize() - grow with specified value");
        printVector(resize_vec, "Before resize(8, 42)");
        resize_vec.resize(8, 42);
        printVector(resize_vec, "After resize(8, 42)");

        // resize - shrink
        printSubsection("resize() - shrink");
        printVector(resize_vec, "Before resize(4)");
        resize_vec.resize(4);
        printVector(resize_vec, "After resize(4)");

        // swap
        printSubsection("swap()");
        SmallVector<int, 8> swap_vec1 = {1, 2, 3};
        SmallVector<int, 8> swap_vec2 = {4, 5, 6, 7};

        printVector(swap_vec1, "swap_vec1 before swap");
        printVector(swap_vec2, "swap_vec2 before swap");

        swap_vec1.swap(swap_vec2);

        printVector(swap_vec1, "swap_vec1 after swap");
        printVector(swap_vec2, "swap_vec2 after swap");

        // global swap
        swap(swap_vec1, swap_vec2);

        printVector(swap_vec1, "swap_vec1 after global swap");
        printVector(swap_vec2, "swap_vec2 after global swap");

        // Example 8: Non-trivial Types
        printSection("8. Non-trivial Types");

        TestObject::resetCounters();

        {
            printSubsection("Basic operations with TestObject");

            SmallVector<TestObject, 4> obj_vec;
            std::cout << "Default constructor calls: "
                      << TestObject::constructor_calls << std::endl;

            std::cout << "Adding elements..." << std::endl;
            obj_vec.emplaceBack(1);
            obj_vec.emplaceBack(2);
            obj_vec.emplaceBack(3);

            std::cout << "TestObject vector: ";
            for (const auto& obj : obj_vec) {
                std::cout << obj.getValue() << " ";
            }
            std::cout << std::endl;

            std::cout << "Constructor calls: " << TestObject::constructor_calls
                      << std::endl;
            std::cout << "Copy constructor calls: "
                      << TestObject::copy_constructor_calls << std::endl;
            std::cout << "Move constructor calls: "
                      << TestObject::move_constructor_calls << std::endl;

            // Test copy
            SmallVector<TestObject, 4> copy_vec = obj_vec;

            std::cout << "After copy construction:" << std::endl;
            std::cout << "Copy constructor calls: "
                      << TestObject::copy_constructor_calls << std::endl;

            // Test move
            SmallVector<TestObject, 4> move_vec = std::move(copy_vec);

            std::cout << "After move construction:" << std::endl;
            std::cout << "Move constructor calls: "
                      << TestObject::move_constructor_calls << std::endl;

            // Force reallocation to test move operations
            std::cout << "Forcing reallocation..." << std::endl;
            move_vec.reserve(10);

            std::cout << "After reserve:" << std::endl;
            std::cout << "Move constructor calls: "
                      << TestObject::move_constructor_calls << std::endl;
        }

        std::cout << "After scope exit:" << std::endl;
        std::cout << "Destructor calls: " << TestObject::destructor_calls
                  << std::endl;

        // Example 9: Comparison Operators
        printSection("9. Comparison Operators");

        SmallVector<int, 8> comp_vec1 = {1, 2, 3, 4, 5};
        SmallVector<int, 8> comp_vec2 = {1, 2, 3, 4, 5};
        SmallVector<int, 8> comp_vec3 = {1, 2, 3, 4, 6};
        SmallVector<int, 8> comp_vec4 = {1, 2, 3};

        std::cout << "comp_vec1 == comp_vec2: "
                  << (comp_vec1 == comp_vec2 ? "true" : "false") << std::endl;
        std::cout << "comp_vec1 != comp_vec3: "
                  << (comp_vec1 != comp_vec3 ? "true" : "false") << std::endl;
        std::cout << "comp_vec1 < comp_vec3: "
                  << (comp_vec1 < comp_vec3 ? "true" : "false") << std::endl;
        std::cout << "comp_vec3 > comp_vec1: "
                  << (comp_vec3 > comp_vec1 ? "true" : "false") << std::endl;
        std::cout << "comp_vec1 <= comp_vec2: "
                  << (comp_vec1 <= comp_vec2 ? "true" : "false") << std::endl;
        std::cout << "comp_vec1 >= comp_vec4: "
                  << (comp_vec1 >= comp_vec4 ? "true" : "false") << std::endl;

        // Example 10: Allocator Awareness
        printSection("10. Allocator Awareness");

        printSubsection("Custom Allocator");
        TrackingAllocator<int>::resetCounters();

        {
            SmallVector<int, 4, TrackingAllocator<int>> alloc_vec1;
            std::cout << "Small vector created with inline storage..."
                      << std::endl;
            std::cout << "Allocations: "
                      << TrackingAllocator<int>::allocation_count << std::endl;

            // Force heap allocation
            alloc_vec1.resize(10, 42);
            std::cout << "After resize beyond inline capacity:" << std::endl;
            std::cout << "Allocations: "
                      << TrackingAllocator<int>::allocation_count << std::endl;
            std::cout << "Bytes allocated: "
                      << TrackingAllocator<int>::total_bytes_allocated
                      << std::endl;

            // Create another vector with the same allocator
            using AllocVecType = SmallVector<int, 4, TrackingAllocator<int>>;
            AllocVecType alloc_vec2(alloc_vec1.get_allocator());

            // Move assignment
            alloc_vec2 = std::move(alloc_vec1);
            std::cout << "After move assignment:" << std::endl;
            std::cout << "Allocations: "
                      << TrackingAllocator<int>::allocation_count << std::endl;
            std::cout << "Deallocations: "
                      << TrackingAllocator<int>::deallocation_count
                      << std::endl;
        }

        std::cout << "After scope exit:" << std::endl;
        std::cout << "Deallocations: "
                  << TrackingAllocator<int>::deallocation_count << std::endl;
        std::cout << "Bytes deallocated: "
                  << TrackingAllocator<int>::total_bytes_deallocated
                  << std::endl;

        // Example 11: Integration with Standard Algorithms
        printSection("11. Integration with Standard Algorithms");

        SmallVector<int, 8> algo_vec = {5, 2, 8, 1, 3};

        // sort
        printSubsection("std::sort");
        printVector(algo_vec, "Before sort");
        std::sort(algo_vec.begin(), algo_vec.end());
        printVector(algo_vec, "After sort");

        // find
        printSubsection("std::find");
        auto find_it = std::find(algo_vec.begin(), algo_vec.end(), 3);
        if (find_it != algo_vec.end()) {
            std::cout << "Found value 3 at position: "
                      << std::distance(algo_vec.begin(), find_it) << std::endl;
        }

        // transform
        printSubsection("std::transform");
        SmallVector<int, 8> transform_vec(algo_vec.size());
        std::transform(algo_vec.begin(), algo_vec.end(), transform_vec.begin(),
                       [](int x) { return x * 2; });
        printVector(transform_vec, "After transform (x*2)");

        // accumulate
        printSubsection("std::accumulate");
        int sum = std::accumulate(algo_vec.begin(), algo_vec.end(), 0);
        std::cout << "Sum of elements: " << sum << std::endl;

        // Example 12: Performance Comparison
        printSection("12. Performance Comparison");

        constexpr size_t test_size = 10000;
        constexpr int iterations = 5;

        // Time SmallVector operations
        printSubsection("Timing SmallVector operations");

        // Insertion test with SmallVector
        double small_vector_time = 0.0;
        for (int iter = 0; iter < iterations; ++iter) {
            SmallVector<int, 16> test_small;

            small_vector_time += measureTime([&]() {
                for (size_t i = 0; i < test_size; ++i) {
                    test_small.pushBack(static_cast<int>(i));
                }
            });
        }
        small_vector_time /= iterations;

        // Time std::vector operations
        printSubsection("Timing std::vector operations");

        // Insertion test with std::vector
        double std_vector_time = 0.0;
        for (int iter = 0; iter < iterations; ++iter) {
            std::vector<int> test_std;

            std_vector_time += measureTime([&]() {
                for (size_t i = 0; i < test_size; ++i) {
                    test_std.push_back(static_cast<int>(i));
                }
            });
        }
        std_vector_time /= iterations;

        // Compare results
        std::cout << "Average time for " << test_size
                  << " insertions:" << std::endl;
        std::cout << "SmallVector: " << small_vector_time << " µs" << std::endl;
        std::cout << "std::vector: " << std_vector_time << " µs" << std::endl;
        std::cout << "Ratio (std::vector / SmallVector): "
                  << (std_vector_time / small_vector_time) << std::endl;

        // Example 13: Edge Cases
        printSection("13. Edge Cases");

        // Empty vector operations
        printSubsection("Empty Vector Operations");
        SmallVector<int, 8> empty_vec;
        std::cout << "Size: " << empty_vec.size() << std::endl;
        std::cout << "Capacity: " << empty_vec.capacity() << std::endl;
        std::cout << "Empty: " << (empty_vec.empty() ? "true" : "false")
                  << std::endl;

        try {
            std::cout << "Attempting popBack() on empty vector: ";
            empty_vec.popBack();
            std::cout << "This should not be printed!" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Error as expected: " << e.what() << std::endl;
        }

        // Stress test with many reallocations
        printSubsection("Reallocation Stress Test");
        SmallVector<int, 4> stress_vec;

        std::cout << "Adding 1000 elements to small inline vector..."
                  << std::endl;
        for (int i = 0; i < 1000; ++i) {
            stress_vec.pushBack(i);
        }

        std::cout << "Final capacity: " << stress_vec.capacity() << std::endl;
        std::cout << "Final size: " << stress_vec.size() << std::endl;

        // Test with objects that throw on copy/move
        printSubsection("Exception Safety");
        std::cout << "Note: Full exception safety testing would require a type "
                     "that throws on demand.\n"
                  << "      This example doesn't perform actual throws but "
                     "demonstrates the pattern."
                  << std::endl;

        std::cout << "\nAll examples completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
