#include "algorithm.hpp"

#include <algorithm>
#include <future>
#include <thread>

#ifdef ATOM_USE_OPENMP
#include <omp.h>
#endif

#ifdef ATOM_USE_SIMD
#include <immintrin.h>
#endif

#ifdef ATOM_USE_BOOST
#include <boost/algorithm/string.hpp>
#endif

#include "atom/error/exception.hpp"
#include "spdlog/spdlog.h"

namespace atom::algorithm {

KMP::KMP(std::string_view pattern) {
    try {
        spdlog::info("Initializing KMP with pattern length: {}",
                     pattern.size());
        if (pattern.empty()) {
            spdlog::warn("Initialized KMP with empty pattern");
        }
        setPattern(pattern);
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize KMP: {}", e.what());
        THROW_INVALID_ARGUMENT(std::string("Invalid pattern: ") + e.what());
    }
}

auto KMP::search(std::string_view text) const -> std::vector<int> {
    std::vector<int> occurrences;
    try {
        std::string pattern_copy;
        std::vector<int> failure_copy;
        {
            std::shared_lock lock(mutex_);
            pattern_copy = pattern_;
            failure_copy = failure_;
        }
        auto n = static_cast<int>(text.length());
        auto m = static_cast<int>(pattern_copy.length());
        spdlog::info("KMP searching text of length {} with pattern length .", n,
                     m);
        if (m == 0) {
            spdlog::warn("Empty pattern provided to KMP::search.");
            return occurrences;
        }
        if (n < m) {
            spdlog::info("Text is shorter than pattern, no matches possible.");
            return occurrences;
        }
#ifdef ATOM_USE_SIMD
        if (m <= 16) {
            int i = 0;
            const int simdWidth = 16;
            while (i <= n - simdWidth) {
                __m128i pattern_chunk = _mm_loadu_si128(
                    reinterpret_cast<const __m128i*>(pattern_copy.data()));
                __m128i text_chunk =
                    _mm_loadu_si128(reinterpret_cast<const __m128i*>(&text[i]));
                __m128i result = _mm_cmpeq_epi8(text_chunk, pattern_chunk);
                unsigned int mask = _mm_movemask_epi8(result);
                if (m == 16) {
                    if (mask == 0xFFFF) {
                        occurrences.push_back(i);
                    }
                } else {
                    if ((mask & ((1 << m) - 1)) == ((1 << m) - 1)) {
                        occurrences.push_back(i);
                    }
                }
                i++;
            }
            while (i <= n - m) {
                int j = 0;
                while (j < m && text[i + j] == pattern_copy[j]) {
                    ++j;
                }
                if (j == m) {
                    occurrences.push_back(i);
                }
                i += (j > 0) ? j - failure_copy[j - 1] : 1;
            }
        } else {
            int i = 0;
            int j = 0;
            while (i < n) {
                if (text[i] == pattern_copy[j]) {
                    ++i;
                    ++j;
                    if (j == m) {
                        occurrences.push_back(i - m);
                        j = failure_copy[j - 1];
                    }
                } else if (j > 0) {
                    j = failure_copy[j - 1];
                } else {
                    ++i;
                }
            }
        }
#elif defined(ATOM_USE_OPENMP)
        // Using std::async for explicit task management and result aggregation
        std::vector<std::future<std::vector<int>>> futures;
        unsigned int thread_count = std::thread::hardware_concurrency();
        size_t chunk_size = std::max(static_cast<size_t>(m), n / thread_count);
        if (chunk_size == 0)
            chunk_size = n;  // Handle very small texts

        for (size_t start = 0; start < text.size(); start += chunk_size) {
            size_t end = std::min(start + chunk_size + m - 1, text.size());
            size_t search_start = start;

            if (start > 0) {
                search_start = start - (m - 1);
            }
            if (search_start > text.size())
                search_start = text.size();  // Prevent overflow

            std::string_view chunk =
                text.substr(search_start, end - search_start);

            futures.push_back(std::async(
                std::launch::async,
                [pattern_copy, bad_char_shift_copy, good_suffix_shift_copy,
                 chunk, search_start, m]() {
                    std::vector<int> local_occurrences;
                    auto chunk_n = static_cast<int>(chunk.length());
                    int i = 0;
                    while (i <= chunk_n - m) {
                        int j = m - 1;
                        while (j >= 0 && pattern_copy[j] == chunk[i + j]) {
                            --j;
                        }
                        if (j < 0) {
                            local_occurrences.push_back(
                                static_cast<int>(search_start) + i);
                            i += good_suffix_shift_copy[0];
                        } else {
                            int badCharShift =
                                bad_char_shift_copy.count(chunk[i + j])
                                    ? bad_char_shift_copy.at(chunk[i + j])
                                    : m;
                            i += std::max(good_suffix_shift_copy[j + 1],
                                          badCharShift - m + 1 + j);
                        }
                    }
                    return local_occurrences;
                }));
        }

        for (auto& future : futures) {
            auto chunk_occurrences = future.get();
            occurrences.insert(occurrences.end(), chunk_occurrences.begin(),
                               chunk_occurrences.end());
        }

        std::ranges::sort(occurrences);
        auto last = std::unique(occurrences.begin(), occurrences.end());
        occurrences.erase(last, occurrences.end());

#elif defined(ATOM_USE_BOOST)
        std::string text_str(text);
        std::string pattern_str(pattern_);
        std::vector<std::string::iterator> iters;
        boost::algorithm::knuth_morris_pratt(
            text_str.begin(), text_str.end(), pattern_str.begin(),
            pattern_str.end(), std::back_inserter(iters));

        // Transform iterators to positions
        occurrences.reserve(iters.size());
        std::ranges::transform(
            iters, std::back_inserter(occurrences), [&text_str](auto it) {
                return static_cast<int>(std::distance(text_str.begin(), it));
            });
#else
        // Standard KMP algorithm with C++20 optimizations
        int i = 0;
        int j = 0;
        while (i < n) {
            if (text[i] == pattern_copy[j]) {
                ++i;
                ++j;
                if (j == m) {
                    occurrences.push_back(i - m);
                    j = failure_copy[j - 1];
                }
            } else if (j > 0) {
                j = failure_copy[j - 1];
            } else {
                ++i;
            }
        }
#endif
        spdlog::info("KMP search completed with {} occurrences found.",
                     occurrences.size());
    } catch (const std::exception& e) {
        spdlog::error("Exception in KMP::search: {}", e.what());
        throw std::runtime_error(std::string("KMP search failed: ") + e.what());
    }
    return occurrences;
}

auto KMP::searchParallel(std::string_view text, size_t chunk_size) const
    -> std::vector<int> {
    if (text.empty())
        return {};
    std::string pattern_copy;
    std::vector<int> failure_copy;
    {
        std::shared_lock lock(mutex_);
        pattern_copy = pattern_;
        failure_copy = failure_;
    }
    if (pattern_copy.empty() || text.length() < pattern_copy.length()) {
        return {};
    }
    try {
        std::vector<int> occurrences;
        auto n = static_cast<int>(text.length());
        auto m = static_cast<int>(pattern_copy.length());

        // Adjust chunk size if needed
        chunk_size = std::max(chunk_size, static_cast<size_t>(m) * 2);
        chunk_size = std::min(chunk_size, text.length());

        // Calculate optimal thread count based on hardware and workload
        unsigned int thread_count = std::min(
            static_cast<unsigned int>(std::thread::hardware_concurrency()),
            static_cast<unsigned int>((text.length() / chunk_size) + 1));

        // If text is too small, just use standard search
        if (thread_count <= 1 || n <= static_cast<int>(chunk_size * 2)) {
            // Use the optimized search (above) with local copies
            int i = 0, j = 0;
            while (i < n) {
                if (text[i] == pattern_copy[j]) {
                    ++i;
                    ++j;
                    if (j == m) {
                        occurrences.push_back(i - m);
                        j = failure_copy[j - 1];
                    }
                } else if (j > 0) {
                    j = failure_copy[j - 1];
                } else {
                    ++i;
                }
            }
            return occurrences;
        }

        // Launch search tasks
        std::vector<std::future<std::vector<int>>> futures;
        futures.reserve(thread_count);

        for (size_t start = 0; start < text.size(); start += chunk_size) {
            // Calculate chunk end with overlap to catch patterns crossing
            // boundaries
            size_t end = std::min(start + chunk_size + m - 1, text.size());
            size_t search_start = start;

            // Adjust start for all chunks except the first one
            if (start > 0) {
                search_start = start - (m - 1);
            }

            std::string_view chunk =
                text.substr(search_start, end - search_start);

            futures.push_back(std::async(
                std::launch::async,
                [pattern_copy, failure_copy, chunk, search_start]() {
                    std::vector<int> local_occurrences;

                    // Standard KMP algorithm on the chunk
                    auto n = static_cast<int>(chunk.length());
                    auto m = static_cast<int>(pattern_copy.length());
                    int i = 0, j = 0;

                    while (i < n) {
                        if (chunk[i] == pattern_copy[j]) {
                            ++i;
                            ++j;
                            if (j == m) {
                                // Adjust position to global text coordinates
                                int position =
                                    static_cast<int>(search_start) + i - m;
                                local_occurrences.push_back(position);
                                j = failure_copy[j - 1];
                            }
                        } else if (j > 0) {
                            j = failure_copy[j - 1];
                        } else {
                            ++i;
                        }
                    }

                    return local_occurrences;
                }));
        }

        // Collect and merge results
        for (auto& future : futures) {
            auto chunk_occurrences = future.get();
            occurrences.insert(occurrences.end(), chunk_occurrences.begin(),
                               chunk_occurrences.end());
        }

        // Sort and remove duplicates (overlapping chunks might find same match)
        std::ranges::sort(occurrences);
        auto last = std::unique(occurrences.begin(), occurrences.end());
        occurrences.erase(last, occurrences.end());

        return occurrences;
    } catch (const std::exception& e) {
        spdlog::error("Exception in KMP::searchParallel: {}", e.what());
        throw std::runtime_error(std::string("KMP parallel search failed: ") +
                                 e.what());
    }
}

void KMP::setPattern(std::string_view pattern) {
    try {
        std::unique_lock lock(mutex_);
        spdlog::info("Setting new pattern for KMP of length {}",
                     pattern.size());
        pattern_ = pattern;
        failure_ = computeFailureFunction(pattern_);
    } catch (const std::exception& e) {
        spdlog::error("Failed to set KMP pattern: {}", e.what());
        THROW_INVALID_ARGUMENT(std::string("Invalid pattern: ") + e.what());
    }
}

auto KMP::computeFailureFunction(std::string_view pattern) noexcept
    -> std::vector<int> {
    spdlog::info("Computing failure function for pattern.");
    auto m = static_cast<int>(pattern.length());
    std::vector<int> failure(m, 0);

    // Optimization: Use constexpr for empty pattern case
    if (m <= 1) {
        return failure;
    }

    // Compute failure function using dynamic programming
    int j = 0;
    for (int i = 1; i < m; ++i) {
        // Use previous values of failure function to avoid recomputation
        while (j > 0 && pattern[i] != pattern[j]) {
            j = failure[j - 1];
        }

        if (pattern[i] == pattern[j]) {
            failure[i] = ++j;
        }
    }

    spdlog::info("Failure function computed.");
    return failure;
}

BoyerMoore::BoyerMoore(std::string_view pattern) {
    try {
        spdlog::info("Initializing BoyerMoore with pattern length: {}",
                     pattern.size());
        if (pattern.empty()) {
            spdlog::warn("Initialized BoyerMoore with empty pattern");
        }
        setPattern(pattern);
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize BoyerMoore: {}", e.what());
        THROW_INVALID_ARGUMENT(std::string("Invalid pattern: ") + e.what());
    }
}

auto BoyerMoore::search(std::string_view text) const -> std::vector<int> {
    std::vector<int> occurrences;
    try {
        // Only lock for copying pattern_ and shift tables
        std::string pattern_copy;
        std::unordered_map<char, int> bad_char_shift_copy;
        std::vector<int> good_suffix_shift_copy;
        {
            std::lock_guard lock(mutex_);
            pattern_copy = pattern_;
            bad_char_shift_copy = bad_char_shift_;
            good_suffix_shift_copy = good_suffix_shift_;
        }
        auto n = static_cast<int>(text.length());
        auto m = static_cast<int>(pattern_copy.length());
        spdlog::info(
            "BoyerMoore searching text of length {} with pattern length .", n,
            m);
        if (m == 0) {
            spdlog::warn("Empty pattern provided to BoyerMoore::search.");
            return occurrences;
        }

#ifdef ATOM_USE_OPENMP
        std::vector<int> local_occurrences[omp_get_max_threads()];
#pragma omp parallel
        {
            int thread_num = omp_get_thread_num();
            int i = thread_num;
            while (i <= n - m) {
                int j = m - 1;
                while (j >= 0 && pattern_copy[j] == text[i + j]) {
                    --j;
                }
                if (j < 0) {
                    local_occurrences[thread_num].push_back(i);
                    i += good_suffix_shift_copy[0];
                } else {
                    int badCharShift = bad_char_shift_copy.find(text[i + j]) !=
                                               bad_char_shift_copy.end()
                                           ? bad_char_shift_copy.at(text[i + j])
                                           : m;
                    i += std::max(good_suffix_shift_copy[j + 1],
                                  badCharShift - m + 1 + j);
                }
            }
        }
        for (int t = 0; t < omp_get_max_threads(); ++t) {
            occurrences.insert(occurrences.end(), local_occurrences[t].begin(),
                               local_occurrences[t].end());
        }
#elif defined(ATOM_USE_BOOST)
        std::string text_str(text);
        std::string pattern_str(pattern_);
        std::vector<std::string::iterator> iters;
        boost::algorithm::boyer_moore_search(
            text_str.begin(), text_str.end(), pattern_str.begin(),
            pattern_str.end(), std::back_inserter(iters));
        for (auto it : iters) {
            occurrences.push_back(std::distance(text_str.begin(), it));
        }
#else
        int i = 0;
        while (i <= n - m) {
            int j = m - 1;
            while (j >= 0 && pattern_copy[j] == text[i + j]) {
                --j;
            }
            if (j < 0) {
                occurrences.push_back(i);
                i += good_suffix_shift_copy[0];
            } else {
                int badCharShift = bad_char_shift_copy.find(text[i + j]) !=
                                           bad_char_shift_copy.end()
                                       ? bad_char_shift_copy.at(text[i + j])
                                       : m;
                i += std::max(good_suffix_shift_copy[j + 1],
                              badCharShift - m + 1 + j);
            }
        }
#endif
        spdlog::info("BoyerMoore search completed with {} occurrences found.",
                     occurrences.size());
    } catch (const std::exception& e) {
        spdlog::error("Exception in BoyerMoore::search: {}", e.what());
        throw;
    }
    return occurrences;
}

auto BoyerMoore::searchOptimized(std::string_view text) const
    -> std::vector<int> {
    std::vector<int> occurrences;
    try {
        std::string pattern_copy;
        std::unordered_map<char, int> bad_char_shift_copy;
        std::vector<int> good_suffix_shift_copy;
        {
            std::lock_guard lock(mutex_);
            pattern_copy = pattern_;
            bad_char_shift_copy = bad_char_shift_;
            good_suffix_shift_copy = good_suffix_shift_;
        }
        auto n = static_cast<int>(text.length());
        auto m = static_cast<int>(pattern_copy.length());
        spdlog::info(
            "BoyerMoore optimized search on text length {} with pattern length "
            "{}",
            n, m);
        if (m == 0 || n < m) {
            spdlog::info(
                "Early return: empty pattern or text shorter than pattern");
            return occurrences;
        }
#ifdef ATOM_USE_SIMD
        if (m <= 16) {
            __m128i pattern_vec = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(pattern_copy.data()));
            for (int i = 0; i <= n - m; ++i) {
                __m128i text_vec = _mm_loadu_si128(
                    reinterpret_cast<const __m128i*>(text.data() + i));
                __m128i cmp = _mm_cmpeq_epi8(text_vec, pattern_vec);
                uint16_t mask = _mm_movemask_epi8(cmp);
                uint16_t expected_mask = (1 << m) - 1;
                if ((mask & expected_mask) == expected_mask) {
                    occurrences.push_back(i);
                }
                if (i + m < n) {
                    char next_char = text[i + m];
                    int skip = bad_char_shift_copy.find(next_char) !=
                                       bad_char_shift_copy.end()
                                   ? bad_char_shift_copy.at(next_char)
                                   : m;
                    i += std::max(1, skip - 1);
                }
            }
            return occurrences;
        } else {
            for (int i = 0; i <= n - m;) {
                int j = m - 1;
                if (j >= 15) {
                    __m128i pattern_end =
                        _mm_loadu_si128(reinterpret_cast<const __m128i*>(
                            pattern_copy.data() + j - 15));
                    __m128i text_end =
                        _mm_loadu_si128(reinterpret_cast<const __m128i*>(
                            text.data() + i + j - 15));
                    uint16_t mask = _mm_movemask_epi8(
                        _mm_cmpeq_epi8(pattern_end, text_end));
                    if (mask != 0xFFFF) {
                        int mismatch_pos = __builtin_ctz(~mask);
                        j = j - 15 + mismatch_pos;
                        char bad_char = text[i + j];
                        int skip = bad_char_shift_copy.find(bad_char) !=
                                           bad_char_shift_copy.end()
                                       ? bad_char_shift_copy.at(bad_char)
                                       : m;
                        i += std::max(1, j - skip + 1);
                        continue;
                    }
                    j -= 16;
                }
                while (j >= 0 && pattern_copy[j] == text[i + j]) {
                    --j;
                }
                if (j < 0) {
                    occurrences.push_back(i);
                    i += good_suffix_shift_copy[0];
                } else {
                    char bad_char = text[i + j];
                    int skip = bad_char_shift_copy.find(bad_char) !=
                                       bad_char_shift_copy.end()
                                   ? bad_char_shift_copy.at(bad_char)
                                   : m;
                    i += std::max(good_suffix_shift_copy[j + 1], j - skip + 1);
                }
            }
            return occurrences;
        }
#elif defined(ATOM_USE_OPENMP)
        std::vector<int> local_occurrences[omp_get_max_threads()];
#pragma omp parallel
        {
            int thread_num = omp_get_thread_num();
            int i = thread_num;
            while (i <= n - m) {
                int j = m - 1;
                while (j >= 0 && pattern_copy[j] == text[i + j]) {
                    --j;
                }
                if (j < 0) {
                    local_occurrences[thread_num].push_back(i);
                    i += good_suffix_shift_copy[0];
                } else {
                    int badCharShift = bad_char_shift_copy.find(text[i + j]) !=
                                               bad_char_shift_copy.end()
                                           ? bad_char_shift_copy.at(text[i + j])
                                           : m;
                    i += std::max(good_suffix_shift_copy[j + 1],
                                  badCharShift - m + 1 + j);
                }
            }
        }
        for (int t = 0; t < omp_get_max_threads(); ++t) {
            occurrences.insert(occurrences.end(), local_occurrences[t].begin(),
                               local_occurrences[t].end());
        }
#else
        int i = 0;
        while (i <= n - m) {
            int j = m - 1;
            while (j >= 0 && pattern_copy[j] == text[i + j]) {
                --j;
            }
            if (j < 0) {
                occurrences.push_back(i);
                i += good_suffix_shift_copy[0];
            } else {
                char bad_char = text[i + j];
                int skip = bad_char_shift_copy.find(bad_char) !=
                                   bad_char_shift_copy.end()
                               ? bad_char_shift_copy.at(bad_char)
                               : m;
                i += std::max(good_suffix_shift_copy[j + 1], j - skip + 1);
            }
        }
#endif
        spdlog::info(
            "BoyerMoore optimized search completed with {} occurrences found.",
            occurrences.size());
    } catch (const std::exception& e) {
        spdlog::error("Exception in BoyerMoore::searchOptimized: {}", e.what());
        throw std::runtime_error(
            std::string("BoyerMoore optimized search failed: ") + e.what());
    }
    return occurrences;
}

