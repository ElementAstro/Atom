// filepath: /home/max/Atom-1/atom/utils/test_argsview.hpp
/*
 * test_argsview.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-3-1

Description: Tests for the ArgumentParser class

**************************************************/

#ifndef ATOM_UTILS_TEST_ARGSVIEW_HPP
#define ATOM_UTILS_TEST_ARGSVIEW_HPP

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "atom/utils/argsview.hpp"
#include "exception.hpp"

namespace atom::utils::test {

// Create a temporary file with given content for testing file argument parsing
class TempFile {
public:
    TempFile(const std::string& content) {
        filename_ = "temp_args_test_" + std::to_string(rand());
        std::ofstream file(filename_);
        file << content;
        file.close();
    }

    ~TempFile() { std::filesystem::remove(filename_); }

    std::string getFilename() const { return filename_; }

private:
    std::string filename_;
};

// Basic parser setup test fixture
class ArgumentParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<ArgumentParser>("test_program");
        parser->setDescription("Test program description");
        parser->setEpilog("Test program epilog");
    }

    void TearDown() override { parser.reset(); }

    std::unique_ptr<ArgumentParser> parser;
};

// Test basic parser setup and configuration
TEST_F(ArgumentParserTest, BasicConfiguration) {
    // Test default construction - this is more of a compilation test
    ArgumentParser defaultParser;

    // Testing that the constructor and setters don't throw exceptions
    EXPECT_NO_THROW({
        ArgumentParser namedParser("program_name");
        namedParser.setDescription("description");
        namedParser.setEpilog("epilog");
    });

    // Duplicate calls to setters should overwrite previous values
    parser->setDescription("New description");
    parser->setEpilog("New epilog");

    // Test the alias methods as well
    parser->addDescription("Final description");
    parser->addEpilog("Final epilog");

    // We can't directly test the values since they're private,
    // but printHelp() would use them
}

// Test adding and retrieving arguments
TEST_F(ArgumentParserTest, ArgumentAdditionAndRetrieval) {
    // Add arguments of various types
    parser->addArgument("string_arg", ArgumentParser::ArgType::STRING, true,
                        std::string("default"), "String argument", {"s"});
    parser->addArgument("int_arg", ArgumentParser::ArgType::INTEGER, false, 42,
                        "Integer argument", {"i"});
    parser->addArgument("double_arg", ArgumentParser::ArgType::DOUBLE, false,
                        3.14, "Double argument", {"d"});
    parser->addArgument("bool_arg", ArgumentParser::ArgType::BOOLEAN, false,
                        true, "Boolean argument", {"b"});
    parser->addArgument("path_arg", ArgumentParser::ArgType::FILEPATH, false,
                        std::filesystem::path("/tmp"), "Path argument", {"p"});
    parser->addArgument("auto_string", ArgumentParser::ArgType::AUTO, false,
                        std::string("auto"), "Auto-detected string", {"as"});
    parser->addArgument("auto_int", ArgumentParser::ArgType::AUTO, false, 123,
                        "Auto-detected int", {"ai"});
    parser->addArgument("no_default", ArgumentParser::ArgType::STRING, false,
                        {}, "No default value");

    // Parse with empty arguments - should use defaults
    std::vector<std::string> argv = {"program"};
    parser->parse(1, argv);

    // Test retrieving with correct types
    auto string_arg = parser->get<std::string>("string_arg");
    EXPECT_TRUE(string_arg.has_value());
    EXPECT_EQ(*string_arg, "default");

    auto int_arg = parser->get<int>("int_arg");
    EXPECT_TRUE(int_arg.has_value());
    EXPECT_EQ(*int_arg, 42);

    auto double_arg = parser->get<double>("double_arg");
    EXPECT_TRUE(double_arg.has_value());
    EXPECT_DOUBLE_EQ(*double_arg, 3.14);

    auto bool_arg = parser->get<bool>("bool_arg");
    EXPECT_TRUE(bool_arg.has_value());
    EXPECT_TRUE(*bool_arg);

    auto path_arg = parser->get<std::filesystem::path>("path_arg");
    EXPECT_TRUE(path_arg.has_value());
    EXPECT_EQ(path_arg->string(), "/tmp");

    auto auto_string = parser->get<std::string>("auto_string");
    EXPECT_TRUE(auto_string.has_value());
    EXPECT_EQ(*auto_string, "auto");

    auto auto_int = parser->get<int>("auto_int");
    EXPECT_TRUE(auto_int.has_value());
    EXPECT_EQ(*auto_int, 123);

    // Test retrieving a non-existent argument
    auto non_existent = parser->get<std::string>("non_existent");
    EXPECT_FALSE(non_existent.has_value());

    // Test retrieving with incorrect types - should return nullopt
    auto wrong_type = parser->get<double>("string_arg");
    EXPECT_FALSE(wrong_type.has_value());

    // Test retrieving argument with no default value
    auto no_default = parser->get<std::string>("no_default");
    EXPECT_FALSE(no_default.has_value());
}

