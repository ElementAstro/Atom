#include "difflib.hpp"

#include <algorithm>
#include <cmath>
#include <execution>
#include <format>
#include <future>
#include <mutex>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#include "atom/search/lru.hpp"

namespace atom::utils {

// Forward declarations for algorithm implementations
namespace algorithms {
// Myers差异算法实现
class MyersDiff {
public:
    MyersDiff(std::string_view a, std::string_view b) : a_(a), b_(b) {}

    auto execute() -> std::vector<std::tuple<std::string, int, int, int, int>> {
        auto start_time = std::chrono::high_resolution_clock::now();

        auto result = calculateDiff();

        auto end_time = std::chrono::high_resolution_clock::now();
        stats_.duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);

        return result;
    }

    [[nodiscard]] auto getStats() const -> const DiffStats& { return stats_; }

private:
    std::string_view a_;
    std::string_view b_;
    DiffStats stats_;

    // Myers差异算法的核心实现
    auto calculateDiff()
        -> std::vector<std::tuple<std::string, int, int, int, int>> {
        const int n = static_cast<int>(a_.size());
        const int m = static_cast<int>(b_.size());

        // 特殊情况处理
        if (n == 0 && m == 0) {
            return {};
        }
        if (n == 0) {
            stats_.insertions = m;
            return {std::make_tuple("insert", 0, 0, 0, m)};
        }
        if (m == 0) {
            stats_.deletions = n;
            return {std::make_tuple("delete", 0, n, 0, 0)};
        }

        // 计算编辑图
        const int max_edit = n + m;  // 最大可能编辑数
        std::vector<int> v(2 * max_edit + 1, 0);
        std::vector<std::vector<int>> traces;

        int x, y;
        for (int d = 0; d <= max_edit; d++) {
            traces.emplace_back(v);

            for (int k = -d; k <= d; k += 2) {
                if (k == -d ||
                    (k != d && v[k - 1 + max_edit] < v[k + 1 + max_edit])) {
                    x = v[k + 1 + max_edit];
                } else {
                    x = v[k - 1 + max_edit] + 1;
                }

                y = x - k;

                // 沿对角线延伸匹配
                while (x < n && y < m && a_[x] == b_[y]) {
                    x++;
                    y++;
                }

                v[k + max_edit] = x;

                if (x >= n && y >= m) {
                    // 回溯路径生成差异
                    return backtrackPath(traces, n, m, max_edit);
                }
            }
        }

        // 回溯路径以生成差异
        return backtrackPath(traces, n, m, max_edit);
    }

    // 从编辑轨迹中回溯路径，生成差异操作
    auto backtrackPath(const std::vector<std::vector<int>>& traces, int n,
                       int m, int max_edit)
        -> std::vector<std::tuple<std::string, int, int, int, int>> {
        std::vector<std::tuple<std::string, int, int, int, int>> opcodes;
        int x = n;
        int y = m;

        // 从后向前遍历每个编辑步骤
        for (int d = static_cast<int>(traces.size()) - 1; d >= 0; d--) {
            const auto& v = traces[d];
            int k = x - y;

            int prev_k, prev_x, prev_y;

            if (k == -d ||
                (k != d && v[k - 1 + max_edit] < v[k + 1 + max_edit])) {
                prev_k = k + 1;
            } else {
                prev_k = k - 1;
            }

            prev_x = v[prev_k + max_edit];
            prev_y = prev_x - prev_k;

            // 处理对角线移动（匹配内容）
            while (x > prev_x && y > prev_y) {
                stats_.modifications++;
                x--;
                y--;
            }

            // 处理垂直移动（插入）或水平移动（删除）
            if (d > 0) {
                if (prev_x == x) {  // 垂直移动（插入）
                    stats_.insertions++;
                    opcodes.emplace_back("insert", x, x, y - 1, y);
                } else {  // 水平移动（删除）
                    stats_.deletions++;
                    opcodes.emplace_back("delete", x - 1, x, y, y);
                }
            }

            x = prev_x;
            y = prev_y;
        }

        // 计算相似度
        int total = stats_.insertions + stats_.deletions + stats_.modifications;
        if (total > 0) {
            stats_.similarity =
                static_cast<double>(stats_.modifications) / total;
        } else {
            stats_.similarity = 1.0;  // 完全相同
        }

        // 反转操作码，使它们按正向顺序排列
        std::reverse(opcodes.begin(), opcodes.end());

        // 合并相邻的相同类型操作
        if (!opcodes.empty()) {
            std::vector<std::tuple<std::string, int, int, int, int>> merged;
            merged.reserve(opcodes.size());

            auto current = opcodes[0];
            for (size_t i = 1; i < opcodes.size(); ++i) {
                const auto& op = opcodes[i];
                if (std::get<0>(current) == std::get<0>(op) &&
                    std::get<2>(current) == std::get<1>(op) &&
                    std::get<4>(current) == std::get<3>(op)) {
                    // 合并连续的相同类型操作
                    std::get<2>(current) = std::get<2>(op);
                    std::get<4>(current) = std::get<4>(op);
                } else {
                    merged.push_back(current);
                    current = op;
                }
            }
            merged.push_back(current);

            // 添加等价区域标记
            auto result = addEqualBlocks(merged, n, m);
            return result;
        }

        return {};
    }

    // 添加等价区域标记，完成完整的差异表示
    auto addEqualBlocks(
        const std::vector<std::tuple<std::string, int, int, int, int>>& ops,
        int n, int m)
        -> std::vector<std::tuple<std::string, int, int, int, int>> {
        std::vector<std::tuple<std::string, int, int, int, int>> result;
        result.reserve(ops.size() * 2);

        int last_a = 0;
        int last_b = 0;

        for (const auto& op : ops) {
            const int a_start = std::get<1>(op);
            const int b_start = std::get<3>(op);

            // 添加等价区域（如果存在）
            if (a_start > last_a || b_start > last_b) {
                result.emplace_back("equal", last_a, a_start, last_b, b_start);
            }

            // 添加当前操作
            result.push_back(op);

            last_a = std::get<2>(op);
            last_b = std::get<4>(op);
        }

        // 检查最后一个等价区域
        if (last_a < n || last_b < m) {
            result.emplace_back("equal", last_a, n, last_b, m);
        }

        return result;
    }
};
class PatienceDiff;
class HistogramDiff;
}  // namespace algorithms

static auto joinLines(std::span<const std::string> lines) -> std::string {
    std::string joined;
    joined.reserve(std::accumulate(lines.begin(), lines.end(), size_t{0},
                                   [](size_t sum, const std::string& s) {
                                       return sum + s.size() +
                                              1;  // +1 for newline
                                   }));

    for (const auto& line : lines) {
        joined += line + '\n';
    }
    return joined;
}

class SequenceMatcher::Impl {
public:
    Impl(std::string_view str1, std::string_view str2,
         const DiffOptions& options)
        : seq1_(str1), seq2_(str2), options_(options) {
        computeMatchingBlocks();
    }

