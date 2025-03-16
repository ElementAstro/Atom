#include <gtest/gtest.h>
#include "atom/function/proxy_params.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace atom::meta::test {

// Test fixture for Arg tests
class ArgTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test Arg constructors
TEST_F(ArgTest, Constructors) {
    // Default constructor
    Arg defaultArg;
    EXPECT_TRUE(defaultArg.getName().empty());
    EXPECT_FALSE(defaultArg.getDefaultValue().has_value());

    // Name-only constructor
    Arg nameOnlyArg("param1");
    EXPECT_EQ(nameOnlyArg.getName(), "param1");
    EXPECT_FALSE(nameOnlyArg.getDefaultValue().has_value());

    // Name and value constructor
    Arg intArg("intParam", 42);
    EXPECT_EQ(intArg.getName(), "intParam");
    EXPECT_TRUE(intArg.getDefaultValue().has_value());
    EXPECT_EQ(intArg.getType(), typeid(int));
    EXPECT_EQ(std::any_cast<int>(*intArg.getDefaultValue()), 42);

    // Move constructor
    Arg movedArg(std::move(Arg("moveParam", "moved")));
    EXPECT_EQ(movedArg.getName(), "moveParam");
    EXPECT_TRUE(movedArg.getDefaultValue().has_value());
    EXPECT_EQ(movedArg.getType(), typeid(std::string));
    EXPECT_EQ(std::any_cast<std::string>(*movedArg.getDefaultValue()), "moved");
}

// Test Arg type checking and value access
TEST_F(ArgTest, TypeCheckingAndValueAccess) {
    Arg intArg("intParam", 42);
    EXPECT_TRUE(intArg.isType<int>());
    EXPECT_FALSE(intArg.isType<std::string>());
    EXPECT_FALSE(intArg.isType<double>());

    auto intValue = intArg.getValueAs<int>();
    EXPECT_TRUE(intValue.has_value());
    EXPECT_EQ(*intValue, 42);

    auto stringValue = intArg.getValueAs<std::string>();
    EXPECT_FALSE(stringValue.has_value());

    // Test value setter
    intArg.setValue(100);
    auto newIntValue = intArg.getValueAs<int>();
    EXPECT_TRUE(newIntValue.has_value());
    EXPECT_EQ(*newIntValue, 100);

    // Test setting different type
    intArg.setValue(std::string("changed"));
    EXPECT_TRUE(intArg.isType<std::string>());
    EXPECT_FALSE(intArg.isType<int>());
    auto newStringValue = intArg.getValueAs<std::string>();
    EXPECT_TRUE(newStringValue.has_value());
    EXPECT_EQ(*newStringValue, "changed");
}

// Test JSON serialization/deserialization for Arg
TEST_F(ArgTest, JsonSerialization) {
    // Test with int value
    Arg intArg("intParam", 42);
    nlohmann::json intJson;
    to_json(intJson, intArg);

    EXPECT_EQ(intJson["name"], "intParam");
    EXPECT_EQ(intJson["default_value"], 42);
    EXPECT_TRUE(intJson.contains("type"));

    // Test with string value
    Arg stringArg("stringParam", std::string("hello"));
    nlohmann::json stringJson;
    to_json(stringJson, stringArg);

    EXPECT_EQ(stringJson["name"], "stringParam");
    EXPECT_EQ(stringJson["default_value"], "hello");
    EXPECT_TRUE(stringJson.contains("type"));

    // Test deserialization
    Arg deserializedArg;
    from_json(stringJson, deserializedArg);
    EXPECT_EQ(deserializedArg.getName(), "stringParam");
    EXPECT_TRUE(deserializedArg.getDefaultValue().has_value());
    EXPECT_EQ(std::any_cast<std::string>(*deserializedArg.getDefaultValue()),
              "hello");

    // Test with no default value
    Arg noDefaultArg("noDefault");
    nlohmann::json noDefaultJson;
    to_json(noDefaultJson, noDefaultArg);

    EXPECT_EQ(noDefaultJson["name"], "noDefault");
    EXPECT_EQ(noDefaultJson["default_value"], nullptr);
}

