// atom/meta/test_conversion.hpp
#ifndef ATOM_TEST_CONVERSION_HPP
#define ATOM_TEST_CONVERSION_HPP

#include <gtest/gtest.h>
#include "atom/meta/conversion.hpp"

#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

namespace atom::test {

// Base classes for testing
class Base {
public:
    virtual ~Base() = default;
    virtual std::string getName() const { return "Base"; }
};

class Derived : public Base {
public:
    std::string getName() const override { return "Derived"; }
};

class AnotherDerived : public Base {
public:
    std::string getName() const override { return "AnotherDerived"; }
};

// Non-polymorphic classes for static conversion
struct SimpleBase {
    int value = 10;
};

struct SimpleDerived : public SimpleBase {
    int extraValue = 20;
};

// Test fixture for conversion tests
class ConversionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test objects
        base = std::make_shared<Base>();
        derived = std::make_shared<Derived>();
        anotherDerived = std::make_shared<AnotherDerived>();

        // Initialize conversions registry
        conversions = atom::meta::TypeConversions::createShared();
    }

    std::shared_ptr<Base> base;
    std::shared_ptr<Derived> derived;
    std::shared_ptr<AnotherDerived> anotherDerived;
    std::shared_ptr<atom::meta::TypeConversions> conversions;
};

// Test static conversion
TEST_F(ConversionTest, StaticConversion) {
    // Create static conversion
    atom::meta::StaticConversion<SimpleDerived*, SimpleBase*> staticConv;

    // Test upcast
    SimpleDerived derivedObj;
    std::any derivedAny = &derivedObj;
    std::any baseAny = staticConv.convert(derivedAny);

    SimpleBase* basePtr = std::any_cast<SimpleBase*>(baseAny);
    ASSERT_NE(basePtr, nullptr);
    EXPECT_EQ(basePtr->value, 10);

    // Test downcast
    std::any downcastAny = staticConv.convertDown(baseAny);
    SimpleDerived* downcastPtr = std::any_cast<SimpleDerived*>(downcastAny);
    ASSERT_NE(downcastPtr, nullptr);
    EXPECT_EQ(downcastPtr->extraValue, 20);
}

// Test dynamic conversion
TEST_F(ConversionTest, DynamicConversion) {
    // Create dynamic conversion
    atom::meta::DynamicConversion<Derived*, Base*> dynamicConv;

    // Test upcast
    Derived derivedObj;
    std::any derivedAny = &derivedObj;
    std::any baseAny = dynamicConv.convert(derivedAny);

    Base* basePtr = std::any_cast<Base*>(baseAny);
    ASSERT_NE(basePtr, nullptr);
    EXPECT_EQ(basePtr->getName(), "Derived");

    // Test downcast
    std::any downcastAny = dynamicConv.convertDown(baseAny);
    Derived* downcastPtr = std::any_cast<Derived*>(downcastAny);
    ASSERT_NE(downcastPtr, nullptr);
}

// Test failed dynamic conversion
TEST_F(ConversionTest, FailedDynamicConversion) {
    // Create dynamic conversion
    atom::meta::DynamicConversion<AnotherDerived*, Derived*> badConv;

    // Try to convert between unrelated types
    Base baseObj;
    std::any baseAny = &baseObj;

    // This should throw because Base cannot be cast to Derived*
    EXPECT_THROW(
        { std::any derivedAny = badConv.convert(baseAny); },
        atom::meta::BadConversionException);
}

// Test vector conversion
TEST_F(ConversionTest, VectorConversion) {
    // Create vector of derived pointers
    std::vector<std::shared_ptr<Derived>> derivedVec;
    derivedVec.push_back(std::make_shared<Derived>());
    derivedVec.push_back(std::make_shared<Derived>());

    // Create vector conversion
    atom::meta::VectorConversion<std::shared_ptr<Derived>,
                                 std::shared_ptr<Base>>
        vectorConv;

    // Test convert up
    std::any derivedVecAny = derivedVec;
    std::any baseVecAny = vectorConv.convert(derivedVecAny);

    auto baseVec =
        std::any_cast<std::vector<std::shared_ptr<Base>>>(baseVecAny);
    ASSERT_EQ(baseVec.size(), 2);
    EXPECT_EQ(baseVec[0]->getName(), "Derived");
    EXPECT_EQ(baseVec[1]->getName(), "Derived");

    // Test convert down
    std::any backToDerivecVecAny = vectorConv.convertDown(baseVecAny);
    auto backToDerivecVec =
        std::any_cast<std::vector<std::shared_ptr<Derived>>>(
            backToDerivecVecAny);
    ASSERT_EQ(backToDerivecVec.size(), 2);
}

// Test sequence conversion with std::list
TEST_F(ConversionTest, SequenceConversion) {
    // Create list of derived pointers
    std::list<std::shared_ptr<Derived>> derivedList;
    derivedList.push_back(std::make_shared<Derived>());
    derivedList.push_back(std::make_shared<Derived>());

    // Create sequence conversion
    atom::meta::SequenceConversion<std::list, std::shared_ptr<Derived>,
                                   std::shared_ptr<Base>>
        seqConv;

    // Test convert up
    std::any derivedListAny = derivedList;
    std::any baseListAny = seqConv.convert(derivedListAny);

    auto baseList =
        std::any_cast<std::list<std::shared_ptr<Base>>>(baseListAny);
    ASSERT_EQ(baseList.size(), 2);
    EXPECT_EQ(baseList.front()->getName(), "Derived");

    // Test convert down
    std::any backToDerivecListAny = seqConv.convertDown(baseListAny);
    auto backToDerivecList = std::any_cast<std::list<std::shared_ptr<Derived>>>(
        backToDerivecListAny);
    ASSERT_EQ(backToDerivecList.size(), 2);
}

