/**
 * @file test_basic_cpp.cpp
 * @brief Basic C++ test file for build verification
 * 
 * This file provides a simple C++ executable to verify the build system
 * is working correctly without dependencies on Atom modules.
 */

#include <iostream>
#include <string>
#include <vector>
#include <memory>

/**
 * @brief Simple test class to verify C++ features
 */
class BasicTest {
public:
    BasicTest(const std::string& name) : name_(name) {
        std::cout << "Created BasicTest: " << name_ << std::endl;
    }
    
    ~BasicTest() {
        std::cout << "Destroyed BasicTest: " << name_ << std::endl;
    }
    
    void run() {
        std::cout << "Running test: " << name_ << std::endl;
        test_cpp20_features();
        test_stl_containers();
        test_smart_pointers();
    }

private:
    std::string name_;
    
    void test_cpp20_features() {
        std::cout << "  Testing C++20 features..." << std::endl;
        
        // Auto type deduction
        auto value = 42;
        
        // Range-based for loop
        std::vector<int> numbers = {1, 2, 3, 4, 5};
        for (const auto& num : numbers) {
            // Lambda with auto parameters
            auto square = [](auto x) { return x * x; };
            std::cout << "    " << num << " squared = " << square(num) << std::endl;
        }
    }
    
    void test_stl_containers() {
        std::cout << "  Testing STL containers..." << std::endl;
        std::vector<std::string> strings = {"hello", "world", "atom", "project"};
        
        for (const auto& str : strings) {
            std::cout << "    String: " << str << std::endl;
        }
    }
    
    void test_smart_pointers() {
        std::cout << "  Testing smart pointers..." << std::endl;
        
        auto ptr = std::make_unique<int>(100);
        std::cout << "    Unique pointer value: " << *ptr << std::endl;
        
        auto shared1 = std::make_shared<std::string>("shared_resource");
        auto shared2 = shared1;
        std::cout << "    Shared pointer use count: " << shared1.use_count() << std::endl;
        std::cout << "    Shared pointer value: " << *shared1 << std::endl;
    }
};

int main() {
    std::cout << "=== Basic C++ Build Test ===" << std::endl;
    
    try {
        BasicTest test("C++20 Compatibility Test");
        test.run();
        
        std::cout << "=== All tests passed! ===" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
}
