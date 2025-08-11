cmake_minimum_required(VERSION 3.15)

# 设置内存模块源文件
set(MEMORY_SOURCES
    memory.cpp
    common.cpp
)

# 根据平台添加对应的源文件
if(WIN32)
    list(APPEND MEMORY_SOURCES windows.cpp)
elseif(UNIX AND NOT APPLE)
    list(APPEND MEMORY_SOURCES linux.cpp)
elseif(APPLE)
    list(APPEND MEMORY_SOURCES macos.cpp)
endif()

# 创建内存模块的库
add_library(atom_sysinfo_memory STATIC ${MEMORY_SOURCES})

# 设置包含目录
target_include_directories(atom_sysinfo_memory
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接依赖库
target_link_libraries(atom_sysinfo_memory
    PUBLIC
        atom_log
)

# 平台特定库链接
if(WIN32)
    target_link_libraries(atom_sysinfo_memory PRIVATE psapi pdh iphlpapi)
elseif(UNIX AND NOT APPLE)
    # Linux特定库
    find_package(Threads REQUIRED)
    target_link_libraries(atom_sysinfo_memory PRIVATE ${CMAKE_THREAD_LIBS_INIT})
elseif(APPLE)
    # macOS特定库
    target_link_libraries(atom_sysinfo_memory PRIVATE "-framework CoreFoundation" "-framework IOKit")
endif()

# 设置编译选项
target_compile_features(atom_sysinfo_memory PUBLIC cxx_std_17)

# 安装规则
install(TARGETS atom_sysinfo_memory
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
        memory.hpp
        common.hpp
        $<$<PLATFORM_ID:Windows>:windows.hpp>
        $<$<PLATFORM_ID:Linux>:linux.hpp>
        $<$<PLATFORM_ID:Darwin>:macos.hpp>
    DESTINATION include/atom/sysinfo/memory
)
