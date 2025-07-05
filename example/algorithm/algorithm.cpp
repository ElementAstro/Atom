#include "atom/algorithm/algorithm.hpp"

#include <iostream>
#include <string>

int main() {
    // Example usage of KMP class
    {
        std::cout << "=== KMP Algorithm Examples ===" << std::endl;

        // Create a KMP object with a pattern
        atom::algorithm::KMP kmp("abc");

        // Search for the pattern in a given text
        std::vector<int> positions = kmp.search("abcabcabc");

        // Print the positions where the pattern starts in the text
        std::cout << "KMP search positions: ";
        for (int pos : positions) {
            std::cout << pos << " ";
        }
        std::cout << std::endl;

        // Set a new pattern
        kmp.setPattern("bca");

        // Search for the new pattern in the text
        positions = kmp.search("abcabcabc");

        // Print the positions where the new pattern starts in the text
        std::cout << "KMP search positions with new pattern: ";
        for (int pos : positions) {
            std::cout << pos << " ";
        }
        std::cout << std::endl;

        // Try parallel search with a larger text
        std::string largeText =
            "abcabcabcabcabcabcabcabcabcabcabcabcabcabcabcabc";
        std::cout << "KMP parallel search positions: ";
        positions = kmp.searchParallel(largeText, 8);
        for (int pos : positions) {
            std::cout << pos << " ";
        }
        std::cout << std::endl;
    }

    // Example usage of BloomFilter class
    {
        std::cout << "\n=== Bloom Filter Examples ===" << std::endl;

        // Create a BloomFilter object with 1000 bits and 3 hash functions
        atom::algorithm::BloomFilter<1000> bloomFilter(3);

        // Insert elements into the Bloom filter
        bloomFilter.insert("hello");
        bloomFilter.insert("world");
        bloomFilter.insert("example");
        bloomFilter.insert("bloom");
        bloomFilter.insert("filter");

        // Check if elements might be present in the Bloom filter
        bool mightContainHello = bloomFilter.contains("hello");
        bool mightContainWorld = bloomFilter.contains("world");
        bool mightContainTest = bloomFilter.contains("test");

        // Print the results
        std::cout << "BloomFilter contains 'hello': " << std::boolalpha
                  << mightContainHello << std::endl;
        std::cout << "BloomFilter contains 'world': " << mightContainWorld
                  << std::endl;
        std::cout << "BloomFilter contains 'test': " << mightContainTest
                  << std::endl;

        // Display additional BloomFilter information
        std::cout << "Number of elements in the filter: "
                  << bloomFilter.elementCount() << std::endl;
        std::cout << "Estimated false positive probability: "
                  << bloomFilter.falsePositiveProbability() << std::endl;

        // Test clear functionality
        std::cout << "Clearing the Bloom filter..." << std::endl;
        bloomFilter.clear();
        std::cout << "Filter now contains 'hello': "
                  << bloomFilter.contains("hello") << std::endl;
        std::cout << "Element count after clear: " << bloomFilter.elementCount()
                  << std::endl;
    }

    // Example usage of BoyerMoore class
    {
        std::cout << "\n=== Boyer-Moore Algorithm Examples ===" << std::endl;

        // Create a BoyerMoore object with a pattern
        atom::algorithm::BoyerMoore boyerMoore("abc");

        // Search for the pattern in a given text
        std::vector<int> positions = boyerMoore.search("abcabcabc");

        // Print the positions where the pattern starts in the text
        std::cout << "BoyerMoore search positions: ";
        for (int pos : positions) {
            std::cout << pos << " ";
        }
        std::cout << std::endl;

        // Set a new pattern
        boyerMoore.setPattern("bca");

        // Search for the new pattern in the text
        positions = boyerMoore.search("abcabcabc");

        // Print the positions where the new pattern starts in the text
        std::cout << "BoyerMoore search positions with new pattern: ";
        for (int pos : positions) {
            std::cout << pos << " ";
        }
        std::cout << std::endl;

        // Try optimized search
        std::string largeText = "abcabcabcabcabcabcbcabcabcabcbcabcabc";
        std::cout << "BoyerMoore optimized search positions: ";
        positions = boyerMoore.searchOptimized(largeText);
        for (int pos : positions) {
            std::cout << pos << " ";
        }
        std::cout << std::endl;
    }

    // Example using custom type with BloomFilter
    {
        std::cout << "\n=== Custom Type BloomFilter Example ===" << std::endl;

        struct CustomHasher {
            std::size_t operator()(int value) const {
                return std::hash<int>{}(value);
            }
        };

        // Create a BloomFilter for integers using a custom hash function
        atom::algorithm::BloomFilter<500, int, CustomHasher> intFilter(2);

        // Insert some integers
        for (int i = 0; i < 10; ++i) {
            intFilter.insert(i * 10);
        }

        // Check if values might be present
        std::cout << "IntFilter contains 30: " << std::boolalpha
                  << intFilter.contains(30) << std::endl;
        std::cout << "IntFilter contains 31: " << intFilter.contains(31)
                  << std::endl;
        std::cout << "Element count: " << intFilter.elementCount() << std::endl;
    }

    return 0;
}
