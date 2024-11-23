#include "atom/algorithm/algorithm.hpp"

#include <iostream>

int main() {
    // Example usage of KMP class
    {
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
    }

    // Example usage of BloomFilter class
    {
        // Create a BloomFilter object with 1000 bits and 3 hash functions
        atom::algorithm::BloomFilter<1000> bloomFilter(3);

        // Insert elements into the Bloom filter
        bloomFilter.insert("hello");
        bloomFilter.insert("world");

        // Check if elements might be present in the Bloom filter
        bool mightContainHello = bloomFilter.contains("hello");
        bool mightContainWorld = bloomFilter.contains("world");
        bool mightContainTest = bloomFilter.contains("test");

        // Print the results
        std::cout << "BloomFilter contains 'hello': " << mightContainHello
                  << std::endl;
        std::cout << "BloomFilter contains 'world': " << mightContainWorld
                  << std::endl;
        std::cout << "BloomFilter contains 'test': " << mightContainTest
                  << std::endl;
    }

    // Example usage of BoyerMoore class
    {
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
    }

    return 0;
}