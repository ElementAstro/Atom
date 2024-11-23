#include "huffman.hpp"
#include <iostream>
#include <unordered_map>
#include <vector>

int main() {
    // Example data
    std::vector<unsigned char> data = {'a', 'b', 'a', 'c', 'a', 'b',
                                       'a', 'd', 'e', 'e', 'e', 'e'};

    // Step 1: Calculate frequencies of each byte
    std::unordered_map<unsigned char, int> frequencies;
    for (unsigned char byte : data) {
        frequencies[byte]++;
    }

    // Step 2: Create Huffman tree
    auto huffmanTreeRoot = atom::algorithm::createHuffmanTree(frequencies);
    if (!huffmanTreeRoot) {
        std::cerr << "Failed to create Huffman tree." << std::endl;
        return 1;
    }

    // Step 3: Generate Huffman codes
    std::unordered_map<unsigned char, std::string> huffmanCodes;
    atom::algorithm::generateHuffmanCodes(huffmanTreeRoot.get(), "",
                                          huffmanCodes);

    // Print Huffman codes
    std::cout << "Huffman Codes:" << std::endl;
    for (const auto& [byte, code] : huffmanCodes) {
        std::cout << byte << ": " << code << std::endl;
    }

    // Step 4: Compress data
    std::string compressedData =
        atom::algorithm::compressData(data, huffmanCodes);
    std::cout << "\nCompressed Data: " << compressedData << std::endl;

    // Step 5: Decompress data
    std::vector<unsigned char> decompressedData =
        atom::algorithm::decompressData(compressedData, huffmanTreeRoot.get());
    std::cout << "\nDecompressed Data: ";
    for (unsigned char byte : decompressedData) {
        std::cout << byte << " ";
    }
    std::cout << std::endl;

    // Step 6: Serialize Huffman tree
    std::string serializedTree =
        atom::algorithm::serializeTree(huffmanTreeRoot.get());
    std::cout << "\nSerialized Huffman Tree: " << serializedTree << std::endl;

    // Step 7: Deserialize Huffman tree
    size_t index = 0;
    auto deserializedTreeRoot =
        atom::algorithm::deserializeTree(serializedTree, index);
    if (!deserializedTreeRoot) {
        std::cerr << "Failed to deserialize Huffman tree." << std::endl;
        return 1;
    }

    // Step 8: Visualize Huffman tree
    std::cout << "\nHuffman Tree Structure:" << std::endl;
    atom::algorithm::visualizeHuffmanTree(huffmanTreeRoot.get());

    return 0;
}