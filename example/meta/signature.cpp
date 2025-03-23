#include "atom/meta/signature.hpp"
#include <cassert>
#include <iostream>
#include <string>

// Helper functions to make output prettier
void printHeader(const std::string& title) {
    std::cout << "\n============================================\n";
    std::cout << title << "\n";
    std::cout << "============================================\n";
}

void printSubHeader(const std::string& title) {
    std::cout << "\n--------------------------------------------\n";
    std::cout << title << "\n";
    std::cout << "--------------------------------------------\n";
}

void printParameter(const atom::meta::Parameter& param) {
    std::cout << "  - Name: " << param.name << "\n";
    std::cout << "    Type: " << param.type << "\n";

    if (param.hasDefaultValue && param.defaultValue) {
        std::cout << "    Default value: " << *param.defaultValue << "\n";
    } else {
        std::cout << "    No default value\n";
    }
}

void printDocComment(const std::optional<atom::meta::DocComment>& docComment) {
    if (!docComment) {
        std::cout << "  No documentation\n";
        return;
    }

    std::cout << "  Raw comment: " << docComment->raw << "\n";
    std::cout << "  Tags:\n";

    for (const auto& [tag, value] : docComment->tags) {
        std::cout << "    @" << tag << ": " << value << "\n";
    }
}

std::string modifierToString(atom::meta::FunctionModifier modifier) {
    using FM = atom::meta::FunctionModifier;

    switch (modifier) {
        case FM::None:
            return "None";
        case FM::Const:
            return "Const";
        case FM::Noexcept:
            return "Noexcept";
        case FM::ConstNoexcept:
            return "ConstNoexcept";
        case FM::Virtual:
            return "Virtual";
        case FM::Override:
            return "Override";
        case FM::Final:
            return "Final";
        default:
            return "Unknown";
    }
}

std::string errorCodeToString(atom::meta::ParsingErrorCode code) {
    using PEC = atom::meta::ParsingErrorCode;

    switch (code) {
        case PEC::InvalidPrefix:
            return "InvalidPrefix";
        case PEC::MissingFunctionName:
            return "MissingFunctionName";
        case PEC::MissingOpenParenthesis:
            return "MissingOpenParenthesis";
        case PEC::MissingCloseParenthesis:
            return "MissingCloseParenthesis";
        case PEC::MalformedParameters:
            return "MalformedParameters";
        case PEC::MalformedReturnType:
            return "MalformedReturnType";
        case PEC::UnbalancedBrackets:
            return "UnbalancedBrackets";
        case PEC::InternalError:
            return "InternalError";
        default:
            return "Unknown";
    }
}

// Function to test parsing a signature
void testParse(const std::string& description, std::string_view signature) {
    std::cout << "\n>>> Testing: " << description << "\n";
    std::cout << "Signature: " << signature << "\n\n";

    auto result = atom::meta::parseFunctionDefinition(signature);

    if (result) {
        const auto& sig = result.value();

        std::cout << "PARSING SUCCESSFUL\n";
        std::cout << "Function name: " << sig.getName() << "\n";

        std::cout << "Return type: ";
        if (sig.getReturnType()) {
            std::cout << *sig.getReturnType() << "\n";
        } else {
            std::cout << "None (void)\n";
        }

        std::cout << "Modifiers: " << modifierToString(sig.getModifiers())
                  << "\n";

        if (sig.isTemplated()) {
            std::cout << "Is templated: Yes\n";
            if (sig.getTemplateParameters()) {
                std::cout << "Template parameters: "
                          << *sig.getTemplateParameters() << "\n";
            }
        } else {
            std::cout << "Is templated: No\n";
        }

        std::cout << "Is inline: " << (sig.isInline() ? "Yes" : "No") << "\n";
        std::cout << "Is static: " << (sig.isStatic() ? "Yes" : "No") << "\n";
        std::cout << "Is explicit: " << (sig.isExplicit() ? "Yes" : "No")
                  << "\n";

        std::cout << "Parameters (" << sig.getParameters().size() << "):\n";
        for (const auto& param : sig.getParameters()) {
            printParameter(param);
        }

        std::cout << "Documentation:\n";
        printDocComment(sig.getDocComment());

        std::cout << "Reconstructed signature: " << sig.toString() << "\n";
    } else {
        const auto& error = result.error();
        std::cout << "PARSING FAILED\n";
        // 修复：直接输出错误对象本身，因为Error<ParsingError>没有code/message/position成员
        std::cout << "Error occurred during parsing\n";
    }

    std::cout << "--------------------------------------\n";
}

