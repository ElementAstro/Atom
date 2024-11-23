cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(GenerateExecutables)

# 获取当前目录的所有子目录
file(GLOB SUBDIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)

# 遍历所有子目录
foreach(SUBDIR ${SUBDIRS})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR})
        # 获取子目录中的所有 .cpp 文件
        file(GLOB CPP_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR} ${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR}/*.cpp)

        # 遍历所有 .cpp 文件
        foreach(CPP_FILE ${CPP_FILES})
            # 构造可执行文件名称（子文件夹名_文件名）
            string(REPLACE ".cpp" "" EXECUTABLE_NAME ${CPP_FILE})
            set(EXECUTABLE_NAME ${SUBDIR}_${EXECUTABLE_NAME})

            # 添加可执行文件
            add_executable(${EXECUTABLE_NAME} ${SUBDIR}/${CPP_FILE})
            message(STATUS "Add executable: ${EXECUTABLE_NAME}")

            # 可选：设置编译选项、链接库等
            # target_compile_options(${EXECUTABLE_NAME} PRIVATE ...)
            # target_link_libraries(${EXECUTABLE_NAME} ...)
            target_link_libraries(${EXECUTABLE_NAME} atom)
        endforeach()
    endif()
endforeach()
