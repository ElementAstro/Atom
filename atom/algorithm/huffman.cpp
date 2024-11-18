/*
 * huffman.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-24

Description: Enhanced implementation of Huffman encoding

**************************************************/

#include "huffman.hpp"
#include <bitset>
#include <functional>
#include <iostream>
#include <queue>
#include <sstream>

#ifdef ATOM_USE_BOOST
#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>
#endif

namespace atom::algorithm {

/* ------------------------ HuffmanNode Implementation ------------------------
 */

HuffmanNode::HuffmanNode(unsigned char data, int frequency)
    : data(data), frequency(frequency), left(nullptr), right(nullptr) {}

/* ------------------------ Priority Queue Comparator ------------------------
 */

struct CompareNode {
    bool operator()(const std::shared_ptr<HuffmanNode>& a,
                    const std::shared_ptr<HuffmanNode>& b) const {
#ifdef ATOM_USE_BOOST
        return a->frequency > b->frequency;
#else
        return a->frequency > b->frequency;
#endif
    }
};

/* ------------------------ createHuffmanTree ------------------------ */

auto createHuffmanTree(const std::unordered_map<unsigned char, int>&
                           frequencies) -> std::shared_ptr<HuffmanNode> {
#ifdef ATOM_USE_BOOST
    typedef boost::shared_ptr<HuffmanNode> HuffmanNodePtr;
#else
    typedef std::shared_ptr<HuffmanNode> HuffmanNodePtr;
#endif

    if (frequencies.empty()) {
#ifdef ATOM_USE_BOOST
        throw HuffmanException(boost::str(boost::format(
            "Frequency map is empty. Cannot create Huffman Tree.")));
#else
        throw HuffmanException(
            "Frequency map is empty. Cannot create Huffman Tree.");
#endif
    }

    std::priority_queue<HuffmanNodePtr, std::vector<HuffmanNodePtr>,
                        CompareNode>
        minHeap;

    // Initialize heap with leaf nodes
    for (const auto& [data, freq] : frequencies) {
#ifdef ATOM_USE_BOOST
        minHeap.push(boost::make_shared<HuffmanNode>(data, freq));
#else
        minHeap.push(std::make_shared<HuffmanNode>(data, freq));
#endif
    }

    // Edge case: Only one unique byte
    if (minHeap.size() == 1) {
        auto soleNode = minHeap.top();
        minHeap.pop();
#ifdef ATOM_USE_BOOST
        auto parent =
            boost::make_shared<HuffmanNode>('\0', soleNode->frequency);
#else
        auto parent = std::make_shared<HuffmanNode>('\0', soleNode->frequency);
#endif
        parent->left = soleNode;
        parent->right = nullptr;
        minHeap.push(parent);
    }

    // Build Huffman Tree
    while (minHeap.size() > 1) {
        auto left = minHeap.top();
        minHeap.pop();
        auto right = minHeap.top();
        minHeap.pop();

#ifdef ATOM_USE_BOOST
        auto merged = boost::make_shared<HuffmanNode>(
            '\0', left->frequency + right->frequency);
#else
        auto merged = std::make_shared<HuffmanNode>(
            '\0', left->frequency + right->frequency);
#endif
        merged->left = left;
        merged->right = right;

        minHeap.push(merged);
    }

    return minHeap.empty() ? nullptr : minHeap.top();
}

/* ------------------------ generateHuffmanCodes ------------------------ */

void generateHuffmanCodes(
    const HuffmanNode* root, const std::string& code,
    std::unordered_map<unsigned char, std::string>& huffmanCodes) {
    if (root == nullptr) {
#ifdef ATOM_USE_BOOST
        throw HuffmanException(boost::str(
            boost::format("Cannot generate Huffman codes from a null tree.")));
#else
        throw HuffmanException(
            "Cannot generate Huffman codes from a null tree.");
#endif
    }

    if (!root->left && !root->right) {
        if (code.empty()) {
            // Edge case: Only one unique byte
            huffmanCodes[root->data] = "0";
        } else {
            huffmanCodes[root->data] = code;
        }
        return;
    }

    if (root->left) {
        generateHuffmanCodes(root->left.get(), code + "0", huffmanCodes);
    }

    if (root->right) {
        generateHuffmanCodes(root->right.get(), code + "1", huffmanCodes);
    }
}

/* ------------------------ compressData ------------------------ */

auto compressData(const std::vector<unsigned char>& data,
                  const std::unordered_map<unsigned char, std::string>&
                      huffmanCodes) -> std::string {
    std::string compressedData;
    compressedData.reserve(data.size() * 2);  // Approximate reserve

    for (unsigned char byte : data) {
        auto it = huffmanCodes.find(byte);
        if (it == huffmanCodes.end()) {
#ifdef ATOM_USE_BOOST
            throw HuffmanException(boost::str(
                boost::format(
                    "Byte '%1%' does not have a corresponding Huffman code.") %
                static_cast<int>(byte)));
#else
            throw HuffmanException(
                std::string("Byte '") + std::to_string(static_cast<int>(byte)) +
                "' does not have a corresponding Huffman code.");
#endif
        }
        compressedData += it->second;
    }

    return compressedData;
}

/* ------------------------ decompressData ------------------------ */

auto decompressData(const std::string& compressedData,
                    const HuffmanNode* root) -> std::vector<unsigned char> {
    if (!root) {
#ifdef ATOM_USE_BOOST
        throw HuffmanException(boost::str(
            boost::format("Huffman tree is null. Cannot decompress data.")));
#else
        throw HuffmanException("Huffman tree is null. Cannot decompress data.");
#endif
    }

    std::vector<unsigned char> decompressedData;
    const HuffmanNode* current = root;

    for (char bit : compressedData) {
        if (bit == '0') {
            if (current->left) {
                current = current->left.get();
            } else {
#ifdef ATOM_USE_BOOST
                throw HuffmanException(boost::str(
                    boost::format("Invalid compressed data. Traversed to a "
                                  "null left child.")));
#else
                throw HuffmanException(
                    "Invalid compressed data. Traversed to a null left child.");
#endif
            }
        } else if (bit == '1') {
            if (current->right) {
                current = current->right.get();
            } else {
#ifdef ATOM_USE_BOOST
                throw HuffmanException(boost::str(
                    boost::format("Invalid compressed data. Traversed to a "
                                  "null right child.")));
#else
                throw HuffmanException(
                    "Invalid compressed data. Traversed to a null right "
                    "child.");
#endif
            }
        } else {
#ifdef ATOM_USE_BOOST
            throw HuffmanException(
                boost::str(boost::format("Invalid bit in compressed data. Only "
                                         "'0' and '1' are allowed.")));
#else
            throw HuffmanException(
                "Invalid bit in compressed data. Only '0' and '1' are "
                "allowed.");
#endif
        }

        // If leaf node, append the data and reset to root
        if (!current->left && !current->right) {
            decompressedData.push_back(current->data);
            current = root;
        }
    }

    // Edge case: compressed data does not end at a leaf node
    if (current != root) {
#ifdef ATOM_USE_BOOST
        throw HuffmanException(boost::str(boost::format(
            "Incomplete compressed data. Did not end at a leaf node.")));
#else
        throw HuffmanException(
            "Incomplete compressed data. Did not end at a leaf node.");
#endif
    }

    return decompressedData;
}

/* ------------------------ serializeTree ------------------------ */

auto serializeTree(const HuffmanNode* root) -> std::string {
    if (root == nullptr) {
#ifdef ATOM_USE_BOOST
        throw HuffmanException(
            boost::str(boost::format("Cannot serialize a null Huffman tree.")));
#else
        throw HuffmanException("Cannot serialize a null Huffman tree.");
#endif
    }

    std::string serialized;
    std::function<void(const HuffmanNode*)> serializeHelper =
        [&](const HuffmanNode* node) {
            if (!node) {
                serialized += '1';  // Marker for null
                return;
            }

            if (!node->left && !node->right) {
                serialized += '0';  // Marker for leaf
                serialized += node->data;
            } else {
                serialized += '2';  // Marker for internal node
                serializeHelper(node->left.get());
                serializeHelper(node->right.get());
            }
        };

    serializeHelper(root);
    return serialized;
}

/* ------------------------ deserializeTree ------------------------ */

auto deserializeTree(const std::string& serializedTree,
                     size_t& index) -> std::shared_ptr<HuffmanNode> {
#ifdef ATOM_USE_BOOST
    typedef boost::shared_ptr<HuffmanNode> HuffmanNodePtr;
#else
    typedef std::shared_ptr<HuffmanNode> HuffmanNodePtr;
#endif

    if (index >= serializedTree.size()) {
#ifdef ATOM_USE_BOOST
        throw HuffmanException(boost::str(boost::format(
            "Invalid serialized tree format: Unexpected end of data.")));
#else
        throw HuffmanException(
            "Invalid serialized tree format: Unexpected end of data.");
#endif
    }

    char marker = serializedTree[index++];
    if (marker == '1') {
        return nullptr;
    } else if (marker == '0') {
        if (index >= serializedTree.size()) {
#ifdef ATOM_USE_BOOST
            throw HuffmanException(
                boost::str(boost::format("Invalid serialized tree format: "
                                         "Missing byte data for leaf node.")));
#else
            throw HuffmanException(
                "Invalid serialized tree format: Missing byte data for leaf "
                "node.");
#endif
        }
        unsigned char data = serializedTree[index++];
#ifdef ATOM_USE_BOOST
        return boost::make_shared<HuffmanNode>(
            data, 0);  // Frequency is not needed for decompression
#else
        return std::make_shared<HuffmanNode>(
            data, 0);  // Frequency is not needed for decompression
#endif
    } else if (marker == '2') {
#ifdef ATOM_USE_BOOST
        auto node = boost::make_shared<HuffmanNode>('\0', 0);
#else
        auto node = std::make_shared<HuffmanNode>('\0', 0);
#endif
        node->left = deserializeTree(serializedTree, index);
        node->right = deserializeTree(serializedTree, index);
        return node;
    } else {
#ifdef ATOM_USE_BOOST
        throw HuffmanException(boost::str(
            boost::format(
                "Invalid serialized tree format: Unknown marker '%1%'.") %
            marker));
#else
        throw HuffmanException(
            "Invalid serialized tree format: Unknown marker encountered.");
#endif
    }
}

/* ------------------------ visualizeHuffmanTree ------------------------ */

void visualizeHuffmanTree(const HuffmanNode* root, const std::string& indent) {
    if (!root) {
        std::cout << indent << "nullptr\n";
        return;
    }

    if (!root->left && !root->right) {
        std::cout << indent << "Leaf: '" << root->data << "'\n";
    } else {
        std::cout << indent << "Internal Node (Frequency: " << root->frequency
                  << ")\n";
    }

    if (root->left) {
        std::cout << indent << " Left:\n";
        visualizeHuffmanTree(root->left.get(), indent + "  ");
    } else {
        std::cout << indent << " Left: nullptr\n";
    }

    if (root->right) {
        std::cout << indent << " Right:\n";
        visualizeHuffmanTree(root->right.get(), indent + "  ");
    } else {
        std::cout << indent << " Right: nullptr\n";
    }
}

}  // namespace atom::algorithm
