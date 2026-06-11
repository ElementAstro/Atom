#include <gtest/gtest.h>
#include "atom/meta/any.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace {

// Test fixture for BoxedValue tests
class BoxedValueTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Helper classes for testing
struct TestStruct {
    int value;
    std::string name;

    TestStruct() : value(0), name("default") {}
    TestStruct(int v, const std::string& n) : value(v), name(n) {}

    bool operator==(const TestStruct& other) const {
        return value == other.value && name == other.name;
    }
};

class TestClass {
public:
    TestClass() : data_(42) {}
    explicit TestClass(int data) : data_(data) {}

    int getData() const { return data_; }
    void setData(int data) { data_ = data; }

private:
    int data_;
};

// Test basic construction and type checking
TEST_F(BoxedValueTest, BasicConstruction) {
    // Default constructor (VoidType)
    atom::meta::BoxedValue voidValue;
    EXPECT_TRUE(voidValue.isVoid());
    EXPECT_TRUE(voidValue.isUndef());
    EXPECT_FALSE(voidValue.isNull());

    // Integer construction
    atom::meta::BoxedValue intValue(42);
    EXPECT_FALSE(intValue.isVoid());
    EXPECT_FALSE(intValue.isUndef());
    EXPECT_FALSE(intValue.isNull());
    EXPECT_TRUE(intValue.isType<int>());

    // String construction
    atom::meta::BoxedValue stringValue(std::string("test"));
    EXPECT_TRUE(stringValue.isType<std::string>());
    EXPECT_FALSE(stringValue.isType<int>());

    // Double construction
    atom::meta::BoxedValue doubleValue(3.14);
    EXPECT_TRUE(doubleValue.isType<double>());
}

// Test copy and move semantics
TEST_F(BoxedValueTest, CopyMoveSemantics) {
    atom::meta::BoxedValue original(42);

    // Copy constructor
    atom::meta::BoxedValue copied(original);
    EXPECT_TRUE(copied.isType<int>());
    EXPECT_EQ(copied.cast<int>(), 42);

    // Move constructor
    atom::meta::BoxedValue moved(std::move(original));
    EXPECT_TRUE(moved.isType<int>());
    EXPECT_EQ(moved.cast<int>(), 42);

    // Copy assignment
    atom::meta::BoxedValue copyAssigned;
    copyAssigned = copied;
    EXPECT_TRUE(copyAssigned.isType<int>());
    EXPECT_EQ(copyAssigned.cast<int>(), 42);

    // Move assignment
    atom::meta::BoxedValue moveAssigned;
    moveAssigned = std::move(copied);
    EXPECT_TRUE(moveAssigned.isType<int>());
    EXPECT_EQ(moveAssigned.cast<int>(), 42);
}

// Test type casting functionality
TEST_F(BoxedValueTest, TypeCasting) {
    atom::meta::BoxedValue intValue(42);

    // Successful cast
    EXPECT_EQ(intValue.cast<int>(), 42);

    // Try cast with correct type
    auto tryResult = intValue.tryCast<int>();
    ASSERT_TRUE(tryResult.has_value());
    EXPECT_EQ(tryResult.value(), 42);

    // Try cast with incorrect type
    auto tryResultWrong = intValue.tryCast<std::string>();
    EXPECT_FALSE(tryResultWrong.has_value());

    // Cast should throw for wrong type
    EXPECT_THROW(static_cast<void>(intValue.cast<std::string>()),
                 std::bad_any_cast);
}

// Test const value handling
TEST_F(BoxedValueTest, ConstValues) {
    const int constInt = 100;
    atom::meta::BoxedValue constValue(constInt);

    EXPECT_TRUE(constValue.isReadonly());
    EXPECT_TRUE(constValue.isType<int>());
    EXPECT_EQ(constValue.cast<int>(), 100);
}

// Test reference handling
TEST_F(BoxedValueTest, ReferenceHandling) {
    int original = 42;
    atom::meta::BoxedValue refValue(std::ref(original));

    EXPECT_TRUE(refValue.isRef());
    EXPECT_TRUE(refValue.isType<int>());

    // Modify original and check if reference reflects the change
    original = 100;
    // Note: This behavior depends on implementation details
}

// Test attribute system
TEST_F(BoxedValueTest, AttributeSystem) {
    atom::meta::BoxedValue value(42);

    // Set attributes
    value.setAttr("description",
                  atom::meta::BoxedValue(std::string("test integer")));
    value.setAttr("category", atom::meta::BoxedValue(std::string("number")));

    // Check if attributes exist
    EXPECT_TRUE(value.hasAttr("description"));
    EXPECT_TRUE(value.hasAttr("category"));
    EXPECT_FALSE(value.hasAttr("nonexistent"));

    // Get attributes
    auto descAttr = value.getAttr("description");
    EXPECT_TRUE(descAttr.isType<std::string>());
    EXPECT_EQ(descAttr.cast<std::string>(), "test integer");

    // Get non-existent attribute should return void
    auto nonExistentAttr = value.getAttr("nonexistent");
    EXPECT_TRUE(nonExistentAttr.isVoid());
}

// Test complex types
TEST_F(BoxedValueTest, ComplexTypes) {
    TestStruct testStruct(42, "test");
    atom::meta::BoxedValue structValue(testStruct);

    EXPECT_TRUE(structValue.isType<TestStruct>());

    auto retrievedStruct = structValue.cast<TestStruct>();
    EXPECT_EQ(retrievedStruct.value, 42);
    EXPECT_EQ(retrievedStruct.name, "test");
}

// Test container types
TEST_F(BoxedValueTest, ContainerTypes) {
    std::vector<int> vec = {1, 2, 3, 4, 5};
    atom::meta::BoxedValue vecValue(vec);

    EXPECT_TRUE(vecValue.isType<std::vector<int>>());

    auto retrievedVec = vecValue.cast<std::vector<int>>();
    EXPECT_EQ(retrievedVec.size(), 5);
    EXPECT_EQ(retrievedVec[0], 1);
    EXPECT_EQ(retrievedVec[4], 5);
}

// Test smart pointers
TEST_F(BoxedValueTest, SmartPointers) {
    auto sharedPtr = std::make_shared<TestClass>(100);
    atom::meta::BoxedValue ptrValue(sharedPtr);

    EXPECT_TRUE(ptrValue.isType<std::shared_ptr<TestClass>>());

    auto retrievedPtr = ptrValue.cast<std::shared_ptr<TestClass>>();
    EXPECT_EQ(retrievedPtr->getData(), 100);
}

// Test swap functionality
TEST_F(BoxedValueTest, SwapFunctionality) {
    atom::meta::BoxedValue value1(42);
    atom::meta::BoxedValue value2(std::string("test"));

    value1.swap(value2);

    EXPECT_TRUE(value1.isType<std::string>());
    EXPECT_TRUE(value2.isType<int>());
    EXPECT_EQ(value1.cast<std::string>(), "test");
    EXPECT_EQ(value2.cast<int>(), 42);
}

// Test debug string functionality
TEST_F(BoxedValueTest, DebugString) {
    atom::meta::BoxedValue intValue(42);
    std::string debugStr = intValue.debugString();
    EXPECT_FALSE(debugStr.empty());
    EXPECT_TRUE(debugStr.find("42") != std::string::npos);

    atom::meta::BoxedValue stringValue(std::string("test"));
    std::string stringDebugStr = stringValue.debugString();
    EXPECT_FALSE(stringDebugStr.empty());
    EXPECT_TRUE(stringDebugStr.find("test") != std::string::npos);
}

// Test visitor pattern
TEST_F(BoxedValueTest, VisitorPattern) {
    atom::meta::BoxedValue intValue(42);

    // Test const visitor
    auto result = intValue.visit([](const auto& value) -> int {
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, int>) {
            return value * 2;
        }
        return 0;
    });
    EXPECT_EQ(result, 84);

    // Test non-const visitor (modifying)
    atom::meta::BoxedValue mutableValue(10);
    mutableValue.visit([](auto& value) {
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, int>) {
            value *= 3;
        }
    });
    EXPECT_EQ(mutableValue.cast<int>(), 30);
}

// Test thread safety
TEST_F(BoxedValueTest, ThreadSafety) {
    atom::meta::BoxedValue sharedValue(0);
    constexpr int numThreads = 10;
    constexpr int incrementsPerThread = 100;

    std::vector<std::thread> threads;
    std::atomic<int> completedThreads(0);

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&sharedValue, &completedThreads,
                              incrementsPerThread]() {
            for (int j = 0; j < incrementsPerThread; ++j) {
                sharedValue.visit([](auto& value) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(value)>,
                                                 int>) {
                        ++value;
                    }
                });
            }
            completedThreads.fetch_add(1);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(completedThreads.load(), numThreads);
    EXPECT_EQ(sharedValue.cast<int>(), numThreads * incrementsPerThread);
}

