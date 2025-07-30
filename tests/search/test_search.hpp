#ifndef ATOM_SEARCH_TEST_SEARCH_HPP
#define ATOM_SEARCH_TEST_SEARCH_HPP

#include <gtest/gtest.h>

#include "atom/search/search.hpp"

using namespace atom::search;

// Test fixture for SearchEngine
class SearchEngineTest : public ::testing::Test {
protected:
    SearchEngine engine;

    void SetUp() override {
        // Add some initial documents to the search engine
        engine.add_document(Document("1", "Hello world", {"greeting", "world"}));
        engine.add_document(
            Document("2", "Goodbye world", {"farewell", "world"}));
    }
};

TEST_F(SearchEngineTest, AddDocument) {
    Document doc("3", "New document", {"new", "document"});
    engine.add_document(doc);
    auto result = engine.search_by_tag("new");
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[0]->get_id(), "3");
}

TEST_F(SearchEngineTest, RemoveDocument) {
    engine.remove_document("1");
    ASSERT_THROW(engine.remove_document("1"), DocumentNotFoundException);
}

TEST_F(SearchEngineTest, UpdateDocument) {
    Document updatedDoc("1", "Updated content", {"updated", "content"});
    engine.update_document(updatedDoc);
    auto result = engine.search_by_tag("updated");
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[0]->get_content(), "Updated content");
}

TEST_F(SearchEngineTest, SearchByTag) {
    auto result = engine.search_by_tag("world");
    ASSERT_EQ(result.size(), 2);
}

TEST_F(SearchEngineTest, FuzzySearchByTag) {
    auto result = engine.fuzzy_search_by_tag("wrold", 1);
    ASSERT_EQ(result.size(), 2);
}

TEST_F(SearchEngineTest, SearchByTags) {
    auto result = engine.search_by_tags({"greeting", "world"});
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[0]->get_id(), "1");
}

TEST_F(SearchEngineTest, SearchByContent) {
    auto result = engine.search_by_content("Goodbye");
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[0]->get_id(), "2");
}

TEST_F(SearchEngineTest, BooleanSearch) {
    auto result = engine.boolean_search("Hello AND world");
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[0]->get_id(), "1");
}

TEST_F(SearchEngineTest, AutoComplete) {
    auto suggestions = engine.auto_complete("wo");
    ASSERT_EQ(suggestions.size(), 1);
    ASSERT_EQ(suggestions[0], "world");
}

TEST_F(SearchEngineTest, SaveAndLoadIndex) {
    engine.save_index("test_index.json");
    SearchEngine newEngine;
    newEngine.load_index("test_index.json");
    auto result = newEngine.search_by_tag("world");
    ASSERT_EQ(result.size(), 2);
}

#endif
TEST_F(SearchEngineTest, AddDocumentMoveSemantics) {
    // Create a document
    Document doc("3", "Document to move", {"move", "test"});
    String originalId(doc.getId());  // Capture before move

    // Add using move semantics
    engine.add_document(std::move(doc));

    // Verify the document was added
    ASSERT_TRUE(engine.has_document(originalId));
    auto result = engine.search_by_tag("move");
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0]->get_id(), originalId);

    // Note: Accessing moved-from object is generally unsafe.
    // We rely on the engine holding the document correctly.
}

TEST_F(SearchEngineTest, AddDocumentThrowsOnDuplicateId) {
    // Attempt to add a document with an existing ID ("1")
    Document duplicateDoc("1", "This should fail", {"fail"});
    EXPECT_THROW(engine.add_document(duplicateDoc), std::invalid_argument);
}

TEST_F(SearchEngineTest, AddDocumentThrowsOnInvalidDocument) {
    // Attempt to add a document with an empty ID
    EXPECT_THROW(Document("", "Invalid ID"), DocumentValidationException);

    // Attempt to add a document with empty content
    EXPECT_THROW(Document("invalid_doc_2", ""), DocumentValidationException);
}

TEST_F(SearchEngineTest, UpdateDocumentThrowsOnNonExistent) {
    // Attempt to update a non-existent document
    Document nonExistentDoc("99", "Update this", {"update"});
    EXPECT_THROW(engine.update_document(nonExistentDoc),
                 DocumentNotFoundException);
}

