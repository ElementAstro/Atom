/**
 * @file argument_parser_example.cpp
 * @brief Comprehensive example demonstrating ArgumentParser functionality
 *
 * This example shows how to use the ArgumentParser class to handle
 * command-line arguments, with examples of all features including:
 * - Various argument types
 * - Flags
 * - Subcommands
 * - Mutually exclusive groups
 * - File-based arguments
 * - Custom nargs handling
 */

#include "atom/utils/argsview.hpp"
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

// Helper function to print section headers
void printSection(const std::string& title) {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==========================================" << std::endl;
}

// Helper function to print parameter values
template <typename T>
void printValue(const std::string& name, const std::optional<T>& value) {
    std::cout << "  " << name << ": ";
    if (value) {
        if constexpr (std::is_same_v<T, std::string>) {
            std::cout << "\"" << *value << "\"";
        } else if constexpr (std::is_same_v<T, std::filesystem::path>) {
            std::cout << "\"" << value->string() << "\"";
        } else {
            std::cout << *value;
        }
    } else {
        std::cout << "not provided";
    }
    std::cout << std::endl;
}

// Helper function to print flag values
void printFlag(const std::string& name, bool value) {
    std::cout << "  " << name << ": " << (value ? "true" : "false")
              << std::endl;
}

// Helper function to print vector values
template <typename T>
void printVectorValue(const std::string& name,
                      const std::optional<std::vector<T>>& value) {
    std::cout << "  " << name << ": ";
    if (value) {
        std::cout << "[";
        const auto& vec = *value;
        for (size_t i = 0; i < vec.size(); ++i) {
            if constexpr (std::is_same_v<T, std::string>) {
                std::cout << "\"" << vec[i] << "\"";
            } else {
                std::cout << vec[i];
            }
            if (i < vec.size() - 1)
                std::cout << ", ";
        }
        std::cout << "]";
    } else {
        std::cout << "not provided";
    }
    std::cout << std::endl;
}

// Create a file with arguments for testing file-based argument parsing
void createArgumentFile(const std::string& filename,
                        const std::vector<std::string>& args) {
    std::ofstream file(filename);
    if (!file) {
        std::cerr << "Failed to create argument file: " << filename
                  << std::endl;
        return;
    }

    file << "# This is a comment line (will be ignored)\n\n";
    for (const auto& arg : args) {
        file << arg << "\n";
    }
    file.close();
    std::cout << "Created argument file: " << filename << std::endl;
}

