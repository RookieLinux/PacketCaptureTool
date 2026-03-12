# Test configuration
find_package(Qt6 REQUIRED COMPONENTS Test)

# Enable Qt Test
set(CMAKE_AUTOMOC ON)

include_directories(SYSTEM BEFORE
        ${PcapPlusPlus_SOURCE_DIR}/Common++/header
        ${PcapPlusPlus_SOURCE_DIR}/Packet++/header
        ${PcapPlusPlus_SOURCE_DIR}/Pcap++/header
)
include_directories(${PROJECT_SOURCE_DIR}/src/backend)

# Test1
add_executable(PacketCaptureTests
    test_packetcapture.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/PacketCaptureEngine.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/CaptureController.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/PacketModel.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/FilterEngine.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/ProtocolParser.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/ConfigurationLoader.cpp
)

target_link_libraries(PacketCaptureTests PRIVATE
    Qt6::Test
    Qt6::Core
    Qt6::Network
    Common++
    Packet++
    Pcap++
)

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
)

# Add tests
add_test(NAME PacketCaptureTests COMMAND PacketCaptureTests)
add_test(NAME ConfigurationLoaderTests COMMAND ConfigurationLoaderTests)
