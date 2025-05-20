# 示例目录: async
cmake_minimum_required(VERSION 3.10)

# 获取子目录名称（用于可执行文件前缀）
get_filename_component(SUBDIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# 可选: 添加特定于async示例的选项
option(ATOM_EXAMPLE_ASYNC_BUILD_ALL "Build all async examples" ON)

# 获取所有CPP文件
file(GLOB CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

# 为每个CPP文件创建可执行文件
foreach(CPP_FILE ${CPP_FILES})
    # 获取不带扩展名的文件名
    get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)
    
    # 构造可执行文件名称（子目录名_文件名）
    set(EXECUTABLE_NAME ${SUBDIR_NAME}_${EXAMPLE_NAME})
    
    # 配置选项，允许单独控制每个示例的构建
    string(TOUPPER ${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
    option(ATOM_EXAMPLE_ASYNC_${EXAMPLE_NAME_UPPER} "Build async example: ${EXAMPLE_NAME}" ${ATOM_EXAMPLE_ASYNC_BUILD_ALL})
    
    # 有条件地添加可执行文件
    if(ATOM_EXAMPLE_ASYNC_${EXAMPLE_NAME_UPPER})
        add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
        target_link_libraries(${EXECUTABLE_NAME} atom)
        
        
        # 设置IDE文件夹分组
        set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/Async")
    endif()
endforeach()