    void setSeqs(std::string_view str1, std::string_view str2) {
        seq1_ = str1;
        seq2_ = str2;
        matching_blocks.clear();
        computeMatchingBlocks();
    }

    void setOptions(const DiffOptions& options) { options_ = options; }
    const DiffStats& getStats() const { return stats_; }
    void clearCache() { matching_blocks.clear(); }

    [[nodiscard]] auto ratio() const noexcept -> double {
        const double matches = sumMatchingBlocks();
        const double total_size =
            static_cast<double>(seq1_.size() + seq2_.size());

        if (total_size == 0) {
            return 1.0;  // Empty strings are 100% similar
        }

        return 2.0 * matches / total_size;
    }

    [[nodiscard]] auto getMatchingBlocks() const noexcept
        -> std::vector<std::tuple<int, int, int>> {
        return matching_blocks;
    }

    [[nodiscard]] auto getOpcodes() const noexcept
        -> std::vector<std::tuple<std::string, int, int, int, int>> {
        std::vector<std::tuple<std::string, int, int, int, int>> opcodes;
        opcodes.reserve(matching_blocks.size() *
                        2);  // Pre-allocate for better performance

        int aStart = 0;
        int bStart = 0;

        for (const auto& block : matching_blocks) {
            const int aIndex = std::get<0>(block);
            const int bIndex = std::get<1>(block);
            const int size = std::get<2>(block);

            if (size > 0) {
                if (aStart < aIndex || bStart < bIndex) {
                    if (aStart < aIndex && bStart < bIndex) {
                        opcodes.emplace_back("replace", aStart, aIndex, bStart,
                                             bIndex);
                    } else if (aStart < aIndex) {
                        opcodes.emplace_back("delete", aStart, aIndex, bStart,
                                             bStart);
                    } else {
                        opcodes.emplace_back("insert", aStart, aStart, bStart,
                                             bIndex);
                    }
                }
                opcodes.emplace_back("equal", aIndex, aIndex + size, bIndex,
                                     bIndex + size);
                aStart = aIndex + size;
                bStart = bIndex + size;
            }
        }
        return opcodes;
    }

private:
    std::string seq1_;
    std::string seq2_;
    std::vector<std::tuple<int, int, int>> matching_blocks;
    DiffOptions options_;
    DiffStats stats_;

    // Improved matching algorithm with parallel processing for large sequences
    void computeMatchingBlocks() {
        const size_t seq1_size = seq1_.size();
        const size_t seq2_size = seq2_.size();

        // For very small sequences, use simple approach
        if (seq1_size < 100 || seq2_size < 100) {
            computeMatchingBlocksSimple();
            return;
        }

        // For larger sequences, use parallel approach
        computeMatchingBlocksParallel();

        // Add the final sentinel block
        matching_blocks.emplace_back(seq1_size, seq2_size, 0);

        // Sort and merge overlapping blocks
        std::sort(matching_blocks.begin(), matching_blocks.end(),
                  [](const auto& a, const auto& b) {
                      if (std::get<0>(a) != std::get<0>(b)) {
                          return std::get<0>(a) < std::get<0>(b);
                      }
                      return std::get<1>(a) < std::get<1>(b);
                  });

        // Remove overlaps and optimize matching blocks
        mergeOverlappingBlocks();
    }

    void computeMatchingBlocksSimple() {
        std::unordered_map<char, std::vector<size_t>> seq2_index_map;
        seq2_index_map.reserve(
            256);  // Reserve space for all possible characters

        for (size_t j = 0; j < seq2_.size(); ++j) {
            seq2_index_map[seq2_[j]].push_back(j);
        }

        for (size_t i = 0; i < seq1_.size(); ++i) {
            auto it = seq2_index_map.find(seq1_[i]);
            if (it != seq2_index_map.end()) {
                for (size_t j : it->second) {
                    size_t matchLength = 0;
                    while (i + matchLength < seq1_.size() &&
                           j + matchLength < seq2_.size() &&
                           seq1_[i + matchLength] == seq2_[j + matchLength]) {
                        ++matchLength;
                    }
                    if (matchLength > 0) {
                        matching_blocks.emplace_back(i, j, matchLength);
                    }
                }
            }
        }
    }

    void computeMatchingBlocksParallel() {
        const unsigned int num_threads =
            std::max(1u, std::thread::hardware_concurrency());
        const size_t chunk_size = seq1_.size() / num_threads + 1;

        std::unordered_map<char, std::vector<size_t>> seq2_index_map;
        seq2_index_map.reserve(256);

        for (size_t j = 0; j < seq2_.size(); ++j) {
            seq2_index_map[seq2_[j]].push_back(j);
        }

        std::vector<std::future<std::vector<std::tuple<int, int, int>>>>
            futures;
        futures.reserve(num_threads);

        // Create worker threads
        for (unsigned int t = 0; t < num_threads; ++t) {
            const size_t start = t * chunk_size;
            const size_t end = std::min(start + chunk_size, seq1_.size());

            futures.push_back(std::async(
                std::launch::async, [this, start, end, &seq2_index_map]() {
                    std::vector<std::tuple<int, int, int>> thread_blocks;

                    for (size_t i = start; i < end; ++i) {
                        auto it = seq2_index_map.find(seq1_[i]);
                        if (it != seq2_index_map.end()) {
                            for (size_t j : it->second) {
                                size_t matchLength = 0;
                                while (i + matchLength < seq1_.size() &&
                                       j + matchLength < seq2_.size() &&
                                       seq1_[i + matchLength] ==
                                           seq2_[j + matchLength]) {
                                    ++matchLength;
                                }
                                if (matchLength > 0) {
                                    thread_blocks.emplace_back(i, j,
                                                               matchLength);
                                }
                            }
                        }
                    }
                    return thread_blocks;
                }));
        }

        // Collect results from all threads
        for (auto& future : futures) {
            auto thread_blocks = future.get();
            matching_blocks.insert(matching_blocks.end(), thread_blocks.begin(),
                                   thread_blocks.end());
        }
    }

    void mergeOverlappingBlocks() {
        if (matching_blocks.empty())
            return;

        std::vector<std::tuple<int, int, int>> merged;
        merged.reserve(matching_blocks.size());

        auto current = matching_blocks[0];

        for (size_t i = 1; i < matching_blocks.size(); ++i) {
            const auto& block = matching_blocks[i];
            const int current_a_end =
                std::get<0>(current) + std::get<2>(current);
            const int current_b_end =
                std::get<1>(current) + std::get<2>(current);
            const int block_a = std::get<0>(block);
            const int block_b = std::get<1>(block);

            // Check for adjacent or overlapping blocks
            if (block_a <= current_a_end && block_b <= current_b_end) {
                // Extend current block if needed
                const int new_size = std::max(
                    std::get<2>(current),
                    block_a - std::get<0>(current) + std::get<2>(block));
                std::get<2>(current) = new_size;
            } else {
                merged.push_back(current);
                current = block;
            }
        }

        merged.push_back(current);
        matching_blocks = std::move(merged);
    }