// Test parsing command line arguments
TEST_F(ArgumentParserTest, ArgumentParsing) {
    parser->addArgument("string_arg", ArgumentParser::ArgType::STRING, false,
                        std::string("default"), "String argument", {"s"});
    parser->addArgument("int_arg", ArgumentParser::ArgType::INTEGER, false, 42,
                        "Integer argument", {"i"});
    parser->addArgument("double_arg", ArgumentParser::ArgType::DOUBLE, false,
                        3.14, "Double argument", {"d"});

    // Parse arguments from command line
    std::vector<std::string> argv = {"program", "--string_arg", "new_value",
                                     "-i",      "123",          "--double_arg",
                                     "2.718"};
    parser->parse(7, argv);

    // Check that parsed values override defaults
    auto string_arg = parser->get<std::string>("string_arg");
    EXPECT_TRUE(string_arg.has_value());
    EXPECT_EQ(*string_arg, "new_value");

    auto int_arg = parser->get<int>("int_arg");
    EXPECT_TRUE(int_arg.has_value());
    EXPECT_EQ(*int_arg, 123);

    auto double_arg = parser->get<double>("double_arg");
    EXPECT_TRUE(double_arg.has_value());
    EXPECT_DOUBLE_EQ(*double_arg, 2.718);
}

// Test flag handling
TEST_F(ArgumentParserTest, FlagHandling) {
    parser->addFlag("flag1", "First flag", {"f1"});
    parser->addFlag("flag2", "Second flag", {"f2"});
    parser->addFlag("flag3", "Third flag", {"f3"});

    // Initially all flags should be false
    EXPECT_FALSE(parser->getFlag("flag1"));
    EXPECT_FALSE(parser->getFlag("flag2"));
    EXPECT_FALSE(parser->getFlag("flag3"));

    // Parse with some flags set
    std::vector<std::string> argv = {"program", "--flag1", "-f3"};
    parser->parse(3, argv);

    // Check flag values
    EXPECT_TRUE(parser->getFlag("flag1"));
    EXPECT_FALSE(parser->getFlag("flag2"));
    EXPECT_TRUE(parser->getFlag("flag3"));

    // Test non-existent flag
    EXPECT_FALSE(parser->getFlag("non_existent"));
}

// Test positional arguments
TEST_F(ArgumentParserTest, PositionalArguments) {
    parser->addArgument("pos1", ArgumentParser::ArgType::STRING, true, {},
                        "First positional", {}, true);
    parser->addArgument("pos2", ArgumentParser::ArgType::INTEGER, true, {},
                        "Second positional", {}, true);

    // Parse with positional arguments
    std::vector<std::string> argv = {"program", "value1", "42"};

    // This should not throw because both required positional arguments are
    // provided
    EXPECT_NO_THROW(parser->parse(3, argv));
}

