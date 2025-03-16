/*!
 * \file test_signature.hpp
 * \brief Unit tests for the signature parsing library
 * \author GitHub Copilot
 * \date 2023-09-15
 */

#include <gtest/gtest.h>
#include "atom/function/signature.hpp"

#include <future>
#include <string>
#include <thread>

namespace atom::meta::test {

// Helper function to check expected parsing errors
void expectParsingError(std::string_view definition,
                        ParsingErrorCode expectedCode) {
    auto result = parseFunctionDefinition(definition);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().error().code, expectedCode);
}

class SignatureTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear registry before each test
        SignatureRegistry::instance().clearCache();
    }
};

//------------------------------------------------------------------------------
// Basic parsing tests
//------------------------------------------------------------------------------

TEST_F(SignatureTest, BasicSignatureParsing) {
    auto result = parseFunctionDefinition("def add(a: int, b: int) -> int");
    ASSERT_TRUE(result.has_value());

    auto& signature = result.value();
    EXPECT_EQ(signature.getName(), "add");

    auto params = signature.getParameters();
    ASSERT_EQ(params.size(), 2);

    EXPECT_EQ(params[0].name, "a");
    EXPECT_EQ(params[0].type, "int");
    EXPECT_FALSE(params[0].hasDefaultValue);

    EXPECT_EQ(params[1].name, "b");
    EXPECT_EQ(params[1].type, "int");
    EXPECT_FALSE(params[1].hasDefaultValue);

    ASSERT_TRUE(signature.getReturnType().has_value());
    EXPECT_EQ(*signature.getReturnType(), "int");

    EXPECT_EQ(signature.getModifiers(), FunctionModifier::None);
    EXPECT_FALSE(signature.isTemplated());
    EXPECT_FALSE(signature.isInline());
    EXPECT_FALSE(signature.isStatic());
    EXPECT_FALSE(signature.isExplicit());
}

TEST_F(SignatureTest, SignatureWithDefaultValues) {
    auto result = parseFunctionDefinition(
        "def greet(name: string = \"World\", prefix: string = \"Hello\") -> "
        "string");
    ASSERT_TRUE(result.has_value());

    auto params = result.value().getParameters();
    ASSERT_EQ(params.size(), 2);

    EXPECT_TRUE(params[0].hasDefaultValue);
    ASSERT_TRUE(params[0].defaultValue.has_value());
    EXPECT_EQ(*params[0].defaultValue, "\"World\"");

    EXPECT_TRUE(params[1].hasDefaultValue);
    ASSERT_TRUE(params[1].defaultValue.has_value());
    EXPECT_EQ(*params[1].defaultValue, "\"Hello\"");
}

TEST_F(SignatureTest, SignatureWithComplexTypes) {
    auto result = parseFunctionDefinition(
        "def process(data: vector<int>, config: map<string, any>) -> "
        "tuple<int, string>");
    ASSERT_TRUE(result.has_value());

    auto params = result.value().getParameters();
    EXPECT_EQ(params[0].type, "vector<int>");
    EXPECT_EQ(params[1].type, "map<string, any>");

    ASSERT_TRUE(result.value().getReturnType().has_value());
    EXPECT_EQ(*result.value().getReturnType(), "tuple<int, string>");
}

TEST_F(SignatureTest, SignatureWithoutReturnType) {
    auto result = parseFunctionDefinition("def notify(message: string)");
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().getReturnType().has_value());
}

//------------------------------------------------------------------------------
// Function modifiers tests
//------------------------------------------------------------------------------

TEST_F(SignatureTest, ConstModifier) {
    auto result = parseFunctionDefinition("def getData() const -> vector<int>");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().getModifiers(), FunctionModifier::Const);
}

TEST_F(SignatureTest, NoexceptModifier) {
    auto result = parseFunctionDefinition("def safeOperation() noexcept");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().getModifiers(), FunctionModifier::Noexcept);
}

TEST_F(SignatureTest, ConstNoexceptModifier) {
    auto result =
        parseFunctionDefinition("def readOnly() const noexcept -> int");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().getModifiers(), FunctionModifier::ConstNoexcept);
}

