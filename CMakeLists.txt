cmake_minimum_required(VERSION 3.20)

project(abstack LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(abstack
    src/main.cxx
    src/lexer.cxx
    src/parser.cxx
)