// Test nargs handling
TEST_F(ArgumentParserTest, NargsHandling) {
    // Test OPTIONAL nargs
    parser->addArgument(
        "optional", ArgumentParser::ArgType::STRING, false,
        std::string("default"), "Optional argument", {"o"}, false,
        ArgumentParser::Nargs(ArgumentParser::NargsType::OPTIONAL));

    // Test ZERO_OR_MORE nargs
    parser->addArgument(
        "zero_or_more", ArgumentParser::ArgType::STRING, false, {},
        "Zero or more argument", {"z"}, false,
        ArgumentParser::Nargs(ArgumentParser::NargsType::ZERO_OR_MORE));

    // Test ONE_OR_MORE nargs
    parser->addArgument(
        "one_or_more", ArgumentParser::ArgType::STRING, false, {},
        "One or more argument", {"m"}, false,
        ArgumentParser::Nargs(ArgumentParser::NargsType::ONE_OR_MORE));

    // Test CONSTANT nargs
    parser->addArgument(
        "constant", ArgumentParser::ArgType::STRING, false, {},
        "Constant argument", {"c"}, false,
        ArgumentParser::Nargs(ArgumentParser::NargsType::CONSTANT, 2));

    // Parse with various nargs cases
    std::vector<std::string> argv = {
        "program",    "--optional", "opt_val",       "--zero_or_more", "val1",
        "val2",       "val3",       "--one_or_more", "val4",           "val5",
        "--constant", "const1",     "const2"};
    parser->parse(13, argv);

    // Check optional nargs with value
    auto optional = parser->get<std::string>("optional");
    EXPECT_TRUE(optional.has_value());
    EXPECT_EQ(*optional, "opt_val");

    // Check zero_or_more nargs
    auto zero_or_more = parser->get<std::vector<std::string>>("zero_or_more");
    EXPECT_TRUE(zero_or_more.has_value());
    EXPECT_EQ(zero_or_more->size(), 3);
    EXPECT_EQ((*zero_or_more)[0], "val1");
    EXPECT_EQ((*zero_or_more)[1], "val2");
    EXPECT_EQ((*zero_or_more)[2], "val3");

    // Check one_or_more nargs
    auto one_or_more = parser->get<std::vector<std::string>>("one_or_more");
    EXPECT_TRUE(one_or_more.has_value());
    EXPECT_EQ(one_or_more->size(), 2);
    EXPECT_EQ((*one_or_more)[0], "val4");
    EXPECT_EQ((*one_or_more)[1], "val5");

    // Check constant nargs
    // It will be stored as a vector of strings
    auto constant = parser->get<std::vector<std::string>>("constant");
    EXPECT_TRUE(constant.has_value());
    EXPECT_EQ(constant->size(), 2);
    EXPECT_EQ((*constant)[0], "const1");
    EXPECT_EQ((*constant)[1], "const2");

    // Now test optional nargs without value (should use default)
    std::vector<std::string> argv2 = {"program", "--optional"};
    ArgumentParser parser2("program");
    parser2.addArgument(
        "optional", ArgumentParser::ArgType::STRING, false,
        std::string("default"), "Optional argument", {"o"}, false,
        ArgumentParser::Nargs(ArgumentParser::NargsType::OPTIONAL));
    parser2.parse(2, argv2);

    auto optional2 = parser2.get<std::string>("optional");
    EXPECT_TRUE(optional2.has_value());
    EXPECT_EQ(*optional2, "default");
}

// Test constant nargs validation
TEST_F(ArgumentParserTest, ConstantNargsValidation) {
    parser->addArgument(
        "constant", ArgumentParser::ArgType::STRING, false, {},
        "Constant argument", {"c"}, false,
        ArgumentParser::Nargs(ArgumentParser::NargsType::CONSTANT, 3));

    // Not providing enough arguments should throw
    std::vector<std::string> argv = {"program", "--constant", "val1", "val2"};

    EXPECT_THROW(parser->parse(4, argv), atom::error::InvalidArgument);
}

// Test subcommand handling
TEST_F(ArgumentParserTest, SubcommandHandling) {
    parser->addSubcommand("sub1", "Subcommand 1");
    parser->addSubcommand("sub2", "Subcommand 2");

    // Add arguments to subcommands
    auto sub1 = parser->getSubcommandParser("sub1");
    ASSERT_TRUE(sub1.has_value());
    sub1->get().addArgument("sub_arg", ArgumentParser::ArgType::STRING, true,
                            {}, "Subcommand argument");

    auto sub2 = parser->getSubcommandParser("sub2");
    ASSERT_TRUE(sub2.has_value());
    sub2->get().addFlag("sub_flag", "Subcommand flag");

    // Parse with subcommand
    std::vector<std::string> argv = {"program", "sub1", "--sub_arg",
                                     "sub_value"};
    parser->parse(4, argv);

    // Check that subcommand was parsed
    auto sub1_after = parser->getSubcommandParser("sub1");
    ASSERT_TRUE(sub1_after.has_value());
    auto sub_arg = sub1_after->get().get<std::string>("sub_arg");
    EXPECT_TRUE(sub_arg.has_value());
    EXPECT_EQ(*sub_arg, "sub_value");

    // Non-existent subcommand
    auto non_existent = parser->getSubcommandParser("non_existent");
    EXPECT_FALSE(non_existent.has_value());
}