TEST_F(SearchEngineTest, UpdateDocumentThrowsOnInvalidDocument) {
    // Attempt to update an existing document ("1") with invalid content
    Document invalidUpdateDoc("1", "", {"invalid"});
    EXPECT_THROW(engine.update_document(invalidUpdateDoc),
                 DocumentValidationException);
}

TEST_F(SearchEngineTest, SearchByTagNoMatch) {
    auto result = engine.search_by_tag("nonexistent_tag");
    EXPECT_TRUE(result.empty());
}

TEST_F(SearchEngineTest, FuzzySearchByTagToleranceZero) {
    // Add a document with a tag that is a fuzzy match but not exact
    engine.add_document(Document("3", "Another doc", {"taggy"}));

    // Tolerance 0 should only match exact tags
    auto resultExact = engine.fuzzy_search_by_tag("world", 0);
    ASSERT_EQ(resultExact.size(), 2);  // Matches "world"

    auto resultFuzzy = engine.fuzzy_search_by_tag("taggi", 0);
    EXPECT_TRUE(
        resultFuzzy.empty());  // Does not match "taggy" with tolerance 0
}

TEST_F(SearchEngineTest, FuzzySearchByTagNoMatch) {
    auto result = engine.fuzzy_search_by_tag("nonexistent_tag", 2);
    EXPECT_TRUE(result.empty());
}

TEST_F(SearchEngineTest, FuzzySearchByTagInvalidTolerance) {
    EXPECT_THROW(engine.fuzzy_search_by_tag("world", -1), std::invalid_argument);
}

TEST_F(SearchEngineTest, SearchByTagsEmptyList) {
    auto result = engine.search_by_tags({});
    EXPECT_TRUE(result.empty());
}

TEST_F(SearchEngineTest, SearchByTagsNoMatch) {
    auto result = engine.search_by_tags({"greeting", "farewell"});  // AND search
    EXPECT_TRUE(result.empty());  // No document has both tags
}

TEST_F(SearchEngineTest, SearchByContentNoMatch) {
    auto result = engine.search_by_content("nonexistent_word");
    EXPECT_TRUE(result.empty());
}

TEST_F(SearchEngineTest, SearchByContentEmptyQuery) {
    auto result = engine.search_by_content("");
    EXPECT_TRUE(result.empty());
}

TEST_F(SearchEngineTest, BooleanSearchComplex) {
    engine.add_document(Document("3", "Hello there", {"greeting"}));
    engine.add_document(Document("4", "Goodbye everyone", {"farewell"}));
    engine.add_document(
        Document("5", "Hello world again", {"greeting", "world", "again"}));

    // Test OR
    auto resultOR = engine.boolean_search("Hello OR Goodbye");
    // Docs with Hello: 1, 3, 5
    // Docs with Goodbye: 2, 4
    // OR should be 1, 2, 3, 4, 5.
    ASSERT_EQ(resultOR.size(), 5);
    std::vector<String> ids_or;
    for (const auto& doc_ptr : resultOR)
        ids_or.push_back(String(doc_ptr->get_id()));
    std::sort(ids_or.begin(), ids_or.end());
    EXPECT_EQ(ids_or, std::vector<String>({"1", "2", "3", "4", "5"}));

    // Test AND NOT
    auto resultANDNOT = engine.boolean_search("Hello AND NOT world");
    // Docs with Hello: 1, 3, 5
    // Docs with world: 1, 2, 5
    // Docs with Hello AND NOT world: 3 ("Hello there")
    ASSERT_EQ(resultANDNOT.size(), 1);
    EXPECT_EQ(resultANDNOT[0]->get_id(), "3");

    // Test parentheses and combinations
    auto resultCombined = engine.boolean_search("(Hello OR Goodbye) AND world");
    // Docs with (Hello OR Goodbye): 1, 2, 3, 4, 5
    // Docs with world: 1, 2, 5
    // Docs with (Hello OR Goodbye) AND world: 1, 2, 5
    ASSERT_EQ(resultCombined.size(), 3);
    std::vector<String> ids_combined;
    for (const auto& doc_ptr : resultCombined)
        ids_combined.push_back(String(doc_ptr->get_id()));
    std::sort(ids_combined.begin(), ids_combined.end());
    EXPECT_EQ(ids_combined, std::vector<String>({"1", "2", "5"}));
}

