/*!
 * \file test_template_traits.cpp
 * \brief Unit tests for the Template Traits library
 * \author GitHub Copilot
 * \date 2023-05-28
 */

#include <gtest/gtest.h>
#include "atom/meta/template_traits.hpp"

#include <array>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace atom::meta::test {

// Helper types for testing
template <typename T>
struct SimpleTemplate {
    using value_type = T;
};

template <typename T, typename U>
struct PairTemplate {
    using first_type = T;
    using second_type = U;
};

template <typename T>
class BaseTemplate {
public:
    using some_param_type = T;
};

template <typename T>
class DerivedTemplate : public BaseTemplate<T> {
public:
    using derived_param_type = T;
};

// Thread-safe type for testing
class ThreadSafeClass {
public:
    struct is_thread_safe : std::true_type {};
};

// Custom POD type
struct TrivialPod {
    int x;
    double y;
};

// Custom non-trivial type
struct NonTrivialType {
    std::string s;
    NonTrivialType() : s("default") {}
    NonTrivialType(const NonTrivialType& other) : s(other.s) {}
    NonTrivialType(NonTrivialType&& other) noexcept : s(std::move(other.s)) {}
    virtual ~NonTrivialType() {}
};

// Test fixture
class TemplateTraitsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

//------------------------------------------------------------------------------
// Basic identity and type_list tests
//------------------------------------------------------------------------------

TEST_F(TemplateTraitsTest, IdentityBasics) {
    // Test basic identity functionality
    using IntIdentity = identity<int>;
    static_assert(std::is_same_v<IntIdentity::type, int>);
    EXPECT_FALSE(IntIdentity::has_value);

    // Test identity with values
    using IntWithValue = identity<int, 42>;
    static_assert(std::is_same_v<IntWithValue::type, int>);
    EXPECT_TRUE(IntWithValue::has_value);
    EXPECT_EQ(IntWithValue::value, 42);

    // Test with multiple values
    using MultiValueIdentity = identity<int, 1, 2, 3>;
    static_assert(std::is_same_v<MultiValueIdentity::type, int>);
    EXPECT_EQ(MultiValueIdentity::value_at<0>(), 1);
    EXPECT_EQ(MultiValueIdentity::value_at<1>(), 2);
    EXPECT_EQ(MultiValueIdentity::value_at<2>(), 3);
}

TEST_F(TemplateTraitsTest, IdentityStructuredBinding) {
    // Test structured binding support
    auto [type, val1, val2] = identity<double, 3.14, 2.71>{};
    static_assert(std::is_same_v<decltype(type), type_identity<double>>);
    EXPECT_DOUBLE_EQ(val1, 3.14);
    EXPECT_DOUBLE_EQ(val2, 2.71);

    // Test standard get interface
    auto id = identity<int, 42, 99>{};
    EXPECT_EQ(std::get<1>(id), 42);
    EXPECT_EQ(std::get<2>(id), 99);
}

TEST_F(TemplateTraitsTest, TypeListBasics) {
    // Test type_list size and access
    using MyList = type_list<int, double, char>;
    static_assert(MyList::size == 3);
    static_assert(std::is_same_v<MyList::at<0>, int>);
    static_assert(std::is_same_v<MyList::at<1>, double>);
    static_assert(std::is_same_v<MyList::at<2>, char>);

    // Test append and prepend
    using AppendedList = typename MyList::template append<float, bool>;
    static_assert(AppendedList::size == 5);
    static_assert(std::is_same_v<AppendedList::at<3>, float>);
    static_assert(std::is_same_v<AppendedList::at<4>, bool>);

    using PrependedList = typename MyList::template prepend<float, bool>;
    static_assert(PrependedList::size == 5);
    static_assert(std::is_same_v<PrependedList::at<0>, float>);
    static_assert(std::is_same_v<PrependedList::at<1>, bool>);
}

// Test transforming a type_list
template <typename T>
struct AddPointer {
    using type = T*;
};