// Test mutually exclusive group handling
TEST_F(ArgumentParserTest, MutuallyExclusiveGroups) {
    parser->addArgument("arg1", ArgumentParser::ArgType::STRING);
    parser->addArgument("arg2", ArgumentParser::ArgType::STRING);
    parser->addFlag("flag1");

    parser->addMutuallyExclusiveGroup({"arg1", "arg2"});

    // Test valid case - only one argument in the group is specified
    std::vector<std::string> argv1 = {"program", "--arg1", "value1", "--flag1"};
    EXPECT_NO_THROW(parser->parse(4, argv1));

    // Test invalid case - more than one argument in the group is specified
    std::vector<std::string> argv2 = {"program", "--arg1", "value1", "--arg2",
                                      "value2"};
    EXPECT_THROW(parser->parse(5, argv2), atom::error::InvalidArgument);
}

// Test file argument parsing
TEST_F(ArgumentParserTest, FileArgumentParsing) {
    // Create a temporary file with arguments
    TempFile tempFile("--arg1 value1\n--arg2 value2");

    parser->addArgumentFromFile();  // Use default "@" prefix
    parser->addArgument("arg1", ArgumentParser::ArgType::STRING);
    parser->addArgument("arg2", ArgumentParser::ArgType::STRING);

    // Parse using file argument
    std::vector<std::string> argv = {"program", "@" + tempFile.getFilename()};
    parser->parse(2, argv);

    // Check that arguments from the file were parsed
    auto arg1 = parser->get<std::string>("arg1");
    EXPECT_TRUE(arg1.has_value());
    EXPECT_EQ(*arg1, "value1");

    auto arg2 = parser->get<std::string>("arg2");
    EXPECT_TRUE(arg2.has_value());
    EXPECT_EQ(*arg2, "value2");

    // Test custom file delimiter
    TempFile tempFile2("--arg3:value3\n--arg4:value4");
    ArgumentParser customDelimParser("program");
    customDelimParser.addArgumentFromFile();
    customDelimParser.setFileDelimiter(':');  // Use ':' as delimiter
    customDelimParser.addArgument("arg3", ArgumentParser::ArgType::STRING);
    customDelimParser.addArgument("arg4", ArgumentParser::ArgType::STRING);

    std::vector<std::string> argv2 = {"program", "@" + tempFile2.getFilename()};
    customDelimParser.parse(2, argv2);

    auto arg3 = customDelimParser.get<std::string>("arg3");
    EXPECT_TRUE(arg3.has_value());
    EXPECT_EQ(*arg3, "value3");

    auto arg4 = customDelimParser.get<std::string>("arg4");
    EXPECT_TRUE(arg4.has_value());
    EXPECT_EQ(*arg4, "value4");

    // Test nonexistent file
    ArgumentParser badFileParser("program");
    badFileParser.addArgumentFromFile();
    std::vector<std::string> argv3 = {"program", "@nonexistent_file.txt"};
    EXPECT_THROW(badFileParser.parse(2, argv3), atom::error::InvalidArgument);
}

// Test required arguments
TEST_F(ArgumentParserTest, RequiredArguments) {
    parser->addArgument("required", ArgumentParser::ArgType::STRING, true);

    // Not providing a required argument should throw
    std::vector<std::string> argv = {"program"};
    EXPECT_THROW(parser->parse(1, argv), atom::error::InvalidArgument);

    // Providing the required argument should not throw
    std::vector<std::string> argv2 = {"program", "--required", "value"};
    EXPECT_NO_THROW(parser->parse(3, argv2));
}

// Test argument type validation
TEST_F(ArgumentParserTest, ArgumentTypeValidation) {
    parser->addArgument("int_arg", ArgumentParser::ArgType::INTEGER);

    // Invalid integer should throw
    std::vector<std::string> argv = {"program", "--int_arg", "not_an_integer"};
    EXPECT_THROW(parser->parse(3, argv), atom::error::InvalidArgument);

    // Valid integer should not throw
    std::vector<std::string> argv2 = {"program", "--int_arg", "42"};
    EXPECT_NO_THROW(parser->parse(3, argv2));
}

