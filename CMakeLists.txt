cmake_minimum_required(VERSION 3.16)

project(PacketCaptureTool VERSION 2.0.0 LANGUAGES CXX)

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
set(FLUENTUI_QML_PLUGIN_DIRECTORY "qml/FluentUI" CACHE PATH "Path to FluentUI plugin" FORCE)

configure_file(
    cmake/build_info.h.in
    ${PROJECT_SOURCE_DIR}/src/build_info.h
)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 QuickDialogs2 LinguistTools) #Network
include(ThirdParty)

set(BACKEND_SOURCES
    src/backend/LanguageSettings.cpp
    src/backend/SettingsStorage.cpp
    src/backend/DesktopIntegration.cpp
    src/backend/PacketCaptureEngine.cpp
    src/backend/CaptureArchive.cpp
    src/backend/FilterEngine.cpp
    src/backend/ProtocolParser.cpp
    src/backend/TcpStreamAnalyzer.cpp
    src/backend/ConfigurationLoader.cpp
    src/backend/CaptureController.cpp
    src/backend/PacketModel.cpp
)

set(BACKEND_HEADERS
    src/backend/LanguageSettings.h
    src/backend/SettingsStorage.h
    src/backend/DesktopIntegration.h
    src/backend/PacketCaptureEngine.h
    src/backend/FilterEngine.h
    src/backend/ProtocolParser.h
    src/backend/ConfigurationLoader.h
    src/backend/CaptureController.h
    src/backend/PacketModel.h
)

set(QML_RESOURCES
    src/ui/qml.qrc
)

add_executable(${PROJECT_NAME}
    src/main.cpp
    ${BACKEND_SOURCES}
    ${BACKEND_HEADERS}
    ${QML_RESOURCES}
)

# Dependency headers are supplied by the linked targets' usage requirements.
target_include_directories(${PROJECT_NAME} PRIVATE
    "${PROJECT_SOURCE_DIR}/src/backend"
    "${PROJECT_SOURCE_DIR}/src/ui"
)

if(WIN32)
    enable_language(RC)
    target_sources(${PROJECT_NAME} PRIVATE src/resources/app.rc)
    target_include_directories(${PROJECT_NAME} PRIVATE src/resources)
endif()

set(APP_TRANSLATION_SOURCE "${PROJECT_SOURCE_DIR}/src/i18n/app_en.ts")
# Explicitly invoked by developers and release packaging, not ordinary builds.
# Do not declare the version-controlled TS file as a build byproduct: clean
# must not remove it. Scan application sources only (including C++ and QML).
add_custom_target(update_app_translations
    COMMAND Qt6::lupdate "${PROJECT_SOURCE_DIR}/src"
        -extensions cpp,h,qml,js,ui
        -locations none -source-language zh_CN -target-language en_US
        -ts "${APP_TRANSLATION_SOURCE}"
    COMMENT "Updating application TS translations"
    VERBATIM)

set(EN_TRANSLATION "${CMAKE_CURRENT_BINARY_DIR}/app_en.qm")
add_custom_command(OUTPUT "${EN_TRANSLATION}"
    COMMAND Qt6::lrelease "${APP_TRANSLATION_SOURCE}" -qm "${EN_TRANSLATION}"
    DEPENDS "${APP_TRANSLATION_SOURCE}"
    VERBATIM)
add_custom_target(app_translations DEPENDS "${EN_TRANSLATION}")
qt_add_resources(${PROJECT_NAME} app_translations_resources PREFIX "/i18n"
    BASE "${CMAKE_CURRENT_BINARY_DIR}" FILES "${EN_TRANSLATION}")
add_dependencies(${PROJECT_NAME} app_translations)

set_target_properties(${PROJECT_NAME} PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)

if(WIN32)
    # Release runs as a desktop GUI; Debug retains a console for diagnostics.
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE "$<CONFIG:Release>")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
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

# Function to deploy dependencies to a target
function(deploy_dependencies target_name)
    if (WIN32)
        set(3RDPARTY_DLL_DIR)

        # 拷贝第三方库到可执行文件同目录
        if (MINGW)
            file(GLOB_RECURSE 3RDPARTY_DLL_DIR ${PROJECT_SOURCE_DIR}/3rdparty/mingw/*.dll)
        elseif (MSVC)
            if (CMAKE_SIZEOF_VOID_P EQUAL 4)
                file(GLOB_RECURSE 3RDPARTY_DLL_DIR ${PROJECT_SOURCE_DIR}/3rdparty/msvc/x86/*.dll)
            elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
                file(GLOB_RECURSE 3RDPARTY_DLL_DIR ${PROJECT_SOURCE_DIR}/3rdparty/msvc/x64/*.dll)
            endif ()
        endif ()

        if (3RDPARTY_DLL_DIR)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${3RDPARTY_DLL_DIR}
                $<TARGET_FILE_DIR:${target_name}>
                COMMENT "Copying third-party DLLs for ${target_name}"
            )
        endif ()

        if (Qt6_FOUND AND WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --compiler-runtime
                --qmldir "${PROJECT_SOURCE_DIR}/src/ui"
                --qmlimport "${PROJECT_BINARY_DIR}/_deps/FluentUI-build/src/qml"
                "$<TARGET_FILE:${target_name}>"
                COMMENT "Running windeployqt for ${target_name}"
            )
        endif ()
    endif ()
endfunction()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Use function to deploy dependencies
if (WIN32)
    deploy_dependencies(${PROJECT_NAME})

    # Manual deployment of FluentUI module to the executable directory
    # This ensures that the module is available for loading at runtime in a standard layout
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/qml/FluentUI"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_BINARY_DIR}/_deps/FluentUI-build/src/qml/FluentUI"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/qml/FluentUI"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_BINARY_DIR}/_deps/FluentUI-build/src/qml/FluentUI/fluentuiplugin.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Deploying FluentUI QML module"
    )

    if (WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --no-translations
            --compiler-runtime
            --dir "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/qml/FluentUI/fluentuiplugin.dll"
            COMMENT "Deploying FluentUI plugin dependencies"
        )
    endif ()
endif ()

# Explicit release packaging; ordinary builds keep their existing deployment behavior.
include(ReleasePackaging)

# Enable testing
enable_testing()
add_subdirectory(tests)
