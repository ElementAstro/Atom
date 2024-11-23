#include "atom/search/search.hpp"

#include <iostream>

using namespace atom::search;

int main() {
    // Create a SearchEngine instance
    SearchEngine searchEngine;

    // Create some documents
    Document doc1("1", "This is the first document.", {"tag1", "tag2"});
    Document doc2("2", "This is the second document.", {"tag2", "tag3"});
    Document doc3("3", "This is the third document.", {"tag1", "tag3"});

    // Add documents to the search engine
    searchEngine.addDocument(doc1);
    searchEngine.addDocument(doc2);
    searchEngine.addDocument(doc3);

    // Search for documents by a specific tag
    auto results = searchEngine.searchByTag("tag1");
    std::cout << "Search by tag 'tag1':" << std::endl;
    for (const auto& doc : results) {
        std::cout << "Document ID: " << doc.id << std::endl;
    }

    // Perform a fuzzy search for documents by a tag with a specified tolerance
    results = searchEngine.fuzzySearchByTag("tag1", 1);
    std::cout << "Fuzzy search by tag 'tag1' with tolerance 1:" << std::endl;
    for (const auto& doc : results) {
        std::cout << "Document ID: " << doc.id << std::endl;
    }

    // Search for documents by multiple tags
    results = searchEngine.searchByTags({"tag1", "tag3"});
    std::cout << "Search by tags 'tag1' and 'tag3':" << std::endl;
    for (const auto& doc : results) {
        std::cout << "Document ID: " << doc.id << std::endl;
    }

    // Search for documents by content
    results = searchEngine.searchByContent("first document");
    std::cout << "Search by content 'first document':" << std::endl;
    for (const auto& doc : results) {
        std::cout << "Document ID: " << doc.id << std::endl;
    }

    // Perform a boolean search for documents by a query
    results = searchEngine.booleanSearch("first AND document");
    std::cout << "Boolean search 'first AND document':" << std::endl;
    for (const auto& doc : results) {
        std::cout << "Document ID: " << doc.id << std::endl;
    }

    // Provide autocomplete suggestions for a given prefix
    auto suggestions = searchEngine.autoComplete("do");
    std::cout << "Autocomplete suggestions for 'do':" << std::endl;
    for (const auto& suggestion : suggestions) {
        std::cout << suggestion << std::endl;
    }

    // Save the current index to a file
    searchEngine.saveIndex("index.dat");

    // Load the index from a file
    searchEngine.loadIndex("index.dat");

    // Update an existing document in the search engine
    Document updatedDoc1("1", "This is the updated first document.",
                         {"tag1", "tag4"});
    searchEngine.updateDocument(updatedDoc1);

    // Remove a document from the search engine
    searchEngine.removeDocument("2");

    return 0;
}