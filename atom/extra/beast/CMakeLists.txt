# 不再设置cmake_minimum_required和project，由主CMake统一

# 源文件
set(BEAST_SOURCES
    http.cpp
    ws.cpp
)

set(BEAST_HEADERS
    http.hpp
    http_utils.hpp
    ws.hpp
)

add_library(beast ${BEAST_SOURCES} ${BEAST_HEADERS})
target_include_directories(beast PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 可选: 安装规则
# install(TARGETS beast DESTINATION lib)
# install(FILES ${BEAST_HEADERS} DESTINATION include/beast)
