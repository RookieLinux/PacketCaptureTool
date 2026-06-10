#include <QtTest/QtTest>
#include "ProtocolParser.h"
#include "DataTypes.h"

class ProtocolValidationTests : public QObject
{
    Q_OBJECT

private slots:
    void testFixedLengthValidation_data()
    {
        QTest::addColumn<quint32>("fixedValue");
        QTest::addColumn<quint32>("packetLen");
        QTest::addColumn<quint32>("headerLen");
        QTest::addColumn<quint32>("fieldLen");
        QTest::addColumn<bool>("expectedValid");

        QTest::newRow("valid match") << 10u << 54u << 44u << 8u << true; // 54-44=10, 8<=10
        QTest::newRow("payload mismatch") << 10u << 60u << 44u << 8u << false; // 60-44=16 != 10
        QTest::newRow("fields too long") << 10u << 54u << 44u << 12u << false; // 12 > 10
    }

    void testFixedLengthValidation()
    {
        QFETCH(quint32, fixedValue);
        QFETCH(quint32, packetLen);
        QFETCH(quint32, headerLen);
        QFETCH(quint32, fieldLen);
        QFETCH(bool, expectedValid);

        ProtocolParser parser;
        ProtocolConfiguration config;
        config.isFixedLength = true;
        config.fixedLength = fixedValue;
        config.transportType = TransportProtocol::UDP;
        config.port = 1234;

        FieldDefinition field;
        field.name = "Field1";
        field.offset = 0;
        field.length = fieldLen;
        field.type = FieldType::ByteArray;
        config.fields.append(field);

        parser.setProtocolConfig(config);

        RawPacketOfTool packet;
        packet.protocol = TransportProtocol::UDP;
        packet.destPort = 1234;
        packet.length = packetLen;
        packet.headerLength = headerLen;
        packet.data.fill(0, packetLen);

        ParsedPacket result = parser.parsePacket(packet);
        QCOMPARE(result.isValid, expectedValid);
    }

    void testVariableLengthValidation_data()
    {
        QTest::addColumn<quint32>("packetLen");
        QTest::addColumn<quint32>("headerLen");
        QTest::addColumn<quint32>("fieldLen");
        QTest::addColumn<bool>("expectedValid");

        QTest::newRow("valid match") << 60u << 44u << 10u << true; // 10+44=54 <= 60
        QTest::newRow("too long") << 50u << 44u << 10u << false; // 10+44=54 > 50
    }

    void testVariableLengthValidation()
    {
        QFETCH(quint32, packetLen);
        QFETCH(quint32, headerLen);
        QFETCH(quint32, fieldLen);
        QFETCH(bool, expectedValid);

        ProtocolParser parser;
        ProtocolConfiguration config;
        config.isFixedLength = false;
        config.transportType = TransportProtocol::UDP;
        config.port = 1234;

        FieldDefinition field;
        field.name = "Field1";
        field.offset = 0;
        field.length = fieldLen;
        field.type = FieldType::ByteArray;
        config.fields.append(field);

        parser.setProtocolConfig(config);

        RawPacketOfTool packet;
        packet.protocol = TransportProtocol::UDP;
        packet.destPort = 1234;
        packet.length = packetLen;
        packet.headerLength = headerLen;
        packet.data.fill(0, packetLen);

        ParsedPacket result = parser.parsePacket(packet);
        QCOMPARE(result.isValid, expectedValid);
    }

    void testSkipHeaderParsing()
    {
        ProtocolParser parser;
        ProtocolConfiguration config;
        config.isFixedLength = true;
        config.fixedLength = 4;
        config.transportType = TransportProtocol::UDP;
        config.port = 1234;

        FieldDefinition field;
        field.name = "PayloadField";
        field.offset = 2; // Offset relative to payload
        field.length = 2;
        field.type = FieldType::UInt16;
        field.endianness = Endianness::BigEndian;
        config.fields.append(field);

        parser.setProtocolConfig(config);

        RawPacketOfTool packet;
        packet.protocol = TransportProtocol::UDP;
        packet.destPort = 1234;
        packet.headerLength = 10;
        packet.length = 14; // Header(10) + Payload(4)
        
        // Data: [10 bytes header] [2 bytes ignore] [0x12 0x34]
        packet.data = QByteArray(10, 'H') + QByteArray::fromHex("00001234");
        
        ParsedPacket result = parser.parsePacket(packet);
        
        QVERIFY(result.isValid);
        QCOMPARE(result.fields.size(), 1);
        QCOMPARE(result.fields[0].value.toUInt(), 0x1234u);
    }
};

QTEST_MAIN(ProtocolValidationTests)
#include "test_protocol_validation.moc"
