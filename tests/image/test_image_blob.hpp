// filepath: atom/image/test_image_blob.hpp
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <vector>

#include "atom/image/image_blob.hpp"

namespace atom::image::test {

class BlobTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple 2x2 image with 3 channels (RGB)
        test_data = {
            // First row
            std::byte{10}, std::byte{20}, std::byte{30},  // First pixel (R,G,B)
            std::byte{40}, std::byte{50}, std::byte{60},  // Second pixel (R,G,B)
            // Second row
            std::byte{70}, std::byte{80}, std::byte{90},    // Third pixel (R,G,B)
            std::byte{100}, std::byte{110}, std::byte{120}  // Fourth pixel (R,G,B)
        };

        // Create test file path for image I/O tests
        test_image_path = "test_image.png";
    }

    void TearDown() override {
        // Clean up any test files
        std::remove(test_image_path.c_str());
    }

    std::vector<std::byte> test_data;
    std::string test_image_path;
};

// Test default constructor
TEST_F(BlobTest, DefaultConstructor) {
    blob b;
    EXPECT_EQ(b.size(), 0);
    EXPECT_EQ(b.getRows(), 0);
    EXPECT_EQ(b.getCols(), 0);
    EXPECT_EQ(b.getChannels(), 1);
}

// Test constructor with raw data
TEST_F(BlobTest, ConstructorWithRawData) {
    blob b(test_data.data(), test_data.size());
    EXPECT_EQ(b.size(), test_data.size());
    
    // Check that data was copied correctly
    for (size_t i = 0; i < test_data.size(); ++i) {
        EXPECT_EQ(b[i], test_data[i]);
    }
}

// Test constructor with array
TEST_F(BlobTest, ConstructorWithArray) {
    std::array<int, 4> arr = {1, 2, 3, 4};
    blob b(arr);
    EXPECT_EQ(b.size(), sizeof(int) * 4);
}

// Test copy constructor
TEST_F(BlobTest, CopyConstructor) {
    blob original(test_data.data(), test_data.size());
    original.rows_ = 2;
    original.cols_ = 2;
    original.channels_ = 3;

    blob copy(original);
    EXPECT_EQ(copy.size(), original.size());
    EXPECT_EQ(copy.getRows(), original.getRows());
    EXPECT_EQ(copy.getCols(), original.getCols());
    EXPECT_EQ(copy.getChannels(), original.getChannels());
    EXPECT_EQ(copy.getDepth(), original.getDepth());
    
    // Check that data was copied correctly
    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_EQ(copy[i], original[i]);
    }
}

// Test move constructor
TEST_F(BlobTest, MoveConstructor) {
    blob original(test_data.data(), test_data.size());
    original.rows_ = 2;
    original.cols_ = 2;
    original.channels_ = 3;
    size_t originalSize = original.size();

    blob moved(std::move(original));
    EXPECT_EQ(moved.size(), originalSize);
    EXPECT_EQ(moved.getRows(), 2);
    EXPECT_EQ(moved.getCols(), 2);
    EXPECT_EQ(moved.getChannels(), 3);
}

// Test const-conversion constructor
TEST_F(BlobTest, ConstConversionConstructor) {
    blob mutable_blob(test_data.data(), test_data.size());
    cblob const_blob(mutable_blob);
    
    EXPECT_EQ(const_blob.size(), mutable_blob.size());
    
    // Check that data was copied correctly
    for (size_t i = 0; i < mutable_blob.size(); ++i) {
        EXPECT_EQ(const_blob[i], mutable_blob[i]);
    }
}

// Test FAST mode blob
TEST_F(BlobTest, FastModeBlob) {
    std::vector<std::byte> data(test_data);
    fast_blob fb(data.data(), data.size());
    
    EXPECT_EQ(fb.size(), data.size());
    
    // Modify original data and check that fast_blob reflects the changes
    data[0] = std::byte{255};
    EXPECT_EQ(fb[0], std::byte{255});
}

// Test slice method
TEST_F(BlobTest, Slice) {
    blob b(test_data.data(), test_data.size());
    b.rows_ = 2;
    b.cols_ = 6;  // 2 pixels per row, 3 channels per pixel
    b.channels_ = 3;
    
    // Slice first row
    blob first_row = b.slice(0, 6);
    EXPECT_EQ(first_row.size(), 6);
    EXPECT_EQ(first_row[0], std::byte{10});
    EXPECT_EQ(first_row[5], std::byte{60});
    
    // Slice second row
    blob second_row = b.slice(6, 6);
    EXPECT_EQ(second_row.size(), 6);
    EXPECT_EQ(second_row[0], std::byte{70});
    EXPECT_EQ(second_row[5], std::byte{120});
    
    // Test out of bounds slice
    EXPECT_THROW(b.slice(10, 10), std::out_of_range);
}