// Test set conversion
// Note: SetConversion<std::set, From, To> operates on std::set<From> /
// std::set<To> with the default comparator; std::any type identity is exact,
// so the stored sets must use the same instantiation.
TEST_F(ConversionTest, SetConversion) {
    // Create set of derived pointers
    std::set<std::shared_ptr<Derived>> derivedSet;
    derivedSet.insert(std::make_shared<Derived>());

    // Create set conversion
    atom::meta::SetConversion<std::set, std::shared_ptr<Derived>,
                              std::shared_ptr<Base>>
        setConv;

    // Test convert up
    std::any derivedSetAny = derivedSet;
    std::any baseSetAny = setConv.convert(derivedSetAny);

    auto baseSet =
        std::any_cast<std::set<std::shared_ptr<Base>>>(baseSetAny);
    ASSERT_EQ(baseSet.size(), 1);
    EXPECT_EQ((*baseSet.begin())->getName(), "Derived");

    // Test convert down
    std::any backToDerivecSetAny = setConv.convertDown(baseSetAny);
    auto backToDerivecSet =
        std::any_cast<std::set<std::shared_ptr<Derived>>>(
            backToDerivecSetAny);
    ASSERT_EQ(backToDerivecSet.size(), 1);
}

// Test map conversion
TEST_F(ConversionTest, MapConversion) {
    // Create map with derived pointers as values
    std::map<int, std::shared_ptr<Derived>> derivedMap;
    derivedMap[1] = std::make_shared<Derived>();
    derivedMap[2] = std::make_shared<Derived>();

    // Create map conversion
    atom::meta::MapConversion<std::map, int, std::shared_ptr<Derived>, int,
                              std::shared_ptr<Base>>
        mapConv;

    // Test convert up
    std::any derivedMapAny = derivedMap;
    std::any baseMapAny = mapConv.convert(derivedMapAny);

    auto baseMap =
        std::any_cast<std::map<int, std::shared_ptr<Base>>>(baseMapAny);
    ASSERT_EQ(baseMap.size(), 2);
    EXPECT_EQ(baseMap[1]->getName(), "Derived");
    EXPECT_EQ(baseMap[2]->getName(), "Derived");

    // Test convert down
    std::any backToDerivecMapAny = mapConv.convertDown(baseMapAny);
    auto backToDerivecMap =
        std::any_cast<std::map<int, std::shared_ptr<Derived>>>(
            backToDerivecMapAny);
    ASSERT_EQ(backToDerivecMap.size(), 2);
}

// Test TypeConversions registry
TEST_F(ConversionTest, TypeConversionsRegistry) {
    // Add base class relationship
    conversions->addBaseClass<Base, Derived>();

    // Check if conversion exists
    EXPECT_TRUE(conversions->canConvert(atom::meta::userType<Derived*>(),
                                        atom::meta::userType<Base*>()));

    // Try conversion
    Derived derivedObj;
    std::any derivedAny = &derivedObj;
    std::any baseAny = conversions->convert<Base*, Derived*>(derivedAny);

    Base* basePtr = std::any_cast<Base*>(baseAny);
    ASSERT_NE(basePtr, nullptr);
    EXPECT_EQ(basePtr->getName(), "Derived");

    // Add vector conversion
    conversions->addVectorConversion<Derived, Base>();

    // Create vector of derived pointers
    std::vector<std::shared_ptr<Derived>> derivedVec;
    derivedVec.push_back(std::make_shared<Derived>());

    // Check if conversion exists
    EXPECT_TRUE(conversions->canConvert(
        atom::meta::userType<std::vector<std::shared_ptr<Derived>>>(),
        atom::meta::userType<std::vector<std::shared_ptr<Base>>>()));
}

// Test for adding multiple conversions
TEST_F(ConversionTest, MultipleConversions) {
    // Add multiple base class relationships
    conversions->addBaseClass<Base, Derived>();
    conversions->addBaseClass<Base, AnotherDerived>();

    // Check if both conversions exist
    EXPECT_TRUE(conversions->canConvert(atom::meta::userType<Derived*>(),
                                        atom::meta::userType<Base*>()));
    EXPECT_TRUE(conversions->canConvert(atom::meta::userType<AnotherDerived*>(),
                                        atom::meta::userType<Base*>()));

    // Try both conversions
    Derived derivedObj;
    AnotherDerived anotherDerivedObj;

    std::any derivedAny = &derivedObj;
    std::any anotherDerivedAny = &anotherDerivedObj;

    std::any baseFromDerivedAny =
        conversions->convert<Base*, Derived*>(derivedAny);
    std::any baseFromAnotherDerivedAny =
        conversions->convert<Base*, AnotherDerived*>(anotherDerivedAny);

    Base* baseFromDerivedPtr = std::any_cast<Base*>(baseFromDerivedAny);
    Base* baseFromAnotherDerivedPtr =
        std::any_cast<Base*>(baseFromAnotherDerivedAny);

    ASSERT_NE(baseFromDerivedPtr, nullptr);
    ASSERT_NE(baseFromAnotherDerivedPtr, nullptr);

    EXPECT_EQ(baseFromDerivedPtr->getName(), "Derived");
    EXPECT_EQ(baseFromAnotherDerivedPtr->getName(), "AnotherDerived");
}

// Test for sequence conversions
TEST_F(ConversionTest, SequenceConversionsInRegistry) {
    // Add sequence conversions for different container types
    conversions->addSequenceConversion<std::list, Derived, Base>();
    conversions->addSequenceConversion<std::deque, Derived, Base>();

    // Check if both conversions exist
    EXPECT_TRUE(conversions->canConvert(
        atom::meta::userType<std::list<std::shared_ptr<Derived>>>(),
        atom::meta::userType<std::list<std::shared_ptr<Base>>>()));

    EXPECT_TRUE(conversions->canConvert(
        atom::meta::userType<std::deque<std::shared_ptr<Derived>>>(),
        atom::meta::userType<std::deque<std::shared_ptr<Base>>>()));
}

