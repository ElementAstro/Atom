#include "atom/function/abi.hpp"

#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <thread>
#include <vector>

// Example of custom types
template <typename T, typename U>
class MyCustomClass {
public:
    T data;
    U otherData;
};

// Custom class hierarchy
struct Base {
    virtual ~Base() = default;
    virtual void doSomething() = 0;
};

struct Derived : Base {
    void doSomething() override {}
};

// Template for complex type generation
template <int N>
struct ComplexTemplate {
    using type = std::pair<typename ComplexTemplate<N - 1>::type,
                           typename ComplexTemplate<N - 2>::type>;
};

template <>
struct ComplexTemplate<1> {
    using type = int;
};

template <>
struct ComplexTemplate<0> {
    using type = double;
};

// Helper function to separate different sections of the output
void printSection(const std::string& title) {
    std::cout << "\n\n" << std::string(80, '=') << std::endl;
    std::cout << " " << title << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

int main() {
    std::cout << "Comprehensive Example of ABI Parsing Tool Library"
              << std::endl;

    //---------------------------------------------------------------------
    printSection("1. Basic Type Parsing");
    //---------------------------------------------------------------------

    std::cout << "Basic Types:" << std::endl;
    std::cout << "  int: " << atom::meta::DemangleHelper::demangleType<int>()
              << std::endl;
    std::cout << "  double: "
              << atom::meta::DemangleHelper::demangleType<double>()
              << std::endl;
    std::cout << "  std::string: "
              << atom::meta::DemangleHelper::demangleType<std::string>()
              << std::endl;

    std::cout << "\nPointers and References:" << std::endl;
    std::cout << "  int*: " << atom::meta::DemangleHelper::demangleType<int*>()
              << std::endl;
    std::cout << "  const char*: "
              << atom::meta::DemangleHelper::demangleType<const char*>()
              << std::endl;
    std::cout << "  int&: " << atom::meta::DemangleHelper::demangleType<int&>()
              << std::endl;

    std::cout << "\nStandard Container Types:" << std::endl;
    std::cout << "  std::vector<int>: "
              << atom::meta::DemangleHelper::demangleType<std::vector<int>>()
              << std::endl;
    std::cout << "  std::map<int, std::string>: "
              << atom::meta::DemangleHelper::demangleType<
                     std::map<int, std::string>>()
              << std::endl;

    //---------------------------------------------------------------------
    printSection("2. Complex Type Parsing");
    //---------------------------------------------------------------------

    // Define some complex types
    using ComplexType1 =
        std::map<std::string, std::vector<std::pair<int, double>>>;
    using ComplexType2 = std::function<int(std::vector<std::string>&, double)>;
    using ComplexType3 =
        std::shared_ptr<std::map<int, MyCustomClass<float, std::string>>>;
    using ComplexType4 =
        typename ComplexTemplate<5>::type;  // Recursive template type

    std::cout << "Complex Type Parsing:" << std::endl;
    std::cout << "  Type1: "
              << atom::meta::DemangleHelper::demangleType<ComplexType1>()
              << std::endl;
    std::cout << "  Type2: "
              << atom::meta::DemangleHelper::demangleType<ComplexType2>()
              << std::endl;
    std::cout << "  Type3: "
              << atom::meta::DemangleHelper::demangleType<ComplexType3>()
              << std::endl;
    std::cout << "  Type4: "
              << atom::meta::DemangleHelper::demangleType<ComplexType4>()
              << std::endl;

    // Parse type from instance
    std::vector<int> myVector{1, 2, 3};
    std::function<void(int)> myFunction = [](int x) { std::cout << x; };

    std::cout << "\nGetting Type from Instance:" << std::endl;
    std::cout << "  myVector: "
              << atom::meta::DemangleHelper::demangleType(myVector)
              << std::endl;
    std::cout << "  myFunction: "
              << atom::meta::DemangleHelper::demangleType(myFunction)
              << std::endl;

    // With source location information
    std::cout << "\nType with Source Location Info:" << std::endl;
    std::cout << "  "
              << atom::meta::DemangleHelper::demangle(
                     typeid(ComplexType1).name(),
                     std::source_location::current())
              << std::endl;

    //---------------------------------------------------------------------
    printSection("3. Batch Parsing of Multiple Types");
    //---------------------------------------------------------------------

    std::vector<std::string_view> mangledNames{
        typeid(int).name(), typeid(std::string).name(),
        typeid(std::vector<int>).name(), typeid(ComplexType1).name(),
        typeid(myFunction).name()};

    std::cout << "Batch Parsing Results:" << std::endl;
    auto demangledNames =
        atom::meta::DemangleHelper::demangleMany(mangledNames);
    for (size_t i = 0; i < demangledNames.size(); ++i) {
        std::cout << "  " << i + 1 << ". " << mangledNames[i] << " -> "
                  << demangledNames[i] << std::endl;
    }

#if defined(ENABLE_DEBUG) || defined(ATOM_META_ENABLE_VISUALIZATION)
    //---------------------------------------------------------------------
    printSection("4. Type Visualization");
    //---------------------------------------------------------------------

    std::cout << "Basic Type Visualization:" << std::endl;
    std::cout << "int*:\n"
              << atom::meta::DemangleHelper::visualizeType<int*>() << std::endl;

    std::cout << "\nSTL Container Visualization:" << std::endl;
    std::cout << "std::vector<int>:\n"
              << atom::meta::DemangleHelper::visualizeType<std::vector<int>>()
              << std::endl;

    std::cout << "\nFunction Type Visualization:" << std::endl;
    using FunctionType = int (*)(double, char);
    std::cout << atom::meta::DemangleHelper::visualizeType<FunctionType>()
              << std::endl;

    std::cout << "\nComplex Nested Type Visualization:" << std::endl;
    std::cout << atom::meta::DemangleHelper::visualizeType<ComplexType1>()
              << std::endl;

    std::cout << "\nCustom Template Class Visualization:" << std::endl;
    using CustomType = MyCustomClass<int, std::string>;
    std::cout << atom::meta::DemangleHelper::visualizeType<CustomType>()
              << std::endl;
#else
    std::cout << "\nType visualization feature is not enabled. Define "
                 "ENABLE_DEBUG or ATOM_META_ENABLE_VISUALIZATION macro."
              << std::endl;
#endif

    //---------------------------------------------------------------------
    printSection("6. Dynamic Type Identification");
    //---------------------------------------------------------------------

    // Create a polymorphic object
    std::unique_ptr<Base> basePtr = std::make_unique<Derived>();

    std::cout << "Polymorphic Type Example:" << std::endl;
    std::cout << "  Static Type: "
              << atom::meta::DemangleHelper::demangleType<decltype(basePtr)>()
              << std::endl;
    std::cout << "  Dynamic Type: "
              << atom::meta::DemangleHelper::demangle(typeid(*basePtr).name())
              << std::endl;

    //---------------------------------------------------------------------
    printSection("7. Exception Handling Example");
    //---------------------------------------------------------------------

    try {
        // Attempt to parse an invalid function name
        std::cout << "Attempting to parse an invalid symbol name..."
                  << std::endl;
        std::cout << atom::meta::DemangleHelper::demangle(
                         "___invalid_mangled_name___")
                  << std::endl;
    } catch (const atom::meta::AbiException& e) {
        std::cout << "Caught ABI Exception: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Caught Standard Exception: " << e.what() << std::endl;
    }

    //---------------------------------------------------------------------
    printSection("8. Cache Performance Testing");
    //---------------------------------------------------------------------

    // Check initial cache state
    std::cout << "Initial Cache State: "
              << atom::meta::DemangleHelper::cacheSize() << " items"
              << std::endl;

    // Create a complex type for performance testing
    using VeryComplexType = typename ComplexTemplate<8>::type;

    std::cout << "Performance Test - Parsing Complex Type:" << std::endl;

    // First call (no cache)
    auto start1 = std::chrono::high_resolution_clock::now();
    std::string result =
        atom::meta::DemangleHelper::demangleType<VeryComplexType>();
    auto end1 = std::chrono::high_resolution_clock::now();

    // Second call (with cache)
    auto start2 = std::chrono::high_resolution_clock::now();
    std::string result2 =
        atom::meta::DemangleHelper::demangleType<VeryComplexType>();
    auto end2 = std::chrono::high_resolution_clock::now();

    auto firstCallDuration =
        std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1)
            .count();
    auto secondCallDuration =
        std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2)
            .count();

    std::cout << "  Complex Type Name Length: " << result.length()
              << " characters" << std::endl;
    std::cout << "  First Parse Time: " << firstCallDuration << " microseconds"
              << std::endl;
    std::cout << "  Cached Parse Time: " << secondCallDuration
              << " microseconds" << std::endl;
    std::cout << "  Speedup: "
              << (firstCallDuration > 0
                      ? static_cast<double>(firstCallDuration) /
                            secondCallDuration
                      : 0)
              << "x" << std::endl;

    // Batch test
    const int iterations = 1000;
    std::cout << "\nBatch Test - " << iterations << " Parses:" << std::endl;

    auto batchStart = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        std::string mangledName =
            "type_" + std::to_string(i % 100);  // Use 100 different type names
        atom::meta::DemangleHelper::demangle(mangledName);
    }
    auto batchEnd = std::chrono::high_resolution_clock::now();

    auto batchDuration = std::chrono::duration_cast<std::chrono::microseconds>(
                             batchEnd - batchStart)
                             .count();

    std::cout << "  Total Time: " << batchDuration << " microseconds"
              << std::endl;
    std::cout << "  Average Time per Parse: "
              << (batchDuration / static_cast<double>(iterations))
              << " microseconds" << std::endl;
    std::cout << "  Throughput: " << (iterations * 1000000.0 / batchDuration)
              << " ops/sec" << std::endl;

    // Test cache management
    std::cout << "\nCache Management Test:" << std::endl;
    std::cout << "  Current Cache Size: "
              << atom::meta::DemangleHelper::cacheSize() << " items"
              << std::endl;

    // Clear cache
    atom::meta::DemangleHelper::clearCache();
    std::cout << "  Cache Size After Clear: "
              << atom::meta::DemangleHelper::cacheSize() << " items"
              << std::endl;

    // Add a large number of items to test automatic cache management
    std::cout << "  Adding 1500 Items to Cache..." << std::endl;
    for (int i = 0; i < 1500; ++i) {
        atom::meta::DemangleHelper::demangle("auto_test_type_" +
                                             std::to_string(i));
    }

    std::cout << "  Size After Automatic Cache Management: "
              << atom::meta::DemangleHelper::cacheSize() << " items"
              << std::endl;
    if (atom::meta::DemangleHelper::cacheSize() <=
        atom::meta::AbiConfig::max_cache_size) {
        std::cout << "  ✓ Success: Cache size remains within the configured "
                     "maximum limit ("
                  << atom::meta::AbiConfig::max_cache_size << ")" << std::endl;
    } else {
        std::cout << "  ✗ Failure: Cache size exceeds the configured maximum "
                     "limit"
                  << std::endl;
    }

    //---------------------------------------------------------------------
    printSection("9. Multi-threading Test");
    //---------------------------------------------------------------------

    std::cout << "Multi-threading Test:" << std::endl;

    // Clear the previous cache
    atom::meta::DemangleHelper::clearCache();

    const int numThreads = 4;
    const int itemsPerThread = 250;
    std::vector<std::thread> threads;

    auto threadFunction = [](int id, int items) {
        for (int i = 0; i < items; ++i) {
            std::string name =
                "thread_" + std::to_string(id) + "_type_" + std::to_string(i);
            auto result = atom::meta::DemangleHelper::demangle(name);
            // Ensure the compiler doesn't optimize away the result
            if (result.empty()) {
                std::cout << "Empty Result" << std::endl;
            }
        }
    };

    auto threadStart = std::chrono::high_resolution_clock::now();

    // Create and start threads
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(threadFunction, i, itemsPerThread);
    }

    // Wait for all threads to complete
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    auto threadEnd = std::chrono::high_resolution_clock::now();
    auto threadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                              threadEnd - threadStart)
                              .count();

    std::cout << "  " << numThreads << " threads processed a total of "
              << (numThreads * itemsPerThread)
              << " operations, taking: " << threadDuration << " milliseconds"
              << std::endl;
    std::cout << "  Throughput: "
              << (numThreads * itemsPerThread * 1000.0 / threadDuration)
              << " ops/sec" << std::endl;
    std::cout << "  Final Cache Size: "
              << atom::meta::DemangleHelper::cacheSize() << " items"
              << std::endl;

    //---------------------------------------------------------------------
    printSection("End of Example");
    //---------------------------------------------------------------------

    std::cout << "End of ABI Parsing Tool Library Example\n" << std::endl;

    return 0;
}