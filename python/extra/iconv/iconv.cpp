#include "atom/extra/iconv/iconv_cpp.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(iconv, m) {
    m.doc() = R"(Character encoding conversion module for the atom package.

This module provides a modern C++ interface for character encoding conversion
using iconv, with support for various encodings, error handling policies,
BOM detection, and encoding detection.

Examples:
    >>> from atom.extra import iconv
    >>>
    >>> # Simple conversion
    >>> converter = iconv.Converter("UTF-8", "ISO-8859-1")
    >>> result = converter.convert_string("Hello, world!")
    >>> print(result)
    >>>
    >>> # Conversion with options
    >>> options = iconv.ConversionOptions()
    >>> options.error_policy = iconv.ErrorHandlingPolicy.Replace
    >>> options.replacement_char = '?'
    >>> converter = iconv.Converter("UTF-8", "ASCII", options)
    >>> result = converter.convert_string("Héllo, wörld!")
    >>>
    >>> # BOM detection
    >>> data = b'\xef\xbb\xbfHello'
    >>> encoding, bom_size = iconv.BomHandler.detect_bom(data)
    >>> print(f"Detected: {encoding}, BOM size: {bom_size}")
    >>>
    >>> # Encoding detection
    >>> data = "你好世界".encode('utf-8')
    >>> results = iconv.EncodingDetector.detect_encoding(data)
    >>> print(f"Most likely: {results[0].encoding}")
)";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const iconv_cpp::IconvError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const iconv_cpp::IconvInitError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const iconv_cpp::IconvConversionError& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Exception classes
    py::register_exception<iconv_cpp::IconvError>(m, "IconvError",
                                                  PyExc_RuntimeError);
    py::register_exception<iconv_cpp::IconvInitError>(m, "IconvInitError",
                                                      PyExc_RuntimeError);

    auto iconvConversionError =
        py::register_exception<iconv_cpp::IconvConversionError>(
            m, "IconvConversionError", PyExc_RuntimeError);
    iconvConversionError.def(
        "processed_bytes", &iconv_cpp::IconvConversionError::processed_bytes,
        "Get the number of bytes processed before the error occurred");

    // ErrorHandlingPolicy enum
    py::enum_<iconv_cpp::ErrorHandlingPolicy>(
        m, "ErrorHandlingPolicy",
        R"(Error handling policy for character conversion.

Defines how to handle characters that cannot be converted to the target encoding.)")
        .value("Strict", iconv_cpp::ErrorHandlingPolicy::Strict,
               "Raise an exception on conversion errors")
        .value("Skip", iconv_cpp::ErrorHandlingPolicy::Skip,
               "Skip characters that cannot be converted")
        .value("Replace", iconv_cpp::ErrorHandlingPolicy::Replace,
               "Replace unconvertible characters with a replacement character")
        .value("Ignore", iconv_cpp::ErrorHandlingPolicy::Ignore,
               "Ignore conversion errors and continue")
        .export_values();

    // ConversionOptions struct
    py::class_<iconv_cpp::ConversionOptions>(
        m, "ConversionOptions",
        R"(Configuration options for character encoding conversion.

This struct defines various options that control how character conversion is performed,
including error handling policy, replacement characters, and special features.

Examples:
    >>> options = iconv.ConversionOptions()
    >>> options.error_policy = iconv.ErrorHandlingPolicy.Replace
    >>> options.replacement_char = '?'
    >>> options.translit = True
    >>> options.ignore_bom = False
)")
        .def(py::init<>(), "Create default conversion options")
        .def_readwrite("error_policy",
                       &iconv_cpp::ConversionOptions::error_policy,
                       "Error handling policy for conversion failures")
        .def_readwrite(
            "replacement_char", &iconv_cpp::ConversionOptions::replacement_char,
            "Character to use as replacement for unconvertible characters")
        .def_readwrite("enable_fallback",
                       &iconv_cpp::ConversionOptions::enable_fallback,
                       "Enable fallback conversion methods")
        .def_readwrite("translit", &iconv_cpp::ConversionOptions::translit,
                       "Enable transliteration (convert similar characters)")
        .def_readwrite("ignore_bom", &iconv_cpp::ConversionOptions::ignore_bom,
                       "Ignore Byte Order Mark (BOM) in input")
        .def("create_encoding_string",
             &iconv_cpp::ConversionOptions::create_encoding_string,
             py::arg("base_encoding"),
             R"(Create an encoding string with options applied.

Args:
    base_encoding: Base encoding name.

Returns:
    Encoding string with options (e.g., "UTF-8//TRANSLIT//IGNORE").
)");

    // ConversionState struct
    py::class_<iconv_cpp::ConversionState>(
        m, "ConversionState",
        R"(State information for incremental character conversion.

This struct tracks the progress of character conversion operations,
including processed bytes and completion status.)")
        .def(py::init<>(), "Create a new conversion state")
        .def_readwrite("processed_input_bytes",
                       &iconv_cpp::ConversionState::processed_input_bytes,
                       "Number of input bytes processed")
        .def_readwrite("processed_output_bytes",
                       &iconv_cpp::ConversionState::processed_output_bytes,
                       "Number of output bytes produced")
        .def_readwrite("is_complete", &iconv_cpp::ConversionState::is_complete,
                       "Whether the conversion is complete")
        .def_readwrite("state_data", &iconv_cpp::ConversionState::state_data,
                       "Internal state data for the conversion")
        .def("reset", &iconv_cpp::ConversionState::reset,
             R"(Reset the conversion state to initial values.)");

    // EncodingInfo struct
    py::class_<iconv_cpp::EncodingInfo>(
        m, "EncodingInfo",
        R"(Information about a character encoding.

This struct contains metadata about a character encoding,
including its properties and characteristics.)")
        .def(py::init<>(), "Create empty encoding info")
        .def_readwrite("name", &iconv_cpp::EncodingInfo::name,
                       "Canonical name of the encoding")
        .def_readwrite("description", &iconv_cpp::EncodingInfo::description,
                       "Human-readable description of the encoding")
        .def_readwrite("is_ascii_compatible",
                       &iconv_cpp::EncodingInfo::is_ascii_compatible,
                       "Whether the encoding is ASCII-compatible")
        .def_readwrite("min_char_size", &iconv_cpp::EncodingInfo::min_char_size,
                       "Minimum character size in bytes")
        .def_readwrite("max_char_size", &iconv_cpp::EncodingInfo::max_char_size,
                       "Maximum character size in bytes")
        .def_readwrite("has_bom", &iconv_cpp::EncodingInfo::has_bom,
                       "Whether the encoding typically uses a BOM")
        .def_static("is_alias", &iconv_cpp::EncodingInfo::is_alias,
                    py::arg("encoding"), py::arg("possible_alias"),
                    R"(Check if one encoding name is an alias for another.

Args:
    encoding: Primary encoding name.
    possible_alias: Potential alias name.

Returns:
    True if the names refer to the same encoding.
)");

    // EncodingDetectionResult struct
    py::class_<iconv_cpp::EncodingDetectionResult>(
        m, "EncodingDetectionResult",
        R"(Result of encoding detection.

This struct contains the detected encoding and confidence level.)")
        .def(py::init<>(), "Create empty detection result")
        .def_readwrite("encoding",
                       &iconv_cpp::EncodingDetectionResult::encoding,
                       "Detected encoding name")
        .def_readwrite("confidence",
                       &iconv_cpp::EncodingDetectionResult::confidence,
                       "Confidence level (0.0 to 1.0)")
        .def("__gt__", &iconv_cpp::EncodingDetectionResult::operator>,
             py::arg("other"),
             R"(Compare detection results by confidence level.)");

    // BomHandler class
    py::class_<iconv_cpp::BomHandler>(
        m, "BomHandler",
        R"(Utility class for handling Byte Order Marks (BOM).

This class provides static methods for detecting, adding, and removing BOMs
to character data for various Unicode encodings.

Examples:
    >>> # Detect BOM
    >>> data = b'\xef\xbb\xbfHello, world!'
    >>> encoding, bom_size = iconv.BomHandler.detect_bom(data)
    >>> print(f"Encoding: {encoding}, BOM size: {bom_size}")
    >>>
    >>> # Add BOM
    >>> data_with_bom = iconv.BomHandler.add_bom("UTF-8", b"Hello")
    >>> print(data_with_bom[:3])  # Should show UTF-8 BOM
    >>>
    >>> # Remove BOM
    >>> data_without_bom = iconv.BomHandler.remove_bom(data_with_bom)
)")
        .def_static("detect_bom", &iconv_cpp::BomHandler::detect_bom,
                    py::arg("data"),
                    R"(Detect BOM in character data.

Args:
    data: Input data to check for BOM.

Returns:
    Tuple of (encoding_name, bom_size). Empty encoding if no BOM detected.
)")
        .def_static("add_bom", &iconv_cpp::BomHandler::add_bom,
                    py::arg("encoding"), py::arg("data"),
                    R"(Add BOM to character data.

Args:
    encoding: Target encoding name.
    data: Input data to prepend BOM to.

Returns:
    Data with BOM prepended.
)")
        .def_static("remove_bom", &iconv_cpp::BomHandler::remove_bom,
                    py::arg("data"),
                    R"(Remove BOM from character data if present.

Args:
    data: Input data that may contain a BOM.

Returns:
    Data with BOM removed (or original data if no BOM found).
)");

    // EncodingDetector class
    py::class_<iconv_cpp::EncodingDetector>(
        m, "EncodingDetector",
        R"(Utility class for detecting character encodings.

This class provides static methods for detecting the encoding of character data
by analyzing byte patterns and BOM markers.

Examples:
    >>> # Detect encoding with confidence scores
    >>> data = "你好世界".encode('utf-8')
    >>> results = iconv.EncodingDetector.detect_encoding(data, max_results=3)
    >>> for result in results:
    ...     print(f"{result.encoding}: {result.confidence}")
    >>>
    >>> # Get most likely encoding
    >>> encoding = iconv.EncodingDetector.detect_most_likely_encoding(data)
    >>> print(f"Detected: {encoding}")
)")
        .def_static("detect_encoding",
                    &iconv_cpp::EncodingDetector::detect_encoding,
                    py::arg("data"), py::arg("max_results") = 3,
                    R"(Detect possible encodings for the given data.

Args:
    data: Input data to analyze.
    max_results: Maximum number of results to return (default: 3).

Returns:
    List of EncodingDetectionResult objects sorted by confidence (highest first).
)")
        .def_static("detect_most_likely_encoding",
                    &iconv_cpp::EncodingDetector::detect_most_likely_encoding,
                    py::arg("data"),
                    R"(Detect the most likely encoding for the given data.

Args:
    data: Input data to analyze.

Returns:
    Name of the most likely encoding (defaults to UTF-8 if uncertain).
)");

    // EncodingRegistry class
    py::class_<iconv_cpp::EncodingRegistry>(
        m, "EncodingRegistry",
        R"(Registry of supported character encodings.

This singleton class maintains information about available character encodings
and provides methods to query encoding support and properties.

Examples:
    >>> # Get the registry instance
    >>> registry = iconv.EncodingRegistry.instance()
    >>>
    >>> # List all encodings
    >>> encodings = registry.list_all_encodings()
    >>> for enc in encodings:
    ...     print(f"{enc.name}: {enc.description}")
    >>>
    >>> # Check if encoding is supported
    >>> if registry.is_encoding_supported("UTF-8"):
    ...     print("UTF-8 is supported")
    >>>
    >>> # Get encoding info
    >>> info = registry.get_encoding_info("UTF-8")
    >>> if info:
    ...     print(f"Min char size: {info.min_char_size}")
)")
        .def_static("instance", &iconv_cpp::EncodingRegistry::instance,
                    py::return_value_policy::reference,
                    R"(Get the singleton instance of the encoding registry.

Returns:
    Reference to the global EncodingRegistry instance.
)")
        .def("list_all_encodings",
             &iconv_cpp::EncodingRegistry::list_all_encodings,
             R"(List all known character encodings.

Returns:
    List of EncodingInfo objects for all registered encodings.
)")
        .def("is_encoding_supported",
             &iconv_cpp::EncodingRegistry::is_encoding_supported,
             py::arg("encoding"),
             R"(Check if a specific encoding is supported.

Args:
    encoding: Name of the encoding to check.

Returns:
    True if the encoding is supported by the system.
)")
        .def("get_encoding_info",
             &iconv_cpp::EncodingRegistry::get_encoding_info,
             py::arg("encoding"),
             R"(Get detailed information about a specific encoding.

Args:
    encoding: Name of the encoding.

Returns:
    EncodingInfo object if found, None otherwise.
)");

    // BufferManager class
    py::class_<iconv_cpp::BufferManager>(
        m, "BufferManager",
        R"(Utility class for managing conversion buffers.

This class provides static methods for creating and managing buffers
used in character encoding conversion operations.

Examples:
    >>> # Create a resizable buffer
    >>> buffer = iconv.BufferManager.create_resizable_buffer(8192)
    >>>
    >>> # Estimate output size
    >>> size = iconv.BufferManager.estimate_output_size(1000, "UTF-8", "UTF-16")
    >>> print(f"Estimated output size: {size} bytes")
)")
        .def_static("create_resizable_buffer",
                    &iconv_cpp::BufferManager::create_resizable_buffer,
                    py::arg("initial_size") = 4096,
                    R"(Create a resizable buffer for conversion operations.

Args:
    initial_size: Initial buffer size in bytes (default: 4096).

Returns:
    A vector of characters with the specified initial size.
)")
        .def_static("ensure_buffer_capacity",
                    &iconv_cpp::BufferManager::ensure_buffer_capacity,
                    py::arg("buffer"), py::arg("required_size"),
                    R"(Ensure a buffer has at least the required capacity.

Args:
    buffer: Buffer to resize if needed.
    required_size: Minimum required size in bytes.
)")
        .def_static("estimate_output_size",
                    &iconv_cpp::BufferManager::estimate_output_size,
                    py::arg("input_size"), py::arg("from_encoding"),
                    py::arg("to_encoding"),
                    R"(Estimate the output buffer size needed for conversion.

Args:
    input_size: Size of input data in bytes.
    from_encoding: Source encoding name.
    to_encoding: Target encoding name.

Returns:
    Estimated output size in bytes.
)");

    // Converter class (main conversion class)
    py::class_<iconv_cpp::Converter>(
        m, "Converter",
        R"(Character encoding converter using iconv.

This is the main class for converting text between different character encodings.
It provides comprehensive conversion capabilities including progress tracking,
stateful conversion, and file conversion.

Examples:
    >>> # Simple string conversion
    >>> converter = iconv.Converter("UTF-8", "ISO-8859-1")
    >>> result = converter.convert_string("Hello, world!")
    >>>
    >>> # Conversion with custom options
    >>> options = iconv.ConversionOptions()
    >>> options.error_policy = iconv.ErrorHandlingPolicy.Replace
    >>> options.replacement_char = '?'
    >>> converter = iconv.Converter("UTF-8", "ASCII", options)
    >>> result = converter.convert_string("Héllo, wörld!")
    >>>
    >>> # File conversion with progress callback
    >>> def progress(processed, total):
    ...     print(f"Progress: {processed}/{total} bytes")
    >>> converter.convert_file("input.txt", "output.txt", progress)
)")
        .def(py::init<std::string_view, std::string_view>(),
             py::arg("from_encoding"), py::arg("to_encoding"),
             R"(Create a converter between two encodings.

Args:
    from_encoding: Source encoding name.
    to_encoding: Target encoding name.

Raises:
    IconvInitError: If the conversion is not supported.
)")
        .def(py::init<std::string_view, std::string_view,
                      const iconv_cpp::ConversionOptions&>(),
             py::arg("from_encoding"), py::arg("to_encoding"),
             py::arg("options"),
             R"(Create a converter with custom options.

Args:
    from_encoding: Source encoding name.
    to_encoding: Target encoding name.
    options: Conversion options.

Raises:
    IconvInitError: If the conversion is not supported.
)")
        .def("reset", &iconv_cpp::Converter::reset,
             R"(Reset the converter's internal state.

This should be called between conversions to ensure clean state.
)")
        .def("from_encoding", &iconv_cpp::Converter::from_encoding,
             R"(Get the source encoding name.

Returns:
    Source encoding name as a string view.
)")
        .def("to_encoding", &iconv_cpp::Converter::to_encoding,
             R"(Get the target encoding name.

Returns:
    Target encoding name as a string view.
)")
        .def("convert", &iconv_cpp::Converter::convert, py::arg("input"),
             R"(Convert character data from source to target encoding.

Args:
    input: Input data as bytes or span of characters.

Returns:
    Converted data as a vector of characters.

Raises:
    IconvConversionError: If conversion fails.
)")
        .def("convert_string", &iconv_cpp::Converter::convert_string,
             py::arg("input"),
             R"(Convert a string from source to target encoding.

Args:
    input: Input string to convert.

Returns:
    Converted string.

Raises:
    IconvConversionError: If conversion fails.
)")
        .def("convert_with_progress",
             &iconv_cpp::Converter::convert_with_progress, py::arg("input"),
             py::arg("progress_callback"),
             R"(Convert data with progress reporting.

Args:
    input: Input data to convert.
    progress_callback: Callback function(processed_bytes, total_bytes) called during conversion.

Returns:
    Converted data as a vector of characters.

Raises:
    IconvConversionError: If conversion fails.
)")
        .def("convert_with_state", &iconv_cpp::Converter::convert_with_state,
             py::arg("input"), py::arg("state"),
             R"(Convert data with state tracking.

This method is useful for incremental conversion of large data streams.

Args:
    input: Input data to convert.
    state: ConversionState object to track progress.

Returns:
    Converted data as a vector of characters.

Raises:
    IconvConversionError: If conversion fails.
)")
        .def("convert_file", &iconv_cpp::Converter::convert_file,
             py::arg("input_path"), py::arg("output_path"),
             py::arg("progress_callback") = nullptr,
             R"(Convert a file from source to target encoding.

Args:
    input_path: Path to input file.
    output_path: Path to output file.
    progress_callback: Optional callback function(processed_bytes, total_bytes).

Returns:
    True if conversion succeeded.

Raises:
    IconvError: If file cannot be opened.
    IconvConversionError: If conversion fails.
)")
        .def("convert_file_async", &iconv_cpp::Converter::convert_file_async,
             py::arg("input_path"), py::arg("output_path"),
             py::arg("progress_callback") = nullptr,
             R"(Convert a file asynchronously.

Args:
    input_path: Path to input file.
    output_path: Path to output file.
    progress_callback: Optional callback function(processed_bytes, total_bytes).

Returns:
    Future object that will contain True if conversion succeeded.
)");

    // StreamConverter class
    py::class_<iconv_cpp::StreamConverter>(
        m, "StreamConverter",
        R"(Stream-based character encoding converter.

This class provides conversion capabilities for input/output streams,
useful for processing large files or streaming data.

Examples:
    >>> # Convert stream to string
    >>> converter = iconv.StreamConverter("UTF-8", "ISO-8859-1")
    >>> with open("input.txt", "rb") as f:
    ...     result = converter.convert_to_string(f)
)")
        .def(py::init<std::string_view, std::string_view>(),
             py::arg("from_encoding"), py::arg("to_encoding"),
             R"(Create a stream converter between two encodings.

Args:
    from_encoding: Source encoding name.
    to_encoding: Target encoding name.
)")
        .def(py::init<std::string_view, std::string_view,
                      const iconv_cpp::ConversionOptions&>(),
             py::arg("from_encoding"), py::arg("to_encoding"),
             py::arg("options"),
             R"(Create a stream converter with custom options.

Args:
    from_encoding: Source encoding name.
    to_encoding: Target encoding name.
    options: Conversion options.
)")
        .def("convert", &iconv_cpp::StreamConverter::convert, py::arg("input"),
             py::arg("output"), py::arg("progress_callback") = nullptr,
             R"(Convert from input stream to output stream.

Args:
    input: Input stream to read from.
    output: Output stream to write to.
    progress_callback: Optional callback function(processed_bytes, total_bytes).
)")
        .def("convert_to_string",
             &iconv_cpp::StreamConverter::convert_to_string, py::arg("input"),
             py::arg("progress_callback") = nullptr,
             R"(Convert input stream to a string.

Args:
    input: Input stream to read from.
    progress_callback: Optional callback function(processed_bytes, total_bytes).

Returns:
    Converted string.
)")
        .def("convert_from_string",
             &iconv_cpp::StreamConverter::convert_from_string, py::arg("input"),
             py::arg("output"), py::arg("progress_callback") = nullptr,
             R"(Convert a string to output stream.

Args:
    input: Input string to convert.
    output: Output stream to write to.
    progress_callback: Optional callback function(processed_bytes, total_bytes).
)");

    // UTF8ToUTF16Converter class
    py::class_<iconv_cpp::UTF8ToUTF16Converter, iconv_cpp::Converter>(
        m, "UTF8ToUTF16Converter",
        R"(Specialized converter from UTF-8 to UTF-16.

This class provides optimized conversion from UTF-8 to UTF-16LE encoding.

Examples:
    >>> converter = iconv.UTF8ToUTF16Converter()
    >>> utf16_str = converter.convert_u16string("Hello, 世界")
)")
        .def(py::init<>(),
             R"(Create a UTF-8 to UTF-16 converter with default options.)")
        .def(py::init<const iconv_cpp::ConversionOptions&>(),
             py::arg("options"),
             R"(Create a UTF-8 to UTF-16 converter with custom options.

Args:
    options: Conversion options.
)")
        .def("convert_u16string",
             &iconv_cpp::UTF8ToUTF16Converter::convert_u16string,
             py::arg("utf8_str"),
             R"(Convert UTF-8 string to UTF-16 string.

Args:
    utf8_str: Input UTF-8 string.

Returns:
    UTF-16 encoded string (as u16string).
)");

    // UTF16ToUTF8Converter class
    py::class_<iconv_cpp::UTF16ToUTF8Converter, iconv_cpp::Converter>(
        m, "UTF16ToUTF8Converter",
        R"(Specialized converter from UTF-16 to UTF-8.

This class provides optimized conversion from UTF-16LE to UTF-8 encoding.

Examples:
    >>> converter = iconv.UTF16ToUTF8Converter()
    >>> utf8_str = converter.convert_u16string(utf16_data)
)")
        .def(py::init<>(),
             R"(Create a UTF-16 to UTF-8 converter with default options.)")
        .def(py::init<const iconv_cpp::ConversionOptions&>(),
             py::arg("options"),
             R"(Create a UTF-16 to UTF-8 converter with custom options.

Args:
    options: Conversion options.
)")
        .def("convert_u16string",
             &iconv_cpp::UTF16ToUTF8Converter::convert_u16string,
             py::arg("utf16_str"),
             R"(Convert UTF-16 string to UTF-8 string.

Args:
    utf16_str: Input UTF-16 string (as u16string_view).

Returns:
    UTF-8 encoded string.
)");

    // UTF8ToUTF32Converter class
    py::class_<iconv_cpp::UTF8ToUTF32Converter, iconv_cpp::Converter>(
        m, "UTF8ToUTF32Converter",
        R"(Specialized converter from UTF-8 to UTF-32.

This class provides optimized conversion from UTF-8 to UTF-32LE encoding.

Examples:
    >>> converter = iconv.UTF8ToUTF32Converter()
    >>> utf32_str = converter.convert_u32string("Hello, 世界")
)")
        .def(py::init<>(),
             R"(Create a UTF-8 to UTF-32 converter with default options.)")
        .def(py::init<const iconv_cpp::ConversionOptions&>(),
             py::arg("options"),
             R"(Create a UTF-8 to UTF-32 converter with custom options.

Args:
    options: Conversion options.
)")
        .def("convert_u32string",
             &iconv_cpp::UTF8ToUTF32Converter::convert_u32string,
             py::arg("utf8_str"),
             R"(Convert UTF-8 string to UTF-32 string.

Args:
    utf8_str: Input UTF-8 string.

Returns:
    UTF-32 encoded string (as u32string).
)");

    // UTF32ToUTF8Converter class
    py::class_<iconv_cpp::UTF32ToUTF8Converter, iconv_cpp::Converter>(
        m, "UTF32ToUTF8Converter",
        R"(Specialized converter from UTF-32 to UTF-8.

This class provides optimized conversion from UTF-32LE to UTF-8 encoding.

Examples:
    >>> converter = iconv.UTF32ToUTF8Converter()
    >>> utf8_str = converter.convert_u32string(utf32_data)
)")
        .def(py::init<>(),
             R"(Create a UTF-32 to UTF-8 converter with default options.)")
        .def(py::init<const iconv_cpp::ConversionOptions&>(),
             py::arg("options"),
             R"(Create a UTF-32 to UTF-8 converter with custom options.

Args:
    options: Conversion options.
)")
        .def("convert_u32string",
             &iconv_cpp::UTF32ToUTF8Converter::convert_u32string,
             py::arg("utf32_str"),
             R"(Convert UTF-32 string to UTF-8 string.

Args:
    utf32_str: Input UTF-32 string (as u32string_view).

Returns:
    UTF-8 encoded string.
)");

    // ChineseEncodingConverter class
    py::class_<iconv_cpp::ChineseEncodingConverter>(
        m, "ChineseEncodingConverter",
        R"(Specialized converter for Chinese encodings.

This class provides convenient methods for converting between UTF-8 and
various Chinese character encodings (GB18030, GBK, BIG5).

Examples:
    >>> converter = iconv.ChineseEncodingConverter()
    >>> gb18030_str = converter.utf8_to_gb18030_string("你好世界")
    >>> utf8_str = converter.gb18030_to_utf8_string(gb18030_str)
)")
        .def(py::init<>(), R"(Create a Chinese encoding converter.)")
        .def("utf8_to_gb18030_string",
             &iconv_cpp::ChineseEncodingConverter::utf8_to_gb18030_string,
             py::arg("utf8_str"),
             R"(Convert UTF-8 string to GB18030 encoding.

Args:
    utf8_str: Input UTF-8 string.

Returns:
    GB18030 encoded string.
)")
        .def("gb18030_to_utf8_string",
             &iconv_cpp::ChineseEncodingConverter::gb18030_to_utf8_string,
             py::arg("gb18030_str"),
             R"(Convert GB18030 string to UTF-8 encoding.

Args:
    gb18030_str: Input GB18030 string.

Returns:
    UTF-8 encoded string.
)")
        .def("utf8_to_gbk_string",
             &iconv_cpp::ChineseEncodingConverter::utf8_to_gbk_string,
             py::arg("utf8_str"),
             R"(Convert UTF-8 string to GBK encoding.

Args:
    utf8_str: Input UTF-8 string.

Returns:
    GBK encoded string.
)")
        .def("gbk_to_utf8_string",
             &iconv_cpp::ChineseEncodingConverter::gbk_to_utf8_string,
             py::arg("gbk_str"),
             R"(Convert GBK string to UTF-8 encoding.

Args:
    gbk_str: Input GBK string.

Returns:
    UTF-8 encoded string.
)")
        .def("utf8_to_big5_string",
             &iconv_cpp::ChineseEncodingConverter::utf8_to_big5_string,
             py::arg("utf8_str"),
             R"(Convert UTF-8 string to BIG5 encoding.

Args:
    utf8_str: Input UTF-8 string.

Returns:
    BIG5 encoded string.
)")
        .def("big5_to_utf8_string",
             &iconv_cpp::ChineseEncodingConverter::big5_to_utf8_string,
             py::arg("big5_str"),
             R"(Convert BIG5 string to UTF-8 encoding.

Args:
    big5_str: Input BIG5 string.

Returns:
    UTF-8 encoded string.
)");

    // JapaneseEncodingConverter class
    py::class_<iconv_cpp::JapaneseEncodingConverter>(
        m, "JapaneseEncodingConverter",
        R"(Specialized converter for Japanese encodings.

This class provides convenient methods for converting between UTF-8 and
various Japanese character encodings (Shift-JIS, EUC-JP).

Examples:
    >>> converter = iconv.JapaneseEncodingConverter()
    >>> sjis_str = converter.utf8_to_shift_jis_string("こんにちは")
    >>> utf8_str = converter.shift_jis_to_utf8_string(sjis_str)
)")
        .def(py::init<>(), R"(Create a Japanese encoding converter.)")
        .def("utf8_to_shift_jis_string",
             &iconv_cpp::JapaneseEncodingConverter::utf8_to_shift_jis_string,
             py::arg("utf8_str"),
             R"(Convert UTF-8 string to Shift-JIS encoding.

Args:
    utf8_str: Input UTF-8 string.

Returns:
    Shift-JIS encoded string.
)")
        .def("shift_jis_to_utf8_string",
             &iconv_cpp::JapaneseEncodingConverter::shift_jis_to_utf8_string,
             py::arg("sjis_str"),
             R"(Convert Shift-JIS string to UTF-8 encoding.

Args:
    sjis_str: Input Shift-JIS string.

Returns:
    UTF-8 encoded string.
)")
        .def("utf8_to_euc_jp_string",
             &iconv_cpp::JapaneseEncodingConverter::utf8_to_euc_jp_string,
             py::arg("utf8_str"),
             R"(Convert UTF-8 string to EUC-JP encoding.

Args:
    utf8_str: Input UTF-8 string.

Returns:
    EUC-JP encoded string.
)")
        .def("euc_jp_to_utf8_string",
             &iconv_cpp::JapaneseEncodingConverter::euc_jp_to_utf8_string,
             py::arg("euc_jp_str"),
             R"(Convert EUC-JP string to UTF-8 encoding.

Args:
    euc_jp_str: Input EUC-JP string.

Returns:
    UTF-8 encoded string.
)");

    // KoreanEncodingConverter class
    py::class_<iconv_cpp::KoreanEncodingConverter>(
        m, "KoreanEncodingConverter",
        R"(Specialized converter for Korean encodings.

This class provides convenient methods for converting between UTF-8 and
Korean character encoding (EUC-KR).

Examples:
    >>> converter = iconv.KoreanEncodingConverter()
    >>> euc_kr_str = converter.utf8_to_euc_kr_string("안녕하세요")
    >>> utf8_str = converter.euc_kr_to_utf8_string(euc_kr_str)
)")
        .def(py::init<>(), R"(Create a Korean encoding converter.)")
        .def("utf8_to_euc_kr_string",
             &iconv_cpp::KoreanEncodingConverter::utf8_to_euc_kr_string,
             py::arg("utf8_str"),
             R"(Convert UTF-8 string to EUC-KR encoding.

Args:
    utf8_str: Input UTF-8 string.

Returns:
    EUC-KR encoded string.
)")
        .def("euc_kr_to_utf8_string",
             &iconv_cpp::KoreanEncodingConverter::euc_kr_to_utf8_string,
             py::arg("euc_kr_str"),
             R"(Convert EUC-KR string to UTF-8 encoding.

Args:
    euc_kr_str: Input EUC-KR string.

Returns:
    UTF-8 encoded string.
)");

    // BatchConverter class
    py::class_<iconv_cpp::BatchConverter>(
        m, "BatchConverter",
        R"(Batch converter for processing multiple strings or files.

This class provides efficient batch conversion of multiple strings or files,
with support for parallel processing.

Examples:
    >>> # Batch string conversion
    >>> converter = iconv.BatchConverter("UTF-8", "ISO-8859-1")
    >>> strings = ["Hello", "World", "Test"]
    >>> results = converter.convert_strings(strings)
    >>>
    >>> # Batch file conversion with parallel processing
    >>> input_files = ["file1.txt", "file2.txt", "file3.txt"]
    >>> output_files = ["out1.txt", "out2.txt", "out3.txt"]
    >>> results = converter.convert_files_parallel(input_files, output_files, num_threads=4)
)")
        .def(py::init<std::string_view, std::string_view>(),
             py::arg("from_encoding"), py::arg("to_encoding"),
             R"(Create a batch converter between two encodings.

Args:
    from_encoding: Source encoding name.
    to_encoding: Target encoding name.
)")
        .def(py::init<std::string_view, std::string_view,
                      const iconv_cpp::ConversionOptions&>(),
             py::arg("from_encoding"), py::arg("to_encoding"),
             py::arg("options"),
             R"(Create a batch converter with custom options.

Args:
    from_encoding: Source encoding name.
    to_encoding: Target encoding name.
    options: Conversion options.
)")
        .def("convert_strings", &iconv_cpp::BatchConverter::convert_strings,
             py::arg("inputs"),
             R"(Convert multiple strings.

Args:
    inputs: List of input strings to convert.

Returns:
    List of converted strings.
)")
        .def("convert_files", &iconv_cpp::BatchConverter::convert_files,
             py::arg("input_paths"), py::arg("output_paths"),
             R"(Convert multiple files sequentially.

Args:
    input_paths: List of input file paths.
    output_paths: List of output file paths (must match input_paths length).

Returns:
    List of boolean values indicating success for each file.

Raises:
    IconvError: If input and output path counts don't match.
)")
        .def("convert_files_parallel",
             &iconv_cpp::BatchConverter::convert_files_parallel,
             py::arg("input_paths"), py::arg("output_paths"),
             py::arg("num_threads") = 0,
             R"(Convert multiple files in parallel.

Args:
    input_paths: List of input file paths.
    output_paths: List of output file paths (must match input_paths length).
    num_threads: Number of threads to use (0 = auto-detect, default: 0).

Returns:
    List of boolean values indicating success for each file.

Raises:
    IconvError: If input and output path counts don't match.
)");

    // Free functions
    m.def("convert", &iconv_cpp::convert, py::arg("from_encoding"),
          py::arg("to_encoding"), py::arg("input"),
          py::arg("options") = iconv_cpp::ConversionOptions(),
          R"(Convert character data between encodings (free function).

This is a convenience function for one-off conversions without creating a Converter object.

Args:
    from_encoding: Source encoding name.
    to_encoding: Target encoding name.
    input: Input data to convert.
    options: Optional conversion options.

Returns:
    Converted data as a vector of characters.

Raises:
    IconvInitError: If the conversion is not supported.
    IconvConversionError: If conversion fails.

Examples:
    >>> data = b"Hello, world!"
    >>> result = iconv.convert("UTF-8", "ISO-8859-1", data)
)");

    m.def("convert_string", &iconv_cpp::convert_string,
          py::arg("from_encoding"), py::arg("to_encoding"), py::arg("input"),
          py::arg("options") = iconv_cpp::ConversionOptions(),
          R"(Convert a string between encodings (free function).

This is a convenience function for one-off string conversions.

Args:
    from_encoding: Source encoding name.
    to_encoding: Target encoding name.
    input: Input string to convert.
    options: Optional conversion options.

Returns:
    Converted string.

Raises:
    IconvInitError: If the conversion is not supported.
    IconvConversionError: If conversion fails.

Examples:
    >>> result = iconv.convert_string("UTF-8", "ISO-8859-1", "Hello, world!")
)");

    m.def("convert_file", &iconv_cpp::convert_file, py::arg("from_encoding"),
          py::arg("to_encoding"), py::arg("input_path"), py::arg("output_path"),
          py::arg("options") = iconv_cpp::ConversionOptions(),
          py::arg("progress_callback") = nullptr,
          R"(Convert a file between encodings (free function).

This is a convenience function for one-off file conversions.

Args:
    from_encoding: Source encoding name.
    to_encoding: Target encoding name.
    input_path: Path to input file.
    output_path: Path to output file.
    options: Optional conversion options.
    progress_callback: Optional callback function(processed_bytes, total_bytes).

Returns:
    True if conversion succeeded.

Raises:
    IconvInitError: If the conversion is not supported.
    IconvError: If file cannot be opened.
    IconvConversionError: If conversion fails.

Examples:
    >>> iconv.convert_file("UTF-8", "ISO-8859-1", "input.txt", "output.txt")
)");

    m.def("convert_file_async", &iconv_cpp::convert_file_async,
          py::arg("from_encoding"), py::arg("to_encoding"),
          py::arg("input_path"), py::arg("output_path"),
          py::arg("options") = iconv_cpp::ConversionOptions(),
          py::arg("progress_callback") = nullptr,
          R"(Convert a file asynchronously (free function).

This is a convenience function for asynchronous file conversions.

Args:
    from_encoding: Source encoding name.
    to_encoding: Target encoding name.
    input_path: Path to input file.
    output_path: Path to output file.
    options: Optional conversion options.
    progress_callback: Optional callback function(processed_bytes, total_bytes).

Returns:
    Future object that will contain True if conversion succeeded.

Examples:
    >>> future = iconv.convert_file_async("UTF-8", "ISO-8859-1", "input.txt", "output.txt")
    >>> result = future.get()  # Wait for completion
)");

    m.def("detect_file_encoding", &iconv_cpp::detect_file_encoding,
          py::arg("file_path"), py::arg("max_check_size") = 16384,
          R"(Detect the encoding of a file (free function).

This function reads the beginning of a file and attempts to detect its encoding.

Args:
    file_path: Path to the file to analyze.
    max_check_size: Maximum number of bytes to read for detection (default: 16384).

Returns:
    Name of the detected encoding (defaults to UTF-8 if uncertain).

Raises:
    IconvError: If file cannot be opened.

Examples:
    >>> encoding = iconv.detect_file_encoding("myfile.txt")
    >>> print(f"Detected encoding: {encoding}")
)");

    // Encoding constants namespace
    auto encodings =
        m.def_submodule("encodings", "Common encoding name constants");
    encodings.attr("UTF8") = iconv_cpp::encodings::UTF8;
    encodings.attr("UTF16") = iconv_cpp::encodings::UTF16;
    encodings.attr("UTF16LE") = iconv_cpp::encodings::UTF16LE;
    encodings.attr("UTF16BE") = iconv_cpp::encodings::UTF16BE;
    encodings.attr("UTF32") = iconv_cpp::encodings::UTF32;
    encodings.attr("UTF32LE") = iconv_cpp::encodings::UTF32LE;
    encodings.attr("UTF32BE") = iconv_cpp::encodings::UTF32BE;
    encodings.attr("ASCII") = iconv_cpp::encodings::ASCII;
    encodings.attr("ISO8859_1") = iconv_cpp::encodings::ISO8859_1;
    encodings.attr("GB18030") = iconv_cpp::encodings::GB18030;
    encodings.attr("GBK") = iconv_cpp::encodings::GBK;
    encodings.attr("BIG5") = iconv_cpp::encodings::BIG5;
    encodings.attr("SHIFT_JIS") = iconv_cpp::encodings::SHIFT_JIS;
    encodings.attr("EUC_JP") = iconv_cpp::encodings::EUC_JP;
    encodings.attr("EUC_KR") = iconv_cpp::encodings::EUC_KR;
}
