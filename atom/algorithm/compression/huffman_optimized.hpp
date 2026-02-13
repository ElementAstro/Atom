/*
 * huffman_optimized.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-24

Description: Optimized Huffman encoding with parallel processing and SIMD

**************************************************/

#ifndef ATOM_ALGORITHM_COMPRESSION_HUFFMAN_OPTIMIZED_HPP
#define ATOM_ALGORITHM_COMPRESSION_HUFFMAN_OPTIMIZED_HPP

#include <memory>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "atom/algorithm/common/concepts.hpp"  // ByteLike
#include "huffman.hpp"

namespace atom::algorithm::huffman_optimized {

/**
 * @brief Parallel frequency counting using SIMD and multithreading
 *
 * @tparam T Byte-like type
 * @param data Input data
 * @param threadCount Number of threads to use (defaults to hardware
 * concurrency)
 * @return Frequency map of each byte
 */
template <ByteLike T>
std::unordered_map<T, size_t> parallelFrequencyCount(
    std::span<const T> data,
    size_t threadCount = std::thread::hardware_concurrency());

/**
 * @brief Builds a Huffman tree in parallel
 *
 * @param frequencies Map of byte frequencies
 * @return Shared pointer to the root of the Huffman tree
 */
std::shared_ptr<HuffmanNode> createTreeParallel(
    const std::unordered_map<unsigned char, size_t>& frequencies);

/**
 * @brief Compresses data using SIMD acceleration
 *
 * @param data Input data to compress
 * @param huffmanCodes Huffman codes for each byte
 * @return Compressed data as string
 */
std::string compressSimd(
    std::span<const unsigned char> data,
    const std::unordered_map<unsigned char, std::string>& huffmanCodes);

/**
 * @brief Compresses data using parallel processing
 *
 * @param data Input data to compress
 * @param huffmanCodes Huffman codes for each byte
 * @param threadCount Number of threads to use (defaults to hardware
 * concurrency)
 * @return Compressed data as string
 */
std::string compressParallel(
    std::span<const unsigned char> data,
    const std::unordered_map<unsigned char, std::string>& huffmanCodes,
    size_t threadCount = std::thread::hardware_concurrency());

/**
 * @brief Validates input data and Huffman codes
 *
 * @param data Input data to validate
 * @param huffmanCodes Huffman codes to validate
 */
void validateInput(
    std::span<const unsigned char> data,
    const std::unordered_map<unsigned char, std::string>& huffmanCodes);

/**
 * @brief Decompresses data using parallel processing
 *
 * @param compressedData Compressed data to decompress
 * @param root Root of the Huffman tree
 * @param threadCount Number of threads to use (defaults to hardware
 * concurrency)
 * @return Decompressed data as byte vector
 */
std::vector<unsigned char> decompressParallel(
    const std::string& compressedData, const HuffmanNode* root,
    size_t threadCount = std::thread::hardware_concurrency());

}  // namespace atom::algorithm::huffman_optimized

// Backward compatibility: allow unqualified huffman_optimized:: usage
namespace huffman_optimized = atom::algorithm::huffman_optimized;

#endif  // ATOM_ALGORITHM_COMPRESSION_HUFFMAN_OPTIMIZED_HPP