// Test std::any serialization with different types
TEST_F(ArgTest, AnyJsonSerialization) {
    // Test with various types
    std::any intAny = 42;
    nlohmann::json intJson;
    to_json(intJson, intAny);
    EXPECT_EQ(intJson, 42);

    std::any doubleAny = 3.14;
    nlohmann::json doubleJson;
    to_json(doubleJson, doubleAny);
    EXPECT_EQ(doubleJson, 3.14);

    std::any boolAny = true;
    nlohmann::json boolJson;
    to_json(boolJson, boolAny);
    EXPECT_EQ(boolJson, true);

    std::any stringAny = std::string("test");
    nlohmann::json stringJson;
    to_json(stringJson, stringAny);
    EXPECT_EQ(stringJson, "test");

    std::any stringViewAny = std::string_view("test_view");
    nlohmann::json stringViewJson;
    to_json(stringViewJson, stringViewAny);
    EXPECT_EQ(stringViewJson, "test_view");

    std::vector<std::string> strVec{"a", "b", "c"};
    std::any vecAny = strVec;
    nlohmann::json vecJson;
    to_json(vecJson, vecAny);
    EXPECT_EQ(vecJson.size(), 3);
    EXPECT_EQ(vecJson[0], "a");
    EXPECT_EQ(vecJson[1], "b");
    EXPECT_EQ(vecJson[2], "c");
}

// Test std::any deserialization with different types
TEST_F(ArgTest, AnyJsonDeserialization) {
    // Test integer
    nlohmann::json intJson = 42;
    std::any intAny;
    from_json(intJson, intAny);
    EXPECT_EQ(std::any_cast<int>(intAny), 42);

    // Test double
    nlohmann::json doubleJson = 3.14;
    std::any doubleAny;
    from_json(doubleJson, doubleAny);
    EXPECT_DOUBLE_EQ(std::any_cast<double>(doubleAny), 3.14);

    // Test string
    nlohmann::json stringJson = "test";
    std::any stringAny;
    from_json(stringJson, stringAny);
    EXPECT_EQ(std::any_cast<std::string>(stringAny), "test");

    // Test boolean
    nlohmann::json boolJson = true;
    std::any boolAny;
    from_json(boolJson, boolAny);
    EXPECT_EQ(std::any_cast<bool>(boolAny), true);

    // Test array
    nlohmann::json arrayJson = {"a", "b", "c"};
    std::any arrayAny;
    from_json(arrayJson, arrayAny);
    auto strVec = std::any_cast<std::vector<std::string>>(arrayAny);
    EXPECT_EQ(strVec.size(), 3);
    EXPECT_EQ(strVec[0], "a");
    EXPECT_EQ(strVec[1], "b");
    EXPECT_EQ(strVec[2], "c");

    // Test empty array
    nlohmann::json emptyArrayJson = nlohmann::json::array();
    std::any emptyArrayAny;
    from_json(emptyArrayJson, emptyArrayAny);
    auto emptyVec = std::any_cast<std::vector<std::string>>(emptyArrayAny);
    EXPECT_TRUE(emptyVec.empty());

    // Test null
    nlohmann::json nullJson = nullptr;
    std::any nullAny = 42;  // Set to non-null to verify nullification
    from_json(nullJson, nullAny);
    // Can't directly test null std::any, but it should not throw
}

// Test error cases for JSON serialization/deserialization
TEST_F(ArgTest, JsonErrorCases) {
    // Test serialization of unsupported type
    struct UnsupportedType {};
    std::any unsupportedAny = UnsupportedType{};
    nlohmann::json errorJson;
    EXPECT_THROW(to_json(errorJson, unsupportedAny), ProxyTypeError);

    // Test deserialization of unsupported JSON type
    nlohmann::json objectJson = {{"key", "value"}};
    std::any objectAny;
    EXPECT_THROW(from_json(objectJson, objectAny), ProxyTypeError);
}

