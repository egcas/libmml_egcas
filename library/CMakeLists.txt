cmake_minimum_required(VERSION 2.8.11)

project(mmlegcas)

# Tell CMake to run moc when necessary:
set(CMAKE_AUTOMOC ON)
# As moc files are generated in the binary dir, tell CMake
# to always look for includes there:
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(mmlegcas SHARED  
        eg_mml_document.cpp 
        eg_mml_entity_table.cpp
)

#only for cmake >= 3.1
#set_property(TARGET mmlegcas PROPERTY CXX_STANDARD 11)
#set_property(TARGET mmlegcas PROPERTY CXX_STANDARD_REQUIRED ON)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        add_definitions(-std=c++11)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        add_definitions(-std=c++11)
endif()

# Widgets finds its own dependencies.
find_package(Qt5 COMPONENTS Widgets Xml)

set_target_properties(mmlegcas PROPERTIES VERSION 1.0.0 SOVERSION 1)
target_link_libraries(mmlegcas Qt5::Xml Qt5::Widgets)

#for in-tree build (publish API headers in binary dir)
add_custom_command(TARGET mmlegcas
        PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/libegcas
        COMMAND ${CMAKE_COMMAND} -E copy eg_mml_document.h ${CMAKE_CURRENT_BINARY_DIR}/libegcas
        COMMAND ${CMAKE_COMMAND} -E copy eg_mml_entity_table.h ${CMAKE_CURRENT_BINARY_DIR}/libegcas
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "copy the mmlegcas library headers to the include directory"
)

install (TARGETS mmlegcas ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
install (FILES eg_mml_document.h  eg_mml_entity_table.h DESTINATION include/mmlegcas )

#add_definitions(-DMML_TEST)
  
#if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
#    set_target_properties(target PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:WINDOWS")
#    set_target_properties(target PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
#endif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt5widgets5, libqt5xml5" )
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY " libmmlegcas is a library that renders Math ML code.")
set(CPACK_PACKAGE_NAME "libmmlegcas")
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set(CPACK_GENERATOR "DEB")        
        set(PACKAGE_OS "linux")
        if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
                set(CPACK_SYSTEM_NAME "${PACKAGE_OS}-amd64")
        endif()
        if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "i386")
                set(CPACK_SYSTEM_NAME "${PACKAGE_OS}-i386")
        endif()        
endif()
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Johannes Maier <maier_jo@gmx.de>")
set(CPACK_PACKAGE_VERSION "0.0.1")

include(CPack)

