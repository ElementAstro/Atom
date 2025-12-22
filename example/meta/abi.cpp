#include "atom/meta/abi.hpp"

#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <span>
#include <thread>
#include <tuple>
#include <variant>
#include <vector>

// Example of custom typestemplate <typename T, typename U>
class MyCustomClass {
public:
    T data;
    U otherData;
};

// Custom class hierarchystruct Base {
virtual ~Base() = default;
virtual void doSomething() = 0;
}
;

struct Derived : Base {
    void doSomething() override {}
};

// Template for complex type generationtemplate <int N>
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

// Helper function to separate different sections of the outputvoid
// printSection(const std::string& title) {
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
    printSection("3. NEW: Type Analysis Utilities");
    //---------------------------------------------------------------------

    std::cout << "GetBareTypeName Examples:" << std::endl;
    std::cout << "  'const std::vector<int>' -> '"
              << atom::meta::DemangleHelper::getBareTypeName(
                     "const std::vector<int>")
              << "'" << std::endl;
    std::cout << "  'std::map<int, string>' -> '"
              << atom::meta::DemangleHelper::getBareTypeName(
                     "std::map<int, string>")
              << "'" << std::endl;
    std::cout << "  'atom::meta::DemangleHelper' -> '"
              << atom::meta::DemangleHelper::getBareTypeName(
                     "atom::meta::DemangleHelper")
              << "'" << std::endl;

    std::cout << "\nExtractNamespace Examples:" << std::endl;
    std::cout << "  'std::string' -> '"
              << atom::meta::DemangleHelper::extractNamespace("std::string")
              << "'" << std::endl;
    std::cout << "  'atom::meta::DemangleHelper' -> '"
              << atom::meta::DemangleHelper::extractNamespace(
                     "atom::meta::DemangleHelper")
              << "'" << std::endl;
    std::cout << "  'int' -> '"
              << atom::meta::DemangleHelper::extractNamespace("int")
              << "' (empty for non-namespaced types)" << std::endl;

    std::cout << "\nExtractTemplateArgs Examples:" << std::endl;
    auto args1 = atom::meta::DemangleHelper::extractTemplateArgs("vector<int>");
    std::cout << "  'vector<int>' args: ";
    for (const auto& arg : args1)
        std::cout << "'" << arg << "' ";
    std::cout << std::endl;

    auto args2 = atom::meta::DemangleHelper::extractTemplateArgs(
        "map<string, vector<int>>");
    std::cout << "  'map<string, vector<int>>' args: ";
    for (const auto& arg : args2)
        std::cout << "'" << arg << "' ";
    std::cout << std::endl;

    //---------------------------------------------------------------------
    printSection("4. NEW: Type Classification");
    //---------------------------------------------------------------------

    std::cout << "isPointerType Examples:" << std::endl;
    std::cout << "  'int*' -> "
              << (atom::meta::DemangleHelper::isPointerType("int*") ? "true"
                                                                    : "false")
              << std::endl;
    std::cout << "  'int' -> "
              << (atom::meta::DemangleHelper::isPointerType("int") ? "true"
                                                                   : "false")
              << std::endl;

    std::cout << "\nisReferenceType Examples:" << std::endl;
    std::cout << "  'int&' -> "
              << (atom::meta::DemangleHelper::isReferenceType("int&") ? "true"
                                                                      : "false")
              << std::endl;
    std::cout << "  'int&&' -> "
              << (atom::meta::DemangleHelper::isReferenceType("int&&")
                      ? "true"
                      : "false")
              << std::endl;
    std::cout << "  'int' -> "
              << (atom::meta::DemangleHelper::isReferenceType("int") ? "true"
                                                                     : "false")
              << std::endl;

    std::cout << "\nisConstType Examples:" << std::endl;
    std::cout << "  'const int' -> "
              << (atom::meta::DemangleHelper::isConstType("const int")
                      ? "true"
                      : "false")
              << std::endl;
    std::cout << "  'int const*' -> "
              << (atom::meta::DemangleHelper::isConstType("int const*")
                      ? "true"
                      : "false")
              << std::endl;
    std::cout << "  'int' -> "
              << (atom::meta::DemangleHelper::isConstType("int") ? "true"
                                                                 : "false")
              << std::endl;

    std::cout << "\nisTemplateType Examples:" << std::endl;
    auto vecType = atom::meta::DemangleHelper::demangleType<std::vector<int>>();
    auto intType = atom::meta::DemangleHelper::demangleType<int>();
    std::cout << "  vector<int> -> "
              << (atom::meta::DemangleHelper::isTemplateType(vecType) ? "true"
                                                                      : "false")
              << std::endl;
    std::cout << "  int -> "
              << (atom::meta::DemangleHelper::isTemplateType(intType) ? "true"
                                                                      : "false")
              << std::endl;

    //---------------------------------------------------------------------
    printSection("5. NEW: Type Category Detection");
    //---------------------------------------------------------------------

    std::cout << "getTypeCategory Examples:" << std::endl;
    std::cout << "  void: "
              << atom::meta::DemangleHelper::getTypeCategory<void>()
              << std::endl;
    std::cout << "  int: " << atom::meta::DemangleHelper::getTypeCategory<int>()
              << std::endl;
    std::cout << "  double: "
              << atom::meta::DemangleHelper::getTypeCategory<double>()
              << std::endl;
    std::cout << "  int[10]: "
              << atom::meta::DemangleHelper::getTypeCategory<int[10]>()
              << std::endl;

    enum class MyEnum { A, B };
    std::cout << "  enum class: "
              << atom::meta::DemangleHelper::getTypeCategory<MyEnum>()
              << std::endl;
    std::cout << "  std::string: "
              << atom::meta::DemangleHelper::getTypeCategory<std::string>()
              << std::endl;
    std::cout << "  int*: "
              << atom::meta::DemangleHelper::getTypeCategory<int*>()
              << std::endl;
    std::cout << "  int&: "
              << atom::meta::DemangleHelper::getTypeCategory<int&>()
              << std::endl;
    std::cout << "  int&&: "
              << atom::meta::DemangleHelper::getTypeCategory<int&&>()
              << std::endl;

    struct TestStruct {
        int member;
        void func() {}
    };
    std::cout
        << "  member pointer: "
        << atom::meta::DemangleHelper::getTypeCategory<int TestStruct::*>()
        << std::endl;
    std::cout
        << "  member function pointer: "
        << atom::meta::DemangleHelper::getTypeCategory<void (TestStruct::*)()>()
        << std::endl;

    //---------------------------------------------------------------------
    printSection("6. NEW: TryDemangle (No-throw API)");
    //---------------------------------------------------------------------

    std::cout << "tryDemangle Examples:" << std::endl;

    auto result1 = atom::meta::DemangleHelper::tryDemangle(typeid(int).name());
    std::cout << "  int -> hasValue: "
              << (result1.hasValue() ? "true" : "false")
              << ", value: " << result1.value << std::endl;

    auto result2 =
        atom::meta::DemangleHelper::tryDemangle("invalid_mangled_name");
    std::cout << "  invalid name -> hasValue: "
              << (result2.hasValue() ? "true" : "false")
              << ", error code: " << static_cast<int>(result2.error)
              << std::endl;

    //---------------------------------------------------------------------
    printSection("7. Batch Parsing of Multiple Types");
    //---------------------------------------------------------------------

    atom::meta::containers::Vector<std::string_view> mangledNames{
        typeid(int).name(), typeid(std::string).name(),
        typeid(std::vector<int>).name(), typeid(ComplexType1).name()};

    std::cout << "" Batch Parsing Results : "" << std::endl;
    auto demangledNames =
        atom::meta::DemangleHelper::demangleMany(mangledNames);
    for (size_t i = 0; i < demangledNames.size(); ++i) {
        std::cout << ""
                     ""
                  << i + 1 << ""."" << demangledNames[i] << std::endl;
    }

