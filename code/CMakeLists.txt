cmake_minimum_required(VERSION 3.8)
project(code)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        inc/common.h
        inc/ctrie.h
        inc/nodes.h
        src/ctrie.c
        src/main.c)

include_directories(inc)

#set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ".")

add_executable(code ${SOURCE_FILES})