cmake_minimum_required(VERSION 3.7)
project(Lab3)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES main.cpp inc/common.h inc/common.cpp inc/tinyxml2.h inc/tinyxml2.cpp)
add_executable(Lab3 ${SOURCE_FILES})