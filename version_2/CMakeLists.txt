cmake_minimum_required(VERSION 2.8)

project(myHttpServer)

aux_source_directory(./src SRC_LIST)

include_directories(./include)

add_executable(myHttpServer ${SRC_LIST})

target_link_libraries(myHttpServer PRIVATE pthread)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")