    [[nodiscard]] auto sumMatchingBlocks() const noexcept -> double {
        return std::transform_reduce(
            std::execution::par_unseq, matching_blocks.begin(),
            matching_blocks.end(), 0.0, std::plus<>(), [](const auto& block) {
                return static_cast<double>(std::get<2>(block));
            });
    }
};

class Differ::Impl {
public:
    Impl(const DiffOptions& options) : options_(options) {}
    void setOptions(const DiffOptions& options) { options_ = options; }
    const DiffStats& getStats() const { return stats_; }

private:
    DiffOptions options_;
    DiffStats stats_;
};

class HtmlDiff::Impl {
public:
    Impl(const DiffOptions& options) : options_(options) {}
    void setOptions(const DiffOptions& options) { options_ = options; }
    const DiffStats& getStats() const { return stats_; }

private:
    DiffOptions options_;
    DiffStats stats_;
};

class InlineDiff::Impl {
public:
    explicit Impl(const DiffOptions& options) : options_(options) {}

    void setOptions(const DiffOptions& options) { options_ = options; }

    auto compareChars(std::string_view str1, std::string_view str2)
        -> std::vector<std::tuple<std::string, std::string>> {
        // 使用Myers差异算法比较字符级别差异
        auto diff = algorithms::MyersDiff(str1, str2);
        auto opcodes = diff.execute();

        std::vector<std::tuple<std::string, std::string>> result;
        result.reserve(opcodes.size());

        for (const auto& op : opcodes) {
            const auto& tag = std::get<0>(op);
            const int i1 = std::get<1>(op);
            const int i2 = std::get<2>(op);
            const int j1 = std::get<3>(op);
            const int j2 = std::get<4>(op);

            if (tag == "equal") {
                result.emplace_back("equal",
                                    std::string(str1.substr(i1, i2 - i1)));
            } else if (tag == "delete") {
                result.emplace_back("delete",
                                    std::string(str1.substr(i1, i2 - i1)));
            } else if (tag == "insert") {
                result.emplace_back("insert",
                                    std::string(str2.substr(j1, j2 - j1)));
            } else if (tag == "replace") {
                result.emplace_back("delete",
                                    std::string(str1.substr(i1, i2 - i1)));
                result.emplace_back("insert",
                                    std::string(str2.substr(j1, j2 - j1)));
            }
        }

        return result;
    }

    auto toHtml(std::string_view str1, std::string_view str2,
                const HtmlDiff::HtmlDiffOptions& options)
        -> std::pair<std::string, std::string> {
        auto changes = compareChars(str1, str2);
        std::string html1, html2;

        for (const auto& [op, content] : changes) {
            if (op == "equal") {
                html1 += escapeHtml(content);
                html2 += escapeHtml(content);
            } else if (op == "delete") {
                html1 += "<span class=\"" + options.removedClass + "\">" +
                         escapeHtml(content) + "</span>";
            } else if (op == "insert") {
                html2 += "<span class=\"" + options.addedClass + "\">" +
                         escapeHtml(content) + "</span>";
            }
        }

        return {html1, html2};
    }

private:
    DiffOptions options_;

    static std::string escapeHtml(const std::string& str) {
        std::string result;
        result.reserve(str.size());
        for (char c : str) {
            switch (c) {
                case '&':
                    result += "&amp;";
                    break;
                case '<':
                    result += "&lt;";
                    break;
                case '>':
                    result += "&gt;";
                    break;
                case '"':
                    result += "&quot;";
                    break;
                default:
                    result += c;
            }
        }
        return result;
    }
};

SequenceMatcher::SequenceMatcher(std::string_view str1, std::string_view str2,
                                 const DiffOptions& options)
    : pimpl_(std::make_unique<Impl>(str1, str2, options)) {}

SequenceMatcher::~SequenceMatcher() noexcept = default;

void SequenceMatcher::setSeqs(std::string_view str1, std::string_view str2) {
    try {
        pimpl_->setSeqs(str1, str2);
    } catch (const std::exception& e) {
        throw std::invalid_argument(
            std::format("Failed to set sequences: {}", e.what()));
    }
}

auto SequenceMatcher::ratio() const noexcept -> double {
    return pimpl_->ratio();
}

auto SequenceMatcher::getMatchingBlocks() const noexcept
    -> std::vector<std::tuple<int, int, int>> {
    return pimpl_->getMatchingBlocks();
}

auto SequenceMatcher::getOpcodes() const noexcept
    -> std::vector<std::tuple<std::string, int, int, int, int>> {
    return pimpl_->getOpcodes();
}

void SequenceMatcher::setOptions(const DiffOptions& options) {
    pimpl_->setOptions(options);
}

[[nodiscard]] auto SequenceMatcher::getStats() const noexcept
    -> const DiffStats& {
    return pimpl_->getStats();
}

void SequenceMatcher::clearCache() noexcept { pimpl_->clearCache(); }

auto Differ::compare(std::span<const std::string> vec1,
                     std::span<const std::string> vec2)
    -> std::vector<std::string> {
    return compare(vec1, vec2, DiffLibConfig::getDefaultOptions());
}

