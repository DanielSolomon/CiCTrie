cmake_minimum_required(VERSION 3.8)
project(code)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        inc/common.h
        inc/ctrie.h
        inc/nodes.h
        src/ctrie.c
        src/main.c
        src/common.c inc/hazard_pointer.h src/hazard_pointer.c)

include_directories(inc)

add_executable(code ${SOURCE_FILES})