// Test fixture for FunctionParams tests
class FunctionParamsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup common test data
        intArg = Arg("intParam", 42);
        stringArg = Arg("stringParam", std::string("hello"));
        boolArg = Arg("boolParam", true);
        doubleArg = Arg("doubleParam", 3.14);
    }

    void TearDown() override {}

    Arg intArg;
    Arg stringArg;
    Arg boolArg;
    Arg doubleArg;
};

// Test FunctionParams constructors
TEST_F(FunctionParamsTest, Constructors) {
    // Default constructor
    FunctionParams emptyParams;
    EXPECT_TRUE(emptyParams.empty());
    EXPECT_EQ(emptyParams.size(), 0);

    // Single Arg constructor
    FunctionParams singleParams(intArg);
    EXPECT_FALSE(singleParams.empty());
    EXPECT_EQ(singleParams.size(), 1);
    EXPECT_EQ(singleParams[0].getName(), "intParam");

    // Range constructor
    std::vector<Arg> argVec{intArg, stringArg, boolArg};
    FunctionParams rangeParams(argVec);
    EXPECT_EQ(rangeParams.size(), 3);
    EXPECT_EQ(rangeParams[0].getName(), "intParam");
    EXPECT_EQ(rangeParams[1].getName(), "stringParam");
    EXPECT_EQ(rangeParams[2].getName(), "boolParam");

    // Initializer list constructor
    FunctionParams initListParams{intArg, stringArg};
    EXPECT_EQ(initListParams.size(), 2);
    EXPECT_EQ(initListParams[0].getName(), "intParam");
    EXPECT_EQ(initListParams[1].getName(), "stringParam");

    // Move constructor
    FunctionParams movedParams(std::move(FunctionParams{intArg, stringArg}));
    EXPECT_EQ(movedParams.size(), 2);
    EXPECT_EQ(movedParams[0].getName(), "intParam");
    EXPECT_EQ(movedParams[1].getName(), "stringParam");
}

// Test access operators
TEST_F(FunctionParamsTest, AccessOperators) {
    FunctionParams params{intArg, stringArg, boolArg};

    // Test const access
    const FunctionParams& constParams = params;
    EXPECT_EQ(constParams[0].getName(), "intParam");
    EXPECT_EQ(constParams[1].getName(), "stringParam");
    EXPECT_EQ(constParams[2].getName(), "boolParam");

    // Test non-const access and modification
    params[0].setValue(100);
    auto intValue = params[0].getValueAs<int>();
    EXPECT_TRUE(intValue.has_value());
    EXPECT_EQ(*intValue, 100);

    // Test out of range
    EXPECT_THROW(params[3], std::out_of_range);
    EXPECT_THROW(constParams[3], std::out_of_range);
}

// Test iterator methods
TEST_F(FunctionParamsTest, IteratorMethods) {
    FunctionParams params{intArg, stringArg, boolArg};

    // Test begin/end with range-based for loop
    std::vector<std::string> names;
    for (const auto& arg : params) {
        names.push_back(arg.getName());
    }

    EXPECT_EQ(names.size(), 3);
    EXPECT_EQ(names[0], "intParam");
    EXPECT_EQ(names[1], "stringParam");
    EXPECT_EQ(names[2], "boolParam");

    // Test begin/end with STL algorithms
    auto findResult = std::find_if(
        params.begin(), params.end(),
        [](const Arg& arg) { return arg.getName() == "stringParam"; });

    EXPECT_NE(findResult, params.end());
    EXPECT_EQ(findResult->getName(), "stringParam");
}

// Test front/back methods
TEST_F(FunctionParamsTest, FrontBackMethods) {
    FunctionParams params{intArg, stringArg, boolArg};

    // Test front
    EXPECT_EQ(params.front().getName(), "intParam");

    // Test back
    EXPECT_EQ(params.back().getName(), "boolParam");

    // Test empty case
    FunctionParams emptyParams;
    EXPECT_THROW(emptyParams.front(), std::out_of_range);
    EXPECT_THROW(emptyParams.back(), std::out_of_range);
}

