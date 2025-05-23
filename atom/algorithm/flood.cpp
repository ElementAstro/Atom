#include "flood.hpp"

#include <spdlog/spdlog.h>

namespace atom::algorithm {

[[nodiscard]] auto FloodFill::getDirections(Connectivity conn)
    -> std::vector<std::pair<i32, i32>> {
    // Using constexpr static to improve performance and avoid repeated creation
    constexpr static std::pair<i32, i32> four_directions[] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}};

    constexpr static std::pair<i32, i32> eight_directions[] = {
        {-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};

    if (conn == Connectivity::Four) {
        return {std::begin(four_directions), std::end(four_directions)};
    }
    return {std::begin(eight_directions), std::end(eight_directions)};
}

// 修正：提供非模板函数的完整实现
usize FloodFill::fillBFS(std::vector<std::vector<i32>>& grid, i32 start_x,
                         i32 start_y, i32 target_color, i32 fill_color,
                         Connectivity conn) {
    // 直接实现而不是调用模板版本
    spdlog::info("Starting specialized BFS Flood Fill at position ({}, {})",
                 start_x, start_y);

    usize filled_cells = 0;  // Counter for filled cells

    try {
        if (grid.empty() || grid[0].empty()) {
            THROW_INVALID_ARGUMENT("Grid cannot be empty");
        }

        i32 rows = static_cast<i32>(grid.size());
        i32 cols = static_cast<i32>(grid[0].size());

        if (start_x < 0 || start_x >= rows || start_y < 0 || start_y >= cols) {
            THROW_INVALID_ARGUMENT("Starting coordinates out of bounds");
        }

        if (grid[static_cast<usize>(start_x)][static_cast<usize>(start_y)] !=
                target_color ||
            target_color == fill_color) {
            spdlog::warn(
                "Start position does not match target color or target color is "
                "the same as fill color");
            return filled_cells;
        }

        const auto directions = getDirections(conn);
        std::queue<std::pair<i32, i32>> toVisitQueue;

        toVisitQueue.emplace(start_x, start_y);
        grid[static_cast<usize>(start_x)][static_cast<usize>(start_y)] =
            fill_color;
        filled_cells++;

        while (!toVisitQueue.empty()) {
            auto [x, y] = toVisitQueue.front();
            toVisitQueue.pop();
            spdlog::debug("Filling position ({}, {})", x, y);

            for (const auto& [dx, dy] : directions) {
                i32 newX = x + dx;
                i32 newY = y + dy;

                if (newX >= 0 && newX < rows && newY >= 0 && newY < cols &&
                    grid[static_cast<usize>(newX)][static_cast<usize>(newY)] ==
                        target_color) {
                    grid[static_cast<usize>(newX)][static_cast<usize>(newY)] =
                        fill_color;
                    filled_cells++;
                    toVisitQueue.emplace(newX, newY);
                    spdlog::debug("Adding position ({}, {}) to queue", newX,
                                  newY);
                }
            }
        }

        return filled_cells;
    } catch (const std::exception& e) {
        spdlog::error("Exception in fillBFS: {}", e.what());
        throw;
    }
}

usize FloodFill::fillDFS(std::vector<std::vector<i32>>& grid, i32 start_x,
                         i32 start_y, i32 target_color, i32 fill_color,
                         Connectivity conn) {
    // 直接实现而不是调用模板版本
    spdlog::info("Starting specialized DFS Flood Fill at position ({}, {})",
                 start_x, start_y);

    usize filled_cells = 0;

    try {
        if (grid.empty() || grid[0].empty()) {
            THROW_INVALID_ARGUMENT("Grid cannot be empty");
        }

        i32 rows = static_cast<i32>(grid.size());
        i32 cols = static_cast<i32>(grid[0].size());

        if (start_x < 0 || start_x >= rows || start_y < 0 || start_y >= cols) {
            THROW_INVALID_ARGUMENT("Starting coordinates out of bounds");
        }

        if (grid[static_cast<usize>(start_x)][static_cast<usize>(start_y)] !=
                target_color ||
            target_color == fill_color) {
            spdlog::warn(
                "Start position does not match target color or target color is "
                "the same as fill color");
            return filled_cells;
        }

        auto directions = getDirections(conn);
        std::stack<std::pair<i32, i32>> toVisitStack;

        toVisitStack.emplace(start_x, start_y);
        grid[static_cast<usize>(start_x)][static_cast<usize>(start_y)] =
            fill_color;
        filled_cells++;

        while (!toVisitStack.empty()) {
            auto [x, y] = toVisitStack.top();
            toVisitStack.pop();
            spdlog::debug("Filling position ({}, {})", x, y);

            for (auto [dx, dy] : directions) {
                i32 newX = x + dx;
                i32 newY = y + dy;

                if (newX >= 0 && newX < rows && newY >= 0 && newY < cols &&
                    grid[static_cast<usize>(newX)][static_cast<usize>(newY)] ==
                        target_color) {
                    grid[static_cast<usize>(newX)][static_cast<usize>(newY)] =
                        fill_color;
                    filled_cells++;
                    toVisitStack.emplace(newX, newY);
                    spdlog::debug("Adding position ({}, {}) to stack", newX,
                                  newY);
                }
            }
        }

        return filled_cells;
    } catch (const std::exception& e) {
        spdlog::error("Exception in fillDFS: {}", e.what());
        throw;
    }
}

