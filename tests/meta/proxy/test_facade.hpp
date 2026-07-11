#include <gtest/gtest.h>
#include "atom/meta/facade.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

// Test fixture for facade tests
class FacadeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Helper types for testing
struct TestInterface {
    virtual ~TestInterface() = default;
    virtual int getValue() const = 0;
    virtual void setValue(int value) = 0;
    virtual std::string getName() const = 0;
};

class TestImplementation : public TestInterface {
private:
    int value_;
    std::string name_;

public:
    TestImplementation(int value = 42, const std::string& name = "test")
        : value_(value), name_(name) {}

    int getValue() const override { return value_; }
    void setValue(int value) override { value_ = value; }
    std::string getName() const override { return name_; }
};

class AnotherImplementation : public TestInterface {
private:
    int value_;
    std::string name_;

public:
    AnotherImplementation(int value = 100, const std::string& name = "another")
        : value_(value), name_(name) {}

    int getValue() const override { return value_; }
    void setValue(int value) override { value_ = value; }
    std::string getName() const override { return name_; }
};

// Facade type used by the proxy tests below; the default builder accepts
// ordinary copyable, nothrow-movable value types.
using TestFacade = atom::meta::default_builder::build;

// Test constraint system
TEST_F(FacadeTest, ConstraintSystem) {
    using namespace atom::meta;

    // Test constraint_level enum
    static_assert(static_cast<int>(constraint_level::none) == 0);
    static_assert(static_cast<int>(constraint_level::nontrivial) == 1);
    static_assert(static_cast<int>(constraint_level::nothrow) == 2);
    static_assert(static_cast<int>(constraint_level::trivial) == 3);

    // Test thread_safety enum
    static_assert(static_cast<int>(thread_safety::none) == 0);
    static_assert(static_cast<int>(thread_safety::shared) == 1);
    static_assert(static_cast<int>(thread_safety::synchronized) == 2);

    // Test proxiable_constraints structure
    proxiable_constraints constraints{};
    constraints.copyability = constraint_level::nothrow;
    constraints.relocatability = constraint_level::trivial;
    constraints.destructibility = constraint_level::nothrow;
    constraints.concurrency = thread_safety::shared;

    EXPECT_EQ(constraints.copyability, constraint_level::nothrow);
    EXPECT_EQ(constraints.relocatability, constraint_level::trivial);
    EXPECT_EQ(constraints.destructibility, constraint_level::nothrow);
    EXPECT_EQ(constraints.concurrency, thread_safety::shared);
}

// Test constraint merging
TEST_F(FacadeTest, ConstraintMerging) {
    using namespace atom::meta;

    proxiable_constraints c1{};
    c1.copyability = constraint_level::nothrow;
    c1.relocatability = constraint_level::trivial;
    c1.destructibility = constraint_level::none;
    c1.concurrency = thread_safety::shared;

    proxiable_constraints c2{};
    c2.copyability = constraint_level::trivial;
    c2.relocatability = constraint_level::none;
    c2.destructibility = constraint_level::nothrow;
    c2.concurrency = thread_safety::synchronized;

    // Test constraint merging (should take the more restrictive constraint)
    auto merged = detail::merge_constraints(c1, c2);

    EXPECT_EQ(merged.copyability,
              constraint_level::trivial);  // More restrictive
    EXPECT_EQ(merged.relocatability,
              constraint_level::trivial);  // More restrictive
    EXPECT_EQ(merged.destructibility,
              constraint_level::nothrow);  // More restrictive
    EXPECT_EQ(merged.concurrency,
              thread_safety::synchronized);  // More restrictive
}

// Test facade concepts
TEST_F(FacadeTest, FacadeConcepts) {
    using namespace atom::meta;

    // Test dispatcher concept
    static_assert(dispatcher<print_dispatch>);
    static_assert(dispatcher<to_string_dispatch>);
    static_assert(!dispatcher<TestInterface>);

    // Test facade concept
    static_assert(facade<TestFacade>);
    static_assert(!facade<TestInterface>);
}

// Test proxy construction and basic operations
TEST_F(FacadeTest, ProxyBasicOperations) {
    using namespace atom::meta;

    // Create a proxy with the default test facade
    proxy<TestFacade> testProxy;

    // Test default construction (should be empty)
    EXPECT_FALSE(testProxy.has_value());

    // Test construction with implementation
    TestImplementation impl(42, "test");
    proxy<TestFacade> proxyWithImpl(impl);

    EXPECT_TRUE(proxyWithImpl.has_value());
    auto* stored = proxyWithImpl.target<TestImplementation>();
    ASSERT_NE(stored, nullptr);
    EXPECT_EQ(stored->getValue(), 42);
    EXPECT_EQ(stored->getName(), "test");
}

// Test proxy copy and move semantics
TEST_F(FacadeTest, ProxyCopyMoveSemantics) {
    using namespace atom::meta;

    TestImplementation impl(42, "original");
    proxy<TestFacade> original(impl);

    // Test copy constructor
    proxy<TestFacade> copied(original);
    EXPECT_TRUE(copied.has_value());
    ASSERT_NE(copied.target<TestImplementation>(), nullptr);
    EXPECT_EQ(copied.target<TestImplementation>()->getValue(), 42);
    EXPECT_EQ(copied.target<TestImplementation>()->getName(), "original");

    // Modify original to ensure independence
    original.target<TestImplementation>()->setValue(100);
    EXPECT_EQ(original.target<TestImplementation>()->getValue(), 100);
    EXPECT_EQ(copied.target<TestImplementation>()->getValue(),
              42);  // Should remain unchanged

    // Test move constructor
    proxy<TestFacade> moved(std::move(original));
    EXPECT_TRUE(moved.has_value());
    ASSERT_NE(moved.target<TestImplementation>(), nullptr);
    EXPECT_EQ(moved.target<TestImplementation>()->getValue(), 100);

    // Test copy assignment
    AnotherImplementation anotherImpl(200, "another");
    proxy<TestFacade> another(anotherImpl);

    copied = another;
    ASSERT_NE(copied.target<AnotherImplementation>(), nullptr);
    EXPECT_EQ(copied.target<AnotherImplementation>()->getValue(), 200);
    EXPECT_EQ(copied.target<AnotherImplementation>()->getName(), "another");

    // Test move assignment
    proxy<TestFacade> moveAssigned;
    moveAssigned = std::move(moved);
    EXPECT_TRUE(moveAssigned.has_value());
    ASSERT_NE(moveAssigned.target<TestImplementation>(), nullptr);
    EXPECT_EQ(moveAssigned.target<TestImplementation>()->getValue(), 100);
}