// Test modification methods
TEST_F(FunctionParamsTest, ModificationMethods) {
    // Test push_back
    FunctionParams params;
    params.push_back(intArg);
    EXPECT_EQ(params.size(), 1);
    EXPECT_EQ(params[0].getName(), "intParam");

    params.push_back(stringArg);
    EXPECT_EQ(params.size(), 2);
    EXPECT_EQ(params[1].getName(), "stringParam");

    // Test emplace_back
    params.emplace_back("emplaceParam", 123);
    EXPECT_EQ(params.size(), 3);
    EXPECT_EQ(params[2].getName(), "emplaceParam");
    auto emplaceValue = params[2].getValueAs<int>();
    EXPECT_TRUE(emplaceValue.has_value());
    EXPECT_EQ(*emplaceValue, 123);

    // Test clear
    params.clear();
    EXPECT_TRUE(params.empty());
    EXPECT_EQ(params.size(), 0);

    // Test reserve and resize
    params.reserve(5);
    params.push_back(intArg);
    params.push_back(stringArg);
    params.resize(4);
    EXPECT_EQ(params.size(), 4);
    EXPECT_EQ(params[0].getName(), "intParam");
    EXPECT_EQ(params[1].getName(), "stringParam");
    // params[2] and params[3] are default-constructed Args
}

// Test vector conversion
TEST_F(FunctionParamsTest, VectorConversion) {
    FunctionParams params{intArg, stringArg, boolArg};

    // Test toVector
    const auto& vec = params.toVector();
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0].getName(), "intParam");
    EXPECT_EQ(vec[1].getName(), "stringParam");
    EXPECT_EQ(vec[2].getName(), "boolParam");

    // Test toAnyVector
    auto anyVec = params.toAnyVector();
    EXPECT_EQ(anyVec.size(), 3);
    EXPECT_EQ(std::any_cast<int>(anyVec[0]), 42);
    EXPECT_EQ(std::any_cast<std::string>(anyVec[1]), "hello");
    EXPECT_EQ(std::any_cast<bool>(anyVec[2]), true);
}

// Test name-based lookup
TEST_F(FunctionParamsTest, NameBasedLookup) {
    FunctionParams params{intArg, stringArg, boolArg};

    // Test getByName
    auto stringArgOpt = params.getByName("stringParam");
    EXPECT_TRUE(stringArgOpt.has_value());
    EXPECT_EQ(stringArgOpt->getName(), "stringParam");
    auto stringValue = stringArgOpt->getValueAs<std::string>();
    EXPECT_TRUE(stringValue.has_value());
    EXPECT_EQ(*stringValue, "hello");

    // Test getByName for non-existent name
    auto notFoundOpt = params.getByName("notFound");
    EXPECT_FALSE(notFoundOpt.has_value());

    // Test getByNameRef
    Arg* stringArgPtr = params.getByNameRef("stringParam");
    EXPECT_NE(stringArgPtr, nullptr);
    EXPECT_EQ(stringArgPtr->getName(), "stringParam");

    // Test getByNameRef for non-existent name
    Arg* notFoundPtr = params.getByNameRef("notFound");
    EXPECT_EQ(notFoundPtr, nullptr);

    // Modify parameter through pointer
    stringArgPtr->setValue(std::string("modified"));
    auto modifiedValue =
        params.getByName("stringParam")->getValueAs<std::string>();
    EXPECT_TRUE(modifiedValue.has_value());
    EXPECT_EQ(*modifiedValue, "modified");
}

// Test slice operation
TEST_F(FunctionParamsTest, SliceOperation) {
    FunctionParams params{intArg, stringArg, boolArg, doubleArg};

    // Test valid slice
    auto sliced = params.slice(1, 3);
    EXPECT_EQ(sliced.size(), 2);
    EXPECT_EQ(sliced[0].getName(), "stringParam");
    EXPECT_EQ(sliced[1].getName(), "boolParam");

    // Test slice to end
    auto toEnd = params.slice(2, 4);
    EXPECT_EQ(toEnd.size(), 2);
    EXPECT_EQ(toEnd[0].getName(), "boolParam");
    EXPECT_EQ(toEnd[1].getName(), "doubleParam");

    // Test empty slice
    auto empty = params.slice(1, 1);
    EXPECT_TRUE(empty.empty());

    // Test invalid slices
    EXPECT_THROW(params.slice(3, 2), std::out_of_range);  // start > end
    EXPECT_THROW(params.slice(1, 5), std::out_of_range);  // end > size
}

