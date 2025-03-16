#include <gtest/gtest.h>
#include "atom/meta/god.hpp"

#include <array>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace atom::meta::test {

class GodTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}

    // Test fixture typedefs and helper classes
    enum class TestEnum { One, Two, Three };

    struct NonTriviallyCopyable {
        std::string value;
        NonTriviallyCopyable() : value("default") {}
        NonTriviallyCopyable(const NonTriviallyCopyable& other)
            : value(other.value) {}
    };

    struct Base {};
    struct Derived : public Base {};

    struct VirtualBase {
        virtual ~VirtualBase() = default;
    };
    struct VirtualDerived : public VirtualBase {};
};

//==============================================================================
// Basic Utilities Tests
//==============================================================================

TEST_F(GodTest, BlessNoBugs) {
    // This function does nothing, just verify it doesn't crash
    blessNoBugs();
}

TEST_F(GodTest, CastTest) {
    int intValue = 42;

    // Test basic casting
    long longValue = cast<long>(intValue);
    EXPECT_EQ(longValue, 42L);

    // Test casting to reference
    int& intRef = cast<int&>(intValue);
    intRef = 84;
    EXPECT_EQ(intValue, 84);

    // Test casting with expressions
    double result = cast<double>(intValue / 2);
    EXPECT_DOUBLE_EQ(result, 42.0);

    // Test with moved value
    std::string str = "test";
    std::string movedStr = cast<std::string>(std::move(str));
    EXPECT_EQ(movedStr, "test");
    EXPECT_TRUE(str.empty());  // str should be moved from
}

TEST_F(GodTest, EnumCastTest) {
    // Test enum casting
    enum class Color { Red, Green, Blue };
    enum class AnotherColor { Red, Green, Blue };

    Color color = Color::Green;
    AnotherColor anotherColor = enumCast<AnotherColor>(color);

    // The underlying values should match
    EXPECT_EQ(static_cast<int>(color), static_cast<int>(anotherColor));
    EXPECT_EQ(static_cast<int>(anotherColor), 1);

    // Test with TestEnum from fixture
    TestEnum enumVal = TestEnum::Two;
    AnotherColor converted = enumCast<AnotherColor>(enumVal);
    EXPECT_EQ(static_cast<int>(converted), 1);
}

//==============================================================================
// Alignment Functions Tests
//==============================================================================

TEST_F(GodTest, IsAlignedTest) {
    // Test isAligned with various values
    EXPECT_TRUE(isAligned<4>(0));
    EXPECT_TRUE(isAligned<4>(4));
    EXPECT_TRUE(isAligned<4>(8));
    EXPECT_FALSE(isAligned<4>(1));
    EXPECT_FALSE(isAligned<4>(2));
    EXPECT_FALSE(isAligned<4>(6));

    // Test with pointer values
    int* ptr = reinterpret_cast<int*>(16);
    EXPECT_TRUE(isAligned<8>(ptr));

    int* unalignedPtr = reinterpret_cast<int*>(10);
    EXPECT_FALSE(isAligned<8>(unalignedPtr));
}

TEST_F(GodTest, AlignUpTest) {
    // Test alignUp with values
    EXPECT_EQ(alignUp<4>(0), 0);
    EXPECT_EQ(alignUp<4>(1), 4);
    EXPECT_EQ(alignUp<4>(4), 4);
    EXPECT_EQ(alignUp<4>(5), 8);
    EXPECT_EQ(alignUp<8>(9), 16);

    // Test with dynamic alignment
    EXPECT_EQ(alignUp(5, 4), 8);
    EXPECT_EQ(alignUp(10, 8), 16);

    // Test with pointers
    int* ptr = reinterpret_cast<int*>(5);
    int* aligned = alignUp<8>(ptr);
    EXPECT_EQ(reinterpret_cast<std::size_t>(aligned), 8);

    // Test with dynamic alignment and pointers
    ptr = reinterpret_cast<int*>(10);
    aligned = alignUp(ptr, 16);
    EXPECT_EQ(reinterpret_cast<std::size_t>(aligned), 16);
}

