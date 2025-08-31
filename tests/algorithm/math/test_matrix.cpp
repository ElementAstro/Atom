#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>
#include <spdlog/spdlog.h>
#include "atom/algorithm/matrix.hpp"

using namespace atom::algorithm;
using namespace std::chrono_literals;

// Helper functions
template <typename T, std::size_t Rows, std::size_t Cols>
void expectMatricesNear(const Matrix<T, Rows, Cols>& actual,
                        const Matrix<T, Rows, Cols>& expected,
                        double epsilon = 1e-6) {
    for (std::size_t i = 0; i < Rows; ++i) {
        for (std::size_t j = 0; j < Cols; ++j) {
            EXPECT_NEAR(actual(i, j), expected(i, j), epsilon)
                << "Matrices differ at position (" << i << "," << j << ")";
        }
    }
}

// Test fixture for Matrix tests
class MatrixTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize loguru for testing
        static bool initialized = false;
        if (!initialized) {
            spdlog::set_level(spdlog::level::off);
            initialized = true;
        }
    }
};

// Basic constructors and accessors
TEST_F(MatrixTest, DefaultConstructor) {
    Matrix<double, 3, 3> mat;

    // Default constructed matrix should be zero-initialized
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            EXPECT_EQ(mat(i, j), 0.0);
        }
    }
}

TEST_F(MatrixTest, ArrayConstructor) {
    std::array<int, 6> arr = {1, 2, 3, 4, 5, 6};
    Matrix<int, 2, 3> mat(arr);

    EXPECT_EQ(mat(0, 0), 1);
    EXPECT_EQ(mat(0, 1), 2);
    EXPECT_EQ(mat(0, 2), 3);
    EXPECT_EQ(mat(1, 0), 4);
    EXPECT_EQ(mat(1, 1), 5);
    EXPECT_EQ(mat(1, 2), 6);
}

TEST_F(MatrixTest, CopyConstructor) {
    Matrix<double, 2, 2> original;
    original(0, 0) = 1.0;
    original(0, 1) = 2.0;
    original(1, 0) = 3.0;
    original(1, 1) = 4.0;

    Matrix<double, 2, 2> copy(original);

    EXPECT_EQ(copy(0, 0), original(0, 0));
    EXPECT_EQ(copy(0, 1), original(0, 1));
    EXPECT_EQ(copy(1, 0), original(1, 0));
    EXPECT_EQ(copy(1, 1), original(1, 1));
}

TEST_F(MatrixTest, MoveConstructor) {
    Matrix<double, 2, 2> original;
    original(0, 0) = 1.0;
    original(0, 1) = 2.0;
    original(1, 0) = 3.0;
    original(1, 1) = 4.0;

    Matrix<double, 2, 2> moved(std::move(original));

    EXPECT_EQ(moved(0, 0), 1.0);
    EXPECT_EQ(moved(0, 1), 2.0);
    EXPECT_EQ(moved(1, 0), 3.0);
    EXPECT_EQ(moved(1, 1), 4.0);

    // Note: After move, original is in a valid but unspecified state
    // We can't make assertions about its content
}

TEST_F(MatrixTest, CopyAssignment) {
    Matrix<double, 2, 3> original;
    original(0, 0) = 1.0;
    original(0, 1) = 2.0;
    original(0, 2) = 3.0;
    original(1, 0) = 4.0;
    original(1, 1) = 5.0;
    original(1, 2) = 6.0;

    Matrix<double, 2, 3> copy;
    copy = original;

    EXPECT_EQ(copy(0, 0), original(0, 0));
    EXPECT_EQ(copy(0, 1), original(0, 1));
    EXPECT_EQ(copy(0, 2), original(0, 2));
    EXPECT_EQ(copy(1, 0), original(1, 0));
    EXPECT_EQ(copy(1, 1), original(1, 1));
    EXPECT_EQ(copy(1, 2), original(1, 2));
}