// Test for map conversions
TEST_F(ConversionTest, MapConversionsInRegistry) {
    // Add map conversions for different container types
    conversions->addMapConversion<std::map, int, std::shared_ptr<Derived>, int,
                                  std::shared_ptr<Base>>();
    conversions->addMapConversion<std::unordered_map, std::string,
                                  std::shared_ptr<Derived>, std::string,
                                  std::shared_ptr<Base>>();

    // Check if both conversions exist
    EXPECT_TRUE(conversions->canConvert(
        atom::meta::userType<std::map<int, std::shared_ptr<Derived>>>(),
        atom::meta::userType<std::map<int, std::shared_ptr<Base>>>()));

    EXPECT_TRUE(conversions->canConvert(
        atom::meta::userType<
            std::unordered_map<std::string, std::shared_ptr<Derived>>>(),
        atom::meta::userType<
            std::unordered_map<std::string, std::shared_ptr<Base>>>()));
}

// Test for set conversions
TEST_F(ConversionTest, SetConversionsInRegistry) {
    // Add set conversions
    conversions->addSetConversion<std::set, Derived, Base>();

    // Check if conversion exists
    EXPECT_TRUE(conversions->canConvert(
        atom::meta::userType<std::set<std::shared_ptr<Derived>>>(),
        atom::meta::userType<std::set<std::shared_ptr<Base>>>()));
}

// Test error handling
TEST_F(ConversionTest, ErrorHandling) {
    // Try to convert without adding conversion
    std::shared_ptr<Derived> derivedPtr = std::make_shared<Derived>();
    std::any derivedAny = derivedPtr;

    // This should throw because no conversion has been registered
    EXPECT_THROW(
        ({
            [[maybe_unused]] auto basePtr =
                conversions
                    ->convert<std::shared_ptr<Base>, std::shared_ptr<Derived>>(
                        derivedAny);
        }),
        atom::meta::BadConversionException);

    // Now add the conversion and try again
    conversions->addBaseClass<Base, Derived>();

    // This should not throw
    EXPECT_NO_THROW(({
        [[maybe_unused]] auto basePtr =
            conversions->convert<Base*, Derived*>(&(*derivedPtr));
    }));
}

// Test convert with invalid type
TEST_F(ConversionTest, InvalidTypeConversion) {
    // Add base class relationship
    conversions->addBaseClass<Base, Derived>();

    // Try to convert with wrong source type
    int notADerived = 42;
    std::any notADerivedAny = notADerived;

    // This should throw because int cannot be converted to Base*
    EXPECT_THROW(({
                     [[maybe_unused]] auto basePtr =
                         conversions->convert<Base*, Derived*>(notADerivedAny);
                 }),
                 atom::meta::BadConversionException);
}

// Test baseClass helper function
TEST_F(ConversionTest, BaseClassHelper) {
    // Create a conversion using the helper function
    auto conversion = atom::meta::baseClass<Base, Derived>();
    ASSERT_NE(conversion, nullptr);

    // Check if it's the right type
    EXPECT_EQ(conversion->from(), atom::meta::userType<Derived*>());
    EXPECT_EQ(conversion->to(), atom::meta::userType<Base*>());

    // Try conversion
    Derived derivedObj;
    std::any derivedAny = &derivedObj;
    std::any baseAny = conversion->convert(derivedAny);

    Base* basePtr = std::any_cast<Base*>(baseAny);
    ASSERT_NE(basePtr, nullptr);
    EXPECT_EQ(basePtr->getName(), "Derived");
}

// Test for reference conversions
TEST_F(ConversionTest, ReferenceConversions) {
    // Create static conversion for references
    atom::meta::StaticConversion<Derived&, Base&> staticRefConv;

    // Test upcast
    Derived derivedObj;
    std::any derivedRefAny = std::ref(derivedObj);
    std::any baseRefAny = staticRefConv.convert(derivedRefAny);

    Base& baseRef =
        std::any_cast<std::reference_wrapper<Base>>(baseRefAny).get();
    EXPECT_EQ(baseRef.getName(), "Derived");

    // Create dynamic conversion for references
    atom::meta::DynamicConversion<Derived&, Base&> dynamicRefConv;

    // Test with dynamic conversion
    std::any baseRefFromDynamicAny = dynamicRefConv.convert(derivedRefAny);
    Base& baseRefFromDynamic =
        std::any_cast<std::reference_wrapper<Base>>(baseRefFromDynamicAny)
            .get();
    EXPECT_EQ(baseRefFromDynamic.getName(), "Derived");
}

// ---------------------------------------------------------------------------
// Additional tests added to increase dedup source-line coverage above 90 %
// ---------------------------------------------------------------------------

// ---- bidir() method ---------------------------------------------------------
TEST_F(ConversionTest, BidirIsTrue) {
    // bidir() is not overridden in any sub-class, so calling it on a concrete
    // conversion object exercises lines 192-193.
    atom::meta::DynamicConversion<Derived*, Base*> conv;
    EXPECT_TRUE(conv.bidir());

    atom::meta::StaticConversion<SimpleDerived*, SimpleBase*> staticConv;
    EXPECT_TRUE(staticConv.bidir());
}

// ---- convertDown exception path in TypeConversionBase::convertDown ----------
// We need to reach lines 153-161 (the catch(...) block in convertDown).
// Construct a DynamicConversion where the down-cast will fail so convertDownImpl
// throws and is caught by the outer handler.
TEST_F(ConversionTest, ConvertDownExceptionPath) {
    // DynamicConversion<AnotherDerived*, Base*>: convertDown casts Base* back to
    // AnotherDerived* via dynamic_cast.  Supply a Derived* (not AnotherDerived*)
    // so the cast fails and throws BadConversionException through convertDown.
    atom::meta::DynamicConversion<AnotherDerived*, Base*> conv;

    Derived derivedObj;
    Base* basePtr = &derivedObj;
    std::any baseAny = basePtr;

    EXPECT_THROW(conv.convertDown(baseAny), atom::meta::BadConversionException);
}

