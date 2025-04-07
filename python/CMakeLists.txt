cmake_minimum_required(VERSION 3.14)
project(atom_python_bindings)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 查找依赖包
find_package(Python COMPONENTS Interpreter Development REQUIRED)
find_package(pybind11 CONFIG REQUIRED)

# 设置输出目录
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# 定义要监视的模块类型
set(MODULE_TYPES
    "algorithm"
    # 在此添加其他模块类型，例如：
    # "graphics"
    # "network"
)

# 为每个模块类型创建源文件列表变量
foreach(type ${MODULE_TYPES})
    set(${type}_SOURCES "" CACHE INTERNAL "")
    set(${type}_FOUND FALSE CACHE INTERNAL "")
endforeach()

# 收集源文件的函数
function(collect_sources_for_module module_type dir)
    # 查找所有 cpp 文件
    file(GLOB cpp_files "${dir}/*.cpp")
    if(NOT cpp_files)
        message(STATUS "No cpp files found in ${dir}, skipping.")
        return()
    endif()
    
    # 将找到的源文件添加到对应的列表中
    message(STATUS "Adding sources from ${dir} to ${module_type} module:")
    foreach(file ${cpp_files})
        message(STATUS "  - ${file}")
    endforeach()
    
    # 将源文件添加到列表
    set(${module_type}_SOURCES ${${module_type}_SOURCES} ${cpp_files} CACHE INTERNAL "")
    set(${module_type}_FOUND TRUE CACHE INTERNAL "")
endfunction()

# 递归扫描目录
function(scan_directories dir)
    # 获取当前目录下的所有项目
    file(GLOB items RELATIVE ${dir} ${dir}/*)
    
    foreach(item ${items})
        # 完整路径
        set(item_path "${dir}/${item}")
        
        # 如果是目录，则进一步处理
        if(IS_DIRECTORY ${item_path})
            # 检查是否是我们感兴趣的模块类型
            list(FIND MODULE_TYPES ${item} type_index)
            if(type_index GREATER -1)
                # 如果是我们感兴趣的模块类型，则收集源文件
                collect_sources_for_module(${item} ${item_path})
            else()
                # 否则继续递归扫描
                scan_directories(${item_path})
            endif()
        endif()
    endforeach()
endfunction()

scan_directories(${CMAKE_CURRENT_SOURCE_DIR})

foreach(type ${MODULE_TYPES})
    if(${type}_FOUND)
        message(STATUS "Creating Python module for ${type} with sources: ${${type}_SOURCES}")
        
        # 创建 Python 模块
        pybind11_add_module(atom_${type} ${${type}_SOURCES})
        
        # 设置包含目录
        target_include_directories(atom_${type} PRIVATE
            ${CMAKE_SOURCE_DIR}/..  # Atom 根目录
        )
        
        # 链接对应的 Atom 库
        target_link_libraries(atom_${type} PRIVATE
            atom-${type}
        )
        
        # 设置输出名称
        set_target_properties(atom_${type} PROPERTIES
            PREFIX ""  # 移除 lib 前缀
            SUFFIX "${PYTHON_MODULE_EXTENSION}"  # 使用正确的扩展名
        )
        
        # 安装规则
        install(TARGETS atom_${type}
            LIBRARY DESTINATION python/${type}
            RUNTIME DESTINATION python/${type}
        )
        
        # 处理 __init__.py
        set(init_py_dir "${CMAKE_CURRENT_SOURCE_DIR}/${type}")
        set(init_py_path "${init_py_dir}/__init__.py")
        
        # 确保目录存在
        if(NOT EXISTS ${init_py_dir})
            file(MAKE_DIRECTORY ${init_py_dir})
        endif()
        
        # 如果不存在 __init__.py，则创建一个
        if(NOT EXISTS ${init_py_path})
            file(WRITE ${init_py_path} "# Auto-generated __init__.py for ${type} module")
        endif()
        
        install(FILES ${init_py_path} DESTINATION python/${type})
    endif()
endforeach()

# 添加测试支持（可选）
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