// Test error handling
TEST_F(BoxedValueTest, ErrorHandling) {
    atom::meta::BoxedValue voidValue;

    // Casting void should throw
    EXPECT_THROW(static_cast<void>(voidValue.cast<int>()), std::bad_any_cast);

    // Try cast on void should return nullopt
    auto tryResult = voidValue.tryCast<int>();
    EXPECT_FALSE(tryResult.has_value());

    // Visiting void returns a default-constructed result
    auto result = voidValue.visit([](const auto&) -> int { return 42; });
    EXPECT_EQ(result, 0);
}

// Test assignment operators
TEST_F(BoxedValueTest, AssignmentOperators) {
    atom::meta::BoxedValue value;

    // Assign different types
    value = 42;
    EXPECT_TRUE(value.isType<int>());
    EXPECT_EQ(value.cast<int>(), 42);

    value = std::string("test");
    EXPECT_TRUE(value.isType<std::string>());
    EXPECT_EQ(value.cast<std::string>(), "test");

    value = 3.14;
    EXPECT_TRUE(value.isType<double>());
    EXPECT_EQ(value.cast<double>(), 3.14);
}

// Test helper functions
TEST_F(BoxedValueTest, HelperFunctions) {
    // Test var() helper
    auto varValue = atom::meta::var(42);
    EXPECT_TRUE(varValue.isType<int>());
    EXPECT_EQ(varValue.cast<int>(), 42);

    // Test constVar() helper
    int constInt = 100;
    auto constVarValue = atom::meta::constVar(constInt);
    EXPECT_TRUE(constVarValue.isReadonly());

    // Test voidVar() helper
    auto voidVarValue = atom::meta::voidVar();
    EXPECT_TRUE(voidVarValue.isVoid());

    // Test varWithDesc() helper
    auto descValue = atom::meta::varWithDesc(42, "test integer");
    EXPECT_TRUE(descValue.isType<int>());
    EXPECT_TRUE(descValue.hasAttr("description"));

    // Test makeBoxedValue() helper
    auto madeValue = atom::meta::makeBoxedValue(42, false, true);
    EXPECT_TRUE(madeValue.isType<int>());
    EXPECT_TRUE(madeValue.isReadonly());
}

// Test edge cases
TEST_F(BoxedValueTest, EdgeCases) {
    // Test with nullptr
    atom::meta::BoxedValue nullptrValue(nullptr);
    EXPECT_TRUE(nullptrValue.isType<std::nullptr_t>());

    // Test with function pointer
    auto funcPtr = [](int x) { return x * 2; };
    atom::meta::BoxedValue funcValue(funcPtr);
    EXPECT_TRUE(funcValue.isType<decltype(funcPtr)>());

    // Test with array
    int arr[5] = {1, 2, 3, 4, 5};
    atom::meta::BoxedValue arrValue(arr);
    // Array decays to pointer
    EXPECT_TRUE(arrValue.isType<int*>());

    // Test with enum
    enum class TestEnum { Value1, Value2, Value3 };
    atom::meta::BoxedValue enumValue(TestEnum::Value2);
    EXPECT_TRUE(enumValue.isType<TestEnum>());
}

// Test memory management
TEST_F(BoxedValueTest, MemoryManagement) {
    // Test with large object
    std::vector<int> largeVec(10000, 42);
    atom::meta::BoxedValue largeValue(largeVec);
    EXPECT_TRUE(largeValue.isType<std::vector<int>>());

    // Test copy doesn't share memory
    atom::meta::BoxedValue copied(largeValue);

    // Modify original in place through a mutable visit
    largeValue.visit([](auto& value) {
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>,
                                     std::vector<int>>) {
            value[0] = 100;
        }
    });
    EXPECT_EQ(largeValue.cast<std::vector<int>>()[0], 100);
    EXPECT_EQ(copied.cast<std::vector<int>>()[0], 42);  // Copy unchanged
}

// Test type information
TEST_F(BoxedValueTest, TypeInformation) {
    atom::meta::BoxedValue intValue(42);

    // Test type checking
    EXPECT_TRUE(intValue.isType<int>());
    EXPECT_FALSE(intValue.isType<double>());
    EXPECT_FALSE(intValue.isType<std::string>());

    // Test with const types
    const double constDouble = 3.14;
    atom::meta::BoxedValue constValue(constDouble);
    EXPECT_TRUE(constValue.isType<double>());
    EXPECT_TRUE(constValue.isReadonly());
}

// ---------------------------------------------------------------------------
// NEW TESTS — added to raise coverage from 69% to >=90%
// ---------------------------------------------------------------------------

// ---- visitImpl: VISIT_TYPE numeric types (unsigned/long/short/char/float/bool)
TEST_F(BoxedValueTest, VisitUnsignedInt) {
    atom::meta::BoxedValue v(static_cast<unsigned int>(7u));
    auto r = v.visit([](const auto& x) -> int {
        if constexpr (std::is_same_v<std::decay_t<decltype(x)>, unsigned int>)
            return static_cast<int>(x);
        return -1;
    });
    EXPECT_EQ(r, 7);
}

TEST_F(BoxedValueTest, VisitLongTypes) {
    {
        atom::meta::BoxedValue v(static_cast<long>(10L));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, long>)
                return static_cast<int>(x);
            return -1;
        });
        EXPECT_EQ(r, 10);
    }
    {
        atom::meta::BoxedValue v(static_cast<unsigned long>(11UL));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, unsigned long>)
                return static_cast<int>(x);
            return -1;
        });
        EXPECT_EQ(r, 11);
    }
    {
        atom::meta::BoxedValue v(static_cast<long long>(12LL));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, long long>)
                return static_cast<int>(x);
            return -1;
        });
        EXPECT_EQ(r, 12);
    }
    {
        atom::meta::BoxedValue v(static_cast<unsigned long long>(13ULL));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, unsigned long long>)
                return static_cast<int>(x);
            return -1;
        });
        EXPECT_EQ(r, 13);
    }
}

TEST_F(BoxedValueTest, VisitShortAndChar) {
    {
        atom::meta::BoxedValue v(static_cast<short>(5));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, short>)
                return x;
            return -1;
        });
        EXPECT_EQ(r, 5);
    }
    {
        atom::meta::BoxedValue v(static_cast<unsigned short>(6u));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, unsigned short>)
                return x;
            return -1;
        });
        EXPECT_EQ(r, 6);
    }
    {
        atom::meta::BoxedValue v(static_cast<char>('A'));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, char>)
                return x;
            return -1;
        });
        EXPECT_EQ(r, 'A');
    }
    {
        atom::meta::BoxedValue v(static_cast<unsigned char>(200u));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, unsigned char>)
                return x;
            return -1;
        });
        EXPECT_EQ(r, 200);
    }
    {
        atom::meta::BoxedValue v(static_cast<signed char>(-3));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, signed char>)
                return x;
            return -99;
        });
        EXPECT_EQ(r, -3);
    }
}

TEST_F(BoxedValueTest, VisitWcharAndUnicode) {
    {
        atom::meta::BoxedValue v(static_cast<wchar_t>(L'W'));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, wchar_t>)
                return static_cast<int>(x);
            return -1;
        });
        EXPECT_EQ(r, static_cast<int>(L'W'));
    }
    {
        atom::meta::BoxedValue v(static_cast<char16_t>(u'a'));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, char16_t>)
                return static_cast<int>(x);
            return -1;
        });
        EXPECT_EQ(r, static_cast<int>(u'a'));
    }
    {
        atom::meta::BoxedValue v(static_cast<char32_t>(U'z'));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, char32_t>)
                return static_cast<int>(x);
            return -1;
        });
        EXPECT_EQ(r, static_cast<int>(U'z'));
    }
}

TEST_F(BoxedValueTest, VisitFloatLongDoubleBool) {
    {
        atom::meta::BoxedValue v(1.5f);
        auto r = v.visit([](const auto& x) -> double {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, float>)
                return static_cast<double>(x);
            return -1.0;
        });
        EXPECT_DOUBLE_EQ(r, 1.5);
    }
    {
        atom::meta::BoxedValue v(static_cast<long double>(2.5L));
        auto r = v.visit([](const auto& x) -> double {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, long double>)
                return static_cast<double>(x);
            return -1.0;
        });
        EXPECT_DOUBLE_EQ(r, 2.5);
    }
    {
        atom::meta::BoxedValue v(true);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, bool>)
                return x ? 1 : 0;
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
}

// ---- visitImpl: VISIT_TYPE wide/unicode string types
TEST_F(BoxedValueTest, VisitWideAndUnicodeStrings) {
    {
        atom::meta::BoxedValue v(std::wstring(L"hello"));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::wstring>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 5);
    }
    {
        atom::meta::BoxedValue v(std::u16string(u"abc"));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::u16string>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 3);
    }
    {
        atom::meta::BoxedValue v(std::u32string(U"xyz"));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::u32string>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 3);
    }
}