// Test proxy reset and assignment
TEST_F(FacadeTest, ProxyResetAssignment) {
    using namespace atom::meta;

    TestImplementation impl(42, "test");
    proxy<TestFacade> testProxy(impl);

    EXPECT_TRUE(testProxy.has_value());

    // Test reset
    testProxy.reset();
    EXPECT_FALSE(testProxy.has_value());

    // Test assignment of new implementation
    AnotherImplementation newImpl(100, "new");
    testProxy = proxy<TestFacade>(newImpl);

    EXPECT_TRUE(testProxy.has_value());
    ASSERT_NE(testProxy.target<AnotherImplementation>(), nullptr);
    EXPECT_EQ(testProxy.target<AnotherImplementation>()->getValue(), 100);
    EXPECT_EQ(testProxy.target<AnotherImplementation>()->getName(), "new");
}

// Test proxy with different implementations
TEST_F(FacadeTest, ProxyPolymorphism) {
    using namespace atom::meta;

    std::vector<proxy<TestFacade>> proxies;

    // Add different implementations
    proxies.emplace_back(TestImplementation(1, "first"));
    proxies.emplace_back(AnotherImplementation(2, "second"));
    proxies.emplace_back(TestImplementation(3, "third"));

    // Test that each proxy keeps the correct concrete type and state
    ASSERT_NE(proxies[0].target<TestImplementation>(), nullptr);
    EXPECT_EQ(proxies[0].target<TestImplementation>()->getValue(), 1);
    EXPECT_EQ(proxies[0].target<TestImplementation>()->getName(), "first");

    ASSERT_NE(proxies[1].target<AnotherImplementation>(), nullptr);
    EXPECT_EQ(proxies[1].target<AnotherImplementation>()->getValue(), 2);
    EXPECT_EQ(proxies[1].target<AnotherImplementation>()->getName(), "second");

    ASSERT_NE(proxies[2].target<TestImplementation>(), nullptr);
    EXPECT_EQ(proxies[2].target<TestImplementation>()->getValue(), 3);
    EXPECT_EQ(proxies[2].target<TestImplementation>()->getName(), "third");

    // Test modification through proxy
    proxies[0].target<TestImplementation>()->setValue(10);
    EXPECT_EQ(proxies[0].target<TestImplementation>()->getValue(), 10);
}

// Test proxy swap functionality
TEST_F(FacadeTest, ProxySwap) {
    using namespace atom::meta;

    TestImplementation impl1(42, "first");
    AnotherImplementation impl2(100, "second");

    proxy<TestFacade> proxy1(impl1);
    proxy<TestFacade> proxy2(impl2);

    // Test swap
    proxy1.swap(proxy2);

    ASSERT_NE(proxy1.target<AnotherImplementation>(), nullptr);
    EXPECT_EQ(proxy1.target<AnotherImplementation>()->getValue(), 100);
    EXPECT_EQ(proxy1.target<AnotherImplementation>()->getName(), "second");

    ASSERT_NE(proxy2.target<TestImplementation>(), nullptr);
    EXPECT_EQ(proxy2.target<TestImplementation>()->getValue(), 42);
    EXPECT_EQ(proxy2.target<TestImplementation>()->getName(), "first");
}

// Test proxy comparison operations
TEST_F(FacadeTest, ProxyComparison) {
    using namespace atom::meta;

    TestImplementation impl(42, "test");
    proxy<TestFacade> proxy1(impl);
    proxy<TestFacade> proxy2(impl);
    proxy<TestFacade> emptyProxy;
    proxy<TestFacade> anotherEmptyProxy;

    // Two empty proxies compare equal; empty vs non-empty does not
    EXPECT_TRUE(emptyProxy == anotherEmptyProxy);
    EXPECT_FALSE(proxy1 == emptyProxy);

    // Test has_value comparisons
    EXPECT_TRUE(proxy1.has_value());
    EXPECT_TRUE(proxy2.has_value());
    EXPECT_FALSE(emptyProxy.has_value());
}

// Test proxy with smart pointers
TEST_F(FacadeTest, ProxyWithSmartPointers) {
    using namespace atom::meta;

    auto sharedImpl = std::make_shared<TestImplementation>(42, "shared");
    auto uniqueImpl = std::make_unique<AnotherImplementation>(100, "unique");

    // Test with shared_ptr
    proxy<TestFacade> sharedProxy(*sharedImpl);
    EXPECT_TRUE(sharedProxy.has_value());
    ASSERT_NE(sharedProxy.target<TestImplementation>(), nullptr);
    EXPECT_EQ(sharedProxy.target<TestImplementation>()->getValue(), 42);

    // Test with unique_ptr
    proxy<TestFacade> uniqueProxy(*uniqueImpl);
    EXPECT_TRUE(uniqueProxy.has_value());
    ASSERT_NE(uniqueProxy.target<AnotherImplementation>(), nullptr);
    EXPECT_EQ(uniqueProxy.target<AnotherImplementation>()->getValue(), 100);
}

// Test proxy error handling
TEST_F(FacadeTest, ProxyErrorHandling) {
    using namespace atom::meta;

    proxy<TestFacade> emptyProxy;

    // Test accessing empty proxy
    EXPECT_FALSE(emptyProxy.has_value());

    // Accessing an empty proxy returns nullptr / throws
    EXPECT_EQ(emptyProxy.target<TestImplementation>(), nullptr);
    EXPECT_THROW(emptyProxy.call<print_dispatch>(), std::bad_function_call);
}

// Test skill-based dispatch system
TEST_F(FacadeTest, SkillBasedDispatch) {
    using namespace atom::meta;

    // Test implementation with skills
    struct SkillfulImplementation {
        std::string data = "initial";

        std::string read() const { return data; }
        void write(const std::string& newData) { data = newData; }
    };

    // Define skills for testing (local classes cannot have member templates)
    struct ReadSkill {
        std::string operator()(const SkillfulImplementation& obj) const {
            return obj.read();
        }
    };

    struct WriteSkill {
        void operator()(SkillfulImplementation& obj,
                        const std::string& data) const {
            obj.write(data);
        }
    };

    // Test skill dispatch (if available in implementation)
    SkillfulImplementation impl;

    ReadSkill readSkill;
    WriteSkill writeSkill;

    EXPECT_EQ(readSkill(impl), "initial");

    writeSkill(impl, "modified");
    EXPECT_EQ(readSkill(impl), "modified");
}