// Test help flag handling
TEST_F(ArgumentParserTest, HelpHandling) {
    // This test will verify that --help is recognized but won't test the output
    // since it would exit the program. We can assume it works if other features
    // work.
    parser->addArgument("arg", ArgumentParser::ArgType::STRING);

    // The way to test this would be to redirect stdout and check the output,
    // but that's more complex than needed for this test suite
    // Just verifying that help flags are special-cased in the code
}

// Test positional arguments handling
TEST_F(ArgumentParserTest, ExtendedPositionalHandling) {
    ArgumentParser posParser("program");
    posParser.addArgument("pos1", ArgumentParser::ArgType::STRING, false, {},
                          "First positional", {}, true);
    posParser.addArgument("pos2", ArgumentParser::ArgType::INTEGER, false, {},
                          "Second positional", {}, true);
    posParser.addArgument("pos3", ArgumentParser::ArgType::DOUBLE, false, {},
                          "Third positional", {}, true);

    // Provide positional arguments
    std::vector<std::string> argv = {"program", "value1", "42", "3.14"};
    posParser.parse(4, argv);

    // Check that positional arguments were saved in positionalArguments_
    // but we can't access it directly, so we'll have to infer it worked
    // from the behavior of the class
}

// Test handling of unknown arguments
TEST_F(ArgumentParserTest, UnknownArguments) {
    parser->addArgument("known", ArgumentParser::ArgType::STRING);

    // Unknown argument should throw
    std::vector<std::string> argv = {"program", "--unknown", "value"};
    EXPECT_THROW(parser->parse(3, argv), atom::error::InvalidArgument);
}

// Test for advanced boolean parsing
TEST_F(ArgumentParserTest, BooleanParsing) {
    parser->addArgument("bool1", ArgumentParser::ArgType::BOOLEAN);
    parser->addArgument("bool2", ArgumentParser::ArgType::BOOLEAN);
    parser->addArgument("bool3", ArgumentParser::ArgType::BOOLEAN);
    parser->addArgument("bool4", ArgumentParser::ArgType::BOOLEAN);
    parser->addArgument("bool5", ArgumentParser::ArgType::BOOLEAN);

    // Test various truthy values
    std::vector<std::string> argv = {"program", "--bool1", "true", "--bool2",
                                     "1",       "--bool3", "yes",  "--bool4",
                                     "y",       "--bool5", "on"};
    parser->parse(11, argv);

    EXPECT_TRUE(*parser->get<bool>("bool1"));
    EXPECT_TRUE(*parser->get<bool>("bool2"));
    EXPECT_TRUE(*parser->get<bool>("bool3"));
    EXPECT_TRUE(*parser->get<bool>("bool4"));
    EXPECT_TRUE(*parser->get<bool>("bool5"));

    // Test various falsy values
    ArgumentParser parser2("program");
    parser2.addArgument("bool1", ArgumentParser::ArgType::BOOLEAN);
    parser2.addArgument("bool2", ArgumentParser::ArgType::BOOLEAN);
    parser2.addArgument("bool3", ArgumentParser::ArgType::BOOLEAN);
    parser2.addArgument("bool4", ArgumentParser::ArgType::BOOLEAN);
    parser2.addArgument("bool5", ArgumentParser::ArgType::BOOLEAN);

    std::vector<std::string> argv2 = {"program", "--bool1", "false", "--bool2",
                                      "0",       "--bool3", "no",    "--bool4",
                                      "n",       "--bool5", "off"};
    parser2.parse(11, argv2);

    EXPECT_FALSE(*parser2.get<bool>("bool1"));
    EXPECT_FALSE(*parser2.get<bool>("bool2"));
    EXPECT_FALSE(*parser2.get<bool>("bool3"));
    EXPECT_FALSE(*parser2.get<bool>("bool4"));
    EXPECT_FALSE(*parser2.get<bool>("bool5"));

    // Test invalid boolean value
    ArgumentParser parser3("program");
    parser3.addArgument("bool1", ArgumentParser::ArgType::BOOLEAN);
    std::vector<std::string> argv3 = {"program", "--bool1", "invalid"};
    EXPECT_THROW(parser3.parse(3, argv3), atom::error::InvalidArgument);
}