// Test filter operation
TEST_F(FunctionParamsTest, FilterOperation) {
    FunctionParams params{intArg, stringArg, boolArg, doubleArg};

    // Filter by name
    auto nameFiltered = params.filter([](const Arg& arg) {
        return arg.getName().find("Param") != std::string::npos;
    });
    EXPECT_EQ(nameFiltered.size(), 4);  // All have "Param" in name

    // Filter by type
    auto typeFiltered = params.filter([](const Arg& arg) {
        return arg.getType() == typeid(int) || arg.getType() == typeid(double);
    });
    EXPECT_EQ(typeFiltered.size(), 2);

    // Check if correct Args were filtered
    bool hasInt = false;
    bool hasDouble = false;
    for (const auto& arg : typeFiltered) {
        if (arg.getName() == "intParam")
            hasInt = true;
        if (arg.getName() == "doubleParam")
            hasDouble = true;
    }
    EXPECT_TRUE(hasInt && hasDouble);

    // Empty filter result
    auto emptyFiltered = params.filter([](const Arg&) { return false; });
    EXPECT_TRUE(emptyFiltered.empty());
}

// Test set operation
TEST_F(FunctionParamsTest, SetOperation) {
    FunctionParams params{intArg, stringArg};

    // Test set with copy
    Arg newArg("newParam", 123.456f);
    params.set(0, newArg);
    EXPECT_EQ(params[0].getName(), "newParam");
    auto floatValue = params[0].getValueAs<float>();
    EXPECT_TRUE(floatValue.has_value());
    EXPECT_FLOAT_EQ(*floatValue, 123.456f);

    // Test set with move
    params.set(1, Arg("movedParam", std::string("moved")));
    EXPECT_EQ(params[1].getName(), "movedParam");
    auto stringValue = params[1].getValueAs<std::string>();
    EXPECT_TRUE(stringValue.has_value());
    EXPECT_EQ(*stringValue, "moved");

    // Test out of range
    EXPECT_THROW(params.set(2, newArg), std::out_of_range);
}

// Test type-safe value access
TEST_F(FunctionParamsTest, TypeSafeValueAccess) {
    FunctionParams params{intArg, stringArg, boolArg, doubleArg};

    // Test getValueAs
    auto intValue = params.getValueAs<int>(0);
    EXPECT_TRUE(intValue.has_value());
    EXPECT_EQ(*intValue, 42);

    auto stringValue = params.getValueAs<std::string>(1);
    EXPECT_TRUE(stringValue.has_value());
    EXPECT_EQ(*stringValue, "hello");

    // Test wrong type
    auto wrongType = params.getValueAs<double>(0);  // int param as double
    EXPECT_FALSE(wrongType.has_value());

    // Test out of range
    auto outOfRange = params.getValueAs<int>(10);
    EXPECT_FALSE(outOfRange.has_value());

    // Test getValue with default
    EXPECT_EQ(params.getValue<int>(0, -1), 42);
    EXPECT_EQ(params.getValue<int>(10, -1), -1);  // Out of range, use default
    EXPECT_EQ(params.getValue<double>(0, 3.14),
              3.14);  // Wrong type, use default
}

// Test string_view optimization
TEST_F(FunctionParamsTest, StringViewOptimization) {
    // Test with std::string
    Arg stringArg("stringParam", std::string("hello"));
    FunctionParams params{stringArg};

    auto strView = params.getStringView(0);
    EXPECT_TRUE(strView.has_value());
    EXPECT_EQ(*strView, "hello");

    // Test with const char*
    Arg charPtrArg("charPtrParam", "direct");
    params.push_back(charPtrArg);

    auto charPtrView = params.getStringView(1);
    EXPECT_TRUE(charPtrView.has_value());
    EXPECT_EQ(*charPtrView, "direct");

    // Test with string_view
    Arg stringViewArg("stringViewParam", std::string_view("viewtest"));
    params.push_back(stringViewArg);

    auto stringViewResult = params.getStringView(2);
    EXPECT_TRUE(stringViewResult.has_value());
    EXPECT_EQ(*stringViewResult, "viewtest");

    // Test with non-string type
    Arg intArg("intParam", 42);
    params.push_back(intArg);

    auto nonStringView = params.getStringView(3);
    EXPECT_FALSE(nonStringView.has_value());

    // Test with out of range
    auto outOfRangeView = params.getStringView(10);
    EXPECT_FALSE(outOfRangeView.has_value());
}

