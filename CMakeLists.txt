cmake_minimum_required(VERSION 3.10)

project(pmLinHash)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g -Wall -Wno-unused-variable")
link_libraries(pmem pthread)
aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_FILES)
set(SRC_DIR_PATH ${PROJECT_SOURCE_DIR}/src )
SET(EXECUTABLE_OUTPUT_PATH .)
add_executable(pmLinHash  ${SRC_FILES})
