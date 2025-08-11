#include "atom/algorithm/huffman.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(huffman, m) {
    m.doc() = R"pbdoc(
        Huffman Encoding and Compression
        -------------------------------

        This module provides functions for compressing and decompressing data
        using Huffman encoding, an efficient variable-length prefix coding
        algorithm.

        **Basic Usage:**

        ```python
        from atom.algorithm.huffman import compress, decompress

        # Compress some data
        data = b"This is an example string with repeating characters"
        compressed_data, serialized_tree = compress(data)

        # Print compression statistics
        print(f"Original size: {len(data)} bytes")
        print(f"Compressed size: {len(compressed_data) // 8} bytes")
        print(f"Compression ratio: {len(compressed_data) / (len(data) * 8):.2%}")

        # Decompress the data
        decompressed_data = decompress(compressed_data, serialized_tree)

        # Verify the data matches
        assert data == decompressed_data
        ```

        **Convenience Functions:**

        For simpler usage with built-in serialization:

        ```python
        from atom.algorithm.huffman import encode, decode

        compressed = encode(b"Hello, world!")
        original = decode(compressed)
        ```
    )pbdoc";

    // Register exception translation
    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p)
                std::rethrow_exception(p);
        } catch (const atom::algorithm::HuffmanException& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::runtime_error& e) {
            PyErr_SetString(PyExc_RuntimeError, e.what());
        } catch (const std::invalid_argument& e) {
            PyErr_SetString(PyExc_ValueError, e.what());
        } catch (const std::exception& e) {
            PyErr_SetString(PyExc_Exception, e.what());
        }
    });

    // Define the HuffmanNode class
    py::class_<atom::algorithm::HuffmanNode,
               std::shared_ptr<atom::algorithm::HuffmanNode>>(
        m, "HuffmanNode", "A node in a Huffman tree")
        .def(py::init<unsigned char, int>(), py::arg("data"),
             py::arg("frequency"),
             "Create a new Huffman node with the given data byte and frequency")
        .def_readonly("data", &atom::algorithm::HuffmanNode::data,
                      "The byte value stored in this node")
        .def_readonly("frequency", &atom::algorithm::HuffmanNode::frequency,
                      "The frequency of this byte or sum of frequencies for "
                      "internal nodes")
        .def_property_readonly(
            "left",
            [](const atom::algorithm::HuffmanNode& node) { return node.left; },
            "The left child node")
        .def_property_readonly(
            "right",
            [](const atom::algorithm::HuffmanNode& node) { return node.right; },
            "The right child node")
        .def(
            "is_leaf",
            [](const atom::algorithm::HuffmanNode& node) {
                return !node.left && !node.right;
            },
            "Check if this node is a leaf node (has no children)");

    // Core functions - low-level API
    m.def("create_huffman_tree", &atom::algorithm::createHuffmanTree,
          py::arg("frequencies"),
          R"pbdoc(
          Create a Huffman tree from a frequency map.

          Args:
              frequencies: A dictionary mapping bytes to their frequencies

          Returns:
              The root node of the Huffman tree

          Raises:
              RuntimeError: If the frequency map is empty
          )pbdoc");

    m.def(
        "generate_huffman_codes",
        [](const atom::algorithm::HuffmanNode* root) {
            std::unordered_map<unsigned char, std::string> huffman_codes;
            atom::algorithm::generateHuffmanCodes(root, "", huffman_codes);
            return huffman_codes;
        },
        py::arg("root"),
        R"pbdoc(
    Generate a mapping of bytes to their Huffman codes.

    Args:
        root: The root node of the Huffman tree

    Returns:
        A dictionary mapping bytes to their Huffman codes (as strings of '0's and '1's)

    Raises:
        RuntimeError: If the root node is null
    )pbdoc");

    m.def("compress_data", &atom::algorithm::compressData, py::arg("data"),
          py::arg("huffman_codes"),
          R"pbdoc(
          Compress data using Huffman codes.

          Args:
              data: The data to compress as a bytes-like object
              huffman_codes: A dictionary mapping bytes to Huffman codes

          Returns:
              A string of '0's and '1's representing the compressed data

          Raises:
              RuntimeError: If a byte in the data doesn't have a corresponding Huffman code
          )pbdoc");

    m.def("decompress_data", &atom::algorithm::decompressData,
          py::arg("compressed_data"), py::arg("root"),
          R"pbdoc(
          Decompress Huffman-encoded data.

          Args:
              compressed_data: The compressed data as a string of '0's and '1's
              root: The root node of the Huffman tree

          Returns:
              The decompressed data as bytes

          Raises:
              RuntimeError: If the compressed data is invalid or the tree is null
          )pbdoc");

    m.def("serialize_tree", &atom::algorithm::serializeTree, py::arg("root"),
          R"pbdoc(
          Serialize a Huffman tree to a binary string.

          Args:
              root: The root node of the Huffman tree

          Returns:
              A string of '0's and '1's representing the serialized tree
          )pbdoc");

    m.def(
        "deserialize_tree",
        [](const std::string& serialized_tree) {
            size_t index = 0;
            return atom::algorithm::deserializeTree(serialized_tree, index);
        },
        py::arg("serialized_tree"),
        R"pbdoc(
    Deserialize a binary string back into a Huffman tree.

    Args:
        serialized_tree: The serialized tree as a string of '0's and '1's

    Returns:
        The root node of the reconstructed Huffman tree

    Raises:
        RuntimeError: If the serialized tree format is invalid
    )pbdoc");

    m.def("visualize_huffman_tree", &atom::algorithm::visualizeHuffmanTree,
          py::arg("root"), py::arg("indent") = "",
          R"pbdoc(
          Print a visualization of a Huffman tree.

          Args:
              root: The root node of the Huffman tree
              indent: The indentation to use (mostly for internal recursion)

          Note:
              This function prints to standard output and doesn't return anything.
          )pbdoc");

    // High-level functions for easier use
    m.def(
        "compress",
        [](const py::bytes& data) {
            // Convert Python bytes to vector of unsigned char
            std::string str = data;
            std::vector<unsigned char> bytes(str.begin(), str.end());

            // Count frequencies
            std::unordered_map<unsigned char, int> frequencies;
            for (auto byte : bytes) {
                frequencies[byte]++;
            }

            // Create Huffman tree
            auto tree = atom::algorithm::createHuffmanTree(frequencies);

            // Generate Huffman codes
            std::unordered_map<unsigned char, std::string> huffman_codes;
            atom::algorithm::generateHuffmanCodes(tree.get(), "",
                                                  huffman_codes);

            // Compress data
            std::string compressed =
                atom::algorithm::compressData(bytes, huffman_codes);

            // Serialize the tree
            std::string serialized_tree =
                atom::algorithm::serializeTree(tree.get());

            return py::make_tuple(compressed, serialized_tree);
        },
        py::arg("data"),
        R"pbdoc(
    Compress data using Huffman encoding.

    Args:
        data: The data to compress as a bytes-like object

    Returns:
        A tuple of (compressed_data, serialized_tree) where:
        - compressed_data: A string of '0's and '1's representing the compressed data
        - serialized_tree: A string of '0's and '1's representing the serialized Huffman tree

    Raises:
        RuntimeError: If compression fails
    )pbdoc");

    m.def(
        "decompress",
        [](const std::string& compressed_data,
           const std::string& serialized_tree) {
            // Deserialize the tree
            size_t index = 0;
            auto tree =
                atom::algorithm::deserializeTree(serialized_tree, index);

            // Decompress data
            std::vector<unsigned char> decompressed =
                atom::algorithm::decompressData(compressed_data, tree.get());

            // Convert to Python bytes
            return py::bytes(
                std::string(decompressed.begin(), decompressed.end()));
        },
        py::arg("compressed_data"), py::arg("serialized_tree"),
        R"pbdoc(
    Decompress Huffman-encoded data.

    Args:
        compressed_data: The compressed data as a string of '0's and '1's
        serialized_tree: The serialized Huffman tree as a string of '0's and '1's

    Returns:
        The decompressed data as bytes

    Raises:
        RuntimeError: If decompression fails
    )pbdoc");

    // All-in-one encode/decode functions with binary serialization
    m.def(
        "encode",
        [&m](const py::bytes& data) {
            // First compress the data and get the serialized tree
            py::tuple result = m.attr("compress")(data);
            std::string compressed_data = result[0].cast<std::string>();
            std::string serialized_tree = result[1].cast<std::string>();

            // Now create a binary format:
            // - First 4 bytes: length of compressed data in bits (uint32)
            // - Next 4 bytes: length of serialized tree in bits (uint32)
            // - Serialized tree (packed bits)
            // - Compressed data (packed bits)

            // Pack the bit strings into actual bits
            auto pack_bits =
                [](const std::string& bit_string) -> std::vector<uint8_t> {
                std::vector<uint8_t> packed;
                for (size_t i = 0; i < bit_string.size(); i += 8) {
                    uint8_t byte = 0;
                    for (size_t j = 0; j < 8 && i + j < bit_string.size();
                         j++) {
                        if (bit_string[i + j] == '1') {
                            byte |= (1 << (7 - j));
                        }
                    }
                    packed.push_back(byte);
                }
                return packed;
            };

            std::vector<uint8_t> packed_tree = pack_bits(serialized_tree);
            std::vector<uint8_t> packed_data = pack_bits(compressed_data);

            // Calculate sizes
            uint32_t compressed_bits =
                static_cast<uint32_t>(compressed_data.size());
            uint32_t tree_bits = static_cast<uint32_t>(serialized_tree.size());

            // Create the binary result
            std::vector<uint8_t> result_bytes;

            // Add the sizes (big-endian order)
            for (int i = 24; i >= 0; i -= 8) {
                result_bytes.push_back(
                    static_cast<uint8_t>((compressed_bits >> i) & 0xFF));
            }
            for (int i = 24; i >= 0; i -= 8) {
                result_bytes.push_back(
                    static_cast<uint8_t>((tree_bits >> i) & 0xFF));
            }

            // Add the packed tree and data
            result_bytes.insert(result_bytes.end(), packed_tree.begin(),
                                packed_tree.end());
            result_bytes.insert(result_bytes.end(), packed_data.begin(),
                                packed_data.end());

            return py::bytes(
                std::string(result_bytes.begin(), result_bytes.end()));
        },
        py::arg("data"),
        R"pbdoc(
    Compress data using Huffman encoding and pack everything into a single binary format.

    Args:
        data: The data to compress as a bytes-like object

    Returns:
        A bytes object containing the compressed data and Huffman tree

    Raises:
        RuntimeError: If compression fails
    )pbdoc");

    m.def(
        "decode",
        [&m](const py::bytes& encoded_data) {
            // Parse the binary format
            std::string str = encoded_data;
            std::vector<uint8_t> bytes(str.begin(), str.end());

            if (bytes.size() < 8) {
                throw py::value_error("Invalid encoded data format: too short");
            }

            // Extract sizes
            uint32_t compressed_bits = 0;
            uint32_t tree_bits = 0;

            for (int i = 0; i < 4; i++) {
                compressed_bits = (compressed_bits << 8) | bytes[i];
            }
            for (int i = 4; i < 8; i++) {
                tree_bits = (tree_bits << 8) | bytes[i];
            }

            // Unpack bits
            auto unpack_bits = [](const std::vector<uint8_t>& packed,
                                  size_t num_bits) -> std::string {
                std::string bit_string;
                bit_string.reserve(num_bits);

                for (size_t i = 0;
                     i < packed.size() && bit_string.size() < num_bits; i++) {
                    for (int j = 7; j >= 0 && bit_string.size() < num_bits;
                         j--) {
                        bit_string.push_back((packed[i] & (1 << j)) ? '1'
                                                                    : '0');
                    }
                }

                return bit_string;
            };

            // Calculate byte sizes (rounded up)
            size_t tree_bytes = (tree_bits + 7) / 8;
            size_t compressed_bytes = (compressed_bits + 7) / 8;

            if (bytes.size() < 8 + tree_bytes + compressed_bytes) {
                throw py::value_error(
                    "Invalid encoded data format: too short for specified "
                    "sizes");
            }

            // Extract packed data
            std::vector<uint8_t> packed_tree(bytes.begin() + 8,
                                             bytes.begin() + 8 + tree_bytes);
            std::vector<uint8_t> packed_data(
                bytes.begin() + 8 + tree_bytes,
                bytes.begin() + 8 + tree_bytes + compressed_bytes);

            // Unpack to bit strings
            std::string serialized_tree = unpack_bits(packed_tree, tree_bits);
            std::string compressed_data =
                unpack_bits(packed_data, compressed_bits);

            // Decompress
            return m.attr("decompress")(compressed_data, serialized_tree);
        },
        py::arg("encoded_data"),
        R"pbdoc(
    Decompress data that was compressed with the encode() function.

    Args:
        encoded_data: The encoded data as returned by encode()

    Returns:
        The original decompressed data as bytes

    Raises:
        ValueError: If the encoded data format is invalid
        RuntimeError: If decompression fails
    )pbdoc");

    // Utility functions
    m.def(
        "calculate_frequencies",
        [](const py::bytes& data) {
            std::string str = data;
            std::unordered_map<unsigned char, int> frequencies;
            for (auto byte : str) {
                frequencies[static_cast<unsigned char>(byte)]++;
            }
            return frequencies;
        },
        py::arg("data"),
        R"pbdoc(
    Calculate the frequency of each byte in the data.

    Args:
        data: The data as a bytes-like object

    Returns:
        A dictionary mapping bytes to their frequencies
    )pbdoc");

    m.def(
        "calculate_compression_ratio",
        [](const py::bytes& original_data,
           const std::string& compressed_bit_string) {
            double original_bits =
                original_data.cast<std::string>().size() * 8.0;
            double compressed_bits = compressed_bit_string.size();
            return compressed_bits / original_bits;
        },
        py::arg("original_data"), py::arg("compressed_bit_string"),
        R"pbdoc(
    Calculate the compression ratio (compressed size / original size).

    Args:
        original_data: The original uncompressed data
        compressed_bit_string: The compressed data as a string of '0's and '1's

    Returns:
        The compression ratio as a float (smaller is better)
    )pbdoc");

    m.def(
        "bit_string_to_bytes",
        [](const std::string& bit_string) {
            std::vector<uint8_t> bytes;
            for (size_t i = 0; i < bit_string.size(); i += 8) {
                uint8_t byte = 0;
                for (size_t j = 0; j < 8 && i + j < bit_string.size(); j++) {
                    if (bit_string[i + j] == '1') {
                        byte |= (1 << (7 - j));
                    }
                }
                bytes.push_back(byte);
            }
            return py::bytes(std::string(bytes.begin(), bytes.end()));
        },
        py::arg("bit_string"),
        R"pbdoc(
    Convert a string of '0's and '1's to bytes.

    Args:
        bit_string: A string of '0's and '1's

    Returns:
        The packed bytes
    )pbdoc");

    m.def(
        "bytes_to_bit_string",
        [](const py::bytes& data, size_t bit_count) {
            std::string str = data;
            std::string bit_string;
            bit_string.reserve(bit_count);

            for (size_t i = 0; i < str.size() && bit_string.size() < bit_count;
                 i++) {
                for (int j = 7; j >= 0 && bit_string.size() < bit_count; j--) {
                    bit_string.push_back((str[i] & (1 << j)) ? '1' : '0');
                }
            }

            return bit_string;
        },
        py::arg("data"), py::arg("bit_count"),
        R"pbdoc(
    Convert bytes to a string of '0's and '1's.

    Args:
        data: The bytes to convert
        bit_count: The number of bits to extract

    Returns:
        A string of '0's and '1's
    )pbdoc");

    // Helper for analyzing Huffman codes
    m.def(
        "analyze_huffman_codes",
        [](const std::unordered_map<unsigned char, std::string>& codes) {
            py::dict result;

            // Calculate statistics
            size_t min_length = SIZE_MAX;
            size_t max_length = 0;
            double avg_length = 0;
            std::unordered_map<size_t, int> length_count;

            for (const auto& [byte, code] : codes) {
                size_t len = code.length();
                min_length = std::min(min_length, len);
                max_length = std::max(max_length, len);
                avg_length += len;
                length_count[len]++;
            }

            if (!codes.empty()) {
                avg_length /= codes.size();
            }

            result["min_length"] = min_length;
            result["max_length"] = max_length;
            result["avg_length"] = avg_length;
            result["code_count"] = codes.size();

            // Convert length_count to a regular dict for Python
            py::dict length_dist;
            for (const auto& [len, count] : length_count) {
                length_dist[py::cast(len)] = count;
            }
            result["length_distribution"] = length_dist;

            return result;
        },
        py::arg("codes"),
        R"pbdoc(
    Analyze the properties of a set of Huffman codes.

    Args:
        codes: A dictionary mapping bytes to Huffman codes

    Returns:
        A dictionary containing statistics about the codes
    )pbdoc");
}