// ---- anyToReferencePtr fallback (line 281) ----------------------------------
// The helper returns std::any_cast<Bare>(&value) when the any holds a T value
// rather than a reference_wrapper<T>.  StaticConversion<T&,U&> calls it; if the
// any holds a plain value object the fallback path is taken.
TEST_F(ConversionTest, AnyToReferencePtrFallback) {
    atom::meta::StaticConversion<Derived&, Base&> conv;

    // Store a plain Derived object (not a reference_wrapper) in the any.
    Derived derivedObj;
    std::any plainValueAny = derivedObj;  // stores by value, not ref_wrapper

    // convertImpl calls anyToReferencePtr which will try ref_wrapper (fails),
    // then fall through to the direct cast path (line 281).
    std::any result = conv.convert(plainValueAny);
    Base& baseRef = std::any_cast<std::reference_wrapper<Base>>(result).get();
    EXPECT_EQ(baseRef.getName(), "Derived");
}

// ---- StaticConversion reference null-ptr branch (line 304) ------------------
// anyToReferencePtr returns nullptr when the any doesn't hold the right type at
// all; that triggers THROW_CONVERSION_ERROR at line 304.
TEST_F(ConversionTest, StaticRefConversionNullFromPtr) {
    atom::meta::StaticConversion<Derived&, Base&> conv;

    // An any containing an int is not cast-able to Derived& or
    // reference_wrapper<Derived>, so anyToReferencePtr returns nullptr.
    std::any wrongAny = 42;
    EXPECT_THROW(conv.convert(wrongAny), atom::meta::BadConversionException);
}

// ---- StaticConversion else branch (non-pointer, non-reference, line 309) ----
// Instantiate with value types (neither pointer nor reference) to hit the else
// branch that always throws.
TEST_F(ConversionTest, StaticConversionElseBranchConvert) {
    // StaticConversion<int, float> – neither pointer nor reference type.
    atom::meta::StaticConversion<int, float> conv;
    std::any intAny = 42;
    EXPECT_THROW(conv.convert(intAny), atom::meta::BadConversionException);
}

TEST_F(ConversionTest, StaticConversionElseBranchConvertDown) {
    atom::meta::StaticConversion<int, float> conv;
    std::any floatAny = 3.14f;
    EXPECT_THROW(conv.convertDown(floatAny), atom::meta::BadConversionException);
}

// ---- StaticConversion convertDownImpl reference path (lines 327-338) --------
TEST_F(ConversionTest, StaticRefConversionDownSuccess) {
    // convertDownImpl reference path: successful round-trip through reference
    // conversion.  Convert down from Base& back to Derived& (static cast).
    atom::meta::StaticConversion<Derived&, Base&> conv;

    Derived derivedObj;
    // First convert up to get a Base& wrapped any.
    std::any derivedRefAny = std::ref(derivedObj);
    std::any baseRefAny = conv.convert(derivedRefAny);

    // Now convert back down.
    std::any backAny = conv.convertDown(baseRefAny);
    Derived& backRef =
        std::any_cast<std::reference_wrapper<Derived>>(backAny).get();
    EXPECT_EQ(backRef.getName(), "Derived");
}

TEST_F(ConversionTest, StaticRefConversionDownNullPtr) {
    atom::meta::StaticConversion<Derived&, Base&> conv;

    // An any with wrong type causes anyToReferencePtr to return nullptr in
    // convertDownImpl (line 327-329 null branch → bad_cast → throw).
    std::any wrongAny = 42;
    EXPECT_THROW(conv.convertDown(wrongAny), atom::meta::BadConversionException);
}

// ---- DynamicConversion reference down-cast success (lines 403-416) ----------
TEST_F(ConversionTest, DynamicRefConversionDownSuccess) {
    atom::meta::DynamicConversion<Derived&, Base&> conv;

    Derived derivedObj;
    std::any derivedRefAny = std::ref(derivedObj);
    std::any baseRefAny = conv.convert(derivedRefAny);

    // convertDownImpl reference path: dynamic_cast<Derived&>(Base&) succeeds.
    std::any backAny = conv.convertDown(baseRefAny);
    Derived& backRef =
        std::any_cast<std::reference_wrapper<Derived>>(backAny).get();
    EXPECT_EQ(backRef.getName(), "Derived");
}

TEST_F(ConversionTest, DynamicRefConversionDownNullPtr) {
    // anyToReferencePtr returns nullptr → throw (line 407-409) → THROW_CONVERSION_ERROR
    atom::meta::DynamicConversion<Derived&, Base&> conv;
    std::any wrongAny = 42;
    EXPECT_THROW(conv.convertDown(wrongAny), atom::meta::BadConversionException);
}

TEST_F(ConversionTest, DynamicRefConversionDownBadCast) {
    // dynamic_cast reference form throws std::bad_cast when types are unrelated.
    // Use DynamicConversion<AnotherDerived&, Derived&>: supply a Derived& but
    // the object is actually a plain Derived (not AnotherDerived), so
    // dynamic_cast<AnotherDerived&> throws std::bad_cast.
    atom::meta::DynamicConversion<AnotherDerived&, Derived&> conv;

    // convert up Derived → Derived (same poly hierarchy)
    Derived derivedObj;
    std::any derivedRefAny = std::ref(derivedObj);

    // convertDownImpl tries to cast the held Derived& to AnotherDerived& → bad_cast
    std::any derivedBaseAny = std::ref(static_cast<Derived&>(derivedObj));
    // First we need a "to" any (Derived&) to pass to convertDown.
    // We convert from AnotherDerived→Derived doesn't make sense directly, so
    // directly pass a reference_wrapper<Derived> as the "toAny" argument.
    EXPECT_THROW(conv.convertDown(derivedRefAny), atom::meta::BadConversionException);
}

TEST_F(ConversionTest, DynamicConversionElseBranchConvert) {
    // DynamicConversion with value types hits the else branch (line 383).
    atom::meta::DynamicConversion<int, float> conv;
    std::any intAny = 42;
    EXPECT_THROW(conv.convert(intAny), atom::meta::BadConversionException);
}

TEST_F(ConversionTest, DynamicConversionElseBranchConvertDown) {
    atom::meta::DynamicConversion<int, float> conv;
    std::any floatAny = 3.14f;
    EXPECT_THROW(conv.convertDown(floatAny), atom::meta::BadConversionException);
}

