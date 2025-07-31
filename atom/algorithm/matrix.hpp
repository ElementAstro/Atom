#ifndef ATOM_ALGORITHM_MATRIX_HPP
#define ATOM_ALGORITHM_MATRIX_HPP

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <complex>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

#include "atom/algorithm/rust_numeric.hpp"
#include "atom/error/exception.hpp"

namespace atom::algorithm {

// Helper type trait to detect std::complex types
template<typename T>
struct is_complex : std::false_type {};

template<typename T>
struct is_complex<std::complex<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_complex_v = is_complex<T>::value;

// Ensure type aliases are available
using usize = std::size_t;
using i32 = std::int32_t;
using u32 = std::uint32_t;

/**
 * @brief Forward declaration of the Matrix class template.
 *
 * @tparam T The type of the matrix elements.
 * @tparam Rows The number of rows in the matrix.
 * @tparam Cols The number of columns in the matrix.
 */
template <typename T, usize Rows, usize Cols>
class Matrix;

/**
 * @brief Creates an identity matrix of the given size.
 *
 * @tparam T The type of the matrix elements.
 * @tparam Size The size of the identity matrix (Size x Size).
 * @return constexpr Matrix<T, Size, Size> The identity matrix.
 */
template <typename T, usize Size>
constexpr Matrix<T, Size, Size> identity();

/**
 * @brief A template class for matrices, supporting compile-time matrix
 * calculations.
 *
 * @tparam T The type of the matrix elements.
 * @tparam Rows The number of rows in the matrix.
 * @tparam Cols The number of columns in the matrix.
 */
template <typename T, usize Rows, usize Cols>
class Matrix {
private:
    std::array<T, Rows * Cols> data_{};
    // Removed static inline std::mutex mutex_;
    // For fixed-size matrices, operations typically return new matrices
    // or are const, making instance-level locking unnecessary for data access.
    // Concurrent modification of a single matrix instance should be managed
    // externally by the caller if needed.

public:
    /**
     * @brief Default constructor.
     */
    constexpr Matrix() = default;

    /**
     * @brief Constructs a matrix from a given array.
     *
     * @param arr The array to initialize the matrix with.
     */
    constexpr explicit Matrix(const std::array<T, Rows * Cols>& arr)
        : data_(arr) {}

    // Explicitly defaulted copy/move constructors and assignment operators
    // are sufficient and often more efficient than manual implementation
    // for simple data members like std::array.
    Matrix(const Matrix& other) = default;
    Matrix(Matrix&& other) noexcept = default;
    Matrix& operator=(const Matrix& other) = default;
    Matrix& operator=(Matrix&& other) noexcept = default;

    /**
     * @brief Accesses the matrix element at the given row and column.
     *
     * @param row The row index.
     * @param col The column index.
     * @return T& A reference to the matrix element.
     */
    constexpr auto operator()(usize row, usize col) -> T& {
        // Use bounds checking in debug builds for safety
        assert(row < Rows && col < Cols && "Matrix index out of bounds");
        return data_[row * Cols + col];
    }

    /**
     * @brief Accesses the matrix element at the given row and column (const
     * version).
     *
     * @param row The row index.
     * @param col The column index.
     * @return const T& A const reference to the matrix element.
     */
    constexpr auto operator()(usize row, usize col) const -> const T& {
        // Use bounds checking in debug builds for safety
        assert(row < Rows && col < Cols && "Matrix index out of bounds");
        return data_[row * Cols + col];
    }

    /**
     * @brief Gets the underlying data array (const version).
     *
     * @return const std::array<T, Rows * Cols>& A const reference to the data
     * array.
     */
    auto getData() const -> const std::array<T, Rows * Cols>& { return data_; }

    /**
     * @brief Gets the underlying data array.
     *
     * @return std::array<T, Rows * Cols>& A reference to the data array.
     */
    auto getData() -> std::array<T, Rows * Cols>& { return data_; }

