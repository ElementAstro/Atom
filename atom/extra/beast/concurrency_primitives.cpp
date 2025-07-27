#include "concurrency_primitives.hpp"
#include <spdlog/spdlog.h>

namespace atom::beast::concurrency {

// Static member definitions for HazardPointer
HazardPointer::HazardRecord HazardPointer::hazard_pointers_[MAX_HAZARD_POINTERS];
std::atomic<std::size_t> HazardPointer::hazard_pointer_count_{0};

} // namespace atom::beast::concurrency