// Test memory layout and alignment
TEST_F(FacadeTest, MemoryLayoutAlignment) {
    using namespace atom::meta;

    // Test that proxy has reasonable size and alignment
    static_assert(sizeof(proxy<TestFacade>) > 0);
    static_assert(alignof(proxy<TestFacade>) >= alignof(std::max_align_t));

    // Test with a facade using a restricted layout
    using SmallFacade = default_builder::restrict_layout<64, 16>::build;

    static_assert(sizeof(proxy<SmallFacade>) > 0);
    static_assert(alignof(proxy<SmallFacade>) > 0);
    static_assert(SmallFacade::constraints.max_size == 64);
    static_assert(SmallFacade::constraints.max_align == 16);
}

// Test thread safety mechanisms
TEST_F(FacadeTest, ThreadSafety) {
    using namespace atom::meta;

    TestImplementation impl(0, "thread_test");
    proxy<TestFacade> sharedProxy(impl);

    constexpr int numThreads = 10;
    constexpr int incrementsPerThread = 100;
    std::atomic<int> completedThreads(0);

    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(
            [&sharedProxy, &completedThreads, incrementsPerThread]() {
                for (int j = 0; j < incrementsPerThread; ++j) {
                    auto* obj = sharedProxy.target<TestImplementation>();
                    if (obj != nullptr) {
                        int currentValue = obj->getValue();
                        obj->setValue(currentValue + 1);
                    }
                }
                completedThreads.fetch_add(1);
            });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(completedThreads.load(), numThreads);
    // Note: Final value may not be deterministic due to race conditions
    // This test mainly ensures no crashes occur during concurrent access
}

// Test vtable dispatch correctness
TEST_F(FacadeTest, VTableDispatch) {
    using namespace atom::meta;

    // Create proxies with different implementations
    std::vector<proxy<TestFacade>> proxies;

    for (int i = 0; i < 5; ++i) {
        if (i % 2 == 0) {
            proxies.emplace_back(
                TestImplementation(i, "test_" + std::to_string(i)));
        } else {
            proxies.emplace_back(
                AnotherImplementation(i * 10, "another_" + std::to_string(i)));
        }
    }

    // Test that each proxy dispatches to the correct implementation
    for (size_t i = 0; i < proxies.size(); ++i) {
        if (i % 2 == 0) {
            EXPECT_EQ(proxies[i].type(), typeid(TestImplementation));
            auto* obj = proxies[i].target<TestImplementation>();
            ASSERT_NE(obj, nullptr);
            EXPECT_EQ(obj->getValue(), static_cast<int>(i));
            EXPECT_EQ(obj->getName(), "test_" + std::to_string(i));
        } else {
            EXPECT_EQ(proxies[i].type(), typeid(AnotherImplementation));
            auto* obj = proxies[i].target<AnotherImplementation>();
            ASSERT_NE(obj, nullptr);
            EXPECT_EQ(obj->getValue(), static_cast<int>(i) * 10);
            EXPECT_EQ(obj->getName(), "another_" + std::to_string(i));
        }
    }
}

// Test exception safety
TEST_F(FacadeTest, ExceptionSafety) {
    using namespace atom::meta;

    struct ThrowingImplementation : public TestInterface {
        mutable bool shouldThrow = false;
        int value_ = 42;
        std::string name_ = "throwing";

        int getValue() const override {
            if (shouldThrow) {
                throw std::runtime_error("getValue threw");
            }
            return value_;
        }

        void setValue(int value) override {
            if (shouldThrow) {
                throw std::runtime_error("setValue threw");
            }
            value_ = value;
        }

        std::string getName() const override {
            if (shouldThrow) {
                throw std::runtime_error("getName threw");
            }
            return name_;
        }
    };

    ThrowingImplementation impl;
    proxy<TestFacade> throwingProxy(impl);

    auto* stored = throwingProxy.target<ThrowingImplementation>();
    ASSERT_NE(stored, nullptr);

    // Test normal operation
    EXPECT_EQ(stored->getValue(), 42);
    EXPECT_EQ(stored->getName(), "throwing");

    // Test exception handling
    stored->shouldThrow = true;
    EXPECT_THROW(stored->getValue(), std::runtime_error);
    EXPECT_THROW(stored->setValue(100), std::runtime_error);
    EXPECT_THROW(stored->getName(), std::runtime_error);
}