// Implementation of SIMD and block optimization methods
#if defined(__x86_64__) || defined(_M_X64)
template <typename T>
usize FloodFill::processRowSIMD(T* row, i32 start_idx, i32 length,
                                T target_color, T fill_color) {
    usize filled = 0;

    if constexpr (std::is_same_v<T, i32>) {
// Process 8 integers at a time using AVX2
#ifdef __AVX2__
        const i32 simd_width = 8;
        i32 i = start_idx;

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
            i32 mask_bits = _mm256_movemask_ps(_mm256_castsi256_ps(mask));
            filled += std::popcount(static_cast<u32>(mask_bits));

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
        for (i32 i = start_idx; i < start_idx + length; i++) {
            if (row[i] == target_color) {
                row[i] = fill_color;
                filled++;
            }
        }
#endif
    } else if constexpr (std::is_same_v<T, f32>) {
// Process 8 floats at a time using AVX
#ifdef __AVX__
        const i32 simd_width = 8;
        i32 i = start_idx;

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
            i32 mask_bits = _mm256_movemask_ps(mask);
            filled += std::popcount(static_cast<u32>(mask_bits));

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
        for (i32 i = start_idx; i < start_idx + length; i++) {
            if (row[i] == target_color) {
                row[i] = fill_color;
                filled++;
            }
        }
#endif
    } else {
        // Generic implementation for other types
        for (i32 i = start_idx; i < start_idx + length; i++) {
            if (row[i] == target_color) {
                row[i] = fill_color;
                filled++;
            }
        }
    }

    return filled;
}

// Explicit template instantiations with correct rust numeric types
template usize FloodFill::processRowSIMD<i32>(i32*, i32, i32, i32, i32);
template usize FloodFill::processRowSIMD<f32>(f32*, i32, i32, f32, f32);
template usize FloodFill::processRowSIMD<u8>(u8*, i32, i32, u8, u8);
#endif

// Implementation of block processing template function
template <Grid GridType>
usize FloodFill::processBlock(
    GridType& grid, i32 blockX, i32 blockY, i32 blockSize,
    typename GridType::value_type::value_type target_color,
    typename GridType::value_type::value_type fill_color, Connectivity conn,
    std::queue<std::pair<i32, i32>>& borderQueue) {
    usize filled_count = 0;
    i32 rows = static_cast<i32>(grid.size());
    i32 cols = static_cast<i32>(grid[0].size());

    // Calculate block boundaries
    i32 endX = std::min(blockX + blockSize, rows);
    i32 endY = std::min(blockY + blockSize, cols);

    // Use BFS to process the block
    std::queue<std::pair<i32, i32>> localQueue;
    std::vector<std::vector<bool>> localVisited(
        static_cast<usize>(blockSize),
        std::vector<bool>(static_cast<usize>(blockSize), false));

    // Find any already filled pixel in the block to use as starting point
    bool found_start = false;
    for (i32 x = blockX; x < endX && !found_start; ++x) {
        for (i32 y = blockY; y < endY && !found_start; ++y) {
            if (grid[static_cast<usize>(x)][static_cast<usize>(y)] ==
                fill_color) {
                // Check neighbors for target color pixels
                auto directions = getDirections(conn);
                for (auto [dx, dy] : directions) {
                    i32 nx = x + dx;
                    i32 ny = y + dy;

                    if (isInBounds(nx, ny, rows, cols) &&
                        grid[static_cast<usize>(nx)][static_cast<usize>(ny)] ==
                            target_color &&
                        nx >= blockX && nx < endX && ny >= blockY &&
                        ny < endY) {
                        localQueue.emplace(nx, ny);
                        localVisited[static_cast<usize>(nx - blockX)]
                                    [static_cast<usize>(ny - blockY)] = true;
                        grid[static_cast<usize>(nx)][static_cast<usize>(ny)] =
                            fill_color;
                        filled_count++;
                        found_start = true;
                    }
                }
            }
        }
    }

    // Perform BFS within the block
    auto directions = getDirections(conn);
    while (!localQueue.empty()) {
        auto [x, y] = localQueue.front();
        localQueue.pop();

        for (auto [dx, dy] : directions) {
            i32 nx = x + dx;
            i32 ny = y + dy;

            if (isInBounds(nx, ny, rows, cols) &&
                grid[static_cast<usize>(nx)][static_cast<usize>(ny)] ==
                    target_color) {
                // Check if the pixel is within the current block
                if (nx >= blockX && nx < endX && ny >= blockY && ny < endY) {
                    if (!localVisited[static_cast<usize>(nx - blockX)]
                                     [static_cast<usize>(ny - blockY)]) {
                        grid[static_cast<usize>(nx)][static_cast<usize>(ny)] =
                            fill_color;
                        localQueue.emplace(nx, ny);
                        localVisited[static_cast<usize>(nx - blockX)]
                                    [static_cast<usize>(ny - blockY)] = true;
                        filled_count++;
                    }
                } else {
                    // Pixel is outside the block, add to border queue
                    borderQueue.emplace(x, y);
                }
            }
        }
    }

    return filled_count;
}

}  // namespace atom::algorithm