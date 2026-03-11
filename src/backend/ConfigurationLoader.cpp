#include "ConfigurationLoader.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>

ConfigurationLoader::ConfigurationLoader(QObject *parent)
    : QObject(parent)
{
}

ProtocolConfiguration ConfigurationLoader::loadFromFile(const QString& filePath, QString& errorMsg)
{
    ProtocolConfiguration config;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMsg = QString("配置文件不存在：%1").arg(filePath);
        return config;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        errorMsg = QString("配置文件JSON格式无效：%1").arg(parseError.errorString());
        return config;
    }
    
    if (!doc.isObject()) {
        errorMsg = "配置文件根元素必须是对象";
        return config;
    }
    
    return parseJson(doc.object(), errorMsg);
}

bool ConfigurationLoader::saveToFile(const ProtocolConfiguration& config, const QString& filePath, QString& errorMsg)
{
    // 验证配置
    if (!validateConfig(config, errorMsg)) {
        return false;
    }
    
    // 转换为JSON对象
    QJsonObject jsonObj = toJson(config);
    
    // 创建JSON文档
    QJsonDocument doc(jsonObj);
    
    // 打开文件进行写入
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        errorMsg = QString("无法打开文件进行写入：%1").arg(filePath);
        return false;
    }
    
    // 写入JSON数据（使用缩进格式化）
    qint64 bytesWritten = file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    if (bytesWritten == -1) {
        errorMsg = QString("写入文件失败：%1").arg(filePath);
        return false;
    }
    
    return true;
}

bool ConfigurationLoader::validateConfig(const ProtocolConfiguration& config, QString& errorMsg)
{
    // 验证协议名称
    if (config.protocolName.isEmpty()) {
        errorMsg = "协议名称不能为空";
        return false;
    }
    
    // 验证端口号（虽然quint16自动限制范围，但我们检查是否为0）
    if (config.port == 0) {
        errorMsg = "端口号不能为0";
        return false;
    }
    
    // 验证固定长度配置
    if (config.isFixedLength && config.fixedLength == 0) {
        errorMsg = "固定长度不能为0";
        return false;
    }
    
    // 验证可变长度配置
    if (!config.isFixedLength) {
        if (config.lengthFieldName.isEmpty()) {
            errorMsg = "可变长度配置缺少长度字段名称";
            return false;
        }
    }
    
    // 验证字段定义
    for (const FieldDefinition& field : config.fields) {
        // 验证字段名称
        if (field.name.isEmpty()) {
            errorMsg = "字段名称不能为空";
            return false;
        }
        
        // 验证字段长度
        if (field.length == 0) {
            errorMsg = QString("字段定义无效：%1 - 字段长度不能为0").arg(field.name);
            return false;
        }
        
        // 验证字段偏移量和长度的合理性（不能超过合理范围，例如64KB）
        const quint32 MAX_REASONABLE_OFFSET = 65536;
        const quint32 MAX_REASONABLE_LENGTH = 65536;
        
        if (field.offset > MAX_REASONABLE_OFFSET) {
            errorMsg = QString("字段定义无效：%1 - 偏移量超出合理范围").arg(field.name);
            return false;
        }
        
        if (field.length > MAX_REASONABLE_LENGTH) {
            errorMsg = QString("字段定义无效：%1 - 长度超出合理范围").arg(field.name);
            return false;
        }
        
        // 验证字段类型与长度的匹配
        switch (field.type) {
            case FieldType::UInt8:
            case FieldType::Int8:
                if (field.length != 1) {
                    errorMsg = QString("字段定义无效：%1 - uint8/int8类型长度必须为1").arg(field.name);
                    return false;
                }
                break;
            case FieldType::UInt16:
            case FieldType::Int16:
                if (field.length != 2) {
                    errorMsg = QString("字段定义无效：%1 - uint16/int16类型长度必须为2").arg(field.name);
                    return false;
                }
                break;
            case FieldType::UInt32:
            case FieldType::Int32:
                if (field.length != 4) {
                    errorMsg = QString("字段定义无效：%1 - uint32/int32类型长度必须为4").arg(field.name);
                    return false;
                }
                break;
            case FieldType::UInt64:
            case FieldType::Int64:
                if (field.length != 8) {
                    errorMsg = QString("字段定义无效：%1 - uint64/int64类型长度必须为8").arg(field.name);
                    return false;
                }
                break;
            case FieldType::String:
            case FieldType::ByteArray:
                // 字符串和字节数组可以是任意长度
                break;
        }
    }
    
    return true;
}