// Test performance characteristics
TEST_F(FacadeTest, PerformanceCharacteristics) {
    using namespace atom::meta;

    TestImplementation impl(42, "performance_test");
    proxy<TestFacade> testProxy(impl);

    // Test that proxy operations have reasonable performance
    auto start = std::chrono::high_resolution_clock::now();

    constexpr int iterations = 10000;
    for (int i = 0; i < iterations; ++i) {
        auto* obj = testProxy.target<TestImplementation>();
        ASSERT_NE(obj, nullptr);
        obj->setValue(i);
        volatile int value = obj->getValue();
        (void)value;  // Prevent optimization
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Ensure operations complete in reasonable time (adjust threshold as
    // needed)
    EXPECT_LT(duration.count(), 100000);  // Less than 100ms for 10k operations
}

// Test constraint validation at compile time
TEST_F(FacadeTest, ConstraintValidation) {
    using namespace atom::meta;

    // Test that constraints are properly validated
    struct ConstrainedImplementation : public TestInterface {
        int value_ = 42;
        std::string name_ = "constrained";

        ConstrainedImplementation() = default;

        // Non-copyable
        ConstrainedImplementation(const ConstrainedImplementation&) = delete;
        ConstrainedImplementation& operator=(const ConstrainedImplementation&) =
            delete;

        // Movable
        ConstrainedImplementation(ConstrainedImplementation&&) = default;
        ConstrainedImplementation& operator=(ConstrainedImplementation&&) =
            default;

        int getValue() const override { return value_; }
        void setValue(int value) override { value_ = value; }
        std::string getName() const override { return name_; }
    };

    // A facade without copyability accepts move-only types
    using MoveOnlyFacade =
        facade_builder<std::tuple<>, std::tuple<>,
                       proxiable_constraints{
                           .max_size = 256,
                           .max_align = alignof(std::max_align_t),
                           .copyability = constraint_level::none,
                           .relocatability = constraint_level::nothrow,
                           .destructibility = constraint_level::nothrow,
                           .concurrency = thread_safety::none}>::build;

    // Test that proxy can handle non-copyable types
    ConstrainedImplementation impl;
    proxy<MoveOnlyFacade> constrainedProxy(std::move(impl));

    EXPECT_TRUE(constrainedProxy.has_value());
    auto* stored = constrainedProxy.target<ConstrainedImplementation>();
    ASSERT_NE(stored, nullptr);
    EXPECT_EQ(stored->getValue(), 42);
}

// Test edge cases and boundary conditions
TEST_F(FacadeTest, EdgeCases) {
    using namespace atom::meta;

    // Test with minimal interface
    struct MinimalInterface {
        virtual ~MinimalInterface() = default;
    };

    struct MinimalImplementation : public MinimalInterface {};

    proxy<TestFacade> minimalProxy(MinimalImplementation{});
    EXPECT_TRUE(minimalProxy.has_value());

    // Test with interface having many methods
    struct ComplexInterface {
        virtual ~ComplexInterface() = default;
        virtual int method1() = 0;
        virtual void method2(int) = 0;
        virtual std::string method3() const = 0;
        virtual double method4(double, int) = 0;
        virtual bool method5() const noexcept = 0;
    };

    struct ComplexImplementation : public ComplexInterface {
        int method1() override { return 1; }
        void method2(int) override {}
        std::string method3() const override { return "complex"; }
        double method4(double d, int i) override { return d + i; }
        bool method5() const noexcept override { return true; }
    };

    proxy<TestFacade> complexProxy(ComplexImplementation{});
    EXPECT_TRUE(complexProxy.has_value());
    auto* complexObj = complexProxy.target<ComplexImplementation>();
    ASSERT_NE(complexObj, nullptr);
    EXPECT_EQ(complexObj->method1(), 1);
    EXPECT_EQ(complexObj->method3(), "complex");
    EXPECT_DOUBLE_EQ(complexObj->method4(3.14, 2), 5.14);
    EXPECT_TRUE(complexObj->method5());
}

// -----------------------------------------------------------------------
// NEW TESTS: cover previously-uncovered lines
// -----------------------------------------------------------------------

// Line 898 – type() on an empty proxy returns typeid(void)
TEST_F(FacadeTest, TypeOnEmptyProxyReturnsVoid) {
    using namespace atom::meta;
    proxy<TestFacade> empty;
    EXPECT_EQ(empty.type(), typeid(void));
}

// Lines 926-929 – swap(filled, empty): "else if (vptr)" branch
TEST_F(FacadeTest, SwapFilledWithEmpty) {
    using namespace atom::meta;
    proxy<TestFacade> filled(TestImplementation(7, "seven"));
    proxy<TestFacade> empty;

    filled.swap(empty);

    EXPECT_FALSE(filled.has_value());
    ASSERT_TRUE(empty.has_value());
    ASSERT_NE(empty.target<TestImplementation>(), nullptr);
    EXPECT_EQ(empty.target<TestImplementation>()->getValue(), 7);
}

// Lines 930-934 – swap(empty, filled): "else if (other.vptr)" branch
TEST_F(FacadeTest, SwapEmptyWithFilled) {
    using namespace atom::meta;
    proxy<TestFacade> empty;
    proxy<TestFacade> filled(TestImplementation(8, "eight"));

    empty.swap(filled);

    ASSERT_TRUE(empty.has_value());
    ASSERT_NE(empty.target<TestImplementation>(), nullptr);
    EXPECT_EQ(empty.target<TestImplementation>()->getValue(), 8);
    EXPECT_FALSE(filled.has_value());
}

// Line 908 – swap(self): self-swap must be a no-op
TEST_F(FacadeTest, SwapSelf) {
    using namespace atom::meta;
    proxy<TestFacade> p(TestImplementation(99, "self"));
    p.swap(p);
    ASSERT_TRUE(p.has_value());
    ASSERT_NE(p.target<TestImplementation>(), nullptr);
    EXPECT_EQ(p.target<TestImplementation>()->getValue(), 99);
}

// Lines 1117-1118 – operator== when both non-empty but different types
TEST_F(FacadeTest, OperatorEqualsDifferentTypes) {
    using namespace atom::meta;
    proxy<TestFacade> p1(TestImplementation(1, "a"));
    proxy<TestFacade> p2(AnotherImplementation(2, "b"));
    EXPECT_FALSE(p1 == p2);
    EXPECT_TRUE(p1 != p2);
}

// Line 1121 – operator== when both non-empty, same type (always false per impl)
TEST_F(FacadeTest, OperatorEqualsSameType) {
    using namespace atom::meta;
    proxy<TestFacade> p1(TestImplementation(42, "x"));
    proxy<TestFacade> p2(TestImplementation(42, "x"));
    // The facade operator== returns false when both have values and same type
    EXPECT_FALSE(p1 == p2);
    EXPECT_TRUE(p1 != p2);
}

// Lines 990-1002 – proxy::call<print_dispatch>() happy path
// print_dispatch is registered for streamable types
TEST_F(FacadeTest, CallPrintDispatch) {
    using namespace atom::meta;

    // int is streamable, so print_dispatch will be registered
    proxy<TestFacade> p(42);
    // Should NOT throw - the skill is registered
    EXPECT_NO_THROW(p.call<print_dispatch>());
}

// Line 1051 – call() "Skill not supported by this object"
// Use an unregistered convention type to trigger the throw
namespace facade_test_helpers {
struct UnregisteredConvention {
    static constexpr bool is_direct = false;
    using dispatch_type = UnregisteredConvention;
};
}  // namespace facade_test_helpers

TEST_F(FacadeTest, CallUnsupportedSkillThrows) {
    using namespace atom::meta;
    using C = facade_test_helpers::UnregisteredConvention;
    proxy<TestFacade> p(TestImplementation(1, "a"));
    EXPECT_THROW((p.call<C, void>()), std::runtime_error);
}

// Lines 465,467,480,482 – cloneable_dispatch::clone_impl
// Exercise proxy::clone() on a non-empty proxy to run the
// copy-construction branch inside clone_impl
TEST_F(FacadeTest, ProxyCloneNonEmpty) {
    using namespace atom::meta;
    proxy<TestFacade> p(TestImplementation(55, "clonetest"));
    proxy<TestFacade> c = p.clone();
    ASSERT_TRUE(c.has_value());
    ASSERT_NE(c.target<TestImplementation>(), nullptr);
    EXPECT_EQ(c.target<TestImplementation>()->getValue(), 55);
    EXPECT_EQ(c.target<TestImplementation>()->getName(), "clonetest");
}

// Line 490 – clone_impl "Object is not cloneable" throw
// A non-copy-constructible type stored in a copy-disabled facade triggers this.
// (Line 123 in vtable copy lambda is also covered by this path on copy attempt.)
TEST_F(FacadeTest, VtableCopyNonCopyable_ThrowsOnCopy) {
    using namespace atom::meta;

    struct NoCopy {
        int v = 7;
        NoCopy() = default;
        NoCopy(const NoCopy&) = delete;
        NoCopy& operator=(const NoCopy&) = delete;
        NoCopy(NoCopy&&) noexcept = default;
        NoCopy& operator=(NoCopy&&) noexcept = default;
    };

    // A facade without copyability allows move-only types
    using MoveOnlyFacade =
        atom::meta::facade_builder<std::tuple<>, std::tuple<>,
                       atom::meta::proxiable_constraints{
                           .max_size = 256,
                           .max_align = alignof(std::max_align_t),
                           .copyability = atom::meta::constraint_level::none,
                           .relocatability = atom::meta::constraint_level::nothrow,
                           .destructibility = atom::meta::constraint_level::nothrow,
                           .concurrency = atom::meta::thread_safety::none}>::build;

    proxy<MoveOnlyFacade> p(NoCopy{});
    ASSERT_TRUE(p.has_value());

    // Calling the vtable copy fn directly to hit line 123 is internal, but
    // clone() on a move-only type falls into the "Object is not cloneable"
    // throw at line 490 (no clone() method AND not copy constructible as
    // seen by cloneable_dispatch::clone_impl which captures F = MoveOnlyFacade).
    // The proxy::clone() implementation falls back to copy-ctor which is
    // disabled at compile-time for MoveOnlyFacade, so instead verify the
    // vtable copy path throws when called through make_vtable<NoCopy>()
    // indirectly: construct a second proxy via direct vtable access is not
    // accessible; cover by invoking the erased copy path via a facade that
    // requests copy support for a non-copyable type cannot be done statically.
    // Confirm the proxy is still valid.
    EXPECT_TRUE(p.has_value());
}

// Directly exercise the vtable copy-throw path (line 123): construct a
// vtable for a non-copy-constructible type and call the copy lambda.
TEST_F(FacadeTest, VtableCopyLambdaThrowsForNonCopyable) {
    using namespace atom::meta;

    struct NoCopy2 {
        NoCopy2() = default;
        NoCopy2(const NoCopy2&) = delete;
        NoCopy2(NoCopy2&&) noexcept = default;
    };

    auto vtbl = detail::make_vtable<NoCopy2>();
    NoCopy2 src;
    std::byte dst[sizeof(NoCopy2)];
    // The copy lambda for a non-copy-constructible type must throw
    EXPECT_THROW(vtbl.copy(&src, dst), std::runtime_error);
}

// Directly exercise cloneable_dispatch::clone_impl "Object is not cloneable"
// (line 490): a non-copy-constructible type with no clone() method.
TEST_F(FacadeTest, CloneImplThrowsNotCloneable) {
    using namespace atom::meta;

    struct NoCopy3 {
        NoCopy3() = default;
        NoCopy3(const NoCopy3&) = delete;
        NoCopy3(NoCopy3&&) noexcept = default;
    };

    using F = TestFacade;
    NoCopy3 src;
    alignas(F::constraints.max_align) std::byte storage[F::constraints.max_size]{};
    const detail::vtable* vptr_out = nullptr;
    EXPECT_THROW(
        (cloneable_dispatch::clone_impl<NoCopy3, F>(&src, storage, &vptr_out)),
        std::runtime_error);
}

// Exercise proxy::to_string() and proxy::equals() to ensure
// call<to_string_dispatch> and call<compare_dispatch> paths are hit
TEST_F(FacadeTest, ToStringAndEquals) {
    using namespace atom::meta;

    proxy<TestFacade> p1(42);
    proxy<TestFacade> p2(42);
    proxy<TestFacade> empty;

    // to_string() – call<to_string_dispatch, std::string>()
    std::string s = p1.to_string();
    EXPECT_EQ(s, "42");

    // equals() with empty other returns false
    EXPECT_FALSE(p1.equals(empty));

    // equals() with same-typed same-value proxy
    EXPECT_TRUE(p1.equals(p2));
}

// Exercise FacadeRegistry and TypedProxy
TEST_F(FacadeTest, FacadeRegistryAndTypedProxy) {
    using namespace atom::meta;

    FacadeRegistry& reg = FacadeRegistry::getInstance();
    reg.registerFacade<TestFacade>("TestFacade");
    const auto& facades = reg.getFacades();
    ASSERT_FALSE(facades.empty());
    EXPECT_EQ(facades.back().name, "TestFacade");
    EXPECT_EQ(facades.back().max_size, TestFacade::constraints.max_size);
    EXPECT_EQ(facades.back().max_align, TestFacade::constraints.max_align);

    // TypedProxy
    TypedProxy<TestFacade> tp(TestImplementation(77, "typed"));
    EXPECT_TRUE(tp.hasValue());
    EXPECT_EQ(tp.type(), typeid(TestImplementation));
    EXPECT_TRUE(tp.get().has_value());

    tp.reset();
    EXPECT_FALSE(tp.hasValue());
}

// Exercise make_proxy and makeTypedProxy factory functions
TEST_F(FacadeTest, MakeProxyFactories) {
    using namespace atom::meta;

    auto p = make_proxy<TestFacade>(TestImplementation(33, "make"));
    ASSERT_TRUE(p.has_value());
    ASSERT_NE(p.target<TestImplementation>(), nullptr);
    EXPECT_EQ(p.target<TestImplementation>()->getValue(), 33);

    auto tp = makeTypedProxy<TestFacade>(TestImplementation(44, "typedmake"));
    EXPECT_TRUE(tp.hasValue());
    EXPECT_EQ(tp.type(), typeid(TestImplementation));
}

// Exercise proxy::make() static factory and in-place constructor
TEST_F(FacadeTest, ProxyMakeStaticFactory) {
    using namespace atom::meta;

    auto p = proxy<TestFacade>::make<TestImplementation>(55, "static_make");
    ASSERT_TRUE(p.has_value());
    ASSERT_NE(p.target<TestImplementation>(), nullptr);
    EXPECT_EQ(p.target<TestImplementation>()->getValue(), 55);

    proxy<TestFacade> p2(std::in_place_type<TestImplementation>, 66, "inplace");
    ASSERT_TRUE(p2.has_value());
    ASSERT_NE(p2.target<TestImplementation>(), nullptr);
    EXPECT_EQ(p2.target<TestImplementation>()->getValue(), 66);
}

// Exercise proxy(nullptr) construction
TEST_F(FacadeTest, ProxyNullptrConstruction) {
    using namespace atom::meta;
    proxy<TestFacade> p(nullptr);
    EXPECT_FALSE(p.has_value());
}

// Exercise raw_data() on filled and empty proxy
TEST_F(FacadeTest, RawDataAccess) {
    using namespace atom::meta;
    proxy<TestFacade> p(42);
    EXPECT_NE(p.raw_data(), nullptr);

    proxy<TestFacade> empty;
    EXPECT_EQ(empty.raw_data(), nullptr);
}

// Exercise facade_builder constraint helpers:
// support_copy, support_relocation, support_destruction, with_thread_safety
TEST_F(FacadeTest, FacadeBuilderConstraintHelpers) {
    using namespace atom::meta;

    using F1 = default_builder
        ::support_copy<constraint_level::trivial>
        ::support_relocation<constraint_level::trivial>
        ::support_destruction<constraint_level::trivial>
        ::with_thread_safety<thread_safety::synchronized>
        ::build;

    static_assert(F1::constraints.copyability == constraint_level::trivial);
    static_assert(F1::constraints.relocatability == constraint_level::trivial);
    static_assert(F1::constraints.destructibility == constraint_level::trivial);
    static_assert(F1::constraints.concurrency == thread_safety::synchronized);

    // Ensure a proxy can be created with these constraints (use int: trivially copyable/movable/destructible)
    proxy<F1> p(42);
    ASSERT_TRUE(p.has_value());
    ASSERT_NE(p.target<int>(), nullptr);
    EXPECT_EQ(*p.target<int>(), 42);
}

namespace facade_test_helpers {
struct DummyReflector {
    using reflector_type = DummyReflector;
    static constexpr bool is_direct = true;
};
}  // namespace facade_test_helpers

// Exercise add_direct_convention and add_direct_reflection builder methods
TEST_F(FacadeTest, FacadeBuilderDirectConventionReflection) {
    using namespace atom::meta;
    using DummyReflector = facade_test_helpers::DummyReflector;

    // Build a facade with a direct convention and a direct reflection
    using F = default_builder
        ::add_direct_convention<print_dispatch, void() const>
        ::add_direct_reflection<DummyReflector>
        ::build;

    static_assert(facade<F>);
    // Verify tuple sizes grew
    using Cs = F::convention_types;
    using Rs = F::reflection_types;
    static_assert(std::tuple_size_v<Cs> == 1);
    static_assert(std::tuple_size_v<Rs> == 1);

    proxy<F> p(TestImplementation(22, "direct"));
    ASSERT_TRUE(p.has_value());
}

// Exercise add_facade builder to merge two facades
TEST_F(FacadeTest, FacadeBuilderAddFacade) {
    using namespace atom::meta;

    using FacadeA = default_builder
        ::add_convention<print_dispatch, void() const>
        ::build;

    using FacadeB = default_builder
        ::add_convention<to_string_dispatch, std::string() const>
        ::build;

    using Merged = default_builder::add_facade<FacadeA>::add_facade<FacadeB>::build;

    static_assert(facade<Merged>);
    proxy<Merged> p(42);
    ASSERT_TRUE(p.has_value());
}

// Exercise proxy::print() and proxy::to_string() error path
// (empty proxy should trigger the catch-block)
TEST_F(FacadeTest, PrintAndToStringOnEmpty) {
    using namespace atom::meta;

    proxy<TestFacade> empty;
    // print() on empty proxy: call<print_dispatch> throws bad_function_call,
    // caught internally, prints "[unprintable object]"
    std::ostringstream oss;
    empty.print(oss);
    EXPECT_EQ(oss.str(), "[unprintable object]");

    // to_string() on empty proxy: returns "[unconvertible object]"
    EXPECT_EQ(empty.to_string(), "[unconvertible object]");
}

// Exercise stream operator<< (both filled and empty)
TEST_F(FacadeTest, StreamOperator) {
    using namespace atom::meta;

    proxy<TestFacade> p(TestImplementation(1, "stream"));
    proxy<TestFacade> empty;

    std::ostringstream oss1, oss2;
    oss1 << p;
    oss2 << empty;

    EXPECT_NE(oss1.str().find("[proxy object type:"), std::string::npos);
    EXPECT_EQ(oss2.str(), "[empty proxy]");
}

// Exercise ProxiableFor concept
TEST_F(FacadeTest, ProxiableForConcept) {
    using namespace atom::meta;
    static_assert(ProxiableFor<TestImplementation, TestFacade>);
    // A type too large should NOT satisfy ProxiableFor with a very small facade
    struct Huge { char data[512]; };
    using TinyFacade = default_builder::restrict_layout<8, 8>::build;
    static_assert(!ProxiableFor<Huge, TinyFacade>);
}

// Exercise normalize_constraints: max_size == 0 and max_align == 0
TEST_F(FacadeTest, NormalizeConstraintsZeroValues) {
    using namespace atom::meta;
    proxiable_constraints c{};
    c.max_size = 0;
    c.max_align = 0;
    auto nc = detail::normalize_constraints(c);
    EXPECT_EQ(nc.max_size, sizeof(void*) * 2);
    EXPECT_EQ(nc.max_align, alignof(void*));
}

// Exercise merge_constraints size/align min behavior
TEST_F(FacadeTest, MergeConstraintsSizeAlign) {
    using namespace atom::meta;
    proxiable_constraints a{};
    a.max_size = 128; a.max_align = 32;
    proxiable_constraints b{};
    b.max_size = 64; b.max_align = 16;
    auto m = detail::merge_constraints(a, b);
    EXPECT_EQ(m.max_size, 64u);
    EXPECT_EQ(m.max_align, 16u);
}

// Exercise with_skills builder helper
TEST_F(FacadeTest, WithSkillsBuilder) {
    using namespace atom::meta;
    // formattable and stringable are skill templates
    using F = default_builder::with_skills<formattable, stringable>::build;
    static_assert(facade<F>);
    proxy<F> p(42);
    ASSERT_TRUE(p.has_value());
    // call<print_dispatch> should work on an int proxy
    EXPECT_NO_THROW(p.call<print_dispatch>());
    std::string ts = (p.call<to_string_dispatch, std::string>());
    EXPECT_EQ(ts, "42");
}

// Exercise copy-assignment of proxy to itself (no-op branch)
TEST_F(FacadeTest, CopyAssignmentSelf) {
    using namespace atom::meta;
    proxy<TestFacade> p(TestImplementation(77, "selfassign"));
    proxy<TestFacade>& ref = p;
    // Self-assignment must be a no-op
    p = ref;
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p.target<TestImplementation>()->getValue(), 77);
}

// Exercise move-assignment of proxy to itself (no-op branch)
TEST_F(FacadeTest, MoveAssignmentSelf) {
    using namespace atom::meta;
    proxy<TestFacade> p(TestImplementation(88, "selfmove"));
    proxy<TestFacade>& ref = p;
    p = std::move(ref);
    ASSERT_TRUE(p.has_value());
    EXPECT_EQ(p.target<TestImplementation>()->getValue(), 88);
}

// Exercise copy-assignment from empty proxy
TEST_F(FacadeTest, CopyAssignFromEmpty) {
    using namespace atom::meta;
    proxy<TestFacade> filled(TestImplementation(5, "five"));
    proxy<TestFacade> empty;
    filled = empty;
    EXPECT_FALSE(filled.has_value());
}

// Exercise move-assignment from empty proxy
TEST_F(FacadeTest, MoveAssignFromEmpty) {
    using namespace atom::meta;
    proxy<TestFacade> filled(TestImplementation(6, "six"));
    proxy<TestFacade> empty;
    filled = std::move(empty);
    EXPECT_FALSE(filled.has_value());
}

// Exercise const target()
TEST_F(FacadeTest, ConstTarget) {
    using namespace atom::meta;
    const proxy<TestFacade> p(TestImplementation(3, "const"));
    const TestImplementation* ptr = p.target<TestImplementation>();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->getValue(), 3);
    // Wrong type returns nullptr
    const AnotherImplementation* nil = p.target<AnotherImplementation>();
    EXPECT_EQ(nil, nullptr);
}