TEST_F(SignatureTest, VirtualOverrideFinalModifiers) {
    auto result1 = parseFunctionDefinition("virtual def baseMethod()");
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value().getModifiers(), FunctionModifier::Virtual);

    auto result2 = parseFunctionDefinition("def derivedMethod() override");
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value().getModifiers(), FunctionModifier::Override);

    auto result3 = parseFunctionDefinition("def finalMethod() final");
    ASSERT_TRUE(result3.has_value());
    EXPECT_EQ(result3.value().getModifiers(), FunctionModifier::Final);
}

TEST_F(SignatureTest, FunctionSpecifiers) {
    auto result1 = parseFunctionDefinition("inline def fastFunction(x: int)");
    ASSERT_TRUE(result1.has_value());
    EXPECT_TRUE(result1.value().isInline());

    auto result2 = parseFunctionDefinition("static def classMethod()");
    ASSERT_TRUE(result2.has_value());
    EXPECT_TRUE(result2.value().isStatic());

    auto result3 = parseFunctionDefinition("explicit def conversion(val: int)");
    ASSERT_TRUE(result3.has_value());
    EXPECT_TRUE(result3.value().isExplicit());

    // Multiple specifiers
    auto result4 =
        parseFunctionDefinition("static inline def optimizedClassMethod()");
    ASSERT_TRUE(result4.has_value());
    EXPECT_TRUE(result4.value().isStatic());
    EXPECT_TRUE(result4.value().isInline());
}

//------------------------------------------------------------------------------
// Template tests
//------------------------------------------------------------------------------

TEST_F(SignatureTest, TemplateFunction) {
    auto result = parseFunctionDefinition(
        "template<typename T> def identity(val: T) -> T");
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(result.value().isTemplated());
    ASSERT_TRUE(result.value().getTemplateParameters().has_value());
    EXPECT_EQ(*result.value().getTemplateParameters(), "typename T");
}

TEST_F(SignatureTest, ComplexTemplateFunction) {
    auto result = parseFunctionDefinition(
        "template<typename T, typename U = int> def convert(val: T) -> U");
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(result.value().isTemplated());
    ASSERT_TRUE(result.value().getTemplateParameters().has_value());
    EXPECT_EQ(*result.value().getTemplateParameters(),
              "typename T, typename U = int");
}

//------------------------------------------------------------------------------
// Doc comment tests
//------------------------------------------------------------------------------

TEST_F(SignatureTest, DocCommentParsing) {
    std::string sig =
        "def multiply(x: int, y: int) -> int /** @brief Multiplies two "
        "numbers\n"
        " * @param x First operand\n"
        " * @param y Second operand\n"
        " * @return Product of the two inputs\n"
        " */";

    auto result = parseFunctionDefinition(sig);
    ASSERT_TRUE(result.has_value());

    auto& signature = result.value();
    ASSERT_TRUE(signature.getDocComment().has_value());

    const DocComment& doc = *signature.getDocComment();
    EXPECT_TRUE(doc.hasTag("brief"));
    EXPECT_TRUE(doc.hasTag("param"));
    EXPECT_TRUE(doc.hasTag("return"));

    ASSERT_TRUE(doc.getTag("brief").has_value());
    EXPECT_EQ(*doc.getTag("brief"), "Multiplies two numbers");

    ASSERT_TRUE(doc.getTag("param").has_value());
    EXPECT_EQ(*doc.getTag("param"), "x First operand");

    ASSERT_TRUE(doc.getTag("return").has_value());
    EXPECT_EQ(*doc.getTag("return"), "Product of the two inputs");
}

TEST_F(SignatureTest, DocCommentWithMultipleTags) {
    std::string sig =
        "def process(data: any) -> bool /** @brief Process data\n"
        " * @param data Input data to process\n"
        " * @throws InvalidDataException if data is invalid\n"
        " * @see otherFunction\n"
        " * @return True if successful\n"
        " */";

    auto result = parseFunctionDefinition(sig);
    ASSERT_TRUE(result.has_value());

    auto& signature = result.value();
    ASSERT_TRUE(signature.getDocComment().has_value());

    const DocComment& doc = *signature.getDocComment();
    EXPECT_TRUE(doc.hasTag("throws"));
    EXPECT_TRUE(doc.hasTag("see"));

    ASSERT_TRUE(doc.getTag("throws").has_value());
    EXPECT_EQ(*doc.getTag("throws"), "InvalidDataException if data is invalid");
}

