#include "atom/algorithm/algorithm.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Helper functions for BloomFilter calculations
std::size_t optimal_hash_functions(std::size_t expected_elements,
                                   std::size_t filter_size) {
    // Formula: k = (m/n) * ln(2), where m is filter size and n is expected
    // elements
    double k = (static_cast<double>(filter_size) / expected_elements) *
               0.693147;  // ln(2)
    return std::max(std::size_t(1), static_cast<std::size_t>(std::round(k)));
}

std::size_t optimal_filter_size(std::size_t expected_elements,
                                double false_positive_rate) {
    // Formula: m = -n*ln(p) / (ln(2)^2), where p is false positive rate
    double ln_p = std::log(false_positive_rate);
    double ln_2_squared = 0.480453;  // ln(2)^2
    double size = -expected_elements * ln_p / ln_2_squared;
    return static_cast<std::size_t>(std::ceil(size));
}

// Global vector to store registered bloom filter sizes
std::vector<std::size_t> g_bloom_filter_sizes;

// Template for declaring bloom filter with different sizes
template <std::size_t N>
py::class_<atom::algorithm::BloomFilter<N, std::string>> declare_bloom_filter(
    py::module& m, const std::string& name) {
    using StringBloomFilter = atom::algorithm::BloomFilter<N, std::string>;

    std::string class_name = "BloomFilter" + name;
    std::string class_doc =
        "Bloom filter data structure with " + name +
        " bits.\n\n"
        "A Bloom filter is a space-efficient probabilistic data structure that "
        "tests whether an element is a member of a set.";

    // Register the size for the create_bloom_filter function
    g_bloom_filter_sizes.push_back(N);

    // Define the Python class
    return py::class_<StringBloomFilter>(m, class_name.c_str(),
                                         class_doc.c_str())
        .def(py::init<std::size_t>(), py::arg("num_hash_functions"),
             "Constructs a new BloomFilter object with the specified number of "
             "hash functions.")
        .def("insert", &StringBloomFilter::insert, py::arg("element"),
             "Inserts an element into the Bloom filter.")
        .def("contains", &StringBloomFilter::contains, py::arg("element"),
             "Checks if an element might be present in the Bloom filter.")
        .def("clear", &StringBloomFilter::clear,
             "Clears the Bloom filter, removing all elements.")
        .def("false_positive_probability",
             &StringBloomFilter::falsePositiveProbability,
             "Estimates the current false positive probability.")
        .def("element_count", &StringBloomFilter::elementCount,
             "Returns the number of elements added to the filter.")
        // Python-specific methods
        .def("__contains__", &StringBloomFilter::contains,
             "Support for the 'in' operator.")
        .def("__len__", &StringBloomFilter::elementCount,
             "Support for len() function.")
        .def(
            "__bool__",
            [](const StringBloomFilter& bf) { return bf.elementCount() > 0; },
            "Support for boolean evaluation.")
        .def("add", &StringBloomFilter::insert, py::arg("element"),
             "Alias for insert() to make the API more Python-like.");
}

// Helper function to create bloom filter of specific size
template <std::size_t N>
py::object create_bloom_filter_impl(std::size_t num_hash_functions,
                                    py::module& m) {
    std::string class_name = "BloomFilter";

    if (N <= 1024)
        class_name += "1K";
    else if (N <= 4096)
        class_name += "4K";
    else if (N <= 16384)
        class_name += "16K";
    else if (N <= 65536)
        class_name += "64K";
    else if (N <= 262144)
        class_name += "256K";
    else
        class_name += "1M";

    return m.attr(class_name.c_str())(num_hash_functions);
}