int main() {
    printHeader("SIGNATURE PARSING LIBRARY EXAMPLES");

    // 1. Basic Function Signatures
    printSubHeader("1. BASIC FUNCTION SIGNATURES");

    // Simple function with no parameters
    testParse("Simple function with no parameters", "def simple_function()");

    // Function with return type
    testParse("Function with return type", "def add(a: int, b: int) -> int");

    // Function with multiple parameters
    testParse("Function with multiple parameters",
              "def process(name: string, age: int, height: float) -> bool");

    // Function with default parameter values
    testParse(
        "Function with default parameter values",
        "def greet(name: string, greeting: string = \"Hello\") -> string");

    // 2. Function Modifiers
    printSubHeader("2. FUNCTION MODIFIERS");

    // Const function
    testParse("Const function", "def getValue() const -> int");

    // Noexcept function
    testParse("Noexcept function", "def critical() noexcept -> bool");

    // Const noexcept function
    testParse("Const noexcept function",
              "def safeRead() const noexcept -> int");

    // Virtual function
    testParse("Virtual function", "def virtual process(data: string) -> void");

    // Override function
    testParse("Override function", "def getData() override -> string");

    // Final function
    testParse("Final function", "def calculate() final -> double");

    // 3. Function Specifiers
    printSubHeader("3. FUNCTION SPECIFIERS");

    // Inline function
    testParse("Inline function", "def inline quick() -> void");

    // Static function
    testParse("Static function", "def static factory(type: string) -> object");

    // Explicit function (constructor)
    testParse("Explicit function",
              "def explicit convert(value: int) -> string");

    // 4. Templated Functions
    printSubHeader("4. TEMPLATED FUNCTIONS");

    // Simple template
    testParse("Simple template",
              "template<typename T> def transform(input: T) -> T");

    // Complex template
    testParse(
        "Complex template",
        "template<typename T, typename U = int> def convert(from: T) -> U");

    // 5. Documentation Comments
    printSubHeader("5. DOCUMENTATION COMMENTS");

    // Function with documentation
    testParse("Function with documentation",
              "/** \n * @brief Adds two numbers together\n * @param a First "
              "number\n * @param b Second number\n * @return Sum of a and b\n "
              "*/ def add(a: int, b: int) -> int");

    // 6. Complex Parameter Types
    printSubHeader("6. COMPLEX PARAMETER TYPES");

    // Function with array parameter
    testParse("Function with array parameter",
              "def processArray(values: int[]) -> int");

    // Function with template parameter
    testParse("Function with template parameter",
              "def processContainer(items: vector<string>) -> size_t");

    // Function with complex nested template parameter
    testParse(
        "Function with complex nested template parameter",
        "def process(data: map<string, vector<pair<int, string>>>) -> void");

    // 7. Error Handling
    printSubHeader("7. ERROR HANDLING");

    // Missing prefix
    testParse("Missing prefix", "function test() -> void");

    // Missing function name
    testParse("Missing function name", "def () -> void");

    // Missing open parenthesis
    testParse("Missing open parenthesis", "def functionName -> void");

    // Missing close parenthesis
    testParse("Missing close parenthesis",
              "def functionName(a: int, b: int -> void");

    // Unbalanced brackets
    testParse("Unbalanced brackets", "def process(data: vector<int) -> void");

    // 8. SignatureRegistry
    printSubHeader("8. SIGNATURE REGISTRY");

    // Get the singleton instance
    auto& registry = atom::meta::SignatureRegistry::instance();

    std::cout << "Initial cache size: " << registry.getCacheSize() << "\n";

    // Register a signature
    auto regResult1 = registry.registerSignature(
        "def cached_function(a: int, b: string) -> bool");

    if (regResult1) {
        // 修复：使用点运算符(.)而不是箭头运算符(->)
        std::cout << "Successfully registered: " << regResult1.value().getName()
                  << "\n";
        std::cout << "Cache size after first registration: "
                  << registry.getCacheSize() << "\n";

        // Register the same signature again (should be cached)
        auto regResult2 = registry.registerSignature(
            "def cached_function(a: int, b: string) -> bool");

        if (regResult2) {
            std::cout << "Successfully retrieved from cache\n";
            std::cout << "Cache size after retrieving from cache: "
                      << registry.getCacheSize() << "\n";

            // Verify it's the same function
            // 修复: 使用点运算符访问值
            assert(regResult1.value().getName() ==
                   regResult2.value().getName());
            std::cout << "Verified cached signature is identical\n";
        }

        // Register a different signature
        auto regResult3 =
            registry.registerSignature("def another_function() -> void");

        if (regResult3) {
            std::cout << "Successfully registered another function\n";
            std::cout << "Cache size after second registration: "
                      << registry.getCacheSize() << "\n";
        }

        // Clear the cache
        registry.clearCache();
        std::cout << "Cache cleared. New size: " << registry.getCacheSize()
                  << "\n";
    } else {
        // 修复：不访问Error类型的message成员
        std::cout << "Failed to register signature\n";
    }

    // 9. Real-world Examples
    printSubHeader("9. REAL-WORLD EXAMPLES");

    // Constructor example
    testParse("Constructor",
              "def explicit DataProcessor(config: Configuration, maxSize: "
              "size_t = 1024)");

    // Method with complex return type
    testParse("Method with complex return type",
              "def processData(input: vector<string>) -> pair<bool, "
              "vector<Result>> const");

    // Method with complex documentation
    testParse(
        "Method with complex documentation",
        "/**\n * @brief Processes a batch of transactions\n * @param "
        "transactions List of transactions to process\n * @param options "
        "Processing options\n * @param callback Callback function to call for "
        "each transaction\n * @return A tuple containing the number of "
        "successful transactions and a vector of failed transactions\n * "
        "@throws TransactionException If a critical error occurs\n */ def "
        "processBatch(transactions: vector<Transaction>, options: "
        "ProcessingOptions, callback: function<void(Transaction)>) -> "
        "tuple<int, vector<Transaction>> noexcept");

    // Full class method example
    testParse("Full class method example",
              "def virtual processImage(image: Image, filters: vector<Filter> "
              "= {}) -> shared_ptr<ProcessedImage> const override");

    // 10. Combined Features Example
    printSubHeader("10. COMBINED FEATURES EXAMPLE");

    // Complex function with all features
    testParse(
        "Complex function with all features",
        "/**\n * @brief Optimized matrix multiplication algorithm\n * @param a "
        "First matrix\n * @param b Second matrix\n * @param parallelism Number "
        "of threads to use\n * @return Result matrix\n * @complexity O(n^3)\n "
        "*/ template<typename T> def static inline multiply(a: Matrix<T>, b: "
        "Matrix<T>, parallelism: int = 4) -> Matrix<T> noexcept");

    return 0;
}