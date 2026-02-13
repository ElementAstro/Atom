/**
 * @file argsview_example.cpp
 * @brief Comprehensive examples for atom::utils ArgumentParser
 *
 * This example demonstrates the ArgumentParser class including:
 * - Basic argument parsing
 * - Different argument types (string, int, float, bool, filepath)
 * - Required and optional arguments
 * - Default values
 * - Flags
 * - Subcommands
 * - Mutually exclusive groups
 * - Help generation
 */

#include "atom/utils/core/argsview.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace atom::utils;

void printSection(const std::string& title) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "========================================" << std::endl;
}

// ============================================
// 1. Basic Argument Parsing
// ============================================
void demonstrateBasicParsing() {
    printSection("1. Basic Argument Parsing");

    ArgumentParser parser("basic_example");
    parser.setDescription("A basic example of argument parsing");

    // Add a simple string argument
    parser.addArgument("--name", ArgumentParser::ArgType::STRING, false,
                       std::any(std::string("World")), "Your name");

    // Add an integer argument
    parser.addArgument("--count", ArgumentParser::ArgType::INTEGER, false,
                       std::any(1), "Number of times to greet");

    // Simulate command line arguments
    Vector<String> args = {"basic_example", "--name", "Alice", "--count", "3"};

    std::cout << "Simulated command: basic_example --name Alice --count 3"
              << std::endl;

    try {
        parser.parse(static_cast<int>(args.size()), args);

        auto name = parser.get<std::string>("--name");
        auto count = parser.get<int>("--count");

        if (name && count) {
            std::cout << "\nParsed values:" << std::endl;
            std::cout << "  name: " << *name << std::endl;
            std::cout << "  count: " << *count << std::endl;

            std::cout << "\nOutput:" << std::endl;
            for (int i = 0; i < *count; ++i) {
                std::cout << "  Hello, " << *name << "!" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cout << "Parse error: " << e.what() << std::endl;
    }
}

// ============================================
// 2. Different Argument Types
// ============================================
void demonstrateArgumentTypes() {
    printSection("2. Different Argument Types");

    ArgumentParser parser("types_example");
    parser.setDescription("Demonstrating different argument types");

    // String argument
    parser.addArgument("--string", ArgumentParser::ArgType::STRING, false,
                       std::any(std::string("default")), "A string value");

    // Integer argument
    parser.addArgument("--integer", ArgumentParser::ArgType::INTEGER, false,
                       std::any(0), "An integer value");

    // Unsigned integer
    parser.addArgument("--unsigned", ArgumentParser::ArgType::UNSIGNED_INTEGER,
                       false, std::any(0u), "An unsigned integer");

    // Long argument
    parser.addArgument("--long", ArgumentParser::ArgType::LONG, false,
                       std::any(0L), "A long value");

    // Float argument
    parser.addArgument("--float", ArgumentParser::ArgType::FLOAT, false,
                       std::any(0.0f), "A float value");

    // Double argument
    parser.addArgument("--double", ArgumentParser::ArgType::DOUBLE, false,
                       std::any(0.0), "A double value");

    // Boolean argument
    parser.addArgument("--bool", ArgumentParser::ArgType::BOOLEAN, false,
                       std::any(false), "A boolean value");

    // Filepath argument
    parser.addArgument("--file", ArgumentParser::ArgType::FILEPATH, false,
                       std::any(std::string("")), "A file path");

    // Auto-detect type
    parser.addArgument("--auto", ArgumentParser::ArgType::AUTO, false,
                       std::any(std::string("")), "Auto-detected type");

    Vector<String> args = {"types_example",
                           "--string",
                           "hello",
                           "--integer",
                           "42",
                           "--unsigned",
                           "100",
                           "--long",
                           "1000000",
                           "--float",
                           "3.14",
                           "--double",
                           "2.71828",
                           "--bool",
                           "true",
                           "--file",
                           "/path/to/file.txt",
                           "--auto",
                           "auto_value"};

    std::cout << "Simulated command with various types..." << std::endl;

    try {
        parser.parse(static_cast<int>(args.size()), args);

        std::cout << "\nParsed values:" << std::endl;

        if (auto val = parser.get<std::string>("--string"))
            std::cout << "  string: " << *val << std::endl;

        if (auto val = parser.get<int>("--integer"))
            std::cout << "  integer: " << *val << std::endl;

        if (auto val = parser.get<unsigned int>("--unsigned"))
            std::cout << "  unsigned: " << *val << std::endl;

        if (auto val = parser.get<long>("--long"))
            std::cout << "  long: " << *val << std::endl;

        if (auto val = parser.get<float>("--float"))
            std::cout << "  float: " << *val << std::endl;

        if (auto val = parser.get<double>("--double"))
            std::cout << "  double: " << *val << std::endl;

        if (auto val = parser.get<bool>("--bool"))
            std::cout << "  bool: " << std::boolalpha << *val << std::endl;

        if (auto val = parser.get<std::string>("--file"))
            std::cout << "  file: " << *val << std::endl;

        if (auto val = parser.get<std::string>("--auto"))
            std::cout << "  auto: " << *val << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Parse error: " << e.what() << std::endl;
    }
}

// ============================================
// 3. Required Arguments and Defaults
// ============================================
void demonstrateRequiredAndDefaults() {
    printSection("3. Required Arguments and Defaults");

    ArgumentParser parser("required_example");
    parser.setDescription("Demonstrating required arguments and defaults");

    // Required argument (no default)
    parser.addArgument("--input", ArgumentParser::ArgType::STRING, true,
                       std::any(), "Input file (required)");

    // Optional with default
    parser.addArgument("--output", ArgumentParser::ArgType::STRING, false,
                       std::any(std::string("output.txt")),
                       "Output file (default: output.txt)");

    // Optional with default number
    parser.addArgument("--threads", ArgumentParser::ArgType::INTEGER, false,
                       std::any(4), "Number of threads (default: 4)");

    // Test with required argument provided
    std::cout << "--- With required argument ---" << std::endl;
    Vector<String> args1 = {"required_example", "--input", "data.csv"};

    try {
        parser.parse(static_cast<int>(args1.size()), args1);

        if (auto input = parser.get<std::string>("--input"))
            std::cout << "  input: " << *input << std::endl;

        if (auto output = parser.get<std::string>("--output"))
            std::cout << "  output: " << *output << " (default)" << std::endl;

        if (auto threads = parser.get<int>("--threads"))
            std::cout << "  threads: " << *threads << " (default)" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Parse error: " << e.what() << std::endl;
    }

    // Test with all arguments
    std::cout << "\n--- With all arguments ---" << std::endl;
    ArgumentParser parser2("required_example");
    parser2.addArgument("--input", ArgumentParser::ArgType::STRING, true,
                        std::any(), "Input file");
    parser2.addArgument("--output", ArgumentParser::ArgType::STRING, false,
                        std::any(std::string("output.txt")), "Output file");
    parser2.addArgument("--threads", ArgumentParser::ArgType::INTEGER, false,
                        std::any(4), "Number of threads");

    Vector<String> args2 = {
        "required_example", "--input",   "data.csv", "--output",
        "result.txt",       "--threads", "8"};

    try {
        parser2.parse(static_cast<int>(args2.size()), args2);

        if (auto input = parser2.get<std::string>("--input"))
            std::cout << "  input: " << *input << std::endl;

        if (auto output = parser2.get<std::string>("--output"))
            std::cout << "  output: " << *output << std::endl;

        if (auto threads = parser2.get<int>("--threads"))
            std::cout << "  threads: " << *threads << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Parse error: " << e.what() << std::endl;
    }
}

// ============================================
// 4. Flags
// ============================================
void demonstrateFlags() {
    printSection("4. Flags");

    ArgumentParser parser("flags_example");
    parser.setDescription("Demonstrating boolean flags");

    // Add flags
    parser.addFlag("--verbose", "Enable verbose output", {"-v"});
    parser.addFlag("--debug", "Enable debug mode", {"-d"});
    parser.addFlag("--quiet", "Suppress output", {"-q"});
    parser.addFlag("--force", "Force operation", {"-f"});

    // Test with some flags
    Vector<String> args = {"flags_example", "--verbose", "-d", "--force"};

    std::cout << "Simulated command: flags_example --verbose -d --force"
              << std::endl;

    try {
        parser.parse(static_cast<int>(args.size()), args);

        std::cout << "\nFlag states:" << std::endl;
        std::cout << "  verbose: " << std::boolalpha
                  << parser.getFlag("--verbose") << std::endl;
        std::cout << "  debug: " << parser.getFlag("--debug") << std::endl;
        std::cout << "  quiet: " << parser.getFlag("--quiet") << std::endl;
        std::cout << "  force: " << parser.getFlag("--force") << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Parse error: " << e.what() << std::endl;
    }
}

// ============================================
// 5. Aliases
// ============================================
void demonstrateAliases() {
    printSection("5. Aliases");

    ArgumentParser parser("alias_example");
    parser.setDescription("Demonstrating argument aliases");

    // Arguments with aliases
    parser.addArgument("--output", ArgumentParser::ArgType::STRING, false,
                       std::any(std::string("out.txt")), "Output file", {"-o"});

    parser.addArgument("--verbose-level", ArgumentParser::ArgType::INTEGER,
                       false, std::any(0), "Verbosity level", {"-V", "--verb"});

    parser.addArgument("--config", ArgumentParser::ArgType::FILEPATH, false,
                       std::any(std::string("")), "Config file path",
                       {"-c", "--cfg"});

    // Test with aliases
    Vector<String> args = {"alias_example", "-o",      "result.txt", "-V", "2",
                           "--cfg",         "app.conf"};

    std::cout
        << "Simulated command: alias_example -o result.txt -V 2 --cfg app.conf"
        << std::endl;

    try {
        parser.parse(static_cast<int>(args.size()), args);

        std::cout << "\nParsed values (using aliases):" << std::endl;

        if (auto val = parser.get<std::string>("--output"))
            std::cout << "  output (-o): " << *val << std::endl;

        if (auto val = parser.get<int>("--verbose-level"))
            std::cout << "  verbose-level (-V): " << *val << std::endl;

        if (auto val = parser.get<std::string>("--config"))
            std::cout << "  config (--cfg): " << *val << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Parse error: " << e.what() << std::endl;
    }
}

// ============================================
// 6. Subcommands
// ============================================
void demonstrateSubcommands() {
    printSection("6. Subcommands");

    ArgumentParser parser("git_like");
    parser.setDescription("A git-like command with subcommands");

    // Add subcommands
    parser.addSubcommand("clone", "Clone a repository");
    parser.addSubcommand("commit", "Record changes to the repository");
    parser.addSubcommand("push", "Update remote refs");

    // Configure clone subcommand
    if (auto cloneParser = parser.getSubcommandParser("clone")) {
        cloneParser->get().addArgument("--url", ArgumentParser::ArgType::STRING,
                                       true, std::any(), "Repository URL");
        cloneParser->get().addArgument("--depth",
                                       ArgumentParser::ArgType::INTEGER, false,
                                       std::any(0), "Create a shallow clone");
    }

    // Configure commit subcommand
    if (auto commitParser = parser.getSubcommandParser("commit")) {
        commitParser->get().addArgument("--message",
                                        ArgumentParser::ArgType::STRING, true,
                                        std::any(), "Commit message", {"-m"});
        commitParser->get().addFlag("--amend", "Amend previous commit");
    }

    // Configure push subcommand
    if (auto pushParser = parser.getSubcommandParser("push")) {
        pushParser->get().addArgument(
            "--remote", ArgumentParser::ArgType::STRING, false,
            std::any(std::string("origin")), "Remote name");
        pushParser->get().addFlag("--force", "Force push", {"-f"});
    }

    std::cout << "Available subcommands: clone, commit, push" << std::endl;
    std::cout << "\nExample usage:" << std::endl;
    std::cout << "  git_like clone --url https://github.com/user/repo.git"
              << std::endl;
    std::cout << "  git_like commit -m \"Initial commit\"" << std::endl;
    std::cout << "  git_like push --remote origin -f" << std::endl;
}

// ============================================
// 7. Mutually Exclusive Groups
// ============================================
void demonstrateMutuallyExclusive() {
    printSection("7. Mutually Exclusive Groups");

    ArgumentParser parser("exclusive_example");
    parser.setDescription("Demonstrating mutually exclusive arguments");

    // Add arguments
    parser.addArgument("--json", ArgumentParser::ArgType::BOOLEAN, false,
                       std::any(false), "Output in JSON format");
    parser.addArgument("--xml", ArgumentParser::ArgType::BOOLEAN, false,
                       std::any(false), "Output in XML format");
    parser.addArgument("--csv", ArgumentParser::ArgType::BOOLEAN, false,
                       std::any(false), "Output in CSV format");

    // Make them mutually exclusive
    parser.addMutuallyExclusiveGroup({"--json", "--xml", "--csv"});

    std::cout << "Arguments --json, --xml, and --csv are mutually exclusive"
              << std::endl;
    std::cout << "Only one output format can be selected at a time"
              << std::endl;

    // Test with valid input (only one format)
    std::cout << "\n--- Valid: Single format ---" << std::endl;
    Vector<String> args1 = {"exclusive_example", "--json", "true"};

    try {
        parser.parse(static_cast<int>(args1.size()), args1);
        std::cout << "  Parsed successfully with --json" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "  Error: " << e.what() << std::endl;
    }
}

// ============================================
// 8. Help Generation
// ============================================
void demonstrateHelpGeneration() {
    printSection("8. Help Generation");

    ArgumentParser parser("myapp");
    parser.setDescription(
        "A comprehensive application demonstrating argument parsing");
    parser.setEpilog("For more information, visit https://example.com");

    // Add various arguments
    parser.addArgument("--input", ArgumentParser::ArgType::FILEPATH, true,
                       std::any(), "Input file path", {"-i"});
    parser.addArgument("--output", ArgumentParser::ArgType::FILEPATH, false,
                       std::any(std::string("output.txt")), "Output file path",
                       {"-o"});
    parser.addArgument("--format", ArgumentParser::ArgType::STRING, false,
                       std::any(std::string("text")),
                       "Output format (text, json, xml)", {"-f"});
    parser.addArgument("--threads", ArgumentParser::ArgType::INTEGER, false,
                       std::any(4), "Number of worker threads", {"-t"});
    parser.addArgument("--timeout", ArgumentParser::ArgType::DOUBLE, false,
                       std::any(30.0), "Timeout in seconds");

    parser.addFlag("--verbose", "Enable verbose output", {"-v"});
    parser.addFlag("--debug", "Enable debug mode", {"-d"});
    parser.addFlag("--quiet", "Suppress all output", {"-q"});

    std::cout << "Generated help output:" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    parser.printHelp();
    std::cout << "----------------------------------------" << std::endl;
}

// ============================================
// 9. Complex Use Case
// ============================================
void demonstrateComplexUseCase() {
    printSection("9. Complex Use Case - Build Tool");

    ArgumentParser parser("buildtool");
    parser.setDescription("A build tool with comprehensive options");

    // Source and output
    parser.addArgument("--source", ArgumentParser::ArgType::FILEPATH, true,
                       std::any(), "Source directory", {"-s", "--src"});
    parser.addArgument("--output", ArgumentParser::ArgType::FILEPATH, false,
                       std::any(std::string("./build")), "Output directory",
                       {"-o", "--out"});

    // Build configuration
    parser.addArgument("--config", ArgumentParser::ArgType::STRING, false,
                       std::any(std::string("release")),
                       "Build configuration (debug, release)", {"-c"});
    parser.addArgument("--jobs", ArgumentParser::ArgType::INTEGER, false,
                       std::any(4), "Parallel jobs", {"-j"});

    // Flags
    parser.addFlag("--clean", "Clean before build");
    parser.addFlag("--verbose", "Verbose output", {"-v"});
    parser.addFlag("--dry-run", "Show what would be done");

    // Simulate a build command
    Vector<String> args = {"buildtool", "-s",      "./src",    "-o",
                           "./dist",    "-c",      "debug",    "-j",
                           "8",         "--clean", "--verbose"};

    std::cout << "Simulated command:" << std::endl;
    std::cout
        << "  buildtool -s ./src -o ./dist -c debug -j 8 --clean --verbose"
        << std::endl;

    try {
        parser.parse(static_cast<int>(args.size()), args);

        std::cout << "\nBuild configuration:" << std::endl;

        if (auto val = parser.get<std::string>("--source"))
            std::cout << "  Source: " << *val << std::endl;

        if (auto val = parser.get<std::string>("--output"))
            std::cout << "  Output: " << *val << std::endl;

        if (auto val = parser.get<std::string>("--config"))
            std::cout << "  Config: " << *val << std::endl;

        if (auto val = parser.get<int>("--jobs"))
            std::cout << "  Jobs: " << *val << std::endl;

        std::cout << "  Clean: " << std::boolalpha << parser.getFlag("--clean")
                  << std::endl;
        std::cout << "  Verbose: " << parser.getFlag("--verbose") << std::endl;
        std::cout << "  Dry-run: " << parser.getFlag("--dry-run") << std::endl;

    } catch (const std::exception& e) {
        std::cout << "Parse error: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  ArgumentParser Examples" << std::endl;
    std::cout << "  atom::utils::ArgumentParser" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        demonstrateBasicParsing();
        demonstrateArgumentTypes();
        demonstrateRequiredAndDefaults();
        demonstrateFlags();
        demonstrateAliases();
        demonstrateSubcommands();
        demonstrateMutuallyExclusive();
        demonstrateHelpGeneration();
        demonstrateComplexUseCase();

        std::cout << "\n========================================" << std::endl;
        std::cout << "  All ArgumentParser examples completed!" << std::endl;
        std::cout << "========================================" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