// Test JSON serialization for FunctionParams
TEST_F(FunctionParamsTest, JsonSerialization) {
    FunctionParams params{intArg, stringArg, boolArg};

    // Test toJson
    auto json = params.toJson();
    EXPECT_EQ(json.size(), 3);
    EXPECT_EQ(json[0]["name"], "intParam");
    EXPECT_EQ(json[0]["default_value"], 42);
    EXPECT_EQ(json[1]["name"], "stringParam");
    EXPECT_EQ(json[1]["default_value"], "hello");
    EXPECT_EQ(json[2]["name"], "boolParam");
    EXPECT_EQ(json[2]["default_value"], true);

    // Test fromJson
    auto deserializedParams = FunctionParams::fromJson(json);
    EXPECT_EQ(deserializedParams.size(), 3);
    EXPECT_EQ(deserializedParams[0].getName(), "intParam");
    EXPECT_EQ(deserializedParams[1].getName(), "stringParam");
    EXPECT_EQ(deserializedParams[2].getName(), "boolParam");

    auto deserializedInt = deserializedParams.getValueAs<int>(0);
    EXPECT_TRUE(deserializedInt.has_value());
    EXPECT_EQ(*deserializedInt, 42);

    auto deserializedString = deserializedParams.getValueAs<std::string>(1);
    EXPECT_TRUE(deserializedString.has_value());
    EXPECT_EQ(*deserializedString, "hello");

    auto deserializedBool = deserializedParams.getValueAs<bool>(2);
    EXPECT_TRUE(deserializedBool.has_value());
    EXPECT_EQ(*deserializedBool, true);
}

// Test complex usage scenarios
TEST_F(FunctionParamsTest, ComplexUsageScenarios) {
    // Create a complex parameter set
    FunctionParams params;
    params.emplace_back("name", std::string("test_function"));
    params.emplace_back("timeout", 5000);
    params.emplace_back("retry", true);
    params.emplace_back("options",
                        std::vector<std::string>{"opt1", "opt2", "opt3"});

    // Access nested vector type
    auto options = params.getValueAs<std::vector<std::string>>(3);
    EXPECT_TRUE(options.has_value());
    EXPECT_EQ(options->size(), 3);
    EXPECT_EQ((*options)[0], "opt1");
    EXPECT_EQ((*options)[1], "opt2");
    EXPECT_EQ((*options)[2], "opt3");

    // Filter only boolean parameters
    auto boolParams =
        params.filter([](const Arg& arg) { return arg.isType<bool>(); });
    EXPECT_EQ(boolParams.size(), 1);
    EXPECT_EQ(boolParams[0].getName(), "retry");

    // JSON serialization and roundtrip
    auto json = params.toJson();
    auto roundtrippedParams = FunctionParams::fromJson(json);

    EXPECT_EQ(roundtrippedParams.size(), 4);

    // Verify nested vector survived the roundtrip
    auto roundtrippedOptions =
        roundtrippedParams.getValueAs<std::vector<std::string>>(3);
    EXPECT_TRUE(roundtrippedOptions.has_value());
    EXPECT_EQ(roundtrippedOptions->size(), 3);
    EXPECT_EQ((*roundtrippedOptions)[0], "opt1");
    EXPECT_EQ((*roundtrippedOptions)[1], "opt2");
    EXPECT_EQ((*roundtrippedOptions)[2], "opt3");
}

}  // namespace atom::meta::test

// Main function to run the tests
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}