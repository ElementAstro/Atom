#include "address.hpp"
#include <memory>
#include "ipv4.hpp"
#include "ipv6.hpp"
#include "unix_domain.hpp"


namespace atom::web {

// Factory method for creating Address objects
auto Address::createFromString(std::string_view addressStr)
    -> std::unique_ptr<Address> {
    std::unique_ptr<Address> address;

    try {
        // Try IPv4 first
        address = std::make_unique<IPv4>(addressStr);
        return address;
    } catch (const InvalidAddressFormat&) {
        // Not IPv4, try IPv6
        try {
            address = std::make_unique<IPv6>(addressStr);
            return address;
        } catch (const InvalidAddressFormat&) {
            // Not IPv6, try Unix Domain Socket
            try {
                address = std::make_unique<UnixDomain>(addressStr);
                return address;
            } catch (const InvalidAddressFormat&) {
                // Unknown address type
                return nullptr;
            }
        }
    }
}

}  // namespace atom::web
