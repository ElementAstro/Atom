#include "atom/error/error_formatter.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(error_formatter, m) {
    m.doc() =
        "Comprehensive error formatting system with customizable output "
        "formats";

    // OutputFormat enum
    py::enum_<atom::error::OutputFormat>(
        m, "OutputFormat",
        R"(Output format types for error formatting.

This enum defines different output formats for error messages.

Examples:
    >>> from atom.error import OutputFormat
    >>> format = OutputFormat.Json
)")
        .value("Plain", atom::error::OutputFormat::Plain, "Plain text format")
        .value("Json", atom::error::OutputFormat::Json, "JSON format")
        .value("Xml", atom::error::OutputFormat::Xml, "XML format")
        .value("Html", atom::error::OutputFormat::Html, "HTML format")
        .value("Markdown", atom::error::OutputFormat::Markdown,
               "Markdown format")
        .value("Colored", atom::error::OutputFormat::Colored,
               "Colored terminal output")
        .value("Structured", atom::error::OutputFormat::Structured,
               "Structured logging format")
        .export_values();

    // Color enum
    py::enum_<atom::error::Color>(m, "Color",
                                  R"(Color codes for terminal output.

ANSI color codes for colored terminal output.
)")
        .value("Reset", atom::error::Color::Reset, "Reset color")
        .value("Black", atom::error::Color::Black, "Black")
        .value("Red", atom::error::Color::Red, "Red")
        .value("Green", atom::error::Color::Green, "Green")
        .value("Yellow", atom::error::Color::Yellow, "Yellow")
        .value("Blue", atom::error::Color::Blue, "Blue")
        .value("Magenta", atom::error::Color::Magenta, "Magenta")
        .value("Cyan", atom::error::Color::Cyan, "Cyan")
        .value("White", atom::error::Color::White, "White")
        .value("BrightBlack", atom::error::Color::BrightBlack, "Bright black")
        .value("BrightRed", atom::error::Color::BrightRed, "Bright red")
        .value("BrightGreen", atom::error::Color::BrightGreen, "Bright green")
        .value("BrightYellow", atom::error::Color::BrightYellow,
               "Bright yellow")
        .value("BrightBlue", atom::error::Color::BrightBlue, "Bright blue")
        .value("BrightMagenta", atom::error::Color::BrightMagenta,
               "Bright magenta")
        .value("BrightCyan", atom::error::Color::BrightCyan, "Bright cyan")
        .value("BrightWhite", atom::error::Color::BrightWhite, "Bright white")
        .export_values();

    // ErrorFormatter base class
    py::class_<atom::error::ErrorFormatter,
               std::shared_ptr<atom::error::ErrorFormatter>>(
        m, "ErrorFormatter",
        R"(Error formatter interface.

Base class for all error formatters. Provides methods for formatting
error contexts into various output formats.
)")
        .def("format", &atom::error::ErrorFormatter::format, py::arg("context"),
             R"(Format error context to string.

Args:
    context (ErrorContext): The error context to format

Returns:
    str: Formatted error string
)")
        .def("format_multiple", &atom::error::ErrorFormatter::formatMultiple,
             py::arg("contexts"),
             R"(Format multiple error contexts.

Args:
    contexts (list[ErrorContext]): List of error contexts to format

Returns:
    str: Formatted string containing all errors
)")
        .def("set_option", &atom::error::ErrorFormatter::setOption,
             py::arg("key"), py::arg("value"),
             R"(Set formatting option.

Args:
    key (str): Option key
    value (str): Option value
)")
        .def("get_option", &atom::error::ErrorFormatter::getOption,
             py::arg("key"),
             R"(Get formatting option.

Args:
    key (str): Option key

Returns:
    str: Option value
)");

    // PlainTextFormatter class
    py::class_<atom::error::PlainTextFormatter, atom::error::ErrorFormatter,
               std::shared_ptr<atom::error::PlainTextFormatter>>(
        m, "PlainTextFormatter",
        R"(Plain text error formatter.

Formats error contexts as plain text with configurable options.

Examples:
    >>> from atom.error import PlainTextFormatter, ErrorContext
    >>> formatter = PlainTextFormatter()
    >>> formatter.set_option("include_stack_trace", "true")
    >>> context = ErrorContext.create(100, "Test error")
    >>> print(formatter.format(context))
)")
        .def(py::init<>(),
             "Constructs a PlainTextFormatter with default settings.")
        .def("format", &atom::error::PlainTextFormatter::format,
             py::arg("context"), "Format error context to plain text.")
        .def("set_option", &atom::error::PlainTextFormatter::setOption,
             py::arg("key"), py::arg("value"), "Set formatting option.")
        .def("get_option", &atom::error::PlainTextFormatter::getOption,
             py::arg("key"), "Get formatting option.");

    // JsonFormatter class
    py::class_<atom::error::JsonFormatter, atom::error::ErrorFormatter,
               std::shared_ptr<atom::error::JsonFormatter>>(
        m, "JsonFormatter",
        R"(JSON error formatter.

Formats error contexts as JSON with optional pretty printing.

Examples:
    >>> from atom.error import JsonFormatter, ErrorContext
    >>> formatter = JsonFormatter()
    >>> formatter.set_option("pretty_print", "true")
    >>> context = ErrorContext.create(100, "Test error")
    >>> json_str = formatter.format(context)
)")
        .def(py::init<>(), "Constructs a JsonFormatter with default settings.")
        .def("format", &atom::error::JsonFormatter::format, py::arg("context"),
             "Format error context to JSON.")
        .def("format_multiple", &atom::error::JsonFormatter::formatMultiple,
             py::arg("contexts"),
             "Format multiple error contexts to JSON array.")
        .def("set_option", &atom::error::JsonFormatter::setOption,
             py::arg("key"), py::arg("value"), "Set formatting option.")
        .def("get_option", &atom::error::JsonFormatter::getOption,
             py::arg("key"), "Get formatting option.");

    // ColoredFormatter class
    py::class_<atom::error::ColoredFormatter, atom::error::ErrorFormatter,
               std::shared_ptr<atom::error::ColoredFormatter>>(
        m, "ColoredFormatter",
        R"(Colored terminal formatter.

Formats error contexts with ANSI color codes for terminal output.

Examples:
    >>> from atom.error import ColoredFormatter, ErrorContext
    >>> formatter = ColoredFormatter()
    >>> context = ErrorContext.create(100, "Test error")
    >>> colored_output = formatter.format(context)
)")
        .def(py::init<>(),
             "Constructs a ColoredFormatter with default settings.")
        .def("format", &atom::error::ColoredFormatter::format,
             py::arg("context"), "Format error context with colors.")
        .def("set_option", &atom::error::ColoredFormatter::setOption,
             py::arg("key"), py::arg("value"), "Set formatting option.")
        .def("get_option", &atom::error::ColoredFormatter::getOption,
             py::arg("key"), "Get formatting option.");

    // HtmlFormatter class
    py::class_<atom::error::HtmlFormatter, atom::error::ErrorFormatter,
               std::shared_ptr<atom::error::HtmlFormatter>>(
        m, "HtmlFormatter",
        R"(HTML error formatter.

Formats error contexts as HTML with optional CSS styling.

Examples:
    >>> from atom.error import HtmlFormatter, ErrorContext
    >>> formatter = HtmlFormatter()
    >>> formatter.set_option("include_css", "true")
    >>> context = ErrorContext.create(100, "Test error")
    >>> html = formatter.format(context)
)")
        .def(py::init<>(), "Constructs an HtmlFormatter with default settings.")
        .def("format", &atom::error::HtmlFormatter::format, py::arg("context"),
             "Format error context to HTML.")
        .def("format_multiple", &atom::error::HtmlFormatter::formatMultiple,
             py::arg("contexts"), "Format multiple error contexts to HTML.")
        .def("set_option", &atom::error::HtmlFormatter::setOption,
             py::arg("key"), py::arg("value"), "Set formatting option.")
        .def("get_option", &atom::error::HtmlFormatter::getOption,
             py::arg("key"), "Get formatting option.");

    // StructuredFormatter class
    py::class_<atom::error::StructuredFormatter, atom::error::ErrorFormatter,
               std::shared_ptr<atom::error::StructuredFormatter>>(
        m, "StructuredFormatter",
        R"(Structured logging formatter.

Formats error contexts in structured logging format (key=value pairs).

Examples:
    >>> from atom.error import StructuredFormatter, ErrorContext
    >>> formatter = StructuredFormatter()
    >>> context = ErrorContext.create(100, "Test error")
    >>> structured = formatter.format(context)
)")
        .def(py::init<>(),
             "Constructs a StructuredFormatter with default settings.")
        .def("format", &atom::error::StructuredFormatter::format,
             py::arg("context"), "Format error context to structured format.")
        .def("set_option", &atom::error::StructuredFormatter::setOption,
             py::arg("key"), py::arg("value"), "Set formatting option.")
        .def("get_option", &atom::error::StructuredFormatter::getOption,
             py::arg("key"), "Get formatting option.");

    // TemplateFormatter class
    py::class_<atom::error::TemplateFormatter, atom::error::ErrorFormatter,
               std::shared_ptr<atom::error::TemplateFormatter>>(
        m, "TemplateFormatter",
        R"(Template formatter for custom formatting.

Allows custom formatting using template strings with variable substitution.

Examples:
    >>> from atom.error import TemplateFormatter, ErrorContext
    >>> template = "Error {error_code}: {message} at {timestamp}"
    >>> formatter = TemplateFormatter(template)
    >>> context = ErrorContext.create(100, "Test error")
    >>> output = formatter.format(context)
)")
        .def(py::init<const std::string&>(), py::arg("template_str"),
             R"(Constructs a TemplateFormatter with template string.

Args:
    template_str (str): Template string with {variable} placeholders
)")
        .def("format", &atom::error::TemplateFormatter::format,
             py::arg("context"), "Format error context using template.")
        .def("set_option", &atom::error::TemplateFormatter::setOption,
             py::arg("key"), py::arg("value"), "Set formatting option.")
        .def("get_option", &atom::error::TemplateFormatter::getOption,
             py::arg("key"), "Get formatting option.")
        .def("set_template", &atom::error::TemplateFormatter::setTemplate,
             py::arg("template_str"),
             R"(Set template string.

Args:
    template_str (str): New template string
)");

    // ErrorLocalizer class
    py::class_<atom::error::ErrorLocalizer,
               std::shared_ptr<atom::error::ErrorLocalizer>>(
        m, "ErrorLocalizer",
        R"(Localization support for error messages.

Provides translation and localization of error messages, severity levels,
and categories to different languages.

Examples:
    >>> from atom.error import ErrorLocalizer, ErrorSeverity
    >>> localizer = ErrorLocalizer()
    >>> localizer.set_locale("zh_CN")
    >>> localizer.add_translation("zh_CN", 100, "文件未找到")
    >>> message = localizer.get_localized_message(100)
)")
        .def(py::init<>(), "Constructs an ErrorLocalizer.")
        .def("set_locale", &atom::error::ErrorLocalizer::setLocale,
             py::arg("locale"),
             R"(Set current locale.

Args:
    locale (str): Locale code (e.g., "en_US", "zh_CN", "ja_JP")
)")
        .def("get_current_locale",
             &atom::error::ErrorLocalizer::getCurrentLocale,
             R"(Get current locale.

Returns:
    str: Current locale code
)")
        .def("add_translation", &atom::error::ErrorLocalizer::addTranslation,
             py::arg("locale"), py::arg("error_code"), py::arg("message"),
             R"(Add translation for error code.

Args:
    locale (str): Locale code
    error_code (int): Error code
    message (str): Translated message
)")
        .def("add_severity_translation",
             &atom::error::ErrorLocalizer::addSeverityTranslation,
             py::arg("locale"), py::arg("severity"), py::arg("translation"),
             R"(Add translation for severity level.

Args:
    locale (str): Locale code
    severity (ErrorSeverity): Severity level
    translation (str): Translated severity name
)")
        .def("add_category_translation",
             &atom::error::ErrorLocalizer::addCategoryTranslation,
             py::arg("locale"), py::arg("category"), py::arg("translation"),
             R"(Add translation for category.

Args:
    locale (str): Locale code
    category (ErrorCategory): Error category
    translation (str): Translated category name
)")
        .def("get_localized_message",
             &atom::error::ErrorLocalizer::getLocalizedMessage,
             py::arg("error_code"),
             R"(Get localized error message.

Args:
    error_code (int): Error code

Returns:
    str: Localized error message
)")
        .def("get_localized_severity",
             &atom::error::ErrorLocalizer::getLocalizedSeverity,
             py::arg("severity"),
             R"(Get localized severity string.

Args:
    severity (ErrorSeverity): Severity level

Returns:
    str: Localized severity name
)")
        .def("get_localized_category",
             &atom::error::ErrorLocalizer::getLocalizedCategory,
             py::arg("category"),
             R"(Get localized category string.

Args:
    category (ErrorCategory): Error category

Returns:
    str: Localized category name
)")
        .def("load_translations",
             &atom::error::ErrorLocalizer::loadTranslations,
             py::arg("file_path"),
             R"(Load translations from file.

Args:
    file_path (str): Path to translation file

Returns:
    bool: True if successful
)");

    // ErrorFormatterFactory class
    py::class_<atom::error::ErrorFormatterFactory>(m, "ErrorFormatterFactory",
                                                   R"(Error formatter factory.

Factory class for creating error formatters by output format type.

Examples:
    >>> from atom.error import ErrorFormatterFactory, OutputFormat
    >>> formatter = ErrorFormatterFactory.create_formatter(OutputFormat.Json)
)")
        .def_static("create_formatter",
                    &atom::error::ErrorFormatterFactory::createFormatter,
                    py::arg("format"),
                    R"(Create formatter by output format.

Args:
    format (OutputFormat): The output format type

Returns:
    ErrorFormatter: A formatter instance for the specified format

Examples:
    >>> from atom.error import ErrorFormatterFactory, OutputFormat
    >>> json_formatter = ErrorFormatterFactory.create_formatter(OutputFormat.Json)
    >>> plain_formatter = ErrorFormatterFactory.create_formatter(OutputFormat.Plain)
)")
        .def_static("register_formatter",
                    &atom::error::ErrorFormatterFactory::registerFormatter,
                    py::arg("name"), py::arg("factory"),
                    R"(Register custom formatter.

Args:
    name (str): Formatter name
    factory (callable): Factory function that returns ErrorFormatter
)")
        .def_static("create_custom_formatter",
                    &atom::error::ErrorFormatterFactory::createCustomFormatter,
                    py::arg("name"),
                    R"(Create custom formatter by name.

Args:
    name (str): Registered formatter name

Returns:
    ErrorFormatter: Custom formatter instance
)")
        .def_static("get_available_formatters",
                    &atom::error::ErrorFormatterFactory::getAvailableFormatters,
                    R"(Get available formatter names.

Returns:
    list[str]: List of available formatter names
)");

    // ErrorDisplayManager class
    py::class_<atom::error::ErrorDisplayManager,
               std::shared_ptr<atom::error::ErrorDisplayManager>>(
        m, "ErrorDisplayManager",
        R"(Error display manager for handling formatted output.

Manages multiple formatters and handles error display with configurable
output streams and localization.

Examples:
    >>> from atom.error import ErrorDisplayManager, OutputFormat, ErrorContext
    >>> manager = ErrorDisplayManager()
    >>> context = ErrorContext.create(100, "Test error")
    >>> manager.display_error(context, OutputFormat.Colored)
)")
        .def(py::init<>(), "Constructs an ErrorDisplayManager.")
        .def("set_default_formatter",
             &atom::error::ErrorDisplayManager::setDefaultFormatter,
             py::arg("formatter"),
             R"(Set default formatter.

Args:
    formatter (ErrorFormatter): The default formatter to use
)")
        .def("add_formatter", &atom::error::ErrorDisplayManager::addFormatter,
             py::arg("format"), py::arg("formatter"),
             R"(Add formatter for specific output format.

Args:
    format (OutputFormat): The output format
    formatter (ErrorFormatter): The formatter for this format
)")
        .def("display_error", &atom::error::ErrorDisplayManager::displayError,
             py::arg("context"),
             py::arg("format") = atom::error::OutputFormat::Plain,
             R"(Display error using specified format.

Args:
    context (ErrorContext): Error context to display
    format (OutputFormat, optional): Output format (default: Plain)
)")
        .def("display_errors", &atom::error::ErrorDisplayManager::displayErrors,
             py::arg("contexts"),
             py::arg("format") = atom::error::OutputFormat::Plain,
             R"(Display multiple errors.

Args:
    contexts (list[ErrorContext]): List of error contexts
    format (OutputFormat, optional): Output format (default: Plain)
)")
        .def("set_localizer", &atom::error::ErrorDisplayManager::setLocalizer,
             py::arg("localizer"),
             R"(Set error localizer.

Args:
    localizer (ErrorLocalizer): The localizer to use
)")
        .def("set_auto_display",
             &atom::error::ErrorDisplayManager::setAutoDisplay,
             py::arg("enabled"),
             R"(Enable/disable automatic display.

Args:
    enabled (bool): True to enable automatic display
)");
}