TEST_F(GodTest, AlignDownTest) {
    // Test alignDown with values
    EXPECT_EQ(alignDown<4>(0), 0);
    EXPECT_EQ(alignDown<4>(1), 0);
    EXPECT_EQ(alignDown<4>(4), 4);
    EXPECT_EQ(alignDown<4>(5), 4);
    EXPECT_EQ(alignDown<8>(9), 8);

    // Test with dynamic alignment
    EXPECT_EQ(alignDown(5, 4), 4);
    EXPECT_EQ(alignDown(10, 8), 8);

    // Test with pointers
    int* ptr = reinterpret_cast<int*>(5);
    int* aligned = alignDown<4>(ptr);
    EXPECT_EQ(reinterpret_cast<std::size_t>(aligned), 4);

    // Test with dynamic alignment and pointers
    ptr = reinterpret_cast<int*>(19);
    aligned = alignDown(ptr, 8);
    EXPECT_EQ(reinterpret_cast<std::size_t>(aligned), 16);
}

//==============================================================================
// Math Functions Tests
//==============================================================================

TEST_F(GodTest, Log2Test) {
    // Test log2 function with various values
    EXPECT_EQ(log2(0), 0);
    EXPECT_EQ(log2(1), 0);
    EXPECT_EQ(log2(2), 1);
    EXPECT_EQ(log2(3), 1);
    EXPECT_EQ(log2(4), 2);
    EXPECT_EQ(log2(7), 2);
    EXPECT_EQ(log2(8), 3);
    EXPECT_EQ(log2(1023), 9);
    EXPECT_EQ(log2(1024), 10);

    // Test with larger types
    EXPECT_EQ(log2(1ULL << 32), 32);

    // Test with signed types
    EXPECT_EQ(log2(static_cast<int>(8)), 3);
}

TEST_F(GodTest, NbTest) {
    // Test nb (number of blocks) function
    EXPECT_EQ((nb<4>(0)), 0);
    EXPECT_EQ((nb<4>(1)), 1);
    EXPECT_EQ((nb<4>(3)), 1);
    EXPECT_EQ((nb<4>(4)), 1);
    EXPECT_EQ((nb<4>(5)), 2);
    EXPECT_EQ((nb<4>(8)), 2);
    EXPECT_EQ((nb<8>(7)), 1);
    EXPECT_EQ((nb<8>(8)), 1);
    EXPECT_EQ((nb<8>(9)), 2);
}

TEST_F(GodTest, DivCeilTest) {
    // Test divCeil function
    EXPECT_EQ(divCeil(0, 5), 0);
    EXPECT_EQ(divCeil(5, 5), 1);
    EXPECT_EQ(divCeil(6, 5), 2);
    EXPECT_EQ(divCeil(10, 5), 2);
    EXPECT_EQ(divCeil(11, 5), 3);
    EXPECT_EQ(divCeil(-10, 3), -3);  // Behavior with negative numbers
}

TEST_F(GodTest, IsPowerOf2Test) {
    // Test isPowerOf2 function
    EXPECT_FALSE(isPowerOf2(0));
    EXPECT_TRUE(isPowerOf2(1));
    EXPECT_TRUE(isPowerOf2(2));
    EXPECT_FALSE(isPowerOf2(3));
    EXPECT_TRUE(isPowerOf2(4));
    EXPECT_FALSE(isPowerOf2(6));
    EXPECT_TRUE(isPowerOf2(8));
    EXPECT_TRUE(isPowerOf2(1024));
    EXPECT_FALSE(isPowerOf2(1023));
    EXPECT_TRUE(isPowerOf2(1ULL << 63));
}

//==============================================================================
// Memory Functions Tests
//==============================================================================

TEST_F(GodTest, EqTest) {
    // Test eq function
    int a = 42, b = 42, c = 24;

    EXPECT_TRUE(eq<int>(&a, &b));
    EXPECT_FALSE(eq<int>(&a, &c));

    std::string s1 = "hello", s2 = "hello", s3 = "world";
    EXPECT_TRUE(eq<std::string>(&s1, &s2));
    EXPECT_FALSE(eq<std::string>(&s1, &s3));
}

