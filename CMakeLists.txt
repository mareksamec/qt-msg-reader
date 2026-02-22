cmake_minimum_required(VERSION 3.16)
project(qt-msg-reader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

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
    Python3::Python
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${Python3_INCLUDE_DIRS}
)

# Define paths for Python packages
set(PYTHON_VENV_PATH "${CMAKE_CURRENT_SOURCE_DIR}/.venv")
set(PYTHON_PACKAGES_DIR "python-packages")

target_compile_definitions(${PROJECT_NAME} PRIVATE
    PYTHON_VENV_PATH="${PYTHON_VENV_PATH}"
    PYTHON_PACKAGES_DIR="${PYTHON_PACKAGES_DIR}"
)

# Copy Python packages to build directory for development
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/${PYTHON_PACKAGES_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory 
        "${PYTHON_VENV_PATH}/lib/python3.14/site-packages"
        "${CMAKE_BINARY_DIR}/${PYTHON_PACKAGES_DIR}"
    COMMENT "Copying Python packages to build directory"
)

# Install target with bundled Python packages
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

install(DIRECTORY "${PYTHON_VENV_PATH}/lib/python3.14/site-packages/"
    DESTINATION ${PYTHON_PACKAGES_DIR}
    PATTERN "*.pyc" EXCLUDE
    PATTERN "__pycache__" EXCLUDE
)