TEST_F(TemplateTraitsTest, TypeListTransform) {
    using OriginalList = type_list<int, double, char>;
    using TransformedList =
        typename OriginalList::template transform<AddPointer>;

    static_assert(std::is_same_v<TransformedList::at<0>, int*>);
    static_assert(std::is_same_v<TransformedList::at<1>, double*>);
    static_assert(std::is_same_v<TransformedList::at<2>, char*>);
}

// Test filtering a type_list
template <typename T>
struct IsIntegral {
    static constexpr bool value = std::is_integral_v<T>;
};

TEST_F(TemplateTraitsTest, TypeListFilter) {
    using MixedList = type_list<int, double, char, float, bool, long>;
    using IntegralList = typename MixedList::template filter<IsIntegral>;

    static_assert(IntegralList::size == 4);  // int, char, bool, long
    static_assert(std::is_same_v<IntegralList::at<0>, int>);
    static_assert(std::is_same_v<IntegralList::at<1>, char>);
    static_assert(std::is_same_v<IntegralList::at<2>, bool>);
    static_assert(std::is_same_v<IntegralList::at<3>, long>);
}

//------------------------------------------------------------------------------
// Template detection and traits tests
//------------------------------------------------------------------------------

TEST_F(TemplateTraitsTest, IsTemplate) {
    // Test is_template
    EXPECT_TRUE(is_template_v<std::vector<int>>);
    EXPECT_TRUE(is_template_v<SimpleTemplate<double>>);
    EXPECT_TRUE((is_template_v<PairTemplate<int, float>>));
    EXPECT_FALSE(is_template_v<int>);
    EXPECT_FALSE(is_template_v<std::string>);  // std::string is an alias

    // Test TemplateInstantiation concept
    static_assert(TemplateInstantiation<std::vector<int>>);
    static_assert(!TemplateInstantiation<int>);
}

TEST_F(TemplateTraitsTest, TemplateTraits) {
    // Test template_traits
    using VecInt = std::vector<int>;
    using Traits = template_traits<VecInt>;

    static_assert(std::is_same_v<Traits::args_type,
                                 std::tuple<int, std::allocator<int>>>);
    static_assert(std::is_same_v<Traits::type_list_args,
                                 type_list<int, std::allocator<int>>>);
    EXPECT_EQ(Traits::arity,
              2);  // vector has two template parameters: T and Allocator

    // Test with multiple args
    using MapStrInt = std::map<std::string, int>;
    using MapTraits = template_traits<MapStrInt>;

    static_assert(
        std::is_same_v<
            MapTraits::args_type,
            std::tuple<std::string, int, std::less<std::string>,
                       std::allocator<std::pair<const std::string, int>>>>);
    static_assert(MapTraits::arity == 4);  // map has 4 template parameters
    EXPECT_TRUE(MapTraits::has_arg<std::string>);
    EXPECT_TRUE(MapTraits::has_arg<int>);
    EXPECT_FALSE(MapTraits::has_arg<double>);
}

TEST_F(TemplateTraitsTest, TemplateTraitsHelpers) {
    // Test helper aliases
    using MapStrInt = std::map<std::string, int>;

    static_assert(
        std::is_same_v<
            args_type_of<MapStrInt>,
            std::tuple<std::string, int, std::less<std::string>,
                       std::allocator<std::pair<const std::string, int>>>>);
    static_assert(
        std::is_same_v<
            type_list_of<MapStrInt>,
            type_list<std::string, int, std::less<std::string>,
                      std::allocator<std::pair<const std::string, int>>>>);
    EXPECT_EQ(template_arity_v<MapStrInt>, 4);

    // Test nth_template_arg
    static_assert(std::is_same_v<template_arg_t<0, MapStrInt>, std::string>);
    static_assert(std::is_same_v<template_arg_t<1, MapStrInt>, int>);
}

