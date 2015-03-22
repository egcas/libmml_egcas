cmake_minimum_required(VERSION 2.8.11)

project(mathmlview)

# Tell CMake to run moc when necessary:
set(CMAKE_AUTOMOC ON)
# As moc files are generated in the binary dir, tell CMake
# to always look for includes there:
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_executable(mathmlview 
        formulaview.cpp 
        mainwindow.cpp 
        main.cpp
)

#only for cmake >= 3.1
#set_property(TARGET mathmlview PROPERTY CXX_STANDARD 11)
#set_property(TARGET mathmlview PROPERTY CXX_STANDARD_REQUIRED ON)

if(${CMAKE_COMPILER_IS_GNUCXX})
        add_definitions(-std=c++11)
endif(${CMAKE_COMPILER_IS_GNUCXX})

# Widgets finds its own dependencies.
find_package(Qt5 COMPONENTS Widgets Xml)
find_library(LIBMMLEGCAS_LIBRARY mmlegcas ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(mathmlview ${LIBMMLEGCAS_LIBRARY} Qt5::Xml Qt5::Widgets)
include_directories(../library)


  
#if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
#    set_target_properties(target PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:WINDOWS")
#    set_target_properties(target PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
#endif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  
