cmake_minimum_required(VERSION 3.25)

add_subdirectory(dep/json)

project(source-package)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES src/*.cxx)
add_executable(spkg ${SOURCES})
target_include_directories(spkg PRIVATE inc)
target_link_libraries(spkg PRIVATE json)

install(TARGETS spkg)
