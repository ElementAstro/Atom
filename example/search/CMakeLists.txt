# 示例目录: search
cmake_minimum_required(VERSION 3.10)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 获取子目录名称（用于可执行文件前缀）
get_filename_component(SUBDIR_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# 可选: 添加特定于search示例的选项
option(ATOM_EXAMPLE_SEARCH_BUILD_ALL "Build all search examples" ON)

# Function to build examples from a directory
function(build_examples_from_dir DIR_NAME FOLDER_NAME)
  file(GLOB CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${DIR_NAME}/*.cpp)

  foreach(CPP_FILE ${CPP_FILES})
    # 获取不带扩展名的文件名
    get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

    # 构造可执行文件名称（子目录名_目录名_文件名）
    set(EXECUTABLE_NAME ${SUBDIR_NAME}_${DIR_NAME}_${EXAMPLE_NAME})

    # 配置选项，允许单独控制每个示例的构建
    string(TOUPPER ${DIR_NAME}_${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
    option(ATOM_EXAMPLE_SEARCH_${EXAMPLE_NAME_UPPER}
           "Build search example: ${DIR_NAME}/${EXAMPLE_NAME}"
           ${ATOM_EXAMPLE_SEARCH_BUILD_ALL})

    # 有条件地添加可执行文件
    if(ATOM_EXAMPLE_SEARCH_${EXAMPLE_NAME_UPPER})
      add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
      target_link_libraries(${EXECUTABLE_NAME} atom)

      # 设置IDE文件夹分组
      set_property(TARGET ${EXECUTABLE_NAME}
                   PROPERTY FOLDER "Examples/Search/${FOLDER_NAME}")
    endif()
  endforeach()
endfunction()

# Build examples from subdirectories
build_examples_from_dir("core" "Core")
build_examples_from_dir("cache" "Cache")
build_examples_from_dir("database" "Database")
build_examples_from_dir("integration" "Integration")
build_examples_from_dir("advanced" "Advanced")

# 获取根目录的所有CPP文件（legacy examples）
file(GLOB CPP_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

# 为每个根目录CPP文件创建可执行文件
foreach(CPP_FILE ${CPP_FILES})
  # 获取不带扩展名的文件名
  get_filename_component(EXAMPLE_NAME ${CPP_FILE} NAME_WE)

  # 构造可执行文件名称（子目录名_文件名）
  set(EXECUTABLE_NAME ${SUBDIR_NAME}_${EXAMPLE_NAME})

  # 配置选项，允许单独控制每个示例的构建
  string(TOUPPER ${EXAMPLE_NAME} EXAMPLE_NAME_UPPER)
  option(ATOM_EXAMPLE_SEARCH_${EXAMPLE_NAME_UPPER}
         "Build search example: ${EXAMPLE_NAME}"
         ${ATOM_EXAMPLE_SEARCH_BUILD_ALL})

  # 有条件地添加可执行文件
  if(ATOM_EXAMPLE_SEARCH_${EXAMPLE_NAME_UPPER})
    add_executable(${EXECUTABLE_NAME} ${CPP_FILE})
    target_link_libraries(${EXECUTABLE_NAME} atom)

    # 设置IDE文件夹分组
    set_property(TARGET ${EXECUTABLE_NAME} PROPERTY FOLDER
                                                    "Examples/Search/Legacy")
  endif()
endforeach()