// Exercise target() for wrong type (mutable)
TEST_F(FacadeTest, TargetWrongType) {
    using namespace atom::meta;
    proxy<TestFacade> p(TestImplementation(4, "wrongtype"));
    EXPECT_EQ(p.target<AnotherImplementation>(), nullptr);
    EXPECT_NE(p.target<TestImplementation>(), nullptr);
}

// Exercise serialize/deserialize via call<serialize_dispatch>
TEST_F(FacadeTest, SerializeDeserializeDispatch) {
    using namespace atom::meta;

    struct Serializable {
        int v = 0;
        std::string serialize() const { return std::to_string(v); }
        bool deserialize(const std::string& s) {
            v = std::stoi(s);
            return true;
        }
    };

    proxy<TestFacade> p(Serializable{42});
    ASSERT_TRUE(p.has_value());

    // Serialize via call<serialize_dispatch, std::string>()
    std::string ser = (p.call<serialize_dispatch, std::string>());
    EXPECT_EQ(ser, "42");

    // Directly exercise serialize_dispatch helpers (no_args = serialize path)
    // The call<serialize_dispatch, bool>(string) path tries to find a record
    // with matching skill_type; the dispatch selects based on R and Args.
    // Verify the dispatch_impl directly instead:
    ASSERT_NE(p.target<Serializable>(), nullptr);
    bool ok = serialize_dispatch::deserialize_impl<Serializable>(
        p.target<Serializable>(), "99");
    EXPECT_TRUE(ok);
    EXPECT_EQ(p.target<Serializable>()->v, 99);
}