TEST_F(GodTest, CopyTest) {
    // Test copy with different sizes
    uint8_t src8 = 123;
    uint8_t dst8 = 0;
    copy<1>(&dst8, &src8);
    EXPECT_EQ(dst8, 123);

    uint16_t src16 = 12345;
    uint16_t dst16 = 0;
    copy<2>(&dst16, &src16);
    EXPECT_EQ(dst16, 12345);

    uint32_t src32 = 1234567;
    uint32_t dst32 = 0;
    copy<4>(&dst32, &src32);
    EXPECT_EQ(dst32, 1234567);

    uint64_t src64 = 12345678901234;
    uint64_t dst64 = 0;
    copy<8>(&dst64, &src64);
    EXPECT_EQ(dst64, 12345678901234);

    // Test with larger size (uses memcpy)
    std::array<char, 20> srcArr = {'H', 'e', 'l', 'l', 'o', '\0'};
    std::array<char, 20> dstArr = {};
    copy<20>(dstArr.data(), srcArr.data());
    EXPECT_STREQ(dstArr.data(), "Hello");
}

TEST_F(GodTest, SafeCopyTest) {
    // Test safeCopy function
    char src[] = "Hello, world!";
    char dst[10];

    // Destination buffer is smaller than source
    std::size_t copied = safeCopy(dst, sizeof(dst), src, sizeof(src));
    EXPECT_EQ(copied, 10);  // Should copy only 10 bytes

    // Reset destination buffer
    std::memset(dst, 0, sizeof(dst));

    // Source is smaller than destination
    char smallSrc[] = "Hi!";
    copied = safeCopy(dst, sizeof(dst), smallSrc, sizeof(smallSrc));
    EXPECT_EQ(copied, 4);  // "Hi!" + null terminator
    EXPECT_STREQ(dst, "Hi!");
}

TEST_F(GodTest, ZeroMemoryTest) {
    // Test zeroMemory function
    std::array<uint8_t, 10> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    zeroMemory(data.data(), data.size());

    for (uint8_t value : data) {
        EXPECT_EQ(value, 0);
    }
}

TEST_F(GodTest, MemoryEqualsTest) {
    // Test memoryEquals function
    std::array<uint8_t, 4> data1 = {1, 2, 3, 4};
    std::array<uint8_t, 4> data2 = {1, 2, 3, 4};
    std::array<uint8_t, 4> data3 = {1, 2, 3, 5};

    EXPECT_TRUE(memoryEquals(data1.data(), data2.data(), 4));
    EXPECT_FALSE(memoryEquals(data1.data(), data3.data(), 4));
    EXPECT_TRUE(memoryEquals(data1.data(), data3.data(),
                             3));  // First 3 elements are equal
}

//==============================================================================
// Atomic Operations Tests
//==============================================================================

TEST_F(GodTest, AtomicSwapTest) {
    // Test atomicSwap function
    std::atomic<int> value(42);

    int oldValue = atomicSwap(&value, 100);
    EXPECT_EQ(oldValue, 42);
    EXPECT_EQ(value.load(), 100);

    // Test with different memory order
    oldValue = atomicSwap(&value, 200, std::memory_order_relaxed);
    EXPECT_EQ(oldValue, 100);
    EXPECT_EQ(value.load(), 200);
}

TEST_F(GodTest, SwapTest) {
    // Test non-atomic swap function
    int value = 42;

    int oldValue = swap(&value, 100);
    EXPECT_EQ(oldValue, 42);
    EXPECT_EQ(value, 100);

    // Test with different types
    double doubleVal = 3.14;
    double oldDouble = swap(&doubleVal, 2.71);
    EXPECT_DOUBLE_EQ(oldDouble, 3.14);
    EXPECT_DOUBLE_EQ(doubleVal, 2.71);
}

TEST_F(GodTest, FetchAddTest) {
    // Test fetchAdd function
    int value = 42;

    int oldValue = fetchAdd(&value, 10);
    EXPECT_EQ(oldValue, 42);
    EXPECT_EQ(value, 52);

    // Test with atomic version
    std::atomic<int> atomicVal(100);
    int oldAtomicVal = atomicFetchAdd(&atomicVal, 5);
    EXPECT_EQ(oldAtomicVal, 100);
    EXPECT_EQ(atomicVal.load(), 105);

    // Test with different memory order
    oldAtomicVal = atomicFetchAdd(&atomicVal, 5, std::memory_order_relaxed);
    EXPECT_EQ(oldAtomicVal, 105);
    EXPECT_EQ(atomicVal.load(), 110);
}