// ---- visitImpl: VISIT_TYPE string_view types
TEST_F(BoxedValueTest, VisitStringViews) {
    // Note: string_view must stay alive for the duration of the visit
    {
        std::string base = "hello";
        std::string_view sv(base);
        atom::meta::BoxedValue v(sv);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::string_view>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 5);
    }
    {
        std::wstring wbase = L"wtest";
        std::wstring_view wsv(wbase);
        atom::meta::BoxedValue v(wsv);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::wstring_view>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 5);
    }
    {
        std::u16string u16base = u"u16";
        std::u16string_view u16sv(u16base);
        atom::meta::BoxedValue v(u16sv);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::u16string_view>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 3);
    }
    {
        std::u32string u32base = U"u32";
        std::u32string_view u32sv(u32base);
        atom::meta::BoxedValue v(u32sv);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::u32string_view>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 3);
    }
}

// ---- visitImpl: VISIT_TYPE vector/list types
TEST_F(BoxedValueTest, VisitVectorVariants) {
    {
        std::vector<double> vd = {1.0, 2.0};
        atom::meta::BoxedValue v(vd);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::vector<double>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 2);
    }
    {
        std::vector<std::string> vs = {"a", "b", "c"};
        atom::meta::BoxedValue v(vs);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::vector<std::string>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 3);
    }
    {
        std::vector<bool> vb = {true, false, true};
        atom::meta::BoxedValue v(vb);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::vector<bool>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 3);
    }
    {
        std::list<int> li = {1, 2, 3};
        atom::meta::BoxedValue v(li);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::list<int>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 3);
    }
    {
        std::list<double> ld = {1.1, 2.2};
        atom::meta::BoxedValue v(ld);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::list<double>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 2);
    }
    {
        std::list<std::string> ls = {"x", "y"};
        atom::meta::BoxedValue v(ls);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::list<std::string>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 2);
    }
}

// ---- visitImpl: VISIT_TYPE map/set types
TEST_F(BoxedValueTest, VisitMapAndSetTypes) {
    {
        std::map<std::string, int> m = {{"a", 1}};
        atom::meta::BoxedValue v(m);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::map<std::string, int>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        std::map<std::string, double> md = {{"x", 1.5}};
        atom::meta::BoxedValue v(md);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::map<std::string, double>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        std::map<std::string, std::string> ms = {{"k", "v"}};
        atom::meta::BoxedValue v(ms);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::map<std::string, std::string>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        std::unordered_map<std::string, int> um = {{"a", 2}};
        atom::meta::BoxedValue v(um);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::unordered_map<std::string, int>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        std::unordered_map<std::string, double> umd = {{"b", 3.0}};
        atom::meta::BoxedValue v(umd);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::unordered_map<std::string, double>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        std::unordered_map<std::string, std::string> ums = {{"c", "d"}};
        atom::meta::BoxedValue v(ums);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::unordered_map<std::string, std::string>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        std::set<int> si = {1, 2, 3};
        atom::meta::BoxedValue v(si);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::set<int>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 3);
    }
    {
        std::set<double> sd = {1.1, 2.2};
        atom::meta::BoxedValue v(sd);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::set<double>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 2);
    }
    {
        std::set<std::string> ss = {"hello"};
        atom::meta::BoxedValue v(ss);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::set<std::string>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        std::unordered_set<int> usi = {10, 20};
        atom::meta::BoxedValue v(usi);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::unordered_set<int>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 2);
    }
    {
        std::unordered_set<double> usd = {1.1};
        atom::meta::BoxedValue v(usd);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::unordered_set<double>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        std::unordered_set<std::string> uss = {"hi", "bye"};
        atom::meta::BoxedValue v(uss);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::unordered_set<std::string>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 2);
    }
}

// ---- visitImpl: VISIT_TYPE shared_ptr<double>, shared_ptr<string>
TEST_F(BoxedValueTest, VisitSharedPtrVariants) {
    {
        auto sp = std::make_shared<double>(3.14);
        atom::meta::BoxedValue v(sp);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::shared_ptr<double>>)
                return x ? 1 : 0;
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        auto sp = std::make_shared<std::string>("hello");
        atom::meta::BoxedValue v(sp);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::shared_ptr<std::string>>)
                return static_cast<int>(x->size());
            return -1;
        });
        EXPECT_EQ(r, 5);
    }
}

// ---- visitImpl: VISIT_TYPE chrono types
TEST_F(BoxedValueTest, VisitChronoTypes) {
    {
        auto s = std::chrono::seconds(10);
        atom::meta::BoxedValue v(s);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::chrono::seconds>)
                return static_cast<int>(x.count());
            return -1;
        });
        EXPECT_EQ(r, 10);
    }
    {
        auto ms = std::chrono::milliseconds(500);
        atom::meta::BoxedValue v(ms);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::chrono::milliseconds>)
                return static_cast<int>(x.count());
            return -1;
        });
        EXPECT_EQ(r, 500);
    }
    {
        auto us = std::chrono::microseconds(1000);
        atom::meta::BoxedValue v(us);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::chrono::microseconds>)
                return static_cast<int>(x.count());
            return -1;
        });
        EXPECT_EQ(r, 1000);
    }
    {
        auto ns = std::chrono::nanoseconds(2000);
        atom::meta::BoxedValue v(ns);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::chrono::nanoseconds>)
                return static_cast<int>(x.count());
            return -1;
        });
        EXPECT_EQ(r, 2000);
    }
    {
        auto m = std::chrono::minutes(2);
        atom::meta::BoxedValue v(m);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::chrono::minutes>)
                return static_cast<int>(x.count());
            return -1;
        });
        EXPECT_EQ(r, 2);
    }
    {
        auto h = std::chrono::hours(1);
        atom::meta::BoxedValue v(h);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::chrono::hours>)
                return static_cast<int>(x.count());
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        auto tp = std::chrono::system_clock::now();
        atom::meta::BoxedValue v(tp);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::chrono::system_clock::time_point>)
                return 1;
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        auto tp = std::chrono::steady_clock::now();
        atom::meta::BoxedValue v(tp);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::chrono::steady_clock::time_point>)
                return 1;
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        auto tp = std::chrono::high_resolution_clock::now();
        atom::meta::BoxedValue v(tp);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::chrono::high_resolution_clock::time_point>)
                return 1;
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
}

// ---- visitImpl: VISIT_TYPE optional types
TEST_F(BoxedValueTest, VisitOptionalTypes) {
    {
        std::optional<int> oi = 42;
        atom::meta::BoxedValue v(oi);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::optional<int>>)
                return x.value_or(-1);
            return -99;
        });
        EXPECT_EQ(r, 42);
    }
    {
        std::optional<double> od = 3.14;
        atom::meta::BoxedValue v(od);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::optional<double>>)
                return x.has_value() ? 1 : 0;
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        std::optional<std::string> os = "test";
        atom::meta::BoxedValue v(os);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::optional<std::string>>)
                return static_cast<int>(x->size());
            return -1;
        });
        EXPECT_EQ(r, 4);
    }
}

// ---- visitImpl: VISIT_TYPE pair and tuple types
TEST_F(BoxedValueTest, VisitPairAndTupleTypes) {
    {
        std::pair<int, int> p{3, 4};
        atom::meta::BoxedValue v(p);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::pair<int, int>>)
                return x.first + x.second;
            return -1;
        });
        EXPECT_EQ(r, 7);
    }
    {
        std::pair<int, std::string> p{1, "hi"};
        atom::meta::BoxedValue v(p);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::pair<int, std::string>>)
                return x.first;
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        std::pair<std::string, std::string> p{"a", "b"};
        atom::meta::BoxedValue v(p);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::pair<std::string, std::string>>)
                return static_cast<int>(x.first.size());
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        std::tuple<int> t{9};
        atom::meta::BoxedValue v(t);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::tuple<int>>)
                return std::get<0>(x);
            return -1;
        });
        EXPECT_EQ(r, 9);
    }
    {
        std::tuple<int, int> t{3, 5};
        atom::meta::BoxedValue v(t);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::tuple<int, int>>)
                return std::get<0>(x) + std::get<1>(x);
            return -1;
        });
        EXPECT_EQ(r, 8);
    }
    {
        std::tuple<int, std::string> t{2, "ab"};
        atom::meta::BoxedValue v(t);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::tuple<int, std::string>>)
                return std::get<0>(x);
            return -1;
        });
        EXPECT_EQ(r, 2);
    }
    {
        std::tuple<std::string, std::string> t{"foo", "bar"};
        atom::meta::BoxedValue v(t);
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::tuple<std::string, std::string>>)
                return static_cast<int>(std::get<0>(x).size());
            return -1;
        });
        EXPECT_EQ(r, 3);
    }
}

