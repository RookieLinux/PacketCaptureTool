cmake_minimum_required(VERSION 3.16)

project(PacketCaptureTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt6 configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (CMAKE_HOST_UNIX)
    configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/.cmake/build_info.h.in
            ${CMAKE_CURRENT_SOURCE_DIR}/src/build_info.h
    )
else (CMAKE_HOST_WIN32)
    configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}\\.cmake\\build_info.h.in
            ${CMAKE_CURRENT_SOURCE_DIR}\\src\\build_info.h
    )
endif ()

# Find Qt6 packages (minimum version 6.4)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)

# Find PcapPlusPlus
find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH}
    "/usr/local/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}"
)
pkg_check_modules(PCAPPLUSPLUS REQUIRED PcapPlusPlus)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src/backend
    ${CMAKE_SOURCE_DIR}/src/ui
    ${PCAPPLUSPLUS_INCLUDE_DIRS}
)

# Source files
set(BACKEND_SOURCES
    src/backend/PacketCaptureEngine.cpp
    src/backend/FilterEngine.cpp
    src/backend/ProtocolParser.cpp
    src/backend/ConfigurationLoader.cpp
    src/backend/CaptureController.cpp
    src/backend/PacketModel.cpp
)

set(BACKEND_HEADERS
    src/backend/PacketCaptureEngine.h
    src/backend/FilterEngine.h
    src/backend/ProtocolParser.h
    src/backend/ConfigurationLoader.h
    src/backend/CaptureController.h
    src/backend/PacketModel.h
)

# QML resources
set(QML_RESOURCES
    src/ui/qml.qrc
)

# Main executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    ${BACKEND_SOURCES}
    ${BACKEND_HEADERS}
    ${QML_RESOURCES}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    ${PCAPPLUSPLUS_LIBRARIES}
)

# Link directories for PcapPlusPlus
target_link_directories(${PROJECT_NAME} PRIVATE
    ${PCAPPLUSPLUS_LIBRARY_DIRS}
)

# Compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

# Enable testing
enable_testing()
add_subdirectory(tests)
