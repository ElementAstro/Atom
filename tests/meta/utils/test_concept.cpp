/*!
 * \file test_concept.cpp
 * \brief Comprehensive tests for atom::meta concepts (including C++23 enhanced)
 * \author Max Qian <lightapt.com>
 * \date 2024
 * \copyright Copyright (C) 2023-2024 Max Qian
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "atom/meta/concept.hpp"

#include <array>
#include <chrono>
#include <complex>
#include <deque>
#include <functional>
#include <future>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

namespace {

// Test helper classes
class TestClass {
public:
    TestClass() = default;
    int getValue() const { return 42; }
};

class TestCallable {
public:
    int operator()() const { return 42; }
};

class PolymorphicBase {
public:
    virtual ~PolymorphicBase() = default;
    virtual int getValue() const { return 0; }
};

class DerivedClass : public PolymorphicBase {
public:
    int getValue() const override { return 42; }
};

class FinalType final {};

class AbstractType {
public:
    virtual ~AbstractType() = default;
    virtual void pureVirtual() = 0;
};

struct AggregateStruct {
    int x;
    double y;
    std::string z;
};

struct WithToString {
    std::string toString() const { return "test"; }
};

struct WithToJson {
    int toJson() const { return 0; }
};

struct CloneableType {
    std::unique_ptr<CloneableType> clone() const {
        return std::make_unique<CloneableType>(*this);
    }
};

enum class ScopedEnum { Value1, Value2 };
enum UnscopedEnum { EnumVal1, EnumVal2 };

// Test fixture
class ConceptTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

//==============================================================================
// Function Concepts
//==============================================================================

TEST_F(ConceptTest, InvocableConcept) {
    auto lambda = [](int x) { return x * 2; };
    static_assert(Invocable<decltype(lambda), int>);
    static_assert(!Invocable<int, int>);
}

TEST_F(ConceptTest, InvocableRConcept) {
    auto add = [](int a, int b) { return a + b; };
    static_assert(InvocableR<decltype(add), int, int, int>);
    static_assert(InvocableR<decltype(add), double, int, int>);
}

TEST_F(ConceptTest, NothrowInvocableConcept) {
    auto noexcept_func = []() noexcept { return 42; };
    static_assert(NothrowInvocable<decltype(noexcept_func)>);

    auto throwing_func = []() { return 42; };
    static_assert(!NothrowInvocable<decltype(throwing_func)>);
}

TEST_F(ConceptTest, FunctionPointerConcept) {
    using FuncPtr = int (*)();
    static_assert(FunctionPointer<FuncPtr>);
    static_assert(!FunctionPointer<int>);
    static_assert(!FunctionPointer<TestClass>);
}

TEST_F(ConceptTest, MemberFunctionPointerConcept) {
    using MemberFuncPtr = int (TestClass::*)();
    static_assert(MemberFunctionPointer<MemberFuncPtr>);
    static_assert(!MemberFunctionPointer<int>);
    static_assert(!MemberFunctionPointer<int (*)()>);
}

TEST_F(ConceptTest, CallableConcept) {
    auto lambda = []() {};
    static_assert(Callable<decltype(lambda)>);
    static_assert(Callable<void (*)()>);
    static_assert(Callable<TestCallable>);
}

TEST_F(ConceptTest, CallableReturnsConcept) {
    auto intFunc = []() { return 42; };
    static_assert(CallableReturns<decltype(intFunc), int>);
    static_assert(!CallableReturns<decltype(intFunc), std::string>);
}

TEST_F(ConceptTest, CallableNoexceptConcept) {
    auto noexceptLambda = []() noexcept { return 42; };
    static_assert(CallableNoexcept<decltype(noexceptLambda)>);

    auto throwingLambda = []() { return 42; };
    static_assert(!CallableNoexcept<decltype(throwingLambda)>);
}

//==============================================================================
// Object Concepts
//==============================================================================

TEST_F(ConceptTest, RelocatableConcept) {
    static_assert(Relocatable<int>);
    static_assert(Relocatable<std::unique_ptr<int>>);
}

TEST_F(ConceptTest, DefaultConstructibleConcept) {
    static_assert(DefaultConstructible<TestClass>);
    static_assert(DefaultConstructible<int>);
    static_assert(DefaultConstructible<std::string>);
}

TEST_F(ConceptTest, CopyConstructibleConcept) {
    static_assert(CopyConstructible<TestClass>);
    static_assert(CopyConstructible<std::string>);
    static_assert(!CopyConstructible<std::unique_ptr<int>>);
}

TEST_F(ConceptTest, CopyAssignableConcept) {
    static_assert(CopyAssignable<TestClass>);
    static_assert(CopyAssignable<std::string>);
    static_assert(!CopyAssignable<std::unique_ptr<int>>);
}

TEST_F(ConceptTest, MoveConstructibleConcept) {
    static_assert(MoveConstructible<TestClass>);
    static_assert(MoveConstructible<std::unique_ptr<int>>);
    static_assert(MoveConstructible<std::string>);
}

TEST_F(ConceptTest, MoveAssignableConcept) {
    static_assert(MoveAssignable<TestClass>);
    static_assert(MoveAssignable<std::unique_ptr<int>>);
    static_assert(MoveAssignable<std::string>);
}

TEST_F(ConceptTest, EqualityComparableConcept) {
    static_assert(EqualityComparable<int>);
    static_assert(EqualityComparable<std::string>);
}

TEST_F(ConceptTest, LessThanComparableConcept) {
    static_assert(LessThanComparable<int>);
    static_assert(LessThanComparable<std::string>);
}

TEST_F(ConceptTest, HashableConcept) {
    static_assert(Hashable<int>);
    static_assert(Hashable<std::string>);
}

TEST_F(ConceptTest, SwappableConcept) {
    static_assert(Swappable<int>);
    static_assert(Swappable<std::string>);
}

TEST_F(ConceptTest, CopyableConcept) {
    static_assert(Copyable<int>);
    static_assert(Copyable<std::string>);
    static_assert(!Copyable<std::unique_ptr<int>>);
}

TEST_F(ConceptTest, DestructibleConcept) {
    static_assert(Destructible<int>);
    static_assert(Destructible<std::string>);
}

//==============================================================================
// Type Concepts
//==============================================================================

TEST_F(ConceptTest, ArithmeticConcept) {
    static_assert(Arithmetic<int>);
    static_assert(Arithmetic<float>);
    static_assert(Arithmetic<double>);
    static_assert(!Arithmetic<std::string>);
    static_assert(!Arithmetic<TestClass>);
}

TEST_F(ConceptTest, IntegralConcept) {
    static_assert(Integral<int>);
    static_assert(Integral<long>);
    static_assert(Integral<unsigned int>);
    static_assert(!Integral<float>);
    static_assert(!Integral<double>);
}

TEST_F(ConceptTest, FloatingPointConcept) {
    static_assert(FloatingPoint<float>);
    static_assert(FloatingPoint<double>);
    static_assert(FloatingPoint<long double>);
    static_assert(!FloatingPoint<int>);
}

TEST_F(ConceptTest, SignedIntegerConcept) {
    static_assert(SignedInteger<int>);
    static_assert(SignedInteger<long>);
    static_assert(!SignedInteger<unsigned int>);
    static_assert(!SignedInteger<float>);
}

TEST_F(ConceptTest, UnsignedIntegerConcept) {
    static_assert(UnsignedInteger<unsigned int>);
    static_assert(UnsignedInteger<size_t>);
    static_assert(!UnsignedInteger<int>);
    static_assert(!UnsignedInteger<float>);
}

TEST_F(ConceptTest, NumberConcept) {
    static_assert(Number<int>);
    static_assert(Number<double>);
    static_assert(!Number<std::string>);
}

TEST_F(ConceptTest, ComplexNumberConcept) {
    static_assert(ComplexNumber<std::complex<double>>);
    static_assert(ComplexNumber<std::complex<float>>);
    static_assert(!ComplexNumber<double>);
}

TEST_F(ConceptTest, CharConcept) {
    static_assert(Char<char>);
    static_assert(!Char<int>);
    static_assert(!Char<wchar_t>);
}

TEST_F(ConceptTest, WCharConcept) {
    static_assert(WChar<wchar_t>);
    static_assert(!WChar<char>);
}

TEST_F(ConceptTest, Char16Concept) {
    static_assert(Char16<char16_t>);
    static_assert(!Char16<char>);
}

TEST_F(ConceptTest, Char32Concept) {
    static_assert(Char32<char32_t>);
    static_assert(!Char32<char>);
}

TEST_F(ConceptTest, AnyCharConcept) {
    static_assert(AnyChar<char>);
    static_assert(AnyChar<wchar_t>);
    static_assert(AnyChar<char16_t>);
    static_assert(AnyChar<char32_t>);
    static_assert(!AnyChar<int>);
}

TEST_F(ConceptTest, StringTypeConcept) {
    static_assert(StringType<std::string>);
    static_assert(StringType<std::string_view>);
    static_assert(StringType<std::wstring>);
    static_assert(!StringType<int>);
    static_assert(!StringType<char*>);
}

TEST_F(ConceptTest, IsBuiltInConcept) {
    static_assert(IsBuiltIn<int>);
    static_assert(IsBuiltIn<double>);
    static_assert(IsBuiltIn<std::string>);
    static_assert(!IsBuiltIn<TestClass>);
}

TEST_F(ConceptTest, EnumConcept) {
    static_assert(Enum<ScopedEnum>);
    static_assert(Enum<UnscopedEnum>);
    static_assert(!Enum<int>);
}

//==============================================================================
// Pointer Concepts
//==============================================================================

TEST_F(ConceptTest, PointerConcept) {
    static_assert(Pointer<int*>);
    static_assert(Pointer<const int*>);
    static_assert(!Pointer<int>);
}

TEST_F(ConceptTest, UniquePointerConcept) {
    static_assert(UniquePointer<std::unique_ptr<int>>);
    static_assert(!UniquePointer<std::shared_ptr<int>>);
    static_assert(!UniquePointer<int*>);
}

TEST_F(ConceptTest, SharedPointerConcept) {
    static_assert(SharedPointer<std::shared_ptr<int>>);
    static_assert(!SharedPointer<std::unique_ptr<int>>);
    static_assert(!SharedPointer<int*>);
}

TEST_F(ConceptTest, WeakPointerConcept) {
    static_assert(WeakPointer<std::weak_ptr<int>>);
    static_assert(!WeakPointer<std::shared_ptr<int>>);
}

TEST_F(ConceptTest, SmartPointerConcept) {
    static_assert(SmartPointer<std::unique_ptr<int>>);
    static_assert(SmartPointer<std::shared_ptr<int>>);
    static_assert(SmartPointer<std::weak_ptr<int>>);
    static_assert(!SmartPointer<int*>);
}

TEST_F(ConceptTest, ReferenceConcept) {
    static_assert(Reference<int&>);
    static_assert(Reference<int&&>);
    static_assert(!Reference<int>);
}

TEST_F(ConceptTest, LvalueReferenceConcept) {
    static_assert(LvalueReference<int&>);
    static_assert(!LvalueReference<int&&>);
    static_assert(!LvalueReference<int>);
}

TEST_F(ConceptTest, RvalueReferenceConcept) {
    static_assert(RvalueReference<int&&>);
    static_assert(!RvalueReference<int&>);
    static_assert(!RvalueReference<int>);
}

TEST_F(ConceptTest, ConstConcept) {
    static_assert(Const<const int>);
    static_assert(Const<const int&>);
    static_assert(!Const<int>);
}

//==============================================================================
// Trait Concepts
//==============================================================================

TEST_F(ConceptTest, TrivialConcept) {
    static_assert(Trivial<int>);
    static_assert(!Trivial<std::string>);
}

TEST_F(ConceptTest, TriviallyConstructibleConcept) {
    static_assert(TriviallyConstructible<int>);
    static_assert(!TriviallyConstructible<std::string>);
}

TEST_F(ConceptTest, TriviallyCopyableConcept) {
    static_assert(TriviallyCopyable<int>);
    static_assert(!TriviallyCopyable<std::string>);
}

//==============================================================================
// Container Concepts
//==============================================================================

TEST_F(ConceptTest, IterableConcept) {
    static_assert(Iterable<std::vector<int>>);
    static_assert(Iterable<std::list<int>>);
    static_assert(Iterable<std::string>);
    static_assert(!Iterable<int>);
}

TEST_F(ConceptTest, ContainerConcept) {
    static_assert(Container<std::vector<int>>);
    static_assert(Container<std::list<int>>);
    static_assert(Container<std::deque<int>>);
    static_assert(!Container<int>);
}

TEST_F(ConceptTest, StringContainerConcept) {
    static_assert(StringContainer<std::string>);
    static_assert(StringContainer<std::wstring>);
    static_assert(!StringContainer<std::vector<int>>);
}

TEST_F(ConceptTest, NumberContainerConcept) {
    static_assert(NumberContainer<std::vector<int>>);
    static_assert(NumberContainer<std::vector<double>>);
    static_assert(!NumberContainer<std::vector<std::string>>);
}

TEST_F(ConceptTest, AssociativeContainerConcept) {
    static_assert(AssociativeContainer<std::map<int, std::string>>);
    static_assert(!AssociativeContainer<std::vector<int>>);
}

TEST_F(ConceptTest, IteratorConcept) {
    static_assert(Iterator<std::vector<int>::iterator>);
    static_assert(Iterator<int*>);
}

TEST_F(ConceptTest, SequenceContainerConcept) {
    static_assert(SequenceContainer<std::vector<int>>);
    static_assert(SequenceContainer<std::list<int>>);
    static_assert(SequenceContainer<std::deque<int>>);
    static_assert(!SequenceContainer<std::map<int, int>>);
}

//==============================================================================
// Multi-threading Concepts
//==============================================================================

TEST_F(ConceptTest, LockableConcept) {
    static_assert(Lockable<std::mutex>);
    static_assert(Lockable<std::shared_mutex>);
}

TEST_F(ConceptTest, SharedLockableConcept) {
    static_assert(SharedLockable<std::shared_mutex>);
    static_assert(!SharedLockable<std::mutex>);
}

TEST_F(ConceptTest, MutexConcept) {
    static_assert(Mutex<std::mutex>);
    static_assert(Mutex<std::shared_mutex>);
}

TEST_F(ConceptTest, SharedMutexConcept) {
    static_assert(SharedMutex<std::shared_mutex>);
    static_assert(!SharedMutex<std::mutex>);
}

//==============================================================================
// Asynchronous Concepts
//==============================================================================

TEST_F(ConceptTest, FutureConcept) {
    static_assert(Future<std::future<int>>);
    static_assert(Future<std::future<void>>);
}

TEST_F(ConceptTest, PromiseConcept) {
    static_assert(Promise<std::promise<int>>);
    static_assert(Promise<std::promise<void>>);
}

TEST_F(ConceptTest, AsyncResultConcept) {
    static_assert(AsyncResult<std::future<int>>);
    static_assert(AsyncResult<std::promise<int>>);
}

//==============================================================================
// C++23 Enhanced Concepts
//==============================================================================

TEST_F(ConceptTest, FormattableConcept) {
    static_assert(Formattable<int>);
    static_assert(Formattable<double>);
    static_assert(Formattable<std::string>);
}

TEST_F(ConceptTest, HasToStringConcept) {
    static_assert(HasToString<WithToString>);
    static_assert(!HasToString<int>);
}

TEST_F(ConceptTest, StringViewConvertibleConcept) {
    static_assert(StringViewConvertible<std::string>);
    static_assert(StringViewConvertible<std::string_view>);
    static_assert(StringViewConvertible<const char*>);
}

TEST_F(ConceptTest, JsonSerializableConcept) {
    static_assert(JsonSerializable<WithToJson>);
    static_assert(!JsonSerializable<int>);
}

TEST_F(ConceptTest, SpanCompatibleConcept) {
    static_assert(SpanCompatible<std::vector<int>>);
    static_assert(SpanCompatible<std::array<int, 5>>);
    static_assert(SpanCompatible<std::string>);
}

TEST_F(ConceptTest, ContiguousRangeConcept) {
    static_assert(ContiguousRange<std::vector<int>>);
    static_assert(ContiguousRange<std::string>);
    static_assert(!ContiguousRange<std::list<int>>);
}

TEST_F(ConceptTest, SizedRangeConcept) {
    static_assert(SizedRange<std::vector<int>>);
    static_assert(SizedRange<std::list<int>>);
}

TEST_F(ConceptTest, ViewableRangeConcept) {
    static_assert(ViewableRange<std::vector<int>>);
    static_assert(ViewableRange<std::string>);
}

//==============================================================================
// Coroutine Concepts
//==============================================================================

TEST_F(ConceptTest, AwaitableTypeConcept) {
    struct SimpleAwaitable {
        bool await_ready() const { return true; }
        void await_suspend(std::coroutine_handle<>) {}
        int await_resume() { return 42; }
    };

    static_assert(AwaitableType<SimpleAwaitable>);
    static_assert(!AwaitableType<int>);
}

//==============================================================================
// Memory and Lifetime Concepts
//==============================================================================

TEST_F(ConceptTest, TriviallyRelocatableConcept) {
    static_assert(TriviallyRelocatable<int>);
    static_assert(!TriviallyRelocatable<std::string>);
}

TEST_F(ConceptTest, AggregateConcept) {
    static_assert(Aggregate<AggregateStruct>);
    static_assert(!Aggregate<TestClass>);
}

TEST_F(ConceptTest, StandardLayoutConcept) {
    static_assert(StandardLayout<int>);
    static_assert(StandardLayout<AggregateStruct>);
}

TEST_F(ConceptTest, PodTypeConcept) {
    static_assert(PodType<int>);
    static_assert(!PodType<std::string>);
}

//==============================================================================
// Enhanced Callable Concepts
//==============================================================================

TEST_F(ConceptTest, MoveOnlyInvocableConcept) {
    auto moveOnlyLambda = [ptr = std::make_unique<int>(42)]() { return *ptr; };
    static_assert(MoveOnlyInvocable<decltype(moveOnlyLambda)>);
}

TEST_F(ConceptTest, ConstInvocableConcept) {
    auto constLambda = [](int x) { return x * 2; };
    static_assert(ConstInvocable<decltype(constLambda), int>);
}

TEST_F(ConceptTest, NoexceptInvocableConcept) {
    static_assert(
        NoexceptInvocable<decltype([](int x) noexcept { return x; }), int>);
}

TEST_F(ConceptTest, PredicateTypeConcept) {
    auto pred = [](int x) { return x > 0; };
    static_assert(PredicateType<decltype(pred), int>);
}

TEST_F(ConceptTest, ComparatorConcept) {
    static_assert(Comparator<std::less<int>, int>);
    static_assert(Comparator<std::greater<int>, int>);
}

//==============================================================================
// Type Relationship Concepts
//==============================================================================

TEST_F(ConceptTest, HasVirtualDestructorConcept) {
    static_assert(HasVirtualDestructor<PolymorphicBase>);
    static_assert(!HasVirtualDestructor<TestClass>);
}

TEST_F(ConceptTest, PolymorphicTypeConcept) {
    static_assert(PolymorphicType<PolymorphicBase>);
    static_assert(!PolymorphicType<int>);
}

TEST_F(ConceptTest, FinalClassConcept) {
    static_assert(FinalClass<FinalType>);
    static_assert(!FinalClass<TestClass>);
}

TEST_F(ConceptTest, AbstractClassConcept) {
    static_assert(AbstractClass<AbstractType>);
    static_assert(!AbstractClass<TestClass>);
}

TEST_F(ConceptTest, ScopedEnumTypeConcept) {
    static_assert(ScopedEnumType<ScopedEnum>);
    static_assert(!ScopedEnumType<UnscopedEnum>);
}

TEST_F(ConceptTest, UnscopedEnumTypeConcept) {
    static_assert(UnscopedEnumType<UnscopedEnum>);
    static_assert(!UnscopedEnumType<ScopedEnum>);
}

//==============================================================================
// Enhanced Numeric Concepts
//==============================================================================

TEST_F(ConceptTest, SignedIntegralTypeConcept) {
    static_assert(SignedIntegralType<int>);
    static_assert(SignedIntegralType<long>);
    static_assert(!SignedIntegralType<unsigned int>);
}

TEST_F(ConceptTest, UnsignedIntegralTypeConcept) {
    static_assert(UnsignedIntegralType<unsigned int>);
    static_assert(UnsignedIntegralType<size_t>);
    static_assert(!UnsignedIntegralType<int>);
}

TEST_F(ConceptTest, FloatingPointPreciseConcept) {
    static_assert(FloatingPointPrecise<float>);
    static_assert(FloatingPointPrecise<double>);
    static_assert(FloatingPointPrecise<long double>);
    static_assert(!FloatingPointPrecise<int>);
}

TEST_F(ConceptTest, NumericArithmeticConcept) {
    static_assert(NumericArithmetic<int>);
    static_assert(NumericArithmetic<double>);
}

TEST_F(ConceptTest, BitwiseOperableConcept) {
    static_assert(BitwiseOperable<int>);
    static_assert(BitwiseOperable<unsigned int>);
    static_assert(!BitwiseOperable<double>);
}

//==============================================================================
// Optional/Expected Concepts
//==============================================================================

TEST_F(ConceptTest, OptionalLikeConcept) {
    static_assert(OptionalLike<std::optional<int>>);
    static_assert(!OptionalLike<int>);
}

//==============================================================================
// Tuple and Variant Concepts
//==============================================================================

TEST_F(ConceptTest, TupleLikeTypeConcept) {
    static_assert(TupleLikeType<std::tuple<int, double>>);
    static_assert(TupleLikeType<std::pair<int, double>>);
    static_assert(TupleLikeType<std::array<int, 5>>);
    static_assert(!TupleLikeType<std::vector<int>>);
}

TEST_F(ConceptTest, VariantLikeConcept) {
    static_assert(VariantLike<std::variant<int, double, std::string>>);
    static_assert(!VariantLike<int>);
}

//==============================================================================
// Meta Module Interoperability Concepts
//==============================================================================

TEST_F(ConceptTest, DemanglableConcept) {
    static_assert(Demanglable<int>);
    static_assert(Demanglable<std::string>);
    static_assert(Demanglable<TestClass>);
}

TEST_F(ConceptTest, TypeInfoSupportedConcept) {
    static_assert(TypeInfoSupported<int>);
    static_assert(TypeInfoSupported<TestClass>);
    static_assert(TypeInfoSupported<ScopedEnum>);
}

TEST_F(ConceptTest, BoxCompatibleConcept) {
    static_assert(BoxCompatible<int>);
    static_assert(BoxCompatible<std::string>);
    static_assert(BoxCompatible<std::unique_ptr<int>>);
}

TEST_F(ConceptTest, ProxyCompatibleConcept) {
    auto lambda = []() {};
    static_assert(ProxyCompatible<decltype(lambda)>);
    static_assert(ProxyCompatible<void (*)()>);
}

TEST_F(ConceptTest, DecoratorCompatibleConcept) {
    auto lambda = []() {};
    static_assert(DecoratorCompatible<decltype(lambda)>);
}

TEST_F(ConceptTest, ReflectionCompatibleConcept) {
    static_assert(ReflectionCompatible<AggregateStruct>);
    static_assert(!ReflectionCompatible<TestClass>);
}

TEST_F(ConceptTest, EnumWithTraitsConcept) {
    static_assert(EnumWithTraits<ScopedEnum>);
    static_assert(EnumWithTraits<UnscopedEnum>);
    static_assert(!EnumWithTraits<int>);
}

TEST_F(ConceptTest, InvokableWithResultConcept) {
    auto intFunc = []() { return 42; };
    auto voidFunc = []() {};

    static_assert(InvokableWithResult<decltype(intFunc)>);
    static_assert(!InvokableWithResult<decltype(voidFunc)>);
}

TEST_F(ConceptTest, VoidInvokableConcept) {
    auto voidFunc = []() {};
    auto intFunc = []() { return 42; };

    static_assert(VoidInvokable<decltype(voidFunc)>);
    static_assert(!VoidInvokable<decltype(intFunc)>);
}

TEST_F(ConceptTest, NothrowInvokableConcept) {
    auto noexceptFunc = []() noexcept {};
    auto throwingFunc = []() {};

    static_assert(NothrowInvokable<decltype(noexceptFunc)>);
    static_assert(!NothrowInvokable<decltype(throwingFunc)>);
}

TEST_F(ConceptTest, MetaComparableConcept) {
    static_assert(MetaComparable<int>);
    static_assert(MetaComparable<std::string>);
}

TEST_F(ConceptTest, MetaHashableConcept) {
    static_assert(MetaHashable<int>);
    static_assert(MetaHashable<std::string>);
}

TEST_F(ConceptTest, RegistryCompatibleConcept) {
    static_assert(RegistryCompatible<int>);
    static_assert(RegistryCompatible<std::string>);
}

TEST_F(ConceptTest, FactoryCreatableConcept) {
    static_assert(FactoryCreatable<int>);
    static_assert(FactoryCreatable<std::string>);
    static_assert(FactoryCreatable<TestClass>);
}

TEST_F(ConceptTest, CloneableConcept) {
    static_assert(Cloneable<CloneableType>);
    static_assert(Cloneable<int>);  // Copy constructible counts
}

TEST_F(ConceptTest, SubscriptableConcept) {
    static_assert(Subscriptable<std::vector<int>>);
    static_assert(Subscriptable<std::string>);
    static_assert(Subscriptable<std::map<int, int>, int>);
    static_assert(!Subscriptable<int>);
}

TEST_F(ConceptTest, ReservableConcept) {
    static_assert(Reservable<std::vector<int>>);
    static_assert(Reservable<std::string>);
    static_assert(!Reservable<std::list<int>>);
}

TEST_F(ConceptTest, AssociativeLookupConcept) {
    static_assert(AssociativeLookup<std::map<int, int>>);
    static_assert(AssociativeLookup<std::set<int>>);
    static_assert(!AssociativeLookup<std::vector<int>>);
}

TEST_F(ConceptTest, OrderedContainerConcept) {
    static_assert(OrderedContainer<std::map<int, int>>);
    static_assert(OrderedContainer<std::set<int>>);
    static_assert(!OrderedContainer<std::unordered_map<int, int>>);
}

TEST_F(ConceptTest, DurationConcept) {
    static_assert(Duration<std::chrono::seconds>);
    static_assert(Duration<std::chrono::milliseconds>);
    static_assert(!Duration<int>);
}

TEST_F(ConceptTest, TimePointConcept) {
    static_assert(TimePoint<std::chrono::system_clock::time_point>);
    static_assert(TimePoint<std::chrono::steady_clock::time_point>);
    static_assert(!TimePoint<std::chrono::seconds>);
}

TEST_F(ConceptTest, HasSizeConcept) {
    static_assert(HasSize<std::vector<int>>);
    static_assert(HasSize<std::string>);
    static_assert(!HasSize<int>);
}

TEST_F(ConceptTest, EmptyCheckableConcept) {
    static_assert(EmptyCheckable<std::vector<int>>);
    static_assert(EmptyCheckable<std::string>);
    static_assert(!EmptyCheckable<int>);
}

TEST_F(ConceptTest, ClearableConcept) {
    static_assert(Clearable<std::vector<int>>);
    static_assert(Clearable<std::string>);
    static_assert(!Clearable<int>);
}

TEST_F(ConceptTest, BackInsertableConcept) {
    static_assert(BackInsertable<std::vector<int>>);
    static_assert(BackInsertable<std::string>);
    static_assert(!BackInsertable<std::set<int>>);
}

TEST_F(ConceptTest, BackEmplaceableConcept) {
    static_assert(BackEmplaceable<std::vector<int>>);
    static_assert(!BackEmplaceable<std::set<int>>);
}

TEST_F(ConceptTest, FrontBackAccessibleConcept) {
    static_assert(FrontBackAccessible<std::vector<int>>);
    static_assert(FrontBackAccessible<std::deque<int>>);
    static_assert(!FrontBackAccessible<std::set<int>>);
}

TEST_F(ConceptTest, NullableConcept) {
    static_assert(Nullable<std::optional<int>>);
    static_assert(Nullable<std::unique_ptr<int>>);
    static_assert(Nullable<std::shared_ptr<int>>);
    static_assert(!Nullable<int>);
}

TEST_F(ConceptTest, AtomicLikeConcept) {
    static_assert(AtomicLike<std::atomic<int>>);
    static_assert(!AtomicLike<int>);
}

TEST_F(ConceptTest, MoveOnlyConcept) {
    static_assert(MoveOnly<std::unique_ptr<int>>);
    static_assert(!MoveOnly<std::string>);
}

TEST_F(ConceptTest, RegularSemiregularConcepts) {
    static_assert(Regular<int>);
    static_assert(Regular<std::string>);
    static_assert(!Regular<std::unique_ptr<int>>);
    static_assert(Semiregular<int>);
    static_assert(Semiregular<std::string>);
}

TEST_F(ConceptTest, ThreeWayComparableConcept) {
    static_assert(ThreeWayComparable<int>);
    static_assert(ThreeWayComparable<std::string>);
}

TEST_F(ConceptTest, TypePackUtilities) {
    static_assert(is_one_of_v<int, char, int, double>);
    static_assert(!is_one_of_v<float, char, int, double>);
    static_assert(std::is_same_v<first_type_t<int, double, char>, int>);
    static_assert(std::is_same_v<last_type_t<int, double, char>, char>);
    static_assert(std::is_same_v<last_type_t<int>, int>);
}

}  // anonymous namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