TEST_F(TemplateTraitsTest, IsSpecializationOf) {
    // Test is_specialization_of
    EXPECT_TRUE((is_specialization_of_v<std::vector, std::vector<int>>));
    EXPECT_TRUE((is_specialization_of_v<std::map, std::map<int, std::string>>));
    EXPECT_FALSE(
        (is_specialization_of_v<std::vector, std::map<int, std::string>>));
    EXPECT_FALSE((is_specialization_of_v<std::vector, int>));

    // Test SpecializationOf concept
    static_assert(SpecializationOf<std::vector, std::vector<double>>);
    static_assert(!SpecializationOf<std::vector, std::list<double>>);
}

//------------------------------------------------------------------------------
// Inheritance and derived type traits tests
//------------------------------------------------------------------------------

TEST_F(TemplateTraitsTest, DerivedFromAll) {
    class Base1 {};
    class Base2 {};
    class Derived1 : public Base1 {};
    class Derived2 : public Base1, public Base2 {};
    class Unrelated {};

    // Test is_derived_from_all
    EXPECT_TRUE((is_derived_from_all_v<Derived1, Base1>));
    EXPECT_TRUE((is_derived_from_all_v<Derived2, Base1, Base2>));
    EXPECT_FALSE((is_derived_from_all_v<Derived1, Base1, Base2>));
    EXPECT_FALSE((is_derived_from_all_v<Unrelated, Base1>));

    // Check missing bases diagnostics
    auto missing = is_derived_from_all<Derived1, Base1, Base2>::missing_bases;
    EXPECT_EQ(missing.size(), 1);

    // Test DerivedFromAll concept
    static_assert(DerivedFromAll<Derived2, Base1, Base2>);
    static_assert(!DerivedFromAll<Derived1, Base1, Base2>);
}

TEST_F(TemplateTraitsTest, DerivedFromAny) {
    class Base1 {};
    class Base2 {};
    class Derived1 : public Base1 {};
    class Unrelated {};

    // Test is_derived_from_any
    EXPECT_TRUE((is_derived_from_any_v<Derived1, Base1, Base2>));
    EXPECT_FALSE((is_derived_from_any_v<Unrelated, Base1, Base2>));

    // Test DerivedFromAny concept
    static_assert(DerivedFromAny<Derived1, Base1, Base2>);
    static_assert(!DerivedFromAny<Unrelated, Base1, Base2>);
}

TEST_F(TemplateTraitsTest, TemplateInheritance) {
    // Test is_base_of_template
    EXPECT_TRUE((is_base_of_template_v<BaseTemplate, DerivedTemplate<int>>));
    EXPECT_FALSE((is_base_of_template_v<SimpleTemplate, DerivedTemplate<int>>));

    // Test is_base_of_any_template
    EXPECT_TRUE((is_base_of_any_template_v<DerivedTemplate<int>, BaseTemplate,
                                           SimpleTemplate>));
    EXPECT_FALSE(
        (is_base_of_any_template_v<SimpleTemplate<int>, BaseTemplate>));

    // Test concepts
    static_assert(DerivedFromTemplate<BaseTemplate, DerivedTemplate<float>>);
    static_assert(DerivedFromAnyTemplate<DerivedTemplate<double>, BaseTemplate,
                                         SimpleTemplate>);
}

//------------------------------------------------------------------------------
// Enhanced template detection tests
//------------------------------------------------------------------------------

TEST_F(TemplateTraitsTest, PartialSpecialization) {
    // Test is_partial_specialization_of
    EXPECT_TRUE((is_partial_specialization_of_v<PairTemplate<int, double>,
                                                PairTemplate>));
    EXPECT_FALSE(
        (is_partial_specialization_of_v<SimpleTemplate<int>, PairTemplate>));
}

TEST_F(TemplateTraitsTest, AliasTemplate) {
    // Test is_alias_template
    using MyAlias = SimpleTemplate<int>;
    EXPECT_TRUE(is_alias_template_v<MyAlias>);

    // Test class template vs function template
    EXPECT_TRUE(ClassTemplate<SimpleTemplate<int>>);

    // Function template detection
    auto lambda = []<typename T>(T x) { return x; };
    EXPECT_FALSE(FunctionTemplate<decltype(lambda)>);  // Lambda is not a
                                                       // template instantiation
}

