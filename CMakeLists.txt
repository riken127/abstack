cmake_minimum_required(VERSION 3.21)

project(abstack VERSION 0.6.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ABSTACK_ENABLE_WERROR "Treat warnings as errors" OFF)
option(ABSTACK_ENABLE_TUI "Enable curses-backed TUI command" ON)

add_library(abstack_core)

target_sources(abstack_core
    PRIVATE
        src/frontend/token.cxx
        src/frontend/lexer.cxx
        src/frontend/parser.cxx
        src/semantic/validator.cxx
        src/ir/lowering.cxx
        src/stdlib/library.cxx
        src/codegen/dockerfile_emitter.cxx
        src/codegen/compose_emitter.cxx
        src/format/formatter.cxx
)

target_include_directories(abstack_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(abstack_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(abstack_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(abstack_core PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
    )

    if(ABSTACK_ENABLE_WERROR)
        target_compile_options(abstack_core PRIVATE -Werror)
    endif()
endif()

add_executable(abstack src/cli/main.cxx)
target_link_libraries(abstack PRIVATE abstack_core)

if(ABSTACK_ENABLE_TUI)
    find_package(Curses QUIET)
    if(Curses_FOUND)
        target_compile_definitions(abstack PRIVATE ABSTACK_HAS_CURSES=1)
        target_link_libraries(abstack PRIVATE ${CURSES_LIBRARIES})
        if(DEFINED CURSES_INCLUDE_DIRS)
            target_include_directories(abstack PRIVATE ${CURSES_INCLUDE_DIRS})
        elseif(DEFINED CURSES_INCLUDE_DIR)
            target_include_directories(abstack PRIVATE ${CURSES_INCLUDE_DIR})
        endif()
    else()
        message(STATUS "Curses not found: 'abstack tui' command will be unavailable.")
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(abstack_tests tests/pipeline_test.cxx)
    target_link_libraries(abstack_tests PRIVATE abstack_core)
    add_test(NAME compiler_pipeline COMMAND abstack_tests)
endif()
