cmake_minimum_required(VERSION 3.16)
project(libmsg VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(LIBMSG_BUILD_TOOLS "Build the msgdump demo CLI tool" ON)
option(LIBMSG_BUILD_TESTS "Build the libmsg test suite" ON)

add_library(msg STATIC
    src/cfb.c
    src/cfb.h
    src/msg_strings.c
    src/msg_strings.h
    src/msg_properties.c
    src/msg_properties.h
    src/msg_reader.c
)

target_include_directories(msg
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    target_compile_options(msg PRIVATE /W4)
else()
    target_compile_options(msg PRIVATE -Wall -Wextra)
endif()

if(LIBMSG_BUILD_TOOLS)
    add_executable(msgdump tools/msgdump.c)
    target_link_libraries(msgdump PRIVATE msg)
endif()

if(LIBMSG_BUILD_TESTS)
    enable_testing()
    add_executable(test_basic tests/test_basic.c)
    target_link_libraries(test_basic PRIVATE msg)
    add_test(NAME test_basic COMMAND test_basic "${CMAKE_CURRENT_SOURCE_DIR}/tests/sample.msg")
endif()
