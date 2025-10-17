# CMakeLists.txt for Modern XML library (pugixml wrapper)
# Integrated with main Atom build system
# Header-only library

# Find and link pugixml library (required)
find_package(pugixml QUIET)
if(pugixml_FOUND)
    # Header files
    set(PUGIXML_HEADERS
        modern_xml.hpp
        xml_builder.hpp
        xml_document.hpp
        xml_node_wrapper.hpp
        xml_query.hpp
    )

    # Create interface library (header-only)
    add_library(atom-extra-pugixml INTERFACE)

    # Set target properties
    target_include_directories(atom-extra-pugixml INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

    # Set target properties for consistency
    target_compile_features(atom-extra-pugixml INTERFACE cxx_std_20)

    target_link_libraries(atom-extra-pugixml INTERFACE pugixml::pugixml)
    message(STATUS "Found pugixml, linking to atom-extra-pugixml")
else()
    message(STATUS "pugixml not found, atom-extra-pugixml will be skipped")
endif()

# Installation rules (optional)
# install(TARGETS atom-extra-pugixml DESTINATION lib)
# install(FILES ${PUGIXML_HEADERS} DESTINATION include/atom/extra/pugixml)