TEST_F(GodTest, FetchSubTest) {
    // Test fetchSub function
    int value = 42;

    int oldValue = fetchSub(&value, 10);
    EXPECT_EQ(oldValue, 42);
    EXPECT_EQ(value, 32);

    // Test with atomic version
    std::atomic<int> atomicVal(100);
    int oldAtomicVal = atomicFetchSub(&atomicVal, 5);
    EXPECT_EQ(oldAtomicVal, 100);
    EXPECT_EQ(atomicVal.load(), 95);

    // Test with different memory order
    oldAtomicVal = atomicFetchSub(&atomicVal, 5, std::memory_order_relaxed);
    EXPECT_EQ(oldAtomicVal, 95);
    EXPECT_EQ(atomicVal.load(), 90);
}

TEST_F(GodTest, FetchAndTest) {
    // Test fetchAnd function
    uint32_t value = 0xFFFF0000;

    uint32_t oldValue = fetchAnd(&value, 0xF0F0FFFF);
    EXPECT_EQ(oldValue, 0xFFFF0000);
    EXPECT_EQ(value, 0xF0F00000);

    // Test with atomic version
    std::atomic<uint32_t> atomicVal(0xFFFFFFFF);
    uint32_t oldAtomicVal = atomicFetchAnd(&atomicVal, 0xF0F0F0F0);
    EXPECT_EQ(oldAtomicVal, 0xFFFFFFFF);
    EXPECT_EQ(atomicVal.load(), 0xF0F0F0F0);

    // Test with enum
    enum class Flags : uint8_t {
        None = 0,
        Flag1 = 1,
        Flag2 = 2,
        Flag3 = 4,
        All = 7
    };

    Flags flags = Flags::All;
    Flags oldFlags = fetchAnd(&flags, Flags::Flag1);
    EXPECT_EQ(static_cast<uint8_t>(oldFlags), 7);
    EXPECT_EQ(static_cast<uint8_t>(flags), 1);
}

TEST_F(GodTest, FetchOrTest) {
    // Test fetchOr function
    uint32_t value = 0xFF00FF00;

    uint32_t oldValue = fetchOr(&value, 0x0F0F0F0F);
    EXPECT_EQ(oldValue, 0xFF00FF00);
    EXPECT_EQ(value, 0xFF0FFF0F);

    // Test with atomic version
    std::atomic<uint32_t> atomicVal(0x00000000);
    uint32_t oldAtomicVal = atomicFetchOr(&atomicVal, 0xF0F0F0F0);
    EXPECT_EQ(oldAtomicVal, 0x00000000);
    EXPECT_EQ(atomicVal.load(), 0xF0F0F0F0);

    // Test with enum
    enum class Flags : uint8_t {
        None = 0,
        Flag1 = 1,
        Flag2 = 2,
        Flag3 = 4,
        All = 7
    };

    Flags flags = Flags::None;
    Flags oldFlags = fetchOr(&flags, Flags::Flag2);
    EXPECT_EQ(static_cast<uint8_t>(oldFlags), 0);
    EXPECT_EQ(static_cast<uint8_t>(flags), 2);
}

TEST_F(GodTest, FetchXorTest) {
    // Test fetchXor function
    uint32_t value = 0xFF00FF00;

    uint32_t oldValue = fetchXor(&value, 0x0F0F0F0F);
    EXPECT_EQ(oldValue, 0xFF00FF00);
    EXPECT_EQ(value, 0xF00FF00F);

    // Test with atomic version
    std::atomic<uint32_t> atomicVal(0xFFFFFFFF);
    uint32_t oldAtomicVal = atomicFetchXor(&atomicVal, 0xF0F0F0F0);
    EXPECT_EQ(oldAtomicVal, 0xFFFFFFFF);
    EXPECT_EQ(atomicVal.load(), 0x0F0F0F0F);

    // Test with enum
    enum class Flags : uint8_t {
        None = 0,
        Flag1 = 1,
        Flag2 = 2,
        Flag3 = 4,
        All = 7
    };

    Flags flags = Flags::All;
    Flags oldFlags = fetchXor(&flags, Flags::Flag2);
    EXPECT_EQ(static_cast<uint8_t>(oldFlags), 7);
    EXPECT_EQ(static_cast<uint8_t>(flags), 5);  // 7 ^ 2 = 5
}

//==============================================================================
// Type Traits Tests
//==============================================================================

