#ifndef ATOM_SEARCH_SEARCH_HPP
#define ATOM_SEARCH_SEARCH_HPP

/**
 * @file search.hpp
 * @brief Main header for the atom::search module.
 * @details This header includes all search module components for convenience.
 *          For more granular includes, use the individual component headers:
 *          - types.hpp: Basic types, enums, concepts, and configurations
 *          - exceptions.hpp: Exception classes
 *          - document.hpp: Document class
 *          - tokenizer.hpp: Text tokenization
 *          - scoring.hpp: TF-IDF and BM25 scoring algorithms
 *          - search_engine.hpp: Main SearchEngine class
 */

#include "document.hpp"
#include "exceptions.hpp"
#include "scoring.hpp"
#include "search_engine.hpp"
#include "tokenizer.hpp"
#include "types.hpp"

namespace atom::search {

// Re-export concepts for backward compatibility
using concepts::Indexable;
using concepts::ScoreFunction;
using concepts::Searchable;

}  // namespace atom::search

#endif  // ATOM_SEARCH_SEARCH_HPP