TEST_F(SearchEngineTest, BooleanSearchInvalidSyntax) {
    EXPECT_THROW(engine.boolean_search("Hello AND OR Goodbye"),
                 SearchOperationException);
    EXPECT_THROW(engine.boolean_search("AND Hello"), SearchOperationException);
    EXPECT_THROW(engine.boolean_search("Hello NOT"), SearchOperationException);
}

TEST_F(SearchEngineTest, BooleanSearchNoMatch) {
    auto result = engine.boolean_search("nonexistent AND query");
    EXPECT_TRUE(result.empty());
}

TEST_F(SearchEngineTest, AutoCompleteEmptyPrefix) {
    auto suggestions = engine.auto_complete("");
    EXPECT_TRUE(suggestions.empty());
}

TEST_F(SearchEngineTest, AutoCompleteMaxResults) {
    // Add documents with tags/content starting with "prefix"
    engine.add_document(Document("3", "Prefix document one", {"prefix1"}));
    engine.add_document(Document("4", "Prefix document two", {"prefix2"}));
    engine.add_document(Document("5", "Prefix document three", {"prefix3"}));
    engine.add_document(Document("6", "Another prefix doc", {"prefix4"}));

    // Autocomplete with limit
    auto suggestions = engine.auto_complete("pre", 2);
    ASSERT_EQ(suggestions.size(), 2);
    // The order might not be guaranteed, just check if the correct number of
    // suggestions are returned. We could check if the suggestions are among the
    // expected ones if order isn't guaranteed. For simplicity, just check size
    // for now.
}

TEST_F(SearchEngineTest, GetDocumentCount) {
    // Initial documents from SetUp
    EXPECT_EQ(engine.getDocumentCount(), 2);

    engine.add_document(Document("3", "Doc 3"));
    EXPECT_EQ(engine.getDocumentCount(), 3);

    engine.remove_document("1");
    EXPECT_EQ(engine.getDocumentCount(), 2);

    engine.clear();
    EXPECT_EQ(engine.getDocumentCount(), 0);
}

TEST_F(SearchEngineTest, Clear) {
    engine.add_document(Document("3", "Doc 3"));
    engine.add_document(Document("4", "Doc 4"));
    EXPECT_EQ(engine.getDocumentCount(), 4);

    engine.clear();

    EXPECT_EQ(engine.getDocumentCount(), 0);
    EXPECT_TRUE(engine.search_by_tag("world").empty());
    EXPECT_FALSE(engine.has_document("1"));
    EXPECT_FALSE(engine.has_document("2"));
    EXPECT_FALSE(engine.has_document("3"));
    EXPECT_FALSE(engine.has_document("4"));
    EXPECT_TRUE(engine.getAllDocumentIds().empty());
}

TEST_F(SearchEngineTest, HasDocument) {
    EXPECT_TRUE(engine.has_document("1"));
    EXPECT_TRUE(engine.has_document("2"));
    EXPECT_FALSE(engine.has_document("nonexistent"));

    engine.remove_document("1");
    EXPECT_FALSE(engine.has_document("1"));
    EXPECT_TRUE(engine.has_document("2"));
}

TEST_F(SearchEngineTest, GetAllDocumentIds) {
    engine.add_document(Document("3", "Doc 3"));
    auto ids = engine.getAllDocumentIds();
    ASSERT_EQ(ids.size(), 3);
    std::sort(ids.begin(), ids.end());  // Sort for consistent comparison
    EXPECT_EQ(ids, std::vector<String>({"1", "2", "3"}));

    engine.remove_document("1");
    ids = engine.getAllDocumentIds();
    ASSERT_EQ(ids.size(), 2);
    std::sort(ids.begin(), ids.end());
    EXPECT_EQ(ids, std::vector<String>({"2", "3"}));

    engine.clear();
    ids = engine.getAllDocumentIds();
    EXPECT_TRUE(ids.empty());
}

