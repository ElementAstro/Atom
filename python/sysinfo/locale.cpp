#include "atom/sysinfo/locale.hpp"

#include <pybind11/chrono.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace atom::system;

PYBIND11_MODULE(locale, m) {
    m.doc() = "System locale information and management module for the atom package";

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

    // LocaleError enum binding
    py::enum_<LocaleError>(m, "LocaleError", "Enumeration of locale operation error codes")
        .value("NONE", LocaleError::None, "No error occurred")
        .value("INVALID_LOCALE", LocaleError::InvalidLocale, "The specified locale is invalid or not recognized")
        .value("SYSTEM_ERROR", LocaleError::SystemError, "A system-level error occurred during the operation")
        .value("UNSUPPORTED_PLATFORM", LocaleError::UnsupportedPlatform, "The operation is not supported on the current platform")
        .export_values();

    // LocaleInfo structure binding
    py::class_<LocaleInfo>(m, "LocaleInfo",
                          R"(Comprehensive information about a system locale.

This class contains detailed information about locale settings including language,
country, formatting preferences, and display characteristics.

Examples:
    >>> from atom.sysinfo import locale
    >>> # Get current system locale information
    >>> locale_info = locale.get_system_language_info()
    >>> print(f"Language: {locale_info.language_display_name}")
    >>> print(f"Country: {locale_info.country_display_name}")
    >>> print(f"Locale: {locale_info.locale_name}")
    >>> print(f"Currency: {locale_info.currency_symbol}")
    >>> print(f"Date format: {locale_info.date_format}")
)")
        .def(py::init<>(), "Constructs a new LocaleInfo object.")
        .def_readwrite("language_code", &LocaleInfo::languageCode,
                       "ISO 639 language code (e.g., 'en')")
        .def_readwrite("country_code", &LocaleInfo::countryCode,
                       "ISO 3166 country code (e.g., 'US')")
        .def_readwrite("locale_name", &LocaleInfo::localeName,
                       "Full locale name (e.g., 'en_US')")
        .def_readwrite("language_display_name", &LocaleInfo::languageDisplayName,
                       "Human-readable language name")
        .def_readwrite("country_display_name", &LocaleInfo::countryDisplayName,
                       "Human-readable country name")
        .def_readwrite("currency_symbol", &LocaleInfo::currencySymbol,
                       "Currency symbol (e.g., '$')")
        .def_readwrite("decimal_symbol", &LocaleInfo::decimalSymbol,
                       "Decimal point symbol (e.g., '.')")
        .def_readwrite("thousand_separator", &LocaleInfo::thousandSeparator,
                       "Thousands separator symbol (e.g., ',')")
        .def_readwrite("date_format", &LocaleInfo::dateFormat,
                       "Date format string")
        .def_readwrite("time_format", &LocaleInfo::timeFormat,
                       "Time format string")
        .def_readwrite("character_encoding", &LocaleInfo::characterEncoding,
                       "Character encoding (e.g., 'UTF-8')")
        .def_readwrite("is_rtl", &LocaleInfo::isRTL,
                       "Whether text is displayed right-to-left")
        .def_readwrite("number_format", &LocaleInfo::numberFormat,
                       "Number format pattern")
        .def_readwrite("measurement_system", &LocaleInfo::measurementSystem,
                       "Measurement system (e.g., 'metric', 'imperial')")
        .def_readwrite("paper_size", &LocaleInfo::paperSize,
                       "Default paper size (e.g., 'A4', 'Letter')")
        .def_readwrite("cache_timeout", &LocaleInfo::cacheTimeout,
                       "Cache timeout duration in seconds")
        .def("__eq__", &LocaleInfo::operator==, py::arg("other"),
             "Equality comparison operator")
        .def("__repr__", [](const LocaleInfo& info) {
            return "<LocaleInfo locale='" + info.localeName + "'" +
                   " language='" + info.languageDisplayName + "'" +
                   " country='" + info.countryDisplayName + "'>";
        });

    // Locale information functions
    m.def("get_system_language_info", &getSystemLanguageInfo,
          R"(Retrieve the current system language and locale information.

Returns:
    LocaleInfo object containing the system's current locale settings with all available details.

Examples:
    >>> from atom.sysinfo import locale
    >>> # Get current system locale
    >>> info = locale.get_system_language_info()
    >>> print(f"System language: {info.language_display_name}")
    >>> print(f"Country: {info.country_display_name}")
    >>> print(f"Locale identifier: {info.locale_name}")
    >>> print(f"Currency symbol: {info.currency_symbol}")
    >>> print(f"Number format: {info.number_format}")
    >>> print(f"Date format: {info.date_format}")
    >>> print(f"Time format: {info.time_format}")
    >>> print(f"Character encoding: {info.character_encoding}")
    >>> print(f"Measurement system: {info.measurement_system}")
    >>> print(f"Paper size: {info.paper_size}")
    >>> print(f"Right-to-left text: {info.is_rtl}")
)");

    m.def("print_locale_info", &printLocaleInfo, py::arg("info"),
          R"(Display locale information in a formatted manner.

Args:
    info: The LocaleInfo instance to display

Examples:
    >>> from atom.sysinfo import locale
    >>> # Get and print locale information
    >>> info = locale.get_system_language_info()
    >>> locale.print_locale_info(info)
)");

    m.def("validate_locale", &validateLocale, py::arg("locale"),
          R"(Validate if a locale identifier is valid and available on the system.

Args:
    locale: The locale identifier to validate (e.g., "en_US")

Returns:
    Boolean indicating whether the locale is valid and available

Examples:
    >>> from atom.sysinfo import locale
    >>> # Check if specific locales are valid
    >>> if locale.validate_locale("en_US"):
    ...     print("en_US locale is available")
    >>> 
    >>> if locale.validate_locale("fr_FR"):
    ...     print("fr_FR locale is available")
    >>> else:
    ...     print("fr_FR locale is not available")
)");

    m.def("set_system_locale", &setSystemLocale, py::arg("locale"),
          R"(Attempt to set the system-wide locale.

Args:
    locale: The locale identifier to set as the system locale

Returns:
    LocaleError indicating success (NONE) or the reason for failure

Note:
    May require administrative privileges on some platforms.

Examples:
    >>> from atom.sysinfo import locale
    >>> # Try to set system locale
    >>> result = locale.set_system_locale("en_US")
    >>> if result == locale.LocaleError.NONE:
    ...     print("Locale set successfully")
    >>> elif result == locale.LocaleError.INVALID_LOCALE:
    ...     print("Invalid locale specified")
    >>> elif result == locale.LocaleError.SYSTEM_ERROR:
    ...     print("System error occurred")
    >>> elif result == locale.LocaleError.UNSUPPORTED_PLATFORM:
    ...     print("Operation not supported on this platform")
)");

    m.def("get_available_locales", &getAvailableLocales,
          R"(Retrieve a list of all available locales on the system.

Returns:
    List of locale identifier strings that are installed and available for use

Examples:
    >>> from atom.sysinfo import locale
    >>> # Get all available locales
    >>> locales = locale.get_available_locales()
    >>> print(f"Found {len(locales)} available locales:")
    >>> for loc in locales[:10]:  # Show first 10
    ...     print(f"  {loc}")
)");

    m.def("get_default_locale", &getDefaultLocale,
          R"(Get the system's default locale.

Returns:
    String containing the default locale identifier

Examples:
    >>> from atom.sysinfo import locale
    >>> # Get default locale
    >>> default = locale.get_default_locale()
    >>> print(f"Default system locale: {default}")
)");

    m.def("get_cached_locale_info", &getCachedLocaleInfo,
          py::return_value_policy::reference_internal,
          R"(Retrieve cached locale information.

Returns:
    Reference to the cached LocaleInfo instance. The information is refreshed
    if the cache timeout has expired.

Examples:
    >>> from atom.sysinfo import locale
    >>> # Get cached locale info (faster than get_system_language_info)
    >>> cached_info = locale.get_cached_locale_info()
    >>> print(f"Cached locale: {cached_info.locale_name}")
)");

    m.def("clear_locale_cache", &clearLocaleCache,
          R"(Clear the locale information cache.

Forces the cache to be invalidated, ensuring that the next call to
get_cached_locale_info will retrieve fresh data from the system.

Examples:
    >>> from atom.sysinfo import locale
    >>> # Clear cache to force refresh
    >>> locale.clear_locale_cache()
    >>> # Next call will get fresh data
    >>> fresh_info = locale.get_cached_locale_info()
)");
}
