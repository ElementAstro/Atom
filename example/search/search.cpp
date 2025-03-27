/**
 * @file search_engine_example.cpp
 * @brief Comprehensive example demonstrating all features of the Atom Search
 * Engine
 * @author Example Author
 * @date 2025-03-23
 */

#include <algorithm>
#include <chrono>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "atom/search/search.hpp"

// Helper function to print section titles
void printSection(const std::string& title) {
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(80, '=') << "\n";
}

// Helper function to print document information
void printDocument(const atom::search::Document& doc) {
    std::cout << "Document ID: " << doc.getId() << std::endl;
    std::cout << "Content: " << doc.getContent() << std::endl;

    std::cout << "Tags: ";
    const auto& tags = doc.getTags();
    if (tags.empty()) {
        std::cout << "[none]";
    } else {
        std::cout << "[ ";
        for (const auto& tag : tags) {
            std::cout << tag << " ";
        }
        std::cout << "]";
    }
    std::cout << std::endl;

    std::cout << "Click Count: " << doc.getClickCount() << std::endl;
}

// Helper function to print search results
void printSearchResults(
    const std::vector<std::shared_ptr<atom::search::Document>>& results) {
    if (results.empty()) {
        std::cout << "No documents found." << std::endl;
        return;
    }

    std::cout << "Found " << results.size() << " document(s):" << std::endl;
    for (size_t i = 0; i < results.size(); ++i) {
        std::cout << "\n--- Result " << (i + 1) << " ---" << std::endl;
        printDocument(*results[i]);
    }
}

// Helper function to handle exceptions
template <typename Func>
void tryOperation(const std::string& operationName, Func&& operation) {
    try {
        operation();
    } catch (const atom::search::DocumentNotFoundException& e) {
        std::cout << "Document not found error: " << e.what() << std::endl;
    } catch (const atom::search::DocumentValidationException& e) {
        std::cout << "Document validation error: " << e.what() << std::endl;
    } catch (const atom::search::SearchOperationException& e) {
        std::cout << "Search operation error: " << e.what() << std::endl;
    } catch (const atom::search::SearchEngineException& e) {
        std::cout << "General search engine error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Standard exception: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "Unknown error" << std::endl;
    }
}

