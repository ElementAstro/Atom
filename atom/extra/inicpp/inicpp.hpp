#ifndef ATOM_EXTRA_INICPP_HPP
#define ATOM_EXTRA_INICPP_HPP

#include "common.hpp"
#include "convert.hpp"
#include "field.hpp"
#include "section.hpp"
#include "file.hpp"

#if INICPP_CONFIG_PATH_QUERY
#include "path_query.hpp"
#endif

#if INICPP_CONFIG_EVENT_LISTENERS
#include "event_listener.hpp"
#endif

#if INICPP_CONFIG_FORMAT_CONVERSION
#include "format_converter.hpp"
#endif

/**
 * @namespace inicpp
 * @brief 提供高性能、类型安全的INI配置文件解析功能
 * 
 * 该库具有以下特点：
 * 1. 类型安全 - 通过模板获取强类型字段值
 * 2. 线程安全 - 使用共享锁实现并发读写
 * 3. 高性能 - 支持并行处理、内存池和Boost容器
 * 4. 可扩展 - 支持自定义分隔符、转义字符和注释前缀
 * 5. 丰富功能 - 支持嵌套段落、事件监听、路径查询、格式转换等
 * 
 * 可通过宏控制功能开关：
 * - INICPP_CONFIG_USE_BOOST: 是否使用Boost库
 * - INICPP_CONFIG_USE_BOOST_CONTAINERS: 是否使用Boost容器
 * - INICPP_CONFIG_USE_MEMORY_POOL: 是否使用内存池
 * - INICPP_CONFIG_NESTED_SECTIONS: 是否支持嵌套段落
 * - INICPP_CONFIG_EVENT_LISTENERS: 是否支持事件监听
 * - INICPP_CONFIG_PATH_QUERY: 是否支持路径查询
 * - INICPP_CONFIG_FORMAT_CONVERSION: 是否支持格式转换
 */
namespace inicpp {}

#endif  // ATOM_EXTRA_INICPP_HPP
