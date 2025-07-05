# - Try to find the Readline library
# Once done, this will define
#  Readline_FOUND - System has Readline
#  Readline_INCLUDE_DIRS - The Readline include directories
#  Readline_LIBRARIES - The libraries needed to use Readline

# Check if Readline_ROOT is defined in environment variables
if(DEFINED ENV{READLINE_ROOT})
    set(READLINE_ROOT $ENV{READLINE_ROOT})
endif()

# **Windows specific search paths**
if(WIN32)
    # Native Windows paths
    list(APPEND CMAKE_PREFIX_PATH
        "C:/Program Files/readline"
        "C:/readline"
    )

    # **MSYS2 environment paths**
    # First, try to get MSYS2 paths from the PATH environment variable
    set(_msys_prefixes_from_env_path "")
    if(DEFINED ENV{PATH})
        set(_path_list "$ENV{PATH}")
        string(REPLACE ";" "\\;" _path_list "${_path_list}")
        string(REPLACE "\\" "/" _path_list "${_path_list}")

        if(WIN32)
            string(REPLACE ";" "\\\\;" _path_list_escaped "${_path_list}")
            string(REPLACE "\\\\;" ";" _path_list_escaped "${_path_list_escaped}")
            string(REPLACE ";" "\;" _path_list_esc "${_path_list_escaped}")
            string(REPLACE "\;" ";" _path_list_cmake "${_path_list_esc}")
        else()
            string(REPLACE ":" ";" _path_list_cmake "${_path_list}")
        endif()

        foreach(_path_entry IN LISTS _path_list_cmake)
            string(REPLACE "\\" "/" _path_entry "${_path_entry}")

            if(_path_entry MATCHES ".*/mingw64/bin$")
                get_filename_component(_prefix_mingw64 "${_path_entry}" DIRECTORY)
                list(APPEND _msys_prefixes_from_env_path "${_prefix_mingw64}")
                get_filename_component(_msys_root "${_prefix_mingw64}" DIRECTORY)
                if(IS_DIRECTORY "${_msys_root}/usr")
                    list(APPEND _msys_prefixes_from_env_path "${_msys_root}/usr")
                endif()
                if(IS_DIRECTORY "${_msys_root}/mingw32")
                    list(APPEND _msys_prefixes_from_env_path "${_msys_root}/mingw32")
                endif()
            elseif(_path_entry MATCHES ".*/mingw32/bin$")
                get_filename_component(_prefix_mingw32 "${_path_entry}" DIRECTORY)
                list(APPEND _msys_prefixes_from_env_path "${_prefix_mingw32}")
                get_filename_component(_msys_root "${_prefix_mingw32}" DIRECTORY)
                if(IS_DIRECTORY "${_msys_root}/usr")
                    list(APPEND _msys_prefixes_from_env_path "${_msys_root}/usr")
                endif()
                if(IS_DIRECTORY "${_msys_root}/mingw64")
                    list(APPEND _msys_prefixes_from_env_path "${_msys_root}/mingw64")
                endif()
            elseif(_path_entry MATCHES ".*/usr/bin$")
                get_filename_component(_prefix_usr "${_path_entry}" DIRECTORY)
                if(IS_DIRECTORY "${_prefix_usr}/include")
                    list(APPEND _msys_prefixes_from_env_path "${_prefix_usr}")
                    get_filename_component(_msys_root "${_prefix_usr}" DIRECTORY)
                    if(IS_DIRECTORY "${_msys_root}/mingw64")
                        list(APPEND _msys_prefixes_from_env_path "${_msys_root}/mingw64")
                    endif()
                    if(IS_DIRECTORY "${_msys_root}/mingw32")
                        list(APPEND _msys_prefixes_from_env_path "${_msys_root}/mingw32")
                    endif()
                endif()
            endif()
        endforeach()

        if(_msys_prefixes_from_env_path)
            list(REMOVE_DUPLICATES _msys_prefixes_from_env_path)
            list(APPEND CMAKE_PREFIX_PATH ${_msys_prefixes_from_env_path})
            message(STATUS "Found MSYS2 prefixes from PATH: ${_msys_prefixes_from_env_path}")
        endif()
    endif()

    # Second, check MSYS2_ROOT environment variable
    if(DEFINED ENV{MSYS2_ROOT})
        string(REPLACE "\\" "/" _MSYS2_ROOT_FWD "$ENV{MSYS2_ROOT}")
        if(IS_DIRECTORY "${_MSYS2_ROOT_FWD}/mingw64")
            list(APPEND CMAKE_PREFIX_PATH "${_MSYS2_ROOT_FWD}/mingw64")
        endif()
        if(IS_DIRECTORY "${_MSYS2_ROOT_FWD}/mingw32")
            list(APPEND CMAKE_PREFIX_PATH "${_MSYS2_ROOT_FWD}/mingw32")
        endif()
        if(IS_DIRECTORY "${_MSYS2_ROOT_FWD}/usr")
            list(APPEND CMAKE_PREFIX_PATH "${_MSYS2_ROOT_FWD}/usr")
        endif()
    else()
        # Finally, check common MSYS2 installation paths
        list(APPEND CMAKE_PREFIX_PATH
            "D:/msys64/mingw64"
            "D:/msys64/mingw32"
            "D:/msys64/usr"
            "C:/msys64/mingw64"
            "C:/msys64/mingw32"
            "C:/msys64/usr"
            "C:/msys32/mingw32"
            "C:/msys32/usr"
        )
    endif()

    # Ensure no duplicates in CMAKE_PREFIX_PATH from all sources
    if(CMAKE_PREFIX_PATH)
        list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
    endif()