//------------------------------------------------------------------------------
// Error handling tests
//------------------------------------------------------------------------------

TEST_F(SignatureTest, ErrorInvalidPrefix) {
    expectParsingError("function add(a: int, b: int)",
                       ParsingErrorCode::InvalidPrefix);
}

TEST_F(SignatureTest, ErrorMissingFunctionName) {
    expectParsingError("def (a: int, b: int) -> int",
                       ParsingErrorCode::MissingFunctionName);
}

TEST_F(SignatureTest, ErrorMissingOpenParenthesis) {
    expectParsingError("def add a: int, b: int -> int",
                       ParsingErrorCode::MissingOpenParenthesis);
}

TEST_F(SignatureTest, ErrorMissingCloseParenthesis) {
    expectParsingError("def add(a: int, b: int -> int",
                       ParsingErrorCode::MissingCloseParenthesis);
}

TEST_F(SignatureTest, ErrorUnbalancedBrackets) {
    expectParsingError(
        "def process(data: vector<int, options: map<string, any>) -> bool",
        ParsingErrorCode::UnbalancedBrackets);
}

TEST_F(SignatureTest, ErrorMalformedTemplate) {
    expectParsingError("template<typename T def identity(val: T) -> T",
                       ParsingErrorCode::InvalidPrefix);
}

//------------------------------------------------------------------------------
// SignatureRegistry tests
//------------------------------------------------------------------------------

TEST_F(SignatureTest, RegistryCaching) {
    auto& registry = SignatureRegistry::instance();
    std::string sig = "def add(a: int, b: int) -> int";

    // First registration should parse the signature
    auto result1 = registry.registerSignature(sig);
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(registry.getCacheSize(), 1);

    // Second registration of the same signature should use the cache
    auto result2 = registry.registerSignature(sig);
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(registry.getCacheSize(), 1);

    // Different signature should add a new entry to the cache
    auto result3 =
        registry.registerSignature("def subtract(a: int, b: int) -> int");
    ASSERT_TRUE(result3.has_value());
    EXPECT_EQ(registry.getCacheSize(), 2);
}

TEST_F(SignatureTest, RegistryCacheClearing) {
    auto& registry = SignatureRegistry::instance();

    registry.registerSignature("def a()");
    registry.registerSignature("def b()");
    EXPECT_EQ(registry.getCacheSize(), 2);

    registry.clearCache();
    EXPECT_EQ(registry.getCacheSize(), 0);
}

//------------------------------------------------------------------------------
// Thread safety tests
//------------------------------------------------------------------------------

TEST_F(SignatureTest, RegistryThreadSafety) {
    auto& registry = SignatureRegistry::instance();
    registry.clearCache();

    constexpr int numThreads = 10;
    constexpr int numSigsPerThread = 100;

    auto threadFunc = [&registry](int id) {
        for (int i = 0; i < numSigsPerThread; ++i) {
            std::string sig =
                "def func" + std::to_string(id * numSigsPerThread + i) + "()";
            registry.registerSignature(sig);

            // Sometimes use the same signature to test cache hits
            if (i % 3 == 0) {
                registry.registerSignature(sig);
            }
        }
    };

    std::vector<std::future<void>> futures;
    for (int i = 0; i < numThreads; ++i) {
        futures.push_back(std::async(std::launch::async, threadFunc, i));
    }

    for (auto& future : futures) {
        future.wait();
    }

    // Each thread creates numSigsPerThread unique signatures
    EXPECT_EQ(registry.getCacheSize(), numThreads * numSigsPerThread);
}

//------------------------------------------------------------------------------
// Edge case tests
//------------------------------------------------------------------------------

