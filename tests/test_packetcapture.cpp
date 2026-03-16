#include <QtTest/QtTest>
#include "PacketCaptureEngine.h"
#include "CaptureController.h"
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
        
        ParsedPacket parsed = parser.parsePacket(packet);
        QVERIFY(parsed.isValid);
        QCOMPARE(parsed.fields.size(), 1);
        QCOMPARE(parsed.fields[0].name, QString("TestField"));
        QCOMPARE(parsed.fields[0].value.toUInt(), 1u);
    }

    void cleanupTestCase()
    {
        // Cleanup code
    }
};

QTEST_MAIN(PacketCaptureTests)
#include "test_packetcapture.moc"
