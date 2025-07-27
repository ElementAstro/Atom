#include "memory_pool.hpp"
#include <vector>
#include <string>

namespace atom::extra::curl {
namespace pools {

// Global memory pools for common curl types using atom library directly
MemoryPool<std::vector<char>, 2048> response_buffer_pool;  // High throughput with more objects per chunk
MemoryPool<std::string> string_pool;  // Default configuration

}  // namespace pools
}  // namespace atom::extra::curl
