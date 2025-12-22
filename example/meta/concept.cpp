/**
 * Comprehensive examples for atom::meta::concept utilities
 *
 * This file demonstrates all concept categories from atom/meta/concept.hpp:
 * 1. Function Concepts
 * 2. Object Concepts
 * 3. Type Concepts
 * 4. Container Concepts
 * 5. Multi-threading Concepts
 * 6. Asynchronous Concepts
 * 7. C++23 Enhanced Concepts (NEW)
 * 8. Meta Module Interoperability Concepts (NEW)
 * 9. Advanced Type Manipulation Concepts (NEW)
 */

#include "" atom / meta / concept.hpp ""

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <complex>
#include <deque>
#include <functional>
#include <future>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

// Helper function to print concept check resultstemplate <bool Result>
void printConceptCheck(const std::string& conceptName,
                       const std::string& typeName) {
    std::cout << ""
                 ""
              << conceptName << "" <
        "" << typeName << "" >
        : "" << (Result ? "" Satisfied "" : "" Not satisfied "") << std::endl;
}

void printSection(const std::string& title) {
    std::cout << ""\n "" << std::string(60, '=') << std::endl;
    std::cout << ""
                 ""
              << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

// -----------------------------------------------------------------------------
// Helper Classes
// -----------------------------------------------------------------------------

int add(int a, int b) { return a + b; }
void noexceptFunc() noexcept {}

class Functor {
public:
    int operator()(int a, int b) const { return a * b; }
};

class NoexceptFunctor {
public:
    int operator()(int a, int b) const noexcept { return a * b; }
};

class TestClass {
public:
    int multiply(int a, int b) const { return a * b; }
};

class RelocatableClass {
public:
    RelocatableClass() = default;
    RelocatableClass(RelocatableClass&&) noexcept = default;
    RelocatableClass& operator=(RelocatableClass&&) noexcept = default;
};

class ComparableClass {
private:
    int value;

public:
    ComparableClass(int v) : value(v) {}
    bool operator==(const ComparableClass& other) const {
        return value == other.value;
    }
    bool operator!=(const ComparableClass& other) const {
        return value != other.value;
    }
    bool operator<(const ComparableClass& other) const {
        return value < other.value;
    }
};

class HashableClass {
private:
    int value;

public:
    HashableClass(int v) : value(v) {}
    bool operator==(const HashableClass& other) const {
        return value == other.value;
    }
    int getValue() const { return value; }
};

namespace std {
template <>
struct hash<HashableClass> {
    std::size_t operator()(const HashableClass& obj) const {
        return std::hash<int>()(obj.getValue());
    }
};
}  // namespace std

enum Color { Red, Green, Blue };
enum class ScopedColor { Red, Green, Blue };

class PolymorphicBase {
public:
    virtual ~PolymorphicBase() = default;
    virtual void doSomething() = 0;
};

class DerivedClass : public PolymorphicBase {
public:
    void doSomething() override {}
};

class FinalClass final {};

struct AggregateStruct {
    int x;
    double y;
    std::string z;
};

struct WithToString {
    std::string toString() const { return "" WithToString ""; }
};

struct WithToJson {
    int toJson() const { return 0; }
};

struct CloneableClass {
    std::unique_ptr<CloneableClass> clone() const {
        return std::make_unique<CloneableClass>(*this);
    }
};

// -----------------------------------------------------------------------------
// Function Concept Examples
// -----------------------------------------------------------------------------

void testFunctionConcepts() {
    printSection("" Function Concepts "");

    printConceptCheck<Invocable<decltype(add), int, int>>("" Invocable "",
                                                          "" add(int, int) "");
    printConceptCheck<InvocableR<decltype(add), int, int, int>>(
        "" InvocableR < int > "", "" add(int, int) "");
    printConceptCheck<NothrowInvocable<decltype(noexceptFunc)>>(
        "" NothrowInvocable "", "" noexceptFunc() "");
    printConceptCheck<FunctionPointer<decltype(&add)>>("" FunctionPointer "",
                                                       "" & add "");
    printConceptCheck<MemberFunctionPointer<decltype(&TestClass::multiply)>>(
        "" MemberFunctionPointer "", "" & TestClass::multiply "");
    printConceptCheck<Callable<Functor>>("" Callable "", "" Functor "");
    printConceptCheck<CallableReturns<Functor, int, int, int>>(
        "" CallableReturns < int > "", "" Functor "");
    printConceptCheck<CallableNoexcept<NoexceptFunctor, int, int>>(
        "" CallableNoexcept "", "" NoexceptFunctor "");
}

// -----------------------------------------------------------------------------
// Object Concept Examples
// -----------------------------------------------------------------------------

void testObjectConcepts() {
    printSection("" Object Concepts "");

    printConceptCheck<Relocatable<RelocatableClass>>("" Relocatable "",
                                                     "" RelocatableClass "");
    printConceptCheck<DefaultConstructible<TestClass>>(
        "" DefaultConstructible "", "" TestClass "");
    printConceptCheck<CopyConstructible<std::string>>("" CopyConstructible "",
                                                      "" std::string "");
    printConceptCheck<CopyAssignable<std::string>>("" CopyAssignable "",
                                                   "" std::string "");
    printConceptCheck<MoveConstructible<std::unique_ptr<int>>>(
        "" MoveConstructible "", "" std::unique_ptr < int > "");
    printConceptCheck<MoveAssignable<std::unique_ptr<int>>>(
        "" MoveAssignable "", "" std::unique_ptr < int > "");
    printConceptCheck<EqualityComparable<ComparableClass>>(
        "" EqualityComparable "", "" ComparableClass "");
    printConceptCheck<LessThanComparable<ComparableClass>>(
        "" LessThanComparable "", "" ComparableClass "");
    printConceptCheck<Hashable<HashableClass>>("" Hashable "",
                                               "" HashableClass "");
    printConceptCheck<Swappable<std::string>>("" Swappable "",
                                              "" std::string "");
    printConceptCheck<Copyable<std::string>>("" Copyable "", "" std::string "");
    printConceptCheck<Destructible<std::string>>("" Destructible "",
                                                 "" std::string "");
}

// -----------------------------------------------------------------------------
// Type Concept Examples
// -----------------------------------------------------------------------------

void testTypeConcepts() {
    printSection("" Type Concepts "");

    printConceptCheck<Arithmetic<int>>("" Arithmetic "", "" int "");
    printConceptCheck<Integral<int>>("" Integral "", "" int "");
    printConceptCheck<FloatingPoint<double>>("" FloatingPoint "", "" double "");
    printConceptCheck<SignedInteger<int>>("" SignedInteger "", "" int "");
    printConceptCheck<UnsignedInteger<unsigned int>>("" UnsignedInteger "",
                                                     "" unsigned int "");
    printConceptCheck<Number<float>>("" Number "", "" float "");
    printConceptCheck<ComplexNumber<std::complex<double>>>(
        "" ComplexNumber "", "" std::complex < double > "");
    printConceptCheck<Char<char>>("" Char "", "" char "");
    printConceptCheck<WChar<wchar_t>>("" WChar "", "" wchar_t "");
    printConceptCheck<AnyChar<char16_t>>("" AnyChar "", "" char16_t "");
    printConceptCheck<StringType<std::string>>("" StringType "",
                                               "" std::string "");
    printConceptCheck<IsBuiltIn<int>>("" IsBuiltIn "", "" int "");
    printConceptCheck<Enum<Color>>("" Enum "", "" Color "");
    printConceptCheck<Pointer<int*>>("" Pointer "", "" int * "");
    printConceptCheck<UniquePointer<std::unique_ptr<int>>>(
        "" UniquePointer "", "" std::unique_ptr < int > "");
    printConceptCheck<SharedPointer<std::shared_ptr<int>>>(
        "" SharedPointer "", "" std::shared_ptr < int > "");
    printConceptCheck<WeakPointer<std::weak_ptr<int>>>(
        "" WeakPointer "", "" std::weak_ptr < int > "");
    printConceptCheck<SmartPointer<std::unique_ptr<int>>>(
        "" SmartPointer "", "" std::unique_ptr < int > "");
    printConceptCheck<Reference<int&>>("" Reference "", "" int & "");
    printConceptCheck<LvalueReference<int&>>("" LvalueReference "",
                                             "" int & "");
    printConceptCheck<RvalueReference<int&&>>("" RvalueReference "",
                                              "" int && "");
    printConceptCheck<Const<const int>>("" Const "", "" const int "");
    printConceptCheck<Trivial<int>>("" Trivial "", "" int "");
    printConceptCheck<TriviallyConstructible<int>>("" TriviallyConstructible "",
                                                   "" int "");
    printConceptCheck<TriviallyCopyable<int>>("" TriviallyCopyable "",
                                              "" int "");
}

// -----------------------------------------------------------------------------
// Container Concept Examples
// -----------------------------------------------------------------------------

void testContainerConcepts() {
    printSection("" Container Concepts "");

    printConceptCheck<Iterable<std::vector<int>>>("" Iterable "",
                                                  "" std::vector < int > "");
    printConceptCheck<Container<std::vector<int>>>("" Container "",
                                                   "" std::vector < int > "");
    printConceptCheck<StringContainer<std::string>>("" StringContainer "",
                                                    "" std::string "");
    printConceptCheck<NumberContainer<std::vector<int>>>(
        "" NumberContainer "", "" std::vector < int > "");
    printConceptCheck<AssociativeContainer<std::map<int, std::string>>>(
        "" AssociativeContainer "", "" std::map < int, std::string > "");
    printConceptCheck<Iterator<std::vector<int>::iterator>>(
        "" Iterator "", "" std::vector<int>::iterator "");
    printConceptCheck<SequenceContainer<std::vector<int>>>(
        "" SequenceContainer "", "" std::vector < int > "");
    printConceptCheck<SequenceContainer<std::list<int>>>(
        "" SequenceContainer "", "" std::list < int > "");
    printConceptCheck<SequenceContainer<std::deque<int>>>(
        "" SequenceContainer "", "" std::deque < int > "");
}

// -----------------------------------------------------------------------------
// Multi-threading Concept Examples
// -----------------------------------------------------------------------------

void testMultiThreadingConcepts() {
    printSection("" Multi - threading Concepts "");

    printConceptCheck<Lockable<std::mutex>>("" Lockable "", "" std::mutex "");
    printConceptCheck<SharedLockable<std::shared_mutex>>(
        "" SharedLockable "", "" std::shared_mutex "");
    printConceptCheck<Mutex<std::mutex>>("" Mutex "", "" std::mutex "");
    printConceptCheck<SharedMutex<std::shared_mutex>>("" SharedMutex "",
                                                      "" std::shared_mutex "");
}

// -----------------------------------------------------------------------------
// Asynchronous Concept Examples
// -----------------------------------------------------------------------------

void testAsynchronousConcepts() {
    printSection("" Asynchronous Concepts "");

    printConceptCheck<Future<std::future<int>>>("" Future "",
                                                "" std::future < int > "");
    printConceptCheck<Promise<std::promise<int>>>("" Promise "",
                                                  "" std::promise < int > "");
    printConceptCheck<AsyncResult<std::future<int>>>("" AsyncResult "",
                                                     "" std::future < int > "");
}

// -----------------------------------------------------------------------------
// C++23 Enhanced Concept Examples (NEW)
// -----------------------------------------------------------------------------

void testCpp23EnhancedConcepts() {
    printSection("" C++ 23 Enhanced Concepts "");

    // Formatting and Serialization
    printConceptCheck<Formattable<int>>("" Formattable "", "" int "");
    printConceptCheck<Formattable<std::string>>("" Formattable "",
                                                "" std::string "");
    printConceptCheck<HasToString<WithToString>>("" HasToString "",
                                                 "" WithToString "");
    printConceptCheck<StringViewConvertible<std::string>>(
        "" StringViewConvertible "", "" std::string "");
    printConceptCheck<JsonSerializable<WithToJson>>("" JsonSerializable "",
                                                    "" WithToJson "");

    // Range Concepts
    printConceptCheck<SpanCompatible<std::vector<int>>>(
        "" SpanCompatible "", "" std::vector < int > "");
    printConceptCheck<ContiguousRange<std::vector<int>>>(
        "" ContiguousRange "", "" std::vector < int > "");
    printConceptCheck<ContiguousRange<std::list<int>>>("" ContiguousRange "",
                                                       "" std::list < int > "");
    printConceptCheck<SizedRange<std::vector<int>>>("" SizedRange "",
                                                    "" std::vector < int > "");
    printConceptCheck<ViewableRange<std::vector<int>>>(
        "" ViewableRange "", "" std::vector < int > "");

    // Memory Concepts
    printConceptCheck<TriviallyRelocatable<int>>("" TriviallyRelocatable "",
                                                 "" int "");
    printConceptCheck<Aggregate<AggregateStruct>>("" Aggregate "",
                                                  "" AggregateStruct "");
    printConceptCheck<StandardLayout<int>>("" StandardLayout "", "" int "");
    printConceptCheck<PodType<int>>("" PodType "", "" int "");
}

// -----------------------------------------------------------------------------
// Type Relationship Concept Examples (NEW)
// -----------------------------------------------------------------------------

void testTypeRelationshipConcepts() {
    printSection("" Type Relationship Concepts "");

    printConceptCheck<HasVirtualDestructor<PolymorphicBase>>(
        "" HasVirtualDestructor "", "" PolymorphicBase "");
    printConceptCheck<PolymorphicType<PolymorphicBase>>("" PolymorphicType "",
                                                        "" PolymorphicBase "");
    printConceptCheck<FinalClass<FinalClass>>("" FinalClass "",
                                              "" FinalClass "");
    printConceptCheck<AbstractClass<PolymorphicBase>>("" AbstractClass "",
                                                      "" PolymorphicBase "");
    printConceptCheck<EnumType<Color>>("" EnumType "", "" Color "");
    printConceptCheck<ScopedEnumType<ScopedColor>>("" ScopedEnumType "",
                                                   "" ScopedColor "");
    printConceptCheck<UnscopedEnumType<Color>>("" UnscopedEnumType "",
                                               "" Color "");
}

// -----------------------------------------------------------------------------
// Enhanced Numeric Concept Examples (NEW)
// -----------------------------------------------------------------------------

void testEnhancedNumericConcepts() {
    printSection("" Enhanced Numeric Concepts "");

    printConceptCheck<SignedIntegralType<int>>("" SignedIntegralType "",
                                               "" int "");
    printConceptCheck<UnsignedIntegralType<unsigned int>>(
        "" UnsignedIntegralType "", "" unsigned int "");
    printConceptCheck<FloatingPointPrecise<double>>("" FloatingPointPrecise "",
                                                    "" double "");
    printConceptCheck<NumericArithmetic<int>>("" NumericArithmetic "",
                                              "" int "");
    printConceptCheck<BitwiseOperable<int>>("" BitwiseOperable "", "" int "");
    printConceptCheck<BitwiseOperable<double>>("" BitwiseOperable "",
                                               "" double "");
}

// -----------------------------------------------------------------------------
// Optional/Expected Concept Examples (NEW)
// -----------------------------------------------------------------------------

void testOptionalExpectedConcepts() {
    printSection("" Optional / Expected Concepts "");

    printConceptCheck<OptionalLike<std::optional<int>>>(
        "" OptionalLike "", "" std::optional < int > "");
}

// -----------------------------------------------------------------------------
// Tuple and Variant Concept Examples (NEW)
// -----------------------------------------------------------------------------

void testTupleVariantConcepts() {
    printSection("" Tuple and Variant Concepts "");

    printConceptCheck<TupleLikeType<std::tuple<int, double>>>(
        "" TupleLikeType "", "" std::tuple < int, double > "");
    printConceptCheck<TupleLikeType<std::pair<int, double>>>(
        "" TupleLikeType "", "" std::pair < int, double > "");
    printConceptCheck<TupleLikeType<std::array<int, 5>>>(
        "" TupleLikeType "", "" std::array < int, 5 > "");
    printConceptCheck<VariantLike<std::variant<int, double, std::string>>>(
        "" VariantLike "", "" std::variant < ... > "");
}

// -----------------------------------------------------------------------------
// Meta Module Interoperability Concept Examples (NEW)
// -----------------------------------------------------------------------------

void testMetaModuleConcepts() {
    printSection("" Meta Module Interoperability Concepts "");

    printConceptCheck<Demanglable<int>>("" Demanglable "", "" int "");
    printConceptCheck<TypeInfoSupported<std::string>>("" TypeInfoSupported "",
                                                      "" std::string "");
    printConceptCheck<BoxCompatible<int>>("" BoxCompatible "", "" int "");
    printConceptCheck<BoxCompatible<std::unique_ptr<int>>>(
        "" BoxCompatible "", "" std::unique_ptr < int > "");

    auto lambda = []() {};
    printConceptCheck<ProxyCompatible<decltype(lambda)>>("" ProxyCompatible "",
                                                         "" lambda "");
    printConceptCheck<DecoratorCompatible<decltype(lambda)>>(
        "" DecoratorCompatible "", "" lambda "");
    printConceptCheck<ReflectionCompatible<AggregateStruct>>(
        "" ReflectionCompatible "", "" AggregateStruct "");
    printConceptCheck<EnumWithTraits<Color>>("" EnumWithTraits "", "" Color "");

    auto intFunc = []() { return 42; };
    auto voidFunc = []() {};
    printConceptCheck<InvokableWithResult<decltype(intFunc)>>(
        "" InvokableWithResult "", "" intFunc "");
    printConceptCheck<VoidInvokable<decltype(voidFunc)>>("" VoidInvokable "",
                                                         "" voidFunc "");

    auto noexceptFunc = []() noexcept {};
    printConceptCheck<NothrowInvokable<decltype(noexceptFunc)>>(
        "" NothrowInvokable "", "" noexceptFunc "");

    printConceptCheck<MetaComparable<int>>("" MetaComparable "", "" int "");
    printConceptCheck<MetaHashable<int>>("" MetaHashable "", "" int "");
    printConceptCheck<RegistryCompatible<int>>("" RegistryCompatible "",
                                               "" int "");
    printConceptCheck<FactoryCreatable<TestClass>>("" FactoryCreatable "",
                                                   "" TestClass "");
    printConceptCheck<Cloneable<CloneableClass>>("" Cloneable "",
                                                 "" CloneableClass "");
}

// -----------------------------------------------------------------------------
// Advanced Container Concept Examples (NEW)
// -----------------------------------------------------------------------------

void testAdvancedContainerConcepts() {
    printSection("" Advanced Container Concepts "");

    printConceptCheck<Subscriptable<std::vector<int>>>(
        "" Subscriptable "", "" std::vector < int > "");
    printConceptCheck<Reservable<std::vector<int>>>("" Reservable "",
                                                    "" std::vector < int > "");
    printConceptCheck<Reservable<std::list<int>>>("" Reservable "",
                                                  "" std::list < int > "");
    printConceptCheck<AssociativeLookup<std::map<int, int>>>(
        "" AssociativeLookup "", "" std::map < int, int > "");
    printConceptCheck<OrderedContainer<std::set<int>>>("" OrderedContainer "",
                                                       "" std::set < int > "");

    printConceptCheck<Duration<std::chrono::seconds>>(
        "" Duration "", "" std::chrono::seconds "");
    printConceptCheck<TimePoint<std::chrono::system_clock::time_point>>(
        "" TimePoint "", "" time_point "");

    printConceptCheck<HasSize<std::vector<int>>>("" HasSize "",
                                                 "" std::vector < int > "");
    printConceptCheck<EmptyCheckable<std::vector<int>>>(
        "" EmptyCheckable "", "" std::vector < int > "");
    printConceptCheck<Clearable<std::vector<int>>>("" Clearable "",
                                                   "" std::vector < int > "");
    printConceptCheck<BackInsertable<std::vector<int>>>(
        "" BackInsertable "", "" std::vector < int > "");
    printConceptCheck<BackEmplaceable<std::vector<int>>>(
        "" BackEmplaceable "", "" std::vector < int > "");
    printConceptCheck<FrontBackAccessible<std::vector<int>>>(
        "" FrontBackAccessible "", "" std::vector < int > "");
}

// -----------------------------------------------------------------------------
// Thread Safety and Atomic Concept Examples (NEW)
// -----------------------------------------------------------------------------

void testThreadSafetyAtomicConcepts() {
    printSection("" Thread Safety and Atomic Concepts "");

    printConceptCheck<Nullable<std::optional<int>>>(
        "" Nullable "", "" std::optional < int > "");
    printConceptCheck<Nullable<std::unique_ptr<int>>>(
        "" Nullable "", "" std::unique_ptr < int > "");
    printConceptCheck<ThreadSafe<std::mutex>>("" ThreadSafe "",
                                              "" std::mutex "");
    printConceptCheck<AtomicLike<std::atomic<int>>>("" AtomicLike "",
                                                    "" std::atomic < int > "");
    printConceptCheck<MoveOnly<std::unique_ptr<int>>>(
        "" MoveOnly "", "" std::unique_ptr < int > "");
    printConceptCheck<Regular<int>>("" Regular "", "" int "");
    printConceptCheck<Semiregular<int>>("" Semiregular "", "" int "");
}

// -----------------------------------------------------------------------------
// Type Constraint Helper Examples (NEW)
// -----------------------------------------------------------------------------

void testTypeConstraintHelpers() {
    printSection("" Type Constraint Helpers "");

    std::cout << "" all_satisfy_concept_v<Integral, int, long, short> : ""
              << (all_satisfy_concept_v<Integral, int, long, short>
                      ? "" true ""
                      : "" false "")
              << std::endl;
    std::cout << "" all_satisfy_concept_v<Integral, int, double, short> : ""
              << (all_satisfy_concept_v<Integral, int, double, short>
                      ? "" true ""
                      : "" false "")
              << std::endl;

    std::cout << "" any_satisfy_concept_v<Integral, int, double, std::string>
        : ""
              << (any_satisfy_concept_v<Integral, int, double, std::string>
                      ? "" true ""
                      : "" false "")
              << std::endl;
    std::cout << "" any_satisfy_concept_v<Integral, double, float, std::string>
        : ""
              << (any_satisfy_concept_v<Integral, double, float, std::string>
                      ? "" true ""
                      : "" false "")
              << std::endl;

    std::cout << "" count_satisfying_v<Integral, int, double, long, std::string>
        : ""
              << count_satisfying_v<Integral, int, double, long,
                                    std::string> << std::endl;

    std::cout << "" is_one_of_v<int, char, int, double>
        : "" << (is_one_of_v<int, char, int, double> ? "" true "" : "" false "")
              << std::endl;
    std::cout << "" is_one_of_v<float, char, int, double> : ""
              << (is_one_of_v<float, char, int, double> ? "" true ""
                                                        : "" false "")
              << std::endl;

    std::cout << "" first_type_t<int, double, char> is int : ""
              << (std::is_same_v<first_type_t<int, double, char>, int>
                      ? "" true ""
                      : "" false "")
              << std::endl;
    std::cout << "" last_type_t<int, double, char> is char : ""
              << (std::is_same_v<last_type_t<int, double, char>, char>
                      ? "" true ""
                      : "" false "")
              << std::endl;
}

// -----------------------------------------------------------------------------
// Practical Usage Examples
// -----------------------------------------------------------------------------

// Generic function that works only on arithmetic typestemplate <typename T>
requires Arithmetic<T> T average(const std::vector<T>& values) {
    if (values.empty())
        return T{};
    return std::accumulate(values.begin(), values.end(), T{}) /
           static_cast<T>(values.size());
}

// Function that requires a containertemplate <Container T>
auto findMax(const T& container) {
    if (container.begin() == container.end()) {
        throw std::runtime_error("" Empty container "");
    }
    return *std::max_element(container.begin(), container.end());
}

// Function that works only on smart pointerstemplate <typename T>
requires SmartPointer<T> void useResource(T ptr) {
    std::cout << "" Resource is "" << (ptr ? "" valid "" : "" invalid "")
              << std::endl;
}

void testPracticalExamples() {
    printSection("" Practical Usage Examples "");

    std::vector<int> intValues = {1, 2, 3, 4, 5};
    std::vector<double> doubleValues = {1.5, 2.5, 3.5};

    std::cout << "" Int average : "" << average(intValues) << std::endl;
    std::cout << "" Double average : "" << average(doubleValues) << std::endl;
    std::cout << "" Max int value : "" << findMax(intValues) << std::endl;
    std::cout << "" Max double value : "" << findMax(doubleValues) << std::endl;

    std::cout << ""\n Smart pointer usage : "" << std::endl;
    auto uniquePtr = std::make_unique<int>(42);
    auto sharedPtr = std::make_shared<int>(100);

    useResource(std::move(uniquePtr));
    useResource(sharedPtr);
}

// Main functionint main() {
std::cout << std::string(60, '=') << std::endl;
std::cout << "" Concept Utilities Comprehensive Examples "" << std::endl;
std::cout << ""(Including C++ 23 Enhanced Concepts) "" << std::endl;
std::cout << std::string(60, '=') << std::endl;

testFunctionConcepts();
testObjectConcepts();
testTypeConcepts();
testContainerConcepts();
testMultiThreadingConcepts();
testAsynchronousConcepts();
testCpp23EnhancedConcepts();
testTypeRelationshipConcepts();
testEnhancedNumericConcepts();
testOptionalExpectedConcepts();
testTupleVariantConcepts();
testMetaModuleConcepts();
testAdvancedContainerConcepts();
testThreadSafetyAtomicConcepts();
testTypeConstraintHelpers();
testPracticalExamples();

std::cout << ""\n "" << std::string(60, '=') << std::endl;
std::cout << "" All examples completed successfully !"" << std::endl;
std::cout << std::string(60, '=') << std::endl;

return 0;
}
