cmake_minimum_required(VERSION 3.10)

project(pmLinHash)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -g -Wall -Wno-unused-variable")
link_libraries(pmem pthread)
aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_FILES)
set(SRC_DIR_PATH ${PROJECT_SOURCE_DIR}/src )
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
add_executable(pmLinHash  ${SRC_FILES})