#if defined(ENABLE_DEBUG) || defined(ATOM_META_ENABLE_VISUALIZATION)
    //---------------------------------------------------------------------
    printSection("" 8. Type Visualization "");
    //---------------------------------------------------------------------

    std::cout << "" Basic Type Visualization : "" << std::endl;
    std::cout << "" int * :\n ""
              << atom::meta::DemangleHelper::visualizeType<int*>() << std::endl;

    std::cout << ""\nSTL Container Visualization : "" << std::endl;
    std::cout << "" std::vector<int> :\n ""
              << atom::meta::DemangleHelper::visualizeType<std::vector<int>>()
              << std::endl;

    std::cout << ""\nFunction Type Visualization : "" << std::endl;
    using FunctionType = int (*)(double, char);
    std::cout << atom::meta::DemangleHelper::visualizeType<FunctionType>()
              << std::endl;

    std::cout << ""\nComplex Nested Type Visualization : "" << std::endl;
    std::cout << atom::meta::DemangleHelper::visualizeType<ComplexType1>()
              << std::endl;

    std::cout << ""\nCustom Template Class Visualization : "" << std::endl;
    using CustomType = MyCustomClass<int, std::string>;
    std::cout << atom::meta::DemangleHelper::visualizeType<CustomType>()
              << std::endl;
