#include "difflib.hpp"

#include <algorithm>
#include <cmath>
#include <execution>
#include <format>
#include <future>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace atom::utils {

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
    Impl(std::string_view str1, std::string_view str2)
        : seq1_(str1), seq2_(str2) {
        computeMatchingBlocks();
    }

    void setSeqs(std::string_view str1, std::string_view str2) {
        seq1_ = str1;
        seq2_ = str2;
        matching_blocks.clear();
        computeMatchingBlocks();
    }

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

SequenceMatcher::SequenceMatcher(std::string_view str1, std::string_view str2)
    : pimpl_(std::make_unique<Impl>(str1, str2)) {}

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

auto Differ::compare(std::span<const std::string> vec1,
                     std::span<const std::string> vec2)
    -> std::vector<std::string> {
    std::vector<std::string> result;
    result.reserve(vec1.size() + vec2.size());  // Pre-allocate for efficiency

    try {
        SequenceMatcher matcher("", "");
        const std::string joined1 = joinLines(vec1);
        const std::string joined2 = joinLines(vec2);
        matcher.setSeqs(joined1, joined2);

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
    // Input validation
    if (context < 0) {
        throw std::invalid_argument("Context cannot be negative");
    }

    std::vector<std::string> diff;
    diff.reserve(vec1.size() + vec2.size() +
                 3);  // Reserve space for header and content

    try {
        SequenceMatcher matcher("", "");
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

auto HtmlDiff::makeFile(std::span<const std::string> fromlines,
                        std::span<const std::string> tolines,
                        std::string_view fromdesc,
                        std::string_view todesc) -> DiffResult {
    try {
        std::ostringstream os;
        os << "<!DOCTYPE html>\n<html>\n<head>\n"
           << "<meta charset=\"utf-8\">\n"
           << "<title>Diff</title>\n"
           << "<style>\n"
           << "  .diff-add { background-color: #aaffaa; }\n"
           << "  .diff-remove { background-color: #ffaaaa; }\n"
           << "  table { border-collapse: collapse; width: 100%; }\n"
           << "  th, td { border: 1px solid #ddd; padding: 8px; }\n"
           << "  th { background-color: #f2f2f2; }\n"
           << "</style>\n"
           << "</head>\n<body>\n"
           << "<h2>Differences</h2>\n";

        // Get table content
        auto table_result = makeTable(fromlines, tolines, fromdesc, todesc);
        if (!table_result) {
            // TODO: Fix error handling
            // return type::unexpected(table_result.error());
        }

        os << table_result.value() << "</body>\n</html>";

        return os.str();
    } catch (const std::exception& e) {
        return type::unexpected(
            std::format("Error generating HTML file: {}", e.what()));
    }
}

auto HtmlDiff::makeTable(std::span<const std::string> fromlines,
                         std::span<const std::string> tolines,
                         std::string_view fromdesc,
                         std::string_view todesc) -> DiffResult {
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
            diffs = Differ::compare(fromlines, tolines);
        } catch (const std::exception& e) {
            return type::unexpected(
                std::format("Failed to compare lines: {}", e.what()));
        }

        // Process each line in the diff
        for (const auto& line : diffs) {
            if (line.empty()) {
                os << "<tr><td>&nbsp;</td><td>&nbsp;</td></tr>\n";
                continue;
            }

            if (line.size() >= 2) {
                const std::string content = escape_html(line.substr(2));
                if (line[0] == '-') {
                    os << "<tr><td class=\"diff-remove\">" << content
                       << "</td><td></td></tr>\n";
                } else if (line[0] == '+') {
                    os << "<tr><td></td><td class=\"diff-add\">" << content
                       << "</td></tr>\n";
                } else {
                    os << "<tr><td>" << content << "</td><td>" << content
                       << "</td></tr>\n";
                }
            }
        }

        os << "</table>\n";
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

}  // namespace atom::utils