void BoyerMoore::setPattern(std::string_view pattern) {
    std::lock_guard lock(mutex_);
    spdlog::info("Setting new pattern for BoyerMoore: {0:.{1}}", pattern.data(),
                 static_cast<int>(pattern.size()));
    pattern_ = std::string(pattern);
    computeBadCharacterShift();
    computeGoodSuffixShift();
}

void BoyerMoore::computeBadCharacterShift() noexcept {
    spdlog::info("Computing bad character shift table.");
    bad_char_shift_.clear();
    for (int i = 0; i < static_cast<int>(pattern_.length()) - 1; ++i) {
        bad_char_shift_[pattern_[i]] =
            static_cast<int>(pattern_.length()) - 1 - i;
    }
    spdlog::info("Bad character shift table computed.");
}

void BoyerMoore::computeGoodSuffixShift() noexcept {
    spdlog::info("Computing good suffix shift table.");
    auto m = static_cast<int>(pattern_.length());
    good_suffix_shift_.resize(m + 1, m);
    std::vector<int> suffix(m + 1, 0);
    suffix[m] = m + 1;

    for (int i = m; i > 0; --i) {
        int j = i - 1;
        while (j >= 0 && pattern_[j] != pattern_[m - 1 - (i - 1 - j)]) {
            --j;
        }
        suffix[i - 1] = j + 1;
    }

    for (int i = 0; i <= m; ++i) {
        good_suffix_shift_[i] = m;
    }

    for (int i = m; i > 0; --i) {
        if (suffix[i - 1] == i) {
            for (int j = 0; j < m - i; ++j) {
                if (good_suffix_shift_[j] == m) {
                    good_suffix_shift_[j] = m - i;
                }
            }
        }
    }

    for (int i = 0; i < m - 1; ++i) {
        good_suffix_shift_[m - suffix[i]] = m - 1 - i;
    }
    spdlog::info("Good suffix shift table computed.");
}

}  // namespace atom::algorithm