TEST_F(GodTest, TypeTraitsAliasesTest) {
    // Test if_t
    static_assert(std::is_same_v<if_t<true, int>, int>);
    static_assert(!std::is_same_v<if_t<false, int>, int>);

    // Test rmRefT
    static_assert(std::is_same_v<rmRefT<int&>, int>);
    static_assert(std::is_same_v<rmRefT<int&&>, int>);
    static_assert(std::is_same_v<rmRefT<const int&>, const int>);

    // Test rmCvT
    static_assert(std::is_same_v<rmCvT<const int>, int>);
    static_assert(std::is_same_v<rmCvT<volatile int>, int>);
    static_assert(std::is_same_v<rmCvT<const volatile int>, int>);

    // Test rmCvRefT
    static_assert(std::is_same_v<rmCvRefT<const int&>, int>);
    static_assert(std::is_same_v<rmCvRefT<volatile int&&>, int>);

    // Test rmArrT
    static_assert(std::is_same_v<rmArrT<int[10]>, int>);
    static_assert(std::is_same_v<rmArrT<int[][10]>, int[10]>);

    // Test constT
    static_assert(std::is_same_v<constT<int>, const int>);
    static_assert(std::is_same_v<constT<const int>, const int>);

    // Test constRefT
    static_assert(std::is_same_v<constRefT<int>, const int&>);
    static_assert(std::is_same_v<constRefT<int&>, const int&>);

    // Test rmPtrT
    static_assert(std::is_same_v<rmPtrT<int*>, int>);
    static_assert(std::is_same_v<rmPtrT<const int*>, const int>);

    // No direct assert for isNothrowRelocatable, just a compile test
    constexpr bool relocatable = isNothrowRelocatable<int>;
    EXPECT_TRUE(relocatable);
}

TEST_F(GodTest, IsSameTest) {
    // Test isSame function
    EXPECT_TRUE((isSame<int, int>()));
    EXPECT_FALSE((isSame<int, double>()));
    EXPECT_TRUE((isSame<int, int, int>()));
    EXPECT_FALSE((isSame<int, int, double>()));

    // Test with more complex types
    EXPECT_TRUE((isSame<std::vector<int>, std::vector<int>>()));
    EXPECT_FALSE((isSame<std::vector<int>, std::vector<double>>()));
}

TEST_F(GodTest, TypePredicatesTest) {
    // Test isRef
    EXPECT_TRUE(isRef<int&>());
    EXPECT_TRUE(isRef<int&&>());
    EXPECT_FALSE(isRef<int>());

    // Test isArray
    EXPECT_TRUE(isArray<int[]>());
    EXPECT_TRUE(isArray<int[10]>());
    EXPECT_FALSE(isArray<int>());
    EXPECT_FALSE(isArray<int*>());

    // Test isClass
    EXPECT_TRUE(isClass<std::vector<int>>());
    EXPECT_FALSE(isClass<int>());

    // Test isScalar
    EXPECT_TRUE(isScalar<int>());
    EXPECT_TRUE(isScalar<int*>());
    EXPECT_TRUE(isScalar<TestEnum>());
    EXPECT_FALSE(isScalar<std::vector<int>>());

    // Test isTriviallyCopyable
    EXPECT_TRUE(isTriviallyCopyable<int>());
    EXPECT_FALSE(isTriviallyCopyable<NonTriviallyCopyable>());

    // Test isTriviallyDestructible
    EXPECT_TRUE(isTriviallyDestructible<int>());
    EXPECT_FALSE(isTriviallyDestructible<std::vector<int>>());

    // Test isBaseOf
    EXPECT_TRUE((isBaseOf<Base, Derived>()));
    EXPECT_FALSE((isBaseOf<Derived, Base>()));

    // Test hasVirtualDestructor
    EXPECT_FALSE(hasVirtualDestructor<Base>());
    EXPECT_TRUE(hasVirtualDestructor<VirtualBase>());
    EXPECT_TRUE(hasVirtualDestructor<VirtualDerived>());
}

//==============================================================================
// Resource Management Tests
//==============================================================================

