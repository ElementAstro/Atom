#ifndef ATOM_ALGORITHM_BLOWFISH_HPP
#define ATOM_ALGORITHM_BLOWFISH_HPP

#include <array>
#include <span>
#include <string_view>

#include <spdlog/spdlog.h>
#include "atom/algorithm/rust_numeric.hpp"

namespace atom::algorithm {

/**
 * @brief Concept to ensure the type is an unsigned integral type of size 1
 * byte.
 */
template <typename T>
concept ByteType = std::is_same_v<T, std::byte> || std::is_same_v<T, char> ||
                   std::is_same_v<T, unsigned char>;

/**
 * @brief Applies PKCS7 padding to the data.
 * @param data The data to pad.
 * @param length The length of the data, will be updated to include padding.
 */
template <ByteType T>
void pkcs7_padding(std::span<T> data, usize& length);

/**
 * @class Blowfish
 * @brief A class implementing the Blowfish encryption algorithm.
 */
class Blowfish {
private:
    static constexpr usize P_ARRAY_SIZE = 18;  ///< Size of the P-array.
    static constexpr usize S_BOX_SIZE = 256;   ///< Size of each S-box.
    static constexpr usize BLOCK_SIZE = 8;     ///< Size of a block in bytes.

    std::array<u32, P_ARRAY_SIZE> P_;  ///< P-array used in the algorithm.
    std::array<std::array<u32, S_BOX_SIZE>, 4>
        S_;  ///< S-boxes used in the algorithm.

    /**
     * @brief The F function used in the Blowfish algorithm.
     * @param x The input to the F function.
     * @return The output of the F function.
     */
    u32 F(u32 x) const noexcept;

public:
    /**
     * @brief Constructs a Blowfish object with the given key.
     * @param key The key used for encryption and decryption.
     */
    explicit Blowfish(std::span<const std::byte> key);

    /**
     * @brief Encrypts a block of data.
     * @param block The block of data to encrypt.
     */
    void encrypt(std::span<std::byte, BLOCK_SIZE> block) noexcept;

    /**
     * @brief Decrypts a block of data.
     * @param block The block of data to decrypt.
     */
    void decrypt(std::span<std::byte, BLOCK_SIZE> block) noexcept;

    /**
     * @brief Encrypts a span of data.
     * @tparam T The type of the data, must satisfy ByteType.
     * @param data The data to encrypt.
     */
    template <ByteType T>
    void encrypt_data(std::span<T> data);

    /**
     * @brief Decrypts a span of data.
     * @tparam T The type of the data, must satisfy ByteType.
     * @param data The data to decrypt.
     * @param length The length of data to decrypt, will be updated to actual
     * length after removing padding.
     */
    template <ByteType T>
    void decrypt_data(std::span<T> data, usize& length);

    /**
     * @brief Encrypts a file.
     * @param input_file The path to the input file.
     * @param output_file The path to the output file.
     */
    void encrypt_file(std::string_view input_file,
                      std::string_view output_file);

    /**
     * @brief Decrypts a file.
     * @param input_file The path to the input file.
     * @param output_file The path to the output file.
     */
    void decrypt_file(std::string_view input_file,
                      std::string_view output_file);

private:
    /**
     * @brief Validates the provided key.
     * @param key The key to validate.
     * @throws std::runtime_error If the key is invalid.
     */
    void validate_key(std::span<const std::byte> key) const;

    /**
     * @brief Initializes the state of the Blowfish algorithm with the given
     * key.
     * @param key The key used for initialization.
     */
    void init_state(std::span<const std::byte> key);

    /**
     * @brief Validates the size of the block.
     * @param size The size of the block.
     * @throws std::runtime_error If the block size is invalid.
     */
    static void validate_block_size(usize size);

    /**
     * @brief Removes PKCS7 padding from the data.
     * @param data The data to unpad.
     * @param length The length of the data after removing padding.
     */
    void remove_padding(std::span<std::byte> data, usize& length);
};

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_BLOWFISH_HPP