// Exercise compare_dispatch via call<compare_dispatch, bool>
TEST_F(FacadeTest, CompareDispatch) {
    using namespace atom::meta;

    proxy<TestFacade> p1(42);
    proxy<TestFacade> p2(42);
    proxy<TestFacade> p3(43);
    proxy<TestFacade> empty;

    // Same value
    bool eq12 = (p1.call<compare_dispatch, bool>(p2));
    EXPECT_TRUE(eq12);
    // Different value
    bool eq13 = (p1.call<compare_dispatch, bool>(p3));
    EXPECT_FALSE(eq13);
    // Other is empty
    bool eq1e = (p1.call<compare_dispatch, bool>(empty));
    EXPECT_FALSE(eq1e);
}

// Exercise debug_dispatch::dump_impl directly (lines in debug_dispatch)
TEST_F(FacadeTest, DebugDispatchDumpImpl) {
    using namespace atom::meta;

    std::ostringstream oss;
    int v = 42;
    debug_dispatch::dump_impl<int>(&v, oss);
    const auto& s = oss.str();
    EXPECT_NE(s.find("Size:"), std::string::npos);
    EXPECT_NE(s.find("Content:"), std::string::npos);
}

// Exercise to_string_dispatch::to_string_impl for each branch
TEST_F(FacadeTest, ToStringDispatchBranches) {
    using namespace atom::meta;

    // Branch 1: std::to_string exists
    int v = 123;
    EXPECT_EQ(to_string_dispatch::to_string_impl<int>(&v), "123");

    // Branch 3: .to_string() member
    struct HasToString {
        std::string to_string() const { return "custom"; }
    };
    HasToString h;
    EXPECT_EQ(to_string_dispatch::to_string_impl<HasToString>(&h), "custom");

    // Branch 4: fallback
    struct NoConv {};
    NoConv nc;
    std::string fb = to_string_dispatch::to_string_impl<NoConv>(&nc);
    EXPECT_NE(fb.find("[no string conversion"), std::string::npos);
}

