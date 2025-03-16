// atom/function/test_conversion.hpp
#ifndef ATOM_TEST_CONVERSION_HPP
#define ATOM_TEST_CONVERSION_HPP

#include <gtest/gtest.h>
#include "atom/function/conversion.hpp"

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
TEST_F(ConversionTest, SetConversion) {
    // Create set of derived pointers
    std::set<std::shared_ptr<Derived>,
             std::owner_less<std::shared_ptr<Derived>>>
        derivedSet;
    derivedSet.insert(std::make_shared<Derived>());

    // Create set conversion
    atom::meta::SetConversion<std::set, std::shared_ptr<Derived>,
                              std::shared_ptr<Base>>
        setConv;

    // Test convert up
    std::any derivedSetAny = derivedSet;
    std::any baseSetAny = setConv.convert(derivedSetAny);

    auto baseSet =
        std::any_cast<std::set<std::shared_ptr<Base>,
                               std::owner_less<std::shared_ptr<Base>>>>(
            baseSetAny);
    ASSERT_EQ(baseSet.size(), 1);
    EXPECT_EQ((*baseSet.begin())->getName(), "Derived");

    // Test convert down
    std::any backToDerivecSetAny = setConv.convertDown(baseSetAny);
    auto backToDerivecSet =
        std::any_cast<std::set<std::shared_ptr<Derived>,
                               std::owner_less<std::shared_ptr<Derived>>>>(
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

}  // namespace atom::test

#endif  // ATOM_TEST_CONVERSION_HPP