TEST_F(GodTest, ScopeGuardTest) {
    bool called = false;
    {
        auto guard = ScopeGuard([&called]() { called = true; });
        EXPECT_FALSE(called);
    }
    EXPECT_TRUE(called);  // Guard should execute at end of scope

    // Test dismiss functionality
    bool dismissed = false;
    {
        auto guard = ScopeGuard([&dismissed]() { dismissed = true; });
        guard.dismiss();
    }
    EXPECT_FALSE(dismissed);  // Guard was dismissed, shouldn't execute

    // Test move constructor
    bool movedFrom = false;
    bool movedTo = false;
    {
        auto guard1 = ScopeGuard([&movedFrom]() { movedFrom = true; });
        {
            auto guard2 = std::move(guard1);
            EXPECT_FALSE(movedFrom);
            EXPECT_FALSE(movedTo);

            // Replace function in guard2 to verify it works
            guard2 = ScopeGuard([&movedTo]() { movedTo = true; });
        }
        EXPECT_FALSE(movedFrom);  // guard1 was moved from, shouldn't execute
        EXPECT_TRUE(movedTo);     // guard2 should have executed
    }
}

TEST_F(GodTest, MakeGuardTest) {
    bool called = false;
    {
        auto guard = makeGuard([&called]() { called = true; });
        EXPECT_FALSE(called);
    }
    EXPECT_TRUE(called);  // Guard should execute at end of scope

    // Test with multiple guards
    int counter = 0;
    {
        auto guard1 = makeGuard([&counter]() { counter += 1; });
        {
            auto guard2 = makeGuard([&counter]() { counter += 2; });
            {
                auto guard3 = makeGuard([&counter]() { counter += 3; });
            }
            EXPECT_EQ(counter, 3);  // guard3 executed
        }
        EXPECT_EQ(counter, 5);  // guard2 executed
    }
    EXPECT_EQ(counter, 6);  // guard1 executed
}

TEST_F(GodTest, SingletonTest) {
    // Test singleton function
    struct TestSingleton {
        int value = 42;
        void setValue(int val) { value = val; }
    };

    // Access the singleton and modify it
    TestSingleton& instance1 = singleton<TestSingleton>();
    EXPECT_EQ(instance1.value, 42);

    instance1.setValue(100);

    // Get another reference to the singleton and check if the modification
    // persists
    TestSingleton& instance2 = singleton<TestSingleton>();
    EXPECT_EQ(instance2.value, 100);
    EXPECT_EQ(&instance1, &instance2);  // Should be the same object

    // Test with a different type
    struct AnotherSingleton {
        std::string name = "default";
    };

    AnotherSingleton& anotherInstance = singleton<AnotherSingleton>();
    EXPECT_EQ(anotherInstance.name, "default");

    // Different singleton types should have different instances
    EXPECT_NE(reinterpret_cast<void*>(&instance1),
              reinterpret_cast<void*>(&anotherInstance));
}

//==============================================================================
// Compilation Tests
//==============================================================================

// This section contains tests that mainly verify that the code compiles
// properly and that templates work with different types

TEST_F(GodTest, CompilationTest) {
    // These tests don't assert anything directly, they just verify the code
    // compiles

    // Test BitwiseOperatable concept
    static_assert(BitwiseOperatable<int>);
    static_assert(BitwiseOperatable<unsigned char>);
    static_assert(BitwiseOperatable<int*>);
    static_assert(BitwiseOperatable<TestEnum>);
    static_assert(!BitwiseOperatable<double>);
    static_assert(!BitwiseOperatable<std::string>);

    // Test Alignable concept
    static_assert(Alignable<int>);
    static_assert(Alignable<void*>);
    static_assert(!Alignable<double>);
    static_assert(!Alignable<std::string>);

    // Test TriviallyCopyable concept
    static_assert(TriviallyCopyable<int>);
    static_assert(TriviallyCopyable<int*>);
    static_assert(!TriviallyCopyable<std::string>);
    static_assert(!TriviallyCopyable<NonTriviallyCopyable>);
}

//==============================================================================
// Thread Safety Tests
//==============================================================================

TEST_F(GodTest, AtomicThreadSafetyTest) {
    constexpr int kNumThreads = 10;
    constexpr int kIterationsPerThread = 1000;

    std::atomic<int> counter(0);

    // Create multiple threads that increment the counter
    std::vector<std::thread> threads;
    for (int i = 0; i < kNumThreads; ++i) {
        threads.emplace_back([&counter, kIterationsPerThread]() {
            for (int j = 0; j < kIterationsPerThread; ++j) {
                atomicFetchAdd(&counter, 1);
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify the final counter value
    EXPECT_EQ(counter.load(), kNumThreads * kIterationsPerThread);
}

}  // namespace atom::meta::test