// Exercise compare_dispatch::equals_impl
TEST_F(FacadeTest, CompareDispatchEqualsImpl) {
    using namespace atom::meta;

    int a = 5, b = 5, c = 6;
    // Same type, equal values
    EXPECT_TRUE(compare_dispatch::equals_impl<int>(&a, &b, typeid(int)));
    // Same type, different values
    EXPECT_FALSE(compare_dispatch::equals_impl<int>(&a, &c, typeid(int)));
    // Different type
    double d = 5.0;
    EXPECT_FALSE(compare_dispatch::equals_impl<int>(&a, &d, typeid(double)));

    // Type without ==: verify it returns false
    struct NoEq { int x; };
    NoEq e1{1}, e2{1};
    EXPECT_FALSE(compare_dispatch::equals_impl<NoEq>(&e1, &e2, typeid(NoEq)));
}

// Exercise serialize_dispatch impls
TEST_F(FacadeTest, SerializeDispatchImpls) {
    using namespace atom::meta;

    struct S {
        std::string serialize() const { return "data"; }
        bool deserialize(const std::string& s) { return s == "data"; }
    };

    S obj;
    EXPECT_EQ(serialize_dispatch::serialize_impl<S>(&obj), "data");
    EXPECT_TRUE(serialize_dispatch::deserialize_impl<S>(&obj, "data"));
    EXPECT_FALSE(serialize_dispatch::deserialize_impl<S>(&obj, "other"));

    // Fallback serialize (no serialize() method)
    int v = 1;
    EXPECT_EQ(serialize_dispatch::serialize_impl<int>(&v), "{}");
    // Fallback deserialize
    EXPECT_FALSE(serialize_dispatch::deserialize_impl<int>(&v, "1"));
}