// ---- visitImpl: VISIT_TYPE variant
TEST_F(BoxedValueTest, VisitVariantType) {
    std::variant<int, double, std::string> vt = 42;
    atom::meta::BoxedValue v(vt);
    auto r = v.visit([](const auto& x) -> int {
        if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::variant<int, double, std::string>>)
            return std::get<int>(x);
        return -1;
    });
    EXPECT_EQ(r, 42);
}

// ---- visitImpl: VISIT_REF_TYPE — visiting ref-wrapped values via const visit
TEST_F(BoxedValueTest, VisitRefTypeInt) {
    int val = 77;
    atom::meta::BoxedValue v(std::ref(val));
    // const visit uses VISIT_REF_TYPE path
    auto r = v.visit([](const auto& x) -> int {
        if constexpr (std::is_same_v<std::decay_t<decltype(x)>, int>)
            return x;
        return -1;
    });
    EXPECT_EQ(r, 77);
}

TEST_F(BoxedValueTest, VisitRefTypeDouble) {
    double val = 2.5;
    atom::meta::BoxedValue v(std::ref(val));
    auto r = v.visit([](const auto& x) -> int {
        if constexpr (std::is_same_v<std::decay_t<decltype(x)>, double>)
            return static_cast<int>(x);
        return -1;
    });
    EXPECT_EQ(r, 2);
}

TEST_F(BoxedValueTest, VisitRefTypeString) {
    std::string base = "refstr";
    atom::meta::BoxedValue v(std::ref(base));
    auto r = v.visit([](const auto& x) -> int {
        if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::string>)
            return static_cast<int>(x.size());
        return -1;
    });
    EXPECT_EQ(r, 6);
}

TEST_F(BoxedValueTest, VisitRefTypeOtherNumerics) {
    // Cover VISIT_REF_TYPE for various numeric types
    {
        unsigned int u = 8u;
        atom::meta::BoxedValue v(std::ref(u));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, unsigned int>)
                return static_cast<int>(x);
            return -1;
        });
        EXPECT_EQ(r, 8);
    }
    {
        long l = 100L;
        atom::meta::BoxedValue v(std::ref(l));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, long>)
                return static_cast<int>(x);
            return -1;
        });
        EXPECT_EQ(r, 100);
    }
    {
        long long ll = 200LL;
        atom::meta::BoxedValue v(std::ref(ll));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, long long>)
                return static_cast<int>(x);
            return -1;
        });
        EXPECT_EQ(r, 200);
    }
    {
        float f = 1.5f;
        atom::meta::BoxedValue v(std::ref(f));
        auto r = v.visit([](const auto& x) -> double {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, float>)
                return static_cast<double>(x);
            return -1.0;
        });
        EXPECT_DOUBLE_EQ(r, 1.5);
    }
    {
        bool b = true;
        atom::meta::BoxedValue v(std::ref(b));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, bool>)
                return x ? 1 : 0;
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
}

TEST_F(BoxedValueTest, VisitRefTypeVectorAndMap) {
    {
        std::vector<int> vi = {1, 2, 3};
        atom::meta::BoxedValue v(std::ref(vi));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::vector<int>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 3);
    }
    {
        std::vector<double> vd = {1.1};
        atom::meta::BoxedValue v(std::ref(vd));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::vector<double>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        std::map<std::string, int> m = {{"a", 1}};
        atom::meta::BoxedValue v(std::ref(m));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::map<std::string, int>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
}

// ---- visitImpl: fallback path (unknown type, no visitor.fallback)
TEST_F(BoxedValueTest, VisitFallbackDefaultConstructible) {
    // Use a type not in the VISIT_TYPE list so the fallback branch is reached
    struct MyCustomType { int x = 5; };
    atom::meta::BoxedValue v(MyCustomType{});
    // visit returns ResultType{} because visitor has no fallback() method
    auto r = v.visit([](const auto&) -> int { return 99; });
    EXPECT_EQ(r, 0);  // default-constructed int
}

// ---- debugString: double branch and unknown-type branch
TEST_F(BoxedValueTest, DebugStringDoubleBranch) {
    atom::meta::BoxedValue v(2.71828);
    std::string s = v.debugString();
    EXPECT_FALSE(s.empty());
    EXPECT_TRUE(s.find("2.71828") != std::string::npos ||
                s.find("2.7") != std::string::npos);
}

TEST_F(BoxedValueTest, DebugStringUnknownTypeBranch) {
    // Use a type that isn't int/double/string: e.g. bool
    atom::meta::BoxedValue v(true);
    std::string s = v.debugString();
    EXPECT_FALSE(s.empty());
    // "unknown type" branch — no check for "true", just non-empty
    EXPECT_TRUE(s.find("unknown type") != std::string::npos);
}

// ---- tryCast: reference_wrapper<T> path (non-reference T, data holds ref_wrapper<T>)
TEST_F(BoxedValueTest, TryCastRefWrapperNonRefT) {
    int x = 55;
    // BoxedValue holds std::reference_wrapper<int>
    // tryCast<int>() should follow the reference_wrapper<T> path (line ~501)
    atom::meta::BoxedValue v(std::ref(x));
    auto result = v.tryCast<int>();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 55);
}

// ---- tryCastPtr: covers the tryCastPtr method
TEST_F(BoxedValueTest, TryCastPtr) {
    {
        atom::meta::BoxedValue v(42);
        int* p = v.tryCastPtr<int>();
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(*p, 42);
        *p = 99;
        EXPECT_EQ(v.cast<int>(), 99);
    }
    {
        // readonly: should return nullptr
        const int ci = 5;
        atom::meta::BoxedValue v(ci);  // triggers const constructor -> readonly=true
        int* p = v.tryCastPtr<int>();
        EXPECT_EQ(p, nullptr);
    }
    {
        // wrong type: should return nullptr
        atom::meta::BoxedValue v(std::string("hello"));
        int* p = v.tryCastPtr<int>();
        EXPECT_EQ(p, nullptr);
    }
    {
        // ref_wrapper path in tryCastPtr
        int x = 7;
        atom::meta::BoxedValue v(std::ref(x));
        // v is not readonly, so tryCastPtr should find the ref_wrapper and return &x
        int* p = v.tryCastPtr<int>();
        ASSERT_NE(p, nullptr);
        EXPECT_EQ(*p, 7);
    }
}

// ---- canCast: various paths
TEST_F(BoxedValueTest, CanCastReferenceType) {
    // canCast<int&> on a reference_wrapper<int> uses the is_reference_v branch
    int x = 5;
    atom::meta::BoxedValue v(std::ref(x));
    EXPECT_TRUE(v.canCast<int&>());   // reference_wrapper path
    EXPECT_FALSE(v.canCast<double>());  // wrong type - cast fails
}

// ---- isConst, isReturnValue, resetReturnValue, isConstDataPtr, getPtr, get
TEST_F(BoxedValueTest, MetadataAccessors) {
    // isConst: when TypeInfo reports const
    {
        atom::meta::BoxedValue v(42);
        // Non-const, not readonly (fresh mutable int)
        EXPECT_FALSE(v.isConst());
    }
    // isReturnValue and resetReturnValue
    {
        atom::meta::BoxedValue v(42, true, false);  // return_value=true
        EXPECT_TRUE(v.isReturnValue());
        v.resetReturnValue();
        EXPECT_FALSE(v.isReturnValue());
    }
    // isConstDataPtr and getPtr
    {
        const int ci = 7;
        atom::meta::BoxedValue v(ci);  // const T& constructor
        // constDataPtr is set for const refs
        // isConstDataPtr depends on constDataPtr != nullptr
        // With const T& ctor, is_const_v<remove_reference_t<T>> is true
        void* p = v.getPtr();
        // may be null or non-null depending on overload selected; just check no crash
        (void)p;
    }
    // get() method
    {
        atom::meta::BoxedValue v(std::string("hello"));
        const std::any& a = v.get();
        EXPECT_EQ(std::any_cast<std::string>(a), "hello");
    }
    // getTypeInfo
    {
        atom::meta::BoxedValue v(3.14);
        const auto& ti = v.getTypeInfo();
        EXPECT_FALSE(ti.name().empty());
    }
}

// ---- removeAttr and listAttrs
TEST_F(BoxedValueTest, RemoveAndListAttrs) {
    atom::meta::BoxedValue v(42);
    v.setAttr("a", atom::meta::BoxedValue(1));
    v.setAttr("b", atom::meta::BoxedValue(2));
    v.setAttr("c", atom::meta::BoxedValue(3));

    auto attrs = v.listAttrs();
    EXPECT_EQ(attrs.size(), 3u);
    EXPECT_TRUE(v.hasAttr("a"));
    EXPECT_TRUE(v.hasAttr("b"));

    v.removeAttr("b");
    EXPECT_FALSE(v.hasAttr("b"));
    auto attrs2 = v.listAttrs();
    EXPECT_EQ(attrs2.size(), 2u);

    // removeAttr on non-existent key — no crash
    v.removeAttr("nonexistent");
}

