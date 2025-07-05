#ifndef ATOM_WEB_ADDRESS_MAIN_HPP
#define ATOM_WEB_ADDRESS_MAIN_HPP

// 主要的项目包含文件
// 统一引入所有地址类型和相关功能

#include "address.hpp"
#include "ipv4.hpp"
#include "ipv6.hpp"
#include "unix_domain.hpp"

/**
 * @namespace atom::web
 * @brief 用于网络相关功能的命名空间
 *
 * 这个命名空间包含了各种网络地址类型和相关操作：
 * - IPv4 地址处理
 * - IPv6 地址处理
 * - Unix 域套接字路径处理
 * - 跨平台网络地址转换
 */

namespace atom::web {

/**
 * @brief 创建一个合适类型的地址对象
 * @param addressString 地址字符串 (可以是IPv4, IPv6或Unix域套接字路径)
 * @return std::unique_ptr<Address> 指向创建的地址对象的智能指针
 *
 * 这是一个便捷函数，它会自动检测地址类型并创建相应的对象。
 * 如果地址类型无法识别，将返回nullptr。
 */
inline auto createAddress(std::string_view addressString) {
    return Address::createFromString(addressString);
}

} // namespace atom::web

#endif // ATOM_WEB_ADDRESS_MAIN_HPP
