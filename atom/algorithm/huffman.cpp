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

#include <functional>
#include <iostream>
#include <queue>
#include <span>
#include <thread>
#include <unordered_map>

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

auto createHuffmanTree(
    const std::unordered_map<unsigned char, int>& frequencies) noexcept(false)
    -> std::shared_ptr<HuffmanNode> {
    if (frequencies.empty()) {
        throw HuffmanException(
            "Frequency map is empty. Cannot create Huffman Tree.");
    }

    std::priority_queue<std::shared_ptr<HuffmanNode>,
                        std::vector<std::shared_ptr<HuffmanNode>>, CompareNode>
        minHeap;

    // Initialize heap with leaf nodes
    for (const auto& [data, freq] : frequencies) {
        minHeap.push(std::make_shared<HuffmanNode>(data, freq));
    }

    // Edge case: Only one unique byte
    if (minHeap.size() == 1) {
        auto soleNode = minHeap.top();
        minHeap.pop();
        auto parent = std::make_shared<HuffmanNode>('\0', soleNode->frequency);
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

        auto merged = std::make_shared<HuffmanNode>(
            '\0', left->frequency + right->frequency);
        merged->left = left;
        merged->right = right;

        minHeap.push(merged);
    }

    return minHeap.empty() ? nullptr : minHeap.top();
}

/* ------------------------ generateHuffmanCodes ------------------------ */

