/*
 * huffman_optimized.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-24

Description: Optimized Huffman encoding with parallel processing and SIMD

**************************************************/

#include "huffman_optimized.hpp"

#include <future>
#include <span>
#include <thread>
#include <unordered_map>
#include <vector>

namespace atom::algorithm::huffman_optimized {

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

std::shared_ptr<HuffmanNode> createTreeParallel(
    const std::unordered_map<unsigned char, size_t>& frequencies) {
    std::unordered_map<unsigned char, int> freq32;
    for (const auto& [k, v] : frequencies) {
        freq32[k] = static_cast<int>(v);
    }
    return createHuffmanTree(freq32);
}

/* ------------------------ compressSimd ------------------------ */

// Keep compressSimd as is, it compresses a chunk and returns a string
std::string compressSimd(
    std::span<const unsigned char> data,
    const std::unordered_map<unsigned char, std::string>& huffmanCodes) {
    std::string compressed;
    compressed.reserve(data.size() * 2);  // 预估大小

    // 未来可添加SIMD优化，当前为基本串行实现
    for (unsigned char b : data) {
        auto it = huffmanCodes.find(b);
        if (it == huffmanCodes.end()) {
            throw HuffmanException(
                "Byte not found in Huffman codes table");
        }
        compressed += it->second;
    }

    return compressed;
}

/* ------------------------ compressParallel ------------------------ */

// Optimized parallel compression with efficient result combination
std::string compressParallel(
    std::span<const unsigned char> data,
    const std::unordered_map<unsigned char, std::string>& huffmanCodes,
    size_t threadCount) {
    // 数据量小或单线程时直接使用SIMD版本
    if (data.size() < 1024 * 32 || threadCount <= 1) {
        return compressSimd(data, huffmanCodes);
    }

    std::vector<std::future<std::string>> futures;
    size_t block_size = data.size() / threadCount;

    for (size_t t = 0; t < threadCount; ++t) {
        size_t begin = t * block_size;
        size_t end =
            (t == threadCount - 1) ? data.size() : (t + 1) * block_size;

        futures.push_back(std::async(std::launch::async, [&, begin, end]() {
            std::span<const unsigned char> chunk(data.begin() + begin,
                                                 data.begin() + end);
            return compressSimd(chunk, huffmanCodes);
        }));
    }

    // Collect results and calculate total size
    std::vector<std::string> results;
    results.reserve(futures.size());  // Reserve space for results
    size_t total_size = 0;
    for (auto& future : futures) {
        results.push_back(future.get());
        total_size += results.back().size();
    }

    // Concatenate results into a single string efficiently
    std::string out;
    out.reserve(total_size);  // Reserve memory to avoid reallocations
    for (const auto& s : results) {
        out.append(s);
    }

    return out;
}

/* ------------------------ validateInput ------------------------ */

void validateInput(
    std::span<const unsigned char> data,
    const std::unordered_map<unsigned char, std::string>& huffmanCodes) {
    if (data.empty()) {
        throw HuffmanException("Input data is empty");
    }
    if (huffmanCodes.empty()) {
        throw HuffmanException("Huffman code map is empty");
    }

    if (!huffmanCodes.contains(data[0])) {
        throw HuffmanException(
            "Data contains byte not in huffmanCodes");
    }
}

/* ------------------------ decompressParallel ------------------------ */

std::vector<unsigned char> decompressParallel(
    const std::string& compressedData, const HuffmanNode* root,
    [[maybe_unused]] size_t threadCount) {
    if (compressedData.empty()) {
        return {};
    }

    if (!root) {
        throw HuffmanException(
            "Huffman tree is null. Cannot decompress data.");
    }

    // 注意：由于Huffman解压缩需要从树根开始，并且状态依赖于之前的位，
    // 这里仍然使用串行版本。未来可以研究更复杂的并行解压缩算法。
    return decompressData(compressedData, root);
}

}  // namespace atom::algorithm::huffman_optimized