// Test equality operator
TEST_F(BlobTest, EqualityOperator) {
    blob b1(test_data.data(), test_data.size());
    blob b2(test_data.data(), test_data.size());
    blob b3(test_data.data(), test_data.size() - 1);  // Different size
    
    EXPECT_EQ(b1, b2);
    EXPECT_NE(b1, b3);
    
    // Modify b2 and check inequality
    b2[0] = std::byte{255};
    EXPECT_NE(b1, b2);
    
    // Set b2 back to equal b1
    b2[0] = b1[0];
    EXPECT_EQ(b1, b2);
    
    // Change other properties and check inequality
    b2.rows_ = 3;
    EXPECT_NE(b1, b2);
}

// Test fill method
TEST_F(BlobTest, Fill) {
    blob b(test_data.data(), test_data.size());
    b.fill(std::byte{42});
    
    for (size_t i = 0; i < b.size(); ++i) {
        EXPECT_EQ(b[i], std::byte{42});
    }
}

// Test append method with another blob
TEST_F(BlobTest, AppendBlob) {
    blob b1(test_data.data(), 6);  // First row
    blob b2(test_data.data() + 6, 6);  // Second row
    
    b1.rows_ = 1;
    b1.cols_ = 6;
    b1.channels_ = 1;
    
    b2.rows_ = 1;
    b2.cols_ = 6;
    b2.channels_ = 1;
    
    b1.append(b2);
    
    EXPECT_EQ(b1.size(), 12);
    EXPECT_EQ(b1.getRows(), 2);
    EXPECT_EQ(b1[6], std::byte{70});
    EXPECT_EQ(b1[11], std::byte{120});
}

// Test append with raw data
TEST_F(BlobTest, AppendRawData) {
    blob b(test_data.data(), 6);  // First row
    b.rows_ = 1;
    b.cols_ = 6;
    b.channels_ = 1;
    
    b.append(test_data.data() + 6, 6);  // Append second row
    
    EXPECT_EQ(b.size(), 12);
    EXPECT_EQ(b.getRows(), 2);
    EXPECT_EQ(b[6], std::byte{70});
    EXPECT_EQ(b[11], std::byte{120});
}

// Test allocate and deallocate
TEST_F(BlobTest, AllocateAndDeallocate) {
    blob b;
    b.allocate(10);
    EXPECT_EQ(b.size(), 10);
    
    b.deallocate();
    EXPECT_EQ(b.size(), 0);
}

// Test XOR operation
TEST_F(BlobTest, XorOperation) {
    blob b1(test_data.data(), test_data.size());
    blob b2(test_data.data(), test_data.size());
    
    // Fill b2 with a constant value
    b2.fill(std::byte{255});
    
    b1.xorWith(b2);
    
    // Check that each byte is now the XOR of the original and 255
    for (size_t i = 0; i < test_data.size(); ++i) {
        EXPECT_EQ(b1[i], std::byte{static_cast<unsigned char>(test_data[i]) ^ 255});
    }
    
    // Test with different sized blobs
    blob b3(test_data.data(), test_data.size() - 1);
    EXPECT_THROW(b1.xorWith(b3), std::runtime_error);
}

// Test compression and decompression
TEST_F(BlobTest, CompressionAndDecompression) {
    // Create a blob with repeated values that should compress well
    std::vector<std::byte> compressible_data(100, std::byte{42});
    blob original(compressible_data.data(), compressible_data.size());
    
    blob compressed = original.compress();
    EXPECT_LT(compressed.size(), original.size());
    
    blob decompressed = compressed.decompress();
    EXPECT_EQ(decompressed.size(), original.size());
    EXPECT_EQ(decompressed, original);
}

// Test serialization and deserialization
TEST_F(BlobTest, SerializationAndDeserialization) {
    blob original(test_data.data(), test_data.size());
    original.rows_ = 2;
    original.cols_ = 2;
    original.channels_ = 3;
    
    std::vector<std::byte> serialized = original.serialize();
    blob deserialized = blob::deserialize(serialized);
    
    EXPECT_EQ(deserialized.size(), original.size());
    
    // Check data equality
    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_EQ(deserialized[i], original[i]);
    }
    
    // Test with invalid data
    std::vector<std::byte> invalid_data(2, std::byte{0});
    EXPECT_THROW(blob::deserialize(invalid_data), std::runtime_error);
}

// Test iteration methods
TEST_F(BlobTest, Iteration) {
    blob b(test_data.data(), test_data.size());
    
    // Test begin/end interface
    size_t i = 0;
    for (auto byte : b) {
        EXPECT_EQ(byte, test_data[i++]);
    }
    EXPECT_EQ(i, test_data.size());
    
    // Test const begin/end interface
    const blob& const_b = b;
    i = 0;
    for (auto byte : const_b) {
        EXPECT_EQ(byte, test_data[i++]);
    }
    EXPECT_EQ(i, test_data.size());
}