// ---- DynamicConversion ref convertImpl null-ptr branch (line 374) -----------
TEST_F(ConversionTest, DynamicRefConversionNullFromPtr) {
    atom::meta::DynamicConversion<Derived&, Base&> conv;
    std::any wrongAny = 42;
    EXPECT_THROW(conv.convert(wrongAny), atom::meta::BadConversionException);
}

// ---- DynamicConversion pointer bad_any_cast on convertImpl (line 360) -------
// Pass an any whose stored type doesn't match From* so std::any_cast<From*>
// throws std::bad_any_cast, which is caught and rethrown as BadConversionException.
TEST_F(ConversionTest, DynamicPtrConversionBadAnyCast) {
    atom::meta::DynamicConversion<Derived*, Base*> conv;
    // Store an AnotherDerived* while From = Derived* – any_cast throws bad_any_cast
    AnotherDerived ad;
    std::any wrongPtrAny = static_cast<AnotherDerived*>(&ad);
    EXPECT_THROW(conv.convert(wrongPtrAny), atom::meta::BadConversionException);
}

// ---- VectorConversion bad_cast path (line 458) – element cast fails --------
// VectorConversion<shared_ptr<Derived>, shared_ptr<Base>>:
// provide a vector<shared_ptr<AnotherDerived>> as if it were
// vector<shared_ptr<Derived>> by stuffing it through any. We need the
// element dynamic_pointer_cast to fail.
// The bad_any_cast path (line 465) fires when the stored any type is wrong.
TEST_F(ConversionTest, VectorConversionBadAnyCast) {
    atom::meta::VectorConversion<std::shared_ptr<Derived>,
                                 std::shared_ptr<Base>>
        vectorConv;

    // Provide wrong type in any (not vector<shared_ptr<Derived>>).
    std::any wrongAny = 42;
    EXPECT_THROW(vectorConv.convert(wrongAny), atom::meta::BadConversionException);
}

TEST_F(ConversionTest, VectorConversionDownBadAnyCast) {
    atom::meta::VectorConversion<std::shared_ptr<Derived>,
                                 std::shared_ptr<Base>>
        vectorConv;

    std::any wrongAny = 42;
    EXPECT_THROW(vectorConv.convertDown(wrongAny), atom::meta::BadConversionException);
}

// bad_cast from failed element cast in convertDownImpl (line 482)
// Note: the VectorConversion only catches std::bad_any_cast; a failed
// dynamic_pointer_cast throws std::bad_cast which propagates out unwrapped.
TEST_F(ConversionTest, VectorConversionElementCastFails) {
    atom::meta::VectorConversion<std::shared_ptr<Derived>,
                                 std::shared_ptr<Base>>
        vectorConv;

    // vector<Base*> with AnotherDerived; convertDown tries
    // dynamic_pointer_cast<Derived>(AnotherDerived) → nullptr → throw bad_cast
    std::vector<std::shared_ptr<Base>> mixedBases;
    mixedBases.push_back(std::make_shared<AnotherDerived>());

    std::any mixedAny = mixedBases;
    // std::bad_cast propagates because VectorConversion only catches bad_any_cast
    EXPECT_THROW(vectorConv.convertDown(mixedAny), std::bad_cast);
}

// ---- SequenceConversion error paths (lines 573, 580, 596, 603) -------------
TEST_F(ConversionTest, SequenceConversionBadAnyCast) {
    atom::meta::SequenceConversion<std::list,
                                   std::shared_ptr<Derived>,
                                   std::shared_ptr<Base>>
        seqConv;

    std::any wrongAny = 42;
    EXPECT_THROW(seqConv.convert(wrongAny), atom::meta::BadConversionException);
    EXPECT_THROW(seqConv.convertDown(wrongAny), atom::meta::BadConversionException);
}

TEST_F(ConversionTest, SequenceConversionElementCastFails) {
    atom::meta::SequenceConversion<std::list,
                                   std::shared_ptr<Derived>,
                                   std::shared_ptr<Base>>
        seqConv;

    // convertDown: list<Base*> with AnotherDerived → cast to Derived* fails
    // SequenceConversion only catches bad_any_cast so std::bad_cast propagates
    std::list<std::shared_ptr<Base>> mixedList;
    mixedList.push_back(std::make_shared<AnotherDerived>());

    std::any mixedAny = mixedList;
    EXPECT_THROW(seqConv.convertDown(mixedAny), std::bad_cast);
}

// ---- SetConversion error paths (lines 629, 636, 652, 659) -----------------
TEST_F(ConversionTest, SetConversionBadAnyCast) {
    atom::meta::SetConversion<std::set,
                              std::shared_ptr<Derived>,
                              std::shared_ptr<Base>>
        setConv;

    std::any wrongAny = 42;
    EXPECT_THROW(setConv.convert(wrongAny), atom::meta::BadConversionException);
    EXPECT_THROW(setConv.convertDown(wrongAny), atom::meta::BadConversionException);
}

TEST_F(ConversionTest, SetConversionElementCastFails) {
    atom::meta::SetConversion<std::set,
                              std::shared_ptr<Derived>,
                              std::shared_ptr<Base>>
        setConv;

    // convertDown: set<Base*> element is AnotherDerived → cast to Derived fails
    // SetConversion only catches bad_any_cast so std::bad_cast propagates
    std::set<std::shared_ptr<Base>> mixedSet;
    mixedSet.insert(std::make_shared<AnotherDerived>());
    std::any mixedAny = mixedSet;
    EXPECT_THROW(setConv.convertDown(mixedAny), std::bad_cast);
}

// ---- MapConversion error paths (lines 517, 524, 540, 547) -----------------
TEST_F(ConversionTest, MapConversionBadAnyCast) {
    atom::meta::MapConversion<std::map, int, std::shared_ptr<Derived>,
                              int, std::shared_ptr<Base>>
        mapConv;

    std::any wrongAny = 42;
    EXPECT_THROW(mapConv.convert(wrongAny), atom::meta::BadConversionException);
    EXPECT_THROW(mapConv.convertDown(wrongAny), atom::meta::BadConversionException);
}

