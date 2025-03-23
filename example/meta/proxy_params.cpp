#include "atom/meta/proxy_params.hpp"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// Custom struct to demonstrate handling user types
struct Point {
    int x;
    int y;

    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

// Helper function to print JSON with indentation
void printJson(const nlohmann::json& j) {
    std::cout << std::setw(4) << j << std::endl;
}

// Helper function to demonstrate error handling
template <typename Func>
void tryOperation(const std::string& description, Func operation) {
    std::cout << "Attempting: " << description << std::endl;
    try {
        operation();
        std::cout << "  Success!" << std::endl;
    } catch (const atom::meta::ProxyTypeError& e) {
        std::cout << "  ProxyTypeError: " << e.what() << std::endl;
    } catch (const atom::meta::ProxyArgumentError& e) {
        std::cout << "  ProxyArgumentError: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  Exception: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "=============================================\n";
    std::cout << "Proxy Parameters Library Usage Examples\n";
    std::cout << "=============================================\n\n";

    // 1. Basic Arg construction and usage
    std::cout << "1. BASIC ARG CREATION AND USAGE\n";
    std::cout << "-------------------------------------------\n";

    // Default constructor
    atom::meta::Arg emptyArg;
    std::cout << "Empty arg name: " << emptyArg.getName() << std::endl;

    // Name-only constructor
    atom::meta::Arg nameOnlyArg("param1");
    std::cout << "Name-only arg: " << nameOnlyArg.getName() << std::endl;

    // Full constructor with default value
    atom::meta::Arg intArg("count", 42);
    std::cout << "Int arg name: " << intArg.getName() << std::endl;

    // Different types of arguments
    atom::meta::Arg stringArg("message", std::string("Hello, World!"));
    atom::meta::Arg doubleArg("price", 99.99);
    atom::meta::Arg boolArg("enabled", true);
    atom::meta::Arg stringViewArg("view", std::string_view("String view"));

    // Vector arguments
    std::vector<int> nums{1, 2, 3, 4, 5};
    atom::meta::Arg vectorArg("numbers", nums);

    std::cout << std::endl;

    // 2. Type-safe value access and manipulation
    std::cout << "2. TYPE-SAFE VALUE ACCESS\n";
    std::cout << "-------------------------------------------\n";

    // Getting typed values
    auto intValue = intArg.getValueAs<int>();
    if (intValue) {
        std::cout << "Int value: " << *intValue << std::endl;
    }

    auto stringValue = stringArg.getValueAs<std::string>();
    if (stringValue) {
        std::cout << "String value: " << *stringValue << std::endl;
    }

    // Setting new values
    std::cout << "Setting new values..." << std::endl;
    intArg.setValue(100);
    stringArg.setValue(std::string("Updated message"));

    // Verify updated values
    auto updatedIntValue = intArg.getValueAs<int>();
    auto updatedStringValue = stringArg.getValueAs<std::string>();

    if (updatedIntValue) {
        std::cout << "Updated int value: " << *updatedIntValue << std::endl;
    }

    if (updatedStringValue) {
        std::cout << "Updated string value: " << *updatedStringValue
                  << std::endl;
    }

    std::cout << std::endl;

    // 3. Type checking
    std::cout << "3. TYPE CHECKING\n";
    std::cout << "-------------------------------------------\n";

    std::cout << "intArg is int: " << intArg.isType<int>() << std::endl;
    std::cout << "intArg is string: " << intArg.isType<std::string>()
              << std::endl;
    std::cout << "stringArg is string: " << stringArg.isType<std::string>()
              << std::endl;
    std::cout << "doubleArg is double: " << doubleArg.isType<double>()
              << std::endl;
    std::cout << "vectorArg is vector<int>: "
              << vectorArg.isType<std::vector<int>>() << std::endl;

    // Type information
    std::cout << "intArg type: " << intArg.getType().name() << std::endl;
    std::cout << "stringArg type: " << stringArg.getType().name() << std::endl;

    std::cout << std::endl;

    // 4. JSON serialization of Arg
    std::cout << "4. ARG JSON SERIALIZATION\n";
    std::cout << "-------------------------------------------\n";

    // Convert single arguments to JSON
    nlohmann::json intArgJson;
    to_json(intArgJson, intArg);
    std::cout << "intArg as JSON:" << std::endl;
    printJson(intArgJson);

    nlohmann::json stringArgJson;
    to_json(stringArgJson, stringArg);
    std::cout << "stringArg as JSON:" << std::endl;
    printJson(stringArgJson);

    nlohmann::json vectorArgJson;
    to_json(vectorArgJson, vectorArg);
    std::cout << "vectorArg as JSON:" << std::endl;
    printJson(vectorArgJson);

    // Deserialize JSON back to Arg
    atom::meta::Arg deserializedArg;
    from_json(intArgJson, deserializedArg);

    std::cout << "Deserialized arg name: " << deserializedArg.getName()
              << std::endl;
    auto deserializedValue = deserializedArg.getValueAs<int>();
    if (deserializedValue) {
        std::cout << "Deserialized value: " << *deserializedValue << std::endl;
    }

    std::cout << std::endl;

    // 5. FunctionParams creation
    std::cout << "5. FUNCTION PARAMS CREATION\n";
    std::cout << "-------------------------------------------\n";

    // Default constructor
    atom::meta::FunctionParams emptyParams;
    std::cout << "Empty params size: " << emptyParams.size() << std::endl;

    // Single arg constructor
    atom::meta::FunctionParams singleArgParams(intArg);
    std::cout << "Single arg params size: " << singleArgParams.size()
              << std::endl;

    // Initializer list constructor
    atom::meta::FunctionParams paramsFromList{intArg, stringArg, doubleArg,
                                              boolArg};
    std::cout << "Params from list size: " << paramsFromList.size()
              << std::endl;

    // Vector constructor
    std::vector<atom::meta::Arg> argVector{intArg, stringArg, doubleArg};
    atom::meta::FunctionParams paramsFromVector(argVector);
    std::cout << "Params from vector size: " << paramsFromVector.size()
              << std::endl;

    std::cout << std::endl;

    // 6. Accessing elements in FunctionParams
    std::cout << "6. ACCESSING ELEMENTS IN FUNCTIONPARAMS\n";
    std::cout << "-------------------------------------------\n";

    // Indexing
    std::cout << "First param name: " << paramsFromList[0].getName()
              << std::endl;
    std::cout << "Second param name: " << paramsFromList[1].getName()
              << std::endl;

    // Front and back
    std::cout << "Front param name: " << paramsFromList.front().getName()
              << std::endl;
    std::cout << "Back param name: " << paramsFromList.back().getName()
              << std::endl;

    // Iterator access
    std::cout << "All params using iterators:" << std::endl;
    for (const auto& arg : paramsFromList) {
        std::cout << "  " << arg.getName();
        if (arg.getDefaultValue()) {
            if (arg.isType<int>()) {
                std::cout << " = " << *arg.getValueAs<int>();
            } else if (arg.isType<std::string>()) {
                std::cout << " = \"" << *arg.getValueAs<std::string>() << "\"";
            } else if (arg.isType<double>()) {
                std::cout << " = " << *arg.getValueAs<double>();
            } else if (arg.isType<bool>()) {
                std::cout << " = "
                          << (*arg.getValueAs<bool>() ? "true" : "false");
            }
        }
        std::cout << std::endl;
    }

    // Error handling with out-of-bounds access
    tryOperation("Access out-of-bounds index", [&paramsFromList]() {
        auto value = paramsFromList[10];  // This should throw
    });

    std::cout << std::endl;

    // 7. Modifying FunctionParams
    std::cout << "7. MODIFYING FUNCTIONPARAMS\n";
    std::cout << "-------------------------------------------\n";

    atom::meta::FunctionParams mutableParams;

    // Reserve space
    mutableParams.reserve(5);
    std::cout << "Reserved size, actual size: " << mutableParams.size()
              << std::endl;

    // Push back
    mutableParams.push_back(intArg);
    mutableParams.push_back(stringArg);
    std::cout << "After push_back, size: " << mutableParams.size() << std::endl;

    // Emplace back
    mutableParams.emplace_back("newParam", 123);
    std::cout << "After emplace_back, size: " << mutableParams.size()
              << std::endl;
    std::cout << "New param name: " << mutableParams.back().getName()
              << std::endl;

    // Set - update existing value
    atom::meta::Arg updatedArg("updatedParam", "Updated value");
    mutableParams.set(0, updatedArg);
    std::cout << "After set, first param name: " << mutableParams[0].getName()
              << std::endl;

    // Resize
    mutableParams.resize(5);
    std::cout << "After resize, size: " << mutableParams.size() << std::endl;

    // Clear
    mutableParams.clear();
    std::cout << "After clear, size: " << mutableParams.size() << std::endl;
    std::cout << "After clear, empty: "
              << (mutableParams.empty() ? "true" : "false") << std::endl;

    std::cout << std::endl;

    // 8. Search and filter operations
    std::cout << "8. SEARCH AND FILTER OPERATIONS\n";
    std::cout << "-------------------------------------------\n";

    // Test data
    atom::meta::FunctionParams searchParams{
        atom::meta::Arg("id", 1001),
        atom::meta::Arg("name", std::string("John")),
        atom::meta::Arg("age", 30), atom::meta::Arg("salary", 50000.0),
        atom::meta::Arg("active", true)};

    // Get by name
    auto nameParam = searchParams.getByName("name");
    if (nameParam) {
        auto name = nameParam->getValueAs<std::string>();
        std::cout << "Found parameter 'name' with value: "
                  << (name ? *name : "not found") << std::endl;
    } else {
        std::cout << "Parameter 'name' not found" << std::endl;
    }

    // Get by name ref (mutable)
    atom::meta::Arg* ageParamRef = searchParams.getByNameRef("age");
    if (ageParamRef) {
        // Modify the parameter directly
        auto oldAge = ageParamRef->getValueAs<int>();
        std::cout << "Found parameter 'age' with value: "
                  << (oldAge ? *oldAge : 0) << std::endl;

        ageParamRef->setValue(31);
        auto newAge = ageParamRef->getValueAs<int>();
        std::cout << "Updated 'age' value: " << (newAge ? *newAge : 0)
                  << std::endl;
    }

    // Filter parameters
    auto numericParams = searchParams.filter([](const atom::meta::Arg& arg) {
        return arg.isType<int>() || arg.isType<double>();
    });

    std::cout << "Numeric parameters found: " << numericParams.size()
              << std::endl;
    for (const auto& arg : numericParams) {
        std::cout << "  " << arg.getName() << std::endl;
    }

    std::cout << std::endl;

    // 9. Conversion operations
    std::cout << "9. CONVERSION OPERATIONS\n";
    std::cout << "-------------------------------------------\n";

    // Convert to vector
    std::vector<atom::meta::Arg> argVec = searchParams.toVector();
    std::cout << "Converted to vector, size: " << argVec.size() << std::endl;

    // Convert to any vector
    std::vector<std::any> anyVec = searchParams.toAnyVector();
    std::cout << "Converted to std::any vector, size: " << anyVec.size()
              << std::endl;

    // Type-safe value access
    auto idValue = searchParams.getValueAs<int>(0);
    std::cout << "ID value: " << (idValue ? *idValue : 0) << std::endl;

    // Get value with default
    int notFoundValue = searchParams.getValue<int>(10, -1);
    std::cout << "Not found value with default: " << notFoundValue << std::endl;

    // Get string view for performance
    auto nameView = searchParams.getStringView(1);
    if (nameView) {
        std::cout << "Name as string_view: " << *nameView << std::endl;
    }

    std::cout << std::endl;

    // 10. Slicing and filtering
    std::cout << "10. SLICING AND FILTERING\n";
    std::cout << "-------------------------------------------\n";

    // Test data
    atom::meta::FunctionParams sliceParams{
        atom::meta::Arg("param0", 0), atom::meta::Arg("param1", 1),
        atom::meta::Arg("param2", 2), atom::meta::Arg("param3", 3),
        atom::meta::Arg("param4", 4)};

    // Slice operations
    auto sliced = sliceParams.slice(1, 4);
    std::cout << "Sliced params size: " << sliced.size() << std::endl;

    std::cout << "Sliced params:" << std::endl;
    for (size_t i = 0; i < sliced.size(); i++) {
        auto value = sliced.getValueAs<int>(i);
        std::cout << "  " << sliced[i].getName() << " = "
                  << (value ? *value : 0) << std::endl;
    }

    // Error handling with invalid slice range
    tryOperation("Slice with invalid range", [&sliceParams]() {
        auto badSlice = sliceParams.slice(3, 10);  // This should throw
    });

    std::cout << std::endl;

    // 11. JSON serialization of FunctionParams
    std::cout << "11. FUNCTIONPARAMS JSON SERIALIZATION\n";
    std::cout << "-------------------------------------------\n";

    // Convert to JSON
    nlohmann::json paramsJson = searchParams.toJson();
    std::cout << "FunctionParams as JSON:" << std::endl;
    printJson(paramsJson);

    // Deserialize from JSON
    atom::meta::FunctionParams deserializedParams =
        atom::meta::FunctionParams::fromJson(paramsJson);

    std::cout << "Deserialized params size: " << deserializedParams.size()
              << std::endl;
    std::cout << "Deserialized params:" << std::endl;
    for (const auto& arg : deserializedParams) {
        std::cout << "  " << arg.getName();
        if (arg.getDefaultValue()) {
            if (arg.isType<int>()) {
                std::cout << " = " << *arg.getValueAs<int>();
            } else if (arg.isType<std::string>()) {
                std::cout << " = \"" << *arg.getValueAs<std::string>() << "\"";
            } else if (arg.isType<double>()) {
                std::cout << " = " << *arg.getValueAs<double>();
            } else if (arg.isType<bool>()) {
                std::cout << " = "
                          << (*arg.getValueAs<bool>() ? "true" : "false");
            }
        }
        std::cout << std::endl;
    }

    std::cout << std::endl;

    // 12. Error handling
    std::cout << "12. ERROR HANDLING\n";
    std::cout << "-------------------------------------------\n";

    // Test data
    atom::meta::FunctionParams errorParams{
        atom::meta::Arg("int_param", 42),
        atom::meta::Arg("string_param", std::string("text"))};

    // Type error handling
    tryOperation("Get int as string", [&errorParams]() {
        auto badCast = errorParams[0].getValueAs<std::string>();
        if (badCast) {
            std::cout << "Value: " << *badCast << std::endl;
        }
    });

    // Boundary checking
    tryOperation("Access out of bounds", [&errorParams]() {
        auto badAccess = errorParams[5];  // This should throw
    });

    // Empty container operations
    atom::meta::FunctionParams emptyContainer;
    tryOperation("Call front() on empty container", [&emptyContainer]() {
        auto frontValue = emptyContainer.front();  // This should throw
    });

    tryOperation("Call back() on empty container", [&emptyContainer]() {
        auto backValue = emptyContainer.back();  // This should throw
    });

    std::cout << std::endl;

    // 13. Advanced usage with complex types
    std::cout << "13. ADVANCED USAGE WITH COMPLEX TYPES\n";
    std::cout << "-------------------------------------------\n";

    // Creating complex parameter combinations
    atom::meta::FunctionParams complexParams;

    // Nested vectors
    std::vector<std::vector<int>> matrix = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};

    // Add parameters using different methods
    complexParams.emplace_back("matrix", std::any(matrix));
    complexParams.emplace_back("command", std::string("process"));
    complexParams.emplace_back("verbose", true);

    // Vector of strings
    std::vector<std::string> tags = {"important", "urgent", "review"};
    complexParams.emplace_back("tags", tags);

    std::cout << "Complex params size: " << complexParams.size() << std::endl;

    // Get tags vector
    auto tagsValue = complexParams.getValueAs<std::vector<std::string>>(3);
    if (tagsValue) {
        std::cout << "Tags: ";
        for (const auto& tag : *tagsValue) {
            std::cout << tag << " ";
        }
        std::cout << std::endl;
    }

    // Serialization test
    nlohmann::json complexJson = complexParams.toJson();
    std::cout << "Complex params as JSON:" << std::endl;
    printJson(complexJson);

    return 0;
}