TEST_F(MatrixTest, MoveAssignment) {
    Matrix<double, 2, 3> original;
    original(0, 0) = 1.0;
    original(0, 1) = 2.0;
    original(0, 2) = 3.0;
    original(1, 0) = 4.0;
    original(1, 1) = 5.0;
    original(1, 2) = 6.0;

    Matrix<double, 2, 3> moved;
    moved = std::move(original);

    EXPECT_EQ(moved(0, 0), 1.0);
    EXPECT_EQ(moved(0, 1), 2.0);
    EXPECT_EQ(moved(0, 2), 3.0);
    EXPECT_EQ(moved(1, 0), 4.0);
    EXPECT_EQ(moved(1, 1), 5.0);
    EXPECT_EQ(moved(1, 2), 6.0);

    // Note: After move, original is in a valid but unspecified state
}

TEST_F(MatrixTest, GetDataAccessor) {
    Matrix<int, 2, 2> mat;
    mat(0, 0) = 1;
    mat(0, 1) = 2;
    mat(1, 0) = 3;
    mat(1, 1) = 4;

    const auto& constData = mat.getData();
    EXPECT_EQ(constData[0], 1);
    EXPECT_EQ(constData[1], 2);
    EXPECT_EQ(constData[2], 3);
    EXPECT_EQ(constData[3], 4);

    auto& data = mat.getData();
    data[0] = 10;
    EXPECT_EQ(mat(0, 0), 10);
}

// Basic operations
TEST_F(MatrixTest, AdditionOperation) {
    Matrix<int, 2, 3> a;
    Matrix<int, 2, 3> b;

    for (std::size_t i = 0; i < 2; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            a(i, j) = i * 3 + j;
            b(i, j) = 10 + i * 3 + j;
        }
    }

    Matrix<int, 2, 3> c = a + b;

    for (std::size_t i = 0; i < 2; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            EXPECT_EQ(c(i, j), a(i, j) + b(i, j));
        }
    }
}

TEST_F(MatrixTest, SubtractionOperation) {
    Matrix<int, 2, 3> a;
    Matrix<int, 2, 3> b;

    for (std::size_t i = 0; i < 2; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            a(i, j) = 10 + i * 3 + j;
            b(i, j) = i * 3 + j;
        }
    }

    Matrix<int, 2, 3> c = a - b;

    for (std::size_t i = 0; i < 2; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            EXPECT_EQ(c(i, j), a(i, j) - b(i, j));
        }
    }
}

TEST_F(MatrixTest, MatrixMultiplication) {
    Matrix<int, 2, 3> a;
    Matrix<int, 3, 4> b;

    // Set up matrix a
    a(0, 0) = 1;
    a(0, 1) = 2;
    a(0, 2) = 3;
    a(1, 0) = 4;
    a(1, 1) = 5;
    a(1, 2) = 6;

    // Set up matrix b
    b(0, 0) = 7;
    b(0, 1) = 8;
    b(0, 2) = 9;
    b(0, 3) = 10;
    b(1, 0) = 11;
    b(1, 1) = 12;
    b(1, 2) = 13;
    b(1, 3) = 14;
    b(2, 0) = 15;
    b(2, 1) = 16;
    b(2, 2) = 17;
    b(2, 3) = 18;

    Matrix<int, 2, 4> c = a * b;

    // Expected result:
    // [1*7 + 2*11 + 3*15, 1*8 + 2*12 + 3*16, 1*9 + 2*13 + 3*17, 1*10 + 2*14 +
    // 3*18] [4*7 + 5*11 + 6*15, 4*8 + 5*12 + 6*16, 4*9 + 5*13 + 6*17, 4*10 +
    // 5*14 + 6*18]

    EXPECT_EQ(c(0, 0), 1 * 7 + 2 * 11 + 3 * 15);
    EXPECT_EQ(c(0, 1), 1 * 8 + 2 * 12 + 3 * 16);
    EXPECT_EQ(c(0, 2), 1 * 9 + 2 * 13 + 3 * 17);
    EXPECT_EQ(c(0, 3), 1 * 10 + 2 * 14 + 3 * 18);
    EXPECT_EQ(c(1, 0), 4 * 7 + 5 * 11 + 6 * 15);
    EXPECT_EQ(c(1, 1), 4 * 8 + 5 * 12 + 6 * 16);
    EXPECT_EQ(c(1, 2), 4 * 9 + 5 * 13 + 6 * 17);
    EXPECT_EQ(c(1, 3), 4 * 10 + 5 * 14 + 6 * 18);
}

