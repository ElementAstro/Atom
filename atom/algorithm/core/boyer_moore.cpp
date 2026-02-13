#include "boyer_moore.hpp"

#include <algorithm>

#include "spdlog/spdlog.h"

#ifdef ATOM_USE_OPENMP
#include <omp.h>
#endif

#ifdef ATOM_USE_SIMD
#include <immintrin.h>
#endif

#ifdef _MSC_VER
#include <intrin.h>  // For _mm_prefetch
#endif

#ifdef ATOM_USE_BOOST
#include <boost/algorithm/string.hpp>
#endif

#include "atom/error/exception.hpp"

namespace atom::algorithm {

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
        std::lock_guard lock(mutex_);
        auto n = static_cast<int>(text.length());
        auto m = static_cast<int>(pattern_.length());
        spdlog::info(
            "BoyerMoore searching text of length {} with pattern length {}.", n,
            m);
        if (m == 0) {
            spdlog::warn("Empty pattern provided to BoyerMoore::search.");
            return occurrences;
        }

#if defined(ATOM_USE_OPENMP) && defined(_OPENMP)
        std::vector<int> local_occurrences[omp_get_max_threads()];
#pragma omp parallel
        {
            int thread_num = omp_get_thread_num();
            int i = thread_num;
            while (i <= n - m) {
                int j = m - 1;
                while (j >= 0 && pattern_[j] == text[i + j]) {
                    --j;
                }
                if (j < 0) {
                    local_occurrences[thread_num].push_back(i);
                    i += good_suffix_shift_[0];
                } else {
                    int badCharShift = bad_char_shift_.find(text[i + j]) !=
                                               bad_char_shift_.end()
                                           ? bad_char_shift_.at(text[i + j])
                                           : m;
                    i += std::max(good_suffix_shift_[j + 1],
                                  static_cast<int>(badCharShift - m + 1 + j));
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
            while (j >= 0 && pattern_[j] == text[i + j]) {
                --j;
            }
            if (j < 0) {
                occurrences.push_back(i);
                i += good_suffix_shift_[0];
            } else {
                int badCharShift =
                    bad_char_shift_.find(text[i + j]) != bad_char_shift_.end()
                        ? bad_char_shift_.at(text[i + j])
                        : m;
                i += std::max(good_suffix_shift_[j + 1],
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
        std::lock_guard lock(mutex_);
        auto n = static_cast<int>(text.length());
        auto m = static_cast<int>(pattern_.length());

        spdlog::info(
            "BoyerMoore optimized search on text length {} with pattern "
            "length {}",
            n, m);

        if (m == 0 || n < m) {
            spdlog::info(
                "Early return: empty pattern or text shorter than pattern");
            return occurrences;
        }

#ifdef ATOM_USE_SIMD
        // SIMD-optimized search for patterns of suitable length
        if (m <= 16) {  // SSE register can compare 16 chars at once
            __m128i pattern_vec = _mm_loadu_si128(
                reinterpret_cast<const __m128i*>(pattern_.data()));

            for (int i = 0; i <= n - m; ++i) {
                // Load 16 bytes from text starting at position i
                __m128i text_vec = _mm_loadu_si128(
                    reinterpret_cast<const __m128i*>(text.data() + i));

                // Compare characters (returns a mask where 1s indicate matches)
                __m128i cmp = _mm_cmpeq_epi8(text_vec, pattern_vec);
                uint16_t mask = _mm_movemask_epi8(cmp);

                // For exact pattern length match
                uint16_t expected_mask = (1 << m) - 1;
                if ((mask & expected_mask) == expected_mask) {
                    occurrences.push_back(i);
                }

                // Use Boyer-Moore shift to skip ahead
                if (i + m < n) {
                    char next_char = text[i + m];
                    int skip =
                        bad_char_shift_.find(next_char) != bad_char_shift_.end()
                            ? bad_char_shift_.at(next_char)
                            : m;
                    i += std::max(1, skip - 1);  // -1 because loop increments i
                }
            }
        } else {
            // Use vectorized bad character lookup for longer patterns
            for (int i = 0; i <= n - m;) {
                int j = m - 1;

                // Compare last 16 characters with SIMD if possible
                if (j >= 15) {
                    __m128i pattern_end =
                        _mm_loadu_si128(reinterpret_cast<const __m128i*>(
                            pattern_.data() + j - 15));
                    __m128i text_end =
                        _mm_loadu_si128(reinterpret_cast<const __m128i*>(
                            text.data() + i + j - 15));

                    uint16_t mask = _mm_movemask_epi8(
                        _mm_cmpeq_epi8(pattern_end, text_end));

                    // If any mismatch in last 16 chars, find first mismatch
                    if (mask != 0xFFFF) {
                        int mismatch_pos = __builtin_ctz(~mask);
                        j = j - 15 + mismatch_pos;

                        // Apply bad character rule
                        char bad_char = text[i + j];
                        int skip = bad_char_shift_.find(bad_char) !=
                                           bad_char_shift_.end()
                                       ? bad_char_shift_.at(bad_char)
                                       : m;
                        i += std::max(
                            1, j - skip + 1);  // -1 because loop increments i
                        continue;
                    }

                    // Last 16 matched, check remaining chars
                    j -= 16;
                }

                // Standard checking for remaining characters
                while (j >= 0 && pattern_[j] == text[i + j]) {
                    --j;
                }

                if (j < 0) {
                    occurrences.push_back(i);
                    i += good_suffix_shift_[0];
                } else {
                    char bad_char = text[i + j];
                    int skip =
                        bad_char_shift_.find(bad_char) != bad_char_shift_.end()
                            ? bad_char_shift_.at(bad_char)
                            : m;
                    i += std::max(good_suffix_shift_[j + 1], j - skip + 1);
                }
            }
        }
#elif defined(ATOM_USE_OPENMP) && defined(_OPENMP)
        // Improved OpenMP implementation with efficient scheduling
        const int max_threads = omp_get_max_threads();
        std::vector<std::vector<int>> local_occurrences(max_threads);

        // Optimal chunk size estimation
        const int chunk_size =
            std::min(1000, std::max(100, n / (max_threads * 2)));

#pragma omp parallel for schedule(dynamic, chunk_size) num_threads(max_threads)
        for (int i = 0; i <= n - m; ++i) {
            int thread_num = omp_get_thread_num();
            int j = m - 1;

            // Inner loop optimization with strength reduction
            while (j >= 0 && pattern_[j] == text[i + j]) {
                --j;
            }

            if (j < 0) {
                local_occurrences[thread_num].push_back(i);
                // Skip ahead using good suffix rule
                i += good_suffix_shift_[0] -
                     1;  // -1 compensates for loop increment
            } else {
                // Calculate shift using precomputed tables
                char bad_char = text[i + j];
                int bc_shift =
                    bad_char_shift_.find(bad_char) != bad_char_shift_.end()
                        ? bad_char_shift_.at(bad_char)
                        : m;
                int shift =
                    std::max(good_suffix_shift_[j + 1], j - bc_shift + 1);

                // Skip ahead, compensating for loop increment
                i += shift - 1;
            }
        }

        // Merge and sort results
        int total_size = 0;
        for (const auto& vec : local_occurrences) {
            total_size += vec.size();
        }

        occurrences.reserve(total_size);
        for (const auto& vec : local_occurrences) {
            occurrences.insert(occurrences.end(), vec.begin(), vec.end());
        }

        // Ensure results are sorted
        if (total_size > 1) {
            std::ranges::sort(occurrences);
        }
#else
        // Optimized standard Boyer-Moore with better cache usage
        int i = 0;
        while (i <= n - m) {
            // Cache pattern length and use registers efficiently
            const int pattern_len = m;
            int j = pattern_len - 1;

            // Process 4 characters at a time when possible
            while (j >= 3 && pattern_[j] == text[i + j] &&
                   pattern_[j - 1] == text[i + j - 1] &&
                   pattern_[j - 2] == text[i + j - 2] &&
                   pattern_[j - 3] == text[i + j - 3]) {
                j -= 4;
            }

            // Handle remaining characters
            while (j >= 0 && pattern_[j] == text[i + j]) {
                --j;
            }

            if (j < 0) {
                occurrences.push_back(i);
                i += good_suffix_shift_[0];
            } else {
                char bad_char = text[i + j];

                // Use reference to avoid map lookups
                const auto& bc_map = bad_char_shift_;
                int bc_shift = bc_map.find(bad_char) != bc_map.end()
                                   ? bc_map.at(bad_char)
                                   : pattern_len;

                // Pre-fetch next text character to improve cache hits
                if (i + pattern_len < n) {
#ifdef _MSC_VER
                    _mm_prefetch(
                        reinterpret_cast<const char*>(&text[i + pattern_len]),
                        _MM_HINT_T0);
#else
                    __builtin_prefetch(&text[i + pattern_len], 0, 0);
#endif
                }

                i += std::max(good_suffix_shift_[j + 1], j - bc_shift + 1);
            }
        }
#endif
        spdlog::info(
            "BoyerMoore optimized search completed with {} occurrences found.",
            occurrences.size());
    } catch (const std::exception& e) {
        spdlog::error("Exception in BoyerMoore::searchOptimized: {}", e.what());
        THROW_RUNTIME_ERROR(
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