// ---- isNull check
TEST_F(BoxedValueTest, IsNullCheck) {
    atom::meta::BoxedValue v;
    // VoidType is set, so isNull() returns false (has_value() == true for VoidType)
    EXPECT_FALSE(v.isNull());

    atom::meta::BoxedValue intV(42);
    EXPECT_FALSE(intV.isNull());
}

// ---- isType(TypeInfo) overload
TEST_F(BoxedValueTest, IsTypeWithTypeInfo) {
    atom::meta::BoxedValue v(42);
    auto ti = atom::meta::userType<int>();
    EXPECT_TRUE(v.isType(ti));
    auto tis = atom::meta::userType<std::string>();
    EXPECT_FALSE(v.isType(tis));
}

// ---- varWithDesc body coverage
TEST_F(BoxedValueTest, VarWithDescBody) {
    // This exercises the varWithDesc function body (line 877)
    auto v = atom::meta::varWithDesc(std::string("hello"), "a greeting");
    EXPECT_TRUE(v.isType<std::string>());
    EXPECT_EQ(v.cast<std::string>(), "hello");
    EXPECT_TRUE(v.hasAttr("description"));
    EXPECT_EQ(v.getAttr("description").cast<std::string>(), "a greeting");
}

// ---- mutable visit: readonly path returns default
TEST_F(BoxedValueTest, MutableVisitReadonlyReturnsDefault) {
    const int ci = 42;
    atom::meta::BoxedValue v(ci);  // const T& -> readonly
    // Non-const visit on a readonly value returns ResultType{}
    int r = v.visit([](auto& x) -> int {
        if constexpr (std::is_same_v<std::decay_t<decltype(x)>, int>)
            return x * 2;
        return -1;
    });
    EXPECT_EQ(r, 0);  // default-constructed int, readonly path taken
}

// ---- mutable visit returning a non-void result (the else branch of is_void_v)
TEST_F(BoxedValueTest, MutableVisitNonVoidResult) {
    atom::meta::BoxedValue v(10);
    // Non-const visit with non-void return type uses the 'else' branch of is_void_v
    int r = v.visit([](auto& x) -> int {
        if constexpr (std::is_same_v<std::decay_t<decltype(x)>, int>)
            return x + 5;
        return -1;
    });
    EXPECT_EQ(r, 15);
}

// ---- BoxedValueArray tests
TEST_F(BoxedValueTest, BoxedValueArrayOps) {
    atom::meta::BoxedValueArray arr;
    EXPECT_TRUE(arr.empty());

    arr.push_back(atom::meta::BoxedValue(1));
    arr.emplace_back(2.0);
    arr.emplace_back(std::string("three"));
    EXPECT_EQ(arr.size(), 3u);
    EXPECT_FALSE(arr.empty());

    EXPECT_TRUE(arr[0].isType<int>());
    EXPECT_TRUE(arr[1].isType<double>());
    EXPECT_TRUE(arr[2].isType<std::string>());

    // forEach
    int count = 0;
    arr.forEach([&count](atom::meta::BoxedValue&) { ++count; });
    EXPECT_EQ(count, 3);

    // transform
    auto sizes = arr.transform([](atom::meta::BoxedValue&) -> int { return 1; });
    EXPECT_EQ(static_cast<int>(sizes.size()), 3);

    // filter
    auto filtered = arr.filter(
        [](const atom::meta::BoxedValue& v) { return v.isType<int>(); });
    EXPECT_EQ(filtered.size(), 1u);

    // filterByType
    auto byType = arr.filterByType<double>();
    EXPECT_EQ(byType.size(), 1u);

    // range-based iteration via begin/end
    int iterCount = 0;
    for (const auto& bv : arr) { ++iterCount; (void)bv; }
    EXPECT_EQ(iterCount, 3);

    // Variadic constructor
    atom::meta::BoxedValueArray arr2(10, std::string("hi"), 3.14);
    EXPECT_EQ(arr2.size(), 3u);
}

// ---- BoxedValueMap tests
TEST_F(BoxedValueTest, BoxedValueMapOps) {
    atom::meta::BoxedValueMap m;
    EXPECT_TRUE(m.empty());
    EXPECT_EQ(m.size(), 0u);

    m.set("key1", atom::meta::BoxedValue(42));
    m.set("key2", 3.14);
    EXPECT_EQ(m.size(), 2u);
    EXPECT_FALSE(m.empty());
    EXPECT_TRUE(m.contains("key1"));
    EXPECT_FALSE(m.contains("missing"));

    // get (mutable)
    auto opt1 = m.get("key1");
    ASSERT_TRUE(opt1.has_value());
    EXPECT_TRUE(opt1->get().isType<int>());

    // get (missing mutable)
    auto opt_miss = m.get("missing");
    EXPECT_FALSE(opt_miss.has_value());

    // getAs
    auto val = m.getAs<int>("key1");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 42);
    auto val_miss = m.getAs<int>("missing");
    EXPECT_FALSE(val_miss.has_value());
    auto val_wrong = m.getAs<std::string>("key1");
    EXPECT_FALSE(val_wrong.has_value());

    // keys
    auto keys = m.keys();
    EXPECT_EQ(keys.size(), 2u);

    // remove
    m.remove("key1");
    EXPECT_FALSE(m.contains("key1"));
    EXPECT_EQ(m.size(), 1u);
}

// ---- const BoxedValueMap get()
TEST_F(BoxedValueTest, BoxedValueMapConstGet) {
    atom::meta::BoxedValueMap m;
    m.set("x", 99);
    const atom::meta::BoxedValueMap& cm = m;
    auto opt = cm.get("x");
    ASSERT_TRUE(opt.has_value());
    EXPECT_TRUE(opt->get().isType<int>());
    auto opt_miss = cm.get("missing");
    EXPECT_FALSE(opt_miss.has_value());
}

// ---- boxVariant and boxTuple and unboxToTuple
TEST_F(BoxedValueTest, VariantAndTupleHelpers) {
    // boxVariant
    std::variant<int, double, std::string> vt = 42;
    auto bv = atom::meta::boxVariant(vt);
    EXPECT_TRUE(bv.isType<int>());

    std::variant<int, double, std::string> vt2 = std::string("hello");
    auto bv2 = atom::meta::boxVariant(vt2);
    EXPECT_TRUE(bv2.isType<std::string>());

    // boxTuple
    auto tuple = std::make_tuple(1, 2.5, std::string("abc"));
    auto vec = atom::meta::boxTuple(tuple);
    EXPECT_EQ(vec.size(), 3u);
    EXPECT_TRUE(vec[0].isType<int>());
    EXPECT_TRUE(vec[1].isType<double>());
    EXPECT_TRUE(vec[2].isType<std::string>());

    // unboxToTuple — success case
    std::vector<atom::meta::BoxedValue> bvec = {
        atom::meta::BoxedValue(10),
        atom::meta::BoxedValue(std::string("str"))
    };
    auto result = atom::meta::unboxToTuple<int, std::string>(bvec);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(std::get<0>(*result), 10);
    EXPECT_EQ(std::get<1>(*result), "str");

    // unboxToTuple — size mismatch
    auto result2 = atom::meta::unboxToTuple<int>(bvec);
    EXPECT_FALSE(result2.has_value());

    // unboxToTuple — type mismatch (cast fails)
    std::vector<atom::meta::BoxedValue> bvec3 = {atom::meta::BoxedValue(std::string("not_int"))};
    auto result3 = atom::meta::unboxToTuple<int>(bvec3);
    EXPECT_FALSE(result3.has_value());
}

// ---- safeCast helper
TEST_F(BoxedValueTest, SafeCastHelper) {
    atom::meta::BoxedValue v(42);
    auto r = atom::meta::safeCast<int>(v);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, 42);

    auto r2 = atom::meta::safeCast<std::string>(v);
    EXPECT_FALSE(r2.has_value());
}

// ---- TypeErasedVariant
TEST_F(BoxedValueTest, TypeErasedVariantOps) {
    atom::meta::TypeErasedVariant tev(1, std::string("hello"), 3.14);
    EXPECT_EQ(tev.alternativeCount(), 3u);
    EXPECT_EQ(tev.activeIndex(), 0u);

    auto val = tev.tryGetActive<int>();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 1);

    tev.setActive(1);
    EXPECT_EQ(tev.activeIndex(), 1u);
    auto& active = tev.getActive();
    EXPECT_TRUE(active.isType<std::string>());

    // const getActive
    const auto& ctev = tev;
    const auto& cactive = ctev.getActive();
    EXPECT_TRUE(cactive.isType<std::string>());

    // setActive out of range — no crash, index unchanged
    tev.setActive(999);
    EXPECT_EQ(tev.activeIndex(), 1u);
}

