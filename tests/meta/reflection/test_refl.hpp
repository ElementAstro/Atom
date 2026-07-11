#include <gtest/gtest.h>
#include "atom/meta/refl.hpp"

#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// Reflected test types. They live at namespace scope (not inside the
// anonymous namespace) so the TypeInfo specializations below land in the
// real ::atom::meta namespace.
namespace refl_test {

struct Point {
    float x;
    float y;
};

struct BaseStruct {
    int base_value;
};

struct DerivedStruct : BaseStruct {
    int derived_value;
};

struct VirtualBase {
    int virtual_value;
};

struct VirtualDerived : virtual VirtualBase {
    int derived_value;
};

struct Tagged {
    int id;
};

enum class Color { Red = 1, Green = 2 };

// Intentionally left without reflection metadata.
struct Unreflected {
    int value;
};

}  // namespace refl_test

ATOM_META_TYPEINFO(refl_test::Point, ATOM_META_FIELD("x", &refl_test::Point::x),
                   ATOM_META_FIELD("y", &refl_test::Point::y))

ATOM_META_TYPEINFO(refl_test::BaseStruct,
                   ATOM_META_FIELD("base_value",
                                   &refl_test::BaseStruct::base_value))

ATOM_META_TYPEINFO(refl_test::VirtualBase,
                   ATOM_META_FIELD("virtual_value",
                                   &refl_test::VirtualBase::virtual_value))

// Specializations with base classes / attributes are written by hand because
// ATOM_META_TYPEINFO only covers the base-less, attribute-less case.
namespace atom::meta {

template <>
struct TypeInfo<refl_test::DerivedStruct>
    : TypeInfoBase<refl_test::DerivedStruct, Base<refl_test::BaseStruct>> {
    static constexpr auto fields = FieldList(
        Field(TSTR("derived_value"), &refl_test::DerivedStruct::derived_value));
};

template <>
struct TypeInfo<refl_test::VirtualDerived>
    : TypeInfoBase<refl_test::VirtualDerived,
                   Base<refl_test::VirtualBase, true>> {
    static constexpr auto fields = FieldList(Field(
        TSTR("derived_value"), &refl_test::VirtualDerived::derived_value));
};

template <>
struct TypeInfo<refl_test::Tagged> : TypeInfoBase<refl_test::Tagged> {
    static constexpr auto fields =
        FieldList(Field(TSTR("id"), &refl_test::Tagged::id,
                        AttrList{Attr{TSTR("key")}, Attr{TSTR("version"), 2}}));
};

template <>
struct TypeInfo<refl_test::Color> : TypeInfoBase<refl_test::Color> {
    static constexpr auto fields =
        FieldList(Field(TSTR("Red"), refl_test::Color::Red),
                  Field(TSTR("Green"), refl_test::Color::Green));
};

}  // namespace atom::meta