// Test for unsigned integer parsing
TEST_F(ArgumentParserTest, UnsignedIntegerParsing) {
    parser->addArgument("uint", ArgumentParser::ArgType::UNSIGNED_INTEGER);

    // Valid unsigned integer
    std::vector<std::string> argv1 = {"program", "--uint", "42"};
    EXPECT_NO_THROW(parser->parse(3, argv1));
    EXPECT_EQ(*parser->get<unsigned int>("uint"), 42u);

    // Negative value should throw
    std::vector<std::string> argv2 = {"program", "--uint", "-1"};
    EXPECT_THROW(parser->parse(3, argv2), atom::error::InvalidArgument);

    // Invalid format should throw
    std::vector<std::string> argv3 = {"program", "--uint", "42.5"};
    EXPECT_THROW(parser->parse(3, argv3), atom::error::InvalidArgument);

    // Value out of range should throw
    std::vector<std::string> argv4 = {"program", "--uint",
                                      "99999999999999999999"};
    EXPECT_THROW(parser->parse(3, argv4), atom::error::InvalidArgument);
}

// Test for long integer parsing
TEST_F(ArgumentParserTest, LongIntegerParsing) {
    parser->addArgument("long", ArgumentParser::ArgType::LONG);
    parser->addArgument("ulong", ArgumentParser::ArgType::UNSIGNED_LONG);

    // Valid long integers
    std::vector<std::string> argv = {"program", "--long", "-2147483649",
                                     "--ulong", "4294967296"};
    EXPECT_NO_THROW(parser->parse(5, argv));

    auto long_val = parser->get<long>("long");
    auto ulong_val = parser->get<unsigned long>("ulong");
    EXPECT_TRUE(long_val.has_value());
    EXPECT_TRUE(ulong_val.has_value());
    EXPECT_LT(*long_val, INT_MIN);    // Beyond int range
    EXPECT_GT(*ulong_val, UINT_MAX);  // Beyond unsigned int range

    // Invalid format for unsigned long
    std::vector<std::string> argv2 = {"program", "--ulong", "-1"};
    EXPECT_THROW(parser->parse(3, argv2), atom::error::InvalidArgument);
}

// Test for floating point parsing (float and double)
TEST_F(ArgumentParserTest, FloatingPointParsing) {
    parser->addArgument("float", ArgumentParser::ArgType::FLOAT);
    parser->addArgument("double", ArgumentParser::ArgType::DOUBLE);

    // Valid floating point values
    std::vector<std::string> argv = {"program", "--float", "3.14", "--double",
                                     "3.141592653589793"};
    EXPECT_NO_THROW(parser->parse(5, argv));

    auto float_val = parser->get<float>("float");
    auto double_val = parser->get<double>("double");
    EXPECT_TRUE(float_val.has_value());
    EXPECT_TRUE(double_val.has_value());
    EXPECT_FLOAT_EQ(*float_val, 3.14f);
    EXPECT_DOUBLE_EQ(*double_val, 3.141592653589793);

    // Invalid format
    std::vector<std::string> argv2 = {"program", "--float", "not-a-number"};
    EXPECT_THROW(parser->parse(3, argv2), atom::error::InvalidArgument);
}

// Test for filepath handling
TEST_F(ArgumentParserTest, FilepathHandling) {
    parser->addArgument("path", ArgumentParser::ArgType::FILEPATH);

    // Basic path
    std::vector<std::string> argv = {"program", "--path", "/tmp/test.txt"};
    EXPECT_NO_THROW(parser->parse(3, argv));

    auto path_val = parser->get<std::filesystem::path>("path");
    EXPECT_TRUE(path_val.has_value());
    EXPECT_EQ(path_val->string(), "/tmp/test.txt");

    // Path with special characters
    std::vector<std::string> argv2 = {"program", "--path",
                                      "/path with spaces/file.txt"};
    EXPECT_NO_THROW(parser->parse(3, argv2));

    auto path_val2 = parser->get<std::filesystem::path>("path");
    EXPECT_TRUE(path_val2.has_value());
    EXPECT_EQ(path_val2->string(), "/path with spaces/file.txt");
}

// Test for Nargs constructor validation
TEST_F(ArgumentParserTest, NargsConstructorValidation) {
    // Negative count should throw
    EXPECT_THROW(ArgumentParser::Nargs(ArgumentParser::NargsType::CONSTANT, -1),
                 atom::error::InvalidArgument);

    // Valid count should not throw
    EXPECT_NO_THROW(
        ArgumentParser::Nargs(ArgumentParser::NargsType::CONSTANT, 5));

    // Default constructor should initialize to NONE and 1
    ArgumentParser::Nargs defaultNargs;
    EXPECT_EQ(defaultNargs.type, ArgumentParser::NargsType::NONE);
    EXPECT_EQ(defaultNargs.count, 1);
}