void generateHuffmanCodes(const HuffmanNode* root, const std::string& code,
                          std::unordered_map<unsigned char, std::string>&
                              huffmanCodes) noexcept(false) {
    if (root == nullptr) {
        throw HuffmanException(
            "Cannot generate Huffman codes from a null tree.");
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
                      huffmanCodes) noexcept(false) -> std::string {
    std::string compressedData;
    compressedData.reserve(data.size() * 2);  // Approximate reserve

    for (unsigned char byte : data) {
        auto it = huffmanCodes.find(byte);
        if (it == huffmanCodes.end()) {
            throw HuffmanException(
                std::string("Byte '") + std::to_string(static_cast<int>(byte)) +
                "' does not have a corresponding Huffman code.");
        }
        compressedData += it->second;
    }

    return compressedData;
}

/* ------------------------ decompressData ------------------------ */

auto decompressData(const std::string& compressedData,
                    const HuffmanNode* root) noexcept(false)
    -> std::vector<unsigned char> {
    if (!root) {
        throw HuffmanException("Huffman tree is null. Cannot decompress data.");
    }

    std::vector<unsigned char> decompressedData;
    const HuffmanNode* current = root;

    for (char bit : compressedData) {
        if (bit == '0') {
            if (current->left) {
                current = current->left.get();
            } else {
                throw HuffmanException(
                    "Invalid compressed data. Traversed to a null left child.");
            }
        } else if (bit == '1') {
            if (current->right) {
                current = current->right.get();
            } else {
                throw HuffmanException(
                    "Invalid compressed data. Traversed to a null right "
                    "child.");
            }
        } else {
            throw HuffmanException(
                "Invalid bit in compressed data. Only '0' and '1' are "
                "allowed.");
        }

        // If leaf node, append the data and reset to root
        if (!current->left && !current->right) {
            decompressedData.push_back(current->data);
            current = root;
        }
    }

    // Edge case: compressed data does not end at a leaf node
    if (current != root) {
        throw HuffmanException(
            "Incomplete compressed data. Did not end at a leaf node.");
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

auto deserializeTree(const std::string& serializedTree, size_t& index)
    -> std::shared_ptr<HuffmanNode> {
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

namespace huffman_optimized {

/* ------------------------ parallelFrequencyCount (unsigned char 特化)
 * ------------------------ */

template <>
std::unordered_map<unsigned char, size_t> parallelFrequencyCount(
    std::span<const unsigned char> data, size_t threadCount) {
    if (data.empty()) {
        return {};
    }

    // 单线程情况下直接串行处理
    if (threadCount <= 1) {
        std::unordered_map<unsigned char, size_t> freq;
        for (const unsigned char& byte : data) {
            freq[byte]++;
        }
        return freq;
    }

    std::vector<std::unordered_map<unsigned char, size_t>> localMaps(
        threadCount);
    std::vector<std::thread> threads;
    size_t block = data.size() / threadCount;

    for (size_t t = 0; t < threadCount; ++t) {
        size_t begin = t * block;
        size_t end = (t == threadCount - 1) ? data.size() : (t + 1) * block;
        threads.emplace_back([&, begin, end, t] {
            for (size_t i = begin; i < end; ++i) {
                localMaps[t][data[i]]++;
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    std::unordered_map<unsigned char, size_t> result;
    for (const auto& m : localMaps) {
        for (const auto& [k, v] : m) {
            result[k] += v;
        }
    }
    return result;
}

/* ------------------------ createTreeParallel ------------------------ */

std::shared_ptr<atom::algorithm::HuffmanNode> createTreeParallel(
    const std::unordered_map<unsigned char, size_t>& frequencies) {
    // 转换为createHuffmanTree所期望的类型
    std::unordered_map<unsigned char, int> freq32;
    for (const auto& [k, v] : frequencies) {
        freq32[k] = static_cast<int>(v);
    }
    return atom::algorithm::createHuffmanTree(freq32);
}

/* ------------------------ compressSimd ------------------------ */

std::string compressSimd(
    std::span<const unsigned char> data,
    const std::unordered_map<unsigned char, std::string>& huffmanCodes) {
    std::string compressed;
    compressed.reserve(data.size() * 2);  // 预估大小

    // 未来可添加SIMD优化，当前为基本串行实现
    for (unsigned char b : data) {
        auto it = huffmanCodes.find(b);
        if (it == huffmanCodes.end()) {
            throw atom::algorithm::HuffmanException(
                "Byte not found in Huffman codes table");
        }
        compressed += it->second;
    }

    return compressed;
}

/* ------------------------ compressParallel ------------------------ */

std::string compressParallel(
    std::span<const unsigned char> data,
    const std::unordered_map<unsigned char, std::string>& huffmanCodes,
    size_t threadCount) {
    // 数据量小或单线程时直接使用SIMD版本
    if (data.size() < 1024 * 32 || threadCount <= 1) {
        return compressSimd(data, huffmanCodes);
    }

    std::vector<std::string> results(threadCount);
    std::vector<std::thread> threads;
    size_t block = data.size() / threadCount;

    for (size_t t = 0; t < threadCount; ++t) {
        size_t begin = t * block;
        size_t end = (t == threadCount - 1) ? data.size() : (t + 1) * block;
        threads.emplace_back([&, begin, end, t] {
            results[t] =
                compressSimd(std::span<const unsigned char>(
                                 data.begin() + begin, data.begin() + end),
                             huffmanCodes);
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // 计算结果大小并合并
    size_t total_size = 0;
    for (const auto& s : results) {
        total_size += s.size();
    }

    std::string out;
    out.reserve(total_size);
    for (auto& s : results) {
        out += s;
    }
    return out;
}

/* ------------------------ validateInput ------------------------ */

void validateInput(
    std::span<const unsigned char> data,
    const std::unordered_map<unsigned char, std::string>& huffmanCodes) {
    if (data.empty()) {
        throw atom::algorithm::HuffmanException("Input data is empty");
    }
    if (huffmanCodes.empty()) {
        throw atom::algorithm::HuffmanException("Huffman code map is empty");
    }

    // 可以选择性执行完整验证，这里仅检查首个字节
    if (!huffmanCodes.contains(data[0])) {
        throw atom::algorithm::HuffmanException(
            "Data contains byte not in huffmanCodes");
    }
}

/* ------------------------ decompressParallel ------------------------ */

std::vector<unsigned char> decompressParallel(
    const std::string& compressedData, const atom::algorithm::HuffmanNode* root,
    size_t threadCount) {
    if (compressedData.empty()) {
        return {};
    }

    if (!root) {
        throw atom::algorithm::HuffmanException(
            "Huffman tree is null. Cannot decompress data.");
    }

    // 注意：由于Huffman解压缩需要从树根开始，并且状态依赖于之前的位，
    // 这里仍然使用串行版本。未来可以研究更复杂的并行解压缩算法。
    return atom::algorithm::decompressData(compressedData, root);
}

}  // namespace huffman_optimized
