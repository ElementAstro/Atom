#ifndef ATOM_ALGORITHM_BLOWFISH_HPP
#define ATOM_ALGORITHM_BLOWFISH_HPP

#include <string>

namespace atom::algorithm {
const int P_ARRAY_SIZE = 18;  // P数组的大小
const int S_BOX_SIZE = 256;   // S盒的大小

/**
 * @class Blowfish
 * @brief A class that implements the Blowfish encryption algorithm.
 */
class Blowfish {
private:
    unsigned int P_[P_ARRAY_SIZE];  ///< P-array used in the Blowfish algorithm.
    unsigned int S_[4]
                   [S_BOX_SIZE];  ///< S-boxes used in the Blowfish algorithm.

public:
    /**
     * @brief Constructs a Blowfish object with the given key.
     * @param key The encryption key.
     * @param key_len The length of the encryption key.
     */
    Blowfish(const unsigned char* key, int key_len);

    /**
     * @brief The F function used in the Blowfish algorithm.
     * @param x The input to the F function.
     * @return The output of the F function.
     */
    unsigned int F(unsigned int x);

    /**
     * @brief Encrypts a single block of data.
     * @param block The block of data to encrypt.
     */
    void encrypt(unsigned char* block);

    /**
     * @brief Decrypts a single block of data.
     * @param block The block of data to decrypt.
     */
    void decrypt(unsigned char* block);

    /**
     * @brief Pads the data to be a multiple of the block size using PKCS7
     * padding.
     * @param data The data to pad.
     * @param length The length of the data. This will be updated to the new
     * length after padding.
     * @param block_size The block size to pad to.
     */
    void pkcs7_padding(unsigned char* data, size_t& length, size_t block_size);

    /**
     * @brief Removes the PKCS7 padding from the data.
     * @param data The data to remove padding from.
     * @param length The length of the data. This will be updated to the new
     * length after removing padding.
     */
    void remove_padding(unsigned char* data, size_t& length);

    /**
     * @brief Encrypts multiple blocks of data.
     * @param data The data to encrypt.
     * @param length The length of the data. This will be updated to the new
     * length after encryption.
     * @param block_size The block size to use for encryption.
     */
    void encrypt_data(unsigned char* data, size_t& length, size_t block_size);

    /**
     * @brief Decrypts multiple blocks of data.
     * @param data The data to decrypt.
     * @param length The length of the data. This will be updated to the new
     * length after decryption.
     * @param block_size The block size to use for decryption.
     */
    void decrypt_data(unsigned char* data, size_t& length, size_t block_size);

    /**
     * @brief Encrypts a file.
     * @param input_file The path to the input file.
     * @param output_file The path to the output file.
     */
    void encrypt_file(const std::string& input_file,
                      const std::string& output_file);

    /**
     * @brief Decrypts a file.
     * @param input_file The path to the input file.
     * @param output_file The path to the output file.
     */
    void decrypt_file(const std::string& input_file,
                      const std::string& output_file);
};
}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_BLOWFISH_HPP