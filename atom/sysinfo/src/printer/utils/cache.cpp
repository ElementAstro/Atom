#include "cache.hpp"

namespace atom::system::utils {

SystemInfoCache& SystemInfoCache::getInstance() {
    static SystemInfoCache instance;
    return instance;
}

} // namespace atom::system::utils
