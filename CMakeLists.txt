cmake_minimum_required(VERSION 3.16)
project(qt-msg-reader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# libmsg is a standalone, Qt-agnostic C library; only the static library
# target is needed here, not its own demo tool/test suite. Keep AUTOMOC/
# AUTOUIC/AUTORCC from leaking into it - it has no Qt sources of its own.
set(LIBMSG_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(LIBMSG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTORCC OFF)
set(CMAKE_AUTOUIC OFF)
add_subdirectory(libmsg)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/EmailTypes.h
    src/MsgParser.h
    src/MsgParser.cpp
    src/MsgFileModel.h
    src/MsgFileModel.cpp
    src/AttachmentModel.h
    src/AttachmentModel.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets
    msg
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