int main() {
    std::cout << "ATOM SEARCH ENGINE COMPREHENSIVE EXAMPLES\n";
    std::cout << "========================================\n";

    //--------------------------------------------------------------------------
    // 1. Creating Documents and Basic Validation
    //--------------------------------------------------------------------------
    printSection("1. Creating Documents and Basic Validation");

    // Creating a valid document
    std::cout << "Creating a valid document..." << std::endl;
    atom::search::Document validDoc(
        "doc1", "This is a sample document about search engines",
        {"search", "engine", "example"});
    printDocument(validDoc);

    // Demonstrating validation
    std::cout << "\nTrying to create documents with invalid parameters..."
              << std::endl;

    // Empty ID
    tryOperation("Create document with empty ID", []() {
        atom::search::Document invalidDoc("", "Content", {"tag"});
    });

    // Empty content
    tryOperation("Create document with empty content", []() {
        atom::search::Document invalidDoc("id", "", {"tag"});
    });

    // Modifying document content
    std::cout << "\nModifying document content..." << std::endl;
    tryOperation("Update content", [&validDoc]() {
        validDoc.setContent(
            "Updated content about search engines and indexing");
        std::cout << "Content updated successfully" << std::endl;
        std::cout << "New content: " << validDoc.getContent() << std::endl;
    });

    // Modifying document tags
    std::cout << "\nModifying document tags..." << std::endl;
    tryOperation("Add tag", [&validDoc]() {
        validDoc.addTag("indexing");
        std::cout << "Tag added successfully" << std::endl;
    });

    tryOperation("Remove tag", [&validDoc]() {
        validDoc.removeTag("example");
        std::cout << "Tag removed successfully" << std::endl;
    });

    std::cout << "\nUpdated document:" << std::endl;
    printDocument(validDoc);

    // Incrementing click count
    std::cout << "\nIncrementing click count..." << std::endl;
    validDoc.incrementClickCount();
    validDoc.incrementClickCount();
    std::cout << "New click count: " << validDoc.getClickCount() << std::endl;

    //--------------------------------------------------------------------------
    // 2. Basic Search Engine Operations
    //--------------------------------------------------------------------------
    printSection("2. Basic Search Engine Operations");

    // Create a search engine
    std::cout << "Creating a search engine with default thread settings..."
              << std::endl;
    atom::search::SearchEngine searchEngine;

    // Adding documents to the search engine
    std::cout << "\nAdding documents to the search engine..." << std::endl;

    // Create several documents
    atom::search::Document doc1("doc1",
                                "The quick brown fox jumps over the lazy dog",
                                {"animals", "fox", "dog"});

    atom::search::Document doc2(
        "doc2",
        "Machine learning algorithms can process large datasets efficiently",
        {"technology", "machine learning", "algorithms"});

    atom::search::Document doc3(
        "doc3", "Artificial intelligence is transforming many industries",
        {"technology", "ai", "transformation"});

    atom::search::Document doc4("doc4",
                                "The lazy cat sleeps all day in the sun",
                                {"animals", "cat", "lazy"});

    atom::search::Document doc5(
        "doc5", "Deep learning is a subset of machine learning",
        {"technology", "deep learning", "machine learning"});

    // Add documents to the search engine
    tryOperation("Add document 1", [&searchEngine, &doc1]() {
        searchEngine.addDocument(doc1);
        std::cout << "Document 1 added successfully" << std::endl;
    });

    tryOperation("Add document 2", [&searchEngine, &doc2]() {
        searchEngine.addDocument(doc2);
        std::cout << "Document 2 added successfully" << std::endl;
    });

    tryOperation("Add document 3", [&searchEngine, &doc3]() {
        searchEngine.addDocument(doc3);
        std::cout << "Document 3 added successfully" << std::endl;
    });

    tryOperation("Add document 4", [&searchEngine, &doc4]() {
        // Using move semantics
        searchEngine.addDocument(std::move(doc4));
        std::cout << "Document 4 added successfully" << std::endl;
    });

    tryOperation("Add document 5", [&searchEngine, &doc5]() {
        searchEngine.addDocument(doc5);
        std::cout << "Document 5 added successfully" << std::endl;
    });

    // Trying to add a document with an existing ID
    std::cout << "\nTrying to add a document with an existing ID..."
              << std::endl;
    tryOperation("Add duplicate document", [&searchEngine]() {
        atom::search::Document duplicateDoc("doc1", "Duplicate content",
                                            {"duplicate"});
        searchEngine.addDocument(duplicateDoc);
    });

    // Updating a document
    std::cout << "\nUpdating a document..." << std::endl;
    tryOperation("Update document", [&searchEngine]() {
        atom::search::Document updatedDoc(
            "doc2", "Updated content about machine learning and deep learning",
            {"technology", "machine learning", "updated"});
        searchEngine.updateDocument(updatedDoc);
        std::cout << "Document updated successfully" << std::endl;
    });

    // Removing a document
    std::cout << "\nRemoving a document..." << std::endl;
    tryOperation("Remove document", [&searchEngine]() {
        searchEngine.removeDocument("doc3");
        std::cout << "Document removed successfully" << std::endl;
    });

    // Trying to remove a non-existent document
    std::cout << "\nTrying to remove a non-existent document..." << std::endl;
    tryOperation("Remove non-existent document", [&searchEngine]() {
        searchEngine.removeDocument("nonexistent");
    });

    //--------------------------------------------------------------------------
    // 3. Basic Search Operations
    //--------------------------------------------------------------------------
    printSection("3. Basic Search Operations");

    // Searching by tag
    std::cout << "Searching documents by tag 'animals'..." << std::endl;
    tryOperation("Search by tag", [&searchEngine]() {
        auto results = searchEngine.searchByTag("animals");
        printSearchResults(results);
    });

    // Searching by content
    std::cout << "\nSearching documents by content 'machine learning'..."
              << std::endl;
    tryOperation("Search by content", [&searchEngine]() {
        auto results = searchEngine.searchByContent("machine learning");
        printSearchResults(results);
    });

    // Searching by multiple tags
    std::cout << "\nSearching documents by multiple tags ['technology', "
                 "'machine learning']..."
              << std::endl;
    tryOperation("Search by multiple tags", [&searchEngine]() {
        auto results =
            searchEngine.searchByTags({"technology", "machine learning"});
        printSearchResults(results);
    });

    // Boolean search
    std::cout << "\nPerforming boolean search 'machine AND learning'..."
              << std::endl;
    tryOperation("Boolean search", [&searchEngine]() {
        auto results = searchEngine.booleanSearch("machine AND learning");
        printSearchResults(results);
    });

    //--------------------------------------------------------------------------
    // 4. Advanced Search Features
    //--------------------------------------------------------------------------
    printSection("4. Advanced Search Features");

    // Add more documents for advanced search demonstrations
    std::cout << "Adding more documents for advanced search demonstrations..."
              << std::endl;

    atom::search::Document doc6(
        "doc6",
        "Natural language processing helps computers understand human language",
        {"technology", "nlp", "language"});

    atom::search::Document doc7(
        "doc7", "Computer vision systems can identify objects in images",
        {"technology", "computer vision", "images"});

    atom::search::Document doc8(
        "doc8",
        "Reinforcement learning enables agents to learn from their environment",
        {"technology", "reinforcement learning", "agents"});

    tryOperation("Add documents 6-8", [&searchEngine, &doc6, &doc7, &doc8]() {
        searchEngine.addDocument(doc6);
        searchEngine.addDocument(doc7);
        searchEngine.addDocument(doc8);
        std::cout << "Documents 6-8 added successfully" << std::endl;
    });

    // Fuzzy search by tag
    std::cout
        << "\nPerforming fuzzy search by tag 'vishion' with tolerance 2..."
        << std::endl;
    tryOperation("Fuzzy search by tag", [&searchEngine]() {
        auto results = searchEngine.fuzzySearchByTag(
            "vishion", 2);  // Should match "vision"
        printSearchResults(results);
    });

    // Autocomplete
    std::cout << "\nGetting autocomplete suggestions for prefix 'mach'..."
              << std::endl;
    tryOperation("Autocomplete", [&searchEngine]() {
        auto suggestions = searchEngine.autoComplete("mach", 5);

        std::cout << "Autocomplete suggestions:" << std::endl;
        for (const auto& suggestion : suggestions) {
            std::cout << "  - " << suggestion << std::endl;
        }
    });

    // Boolean search with more complex query
    std::cout << "\nPerforming complex boolean search 'technology AND "
                 "(learning OR language)'..."
              << std::endl;
    tryOperation("Complex boolean search", [&searchEngine]() {
        auto results =
            searchEngine.booleanSearch("technology AND (learning OR language)");
        printSearchResults(results);
    });

    //--------------------------------------------------------------------------
    // 5. Persistence - Save and Load Index
    //--------------------------------------------------------------------------
    printSection("5. Persistence - Save and Load Index");

    const std::string indexFile = "search_index.dat";

    // Save the search index
    std::cout << "Saving search index to file: " << indexFile << std::endl;
    tryOperation("Save index", [&searchEngine, &indexFile]() {
        searchEngine.saveIndex(indexFile);
        std::cout << "Search index saved successfully" << std::endl;
    });

    // Create a new search engine and load the index
    std::cout << "\nCreating a new search engine and loading the saved index..."
              << std::endl;
    atom::search::SearchEngine loadedEngine;

    tryOperation("Load index", [&loadedEngine, &indexFile]() {
        loadedEngine.loadIndex(indexFile);
        std::cout << "Search index loaded successfully" << std::endl;
    });

    // Verify the loaded index
    std::cout
        << "\nVerifying the loaded index by searching for 'machine learning'..."
        << std::endl;
    tryOperation("Search in loaded engine", [&loadedEngine]() {
        auto results = loadedEngine.searchByContent("machine learning");
        printSearchResults(results);
    });

    // Clean up the index file
    std::remove(indexFile.c_str());
    std::cout << "Cleaned up the index file" << std::endl;

    //--------------------------------------------------------------------------
    // 6. Click Tracking and Result Ranking
    //--------------------------------------------------------------------------
    printSection("6. Click Tracking and Result Ranking");

    // Add more documents with similar content but different click counts
    std::cout
        << "Adding documents with similar content but different click counts..."
        << std::endl;

    // Create documents with similar content
    atom::search::Document rankDoc1(
        "rank1", "Information retrieval systems help find relevant documents",
        {"information retrieval", "search"});

    atom::search::Document rankDoc2(
        "rank2",
        "Modern information retrieval uses machine learning techniques",
        {"information retrieval", "machine learning"});

    atom::search::Document rankDoc3(
        "rank3", "Information retrieval is essential for search engines",
        {"information retrieval", "search engines"});

    // Add documents to the search engine
    tryOperation("Add ranking test documents", [&searchEngine, &rankDoc1,
                                                &rankDoc2, &rankDoc3]() {
        searchEngine.addDocument(rankDoc1);
        searchEngine.addDocument(rankDoc2);
        searchEngine.addDocument(rankDoc3);
        std::cout << "Ranking test documents added successfully" << std::endl;
    });

    // Simulate clicks to affect ranking
    std::cout << "\nSimulating user clicks on documents..." << std::endl;

    // Find documents and increment click counts
    tryOperation("Simulate clicks", [&searchEngine]() {
        // Simulate multiple clicks on rankDoc2
        auto results = searchEngine.searchByTag("information retrieval");

        for (auto& doc : results) {
            if (doc->getId() == "rank2") {
                // Simulate 5 clicks on rank2
                for (int i = 0; i < 5; i++) {
                    doc->incrementClickCount();
                }
                std::cout << "Simulated 5 clicks on document 'rank2'"
                          << std::endl;
            } else if (doc->getId() == "rank3") {
                // Simulate 2 clicks on rank3
                for (int i = 0; i < 2; i++) {
                    doc->incrementClickCount();
                }
                std::cout << "Simulated 2 clicks on document 'rank3'"
                          << std::endl;
            }
        }
    });

    // Search again to see ranking changes
    std::cout
        << "\nSearching for 'information retrieval' to see ranking changes..."
        << std::endl;
    tryOperation("Search after clicks", [&searchEngine]() {
        auto results = searchEngine.searchByContent("information retrieval");
        printSearchResults(results);

        // The documents with more clicks should be ranked higher
        if (!results.empty()) {
            std::cout << "\nNote: Documents should be ranked by relevance and "
                         "click count"
                      << std::endl;
            std::cout << "Document 'rank2' (5 clicks) should be ranked higher "
                         "than 'rank3' (2 clicks),"
                      << std::endl;
            std::cout << "which should be ranked higher than 'rank1' (0 clicks)"
                      << std::endl;
        }
    });

    //--------------------------------------------------------------------------
    // 7. Error Handling and Edge Cases
    //--------------------------------------------------------------------------
    printSection("7. Error Handling and Edge Cases");

    // Test with empty search queries
    std::cout << "Testing with empty search queries..." << std::endl;

    tryOperation("Empty tag search", [&searchEngine]() {
        auto results = searchEngine.searchByTag("");
        std::cout << "Empty tag search returned " << results.size()
                  << " results" << std::endl;
    });

    tryOperation("Empty content search", [&searchEngine]() {
        auto results = searchEngine.searchByContent("");
        std::cout << "Empty content search returned " << results.size()
                  << " results" << std::endl;
    });

    // Test with non-existent tags
    std::cout << "\nTesting with non-existent tags..." << std::endl;

    tryOperation("Non-existent tag search", [&searchEngine]() {
        auto results = searchEngine.searchByTag("nonexistenttag123456789");
        std::cout << "Non-existent tag search returned " << results.size()
                  << " results" << std::endl;
    });

    // Test fuzzy search with invalid tolerance
    std::cout << "\nTesting fuzzy search with invalid tolerance..."
              << std::endl;

    tryOperation("Negative tolerance fuzzy search", [&searchEngine]() {
        auto results = searchEngine.fuzzySearchByTag("technology", -1);
    });

    // Test autocomplete with edge cases
    std::cout << "\nTesting autocomplete edge cases..." << std::endl;

    tryOperation("Empty prefix autocomplete", [&searchEngine]() {
        auto suggestions = searchEngine.autoComplete("", 5);
        std::cout << "Empty prefix autocomplete returned " << suggestions.size()
                  << " suggestions" << std::endl;
    });

    tryOperation("Non-matching prefix autocomplete", [&searchEngine]() {
        auto suggestions = searchEngine.autoComplete("xyznonexistent", 5);
        std::cout << "Non-matching prefix autocomplete returned "
                  << suggestions.size() << " suggestions" << std::endl;
    });

    // Test saving to an invalid location
    std::cout << "\nTesting saving to an invalid location..." << std::endl;

    tryOperation("Save to invalid location", [&searchEngine]() {
        searchEngine.saveIndex("/nonexistent/directory/index.dat");
    });

    //--------------------------------------------------------------------------
    // 8. Multithreaded Search Engine
    //--------------------------------------------------------------------------
    printSection("8. Multithreaded Search Engine");

    // Create a search engine with explicit thread count
    std::cout << "Creating a search engine with 4 worker threads..."
              << std::endl;
    atom::search::SearchEngine mtSearchEngine(4);

    // Add a large number of documents
    std::cout << "\nAdding a larger set of documents to demonstrate parallel "
                 "processing..."
              << std::endl;

    const int numDocs = 100;
    tryOperation("Add multiple documents", [&mtSearchEngine, numDocs]() {
        for (int i = 0; i < numDocs; ++i) {
            std::string docId = "mt" + std::to_string(i);
            std::string content =
                "Document " + std::to_string(i) + " content with ";

            // Add some varied content and tags
            if (i % 3 == 0) {
                content += "machine learning and artificial intelligence";
                mtSearchEngine.addDocument(atom::search::Document(
                    docId, content, {"technology", "machine learning", "ai"}));
            } else if (i % 3 == 1) {
                content += "data processing and information retrieval";
                mtSearchEngine.addDocument(atom::search::Document(
                    docId, content, {"data", "information retrieval"}));
            } else {
                content += "search engines and indexing techniques";
                mtSearchEngine.addDocument(atom::search::Document(
                    docId, content, {"search", "indexing"}));
            }
        }
        std::cout << "Added " << numDocs << " documents successfully"
                  << std::endl;
    });

    // Perform a search that will be processed in parallel
    std::cout << "\nPerforming a search that will be processed in parallel..."
              << std::endl;
    tryOperation("Parallel content search", [&mtSearchEngine]() {
        // Measure the time taken
        auto startTime = std::chrono::high_resolution_clock::now();

        auto results =
            mtSearchEngine.searchByContent("machine learning information");

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - startTime);

        std::cout << "Search completed in " << duration.count()
                  << " milliseconds" << std::endl;
        std::cout << "Found " << results.size() << " matching documents"
                  << std::endl;

        // Print just the first few results to avoid overwhelming output
        const size_t maxResultsToPrint = 5;
        std::vector<std::shared_ptr<atom::search::Document>> limitedResults;

        for (size_t i = 0; i < std::min(results.size(), maxResultsToPrint);
             ++i) {
            limitedResults.push_back(results[i]);
        }

        if (!results.empty()) {
            std::cout << "\nShowing first " << limitedResults.size() << " of "
                      << results.size() << " results:" << std::endl;
            printSearchResults(limitedResults);

            if (results.size() > maxResultsToPrint) {
                std::cout << "... and " << (results.size() - maxResultsToPrint)
                          << " more documents" << std::endl;
            }
        }
    });

    //--------------------------------------------------------------------------
    // Summary
    //--------------------------------------------------------------------------
    printSection("Summary");

    std::cout
        << "This example demonstrated the following Search Engine features:"
        << std::endl;
    std::cout << "  1. Creating and validating documents" << std::endl;
    std::cout
        << "  2. Basic search engine operations (add, update, remove documents)"
        << std::endl;
    std::cout << "  3. Basic search operations (by tag, content, multiple "
                 "tags, boolean)"
              << std::endl;
    std::cout << "  4. Advanced search features (fuzzy search, autocomplete)"
              << std::endl;
    std::cout << "  5. Persistence with save and load operations" << std::endl;
    std::cout << "  6. Click tracking and result ranking" << std::endl;
    std::cout << "  7. Error handling and edge cases" << std::endl;
    std::cout << "  8. Multithreaded search operations" << std::endl;

    return 0;
}