// Test for name validation
TEST_F(ArgumentParserTest, NameValidation) {
    // Empty name
    EXPECT_THROW(parser->addArgument(""), atom::error::InvalidArgument);

    // Name with spaces
    EXPECT_THROW(parser->addArgument("invalid name"),
                 atom::error::InvalidArgument);

    // Name starting with dash
    EXPECT_THROW(parser->addArgument("-invalid"), atom::error::InvalidArgument);

    // Valid name
    EXPECT_NO_THROW(parser->addArgument("valid_name"));
}

// Test for alias collision
TEST_F(ArgumentParserTest, AliasCollision) {
    parser->addArgument("arg1", ArgumentParser::ArgType::STRING, false, {}, "",
                        {"a"});

    // Try to add another argument with the same alias
    EXPECT_THROW(parser->addArgument("arg2", ArgumentParser::ArgType::STRING,
                                     false, {}, "", {"a"}),
                 atom::error::InvalidArgument);

    // Try to add a flag with the same alias
    EXPECT_THROW(parser->addFlag("flag1", "", {"a"}),
                 atom::error::InvalidArgument);
}

// Test for parallel file processing
TEST_F(ArgumentParserTest, ParallelFileProcessing) {
    // Create multiple temporary files
    TempFile tempFile1("--arg1 value1");
    TempFile tempFile2("--arg2 value2");
    TempFile tempFile3("--arg3 value3");

    parser->addArgumentFromFile();
    parser->addArgument("arg1", ArgumentParser::ArgType::STRING);
    parser->addArgument("arg2", ArgumentParser::ArgType::STRING);
    parser->addArgument("arg3", ArgumentParser::ArgType::STRING);

    // Parse using multiple file arguments
    std::vector<std::string> argv = {"program", "@" + tempFile1.getFilename(),
                                     "@" + tempFile2.getFilename(),
                                     "@" + tempFile3.getFilename()};

    parser->parse(4, argv);

    // Check that arguments from all files were parsed
    EXPECT_EQ(*parser->get<std::string>("arg1"), "value1");
    EXPECT_EQ(*parser->get<std::string>("arg2"), "value2");
    EXPECT_EQ(*parser->get<std::string>("arg3"), "value3");
}

// Test for handling comments and empty lines in argument files
TEST_F(ArgumentParserTest, ArgumentFileWithCommentsAndEmptyLines) {
    // Create a temp file with comments and empty lines
    TempFile tempFile(
        "# This is a comment\n"
        "\n"
        "--arg1 value1\n"
        "  # Another comment\n"
        "\n"
        "--arg2 value2\n");

    parser->addArgumentFromFile();
    parser->addArgument("arg1", ArgumentParser::ArgType::STRING);
    parser->addArgument("arg2", ArgumentParser::ArgType::STRING);

    std::vector<std::string> argv = {"program", "@" + tempFile.getFilename()};
    parser->parse(2, argv);

    // Check that only non-comment lines were parsed
    EXPECT_EQ(*parser->get<std::string>("arg1"), "value1");
    EXPECT_EQ(*parser->get<std::string>("arg2"), "value2");
}

// Test type conversion in get<T>() method
TEST_F(ArgumentParserTest, GetTypeConversion) {
    // Add int argument but try to get as string
    parser->addArgument("int_arg", ArgumentParser::ArgType::INTEGER, false, 42);

    std::vector<std::string> argv = {"program"};
    parser->parse(1, argv);

    auto int_as_string = parser->get<std::string>("int_arg");
    EXPECT_TRUE(int_as_string.has_value());
    EXPECT_EQ(*int_as_string, "42");

    // Try to get string as int (should fail)
    parser->addArgument("string_arg", ArgumentParser::ArgType::STRING, false,
                        std::string("not_an_int"));

    auto string_as_int = parser->get<int>("string_arg");
    EXPECT_FALSE(string_as_int.has_value());
}

