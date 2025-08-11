/**
 * Comprehensive examples for atom::meta utility functions (god.hpp)
 *
 * This file demonstrates all functionality provided in the god.hpp header:
 * 1. Basic utilities (casting, enum handling)
 * 2. Alignment functions
 * 3. Math utilities
 * 4. Memory operations
 * 5. Atomic operations
 * 6. Type traits and type manipulation
 * 7. Resource management (ScopeGuard, singleton)
 *
 * @author Example Author
 * @date 2025-03-21
 */

#include "atom/meta/god.hpp"
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

// Example enum types for testing enum utilities
enum class Color : uint8_t { Red = 0, Green = 1, Blue = 2 };

enum class ColorCode : uint16_t { Red = 0, Green = 1, Blue = 2, Alpha = 3 };

// Example classes for testing type traits
class Base {
public:
    virtual ~Base() = default;
    virtual void foo() { std::cout << "Base::foo()" << std::endl; }
};

class Derived : public Base {
public:
    void foo() override { std::cout << "Derived::foo()" << std::endl; }
};

// Simple POD struct for alignment tests
struct alignas(16) AlignedStruct {
    double value;
    int counter;
};

// Forward declarations
void demonstrateBasicUtilities();
void demonstrateAlignmentFunctions();
void demonstrateMathFunctions();
void demonstrateMemoryFunctions();
void demonstrateAtomicOperations();
void demonstrateTypeTraits();
void demonstrateResourceManagement();

// Print separator for examples
void printSeparator(const std::string& title) {
    std::cout << "\n=================================================="
              << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==================================================\n"
              << std::endl;
}

