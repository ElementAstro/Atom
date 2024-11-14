#include <iostream>
#include <unordered_map>
#include <vector>

#include "atom/algorithm/huffman.hpp"

using namespace atom::algorithm;

int main() {
    // Example data to compress
    std::string data = "this is an example for huffman encoding";
    std::vector<unsigned char> inputData(data.begin(), data.end());

    // Step 1: Calculate frequencies of each byte
    std::unordered_map<unsigned char, int> frequencies;
    for (unsigned char byte : inputData) {
        frequencies[byte]++;
    }

    // Step 2: Create Huffman Tree
    auto huffmanTreeRoot = createHuffmanTree(frequencies);

    // Step 3: Generate Huffman Codes
    std::unordered_map<unsigned char, std::string> huffmanCodes;
    generateHuffmanCodes(huffmanTreeRoot.get(), "", huffmanCodes);

    // Step 4: Compress Data
    std::string compressedData = compressData(inputData, huffmanCodes);
    std::cout << "Compressed Data: " << compressedData << std::endl;

    // Step 5: Decompress Data
    std::vector<unsigned char> decompressedData =
        decompressData(compressedData, huffmanTreeRoot.get());
    std::string decompressedString(decompressedData.begin(),
                                   decompressedData.end());
    std::cout << "Decompressed Data: " << decompressedString << std::endl;

    // Step 6: Serialize Huffman Tree
    std::string serializedTree = serializeTree(huffmanTreeRoot.get());
    std::cout << "Serialized Huffman Tree: " << serializedTree << std::endl;

    // Step 7: Deserialize Huffman Tree
    size_t index = 0;
    auto deserializedTreeRoot = deserializeTree(serializedTree, index);

    // Step 8: Visualize Huffman Tree
    std::cout << "Huffman Tree Structure:" << std::endl;
    visualizeHuffmanTree(huffmanTreeRoot.get());

    return 0;
}