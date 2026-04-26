cmake_minimum_required(VERSION 3.25)

project(spkg)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(deps/toolkit)

file(GLOB_RECURSE SOURCES src/*.cxx)
add_executable(spkg ${SOURCES})
target_include_directories(spkg PRIVATE inc)
target_link_libraries(spkg PRIVATE toolkit::json toolkit::toml)

install(TARGETS spkg)
