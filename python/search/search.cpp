#include "atom/search/search.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(search, m) {
    m.doc() = "Search engine module for the atom package";

    // Register exception translations
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::search::DocumentNotFoundException& e) {
            PyErr_SetString(PyExc_KeyError, e.what());
        } catch (const atom::search::DocumentValidationException& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const atom::search::SearchOperationException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const atom::search::SearchEngineException& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Document class binding
    py::class_<atom::search::Document>(
        m, "Document",
        R"(Represents a document with an ID, content, tags, and click count.

This class stores a document's metadata and provides methods to access and modify it.

Args:
    id: Unique identifier for the document
    content: Document text content
    tags: List of tags associated with the document

Examples:
    >>> from atom.search import Document
    >>> doc = Document("doc1", "This is a test document", ["test", "example"])
    >>> doc.get_id()
    'doc1'
)")
        .def(py::init<std::string, std::string,
                      std::initializer_list<std::string>>(),
             py::arg("id"), py::arg("content"), py::arg("tags"),
             "Constructs a Document object with ID, content, and tags.")
        .def("get_id", &atom::search::Document::getId,
             "Returns the document's unique ID")
        .def("get_content", &atom::search::Document::getContent,
             "Returns the document's content")
        .def("get_tags", &atom::search::Document::getTags,
             "Returns the set of tags associated with the document")
        .def("get_click_count", &atom::search::Document::getClickCount,
             "Returns the number of clicks on this document")
        .def("set_content", &atom::search::Document::setContent,
             py::arg("content"), "Updates the document's content")
        .def("add_tag", &atom::search::Document::addTag, py::arg("tag"),
             "Adds a tag to the document")
        .def("remove_tag", &atom::search::Document::removeTag, py::arg("tag"),
             "Removes a tag from the document")
        .def("increment_click_count",
             &atom::search::Document::incrementClickCount,
             "Increments the document's click count");

    // SearchEngine class binding
    py::class_<atom::search::SearchEngine>(
        m, "SearchEngine",
        R"(A search engine for indexing and searching documents.

This class provides functionality to add, update, and search documents by various criteria.

Args:
    max_threads: Maximum number of threads to use (0 = use hardware concurrency)

Examples:
    >>> from atom.search import SearchEngine, Document
    >>> engine = SearchEngine()
    >>> doc = Document("doc1", "This is a test document", ["test", "example"])
    >>> engine.add_document(doc)
    >>> results = engine.search_by_tag("test")
)")
        .def(py::init<unsigned>(), py::arg("max_threads") = 0,
             "Constructs a SearchEngine with optional parallelism settings.")
        .def("add_document",
             py::overload_cast<const atom::search::Document&>(
                 &atom::search::SearchEngine::addDocument),
             py::arg("doc"),
             R"(Adds a document to the search engine.

Args:
    doc: The document to add

Raises:
    ValueError: If the document ID already exists or document is invalid
)")
        .def("remove_document", &atom::search::SearchEngine::removeDocument,
             py::arg("doc_id"),
             R"(Removes a document from the search engine.

Args:
    doc_id: The ID of the document to remove

Raises:
    KeyError: If the document does not exist
)")
        .def("update_document", &atom::search::SearchEngine::updateDocument,
             py::arg("doc"),
             R"(Updates an existing document in the search engine.

Args:
    doc: The updated document

Raises:
    KeyError: If the document does not exist
    ValueError: If the document is invalid
)")
        .def("search_by_tag", &atom::search::SearchEngine::searchByTag,
             py::arg("tag"),
             R"(Searches for documents by a specific tag.

Args:
    tag: The tag to search for

Returns:
    List of documents that match the tag
)")
        .def(
            "fuzzy_search_by_tag",
            &atom::search::SearchEngine::fuzzySearchByTag, py::arg("tag"),
            py::arg("tolerance"),
            R"(Performs a fuzzy search for documents by tag with specified tolerance.

Args:
    tag: The tag to search for
    tolerance: The tolerance for the fuzzy search (edit distance)

Returns:
    List of documents that match the tag within the tolerance

Raises:
    ValueError: If tolerance is negative
)")
        .def("search_by_tags", &atom::search::SearchEngine::searchByTags,
             py::arg("tags"),
             R"(Searches for documents that match all specified tags.

Args:
    tags: List of tags to search for

Returns:
    List of documents that match all the tags
)")
        .def("search_by_content", &atom::search::SearchEngine::searchByContent,
             py::arg("query"),
             R"(Searches for documents by content.

Args:
    query: The content query to search for

Returns:
    List of documents that match the content query, ranked by relevance
)")
        .def("boolean_search", &atom::search::SearchEngine::booleanSearch,
             py::arg("query"),
             R"(Performs a boolean search for documents.

Supports operators AND, OR, NOT, and parentheses.

Args:
    query: The boolean query to search for

Returns:
    List of documents that match the boolean query
)")
        .def("auto_complete", &atom::search::SearchEngine::autoComplete,
             py::arg("prefix"), py::arg("max_results") = 0,
             R"(Provides autocomplete suggestions for a given prefix.

Args:
    prefix: The prefix to autocomplete
    max_results: Maximum number of results to return (0 = no limit)

Returns:
    List of autocomplete suggestions
)")
        .def("save_index", &atom::search::SearchEngine::saveIndex,
             py::arg("filename"),
             R"(Saves the current index to a file.

Args:
    filename: The file to save the index to

Raises:
    IOError: If the file cannot be written
)")
        .def("load_index", &atom::search::SearchEngine::loadIndex,
             py::arg("filename"),
             R"(Loads the index from a file.

Args:
    filename: The file to load the index from

Raises:
    IOError: If the file cannot be read
)");
}
