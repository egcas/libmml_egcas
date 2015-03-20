cmake_minimum_required(VERSION 2.8.11)

project(egcas)

# Tell CMake to run moc when necessary:
set(CMAKE_AUTOMOC ON)
# As moc files are generated in the binary dir, tell CMake
# to always look for includes there:
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library(egcas SHARED ${LIBMMLEGCAS_HEADERS_MOC} eg_mml_document.cpp eg_mml_entity_table.cpp)

#only for cmake >= 3.1
#set_property(TARGET egcas PROPERTY CXX_STANDARD 11)
#set_property(TARGET egcas PROPERTY CXX_STANDARD_REQUIRED ON)

add_definitions(-std=c++11)

# Widgets finds its own dependencies.
find_package(Qt5 COMPONENTS Widgets Xml)

qt5_wrap_cpp(LIBMMLEGCAS_HEADERS_MOC eg_mml_document.h eg_mml_entity_table.h)


target_link_libraries(egcas Qt5::Xml Qt5::Widgets)

add_definitions(-DMML_TEST)
  
#if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
#    set_target_properties(target PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:WINDOWS")
#    set_target_properties(target PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
#endif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  
