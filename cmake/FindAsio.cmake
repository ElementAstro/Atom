find_path(ASIO_INCLUDE_DIR NAMES asio.hpp PATH_SUFFIXES asio)

if(ASIO_INCLUDE_DIR)
    set(Asio_FOUND TRUE)
    set(ASIO_STANDALONE TRUE)
    set(ASIO_INCLUDE_DIRS ${ASIO_INCLUDE_DIR})

    if(NOT TARGET Asio::Asio)
        add_library(Asio::Asio INTERFACE IMPORTED)
        set_target_properties(Asio::Asio PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${ASIO_INCLUDE_DIRS}"
            INTERFACE_COMPILE_DEFINITIONS "ASIO_STANDALONE")
    endif()

    mark_as_advanced(ASIO_INCLUDE_DIR)
else()
    find_package(Boost QUIET COMPONENTS system)
    if(Boost_FOUND)
        find_path(BOOST_ASIO_INCLUDE_DIR boost/asio.hpp PATHS ${Boost_INCLUDE_DIRS})
        if(BOOST_ASIO_INCLUDE_DIR)
            set(Asio_FOUND TRUE)
            set(ASIO_STANDALONE FALSE)
            set(ASIO_INCLUDE_DIRS ${Boost_INCLUDE_DIRS})

            if(NOT TARGET Asio::Asio)
                add_library(Asio::Asio INTERFACE IMPORTED)
                set_target_properties(Asio::Asio PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${Boost_INCLUDE_DIRS}"
                    INTERFACE_LINK_LIBRARIES Boost::system)
            endif()
        endif()
        mark_as_advanced(BOOST_ASIO_INCLUDE_DIR)
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Asio DEFAULT_MSG Asio_FOUND)
