#include "flood.hpp"

#include "atom/log/loguru.hpp"

namespace atom::algorithm {

[[nodiscard]] auto FloodFill::getDirections(Connectivity conn)
    -> std::vector<std::pair<int, int>> {
    // 使用constexpr static来提高性能，避免重复创建
    constexpr static std::pair<int, int> four_directions[] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}};

    constexpr static std::pair<int, int> eight_directions[] = {
        {-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};

    if (conn == Connectivity::Four) {
        return {std::begin(four_directions), std::end(four_directions)};
    }
    return {std::begin(eight_directions), std::end(eight_directions)};
}

size_t FloodFill::fillBFS(std::vector<std::vector<int>>& grid, int start_x,
                          int start_y, int target_color, int fill_color,
                          Connectivity conn) {
    return fillBFS<std::vector<std::vector<int>>>(
        grid, start_x, start_y, target_color, fill_color, conn);
}

size_t FloodFill::fillDFS(std::vector<std::vector<int>>& grid, int start_x,
                          int start_y, int target_color, int fill_color,
                          Connectivity conn) {
    return fillDFS<std::vector<std::vector<int>>>(
        grid, start_x, start_y, target_color, fill_color, conn);
}

// 实现SIMD和块优化处理方法
#if defined(__x86_64__) || defined(_M_X64)
template <typename T>
size_t FloodFill::processRowSIMD(T* row, int start_idx, int length,
                                 T target_color, T fill_color) {
    size_t filled = 0;

    if constexpr (std::is_same_v<T, int>) {
// Process 8 integers at a time using AVX2
#ifdef __AVX2__
        const int simd_width = 8;
        int i = start_idx;

        // Align to simd_width boundary
        while (i < start_idx + length && (i % simd_width != 0)) {
            if (row[i] == target_color) {
                row[i] = fill_color;
                filled++;
            }
            i++;
        }

        // Process full SIMD widths
        __m256i target_vec = _mm256_set1_epi32(target_color);
        __m256i fill_vec = _mm256_set1_epi32(fill_color);

        for (; i + simd_width <= start_idx + length; i += simd_width) {
            // Load 8 integers
            __m256i current =
                _mm256_loadu_si256(reinterpret_cast<const __m256i*>(row + i));

            // Create mask where current == target_color
            __m256i mask = _mm256_cmpeq_epi32(current, target_vec);

            // Count number of matches (filled pixels)
            int mask_bits = _mm256_movemask_ps(_mm256_castsi256_ps(mask));
            filled += std::popcount(static_cast<unsigned int>(mask_bits));

            // Blend current values with fill_color where mask is set
            __m256i result = _mm256_blendv_epi8(current, fill_vec, mask);

            // Store result back
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(row + i), result);
        }

        // Handle remaining elements
        for (; i < start_idx + length; i++) {
            if (row[i] == target_color) {
                row[i] = fill_color;
                filled++;
            }
        }
#else
        // Fallback for non-AVX systems
        for (int i = start_idx; i < start_idx + length; i++) {
            if (row[i] == target_color) {
                row[i] = fill_color;
                filled++;
            }
        }
#endif
    } else if constexpr (std::is_same_v<T, float>) {
// Process 8 floats at a time using AVX
#ifdef __AVX__
        const int simd_width = 8;
        int i = start_idx;

        // Align to simd_width boundary
        while (i < start_idx + length && (i % simd_width != 0)) {
            if (row[i] == target_color) {
                row[i] = fill_color;
                filled++;
            }
            i++;
        }

        // Process full SIMD widths
        __m256 target_vec = _mm256_set1_ps(target_color);
        __m256 fill_vec = _mm256_set1_ps(fill_color);

        for (; i + simd_width <= start_idx + length; i += simd_width) {
            // Load 8 floats
            __m256 current = _mm256_loadu_ps(row + i);

            // Create mask where current == target_color
            __m256 mask = _mm256_cmp_ps(current, target_vec, _CMP_EQ_OQ);

            // Count number of matches
            int mask_bits = _mm256_movemask_ps(mask);
            filled += std::popcount(static_cast<unsigned int>(mask_bits));

            // Blend current values with fill_color where mask is set
            __m256 result = _mm256_blendv_ps(current, fill_vec, mask);

            // Store result back
            _mm256_storeu_ps(row + i, result);
        }

        // Handle remaining elements
        for (; i < start_idx + length; i++) {
            if (row[i] == target_color) {
                row[i] = fill_color;
                filled++;
            }
        }
#else
        // Fallback for non-AVX systems
        for (int i = start_idx; i < start_idx + length; i++) {
            if (row[i] == target_color) {
                row[i] = fill_color;
                filled++;
            }
        }
#endif
    } else {
        // Generic implementation for other types
        for (int i = start_idx; i < start_idx + length; i++) {
            if (row[i] == target_color) {
                row[i] = fill_color;
                filled++;
            }
        }
    }

    return filled;
}

