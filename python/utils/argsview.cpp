#include "atom/utils/argsview.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

namespace py = pybind11;

PYBIND11_MODULE(argsview, m) {
    m.doc() = "Command-line argument parsing utilities module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // ArgType enumeration
    py::enum_<atom::utils::ArgumentParser::ArgType>(m, "ArgType",
        R"(Enumeration of possible argument types for command-line parsing.
        
        This enum defines the supported data types that can be parsed
        from command-line arguments.
        )")
        .value("STRING", atom::utils::ArgumentParser::ArgType::STRING, "String type")
        .value("INTEGER", atom::utils::ArgumentParser::ArgType::INTEGER, "Integer type")
        .value("UNSIGNED_INTEGER", atom::utils::ArgumentParser::ArgType::UNSIGNED_INTEGER, "Unsigned integer type")
        .value("LONG", atom::utils::ArgumentParser::ArgType::LONG, "Long integer type")
        .value("UNSIGNED_LONG", atom::utils::ArgumentParser::ArgType::UNSIGNED_LONG, "Unsigned long integer type")
        .value("FLOAT", atom::utils::ArgumentParser::ArgType::FLOAT, "Float type")
        .value("DOUBLE", atom::utils::ArgumentParser::ArgType::DOUBLE, "Double precision float type")
        .value("BOOLEAN", atom::utils::ArgumentParser::ArgType::BOOLEAN, "Boolean type")
        .value("FILEPATH", atom::utils::ArgumentParser::ArgType::FILEPATH, "File path type")
        .value("AUTO", atom::utils::ArgumentParser::ArgType::AUTO, "Automatic type detection")
        .export_values();

    // NargsType enumeration
    py::enum_<atom::utils::ArgumentParser::NargsType>(m, "NargsType",
        R"(Enumeration of possible nargs types for argument parsing.
        
        This enum defines how many values an argument can accept.
        )")
        .value("NONE", atom::utils::ArgumentParser::NargsType::NONE, "No special nargs behavior")
        .value("OPTIONAL", atom::utils::ArgumentParser::NargsType::OPTIONAL, "Optional argument (?)")
        .value("ZERO_OR_MORE", atom::utils::ArgumentParser::NargsType::ZERO_OR_MORE, "Zero or more arguments (*)")
        .value("ONE_OR_MORE", atom::utils::ArgumentParser::NargsType::ONE_OR_MORE, "One or more arguments (+)")
        .value("CONSTANT", atom::utils::ArgumentParser::NargsType::CONSTANT, "Constant number of arguments")
        .export_values();

    // Nargs structure
    py::class_<atom::utils::ArgumentParser::Nargs>(m, "Nargs",
        R"(Structure to define the number of arguments for a parameter.
        
        This structure specifies how many values an argument should accept
        and the behavior when parsing multiple values.
        )")
        .def(py::init<>(), "Create default Nargs (NONE, count=1)")
        .def(py::init<atom::utils::ArgumentParser::NargsType, int>(),
             py::arg("type"), py::arg("count") = 1,
             R"(Create Nargs with specified type and count.
             
             Args:
                 type: The NargsType specifying the behavior.
                 count: The number of arguments (for CONSTANT type).
                 
             Raises:
                 ValueError: If count is negative.
             )")
        .def_readwrite("type", &atom::utils::ArgumentParser::Nargs::type, "The nargs type")
        .def_readwrite("count", &atom::utils::ArgumentParser::Nargs::count, "The argument count");

    // ArgumentParser class
    py::class_<atom::utils::ArgumentParser>(m, "ArgumentParser",
        R"(A comprehensive command-line argument parser with enhanced C++20 features.
        
        This class provides a powerful and flexible way to parse command-line arguments
        with support for various data types, subcommands, mutually exclusive groups,
        and file-based argument expansion.
        
        Examples:
            >>> from atom.utils import argsview
            >>> parser = argsview.ArgumentParser("myprogram")
            >>> parser.add_argument("input", argsview.ArgType.STRING, True, help="Input file")
            >>> parser.add_flag("verbose", "Enable verbose output", ["v"])
            >>> # Parse arguments from sys.argv
        )")
        .def(py::init<>(), "Create an ArgumentParser with default settings")
        .def(py::init<const std::string&>(), py::arg("program_name"),
             R"(Create an ArgumentParser with a program name.
             
             Args:
                 program_name: The name of the program for help display.
             )")
        .def("set_description", &atom::utils::ArgumentParser::setDescription,
             py::arg("description"),
             R"(Set the description for the program.
             
             Args:
                 description: A description of what the program does.
             )")
        .def("set_epilog", &atom::utils::ArgumentParser::setEpilog,
             py::arg("epilog"),
             R"(Set the epilog text displayed after the help.
             
             Args:
                 epilog: Text to display at the end of help output.
             )")
        .def("add_argument", &atom::utils::ArgumentParser::addArgument,
             py::arg("name"), 
             py::arg("type") = atom::utils::ArgumentParser::ArgType::AUTO,
             py::arg("required") = false,
             py::arg("default_value") = std::any{},
             py::arg("help") = "",
             py::arg("aliases") = std::vector<std::string>{},
             py::arg("is_positional") = false,
             py::arg("nargs") = atom::utils::ArgumentParser::Nargs{},
             R"(Add an argument to the parser.
             
             Args:
                 name: The name of the argument.
                 type: The expected type of the argument value.
                 required: Whether the argument is required.
                 default_value: Default value if not provided.
                 help: Help text for the argument.
                 aliases: List of alternative names for the argument.
                 is_positional: Whether this is a positional argument.
                 nargs: Specification for number of argument values.
                 
             Raises:
                 ValueError: If the argument name is invalid.
                 RuntimeError: If the argument is already registered.
             )")
        .def("add_flag", &atom::utils::ArgumentParser::addFlag,
             py::arg("name"), py::arg("help") = "", py::arg("aliases") = std::vector<std::string>{},
             R"(Add a boolean flag to the parser.
             
             Args:
                 name: The name of the flag.
                 help: Help text for the flag.
                 aliases: List of alternative names for the flag.
                 
             Raises:
                 ValueError: If the flag name is invalid.
                 RuntimeError: If the flag is already registered.
             )")
        .def("add_subcommand", &atom::utils::ArgumentParser::addSubcommand,
             py::arg("name"), py::arg("help") = "",
             R"(Add a subcommand to the parser.
             
             Args:
                 name: The name of the subcommand.
                 help: Help text for the subcommand.
                 
             Raises:
                 ValueError: If the subcommand name is invalid.
                 RuntimeError: If the subcommand is already registered.
             )")
        .def("add_mutually_exclusive_group", &atom::utils::ArgumentParser::addMutuallyExclusiveGroup,
             py::arg("group_args"),
             R"(Add a mutually exclusive group of arguments.
             
             Args:
                 group_args: List of argument names that are mutually exclusive.
                 
             Raises:
                 ValueError: If the group has fewer than 2 arguments or if any argument doesn't exist.
             )")
        .def("add_argument_from_file", &atom::utils::ArgumentParser::addArgumentFromFile,
             py::arg("prefix") = "@",
             R"(Enable parsing arguments from files.
             
             Args:
                 prefix: The prefix that indicates a filename (default: "@").
             )")
        .def("set_file_delimiter", &atom::utils::ArgumentParser::setFileDelimiter,
             py::arg("delimiter"),
             R"(Set the delimiter for parsing arguments from files.
             
             Args:
                 delimiter: The character used to separate arguments in files.
             )")
        .def("parse", 
             [](atom::utils::ArgumentParser& self, const std::vector<std::string>& args) {
                 std::vector<atom::utils::String> atom_args;
                 atom_args.reserve(args.size());
                 for (const auto& arg : args) {
                     atom_args.emplace_back(arg.c_str());
                 }
                 self.parse(static_cast<int>(atom_args.size()), std::span<const atom::utils::String>(atom_args));
             },
             py::arg("args"),
             R"(Parse the given command-line arguments.
             
             Args:
                 args: List of command-line arguments to parse.
                 
             Raises:
                 ValueError: If parsing fails due to invalid arguments.
                 RuntimeError: If required arguments are missing.
             )")
        .def("get_string", 
             [](const atom::utils::ArgumentParser& self, const std::string& name) -> py::object {
                 auto result = self.get<atom::utils::String>(name);
                 if (result) {
                     return py::cast(std::string(*result));
                 }
                 return py::none();
             },
             py::arg("name"),
             R"(Get a string argument value.
             
             Args:
                 name: The name of the argument.
                 
             Returns:
                 The string value or None if not found.
             )")
        .def("get_int", 
             [](const atom::utils::ArgumentParser& self, const std::string& name) -> py::object {
                 auto result = self.get<int>(name);
                 if (result) {
                     return py::cast(*result);
                 }
                 return py::none();
             },
             py::arg("name"),
             R"(Get an integer argument value.
             
             Args:
                 name: The name of the argument.
                 
             Returns:
                 The integer value or None if not found.
             )")
        .def("get_float", 
             [](const atom::utils::ArgumentParser& self, const std::string& name) -> py::object {
                 auto result = self.get<double>(name);
                 if (result) {
                     return py::cast(*result);
                 }
                 return py::none();
             },
             py::arg("name"),
             R"(Get a float argument value.
             
             Args:
                 name: The name of the argument.
                 
             Returns:
                 The float value or None if not found.
             )")
        .def("get_bool", 
             [](const atom::utils::ArgumentParser& self, const std::string& name) -> py::object {
                 auto result = self.get<bool>(name);
                 if (result) {
                     return py::cast(*result);
                 }
                 return py::none();
             },
             py::arg("name"),
             R"(Get a boolean argument value.
             
             Args:
                 name: The name of the argument.
                 
             Returns:
                 The boolean value or None if not found.
             )")
        .def("get_flag", &atom::utils::ArgumentParser::getFlag,
             py::arg("name"),
             R"(Get the value of a flag.
             
             Args:
                 name: The name of the flag.
                 
             Returns:
                 True if the flag was set, False otherwise.
             )")
        .def("print_help", &atom::utils::ArgumentParser::printHelp,
             R"(Print the help message to stdout.
             
             This displays usage information, argument descriptions,
             and other helpful information about the program.
             )");

    // Utility functions
    m.def("create_parser", 
          [](const std::string& program_name) {
              return atom::utils::ArgumentParser(program_name);
          },
          py::arg("program_name"),
          R"(Create a new ArgumentParser instance.
          
          Args:
              program_name: The name of the program.
              
          Returns:
              A new ArgumentParser instance.
              
          Examples:
              >>> parser = argsview.create_parser("myapp")
          )");
}