//------------------------------------------------------------------------------
// Type sequence and parameter pack utilities tests
//------------------------------------------------------------------------------

TEST_F(TemplateTraitsTest, CountOccurrences) {
    // Test count_occurrences
    constexpr auto count =
        count_occurrences_v<int, double, int, char, int, float>;
    EXPECT_EQ(count, 3);

    constexpr auto noMatches =
        count_occurrences_v<bool, double, int, char, float>;
    EXPECT_EQ(noMatches, 0);
}

TEST_F(TemplateTraitsTest, FindFirstIndex) {
    // Test find_first_index
    constexpr auto idx = find_first_index_v<int, double, char, int, float>;
    EXPECT_EQ(idx, 2);

    // This would trigger a static assertion:
    // constexpr auto notFound = find_first_index_v<bool, double, char, int,
    // float>;
}

TEST_F(TemplateTraitsTest, FindAllIndices) {
    // Test find_all_indices
    constexpr auto indices =
        find_all_indices_v<int, double, int, char, int, float>;
    EXPECT_EQ(indices.size(), 2);
    EXPECT_EQ(indices[0], 1);
    EXPECT_EQ(indices[1], 3);

    constexpr auto count =
        find_all_indices<int, double, int, char, int, float>::count;
    EXPECT_EQ(count, 2);
}

//------------------------------------------------------------------------------
// Type extraction and manipulation tests
//------------------------------------------------------------------------------

TEST_F(TemplateTraitsTest, ExtractReferenceWrapper) {
    // Test extract_reference_wrapper
    using RefWrapInt = std::reference_wrapper<int>;
    static_assert(
        std::is_same_v<extract_reference_wrapper_type_t<RefWrapInt>, int>);

    using ConstRefWrapInt = std::reference_wrapper<const int>;
    static_assert(
        std::is_same_v<extract_reference_wrapper_type_t<ConstRefWrapInt>,
                       const int>);

    static_assert(std::is_same_v<extract_reference_wrapper_type_t<int&>, int>);
    static_assert(std::is_same_v<extract_reference_wrapper_type_t<int>, int>);
}

TEST_F(TemplateTraitsTest, ExtractPointer) {
    // Test extract_pointer
    using PtrInfo = extract_pointer<int*>;
    static_assert(std::is_same_v<PtrInfo::type, int>);
    static_assert(std::is_same_v<PtrInfo::element_type, int>);
    EXPECT_TRUE(PtrInfo::is_pointer);
    EXPECT_FALSE(PtrInfo::is_smart_pointer);

    // Test smart pointer detection
    using SharedPtrInfo = extract_pointer<std::shared_ptr<double>>;
    static_assert(std::is_same_v<SharedPtrInfo::element_type, double>);
    EXPECT_TRUE(SharedPtrInfo::is_smart_pointer);

    using UniquePtrInfo = extract_pointer<std::unique_ptr<double>>;
    static_assert(std::is_same_v<UniquePtrInfo::element_type, double>);
    EXPECT_TRUE(UniquePtrInfo::is_smart_pointer);

    // Test helper alias
    static_assert(std::is_same_v<extract_pointer_type_t<int*>, int>);
    static_assert(
        std::is_same_v<extract_pointer_type_t<std::shared_ptr<double>>,
                       double>);
}

