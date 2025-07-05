/**
 * @file type_info_example.cpp
 * @brief Comprehensive examples of using the TypeInfo library
 * @author Example Author
 * @date 2025-03-23
 */

#include <array>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include "atom/meta/type_info.hpp"

// Example custom types for testing TypeInfo
struct SimpleStruct {
    int a;
    double b;
};

class ComplexClass {
public:
    ComplexClass() = default;
    virtual ~ComplexClass() = default;
    virtual void virtualMethod() {}
};

class DerivedClass : public ComplexClass {
public:
    void virtualMethod() override {}
};

class FinalClass final : public ComplexClass {
public:
    void virtualMethod() override {}
};

class EmptyClass {};

class NonCopyableClass {
public:
    NonCopyableClass() = default;
    ~NonCopyableClass() = default;
    NonCopyableClass(const NonCopyableClass&) = delete;
    NonCopyableClass& operator=(const NonCopyableClass&) = delete;
    NonCopyableClass(NonCopyableClass&&) = default;
    NonCopyableClass& operator=(NonCopyableClass&&) = default;
};

class NonMoveableClass {
public:
    NonMoveableClass() = default;
    ~NonMoveableClass() = default;
    NonMoveableClass(const NonMoveableClass&) = default;
    NonMoveableClass& operator=(const NonMoveableClass&) = default;
    NonMoveableClass(NonMoveableClass&&) = delete;
    NonMoveableClass& operator=(NonMoveableClass&&) = delete;
};

class AbstractClass {
public:
    virtual ~AbstractClass() = default;
    virtual void pureVirtualMethod() = 0;
};

struct AggregateType {
    int x;
    double y;
    std::string z;
};

enum class Color { Red, Green, Blue };

enum LegacyEnum { One, Two, Three };

// Custom smart pointer for testing
template <typename T>
class CustomPtr {
private:
    T* ptr;

public:
    explicit CustomPtr(T* p = nullptr) : ptr(p) {}
    ~CustomPtr() { delete ptr; }
    T& operator*() const { return *ptr; }
    T* operator->() const { return ptr; }
    T* get() const { return ptr; }
};

// Example function for testing function traits
int exampleFunction(double a, std::string b) {
    return static_cast<int>(a) + b.length();
}

// Simple utility to print section headers
void printSection(const std::string& title) {
    std::cout << "\n============== " << title << " ==============\n";
}

// Utility to print TypeInfo
void printTypeInfo(const std::string& label, const atom::meta::TypeInfo& info) {
    std::cout << "Type information for " << label << ":\n";
    std::cout << "  - Name: " << info.name() << "\n";
    std::cout << "  - Bare name: " << info.bareName() << "\n";
    std::cout << "  - Is class: " << (info.isClass() ? "Yes" : "No") << "\n";
    std::cout << "  - Is pointer: " << (info.isPointer() ? "Yes" : "No")
              << "\n";
    std::cout << "  - Is reference: " << (info.isReference() ? "Yes" : "No")
              << "\n";
    std::cout << "  - Is arithmetic: " << (info.isArithmetic() ? "Yes" : "No")
              << "\n";
    std::cout << "  - Is enum: " << (info.isEnum() ? "Yes" : "No") << "\n";
    std::cout << "  - Is array: " << (info.isArray() ? "Yes" : "No") << "\n";
    std::cout << "  - Is const: " << (info.isConst() ? "Yes" : "No") << "\n";
    std::cout << "  - Is void: " << (info.isVoid() ? "Yes" : "No") << "\n";
    std::cout << "  - Is function: " << (info.isFunction() ? "Yes" : "No")
              << "\n";
    std::cout << "  - Is trivial: " << (info.isTrivial() ? "Yes" : "No")
              << "\n";
    std::cout << "  - Is standard layout: "
              << (info.isStandardLayout() ? "Yes" : "No") << "\n";
    std::cout << "  - Is POD: " << (info.isPod() ? "Yes" : "No") << "\n";
    std::cout << "  - Is default constructible: "
              << (info.isDefaultConstructible() ? "Yes" : "No") << "\n";
    std::cout << "  - Is moveable: " << (info.isMoveable() ? "Yes" : "No")
              << "\n";
    std::cout << "  - Is copyable: " << (info.isCopyable() ? "Yes" : "No")
              << "\n";
    std::cout << "  - Is aggregate: " << (info.isAggregate() ? "Yes" : "No")
              << "\n";
    std::cout << "  - Is bounded array: "
              << (info.isBoundedArray() ? "Yes" : "No") << "\n";
    std::cout << "  - Is unbounded array: "
              << (info.isUnboundedArray() ? "Yes" : "No") << "\n";
    std::cout << "  - Is scoped enum: " << (info.isScopedEnum() ? "Yes" : "No")
              << "\n";
    std::cout << "  - Is final: " << (info.isFinal() ? "Yes" : "No") << "\n";
    std::cout << "  - Is abstract: " << (info.isAbstract() ? "Yes" : "No")
              << "\n";
    std::cout << "  - Is polymorphic: " << (info.isPolymorphic() ? "Yes" : "No")
              << "\n";
    std::cout << "  - Is empty: " << (info.isEmpty() ? "Yes" : "No") << "\n";
}