TEST_F(ConversionTest, MapConversionValueCastFails) {
    atom::meta::MapConversion<std::map, int, std::shared_ptr<Derived>,
                              int, std::shared_ptr<Base>>
        mapConv;

    // convertDown: map<int, shared_ptr<Base>> whose value is AnotherDerived →
    // dynamic_pointer_cast to Derived fails → THROW_CONVERSION_ERROR (line 540)
    std::map<int, std::shared_ptr<Base>> mixedMap;
    mixedMap[1] = std::make_shared<AnotherDerived>();
    std::any mixedAny = mixedMap;
    EXPECT_THROW(mapConv.convertDown(mixedAny), atom::meta::BadConversionException);
}

// ---- TypeConversions::convert bad_any_cast path (line 708) -----------------
// We need the registry's convert<To,From> to receive a std::bad_any_cast from
// the underlying conversion.  A StaticConversion<int, float> (pointer branch
// active) would be ideal, but we can abuse a StaticConversion wrapping pointers
// and feed an int: that throws from std::any_cast inside convertImpl which
// propagates as std::bad_any_cast to the registry.
TEST_F(ConversionTest, RegistryConvertBadAnyCast) {
    // Manually add a DynamicConversion<Derived*, Base*>, then feed an any whose
    // stored type is Base* (not Derived*), so std::any_cast<Derived*> throws
    // std::bad_any_cast which is caught at line 707-710 and re-thrown as
    // BadConversionException.
    conversions->addConversion(
        std::make_shared<atom::meta::DynamicConversion<Derived*, Base*>>());

    // any holds a Base* (not a Derived*) — std::any_cast<Derived*> throws
    Base baseObj;
    std::any wrongTypedAny = static_cast<Base*>(&baseObj);

    EXPECT_THROW(
        (conversions->convert<Base*, Derived*>(wrongTypedAny)),
        atom::meta::BadConversionException);
}

// ---- TypeConversions::canConvert returns false (line 763) ------------------
TEST_F(ConversionTest, CanConvertReturnsFalse) {
    // No conversions registered at all.
    EXPECT_FALSE(conversions->canConvert(atom::meta::userType<int>(),
                                         atom::meta::userType<float>()));

    // Registered for Derived*->Base*, but not Derived*->AnotherDerived*.
    conversions->addBaseClass<Base, Derived>();
    EXPECT_FALSE(
        conversions->canConvert(atom::meta::userType<Derived*>(),
                                atom::meta::userType<AnotherDerived*>()));
}

// ---- TypeConversions::convertTo (lines 725-745) ----------------------------
TEST_F(ConversionTest, ConvertToSuccess) {
    conversions->addBaseClass<Base, Derived>();

    Derived derivedObj;
    std::any derivedAny = static_cast<Derived*>(&derivedObj);

    // convertTo<Base*> tries all registered conversions and succeeds.
    std::any result = conversions->convertTo<Base*>(derivedAny);
    Base* ptr = std::any_cast<Base*>(result);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->getName(), "Derived");
}

TEST_F(ConversionTest, ConvertToNoConversionFound) {
    // No conversion registered — throws.
    std::any anyVal = 42;
    EXPECT_THROW(conversions->convertTo<Base*>(anyVal),
                 atom::meta::BadConversionException);
}

// ---- safeConvert / ConversionBuilder / ConversionChain / ConversionDetector -
TEST_F(ConversionTest, SafeConvertImplicit) {
    auto result = atom::meta::safeConvert<double>(42);
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 42.0);
}

TEST_F(ConversionTest, SafeConvertNullopt) {
    // std::string → int is not implicitly convertible, so nullopt branch.
    auto result = atom::meta::safeConvert<int>(std::string("hello"));
    EXPECT_FALSE(result.has_value());
}

TEST_F(ConversionTest, ConversionBuilderTo) {
    auto builder = atom::meta::convert(42);
    auto result = builder.to<double>();
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 42.0);
}

TEST_F(ConversionTest, ConversionBuilderToOrDefault) {
    auto builder = atom::meta::convert(std::string("hello"));
    // No implicit conversion from string to int → returns default.
    int val = builder.toOrDefault<int>(99);
    EXPECT_EQ(val, 99);

    // Successful case: int → double, no default used.
    auto builder2 = atom::meta::convert(7);
    double val2 = builder2.toOrDefault<double>(0.0);
    EXPECT_DOUBLE_EQ(val2, 7.0);
}

TEST_F(ConversionTest, ConversionBuilderToOrThrow) {
    auto builder = atom::meta::convert(42);
    double val = builder.toOrThrow<double>();
    EXPECT_DOUBLE_EQ(val, 42.0);

    // Non-convertible type → throws.
    auto builder2 = atom::meta::convert(std::string("hi"));
    EXPECT_THROW(builder2.toOrThrow<int>(), atom::meta::BadConversionException);
}

TEST_F(ConversionTest, ConversionChainTwoTypes) {
    // Two-type chain: int → double.
    auto result = atom::meta::ConversionChain<int, double>::convert(5);
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 5.0);
}

TEST_F(ConversionTest, ConversionChainTwoTypesNullopt) {
    // Two-type chain where conversion is not possible (std::string → int).
    auto result = atom::meta::ConversionChain<std::string, int>::convert(
        std::string("hi"));
    EXPECT_FALSE(result.has_value());
}

TEST_F(ConversionTest, IsConversionRegistered) {
    EXPECT_FALSE(
        (atom::meta::isConversionRegistered<Derived*, Base*>(*conversions)));
    conversions->addBaseClass<Base, Derived>();
    EXPECT_TRUE(
        (atom::meta::isConversionRegistered<Derived*, Base*>(*conversions)));
}

TEST_F(ConversionTest, ConversionDetector) {
    using D1 = atom::meta::ConversionDetector<int, double>;
    EXPECT_TRUE(D1::is_implicit);
    EXPECT_FALSE(D1::is_explicit);
    EXPECT_TRUE(D1::is_static_castable);
    EXPECT_FALSE(D1::is_reinterpret_castable);
    EXPECT_TRUE(D1::is_any_convertible);

    using D2 = atom::meta::ConversionDetector<int*, float*>;
    EXPECT_FALSE(D2::is_implicit);
    EXPECT_TRUE(D2::is_reinterpret_castable);
}

