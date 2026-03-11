# Test configuration
find_package(Qt6 REQUIRED COMPONENTS Test)

# Enable Qt Test
set(CMAKE_AUTOMOC ON)

# Include backend directory for headers
include_directories(${CMAKE_SOURCE_DIR}/src/backend)

# Test executable for basic tests
add_executable(PacketCaptureTests
    test_packetcapture.cpp
    ${CMAKE_SOURCE_DIR}/src/backend/PacketCaptureEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/backend/CaptureController.cpp
    ${CMAKE_SOURCE_DIR}/src/backend/PacketModel.cpp
    ${CMAKE_SOURCE_DIR}/src/backend/FilterEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/backend/ProtocolParser.cpp
    ${CMAKE_SOURCE_DIR}/src/backend/ConfigurationLoader.cpp
)

target_link_libraries(PacketCaptureTests PRIVATE
    Qt6::Test
    Qt6::Core
    ${PCAPPLUSPLUS_LIBRARIES}
)

target_link_directories(PacketCaptureTests PRIVATE
    ${PCAPPLUSPLUS_LIBRARY_DIRS}
)

target_include_directories(PacketCaptureTests PRIVATE
    ${PCAPPLUSPLUS_INCLUDE_DIRS}
)

# Test executable for ConfigurationLoader
add_executable(ConfigurationLoaderTests
    test_configurationloader.cpp
    ${CMAKE_SOURCE_DIR}/src/backend/ConfigurationLoader.cpp
)

target_link_libraries(ConfigurationLoaderTests PRIVATE
    Qt6::Test
    Qt6::Core
)

# Add tests
add_test(NAME PacketCaptureTests COMMAND PacketCaptureTests)
add_test(NAME ConfigurationLoaderTests COMMAND ConfigurationLoaderTests)