TEST_F(TemplateTraitsTest, ExtractFunctionTraits) {
    // Test regular function
    using RegFunc = int(double, char);
    using RegFuncTraits = extract_function_traits<RegFunc>;
    static_assert(std::is_same_v<RegFuncTraits::return_type, int>);
    static_assert(std::is_same_v<RegFuncTraits::parameter_types,
                                 std::tuple<double, char>>);
    static_assert(RegFuncTraits::arity == 2);
    static_assert(std::is_same_v<RegFuncTraits::arg_t<0>, double>);
    static_assert(std::is_same_v<RegFuncTraits::arg_t<1>, char>);

    // Test function pointer
    using FuncPtr = int (*)(double, char);
    using FuncPtrTraits = extract_function_traits<FuncPtr>;
    static_assert(std::is_same_v<FuncPtrTraits::return_type, int>);
    static_assert(FuncPtrTraits::arity == 2);

    // Test member function
    struct TestClass {
        void member(int, double) {}
        int const_member(float) const { return 0; }
    };

    using MemberFunc = void (TestClass::*)(int, double);
    using MemberFuncTraits = extract_function_traits<MemberFunc>;
    static_assert(std::is_same_v<MemberFuncTraits::class_type, TestClass>);
    static_assert(std::is_same_v<MemberFuncTraits::return_type, void>);
    static_assert(MemberFuncTraits::arity == 2);

    // Test const member function
    using ConstMemberFunc = int (TestClass::*)(float) const;
    using ConstMemberFuncTraits = extract_function_traits<ConstMemberFunc>;
    static_assert(std::is_same_v<ConstMemberFuncTraits::return_type, int>);
    static_assert(ConstMemberFuncTraits::arity == 1);

    // Test noexcept function
    using NoexceptFunc = void(int) noexcept;
    using NoexceptFuncTraits = extract_function_traits<NoexceptFunc>;
    static_assert(std::is_same_v<NoexceptFuncTraits::return_type, void>);
    EXPECT_TRUE(NoexceptFuncTraits::is_noexcept);

    // Test lambda
    auto lambda = [](int x, double y) -> char { return 'a'; };
    using LambdaTraits = extract_function_traits<decltype(lambda)>;
    static_assert(std::is_same_v<LambdaTraits::return_type, char>);
    static_assert(LambdaTraits::arity == 2);
    static_assert(std::is_same_v<LambdaTraits::arg_t<0>, int>);

    // Test helper aliases
    static_assert(
        std::is_same_v<extract_function_return_type_t<decltype(lambda)>, char>);
    static_assert(
        std::is_same_v<extract_function_parameters_t<decltype(lambda)>,
                       std::tuple<int, double>>);
}

//------------------------------------------------------------------------------
// Tuple and structured binding tests
//------------------------------------------------------------------------------

TEST_F(TemplateTraitsTest, TupleLikeTests) {
    // Test has_tuple_element concept
    static_assert(has_tuple_element<std::tuple<int, double>, 0>);
    static_assert(has_tuple_element<std::array<int, 3>, 1>);
    static_assert(!has_tuple_element<int, 0>);

    // Test TupleLike concept
    static_assert(TupleLike<std::tuple<int, char>>);
    static_assert(TupleLike<std::array<double, 5>>);
    static_assert(TupleLike<std::pair<int, float>>);
    static_assert(!TupleLike<int>);
    static_assert(!TupleLike<std::vector<int>>);

    // Test with our identity type
    static_assert(TupleLike<identity<int, 1, 2>>);
}

//------------------------------------------------------------------------------
// Advanced type constraint tests
//------------------------------------------------------------------------------

TEST_F(TemplateTraitsTest, ConstraintLevelTests) {
    // Test has_copyability
    EXPECT_TRUE(has_copyability<int>(constraint_level::trivial));
    EXPECT_TRUE(has_copyability<std::string>(constraint_level::nontrivial));
    EXPECT_FALSE(
        has_copyability<std::unique_ptr<int>>(constraint_level::nontrivial));

    // Test has_relocatability
    EXPECT_TRUE(has_relocatability<int>(constraint_level::trivial));
    EXPECT_TRUE(has_relocatability<std::string>(constraint_level::nothrow));
    EXPECT_TRUE(
        has_relocatability<std::unique_ptr<int>>(constraint_level::nothrow));

    // Test has_destructibility
    EXPECT_TRUE(has_destructibility<int>(constraint_level::trivial));
    EXPECT_TRUE(has_destructibility<std::string>(constraint_level::nothrow));

    // Test concepts
    static_assert(Copyable<int>);
    static_assert(NothrowCopyable<int>);
    static_assert(TriviallyCopyable<int>);

    static_assert(Relocatable<std::string>);
    static_assert(NothrowRelocatable<std::string>);

    static_assert(!Copyable<std::unique_ptr<int>>);
    static_assert(Relocatable<std::unique_ptr<int>>);
}

