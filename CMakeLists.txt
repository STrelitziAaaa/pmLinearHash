cmake_minimum_required(VERSION 3.10)

project(pmlh)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -g -Wall -Wno-unused-variable")
link_libraries(pmem pthread)
add_executable(pmlhash  pml_hash.cc pml_hash_testing.cc util.cc)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
