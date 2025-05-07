# - Try to find the Readline library
# Once done, this will define
#  Readline_FOUND - System has Readline
#  Readline_INCLUDE_DIRS - The Readline include directories
#  Readline_LIBRARIES - The libraries needed to use Readline

# 检查环境变量中是否定义了Readline路径
if(DEFINED ENV{READLINE_ROOT})
    set(READLINE_ROOT $ENV{READLINE_ROOT})
endif()

# **Windows特定搜索路径**
if(WIN32)
    # 原生Windows路径
    list(APPEND CMAKE_PREFIX_PATH 
        "C:/Program Files/readline"
        "C:/readline"
    )
    
    # **MSYS2环境路径**
    if(DEFINED ENV{MSYS2_ROOT})
        list(APPEND CMAKE_PREFIX_PATH 
            "$ENV{MSYS2_ROOT}/mingw64"
            "$ENV{MSYS2_ROOT}/mingw32"
            "$ENV{MSYS2_ROOT}/usr"
        )
    else()
        # 常见MSYS2安装路径
        list(APPEND CMAKE_PREFIX_PATH 
            "C:/msys64/mingw64"
            "C:/msys64/mingw32"
            "C:/msys64/usr"
            "C:/msys32/mingw32"
            "C:/msys32/usr"
        )
    endif()
endif()

# 查找头文件
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

# 查找库文件 - 考虑不同的库名和扩展名
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
    
    # **在Windows/MSYS2上，Readline通常依赖ncurses或termcap**
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