    /**
     * @brief Prints the matrix to the provided output stream.
     *
     * @param os The output stream to print to.
     * @param width The width of each element when printed.
     * @param precision The precision of each element when printed.
     */
    void print(std::ostream& os = std::cout, i32 width = 8,
               i32 precision = 2) const {
        for (usize i = 0; i < Rows; ++i) {
            for (usize j = 0; j < Cols; ++j) {
                os << std::setw(width) << std::fixed
                   << std::setprecision(precision) << (*this)(i, j) << ' ';
            }
            os << '\n';
        }
    }

    /**
     * @brief Computes the trace of the matrix (sum of diagonal elements).
     *
     * @return constexpr T The trace of the matrix.
     */
    constexpr auto trace() const -> T {
        static_assert(Rows == Cols,
                      "Trace is only defined for square matrices");
        T result = T{};
        for (usize i = 0; i < Rows; ++i) {
            result += (*this)(i, i);
        }
        return result;
    }

    /**
     * @brief Computes the Frobenius norm of the matrix.
     *
     * @return T The Frobenius norm of the matrix.
     */
    auto frobeniusNorm() const -> T {
        T sum_sq = T{};
        // Use std::accumulate with a lambda for potentially better optimization
        sum_sq = std::accumulate(
            data_.begin(), data_.end(), T{}, [](T current_sum, const T& elem) {
                // Use std::norm for complex numbers
                if constexpr (is_complex_v<T>) {
                    return current_sum + std::norm(elem);
                } else {
                    return current_sum + elem * elem;
                }
            });
        return std::sqrt(sum_sq);
    }

    /**
     * @brief Finds the maximum element in the matrix (based on value).
     *
     * @return T The maximum element in the matrix.
     * @throws std::runtime_error if the matrix is empty (though std::array is
     * never empty).
     */
    auto maxElement() const -> T {
        // std::array is never empty, so no need to check
        return *std::max_element(data_.begin(), data_.end());
    }

    /**
     * @brief Finds the minimum element in the matrix (based on value).
     *
     * @return T The minimum element in the matrix.
     * @throws std::runtime_error if the matrix is empty (though std::array is
     * never empty).
     */
    auto minElement() const -> T {
        // std::array is never empty, so no need to check
        return *std::min_element(data_.begin(), data_.end());
    }

    /**
     * @brief Finds the element with the maximum absolute value in the matrix.
     *
     * @return T The element with the maximum absolute value.
     */
    auto maxAbsElement() const -> T {
        return *std::max_element(
            data_.begin(), data_.end(),
            [](const T& a, const T& b) { return std::abs(a) < std::abs(b); });
    }

    /**
     * @brief Finds the element with the minimum absolute value in the matrix.
     *
     * @return T The element with the minimum absolute value.
     */
    auto minAbsElement() const -> T {
        return *std::min_element(
            data_.begin(), data_.end(),
            [](const T& a, const T& b) { return std::abs(a) < std::abs(b); });
    }