// Tests for Document class methods (via SearchEngine interaction)
TEST_F(SearchEngineTest, DocumentClickCount) {
    // Get shared pointer to document 1
    auto result = engine.search_by_tag("greeting");  // Get doc 1
    ASSERT_EQ(result.size(), 1);
    auto doc1 = result[0];

    EXPECT_EQ(doc1->getClickCount(), 0);

    doc1->incrementClickCount();
    EXPECT_EQ(doc1->getClickCount(), 1);

    doc1->incrementClickCount();
    EXPECT_EQ(doc1->getClickCount(), 2);

    doc1->setClickCount(10);
    EXPECT_EQ(doc1->getClickCount(), 10);

    doc1->resetClickCount();
    EXPECT_EQ(doc1->getClickCount(), 0);

    // Verify click count persists in the engine's copy (if shared_ptr is used
    // correctly)
    auto doc1_again = engine.search_by_tag("greeting")[0];
    EXPECT_EQ(doc1_again->getClickCount(), 0);  // Should be 0 after reset
}

TEST_F(SearchEngineTest, DocumentSetContent) {
    auto result = engine.search_by_tag("greeting");  // Get doc 1
    ASSERT_EQ(result.size(), 1);
    auto doc1 = result[0];

    doc1->setContent("New content for doc 1");
    EXPECT_EQ(doc1->get_content(), "New content for doc 1");

    // Test validation
    EXPECT_THROW(doc1->setContent(""), DocumentValidationException);
}

TEST_F(SearchEngineTest, DocumentAddRemoveTag) {
    auto result = engine.search_by_tag("greeting");  // Get doc 1
    ASSERT_EQ(result.size(), 1);
    auto doc1 = result[0];

    // Add a new tag
    doc1->addTag("new_tag");
    const auto& tags = doc1->getTags();
    EXPECT_TRUE(tags.count("new_tag"));
    EXPECT_TRUE(tags.count("greeting"));
    EXPECT_TRUE(tags.count("world"));
    EXPECT_EQ(tags.size(), 3);

    // Remove an existing tag
    doc1->removeTag("greeting");
    const auto& tags_after_remove = doc1->getTags();
    EXPECT_FALSE(tags_after_remove.count("greeting"));
    EXPECT_TRUE(tags_after_remove.count("new_tag"));
    EXPECT_TRUE(tags_after_remove.count("world"));
    EXPECT_EQ(tags_after_remove.size(), 2);

    // Remove a non-existent tag (should do nothing)
    doc1->removeTag("nonexistent_tag");
    EXPECT_EQ(doc1->getTags().size(), 2);  // Size should remain 2

    // Test validation
    EXPECT_THROW(doc1->addTag(""), DocumentValidationException);
}

TEST_F(SearchEngineTest, DocumentValidation) {
    // Test constructor validation
    EXPECT_THROW(Document("", "Valid content"), DocumentValidationException);
    EXPECT_THROW(Document("valid_id", ""), DocumentValidationException);
    EXPECT_THROW(Document("valid_id", "valid content", {""}),
                 DocumentValidationException);

    // Test setContent validation
    Document doc("test_val", "initial");
    EXPECT_THROW(doc.setContent(""), DocumentValidationException);

    // Test addTag validation
    EXPECT_THROW(doc.addTag(""), DocumentValidationException);
}

TEST_F(SearchEngineTest, ConstructorMaxThreads) {
    // This test primarily ensures the constructor with maxThreads doesn't throw
    // and can be instantiated. We can't easily verify the internal thread count
    // from the public interface.
    EXPECT_NO_THROW(SearchEngine engine_threaded(4));
    EXPECT_NO_THROW(
        SearchEngine engine_auto(0));  // 0 means use hardware concurrency
}

TEST_F(SearchEngineTest, SaveLoadIndexEmpty) {
    // Clear the initial documents
    engine.clear();
    EXPECT_EQ(engine.getDocumentCount(), 0);

    const String testFile = "test_search_empty_index.json";
    engine.save_index(testFile);

    SearchEngine newEngine;
    newEngine.loadIndex(testFile);

    EXPECT_EQ(newEngine.getDocumentCount(), 0);
    EXPECT_TRUE(newEngine.getAllDocumentIds().empty());

    // Clean up
    std::remove(testFile.c_str());
}

TEST_F(SearchEngineTest, LoadIndexNonExistent) {
    // Loading a non-existent file should throw
    EXPECT_THROW(engine.load_index("non_existent_index_file.json"),
                 std::ios_base::failure);
}

// Note: Testing loadIndex with an invalid file format is difficult without
// knowing the serialization format. Skipping for now.
