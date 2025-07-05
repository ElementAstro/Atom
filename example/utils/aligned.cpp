/**
 * @file aligned_example.cpp
 * @brief Comprehensive examples demonstrating the ValidateAlignedStorage class
 *
 * This file provides examples of how to use the ValidateAlignedStorage class
 * to validate alignment requirements for various data types and structures.
 */

#include "atom/utils/aligned.hpp"
#include <iostream>
#include <type_traits>
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

// Helper function to print alignment information for a type
template <typename T>
void printTypeInfo(const std::string& typeName) {
    std::cout << "Type: " << typeName << std::endl;
    std::cout << "Size: " << sizeof(T) << " bytes" << std::endl;
    std::cout << "Alignment: " << alignof(T) << " bytes" << std::endl;
}

// Helper function to check if allocation is properly aligned
template <typename T>
bool isAligned(T* ptr, size_t alignment) {
    return (reinterpret_cast<uintptr_t>(ptr) % alignment) == 0;
}

// Define an allocator that respects alignment
template <typename T, size_t Alignment>
class AlignedAllocator {
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
        using other = AlignedAllocator<U, Alignment>;
    };

    AlignedAllocator() = default;

    template <typename U>
    AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

    pointer allocate(size_type n) {
        void* ptr = nullptr;
#if defined(_MSC_VER)
        ptr = _aligned_malloc(n * sizeof(T), Alignment);
#elif defined(__GNUC__)
        if (posix_memalign(&ptr, Alignment, n * sizeof(T)) != 0) {
            ptr = nullptr;
        }
#else
        // Fallback to aligned_alloc
        ptr = aligned_alloc(Alignment, n * sizeof(T));
#endif

        if (!ptr) {
            throw std::bad_alloc();
        }
        return static_cast<pointer>(ptr);
    }

    void deallocate(pointer p, size_type) noexcept {
#if defined(_MSC_VER)
        _aligned_free(p);
#else
        free(p);
#endif
    }

    bool operator==(const AlignedAllocator&) const noexcept { return true; }
    bool operator!=(const AlignedAllocator&) const noexcept { return false; }
};

// A struct with default alignment
struct DefaultStruct {
    int a;
    char b;
    double c;
    bool d;
};

// A struct with specific alignment requirements
struct alignas(16) AlignedStruct {
    int a;
    char b;
    double c;
    bool d;
};

// A large struct for testing larger alignments
struct alignas(32) LargeAlignedStruct {
    double values[8];  // 64 bytes
    int extra[4];      // 16 bytes
};

// A struct with mixed alignment requirements
struct MixedAlignmentStruct {
    char a;
    alignas(8) double b;
    int c;
};

// SIMD-friendly struct with specific alignment
struct alignas(16) SimdVector {
    float x, y, z, w;
};

