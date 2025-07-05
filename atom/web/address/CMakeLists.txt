# CMakeLists.txt for Atom-Web-Address
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Atom-Web-Address
# Description: Address Management API
# Author: Max Qian
# License: GPL3

cmake_minimum_required(VERSION 3.20)

# 获取当前目录下的所有源文件
file(GLOB ADDRESS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB ADDRESS_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")

# 打印找到的源文件，方便调试
message(STATUS "Found address sources: ${ADDRESS_SOURCES}")

# 添加这些文件到父作用域，供主库包含
set(ADDRESS_SOURCES ${ADDRESS_SOURCES} PARENT_SCOPE)
set(ADDRESS_HEADERS ${ADDRESS_HEADERS} PARENT_SCOPE)

# 构建对象库（如需单独使用）
add_library(atom-web-address OBJECT ${ADDRESS_SOURCES} ${ADDRESS_HEADERS})
set_property(TARGET atom-web-address PROPERTY POSITION_INDEPENDENT_CODE 1)

# 依赖项
set(LIBS
    loguru
    ${CMAKE_THREAD_LIBS_INIT}
)

# 根据需要添加特定平台的链接库
if(WIN32)
   list(APPEND LIBS wsock32 ws2_32)
endif()

target_link_libraries(atom-web-address PRIVATE ${LIBS})

# 包含目录
target_include_directories(atom-web-address PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(atom-web-address PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# 确保公共头文件可见
install(FILES ${ADDRESS_HEADERS}
    DESTINATION include/atom/web/address
    COMPONENT Development
)
