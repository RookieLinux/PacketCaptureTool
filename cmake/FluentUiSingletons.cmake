# FluentUI's Qt6 module must declare its QML singletons in the generated
# qmldir as well as in the QML source. Keep the correction outside vendored files.
file(GLOB FLUENTUI_SINGLETON_CANDIDATES
    "${CMAKE_CURRENT_SOURCE_DIR}/Qt6/imports/FluentUI/Controls/*.qml")
foreach(FLUENTUI_QML_FILE IN LISTS FLUENTUI_SINGLETON_CANDIDATES)
    file(READ "${FLUENTUI_QML_FILE}" FLUENTUI_QML_PREFIX LIMIT 128)
    if(FLUENTUI_QML_PREFIX MATCHES "pragma Singleton")
        set_source_files_properties("${FLUENTUI_QML_FILE}" PROPERTIES
            QT_QML_SINGLETON_TYPE TRUE)
    endif()
endforeach()