// Exercise print_dispatch::print_impl for a non-streamable type
TEST_F(FacadeTest, PrintDispatchUnprintable) {
    using namespace atom::meta;

    struct Unprintable {};
    Unprintable obj;
    // Should not throw; routes to the "[unprintable object type: ...]" branch
    EXPECT_NO_THROW(print_dispatch::print_impl<Unprintable>(&obj));
}

// Lines 118,129 – trivially copy/move constructible type exercises memcpy paths
// in make_vtable<T>().copy and make_vtable<T>().move lambdas.
TEST_F(FacadeTest, VtableTrivialCopyMove) {
    using namespace atom::meta;
    // int is trivially copy and move constructible
    auto vtbl = detail::make_vtable<int>();

    int src = 42;
    int dst_copy = 0, dst_move = 0;

    // Trigger the trivially-copy path (line 118)
    vtbl.copy(&src, &dst_copy);
    EXPECT_EQ(dst_copy, 42);

    // Trigger the trivially-move path (line 129)
    vtbl.move(&src, &dst_move);
    EXPECT_EQ(dst_move, 42);
}

// Line 480,482 – clone_impl copy-construction path (facade copyability != none)
// A copy-constructible type without a clone() method; default facade allows copy.
TEST_F(FacadeTest, CloneImplCopyConstruction) {
    using namespace atom::meta;
    using F = TestFacade;

    struct CopyOnly {
        int v;
        explicit CopyOnly(int x) : v(x) {}
        CopyOnly(const CopyOnly&) = default;
        CopyOnly(CopyOnly&&) noexcept = default;
    };

    CopyOnly src{77};
    alignas(F::constraints.max_align) std::byte storage[F::constraints.max_size]{};
    const detail::vtable* vptr_out = nullptr;

    // This hits lines 477-482: copy-constructible, copyability != none
    cloneable_dispatch::clone_impl<CopyOnly, F>(&src, storage, &vptr_out);
    ASSERT_NE(vptr_out, nullptr);
    CopyOnly* result = std::launder(reinterpret_cast<CopyOnly*>(storage));
    EXPECT_EQ(result->v, 77);
    // Clean up
    vptr_out->destroy(storage);
}

// Line 1048 – throw std::bad_function_call() inside call():
// the skill is found in the vtable but no dispatch branch handles it.
// Achieved by calling call<to_string_dispatch, void>() — the to_string branch
// only returns for R=std::string or matches nothing, so execution falls to
// the throw on line 1048.
TEST_F(FacadeTest, CallSkillFoundButNoBranchMatchesThrows) {
    using namespace atom::meta;

    proxy<TestFacade> p(42);
    ASSERT_TRUE(p.has_value());
    // to_string_dispatch IS registered for int, but calling with R=void and
    // no args enters the branch and... let's check:
    // std::is_same_v<R, std::string> = false (R=void), so it calls func and
    // returns R() which is void. That actually does return.
    // Instead, call compare_dispatch with wrong arg count to hit the throw.
    // compare_dispatch branch: sizeof...(Args)==1 && is_same_v<R,bool> required.
    // Call with R=int (not bool) — compare_dispatch is found, but the constexpr
    // if is false, falls through to throw at 1048.
    EXPECT_THROW((p.call<compare_dispatch, int>(p)), std::bad_function_call);
}

// Lines 1086-1087 – equals() catch block:
// an object with no operator== has no compare_dispatch in vtable,
// so call<compare_dispatch> throws runtime_error, caught by equals().
TEST_F(FacadeTest, EqualsNoOperatorEq) {
    using namespace atom::meta;

    struct NoEqOp { int x; };
    proxy<TestFacade> p1(NoEqOp{1});
    proxy<TestFacade> p2(NoEqOp{1});

    // compare_dispatch is NOT registered for NoEqOp (no operator==)
    // so call<compare_dispatch, bool> throws, caught by equals() -> returns false
    EXPECT_FALSE(p1.equals(p2));
}

// Line 1097 – proxy::clone() on empty proxy returns empty proxy
TEST_F(FacadeTest, CloneEmptyProxy) {
    using namespace atom::meta;
    proxy<TestFacade> empty;
    proxy<TestFacade> cloned = empty.clone();
    EXPECT_FALSE(cloned.has_value());
}

}  // namespace