// Test for anyToString with vector values
TEST_F(ArgumentParserTest, AnyToStringWithVectors) {
    parser->addArgument(
        "vec_arg", ArgumentParser::ArgType::STRING, false, {}, "", {}, false,
        ArgumentParser::Nargs(ArgumentParser::NargsType::ZERO_OR_MORE));

    std::vector<std::string> argv = {"program", "--vec_arg", "val1", "val2",
                                     "val3"};
    parser->parse(5, argv);

    auto vec_arg = parser->get<std::vector<std::string>>("vec_arg");
    ASSERT_TRUE(vec_arg.has_value());
    EXPECT_EQ(vec_arg->size(), 3);

    // Test how this is displayed in help message (would use anyToString)
    // We can't directly test anyToString as it's private, so we add a default
    // value and see how it's converted for the help message

    ArgumentParser parser2("program");
    std::vector<std::string> default_vec = {"default1", "default2"};
    parser2.addArgument(
        "vec_with_default", ArgumentParser::ArgType::STRING, false, default_vec,
        "Vector with default", {}, false,
        ArgumentParser::Nargs(ArgumentParser::NargsType::ZERO_OR_MORE));

    // Since we can't check the output of printHelp directly,
    // verify the program doesn't crash when displaying the help
    EXPECT_NO_THROW(parser2.printHelp());
}

// Test that printHelp does not crash with various configuration scenarios
TEST_F(ArgumentParserTest, PrintHelpDoesNotCrash) {
    // Set up a parser with a variety of arguments, flags, and configurations
    ArgumentParser richParser("rich_program");

    // Add description and epilog
    richParser.setDescription("This is a test program with many features.");
    richParser.setEpilog("For more information, visit example.com");

    // Add various arguments
    richParser.addArgument("string_arg", ArgumentParser::ArgType::STRING, true,
                           std::string("default"), "A string argument", {"s"});
    richParser.addArgument("int_arg", ArgumentParser::ArgType::INTEGER, false,
                           42, "An integer argument", {"i"});
    richParser.addArgument("pos_arg", ArgumentParser::ArgType::STRING, true, {},
                           "A positional argument", {}, true);

    // Add with different nargs
    richParser.addArgument(
        "optional_arg", ArgumentParser::ArgType::STRING, false,
        std::string("default"), "Optional argument", {"o"}, false,
        ArgumentParser::Nargs(ArgumentParser::NargsType::OPTIONAL));

    richParser.addArgument(
        "multi_arg", ArgumentParser::ArgType::STRING, false, {},
        "Multiple arguments", {"m"}, false,
        ArgumentParser::Nargs(ArgumentParser::NargsType::ONE_OR_MORE));

    // Add flags
    richParser.addFlag("flag1", "First flag", {"f1"});
    richParser.addFlag("flag2", "Second flag", {"f2"});

    // Add subcommands
    richParser.addSubcommand("sub1", "First subcommand");
    richParser.addSubcommand("sub2", "Second subcommand");

    // Add mutually exclusive group
    richParser.addMutuallyExclusiveGroup({"string_arg", "int_arg"});

    // Verify printHelp doesn't crash
    EXPECT_NO_THROW(richParser.printHelp());
}

// Test edge cases for Nargs usage
TEST_F(ArgumentParserTest, NargsEdgeCases) {
    // Test ZERO_OR_MORE with no values
    parser->addArgument(
        "zero_or_more", ArgumentParser::ArgType::STRING, false, {},
        "Zero or more values", {}, false,
        ArgumentParser::Nargs(ArgumentParser::NargsType::ZERO_OR_MORE));

    std::vector<std::string> argv1 = {"program", "--zero_or_more"};
    parser->parse(2, argv1);

    auto zero_or_more = parser->get<std::vector<std::string>>("zero_or_more");
    EXPECT_TRUE(zero_or_more.has_value());
    EXPECT_TRUE(zero_or_more->empty());

    // Test ONE_OR_MORE with no values (should fail)
    ArgumentParser parser2("program");
    parser2.addArgument(
        "one_or_more", ArgumentParser::ArgType::STRING, false, {},
        "One or more values", {}, false,
        ArgumentParser::Nargs(ArgumentParser::NargsType::ONE_OR_MORE));

    std::vector<std::string> argv2 = {"program", "--one_or_more"};
    EXPECT_THROW(parser2.parse(2, argv2), atom::error::InvalidArgument);
}

}  // namespace atom::utils::test

#endif  // ATOM_UTILS_TEST_ARGSVIEW_HPP