TEST_F(SignatureTest, EmptyParameters) {
    auto result = parseFunctionDefinition("def noParams() -> void");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().getParameters().size(), 0);
}

TEST_F(SignatureTest, WhitespaceHandling) {
    auto result = parseFunctionDefinition(
        "def  spacey  (  a : int  ,  b : int  )  ->  int  ");
    ASSERT_TRUE(result.has_value());

    auto& signature = result.value();
    EXPECT_EQ(signature.getName(), "spacey");

    auto params = signature.getParameters();
    ASSERT_EQ(params.size(), 2);
    EXPECT_EQ(params[0].name, "a");
    EXPECT_EQ(params[1].name, "b");
}

TEST_F(SignatureTest, ParameterWithoutType) {
    auto result = parseFunctionDefinition("def implicitType(x)");
    ASSERT_TRUE(result.has_value());

    auto params = result.value().getParameters();
    ASSERT_EQ(params.size(), 1);
    EXPECT_EQ(params[0].name, "x");
    EXPECT_EQ(params[0].type, "any");  // Default type
}

TEST_F(SignatureTest, ComplexDefaultValues) {
    auto result = parseFunctionDefinition(
        "def complex(arr: vector<int> = {1, 2, 3}, options: map<string, any> = "
        "{\"key\": value})");
    ASSERT_TRUE(result.has_value());

    auto params = result.value().getParameters();
    ASSERT_EQ(params.size(), 2);

    EXPECT_TRUE(params[0].hasDefaultValue);
    ASSERT_TRUE(params[0].defaultValue.has_value());
    EXPECT_EQ(*params[0].defaultValue, "{1, 2, 3}");

    EXPECT_TRUE(params[1].hasDefaultValue);
    ASSERT_TRUE(params[1].defaultValue.has_value());
    EXPECT_EQ(*params[1].defaultValue, "{\"key\": value}");
}

//------------------------------------------------------------------------------
// toString method tests
//------------------------------------------------------------------------------

TEST_F(SignatureTest, ToStringBasic) {
    auto result = parseFunctionDefinition("def add(a: int, b: int) -> int");
    ASSERT_TRUE(result.has_value());

    std::string str = result.value().toString();
    EXPECT_TRUE(str.find("add") != std::string::npos);
    EXPECT_TRUE(str.find("a: int") != std::string::npos);
    EXPECT_TRUE(str.find("b: int") != std::string::npos);
    EXPECT_TRUE(str.find("int") != std::string::npos);
}

TEST_F(SignatureTest, ToStringWithModifiers) {
    auto result = parseFunctionDefinition(
        "static inline def multiply(x: int, y: int) const -> int");
    ASSERT_TRUE(result.has_value());

    std::string str = result.value().toString();
    EXPECT_TRUE(str.find("static") != std::string::npos);
    EXPECT_TRUE(str.find("inline") != std::string::npos);
    EXPECT_TRUE(str.find("multiply") != std::string::npos);
    EXPECT_TRUE(str.find("const") != std::string::npos);
}

TEST_F(SignatureTest, ToStringWithDefaultValues) {
    auto result = parseFunctionDefinition(
        "def config(timeout: int = 30, retry: bool = true)");
    ASSERT_TRUE(result.has_value());

    std::string str = result.value().toString();
    EXPECT_TRUE(str.find("timeout: int = 30") != std::string::npos);
    EXPECT_TRUE(str.find("retry: bool = true") != std::string::npos);
}

//------------------------------------------------------------------------------
// Parameter comparison test
//------------------------------------------------------------------------------

TEST_F(SignatureTest, ParameterComparison) {
    Parameter p1{"name", "string", false, std::nullopt};
    Parameter p2{"name", "string", false, std::nullopt};
    Parameter p3{"name", "int", false, std::nullopt};
    Parameter p4{"other", "string", false, std::nullopt};
    Parameter p5{"name", "string", true,
                 std::make_optional(std::string_view{"default"})};

    EXPECT_EQ(p1, p2);
    EXPECT_NE(p1, p3);
    EXPECT_NE(p1, p4);
    EXPECT_NE(p1, p5);
}

}  // namespace atom::meta::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}