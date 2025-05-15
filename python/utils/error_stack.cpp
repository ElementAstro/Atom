#include "atom/utils/error_stack.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <ctime>
#include <sstream>

namespace py = pybind11;

// Format timestamp as human readable string
std::string format_timestamp(time_t timestamp) {
    char buffer[64];
    std::tm* timeinfo = std::localtime(&timestamp);
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return std::string(buffer);
}

PYBIND11_MODULE(error_stack, m) {
    m.doc() = "Error tracking and management module for the atom package";

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

    // Bind ErrorInfo struct
    py::class_<atom::error::ErrorInfo>(
        m, "ErrorInfo",
        R"(Error information structure containing details about an error.

This class holds comprehensive information about an error, including the error message,
the module and function where it occurred, line number, file name, and timestamp.

Examples:
    >>> from atom.utils import error_stack
    >>> # ErrorInfo objects are typically created by the ErrorStack class
    >>> stack = error_stack.ErrorStack()
    >>> stack.insert_error("File not found", "IO", "readFile", 42, "file_io.cpp")
    >>> error = stack.get_latest_error()
    >>> print(error.error_message)
    'File not found'
)")
        .def(py::init<>())
        .def_readwrite("error_message", &atom::error::ErrorInfo::errorMessage,
                       "The error message")
        .def_readwrite("module_name", &atom::error::ErrorInfo::moduleName,
                       "Module name where the error occurred")
        .def_readwrite("function_name", &atom::error::ErrorInfo::functionName,
                       "Function name where the error occurred")
        .def_readwrite("line", &atom::error::ErrorInfo::line,
                       "Line number where the error occurred")
        .def_readwrite("file_name", &atom::error::ErrorInfo::fileName,
                       "File name where the error occurred")
        .def_property_readonly(
            "timestamp",
            [](const atom::error::ErrorInfo& self) { return self.timestamp; },
            "Timestamp when the error occurred (seconds since epoch)")
        .def_property_readonly(
            "formatted_time",
            [](const atom::error::ErrorInfo& self) {
                return format_timestamp(self.timestamp);
            },
            "Human-readable formatted timestamp")
        .def_readwrite("uuid", &atom::error::ErrorInfo::uuid,
                       "UUID of the error")
        .def(
            "__eq__",
            [](const atom::error::ErrorInfo& self,
               const atom::error::ErrorInfo& other) { return self == other; },
            py::is_operator())
        .def("__repr__",
             [](const atom::error::ErrorInfo& self) {
                 std::ostringstream oss;
                 oss << "ErrorInfo(message='" << self.errorMessage
                     << "', module='" << self.moduleName << "', function='"
                     << self.functionName << "', file='" << self.fileName
                     << "', line=" << self.line << ", time='"
                     << format_timestamp(self.timestamp) << "')";
                 return oss.str();
             })
        .def_readwrite("level", &atom::error::ErrorInfo::level,
                       "Error severity level")
        .def_readwrite("category", &atom::error::ErrorInfo::category,
                       "Error category")
        .def_readwrite("error_code", &atom::error::ErrorInfo::errorCode,
                       "Error code")
        .def_property(
            "metadata",
            [](const atom::error::ErrorInfo& self) {
                py::dict result;
                for (const auto& [key, value] : self.metadata) {
                    result[py::str(key)] = py::str(value);
                }
                return result;
            },
            [](atom::error::ErrorInfo& self, const py::dict& metadata) {
                self.metadata.clear();
                for (const auto& item : metadata) {
                    std::string key = item.first.cast<std::string>();
                    std::string value = item.second.cast<std::string>();
                    self.metadata[key] = value;
                }
            },
            "Additional metadata for the error");

    // Bind ErrorLevel enum
    py::enum_<atom::error::ErrorLevel>(m, "ErrorLevel",
                                       "Enumeration of error severity levels")
        .value("DEBUG", atom::error::ErrorLevel::Debug, "Debug information")
        .value("INFO", atom::error::ErrorLevel::Info, "Informational message")
        .value("WARNING", atom::error::ErrorLevel::Warning, "Warning message")
        .value("ERROR", atom::error::ErrorLevel::Error, "Error message")
        .value("CRITICAL", atom::error::ErrorLevel::Critical, "Critical error")
        .export_values();

    // Bind ErrorCategory enum
    py::enum_<atom::error::ErrorCategory>(m, "ErrorCategory",
                                          "Enumeration of error categories")
        .value("GENERAL", atom::error::ErrorCategory::General, "General error")
        .value("SYSTEM", atom::error::ErrorCategory::System, "System error")
        .value("NETWORK", atom::error::ErrorCategory::Network, "Network error")
        .value("DATABASE", atom::error::ErrorCategory::Database,
               "Database error")
        .value("SECURITY", atom::error::ErrorCategory::Security,
               "Security error")
        .value("IO", atom::error::ErrorCategory::IO, "Input/Output error")
        .value("MEMORY", atom::error::ErrorCategory::Memory, "Memory error")
        .value("CONFIGURATION", atom::error::ErrorCategory::Configuration,
               "Configuration error")
        .value("VALIDATION", atom::error::ErrorCategory::Validation,
               "Validation error")
        .value("OTHER", atom::error::ErrorCategory::Other, "Other error type")
        .export_values();

    // Bind ErrorInfoBuilder class
    py::class_<atom::error::ErrorInfoBuilder>(
        m, "ErrorInfoBuilder", "Builder for creating ErrorInfo objects")
        .def(py::init<>())
        .def("message", &atom::error::ErrorInfoBuilder::message,
             py::arg("message"), "Set the error message")
        .def("module", &atom::error::ErrorInfoBuilder::module,
             py::arg("module"), "Set the module name")
        .def("function", &atom::error::ErrorInfoBuilder::function,
             py::arg("function"), "Set the function name")
        .def("file", &atom::error::ErrorInfoBuilder::file, py::arg("file"),
             py::arg("line"), "Set the file name and line number")
        .def("level", &atom::error::ErrorInfoBuilder::level, py::arg("level"),
             "Set the error severity level")
        .def("category", &atom::error::ErrorInfoBuilder::category,
             py::arg("category"), "Set the error category")
        .def("code", &atom::error::ErrorInfoBuilder::code, py::arg("code"),
             "Set the error code")
        .def("add_metadata", &atom::error::ErrorInfoBuilder::addMetadata,
             py::arg("key"), py::arg("value"), "Add metadata key-value pair")
        .def("build", &atom::error::ErrorInfoBuilder::build,
             "Build and return the ErrorInfo object");

    // Bind ErrorStatistics struct
    py::class_<atom::error::ErrorStatistics>(m, "ErrorStatistics",
                                             "Statistics about errors")
        .def_readonly("total_errors",
                      &atom::error::ErrorStatistics::totalErrors,
                      "Total number of errors recorded")
        .def_readonly("unique_errors",
                      &atom::error::ErrorStatistics::uniqueErrors,
                      "Number of unique errors")
        .def_readonly("first_error_time",
                      &atom::error::ErrorStatistics::firstErrorTime,
                      "Time of the first error")
        .def_readonly("last_error_time",
                      &atom::error::ErrorStatistics::lastErrorTime,
                      "Time of the most recent error")
        .def_property_readonly(
            "errors_by_level",
            [](const atom::error::ErrorStatistics& stats) {
                py::dict result;
                result["debug"] = stats.errorsByLevel[0];
                result["info"] = stats.errorsByLevel[1];
                result["warning"] = stats.errorsByLevel[2];
                result["error"] = stats.errorsByLevel[3];
                result["critical"] = stats.errorsByLevel[4];
                return result;
            },
            "Count of errors by severity level")
        .def_property_readonly(
            "errors_by_category",
            [](const atom::error::ErrorStatistics& stats) {
                py::dict result;
                result["general"] = stats.errorsByCategory[0];
                result["system"] = stats.errorsByCategory[1];
                result["network"] = stats.errorsByCategory[2];
                result["database"] = stats.errorsByCategory[3];
                result["security"] = stats.errorsByCategory[4];
                result["io"] = stats.errorsByCategory[5];
                result["memory"] = stats.errorsByCategory[6];
                result["configuration"] = stats.errorsByCategory[7];
                result["validation"] = stats.errorsByCategory[8];
                result["other"] = stats.errorsByCategory[9];
                return result;
            },
            "Count of errors by category")
        .def_readonly("top_modules", &atom::error::ErrorStatistics::topModules,
                      "Modules with the most errors")
        .def_readonly("top_messages",
                      &atom::error::ErrorStatistics::topMessages,
                      "Most common error messages");

    // Bind ErrorStack class
    py::class_<atom::error::ErrorStack,
               std::shared_ptr<atom::error::ErrorStack>>(
        m, "ErrorStack",
        R"(A stack for tracking and managing errors.

This class provides functionality to record, filter, and analyze errors that occur
during program execution. It supports various operations like inserting new errors,
filtering errors by module or severity, and exporting error data.

Examples:
    >>> from atom.utils import error_stack
    >>> from atom.utils.error_stack import ErrorLevel, ErrorCategory
    >>> 
    >>> # Create an error stack
    >>> stack = error_stack.ErrorStack()
    >>> 
    >>> # Insert a simple error
    >>> stack.insert_error("File not found", "IO", "readFile", 42, "file_io.cpp")
    >>> 
    >>> # Insert an error with additional information
    >>> stack.insert_error_with_level(
    ...     "Connection timeout", "Network", "connect", 123, "network.cpp",
    ...     ErrorLevel.ERROR, ErrorCategory.NETWORK, 408)
    >>> 
    >>> # Get the latest error
    >>> latest = stack.get_latest_error()
    >>> if latest:
    ...     print(f"Latest error: {latest.error_message} in {latest.module_name}")
    >>> 
    >>> # Export errors to JSON
    >>> json_data = stack.export_to_json()
)")
        .def(py::init<>())
        .def_static("create_shared", &atom::error::ErrorStack::createShared,
                    "Create a shared pointer to an ErrorStack object")
        .def_static(
            "create_unique",
            []() { return atom::error::ErrorStack::createUnique(); },
            "Create a unique pointer to an ErrorStack object")
        .def(
            "insert_error",
            [](atom::error::ErrorStack& self, const std::string& errorMessage,
               const std::string& moduleName, const std::string& functionName,
               int line, const std::string& fileName) {
                return self.insertError(errorMessage, moduleName, functionName,
                                        line, fileName);
            },
            py::arg("error_message"), py::arg("module_name"),
            py::arg("function_name"), py::arg("line"), py::arg("file_name"),
            "Insert a new error into the error stack")
        .def(
            "insert_error_with_level",
            [](atom::error::ErrorStack& self, const std::string& errorMessage,
               const std::string& moduleName, const std::string& functionName,
               int line, const std::string& fileName,
               atom::error::ErrorLevel level,
               atom::error::ErrorCategory category, int64_t errorCode) {
                return self.insertErrorWithLevel(errorMessage, moduleName,
                                                 functionName, line, fileName,
                                                 level, category, errorCode);
            },
            py::arg("error_message"), py::arg("module_name"),
            py::arg("function_name"), py::arg("line"), py::arg("file_name"),
            py::arg("level") = atom::error::ErrorLevel::Error,
            py::arg("category") = atom::error::ErrorCategory::General,
            py::arg("error_code") = 0,
            "Insert a new error with level and category information")
        .def("insert_error_info", &atom::error::ErrorStack::insertErrorInfo,
             py::arg("error_info"),
             "Insert a fully constructed ErrorInfo object")
        .def("insert_error_async", &atom::error::ErrorStack::insertErrorAsync,
             py::arg("error_info"), "Insert an error asynchronously")
        .def("process_async_errors",
             &atom::error::ErrorStack::processAsyncErrors,
             "Process pending asynchronous errors and return the number "
             "processed")
        .def("start_async_processing",
             &atom::error::ErrorStack::startAsyncProcessing,
             py::arg("interval_ms") = 100,
             "Start background processing of async errors")
        .def("stop_async_processing",
             &atom::error::ErrorStack::stopAsyncProcessing,
             "Stop background processing of async errors")
        .def(
            "register_error_callback",
            [](atom::error::ErrorStack& self, py::function callback) {
                self.registerErrorCallback(
                    [callback](const atom::error::ErrorInfo& error) {
                        py::gil_scoped_acquire acquire;
                        try {
                            callback(error);
                        } catch (py::error_already_set& e) {
                            // Handle any Python exceptions from the callback
                            PyErr_Clear();
                            py::print(
                                "Exception in error callback:", e.what(),
                                py::arg("file") =
                                    py::module::import("sys").attr("stderr"));
                        }
                    });
            },
            py::arg("callback"), "Register a callback function for new errors")
        .def(
            "set_filtered_modules",
            [](atom::error::ErrorStack& self, const py::list& modules) {
                std::vector<std::string> moduleVector;
                for (const auto& module : modules) {
                    moduleVector.push_back(module.cast<std::string>());
                }
                self.setFilteredModules(moduleVector);
            },
            py::arg("modules"),
            "Set modules to filter out when printing errors")
        .def("clear_filtered_modules",
             &atom::error::ErrorStack::clearFilteredModules,
             "Clear the list of filtered modules")
        .def("print_filtered_error_stack",
             &atom::error::ErrorStack::printFilteredErrorStack,
             "Print the filtered error stack to standard output")
        .def("get_filtered_errors_by_module",
             &atom::error::ErrorStack::getFilteredErrorsByModule,
             py::arg("module_name"), "Get errors filtered by a specific module")
        .def("get_filtered_errors_by_level",
             &atom::error::ErrorStack::getFilteredErrorsByLevel,
             py::arg("level"), "Get errors filtered by severity level")
        .def("get_filtered_errors_by_category",
             &atom::error::ErrorStack::getFilteredErrorsByCategory,
             py::arg("category"), "Get errors filtered by category")
        .def("get_compressed_errors",
             &atom::error::ErrorStack::getCompressedErrors,
             "Get a string containing the compressed errors in the stack")
        .def("is_empty", &atom::error::ErrorStack::isEmpty,
             "Check if the error stack is empty")
        .def("size", &atom::error::ErrorStack::size,
             "Get the number of errors in the stack")
        .def("get_latest_error", &atom::error::ErrorStack::getLatestError,
             "Get the most recent error")
        .def("get_errors_in_time_range",
             &atom::error::ErrorStack::getErrorsInTimeRange, py::arg("start"),
             py::arg("end"),
             "Get errors within a specific time range (Unix timestamps)")
        .def("get_statistics", &atom::error::ErrorStack::getStatistics,
             "Get error statistics")
        .def("clear", &atom::error::ErrorStack::clear,
             "Clear all errors in the stack")
        .def("export_to_json", &atom::error::ErrorStack::exportToJson,
             "Export errors to JSON format")
        .def("export_to_csv", &atom::error::ErrorStack::exportToCsv,
             py::arg("include_metadata") = false,
             "Export errors to CSV format");

#ifdef ATOM_ERROR_STACK_USE_SERIALIZATION
    // Add serialization support if available
    m.def(
        "serialize_stack",
        [](const atom::error::ErrorStack& stack) { return stack.serialize(); },
        "Serialize an error stack to binary format");

    m.def(
        "deserialize_stack",
        [](atom::error::ErrorStack& stack, py::bytes data) {
            py::buffer_info info(py::buffer(data).request());
            const char* ptr = static_cast<const char*>(info.ptr);
            std::span<const char> span(ptr, info.size);
            return stack.deserialize(span);
        },
        py::arg("stack"), py::arg("data"),
        "Deserialize binary data into an error stack");
#endif

    // Convenience function to create an error builder
    m.def(
        "build_error", []() { return atom::error::ErrorInfoBuilder(); },
        "Create a new ErrorInfoBuilder for constructing error information");

    // Version information
    m.attr("__version__") = "1.0.0";
}