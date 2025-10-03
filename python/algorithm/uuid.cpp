#include "atom/algorithm/utils/uuid.hpp"
#include "atom/error/exception.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>

namespace py = pybind11;

PYBIND11_MODULE(uuid, m) {
    m.doc() = R"pbdoc(
        UUID Generation and Manipulation
        -------------------------------

        This module provides UUID (Universally Unique Identifier) generation and manipulation
        utilities optimized for high-performance applications.

        Features:
        - Time-based UUID generation (version 1)
        - Name-based UUID generation (versions 3 and 5)
        - Random UUID generation (version 4)
        - UUID parsing and validation
        - High-performance UUID operations
        - Thread-safe generation

        Examples:
            >>> from atom.algorithm.uuid import UUID
            >>> 
            >>> # Generate different types of UUIDs
            >>> time_uuid = UUID.generate_time_based()
            >>> random_uuid = UUID.generate_random()
            >>> 
            >>> # Parse UUID from string
            >>> uuid_obj = UUID.from_string("550e8400-e29b-41d4-a716-446655440000")
            >>> 
            >>> # Convert to different formats
            >>> uuid_str = uuid_obj.to_string()
            >>> uuid_bytes = uuid_obj.to_bytes()
            >>> 
            >>> # Validate UUID strings
            >>> is_valid = UUID.is_valid("550e8400-e29b-41d4-a716-446655440000")
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::error::InvalidArgument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const atom::error::RuntimeError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        }
    });

    // UUID class bindings
    py::class_<atom::algorithm::UUID>(m, "UUID", R"pbdoc(
        High-performance UUID class for generating and manipulating UUIDs.
        
        This class provides efficient UUID operations with support for all
        standard UUID versions and formats.
    )pbdoc")
        .def(py::init<>(), "Create a nil UUID (all zeros).")
        
        .def(py::init<const std::array<uint8_t, 16>&>(), py::arg("bytes"),
             "Create UUID from 16-byte array.")
        
        .def_static("generate_random", &atom::algorithm::UUID::generateRandom,
                   R"pbdoc(
                   Generate a random UUID (version 4).
                   
                   Returns:
                       A new random UUID
                       
                   Examples:
                       >>> uuid = UUID.generate_random()
                   )pbdoc")
        
        .def_static("generate_time_based", [](const std::array<uint8_t, 6>& node_id) {
            return atom::algorithm::UUID::generateTimeBased(node_id);
        }, py::arg("node_id"),
        R"pbdoc(
        Generate a time-based UUID (version 1).
        
        Args:
            node_id: 6-byte node identifier (MAC address or random)
            
        Returns:
            A new time-based UUID
            
        Examples:
            >>> node_id = UUID.generate_random_node_id()
            >>> uuid = UUID.generate_time_based(node_id)
        )pbdoc")
        
        .def_static("generate_name_based_md5", &atom::algorithm::UUID::generateNameBasedMD5,
                   py::arg("namespace_uuid"), py::arg("name"),
                   R"pbdoc(
                   Generate a name-based UUID using MD5 (version 3).
                   
                   Args:
                       namespace_uuid: The namespace UUID
                       name: The name string
                       
                   Returns:
                       A new name-based UUID
                   )pbdoc")
        
        .def_static("generate_name_based_sha1", &atom::algorithm::UUID::generateNameBasedSHA1,
                   py::arg("namespace_uuid"), py::arg("name"),
                   R"pbdoc(
                   Generate a name-based UUID using SHA-1 (version 5).
                   
                   Args:
                       namespace_uuid: The namespace UUID
                       name: The name string
                       
                   Returns:
                       A new name-based UUID
                   )pbdoc")
        
        .def_static("from_string", &atom::algorithm::UUID::fromString,
                   py::arg("uuid_string"),
                   R"pbdoc(
                   Parse UUID from string representation.
                   
                   Args:
                       uuid_string: UUID string (with or without hyphens)
                       
                   Returns:
                       Parsed UUID object
                       
                   Raises:
                       ValueError: If the string is not a valid UUID
                       
                   Examples:
                       >>> uuid = UUID.from_string("550e8400-e29b-41d4-a716-446655440000")
                   )pbdoc")
        
        .def_static("is_valid", &atom::algorithm::UUID::isValid,
                   py::arg("uuid_string"),
                   R"pbdoc(
                   Check if a string is a valid UUID format.
                   
                   Args:
                       uuid_string: String to validate
                       
                   Returns:
                       True if valid UUID format, False otherwise
                       
                   Examples:
                       >>> is_valid = UUID.is_valid("550e8400-e29b-41d4-a716-446655440000")
                   )pbdoc")
        
        .def_static("generate_random_node_id", &atom::algorithm::UUID::generateRandomNodeId,
                   R"pbdoc(
                   Generate a random 6-byte node ID for time-based UUIDs.
                   
                   Returns:
                       6-byte array suitable for use as node ID
                       
                   Examples:
                       >>> node_id = UUID.generate_random_node_id()
                   )pbdoc")
        
        .def("to_string", [](const atom::algorithm::UUID& self, bool with_hyphens) {
            return self.toString(with_hyphens);
        }, py::arg("with_hyphens") = true,
        R"pbdoc(
        Convert UUID to string representation.
        
        Args:
            with_hyphens: Whether to include hyphens in the output
            
        Returns:
            String representation of the UUID
            
        Examples:
            >>> uuid_str = uuid.to_string()  # With hyphens
            >>> uuid_str_compact = uuid.to_string(False)  # Without hyphens
        )pbdoc")
        
        .def("to_bytes", &atom::algorithm::UUID::toBytes,
             R"pbdoc(
             Convert UUID to 16-byte array.
             
             Returns:
                 16-byte array representation of the UUID
             )pbdoc")
        
        .def("get_version", &atom::algorithm::UUID::getVersion,
             R"pbdoc(
             Get the version number of the UUID.
             
             Returns:
                 Version number (1-5)
             )pbdoc")
        
        .def("get_variant", &atom::algorithm::UUID::getVariant,
             R"pbdoc(
             Get the variant of the UUID.
             
             Returns:
                 Variant identifier
             )pbdoc")
        
        .def("is_nil", &atom::algorithm::UUID::isNil,
             R"pbdoc(
             Check if this is a nil UUID (all zeros).
             
             Returns:
                 True if nil UUID, False otherwise
             )pbdoc")
        
        .def("get_timestamp", &atom::algorithm::UUID::getTimestamp,
             R"pbdoc(
             Extract timestamp from time-based UUID (version 1).
             
             Returns:
                 Timestamp as system clock time point
                 
             Raises:
                 ValueError: If UUID is not version 1
             )pbdoc")
        
        .def("get_node_id", &atom::algorithm::UUID::getNodeId,
             R"pbdoc(
             Extract node ID from time-based UUID (version 1).
             
             Returns:
                 6-byte node ID array
                 
             Raises:
                 ValueError: If UUID is not version 1
             )pbdoc")
        
        .def("get_clock_sequence", &atom::algorithm::UUID::getClockSequence,
             R"pbdoc(
             Extract clock sequence from time-based UUID (version 1).
             
             Returns:
                 Clock sequence value
                 
             Raises:
                 ValueError: If UUID is not version 1
             )pbdoc")
        
        // Comparison operators
        .def("__eq__", [](const atom::algorithm::UUID& self, const atom::algorithm::UUID& other) {
            return self == other;
        }, "Equality comparison")
        
        .def("__ne__", [](const atom::algorithm::UUID& self, const atom::algorithm::UUID& other) {
            return self != other;
        }, "Inequality comparison")
        
        .def("__lt__", [](const atom::algorithm::UUID& self, const atom::algorithm::UUID& other) {
            return self < other;
        }, "Less than comparison")
        
        .def("__le__", [](const atom::algorithm::UUID& self, const atom::algorithm::UUID& other) {
            return self <= other;
        }, "Less than or equal comparison")
        
        .def("__gt__", [](const atom::algorithm::UUID& self, const atom::algorithm::UUID& other) {
            return self > other;
        }, "Greater than comparison")
        
        .def("__ge__", [](const atom::algorithm::UUID& self, const atom::algorithm::UUID& other) {
            return self >= other;
        }, "Greater than or equal comparison")
        
        // Python special methods
        .def("__str__", [](const atom::algorithm::UUID& self) {
            return self.toString();
        }, "String representation")
        
        .def("__repr__", [](const atom::algorithm::UUID& self) {
            return "UUID('" + self.toString() + "')";
        }, "Representation string")
        
        .def("__hash__", [](const atom::algorithm::UUID& self) {
            return self.hash();
        }, "Hash value for use in dictionaries");

    // Utility functions
    m.def("generate_uuid_batch", [](size_t count) {
        std::vector<atom::algorithm::UUID> batch;
        batch.reserve(count);
        
        for (size_t i = 0; i < count; ++i) {
            batch.push_back(atom::algorithm::UUID::generateRandom());
        }
        
        return batch;
    }, py::arg("count"),
    R"pbdoc(
    Generate a batch of random UUIDs efficiently.
    
    Args:
        count: Number of UUIDs to generate
        
    Returns:
        List of UUID objects
        
    Examples:
        >>> uuids = generate_uuid_batch(1000)
    )pbdoc");

    // Predefined namespace UUIDs
    m.attr("NAMESPACE_DNS") = atom::algorithm::UUID::fromString("6ba7b810-9dad-11d1-80b4-00c04fd430c8");
    m.attr("NAMESPACE_URL") = atom::algorithm::UUID::fromString("6ba7b811-9dad-11d1-80b4-00c04fd430c8");
    m.attr("NAMESPACE_OID") = atom::algorithm::UUID::fromString("6ba7b812-9dad-11d1-80b4-00c04fd430c8");
    m.attr("NAMESPACE_X500") = atom::algorithm::UUID::fromString("6ba7b814-9dad-11d1-80b4-00c04fd430c8");
}