#else
    std::cout << ""\nType visualization feature is not enabled.Define
                 ""
                 "" ENABLE_DEBUG or
        ATOM_META_ENABLE_VISUALIZATION macro."" << std::endl;
#endif

    //---------------------------------------------------------------------
    printSection("" 9. Dynamic Type Identification "");
    //---------------------------------------------------------------------

    // Create a polymorphic object
    std::unique_ptr<Base> basePtr = std::make_unique<Derived>();

    std::cout << "" Polymorphic Type Example : "" << std::endl;
    std::cout << "" Static Type : ""
              << atom::meta::DemangleHelper::demangleType<decltype(basePtr)>()
              << std::endl;
    std::cout << "" Dynamic Type : ""
              << atom::meta::DemangleHelper::demangle(typeid(*basePtr).name())
              << std::endl;

    //---------------------------------------------------------------------
    printSection("" 10. Cache Performance Testing "");
    //---------------------------------------------------------------------

    // Check initial cache state
    std::cout << "" Initial Cache State : ""
              << atom::meta::DemangleHelper::cacheSize()
              << "" items ""
              << std::endl;

    // Create a complex type for performance testing
    using VeryComplexType = typename ComplexTemplate<8>::type;

    std::cout << "" Performance Test - Parsing Complex Type : "" << std::endl;

    // First call (no cache)
    auto start1 = std::chrono::high_resolution_clock::now();
    atom::meta::String result =
        atom::meta::DemangleHelper::demangleType<VeryComplexType>();
    auto end1 = std::chrono::high_resolution_clock::now();

    // Second call (with cache)
    auto start2 = std::chrono::high_resolution_clock::now();
    atom::meta::String result2 =
        atom::meta::DemangleHelper::demangleType<VeryComplexType>();
    auto end2 = std::chrono::high_resolution_clock::now();

    auto firstCallDuration =
        std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1)
            .count();
    auto secondCallDuration =
        std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2)
            .count();

    std::cout << "" Complex Type Name Length : "" << result.length()
              << "" characters ""
              << std::endl;
    std::cout << "" First Parse Time : "" << firstCallDuration
              << "" microseconds ""
              << std::endl;
    std::cout << "" Cached Parse Time : "" << secondCallDuration
              << "" microseconds ""
              << std::endl;
    std::cout << "" Speedup : ""
              << (firstCallDuration > 0 && secondCallDuration > 0
                      ? static_cast<double>(firstCallDuration) /
                            secondCallDuration
                      : 0)
              << "" x ""
              << std::endl;

    // Cache management test
    std::cout << ""\nCache Management Test : "" << std::endl;
    std::cout << "" Current Cache Size : ""
              << atom::meta::DemangleHelper::cacheSize()
              << "" items ""
              << std::endl;

    // Clear cache
    atom::meta::DemangleHelper::clearCache();
    std::cout << "" Cache Size After Clear : ""
              << atom::meta::DemangleHelper::cacheSize()
              << "" items ""
              << std::endl;

    // Add items to test automatic cache management
    std::cout << "" Adding 1500 Items to Cache... "" << std::endl;
    for (int i = 0; i < 1500; ++i) {
        atom::meta::DemangleHelper::demangle("" auto_test_type_ "" +
                                             std::to_string(i));
    }

    std::cout << "" Size After Automatic Cache Management : ""
              << atom::meta::DemangleHelper::cacheSize()
              << "" items ""
              << std::endl;
    if (atom::meta::DemangleHelper::cacheSize() <=
        atom::meta::AbiConfig::max_cache_size) {
        std::cout << "" Success
            : Cache size within configured maximum(
                  "" << atom::meta::AbiConfig::max_cache_size << "") ""
                  << std::endl;
    }

    //---------------------------------------------------------------------
    printSection("" 11. Multi - threading Test "");
    //---------------------------------------------------------------------

    std::cout << "" Multi - threading Test : "" << std::endl;

    // Clear the previous cache
    atom::meta::DemangleHelper::clearCache();

    const int numThreads = 4;
    const int itemsPerThread = 250;
    std::vector<std::thread> threads;

    auto threadFunction = [](int id, int items) {
        for (int i = 0; i < items; ++i) {
            std::string name = "" thread_ "" + std::to_string(id) +
                               ""_type_
                               "" +
                               std::to_string(i);
            auto result = atom::meta::DemangleHelper::demangle(name);
            if (result.empty()) {
                std::cout << "" Empty Result "" << std::endl;
            }
        }
    };

    auto threadStart = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(threadFunction, i, itemsPerThread);
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    auto threadEnd = std::chrono::high_resolution_clock::now();
    auto threadDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                              threadEnd - threadStart)
                              .count();

    std::cout << ""
                 ""
              << numThreads << "" threads processed ""
              << (numThreads * itemsPerThread) << "" operations in ""
              << threadDuration << "" ms "" << std::endl;
    std::cout << "" Throughput : ""
              << (numThreads * itemsPerThread * 1000.0 / threadDuration)
              << "" ops / sec ""
              << std::endl;
    std::cout << "" Final Cache Size : ""
              << atom::meta::DemangleHelper::cacheSize()
              << "" items ""
              << std::endl;

    //---------------------------------------------------------------------
    printSection("" 12. AbiConfig Information "");
    //---------------------------------------------------------------------

    std::cout << "" AbiConfig Values : "" << std::endl;
    std::cout << "" buffer_size : "" << atom::meta::AbiConfig::buffer_size
              << std::endl;
    std::cout << "" max_cache_size : "" << atom::meta::AbiConfig::max_cache_size
              << std::endl;
    std::cout << "" thread_safe_cache : ""
              << (atom::meta::AbiConfig::thread_safe_cache ? "" true ""
                                                           : "" false "")
              << std::endl;
    std::cout << "" enable_lru_eviction : ""
              << (atom::meta::AbiConfig::enable_lru_eviction ? "" true ""
                                                             : "" false "")
              << std::endl;
    std::cout << "" eviction_batch_size : ""
              << atom::meta::AbiConfig::eviction_batch_size
              << std::endl;

    //---------------------------------------------------------------------
    printSection("" End of Example "");
    //---------------------------------------------------------------------

    std::cout << "" End of ABI Parsing Tool Library Example\n "" << std::endl;

    return 0;
}
