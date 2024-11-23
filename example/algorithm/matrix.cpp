#include "matrix.hpp"
#include <iostream>

int main() {
    using namespace atom::algorithm;

    // Create a 3x3 identity matrix
    Matrix<double, 3, 3> identityMatrix = identity<double, 3>();
    std::cout << "Identity Matrix:" << std::endl;
    identityMatrix.print();

    // Create a 3x3 matrix with specific values
    std::array<double, 9> values = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    Matrix<double, 3, 3> matrix(values);
    std::cout << "\nMatrix:" << std::endl;
    matrix.print();

    // Access and modify matrix elements
    matrix(0, 0) = 10;
    std::cout << "\nModified Matrix:" << std::endl;
    matrix.print();

    // Calculate the trace of the matrix
    double trace = matrix.trace();
    std::cout << "\nTrace of the Matrix: " << trace << std::endl;

    // Calculate the Frobenius norm of the matrix
    double frobeniusNorm = matrix.freseniusNorm();
    std::cout << "Frobenius Norm of the Matrix: " << frobeniusNorm << std::endl;

    // Find the maximum and minimum elements in the matrix
    double maxElement = matrix.maxElement();
    double minElement = matrix.minElement();
    std::cout << "Max Element: " << maxElement
              << ", Min Element: " << minElement << std::endl;

    // Check if the matrix is symmetric
    bool isSymmetric = matrix.isSymmetric();
    std::cout << "Is the Matrix Symmetric? " << (isSymmetric ? "Yes" : "No")
              << std::endl;

    // Calculate the power of the matrix
    Matrix<double, 3, 3> matrixPower = matrix.pow(2);
    std::cout << "\nMatrix Power (2):" << std::endl;
    matrixPower.print();

    // Calculate the determinant of the matrix
    double determinant = matrix.determinant();
    std::cout << "Determinant of the Matrix: " << determinant << std::endl;

    // Calculate the rank of the matrix
    std::size_t rank = matrix.rank();
    std::cout << "Rank of the Matrix: " << rank << std::endl;

    // Calculate the condition number of the matrix
    double conditionNumber = matrix.conditionNumber();
    std::cout << "Condition Number of the Matrix: " << conditionNumber
              << std::endl;

    // Perform matrix addition
    Matrix<double, 3, 3> sumMatrix = matrix + identityMatrix;
    std::cout << "\nSum of Matrix and Identity Matrix:" << std::endl;
    sumMatrix.print();

    // Perform matrix subtraction
    Matrix<double, 3, 3> diffMatrix = matrix - identityMatrix;
    std::cout << "\nDifference of Matrix and Identity Matrix:" << std::endl;
    diffMatrix.print();

    // Perform matrix multiplication
    Matrix<double, 3, 3> productMatrix = matrix * identityMatrix;
    std::cout << "\nProduct of Matrix and Identity Matrix:" << std::endl;
    productMatrix.print();

    // Perform scalar multiplication
    Matrix<double, 3, 3> scalarProductMatrix = matrix * 2.0;
    std::cout << "\nScalar Product of Matrix (2.0):" << std::endl;
    scalarProductMatrix.print();

    // Perform Hadamard product
    Matrix<double, 3, 3> hadamardMatrix =
        hadamardProduct(matrix, identityMatrix);
    std::cout << "\nHadamard Product of Matrix and Identity Matrix:"
              << std::endl;
    hadamardMatrix.print();

    // Transpose the matrix
    Matrix<double, 3, 3> transposedMatrix = transpose(matrix);
    std::cout << "\nTransposed Matrix:" << std::endl;
    transposedMatrix.print();

    // Perform LU decomposition
    auto [L, U] = luDecomposition(matrix);
    std::cout << "\nLU Decomposition:" << std::endl;
    std::cout << "L Matrix:" << std::endl;
    L.print();
    std::cout << "U Matrix:" << std::endl;
    U.print();

    // Perform singular value decomposition
    std::vector<double> singularValues = singularValueDecomposition(matrix);
    std::cout << "\nSingular Values:" << std::endl;
    for (const auto& value : singularValues) {
        std::cout << value << " ";
    }
    std::cout << std::endl;

    // Generate a random matrix
    Matrix<double, 3, 3> randomMatrix =
        atom::algorithm::randomMatrix<double, 3, 3>(0.0, 10.0);
    std::cout << "\nRandom Matrix:" << std::endl;
    randomMatrix.print();

    return 0;
}