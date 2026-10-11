#include <QtTest/QtTest>
#include "ProtocolParser.h"
#include "DataTypes.h"

class ProtocolValidationTests : public QObject
{
    Q_OBJECT

private slots:
    void testFloatBitsEnumsAndPhysicalUnits() {
        ProtocolConfiguration c;
        c.protocolName="Extended"; c.transportType=TransportProtocol::UDP; c.port=8080; c.isFixedLength=true; c.fixedLength=5;
        FieldDefinition voltage{"电压",0,4,FieldType::Float32,Endianness::BigEndian};
        voltage.scale=2; voltage.bias=1; voltage.unit="V";
        FieldDefinition flag{"状态",4,1,FieldType::UInt8,Endianness::BigEndian};
        flag.bitOffset=4; flag.bitWidth=2; flag.enumValues.insert("2","就绪");
        c.fields={voltage,flag,FieldDefinition{"原字节",4,1,FieldType::UInt8,Endianness::BigEndian}};
        RawPacketOfTool raw; raw.protocol=TransportProtocol::UDP; raw.destPort=8080; raw.data=QByteArray::fromHex("3fc0000021"); raw.length=5;
        ProtocolParser parser; parser.setProtocolConfig(c);
        auto result=parser.parsePacket(raw);
        QVERIFY(result.isValid);
        QCOMPARE(result.fields[0].value.toDouble(),1.5);
        QCOMPARE(result.fields[0].displayValue,QString("4 V"));
        QCOMPARE(result.fields[1].value.toULongLong(),quint64(2));
        QCOMPARE(result.fields[1].displayValue,QString("就绪（2）"));
        QCOMPARE(result.fields[2].value.toULongLong(),quint64(33));
        raw.data=QByteArray::fromHex("7fc0000021");
        QVERIFY(!parser.parsePacket(raw).isValid);
        c.fixedLength=8; c.fields={{"double",0,8,FieldType::Float64,Endianness::LittleEndian}};
        raw.data=QByteArray::fromHex("00000000000004c0"); raw.length=8;
        parser.setProtocolConfig(c);
        QCOMPARE(parser.parsePacket(raw).fields.first().value.toDouble(),-2.5);
        c.fields={{"allBits",0,8,FieldType::UInt64,Endianness::BigEndian}};
        c.fields[0].bitWidth=64;
        raw.data=QByteArray(8,char(0xff));
        parser.setProtocolConfig(c);
        QCOMPARE(parser.parsePacket(raw).fields.first().displayValue,QString("18446744073709551615"));
    }
    void testSignedValuesAndInvalidRanges()
    {
        ProtocolParser parser;
        ProtocolConfiguration config{};
        config.transportType = TransportProtocol::UDP;
        config.port = 1234;
        config.isFixedLength = true;
        config.fixedLength = 4;
        config.fields.append({"signed", 1, 2, FieldType::Int16, Endianness::BigEndian});
        RawPacketOfTool packet{};
        packet.protocol = TransportProtocol::UDP;
        packet.destPort = 1234;
        packet.data = QByteArray::fromHex("00fffe00");
        packet.length = 4;
        QCOMPARE(parser.parsePacket(packet).status, ParseStatus::Unparsed);
        parser.setProtocolConfig(config);
        auto result = parser.parsePacket(packet);
        QVERIFY(result.isValid);
        QCOMPARE(result.fields.first().displayValue, QString("-2"));
        config.fields[0].offset = 3;
        parser.setProtocolConfig(config);
        result = parser.parsePacket(packet);
        QVERIFY(!result.isValid);
        QCOMPARE(result.status, ParseStatus::Failed);
        packet.destPort = 9999;
        QCOMPARE(parser.parsePacket(packet).status, ParseStatus::Unmatched);
    }

    void testDeclaredVariableLength()
    {
        ProtocolParser parser;
        ProtocolConfiguration config{};
        config.transportType = TransportProtocol::UDP;
        config.port = 1234;
        config.lengthFieldName = "length";
        config.fields.append({"length", 0, 2, FieldType::UInt16, Endianness::BigEndian});
        RawPacketOfTool packet{};
        packet.protocol = TransportProtocol::UDP;
        packet.destPort = 1234;
        packet.data = QByteArray::fromHex("00040000");
        packet.length = 4;
        parser.setProtocolConfig(config);
        QVERIFY(parser.parsePacket(packet).isValid);
        packet.data[1] = 5;
        QVERIFY(!parser.parsePacket(packet).isValid);
    }

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
