cmake_minimum_required(VERSION 3.0)

project(cmake_test)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_BUILD_TYPE Debug)

# Prepare "Catch" library for other executables
set(CATCH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
add_library(Catch INTERFACE)
target_include_directories(Catch INTERFACE ${CATCH_INCLUDE_DIR})

add_definitions(-DUNIT_TEST -DCONFIG_MILLIS_TIME_EXPR=millis\(\))

set(LIB_SOURCES
    ../src/scriptrunner.cpp
    ../.pio/libdeps/build/rvt-OptionParser/src/optparser.cpp
)

set(LIB_HEADERS
    stub
    ../src
    ../.pio/libdeps/build/rvt-OptionParser/src
)

include_directories(catch2 ${LIB_HEADERS})

# Make test executable
add_executable(tests main.cpp ${LIB_SOURCES})
target_link_libraries(tests Catch)