ProtocolConfiguration ConfigurationLoader::parseJson(const QJsonObject& json, QString& errorMsg)
{
    ProtocolConfiguration config;
    
    // 验证必需字段：protocolName
    if (!json.contains("protocolName")) {
        errorMsg = "配置文件缺少必需字段：protocolName";
        return config;
    }
    config.protocolName = json["protocolName"].toString();
    
    // 验证必需字段：transportType
    if (!json.contains("transportType")) {
        errorMsg = "配置文件缺少必需字段：transportType";
        return config;
    }
    QString transportStr = json["transportType"].toString().toUpper();
    if (transportStr == "UDP") {
        config.transportType = TransportProtocol::UDP;
    } else if (transportStr == "TCP") {
        config.transportType = TransportProtocol::TCP;
    } else {
        errorMsg = QString("无效的传输层协议类型：%1").arg(transportStr);
        return config;
    }
    
    // 验证必需字段：port
    if (!json.contains("port")) {
        errorMsg = "配置文件缺少必需字段：port";
        return config;
    }
    config.port = static_cast<quint16>(json["port"].toInt());
    
    // 解析长度配置
    if (!json.contains("length")) {
        errorMsg = "配置文件缺少必需字段：length";
        return config;
    }
    QJsonObject lengthObj = json["length"].toObject();
    QString lengthType = lengthObj["type"].toString();
    
    if (lengthType == "fixed") {
        config.isFixedLength = true;
        config.fixedLength = static_cast<quint32>(lengthObj["fixedValue"].toInt());
        config.lengthFieldName = QString(); // Clear variable length fields
        config.lengthFieldOffset = 0;
    } else if (lengthType == "variable") {
        config.isFixedLength = false;
        config.fixedLength = 0; // Initialize to 0 when not used
        config.lengthFieldName = lengthObj["fieldName"].toString();
        config.lengthFieldOffset = static_cast<quint32>(lengthObj["fieldOffset"].toInt());
    } else {
        errorMsg = QString("无效的长度类型：%1").arg(lengthType);
        return config;
    }
    
    // 解析字段数组
    if (!json.contains("fields")) {
        errorMsg = "配置文件缺少必需字段：fields";
        return config;
    }
    
    QJsonArray fieldsArray = json["fields"].toArray();
    for (const QJsonValue& fieldValue : fieldsArray) {
        QJsonObject fieldObj = fieldValue.toObject();
        FieldDefinition field;
        
        field.name = fieldObj["name"].toString();
        field.offset = static_cast<quint32>(fieldObj["offset"].toInt());
        field.length = static_cast<quint32>(fieldObj["length"].toInt());
        
        // 解析字段类型
        QString typeStr = fieldObj["type"].toString().toLower();
        if (typeStr == "uint8") {
            field.type = FieldType::UInt8;
        } else if (typeStr == "uint16") {
            field.type = FieldType::UInt16;
        } else if (typeStr == "uint32") {
            field.type = FieldType::UInt32;
        } else if (typeStr == "uint64") {
            field.type = FieldType::UInt64;
        } else if (typeStr == "int8") {
            field.type = FieldType::Int8;
        } else if (typeStr == "int16") {
            field.type = FieldType::Int16;
        } else if (typeStr == "int32") {
            field.type = FieldType::Int32;
        } else if (typeStr == "int64") {
            field.type = FieldType::Int64;
        } else if (typeStr == "string") {
            field.type = FieldType::String;
        } else if (typeStr == "bytearray") {
            field.type = FieldType::ByteArray;
        } else {
            errorMsg = QString("字段 %1 的类型无效：%2").arg(field.name, typeStr);
            return config;
        }
        
        // 解析字节序
        QString endiannessStr = fieldObj["endianness"].toString().toLower();
        if (endiannessStr == "big") {
            field.endianness = Endianness::BigEndian;
        } else if (endiannessStr == "little") {
            field.endianness = Endianness::LittleEndian;
        } else {
            errorMsg = QString("字段 %1 的字节序无效：%2").arg(field.name, endiannessStr);
            return config;
        }
        
        config.fields.append(field);
    }
    
    return config;
}

QJsonObject ConfigurationLoader::toJson(const ProtocolConfiguration& config)
{
    QJsonObject json;
    
    // 基本字段
    json["protocolName"] = config.protocolName;
    
    // 传输层协议类型
    QString transportStr;
    switch (config.transportType) {
        case TransportProtocol::UDP:
            transportStr = "UDP";
            break;
        case TransportProtocol::TCP:
            transportStr = "TCP";
            break;
        case TransportProtocol::Both:
            transportStr = "Both";
            break;
    }
    json["transportType"] = transportStr;
    
    json["port"] = config.port;
    
    // 长度配置
    QJsonObject lengthObj;
    if (config.isFixedLength) {
        lengthObj["type"] = "fixed";
        lengthObj["fixedValue"] = static_cast<int>(config.fixedLength);
    } else {
        lengthObj["type"] = "variable";
        lengthObj["fieldName"] = config.lengthFieldName;
        lengthObj["fieldOffset"] = static_cast<int>(config.lengthFieldOffset);
    }
    json["length"] = lengthObj;
    
    // 字段数组
    QJsonArray fieldsArray;
    for (const FieldDefinition& field : config.fields) {
        QJsonObject fieldObj;
        fieldObj["name"] = field.name;
        fieldObj["offset"] = static_cast<int>(field.offset);
        fieldObj["length"] = static_cast<int>(field.length);
        
        // 字段类型
        QString typeStr;
        switch (field.type) {
            case FieldType::UInt8:
                typeStr = "uint8";
                break;
            case FieldType::UInt16:
                typeStr = "uint16";
                break;
            case FieldType::UInt32:
                typeStr = "uint32";
                break;
            case FieldType::UInt64:
                typeStr = "uint64";
                break;
            case FieldType::Int8:
                typeStr = "int8";
                break;
            case FieldType::Int16:
                typeStr = "int16";
                break;
            case FieldType::Int32:
                typeStr = "int32";
                break;
            case FieldType::Int64:
                typeStr = "int64";
                break;
            case FieldType::String:
                typeStr = "string";
                break;
            case FieldType::ByteArray:
                typeStr = "bytearray";
                break;
        }
        fieldObj["type"] = typeStr;
        
        // 字节序
        QString endiannessStr;
        switch (field.endianness) {
            case Endianness::BigEndian:
                endiannessStr = "big";
                break;
            case Endianness::LittleEndian:
                endiannessStr = "little";
                break;
        }
        fieldObj["endianness"] = endiannessStr;
        
        fieldsArray.append(fieldObj);
    }
    json["fields"] = fieldsArray;
    
    return json;
}
