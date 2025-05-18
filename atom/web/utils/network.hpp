/*
 * network.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2025-5-17

Description: Network connectivity status functions

**************************************************/

#ifndef ATOM_WEB_UTILS_NETWORK_HPP
#define ATOM_WEB_UTILS_NETWORK_HPP

namespace atom::web {

/**
 * @brief Check if the device has active internet connectivity
 *
 * @return true if internet is available, false otherwise
 */
auto checkInternetConnectivity() -> bool;

}  // namespace atom::web

#endif  // ATOM_WEB_UTILS_NETWORK_HPP