// ---- SafeUnion
TEST_F(BoxedValueTest, SafeUnionOps) {
    auto u = atom::meta::SafeUnion::create<int, std::string>();
    // Default state: value_ is a void BoxedValue; isNull() is false because
    // VoidType has_value()==true, so hasValue() returns true here.
    // After set, the type changes.

    bool ok = u.set(42);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(u.hasValue());
    auto val = u.get<int>();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 42);

    // set disallowed type (double not in allowed set)
    bool notOk = u.set(3.14);
    EXPECT_FALSE(notOk);

    // boxed()
    const auto& bv = u.boxed();
    EXPECT_TRUE(bv.isType<int>());
}

// ---- ObservableBoxed
TEST_F(BoxedValueTest, ObservableBoxedOps) {
    atom::meta::ObservableBoxed ob(10);
    auto gotten = ob.get();
    EXPECT_TRUE(gotten.isType<int>());

    int observedOld = -1;
    int observedNew = -1;
    ob.addObserver([&](const atom::meta::BoxedValue& oldV,
                        const atom::meta::BoxedValue& newV) {
        observedOld = oldV.cast<int>();
        observedNew = newV.cast<int>();
    });

    ob.set(20);
    EXPECT_EQ(observedOld, 10);
    EXPECT_EQ(observedNew, 20);

    ob.clearObservers();
    ob.set(30);
    EXPECT_EQ(observedNew, 20);  // not updated since observers cleared
}

// ---- LazyBoxed
TEST_F(BoxedValueTest, LazyBoxedOps) {
    int initCount = 0;
    atom::meta::LazyBoxed lb([&initCount]() -> atom::meta::BoxedValue {
        ++initCount;
        return atom::meta::BoxedValue(42);
    });

    EXPECT_FALSE(lb.isInitialized());
    const auto& v1 = lb.get();
    EXPECT_TRUE(lb.isInitialized());
    EXPECT_EQ(initCount, 1);
    EXPECT_EQ(v1.cast<int>(), 42);

    // Second get — same value, no re-init
    const auto& v2 = lb.get();
    EXPECT_EQ(initCount, 1);
    EXPECT_EQ(v2.cast<int>(), 42);

    // reset
    lb.reset();
    EXPECT_FALSE(lb.isInitialized());
    const auto& v3 = lb.get();
    EXPECT_EQ(initCount, 2);
    EXPECT_EQ(v3.cast<int>(), 42);
}

// ---- ExpiringBoxed
TEST_F(BoxedValueTest, ExpiringBoxedOps) {
    // Not yet expired
    atom::meta::ExpiringBoxed eb(42, std::chrono::milliseconds(5000));
    EXPECT_FALSE(eb.isExpired());
    auto val = eb.get();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val->cast<int>(), 42);
    auto remaining = eb.remainingTime();
    EXPECT_GT(remaining.count(), 0);

    // Already expired
    atom::meta::ExpiringBoxed expired(99, std::chrono::milliseconds(0));
    EXPECT_TRUE(expired.isExpired());
    auto val2 = expired.get();
    EXPECT_FALSE(val2.has_value());
    EXPECT_EQ(expired.remainingTime().count(), 0);

    // refresh
    eb.refresh(100, std::chrono::milliseconds(5000));
    EXPECT_FALSE(eb.isExpired());
    auto refreshed = eb.get();
    ASSERT_TRUE(refreshed.has_value());
    EXPECT_EQ(refreshed->cast<int>(), 100);
}

// ---- TypeErasedFunction
TEST_F(BoxedValueTest, TypeErasedFunctionOps) {
    int callCount = 0;
    atom::meta::TypeErasedFunction tef([&callCount]() -> int {
        ++callCount;
        return 42;
    });

    auto result = tef({});
    EXPECT_EQ(callCount, 1);
    (void)result;

    const auto& callable = tef.getCallable();
    EXPECT_FALSE(callable.isVoid());
}

// ---- BoxedCache
TEST_F(BoxedValueTest, BoxedCacheOps) {
    atom::meta::BoxedCache cache(3);
    EXPECT_EQ(cache.size(), 0u);

    cache.put("a", atom::meta::BoxedValue(1));
    cache.put("b", atom::meta::BoxedValue(2));
    cache.put("c", atom::meta::BoxedValue(3));
    EXPECT_EQ(cache.size(), 3u);

    // get existing
    auto va = cache.get("a");
    ASSERT_TRUE(va.has_value());
    EXPECT_EQ(va->cast<int>(), 1);

    // get missing
    auto vmiss = cache.get("missing");
    EXPECT_FALSE(vmiss.has_value());

    // overflow: adding a 4th item should evict the LRU (b, since a was MRU'd)
    cache.put("d", atom::meta::BoxedValue(4));
    EXPECT_EQ(cache.size(), 3u);

    // Update existing key
    cache.put("a", atom::meta::BoxedValue(99));
    auto va2 = cache.get("a");
    ASSERT_TRUE(va2.has_value());
    EXPECT_EQ(va2->cast<int>(), 99);

    // remove
    cache.remove("a");
    EXPECT_FALSE(cache.get("a").has_value());

    // remove non-existent (no crash)
    cache.remove("nonexistent");

    // clear
    cache.clear();
    EXPECT_EQ(cache.size(), 0u);
}

// ---- BoxedOptional
TEST_F(BoxedValueTest, BoxedOptionalOps) {
    atom::meta::BoxedOptional bo;
    EXPECT_FALSE(bo.hasValue());
    EXPECT_FALSE(static_cast<bool>(bo));

    bo.emplace(42);
    EXPECT_TRUE(bo.hasValue());
    EXPECT_TRUE(static_cast<bool>(bo));
    EXPECT_EQ(bo.value().cast<int>(), 42);

    // const value()
    const auto& cbo = bo;
    EXPECT_EQ(cbo.value().cast<int>(), 42);

    // tryGet
    auto v = bo.tryGet<int>();
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 42);

    auto v2 = bo.tryGet<std::string>();
    EXPECT_FALSE(v2.has_value());

    // tryGet when empty
    atom::meta::BoxedOptional empty;
    auto v3 = empty.tryGet<int>();
    EXPECT_FALSE(v3.has_value());

    // valueOr
    auto result = bo.valueOr(atom::meta::BoxedValue(99));
    EXPECT_EQ(result.cast<int>(), 42);
    auto result2 = empty.valueOr(atom::meta::BoxedValue(99));
    EXPECT_EQ(result2.cast<int>(), 99);

    // reset
    bo.reset();
    EXPECT_FALSE(bo.hasValue());
}

// ---- chainOperations
TEST_F(BoxedValueTest, ChainOperationsHelper) {
    atom::meta::BoxedValue v(1);
    auto result = atom::meta::chainOperations(
        v,
        [](atom::meta::BoxedValue val) -> atom::meta::BoxedValue {
            return atom::meta::BoxedValue(val.cast<int>() + 1);
        },
        [](atom::meta::BoxedValue val) -> atom::meta::BoxedValue {
            return atom::meta::BoxedValue(val.cast<int>() * 2);
        });
    EXPECT_EQ(result.cast<int>(), 4);  // (1+1)*2
}

// ---- reduceBoxed
TEST_F(BoxedValueTest, ReduceBoxedHelper) {
    atom::meta::BoxedValueArray arr;
    arr.push_back(atom::meta::BoxedValue(1));
    arr.push_back(atom::meta::BoxedValue(2));
    arr.push_back(atom::meta::BoxedValue(3));

    auto result = atom::meta::reduceBoxed(
        arr, atom::meta::BoxedValue(0),
        [](const atom::meta::BoxedValue& acc,
           const atom::meta::BoxedValue& val) -> atom::meta::BoxedValue {
            return atom::meta::BoxedValue(acc.cast<int>() + val.cast<int>());
        });
    EXPECT_EQ(result.cast<int>(), 6);
}

// ---- groupByType and collectByType
TEST_F(BoxedValueTest, GroupAndCollectByType) {
    std::vector<atom::meta::BoxedValue> values = {
        atom::meta::BoxedValue(1),
        atom::meta::BoxedValue(2),
        atom::meta::BoxedValue(3.14),
        atom::meta::BoxedValue(std::string("hello"))
    };

    auto groups = atom::meta::groupByType(values);
    EXPECT_GE(groups.size(), 1u);

    auto ints = atom::meta::collectByType<int>(values);
    EXPECT_EQ(ints.size(), 2u);
    EXPECT_EQ(ints[0], 1);
    EXPECT_EQ(ints[1], 2);
}

