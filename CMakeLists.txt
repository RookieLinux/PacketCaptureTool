cmake_minimum_required(VERSION 3.16)

project(PacketCaptureTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
set(CMAKE_PREFIX_PATH CACHE "/opt/Qt/Qt6.6.2/6.6.2/gcc_64/" FORCE)
set(FLUENTUI_BUILD_EXAMPLES OFF CACHE BOOL "Disable FluentUI examples" FORCE)
set(FLUENTUI_QML_PLUGIN_DIRECTORY "${CMAKE_BINARY_DIR}/qml/FluentUI" CACHE PATH "Path to FluentUI plugin" FORCE)

include(GitCheck)
include(ThirdParty)

if (CMAKE_HOST_UNIX)
    configure_file(
            cmake/build_info.h.in
            ${PROJECT_SOURCE_DIR}/src/build_info.h
    )
else (CMAKE_HOST_WIN32)
    configure_file(
            ${PROJECT_SOURCE_DIR}\\cmake\\build_info.h.in
            ${PROJECT_SOURCE_DIR}\\src\\build_info.h
    )
endif ()

# Find Qt6 packages (minimum version 6.4)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Network)

# Include directories
include_directories(SYSTEM BEFORE
    ${PcapPlusPlus_SOURCE_DIR}/Common++/header
    ${PcapPlusPlus_SOURCE_DIR}/Packet++/header
    ${PcapPlusPlus_SOURCE_DIR}/Pcap++/header
)
include_directories(BEFORE
    ${PROJECT_SOURCE_DIR}/src/backend
    ${PROJECT_SOURCE_DIR}/src/ui
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

# Enable Qt-specific automatic build tools for this target
set_target_properties(${PROJECT_NAME} PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Common++
    Packet++
    Pcap++
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
