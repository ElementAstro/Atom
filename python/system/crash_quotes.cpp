#include "atom/system/crash_quotes.hpp"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>


namespace py = pybind11;

PYBIND11_MODULE(crash_quotes, m) {
    m.doc() = "Crash quotes module for the atom package";

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

    // Quote class binding
    py::class_<atom::system::Quote>(
        m, "Quote",
        R"(Represents a quote with its text and author.

A Quote object stores the text content of a quote along with metadata
such as the author, category, and year.

Args:
    text: The text content of the quote.
    author: The name of the quote's author.
    category: Optional category for the quote.
    year: Optional year when the quote was made.

Examples:
    >>> from atom.system import Quote
    >>> quote = Quote("To be or not to be", "Shakespeare", "Literature", 1600)
    >>> print(quote.to_string())
    "To be or not to be" - Shakespeare
)")
        .def(py::init<std::string, std::string, std::string, int>(),
             py::arg("text"), py::arg("author"), py::arg("category") = "",
             py::arg("year") = 0,
             "Constructs a new Quote object with the specified text, author, "
             "and optional metadata.")
        .def("get_text", &atom::system::Quote::getText,
             "Gets the text of the quote.")
        .def("get_author", &atom::system::Quote::getAuthor,
             "Gets the author of the quote.")
        .def("get_category", &atom::system::Quote::getCategory,
             "Gets the category of the quote.")
        .def("get_year", &atom::system::Quote::getYear,
             "Gets the year of the quote.")
        .def("set_category", &atom::system::Quote::setCategory,
             py::arg("category"), "Sets the category of the quote.")
        .def("set_year", &atom::system::Quote::setYear, py::arg("year"),
             "Sets the year of the quote.")
        .def("to_string", &atom::system::Quote::toString,
             py::arg("include_metadata") = false,
             R"(Creates a formatted string representation of the quote.

Args:
    include_metadata: Whether to include category and year in the output.

Returns:
    Formatted quote string.
)")
        .def(
            "__eq__",
            [](const atom::system::Quote& self,
               const atom::system::Quote& other) { return self == other; },
            py::is_operator(), "Compares two quotes for equality.")
        .def(
            "__str__",
            [](const atom::system::Quote& self) {
                return self.toString(false);
            },
            "Returns a string representation of the quote.");

    // QuoteManager class binding
    py::class_<atom::system::QuoteManager>(m, "QuoteManager",
                                           R"(Manages a collection of quotes.

This class provides methods to store, retrieve, search, and filter quotes.
It can load quotes from and save them to JSON files.

Examples:
    >>> from atom.system import QuoteManager, Quote
    >>> manager = QuoteManager()
    >>> manager.add_quote(Quote("Hello, World!", "Programmer"))
    >>> random_quote = manager.get_random_quote()
)")
        .def(py::init<>(), "Default constructor.")
        .def(py::init<const std::string&>(), py::arg("filename"),
             "Constructs a QuoteManager and loads quotes from the specified "
             "file.")
        .def("add_quote", &atom::system::QuoteManager::addQuote,
             py::arg("quote"),
             R"(Adds a quote to the collection.

Args:
    quote: The quote to add.

Returns:
    True if added successfully, false otherwise.
)")
        .def("add_quotes", &atom::system::QuoteManager::addQuotes,
             py::arg("quotes"),
             R"(Adds multiple quotes to the collection.

Args:
    quotes: List of quotes to add.

Returns:
    Number of quotes successfully added.
)")
        .def("remove_quote", &atom::system::QuoteManager::removeQuote,
             py::arg("quote"),
             R"(Removes a quote from the collection.

Args:
    quote: The quote to remove.

Returns:
    True if removed successfully, false if not found.
)")
        .def("remove_quotes_by_author",
             &atom::system::QuoteManager::removeQuotesByAuthor,
             py::arg("author"),
             R"(Removes quotes by author.

Args:
    author: The author whose quotes should be removed.

Returns:
    Number of quotes removed.
)")
#ifdef DEBUG
        .def("display_quotes", &atom::system::QuoteManager::displayQuotes,
             "Displays all quotes in the collection.")
#endif
        .def("shuffle_quotes", &atom::system::QuoteManager::shuffleQuotes,
             "Shuffles the quotes in the collection.")
        .def("clear_quotes", &atom::system::QuoteManager::clearQuotes,
             "Clears all quotes in the collection.")
        .def("load_quotes_from_json",
             &atom::system::QuoteManager::loadQuotesFromJson,
             py::arg("filename"), py::arg("append") = false,
             R"(Loads quotes from a JSON file.

Args:
    filename: The JSON file to load quotes from.
    append: Whether to append to existing quotes or replace them.

Returns:
    True if loaded successfully, false otherwise.
)")
        .def("save_quotes_to_json",
             &atom::system::QuoteManager::saveQuotesToJson, py::arg("filename"),
             R"(Saves quotes to a JSON file.

Args:
    filename: The JSON file to save quotes to.

Returns:
    True if saved successfully, false otherwise.
)")
        .def("search_quotes", &atom::system::QuoteManager::searchQuotes,
             py::arg("keyword"), py::arg("case_sensitive") = false,
             R"(Searches for quotes containing a keyword.

Args:
    keyword: The keyword to search for.
    case_sensitive: Whether the search should be case-sensitive.

Returns:
    A list of quotes containing the keyword.
)")
        .def("filter_quotes_by_author",
             &atom::system::QuoteManager::filterQuotesByAuthor,
             py::arg("author"),
             R"(Filters quotes by author.

Args:
    author: The name of the author to filter by.

Returns:
    A list of quotes by the specified author.
)")
        .def("filter_quotes_by_category",
             &atom::system::QuoteManager::filterQuotesByCategory,
             py::arg("category"),
             R"(Filters quotes by category.

Args:
    category: The category to filter by.

Returns:
    A list of quotes in the specified category.
)")
        .def("filter_quotes_by_year",
             &atom::system::QuoteManager::filterQuotesByYear, py::arg("year"),
             R"(Filters quotes by year.

Args:
    year: The year to filter by.

Returns:
    A list of quotes from the specified year.
)")
        .def("filter_quotes", &atom::system::QuoteManager::filterQuotes,
             py::arg("filter_func"),
             R"(Filters quotes using a custom filter function.

Args:
    filter_func: The function to use for filtering.

Returns:
    A list of quotes that pass the filter.

Examples:
    >>> from atom.system import QuoteManager
    >>> manager = QuoteManager("quotes.json")
    >>> # Get quotes with text shorter than 50 characters
    >>> short_quotes = manager.filter_quotes(lambda q: len(q.get_text()) < 50)
)")
        .def("get_random_quote", &atom::system::QuoteManager::getRandomQuote,
             R"(Gets a random quote from the collection.

Returns:
    A random quote formatted as string, or empty string if no quotes.
)")
        .def(
            "get_random_quote_object",
            [](const atom::system::QuoteManager& self) {
                auto quote_opt = self.getRandomQuoteObject();
                return quote_opt ? py::cast(*quote_opt) : py::none();
            },
            R"(Gets a random quote from the collection as a Quote object.

Returns:
    A random Quote object, or None if no quotes.
)")
        .def("size", &atom::system::QuoteManager::size,
             "Gets the number of quotes in the collection.")
        .def("empty", &atom::system::QuoteManager::empty,
             "Checks if the collection is empty.")
        .def("get_all_quotes", &atom::system::QuoteManager::getAllQuotes,
             "Gets all quotes in the collection.")
        .def("__len__", &atom::system::QuoteManager::size,
             "Support for len() function.")
        .def(
            "__bool__",
            [](const atom::system::QuoteManager& self) {
                return !self.empty();
            },
            "Support for boolean evaluation.");
}