TEST_F(ConversionTest, ConversionEntryStruct) {
    atom::meta::ConversionEntry<int, double> entry;
    entry.converter = [](const int& v) { return static_cast<double>(v); };
    entry.is_bidirectional = true;
    entry.reverse_converter = [](const double& v) { return static_cast<int>(v); };

    EXPECT_DOUBLE_EQ(entry.converter(5), 5.0);
    EXPECT_EQ(entry.reverse_converter(3.7), 3);
    EXPECT_TRUE(entry.is_bidirectional);
}

// ---- ConversionMetrics (isEfficient / getMetrics) --------------------------
TEST_F(ConversionTest, ConversionMetricsAndIsEfficient) {
    atom::meta::DynamicConversion<Derived*, Base*> conv;

    // Before any conversions: success rate = 0 → not efficient.
    EXPECT_FALSE(conv.isEfficient());

    const auto& m = conv.getMetrics();
    EXPECT_EQ(m.conversion_count.load(), 0u);

    // Perform a successful conversion to update metrics.
    Derived derivedObj;
    std::any da = static_cast<Derived*>(&derivedObj);
    conv.convert(da);

    EXPECT_EQ(m.conversion_count.load(), 1u);
    EXPECT_EQ(m.success_count.load(), 1u);
    EXPECT_DOUBLE_EQ(m.getSuccessRate(), 1.0);
    EXPECT_GT(m.getAverageExecutionTime(), 0.0);
    EXPECT_DOUBLE_EQ(m.getCacheHitRate(), 0.0);
}

TEST_F(ConversionTest, ConversionMetricsRecordCacheHit) {
    atom::meta::DynamicConversion<Derived*, Base*> conv;
    const auto& m = conv.getMetrics();

    // No conversions yet — getCacheHitRate = 0 (total == 0 branch).
    EXPECT_DOUBLE_EQ(m.getCacheHitRate(), 0.0);

    // Trigger a conversion so total > 0, then record a cache hit.
    Derived derivedObj;
    std::any da = static_cast<Derived*>(&derivedObj);
    conv.convert(da);
    m.recordCacheHit();

    EXPECT_EQ(m.cache_hits.load(), 1u);
    EXPECT_DOUBLE_EQ(m.getCacheHitRate(), 1.0);
}

TEST_F(ConversionTest, ConversionMetricsFailurePath) {
    atom::meta::DynamicConversion<AnotherDerived*, Base*> conv;
    const auto& m = conv.getMetrics();

    // Trigger a failed conversion (down-cast of wrong type).
    Derived derivedObj;
    Base* bptr = &derivedObj;
    std::any baseAny = bptr;
    EXPECT_THROW(conv.convertDown(baseAny), atom::meta::BadConversionException);

    // The convertDown wrapper records the failure in metrics_ (lines 153-160).
    EXPECT_GE(m.conversion_count.load(), 1u);
    EXPECT_LT(m.getSuccessRate(), 1.0);
}

// ---- baseClass() with non-polymorphic types (StaticConversion branch) ------
TEST_F(ConversionTest, BaseClassHelperNonPolymorphic) {
    // SimpleBase/SimpleDerived are not polymorphic → StaticConversion branch.
    auto conv = atom::meta::baseClass<SimpleBase, SimpleDerived>();
    ASSERT_NE(conv, nullptr);

    // from type should be SimpleDerived (value, not pointer for static path)
    EXPECT_EQ(conv->from(), atom::meta::userType<SimpleDerived>());
    EXPECT_EQ(conv->to(),   atom::meta::userType<SimpleBase>());
}

// ---- TypeConversions copy semantics (operator= / copy ctor) ----------------
TEST_F(ConversionTest, TypeConversionBaseCopyAndMove) {
    atom::meta::DynamicConversion<Derived*, Base*> original;

    // Copy constructor via derived slice – exercise the copy ctor on the base.
    atom::meta::DynamicConversion<Derived*, Base*> copied(original);
    EXPECT_EQ(copied.from(), original.from());
    EXPECT_EQ(copied.to(),   original.to());

    // Move constructor.
    atom::meta::DynamicConversion<Derived*, Base*> moved(std::move(copied));
    EXPECT_EQ(moved.from(), original.from());
}

// ---------------------------------------------------------------------------
// Second round of additional tests targeting remaining uncovered lines
// ---------------------------------------------------------------------------

// ---- getAverageExecutionTime zero-count branch (line 92) -------------------
TEST_F(ConversionTest, AverageExecutionTimeZeroCount) {
    atom::meta::DynamicConversion<Derived*, Base*> conv;
    const auto& m = conv.getMetrics();
    // Before any conversion, count == 0 → returns 0.0 (line 92 branch)
    EXPECT_DOUBLE_EQ(m.getAverageExecutionTime(), 0.0);
}

// ---- isEfficient true path (line 210 evaluates second operand) -------------
TEST_F(ConversionTest, IsEfficientTrue) {
    atom::meta::DynamicConversion<Derived*, Base*> conv;

    // Perform many fast successful conversions so success rate > 0.95 and
    // average execution time < 1000ns → isEfficient() returns true.
    Derived derivedObj;
    std::any da = static_cast<Derived*>(&derivedObj);
    for (int i = 0; i < 10; ++i) {
        conv.convert(da);
    }
    // All succeeded → success rate == 1.0 > 0.95, and time is sub-microsecond
    EXPECT_TRUE(conv.isEfficient());
}

// ---- StaticConversion catch(bad_cast) in convertImpl pointer branch (312-313)
// std::bad_any_cast inherits from std::bad_cast in libstdc++, so
// std::any_cast<From>(from) throwing bad_any_cast IS caught at line 312.
// Trigger: pass wrong-typed any to pointer-branch StaticConversion.
TEST_F(ConversionTest, StaticPtrConversionBadAnyCast) {
    atom::meta::StaticConversion<SimpleDerived*, SimpleBase*> conv;
    // Storing an int, not a SimpleDerived* → any_cast throws bad_any_cast
    // → caught at line 312 → THROW_CONVERSION_ERROR at line 313.
    std::any wrongAny = 42;
    EXPECT_THROW(conv.convert(wrongAny), atom::meta::BadConversionException);
}

