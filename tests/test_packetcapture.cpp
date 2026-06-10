#include <QtTest/QtTest>
#include "PacketCaptureEngine.h"
#include "CaptureController.h"
#include "PacketModel.h"
#include "ProtocolParser.h"
#include <QSignalSpy>

class PacketCaptureTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Setup code
    }

    void testGetInterfaces()
    {
        PacketCaptureEngine engine;
        QList<NetworkInterface> interfaces = engine.getAvailableInterfaces();
        // 至少应该能发现回环接口或物理接口，但在某些CI环境可能为空
        qDebug() << "Found" << interfaces.size() << "interfaces";
        for (const auto& iface : interfaces) {
            QVERIFY(!iface.name.isEmpty());
        }
    }

    void testCaptureEngineState()
    {
        PacketCaptureEngine engine;
        QVERIFY(!engine.isCapturing());
        
        QString errorMsg;
        // 尝试开启一个不存在的网卡，应该返回false
        bool started = engine.startCapture("non_existent_interface", errorMsg);
        QVERIFY(!started);
        QVERIFY(!errorMsg.isEmpty());
        QVERIFY(!engine.isCapturing());
        
        engine.stopCapture();
        QVERIFY(!engine.isCapturing());
    }

    void testControllerInitialization()
    {
        CaptureController controller;
        QVERIFY(!controller.isCapturing());
        QCOMPARE(controller.totalPackets(), 0);
        QCOMPARE(controller.udpPackets(), 0);
        QCOMPARE(controller.tcpPackets(), 0);
        QCOMPARE(controller.matchedPackets(), 0);
        QVERIFY(controller.getPacketModel() != nullptr);
    }

    void testControllerInterfaceList()
    {
        CaptureController controller;
        QVariantList interfaces = controller.getInterfaces();
        qDebug() << "Controller found" << interfaces.size() << "interfaces";
        // 验证返回的是QVariantMap列表
        for (const auto& var : interfaces) {
            QVariantMap map = var.toMap();
            QVERIFY(map.contains("name"));
            QVERIFY(map.contains("description"));
            QVERIFY(map.contains("ipAddress"));
        }
    }

    void testProtocolParser()
    {
        ProtocolParser parser;
        ProtocolConfiguration config;
        config.protocolName = "TestProtocol";
        config.transportType = TransportProtocol::UDP;
        config.port = 1234;
        config.isFixedLength = true;
        config.fixedLength = 4;

        FieldDefinition field;
        field.name = "TestField";
        field.offset = 0;
        field.length = 4;
        field.type = FieldType::UInt32;
        field.endianness = Endianness::BigEndian;
        config.fields.append(field);

        parser.setProtocolConfig(config);

        RawPacketOfTool packet;
        packet.protocol = TransportProtocol::UDP;
        packet.destPort = 1234;
        // 模拟4字节数据 0x00000001 (BigEndian)
        packet.data = QByteArray::fromHex("00000001");
        packet.length = packet.data.length();
        packet.headerLength = 0;
        
        ParsedPacket parsed = parser.parsePacket(packet);
        QVERIFY(parsed.isValid);
        QCOMPARE(parsed.fields.size(), 1);
        QCOMPARE(parsed.fields[0].name, QString("TestField"));
        QCOMPARE(parsed.fields[0].value.toUInt(), 1u);
    }

    void testPacketModelUsesDisplayedRawDataLength()
    {
        PacketModel model;

        RawPacketOfTool packet;
        packet.data = QByteArray::fromHex("00ff8041");
        packet.length = 99;
        packet.timestamp = 123456;
        packet.sourceIP = "192.0.2.1";
        packet.destIP = "192.0.2.2";
        packet.sourcePort = 1000;
        packet.destPort = 2000;
        packet.protocol = TransportProtocol::UDP;

        ParsedPacket parsed;
        parsed.isValid = false;
        parsed.errorMessage = "No protocol configuration loaded";
        parsed.rawData = packet.data;

        model.addPacket(packet, parsed);

        const QModelIndex packetIndex = model.index(0, 0);
        QCOMPARE(model.data(packetIndex, PacketModel::LengthRole).toInt(), packet.data.length());
        QCOMPARE(model.data(packetIndex, PacketModel::RawDataRole).toString(), QString("00ff8041"));

        const QVariantMap details = model.getPacketDetails(0);
        QCOMPARE(details["length"].toInt(), packet.data.length());
        QCOMPARE(details["rawData"].toString(), QString("00ff8041"));
    }

    void testPacketModelReparsesExistingPacketsAfterConfigLoad()
    {
        PacketModel model;
        ProtocolParser parser;

        RawPacketOfTool packet;
        packet.data = QByteArray::fromHex("01020304");
        packet.length = packet.data.length();
        packet.headerLength = 0;
        packet.timestamp = 123456;
        packet.sourceIP = "192.0.2.1";
        packet.destIP = "192.0.2.2";
        packet.sourcePort = 53913;
        packet.destPort = 443;
        packet.protocol = TransportProtocol::UDP;

        ParsedPacket parsedWithoutConfig = parser.parsePacket(packet);
        model.addPacket(packet, parsedWithoutConfig);

        QVariantMap details = model.getPacketDetails(0);
        QVERIFY(!details["isValid"].toBool());
        QCOMPARE(details["errorMessage"].toString(), QString("No protocol configuration loaded"));

        ProtocolConfiguration config;
        config.protocolName = "TestProtocol";
        config.transportType = TransportProtocol::UDP;
        config.port = 53913;
        config.isFixedLength = true;
        config.fixedLength = 4;
        parser.setProtocolConfig(config);

        model.reparsePackets(&parser);

        details = model.getPacketDetails(0);
        QVERIFY(details["isValid"].toBool());
        QCOMPARE(details["errorMessage"].toString(), QString(""));
    }

    void cleanupTestCase()
    {
        // Cleanup code
    }
};

QTEST_MAIN(PacketCaptureTests)
#include "test_packetcapture.moc"
