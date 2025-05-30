/*
 * huffman.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-11-24

Description: Enhanced implementation of Huffman encoding

**************************************************/

#ifndef ATOM_ALGORITHM_HUFFMAN_HPP
#define ATOM_ALGORITHM_HUFFMAN_HPP

#include <concepts>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace atom::algorithm {

/**
 * @brief Exception class for Huffman encoding/decoding errors.
 */
class HuffmanException : public std::runtime_error {
public:
    explicit HuffmanException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Represents a node in the Huffman tree.
 *
 * This structure is used to construct the Huffman tree for encoding and
 * decoding data based on byte frequencies.
 */
struct HuffmanNode {
    unsigned char
        data;      /**< Byte stored in this node (used only in leaf nodes) */
    int frequency; /**< Frequency of the byte or sum of frequencies for internal
                      nodes */
    std::shared_ptr<HuffmanNode> left;  /**< Pointer to the left child node */
    std::shared_ptr<HuffmanNode> right; /**< Pointer to the right child node */

    /**
     * @brief Constructs a new Huffman Node.
     *
     * @param data Byte to store in the node.
     * @param frequency Frequency of the byte or combined frequency for a parent
     * node.
     */
    HuffmanNode(unsigned char data, int frequency);
};

/**
 * @brief Creates a Huffman tree based on the frequency of bytes.
 *
 * This function builds a Huffman tree using the frequencies of bytes in
 * the input data. It employs a priority queue to build the tree from the bottom
 * up by merging the two least frequent nodes until only one node remains, which
 * becomes the root.
 *
 * @param frequencies A map of bytes and their corresponding frequencies.
 * @return A unique pointer to the root of the Huffman tree.
 * @throws HuffmanException if the frequency map is empty.
 */
[[nodiscard]] auto createHuffmanTree(
    const std::unordered_map<unsigned char, int>& frequencies) noexcept(false)
    -> std::shared_ptr<HuffmanNode>;

/**
 * @brief Generates Huffman codes for each byte from the Huffman tree.
 *
 * This function recursively traverses the Huffman tree and assigns a binary
 * code to each byte. These codes are derived from the path taken to reach
 * the byte: left child gives '0' and right child gives '1'.
 *
 * @param root Pointer to the root node of the Huffman tree.
 * @param code Current Huffman code generated during the traversal.
 * @param huffmanCodes A reference to a map where the byte and its
 * corresponding Huffman code will be stored.
 * @throws HuffmanException if the root is null.
 */
void generateHuffmanCodes(const HuffmanNode* root, const std::string& code,
                          std::unordered_map<unsigned char, std::string>&
                              huffmanCodes) noexcept(false);

/**
 * @brief Compresses data using Huffman codes.
 *
 * This function converts a vector of bytes into a string of binary codes based
 * on the Huffman codes provided. Each byte in the input data is replaced
 * by its corresponding Huffman code.
 *
 * @param data The original data to compress.
 * @param huffmanCodes The map of bytes to their corresponding Huffman codes.
 * @return A string representing the compressed data.
 * @throws HuffmanException if a byte in data does not have a corresponding
 * Huffman code.
 */
[[nodiscard]] auto compressData(
    const std::vector<unsigned char>& data,
    const std::unordered_map<unsigned char, std::string>&
        huffmanCodes) noexcept(false) -> std::string;

/**
 * @brief Decompresses Huffman encoded data back to its original form.
 *
 * This function decodes a string of binary codes back into the original data
 * using the provided Huffman tree. It traverses the Huffman tree from the root
 * to the leaf nodes based on the binary string, reconstructing the original
 * data.
 *
 * @param compressedData The Huffman encoded data.
 * @param root Pointer to the root of the Huffman tree.
 * @return The original decompressed data as a vector of bytes.
 * @throws HuffmanException if the compressed data is invalid or the tree is
 * null.
 */
[[nodiscard]] auto decompressData(const std::string& compressedData,
                                  const HuffmanNode* root) noexcept(false)
    -> std::vector<unsigned char>;

/**
 * @brief Serializes the Huffman tree into a binary string.
 *
 * This function converts the Huffman tree into a binary string representation
 * which can be stored or transmitted alongside the compressed data.
 *
 * @param root Pointer to the root node of the Huffman tree.
 * @return A binary string representing the serialized Huffman tree.
 */
[[nodiscard]] auto serializeTree(const HuffmanNode* root) -> std::string;

/**
 * @brief Deserializes the binary string back into a Huffman tree.
 *
 * This function reconstructs the Huffman tree from its binary string
 * representation.
 *
 * @param serializedTree The binary string representing the serialized Huffman
 * tree.
 * @param index Reference to the current index in the binary string (used during
 * recursion).
 * @return A unique pointer to the root of the reconstructed Huffman tree.
 * @throws HuffmanException if the serialized tree format is invalid.
 */
[[nodiscard]] auto deserializeTree(const std::string& serializedTree,
                                   size_t& index)
    -> std::shared_ptr<HuffmanNode>;

/**
 * @brief Visualizes the Huffman tree structure.
 *
 * This function prints the Huffman tree in a human-readable format for
 * debugging and analysis purposes.
 *
 * @param root Pointer to the root node of the Huffman tree.
 * @param indent Current indentation level (used during recursion).
 */
void visualizeHuffmanTree(const HuffmanNode* root,
                          const std::string& indent = "");

}  // namespace atom::algorithm

namespace huffman_optimized {
/**
 * @concept ByteLike
 * @brief Type constraint for byte-like types
 * @tparam T Type to check
 */
template <typename T>
concept ByteLike = std::integral<T> && sizeof(T) == 1;

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
std::shared_ptr<atom::algorithm::HuffmanNode> createTreeParallel(
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
    const std::string& compressedData, const atom::algorithm::HuffmanNode* root,
    size_t threadCount = std::thread::hardware_concurrency());

}  // namespace huffman_optimized

#endif  // ATOM_ALGORITHM_HUFFMAN_HPP