    /**
     * @brief Checks if the matrix is symmetric within a given tolerance.
     *
     * @param tolerance The tolerance for floating-point comparison.
     * @return true If the matrix is symmetric within the tolerance.
     * @return false If the matrix is not symmetric.
     */
    [[nodiscard]] auto isSymmetric(T tolerance = 1e-9) const -> bool {
        static_assert(Rows == Cols,
                      "Symmetry is only defined for square matrices");
        for (usize i = 0; i < Rows; ++i) {
            for (usize j = i + 1; j < Cols; ++j) {
                if constexpr (std::is_floating_point_v<T> || is_complex_v<T>) {
                    if (std::abs((*this)(i, j) - (*this)(j, i)) > tolerance) {
                        return false;
                    }
                } else {  // Integral types
                    if ((*this)(i, j) != (*this)(j, i)) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    /**
     * @brief Raises the matrix to the power of n using exponentiation by
     * squaring.
     *
     * @param n The exponent.
     * @return Matrix The resulting matrix after exponentiation.
     */
    auto pow(u32 n) const -> Matrix {
        static_assert(Rows == Cols,
                      "Matrix power is only defined for square matrices");
        if (n == 0) {
            return identity<T, Rows>();
        }
        if (n == 1) {
            return *this;
        }

        Matrix result = identity<T, Rows>();
        Matrix base = *this;

        u32 exponent = n;
        while (exponent > 0) {
            if (exponent % 2 == 1) {
                result = result * base;
            }
            // Optimization: Avoid squaring if base is already the identity
            // matrix
            if (exponent > 1 && base.isIdentity()) {
                break;
            }
            base = base * base;
            exponent /= 2;
        }

        return result;
    }

    /**
     * @brief Checks if the matrix is an identity matrix within a given
     * tolerance.
     *
     * @param tolerance The tolerance for floating-point comparison.
     * @return true If the matrix is an identity matrix within the tolerance.
     * @return false If the matrix is not an identity matrix.
     */
    [[nodiscard]] auto isIdentity(T tolerance = 1e-9) const -> bool {
        static_assert(Rows == Cols,
                      "Identity check is only defined for square matrices");
        for (usize i = 0; i < Rows; ++i) {
            for (usize j = 0; j < Cols; ++j) {
                if (i == j) {
                    if constexpr (std::is_floating_point_v<T> || is_complex_v<T>) {
                        if (std::abs((*this)(i, j) - T{1}) > tolerance)
                            return false;
                    } else {  // Integral types
                        if ((*this)(i, j) != T{1})
                            return false;
                    }
                } else {
                    if constexpr (std::is_floating_point_v<T> || is_complex_v<T>) {
                        if (std::abs((*this)(i, j)) > tolerance)
                            return false;
                    } else {  // Integral types
                        if ((*this)(i, j) != T{0})
                            return false;
                    }
                }
            }
        }
        return true;
    }

    /**
     * @brief Computes the determinant of the matrix using LU decomposition.
     *
     * @return T The determinant of the matrix.
     * @note This implementation is basic and may not be numerically stable for
     * all matrices. For high-performance numerical linear algebra, consider
     * using optimized libraries like LAPACK.
     */
    auto determinant() const -> T {
        static_assert(Rows == Cols,
                      "Determinant is only defined for square matrices");
        // LU decomposition is performed without pivoting in the current
        // luDecomposition function, which can lead to numerical instability
        // and failure for matrices that are singular or near-singular,
        // even if they are invertible with pivoting.
        // A more robust implementation would include partial or full pivoting.
        auto [L, U] = luDecomposition(*this);
        T det = T{1};
        for (usize i = 0; i < Rows; ++i) {
            det *= U(i, i);
        }
        return det;
    }

    /**
     * @brief Computes the inverse of the matrix using LU decomposition.
     *
     * @return Matrix The inverse matrix.
     * @throws std::runtime_error If the matrix is singular (non-invertible)
     * or if LU decomposition fails.
     * @note This implementation is basic and may not be numerically stable for
     * all matrices. For high-performance numerical linear algebra, consider
     * using optimized libraries like LAPACK.
     */
    auto inverse() const -> Matrix {
        static_assert(Rows == Cols,
                      "Inverse is only defined for square matrices");
        const T det = determinant();
        // Using a small tolerance for floating-point comparison
        if constexpr (std::is_floating_point_v<T> || is_complex_v<T>) {
            if (std::abs(det) < 1e-10) {
                THROW_RUNTIME_ERROR("Matrix is singular (non-invertible)");
            }
        } else {  // Integral types
            if (det == T{0}) {
                THROW_RUNTIME_ERROR("Matrix is singular (non-invertible)");
            }
        }

        auto [L, U] = luDecomposition(*this);  // luDecomposition might throw

        Matrix<T, Rows, Cols> inv = identity<T, Rows>();

        // Solve L * Y = I for Y using forward substitution
        // Y is stored in the 'inv' matrix column by column
        for (usize k = 0; k < Cols; ++k) {  // For each column k of I (and Y)
            for (usize i = 0; i < Rows; ++i) {  // For each row i
                T sum = T{0};
                for (usize j = 0; j < i; ++j) {
                    sum += L(i, j) * inv(j, k);
                }
                // L(i, i) is 1 for the standard Doolittle LU decomposition
                // inv(i, k) = (I(i, k) - sum) / L(i, i)
                // Since I(i, k) is 1 if i == k and 0 otherwise, and L(i,i) is
                // 1:
                inv(i, k) = ((i == k ? T{1} : T{0}) - sum);
            }
        }

        // Solve U * X = Y for X using backward substitution
        // X is the inverse matrix, stored in 'inv'
        for (usize k = 0; k < Cols; ++k) {    // For each column k of Y (and X)
            for (usize i = Rows; i-- > 0;) {  // For each row i, from bottom up
                T sum = T{0};
                for (usize j = i + 1; j < Cols; ++j) {
                    sum += U(i, j) * inv(j, k);
                }
                if (std::abs(U(i, i)) < 1e-10) {
                    // This case should ideally be caught by the determinant
                    // check, but as a safeguard during substitution.
                    THROW_RUNTIME_ERROR(
                        "Inverse failed: division by zero during backward "
                        "substitution");
                }
                inv(i, k) = (inv(i, k) - sum) / U(i, i);
            }
        }

        return inv;
    }

    /**
     * @brief Computes the rank of the matrix using Gaussian elimination.
     *
     * @return usize The rank of the matrix.
     * @note This implementation is basic and may not be numerically stable for
     * all matrices, especially for floating-point types. For high-performance
     * numerical linear algebra, consider using optimized libraries like LAPACK.
     */
    [[nodiscard]] auto rank() const -> usize {
        Matrix<T, Rows, Cols> temp = *this;
        usize rank = 0;
        usize pivot_col = 0;  // Track the current column for pivoting

        for (usize i = 0; i < Rows && pivot_col < Cols; ++i) {
            // Find the pivot row in the current column (pivot_col)
            usize pivot_row = i;
            for (usize j = i + 1; j < Rows; ++j) {
                if (std::abs(temp(j, pivot_col)) >
                    std::abs(temp(pivot_row, pivot_col))) {
                    pivot_row = j;
                }
            }

            // If the pivot element is close to zero, move to the next column
            if (std::abs(temp(pivot_row, pivot_col)) < 1e-10) {
                pivot_col++;
                i--;  // Stay on the current row for the next column
                continue;
            }

            // Swap current row with the pivot row
            if (pivot_row != i) {
                for (usize k = pivot_col; k < Cols; ++k) {
                    std::swap(temp(i, k), temp(pivot_row, k));
                }
            }

            // Eliminate elements below the pivot
            for (usize j = i + 1; j < Rows; ++j) {
                T factor = temp(j, pivot_col) / temp(i, pivot_col);
                for (usize k = pivot_col; k < Cols; ++k) {
                    temp(j, k) -= factor * temp(i, k);
                }
            }
            ++rank;
            pivot_col++;  // Move to the next column
        }
        return rank;
    }

    /**
     * @brief Computes the condition number of the matrix using the 2-norm.
     * Requires SVD.
     *
     * @return T The condition number of the matrix.
     * @throws std::runtime_error if the matrix is singular or SVD fails.
     * @note This relies on the basic SVD implementation, which may not be
     * robust or accurate for all matrices.
     */
    auto conditionNumber() const -> T {
        static_assert(Rows == Cols,
                      "Condition number is only defined for square matrices");
        std::vector<T> svd_values = singularValueDecomposition(*this);

        // Singular values are sorted in descending order by
        // singularValueDecomposition
        if (svd_values.empty() || std::abs(svd_values.back()) < 1e-10) {
            THROW_RUNTIME_ERROR(
                "Cannot compute condition number: matrix is singular or SVD "
                "failed");
        }

        return svd_values.front() / svd_values.back();
    }
};

/**
 * @brief Adds two matrices element-wise.
 *
 * @tparam T The type of the matrix elements.
 * @tparam Rows The number of rows in the matrices.
 * @tparam Cols The number of columns in the matrices.
 * @param a The first matrix.
 * @param b The second matrix.
 * @return constexpr Matrix<T, Rows, Cols> The resulting matrix after addition.
 */
template <typename T, usize Rows, usize Cols>
constexpr auto operator+(const Matrix<T, Rows, Cols>& a,
                         const Matrix<T, Rows, Cols>& b)
    -> Matrix<T, Rows, Cols> {
    Matrix<T, Rows, Cols> result{};
    for (usize i = 0; i < Rows * Cols; ++i) {
        result.getData()[i] = a.getData()[i] + b.getData()[i];
    }
    return result;
}

/**
 * @brief Subtracts one matrix from another element-wise.
 *
 * @tparam T The type of the matrix elements.
 * @tparam Rows The number of rows in the matrices.
 * @tparam Cols The number of columns in the matrices.
 * @param a The first matrix.
 * @param b The second matrix.
 * @return constexpr Matrix<T, Rows, Cols> The resulting matrix after
 * subtraction.
 */
template <typename T, usize Rows, usize Cols>
constexpr auto operator-(const Matrix<T, Rows, Cols>& a,
                         const Matrix<T, Rows, Cols>& b)
    -> Matrix<T, Rows, Cols> {
    Matrix<T, Rows, Cols> result{};
    for (usize i = 0; i < Rows * Cols; ++i) {
        result.getData()[i] = a.getData()[i] - b.getData()[i];
    }
    return result;
}

/**
 * @brief Multiplies two matrices.
 *
 * @tparam T The type of the matrix elements.
 * @tparam RowsA The number of rows in the first matrix.
 * @tparam ColsA_RowsB The number of columns in the first matrix and the number
 * of rows in the second matrix.
 * @tparam ColsB The number of columns in the second matrix.
 * @param a The first matrix.
 * @param b The second matrix.
 * @return Matrix<T, RowsA, ColsB> The resulting matrix after multiplication.
 * @note For larger matrices, performance can be significantly improved by
 * using techniques like loop tiling/blocking for cache efficiency or
 * leveraging SIMD instructions or optimized libraries (e.g., BLAS).
 */
template <typename T, usize RowsA, usize ColsA_RowsB, usize ColsB>
auto operator*(const Matrix<T, RowsA, ColsA_RowsB>& a,
               const Matrix<T, ColsA_RowsB, ColsB>& b)
    -> Matrix<T, RowsA, ColsB> {
    Matrix<T, RowsA, ColsB> result{};
    // Standard triple nested loop for matrix multiplication
    for (usize i = 0; i < RowsA; ++i) {
        for (usize j = 0; j < ColsB; ++j) {
            for (usize k = 0; k < ColsA_RowsB; ++k) {
                result(i, j) += a(i, k) * b(k, j);
            }
        }
    }
    return result;
}

/**
 * @brief Multiplies a matrix by a scalar (left multiplication).
 *
 * @tparam T The type of the matrix elements.
 * @tparam U The type of the scalar.
 * @tparam Rows The number of rows in the matrix.
 * @tparam Cols The number of columns in the matrix.
 * @param m The matrix.
 * @param scalar The scalar.
 * @return constexpr auto The resulting matrix after multiplication.
 */
template <typename T, typename U, usize Rows, usize Cols>
constexpr auto operator*(const Matrix<T, Rows, Cols>& m, U scalar) {
    Matrix<decltype(T{} * U{}), Rows, Cols> result;
    for (usize i = 0; i < Rows * Cols; ++i) {
        result.getData()[i] = m.getData()[i] * scalar;
    }
    return result;
}

/**
 * @brief Multiplies a scalar by a matrix (right multiplication).
 *
 * @tparam T The type of the matrix elements.
 * @tparam U The type of the scalar.
 * @tparam Rows The number of rows in the matrix.
 * @tparam Cols The number of columns in the matrix.
 * @param scalar The scalar.
 * @param m The matrix.
 * @return constexpr auto The resulting matrix after multiplication.
 */
template <typename T, typename U, usize Rows, usize Cols>
constexpr auto operator*(U scalar, const Matrix<T, Rows, Cols>& m) {
    return m * scalar;
}

/**
 * @brief Computes the Hadamard product (element-wise multiplication) of two
 * matrices.
 *
 * @tparam T The type of the matrix elements.
 * @tparam Rows The number of rows in the matrices.
 * @tparam Cols The number of columns in the matrices.
 * @param a The first matrix.
 * @param b The second matrix.
 * @return constexpr Matrix<T, Rows, Cols> The resulting matrix after Hadamard
 * product.
 */
template <typename T, usize Rows, usize Cols>
constexpr auto elementWiseProduct(const Matrix<T, Rows, Cols>& a,
                                  const Matrix<T, Rows, Cols>& b)
    -> Matrix<T, Rows, Cols> {
    Matrix<T, Rows, Cols> result{};
    for (usize i = 0; i < Rows * Cols; ++i) {
        result.getData()[i] = a.getData()[i] * b.getData()[i];
    }
    return result;
}

/**
 * @brief Transposes the given matrix.
 *
 * @tparam T The type of the matrix elements.
 * @tparam Rows The number of rows in the matrix.
 * @tparam Cols The number of columns in the matrix.
 * @param m The matrix to transpose.
 * @return constexpr Matrix<T, Cols, Rows> The transposed matrix.
 */
template <typename T, usize Rows, usize Cols>
constexpr auto transpose(const Matrix<T, Rows, Cols>& m)
    -> Matrix<T, Cols, Rows> {
    Matrix<T, Cols, Rows> result{};
    for (usize i = 0; i < Rows; ++i) {
        for (usize j = 0; j < Cols; ++j) {
            result(j, i) = m(i, j);
        }
    }
    return result;
}

/**
 * @brief Creates an identity matrix of the given size.
 *
 * @tparam T The type of the matrix elements.
 * @tparam Size The size of the identity matrix (Size x Size).
 * @return constexpr Matrix<T, Size, Size> The identity matrix.
 */
template <typename T, usize Size>
constexpr auto identity() -> Matrix<T, Size, Size> {
    Matrix<T, Size, Size> result{};
    for (usize i = 0; i < Size; ++i) {
        result(i, i) = T{1};
    }
    return result;
}

/**
 * @brief Performs LU decomposition of the given matrix (without pivoting).
 *
 * @tparam T The type of the matrix elements.
 * @tparam Size The size of the matrix (Size x Size).
 * @param m The matrix to decompose.
 * @return std::pair<Matrix<T, Size, Size>, Matrix<T, Size, Size>> A pair of
 * matrices (L, U) where L is the lower triangular matrix and U is the upper
 * triangular matrix. L has 1s on the diagonal.
 * @throws std::runtime_error if division by zero occurs (matrix is singular
 * or requires pivoting).
 * @note This is a basic Doolittle LU decomposition without pivoting. It may
 * fail or produce incorrect results for matrices that require row swaps
 * (pivoting) for numerical stability or to avoid division by zero. For a
 * robust implementation, consider partial or full pivoting, or use optimized
 * libraries like LAPACK.
 */
template <typename T, usize Size>
auto luDecomposition(const Matrix<T, Size, Size>& m)
    -> std::pair<Matrix<T, Size, Size>, Matrix<T, Size, Size>> {
    Matrix<T, Size, Size> L = identity<T, Size>();
    Matrix<T, Size, Size> U = m;

    for (usize k = 0; k < Size; ++k) {  // k is the pivot row/column index
        // Check pivot element in U
        if constexpr (std::is_floating_point_v<T> || is_complex_v<T>) {
            if (std::abs(U(k, k)) < 1e-10) {
                THROW_RUNTIME_ERROR(
                    "LU decomposition failed: pivot element is zero or near "
                    "zero. Matrix may be singular or require pivoting.");
            }
        } else {  // Integral types
            if (U(k, k) == T{0}) {
                THROW_RUNTIME_ERROR(
                    "LU decomposition failed: pivot element is zero. Matrix is "
                    "singular or requires pivoting.");
            }
        }

        for (usize i = k + 1; i < Size;
             ++i) {  // i is the row index below the pivot
            T factor = U(i, k) / U(k, k);
            L(i, k) = factor;                   // Store the multiplier in L
            for (usize j = k; j < Size; ++j) {  // j is the column index
                U(i, j) -= factor * U(k, j);    // Perform row operation on U
            }
        }
    }

    return {L, U};
}

/**
 * @brief Performs singular value decomposition (SVD) of the given matrix and
 * returns the singular values.
 *
 * @tparam T The type of the matrix elements.
 * @tparam Rows The number of rows in the matrix.
 * @tparam Cols The number of columns in the matrix.
 * @param m The matrix to decompose.
 * @return std::vector<T> A vector of singular values, sorted in descending
 * order.
 * @note This is a simplified implementation that computes singular values by
 * finding the square roots of the eigenvalues of M^T * M using a basic
 * power iteration method with deflation. This approach is generally less
 * robust, less accurate, and slower than standard SVD algorithms (e.g.,
 * QR algorithm, Jacobi method) and may fail for certain matrices. For
 * high-performance and reliable SVD, consider using optimized libraries
 * like LAPACK.
 */
template <typename T, usize Rows, usize Cols>
auto singularValueDecomposition(const Matrix<T, Rows, Cols>& m)
    -> std::vector<T> {
    const usize n = std::min(Rows, Cols);
    if (n == 0)
        return {};

    Matrix<T, Cols, Rows> mt = transpose(m);
    Matrix<T, Cols, Cols> mtm = mt * m;  // Compute M^T * M

    std::vector<T> singularValues;
    singularValues.reserve(n);

    // Basic power iteration to find the largest eigenvalue of MTM
    // and deflation to find subsequent eigenvalues.
    // This is a very simplified approach for demonstration.
    auto powerIteration_with_deflation = [&](Matrix<T, Cols, Cols>& current_mtm,
                                             usize max_iter = 1000,
                                             T tol = 1e-10) -> T {
        std::vector<T> v(Cols);
        // Initialize with random vector using thread-local RNG
        thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<> dist(0.0, 1.0);
        std::generate(v.begin(), v.end(),
                      [&]() { return static_cast<T>(dist(gen)); });

        T lambda_old = T{0};

        for (usize iter = 0; iter < max_iter; ++iter) {
            std::vector<T> v_new(Cols, T{0});
            // v_new = current_mtm * v
            for (usize i = 0; i < Cols; ++i) {
                for (usize j = 0; j < Cols; ++j) {
                    v_new[i] += current_mtm(i, j) * v[j];
                }
            }

            // Calculate eigenvalue (Rayleigh quotient)
            T v_new_dot_v =
                std::inner_product(v_new.begin(), v_new.end(), v.begin(), T{0});
            T v_dot_v = std::inner_product(v.begin(), v.end(), v.begin(), T{0});

            T lambda = T{0};
            if constexpr (std::is_floating_point_v<T> || is_complex_v<T>) {
                if (std::abs(v_dot_v) > 1e-15) {  // Avoid division by zero
                    lambda = v_new_dot_v / v_dot_v;
                } else {
                    // Vector is zero, cannot converge
                    return T{0};
                }
            } else {  // Integral types
                if (v_dot_v != T{0}) {
                    lambda = v_new_dot_v /
                             v_dot_v;  // Integer division might not be suitable
                } else {
                    return T{0};
                }
            }

            // Normalize v_new
            T norm_v_new = std::sqrt(std::inner_product(
                v_new.begin(), v_new.end(), v_new.begin(), T{0}));
            if constexpr (std::is_floating_point_v<T> || is_complex_v<T>) {
                if (std::abs(norm_v_new) > 1e-15) {  // Avoid division by zero
                    for (auto& val : v_new) {
                        val /= norm_v_new;
                    }
                } else {
                    // Vector is zero, cannot converge
                    return T{0};
                }
            } else {  // Integral types
                if (norm_v_new != T{0}) {
                    for (auto& val : v_new) {
                        val /= norm_v_new;  // Integer division might not be
                                            // suitable
                    }
                } else {
                    return T{0};
                }
            }

            // Check for convergence
            if constexpr (std::is_floating_point_v<T> || is_complex_v<T>) {
                if (std::abs(lambda - lambda_old) < tol) {
                    // Deflate the matrix: current_mtm = current_mtm - lambda *
                    // v * v^T
                    Matrix<T, Cols, Cols> outer_product;
                    for (usize r = 0; r < Cols; ++r) {
                        for (usize c = 0; c < Cols; ++c) {
                            outer_product(r, c) = v_new[r] * v_new[c];
                        }
                    }
                    current_mtm = current_mtm - (outer_product * lambda);
                    return std::sqrt(std::abs(
                        lambda));  // Singular value is sqrt of eigenvalue
                }
            } else {  // Integral types - convergence check and deflation need
                      // careful consideration
                if (lambda == lambda_old) {
                    // Deflate the matrix: current_mtm = current_mtm - lambda *
                    // v * v^T
                    Matrix<T, Cols, Cols> outer_product;
                    for (usize r = 0; r < Cols; ++r) {
                        for (usize c = 0; c < Cols; ++c) {
                            outer_product(r, c) = v_new[r] * v_new[c];
                        }
                    }
                    current_mtm = current_mtm - (outer_product * lambda);
                    // Note: sqrt of integral lambda might not be integral
                    return static_cast<T>(
                        std::sqrt(static_cast<double>(lambda)));
                }
            }

            lambda_old = lambda;
            v = v_new;
        }
        // If it didn't converge, return 0 or throw, depending on desired
        // behavior For simplicity here, return 0. A real SVD would handle this
        // better.
        return T{0};
    };

    // Extract n singular values
    Matrix<T, Cols, Cols> current_mtm = mtm;  // Work on a copy for deflation
    for (usize i = 0; i < n; ++i) {
        T sigma = powerIteration_with_deflation(current_mtm);
        // Only add positive singular values (or values above a tolerance)
        if constexpr (std::is_floating_point_v<T> || is_complex_v<T>) {
            if (std::abs(sigma) > 1e-10) {
                singularValues.push_back(
                    std::abs(sigma));  // Singular values are non-negative
            }
        } else {  // Integral types
            if (sigma > T{0}) {
                singularValues.push_back(sigma);
            }
        }
    }

    // Sort singular values in descending order
    std::sort(singularValues.begin(), singularValues.end(), std::greater<T>());

    return singularValues;
}

/**
 * @brief Generates a random matrix with elements in the specified range.
 *
 * This function creates a matrix of the specified dimensions (Rows x Cols)
 * with elements of type T. The elements are randomly generated within the
 * range [min, max).
 *
 * @tparam T The type of the elements in the matrix.
 * @tparam Rows The number of rows in the matrix.
 * @tparam Cols The number of columns in the matrix.
 * @param min The minimum value for the random elements (inclusive). Default is
 * 0.
 * @param max The maximum value for the random elements (exclusive). Default
 * is 1.
 * @return Matrix<T, Rows, Cols> A matrix with randomly generated elements.
 *
 * @note This function uses a uniform real distribution for floating-point
 * types and a uniform integer distribution for integral types. A thread-local
 * random number generator is used for better performance in multi-threaded
 * scenarios.
 */
template <typename T, usize Rows, usize Cols>
auto randomMatrix(T min = 0, T max = 1) -> Matrix<T, Rows, Cols> {
    // Use thread_local for the random number generator to avoid contention
    thread_local std::mt19937 gen(std::random_device{}());

    Matrix<T, Rows, Cols> result;

    if constexpr (std::is_floating_point_v<T>) {
        std::uniform_real_distribution<T> dis(min, max);
        for (auto& elem : result.getData()) {
            elem = dis(gen);
        }
    } else if constexpr (std::is_integral_v<T>) {
        // For integral types, distribution range is inclusive [min, max]
        std::uniform_int_distribution<T> dis(min, max);
        for (auto& elem : result.getData()) {
            elem = dis(gen);
        }
    } else if constexpr (is_complex_v<T>) {
        using RealT = typename T::value_type;
        std::uniform_real_distribution<RealT> dis_real(static_cast<RealT>(min),
                                                       static_cast<RealT>(max));
        std::uniform_real_distribution<RealT> dis_imag(
            static_cast<RealT>(min),
            static_cast<RealT>(
                max));  // Or a different range for imaginary part? Assuming
                        // same range for simplicity.
        for (auto& elem : result.getData()) {
            elem = T(dis_real(gen), dis_imag(gen));
        }
    }
    // Add more type specializations if needed (e.g., custom numeric types)

    return result;
}

}  // namespace atom::algorithm

#endif
