find_path(
  ASIO_INCLUDE_DIR
  NAMES asio.hpp
  PATH_SUFFIXES asio)

set(_asio_include_dirs "")
set(_asio_compile_defs "")
set(_asio_link_libs "")

if(ASIO_INCLUDE_DIR)
  set(ASIO_STANDALONE TRUE)
  set(ASIO_INCLUDE_DIRS ${ASIO_INCLUDE_DIR})
  set(_asio_include_dirs ${ASIO_INCLUDE_DIRS})
  set(_asio_compile_defs "ASIO_STANDALONE")

  mark_as_advanced(ASIO_INCLUDE_DIR)
else()
  find_package(Boost QUIET COMPONENTS system)
  if(Boost_FOUND)
    find_path(BOOST_ASIO_INCLUDE_DIR boost/asio.hpp PATHS ${Boost_INCLUDE_DIRS})
    if(BOOST_ASIO_INCLUDE_DIR)
      set(ASIO_STANDALONE FALSE)
      set(ASIO_INCLUDE_DIR ${BOOST_ASIO_INCLUDE_DIR})
      set(ASIO_INCLUDE_DIRS ${Boost_INCLUDE_DIRS})
      set(_asio_include_dirs ${ASIO_INCLUDE_DIRS})
      set(_asio_link_libs Boost::system)

    endif()
    mark_as_advanced(BOOST_ASIO_INCLUDE_DIR)
  endif()
endif()

if(_asio_include_dirs)
  if(NOT TARGET Asio::Asio)
    add_library(Asio::Asio INTERFACE IMPORTED)
  endif()
  set_target_properties(
    Asio::Asio
    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_asio_include_dirs}"
               INTERFACE_COMPILE_DEFINITIONS "${_asio_compile_defs}"
               INTERFACE_LINK_LIBRARIES "${_asio_link_libs}")

  if(NOT TARGET asio::asio)
    add_library(asio::asio INTERFACE IMPORTED)
  endif()
  set_target_properties(
    asio::asio
    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${_asio_include_dirs}"
               INTERFACE_COMPILE_DEFINITIONS "${_asio_compile_defs}"
               INTERFACE_LINK_LIBRARIES "${_asio_link_libs}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(asio DEFAULT_MSG ASIO_INCLUDE_DIR)

set(Asio_FOUND ${asio_FOUND})
set(ASIO_FOUND ${asio_FOUND})
