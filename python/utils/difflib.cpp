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
        } catch (const atom::utils::DiffException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const atom::utils::InvalidInputException& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const atom::utils::AlgorithmFailureException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Register custom exceptions
    py::register_exception<atom::utils::DiffException>(m, "DiffException");
    py::register_exception<atom::utils::InvalidInputException>(m, "InvalidInputException");
    py::register_exception<atom::utils::AlgorithmFailureException>(m, "AlgorithmFailureException");

    // DiffAlgorithm enumeration
    py::enum_<atom::utils::DiffAlgorithm>(m, "DiffAlgorithm",
        R"(Enumeration of available diff algorithms.

        Different algorithms have different performance characteristics
        and may be better suited for different types of data.
        )")
        .value("DEFAULT", atom::utils::DiffAlgorithm::Default, "Original algorithm")
        .value("MYERS", atom::utils::DiffAlgorithm::Myers, "Myers diff algorithm")
        .value("PATIENCE", atom::utils::DiffAlgorithm::Patience, "Patience diff algorithm")
        .value("HISTOGRAM", atom::utils::DiffAlgorithm::Histogram, "Histogram diff algorithm")
#ifdef ATOM_HAS_BOOST_GRAPH
        .value("GRAPH", atom::utils::DiffAlgorithm::Graph, "Boost graph-based algorithm")
#endif
        .export_values();

    // DiffStats structure
    py::class_<atom::utils::DiffStats>(m, "DiffStats",
        R"(Statistics about a diff operation.

        This structure contains information about the performance
        and results of a diff operation.
        )")
        .def(py::init<>(), "Create default DiffStats")
        .def_readwrite("insertions", &atom::utils::DiffStats::insertions, "Number of insertions")
        .def_readwrite("deletions", &atom::utils::DiffStats::deletions, "Number of deletions")
        .def_readwrite("modifications", &atom::utils::DiffStats::modifications, "Number of modifications")
        .def_readwrite("similarity", &atom::utils::DiffStats::similarity, "Overall similarity ratio")
        .def_property_readonly("duration_microseconds",
                              [](const atom::utils::DiffStats& stats) {
                                  return stats.duration.count();
                              }, "Duration of the diff operation in microseconds")
        .def("to_string", &atom::utils::DiffStats::toString,
             R"(Get a formatted string representation of the statistics.

             Returns:
                 A string describing the diff statistics.
             )")
        .def("__repr__", [](const atom::utils::DiffStats& stats) {
            return std::format("DiffStats(insertions={}, deletions={}, modifications={}, similarity={:.3f})",
                             stats.insertions, stats.deletions, stats.modifications, stats.similarity);
        });

    // DiffOptions structure
    py::class_<atom::utils::DiffOptions>(m, "DiffOptions",
        R"(Configuration options for diff operations.

        This structure allows customization of diff algorithm behavior,
        performance settings, and caching options.
        )")
        .def(py::init<>(), "Create default DiffOptions")
        .def_readwrite("enable_caching", &atom::utils::DiffOptions::enableCaching,
                      "Enable result caching")
        .def_readwrite("use_parallel_processing", &atom::utils::DiffOptions::useParallelProcessing,
                      "Use parallel algorithms when possible")
        .def_readwrite("lazy_loading", &atom::utils::DiffOptions::lazyLoading,
                      "Enable lazy loading for large diffs")
        .def_readwrite("cache_size_limit", &atom::utils::DiffOptions::cacheSizeLimit,
                      "Maximum number of cached results")
        .def_readwrite("large_file_threshold", &atom::utils::DiffOptions::largeFileThreshold,
                      "Threshold for large file optimization (bytes)")
        .def_readwrite("algorithm", &atom::utils::DiffOptions::algorithm,
                      "Diff algorithm to use")
        .def_readwrite("use_flat_containers", &atom::utils::DiffOptions::useFlatContainers,
                      "Use flat containers to improve cache efficiency")
        .def_readwrite("use_small_buffers", &atom::utils::DiffOptions::useSmallBuffers,
                      "Use stack allocation for small data")
        .def_readwrite("use_intrusive", &atom::utils::DiffOptions::useIntrusive,
                      "Use intrusive containers to reduce memory allocations")
        .def("__repr__", [](const atom::utils::DiffOptions& opts) {
            return std::format("DiffOptions(algorithm={}, caching={}, parallel={})",
                             static_cast<int>(opts.algorithm), opts.enableCaching, opts.useParallelProcessing);
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
    options: Optional performance and algorithm options.

Examples:
    >>> from atom.utils import difflib
    >>> s = difflib.SequenceMatcher("hello world", "hello there")
    >>> s.ratio()
    0.65
)")
        .def(py::init<std::string_view, std::string_view>(), py::arg("str1"),
             py::arg("str2"),
             "Constructs a SequenceMatcher with two sequences.")
        .def(py::init<std::string_view, std::string_view, const atom::utils::DiffOptions&>(),
             py::arg("str1"), py::arg("str2"), py::arg("options"),
             "Constructs a SequenceMatcher with two sequences and options.")
        .def("set_seqs", &atom::utils::SequenceMatcher::setSeqs,
             py::arg("str1"), py::arg("str2"),
             R"(Set the sequences to be compared.

Args:
    str1: The first sequence.
    str2: The second sequence.

Raises:
    InvalidInputException: If the sequences are invalid.
)")
        .def("set_options", &atom::utils::SequenceMatcher::setOptions,
             py::arg("options"),
             R"(Set the algorithm and performance options.

             Args:
                 options: The options to use for diff operations.
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
        .def("get_opcodes", &atom::utils::SequenceMatcher::getOpcodes,
            R"(Get a list of opcodes describing how to turn the first sequence into the second.

Returns:
    A list of tuples, each containing (tag, i1, i2, j1, j2) where:
    - tag: A string describing the operation ('equal', 'replace', 'delete', 'insert')
    - i1, i2: Start and end indices in the first sequence
    - j1, j2: Start and end indices in the second sequence
)")
        .def("get_stats", &atom::utils::SequenceMatcher::getStats,
             py::return_value_policy::reference_internal,
             R"(Get performance statistics for the last diff operation.

             Returns:
                 Structure containing statistics about the last diff operation.
             )")
        .def("clear_cache", &atom::utils::SequenceMatcher::clearCache,
             R"(Clear the internal cache to free memory.)");

    // Bind Differ class first, then its methods
    py::class_<atom::utils::Differ>(m, "Differ",
        R"(A class for comparing sequences and generating differences.

        This class provides methods to compare sequences of strings
        and generate various diff formats including unified diffs.

        Examples:
            >>> from atom.utils import difflib
            >>> differ = difflib.Differ()
            >>> a = ["hello", "world"]
            >>> b = ["hello", "there"]
            >>> result = differ.compare(a, b)
        )")
        .def(py::init<>(), "Create a new Differ instance with default options")
        .def(py::init<const atom::utils::DiffOptions&>(), py::arg("options"),
             "Create a new Differ instance with specified options")
        .def("compare",
             [](atom::utils::Differ& self, const std::vector<std::string>& vec1,
                const std::vector<std::string>& vec2) {
                 return self.compare(std::span<const std::string>(vec1),
                                   std::span<const std::string>(vec2));
             },
             py::arg("vec1"), py::arg("vec2"),
             R"(Compare two sequences and return the differences.

             Args:
                 vec1: The first sequence of strings.
                 vec2: The second sequence of strings.

             Returns:
                 A list of strings representing the differences.

             Raises:
                 InvalidInputException: If the input sequences are invalid.
             )")
        .def("unified_diff",
             [](atom::utils::Differ& self, const std::vector<std::string>& vec1,
                const std::vector<std::string>& vec2, std::string_view label1,
                std::string_view label2, int context) {
                 return self.unifiedDiff(std::span<const std::string>(vec1),
                                       std::span<const std::string>(vec2),
                                       label1, label2, context);
             },
             py::arg("vec1"), py::arg("vec2"), py::arg("label1") = "a",
             py::arg("label2") = "b", py::arg("context") = 3,
             R"(Generate a unified diff between two sequences.

             Args:
                 vec1: The first sequence of strings.
                 vec2: The second sequence of strings.
                 label1: The label for the first sequence.
                 label2: The label for the second sequence.
                 context: The number of context lines to include.

             Returns:
                 A list of strings representing the unified diff.

             Raises:
                 InvalidInputException: If the input parameters are invalid.
             )")
        .def("set_options", &atom::utils::Differ::setOptions,
             py::arg("options"),
             R"(Set the algorithm and performance options.

             Args:
                 options: The options to use for diff operations.
             )")
        .def("get_stats", &atom::utils::Differ::getStats,
             py::return_value_policy::reference_internal,
             R"(Get performance statistics for the last diff operation.

             Returns:
                 Structure containing statistics about the last diff operation.
             )");

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
    m.def("get_close_matches",
          [](std::string_view word, const std::vector<std::string>& possibilities,
             int n, double cutoff, const atom::utils::DiffOptions& options) {
              return atom::utils::getCloseMatches(word,
                                                std::span<const std::string>(possibilities),
                                                n, cutoff, options);
          },
          py::arg("word"), py::arg("possibilities"), py::arg("n") = 3,
          py::arg("cutoff") = 0.6, py::arg("options") = atom::utils::DiffOptions{},
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
    InvalidInputException: If n <= 0 or cutoff is outside valid range.

Examples:
    >>> from atom.utils import difflib
    >>> difflib.get_close_matches("appel", ["ape", "apple", "peach", "puppy"])
    ['apple', 'ape']
)");

    // FuzzyMatcher class
    py::class_<atom::utils::FuzzyMatcher>(m, "FuzzyMatcher",
        R"(Provides fuzzy matching and similarity calculation functionality.

        This class offers advanced string matching capabilities including
        Levenshtein distance calculation and best match finding.

        Examples:
            >>> from atom.utils import difflib
            >>> matcher = difflib.FuzzyMatcher()
            >>> distance = matcher.levenshtein_distance("kitten", "sitting")
            >>> similarity = matcher.similarity("hello", "hallo")
        )")
        .def(py::init<>(), "Create a FuzzyMatcher with default options")
        .def(py::init<const atom::utils::DiffOptions&>(), py::arg("options"),
             "Create a FuzzyMatcher with specified options")
        .def_static("levenshtein_distance", &atom::utils::FuzzyMatcher::levenshteinDistance,
                   py::arg("s1"), py::arg("s2"),
                   R"(Calculate the Levenshtein edit distance between two strings.

                   Args:
                       s1: First string.
                       s2: Second string.

                   Returns:
                       The edit distance as an integer.

                   Examples:
                       >>> difflib.FuzzyMatcher.levenshtein_distance("kitten", "sitting")
                       3
                   )")
        .def_static("similarity", &atom::utils::FuzzyMatcher::similarity,
                   py::arg("s1"), py::arg("s2"),
                   R"(Calculate the similarity ratio between two strings (0.0-1.0).

                   Args:
                       s1: First string.
                       s2: Second string.

                   Returns:
                       The similarity ratio as a float between 0.0 and 1.0.

                   Examples:
                       >>> difflib.FuzzyMatcher.similarity("hello", "hallo")
                       0.8
                   )")
        .def("find_best_match", &atom::utils::FuzzyMatcher::findBestMatch,
             py::arg("needle"), py::arg("haystack"), py::arg("cutoff") = 0.7,
             R"(Find the best match in the text.

             Args:
                 needle: Text to search for.
                 haystack: Text to search in.
                 cutoff: Minimum similarity threshold (0.0-1.0).

             Returns:
                 A tuple of (matched substring, similarity ratio).

             Examples:
                 >>> matcher.find_best_match("hello", "say hello world")
                 ('hello', 1.0)
             )")
        .def("find_all_matches",
             [](atom::utils::FuzzyMatcher& self, std::string_view needle,
                const std::vector<std::string>& haystacks, double cutoff) {
                 return self.findAllMatches(needle,
                                          std::span<const std::string>(haystacks),
                                          cutoff);
             },
             py::arg("needle"), py::arg("haystacks"), py::arg("cutoff") = 0.7,
             R"(Find all matches in a collection of texts.

             Args:
                 needle: Text to search for.
                 haystacks: List of texts to search in.
                 cutoff: Minimum similarity threshold (0.0-1.0).

             Returns:
                 A list of tuples (matched text, similarity ratio).

             Examples:
                 >>> matcher.find_all_matches("hello", ["hello world", "hi there", "hello"])
                 [('hello world', 0.9), ('hello', 1.0)]
             )");
    // InlineDiff class
    py::class_<atom::utils::InlineDiff>(m, "InlineDiff",
        R"(A utility class to detect changes between lines of text with character-level precision.

        This class provides character-level diff capabilities for fine-grained
        text comparison and HTML generation.

        Examples:
            >>> from atom.utils import difflib
            >>> inline_diff = difflib.InlineDiff()
            >>> result = inline_diff.compare_chars("hello world", "hello there")
        )")
        .def(py::init<>(), "Create an InlineDiff with default options")
        .def(py::init<const atom::utils::DiffOptions&>(), py::arg("options"),
             "Create an InlineDiff with specified options")
        .def("compare_chars", &atom::utils::InlineDiff::compareChars,
             py::arg("str1"), py::arg("str2"),
             R"(Compare two strings at character level.

             Args:
                 str1: First string.
                 str2: Second string.

             Returns:
                 A list of tuples representing operations (equal, insert, delete) with the content.

             Examples:
                 >>> inline_diff.compare_chars("hello", "hallo")
                 [('equal', 'h'), ('delete', 'e'), ('insert', 'a'), ('equal', 'llo')]
             )")
        .def("to_html", &atom::utils::InlineDiff::toHtml,
             py::arg("str1"), py::arg("str2"),
             py::arg("options") = atom::utils::HtmlDiff::HtmlDiffOptions{},
             R"(Generate HTML representation of inline diff.

             Args:
                 str1: First string.
                 str2: Second string.
                 options: HTML styling options.

             Returns:
                 A tuple of HTML-formatted strings showing character-level differences.

             Examples:
                 >>> html1, html2 = inline_diff.to_html("hello", "hallo")
             )")
        .def("set_options", &atom::utils::InlineDiff::setOptions,
             py::arg("options"),
             R"(Set the algorithm and performance options.

             Args:
                 options: The options to use for diff operations.
             )");

    // HtmlDiff options structure
    py::class_<atom::utils::HtmlDiff::HtmlDiffOptions>(m, "HtmlDiffOptions",
        R"(HTML diff styling options.

        This structure contains configuration options for HTML diff generation
        including CSS classes and display preferences.
        )")
        .def(py::init<>(), "Create default HtmlDiffOptions")
        .def_readwrite("added_class", &atom::utils::HtmlDiff::HtmlDiffOptions::addedClass,
                      "CSS class for added content")
        .def_readwrite("removed_class", &atom::utils::HtmlDiff::HtmlDiffOptions::removedClass,
                      "CSS class for removed content")
        .def_readwrite("changed_class", &atom::utils::HtmlDiff::HtmlDiffOptions::changedClass,
                      "CSS class for changed content")
        .def_readwrite("inline_diff", &atom::utils::HtmlDiff::HtmlDiffOptions::inlineDiff,
                      "Show character-level inline diffs")
        .def_readwrite("show_statistics", &atom::utils::HtmlDiff::HtmlDiffOptions::showStatistics,
                      "Show diff statistics")
        .def_readwrite("show_line_numbers", &atom::utils::HtmlDiff::HtmlDiffOptions::showLineNumbers,
                      "Show line numbers")
        .def_readwrite("show_side_by_side", &atom::utils::HtmlDiff::HtmlDiffOptions::showSideBySide,
                      "Show side-by-side diff view")
        .def_readwrite("collapsable_unchanged", &atom::utils::HtmlDiff::HtmlDiffOptions::collapsableUnchanged,
                      "Make unchanged sections collapsible")
        .def_readwrite("context_lines", &atom::utils::HtmlDiff::HtmlDiffOptions::contextLines,
                      "Number of context lines to display")
        .def("__repr__", [](const atom::utils::HtmlDiff::HtmlDiffOptions& opts) {
            return std::format("HtmlDiffOptions(inline_diff={}, show_statistics={}, context_lines={})",
                             opts.inlineDiff, opts.showStatistics, opts.contextLines);
        });

    // Enhanced HtmlDiff class
    py::class_<atom::utils::HtmlDiff>(m, "HtmlDiff",
        R"(A class for generating HTML representations of differences between sequences.

        This class provides methods to generate HTML files and tables showing
        differences between text sequences with customizable styling.

        Examples:
            >>> from atom.utils import difflib
            >>> html_diff = difflib.HtmlDiff()
            >>> result = html_diff.make_file(["hello", "world"], ["hello", "there"])
        )")
        .def(py::init<>(), "Create an HtmlDiff with default options")
        .def(py::init<const atom::utils::DiffOptions&>(), py::arg("options"),
             "Create an HtmlDiff with specified options")
        .def("make_file",
             [](atom::utils::HtmlDiff& self, const std::vector<std::string>& fromlines,
                const std::vector<std::string>& tolines, std::string_view fromdesc,
                std::string_view todesc, const atom::utils::HtmlDiff::HtmlDiffOptions& htmlOptions) -> std::string {
                 auto result = self.makeFile(std::span<const std::string>(fromlines),
                                           std::span<const std::string>(tolines),
                                           fromdesc, todesc, htmlOptions);
                 if (result.has_value()) {
                     return result.value();
                 } else {
                     throw std::runtime_error(result.error());
                 }
             },
             py::arg("fromlines"), py::arg("tolines"), py::arg("fromdesc") = "",
             py::arg("todesc") = "", py::arg("html_options") = atom::utils::HtmlDiff::HtmlDiffOptions{},
             R"(Generate an HTML file showing the differences between two sequences.

             Args:
                 fromlines: The first sequence of strings.
                 tolines: The second sequence of strings.
                 fromdesc: Description for the first sequence.
                 todesc: Description for the second sequence.
                 html_options: HTML styling options.

             Returns:
                 A string containing the HTML representation of the differences.

             Raises:
                 RuntimeError: If HTML generation fails.
             )")
        .def("make_table",
             [](atom::utils::HtmlDiff& self, const std::vector<std::string>& fromlines,
                const std::vector<std::string>& tolines, std::string_view fromdesc,
                std::string_view todesc, const atom::utils::HtmlDiff::HtmlDiffOptions& htmlOptions) -> std::string {
                 auto result = self.makeTable(std::span<const std::string>(fromlines),
                                            std::span<const std::string>(tolines),
                                            fromdesc, todesc, htmlOptions);
                 if (result.has_value()) {
                     return result.value();
                 } else {
                     throw std::runtime_error(result.error());
                 }
             },
             py::arg("fromlines"), py::arg("tolines"), py::arg("fromdesc") = "",
             py::arg("todesc") = "", py::arg("html_options") = atom::utils::HtmlDiff::HtmlDiffOptions{},
             R"(Generate an HTML table showing the differences between two sequences.

             Args:
                 fromlines: The first sequence of strings.
                 tolines: The second sequence of strings.
                 fromdesc: Description for the first sequence.
                 todesc: Description for the second sequence.
                 html_options: HTML styling options.

             Returns:
                 A string containing the HTML table representation of the differences.

             Raises:
                 RuntimeError: If HTML generation fails.
             )")
        .def("set_options", &atom::utils::HtmlDiff::setOptions,
             py::arg("options"),
             R"(Set the algorithm and performance options.

             Args:
                 options: The options to use for diff operations.
             )")
        .def("get_stats", &atom::utils::HtmlDiff::getStats,
             py::return_value_policy::reference_internal,
             R"(Get performance statistics for the last diff operation.

             Returns:
                 Structure containing statistics about the last diff operation.
             )");

    // DiffLibConfig class for global configuration
    py::class_<atom::utils::DiffLibConfig>(m, "DiffLibConfig",
        R"(Configuration class for difflib logging and telemetry.

        This class provides static methods to configure global settings
        for the difflib module.
        )")
        .def_static("set_telemetry_enabled", &atom::utils::DiffLibConfig::setTelemetryEnabled,
                   py::arg("enabled"),
                   R"(Enable or disable telemetry data collection.

                   Args:
                       enabled: Whether telemetry is enabled.
                   )")
        .def_static("set_default_options", &atom::utils::DiffLibConfig::setDefaultOptions,
                   py::arg("options"),
                   R"(Set global default options.

                   Args:
                       options: The default options to use.
                   )")
        .def_static("get_default_options", &atom::utils::DiffLibConfig::getDefaultOptions,
                   py::return_value_policy::reference,
                   R"(Get global default options.

                   Returns:
                       The current default options.
                   )")
        .def_static("clear_caches", &atom::utils::DiffLibConfig::clearCaches,
                   R"(Clear all internal caches.)")
        .def_static("supports_high_performance_containers",
                   &atom::utils::DiffLibConfig::supportsHighPerformanceContainers,
                   R"(Check if high performance containers are supported.

                   Returns:
                       Whether high performance containers are supported.
                   )");

    // Convenience functions for quick operations
    m.def("quick_ratio",
          [](std::string_view s1, std::string_view s2) -> double {
              atom::utils::SequenceMatcher matcher(s1, s2);
              return matcher.ratio();
          },
          py::arg("s1"), py::arg("s2"),
          R"(Quick similarity ratio calculation between two strings.

          Args:
              s1: First string.
              s2: Second string.

          Returns:
              The similarity ratio as a float between 0.0 and 1.0.

          Examples:
              >>> difflib.quick_ratio("hello", "hallo")
              0.8
          )");

    m.def("levenshtein_distance", &atom::utils::FuzzyMatcher::levenshteinDistance,
          py::arg("s1"), py::arg("s2"),
          R"(Calculate Levenshtein distance between two strings.

          Args:
              s1: First string.
              s2: Second string.

          Returns:
              The edit distance as an integer.

          Examples:
              >>> difflib.levenshtein_distance("kitten", "sitting")
              3
          )");
}