int main() {
    std::cout << "TYPEINFO COMPREHENSIVE EXAMPLES\n";
    std::cout << "==============================\n";

    //--------------------------------------------------------------------------
    // 1. Basic Type Information
    //--------------------------------------------------------------------------
    printSection("Basic Type Information");

    // Get TypeInfo for built-in types
    auto intInfo = atom::meta::TypeInfo::create<int>();
    auto doubleInfo = atom::meta::TypeInfo::create<double>();
    auto stringInfo = atom::meta::TypeInfo::create<std::string>();
    auto voidInfo = atom::meta::TypeInfo::create<void>();

    printTypeInfo("int", intInfo);
    printTypeInfo("double", doubleInfo);
    printTypeInfo("std::string", stringInfo);
    printTypeInfo("void", voidInfo);

    //--------------------------------------------------------------------------
    // 2. CVR Qualifiers (Const, Volatile, Reference)
    //--------------------------------------------------------------------------
    printSection("CVR Qualifiers");

    auto constIntInfo = atom::meta::TypeInfo::create<const int>();
    auto intRefInfo = atom::meta::TypeInfo::create<int&>();
    auto constIntRefInfo = atom::meta::TypeInfo::create<const int&>();

    printTypeInfo("const int", constIntInfo);
    printTypeInfo("int&", intRefInfo);
    printTypeInfo("const int&", constIntRefInfo);

    std::cout << "\nBare Type Comparisons:\n";
    std::cout << "  - const int bareEqual int: "
              << (constIntInfo.bareEqual(intInfo) ? "Yes" : "No") << "\n";
    std::cout << "  - int& bareEqual int: "
              << (intRefInfo.bareEqual(intInfo) ? "Yes" : "No") << "\n";
    std::cout << "  - const int& bareEqual int: "
              << (constIntRefInfo.bareEqual(intInfo) ? "Yes" : "No") << "\n";

    //--------------------------------------------------------------------------
    // 3. Pointer Types
    //--------------------------------------------------------------------------
    printSection("Pointer Types");

    auto intPtrInfo = atom::meta::TypeInfo::create<int*>();
    auto constIntPtrInfo = atom::meta::TypeInfo::create<const int*>();
    auto intPtrPtrInfo = atom::meta::TypeInfo::create<int**>();

    printTypeInfo("int*", intPtrInfo);
    printTypeInfo("const int*", constIntPtrInfo);
    printTypeInfo("int**", intPtrPtrInfo);

    //--------------------------------------------------------------------------
    // 4. Smart Pointers
    //--------------------------------------------------------------------------
    printSection("Smart Pointers");

    auto sharedPtrInfo = atom::meta::TypeInfo::create<std::shared_ptr<int>>();
    auto uniquePtrInfo =
        atom::meta::TypeInfo::create<std::unique_ptr<double>>();
    auto weakPtrInfo =
        atom::meta::TypeInfo::create<std::weak_ptr<std::string>>();
    auto customPtrInfo = atom::meta::TypeInfo::create<CustomPtr<int>>();

    printTypeInfo("std::shared_ptr<int>", sharedPtrInfo);
    printTypeInfo("std::unique_ptr<double>", uniquePtrInfo);
    printTypeInfo("std::weak_ptr<std::string>", weakPtrInfo);
    printTypeInfo("CustomPtr<int>", customPtrInfo);

    //--------------------------------------------------------------------------
    // 5. Container Types
    //--------------------------------------------------------------------------
    printSection("Container Types");

    auto vectorInfo = atom::meta::TypeInfo::create<std::vector<int>>();
    auto mapInfo =
        atom::meta::TypeInfo::create<std::map<std::string, double>>();
    auto arrayInfo = atom::meta::TypeInfo::create<std::array<char, 10>>();
    auto spanInfo = atom::meta::TypeInfo::create<std::span<int>>();

    printTypeInfo("std::vector<int>", vectorInfo);
    printTypeInfo("std::map<std::string, double>", mapInfo);
    printTypeInfo("std::array<char, 10>", arrayInfo);
    printTypeInfo("std::span<int>", spanInfo);

    //--------------------------------------------------------------------------
    // 6. Array Types
    //--------------------------------------------------------------------------
    printSection("Array Types");

    auto staticArrayInfo = atom::meta::TypeInfo::create<int[10]>();
    auto dynamicArrayInfo = atom::meta::TypeInfo::create<int[]>();
    auto multidimArrayInfo = atom::meta::TypeInfo::create<int[3][4]>();

    printTypeInfo("int[10]", staticArrayInfo);
    printTypeInfo("int[]", dynamicArrayInfo);
    printTypeInfo("int[3][4]", multidimArrayInfo);

    //--------------------------------------------------------------------------
    // 7. Custom Class Types
    //--------------------------------------------------------------------------
    printSection("Custom Class Types");

    auto simpleStructInfo = atom::meta::TypeInfo::create<SimpleStruct>();
    auto complexClassInfo = atom::meta::TypeInfo::create<ComplexClass>();
    auto derivedClassInfo = atom::meta::TypeInfo::create<DerivedClass>();
    auto finalClassInfo = atom::meta::TypeInfo::create<FinalClass>();
    auto emptyClassInfo = atom::meta::TypeInfo::create<EmptyClass>();

    printTypeInfo("SimpleStruct", simpleStructInfo);
    printTypeInfo("ComplexClass", complexClassInfo);
    printTypeInfo("DerivedClass", derivedClassInfo);
    printTypeInfo("FinalClass", finalClassInfo);
    printTypeInfo("EmptyClass", emptyClassInfo);

    //--------------------------------------------------------------------------
    // 8. Special Class Types
    //--------------------------------------------------------------------------
    printSection("Special Class Types");

    auto nonCopyableInfo = atom::meta::TypeInfo::create<NonCopyableClass>();
    auto nonMoveableInfo = atom::meta::TypeInfo::create<NonMoveableClass>();
    auto abstractClassInfo = atom::meta::TypeInfo::create<AbstractClass>();
    auto aggregateTypeInfo = atom::meta::TypeInfo::create<AggregateType>();

    printTypeInfo("NonCopyableClass", nonCopyableInfo);
    printTypeInfo("NonMoveableClass", nonMoveableInfo);
    printTypeInfo("AbstractClass", abstractClassInfo);
    printTypeInfo("AggregateType", aggregateTypeInfo);

    //--------------------------------------------------------------------------
    // 9. Enum Types
    //--------------------------------------------------------------------------
    printSection("Enum Types");

    auto enumClassInfo = atom::meta::TypeInfo::create<Color>();
    auto legacyEnumInfo = atom::meta::TypeInfo::create<LegacyEnum>();

    printTypeInfo("Color (enum class)", enumClassInfo);
    printTypeInfo("LegacyEnum", legacyEnumInfo);

    //--------------------------------------------------------------------------
    // 10. Function Types
    //--------------------------------------------------------------------------
    printSection("Function Types");

    auto functionPtrInfo =
        atom::meta::TypeInfo::create<int (*)(double, std::string)>();
    auto functionRefInfo =
        atom::meta::TypeInfo::create<int (&)(double, std::string)>();
    auto functionInfo =
        atom::meta::TypeInfo::create<int(double, std::string)>();

    printTypeInfo("int(*)(double, std::string)", functionPtrInfo);
    printTypeInfo("int(&)(double, std::string)", functionRefInfo);
    printTypeInfo("int(double, std::string)", functionInfo);

    //--------------------------------------------------------------------------
    // 11. Type Comparison
    //--------------------------------------------------------------------------
    printSection("Type Comparison");

    // Equality comparison
    std::cout << "Equality Comparisons:\n";
    std::cout << "  - int == int: "
              << (atom::meta::TypeInfo::create<int>() ==
                          atom::meta::TypeInfo::create<int>()
                      ? "Yes"
                      : "No")
              << "\n";
    std::cout << "  - int == double: "
              << (atom::meta::TypeInfo::create<int>() ==
                          atom::meta::TypeInfo::create<double>()
                      ? "Yes"
                      : "No")
              << "\n";
    std::cout << "  - const int == int: "
              << (atom::meta::TypeInfo::create<const int>() ==
                          atom::meta::TypeInfo::create<int>()
                      ? "Yes"
                      : "No")
              << "\n";

    // Less than comparison (for containers)
    std::cout << "\nLess Than Comparisons (for ordering):\n";
    std::cout << "  - int < double: "
              << (atom::meta::TypeInfo::create<int>() <
                          atom::meta::TypeInfo::create<double>()
                      ? "Yes"
                      : "No")
              << "\n";
    std::cout << "  - double < int: "
              << (atom::meta::TypeInfo::create<double>() <
                          atom::meta::TypeInfo::create<int>()
                      ? "Yes"
                      : "No")
              << "\n";

    //--------------------------------------------------------------------------
    // 12. Type Registry and Management
    //--------------------------------------------------------------------------
    printSection("Type Registry and Management");

    // Register types
    atom::meta::registerType<int>("Int");
    atom::meta::registerType<double>("Double");
    atom::meta::registerType<std::string>("String");
    atom::meta::registerType<SimpleStruct>("SimpleStruct");
    atom::meta::registerType<ComplexClass>("ComplexClass");

    // Get registered types
    auto registeredTypes = atom::meta::getRegisteredTypeNames();
    std::cout << "Registered types:\n";
    for (const auto& typeName : registeredTypes) {
        std::cout << "  - " << typeName << "\n";
    }

    // Check registration
    bool isIntRegistered = atom::meta::isTypeRegistered("Int");
    bool isBoolRegistered = atom::meta::isTypeRegistered("Bool");

    std::cout << "\nType Registration Checks:\n";
    std::cout << "  - Is 'Int' registered: " << (isIntRegistered ? "Yes" : "No")
              << "\n";
    std::cout << "  - Is 'Bool' registered: "
              << (isBoolRegistered ? "Yes" : "No") << "\n";

    // Get type info from registry
    auto optIntInfo = atom::meta::getTypeInfo("Int");
    if (optIntInfo.has_value()) {
        std::cout << "\nRetrieved 'Int' from registry: "
                  << optIntInfo.value().name() << "\n";
    }

    //--------------------------------------------------------------------------
    // 13. Type Factory
    //--------------------------------------------------------------------------
    printSection("Type Factory");

    // Register factories
    atom::meta::TypeFactory::registerFactory<int>("Int");
    atom::meta::TypeFactory::registerFactory<std::string>("String");
    atom::meta::TypeFactory::registerFactory<SimpleStruct>("SimpleStruct");

    // Create instances
    auto intPtr = atom::meta::TypeFactory::createInstance("Int");
    auto stringPtr =
        atom::meta::TypeFactory::createInstance<std::string>("String");
    auto simpleStructPtr =
        atom::meta::TypeFactory::createInstance<SimpleStruct>("SimpleStruct");

    std::cout << "Type Factory Instance Creation:\n";
    std::cout << "  - Created Int instance: "
              << (intPtr != nullptr ? "Yes" : "No") << "\n";
    std::cout << "  - Created String instance: "
              << (stringPtr != nullptr ? "Yes" : "No") << "\n";
    std::cout << "  - Created SimpleStruct instance: "
              << (simpleStructPtr != nullptr ? "Yes" : "No") << "\n";

    //--------------------------------------------------------------------------
    // 14. Type Compatibility Checking
    //--------------------------------------------------------------------------
    printSection("Type Compatibility Checking");

    // Check type compatibility
    bool intDoubleCompat = atom::meta::areTypesCompatible<int, double>();
    bool intStringCompat = atom::meta::areTypesCompatible<int, std::string>();
    bool constIntIntCompat = atom::meta::areTypesCompatible<const int, int>();
    bool doubleDoublePtrCompat =
        atom::meta::areTypesCompatible<double, double*>();
    bool complexDerivedCompat =
        atom::meta::areTypesCompatible<ComplexClass, DerivedClass>();

    std::cout << "Type Compatibility:\n";
    std::cout << "  - int, double: "
              << (intDoubleCompat ? "Compatible" : "Incompatible") << "\n";
    std::cout << "  - int, std::string: "
              << (intStringCompat ? "Compatible" : "Incompatible") << "\n";
    std::cout << "  - const int, int: "
              << (constIntIntCompat ? "Compatible" : "Incompatible") << "\n";
    std::cout << "  - double, double*: "
              << (doubleDoublePtrCompat ? "Compatible" : "Incompatible")
              << "\n";
    std::cout << "  - ComplexClass, DerivedClass: "
              << (complexDerivedCompat ? "Compatible" : "Incompatible") << "\n";

    //--------------------------------------------------------------------------
    // 15. TypeInfo JSON Serialization
    //--------------------------------------------------------------------------
    printSection("TypeInfo JSON Serialization");

    // Serialize type info to JSON
    std::string intJson = intInfo.toJson();
    std::string stringJson = stringInfo.toJson();
    std::string complexClassJson = complexClassInfo.toJson();

    std::cout << "JSON for int type:\n" << intJson << "\n\n";
    std::cout << "JSON for std::string type:\n" << stringJson << "\n\n";
    std::cout << "JSON for ComplexClass type:\n" << complexClassJson << "\n";

    //--------------------------------------------------------------------------
    // 16. Type Information from Instances
    //--------------------------------------------------------------------------
    printSection("Type Information from Instances");

    // Create instances
    int intValue = 42;
    std::string stringValue = "Hello, world!";
    SimpleStruct simpleStruct{1, 2.3};

    // Get TypeInfo from instances
    auto intInstanceInfo = atom::meta::TypeInfo::fromInstance(intValue);
    auto stringInstanceInfo = atom::meta::TypeInfo::fromInstance(stringValue);
    auto simpleStructInstanceInfo =
        atom::meta::TypeInfo::fromInstance(simpleStruct);

    std::cout << "Type information from instances:\n";
    std::cout << "  - From int instance: " << intInstanceInfo.name() << "\n";
    std::cout << "  - From string instance: " << stringInstanceInfo.name()
              << "\n";
    std::cout << "  - From SimpleStruct instance: "
              << simpleStructInstanceInfo.name() << "\n";

    //--------------------------------------------------------------------------
    // 17. User Type Helper Function
    //--------------------------------------------------------------------------
    printSection("User Type Helper Function");

    // Get type info using userType helper
    auto userTypeInt = atom::meta::userType<int>();
    auto userTypeString = atom::meta::userType<std::string>();
    auto userTypeByInstance = atom::meta::userType(simpleStruct);

    std::cout << "User type helper results:\n";
    std::cout << "  - userType<int>(): " << userTypeInt.name() << "\n";
    std::cout << "  - userType<std::string>(): " << userTypeString.name()
              << "\n";
    std::cout << "  - userType(simpleStruct): " << userTypeByInstance.name()
              << "\n";

    //--------------------------------------------------------------------------
    // 18. Hashing and Set/Map Support
    //--------------------------------------------------------------------------
    printSection("Hashing and Set/Map Support");

    // Create a set of TypeInfo
    std::set<atom::meta::TypeInfo> typeInfoSet;
    typeInfoSet.insert(intInfo);
    typeInfoSet.insert(doubleInfo);
    typeInfoSet.insert(stringInfo);
    typeInfoSet.insert(intInfo);  // Duplicate to test set behavior

    std::cout << "Set of TypeInfo contains " << typeInfoSet.size()
              << " unique types:\n";
    for (const auto& info : typeInfoSet) {
        std::cout << "  - " << info.name() << "\n";
    }

    // Create a map with TypeInfo as key
    std::map<atom::meta::TypeInfo, std::string> typeDescriptions;
    typeDescriptions[intInfo] = "Integer type";
    typeDescriptions[doubleInfo] = "Double precision floating point";
    typeDescriptions[stringInfo] = "String type";

    std::cout << "\nMap with TypeInfo keys:\n";
    for (const auto& [type, description] : typeDescriptions) {
        std::cout << "  - " << type.name() << ": " << description << "\n";
    }

    //--------------------------------------------------------------------------
    // 19. Dynamic Type Operations
    //--------------------------------------------------------------------------
    printSection("Dynamic Type Operations");

    // The type casting and conversion would typically use the TypeInfo system
    // The following demonstrates conceptually how you could use TypeInfo for
    // casting

    void* voidPtr = &intValue;
    int* recoveredIntPtr = nullptr;

    // Check if the type is correct before casting
    auto voidPtrTypeInfo = atom::meta::TypeInfo::create<void*>();
    auto intPtrTypeInfoCheck = atom::meta::TypeInfo::create<int*>();

    if (voidPtrTypeInfo.bareEqual(intPtrTypeInfoCheck)) {
        std::cout << "Type check passed, safe to cast void* to int*\n";
        recoveredIntPtr = static_cast<int*>(voidPtr);
        std::cout << "  - Value after cast: " << *recoveredIntPtr << "\n";
    } else {
        std::cout << "Type check failed, not safe to cast\n";
    }

    //--------------------------------------------------------------------------
    // 20. Exception Handling
    //--------------------------------------------------------------------------
    printSection("Exception Handling");

    // Attempt to use TypeInfo::create with incompatible types
    try {
        // This would cause a compile-time error with consteval,
        // so we're using a runtime example instead
        atom::meta::registerType<int>("AlreadyRegisteredInt");
        atom::meta::registerType<int>("AlreadyRegisteredInt");  // Should throw
        std::cout << "No exception was thrown (unexpected!)\n";
    } catch (const atom::meta::TypeInfoException& e) {
        std::cout << "Caught TypeInfoException as expected: " << e.what()
                  << "\n";
    } catch (const std::exception& e) {
        std::cout << "Caught exception: " << e.what() << "\n";
    }

    std::cout << "\nAll TypeInfo examples completed successfully!\n";
    return 0;
}
