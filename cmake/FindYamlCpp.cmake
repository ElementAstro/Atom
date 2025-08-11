# - Try to find yaml-cpp
# Once done this will define
#
#  YAMLCPP_FOUND - system has yaml-cpp
#  YAMLCPP_INCLUDE_DIRS - the yaml-cpp include directory
#  YAMLCPP_LIBRARIES - Link these to use yaml-cpp
#  YAMLCPP_DEFINITIONS - Compiler switches required for using yaml-cpp
#  YAMLCPP_VERSION - yaml-cpp 的版本号
#
# Copyright (c) 2006, Alexander Neundorf, <neundorf@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

# 使用 pkg-config 查找 yaml-cpp（主要用于 Linux/Unix）
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_YAMLCPP QUIET yaml-cpp)
  set(YAMLCPP_VERSION ${PC_YAMLCPP_VERSION})
  set(YAMLCPP_DEFINITIONS ${PC_YAMLCPP_CFLAGS_OTHER})
endif()

# 允许用户通过环境变量或 CMake 变量指定自定义路径
set(_YAMLCPP_POSSIBLE_ROOT_DIRS
  ${YAMLCPP_ROOT_DIR}
  $ENV{YAMLCPP_ROOT_DIR}
  ${CMAKE_PREFIX_PATH}
  $ENV{CMAKE_PREFIX_PATH}
)

# 特定平台的路径提示
if(WIN32)
  list(APPEND _YAMLCPP_POSSIBLE_ROOT_DIRS
    "C:/Program Files/yaml-cpp"
    "C:/yaml-cpp"
    "$ENV{PROGRAMFILES}/yaml-cpp"
  )
  # 在 Windows 上，库可能具有版本化的文件名，如 yaml-cpp0.7.dll
  if(MSVC)
    set(_YAMLCPP_MSVC_SUFFIX "md" CACHE STRING "基于运行时库选择的 yaml-cpp 库后缀")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      set(_YAMLCPP_LIB_NAMES
        yaml-cppd
        libyaml-cppd
        yaml-cpp${_YAMLCPP_MSVC_SUFFIX}d
        libyaml-cpp${_YAMLCPP_MSVC_SUFFIX}d
        yaml-cpp
        libyaml-cpp)
    else()
      set(_YAMLCPP_LIB_NAMES
        yaml-cpp
        libyaml-cpp
        yaml-cpp${_YAMLCPP_MSVC_SUFFIX}
        libyaml-cpp${_YAMLCPP_MSVC_SUFFIX})
    endif()
  else()
    set(_YAMLCPP_LIB_NAMES yaml-cpp libyaml-cpp)
  endif()
elseif(APPLE)
  list(APPEND _YAMLCPP_POSSIBLE_ROOT_DIRS
    "/usr/local"
    "/usr/local/Cellar/yaml-cpp" # Homebrew
    "/opt/homebrew/Cellar/yaml-cpp" # Apple Silicon 上的 Homebrew
    "/opt/local" # MacPorts
  )
  set(_YAMLCPP_LIB_NAMES yaml-cpp libyaml-cpp)
else() # Linux 和其他类 Unix 系统
  list(APPEND _YAMLCPP_POSSIBLE_ROOT_DIRS
    "/usr"
    "/usr/local"
    "/opt/yaml-cpp"
  )
  set(_YAMLCPP_LIB_NAMES yaml-cpp libyaml-cpp)
endif()

# 尝试查找包含目录
find_path(YAMLCPP_INCLUDE_DIR
  NAMES yaml-cpp/yaml.h
  HINTS
    ${PC_YAMLCPP_INCLUDEDIR}
    ${PC_YAMLCPP_INCLUDE_DIRS}
    ${_YAMLCPP_POSSIBLE_ROOT_DIRS}
  PATH_SUFFIXES
    include
    yaml-cpp/include
)

# 尝试查找库文件
find_library(YAMLCPP_LIBRARY
  NAMES ${_YAMLCPP_LIB_NAMES}
  HINTS
    ${PC_YAMLCPP_LIBDIR}
    ${PC_YAMLCPP_LIBRARY_DIRS}
    ${_YAMLCPP_POSSIBLE_ROOT_DIRS}
  PATH_SUFFIXES
    lib
    lib64
    lib/${CMAKE_LIBRARY_ARCHITECTURE}
    libs
    yaml-cpp/lib
    yaml-cpp/lib64
)

# 如果未通过 pkg-config 找到版本信息，则尝试从 yaml-cpp/version.h 提取
if(YAMLCPP_INCLUDE_DIR AND NOT YAMLCPP_VERSION)
  file(READ "${YAMLCPP_INCLUDE_DIR}/yaml-cpp/version.h" _YAMLCPP_VERSION_HEADER)
  string(REGEX MATCH "#define YAML_CPP_VERSION_MAJOR ([0-9]+)" _YAMLCPP_MAJOR_VERSION_MATCH "${_YAMLCPP_VERSION_HEADER}")
  string(REGEX MATCH "#define YAML_CPP_VERSION_MINOR ([0-9]+)" _YAMLCPP_MINOR_VERSION_MATCH "${_YAMLCPP_VERSION_HEADER}")
  string(REGEX MATCH "#define YAML_CPP_VERSION_PATCH ([0-9]+)" _YAMLCPP_PATCH_VERSION_MATCH "${_YAMLCPP_VERSION_HEADER}")

  if(_YAMLCPP_MAJOR_VERSION_MATCH AND _YAMLCPP_MINOR_VERSION_MATCH AND _YAMLCPP_PATCH_VERSION_MATCH)
    string(REGEX REPLACE "#define YAML_CPP_VERSION_MAJOR ([0-9]+)" "\\1" _YAMLCPP_MAJOR_VERSION "${_YAMLCPP_MAJOR_VERSION_MATCH}")
    string(REGEX REPLACE "#define YAML_CPP_VERSION_MINOR ([0-9]+)" "\\1" _YAMLCPP_MINOR_VERSION "${_YAMLCPP_MINOR_VERSION_MATCH}")
    string(REGEX REPLACE "#define YAML_CPP_VERSION_PATCH ([0-9]+)" "\\1" _YAMLCPP_PATCH_VERSION "${_YAMLCPP_PATCH_VERSION_MATCH}")

    set(YAMLCPP_VERSION "${_YAMLCPP_MAJOR_VERSION}.${_YAMLCPP_MINOR_VERSION}.${_YAMLCPP_PATCH_VERSION}")
  endif()
endif()

# 设置标准导出变量
set(YAMLCPP_LIBRARIES ${YAMLCPP_LIBRARY})
set(YAMLCPP_INCLUDE_DIRS ${YAMLCPP_INCLUDE_DIR})

# 处理标准 find_package 参数
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(YAMLCPP
  REQUIRED_VARS YAMLCPP_LIBRARY YAMLCPP_INCLUDE_DIR
  VERSION_VAR YAMLCPP_VERSION
)

# 将缓存变量标记为高级
mark_as_advanced(
  YAMLCPP_INCLUDE_DIR
  YAMLCPP_LIBRARY
  _YAMLCPP_MSVC_SUFFIX
)