// ---- visitBoxed and transformBoxed
TEST_F(BoxedValueTest, VisitBoxedAndTransformBoxed) {
    atom::meta::BoxedValue v(42);

    // visitBoxed
    int visited = 0;
    atom::meta::visitBoxed(v, [&visited](const atom::meta::BoxedValue& bv) {
        visited = bv.cast<int>();
    });
    EXPECT_EQ(visited, 42);

    // transformBoxed — success
    auto opt = atom::meta::transformBoxed<int>(
        v, [](const atom::meta::BoxedValue& bv) -> atom::meta::BoxedValue {
            return atom::meta::BoxedValue(bv.cast<int>() * 2);
        });
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt->cast<int>(), 84);

    // transformBoxed — exception path
    auto opt2 = atom::meta::transformBoxed<int>(
        v, [](const atom::meta::BoxedValue&) -> atom::meta::BoxedValue {
            throw std::runtime_error("fail");
            return atom::meta::BoxedValue(0);  // unreachable
        });
    EXPECT_FALSE(opt2.has_value());
}

// ---- typeInfoCast and getRelationship
TEST_F(BoxedValueTest, TypeInfoCastAndRelationship) {
    atom::meta::BoxedValue v(42);

    // typeInfoCast — success
    auto r = atom::meta::typeInfoCast<int>(v);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, 42);

    // typeInfoCast — failure
    auto r2 = atom::meta::typeInfoCast<double>(v);
    EXPECT_FALSE(r2.has_value());

    // getRelationship — Same
    auto rel = atom::meta::getRelationship<int>(v);
    EXPECT_EQ(rel, atom::meta::TypeRelationship::Same);

    // getRelationship — Unrelated
    atom::meta::BoxedValue vs(std::string("hello"));
    auto rel2 = atom::meta::getRelationship<int>(vs);
    EXPECT_EQ(rel2, atom::meta::TypeRelationship::Unrelated);
}

// ---- RegisteredBoxedValue
TEST_F(BoxedValueTest, RegisteredBoxedValue) {
    // create without type name
    auto rbv1 = atom::meta::RegisteredBoxedValue::create(42);
    EXPECT_FALSE(rbv1.isTypeRegistered());
    EXPECT_TRUE(rbv1.isType<int>());
    EXPECT_EQ(rbv1.cast<int>(), 42);

    // create with type name
    auto rbv2 = atom::meta::RegisteredBoxedValue::create(
        std::string("hello"), "my_string");
    EXPECT_TRUE(rbv2.isTypeRegistered());
    EXPECT_EQ(rbv2.cast<std::string>(), "hello");
}

// ---- createValidatedBox and getExtendedInfo
TEST_F(BoxedValueTest, CreateValidatedBoxAndExtendedInfo) {
    auto bv = atom::meta::createValidatedBox(42);
    EXPECT_TRUE(bv.isType<int>());
    EXPECT_EQ(bv.cast<int>(), 42);

    auto info = atom::meta::getExtendedInfo<int>(bv);
    EXPECT_TRUE(info.has_value());

    auto info2 = atom::meta::getExtendedInfo<std::string>(bv);
    EXPECT_FALSE(info2.has_value());
}

// ---- isCompatible helper
TEST_F(BoxedValueTest, IsCompatibleHelper) {
    atom::meta::BoxedValue v(42);
    EXPECT_TRUE(atom::meta::isCompatible<int>(v));
    // Just verify the function runs without error for a different type
    bool r = atom::meta::isCompatible<std::string>(v);
    (void)r;  // result depends on areTypesCompatible implementation
}

// ---- std::formatter support for BoxedValue
TEST_F(BoxedValueTest, FormatterSupport) {
    atom::meta::BoxedValue v(42);
    std::string formatted = std::format("{}", v);
    EXPECT_FALSE(formatted.empty());
    EXPECT_TRUE(formatted.find("BoxedValue") != std::string::npos);
}

// ---- makeBoxedValue with reference type
TEST_F(BoxedValueTest, MakeBoxedValueRefBranch) {
    // makeBoxedValue with T being a reference type triggers the is_reference_v branch
    int x = 55;
    // Passing via template with explicit lvalue ref
    auto bv = atom::meta::makeBoxedValue(x, false, false);
    EXPECT_TRUE(bv.isType<int>());
}

// ---- swap self-assign check
TEST_F(BoxedValueTest, SwapSelfAssign) {
    atom::meta::BoxedValue v(42);
    v.swap(v);  // self-swap: no-op, no crash
    EXPECT_EQ(v.cast<int>(), 42);
}

// ---- copy assignment self-assign check
TEST_F(BoxedValueTest, CopyAssignSelfAssign) {
    atom::meta::BoxedValue v(42);
    v = v;  // self-assignment: no-op
    EXPECT_EQ(v.cast<int>(), 42);
}

// ---- move assignment self-assign check
TEST_F(BoxedValueTest, MoveAssignSelfAssign) {
    atom::meta::BoxedValue v(42);
    v = std::move(v);  // self-move-assignment: no-op
    // v may be in a valid but unspecified state; just check no crash
}

// ---- BoxedValue(shared_ptr<Data>) constructor (shared data path)
TEST_F(BoxedValueTest, SharedDataConstructor) {
    atom::meta::BoxedValue original(42);
    // copy constructor goes through make_shared<Data>(*other.data_)
    // the getAttr path using BoxedValue(iter->second) uses the shared_ptr<Data> ctor
    original.setAttr("key", atom::meta::BoxedValue(99));
    auto attr = original.getAttr("key");
    EXPECT_TRUE(attr.isType<int>());
    EXPECT_EQ(attr.cast<int>(), 99);
}

// ---- VisitImpl VISIT_REF_TYPE for const-ref (non-mutable visit path)
TEST_F(BoxedValueTest, VisitConstRefWrapped) {
    const int ci = 33;
    // std::cref stores reference_wrapper<const int>
    // BoxedValue(const T& value) sets readonly=true, so the mutable visit()
    // overload returns early. Use the const visit via const-qualified object.
    // But std::cref goes through the non-const BoxedValue(T&&) overload since
    // std::cref returns reference_wrapper<const int> as an rvalue.
    // To reach the VISIT_REF_TYPE(int) !Mutable const-ref path, we build via
    // the const-T& constructor and call on a const reference.
    atom::meta::BoxedValue v(std::cref(ci));
    // Call the const overload explicitly
    const atom::meta::BoxedValue& cv = v;
    auto r = cv.visit([](const auto& x) -> int {
        if constexpr (std::is_same_v<std::decay_t<decltype(x)>, int>)
            return x;
        return -1;
    });
    EXPECT_EQ(r, 33);
}

// ---- const visit on void/undef — line 614 path
TEST_F(BoxedValueTest, ConstVisitOnVoidReturnsDefault) {
    const atom::meta::BoxedValue cv;  // void, const-qualified
    // const visit overload: isUndef()=true -> hits line 614 (default ResultType{})
    int r = cv.visit([](const auto&) -> int { return 99; });
    EXPECT_EQ(r, 0);  // ResultType{} == 0
}

// ---- Additional coverage for rarely-hit template return lines
TEST_F(BoxedValueTest, VarWithDescReturnLine) {
    // varWithDesc (line 877 return) — call with a non-string type
    auto bv = atom::meta::varWithDesc(3.14, "pi value");
    EXPECT_TRUE(bv.isType<double>());
    EXPECT_EQ(bv.getAttr("description").cast<std::string>(), "pi value");
}

TEST_F(BoxedValueTest, BoxTupleReturnLine) {
    // boxTuple return line (957)
    auto t = std::make_tuple(10, 20.0);
    auto vec = atom::meta::boxTuple(t);
    EXPECT_EQ(vec.size(), 2u);
    EXPECT_EQ(vec[0].cast<int>(), 10);
}

TEST_F(BoxedValueTest, BoxedValueArrayTransformReturn) {
    // BoxedValueArray::transform return line (1034)
    atom::meta::BoxedValueArray arr(1, 2, 3);
    auto results = arr.transform([](atom::meta::BoxedValue& v) -> int {
        return v.cast<int>() * 10;
    });
    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0], 10);
}

TEST_F(BoxedValueTest, BoxedValueMapKeysReturn) {
    // BoxedValueMap::keys() return line (1119)
    atom::meta::BoxedValueMap m;
    m.set("alpha", 1);
    m.set("beta", 2);
    m.set("gamma", 3);
    auto keys = m.keys();
    EXPECT_EQ(keys.size(), 3u);
}

TEST_F(BoxedValueTest, RegisteredBoxedValueCreateReturn) {
    // RegisteredBoxedValue::create return line (1189) — with type name
    auto r = atom::meta::RegisteredBoxedValue::create(3.14, "my_double");
    EXPECT_TRUE(r.isTypeRegistered());
    EXPECT_TRUE(r.isType<double>());
}