#if __has_include(<opencv2/core.hpp>)
// Test OpenCV integration
TEST_F(BlobTest, OpenCVIntegration) {
    // Create a test matrix
    cv::Mat mat(2, 2, CV_8UC3);
    
    // Fill with test data
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            for (int c = 0; c < 3; ++c) {
                mat.at<cv::Vec3b>(i, j)[c] = static_cast<unsigned char>(i * 2 * 3 + j * 3 + c + 10);
            }
        }
    }
    
    // Create blob from matrix
    blob b(mat);
    
    EXPECT_EQ(b.getRows(), 2);
    EXPECT_EQ(b.getCols(), 2);
    EXPECT_EQ(b.getChannels(), 3);
    EXPECT_EQ(b.size(), 12);
    
    // Convert back to matrix
    cv::Mat reconstructed = b.to_mat();
    
    // Verify matrix equality
    EXPECT_TRUE(cv::countNonZero(mat != reconstructed) == 0);
    
    // Test image operations
    blob resized = b;
    resized.resize(4, 4);
    EXPECT_EQ(resized.getRows(), 4);
    EXPECT_EQ(resized.getCols(), 4);
    EXPECT_EQ(resized.getChannels(), 3);
    EXPECT_EQ(resized.size(), 48);
    
    // Test channel splitting and merging
    std::vector<blob> channels = b.split_channels();
    EXPECT_EQ(channels.size(), 3);
    EXPECT_EQ(channels[0].getChannels(), 1);
    EXPECT_EQ(channels[0].size(), 4);
    
    blob merged = blob::merge_channels(channels);
    EXPECT_EQ(merged.getChannels(), 3);
    EXPECT_EQ(merged.size(), 12);
    EXPECT_EQ(merged, b);
    
    // Test filtering
    cv::Mat kernel = (cv::Mat_<float>(3, 3) << 0, -1, 0, -1, 5, -1, 0, -1, 0);
    blob filtered = b;
    filtered.apply_filter(kernel);
    
    // Test rotation and flipping
    blob rotated = b;
    rotated.rotate(90);
    EXPECT_NE(rotated, b);
    
    blob flipped = b;
    flipped.flip(1);  // Horizontal flip
    EXPECT_NE(flipped, b);
    
    // Test color conversion
    if (b.getChannels() == 3) {
        blob gray = b;
        gray.convert_color(cv::COLOR_BGR2GRAY);
        EXPECT_EQ(gray.getChannels(), 1);
    }
}

// Test OpenCV image I/O
TEST_F(BlobTest, OpenCVImageIO) {
    // Create a test matrix
    cv::Mat mat(2, 2, CV_8UC3);
    
    // Fill with test data
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            for (int c = 0; c < 3; ++c) {
                mat.at<cv::Vec3b>(i, j)[c] = static_cast<unsigned char>(i * 2 * 3 + j * 3 + c + 10);
            }
        }
    }
    
    // Create blob from matrix
    blob b(mat);
    
    // Save to file
    b.save(test_image_path);
    
    // Load from file
    blob loaded = blob::load(test_image_path);
    
    // Size and channels should be the same
    EXPECT_EQ(loaded.getRows(), b.getRows());
    EXPECT_EQ(loaded.getCols(), b.getCols());
    EXPECT_EQ(loaded.getChannels(), b.getChannels());
    
    // Test loading non-existent file
    EXPECT_THROW(blob::load("non_existent_file.png"), std::runtime_error);
}
#endif

#if __has_include(<CImg.h>)
// Test CImg integration
TEST_F(BlobTest, CImgIntegration) {
    // Create a CImg
    cimg_library::CImg<unsigned char> img(2, 2, 1, 3);
    
    // Fill with test data
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            for (int c = 0; c < 3; ++c) {
                img(x, y, 0, c) = static_cast<unsigned char>(y * 2 * 3 + x * 3 + c + 10);
            }
        }
    }
    
    // Create blob from CImg
    blob b(img);
    
    EXPECT_EQ(b.getRows(), 2);
    EXPECT_EQ(b.getCols(), 2);
    EXPECT_EQ(b.getChannels(), 3);
    EXPECT_EQ(b.size(), 12);
    
    // Convert back to CImg
    cimg_library::CImg<unsigned char> reconstructed = b.to_cimg();
    
    // Verify image equality
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            for (int c = 0; c < 3; ++c) {
                EXPECT_EQ(reconstructed(x, y, 0, c), img(x, y, 0, c));
            }
        }
    }
    
    // Test filter application
    cimg_library::CImg<float> kernel(3, 3, 1, 1, 0);
    kernel(1, 1) = 1.0f;  // Identity filter
    
    blob filtered = b;
    filtered.apply_cimg_filter(kernel);
    
    // Should be similar to original after applying identity filter
    EXPECT_EQ(filtered.getRows(), b.getRows());
    EXPECT_EQ(filtered.getCols(), b.getCols());
    EXPECT_EQ(filtered.getChannels(), b.getChannels());
}
#endif

