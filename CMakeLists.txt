cmake_minimum_required(VERSION 3.1.0)

project(WBOIT_tester)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

file(GLOB_RECURSE SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp *.ui *.qrc)

add_executable(WBOIT_tester WIN32 ${SRCS})

find_package(Qt5Widgets CONFIG REQUIRED)
target_link_libraries(WBOIT_tester Qt5::Widgets)
