# CMakeLists.txt for dotenv library
# Integrated with main Atom build system

# Source files
set(DOTENV_SOURCES
    dotenv.cpp
    parser.cpp
    validator.cpp
    loader.cpp
)

# Header files
set(DOTENV_HEADERS
    dotenv.hpp
    parser.hpp
    validator.hpp
    loader.hpp
    exceptions.hpp
    test_dotenv.hpp
    test_validator.hpp
)

# Create library
add_library(atom-extra-dotenv ${DOTENV_SOURCES} ${DOTENV_HEADERS})

# Set target properties
target_include_directories(atom-extra-dotenv PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Set target properties for consistency
set_target_properties(atom-extra-dotenv PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Find required packages
find_package(Threads REQUIRED)
target_link_libraries(atom-extra-dotenv PUBLIC Threads::Threads)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(atom-extra-dotenv PUBLIC ws2_32)
endif()

# Installation rules (optional)
# install(TARGETS atom-extra-dotenv DESTINATION lib)
# install(FILES ${DOTENV_HEADERS} DESTINATION include/atom/extra/dotenv)
