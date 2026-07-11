// Standalone translation unit for the rtype (Reflectable serialization) tests.
//
// Unlike most type tests, test_rtype.hpp is NOT pulled into the aggregated
// test_header_only.cpp: rtype.hpp does `using namespace atom::meta`, and in the
// aggregated TU (where earlier headers leave `using namespace atom::type`
// active) that makes the unqualified `detail` inside atom/meta/concept.hpp
// ambiguous between atom::type::detail and atom::meta::detail. Compiling it in
// its own TU keeps those directives isolated. The gtest main() comes from the
// gtest_main library linked into atom_type_tests.
#include "test_rtype.hpp"
