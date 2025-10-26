# 示例目录: meta
cmake_minimum_required(VERSION 3.10)

# 获取子目录名称（用于可执行文件前缀）
get_filename_component(SUBDIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# 可选: 添加特定于meta示例的选项
option(ATOM_EXAMPLE_META_BUILD_ALL "Build all meta examples" ON)

# 获取所有CPP文件
file(GLOB CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

# 为每个CPP文件创建可执行文件
foreach(CPP_FILE ${CPP_FILES})
  # 获取不带扩展名的文件名
  get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

  # 构造可执行文件名称（子目录名_文件名）
  set(EXECUTABLE_NAME ${SUBDIR_NAME}_${EXAMPLE_NAME})

  # 暂时跳过不通过的示例以解锁主构建
  if(EXAMPLE_NAME STREQUAL "anymeta"
     OR EXAMPLE_NAME STREQUAL "any"
     OR EXAMPLE_NAME STREQUAL "decorate"
     OR EXAMPLE_NAME STREQUAL "bind_first"
     OR EXAMPLE_NAME STREQUAL "constructor"
     OR EXAMPLE_NAME STREQUAL "concept"
     OR EXAMPLE_NAME STREQUAL "container_traits"
     OR EXAMPLE_NAME STREQUAL "enum"
     OR EXAMPLE_NAME STREQUAL "facade"
     OR EXAMPLE_NAME STREQUAL "facade_any"
     OR EXAMPLE_NAME STREQUAL "ffi"
     OR EXAMPLE_NAME STREQUAL "property"
     OR EXAMPLE_NAME STREQUAL "invoke"
     OR EXAMPLE_NAME STREQUAL "refl")
    message(STATUS "Skipping meta example: ${EXAMPLE_NAME}")
    continue()
  endif()

  # 配置选项，允许单独控制每个示例的构建
  string(TOUPPER ${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
  option(ATOM_EXAMPLE_META_${EXAMPLE_NAME_UPPER}
         "Build meta example: ${EXAMPLE_NAME}" ${ATOM_EXAMPLE_META_BUILD_ALL})

  # 有条件地添加可执行文件
  if(ATOM_EXAMPLE_META_${EXAMPLE_NAME_UPPER})
    add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
    target_link_libraries(${EXECUTABLE_NAME} atom)

    # 设置IDE文件夹分组
    set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER "Examples/Meta")
  endif()
endforeach()
