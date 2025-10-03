#include "atom/utils/valid_string.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(valid_string, m) {
    m.doc() = "String validation utilities module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::utils::ValidationException& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const atom::utils::BracketMismatchException& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const atom::utils::QuoteMismatchException& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // BracketType enumeration
    py::enum_<atom::utils::BracketType>(m, "BracketType",
        R"(Enumeration of bracket types for validation.
        
        This enum defines the different types of brackets that can be validated.
        )")
        .value("ROUND", atom::utils::BracketType::Round, "Round brackets ()")
        .value("SQUARE", atom::utils::BracketType::Square, "Square brackets []")
        .value("CURLY", atom::utils::BracketType::Curly, "Curly brackets {}")
        .value("ANGLE", atom::utils::BracketType::Angle, "Angle brackets <>")
        .value("CUSTOM", atom::utils::BracketType::Custom, "Custom bracket types")
        .export_values();

    // BracketInfo structure
    py::class_<atom::utils::BracketInfo>(m, "BracketInfo",
        R"(Information about a bracket in a string.
        
        This structure contains details about a bracket character,
        its position, and type.
        )")
        .def(py::init<>(), "Create default BracketInfo")
        .def(py::init<char, int, atom::utils::BracketType>(),
             py::arg("character"), py::arg("position"), py::arg("type") = atom::utils::BracketType::Custom,
             R"(Create BracketInfo with specified values.
             
             Args:
                 character: The bracket character.
                 position: The position in the string.
                 type: The type of bracket.
             )")
        .def_readwrite("character", &atom::utils::BracketInfo::character, "The bracket character")
        .def_readwrite("position", &atom::utils::BracketInfo::position, "The position in the string")
        .def_readwrite("type", &atom::utils::BracketInfo::type, "The type of bracket")
        .def("get_bracket_type_name", &atom::utils::BracketInfo::getBracketTypeName,
             R"(Get a readable name for the bracket type.
             
             Returns:
                 A string describing the bracket type.
             )")
        .def("__eq__", &atom::utils::BracketInfo::operator==, "Equality comparison")
        .def("__repr__", [](const atom::utils::BracketInfo& info) {
            return std::format("BracketInfo('{}', {}, {})", 
                             info.character, info.position, 
                             static_cast<int>(info.type));
        });

    // ValidationOptions structure
    py::class_<atom::utils::ValidationOptions>(m, "ValidationOptions",
        R"(Options for string validation.
        
        This structure contains configuration options for how
        string validation should be performed.
        )")
        .def(py::init<>(), "Create default ValidationOptions")
        .def_readwrite("ignore_quotes", &atom::utils::ValidationOptions::ignoreQuotes,
                      "Whether to ignore quotes during validation")
        .def_readwrite("ignore_escaped", &atom::utils::ValidationOptions::ignoreEscaped,
                      "Whether to ignore escaped characters")
        .def_readwrite("strict_mode", &atom::utils::ValidationOptions::strictMode,
                      "Whether to use strict validation mode")
        .def_readwrite("allow_nested", &atom::utils::ValidationOptions::allowNested,
                      "Whether to allow nested brackets")
        .def_readwrite("max_depth", &atom::utils::ValidationOptions::maxDepth,
                      "Maximum nesting depth allowed")
        .def("__repr__", [](const atom::utils::ValidationOptions& opts) {
            return std::format("ValidationOptions(ignore_quotes={}, ignore_escaped={}, strict_mode={}, allow_nested={}, max_depth={})",
                             opts.ignoreQuotes, opts.ignoreEscaped, opts.strictMode, opts.allowNested, opts.maxDepth);
        });

    // ValidationResult structure
    py::class_<atom::utils::ValidationResult>(m, "ValidationResult",
        R"(Result of string validation.
        
        This structure contains the results of a validation operation,
        including whether the string is valid and details about any errors.
        )")
        .def(py::init<>(), "Create default ValidationResult")
        .def_readwrite("is_valid", &atom::utils::ValidationResult::isValid,
                      "Whether the string is valid")
        .def_readwrite("error_message", &atom::utils::ValidationResult::errorMessage,
                      "Error message if validation failed")
        .def_readwrite("error_position", &atom::utils::ValidationResult::errorPosition,
                      "Position of the first error")
        .def_readwrite("invalid_brackets", &atom::utils::ValidationResult::invalidBrackets,
                      "List of invalid bracket information")
        .def_readwrite("nesting_depth", &atom::utils::ValidationResult::nestingDepth,
                      "Maximum nesting depth found")
        .def("__repr__", [](const atom::utils::ValidationResult& result) {
            return std::format("ValidationResult(is_valid={}, error_position={}, nesting_depth={})",
                             result.isValid, result.errorPosition, result.nestingDepth);
        });

    // Exception classes
    py::register_exception<atom::utils::ValidationException>(m, "ValidationException",
        R"(Exception raised when string validation fails.
        
        This exception is thrown when validation encounters errors
        that cannot be represented in the ValidationResult.
        )");

    py::register_exception<atom::utils::BracketMismatchException>(m, "BracketMismatchException",
        R"(Exception raised when bracket validation fails.
        
        This exception is thrown when brackets are mismatched
        or improperly nested in a string.
        )");

    py::register_exception<atom::utils::QuoteMismatchException>(m, "QuoteMismatchException",
        R"(Exception raised when quote validation fails.
        
        This exception is thrown when quotes are unclosed
        or improperly matched in a string.
        )");

    // Quote type enumeration for QuoteMismatchException
    py::enum_<atom::utils::QuoteMismatchException::QuoteType>(m, "QuoteType",
        R"(Type of quote that caused a mismatch.)")
        .value("SINGLE", atom::utils::QuoteMismatchException::QuoteType::Single, "Single quote '")
        .value("DOUBLE", atom::utils::QuoteMismatchException::QuoteType::Double, "Double quote \"")
        .export_values();

    // Main validation functions
    m.def("is_valid_bracket", 
          [](const std::string& str, const atom::utils::ValidationOptions& options = {}) -> py::object {
              auto result = atom::utils::isValidBracket(str, options);
              if (result) {
                  return py::cast(*result);
              } else {
                  // Return error as exception or error message
                  throw std::runtime_error(result.error());
              }
          },
          py::arg("str"), py::arg("options") = atom::utils::ValidationOptions{},
          R"(Validate bracket matching in a string.
          
          Args:
              str: The string to validate.
              options: Validation options to use.
              
          Returns:
              ValidationResult object with validation details.
              
          Raises:
              RuntimeError: If validation encounters an error.
              
          Examples:
              >>> from atom.utils import valid_string
              >>> result = valid_string.is_valid_bracket("(hello [world])")
              >>> print(result.is_valid)  # True
          )");

    // Utility functions for common validation tasks
    m.def("validate_parentheses", 
          [](const std::string& str) -> bool {
              atom::utils::ValidationOptions options;
              options.strictMode = true;
              auto result = atom::utils::isValidBracket(str, options);
              return result && result->isValid;
          },
          py::arg("str"),
          R"(Simple validation for parentheses matching.
          
          Args:
              str: The string to validate.
              
          Returns:
              True if parentheses are properly matched, False otherwise.
              
          Examples:
              >>> valid_string.validate_parentheses("(hello)")
              True
              >>> valid_string.validate_parentheses("(hello")
              False
          )");

    m.def("validate_brackets", 
          [](const std::string& str) -> bool {
              atom::utils::ValidationOptions options;
              options.allowNested = true;
              auto result = atom::utils::isValidBracket(str, options);
              return result && result->isValid;
          },
          py::arg("str"),
          R"(Simple validation for all bracket types.
          
          Args:
              str: The string to validate.
              
          Returns:
              True if all brackets are properly matched, False otherwise.
              
          Examples:
              >>> valid_string.validate_brackets("{[()]}")
              True
              >>> valid_string.validate_brackets("{[(]}")
              False
          )");

    m.def("get_bracket_depth", 
          [](const std::string& str) -> int {
              atom::utils::ValidationOptions options;
              auto result = atom::utils::isValidBracket(str, options);
              if (result && result->isValid) {
                  return result->nestingDepth;
              }
              return -1;  // Invalid string
          },
          py::arg("str"),
          R"(Get the maximum nesting depth of brackets in a string.
          
          Args:
              str: The string to analyze.
              
          Returns:
              The maximum nesting depth, or -1 if the string is invalid.
              
          Examples:
              >>> valid_string.get_bracket_depth("((()))")
              3
              >>> valid_string.get_bracket_depth("(())")
              2
          )");

    m.def("find_bracket_errors", 
          [](const std::string& str) -> std::vector<atom::utils::BracketInfo> {
              atom::utils::ValidationOptions options;
              auto result = atom::utils::isValidBracket(str, options);
              if (result) {
                  return result->invalidBrackets;
              }
              return {};
          },
          py::arg("str"),
          R"(Find all bracket errors in a string.
          
          Args:
              str: The string to analyze.
              
          Returns:
              List of BracketInfo objects describing the errors.
              
          Examples:
              >>> errors = valid_string.find_bracket_errors("(hello]")
              >>> print(len(errors))  # Number of bracket errors
          )");

    // Constants for common validation scenarios
    m.attr("DEFAULT_OPTIONS") = atom::utils::ValidationOptions{};
    
    // Create a strict options constant
    atom::utils::ValidationOptions strict_options;
    strict_options.strictMode = true;
    strict_options.ignoreQuotes = false;
    strict_options.ignoreEscaped = false;
    m.attr("STRICT_OPTIONS") = strict_options;
    
    // Create a lenient options constant
    atom::utils::ValidationOptions lenient_options;
    lenient_options.strictMode = false;
    lenient_options.ignoreQuotes = true;
    lenient_options.allowNested = true;
    lenient_options.maxDepth = 100;
    m.attr("LENIENT_OPTIONS") = lenient_options;
}