//------------------------------------------------------------------------------
// Thread safety, variants, and containers tests
//------------------------------------------------------------------------------

TEST_F(TemplateTraitsTest, ThreadSafetyTests) {
    // Test ThreadSafe concept
    static_assert(ThreadSafe<ThreadSafeClass>);
    static_assert(!ThreadSafe<int>);
    static_assert(!ThreadSafe<std::vector<int>>);
}

TEST_F(TemplateTraitsTest, VariantTraitsTests) {
    // Test variant_traits
    using MyVariant = std::variant<int, double, std::string>;

    EXPECT_TRUE(variant_traits<MyVariant>::is_variant);
    EXPECT_FALSE(variant_traits<int>::is_variant);

    EXPECT_TRUE((variant_traits<MyVariant>::contains<int>));
    EXPECT_TRUE((variant_traits<MyVariant>::contains<double>));
    EXPECT_TRUE((variant_traits<MyVariant>::contains<std::string>));
    EXPECT_FALSE((variant_traits<MyVariant>::contains<float>));

    EXPECT_EQ(variant_traits<MyVariant>::size, 3);

    static_assert(
        std::is_same_v<variant_traits<MyVariant>::alternative_t<0>, int>);
    static_assert(
        std::is_same_v<variant_traits<MyVariant>::alternative_t<1>, double>);
    static_assert(std::is_same_v<variant_traits<MyVariant>::alternative_t<2>,
                                 std::string>);
}

TEST_F(TemplateTraitsTest, ContainerTraitsTests) {
    // Test container_traits
    EXPECT_TRUE(container_traits<std::vector<int>>::is_container);
    EXPECT_TRUE(container_traits<std::list<double>>::is_container);
    EXPECT_TRUE(container_traits<std::map<int, std::string>>::is_container);
    EXPECT_FALSE(container_traits<int>::is_container);

    // Test sequence container
    EXPECT_TRUE(container_traits<std::vector<int>>::is_sequence_container);
    EXPECT_TRUE(container_traits<std::list<double>>::is_sequence_container);
    EXPECT_FALSE(
        container_traits<std::map<int, std::string>>::is_sequence_container);

    // Test associative container
    EXPECT_FALSE(container_traits<std::vector<int>>::is_associative_container);
    EXPECT_TRUE(
        container_traits<std::map<int, std::string>>::is_associative_container);
    EXPECT_TRUE(container_traits<
                std::unordered_map<int, double>>::is_associative_container);

    // Test fixed size container
    EXPECT_TRUE(container_traits<std::array<int, 10>>::is_fixed_size);
    EXPECT_FALSE(container_traits<std::vector<int>>::is_fixed_size);
}

//------------------------------------------------------------------------------
// Error reporting and static diagnostics tests
//------------------------------------------------------------------------------

TEST_F(TemplateTraitsTest, StaticDiagnosticsTests) {
    // Test static_check
    constexpr bool condition = 1 + 1 == 2;
    static_assert(static_check<condition>::value);

    // Test type_name
    std::string_view intName = type_name<int>;
    std::string_view vectorName = type_name<std::vector<double>>;
    EXPECT_EQ(intName, "int");
    EXPECT_TRUE(vectorName.find("vector") != std::string_view::npos);
    EXPECT_TRUE(vectorName.find("double") != std::string_view::npos);
}

}  // namespace atom::meta::test

// Main function
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}