#include <QtTest/QtTest>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include "ConfigurationLoader.h"
#include "DataTypes.h"

class ConfigurationLoaderTest : public QObject
{
    Q_OBJECT

private slots:
    void testExtendedFieldsRoundTripAndValidation() {
        const QString source=R"({"protocolName":"Extended","transportType":"TCP","port":8080,"length":{"type":"fixed","fixedValue":9},"fields":[{"name":"temperature","offset":0,"length":8,"type":"float64","endianness":"little","scale":0.5,"bias":-10,"unit":"C"},{"name":"state","offset":8,"length":1,"type":"uint8","endianness":"big","bitOffset":2,"bitWidth":3,"enumValues":{"2":"Ready"}}]})";
        ConfigurationLoader loader; QString error;
        auto config=loader.loadFromJson(source,error);
        QVERIFY2(error.isEmpty(),qPrintable(error));
        QCOMPARE(config.fields[0].type,FieldType::Float64);
        auto round=loader.loadFromJson(QString::fromUtf8(QJsonDocument(loader.toJson(config)).toJson()),error);
        QVERIFY(error.isEmpty()); QVERIFY(round == config);
        config.fields[1].bitOffset=7;
        QVERIFY(!loader.validateConfig(config,error));
        config.fields[1].bitOffset=2; config.fields[1].enumValues.insert("8","outside");
        QVERIFY(!loader.validateConfig(config,error));
        config.fields[1].enumValues.remove("8"); config.fields[0].length=4;
        QVERIFY(!loader.validateConfig(config,error));
        auto json=QJsonDocument::fromJson(source.toUtf8()).object();
        auto fields=json["fields"].toArray(); auto field=fields[0].toObject(); field["scale"]="bad"; fields[0]=field; json["fields"]=fields;
        loader.loadFromJson(QString::fromUtf8(QJsonDocument(json).toJson()),error);
        QVERIFY(!error.isEmpty());
    }
    void testRejectMalformedNumericConfiguration_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("port overflow") << QString("port") << QJsonValue(65536);
        QTest::newRow("negative port") << QString("port") << QJsonValue(-1);
        QTest::newRow("text port") << QString("port") << QJsonValue("1234");
        QTest::newRow("negative payload length") << QString("fixedValue") << QJsonValue(-1);
        QTest::newRow("fractional offset") << QString("offset") << QJsonValue(0.5);
        QTest::newRow("missing offset") << QString("offset") << QJsonValue(QJsonValue::Undefined);
        QTest::newRow("field outside payload") << QString("offset") << QJsonValue(100);
    }

    void testRejectMalformedNumericConfiguration()
    {
        QFETCH(QString, key);
        QFETCH(QJsonValue, value);
        auto object = QJsonDocument::fromJson(R"({"protocolName":"Test","transportType":"UDP","port":1234,
            "length":{"type":"fixed","fixedValue":4},
            "fields":[{"name":"value","offset":0,"length":2,"type":"uint16","endianness":"big"}]})").object();
        if (key == "port") object[key] = value;
        else if (key == "fixedValue") {
            auto length = object["length"].toObject();
            length[key] = value;
            object["length"] = length;
        } else {
            auto fields = object["fields"].toArray();
            auto field = fields[0].toObject();
            field[key] = value;
            fields[0] = field;
            object["fields"] = fields;
        }
        QTemporaryFile file;
        QVERIFY(file.open());
        file.write(QJsonDocument(object).toJson());
        file.flush();
        ConfigurationLoader loader;
        QString error;
        loader.loadFromFile(file.fileName(), error);
        QVERIFY(!error.isEmpty());
    }

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

    void testLoadFileUrlPath()
    {
        QTemporaryFile tempFile;
        QVERIFY(tempFile.open());

        QString jsonContent = R"({
            "protocolName": "UrlProtocol",
            "transportType": "UDP",
            "port": 9000,
            "length": {
                "type": "fixed",
                "fixedValue": 8
            },
            "fields": []
        })";

        tempFile.write(jsonContent.toUtf8());
        tempFile.flush();

        ConfigurationLoader loader;
        QString errorMsg;
        const QString fileUrl = QUrl::fromLocalFile(tempFile.fileName()).toString();
        ProtocolConfiguration config = loader.loadFromFile(fileUrl, errorMsg);

        QVERIFY2(errorMsg.isEmpty(), errorMsg.toUtf8().constData());
        QCOMPARE(config.protocolName, QString("UrlProtocol"));
    }

    void testLoadDirectoryPathShouldReportOpenError()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ConfigurationLoader loader;
        QString errorMsg;
        ProtocolConfiguration config = loader.loadFromFile(tempDir.path(), errorMsg);

        QVERIFY(config.protocolName.isEmpty());
        QVERIFY(!errorMsg.isEmpty());
        QVERIFY(errorMsg.contains("无法打开配置文件"));
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
        originalConfig.fields.append({"length", 4, 2, FieldType::UInt16, Endianness::LittleEndian});
        
        // Save to file
        QTemporaryDir tempDirectory;
        QVERIFY(tempDirectory.isValid());
        const QString savedPath=tempDirectory.filePath("round-trip.json");
        
        ConfigurationLoader loader;
        QString errorMsg;
        bool saved = loader.saveToFile(originalConfig, savedPath, errorMsg);
        
        QVERIFY2(saved, errorMsg.toUtf8().constData());
        QVERIFY(errorMsg.isEmpty());
        
        // Load back
        ProtocolConfiguration loadedConfig = loader.loadFromFile(savedPath, errorMsg);
        
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
