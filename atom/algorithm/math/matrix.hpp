#ifndef ATOM_ALGORITHM_MATH_MATRIX_HPP
#define ATOM_ALGORITHM_MATH_MATRIX_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

#include "atom/algorithm/core/rust_numeric.hpp"
#include "atom/error/exception.hpp"

namespace atom::algorithm {

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

    // 添加显式复制构造函数
    Matrix(const Matrix& other) {
        std::copy(other.data_.begin(), other.data_.end(), data_.begin());
    }

    // 添加移动构造函数
    Matrix(Matrix&& other) noexcept { data_ = std::move(other.data_); }

    // 添加复制赋值运算符
    Matrix& operator=(const Matrix& other) {
        if (this != &other) {
            std::copy(other.data_.begin(), other.data_.end(), data_.begin());
        }
        return *this;
    }

    // 添加移动赋值运算符
    Matrix& operator=(Matrix&& other) noexcept {
        if (this != &other) {
            data_ = std::move(other.data_);
        }
        return *this;
    }

    /**
     * @brief Accesses the matrix element at the given row and column.
     *
     * @param row The row index.
     * @param col The column index.
     * @return T& A reference to the matrix element.
     */
    constexpr auto operator()(usize row, usize col) -> T& {
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
     * @brief Prints the matrix to the standard output.
     *
     * @param width The width of each element when printed.
     * @param precision The precision of each element when printed.
     */
    void print(i32 width = 8, i32 precision = 2) const {
        for (usize i = 0; i < Rows; ++i) {
            for (usize j = 0; j < Cols; ++j) {
                std::cout << std::setw(width) << std::fixed
                          << std::setprecision(precision) << (*this)(i, j)
                          << ' ';
            }
            std::cout << '\n';
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
        T sum = T{};
        for (const auto& elem : data_) {
            sum += std::norm(elem);
        }
        return std::sqrt(sum);
    }

    /**
     * @brief Finds the maximum element in the matrix.
     *
     * @return T The maximum element in the matrix.
     */
    auto maxElement() const -> T {
        return *std::max_element(
            data_.begin(), data_.end(),
            [](const T& a, const T& b) { return std::abs(a) < std::abs(b); });
    }

    /**
     * @brief Finds the minimum element in the matrix.
     *
     * @return T The minimum element in the matrix.
     */
    auto minElement() const -> T {
        return *std::min_element(
            data_.begin(), data_.end(),
            [](const T& a, const T& b) { return std::abs(a) < std::abs(b); });
    }

    /**
     * @brief Checks if the matrix is symmetric.
     *
     * @return true If the matrix is symmetric.
     * @return false If the matrix is not symmetric.
     */
    [[nodiscard]] auto isSymmetric() const -> bool {
        static_assert(Rows == Cols,
                      "Symmetry is only defined for square matrices");
        for (usize i = 0; i < Rows; ++i) {
            for (usize j = i + 1; j < Cols; ++j) {
                if ((*this)(i, j) != (*this)(j, i)) {
                    return false;
                }
            }
        }
        return true;
    }

    /**
     * @brief Raises the matrix to the power of n.
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
        Matrix result = *this;
        for (u32 i = 1; i < n; ++i) {
            result = result * (*this);
        }
        return result;
    }

    /**
     * @brief Computes the determinant of the matrix using LU decomposition.
     *
     * @return T The determinant of the matrix.
     */
    auto determinant() const -> T {
        static_assert(Rows == Cols,
                      "Determinant is only defined for square matrices");
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
     * @throws std::runtime_error If the matrix is singular (non-invertible).
     */
    auto inverse() const -> Matrix {
        static_assert(Rows == Cols,
                      "Inverse is only defined for square matrices");
        const T det = determinant();
        if (std::abs(det) < 1e-10) {
            THROW_RUNTIME_ERROR("Matrix is singular (non-invertible)");
        }

        auto [L, U] = luDecomposition(*this);
        Matrix<T, Rows, Cols> inv;

        // Solve for each column of the inverse
        for (usize col = 0; col < Cols; ++col) {
            // Create unit vector for this column
            std::array<T, Rows> e{};
            e[col] = T{1};

            // Forward substitution: L * y = e
            std::array<T, Rows> y{};
            for (usize i = 0; i < Rows; ++i) {
                y[i] = e[i];
                for (usize j = 0; j < i; ++j) {
                    y[i] -= L(i, j) * y[j];
                }
                // L has 1's on diagonal, no division needed
            }

            // Backward substitution: U * x = y
            std::array<T, Rows> x{};
            for (usize ii = Rows; ii-- > 0;) {
                x[ii] = y[ii];
                for (usize j = ii + 1; j < Cols; ++j) {
                    x[ii] -= U(ii, j) * x[j];
                }
                x[ii] /= U(ii, ii);
            }

            // Copy result to inverse matrix column
            for (usize i = 0; i < Rows; ++i) {
                inv(i, col) = x[i];
            }
        }

        return inv;
    }

    /**
     * @brief Computes the rank of the matrix using Gaussian elimination.
     *
     * @return usize The rank of the matrix.
     */
    [[nodiscard]] auto rank() const -> usize {
        Matrix<T, Rows, Cols> temp = *this;
        usize rank = 0;
        for (usize i = 0; i < Rows && i < Cols; ++i) {
            // Find the pivot
            usize pivot = i;
            for (usize j = i + 1; j < Rows; ++j) {
                if (std::abs(temp(j, i)) > std::abs(temp(pivot, i))) {
                    pivot = j;
                }
            }
            if (std::abs(temp(pivot, i)) < 1e-10) {
                continue;
            }
            // Swap rows
            if (pivot != i) {
                for (usize j = i; j < Cols; ++j) {
                    std::swap(temp(i, j), temp(pivot, j));
                }
            }
            // Eliminate
            for (usize j = i + 1; j < Rows; ++j) {
                T factor = temp(j, i) / temp(i, i);
                for (usize k = i; k < Cols; ++k) {
                    temp(j, k) -= factor * temp(i, k);
                }
            }
            ++rank;
        }
        return rank;
    }

    /**
     * @brief Computes the condition number of the matrix using the 2-norm.
     *
     * @return T The condition number of the matrix.
     */
    auto conditionNumber() const -> T {
        static_assert(Rows == Cols,
                      "Condition number is only defined for square matrices");
        auto svd = singularValueDecomposition(*this);
        return svd[0] / svd[svd.size() - 1];
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
 */
template <typename T, usize RowsA, usize ColsA_RowsB, usize ColsB>
auto operator*(const Matrix<T, RowsA, ColsA_RowsB>& a,
               const Matrix<T, ColsA_RowsB, ColsB>& b)
    -> Matrix<T, RowsA, ColsB> {
    Matrix<T, RowsA, ColsB> result{};
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
 * @brief Performs LU decomposition of the given matrix.
 *
 * @tparam T The type of the matrix elements.
 * @tparam Size The size of the matrix (Size x Size).
 * @param m The matrix to decompose.
 * @return std::pair<Matrix<T, Size, Size>, Matrix<T, Size, Size>> A pair of
 * matrices (L, U) where L is the lower triangular matrix and U is the upper
 * triangular matrix.
 */
template <typename T, usize Size>
auto luDecomposition(const Matrix<T, Size, Size>& m)
    -> std::pair<Matrix<T, Size, Size>, Matrix<T, Size, Size>> {
    Matrix<T, Size, Size> L = identity<T, Size>();
    Matrix<T, Size, Size> U = m;

    for (usize k = 0; k < Size - 1; ++k) {
        for (usize i = k + 1; i < Size; ++i) {
            if (std::abs(U(k, k)) < 1e-10) {
                THROW_RUNTIME_ERROR(
                    "LU decomposition failed: division by zero");
            }
            T factor = U(i, k) / U(k, k);
            L(i, k) = factor;
            for (usize j = k; j < Size; ++j) {
                U(i, j) -= factor * U(k, j);
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
 * @return std::vector<T> A vector of singular values.
 */
template <typename T, usize Rows, usize Cols>
auto singularValueDecomposition(const Matrix<T, Rows, Cols>& m)
    -> std::vector<T> {
    const usize n = std::min(Rows, Cols);
    Matrix<T, Cols, Rows> mt = transpose(m);
    Matrix<T, Cols, Cols> mtm = mt * m;

    // Power iteration to find eigenvalue and eigenvector
    auto powerIteration = [](Matrix<T, Cols, Cols>& mat, usize max_iter = 1000,
                             T tol = 1e-12) -> std::pair<T, std::vector<T>> {
        std::vector<T> v(Cols);
        // Initialize with a deterministic vector for reproducibility
        for (usize i = 0; i < Cols; ++i) {
            v[i] = T{1} / std::sqrt(static_cast<T>(Cols));
        }

        T lambdaOld = 0;
        for (usize iter = 0; iter < max_iter; ++iter) {
            std::vector<T> vNew(Cols, T{0});
            for (usize i = 0; i < Cols; ++i) {
                for (usize j = 0; j < Cols; ++j) {
                    vNew[i] += mat(i, j) * v[j];
                }
            }

            // Compute Rayleigh quotient (eigenvalue estimate)
            T lambda = T{0};
            for (usize i = 0; i < Cols; ++i) {
                lambda += vNew[i] * v[i];
            }

            // Normalize
            T norm = std::sqrt(std::inner_product(vNew.begin(), vNew.end(),
                                                  vNew.begin(), T{0}));
            if (norm < tol) {
                return {T{0}, v};
            }
            for (auto& x : vNew) {
                x /= norm;
            }

            if (std::abs(lambda - lambdaOld) < tol) {
                return {lambda, vNew};
            }
            lambdaOld = lambda;
            v = vNew;
        }
        // Return best estimate even if not fully converged
        T lambda = T{0};
        std::vector<T> vNew(Cols, T{0});
        for (usize i = 0; i < Cols; ++i) {
            for (usize j = 0; j < Cols; ++j) {
                vNew[i] += mat(i, j) * v[j];
            }
        }
        for (usize i = 0; i < Cols; ++i) {
            lambda += vNew[i] * v[i];
        }
        return {lambda, v};
    };

    constexpr T svdTol = T{1e-12};
    std::vector<T> singularValues;
    for (usize i = 0; i < n; ++i) {
        auto [eigenvalue, eigenvector] = powerIteration(mtm);
        if (eigenvalue < svdTol) {
            singularValues.push_back(T{0});
            continue;
        }
        T sigma = std::sqrt(std::abs(eigenvalue));
        singularValues.push_back(sigma);

        // Deflate: A = A - lambda * v * v^T
        for (usize j = 0; j < Cols; ++j) {
            for (usize k = 0; k < Cols; ++k) {
                mtm(j, k) -= eigenvalue * eigenvector[j] * eigenvector[k];
            }
        }
    }

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
 * @note This function uses a uniform real distribution to generate the random
 * elements. The random number generator is seeded with a random device.
 */
template <typename T, usize Rows, usize Cols>
auto randomMatrix(T min = 0, T max = 1) -> Matrix<T, Rows, Cols> {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(min, max);

    Matrix<T, Rows, Cols> result;
    for (auto& elem : result.getData()) {
        elem = dis(gen);
    }
    return result;
}

}  // namespace atom::algorithm

#endif  // ATOM_ALGORITHM_MATH_MATRIX_HPP