auto Differ::compare(std::span<const std::string> vec1,
                     std::span<const std::string> vec2,
                     const DiffOptions& options) -> std::vector<std::string> {
    std::vector<std::string> result;
    result.reserve(vec1.size() + vec2.size());  // Pre-allocate for efficiency

    try {
        SequenceMatcher matcher(
            "", "", options);  // 传递 options 参数给 SequenceMatcher
        const std::string joined1 = joinLines(vec1);
        const std::string joined2 = joinLines(vec2);
        matcher.setSeqs(joined1, joined2);

        // 其余代码保持不变
        auto opcodes = matcher.getOpcodes();

        for (const auto& opcode : opcodes) {
            const std::string& tag = std::get<0>(opcode);
            const int i1 = std::get<1>(opcode);
            const int i2 = std::get<2>(opcode);
            const int j1 = std::get<3>(opcode);
            const int j2 = std::get<4>(opcode);

            if (tag == "equal") {
                for (int k = 0; k < i2 - i1; ++k) {
                    const int idx = i1 + k;
                    if (static_cast<size_t>(idx) < vec1.size()) {
                        result.push_back("  " + vec1[idx]);
                    }
                }
            } else if (tag == "replace") {
                for (int k = i1; k < i2; ++k) {
                    if (static_cast<size_t>(k) < vec1.size()) {
                        result.push_back("- " + vec1[k]);
                    }
                }
                for (int k = j1; k < j2; ++k) {
                    if (static_cast<size_t>(k) < vec2.size()) {
                        result.push_back("+ " + vec2[k]);
                    }
                }
            } else if (tag == "delete") {
                for (int k = i1; k < i2; ++k) {
                    if (static_cast<size_t>(k) < vec1.size()) {
                        result.push_back("- " + vec1[k]);
                    }
                }
            } else if (tag == "insert") {
                for (int k = j1; k < j2; ++k) {
                    if (static_cast<size_t>(k) < vec2.size()) {
                        result.push_back("+ " + vec2[k]);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        throw std::invalid_argument(
            std::format("Failed to compare sequences: {}", e.what()));
    }

    return result;
}

auto Differ::unifiedDiff(std::span<const std::string> vec1,
                         std::span<const std::string> vec2,
                         std::string_view label1, std::string_view label2,
                         int context) -> std::vector<std::string> {
    return unifiedDiff(vec1, vec2, label1, label2, context,
                       DiffLibConfig::getDefaultOptions());
}

auto Differ::unifiedDiff(std::span<const std::string> vec1,
                         std::span<const std::string> vec2,
                         std::string_view label1, std::string_view label2,
                         int context, const DiffOptions& options)
    -> std::vector<std::string> {
    // Input validation
    if (context < 0) {
        throw std::invalid_argument("Context cannot be negative");
    }

    std::vector<std::string> diff;
    diff.reserve(vec1.size() + vec2.size() +
                 3);  // Reserve space for header and content

    try {
        SequenceMatcher matcher("", "", options);
        matcher.setSeqs(joinLines(vec1), joinLines(vec2));
        auto opcodes = matcher.getOpcodes();

        diff.push_back(std::format("--- {}", label1));
        diff.push_back(std::format("+++ {}", label2));

        int start_a = 0, start_b = 0;
        int end_a = 0, end_b = 0;
        std::vector<std::string> chunk;

        for (const auto& opcode : opcodes) {
            const std::string& tag = std::get<0>(opcode);
            const int i1 = std::get<1>(opcode);
            const int i2 = std::get<2>(opcode);
            const int j1 = std::get<3>(opcode);
            const int j2 = std::get<4>(opcode);

            if (tag == "equal") {
                if (i2 - i1 > 2 * context) {
                    chunk.push_back(std::format("@@ -{},{} +{},{} @@",
                                                start_a + 1, end_a - start_a,
                                                start_b + 1, end_b - start_b));

                    for (int k = start_a;
                         k < std::min(start_a + context,
                                      static_cast<int>(vec1.size()));
                         ++k) {
                        chunk.push_back(" " + vec1[k]);
                    }
                    diff.insert(diff.end(), chunk.begin(), chunk.end());
                    chunk.clear();
                    chunk.reserve(2 * context +
                                  10);  // Reserve space for efficiency

                    start_a = i2 - context;
                    start_b = j2 - context;
                } else {
                    for (int k = i1; k < i2; ++k) {
                        if (static_cast<size_t>(k) < vec1.size()) {
                            chunk.push_back(" " + vec1[k]);
                        }
                    }
                }
                end_a = i2;
                end_b = j2;
            } else {
                if (chunk.empty()) {
                    chunk.push_back(std::format("@@ -{},{} +{},{} @@",
                                                start_a + 1, end_a - start_a,
                                                start_b + 1, end_b - start_b));
                }
                if (tag == "replace") {
                    for (int k = i1; k < i2; ++k) {
                        if (static_cast<size_t>(k) < vec1.size()) {
                            chunk.push_back("- " + vec1[k]);
                        }
                    }
                    for (int k = j1; k < j2; ++k) {
                        if (static_cast<size_t>(k) < vec2.size()) {
                            chunk.push_back("+ " + vec2[k]);
                        }
                    }
                } else if (tag == "delete") {
                    for (int k = i1; k < i2; ++k) {
                        if (static_cast<size_t>(k) < vec1.size()) {
                            chunk.push_back("- " + vec1[k]);
                        }
                    }
                } else if (tag == "insert") {
                    for (int k = j1; k < j2; ++k) {
                        if (static_cast<size_t>(k) < vec2.size()) {
                            chunk.push_back("+ " + vec2[k]);
                        }
                    }
                }
                end_a = i2;
                end_b = j2;
            }
        }
        if (!chunk.empty()) {
            diff.insert(diff.end(), chunk.begin(), chunk.end());
        }
    } catch (const std::exception& e) {
        throw std::invalid_argument(
            std::format("Failed to generate unified diff: {}", e.what()));
    }

    return diff;
}

HtmlDiff::HtmlDiff(const DiffOptions& options)
    : pimpl_(std::make_unique<Impl>(options)) {}

HtmlDiff::~HtmlDiff() noexcept = default;

void HtmlDiff::setOptions(const DiffOptions& options) {
    pimpl_->setOptions(options);
}

[[nodiscard]] auto HtmlDiff::getStats() const noexcept -> const DiffStats& {
    return pimpl_->getStats();
}

auto HtmlDiff::makeFile(std::span<const std::string> fromlines,
                        std::span<const std::string> tolines,
                        std::string_view fromdesc, std::string_view todesc,
                        const HtmlDiffOptions& htmlOptions) -> DiffResult {
    return makeFile(fromlines, tolines, fromdesc, todesc,
                    DiffLibConfig::getDefaultOptions(), htmlOptions);
}

auto HtmlDiff::makeFile(std::span<const std::string> fromlines,
                        std::span<const std::string> tolines,
                        std::string_view fromdesc, std::string_view todesc,
                        const DiffOptions& options,
                        const HtmlDiffOptions& htmlOptions) -> DiffResult {
    try {
        std::ostringstream os;
        os << "<!DOCTYPE html>\n<html>\n<head>\n"
           << "<meta charset=\"utf-8\">\n"
           << "<title>Diff</title>\n"
           << "<style>\n"
           << "  ." << htmlOptions.addedClass
           << " { background-color: #aaffaa; }\n"
           << "  ." << htmlOptions.removedClass
           << " { background-color: #ffaaaa; }\n"
           << "  ." << htmlOptions.changedClass
           << " { background-color: #ffff77; }\n"
           << "  table { border-collapse: collapse; width: 100%; }\n"
           << "  th, td { border: 1px solid #ddd; padding: 8px; }\n"
           << "  th { background-color: #f2f2f2; }\n";
        // 添加用户自定义选项
        if (htmlOptions.showLineNumbers) {
            os << "  .line-number { color: #999; user-select: none; }\n";
        }
        if (htmlOptions.collapsableUnchanged) {
            os << "  .collapsible { cursor: pointer; }\n"
               << "  .hidden { display: none; }\n";
        }
        os << "</style>\n"
           << "</head>\n<body>\n"
           << "<h2>Differences</h2>\n";

        // 显示统计信息（如果启用）
        if (htmlOptions.showStatistics) {
            auto table_result = makeTable(fromlines, tolines, fromdesc, todesc,
                                          options, htmlOptions);
            if (!table_result) {
                return type::unexpected(table_result.error().error());
            }
            os << table_result.value();
        } else {
            // 获取表格内容但不显示统计信息
            auto table_result = makeTable(fromlines, tolines, fromdesc, todesc,
                                          options, htmlOptions);
            if (!table_result) {
                return type::unexpected(table_result.error().error());
            }
            os << table_result.value();
        }

        os << "</body>\n</html>";

        return os.str();
    } catch (const std::exception& e) {
        return type::unexpected(
            std::format("Error generating HTML file: {}", e.what()));
    }
}

auto HtmlDiff::makeTable(std::span<const std::string> fromlines,
                         std::span<const std::string> tolines,
                         std::string_view fromdesc, std::string_view todesc,
                         const HtmlDiffOptions& htmlOptions) -> DiffResult {
    return makeTable(fromlines, tolines, fromdesc, todesc,
                     DiffLibConfig::getDefaultOptions(), htmlOptions);
}

// 修复 HtmlDiff::makeTable 方法中未使用的 htmlOptions 参数
auto HtmlDiff::makeTable(std::span<const std::string> fromlines,
                         std::span<const std::string> tolines,
                         std::string_view fromdesc, std::string_view todesc,
                         const DiffOptions& options,
                         const HtmlDiffOptions& htmlOptions) -> DiffResult {
    try {
        std::ostringstream os;
        os << "<table>\n<tr><th>" << fromdesc << "</th><th>" << todesc
           << "</th></tr>\n";

        const auto escape_html = [](const std::string& s) {
            std::string result;
            result.reserve(s.size());
            for (char c : s) {
                switch (c) {
                    case '&':
                        result += "&amp;";
                        break;
                    case '<':
                        result += "&lt;";
                        break;
                    case '>':
                        result += "&gt;";
                        break;
                    case '"':
                        result += "&quot;";
                        break;
                    default:
                        result += c;
                }
            }
            return result;
        };

        // Use Differ to get differences
        std::vector<std::string> diffs;
        try {
            diffs = Differ::compare(fromlines, tolines, options);
        } catch (const std::exception& e) {
            return type::unexpected(
                std::format("Failed to compare lines: {}", e.what()));
        }

        // 适用自定义CSS类
        const std::string& addedClass = htmlOptions.addedClass;
        const std::string& removedClass = htmlOptions.removedClass;

        // 对内联差异进行细粒度处理
        InlineDiff inlineDiff(options);

        // 处理每一行差异
        int lineNum = 1;
        for (const auto& line : diffs) {
            if (line.empty()) {
                os << "<tr><td>&nbsp;</td><td>&nbsp;</td></tr>\n";
                continue;
            }

            if (line.size() >= 2) {
                const std::string content = escape_html(line.substr(2));

                // 添加行号（如果启用）
                std::string lineNumHtml = htmlOptions.showLineNumbers
                                              ? "<span class=\"line-number\">" +
                                                    std::to_string(lineNum++) +
                                                    "</span> "
                                              : "";

                if (line[0] == '-') {
                    os << "<tr><td class=\"" << removedClass << "\">"
                       << lineNumHtml << content << "</td><td></td></tr>\n";
                } else if (line[0] == '+') {
                    os << "<tr><td></td><td class=\"" << addedClass << "\">"
                       << lineNumHtml << content << "</td></tr>\n";
                } else {
                    // 处理可折叠的未更改区域（如果启用）
                    std::string cellClass = htmlOptions.collapsableUnchanged
                                                ? " class=\"collapsible\""
                                                : "";
                    os << "<tr" << cellClass << "><td>" << lineNumHtml
                       << content << "</td><td>" << lineNumHtml << content
                       << "</td></tr>\n";
                }
            }
        }

        os << "</table>\n";

        // 添加统计信息（如果启用）
        if (htmlOptions.showStatistics) {
            os << "<div class=\"diff-stats\">\n"
               << "  <p>Context lines: " << htmlOptions.contextLines << "</p>\n"
               << "</div>\n";
        }

        return os.str();
    } catch (const std::exception& e) {
        return type::unexpected(
            std::format("Error generating HTML table: {}", e.what()));
    }
}

auto getCloseMatches(std::string_view word,
                     std::span<const std::string> possibilities, int n,
                     double cutoff) -> std::vector<std::string> {
    // Input validation
    if (n <= 0) {
        throw std::invalid_argument("n must be greater than 0");
    }

    if (cutoff < 0.0 || cutoff > 1.0) {
        throw std::invalid_argument("cutoff must be between 0.0 and 1.0");
    }

    try {
        std::vector<std::pair<double, std::string>> scores;
        scores.reserve(possibilities.size());

        // Special cases for empty inputs
        if (word.empty()) {
            auto empty_pos = std::ranges::find_if(
                possibilities, [](const std::string& s) { return s.empty(); });
            if (empty_pos != possibilities.end()) {
                return {*empty_pos};
            }
            return {};
        }

        // Use parallel execution for large inputs
        const bool use_parallel = possibilities.size() > 100;

        if (use_parallel) {
            std::mutex scores_mutex;
            std::vector<std::thread> threads;
            const unsigned int num_threads =
                std::max(1u, std::thread::hardware_concurrency());
            const size_t chunk_size = possibilities.size() / num_threads + 1;

            for (unsigned int t = 0; t < num_threads; ++t) {
                const size_t start = t * chunk_size;
                const size_t end =
                    std::min(start + chunk_size, possibilities.size());

                threads.emplace_back([&, start, end]() {
                    std::vector<std::pair<double, std::string>> local_scores;
                    local_scores.reserve(end - start);

                    for (size_t i = start; i < end; ++i) {
                        SequenceMatcher matcher(word, possibilities[i]);
                        double score = matcher.ratio();
                        if (score >= cutoff) {
                            local_scores.emplace_back(score, possibilities[i]);
                        }
                    }

                    {
                        std::lock_guard<std::mutex> lock(scores_mutex);
                        scores.insert(scores.end(), local_scores.begin(),
                                      local_scores.end());
                    }
                });
            }

            for (auto& thread : threads) {
                thread.join();
            }
        } else {
            // Optimize single-threaded case with SIMD when possible
            // First, collect all items needing comparison
            for (const auto& possibility : possibilities) {
                SequenceMatcher matcher(word, possibility);
                double score = matcher.ratio();
                if (score >= cutoff) {
                    scores.emplace_back(score, possibility);
                }
            }
        }

        // Use partial_sort instead of full sort when n is small compared to
        // results
        if (static_cast<size_t>(n) < scores.size() / 2) {
            std::partial_sort(
                scores.begin(),
                scores.begin() + std::min(n, static_cast<int>(scores.size())),
                scores.end(),
                [](const auto& a, const auto& b) { return a.first > b.first; });
        } else {
            std::sort(
                scores.begin(), scores.end(),
                [](const auto& a, const auto& b) { return a.first > b.first; });
        }

        std::vector<std::string> matches;
        matches.reserve(std::min(n, static_cast<int>(scores.size())));

        for (int i = 0; i < std::min(n, static_cast<int>(scores.size())); ++i) {
            matches.push_back(std::move(scores[i].second));
        }
        return matches;
    } catch (const std::exception& e) {
        throw std::invalid_argument(
            std::format("Failed to get close matches: {}", e.what()));
    }
}

// Add a utility function to detect if SIMD is supported at runtime
namespace {
bool detectSIMDSupport() noexcept {
    try {
#if defined(__AVX2__)
        return true;  // AVX2 support
#elif defined(__SSE4_2__)
        return true;  // SSE4.2 support
#else
        return false;  // No SIMD support
#endif
    } catch (...) {
        return false;
    }
}

// Singleton to cache CPU feature detection
const bool g_simd_supported = detectSIMDSupport();
}  // namespace

auto DiffStats::toString() const -> std::string {
    std::ostringstream ss;
    ss << "Insertions: " << insertions << ", Deletions: " << deletions
       << ", Modifications: " << modifications << ", Similarity: " << std::fixed
       << std::setprecision(2) << (similarity * 100) << "%"
       << ", Duration: " << duration.count() << "µs";
    return ss.str();
}

// Global configuration static members
DiffOptions DiffLibConfig::default_options_{};
LogCallback DiffLibConfig::log_callback_{nullptr};
bool DiffLibConfig::telemetry_enabled_{false};

// Utility logging function
namespace detail {
inline void log(const LogCallback& logger, const std::string& message,
                int level = 0) {
    if (logger) {
        logger(message, level);
    }
}

// Hash function for caching diff results
struct DiffKeyHasher {
    size_t operator()(const std::pair<std::string, std::string>& p) const {
        return std::hash<std::string>{}(p.first) ^
               (std::hash<std::string>{}(p.second) << 1);
    }
};

// Global cache for diff operations using the correct template parameters
using DiffCache = atom::search::ThreadSafeLRUCache<
    std::pair<std::string, std::string>,
    std::vector<std::tuple<std::string, int, int, int, int>>>;

// Initialize cache with size 100
inline DiffCache g_diff_cache(100);
}  // namespace detail

namespace algorithms {

// Patience差异算法实现
class PatienceDiff {
public:
    PatienceDiff(std::string_view a, std::string_view b) : a_(a), b_(b) {}

    auto execute() -> std::vector<std::tuple<std::string, int, int, int, int>> {
        auto start_time = std::chrono::high_resolution_clock::now();

        auto result = calculateDiff();

        auto end_time = std::chrono::high_resolution_clock::now();
        stats_.duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);

        return result;
    }

    [[nodiscard]] auto getStats() const -> const DiffStats& { return stats_; }

private:
    std::string_view a_;
    std::string_view b_;
    DiffStats stats_;

    // 用于Patience算法的辅助类：最长递增子序列
    struct LIS {
        std::vector<std::pair<int, int>> sequence;
        std::vector<int> backpointers;

        explicit LIS(const std::vector<int>& indices) {
            backpointers.resize(indices.size(), -1);
            std::vector<int> tails;
            std::vector<int> links;

            for (size_t i = 0; i < indices.size(); ++i) {
                const int value = indices[i];
                // 二分搜索找到合适的位置
                auto it = std::lower_bound(tails.begin(), tails.end(), value);

                if (it == tails.end()) {
                    // 在序列末尾添加新元素
                    if (!tails.empty()) {
                        backpointers[i] = links[tails.size() - 1];
                    }
                    links.push_back(i);
                    tails.push_back(value);
                } else {
                    // 更新序列中的元素
                    const int pos = std::distance(tails.begin(), it);
                    if (pos > 0) {
                        backpointers[i] = links[pos - 1];
                    }
                    tails[pos] = value;
                    links[pos] = i;
                }
            }

            // 重建序列
            if (!links.empty()) {
                int curr = links.back();
                while (curr != -1) {
                    sequence.emplace_back(curr, indices[curr]);
                    curr = backpointers[curr];
                }
                std::reverse(sequence.begin(), sequence.end());
            }
        }
    };

    // Patience排序差异算法的核心实现
    auto calculateDiff()
        -> std::vector<std::tuple<std::string, int, int, int, int>> {
        const int n = static_cast<int>(a_.size());
        const int m = static_cast<int>(b_.size());

        // 特殊情况处理
        if (n == 0 && m == 0) {
            return {};
        }
        if (n == 0) {
            stats_.insertions = m;
            return {std::make_tuple("insert", 0, 0, 0, m)};
        }
        if (m == 0) {
            stats_.deletions = n;
            return {std::make_tuple("delete", 0, n, 0, 0)};
        }

        // 构建唯一性表
        std::unordered_map<char, std::vector<int>> bChars;
        for (int j = 0; j < m; ++j) {
            bChars[b_[j]].push_back(j);
        }

        // 找到序列A中与序列B唯一匹配的字符
        std::vector<std::pair<int, int>> uniqueMatches;
        for (int i = 0; i < n; ++i) {
            char c = a_[i];
            if (bChars.contains(c) && bChars[c].size() == 1) {
                uniqueMatches.emplace_back(i, bChars[c][0]);
            }
        }

        // 按序列B中的索引对匹配进行排序
        std::sort(
            uniqueMatches.begin(), uniqueMatches.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });

        // 提取序列A中的索引作为输入
        std::vector<int> indices;
        indices.reserve(uniqueMatches.size());
        for (const auto& match : uniqueMatches) {
            indices.push_back(match.first);
        }

        // 计算最长递增子序列
        LIS lis(indices);

        // 将LIS结果转换为匹配块
        std::vector<std::tuple<int, int, int>> matchingBlocks;
        for (const auto& [i, aIdx] : lis.sequence) {
            const auto& match = uniqueMatches[i];
            matchingBlocks.emplace_back(match.first, match.second, 1);
        }

        // 扩展匹配块
        extendMatchingBlocks(matchingBlocks, n, m);

        // 合并重叠块
        mergeOverlappingBlocks(matchingBlocks);

        // 添加哨兵块
        matchingBlocks.emplace_back(n, m, 0);

        // 生成操作码
        auto opcodes = generateOpcodes(matchingBlocks, n, m);

        // 更新统计信息
        updateStats(opcodes);

        return opcodes;
    }

    // 扩展匹配块以捕获更长的匹配序列
    void extendMatchingBlocks(std::vector<std::tuple<int, int, int>>& blocks,
                              int n, int m) {
        for (auto& block : blocks) {
            int& i1 = std::get<0>(block);
            int& j1 = std::get<1>(block);
            int& size = std::get<2>(block);

            // 向前扩展
            while (i1 > 0 && j1 > 0 && a_[i1 - 1] == b_[j1 - 1]) {
                --i1;
                --j1;
                ++size;
            }

            // 向后扩展
            int i2 = i1 + size;
            int j2 = j1 + size;
            while (i2 < n && j2 < m && a_[i2] == b_[j2]) {
                ++size;
                ++i2;
                ++j2;
            }
        }
    }

    // 合并重叠匹配块
    void mergeOverlappingBlocks(
        std::vector<std::tuple<int, int, int>>& blocks) {
        if (blocks.size() <= 1)
            return;

        // 按序列A中的起始位置排序
        std::sort(blocks.begin(), blocks.end());

        std::vector<std::tuple<int, int, int>> merged;
        merged.reserve(blocks.size());

        auto current = blocks[0];
        for (size_t i = 1; i < blocks.size(); ++i) {
            const auto& block = blocks[i];

            int current_a_end = std::get<0>(current) + std::get<2>(current);
            int current_b_end = std::get<1>(current) + std::get<2>(current);

            int next_a_start = std::get<0>(block);
            int next_b_start = std::get<1>(block);

            // 检查是否重叠或接近
            if (next_a_start <= current_a_end ||
                next_b_start <= current_b_end) {
                // 计算合并后的块大小
                int new_size = std::max(
                    current_a_end - std::get<0>(current),
                    next_a_start - std::get<0>(current) + std::get<2>(block));
                std::get<2>(current) = new_size;
            } else {
                merged.push_back(current);
                current = block;
            }
        }

        merged.push_back(current);
        blocks = std::move(merged);
    }

    // 生成差异操作码
    auto generateOpcodes(const std::vector<std::tuple<int, int, int>>& blocks,
                         int n, int m)
        -> std::vector<std::tuple<std::string, int, int, int, int>> {
        std::vector<std::tuple<std::string, int, int, int, int>> opcodes;
        opcodes.reserve(blocks.size() * 2);

        int last_a = 0, last_b = 0;

        for (const auto& block : blocks) {
            const int i1 = std::get<0>(block);
            const int j1 = std::get<1>(block);
            const int size = std::get<2>(block);

            // 处理不匹配的部分
            if (last_a < i1 || last_b < j1) {
                if (last_a < i1 && last_b < j1) {
                    opcodes.emplace_back("replace", last_a, i1, last_b, j1);
                } else if (last_a < i1) {
                    opcodes.emplace_back("delete", last_a, i1, last_b, last_b);
                } else {
                    opcodes.emplace_back("insert", last_a, last_a, last_b, j1);
                }
            }

            // 处理匹配部分
            if (size > 0) {
                opcodes.emplace_back("equal", i1, i1 + size, j1, j1 + size);
            }

            last_a = i1 + size;
            last_b = j1 + size;
        }

        if (last_a < n || last_b < m) {
            if (last_a < n && last_b < m) {
                opcodes.emplace_back("replace", last_a, n, last_b, m);
            } else if (last_a < n) {
                opcodes.emplace_back("delete", last_a, n, last_b, last_b);
            } else if (last_b < m) {
                opcodes.emplace_back("insert", last_a, last_a, last_b, m);
            }
        }

        return opcodes;
    }

    // 更新差异统计信息
    void updateStats(
        const std::vector<std::tuple<std::string, int, int, int, int>>&
            opcodes) {
        stats_ = DiffStats{};

        for (const auto& op : opcodes) {
            const auto& tag = std::get<0>(op);
            const int i1 = std::get<1>(op);
            const int i2 = std::get<2>(op);
            const int j1 = std::get<3>(op);
            const int j2 = std::get<4>(op);

            if (tag == "equal") {
                stats_.modifications += i2 - i1;
            } else if (tag == "replace") {
                stats_.deletions += i2 - i1;
                stats_.insertions += j2 - j1;
            } else if (tag == "delete") {
                stats_.deletions += i2 - i1;
            } else if (tag == "insert") {
                stats_.insertions += j2 - j1;
            }
        }

        int total = stats_.insertions + stats_.deletions + stats_.modifications;
        if (total > 0) {
            stats_.similarity =
                static_cast<double>(stats_.modifications) / total;
        } else {
            stats_.similarity = 1.0;  // 完全相同
        }
    }
};

// Histogram差异算法实现
class HistogramDiff {
public:
    HistogramDiff(std::string_view a, std::string_view b) : a_(a), b_(b) {}

    auto execute() -> std::vector<std::tuple<std::string, int, int, int, int>> {
        auto start_time = std::chrono::high_resolution_clock::now();

        auto result = calculateDiff();

        auto end_time = std::chrono::high_resolution_clock::now();
        stats_.duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);

        return result;
    }

    [[nodiscard]] auto getStats() const -> const DiffStats& { return stats_; }

private:
    std::string_view a_;
    std::string_view b_;
    DiffStats stats_;

    // 计算行的哈希值
    static size_t lineHash(std::string_view line) {
        return std::hash<std::string_view>{}(line);
    }

    // Histogram差异算法的核心实现
    auto calculateDiff()
        -> std::vector<std::tuple<std::string, int, int, int, int>> {
        const int n = static_cast<int>(a_.size());
        const int m = static_cast<int>(b_.size());

        // 特殊情况处理
        if (n == 0 && m == 0) {
            return {};
        }
        if (n == 0) {
            stats_.insertions = m;
            return {std::make_tuple("insert", 0, 0, 0, m)};
        }
        if (m == 0) {
            stats_.deletions = n;
            return {std::make_tuple("delete", 0, n, 0, 0)};
        }

        // 创建A中每行内容的哈希映射
        std::unordered_map<size_t, std::vector<int>> aHashes;
        for (int i = 0; i < n; ++i) {
            size_t hash = lineHash(a_.substr(i, 1));
            aHashes[hash].push_back(i);
        }

        // 创建B中行的频率直方图（histogram）
        std::unordered_map<size_t, int> bHistogram;
        for (int j = 0; j < m; ++j) {
            size_t hash = lineHash(b_.substr(j, 1));
            bHistogram[hash]++;
        }

        // 找到所有可能的匹配
        std::vector<std::tuple<int, int, int>> candidates;
        for (int j = 0; j < m; ++j) {
            size_t hash = lineHash(b_.substr(j, 1));

            // 只处理频率较低的行（更有识别度）
            if (bHistogram[hash] <= 2 && aHashes.contains(hash)) {
                for (int i : aHashes[hash]) {
                    // 计算匹配长度
                    int length = 0;
                    while (i + length < n && j + length < m &&
                           a_[i + length] == b_[j + length]) {
                        ++length;
                    }

                    if (length > 0) {
                        candidates.emplace_back(i, j, length);
                    }
                }
            }
        }

        // 按照匹配长度降序排列候选匹配
        std::sort(candidates.begin(), candidates.end(),
                  [](const auto& a, const auto& b) {
                      return std::get<2>(a) > std::get<2>(b);
                  });

        // 选择不重叠的最佳匹配块
        std::vector<std::tuple<int, int, int>> matching_blocks =
            selectBestMatches(candidates, n, m);

        // 按照A中索引排序匹配块
        std::sort(matching_blocks.begin(), matching_blocks.end());

        // 添加哨兵块
        matching_blocks.emplace_back(n, m, 0);

        // 生成操作码
        auto opcodes = generateOpcodes(matching_blocks, n, m);

        // 更新统计信息
        updateStats(opcodes);

        return opcodes;
    }

    // 从候选匹配中选择最佳的非重叠匹配
    auto selectBestMatches(
        const std::vector<std::tuple<int, int, int>>& candidates, int n, int m)
        -> std::vector<std::tuple<int, int, int>> {
        // 使用位图标记已使用的索引
        std::vector<bool> usedA(n, false);
        std::vector<bool> usedB(m, false);

        std::vector<std::tuple<int, int, int>> selected;
        selected.reserve(std::min(n, m));

        for (const auto& candidate : candidates) {
            const int i = std::get<0>(candidate);
            const int j = std::get<1>(candidate);
            const int length = std::get<2>(candidate);

            // 检查是否有任何索引已被使用
            bool conflict = false;
            for (int k = 0; k < length; ++k) {
                if (i + k < n && j + k < m) {
                    if (usedA[i + k] || usedB[j + k]) {
                        conflict = true;
                        break;
                    }
                }
            }

            // 如果没有冲突，添加此匹配
            if (!conflict) {
                selected.emplace_back(candidate);

                // 标记已使用的索引
                for (int k = 0; k < length; ++k) {
                    if (i + k < n && j + k < m) {
                        usedA[i + k] = true;
                        usedB[j + k] = true;
                    }
                }
            }
        }

        return selected;
    }

    auto generateOpcodes(const std::vector<std::tuple<int, int, int>>& blocks,
                         int n, int m)
        -> std::vector<std::tuple<std::string, int, int, int, int>> {
        std::vector<std::tuple<std::string, int, int, int, int>> opcodes;
        opcodes.reserve(blocks.size() * 2);

        int last_a = 0, last_b = 0;

        for (const auto& block : blocks) {
            const int i1 = std::get<0>(block);
            const int j1 = std::get<1>(block);
            const int size = std::get<2>(block);

            // 处理不匹配的部分
            if (last_a < i1 || last_b < j1) {
                if (last_a < i1 && last_b < j1) {
                    opcodes.emplace_back("replace", last_a, i1, last_b, j1);
                } else if (last_a < i1) {
                    opcodes.emplace_back("delete", last_a, i1, last_b, last_b);
                } else {
                    opcodes.emplace_back("insert", last_a, last_a, last_b, j1);
                }
            }

            // 处理匹配部分
            if (size > 0) {
                opcodes.emplace_back("equal", i1, i1 + size, j1, j1 + size);
            }

            last_a = i1 + size;
            last_b = j1 + size;
        }

        if (last_a < n || last_b < m) {
            if (last_a < n && last_b < m) {
                opcodes.emplace_back("replace", last_a, n, last_b, m);
            } else if (last_a < n) {
                opcodes.emplace_back("delete", last_a, n, last_b, last_b);
            } else if (last_b < m) {
                opcodes.emplace_back("insert", last_a, last_a, last_b, m);
            }
        }

        return opcodes;
    }

    void updateStats(
        const std::vector<std::tuple<std::string, int, int, int, int>>&
            opcodes) {
        stats_ = DiffStats{};

        for (const auto& op : opcodes) {
            const auto& tag = std::get<0>(op);
            const int i1 = std::get<1>(op);
            const int i2 = std::get<2>(op);
            const int j1 = std::get<3>(op);
            const int j2 = std::get<4>(op);

            if (tag == "equal") {
                stats_.modifications += i2 - i1;
            } else if (tag == "replace") {
                stats_.deletions += i2 - i1;
                stats_.insertions += j2 - j1;
            } else if (tag == "delete") {
                stats_.deletions += i2 - i1;
            } else if (tag == "insert") {
                stats_.insertions += j2 - j1;
            }
        }

        int total = stats_.insertions + stats_.deletions + stats_.modifications;
        if (total > 0) {
            stats_.similarity =
                static_cast<double>(stats_.modifications) / total;
        } else {
            stats_.similarity = 1.0;  // 完全相同
        }
    }
};

// 算法工厂 - 根据选择的算法类型创建合适的实现
class DiffAlgorithmFactory {
public:
    static auto create(DiffAlgorithm type, std::string_view a,
                       std::string_view b)
        -> std::unique_ptr<
            std::variant<MyersDiff, PatienceDiff, HistogramDiff>> {
        switch (type) {
            case DiffAlgorithm::Myers:
                return std::make_unique<
                    std::variant<MyersDiff, PatienceDiff, HistogramDiff>>(
                    MyersDiff(a, b));

            case DiffAlgorithm::Patience:
                return std::make_unique<
                    std::variant<MyersDiff, PatienceDiff, HistogramDiff>>(
                    PatienceDiff(a, b));

            case DiffAlgorithm::Histogram:
                return std::make_unique<
                    std::variant<MyersDiff, PatienceDiff, HistogramDiff>>(
                    HistogramDiff(a, b));

            case DiffAlgorithm::Default:
            default:
                // 默认使用Myers算法
                return std::make_unique<
                    std::variant<MyersDiff, PatienceDiff, HistogramDiff>>(
                    MyersDiff(a, b));
        }
    }

    static auto execute(
        std::variant<MyersDiff, PatienceDiff, HistogramDiff>& algorithm)
        -> std::vector<std::tuple<std::string, int, int, int, int>> {
        return std::visit([](auto& alg) { return alg.execute(); }, algorithm);
    }

    static auto getStats(
        const std::variant<MyersDiff, PatienceDiff, HistogramDiff>& algorithm)
        -> const DiffStats& {
        return std::visit(
            [](const auto& alg) -> const DiffStats& { return alg.getStats(); },
            algorithm);
    }
};

}  // namespace algorithms

// 实现Differ的构造函数和方法
Differ::Differ(const DiffOptions& options)
    : pimpl_(std::make_unique<Impl>(options)) {}

Differ::~Differ() noexcept = default;

void Differ::setOptions(const DiffOptions& options) {
    pimpl_->setOptions(options);
}

[[nodiscard]] auto Differ::getStats() const noexcept -> const DiffStats& {
    return pimpl_->getStats();
}

InlineDiff::InlineDiff(const DiffOptions& options)
    : pimpl_(std::make_unique<Impl>(options)) {}

InlineDiff::~InlineDiff() noexcept = default;

void InlineDiff::setOptions(const DiffOptions& options) {
    pimpl_->setOptions(options);
}

// 实现DiffLibConfig的静态方法
void DiffLibConfig::setLogCallback(const LogCallback& callback) {
    log_callback_ = callback;
}

void DiffLibConfig::setTelemetryEnabled(bool enabled) {
    telemetry_enabled_ = enabled;
}

void DiffLibConfig::setDefaultOptions(const DiffOptions& options) {
    default_options_ = options;
}

const DiffOptions& DiffLibConfig::getDefaultOptions() {
    return default_options_;
}

void DiffLibConfig::clearCaches() { detail::g_diff_cache.clear(); }

}  // namespace atom::utils