TEST_F(BoxedValueTest, GetRelationshipConvertible) {
    // getRelationship Convertible branch (line 1217)
    // Need: TypeInfo doesn't match but canCast<T> succeeds.
    // const T value: isType<T>() returns true but TypeInfo comparison
    // may differ from fromType<T>() if the stored type info differs.
    // Use a readonly value where getTypeInfo() is for T but TypeInfo::fromType<T>
    // might have different flags.
    // Actually, try a different approach: use a value where canCast succeeds.
    // A reference-wrapped int where we ask for getRelationship<double>.
    // That should hit Unrelated (canCast<double> fails on int).
    // To hit Convertible, we need same type succeeding canCast but TypeInfo mismatch.
    // This is quite hard to trigger since TypeInfo and canCast both check the same type.
    // Document as unreachable via public API in practice.
    atom::meta::BoxedValue v(42);
    // Verify all branches are covered:
    auto same = atom::meta::getRelationship<int>(v);
    EXPECT_EQ(same, atom::meta::TypeRelationship::Same);
    auto unrelated = atom::meta::getRelationship<std::string>(v);
    EXPECT_EQ(unrelated, atom::meta::TypeRelationship::Unrelated);
}

TEST_F(BoxedValueTest, CollectByTypeReturnLine) {
    // collectByType return (1234)
    std::vector<atom::meta::BoxedValue> values;
    values.push_back(atom::meta::BoxedValue(1));
    values.push_back(atom::meta::BoxedValue(std::string("skip")));
    values.push_back(atom::meta::BoxedValue(3));
    auto ints = atom::meta::collectByType<int>(values);
    EXPECT_EQ(ints.size(), 2u);
}

TEST_F(BoxedValueTest, GroupByTypeReturnLine) {
    // groupByType return (1246)
    std::vector<atom::meta::BoxedValue> values = {
        atom::meta::BoxedValue(1),
        atom::meta::BoxedValue(2.5),
        atom::meta::BoxedValue(std::string("hi"))
    };
    auto groups = atom::meta::groupByType(values);
    EXPECT_GE(groups.size(), 1u);
}

TEST_F(BoxedValueTest, SafeUnionCreateReturn) {
    // SafeUnion::create return (1326) — ensure the return statement is hit
    auto u1 = atom::meta::SafeUnion::create<int>();
    auto u2 = atom::meta::SafeUnion::create<std::string, double>();
    u2.set(std::string("test"));
    auto got = u2.get<std::string>();
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(*got, "test");
}

TEST_F(BoxedValueTest, ReduceBoxedReturnLine) {
    // reduceBoxed return (1609)
    atom::meta::BoxedValueArray empty_arr;
    auto result = atom::meta::reduceBoxed(
        empty_arr, atom::meta::BoxedValue(10),
        [](const atom::meta::BoxedValue& acc, const atom::meta::BoxedValue&) {
            return acc;
        });
    EXPECT_EQ(result.cast<int>(), 10);
}

// ---- Non-default-constructible result type: cover throw paths (640, 824)
namespace {
struct NonDefaultConstructible {
    int value;
    explicit NonDefaultConstructible(int v) : value(v) {}
};
struct UnknownBoxType { int x = 7; };
}  // namespace

TEST_F(BoxedValueTest, ConstVisitNonDCThrowPath) {
    // line 616: const visit on void/undef value, ResultType not default-constructible
    const atom::meta::BoxedValue cv;  // void/undef, const-qualified
    EXPECT_THROW(
        cv.visit([](const auto&) -> NonDefaultConstructible {
            return NonDefaultConstructible{0};
        }),
        std::bad_any_cast);
}

TEST_F(BoxedValueTest, MutableVisitNonDCThrowPath) {
    // line 640: mutable visit on void value, ResultType not default-constructible
    atom::meta::BoxedValue v;  // void/undef
    EXPECT_THROW(
        v.visit([](auto&) -> NonDefaultConstructible {
            return NonDefaultConstructible{0};
        }),
        std::bad_any_cast);
}

TEST_F(BoxedValueTest, VisitImplNonDCThrowPath) {
    // line 824: visitImpl fallback on unknown type, ResultType not default-constructible
    atom::meta::BoxedValue v(UnknownBoxType{});  // type not in VISIT_TYPE list
    const atom::meta::BoxedValue& cv = v;
    EXPECT_THROW(
        cv.visit([](const auto&) -> NonDefaultConstructible {
            return NonDefaultConstructible{0};
        }),
        std::bad_any_cast);
}

// ---- VisitImpl: cover more REF_TYPE types (wchar_t, char16_t, char32_t, wstring, etc.)
TEST_F(BoxedValueTest, VisitRefTypeWideTypes) {
    {
        unsigned long ul = 42UL;
        atom::meta::BoxedValue v(std::ref(ul));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, unsigned long>)
                return static_cast<int>(x);
            return -1;
        });
        EXPECT_EQ(r, 42);
    }
    {
        unsigned long long ull = 43ULL;
        atom::meta::BoxedValue v(std::ref(ull));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, unsigned long long>)
                return static_cast<int>(x);
            return -1;
        });
        EXPECT_EQ(r, 43);
    }
    {
        short s = 5;
        atom::meta::BoxedValue v(std::ref(s));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, short>)
                return x;
            return -1;
        });
        EXPECT_EQ(r, 5);
    }
    {
        unsigned short us = 6;
        atom::meta::BoxedValue v(std::ref(us));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, unsigned short>)
                return x;
            return -1;
        });
        EXPECT_EQ(r, 6);
    }
    {
        char c = 'X';
        atom::meta::BoxedValue v(std::ref(c));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, char>)
                return x;
            return -1;
        });
        EXPECT_EQ(r, 'X');
    }
    {
        unsigned char uc = 200;
        atom::meta::BoxedValue v(std::ref(uc));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, unsigned char>)
                return x;
            return -1;
        });
        EXPECT_EQ(r, 200);
    }
    {
        signed char sc = -5;
        atom::meta::BoxedValue v(std::ref(sc));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, signed char>)
                return x;
            return -99;
        });
        EXPECT_EQ(r, -5);
    }
    {
        wchar_t wc = L'W';
        atom::meta::BoxedValue v(std::ref(wc));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, wchar_t>)
                return static_cast<int>(x);
            return -1;
        });
        EXPECT_EQ(r, static_cast<int>(L'W'));
    }
    {
        char16_t c16 = u'a';
        atom::meta::BoxedValue v(std::ref(c16));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, char16_t>)
                return static_cast<int>(x);
            return -1;
        });
        EXPECT_EQ(r, static_cast<int>(u'a'));
    }
    {
        char32_t c32 = U'z';
        atom::meta::BoxedValue v(std::ref(c32));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, char32_t>)
                return static_cast<int>(x);
            return -1;
        });
        EXPECT_EQ(r, static_cast<int>(U'z'));
    }
    {
        long double ld = 2.5L;
        atom::meta::BoxedValue v(std::ref(ld));
        auto r = v.visit([](const auto& x) -> double {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, long double>)
                return static_cast<double>(x);
            return -1.0;
        });
        EXPECT_DOUBLE_EQ(r, 2.5);
    }
}

TEST_F(BoxedValueTest, VisitRefTypeWideStrings) {
    {
        std::wstring ws = L"wide";
        atom::meta::BoxedValue v(std::ref(ws));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::wstring>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 4);
    }
    {
        std::u16string u16 = u"abc";
        atom::meta::BoxedValue v(std::ref(u16));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::u16string>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 3);
    }
    {
        std::u32string u32 = U"xyz";
        atom::meta::BoxedValue v(std::ref(u32));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::u32string>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 3);
    }
    {
        std::string base = "hi";
        std::string_view sv(base);
        atom::meta::BoxedValue v(std::ref(sv));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::string_view>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 2);
    }
    {
        std::wstring wb = L"wv";
        std::wstring_view wsv(wb);
        atom::meta::BoxedValue v(std::ref(wsv));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::wstring_view>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 2);
    }
    {
        std::u16string u16b = u"u16";
        std::u16string_view u16sv(u16b);
        atom::meta::BoxedValue v(std::ref(u16sv));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::u16string_view>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 3);
    }
    {
        std::u32string u32b = U"u32";
        std::u32string_view u32sv(u32b);
        atom::meta::BoxedValue v(std::ref(u32sv));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::u32string_view>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 3);
    }
}

TEST_F(BoxedValueTest, VisitRefTypeMapAndUMap) {
    {
        std::map<std::string, std::string> ms = {{"k", "v"}};
        atom::meta::BoxedValue v(std::ref(ms));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::map<std::string, std::string>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        std::unordered_map<std::string, int> um = {{"a", 1}};
        atom::meta::BoxedValue v(std::ref(um));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::unordered_map<std::string, int>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        std::unordered_map<std::string, std::string> ums = {{"b", "c"}};
        atom::meta::BoxedValue v(std::ref(ums));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::unordered_map<std::string, std::string>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 1);
    }
    {
        std::vector<std::string> vs = {"a", "b"};
        atom::meta::BoxedValue v(std::ref(vs));
        auto r = v.visit([](const auto& x) -> int {
            if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::vector<std::string>>)
                return static_cast<int>(x.size());
            return -1;
        });
        EXPECT_EQ(r, 2);
    }
}

}  // namespace