PYBIND11_MODULE(algorithm, m) {
    m.doc() = "Algorithm implementation module for the atom package";

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

    // KMP class binding
    py::class_<atom::algorithm::KMP>(
        m, "KMP",
        R"(Implements the Knuth-Morris-Pratt (KMP) string searching algorithm.

This class provides methods to search for occurrences of a pattern within a text
using the KMP algorithm, which preprocesses the pattern to achieve efficient
string searching.

Args:
    pattern: The string pattern to search for.

Examples:
    >>> from atom.algorithm import KMP
    >>> kmp = KMP("abc")
    >>> kmp.search("abcabcabc")
    [0, 3, 6]
)")
        .def(py::init<std::string_view>(), py::arg("pattern"),
             "Constructs a KMP object with the given pattern.")
        .def("search", &atom::algorithm::KMP::search, py::arg("text"),
             R"(Searches for occurrences of the pattern in the given text.

Args:
    text: The text to search within.

Returns:
    List of positions where the pattern starts in the text.
)")
        .def("set_pattern", &atom::algorithm::KMP::setPattern,
             py::arg("pattern"),
             R"(Sets a new pattern for searching.

Args:
    pattern: The new pattern to search for.
)")
        .def(
            "search_parallel", &atom::algorithm::KMP::searchParallel,
            py::arg("text"), py::arg("chunk_size") = 1024,
            R"(Asynchronously searches for pattern occurrences in chunks of text.

Args:
    text: The text to search within.
    chunk_size: Size of each text chunk to process separately. Default is 1024.

Returns:
    List of positions where the pattern starts in the text.
)");

    // BoyerMoore class binding
    py::class_<atom::algorithm::BoyerMoore>(
        m, "BoyerMoore",
        R"(Implements the Boyer-Moore string searching algorithm.

This class provides methods to search for occurrences of a pattern within a text
using the Boyer-Moore algorithm, which preprocesses the pattern to achieve efficient
string searching.

Args:
    pattern: The string pattern to search for.

Examples:
    >>> from atom.algorithm import BoyerMoore
    >>> bm = BoyerMoore("abc")
    >>> bm.search("abcabcabc")
    [0, 3, 6]
)")
        .def(py::init<std::string_view>(), py::arg("pattern"),
             "Constructs a BoyerMoore object with the given pattern.")
        .def("search", &atom::algorithm::BoyerMoore::search, py::arg("text"),
             R"(Searches for occurrences of the pattern in the given text.

Args:
    text: The text to search within.

Returns:
    List of positions where the pattern starts in the text.
)")
        .def("set_pattern", &atom::algorithm::BoyerMoore::setPattern,
             py::arg("pattern"),
             R"(Sets a new pattern for searching.

Args:
    pattern: The new pattern to search for.
)")
        .def(
            "search_optimized", &atom::algorithm::BoyerMoore::searchOptimized,
            py::arg("text"),
            R"(Performs a Boyer-Moore search using SIMD instructions if available.

Args:
    text: The text to search within.

Returns:
    List of positions where the pattern starts in the text.
)");

    // Register BloomFilters of different sizes
    declare_bloom_filter<1024>(m, "1K");
    declare_bloom_filter<4096>(m, "4K");
    declare_bloom_filter<16384>(m, "16K");
    declare_bloom_filter<65536>(m, "64K");
    declare_bloom_filter<262144>(m, "256K");
    declare_bloom_filter<1048576>(m, "1M");

    // Sort sizes for efficient lookup
    std::sort(g_bloom_filter_sizes.begin(), g_bloom_filter_sizes.end());

    // Factory function for bloom filters
    m.def(
        "create_bloom_filter",
        [&m](std::size_t size, std::size_t num_hash_functions) {
            // Find the closest size that is at least as large as requested
            auto it = std::lower_bound(g_bloom_filter_sizes.begin(),
                                       g_bloom_filter_sizes.end(), size);

            // If no suitable size found, use the largest available
            if (it == g_bloom_filter_sizes.end() &&
                !g_bloom_filter_sizes.empty()) {
                it = std::prev(g_bloom_filter_sizes.end());
            }

            std::size_t best_size =
                (it != g_bloom_filter_sizes.end()) ? *it : 1024;

            // Create the appropriate bloom filter based on size
            if (best_size <= 1024) {
                return create_bloom_filter_impl<1024>(num_hash_functions, m);
            } else if (best_size <= 4096) {
                return create_bloom_filter_impl<4096>(num_hash_functions, m);
            } else if (best_size <= 16384) {
                return create_bloom_filter_impl<16384>(num_hash_functions, m);
            } else if (best_size <= 65536) {
                return create_bloom_filter_impl<65536>(num_hash_functions, m);
            } else if (best_size <= 262144) {
                return create_bloom_filter_impl<262144>(num_hash_functions, m);
            } else {
                return create_bloom_filter_impl<1048576>(num_hash_functions, m);
            }
        },
        py::arg("size"), py::arg("num_hash_functions"),
        R"(Factory function to create a bloom filter with appropriate size.

Args:
    size: Desired bit size (will be rounded up to nearest predefined size)
    num_hash_functions: Number of hash functions to use

Returns:
    A BloomFilter object with the selected size

Examples:
    >>> from atom.algorithm import create_bloom_filter
    >>> bf = create_bloom_filter(10000, 5)
    >>> bf.add("test")
    >>> "test" in bf
    True
)",
        py::return_value_policy::reference);

    // Helper functions for bloom filter optimization
    m.def("optimal_hash_functions", &optimal_hash_functions,
          py::arg("expected_elements"), py::arg("filter_size"),
          R"(Calculate the optimal number of hash functions for a bloom filter.

Args:
    expected_elements: Expected number of elements to be inserted
    filter_size: Size of the bloom filter in bits

Returns:
    Optimal number of hash functions to minimize false positives
)");

    m.def("optimal_filter_size", &optimal_filter_size,
          py::arg("expected_elements"), py::arg("false_positive_rate") = 0.01,
          R"(Calculate the optimal bloom filter size in bits.

Args:
    expected_elements: Expected number of elements to be inserted
    false_positive_rate: Desired false positive rate (default: 0.01 or 1%)

Returns:
    Optimal size of the bloom filter in bits
)");

    // One-step creation of optimal bloom filter
    m.def(
        "create_optimal_filter",
        [&m](std::size_t expected_elements, double false_positive_rate) {
            std::size_t size =
                optimal_filter_size(expected_elements, false_positive_rate);
            std::size_t num_hash =
                optimal_hash_functions(expected_elements, size);

            // Use the module's create_bloom_filter function
            return py::cast<py::object>(
                m.attr("create_bloom_filter")(size, num_hash));
        },
        py::arg("expected_elements"), py::arg("false_positive_rate") = 0.01,
        R"(Create a bloom filter with optimal parameters for the given requirements.

Args:
    expected_elements: Expected number of elements to be inserted
    false_positive_rate: Desired false positive rate (default: 0.01 or 1%)

Returns:
    A BloomFilter object with optimal size and hash function count

Examples:
    >>> from atom.algorithm import create_optimal_filter
    >>> bf = create_optimal_filter(1000, 0.01)
    >>> # Add 1000 elements for ~1% false positive rate
)");
}
