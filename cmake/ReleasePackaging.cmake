# Usage: cmake --build <build-dir> --config Release --target package_release
if(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    return()
endif()

get_target_property(RELEASE_QMAKE Qt6::qmake IMPORTED_LOCATION)
get_filename_component(RELEASE_QT_BIN "${RELEASE_QMAKE}" DIRECTORY)
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt
        HINTS "${RELEASE_QT_BIN}" DOC "windeployqt from the Qt kit used to build this application")
    set(RELEASE_DEPLOY_TOOL "${WINDEPLOYQT_EXECUTABLE}")
else()
    find_program(LINUXDEPLOYQT_EXECUTABLE NAMES linuxdeployqt linuxdeployqt.AppImage
        DOC "Path to linuxdeployqt (executable or AppImage)")
    set(RELEASE_DEPLOY_TOOL "${LINUXDEPLOYQT_EXECUTABLE}")
endif()

set(RELEASE_SOURCE_DIR "${PROJECT_SOURCE_DIR}")
set(RELEASE_BINARY_DIR "${PROJECT_BINARY_DIR}")
set(RELEASE_VERSION "${PROJECT_VERSION}")
set(RELEASE_APP_TARGET "${PROJECT_NAME}")
set(RELEASE_SYSTEM "${CMAKE_SYSTEM_NAME}")
set(RELEASE_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
set(RELEASE_POINTER_SIZE "${CMAKE_SIZEOF_VOID_P}")
set(RELEASE_MINGW "${MINGW}")
set(RELEASE_MSVC "${MSVC}")
get_target_property(RELEASE_FLUENT_QML_DIR fluentuiplugin QT_QML_MODULE_OUTPUT_DIRECTORY)
if(NOT RELEASE_FLUENT_QML_DIR)
    set(RELEASE_FLUENT_QML_DIR "${PROJECT_BINARY_DIR}/_deps/FluentUI-build/src/qml/FluentUI")
endif()
configure_file("${CMAKE_CURRENT_LIST_DIR}/PackageRelease.cmake.in"
    "${PROJECT_BINARY_DIR}/PackageRelease.cmake" @ONLY)

# Resolve paths without TARGET_FILE expressions in the custom target command:
# those expressions would implicitly build the application before TS updates.
file(GENERATE OUTPUT "${PROJECT_BINARY_DIR}/ReleaseInputs-$<CONFIG>.cmake"
    CONTENT "set(RELEASE_APP \"$<TARGET_FILE:${PROJECT_NAME}>\")\nset(RELEASE_PLUGIN \"$<TARGET_FILE:fluentuiplugin>\")\n")

add_custom_target(package_release
    COMMAND "${CMAKE_COMMAND}"
        "-DRELEASE_CONFIG=$<CONFIG>"
        -P "${PROJECT_BINARY_DIR}/PackageRelease.cmake"
    # The script updates TS first, then builds the application. Parallel
    # DEPENDS here would allow the executable to embed the previous QM file.
    COMMENT "Deploying and packaging PacketCaptureTool Release"
    VERBATIM USES_TERMINAL)