// 修改显式实例化的语法，确保其引用了正确声明的模板函数
// 注意：这里应该放在命名空间内部
template size_t FloodFill::processRowSIMD<int>(int*, int, int, int, int);
template size_t FloodFill::processRowSIMD<float>(float*, int, int, float,
                                                 float);
template size_t FloodFill::processRowSIMD<uint8_t>(uint8_t*, int, int, uint8_t,
                                                   uint8_t);
#endif

// 修正显式实例化语法
template void FloodFill::validateInput<std::vector<std::vector<int>>>(
    const std::vector<std::vector<int>>& grid, int start_x, int start_y);

// 实现块处理模板函数
template <Grid GridType>
size_t FloodFill::processBlock(
    GridType& grid, int blockX, int blockY, int blockSize,
    typename GridType::value_type::value_type target_color,
    typename GridType::value_type::value_type fill_color, Connectivity conn,
    std::queue<std::pair<int, int>>& borderQueue) {
    size_t filled_count = 0;
    int rows = static_cast<int>(grid.size());
    int cols = static_cast<int>(grid[0].size());

    // 计算块的边界
    int endX = std::min(blockX + blockSize, rows);
    int endY = std::min(blockY + blockSize, cols);

    // 使用BFS处理块
    std::queue<std::pair<int, int>> localQueue;
    std::vector<std::vector<bool>> localVisited(
        blockSize, std::vector<bool>(blockSize, false));

    // 寻找块内任何已填充的像素作为起点
    bool found_start = false;
    for (int x = blockX; x < endX && !found_start; ++x) {
        for (int y = blockY; y < endY && !found_start; ++y) {
            if (grid[x][y] == fill_color) {
                // 检查邻居是否有目标颜色像素
                auto directions = getDirections(conn);
                for (auto [dx, dy] : directions) {
                    int nx = x + dx;
                    int ny = y + dy;

                    if (isInBounds(nx, ny, rows, cols) &&
                        grid[nx][ny] == target_color && nx >= blockX &&
                        nx < endX && ny >= blockY && ny < endY) {
                        localQueue.emplace(nx, ny);
                        localVisited[nx - blockX][ny - blockY] = true;
                        grid[nx][ny] = fill_color;
                        filled_count++;
                        found_start = true;
                    }
                }
            }
        }
    }

    // 在块内执行BFS
    auto directions = getDirections(conn);
    while (!localQueue.empty()) {
        auto [x, y] = localQueue.front();
        localQueue.pop();

        for (auto [dx, dy] : directions) {
            int nx = x + dx;
            int ny = y + dy;

            if (isInBounds(nx, ny, rows, cols) &&
                grid[nx][ny] == target_color) {
                // 检查像素是否在当前块内
                if (nx >= blockX && nx < endX && ny >= blockY && ny < endY) {
                    if (!localVisited[nx - blockX][ny - blockY]) {
                        grid[nx][ny] = fill_color;
                        localQueue.emplace(nx, ny);
                        localVisited[nx - blockX][ny - blockY] = true;
                        filled_count++;
                    }
                } else {
                    // 像素在块外，添加到边界队列
                    borderQueue.emplace(x, y);
                }
            }
        }
    }

    return filled_count;
}

// 修正显式实例化的语法
template size_t FloodFill::processBlock<std::vector<std::vector<int>>>(
    std::vector<std::vector<int>>&, int, int, int, int, int, Connectivity,
    std::queue<std::pair<int, int>>&);

}  // namespace atom::algorithm