TEST_F(MatrixTest, ScalarMultiplication) {
    Matrix<int, 2, 3> a;

    a(0, 0) = 1;
    a(0, 1) = 2;
    a(0, 2) = 3;
    a(1, 0) = 4;
    a(1, 1) = 5;
    a(1, 2) = 6;

    // Left multiplication
    Matrix<int, 2, 3> b = 2 * a;

    EXPECT_EQ(b(0, 0), 2);
    EXPECT_EQ(b(0, 1), 4);
    EXPECT_EQ(b(0, 2), 6);
    EXPECT_EQ(b(1, 0), 8);
    EXPECT_EQ(b(1, 1), 10);
    EXPECT_EQ(b(1, 2), 12);

    // Right multiplication
    Matrix<int, 2, 3> c = a * 3;

    EXPECT_EQ(c(0, 0), 3);
    EXPECT_EQ(c(0, 1), 6);
    EXPECT_EQ(c(0, 2), 9);
    EXPECT_EQ(c(1, 0), 12);
    EXPECT_EQ(c(1, 1), 15);
    EXPECT_EQ(c(1, 2), 18);
}

TEST_F(MatrixTest, ElementWiseProduct) {
    Matrix<int, 2, 3> a;
    Matrix<int, 2, 3> b;

    a(0, 0) = 1;
    a(0, 1) = 2;
    a(0, 2) = 3;
    a(1, 0) = 4;
    a(1, 1) = 5;
    a(1, 2) = 6;

    b(0, 0) = 7;
    b(0, 1) = 8;
    b(0, 2) = 9;
    b(1, 0) = 10;
    b(1, 1) = 11;
    b(1, 2) = 12;

    Matrix<int, 2, 3> c = elementWiseProduct(a, b);

    EXPECT_EQ(c(0, 0), 7);
    EXPECT_EQ(c(0, 1), 16);
    EXPECT_EQ(c(0, 2), 27);
    EXPECT_EQ(c(1, 0), 40);
    EXPECT_EQ(c(1, 1), 55);
    EXPECT_EQ(c(1, 2), 72);
}

TEST_F(MatrixTest, Transpose) {
    Matrix<int, 2, 3> a;

    a(0, 0) = 1;
    a(0, 1) = 2;
    a(0, 2) = 3;
    a(1, 0) = 4;
    a(1, 1) = 5;
    a(1, 2) = 6;

    Matrix<int, 3, 2> at = transpose(a);

    EXPECT_EQ(at(0, 0), 1);
    EXPECT_EQ(at(0, 1), 4);
    EXPECT_EQ(at(1, 0), 2);
    EXPECT_EQ(at(1, 1), 5);
    EXPECT_EQ(at(2, 0), 3);
    EXPECT_EQ(at(2, 1), 6);
}

// Matrix properties
TEST_F(MatrixTest, Trace) {
    Matrix<int, 3, 3> a;

    a(0, 0) = 1;
    a(0, 1) = 2;
    a(0, 2) = 3;
    a(1, 0) = 4;
    a(1, 1) = 5;
    a(1, 2) = 6;
    a(2, 0) = 7;
    a(2, 1) = 8;
    a(2, 2) = 9;

    EXPECT_EQ(a.trace(), 15);  // 1 + 5 + 9 = 15
}