int main() {
    std::cout << "==========================================" << std::endl;
    std::cout << "  ValidateAlignedStorage Demonstration" << std::endl;
    std::cout << "==========================================" << std::endl;

    try {
        // Example 1: Basic Alignment Checks
        printSection("1. Basic Alignment Checks");

        printSubsection("Fundamental Types");
        printTypeInfo<char>("char");
        printTypeInfo<int>("int");
        printTypeInfo<double>("double");
        printTypeInfo<long long>("long long");

        printSubsection("Custom Structures");
        printTypeInfo<DefaultStruct>("DefaultStruct");
        printTypeInfo<AlignedStruct>("AlignedStruct");
        printTypeInfo<LargeAlignedStruct>("LargeAlignedStruct");
        printTypeInfo<MixedAlignmentStruct>("MixedAlignmentStruct");

        // Example 2: Validating Storage for Types
        printSection("2. Validating Storage for Types");

        printSubsection("Valid Storage");

        // Validate storage for int with exact match (typically 4 bytes, aligned
        // to 4)
        atom::utils::ValidateAlignedStorage<sizeof(int), alignof(int),
                                            sizeof(int), alignof(int)>
            validateInt;
        std::cout << "Validating storage for int: Success!" << std::endl;

        // Validate storage for double with larger storage (typically 8 bytes,
        // aligned to 8)
        atom::utils::ValidateAlignedStorage<sizeof(double), alignof(double), 16,
                                            16>
            validateDouble;
        std::cout << "Validating larger storage for double: Success!"
                  << std::endl;

        // Validate storage for AlignedStruct (specially aligned to 16)
        atom::utils::ValidateAlignedStorage<
            sizeof(AlignedStruct), alignof(AlignedStruct),
            sizeof(AlignedStruct), alignof(AlignedStruct)>
            validateAligned;
        std::cout << "Validating storage for AlignedStruct: Success!"
                  << std::endl;

        // Validate storage for LargeAlignedStruct with extra padding
        constexpr size_t largeSize =
            sizeof(LargeAlignedStruct) + 32;  // Extra padding
        constexpr size_t largeAlign = alignof(LargeAlignedStruct);
        atom::utils::ValidateAlignedStorage<sizeof(LargeAlignedStruct),
                                            alignof(LargeAlignedStruct),
                                            largeSize, largeAlign>
            validateLarge;
        std::cout << "Validating storage for LargeAlignedStruct with padding: "
                     "Success!"
                  << std::endl;

        // Example 3: Aligned Memory Allocation
        printSection("3. Aligned Memory Allocation");

        printSubsection("Custom Aligned Storage");

        // Define storage requirements
        constexpr size_t storageSize = 64;
        constexpr size_t storageAlign = 32;

        // Validate for different implementations - create instances rather than
        // just aliases
        atom::utils::ValidateAlignedStorage<sizeof(int), alignof(int),
                                            storageSize, storageAlign>
            validateForInt;
        atom::utils::ValidateAlignedStorage<sizeof(double), alignof(double),
                                            storageSize, storageAlign>
            validateForDouble;
        atom::utils::ValidateAlignedStorage<
            sizeof(SimdVector), alignof(SimdVector), storageSize, storageAlign>
            validateForSimdVector;

        std::cout << "Custom storage (size=" << storageSize
                  << ", align=" << storageAlign
                  << ") is valid for:" << std::endl;
        std::cout << "- int" << std::endl;
        std::cout << "- double" << std::endl;
        std::cout << "- SimdVector" << std::endl;

        // Allocate aligned memory
        void* rawMemory = nullptr;

#if defined(_MSC_VER)
        rawMemory = _aligned_malloc(storageSize, storageAlign);
        if (rawMemory) {
            std::cout
                << "Successfully allocated aligned memory with _aligned_malloc"
                << std::endl;
            _aligned_free(rawMemory);
        }
#elif defined(__GNUC__)
        if (posix_memalign(&rawMemory, storageAlign, storageSize) == 0) {
            std::cout
                << "Successfully allocated aligned memory with posix_memalign"
                << std::endl;
            free(rawMemory);
        }
#else
        // Fallback to aligned_alloc if available (C++17)
        try {
            rawMemory = aligned_alloc(storageAlign, storageSize);
            if (rawMemory) {
                std::cout << "Successfully allocated aligned memory with "
                             "aligned_alloc"
                          << std::endl;
                free(rawMemory);
            }
        } catch (...) {
            std::cout << "aligned_alloc not available" << std::endl;
        }
#endif

        // Example 4: Practical Use Cases
        printSection("4. Practical Use Cases");

        printSubsection("Aligned std::vector with custom allocator");

        // Create vectors of SimdVector with proper alignment
        std::vector<SimdVector,
                    AlignedAllocator<SimdVector, alignof(SimdVector)>>
            alignedVectors;

        // Validate our storage - create instance instead of alias
        atom::utils::ValidateAlignedStorage<
            sizeof(SimdVector), alignof(SimdVector), sizeof(SimdVector),
            alignof(SimdVector)>
            validateVectorStorage;

        // Add some elements
        alignedVectors.push_back({1.0f, 2.0f, 3.0f, 4.0f});
        alignedVectors.push_back({5.0f, 6.0f, 7.0f, 8.0f});

        // Check alignment of elements in the vector
        if (!alignedVectors.empty() &&
            isAligned(&alignedVectors[0], alignof(SimdVector))) {
            std::cout << "Vector elements are properly aligned to "
                      << alignof(SimdVector) << " bytes" << std::endl;
        } else {
            std::cout << "Vector elements are not properly aligned"
                      << std::endl;
        }

        // Example 5: Compile-time Validation (can't be visualized at runtime)
        printSection("5. Compile-time Validation");

        std::cout << "The following validations happen at compile-time:"
                  << std::endl;

        // These would cause compile-time errors if uncommented:

        /*
        // Insufficient storage size
        using InvalidSize = atom::utils::ValidateAlignedStorage<
            16, 8,  // Implementation: 16 bytes, 8-byte alignment
            8, 8    // Storage: 8 bytes, 8-byte alignment (too small)
        >;
        */
        std::cout << "- Insufficient storage size: Commented out to avoid "
                     "compile error"
                  << std::endl;

        /*
        // Insufficient alignment
        using InvalidAlign = atom::utils::ValidateAlignedStorage<
            8, 8,   // Implementation: 8 bytes, 8-byte alignment
            16, 4   // Storage: 16 bytes, 4-byte alignment (not a multiple of 8)
        >;
        */
        std::cout
            << "- Insufficient alignment: Commented out to avoid compile error"
            << std::endl;

        // Example 6: Common Alignment Cases
        printSection("6. Common Alignment Cases");

        printSubsection("Cache Line Alignment");
        constexpr size_t CACHE_LINE_SIZE = 64;  // Common cache line size

        // A struct that should be cache-line aligned for performance
        struct alignas(CACHE_LINE_SIZE) CacheAlignedStruct {
            int data[CACHE_LINE_SIZE / sizeof(int)];
        };

        printTypeInfo<CacheAlignedStruct>("CacheAlignedStruct");

        // Validate storage for cache-aligned struct - create instance instead
        // of alias
        atom::utils::ValidateAlignedStorage<
            sizeof(CacheAlignedStruct), alignof(CacheAlignedStruct),
            sizeof(CacheAlignedStruct), CACHE_LINE_SIZE>
            validateCacheAligned;

        std::cout << "Validating storage for CacheAlignedStruct: Success!"
                  << std::endl;

        printSubsection("SIMD Alignment");

        // Common SIMD alignment requirements
        constexpr size_t SSE_ALIGN = 16;  // SSE
        constexpr size_t AVX_ALIGN = 32;  // AVX

        // Validate SimdVector for different SIMD extensions - create instance
        // instead of alias
        atom::utils::ValidateAlignedStorage<sizeof(SimdVector),
                                            alignof(SimdVector),
                                            sizeof(SimdVector), SSE_ALIGN>
            validateSimdSSE;

        std::cout
            << "SimdVector is valid for SSE operations (16-byte alignment)"
            << std::endl;

        // For higher alignment requirements, we need a larger struct
        struct alignas(AVX_ALIGN) AvxVector {
            float values[8];  // 32 bytes, AVX can process 8 floats at once
        };

        // Create instance instead of alias
        atom::utils::ValidateAlignedStorage<
            sizeof(AvxVector), alignof(AvxVector), sizeof(AvxVector), AVX_ALIGN>
            validateAvxVector;

        std::cout << "AvxVector is valid for AVX operations (32-byte alignment)"
                  << std::endl;

        // Example 7: Using Alignment with Standard Library
        printSection("7. Using Alignment with Standard Library");

        printSubsection("std::aligned_storage - replaced with std::byte array");

        // Replace deprecated std::aligned_storage with std::byte array +
        // alignas
        constexpr size_t INT_SIZE = sizeof(int);
        constexpr size_t INT_ALIGN = alignof(int);

        // Create storage for an int using std::byte array with proper alignment
        alignas(INT_ALIGN) std::byte intStorageBytes[INT_SIZE];

        // Validate our storage with ValidateAlignedStorage - create instance
        // instead of alias
        atom::utils::ValidateAlignedStorage<sizeof(int), alignof(int),
                                            sizeof(intStorageBytes), INT_ALIGN>
            validateIntStorage;

        // Place an int into the storage
        int* intPtr = new (intStorageBytes) int(42);
        std::cout << "Int value from aligned storage: " << *intPtr << std::endl;
        std::cout << "Int storage is aligned to "
                  << alignof(decltype(intStorageBytes)) << " bytes"
                  << std::endl;

        // No need to call destructor for primitive types like int

        printSubsection("std::aligned_union - replaced with union");

        // Replace deprecated std::aligned_union with actual union
        union AlignedUnion {
            int i;
            double d;
            char c[16];
        };

        // Create an instance of the union to use
        AlignedUnion myUnion;
        myUnion.i = 42;  // Use the union to avoid unused variable warning

        // Validate storage for union types - create instances instead of
        // aliases
        atom::utils::ValidateAlignedStorage<sizeof(int), alignof(int),
                                            sizeof(AlignedUnion),
                                            alignof(AlignedUnion)>
            validateUnionInt;

        atom::utils::ValidateAlignedStorage<sizeof(double), alignof(double),
                                            sizeof(AlignedUnion),
                                            alignof(AlignedUnion)>
            validateUnionDouble;

        std::cout << "Union storage size: " << sizeof(AlignedUnion) << " bytes"
                  << std::endl;
        std::cout << "Union storage alignment: " << alignof(AlignedUnion)
                  << " bytes" << std::endl;
        std::cout << "Union storage is valid for int, double, and char[16]"
                  << std::endl;
        std::cout << "Current union value (as int): " << myUnion.i << std::endl;

        std::cout << "\nAll examples completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