int main() {
    std::cout << "================================================"
              << std::endl;
    std::cout << "  atom::meta Utility Functions Examples" << std::endl;
    std::cout << "================================================"
              << std::endl;

    // Call atom::meta::blessNoBugs() for good luck!
    atom::meta::blessNoBugs();

    try {
        demonstrateBasicUtilities();
        demonstrateAlignmentFunctions();
        demonstrateMathFunctions();
        demonstrateMemoryFunctions();
        demonstrateAtomicOperations();
        demonstrateTypeTraits();
        demonstrateResourceManagement();

        std::cout << "\nAll demonstrations completed successfully!"
                  << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}

//==============================================================================
// 1. Basic Utilities
//==============================================================================
void demonstrateBasicUtilities() {
    printSeparator("1. Basic Utilities");

    // Demonstrate cast() function
    {
        std::cout << "cast<T>() Examples:" << std::endl;

        double pi = 3.14159265359;
        int rounded = atom::meta::cast<int>(pi);
        std::cout << "  cast<int>(3.14159265359) = " << rounded << std::endl;

        const char* str = "12345";
        intptr_t ptr_value =
            reinterpret_cast<intptr_t>(static_cast<const void*>(str));
        std::cout << "  reinterpret_cast<intptr_t>(\"12345\") = 0x" << std::hex
                  << ptr_value << std::dec << std::endl;

        // Cast with move semantics
        std::string source = "Hello World";
        std::string destination =
            atom::meta::cast<std::string>(std::move(source));
        std::cout << "  cast with move: destination = \"" << destination
                  << "\", source = \"" << source << "\"" << std::endl;

        std::cout << std::endl;
    }

    // Demonstrate enumCast
    {
        std::cout << "enumCast<T>() Examples:" << std::endl;

        Color color = Color::Blue;
        ColorCode colorCode = atom::meta::enumCast<ColorCode>(color);

        std::cout << "  Original enum value (Color::Blue): "
                  << static_cast<int>(color) << std::endl;
        std::cout << "  Converted enum value (ColorCode): "
                  << static_cast<int>(colorCode) << std::endl;

        // Convert back
        Color convertedBack = atom::meta::enumCast<Color>(colorCode);
        std::cout << "  Converted back to Color: "
                  << static_cast<int>(convertedBack) << std::endl;

        std::cout << std::endl;
    }
}

//==============================================================================
// 2. Alignment Functions
//==============================================================================
void demonstrateAlignmentFunctions() {
    printSeparator("2. Alignment Functions");

    // isAligned examples
    {
        std::cout << "isAligned<Alignment>() Examples:" << std::endl;

        std::cout << "  isAligned<4>(8) = " << std::boolalpha
                  << atom::meta::isAligned<4>(8) << std::endl;
        std::cout << "  isAligned<4>(6) = " << atom::meta::isAligned<4>(6)
                  << std::endl;
        std::cout << "  isAligned<8>(16) = " << atom::meta::isAligned<8>(16)
                  << std::endl;

        // Pointer alignment (needs actual aligned memory)
        AlignedStruct* aligned_obj = new AlignedStruct();
        void* ptr = aligned_obj;
        std::cout << "  Pointer at " << ptr
                  << " isAligned<16> = " << atom::meta::isAligned<16>(ptr)
                  << std::endl;

        char* unaligned_ptr = new char[1];
        std::cout << "  Pointer at " << static_cast<void*>(unaligned_ptr)
                  << " isAligned<16> = "
                  << atom::meta::isAligned<16>(unaligned_ptr) << std::endl;

        delete aligned_obj;
        delete[] unaligned_ptr;
        std::cout << std::endl;
    }

    // alignUp examples
    {
        std::cout << "alignUp<Alignment>() Examples:" << std::endl;

        // Align integers
        uint32_t value = 123;
        uint32_t aligned4 = atom::meta::alignUp<4>(value);
        uint32_t aligned8 = atom::meta::alignUp<8>(value);
        uint32_t aligned16 = atom::meta::alignUp<16>(value);

        std::cout << "  Original value: " << value << std::endl;
        std::cout << "  Aligned to 4: " << aligned4 << std::endl;
        std::cout << "  Aligned to 8: " << aligned8 << std::endl;
        std::cout << "  Aligned to 16: " << aligned16 << std::endl;

        // Runtime alignment
        uint32_t rt_aligned = atom::meta::alignUp(value, 32);
        std::cout << "  Runtime aligned to 32: " << rt_aligned << std::endl;

        // Align pointers
        char* buffer = new char[128];
        void* original_ptr = buffer + 5;  // Create an unaligned pointer

        void* aligned_ptr_16 = atom::meta::alignUp<16>(original_ptr);
        void* aligned_ptr_32 = atom::meta::alignUp(original_ptr, 32);

        std::cout << "  Original pointer: " << original_ptr << std::endl;
        std::cout << "  Aligned to 16: " << aligned_ptr_16 << std::endl;
        std::cout << "  Aligned to 32: " << aligned_ptr_32 << std::endl;

        delete[] buffer;
        std::cout << std::endl;
    }

    // alignDown examples
    {
        std::cout << "alignDown<Alignment>() Examples:" << std::endl;

        // Align integers
        uint32_t value = 123;
        uint32_t aligned4 = atom::meta::alignDown<4>(value);
        uint32_t aligned8 = atom::meta::alignDown<8>(value);
        uint32_t aligned16 = atom::meta::alignDown<16>(value);

        std::cout << "  Original value: " << value << std::endl;
        std::cout << "  Aligned down to 4: " << aligned4 << std::endl;
        std::cout << "  Aligned down to 8: " << aligned8 << std::endl;
        std::cout << "  Aligned down to 16: " << aligned16 << std::endl;

        // Runtime alignment
        uint32_t rt_aligned = atom::meta::alignDown(value, 32);
        std::cout << "  Runtime aligned down to 32: " << rt_aligned
                  << std::endl;

        // Align pointers
        char* buffer = new char[128];
        void* original_ptr = buffer + 37;  // Create an unaligned pointer

        void* aligned_ptr_16 = atom::meta::alignDown<16>(original_ptr);
        void* aligned_ptr_32 = atom::meta::alignDown(original_ptr, 32);

        std::cout << "  Original pointer: " << original_ptr << std::endl;
        std::cout << "  Aligned down to 16: " << aligned_ptr_16 << std::endl;
        std::cout << "  Aligned down to 32: " << aligned_ptr_32 << std::endl;

        delete[] buffer;
        std::cout << std::endl;
    }
}

//==============================================================================
// 3. Math Functions
//==============================================================================
void demonstrateMathFunctions() {
    printSeparator("3. Math Functions");

    // log2 examples
    {
        std::cout << "log2() Examples:" << std::endl;

        std::cout << "  log2(1) = " << atom::meta::log2(1) << std::endl;
        std::cout << "  log2(2) = " << atom::meta::log2(2) << std::endl;
        std::cout << "  log2(4) = " << atom::meta::log2(4) << std::endl;
        std::cout << "  log2(8) = " << atom::meta::log2(8) << std::endl;
        std::cout << "  log2(10) = " << atom::meta::log2(10) << std::endl;
        std::cout << "  log2(16) = " << atom::meta::log2(16) << std::endl;
        std::cout << "  log2(1023) = " << atom::meta::log2(1023) << std::endl;
        std::cout << "  log2(1024) = " << atom::meta::log2(1024) << std::endl;
        std::cout << "  log2(1025) = " << atom::meta::log2(1025) << std::endl;

        std::cout << std::endl;
    }

    // nb (number of blocks) examples
    {
        std::cout << "nb<BlockSize>() Examples:" << std::endl;

        // Calculate number of blocks of size 4 needed to store different values
        std::cout << "  nb<4>(0) = " << atom::meta::nb<4>(0) << std::endl;
        std::cout << "  nb<4>(4) = " << atom::meta::nb<4>(4) << std::endl;
        std::cout << "  nb<4>(5) = " << atom::meta::nb<4>(5) << std::endl;
        std::cout << "  nb<4>(7) = " << atom::meta::nb<4>(7) << std::endl;
        std::cout << "  nb<4>(8) = " << atom::meta::nb<4>(8) << std::endl;
        std::cout << "  nb<4>(9) = " << atom::meta::nb<4>(9) << std::endl;

        // Blocks of size 1024
        std::cout << "  nb<1024>(1024) = " << atom::meta::nb<1024>(1024)
                  << std::endl;
        std::cout << "  nb<1024>(1025) = " << atom::meta::nb<1024>(1025)
                  << std::endl;
        std::cout << "  nb<1024>(2048) = " << atom::meta::nb<1024>(2048)
                  << std::endl;
        std::cout << "  nb<1024>(3000) = " << atom::meta::nb<1024>(3000)
                  << std::endl;

        std::cout << std::endl;
    }

    // divCeil examples
    {
        std::cout << "divCeil() Examples:" << std::endl;

        std::cout << "  divCeil(10, 3) = " << atom::meta::divCeil(10, 3)
                  << std::endl;
        std::cout << "  divCeil(9, 3) = " << atom::meta::divCeil(9, 3)
                  << std::endl;
        std::cout << "  divCeil(11, 3) = " << atom::meta::divCeil(11, 3)
                  << std::endl;
        std::cout << "  divCeil(0, 5) = " << atom::meta::divCeil(0, 5)
                  << std::endl;
        std::cout << "  divCeil(100, 10) = " << atom::meta::divCeil(100, 10)
                  << std::endl;
        std::cout << "  divCeil(101, 10) = " << atom::meta::divCeil(101, 10)
                  << std::endl;

        std::cout << std::endl;
    }

    // isPowerOf2 examples
    {
        std::cout << "isPowerOf2() Examples:" << std::endl;

        std::cout << "  isPowerOf2(0) = " << std::boolalpha
                  << atom::meta::isPowerOf2(0) << std::endl;
        std::cout << "  isPowerOf2(1) = " << atom::meta::isPowerOf2(1)
                  << std::endl;
        std::cout << "  isPowerOf2(2) = " << atom::meta::isPowerOf2(2)
                  << std::endl;
        std::cout << "  isPowerOf2(3) = " << atom::meta::isPowerOf2(3)
                  << std::endl;
        std::cout << "  isPowerOf2(4) = " << atom::meta::isPowerOf2(4)
                  << std::endl;
        std::cout << "  isPowerOf2(16) = " << atom::meta::isPowerOf2(16)
                  << std::endl;
        std::cout << "  isPowerOf2(31) = " << atom::meta::isPowerOf2(31)
                  << std::endl;
        std::cout << "  isPowerOf2(32) = " << atom::meta::isPowerOf2(32)
                  << std::endl;
        std::cout << "  isPowerOf2(33) = " << atom::meta::isPowerOf2(33)
                  << std::endl;
        std::cout << "  isPowerOf2(1024) = " << atom::meta::isPowerOf2(1024)
                  << std::endl;
        std::cout << "  isPowerOf2(1023) = " << atom::meta::isPowerOf2(1023)
                  << std::endl;

        std::cout << std::endl;
    }
}

//==============================================================================
// 4. Memory Functions
//==============================================================================
void demonstrateMemoryFunctions() {
    printSeparator("4. Memory Functions");

    // eq (equality comparison) examples
    {
        std::cout << "eq<T>() Examples:" << std::endl;

        int a = 42, b = 42, c = 100;
        std::cout << "  eq<int>(&a, &b) [42 == 42] = " << std::boolalpha
                  << atom::meta::eq<int>(&a, &b) << std::endl;
        std::cout << "  eq<int>(&a, &c) [42 == 100] = "
                  << atom::meta::eq<int>(&a, &c) << std::endl;

        double d1 = 3.14159, d2 = 3.14159, d3 = 2.71828;
        std::cout << "  eq<double>(&d1, &d2) = "
                  << atom::meta::eq<double>(&d1, &d2) << std::endl;
        std::cout << "  eq<double>(&d1, &d3) = "
                  << atom::meta::eq<double>(&d1, &d3) << std::endl;

        std::cout << std::endl;
    }

    // copy<N> examples
    {
        std::cout << "copy<N>() Examples:" << std::endl;

        // Single byte copy
        uint8_t src_byte = 0xAA;
        uint8_t dst_byte = 0;
        atom::meta::copy<1>(&dst_byte, &src_byte);
        std::cout << "  copy<1>: 0x" << std::hex << static_cast<int>(dst_byte)
                  << std::dec << std::endl;

        // 2-byte copy
        uint16_t src_word = 0xABCD;
        uint16_t dst_word = 0;
        atom::meta::copy<2>(&dst_word, &src_word);
        std::cout << "  copy<2>: 0x" << std::hex << dst_word << std::dec
                  << std::endl;

        // 4-byte copy
        uint32_t src_dword = 0x12345678;
        uint32_t dst_dword = 0;
        atom::meta::copy<4>(&dst_dword, &src_dword);
        std::cout << "  copy<4>: 0x" << std::hex << dst_dword << std::dec
                  << std::endl;

        // 8-byte copy
        uint64_t src_qword = 0x123456789ABCDEF0;
        uint64_t dst_qword = 0;
        atom::meta::copy<8>(&dst_qword, &src_qword);
        std::cout << "  copy<8>: 0x" << std::hex << dst_qword << std::dec
                  << std::endl;

        // Multi-byte copy
        char src_str[] = "Hello, World!";
        char dst_str[20] = {0};
        atom::meta::copy<13>(dst_str, src_str);
        std::cout << "  copy<13>: \"" << dst_str << "\"" << std::endl;

        // Zero byte copy (no-op)
        char no_change[10] = "Original";
        atom::meta::copy<0>(no_change, "New");
        std::cout << "  copy<0>: \"" << no_change << "\"" << std::endl;

        std::cout << std::endl;
    }

    // safeCopy examples
    {
        std::cout << "safeCopy() Examples:" << std::endl;

        const char src[] = "This is a test of safe copy functionality";
        char dest[20];

        // Copy that fits in destination
        std::size_t copied1 = atom::meta::safeCopy(dest, sizeof(dest), src, 10);
        dest[copied1] = '\0';  // Null-terminate
        std::cout << "  safeCopy (fits): copied " << copied1 << " bytes: \""
                  << dest << "\"" << std::endl;

        // Copy that exceeds destination size
        std::size_t copied2 =
            atom::meta::safeCopy(dest, sizeof(dest), src, sizeof(src));
        dest[sizeof(dest) - 1] = '\0';  // Ensure null-termination
        std::cout << "  safeCopy (truncated): copied " << copied2
                  << " bytes: \"" << dest << "\"" << std::endl;

        std::cout << std::endl;
    }

    // zeroMemory examples
    {
        std::cout << "zeroMemory() Examples:" << std::endl;

        int values[5] = {1, 2, 3, 4, 5};
        std::cout << "  Before: [" << values[0] << ", " << values[1] << ", "
                  << values[2] << ", " << values[3] << ", " << values[4] << "]"
                  << std::endl;

        atom::meta::zeroMemory(values, sizeof(values));

        std::cout << "  After: [" << values[0] << ", " << values[1] << ", "
                  << values[2] << ", " << values[3] << ", " << values[4] << "]"
                  << std::endl;

        std::cout << std::endl;
    }

    // memoryEquals examples
    {
        std::cout << "memoryEquals() Examples:" << std::endl;

        const char str1[] = "Test string";
        const char str2[] = "Test string";
        const char str3[] = "Different!";

        bool equal1 = atom::meta::memoryEquals(str1, str2, sizeof(str1));
        bool equal2 = atom::meta::memoryEquals(str1, str3, sizeof(str1));

        std::cout << "  memoryEquals(\"" << str1 << "\", \"" << str2
                  << "\") = " << std::boolalpha << equal1 << std::endl;
        std::cout << "  memoryEquals(\"" << str1 << "\", \"" << str3
                  << "\") = " << equal2 << std::endl;

        // Partial comparison
        bool partial_equal =
            atom::meta::memoryEquals(str1, str3, 4);  // Compare just "Test"
        std::cout << "  memoryEquals(str1, str3, 4) = " << partial_equal
                  << std::endl;

        std::cout << std::endl;
    }
}

//==============================================================================
// 5. Atomic Operations
//==============================================================================
void demonstrateAtomicOperations() {
    printSeparator("5. Atomic Operations");

    // Regular (non-atomic) operations
    {
        std::cout << "Regular (Non-atomic) Operations:" << std::endl;

        int value = 42;

        int old_value = atom::meta::swap(&value, 100);
        std::cout << "  swap(&value, 100): old = " << old_value
                  << ", new = " << value << std::endl;

        old_value = atom::meta::fetchAdd(&value, 10);
        std::cout << "  fetchAdd(&value, 10): old = " << old_value
                  << ", new = " << value << std::endl;

        old_value = atom::meta::fetchSub(&value, 5);
        std::cout << "  fetchSub(&value, 5): old = " << old_value
                  << ", new = " << value << std::endl;

        old_value = atom::meta::fetchAnd(&value, 0xF0);
        std::cout << "  fetchAnd(&value, 0xF0): old = " << old_value
                  << ", new = " << value << std::endl;

        old_value = atom::meta::fetchOr(&value, 0x0F);
        std::cout << "  fetchOr(&value, 0x0F): old = " << old_value
                  << ", new = " << value << std::endl;

        old_value = atom::meta::fetchXor(&value, 0xFF);
        std::cout << "  fetchXor(&value, 0xFF): old = " << old_value
                  << ", new = " << value << std::endl;

        std::cout << std::endl;
    }

    // Atomic operations
    {
        std::cout << "Atomic Operations:" << std::endl;

        std::atomic<int> atom_value(42);

        int old_value = atom::meta::atomicSwap(&atom_value, 100);
        std::cout << "  atomicSwap(&atom_value, 100): old = " << old_value
                  << ", new = " << atom_value.load() << std::endl;

        old_value = atom::meta::atomicFetchAdd(&atom_value, 10);
        std::cout << "  atomicFetchAdd(&atom_value, 10): old = " << old_value
                  << ", new = " << atom_value.load() << std::endl;

        old_value = atom::meta::atomicFetchSub(&atom_value, 5);
        std::cout << "  atomicFetchSub(&atom_value, 5): old = " << old_value
                  << ", new = " << atom_value.load() << std::endl;

        old_value = atom::meta::atomicFetchAnd(&atom_value, 0xF0);
        std::cout << "  atomicFetchAnd(&atom_value, 0xF0): old = " << old_value
                  << ", new = " << atom_value.load() << std::endl;

        old_value = atom::meta::atomicFetchOr(&atom_value, 0x0F);
        std::cout << "  atomicFetchOr(&atom_value, 0x0F): old = " << old_value
                  << ", new = " << atom_value.load() << std::endl;

        old_value = atom::meta::atomicFetchXor(&atom_value, 0xFF);
        std::cout << "  atomicFetchXor(&atom_value, 0xFF): old = " << old_value
                  << ", new = " << atom_value.load() << std::endl;

        std::cout << std::endl;
    }

    // Demonstrate thread safety with atomic operations
    {
        std::cout << "Thread Safety with Atomic Operations:" << std::endl;

        std::atomic<int> counter(0);

        auto increment_task = [&counter]() {
            for (int i = 0; i < 1000; ++i) {
                atom::meta::atomicFetchAdd(&counter, 1);
            }
        };

        std::vector<std::thread> threads;
        const int num_threads = 5;

        for (int i = 0; i < num_threads; ++i) {
            threads.emplace_back(increment_task);
        }

        for (auto& thread : threads) {
            thread.join();
        }

        std::cout << "  Final counter value after " << num_threads
                  << " threads incrementing 1000 times each: " << counter.load()
                  << std::endl;

        std::cout << std::endl;
    }
}

//==============================================================================
// 6. Type Traits
//==============================================================================
void demonstrateTypeTraits() {
    printSeparator("6. Type Traits");

    // Type aliases
    {
        std::cout << "Type Aliases Examples:" << std::endl;

        // rmRefT - remove reference
        bool is_same = std::is_same_v<atom::meta::rmRefT<int&>, int>;
        std::cout << "  rmRefT<int&> is same as int: " << std::boolalpha
                  << is_same << std::endl;

        // rmCvT - remove const and volatile
        is_same = std::is_same_v<atom::meta::rmCvT<const int>, int>;
        std::cout << "  rmCvT<const int> is same as int: " << is_same
                  << std::endl;

        // rmCvRefT - remove const, volatile, and reference
        is_same = std::is_same_v<atom::meta::rmCvRefT<const int&>, int>;
        std::cout << "  rmCvRefT<const int&> is same as int: " << is_same
                  << std::endl;

        // rmArrT - remove array extent
        is_same = std::is_same_v<atom::meta::rmArrT<int[5]>, int>;
        std::cout << "  rmArrT<int[5]> is same as int: " << is_same
                  << std::endl;

        // constT - add const
        is_same = std::is_same_v<atom::meta::constT<int>, const int>;
        std::cout << "  constT<int> is same as const int: " << is_same
                  << std::endl;

        // constRefT - add const and reference
        is_same = std::is_same_v<atom::meta::constRefT<int>, const int&>;
        std::cout << "  constRefT<int> is same as const int&: " << is_same
                  << std::endl;

        // rmPtrT - remove pointer
        is_same = std::is_same_v<atom::meta::rmPtrT<int*>, int>;
        std::cout << "  rmPtrT<int*> is same as int: " << is_same << std::endl;

        // if_t - conditional type
        is_same = std::is_same_v<atom::meta::if_t<true, int, double>, int>;
        std::cout << "  if_t<true, int, double> is same as int: " << is_same
                  << std::endl;

        is_same = std::is_same_v<atom::meta::if_t<false, int, double>, double>;
        std::cout << "  if_t<false, int, double> is same as double: " << is_same
                  << std::endl;

        std::cout << std::endl;
    }

    // Type traits functions
    {
        std::cout << "Type Traits Functions Examples:" << std::endl;

        // isSame
        std::cout << "  isSame<int, int>() = " << std::boolalpha
                  << atom::meta::isSame<int, int>() << std::endl;
        std::cout << "  isSame<int, double>() = "
                  << atom::meta::isSame<int, double>() << std::endl;
        std::cout << "  isSame<int, int, double>() = "
                  << atom::meta::isSame<int, int, double>() << std::endl;

        // isRef
        std::cout << "  isRef<int>() = " << atom::meta::isRef<int>()
                  << std::endl;
        std::cout << "  isRef<int&>() = " << atom::meta::isRef<int&>()
                  << std::endl;

        // isArray
        std::cout << "  isArray<int>() = " << atom::meta::isArray<int>()
                  << std::endl;
        std::cout << "  isArray<int[5]>() = " << atom::meta::isArray<int[5]>()
                  << std::endl;

        // isClass
        std::cout << "  isClass<int>() = " << atom::meta::isClass<int>()
                  << std::endl;
        std::cout << "  isClass<Base>() = " << atom::meta::isClass<Base>()
                  << std::endl;

        // isScalar
        std::cout << "  isScalar<int>() = " << atom::meta::isScalar<int>()
                  << std::endl;
        std::cout << "  isScalar<Base>() = " << atom::meta::isScalar<Base>()
                  << std::endl;

        // isTriviallyCopyable
        std::cout << "  isTriviallyCopyable<int>() = "
                  << atom::meta::isTriviallyCopyable<int>() << std::endl;
        std::cout << "  isTriviallyCopyable<Base>() = "
                  << atom::meta::isTriviallyCopyable<Base>() << std::endl;

        // isTriviallyDestructible
        std::cout << "  isTriviallyDestructible<int>() = "
                  << atom::meta::isTriviallyDestructible<int>() << std::endl;
        std::cout << "  isTriviallyDestructible<Base>() = "
                  << atom::meta::isTriviallyDestructible<Base>() << std::endl;

        // isBaseOf
        std::cout << "  isBaseOf<Base, Derived>() = "
                  << atom::meta::isBaseOf<Base, Derived>() << std::endl;
        std::cout << "  isBaseOf<Derived, Base>() = "
                  << atom::meta::isBaseOf<Derived, Base>() << std::endl;

        // hasVirtualDestructor
        std::cout << "  hasVirtualDestructor<int>() = "
                  << atom::meta::hasVirtualDestructor<int>() << std::endl;
        std::cout << "  hasVirtualDestructor<Base>() = "
                  << atom::meta::hasVirtualDestructor<Base>() << std::endl;

        // isNothrowRelocatable
        std::cout << "  isNothrowRelocatable<int> = "
                  << atom::meta::isNothrowRelocatable<int> << std::endl;
        std::cout << "  isNothrowRelocatable<std::string> = "
                  << atom::meta::isNothrowRelocatable<std::string> << std::endl;

        std::cout << std::endl;
    }
}

//==============================================================================
// 7. Resource Management
//==============================================================================

// Example singleton class
class ConfigManager {
public:
    ConfigManager() {
        std::cout << "  ConfigManager singleton created" << std::endl;
    }

    ~ConfigManager() {
        std::cout << "  ConfigManager singleton destroyed" << std::endl;
    }

    void setConfig(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_[key] = value;
    }

    std::string getConfig(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = config_.find(key);
        return (it != config_.end()) ? it->second : "";
    }

private:
    std::unordered_map<std::string, std::string> config_;
    mutable std::mutex mutex_;
};

void demonstrateResourceManagement() {
    printSeparator("7. Resource Management");

    // ScopeGuard examples
    {
        std::cout << "ScopeGuard Examples:" << std::endl;

        // Basic scope guard
        {
            auto guard = atom::meta::ScopeGuard([]() {
                std::cout << "  Basic scope guard executed on scope exit"
                          << std::endl;
            });

            std::cout << "  Inside scope with basic guard" << std::endl;
        }
        std::cout << "  After basic guard scope" << std::endl;

        // Using makeGuard helper
        {
            auto guard = atom::meta::makeGuard([]() {
                std::cout << "  Guard created with makeGuard executed"
                          << std::endl;
            });

            std::cout << "  Inside scope with makeGuard" << std::endl;
        }

        // Guard that is dismissed
        {
            auto guard =
                atom::meta::makeGuard([]() {
                    std::cout
                        << "  This guard was dismissed (you shouldn't see this)"
                        << std::endl;
                });

            std::cout << "  Inside scope with dismissed guard" << std::endl;
            guard.dismiss();
        }
        std::cout << "  After dismissed guard scope" << std::endl;

        // More practical example: file handle cleanup
        {
            FILE* file = std::fopen("/tmp/test_file.txt", "w");
            auto file_guard = atom::meta::makeGuard([file]() {
                if (file) {
                    std::fclose(file);
                    std::cout << "  File automatically closed by guard"
                              << std::endl;
                }
            });

            // Use the file
            if (file) {
                std::fprintf(file, "Hello, World!");
                std::cout << "  Wrote to file" << std::endl;
            }

            // No need to manually close the file, the guard will do it
        }

        // Move semantics
        {
            auto outer_guard = atom::meta::makeGuard(
                []() { std::cout << "  Moved guard executed" << std::endl; });

            {
                auto inner_guard = std::move(outer_guard);
                std::cout << "  Guard moved to inner scope" << std::endl;
            }

            std::cout << "  After inner scope (moved guard already executed)"
                      << std::endl;
        }

        std::cout << std::endl;
    }

    // Singleton examples
    {
        std::cout << "Singleton Examples:" << std::endl;

        // Access singleton for the first time
        auto& config = atom::meta::singleton<ConfigManager>();
        config.setConfig("version", "1.0.0");

        // Access singleton from another part of code
        auto& config2 = atom::meta::singleton<ConfigManager>();
        config2.setConfig("debug", "true");

        // Verify it's the same instance
        std::cout << "  config.getConfig(\"version\") = "
                  << config.getConfig("version") << std::endl;
        std::cout << "  config.getConfig(\"debug\") = "
                  << config.getConfig("debug") << std::endl;
        std::cout << "  config2.getConfig(\"version\") = "
                  << config2.getConfig("version") << std::endl;
        std::cout << "  config2.getConfig(\"debug\") = "
                  << config2.getConfig("debug") << std::endl;

        // Thread-safety test with concurrent singleton access
        std::vector<std::thread> threads;

        for (int i = 0; i < 5; ++i) {
            threads.emplace_back([i]() {
                auto& config = atom::meta::singleton<ConfigManager>();
                config.setConfig("thread_" + std::to_string(i), "active");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                std::cout << "  Thread " << i << " accessed singleton"
                          << std::endl;
            });
        }

        for (auto& thread : threads) {
            thread.join();
        }

        // Verify thread settings
        for (int i = 0; i < 5; ++i) {
            std::cout << "  config.getConfig(\"thread_" << i << "\") = "
                      << config.getConfig("thread_" + std::to_string(i))
                      << std::endl;
        }

        std::cout << std::endl;
    }
}
