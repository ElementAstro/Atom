#include "atom/utils/argsview.hpp"

#include <iostream>
#include <optional>
#include <vector>

using namespace atom::utils;

int main(int argc, char* argv[]) {
    // Create an ArgumentParser instance with program name
    ArgumentParser parser("example_program");

    // Set the description of the program
    parser.setDescription(
        "This is an example program to demonstrate ArgumentParser.");

    // Set the epilog of the program
    parser.setEpilog("This is the epilog of the example program.");

    // Add an argument to the parser
    parser.addArgument("input", ArgumentParser::ArgType::STRING, true, {},
                       "Input file path");

    // Add a flag to the parser
    parser.addFlag("verbose", "Enable verbose output");

    // Add a subcommand to the parser
    parser.addSubcommand("convert", "Convert the input file to another format");

    // Add a mutually exclusive group of arguments
    parser.addMutuallyExclusiveGroup({"option1", "option2"});

    // Enable parsing arguments from a file
    parser.addArgumentFromFile("@");

    // Set the delimiter for file parsing
    parser.setFileDelimiter(',');

    // Parse the command-line arguments
    std::vector<std::string> args(argv, argv + argc);
    parser.parse(argc, args);

    // Get the value of an argument
    std::optional<std::string> inputFile = parser.get<std::string>("input");
    if (inputFile) {
        std::cout << "Input file: " << *inputFile << std::endl;
    }

    // Get the value of a flag
    bool verbose = parser.getFlag("verbose");
    std::cout << "Verbose flag: " << std::boolalpha << verbose << std::endl;

    // Get the parser for a subcommand
    auto subcommandParser = parser.getSubcommandParser("convert");
    if (subcommandParser) {
        std::cout << "Subcommand 'convert' is used." << std::endl;
    }

    // Print the help message
    parser.printHelp();

    return 0;
}