int main(int argc, char* argv[]) {
    try {
        // Convert C-style arguments to std::string
        std::vector<std::string> args;
        for (int i = 0; i < argc; ++i) {
            args.push_back(argv[i]);
        }

        printSection("ArgumentParser Demonstration");
        std::cout << "This program demonstrates the full functionality of "
                     "ArgumentParser.\n"
                  << "Different parser examples will be shown.\n";

        // =======================================================
        // Example 1: Basic Parser
        // =======================================================
        printSection("Example 1: Basic Parser");

        // Create a parser with program name
        atom::utils::ArgumentParser basicParser("example1");

        // Add description and epilog
        basicParser.setDescription(
            "A simple example demonstrating basic functionality.");
        basicParser.setEpilog(
            "This example shows how to handle simple arguments and flags.");

        // Add different types of arguments
        basicParser.addArgument("string",
                                atom::utils::ArgumentParser::ArgType::STRING,
                                false, "default string", "A string parameter");
        basicParser.addArgument("required",
                                atom::utils::ArgumentParser::ArgType::STRING,
                                true, {}, "A required parameter");
        basicParser.addArgument("int",
                                atom::utils::ArgumentParser::ArgType::INTEGER,
                                false, 42, "An integer parameter");
        basicParser.addArgument(
            "uint", atom::utils::ArgumentParser::ArgType::UNSIGNED_INTEGER,
            false, 100u, "An unsigned integer parameter");
        basicParser.addArgument("long",
                                atom::utils::ArgumentParser::ArgType::LONG,
                                false, 1000L, "A long integer parameter");
        basicParser.addArgument(
            "ulong", atom::utils::ArgumentParser::ArgType::UNSIGNED_LONG, false,
            2000UL, "An unsigned long parameter");
        basicParser.addArgument("float",
                                atom::utils::ArgumentParser::ArgType::FLOAT,
                                false, 3.14f, "A float parameter");
        basicParser.addArgument("double",
                                atom::utils::ArgumentParser::ArgType::DOUBLE,
                                false, 2.71828, "A double parameter");
        basicParser.addArgument("bool",
                                atom::utils::ArgumentParser::ArgType::BOOLEAN,
                                false, true, "A boolean parameter");
        basicParser.addArgument(
            "path", atom::utils::ArgumentParser::ArgType::FILEPATH, false,
            std::filesystem::path("/tmp"), "A file path parameter");
        basicParser.addArgument(
            "auto", atom::utils::ArgumentParser::ArgType::AUTO, false,
            "auto-detected", "An auto-detected type parameter");

        // Add a flag
        basicParser.addFlag("flag", "A simple flag");
        basicParser.addFlag("verbose", "Verbose output", {"v"});  // with alias

        // Display help
        std::cout << "Help message for Basic Parser:" << std::endl;
        basicParser.printHelp();

        // Create a vector of arguments to parse
        std::vector<std::string> basicArgs = {
            "example1",
            "--string",
            "custom string",
            "--required",
            "required value",
            "--int",
            "123",
            "--uint",
            "456",
            "--long",
            "789",
            "--ulong",
            "1011",
            "--float",
            "6.28",
            "--double",
            "1.618",
            "--bool",
            "true",
            "--path",
            "/path/to/file",
            "--auto",
            "auto string",
            "--flag",
            "-v"  // Using the alias for verbose
        };

        std::cout << "\nParsing the following arguments:" << std::endl;
        for (size_t i = 1; i < basicArgs.size(); ++i) {
            std::cout << "  " << basicArgs[i];
            if (i % 2 == 0 || basicArgs[i].starts_with("-") &&
                                  (i + 1 >= basicArgs.size() ||
                                   basicArgs[i + 1].starts_with("-")))
                std::cout << std::endl;
            else
                std::cout << " ";
        }
        std::cout << std::endl;

        // Parse the arguments
        basicParser.parse(static_cast<int>(basicArgs.size()), basicArgs);

        // Get and display the values
        std::cout << "\nParsed values:" << std::endl;
        printValue<std::string>("string",
                                basicParser.get<std::string>("string"));
        printValue<std::string>("required",
                                basicParser.get<std::string>("required"));
        printValue<int>("int", basicParser.get<int>("int"));
        printValue<unsigned int>("uint", basicParser.get<unsigned int>("uint"));
        printValue<long>("long", basicParser.get<long>("long"));
        printValue<unsigned long>("ulong",
                                  basicParser.get<unsigned long>("ulong"));
        printValue<float>("float", basicParser.get<float>("float"));
        printValue<double>("double", basicParser.get<double>("double"));
        printValue<bool>("bool", basicParser.get<bool>("bool"));
        printValue<std::filesystem::path>(
            "path", basicParser.get<std::filesystem::path>("path"));
        printValue<std::string>("auto", basicParser.get<std::string>("auto"));
        printFlag("flag", basicParser.getFlag("flag"));
        printFlag("verbose", basicParser.getFlag("verbose"));

        // =======================================================
        // Example 2: Positional Arguments and Multiple Values
        // =======================================================
        printSection("Example 2: Positional Arguments and Multiple Values");

        atom::utils::ArgumentParser posParser("example2");
        posParser.setDescription(
            "Demonstrating positional arguments and multiple values.");

        // Add positional arguments with different nargs
        posParser.addArgument("file",
                              atom::utils::ArgumentParser::ArgType::STRING,
                              true, {}, "Input file", {}, true);

        // One or more values with +
        atom::utils::ArgumentParser::Nargs oneOrMore(
            atom::utils::ArgumentParser::NargsType::ONE_OR_MORE);
        posParser.addArgument("sources",
                              atom::utils::ArgumentParser::ArgType::STRING,
                              true, {}, "Source files", {}, true, oneOrMore);

        // Zero or more values with *
        atom::utils::ArgumentParser::Nargs zeroOrMore(
            atom::utils::ArgumentParser::NargsType::ZERO_OR_MORE);
        posParser.addArgument(
            "includes", atom::utils::ArgumentParser::ArgType::STRING, false, {},
            "Include directories", {}, true, zeroOrMore);

        // Optional value with ?
        atom::utils::ArgumentParser::Nargs optional(
            atom::utils::ArgumentParser::NargsType::OPTIONAL);
        posParser.addArgument(
            "output", atom::utils::ArgumentParser::ArgType::STRING, false,
            "a.out", "Output file", {}, true, optional);

        // Constant number of values with exact count
        atom::utils::ArgumentParser::Nargs exactThree(
            atom::utils::ArgumentParser::NargsType::CONSTANT, 3);
        posParser.addArgument(
            "dimensions", atom::utils::ArgumentParser::ArgType::INTEGER, false,
            {}, "Three dimensions (width, height, depth)", {}, false,
            exactThree);

        // Regular arguments
        posParser.addArgument("optimization",
                              atom::utils::ArgumentParser::ArgType::INTEGER,
                              false, 0, "Optimization level");

        // Display help
        std::cout << "Help message for Positional Arguments Parser:"
                  << std::endl;
        posParser.printHelp();

        // Create arguments to parse
        std::vector<std::string> posArgs = {
            "example2",
            "input.txt",  // file (positional)
            "src1.cpp",
            "src2.cpp",  // sources (positional, one or more)
            "include1",
            "include2",    // includes (positional, zero or more)
            "output.exe",  // output (positional, optional)
            "--dimensions",
            "10",
            "20",
            "30",  // dimensions (constant of 3)
            "--optimization",
            "2"  // regular argument
        };

        std::cout << "\nParsing the following arguments:" << std::endl;
        for (size_t i = 1; i < posArgs.size(); ++i) {
            std::cout << "  " << posArgs[i];
            if (i == posArgs.size() - 1 || posArgs[i].starts_with("--") ||
                (i > 0 && posArgs[i - 1].starts_with("--") &&
                 i < posArgs.size() - 1 && !posArgs[i + 1].starts_with("--")))
                std::cout << std::endl;
            else
                std::cout << " ";
        }
        std::cout << std::endl;

        // Parse the arguments
        posParser.parse(static_cast<int>(posArgs.size()), posArgs);

        // Get and display the values
        std::cout << "\nParsed values:" << std::endl;
        printValue<std::string>("file", posParser.get<std::string>("file"));
        printVectorValue<std::string>(
            "sources", posParser.get<std::vector<std::string>>("sources"));
        printVectorValue<std::string>(
            "includes", posParser.get<std::vector<std::string>>("includes"));
        printValue<std::string>("output", posParser.get<std::string>("output"));
        printVectorValue<int>("dimensions",
                              posParser.get<std::vector<int>>("dimensions"));
        printValue<int>("optimization", posParser.get<int>("optimization"));

        // =======================================================
        // Example 3: Mutually Exclusive Groups
        // =======================================================
        printSection("Example 3: Mutually Exclusive Groups");

        atom::utils::ArgumentParser mutexParser("example3");
        mutexParser.setDescription(
            "Demonstrating mutually exclusive argument groups.");

        // Add arguments that cannot be used together
        mutexParser.addArgument("input",
                                atom::utils::ArgumentParser::ArgType::STRING,
                                false, {}, "Input file path");
        mutexParser.addArgument("url",
                                atom::utils::ArgumentParser::ArgType::STRING,
                                false, {}, "URL to fetch data from");

        // Add another set of mutually exclusive arguments
        mutexParser.addFlag("verbose", "Enable verbose output");
        mutexParser.addFlag("quiet", "Suppress all output");

        // Define the mutually exclusive groups
        mutexParser.addMutuallyExclusiveGroup({"input", "url"});
        mutexParser.addMutuallyExclusiveGroup({"verbose", "quiet"});

        // Display help
        std::cout << "Help message for Mutually Exclusive Groups Parser:"
                  << std::endl;
        mutexParser.printHelp();

        // Create arguments to parse
        std::vector<std::string> mutexArgs = {"example3", "--input", "data.csv",
                                              "--verbose"};

        std::cout << "\nParsing the following arguments:" << std::endl;
        for (size_t i = 1; i < mutexArgs.size(); ++i) {
            std::cout << "  " << mutexArgs[i];
            if (i % 2 == 0 || mutexArgs[i].starts_with("-") &&
                                  (i + 1 >= mutexArgs.size() ||
                                   mutexArgs[i + 1].starts_with("-")))
                std::cout << std::endl;
            else
                std::cout << " ";
        }
        std::cout << std::endl;

        // Parse the arguments
        mutexParser.parse(static_cast<int>(mutexArgs.size()), mutexArgs);

        // Get and display the values
        std::cout << "\nParsed values:" << std::endl;
        printValue<std::string>("input", mutexParser.get<std::string>("input"));
        printValue<std::string>("url", mutexParser.get<std::string>("url"));
        printFlag("verbose", mutexParser.getFlag("verbose"));
        printFlag("quiet", mutexParser.getFlag("quiet"));

        // =======================================================
        // Example 4: Subcommands
        // =======================================================
        printSection("Example 4: Subcommands");

        atom::utils::ArgumentParser mainParser("example4");
        mainParser.setDescription("Demonstrating subcommands - like git.");

        // Add main level arguments
        mainParser.addFlag("version", "Show version information");

        // Add subcommands
        mainParser.addSubcommand("add", "Add files to staging");
        mainParser.addSubcommand("commit", "Commit changes");
        mainParser.addSubcommand("push", "Push commits to remote");

        // Configure subcommand parsers
        if (auto addParser = mainParser.getSubcommandParser("add")) {
            addParser->get().addArgument(
                "file", atom::utils::ArgumentParser::ArgType::STRING, true, {},
                "Files to add", {}, true,
                atom::utils::ArgumentParser::Nargs(
                    atom::utils::ArgumentParser::NargsType::ONE_OR_MORE));
            addParser->get().addFlag("all", "Add all files", {"a"});
        }

        if (auto commitParser = mainParser.getSubcommandParser("commit")) {
            commitParser->get().addArgument(
                "message", atom::utils::ArgumentParser::ArgType::STRING, true,
                {}, "Commit message", {"m"});
            commitParser->get().addFlag("amend", "Amend previous commit");
        }

        if (auto pushParser = mainParser.getSubcommandParser("push")) {
            pushParser->get().addArgument(
                "remote", atom::utils::ArgumentParser::ArgType::STRING, false,
                "origin", "Remote name");
            pushParser->get().addArgument(
                "branch", atom::utils::ArgumentParser::ArgType::STRING, false,
                "master", "Branch name");
            pushParser->get().addFlag("force", "Force push", {"f"});
        }

        // Display help for main parser
        std::cout << "Help message for Main Parser:" << std::endl;
        mainParser.printHelp();

        // Display help for a subcommand parser
        std::cout << "\nHelp message for 'commit' Subcommand Parser:"
                  << std::endl;
        if (auto commitParser = mainParser.getSubcommandParser("commit")) {
            commitParser->get().printHelp();
        }

        // Parse arguments for main with a subcommand
        std::vector<std::string> subcommandArgs = {
            "example4", "commit", "--message", "Fixed bug #123", "--amend"};

        std::cout << "\nParsing the following arguments:" << std::endl;
        for (size_t i = 1; i < subcommandArgs.size(); ++i) {
            std::cout << "  " << subcommandArgs[i];
            if (i % 2 == 0 || subcommandArgs[i].starts_with("-") &&
                                  (i + 1 >= subcommandArgs.size() ||
                                   subcommandArgs[i + 1].starts_with("-")))
                std::cout << std::endl;
            else
                std::cout << " ";
        }
        std::cout << std::endl;

        // Parse the arguments
        mainParser.parse(static_cast<int>(subcommandArgs.size()),
                         subcommandArgs);

        // Get and display the values
        std::cout << "\nParsed values:" << std::endl;
        printFlag("version", mainParser.getFlag("version"));

        // Display values for the specific subcommand used
        std::cout << "\nSubcommand values for 'commit':" << std::endl;
        if (auto commitParser = mainParser.getSubcommandParser("commit")) {
            printValue<std::string>(
                "message", commitParser->get().get<std::string>("message"));
            printFlag("amend", commitParser->get().getFlag("amend"));
        }

        // =======================================================
        // Example 5: File-Based Arguments
        // =======================================================
        printSection("Example 5: File-Based Arguments");

        atom::utils::ArgumentParser fileParser("example5");
        fileParser.setDescription("Demonstrating file-based arguments.");

        // Enable file-based argument parsing with @ prefix
        fileParser.addArgumentFromFile("@");

        // Add some arguments
        fileParser.addArgument("config",
                               atom::utils::ArgumentParser::ArgType::STRING,
                               false, "default.cfg", "Configuration file");
        fileParser.addArgument("threads",
                               atom::utils::ArgumentParser::ArgType::INTEGER,
                               false, 1, "Number of threads");
        fileParser.addFlag("debug", "Enable debug mode");

        // Create an argument file for testing
        std::string argFileName = "example5_args.txt";
        std::vector<std::string> fileContents = {"--config", "production.cfg",
                                                 "--threads", "8", "--debug"};
        createArgumentFile(argFileName, fileContents);

        // Display help
        std::cout << "Help message for File-Based Arguments Parser:"
                  << std::endl;
        fileParser.printHelp();

        // Create arguments to parse with file reference
        std::vector<std::string> fileArgs = {
            "example5",
            "@" + argFileName  // Reference to the argument file
        };

        std::cout << "\nParsing the following arguments:" << std::endl;
        for (size_t i = 1; i < fileArgs.size(); ++i) {
            std::cout << "  " << fileArgs[i] << std::endl;
        }
        std::cout
            << "(File contents: --config production.cfg --threads 8 --debug)"
            << std::endl;

        // Parse the arguments
        fileParser.parse(static_cast<int>(fileArgs.size()), fileArgs);

        // Get and display the values
        std::cout << "\nParsed values:" << std::endl;
        printValue<std::string>("config",
                                fileParser.get<std::string>("config"));
        printValue<int>("threads", fileParser.get<int>("threads"));
        printFlag("debug", fileParser.getFlag("debug"));

        // =======================================================
        // Example 6: Advanced Type Parsing
        // =======================================================
        printSection("Example 6: Advanced Type Parsing");

        atom::utils::ArgumentParser advancedParser("example6");
        advancedParser.setDescription(
            "Demonstrating advanced type handling and parsing.");

        // Add arguments with special parsing requirements
        advancedParser.addArgument(
            "integer", atom::utils::ArgumentParser::ArgType::INTEGER, true, {},
            "Integer value");
        advancedParser.addArgument(
            "boolean", atom::utils::ArgumentParser::ArgType::BOOLEAN, true, {},
            "Boolean value");
        advancedParser.addArgument(
            "filepath", atom::utils::ArgumentParser::ArgType::FILEPATH, true,
            {}, "File path");

        // Create arguments to parse with various formats
        std::vector<std::string> advancedArgs = {
            "example6",   "--integer",         "42", "--boolean",
            "yes",  // Alternative boolean format
            "--filepath", "./data/config.json"};

        std::cout << "\nParsing the following arguments:" << std::endl;
        for (size_t i = 1; i < advancedArgs.size(); ++i) {
            std::cout << "  " << advancedArgs[i];
            if (i % 2 == 0)
                std::cout << std::endl;
            else
                std::cout << " ";
        }
        std::cout << std::endl;

        // Parse the arguments
        advancedParser.parse(static_cast<int>(advancedArgs.size()),
                             advancedArgs);

        // Get and display the values
        std::cout << "\nParsed values:" << std::endl;
        printValue<int>("integer", advancedParser.get<int>("integer"));
        printValue<bool>("boolean", advancedParser.get<bool>("boolean"));
        printValue<std::filesystem::path>(
            "filepath", advancedParser.get<std::filesystem::path>("filepath"));

        std::cout << "\nAll examples completed successfully!" << std::endl;

        // Clean up
        std::filesystem::remove(argFileName);

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}