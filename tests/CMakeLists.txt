# Test configuration
find_package(Qt6 REQUIRED COMPONENTS Test Network Widgets PrintSupport)

# Enable Qt Test
set(CMAKE_AUTOMOC ON)

# Test1
add_executable(PacketCaptureTests
    test_packetcapture.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/PacketCaptureEngine.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/CaptureArchive.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/CaptureController.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/PacketModel.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/FilterEngine.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/ProtocolParser.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/TcpStreamAnalyzer.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/ConfigurationLoader.cpp
)

target_link_libraries(PacketCaptureTests PRIVATE
    Qt6::Test
    Qt6::Core
    Qt6::Network
    Common++
    Packet++
    Pcap++
    ${WPCAP_LIB}
    ${PACKET_LIB}
)
deploy_dependencies(PacketCaptureTests)

# Test2
add_executable(ConfigurationLoaderTests
    test_configurationloader.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/ConfigurationLoader.cpp
)

target_link_libraries(ConfigurationLoaderTests PRIVATE
    Qt6::Test
    Qt6::Core
    Common++
    Packet++
    Pcap++
    ${WPCAP_LIB}
    ${PACKET_LIB}
)
deploy_dependencies(ConfigurationLoaderTests)

# Test3
add_executable(ProtocolValidationTests
    test_protocol_validation.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/ProtocolParser.cpp
)

target_link_libraries(ProtocolValidationTests PRIVATE
    Qt6::Test
    Qt6::Core
)
deploy_dependencies(ProtocolValidationTests)

# Add tests
add_executable(TcpStreamTests test_tcpstreams.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/TcpStreamAnalyzer.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/ProtocolParser.cpp)
target_link_libraries(TcpStreamTests PRIVATE Qt6::Test Qt6::Core Common++ Packet++)
deploy_dependencies(TcpStreamTests)
add_test(NAME TcpStreamTests COMMAND TcpStreamTests)
add_test(NAME PacketCaptureTests COMMAND PacketCaptureTests
    -o "${CMAKE_CURRENT_BINARY_DIR}/packet-test-results.txt,txt")
add_test(NAME ConfigurationLoaderTests COMMAND ConfigurationLoaderTests)
add_test(NAME ProtocolValidationTests COMMAND ProtocolValidationTests)

get_target_property(PACKET_BACKEND_TEST_SOURCES PacketCaptureTests SOURCES)
list(REMOVE_ITEM PACKET_BACKEND_TEST_SOURCES test_packetcapture.cpp)
add_executable(QmlInterfaceTests
    test_qmlinterface.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/LanguageSettings.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/LanguageSettings.h
    ${PROJECT_SOURCE_DIR}/src/backend/SettingsStorage.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/SettingsStorage.h
    ${PROJECT_SOURCE_DIR}/src/backend/DesktopIntegration.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/DesktopIntegration.h
    ${PACKET_BACKEND_TEST_SOURCES}
    ${PROJECT_SOURCE_DIR}/src/ui/qml.qrc
)
qt_add_resources(QmlInterfaceTests test_translations PREFIX "/i18n"
    BASE "${PROJECT_BINARY_DIR}" FILES "${EN_TRANSLATION}")
add_dependencies(QmlInterfaceTests app_translations)
set_target_properties(QmlInterfaceTests PROPERTIES AUTORCC ON)
target_link_libraries(QmlInterfaceTests PRIVATE
    Qt6::Test Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network Qt6::Widgets Qt6::PrintSupport
    Common++ Packet++ Pcap++ ${WPCAP_LIB} ${PACKET_LIB}
)
deploy_dependencies(QmlInterfaceTests)
if(WIN32)
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET QmlInterfaceTests POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations
            --dir "$<TARGET_FILE_DIR:QmlInterfaceTests>"
            "${PROJECT_BINARY_DIR}/_deps/FluentUI-build/src/qml/FluentUI/fluentuiplugin.dll"
        )
    endif()
    add_custom_command(TARGET QmlInterfaceTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE_DIR:Qt6::Gui>/../plugins/platforms/qoffscreen.dll"
        "$<TARGET_FILE_DIR:QmlInterfaceTests>/platforms/qoffscreen.dll"
    )
endif()
add_test(NAME QmlInterfaceTests COMMAND QmlInterfaceTests
    -o "${CMAKE_CURRENT_BINARY_DIR}/qml-test-results.txt,txt")
set_tests_properties(QmlInterfaceTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    TIMEOUT 30
)

foreach(test_target IN ITEMS PacketCaptureTests ConfigurationLoaderTests
        ProtocolValidationTests TcpStreamTests QmlInterfaceTests)
    target_include_directories(${test_target} PRIVATE
        "${PROJECT_SOURCE_DIR}/src/backend")
endforeach()