endif()

# Find include directory
find_path(Readline_INCLUDE_DIR
    NAMES readline/readline.h
    PATHS
        ${READLINE_ROOT}/include
        /usr/include
        /usr/local/include
        /opt/local/include
        /sw/include
    PATH_SUFFIXES readline
)

# Find library - consider different library names and extensions
if(WIN32)
    find_library(Readline_LIBRARY
        NAMES readline libreadline readline.lib
        PATHS
            ${READLINE_ROOT}/lib
            /usr/lib
            /usr/local/lib
            /opt/local/lib
            /sw/lib
    )

    # **On Windows/MSYS2, Readline often depends on ncurses or termcap**
    find_library(Readline_NCURSES_LIBRARY
        NAMES ncurses libncurses ncursesw libncursesw pdcurses
        PATHS
            ${READLINE_ROOT}/lib
            /usr/lib
            /usr/local/lib
            /opt/local/lib
            /sw/lib
    )

    find_library(Readline_TERMCAP_LIBRARY
        NAMES termcap libtermcap
        PATHS
            ${READLINE_ROOT}/lib
            /usr/lib
            /usr/local/lib
            /opt/local/lib
            /sw/lib
    )
else()
    find_library(Readline_LIBRARY
        NAMES readline
        PATHS
            ${READLINE_ROOT}/lib
            /usr/lib
            /usr/local/lib
            /opt/local/lib
            /sw/lib
    )
endif()

# 设置依赖库
set(Readline_DEPS_LIBRARIES "")
if(Readline_NCURSES_LIBRARY)
    list(APPEND Readline_DEPS_LIBRARIES ${Readline_NCURSES_LIBRARY})
endif()
if(Readline_TERMCAP_LIBRARY)
    list(APPEND Readline_DEPS_LIBRARIES ${Readline_TERMCAP_LIBRARY})
endif()

include(FindPackageHandleStandardArgs)
# 处理QUIETLY和REQUIRED参数并设置Readline_FOUND
find_package_handle_standard_args(Readline DEFAULT_MSG
    Readline_LIBRARY Readline_INCLUDE_DIR
)

if(Readline_FOUND)
    set(Readline_LIBRARIES ${Readline_LIBRARY} ${Readline_DEPS_LIBRARIES})
    set(Readline_INCLUDE_DIRS ${Readline_INCLUDE_DIR})
endif()

mark_as_advanced(
    Readline_INCLUDE_DIR
    Readline_LIBRARY
    Readline_NCURSES_LIBRARY
    Readline_TERMCAP_LIBRARY
)