// ---- DynamicConversion: dynamic_cast returns null for non-null ptr (line 360)
// DynamicConversion<Base*, Derived*>: convertImpl does dynamic_cast<Derived*>
// on a non-Derived Base → null → line 360 fires.
TEST_F(ConversionTest, DynamicConversionNullDynamicCastConvertImpl) {
    // From=Base*, To=Derived*: convertImpl will try dynamic_cast<Derived*>(base)
    atom::meta::DynamicConversion<Base*, Derived*> conv;

    Base plainBase;
    std::any basePtrAny = static_cast<Base*>(&plainBase);

    // dynamic_cast<Derived*>(&plainBase) → null, fromPtr != null → throw (line 360)
    EXPECT_THROW(conv.convert(basePtrAny), atom::meta::BadConversionException);
}

// ---- VectorConversion convertImpl element cast fail (line 458) -------------
// convertImpl: provide a vector<shared_ptr<Base>> when expecting
// vector<shared_ptr<Derived>>. VectorConversion<Derived,Base>::convertImpl
// casts Derived→Base, but if we supply a Base holding AnotherDerived and ask
// convertImpl for From=AnotherDerived→To=Base... easier: use
// VectorConversion<Base,Derived> (converts vector<Base*>→vector<Derived*>)
// and supply a vec with plain Base elements (not Derived) so cast fails.
TEST_F(ConversionTest, VectorConversionConvertImplElementCastFails) {
    // VectorConversion<From=shared_ptr<Base>, To=shared_ptr<Derived>>:
    // convertImpl casts Base→Derived via dynamic_pointer_cast.
    // Provide a vec<shared_ptr<Base>> with plain Base elements.
    atom::meta::VectorConversion<std::shared_ptr<Base>,
                                 std::shared_ptr<Derived>>
        conv;

    std::vector<std::shared_ptr<Base>> baseVec;
    baseVec.push_back(std::make_shared<Base>());  // not a Derived

    std::any baseVecAny = baseVec;
    // dynamic_pointer_cast<Derived>(Base) → null → throw bad_cast (line 458)
    EXPECT_THROW(conv.convert(baseVecAny), std::bad_cast);
}

// ---- SequenceConversion convertImpl element cast fail (line 573) -----------
TEST_F(ConversionTest, SequenceConversionConvertImplElementCastFails) {
    atom::meta::SequenceConversion<std::list,
                                   std::shared_ptr<Base>,
                                   std::shared_ptr<Derived>>
        conv;

    std::list<std::shared_ptr<Base>> baseList;
    baseList.push_back(std::make_shared<Base>());

    std::any baseListAny = baseList;
    EXPECT_THROW(conv.convert(baseListAny), std::bad_cast);
}

// ---- SetConversion convertImpl element cast fail (line 629) ----------------
TEST_F(ConversionTest, SetConversionConvertImplElementCastFails) {
    atom::meta::SetConversion<std::set,
                              std::shared_ptr<Base>,
                              std::shared_ptr<Derived>>
        conv;

    std::set<std::shared_ptr<Base>> baseSet;
    baseSet.insert(std::make_shared<Base>());

    std::any baseSetAny = baseSet;
    EXPECT_THROW(conv.convert(baseSetAny), std::bad_cast);
}

// ---- MapConversion convertImpl element cast fail (line 517) ----------------
TEST_F(ConversionTest, MapConversionConvertImplValueCastFails) {
    atom::meta::MapConversion<std::map, int, std::shared_ptr<Base>,
                              int, std::shared_ptr<Derived>>
        conv;

    std::map<int, std::shared_ptr<Base>> baseMap;
    baseMap[1] = std::make_shared<Base>();  // not a Derived

    std::any baseMapAny = baseMap;
    // dynamic_pointer_cast<Derived>(Base) → null → THROW_CONVERSION_ERROR (line 517)
    EXPECT_THROW(conv.convert(baseMapAny), atom::meta::BadConversionException);
}

// ---- TypeConversions::convertTo catch paths (lines 734-737) ----------------
// Line 734: bad_any_cast continuation in convertTo
// Line 736: BadConversionException continuation in convertTo
// These paths fire when a registered conversion matches the To type but throws.
// We need multiple registered conversions to target the same To type so that
// the first attempt throws (and continues) and the second succeeds — or we
// just ensure the throw paths are hit before the "not found" exit.
TEST_F(ConversionTest, ConvertToContinueOnBadAnyCast) {
    // Register DynamicConversion<Derived*,Base*> (To=Base*).
    // Call convertTo<Base*> with an any that holds an int (bad_any_cast) →
    // the loop catches it and continues, then throws "not found".
    conversions->addBaseClass<Base, Derived>();

    std::any wrongAny = 42;  // not Derived*
    // convertTo tries the conversion (bad_any_cast from int → Derived*), catches,
    // continues to next, exhausts all → throws BadConversionException.
    EXPECT_THROW(conversions->convertTo<Base*>(wrongAny),
                 atom::meta::BadConversionException);
}

TEST_F(ConversionTest, ConvertToContinueOnBadConversion) {
    // Register a DynamicConversion<AnotherDerived*, Base*> and
    // DynamicConversion<Derived*, Base*>.
    // Supply an AnotherDerived* any and ask for Base* — both conversions target
    // Base* but only the right one succeeds.
    conversions->addBaseClass<Base, AnotherDerived>();  // AnotherDerived*→Base*
    conversions->addBaseClass<Base, Derived>();          // Derived*→Base*

    AnotherDerived adObj;
    std::any adAny = static_cast<AnotherDerived*>(&adObj);
    std::any result = conversions->convertTo<Base*>(adAny);
    Base* ptr = std::any_cast<Base*>(result);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->getName(), "AnotherDerived");
}

}  // namespace atom::test

#endif  // ATOM_TEST_CONVERSION_HPP
