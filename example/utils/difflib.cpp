/**
 * @file difflib_example.cpp
 * @brief Comprehensive examples demonstrating difflib utilities
 *
 * This example demonstrates all functions available in atom::utils::difflib.hpp:
 * - SequenceMatcher for comparing sequences
 * - Differ for generating text differences
 * - HtmlDiff for generating HTML diff visualizations
 * - getCloseMatches for finding similar strings
 */

 #include "atom/utils/difflib.hpp"
 #include <iostream>
 #include <iomanip>
 #include <fstream>
 #include <sstream>
 #include <string>
 #include <vector>
 #include <chrono>

 // Helper function to print section headers
 void printSection(const std::string& title) {
     std::cout << "\n===============================================" << std::endl;
     std::cout << "  " << title << std::endl;
     std::cout << "===============================================" << std::endl;
 }

 // Helper function to print sequences
 void printSequences(const std::vector<std::string>& seq1, const std::vector<std::string>& seq2) {
     std::cout << "Sequence 1:" << std::endl;
     for (const auto& item : seq1) {
         std::cout << "  " << item << std::endl;
     }

     std::cout << "\nSequence 2:" << std::endl;
     for (const auto& item : seq2) {
         std::cout << "  " << item << std::endl;
     }
     std::cout << std::endl;
 }

 // Helper function to print the matching blocks
 void printMatchingBlocks(const std::vector<std::tuple<int, int, int>>& blocks) {
     std::cout << "Matching blocks:" << std::endl;
     for (const auto& [a, b, size] : blocks) {
         std::cout << "  a[" << a << ":" << (a + size) << "] == b[" << b << ":"
                   << (b + size) << "] (size: " << size << ")" << std::endl;
     }
     std::cout << std::endl;
 }

 // Helper function to print the opcodes
 void printOpcodes(const std::vector<std::tuple<std::string, int, int, int, int>>& opcodes) {
     std::cout << "Opcodes:" << std::endl;
     for (const auto& [tag, i1, i2, j1, j2] : opcodes) {
         std::cout << "  " << std::left << std::setw(8) << tag
                   << " a[" << i1 << ":" << i2 << "] b[" << j1 << ":" << j2 << "]" << std::endl;
     }
     std::cout << std::endl;
 }

 // Helper function to save a string to a file
 bool saveToFile(const std::string& filename, const std::string& content) {
     std::ofstream file(filename);
     if (!file.is_open()) {
         std::cerr << "Failed to open file for writing: " << filename << std::endl;
         return false;
     }

     file << content;
     file.close();
     return true;
 }

 int main() {
     try {
         std::cout << "Difflib Utilities Demonstration" << std::endl;

         // ===================================================
         // Example 1: Basic String Comparison with SequenceMatcher
         // ===================================================
         printSection("1. Basic String Comparison with SequenceMatcher");

         // Compare two simple strings
         std::string str1 = "This is the first test string.";
         std::string str2 = "This is the second test string.";

         std::cout << "String 1: \"" << str1 << "\"" << std::endl;
         std::cout << "String 2: \"" << str2 << "\"" << std::endl;

         // Create a SequenceMatcher
         atom::utils::SequenceMatcher matcher(str1, str2);

         // Calculate similarity ratio
         double similarity = matcher.ratio();
         std::cout << "Similarity ratio: " << similarity << " ("
                   << static_cast<int>(similarity * 100) << "%)" << std::endl;

         // Get matching blocks
         auto blocks = matcher.getMatchingBlocks();
         printMatchingBlocks(blocks);

         // Get opcodes
         auto opcodes = matcher.getOpcodes();
         printOpcodes(opcodes);

         // ===================================================
         // Example 2: Comparing Different Strings
         // ===================================================
         printSection("2. Comparing Different Strings");

         // Compare two more different strings
         std::string text1 = "The quick brown fox jumps over the lazy dog.";
         std::string text2 = "A quick brown dog jumps over the lazy fox.";

         std::cout << "Text 1: \"" << text1 << "\"" << std::endl;
         std::cout << "Text 2: \"" << text2 << "\"" << std::endl;

         // Set new sequences to compare
         matcher.setSeqs(text1, text2);

         // Calculate similarity ratio
         similarity = matcher.ratio();
         std::cout << "Similarity ratio: " << similarity << " ("
                   << static_cast<int>(similarity * 100) << "%)" << std::endl;

         // Get matching blocks and opcodes
         blocks = matcher.getMatchingBlocks();
         printMatchingBlocks(blocks);

         opcodes = matcher.getOpcodes();
         printOpcodes(opcodes);

         // ===================================================
         // Example 3: Comparing Line Sequences with Differ
         // ===================================================
         printSection("3. Comparing Line Sequences with Differ");

         // Create two sequences of lines
         std::vector<std::string> lines1 = {
             "Line 1: This is a test.",
             "Line 2: The quick brown fox jumps over the lazy dog.",
             "Line 3: Python programming is fun.",
             "Line 4: This line will be removed.",
             "Line 5: The end."
         };

         std::vector<std::string> lines2 = {
             "Line 1: This is a test.",
             "Line 2: The quick brown fox jumps over the lazy cat.", // Changed dog -> cat
             "Line 3: C++ programming is fun.",                      // Changed Python -> C++
             "Line 5: The end.",                                     // Line 4 removed
             "Line 6: An additional line."                           // New line added
         };

         // Print the original sequences
         printSequences(lines1, lines2);

         // Generate differences using Differ
         std::cout << "Differences (Differ::compare):" << std::endl;
         auto diffs = atom::utils::Differ::compare(lines1, lines2);

         for (const auto& line : diffs) {
             std::cout << line << std::endl;
         }

         // ===================================================
         // Example 4: Unified Diff Format
         // ===================================================
         printSection("4. Unified Diff Format");

         // Generate unified diff with default parameters
         std::cout << "Unified diff (default context=3):" << std::endl;
         auto unified_diff = atom::utils::Differ::unifiedDiff(lines1, lines2);

         for (const auto& line : unified_diff) {
             std::cout << line << std::endl;
         }

         // Generate unified diff with custom parameters
         std::cout << "\nUnified diff (custom labels, context=1):" << std::endl;
         auto custom_diff = atom::utils::Differ::unifiedDiff(
             lines1, lines2, "original.txt", "modified.txt", 1);

         for (const auto& line : custom_diff) {
             std::cout << line << std::endl;
         }

         // ===================================================
         // Example 5: HTML Diff Visualization
         // ===================================================
         printSection("5. HTML Diff Visualization");

         // Generate HTML diff table
         std::cout << "Generating HTML diff table..." << std::endl;
         auto html_table_result = atom::utils::HtmlDiff::makeTable(
             lines1, lines2, "Original Text", "Modified Text");

         if (html_table_result) {
             std::cout << "HTML table generated successfully." << std::endl;
             std::cout << "HTML table size: " << html_table_result->size() << " bytes" << std::endl;

             // Show first 200 characters
             std::cout << "Preview:" << std::endl;
             std::cout << html_table_result->substr(0, 200) << "..." << std::endl;

             // Save to file
             if (saveToFile("diff_table.html", *html_table_result)) {
                 std::cout << "Saved to diff_table.html" << std::endl;
             }
         } else {
             std::cerr << "Failed to generate HTML table: " << html_table_result.error() << std::endl;
         }

         // Generate complete HTML file
         std::cout << "\nGenerating complete HTML diff file..." << std::endl;
         auto html_file_result = atom::utils::HtmlDiff::makeFile(
             lines1, lines2, "Original Text", "Modified Text");

         if (html_file_result) {
             std::cout << "HTML file generated successfully." << std::endl;
             std::cout << "HTML file size: " << html_file_result->size() << " bytes" << std::endl;

             // Save to file
             if (saveToFile("diff_complete.html", *html_file_result)) {
                 std::cout << "Saved to diff_complete.html" << std::endl;
             }
         } else {
             std::cerr << "Failed to generate HTML file: " << html_file_result.error() << std::endl;
         }

         // ===================================================
         // Example 6: Finding Close Matches
         // ===================================================
         printSection("6. Finding Close Matches");

         // Define a list of words
         std::vector<std::string> words = {
             "apple", "banana", "cherry", "date", "elderberry",
             "fig", "grape", "honeydew", "imbe", "jackfruit",
             "kiwi", "lemon", "mango", "nectarine", "orange",
             "papaya", "quince", "raspberry", "strawberry", "tangerine"
         };

         std::cout << "List of words:" << std::endl;
         for (size_t i = 0; i < words.size(); ++i) {
             std::cout << words[i];
             if (i < words.size() - 1) {
                 std::cout << ", ";
             }
             if ((i + 1) % 5 == 0) {
                 std::cout << std::endl;
             }
         }
         std::cout << std::endl;

         // Find close matches for slightly misspelled words
         std::vector<std::string> test_words = {
             "aple", "strberry", "lemen", "banna", "grap"
         };

         for (const auto& test_word : test_words) {
             std::cout << "Finding close matches for \"" << test_word << "\":" << std::endl;

             // Test with default parameters
             auto matches = atom::utils::getCloseMatches(test_word, words);
             std::cout << "  Default (n=3, cutoff=0.6): ";
             for (size_t i = 0; i < matches.size(); ++i) {
                 std::cout << matches[i];
                 if (i < matches.size() - 1) {
                     std::cout << ", ";
                 }
             }
             std::cout << std::endl;

             // Test with different parameters
             auto matches2 = atom::utils::getCloseMatches(test_word, words, 1, 0.7);
             std::cout << "  Custom (n=1, cutoff=0.7): ";
             for (const auto& match : matches2) {
                 std::cout << match;
             }
             std::cout << std::endl;

             // Test with very low cutoff
             auto matches3 = atom::utils::getCloseMatches(test_word, words, 5, 0.4);
             std::cout << "  Custom (n=5, cutoff=0.4): ";
             for (size_t i = 0; i < matches3.size(); ++i) {
                 std::cout << matches3[i];
                 if (i < matches3.size() - 1) {
                     std::cout << ", ";
                 }
             }
             std::cout << std::endl;
         }

         // ===================================================
         // Example 7: Performance Testing with Larger Texts
         // ===================================================
         printSection("7. Performance Testing with Larger Texts");

         // Generate larger text samples
         std::vector<std::string> large_text1;
         std::vector<std::string> large_text2;

         // Add some repeated content with variations
         for (int i = 0; i < 100; ++i) {
             std::ostringstream line1, line2;
             line1 << "Line " << i << ": This is test line number " << i << " in the first document.";
             large_text1.push_back(line1.str());

             // Make some differences in the second document
             if (i % 10 == 0) {
                 // Skip this line in text2 (deletion)
                 continue;
             } else if (i % 15 == 0) {
                 // Add an extra line (insertion)
                 line2 << "Line " << i << ": This is an EXTRA line for test " << i << ".";
                 large_text2.push_back(line2.str());
                 line2.str("");
                 line2 << "Line " << i << ": This is test line number " << i << " in the second document.";
                 large_text2.push_back(line2.str());
             } else if (i % 7 == 0) {
                 // Modify the line (replacement)
                 line2 << "Line " << i << ": This is MODIFIED test line " << i << " in the second document.";
                 large_text2.push_back(line2.str());
             } else {
                 // Keep the line the same with minimal changes
                 line2 << "Line " << i << ": This is test line number " << i << " in the second document.";
                 large_text2.push_back(line2.str());
             }
         }

         std::cout << "Created large text samples:" << std::endl;
         std::cout << "  Text 1: " << large_text1.size() << " lines" << std::endl;
         std::cout << "  Text 2: " << large_text2.size() << " lines" << std::endl;

         // Measure performance of different operations

         // 1. SequenceMatcher
         std::cout << "\nTesting SequenceMatcher performance..." << std::endl;
         auto start_time = std::chrono::high_resolution_clock::now();

         std::string joined_text1;
         std::string joined_text2;

         for (const auto& line : large_text1) {
             joined_text1 += line + "\n";
         }

         for (const auto& line : large_text2) {
             joined_text2 += line + "\n";
         }

         atom::utils::SequenceMatcher large_matcher(joined_text1, joined_text2);
         double large_similarity = large_matcher.ratio();

         auto end_time = std::chrono::high_resolution_clock::now();
         auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

         std::cout << "  Similarity ratio: " << large_similarity << std::endl;
         std::cout << "  Time taken: " << duration << " ms" << std::endl;

         // 2. Differ::compare
         std::cout << "\nTesting Differ::compare performance..." << std::endl;
         start_time = std::chrono::high_resolution_clock::now();

         auto large_diffs = atom::utils::Differ::compare(large_text1, large_text2);

         end_time = std::chrono::high_resolution_clock::now();
         duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

         std::cout << "  Generated diff with " << large_diffs.size() << " lines" << std::endl;
         std::cout << "  Time taken: " << duration << " ms" << std::endl;

         // 3. HtmlDiff::makeTable
         std::cout << "\nTesting HtmlDiff::makeTable performance..." << std::endl;
         start_time = std::chrono::high_resolution_clock::now();

         auto large_html_table = atom::utils::HtmlDiff::makeTable(large_text1, large_text2);

         end_time = std::chrono::high_resolution_clock::now();
         duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

         if (large_html_table) {
             std::cout << "  Generated HTML table with " << large_html_table->size() << " bytes" << std::endl;
         } else {
             std::cout << "  Failed to generate HTML table: " << large_html_table.error() << std::endl;
         }
         std::cout << "  Time taken: " << duration << " ms" << std::endl;

         // ===================================================
         // Example 8: Edge Cases and Special Scenarios
         // ===================================================
         printSection("8. Edge Cases and Special Scenarios");

         // Case 1: Empty strings
         std::cout << "Comparing empty strings:" << std::endl;
         atom::utils::SequenceMatcher empty_matcher("", "");
         std::cout << "  Similarity ratio: " << empty_matcher.ratio() << std::endl;

         // Case 2: Empty vs non-empty
         std::cout << "\nComparing empty vs non-empty string:" << std::endl;
         atom::utils::SequenceMatcher mixed_matcher("", "Hello world");
         std::cout << "  Similarity ratio: " << mixed_matcher.ratio() << std::endl;

         // Case 3: Identical strings
         std::cout << "\nComparing identical strings:" << std::endl;
         std::string identical = "This string is exactly the same in both cases.";
         atom::utils::SequenceMatcher identical_matcher(identical, identical);
         std::cout << "  Similarity ratio: " << identical_matcher.ratio() << std::endl;

         // Case 4: Finding close matches with empty string
         std::cout << "\nFinding close matches for empty string:" << std::endl;
         auto empty_matches = atom::utils::getCloseMatches("", words);
         std::cout << "  Found " << empty_matches.size() << " matches" << std::endl;

         // Case 5: Finding close matches in empty list
         std::cout << "\nFinding close matches in empty list:" << std::endl;
         std::vector<std::string> empty_list;
         auto no_matches = atom::utils::getCloseMatches("apple", empty_list);
         std::cout << "  Found " << no_matches.size() << " matches" << std::endl;

         // ===================================================
         // Example 9: Practical Application - Spell Checker
         // ===================================================
         printSection("9. Practical Application - Simple Spell Checker");

         // Define a dictionary of correctly spelled words
         std::vector<std::string> dictionary = {
             "algorithm", "application", "binary", "compiler", "computer",
             "database", "development", "encryption", "function", "hardware",
             "interface", "iteration", "keyboard", "language", "memory",
             "network", "operating", "processor", "programming", "recursive",
             "software", "storage", "structure", "system", "variable"
         };

         // Define some misspelled words to check
         std::vector<std::string> misspelled_words = {
             "algorthm", "aplicasion", "compiller", "developmint", "recursve"
         };

         std::cout << "Simple spell checker:" << std::endl;
         for (const auto& word : misspelled_words) {
             std::cout << "Checking \"" << word << "\":" << std::endl;
             auto suggestions = atom::utils::getCloseMatches(word, dictionary, 3, 0.6);

             std::cout << "  Did you mean: ";
             if (suggestions.empty()) {
                 std::cout << "No suggestions found.";
             } else {
                 for (size_t i = 0; i < suggestions.size(); ++i) {
                     std::cout << suggestions[i];
                     if (i < suggestions.size() - 1) {
                         std::cout << ", ";
                     }
                 }
             }
             std::cout << std::endl;
         }

         std::cout << "\nAll examples completed successfully!" << std::endl;

     } catch (const std::exception& e) {
         std::cerr << "ERROR: " << e.what() << std::endl;
         return 1;
     }

     return 0;
 }
