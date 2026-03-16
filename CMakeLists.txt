cmake_minimum_required(VERSION 3.16)

project(PacketCaptureTool VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(GitCheck)

if (CMAKE_HOST_WIN32)
    set(PCAP_ROOT "${PROJECT_SOURCE_DIR}/3rdparty/npcap-sdk-1.15")
    set(Packet_ROOT "${PROJECT_SOURCE_DIR}/3rdparty/npcap-sdk-1.15")

    # 根据编译器架构选择正确的库目录
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(PCAP_LIB_DIR "${PCAP_ROOT}/Lib/x64")
    else()
        set(PCAP_LIB_DIR "${PCAP_ROOT}/Lib")
    endif()

    # MinGW 缺少 sal.h，Npcap SDK 需要它。定义空的 SAL 宏以兼容。
    if (MINGW)
        add_definitions(-D_In_= -D_Out_= -D_Inout_= -D_In_opt_= -D_Out_opt_= -D_Inout_opt_= -D_Out_writes_bytes_to_opt_= -D_Out_writes_bytes_= -D_In_reads_bytes_= -D_Out_writes_to_= -D_In_reads_= -D_Out_writes_= -D_Post_invalid_= -D_Outptr_= -D_Outptr_result_maybenull_= -D_In_reads_opt_= -D_Out_writes_to_opt_= -D_Inout_updates_bytes_all_= -D_Out_writes_all_opt_= -D_In_reads_bytes_opt_=)
    endif()

    find_library(WPCAP_LIB wpcap PATHS ${PCAP_LIB_DIR} NO_DEFAULT_PATH)
    find_library(PACKET_LIB Packet PATHS ${PCAP_LIB_DIR} NO_DEFAULT_PATH)

    message(STATUS "WPCAP_LIB: ${WPCAP_LIB}")
    message(STATUS "PACKET_LIB: ${PACKET_LIB}")
endif ()

set(FLUENTUI_BUILD_EXAMPLES OFF CACHE BOOL "Disable FluentUI examples" FORCE)
set(FLUENTUI_QML_PLUGIN_DIRECTORY "3rdparty/gitrep/FluentUI/build/qml/FluentUI" CACHE PATH "Path to FluentUI plugin" FORCE)

configure_file(
    cmake/build_info.h.in
    ${PROJECT_SOURCE_DIR}/src/build_info.h
)

# Find Qt6 packages (minimum version 6.4)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 QuickDialogs2) #Network
include(ThirdParty)

# Include directories
include_directories(SYSTEM BEFORE
    ${PROJECT_SOURCE_DIR}/3rdparty/gitrep/FluentUI/src
    ${PROJECT_SOURCE_DIR}/3rdparty/gitrep/PcapPlusPlus/Common++/header
    ${PROJECT_SOURCE_DIR}/3rdparty/gitrep/PcapPlusPlus/Packet++/header
    ${PROJECT_SOURCE_DIR}/3rdparty/gitrep/PcapPlusPlus/Pcap++/header
)

if (CMAKE_HOST_WIN32)
    include_directories(SYSTEM BEFORE
        ${PCAP_ROOT}/Include
    )
endif ()
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
    Qt6::QuickDialogs2
    fluentuiplugin
    Common++
    Packet++
    Pcap++
    ${WPCAP_LIB}
    ${PACKET_LIB}
)

# Compiler flags
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

#Deal libraries
if (WIN32)
    if (MSVC)
        if (CMAKE_SIZEOF_VOID_P EQUAL 4)
            file(GLOB_RECURSE 3RDPARTY_DLL_DIR ${PROJECT_SOURCE_DIR}/3rdparty/msvc/x86/*.dll)
        elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
            file(GLOB_RECURSE 3RDPARTY_DLL_DIR ${PROJECT_SOURCE_DIR}/3rdparty/msvc/x64/*.dll)
        endif ()
    elseif (MINGW)
        file(GLOB_RECURSE 3RDPARTY_DLL_DIR ${PROJECT_SOURCE_DIR}/3rdparty/mingw/*.dll)
    endif ()

    if (3RDPARTY_DLL_DIR)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${3RDPARTY_DLL_DIR}
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMENT "Copying third-party runtime DLLs"
        )
    endif ()
    find_file(NPCAP_PACKET_DLL
            NAMES Packet.dll
            HINTS
            ${PCAP_ROOT}
            ${PCAP_ROOT}/Redist
            ${PCAP_ROOT}/Bin
            $ENV{SystemRoot}/System32
            $ENV{WINDIR}/System32/Npcap
            PATH_SUFFIXES x64 Win32
    )

    find_file(NPCAP_WPCAP_DLL
            NAMES wpcap.dll
            HINTS
            ${PCAP_ROOT}
            ${PCAP_ROOT}/Redist
            ${PCAP_ROOT}/Bin
            $ENV{SystemRoot}/System32
            $ENV{WINDIR}/System32/Npcap
            PATH_SUFFIXES x64 Win32
    )

    if (NPCAP_PACKET_DLL)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${NPCAP_PACKET_DLL}"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMENT "Copying Packet.dll"
        )
    else ()
        message(WARNING "Packet.dll not found. The application may fail to start unless Npcap runtime is installed and Packet.dll is available in PATH.")
    endif ()

    if (NPCAP_WPCAP_DLL)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${NPCAP_WPCAP_DLL}"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
            COMMENT "Copying wpcap.dll"
        )
    else ()
        message(WARNING "wpcap.dll not found. The application may fail to start unless Npcap runtime is installed and wpcap.dll is available in PATH.")
    endif ()

    if (WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --qmldir "${PROJECT_SOURCE_DIR}/src/ui"
            "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMENT "Running windeployqt for ${PROJECT_NAME}"
        )
    else ()
        message(WARNING "WINDEPLOYQT_EXECUTABLE is not set, skipping Qt deployment.")
    endif ()
endif ()

# Enable testing
enable_testing()
add_subdirectory(tests)
