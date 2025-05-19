#include "atom/utils/difflib.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(difflib, m) {
    m.doc() =
        "Sequence comparison and differencing utilities module for the atom "
        "package";

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

    // SequenceMatcher class binding
    py::class_<atom::utils::SequenceMatcher>(
        m, "SequenceMatcher",
        R"(A class for comparing pairs of sequences of any type.

This class provides methods to compare sequences and calculate the similarity ratio
between them. It is useful for fuzzy matching and diff generation.

Args:
    str1: The first sequence to compare.
    str2: The second sequence to compare.

Examples:
    >>> from atom.utils import difflib
    >>> s = difflib.SequenceMatcher("hello world", "hello there")
    >>> s.ratio()
    0.65
)")
        .def(py::init<std::string_view, std::string_view>(), py::arg("str1"),
             py::arg("str2"),
             "Constructs a SequenceMatcher with two sequences.")
        .def("set_seqs", &atom::utils::SequenceMatcher::setSeqs,
             py::arg("str1"), py::arg("str2"),
             R"(Set the sequences to be compared.

Args:
    str1: The first sequence.
    str2: The second sequence.

Raises:
    ValueError: If the sequences are invalid.
)")
        .def("ratio", &atom::utils::SequenceMatcher::ratio,
             R"(Calculate the similarity ratio between the sequences.

The ratio is a float in the range [0, 1], where 0 means completely different
and 1 means identical sequences.

Returns:
    The similarity ratio as a float between 0 and 1.
)")
        .def("get_matching_blocks",
             &atom::utils::SequenceMatcher::getMatchingBlocks,
             R"(Get the matching blocks between the sequences.

Returns:
    A list of tuples, each containing (a_start, b_start, length) where:
    - a_start: Starting index in first sequence
    - b_start: Starting index in second sequence
    - length: Length of the matching block
)")
        .def(
            "get_opcodes", &atom::utils::SequenceMatcher::getOpcodes,
            R"(Get a list of opcodes describing how to turn the first sequence into the second.

Returns:
    A list of tuples, each containing (tag, i1, i2, j1, j2) where:
    - tag: A string describing the operation ('equal', 'replace', 'delete', 'insert')
    - i1, i2: Start and end indices in the first sequence
    - j1, j2: Start and end indices in the second sequence
)");

    // Bind Differ class first, then its methods
    py::class_<atom::utils::Differ>(m, "Differ",
                                    "Class for comparing sequences of strings")
        .def(py::init<>(), "Create a new Differ instance");

    // Add the static or free functions properly
    m.def(
        "compare",
        [](const std::vector<std::string>& vec1,
           const std::vector<std::string>& vec2) {
            atom::utils::Differ differ;
            return differ.compare(vec1, vec2);
        },
        py::arg("vec1"), py::arg("vec2"),
        R"(Compare two sequences of strings and return the differences.

Args:
    vec1: The first sequence of strings.
    vec2: The second sequence of strings.

Returns:
    A list of strings showing line-by-line differences.

Examples:
    >>> from atom.utils import difflib
    >>> a = ["hello", "world"]
    >>> b = ["hello", "there"]
    >>> difflib.compare(a, b)
    ['  hello', '- world', '+ there']
)");

    m.def(
        "unified_diff",
        [](const std::vector<std::string>& vec1,
           const std::vector<std::string>& vec2, std::string_view label1,
           std::string_view label2, int context) {
            atom::utils::Differ differ;  // Create an instance
            return differ.unifiedDiff(vec1, vec2, label1, label2, context);
        },
        py::arg("vec1"), py::arg("vec2"), py::arg("label1") = "a",
        py::arg("label2") = "b", py::arg("context") = 3,
        R"(Generate a unified diff between two sequences.

Args:
    vec1: The first sequence of strings.
    vec2: The second sequence of strings.
    label1: The label for the first sequence (default: "a").
    label2: The label for the second sequence (default: "b").
    context: The number of context lines to include (default: 3).

Returns:
    A list of strings representing the unified diff.

Examples:
    >>> from atom.utils import difflib
    >>> a = ["hello", "world"]
    >>> b = ["hello", "there"]
    >>> difflib.unified_diff(a, b)
    ['--- a', '+++ b', '@@ -1,2 +1,2 @@', ' hello', '-world', '+there']
)");

    // Create HtmlDiff class binding first
    py::class_<atom::utils::HtmlDiff> htmlDiffClass(
        m, "HtmlDiff", "Class for generating HTML diffs");
    htmlDiffClass.def(py::init<>(), "Create a new HtmlDiff instance");

    // HtmlDiff functions that correctly create an instance
    m.def(
        "make_file",
        [](std::span<const std::string> fromlines,
           std::span<const std::string> tolines, std::string_view fromdesc,
           std::string_view todesc) -> std::string {
            atom::utils::HtmlDiff differ;  // Create an instance
            auto result = differ.makeFile(fromlines, tolines, fromdesc, todesc);
            if (result.has_value()) {
                return result.value();
            } else {
                throw std::runtime_error(result.error().error());
            }
        },
        py::arg("fromlines"), py::arg("tolines"), py::arg("fromdesc") = "",
        py::arg("todesc") = "",
        R"(Generate an HTML file showing the differences between two sequences.

Args:
    fromlines: The first sequence of strings.
    tolines: The second sequence of strings.
    fromdesc: Description for the first sequence (default: "").
    todesc: Description for the second sequence (default: "").

Returns:
    A string containing the HTML representation of the differences.

Raises:
    RuntimeError: If HTML generation fails.

Examples:
    >>> from atom.utils import difflib
    >>> a = ["hello", "world"]
    >>> b = ["hello", "there"]
    >>> html = difflib.make_file(a, b, "Original", "Modified")
)");

    m.def(
        "make_table",
        [](std::span<const std::string> fromlines,
           std::span<const std::string> tolines, std::string_view fromdesc,
           std::string_view todesc) {
            atom::utils::HtmlDiff differ;  // Create an instance
            auto result =
                differ.makeTable(fromlines, tolines, fromdesc, todesc);
            if (result.has_value()) {
                return result.value();
            } else {
                throw std::runtime_error(result.error().error());
            }
        },
        py::arg("fromlines"), py::arg("tolines"), py::arg("fromdesc") = "",
        py::arg("todesc") = "",
        R"(Generate an HTML table showing the differences between two sequences.

Args:
    fromlines: The first sequence of strings.
    tolines: The second sequence of strings.
    fromdesc: Description for the first sequence (default: "").
    todesc: Description for the second sequence (default: "").

Returns:
    A string containing the HTML table representation of the differences.

Raises:
    RuntimeError: If HTML generation fails.

Examples:
    >>> from atom.utils import difflib
    >>> a = ["hello", "world"]
    >>> b = ["hello", "there"]
    >>> html_table = difflib.make_table(a, b, "Original", "Modified")
)");

    // Get close matches function
    m.def("get_close_matches", &atom::utils::getCloseMatches, py::arg("word"),
          py::arg("possibilities"), py::arg("n") = 3, py::arg("cutoff") = 0.6,
          py::arg("options") = atom::utils::DiffOptions{},
          R"(Get a list of close matches to a word from a list of possibilities.

Args:
    word: The word to match.
    possibilities: The list of possible matches.
    n: The maximum number of close matches to return (default: 3).
    cutoff: The similarity ratio threshold for considering a match (default: 0.6).
    options: Optional performance and algorithm options (default: DiffOptions{}).

Returns:
    A list of strings containing the close matches.

Raises:
    ValueError: If n <= 0 or cutoff is outside valid range.

Examples:
    >>> from atom.utils import difflib
    >>> difflib.get_close_matches("appel", ["ape", "apple", "peach", "puppy"])
    ['apple', 'ape']
)");
}