// Additional module for error calibration utilities
PYBIND11_MODULE(error_calibration, m) {
    m.doc() = R"pbdoc(
        Error Calibration and Analysis Utilities
        ---------------------------------------

        This module provides advanced error calibration and analysis tools for
        numerical algorithms and measurement systems.

        Features:
        - Linear calibration with least squares fitting
        - Polynomial calibration for non-linear relationships
        - Power law calibration for exponential relationships
        - Statistical metrics (R-squared, MSE, MAE)
        - Residual analysis
        - Robust calibration methods

        Examples:
            >>> from atom.algorithm.error_calibration import LinearCalibration
            >>>
            >>> # Measured vs actual values
            >>> measured = [1.1, 2.05, 2.98, 4.02, 5.1]
            >>> actual = [1.0, 2.0, 3.0, 4.0, 5.0]
            >>>
            >>> # Perform linear calibration
            >>> calibrator = LinearCalibration()
            >>> calibrator.calibrate(measured, actual)
            >>>
            >>> # Apply calibration to new measurements
            >>> corrected = calibrator.apply(3.5)
            >>>
            >>> # Get calibration metrics
            >>> r_squared = calibrator.get_r_squared()
            >>> mse = calibrator.get_mse()
    )pbdoc";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::error::InvalidArgument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const atom::error::RuntimeError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        }
    });

    // Linear Calibration class
    py::class_<atom::algorithm::LinearCalibration<double>>(m, "LinearCalibration", R"pbdoc(
        Linear calibration using least squares fitting.

        Fits a linear relationship y = mx + b between measured and actual values.
    )pbdoc")
        .def(py::init<>(), "Initialize linear calibration.")

        .def("calibrate", [](atom::algorithm::LinearCalibration<double>& self,
                            const std::vector<double>& measured,
                            const std::vector<double>& actual) {
            self.calibrate(measured, actual);
        }, py::arg("measured"), py::arg("actual"),
        R"pbdoc(
        Perform linear calibration on the given data.

        Args:
            measured: Vector of measured values
            actual: Vector of corresponding actual values

        Raises:
            ValueError: If vectors have different sizes or insufficient data
        )pbdoc")

        .def("apply", &atom::algorithm::LinearCalibration<double>::apply,
             py::arg("value"),
             R"pbdoc(
             Apply calibration to a measured value.

             Args:
                 value: Measured value to correct

             Returns:
                 Calibrated value
             )pbdoc")

        .def("get_slope", &atom::algorithm::LinearCalibration<double>::getSlope,
             "Get the calibration slope parameter.")

        .def("get_intercept", &atom::algorithm::LinearCalibration<double>::getIntercept,
             "Get the calibration intercept parameter.")

        .def("get_r_squared", [](const atom::algorithm::LinearCalibration<double>& self) {
            auto r_sq = self.getRSquared();
            return r_sq.has_value() ? r_sq.value() : py::none();
        }, "Get the R-squared value (coefficient of determination).")

        .def("get_mse", &atom::algorithm::LinearCalibration<double>::getMSE,
             "Get the mean squared error.")

        .def("get_mae", &atom::algorithm::LinearCalibration<double>::getMAE,
             "Get the mean absolute error.")

        .def("get_residuals", &atom::algorithm::LinearCalibration<double>::getResiduals,
             "Get the residuals (differences between fitted and actual values).")

        .def("print_parameters", &atom::algorithm::LinearCalibration<double>::printParameters,
             "Print calibration parameters and metrics to console.");

    // Polynomial Calibration class
    py::class_<atom::algorithm::PolynomialCalibration<double>>(m, "PolynomialCalibration", R"pbdoc(
        Polynomial calibration for non-linear relationships.

        Fits a polynomial relationship of specified degree between measured and actual values.
    )pbdoc")
        .def(py::init<int>(), py::arg("degree") = 2,
             "Initialize polynomial calibration with specified degree.")

        .def("calibrate", [](atom::algorithm::PolynomialCalibration<double>& self,
                            const std::vector<double>& measured,
                            const std::vector<double>& actual) {
            self.calibrate(measured, actual);
        }, py::arg("measured"), py::arg("actual"),
        R"pbdoc(
        Perform polynomial calibration on the given data.

        Args:
            measured: Vector of measured values
            actual: Vector of corresponding actual values
        )pbdoc")

        .def("apply", &atom::algorithm::PolynomialCalibration<double>::apply,
             py::arg("value"),
             "Apply polynomial calibration to a measured value.")

        .def("get_coefficients", &atom::algorithm::PolynomialCalibration<double>::getCoefficients,
             "Get the polynomial coefficients.")

        .def("get_degree", &atom::algorithm::PolynomialCalibration<double>::getDegree,
             "Get the polynomial degree.")

        .def("get_r_squared", [](const atom::algorithm::PolynomialCalibration<double>& self) {
            auto r_sq = self.getRSquared();
            return r_sq.has_value() ? r_sq.value() : py::none();
        }, "Get the R-squared value.")

        .def("get_mse", &atom::algorithm::PolynomialCalibration<double>::getMSE,
             "Get the mean squared error.")

        .def("get_mae", &atom::algorithm::PolynomialCalibration<double>::getMAE,
             "Get the mean absolute error.")

        .def("get_residuals", &atom::algorithm::PolynomialCalibration<double>::getResiduals,
             "Get the residuals.")

        .def("print_parameters", &atom::algorithm::PolynomialCalibration<double>::printParameters,
             "Print calibration parameters and metrics.");

    // Power Law Calibration class
    py::class_<atom::algorithm::PowerLawCalibration<double>>(m, "PowerLawCalibration", R"pbdoc(
        Power law calibration for exponential relationships.

        Fits a power law relationship y = a * x^b between measured and actual values.
    )pbdoc")
        .def(py::init<>(), "Initialize power law calibration.")

        .def("calibrate", [](atom::algorithm::PowerLawCalibration<double>& self,
                            const std::vector<double>& measured,
                            const std::vector<double>& actual) {
            self.calibrate(measured, actual);
        }, py::arg("measured"), py::arg("actual"),
        "Perform power law calibration on the given data.")

        .def("apply", &atom::algorithm::PowerLawCalibration<double>::apply,
             py::arg("value"),
             "Apply power law calibration to a measured value.")

        .def("get_residuals", &atom::algorithm::PowerLawCalibration<double>::getResiduals,
             "Get the residuals.")

        .def("print_parameters", &atom::algorithm::PowerLawCalibration<double>::printParameters,
             "Print calibration parameters and metrics.");
}