TEST_F(MatrixTest, FrobeniusNorm) {
    Matrix<double, 2, 2> a;

    a(0, 0) = 1.0;
    a(0, 1) = 2.0;
    a(1, 0) = 3.0;
    a(1, 1) = 4.0;

    // Frobenius norm = sqrt(1^2 + 2^2 + 3^2 + 4^2) = sqrt(30) ≈ 5.477
    EXPECT_NEAR(a.frobeniusNorm(), std::sqrt(30), 1e-10);
}

TEST_F(MatrixTest, MaxElement) {
    Matrix<double, 2, 3> a;

    a(0, 0) = 1.5;
    a(0, 1) = -7.2;
    a(0, 2) = 3.8;
    a(1, 0) = 4.6;
    a(1, 1) = 5.0;
    a(1, 2) = -6.1;

    // Max by absolute value is -7.2
    EXPECT_DOUBLE_EQ(a.maxElement(), -7.2);
}

TEST_F(MatrixTest, MinElement) {
    Matrix<double, 2, 3> a;

    a(0, 0) = 1.5;
    a(0, 1) = -7.2;
    a(0, 2) = 3.8;
    a(1, 0) = 4.6;
    a(1, 1) = 5.0;
    a(1, 2) = -6.1;

    // Min by absolute value is 1.5
    EXPECT_DOUBLE_EQ(a.minElement(), 1.5);
}

TEST_F(MatrixTest, IsSymmetric) {
    Matrix<int, 3, 3> symmetric;
    Matrix<int, 3, 3> nonSymmetric;

    symmetric(0, 0) = 1;
    symmetric(0, 1) = 2;
    symmetric(0, 2) = 3;
    symmetric(1, 0) = 2;
    symmetric(1, 1) = 4;
    symmetric(1, 2) = 5;
    symmetric(2, 0) = 3;
    symmetric(2, 1) = 5;
    symmetric(2, 2) = 6;

    nonSymmetric(0, 0) = 1;
    nonSymmetric(0, 1) = 2;
    nonSymmetric(0, 2) = 3;
    nonSymmetric(1, 0) = 4;
    nonSymmetric(1, 1) = 5;
    nonSymmetric(1, 2) = 6;
    nonSymmetric(2, 0) = 7;
    nonSymmetric(2, 1) = 8;
    nonSymmetric(2, 2) = 9;

    EXPECT_TRUE(symmetric.isSymmetric());
    EXPECT_FALSE(nonSymmetric.isSymmetric());
}

// Advanced matrix operations
TEST_F(MatrixTest, MatrixPower) {
    Matrix<int, 2, 2> a;

    a(0, 0) = 1;
    a(0, 1) = 2;
    a(1, 0) = 3;
    a(1, 1) = 4;

    // A^0 should be identity
    Matrix<int, 2, 2> a0 = a.pow(0);
    EXPECT_EQ(a0(0, 0), 1);
    EXPECT_EQ(a0(0, 1), 0);
    EXPECT_EQ(a0(1, 0), 0);
    EXPECT_EQ(a0(1, 1), 1);

    // A^1 should be A
    Matrix<int, 2, 2> a1 = a.pow(1);
    EXPECT_EQ(a1(0, 0), 1);
    EXPECT_EQ(a1(0, 1), 2);
    EXPECT_EQ(a1(1, 0), 3);
    EXPECT_EQ(a1(1, 1), 4);

    // A^2 should be A*A
    Matrix<int, 2, 2> a2 = a.pow(2);
    EXPECT_EQ(a2(0, 0), 7);
    EXPECT_EQ(a2(0, 1), 10);
    EXPECT_EQ(a2(1, 0), 15);
    EXPECT_EQ(a2(1, 1), 22);
}

