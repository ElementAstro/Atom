#include <gtest/gtest.h>
#include "atom/meta/refl.hpp"

#include <string>
#include <type_traits>

namespace {

// Test fixture for reflection tests
class ReflTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Helper types for testing
struct TestStruct {
    int value;
    std::string name;
    double data;
};

struct BaseStruct {
    int base_value;
};

struct DerivedStruct : public BaseStruct {
    std::string derived_name;
};

// Test TStr template string system
TEST_F(ReflTest, TStrBasics) {
    using namespace atom::meta;

    // Test TStr creation and basic operations
    constexpr auto str1 = TStr<'h', 'e', 'l', 'l', 'o'>{};
    constexpr auto str2 = TStr<'w', 'o', 'r', 'l', 'd'>{};

    // Test size
    static_assert(str1.size() == 5);
    static_assert(str2.size() == 5);

    // Test string conversion
    EXPECT_EQ(str1.str(), "hello");
    EXPECT_EQ(str2.str(), "world");

    // Test c_str
    EXPECT_STREQ(str1.c_str(), "hello");
    EXPECT_STREQ(str2.c_str(), "world");
}

// Test TStr concatenation
TEST_F(ReflTest, TStrConcatenation) {
    using namespace atom::meta;

    constexpr auto hello = TStr<'h', 'e', 'l', 'l', 'o'>{};
    constexpr auto space = TStr<' '>{};
    constexpr auto world = TStr<'w', 'o', 'r', 'l', 'd'>{};

    // Test concatenation
    constexpr auto combined = hello + space + world;
    static_assert(combined.size() == 11);

    EXPECT_EQ(combined.str(), "hello world");
}

// Test TStr comparison
TEST_F(ReflTest, TStrComparison) {
    using namespace atom::meta;

    constexpr auto str1 = TStr<'t', 'e', 's', 't'>{};
    constexpr auto str2 = TStr<'t', 'e', 's', 't'>{};
    constexpr auto str3 = TStr<'o', 't', 'h', 'e', 'r'>{};

    // Test equality
    static_assert(str1 == str2);
    static_assert(!(str1 == str3));

    // Test inequality
    static_assert(!(str1 != str2));
    static_assert(str1 != str3);
}

// Test TStr platform-specific implementations
TEST_F(ReflTest, TStrPlatformSpecific) {
    using namespace atom::meta;

    // Test that TStr works with different character types
    constexpr auto ascii_str = TStr<'A', 'S', 'C', 'I', 'I'>{};
    EXPECT_EQ(ascii_str.str(), "ASCII");

    // Test empty string
    constexpr auto empty_str = TStr<>{};
    static_assert(empty_str.size() == 0);
    EXPECT_EQ(empty_str.str(), "");
}

// Test ElemList template operations
TEST_F(ReflTest, ElemListOperations) {
    using namespace atom::meta;

    // Test basic ElemList operations
    using TestList = ElemList<int, double, std::string>;

    // Test size
    static_assert(TestList::size() == 3);

    // Test type access (if available)
    static_assert(std::is_same_v<TestList::template at<0>, int>);
    static_assert(std::is_same_v<TestList::template at<1>, double>);
    static_assert(std::is_same_v<TestList::template at<2>, std::string>);
}

// Test ElemList Find operation
TEST_F(ReflTest, ElemListFind) {
    using namespace atom::meta;

    using TestList = ElemList<int, double, std::string, int>;

    // Test Find operation
    static_assert(TestList::template Find<int>() == 0);  // First occurrence
    static_assert(TestList::template Find<double>() == 1);
    static_assert(TestList::template Find<std::string>() == 2);

    // Test Contains operation
    static_assert(TestList::template Contains<int>());
    static_assert(TestList::template Contains<double>());
    static_assert(TestList::template Contains<std::string>());
    static_assert(!TestList::template Contains<char>());
}

// Test ElemList Push operations
TEST_F(ReflTest, ElemListPush) {
    using namespace atom::meta;

    using OriginalList = ElemList<int, double>;
    using PushedList = OriginalList::template Push<std::string>;

    // Test that Push adds element to the end
    static_assert(PushedList::size() == 3);
    static_assert(std::is_same_v<PushedList::template at<0>, int>);
    static_assert(std::is_same_v<PushedList::template at<1>, double>);
    static_assert(std::is_same_v<PushedList::template at<2>, std::string>);
}

// Test ElemList Insert operations
TEST_F(ReflTest, ElemListInsert) {
    using namespace atom::meta;

    using OriginalList = ElemList<int, std::string>;
    using InsertedList = OriginalList::template Insert<1, double>;

    // Test that Insert adds element at specified position
    static_assert(InsertedList::size() == 3);
    static_assert(std::is_same_v<InsertedList::template at<0>, int>);
    static_assert(std::is_same_v<InsertedList::template at<1>, double>);
    static_assert(std::is_same_v<InsertedList::template at<2>, std::string>);
}

// Test FieldList operations
TEST_F(ReflTest, FieldListOperations) {
    using namespace atom::meta;

    // Create field list for TestStruct
    using TestFieldList = FieldList<
        Field<TStr<'v', 'a', 'l', 'u', 'e'>, int>,
        Field<TStr<'n', 'a', 'm', 'e'>, std::string>,
        Field<TStr<'d', 'a', 't', 'a'>, double>
    >;

    // Test field list size
    static_assert(TestFieldList::size() == 3);

    // Test field access
    using FirstField = TestFieldList::template at<0>;
    static_assert(std::is_same_v<typename FirstField::Type, int>);

    using SecondField = TestFieldList::template at<1>;
    static_assert(std::is_same_v<typename SecondField::Type, std::string>);

    using ThirdField = TestFieldList::template at<2>;
    static_assert(std::is_same_v<typename ThirdField::Type, double>);
}

// Test AttrList operations
TEST_F(ReflTest, AttrListOperations) {
    using namespace atom::meta;

    // Create attribute list
    using TestAttrList = AttrList<
        Attr<TStr<'s', 'e', 'r', 'i', 'a', 'l', 'i', 'z', 'a', 'b', 'l', 'e'>, bool>,
        Attr<TStr<'v', 'e', 'r', 's', 'i', 'o', 'n'>, int>
    >;

    // Test attribute list size
    static_assert(TestAttrList::size() == 2);

    // Test attribute access
    using FirstAttr = TestAttrList::template at<0>;
    static_assert(std::is_same_v<typename FirstAttr::Type, bool>);

    using SecondAttr = TestAttrList::template at<1>;
    static_assert(std::is_same_v<typename SecondAttr::Type, int>);
}

// Test BaseList operations for inheritance
TEST_F(ReflTest, BaseListOperations) {
    using namespace atom::meta;

    // Create base list for inheritance hierarchy
    using TestBaseList = BaseList<BaseStruct>;

    // Test base list size
    static_assert(TestBaseList::size() == 1);

    // Test base access
    using FirstBase = TestBaseList::template at<0>;
    static_assert(std::is_same_v<FirstBase, BaseStruct>);
}

// Test TypeInfo and TypeInfoBase
TEST_F(ReflTest, TypeInfoSystem) {
    using namespace atom::meta;

    // Test TypeInfo creation
    using TestTypeInfo = TypeInfo<
        TStr<'T', 'e', 's', 't', 'S', 't', 'r', 'u', 'c', 't'>,
        FieldList<
            Field<TStr<'v', 'a', 'l', 'u', 'e'>, int>,
            Field<TStr<'n', 'a', 'm', 'e'>, std::string>
        >,
        AttrList<>,
        BaseList<>
    >;

    // Test TypeInfo properties
    static_assert(TestTypeInfo::fields.size() == 2);
    static_assert(TestTypeInfo::attrs.size() == 0);
    static_assert(TestTypeInfo::bases.size() == 0);

    // Test name access
    EXPECT_EQ(TestTypeInfo::name.str(), "TestStruct");
}

// Test DFS traversal for inheritance
TEST_F(ReflTest, DFSTraversal) {
    using namespace atom::meta;

    // Create type info with inheritance
    using DerivedTypeInfo = TypeInfo<
        TStr<'D', 'e', 'r', 'i', 'v', 'e', 'd'>,
        FieldList<Field<TStr<'d', 'e', 'r', 'i', 'v', 'e', 'd', '_', 'n', 'a', 'm', 'e'>, std::string>>,
        AttrList<>,
        BaseList<BaseStruct>
    >;

    // Test that DFS traversal works (implementation-specific)
    static_assert(DerivedTypeInfo::bases.size() == 1);

    using BaseType = DerivedTypeInfo::bases::template at<0>;
    static_assert(std::is_same_v<BaseType, BaseStruct>);
}

// Test compile-time string manipulation
TEST_F(ReflTest, CompileTimeStringManipulation) {
    using namespace atom::meta;

    // Test string creation from literals
    constexpr auto test_str = TStr<'t', 'e', 's', 't'>{};

    // Test string operations
    EXPECT_EQ(test_str.size(), 4);
    EXPECT_EQ(test_str.str(), "test");
    EXPECT_STREQ(test_str.c_str(), "test");

    // Test string comparison
    constexpr auto same_str = TStr<'t', 'e', 's', 't'>{};
    constexpr auto diff_str = TStr<'o', 't', 'h', 'e', 'r'>{};

    static_assert(test_str == same_str);
    static_assert(test_str != diff_str);
}

// Test template metaprogramming utilities
TEST_F(ReflTest, TemplateMetaprogrammingUtilities) {
    using namespace atom::meta;

    // Test SFINAE techniques (if available)
    static_assert(std::is_same_v<int, int>);
    static_assert(!std::is_same_v<int, double>);

    // Test type trait utilities
    static_assert(std::is_integral_v<int>);
    static_assert(std::is_floating_point_v<double>);
    static_assert(std::is_class_v<TestStruct>);
}

// Test reflection macros (if available)
TEST_F(ReflTest, ReflectionMacros) {
    using namespace atom::meta;

    // Test ATOM_META_TYPEINFO macro usage (if available)
    // This would typically be used in actual type definitions

    // Test ATOM_META_FIELD macro usage (if available)
    // This would typically be used to define field metadata

    // For now, test that the basic reflection system works
    // without macros by manually creating type info

    using ManualTypeInfo = TypeInfo<
        TStr<'M', 'a', 'n', 'u', 'a', 'l'>,
        FieldList<
            Field<TStr<'f', 'i', 'e', 'l', 'd', '1'>, int>,
            Field<TStr<'f', 'i', 'e', 'l', 'd', '2'>, std::string>
        >,
        AttrList<
            Attr<TStr<'v', 'e', 'r', 's', 'i', 'o', 'n'>, int>
        >,
        BaseList<>
    >;

    static_assert(ManualTypeInfo::fields.size() == 2);
    static_assert(ManualTypeInfo::attrs.size() == 1);
    EXPECT_EQ(ManualTypeInfo::name.str(), "Manual");
}

// Test field metadata extraction
TEST_F(ReflTest, FieldMetadataExtraction) {
    using namespace atom::meta;

    using TestTypeInfo = TypeInfo<
        TStr<'T', 'e', 's', 't'>,
        FieldList<
            Field<TStr<'i', 'd'>, int>,
            Field<TStr<'n', 'a', 'm', 'e'>, std::string>,
            Field<TStr<'v', 'a', 'l', 'u', 'e'>, double>
        >,
        AttrList<>,
        BaseList<>
    >;

    // Test field count
    static_assert(TestTypeInfo::fields.size() == 3);

    // Test individual field access
    using IdField = TestTypeInfo::fields::template at<0>;
    using NameField = TestTypeInfo::fields::template at<1>;
    using ValueField = TestTypeInfo::fields::template at<2>;

    // Test field types
    static_assert(std::is_same_v<typename IdField::Type, int>);
    static_assert(std::is_same_v<typename NameField::Type, std::string>);
    static_assert(std::is_same_v<typename ValueField::Type, double>);

    // Test field names
    EXPECT_EQ(IdField::name.str(), "id");
    EXPECT_EQ(NameField::name.str(), "name");
    EXPECT_EQ(ValueField::name.str(), "value");
}

// Test attribute metadata extraction
TEST_F(ReflTest, AttributeMetadataExtraction) {
    using namespace atom::meta;

    using TestTypeInfo = TypeInfo<
        TStr<'T', 'e', 's', 't'>,
        FieldList<>,
        AttrList<
            Attr<TStr<'s', 'e', 'r', 'i', 'a', 'l', 'i', 'z', 'a', 'b', 'l', 'e'>, bool>,
            Attr<TStr<'v', 'e', 'r', 's', 'i', 'o', 'n'>, int>,
            Attr<TStr<'a', 'u', 't', 'h', 'o', 'r'>, std::string>
        >,
        BaseList<>
    >;

    // Test attribute count
    static_assert(TestTypeInfo::attrs.size() == 3);

    // Test individual attribute access
    using SerializableAttr = TestTypeInfo::attrs::template at<0>;
    using VersionAttr = TestTypeInfo::attrs::template at<1>;
    using AuthorAttr = TestTypeInfo::attrs::template at<2>;

    // Test attribute types
    static_assert(std::is_same_v<typename SerializableAttr::Type, bool>);
    static_assert(std::is_same_v<typename VersionAttr::Type, int>);
    static_assert(std::is_same_v<typename AuthorAttr::Type, std::string>);

    // Test attribute names
    EXPECT_EQ(SerializableAttr::name.str(), "serializable");
    EXPECT_EQ(VersionAttr::name.str(), "version");
    EXPECT_EQ(AuthorAttr::name.str(), "author");
}

// Test inheritance hierarchy reflection
TEST_F(ReflTest, InheritanceHierarchyReflection) {
    using namespace atom::meta;

    // Base class type info
    using BaseTypeInfo = TypeInfo<
        TStr<'B', 'a', 's', 'e'>,
        FieldList<Field<TStr<'b', 'a', 's', 'e', '_', 'v', 'a', 'l', 'u', 'e'>, int>>,
        AttrList<>,
        BaseList<>
    >;

    // Derived class type info
    using DerivedTypeInfo = TypeInfo<
        TStr<'D', 'e', 'r', 'i', 'v', 'e', 'd'>,
        FieldList<Field<TStr<'d', 'e', 'r', 'i', 'v', 'e', 'd', '_', 'n', 'a', 'm', 'e'>, std::string>>,
        AttrList<>,
        BaseList<BaseStruct>
    >;

    // Test base class has no bases
    static_assert(BaseTypeInfo::bases.size() == 0);

    // Test derived class has one base
    static_assert(DerivedTypeInfo::bases.size() == 1);

    using DerivedBase = DerivedTypeInfo::bases::template at<0>;
    static_assert(std::is_same_v<DerivedBase, BaseStruct>);
}

// Test virtual base class handling
TEST_F(ReflTest, VirtualBaseClassHandling) {
    using namespace atom::meta;

    // Test with virtual inheritance (if supported)
    struct VirtualBase {
        int virtual_value;
    };

    struct VirtualDerived : virtual public VirtualBase {
        std::string derived_data;
    };

    using VirtualDerivedTypeInfo = TypeInfo<
        TStr<'V', 'i', 'r', 't', 'u', 'a', 'l', 'D', 'e', 'r', 'i', 'v', 'e', 'd'>,
        FieldList<Field<TStr<'d', 'e', 'r', 'i', 'v', 'e', 'd', '_', 'd', 'a', 't', 'a'>, std::string>>,
        AttrList<>,
        BaseList<VirtualBase>
    >;

    static_assert(VirtualDerivedTypeInfo::bases.size() == 1);

    using VirtualBaseType = VirtualDerivedTypeInfo::bases::template at<0>;
    static_assert(std::is_same_v<VirtualBaseType, VirtualBase>);
}

// Test complex type hierarchies
TEST_F(ReflTest, ComplexTypeHierarchies) {
    using namespace atom::meta;

    // Multiple inheritance scenario
    struct Interface1 {
        virtual void method1() = 0;
    };

    struct Interface2 {
        virtual void method2() = 0;
    };

    struct Implementation : public Interface1, public Interface2 {
        void method1() override {}
        void method2() override {}
        int impl_data;
    };

    using ImplementationTypeInfo = TypeInfo<
        TStr<'I', 'm', 'p', 'l', 'e', 'm', 'e', 'n', 't', 'a', 't', 'i', 'o', 'n'>,
        FieldList<Field<TStr<'i', 'm', 'p', 'l', '_', 'd', 'a', 't', 'a'>, int>>,
        AttrList<>,
        BaseList<Interface1, Interface2>
    >;

    static_assert(ImplementationTypeInfo::bases.size() == 2);

    using FirstBase = ImplementationTypeInfo::bases::template at<0>;
    using SecondBase = ImplementationTypeInfo::bases::template at<1>;

    static_assert(std::is_same_v<FirstBase, Interface1>);
    static_assert(std::is_same_v<SecondBase, Interface2>);
}

// Test integration with type_info system
TEST_F(ReflTest, TypeInfoSystemIntegration) {
    using namespace atom::meta;

    // Test that reflection system integrates with type_info
    using IntegratedTypeInfo = TypeInfo<
        TStr<'I', 'n', 't', 'e', 'g', 'r', 'a', 't', 'e', 'd'>,
        FieldList<
            Field<TStr<'i', 'd'>, int>,
            Field<TStr<'n', 'a', 'm', 'e'>, std::string>
        >,
        AttrList<
            Attr<TStr<'v', 'e', 'r', 's', 'i', 'o', 'n'>, int>
        >,
        BaseList<>
    >;

    // Test type name
    EXPECT_EQ(IntegratedTypeInfo::name.str(), "Integrated");

    // Test field integration
    static_assert(IntegratedTypeInfo::fields.size() == 2);

    using IdField = IntegratedTypeInfo::fields::template at<0>;
    using NameField = IntegratedTypeInfo::fields::template at<1>;

    EXPECT_EQ(IdField::name.str(), "id");
    EXPECT_EQ(NameField::name.str(), "name");

    // Test attribute integration
    static_assert(IntegratedTypeInfo::attrs.size() == 1);

    using VersionAttr = IntegratedTypeInfo::attrs::template at<0>;
    EXPECT_EQ(VersionAttr::name.str(), "version");
}

// Test compile-time reflection validation
TEST_F(ReflTest, CompileTimeReflectionValidation) {
    using namespace atom::meta;

    // Test that reflection information is available at compile time
    using ValidatedTypeInfo = TypeInfo<
        TStr<'V', 'a', 'l', 'i', 'd', 'a', 't', 'e', 'd'>,
        FieldList<
            Field<TStr<'f', 'i', 'e', 'l', 'd', '1'>, int>,
            Field<TStr<'f', 'i', 'e', 'l', 'd', '2'>, double>,
            Field<TStr<'f', 'i', 'e', 'l', 'd', '3'>, std::string>
        >,
        AttrList<
            Attr<TStr<'a', 't', 't', 'r', '1'>, bool>,
            Attr<TStr<'a', 't', 't', 'r', '2'>, int>
        >,
        BaseList<BaseStruct>
    >;

    // All these checks happen at compile time
    static_assert(ValidatedTypeInfo::fields.size() == 3);
    static_assert(ValidatedTypeInfo::attrs.size() == 2);
    static_assert(ValidatedTypeInfo::bases.size() == 1);

    // Test field types are correct
    static_assert(std::is_same_v<typename ValidatedTypeInfo::fields::template at<0>::Type, int>);
    static_assert(std::is_same_v<typename ValidatedTypeInfo::fields::template at<1>::Type, double>);
    static_assert(std::is_same_v<typename ValidatedTypeInfo::fields::template at<2>::Type, std::string>);

    // Test attribute types are correct
    static_assert(std::is_same_v<typename ValidatedTypeInfo::attrs::template at<0>::Type, bool>);
    static_assert(std::is_same_v<typename ValidatedTypeInfo::attrs::template at<1>::Type, int>);

    // Test base type is correct
    static_assert(std::is_same_v<typename ValidatedTypeInfo::bases::template at<0>, BaseStruct>);
}

// Test edge cases and error conditions
TEST_F(ReflTest, EdgeCasesAndErrorConditions) {
    using namespace atom::meta;

    // Test empty type info
    using EmptyTypeInfo = TypeInfo<
        TStr<'E', 'm', 'p', 't', 'y'>,
        FieldList<>,
        AttrList<>,
        BaseList<>
    >;

    static_assert(EmptyTypeInfo::fields.size() == 0);
    static_assert(EmptyTypeInfo::attrs.size() == 0);
    static_assert(EmptyTypeInfo::bases.size() == 0);

    EXPECT_EQ(EmptyTypeInfo::name.str(), "Empty");

    // Test single element lists
    using SingleFieldTypeInfo = TypeInfo<
        TStr<'S', 'i', 'n', 'g', 'l', 'e'>,
        FieldList<Field<TStr<'o', 'n', 'l', 'y'>, int>>,
        AttrList<>,
        BaseList<>
    >;

    static_assert(SingleFieldTypeInfo::fields.size() == 1);

    using OnlyField = SingleFieldTypeInfo::fields::template at<0>;
    static_assert(std::is_same_v<typename OnlyField::Type, int>);
    EXPECT_EQ(OnlyField::name.str(), "only");
}

}  // namespace