#if __has_include(<stb_image.h>)
// Test stb_image integration
TEST_F(BlobTest, StbImageIntegration) {
    // Create a test image with OpenCV and save it
    #if __has_include(<opencv2/core.hpp>)
    cv::Mat mat(2, 2, CV_8UC3);
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            for (int c = 0; c < 3; ++c) {
                mat.at<cv::Vec3b>(i, j)[c] = static_cast<unsigned char>(i * 2 * 3 + j * 3 + c + 10);
            }
        }
    }
    cv::imwrite(test_image_path, mat);
    #else
    // Create a simple bitmap file for testing
    FILE* f = fopen(test_image_path.c_str(), "wb");
    if (f) {
        // Simple BMP header
        unsigned char bmp_header[54] = {
            'B', 'M',                       // Signature
            0x36, 0x00, 0x00, 0x00,         // File size
            0x00, 0x00, 0x00, 0x00,         // Reserved
            0x36, 0x00, 0x00, 0x00,         // Pixel data offset
            0x28, 0x00, 0x00, 0x00,         // DIB header size
            0x02, 0x00, 0x00, 0x00,         // Width
            0x02, 0x00, 0x00, 0x00,         // Height
            0x01, 0x00,                     // Planes
            0x18, 0x00,                     // Bits per pixel (24)
            0x00, 0x00, 0x00, 0x00,         // Compression
            0x10, 0x00, 0x00, 0x00,         // Image size
            0x00, 0x00, 0x00, 0x00,         // X pixels per meter
            0x00, 0x00, 0x00, 0x00,         // Y pixels per meter
            0x00, 0x00, 0x00, 0x00,         // Total colors
            0x00, 0x00, 0x00, 0x00          // Important colors
        };
        fwrite(bmp_header, sizeof(bmp_header), 1, f);
        
        // Write test data (BGR order for BMP)
        for (int i = 0; i < test_data.size(); i += 3) {
            unsigned char bgr[3] = {
                static_cast<unsigned char>(test_data[i+2]),
                static_cast<unsigned char>(test_data[i+1]),
                static_cast<unsigned char>(test_data[i])
            };
            fwrite(bgr, 3, 1, f);
        }
        fclose(f);
    }
    #endif
    
    // Load with stb_image
    blob b(test_image_path);
    
    // Basic checks
    EXPECT_EQ(b.getCols(), 2);
    EXPECT_EQ(b.getRows(), 2);
    EXPECT_EQ(b.getChannels(), 3);
    
    // Save with different formats
    b.save_as(test_image_path + ".png", "png");
    b.save_as(test_image_path + ".bmp", "bmp");
    b.save_as(test_image_path + ".jpg", "jpg");
    b.save_as(test_image_path + ".tga", "tga");
    
    // Clean up
    std::remove((test_image_path + ".png").c_str());
    std::remove((test_image_path + ".bmp").c_str());
    std::remove((test_image_path + ".jpg").c_str());
    std::remove((test_image_path + ".tga").c_str());
    
    // Test invalid format
    EXPECT_THROW(b.save_as(test_image_path + ".invalid", "invalid"), std::runtime_error);
}
#endif

// Test FAST mode limitations
TEST_F(BlobTest, FastModeLimitations) {
    // Create a fast blob
    std::vector<std::byte> data(test_data);
    fast_blob fb(data.data(), data.size());
    
    // These operations should throw in FAST mode
    EXPECT_THROW(fb.append(fb), std::runtime_error);
    EXPECT_THROW(fb.append(data.data(), data.size()), std::runtime_error);
    EXPECT_THROW(fb.allocate(20), std::runtime_error);
    EXPECT_THROW(fb.deallocate(), std::runtime_error);
    
    #if __has_include(<CImg.h>)
    // CImg operations should throw in FAST mode
    cimg_library::CImg<float> kernel(3, 3);
    EXPECT_THROW(fb.apply_cimg_filter(kernel), std::runtime_error);
    EXPECT_THROW(fb.to_cimg(), std::runtime_error);
    #endif
    
    #if __has_include(<stb_image.h>)
    // stb_image operations should throw in FAST mode
    EXPECT_THROW(fb.save_as(test_image_path, "png"), std::runtime_error);
    
    // Fast mode constructor from stb_image should throw
    EXPECT_THROW(fast_blob bad_fb(test_image_path), std::runtime_error);
    #endif
}

} // namespace atom::image::test

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}