TEST_F(MatrixTest, LuDecomposition) {
    Matrix<double, 3, 3> a;

    a(0, 0) = 4.0;
    a(0, 1) = 3.0;
    a(0, 2) = 8.0;
    a(1, 0) = 2.0;
    a(1, 1) = 6.0;
    a(1, 2) = 7.0;
    a(2, 0) = 1.0;
    a(2, 1) = 5.0;
    a(2, 2) = 9.0;

    auto [L, U] = luDecomposition(a);

    // Check L is lower triangular with 1's on diagonal
    EXPECT_DOUBLE_EQ(L(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(L(0, 1), 0.0);
    EXPECT_DOUBLE_EQ(L(0, 2), 0.0);
    EXPECT_NEAR(L(1, 0), 0.5, 1e-10);
    EXPECT_DOUBLE_EQ(L(1, 1), 1.0);
    EXPECT_DOUBLE_EQ(L(1, 2), 0.0);
    EXPECT_NEAR(L(2, 0), 0.25, 1e-10);
    EXPECT_NEAR(L(2, 1), 0.8, 1e-10);
    EXPECT_DOUBLE_EQ(L(2, 2), 1.0);

    // Check U is upper triangular
    EXPECT_DOUBLE_EQ(U(0, 0), 4.0);
    EXPECT_DOUBLE_EQ(U(0, 1), 3.0);
    EXPECT_DOUBLE_EQ(U(0, 2), 8.0);
    EXPECT_DOUBLE_EQ(U(1, 0), 0.0);
    EXPECT_DOUBLE_EQ(U(1, 1), 4.5);
    EXPECT_DOUBLE_EQ(U(1, 2), 3.0);
    EXPECT_DOUBLE_EQ(U(2, 0), 0.0);
    EXPECT_DOUBLE_EQ(U(2, 1), 0.0);
    EXPECT_NEAR(U(2, 2), 4.0, 1e-9);

    // Check L*U = A
    Matrix<double, 3, 3> product = L * U;
    expectMatricesNear(product, a);
}

TEST_F(MatrixTest, Determinant) {
    Matrix<double, 3, 3> a;

    a(0, 0) = 4.0;
    a(0, 1) = 3.0;
    a(0, 2) = 8.0;
    a(1, 0) = 2.0;
    a(1, 1) = 6.0;
    a(1, 2) = 7.0;
    a(2, 0) = 1.0;
    a(2, 1) = 5.0;
    a(2, 2) = 9.0;

    // det(A) = 4*(6*9 - 7*5) - 3*(2*9 - 7*1) + 8*(2*5 - 6*1)
    //        = 4*(54 - 35) - 3*(18 - 7) + 8*(10 - 6)
    //        = 4*19 - 3*11 + 8*4
    //        = 76 - 33 + 32
    //        = 75
    EXPECT_NEAR(a.determinant(), 75.0, 1e-9);
}

TEST_F(MatrixTest, Inverse) {
    Matrix<double, 3, 3> a;

    a(0, 0) = 4.0;
    a(0, 1) = 3.0;
    a(0, 2) = 8.0;
    a(1, 0) = 2.0;
    a(1, 1) = 6.0;
    a(1, 2) = 7.0;
    a(2, 0) = 1.0;
    a(2, 1) = 5.0;
    a(2, 2) = 9.0;

    Matrix<double, 3, 3> aInv = a.inverse();

    // Check that A * A^-1 = I
    Matrix<double, 3, 3> product = a * aInv;
    Matrix<double, 3, 3> identity = ::identity<double, 3>();

    expectMatricesNear(product, identity);
}

TEST_F(MatrixTest, SingularInverse) {
    Matrix<double, 3, 3> singular;

    // Create a singular matrix (rank < 3)
    singular(0, 0) = 1.0;
    singular(0, 1) = 2.0;
    singular(0, 2) = 3.0;
    singular(1, 0) = 4.0;
    singular(1, 1) = 5.0;
    singular(1, 2) = 6.0;
    singular(2, 0) = 7.0;
    singular(2, 1) = 8.0;
    singular(2, 2) = 9.0;

    // Determinant should be close to 0
    EXPECT_NEAR(singular.determinant(), 0.0, 1e-9);

    // Inverse should throw an exception
    EXPECT_THROW(singular.inverse(), std::runtime_error);
}

TEST_F(MatrixTest, Rank) {
    Matrix<double, 3, 3> fullRank;
    Matrix<double, 3, 3> rank2;
    Matrix<double, 3, 3> rank1;

    // Full rank matrix (rank 3)
    fullRank(0, 0) = 4.0;
    fullRank(0, 1) = 3.0;
    fullRank(0, 2) = 8.0;
    fullRank(1, 0) = 2.0;
    fullRank(1, 1) = 6.0;
    fullRank(1, 2) = 7.0;
    fullRank(2, 0) = 1.0;
    fullRank(2, 1) = 5.0;
    fullRank(2, 2) = 9.0;

    // Rank 2 matrix (3rd row is a linear combination of rows 1 and 2)
    rank2(0, 0) = 1.0;
    rank2(0, 1) = 2.0;
    rank2(0, 2) = 3.0;
    rank2(1, 0) = 4.0;
    rank2(1, 1) = 5.0;
    rank2(1, 2) = 6.0;
    rank2(2, 0) = 5.0;
    rank2(2, 1) = 7.0;
    rank2(2, 2) = 9.0;  // = row1 + row2

    // Rank 1 matrix (all rows are multiples of first row)
    rank1(0, 0) = 1.0;
    rank1(0, 1) = 2.0;
    rank1(0, 2) = 3.0;
    rank1(1, 0) = 2.0;
    rank1(1, 1) = 4.0;
    rank1(1, 2) = 6.0;  // = 2 * row1
    rank1(2, 0) = 3.0;
    rank1(2, 1) = 6.0;
    rank1(2, 2) = 9.0;  // = 3 * row1

    EXPECT_EQ(fullRank.rank(), 3);
    EXPECT_EQ(rank2.rank(), 2);
    EXPECT_EQ(rank1.rank(), 1);
}

TEST_F(MatrixTest, SVDAndConditionNumber) {
    Matrix<double, 3, 3> wellConditioned;

    wellConditioned(0, 0) = 4.0;
    wellConditioned(0, 1) = 0.0;
    wellConditioned(0, 2) = 0.0;
    wellConditioned(1, 0) = 0.0;
    wellConditioned(1, 1) = 2.0;
    wellConditioned(1, 2) = 0.0;
    wellConditioned(2, 0) = 0.0;
    wellConditioned(2, 1) = 0.0;
    wellConditioned(2, 2) = 1.0;

    // Singular values should be [4, 2, 1]
    auto singularValues = singularValueDecomposition(wellConditioned);

    ASSERT_EQ(singularValues.size(), 3);
    EXPECT_NEAR(singularValues[0], 4.0, 1e-9);
    EXPECT_NEAR(singularValues[1], 2.0, 1e-9);
    EXPECT_NEAR(singularValues[2], 1.0, 1e-9);

    // Condition number should be max(singular values) / min(singular values) =
    // 4/1 = 4
    EXPECT_NEAR(wellConditioned.conditionNumber(), 4.0, 1e-9);

    // Test with an ill-conditioned matrix
    Matrix<double, 3, 3> illConditioned;

    illConditioned(0, 0) = 1000.0;
    illConditioned(0, 1) = 0.0;
    illConditioned(0, 2) = 0.0;
    illConditioned(1, 0) = 0.0;
    illConditioned(1, 1) = 1.0;
    illConditioned(1, 2) = 0.0;
    illConditioned(2, 0) = 0.0;
    illConditioned(2, 1) = 0.0;
    illConditioned(2, 2) = 0.001;

    // Condition number should be approximately 1000/0.001 = 1000000
    EXPECT_NEAR(illConditioned.conditionNumber(), 1000000.0, 1e-3);
}