namespace {

class ReflTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test the compile-time string produced by the TSTR macro
TEST_F(ReflTest, TStrBasics) {
    constexpr auto str = TSTR("hello");
    using Str = std::decay_t<decltype(str)>;

    static_assert(Str::Size() == 5);
    static_assert(Str::View() == "hello");
    static_assert(Str::Is(TSTR("hello")));
    static_assert(!Str::Is(TSTR("world")));

    EXPECT_EQ(Str::View(), "hello");
    EXPECT_STREQ(Str::Data(), "hello");
}

// Test NamedValue name/value semantics
TEST_F(ReflTest, NamedValueBasics) {
    using Name = std::decay_t<decltype(TSTR("answer"))>;
    constexpr atom::meta::NamedValue<Name, int> nv{42};

    static_assert(nv.has_value);
    static_assert(nv.name == "answer");

    EXPECT_TRUE(nv == 42);
    EXPECT_FALSE(nv == 43);
    EXPECT_FALSE(nv == 42.0);  // different type never compares equal

    constexpr atom::meta::NamedValue<Name, void> empty{};
    static_assert(!empty.has_value);
    EXPECT_FALSE(empty == 42);
}

// Test FieldList / ElemList operations on a reflected type
TEST_F(ReflTest, FieldListOperations) {
    using PointInfo = atom::meta::TypeInfo<refl_test::Point>;

    static_assert(PointInfo::fields.size == 2);
    static_assert(!decltype(PointInfo::fields)::empty());
    static_assert(PointInfo::fields.Contains(TSTR("x")));
    static_assert(PointInfo::fields.Contains(TSTR("y")));
    static_assert(!PointInfo::fields.Contains(TSTR("z")));

    constexpr auto& xField = PointInfo::fields.Find(TSTR("x"));
    static_assert(xField.name == "x");
    static_assert(!xField.is_static);
    static_assert(!xField.is_func);

    constexpr auto& yField = PointInfo::fields.Get<1>();
    static_assert(yField.name == "y");

    refl_test::Point p{1.0F, 2.0F};
    EXPECT_FLOAT_EQ(p.*(xField.value), 1.0F);
    EXPECT_FLOAT_EQ(p.*(yField.value), 2.0F);
}

// Test ElemList::Push and ElemList::Insert
TEST_F(ReflTest, ElemListPushAndInsert) {
    using namespace atom::meta;

    static constexpr auto list = ElemList{Attr{TSTR("one"), 1}};
    static constexpr auto pushed = list.Push(Attr{TSTR("two"), 2});
    static_assert(pushed.size == 2);
    static_assert(pushed.Contains(TSTR("two")));

    // Insert is a no-op for an element type already in the list
    static constexpr auto inserted = pushed.Insert(Attr{TSTR("two"), 2});
    static_assert(inserted.size == 2);
}

// Test field value access helpers
TEST_F(ReflTest, FieldAccess) {
    using PointInfo = atom::meta::TypeInfo<refl_test::Point>;
    using XName = std::decay_t<decltype(TSTR("x"))>;

    refl_test::Point p{1.5F, 2.5F};
    EXPECT_FLOAT_EQ(PointInfo::GetFieldValue<XName>(p), 1.5F);

    PointInfo::SetFieldValue<XName>(p, 3.5F);
    EXPECT_FLOAT_EQ(p.x, 3.5F);
}

// Test iteration over all non-static member variables, including bases
TEST_F(ReflTest, ForEachVarOf) {
    refl_test::DerivedStruct d{};
    d.base_value = 10;
    d.derived_value = 32;

    int sum = 0;
    std::size_t count = 0;
    atom::meta::TypeInfo<refl_test::DerivedStruct>::ForEachVarOf(
        d, [&](const auto& /*field*/, const auto& value) {
            sum += value;
            ++count;
        });

    EXPECT_EQ(count, 2U);
    EXPECT_EQ(sum, 42);
}

// Test iteration across a virtual inheritance hierarchy
TEST_F(ReflTest, VirtualBaseClassHandling) {
    using VDInfo = atom::meta::TypeInfo<refl_test::VirtualDerived>;

    static_assert(VDInfo::bases.size == 1);
    static_assert(VDInfo::bases.Get<0>().is_virtual);
    static_assert(VDInfo::VirtualBases().size == 1);

    refl_test::VirtualDerived vd{};
    vd.virtual_value = 5;
    vd.derived_value = 6;

    int sum = 0;
    VDInfo::ForEachVarOf(vd, [&](const auto& /*field*/, const auto& value) {
        sum += value;
    });
    EXPECT_EQ(sum, 11);
}

// Test depth-first traversal of the inheritance hierarchy
TEST_F(ReflTest, DFSTraversal) {
    std::size_t types_visited = 0;
    atom::meta::TypeInfo<refl_test::DerivedStruct>::DFS_ForEach(
        [&](auto /*type_info*/, auto /*depth*/) { ++types_visited; });
    EXPECT_EQ(types_visited, 2U);  // DerivedStruct + BaseStruct
}

// Test the HasReflection concept and the helper variable templates
TEST_F(ReflTest, ReflectionConceptAndHelpers) {
    using namespace atom::meta;

    static_assert(HasReflection<refl_test::Point>);
    static_assert(!HasReflection<refl_test::Unreflected>);

    static_assert(field_count_v<refl_test::Point> == 2);
    static_assert(
        has_field_v<refl_test::Point, std::decay_t<decltype(TSTR("x"))>>);
    static_assert(
        !has_field_v<refl_test::Point, std::decay_t<decltype(TSTR("nope"))>>);
}

// Test field-wise object comparison
TEST_F(ReflTest, EqualByFields) {
    refl_test::Point a{1.0F, 2.0F};
    refl_test::Point b{1.0F, 2.0F};
    refl_test::Point c{1.0F, 3.0F};

    EXPECT_TRUE(atom::meta::equalByFields(a, b));
    EXPECT_FALSE(atom::meta::equalByFields(a, c));
    EXPECT_TRUE(atom::meta::reflectedEqual(a, b));
    EXPECT_FALSE(atom::meta::reflectedEqual(a, c));
}

// Test field attributes and the attribute query helpers
TEST_F(ReflTest, AttributeMetadata) {
    using TaggedInfo = atom::meta::TypeInfo<refl_test::Tagged>;
    using IdName = std::decay_t<decltype(TSTR("id"))>;
    using KeyName = std::decay_t<decltype(TSTR("key"))>;
    using MissingName = std::decay_t<decltype(TSTR("missing"))>;

    constexpr auto& idField = TaggedInfo::fields.Find(TSTR("id"));
    static_assert(idField.attrs.size == 2);
    static_assert(idField.attrs.Contains(TSTR("key")));

    constexpr auto& versionAttr = idField.attrs.Find(TSTR("version"));
    static_assert(versionAttr.value == 2);

    static_assert(TaggedInfo::HasFieldAttribute<IdName, KeyName>());
    static_assert(!TaggedInfo::HasFieldAttribute<IdName, MissingName>());

    constexpr auto metadata = TaggedInfo::GetFieldMetadata<IdName>();
    static_assert(metadata.size == 2);
}

// Test enum-style reflection with static fields
TEST_F(ReflTest, EnumReflection) {
    constexpr auto& fields = atom::meta::TypeInfo<refl_test::Color>::fields;

    static_assert(fields.size == 2);
    static_assert(fields.Get<0>().is_static);

    EXPECT_EQ(fields.NameOfValue(refl_test::Color::Red), "Red");
    EXPECT_EQ(fields.ValueOfName<refl_test::Color>(std::string_view{"Green"}),
              refl_test::Color::Green);

    constexpr auto idx = fields.FindValue(refl_test::Color::Green);
    static_assert(idx == 1);
}

// Test compile-time field counting helpers on TypeInfoBase
TEST_F(ReflTest, FieldCountHelpers) {
    using PointInfo = atom::meta::TypeInfo<refl_test::Point>;

    static_assert(PointInfo::GetFieldCount() == 2);
    static_assert(PointInfo::GetNonStaticFieldCount() == 2);
    static_assert(
        atom::meta::TypeInfo<refl_test::Color>::GetNonStaticFieldCount() == 0);

    constexpr bool allNonFunc = PointInfo::ValidateFields([](const auto& f) {
        return !std::decay_t<decltype(f)>::is_func;
    });
    static_assert(allNonFunc);
}

// Test indexed iteration over member variables
TEST_F(ReflTest, ForEachVarOfWithIndex) {
    refl_test::Point p{1.0F, 2.0F};

    std::vector<std::size_t> indices;
    atom::meta::TypeInfo<refl_test::Point>::ForEachVarOfWithIndex(
        p, [&](const auto& /*field*/, const auto& /*value*/,
               std::size_t index) { indices.push_back(index); });

    EXPECT_EQ(indices, (std::vector<std::size_t>{0, 1}));
}

}  // namespace
