#include <QtTest/QtTest>
#include <QTemporaryFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "ConfigurationLoader.h"
#include "DataTypes.h"

class ConfigurationLoaderTest : public QObject
{
    Q_OBJECT

private slots:
    void testLoadValidConfiguration()
    {
        // Create a temporary JSON configuration file
        QTemporaryFile tempFile;
        QVERIFY(tempFile.open());
        
        QString jsonContent = R"({
            "protocolName": "TestProtocol",
            "transportType": "UDP",
            "port": 12345,
            "length": {
                "type": "fixed",
                "fixedValue": 64
            },
            "fields": [
                {
                    "name": "messageType",
                    "offset": 0,
                    "length": 2,
                    "type": "uint16",
                    "endianness": "big"
                },
                {
                    "name": "payload",
                    "offset": 2,
                    "length": 32,
                    "type": "string",
                    "endianness": "big"
                }
            ]
        })";
        
        tempFile.write(jsonContent.toUtf8());
        tempFile.flush();
        
        // Load the configuration
        ConfigurationLoader loader;
        QString errorMsg;
        ProtocolConfiguration config = loader.loadFromFile(tempFile.fileName(), errorMsg);
        
        // Verify the configuration was loaded correctly
        QVERIFY(errorMsg.isEmpty());
        QCOMPARE(config.protocolName, QString("TestProtocol"));
        QCOMPARE(config.transportType, TransportProtocol::UDP);
        QCOMPARE(config.port, quint16(12345));
        QVERIFY(config.isFixedLength);
        QCOMPARE(config.fixedLength, quint32(64));
        QCOMPARE(config.fields.size(), 2);
        
        // Verify first field
        QCOMPARE(config.fields[0].name, QString("messageType"));
        QCOMPARE(config.fields[0].offset, quint32(0));
        QCOMPARE(config.fields[0].length, quint32(2));
        QCOMPARE(config.fields[0].type, FieldType::UInt16);
        QCOMPARE(config.fields[0].endianness, Endianness::BigEndian);
        
        // Verify second field
        QCOMPARE(config.fields[1].name, QString("payload"));
        QCOMPARE(config.fields[1].offset, quint32(2));
        QCOMPARE(config.fields[1].length, quint32(32));
        QCOMPARE(config.fields[1].type, FieldType::String);
    }
    
    void testLoadFileNotFound()
    {
        ConfigurationLoader loader;
        QString errorMsg;
        ProtocolConfiguration config = loader.loadFromFile("/nonexistent/file.json", errorMsg);
        
        QVERIFY(!errorMsg.isEmpty());
        QVERIFY(errorMsg.contains("配置文件不存在"));
    }
    
    void testLoadInvalidJson()
    {
        QTemporaryFile tempFile;
        QVERIFY(tempFile.open());
        
        // Write invalid JSON
        tempFile.write("{ invalid json }");
        tempFile.flush();
        
        ConfigurationLoader loader;
        QString errorMsg;
        ProtocolConfiguration config = loader.loadFromFile(tempFile.fileName(), errorMsg);
        
        QVERIFY(!errorMsg.isEmpty());
        QVERIFY(errorMsg.contains("JSON格式无效"));
    }
    
    void testLoadMissingRequiredField()
    {
        QTemporaryFile tempFile;
        QVERIFY(tempFile.open());
        
        // Missing "port" field
        QString jsonContent = R"({
            "protocolName": "TestProtocol",
            "transportType": "UDP",
            "length": {
                "type": "fixed",
                "fixedValue": 64
            },
            "fields": []
        })";
        
        tempFile.write(jsonContent.toUtf8());
        tempFile.flush();
        
        ConfigurationLoader loader;
        QString errorMsg;
        ProtocolConfiguration config = loader.loadFromFile(tempFile.fileName(), errorMsg);
        
        QVERIFY(!errorMsg.isEmpty());
        QVERIFY(errorMsg.contains("缺少必需字段"));
    }
    
    void testSaveAndLoadRoundTrip()
    {
        // Create a configuration
        ProtocolConfiguration originalConfig;
        originalConfig.protocolName = "RoundTripTest";
        originalConfig.transportType = TransportProtocol::TCP;
        originalConfig.port = 8080;
        originalConfig.isFixedLength = false;
        originalConfig.fixedLength = 0; // Initialize to 0 when not used
        originalConfig.lengthFieldName = "length";
        originalConfig.lengthFieldOffset = 4;
        
        FieldDefinition field1;
        field1.name = "header";
        field1.offset = 0;
        field1.length = 4;
        field1.type = FieldType::UInt32;
        field1.endianness = Endianness::LittleEndian;
        originalConfig.fields.append(field1);
        
        // Save to file
        QTemporaryFile tempFile;
        QVERIFY(tempFile.open());
        tempFile.close(); // Close to allow ConfigurationLoader to write
        
        ConfigurationLoader loader;
        QString errorMsg;
        bool saved = loader.saveToFile(originalConfig, tempFile.fileName(), errorMsg);
        
        QVERIFY2(saved, errorMsg.toUtf8().constData());
        QVERIFY(errorMsg.isEmpty());
        
        // Load back
        ProtocolConfiguration loadedConfig = loader.loadFromFile(tempFile.fileName(), errorMsg);
        
        QVERIFY(errorMsg.isEmpty());
        
        // Debug output
        if (loadedConfig != originalConfig) {
            qDebug() << "Original protocolName:" << originalConfig.protocolName;
            qDebug() << "Loaded protocolName:" << loadedConfig.protocolName;
            qDebug() << "Original transportType:" << (int)originalConfig.transportType;
            qDebug() << "Loaded transportType:" << (int)loadedConfig.transportType;
            qDebug() << "Original port:" << originalConfig.port;
            qDebug() << "Loaded port:" << loadedConfig.port;
            qDebug() << "Original isFixedLength:" << originalConfig.isFixedLength;
            qDebug() << "Loaded isFixedLength:" << loadedConfig.isFixedLength;
            qDebug() << "Original fixedLength:" << originalConfig.fixedLength;
            qDebug() << "Loaded fixedLength:" << loadedConfig.fixedLength;
            qDebug() << "Original lengthFieldName:" << originalConfig.lengthFieldName;
            qDebug() << "Loaded lengthFieldName:" << loadedConfig.lengthFieldName;
            qDebug() << "Original lengthFieldOffset:" << originalConfig.lengthFieldOffset;
            qDebug() << "Loaded lengthFieldOffset:" << loadedConfig.lengthFieldOffset;
            qDebug() << "Original fields count:" << originalConfig.fields.size();
            qDebug() << "Loaded fields count:" << loadedConfig.fields.size();
            
            if (originalConfig.fields.size() == loadedConfig.fields.size()) {
                for (int i = 0; i < originalConfig.fields.size(); i++) {
                    qDebug() << "Field" << i << ":";
                    qDebug() << "  Original name:" << originalConfig.fields[i].name;
                    qDebug() << "  Loaded name:" << loadedConfig.fields[i].name;
                    qDebug() << "  Original offset:" << originalConfig.fields[i].offset;
                    qDebug() << "  Loaded offset:" << loadedConfig.fields[i].offset;
                    qDebug() << "  Original length:" << originalConfig.fields[i].length;
                    qDebug() << "  Loaded length:" << loadedConfig.fields[i].length;
                    qDebug() << "  Original type:" << (int)originalConfig.fields[i].type;
                    qDebug() << "  Loaded type:" << (int)loadedConfig.fields[i].type;
                    qDebug() << "  Original endianness:" << (int)originalConfig.fields[i].endianness;
                    qDebug() << "  Loaded endianness:" << (int)loadedConfig.fields[i].endianness;
                    qDebug() << "  Fields equal:" << (originalConfig.fields[i] == loadedConfig.fields[i]);
                }
            }
        }
        
        QCOMPARE(loadedConfig, originalConfig);
    }
    
    void testValidateConfigInvalidFieldLength()
    {
        ProtocolConfiguration config;
        config.protocolName = "TestProtocol";
        config.transportType = TransportProtocol::UDP;
        config.port = 12345;
        config.isFixedLength = true;
        config.fixedLength = 64;
        
        // Add a field with invalid length for uint16 (should be 2, not 4)
        FieldDefinition field;
        field.name = "invalidField";
        field.offset = 0;
        field.length = 4; // Wrong length for uint16
        field.type = FieldType::UInt16;
        field.endianness = Endianness::BigEndian;
        config.fields.append(field);
        
        ConfigurationLoader loader;
        QString errorMsg;
        bool valid = loader.validateConfig(config, errorMsg);
        
        QVERIFY(!valid);
        QVERIFY(!errorMsg.isEmpty());
        QVERIFY(errorMsg.contains("长度必须为2"));
    }
    
    void testValidateConfigZeroFieldLength()
    {
        ProtocolConfiguration config;
        config.protocolName = "TestProtocol";
        config.transportType = TransportProtocol::UDP;
        config.port = 12345;
        config.isFixedLength = true;
        config.fixedLength = 64;
        
        // Add a field with zero length
        FieldDefinition field;
        field.name = "zeroLengthField";
        field.offset = 0;
        field.length = 0; // Invalid
        field.type = FieldType::String;
        field.endianness = Endianness::BigEndian;
        config.fields.append(field);
        
        ConfigurationLoader loader;
        QString errorMsg;
        bool valid = loader.validateConfig(config, errorMsg);
        
        QVERIFY(!valid);
        QVERIFY(!errorMsg.isEmpty());
        QVERIFY(errorMsg.contains("长度不能为0"));
    }
};

QTEST_MAIN(ConfigurationLoaderTest)
#include "test_configurationloader.moc"
