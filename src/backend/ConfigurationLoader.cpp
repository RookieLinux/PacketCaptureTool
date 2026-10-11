#include <QCoreApplication>
#include "ConfigurationLoader.h"
#include <QFile>
#include <QSaveFile>
#include <QRegularExpression>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrl>
#include <QSet>
#include <cmath>

namespace {
bool readUnsigned(const QJsonObject& object, const QString& key, quint32 minimum,
                  quint32 maximum, quint32& result, QString& errorMsg)
{
    const auto value = object.value(key);
    const double number = value.toDouble(-1);
    if (!value.isDouble() || number < minimum || number > maximum || std::floor(number) != number) {
        errorMsg = QCoreApplication::translate("Backend", "配置字段 %1 必须是 %2 到 %3 之间的整数").arg(key).arg(minimum).arg(maximum);
        return false;
    }
    result = static_cast<quint32>(number);
    return true;
}

QString normalizeConfigPath(const QString& filePath)
{
    if (filePath.startsWith("file://")) {
        const QUrl url(filePath);
        if (url.isLocalFile()) {
            return url.toLocalFile();
        }
    }
    return filePath;
}
}

ConfigurationLoader::ConfigurationLoader(QObject *parent)
    : QObject(parent)
{
}

ProtocolConfiguration ConfigurationLoader::loadFromFile(const QString& filePath, QString& errorMsg)
{
    errorMsg.clear();
    ProtocolConfiguration config;
    const QString normalizedPath = normalizeConfigPath(filePath);
    const QFileInfo fileInfo(normalizedPath);

    if (!fileInfo.exists()) {
        errorMsg = QCoreApplication::translate("Backend", "配置文件不存在：%1").arg(normalizedPath);
        return config;
    }
    
    QFile file(normalizedPath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMsg = QCoreApplication::translate("Backend", "无法打开配置文件：%1（%2）")
                       .arg(normalizedPath, file.errorString());
        return config;
    }
    
    if (file.size() > 1024*1024) { errorMsg = QCoreApplication::translate("Backend", "协议配置文件不能超过 1 MiB"); return config; }
    QByteArray data = file.readAll();
    file.close();
    return loadFromJson(QString::fromUtf8(data),errorMsg);
}

ProtocolConfiguration ConfigurationLoader::loadFromJson(const QString& json, QString& errorMsg)
{
    errorMsg.clear();
    ProtocolConfiguration config;
    const auto data = json.toUtf8();
    if (data.size() > 1024*1024) { errorMsg = QCoreApplication::translate("Backend", "协议配置不能超过 1 MiB"); return config; }
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        errorMsg = QCoreApplication::translate("Backend", "配置文件JSON格式无效：%1").arg(parseError.errorString());
        return config;
    }
    
    if (!doc.isObject()) {
        errorMsg = QCoreApplication::translate("Backend", "配置文件根元素必须是对象");
        return config;
    }
    
    config = parseJson(doc.object(), errorMsg);
    if (errorMsg.isEmpty()) validateConfig(config, errorMsg);
    return config;
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
    QSaveFile file(normalizeConfigPath(filePath));
    if (!file.open(QIODevice::WriteOnly)) {
        errorMsg = QCoreApplication::translate("Backend", "无法打开文件进行写入：%1").arg(filePath);
        return false;
    }
    
    // 写入JSON数据（使用缩进格式化）
    const auto data = doc.toJson(QJsonDocument::Indented);
    if (file.write(data) != data.size() || !file.commit()) {
        errorMsg = QCoreApplication::translate("Backend", "写入文件失败：%1").arg(filePath);
        return false;
    }
    
    return true;
}

bool ConfigurationLoader::validateConfig(const ProtocolConfiguration& config, QString& errorMsg)
{
    errorMsg.clear();
    if (config.transportType != TransportProtocol::UDP && config.transportType != TransportProtocol::TCP) {
        errorMsg = QCoreApplication::translate("Backend", "仅支持 UDP 或 TCP 协议配置"); return false;
    }
    if (config.fields.size() > 1024) { errorMsg = QCoreApplication::translate("Backend", "协议字段不能超过 1024 个"); return false; }
    // 验证协议名称
    if (config.protocolName.isEmpty()) {
        errorMsg = QCoreApplication::translate("Backend", "协议名称不能为空");
        return false;
    }
    
    // 验证端口号（虽然quint16自动限制范围，但我们检查是否为0）
    if (config.port == 0) {
        errorMsg = QCoreApplication::translate("Backend", "端口号不能为0");
        return false;
    }
    
    // 验证固定长度配置
    if (config.isFixedLength && (config.fixedLength == 0 || config.fixedLength > 65536)) {
        errorMsg = QCoreApplication::translate("Backend", "固定长度不能为0");
        return false;
    }
    
    // 验证可变长度配置
    if (!config.isFixedLength) {
        if (config.lengthFieldName.isEmpty()) {
            errorMsg = QCoreApplication::translate("Backend", "可变长度配置缺少长度字段名称");
            return false;
        }
    }
    
    // 验证字段定义
    QSet<QString> names;
    bool hasLengthField = config.isFixedLength;
    for (const FieldDefinition& field : config.fields) {
        if (names.contains(field.name)) {
            errorMsg = QCoreApplication::translate("Backend", "字段名称重复：%1").arg(field.name);
            return false;
        }
        names.insert(field.name);
        const bool unsignedInteger = field.type >= FieldType::UInt8 && field.type <= FieldType::UInt64;
        const bool integer = field.type >= FieldType::UInt8 && field.type <= FieldType::Int64;
        const bool numeric = integer || field.type == FieldType::Float32 || field.type == FieldType::Float64;
        if ((field.bitWidth || field.bitOffset) && (!unsignedInteger || !field.bitWidth ||
            quint64(field.bitOffset)+field.bitWidth > quint64(field.length)*8)) {
            errorMsg = QCoreApplication::translate("Backend", "字段 %1 的位域必须位于无符号整数范围内").arg(field.name); return false;
        }
        if (!std::isfinite(field.scale) || !std::isfinite(field.bias) ||
            (!numeric && (field.scale != 1 || field.bias != 0 || !field.unit.isEmpty()))) {
            errorMsg = QCoreApplication::translate("Backend", "字段 %1 的比例、偏置和单位仅适用于数值类型，且必须有限").arg(field.name); return false;
        }
        if (!field.enumValues.isEmpty() && !integer) { errorMsg = QCoreApplication::translate("Backend", "字段 %1 的枚举仅适用于整数").arg(field.name); return false; }
        if (!field.enumValues.isEmpty() && (field.length < 1 || field.length > 8)) { errorMsg = QCoreApplication::translate("Backend", "枚举字段长度必须在 1 到 8 字节内"); return false; }
        for (auto it=field.enumValues.cbegin();it!=field.enumValues.cend();++it) {
            bool ok=false;
            const bool canonical = QRegularExpression("^(0|-?[1-9][0-9]*)$").match(it.key()).hasMatch();
            if (unsignedInteger) {
                const auto n=it.key().toULongLong(&ok);
                const quint32 bits=field.bitWidth ? field.bitWidth : field.length*8;
                if (bits < 64 && n >= (quint64(1)<<bits)) ok=false;
            } else {
                const auto n=it.key().toLongLong(&ok);
                const quint32 bits=field.length*8;
                if (bits < 64 && (n < -(qint64(1)<<(bits-1)) || n > (qint64(1)<<(bits-1))-1)) ok=false;
            }
            if (!canonical || !ok || it.value().isEmpty()) { errorMsg = QCoreApplication::translate("Backend", "字段 %1 的枚举键必须是范围内的十进制整数，标签不能为空").arg(field.name); return false; }
        }
        if (!config.isFixedLength && field.name == config.lengthFieldName) {
            hasLengthField = unsignedInteger && field.bitWidth == 0;
        }
        // 验证字段名称
        if (field.name.isEmpty()) {
            errorMsg = QCoreApplication::translate("Backend", "字段名称不能为空");
            return false;
        }
        
        // 验证字段长度
        if (field.length == 0) {
            errorMsg = QCoreApplication::translate("Backend", "字段定义无效：%1 - 字段长度不能为0").arg(field.name);
            return false;
        }
        
        // 验证字段偏移量和长度的合理性（不能超过合理范围，例如64KB）
        const quint32 MAX_REASONABLE_OFFSET = 65536;
        const quint32 MAX_REASONABLE_LENGTH = 65536;
        
        if (field.offset > MAX_REASONABLE_OFFSET) {
            errorMsg = QCoreApplication::translate("Backend", "字段定义无效：%1 - 偏移量超出合理范围").arg(field.name);
            return false;
        }
        
        if (field.length > MAX_REASONABLE_LENGTH) {
            errorMsg = QCoreApplication::translate("Backend", "字段定义无效：%1 - 长度超出合理范围").arg(field.name);
            return false;
        }
        if (config.isFixedLength && static_cast<quint64>(field.offset) + field.length > config.fixedLength) {
            errorMsg = QCoreApplication::translate("Backend", "字段 %1 超出固定负载长度").arg(field.name);
            return false;
        }
        
        // 验证字段类型与长度的匹配
        switch (field.type) {
            case FieldType::UInt8:
            case FieldType::Int8:
                if (field.length != 1) {
                    errorMsg = QCoreApplication::translate("Backend", "字段定义无效：%1 - uint8/int8类型长度必须为1").arg(field.name);
                    return false;
                }
                break;
            case FieldType::UInt16:
            case FieldType::Int16:
                if (field.length != 2) {
                    errorMsg = QCoreApplication::translate("Backend", "字段定义无效：%1 - uint16/int16类型长度必须为2").arg(field.name);
                    return false;
                }
                break;
            case FieldType::UInt32:
            case FieldType::Int32:
            case FieldType::Float32:
                if (field.length != 4) {
                    errorMsg = QCoreApplication::translate("Backend", "字段定义无效：%1 - uint32/int32/float32类型长度必须为4").arg(field.name);
                    return false;
                }
                break;
            case FieldType::UInt64:
            case FieldType::Int64:
            case FieldType::Float64:
                if (field.length != 8) {
                    errorMsg = QCoreApplication::translate("Backend", "字段定义无效：%1 - uint64/int64/float64类型长度必须为8").arg(field.name);
                    return false;
                }
                break;
            case FieldType::String:
            case FieldType::ByteArray:
                // 字符串和字节数组可以是任意长度
                break;
        }
    }
    
    if (!hasLengthField) {
        errorMsg = QCoreApplication::translate("Backend", "可变长度必须引用已定义的无符号整数字段");
        return false;
    }
    return true;
}

ProtocolConfiguration ConfigurationLoader::parseJson(const QJsonObject& json, QString& errorMsg)
{
    ProtocolConfiguration config;
    
    // 验证必需字段：protocolName
    if (!json.contains("protocolName")) {
        errorMsg = QCoreApplication::translate("Backend", "配置文件缺少必需字段：protocolName");
        return config;
    }
    config.protocolName = json["protocolName"].toString();
    
    // 验证必需字段：transportType
    if (!json.contains("transportType")) {
        errorMsg = QCoreApplication::translate("Backend", "配置文件缺少必需字段：transportType");
        return config;
    }
    QString transportStr = json["transportType"].toString().toUpper();
    if (transportStr == "UDP") {
        config.transportType = TransportProtocol::UDP;
    } else if (transportStr == "TCP") {
        config.transportType = TransportProtocol::TCP;
    } else {
        errorMsg = QCoreApplication::translate("Backend", "无效的传输层协议类型：%1").arg(transportStr);
        return config;
    }
    
    // 验证必需字段：port
    if (!json.contains("port")) {
        errorMsg = QCoreApplication::translate("Backend", "配置文件缺少必需字段：port");
        return config;
    }
    const double port = json["port"].toDouble(-1);
    if (port < 1 || port > 65535 || port != static_cast<int>(port)) {
        errorMsg = QCoreApplication::translate("Backend", "端口号必须是1到65535之间的整数");
        return config;
    }
    config.port = static_cast<quint16>(port);
    
    // 解析长度配置
    if (!json.contains("length")) {
        errorMsg = QCoreApplication::translate("Backend", "配置文件缺少必需字段：length");
        return config;
    }
    QJsonObject lengthObj = json["length"].toObject();
    QString lengthType = lengthObj["type"].toString();
    
    if (lengthType == "fixed") {
        config.isFixedLength = true;
        if (!readUnsigned(lengthObj, "fixedValue", 1, 65536, config.fixedLength, errorMsg)) return config;
        config.lengthFieldName = QString(); // Clear variable length fields
        config.lengthFieldOffset = 0;
    } else if (lengthType == "variable") {
        config.isFixedLength = false;
        config.fixedLength = 0; // Initialize to 0 when not used
        config.lengthFieldName = lengthObj["fieldName"].toString();
        config.lengthFieldOffset = static_cast<quint32>(lengthObj["fieldOffset"].toInt());
    } else {
        errorMsg = QCoreApplication::translate("Backend", "无效的长度类型：%1").arg(lengthType);
        return config;
    }
    
    // 解析字段数组
    if (!json.contains("fields")) {
        errorMsg = QCoreApplication::translate("Backend", "配置文件缺少必需字段：fields");
        return config;
    }
    
    if (!json["fields"].isArray()) {
        errorMsg = QCoreApplication::translate("Backend", "fields 必须是数组");
        return config;
    }
    QJsonArray fieldsArray = json["fields"].toArray();
    for (const QJsonValue& fieldValue : fieldsArray) {
        QJsonObject fieldObj = fieldValue.toObject();
        FieldDefinition field;
        
        field.name = fieldObj["name"].toString();
        if (!readUnsigned(fieldObj, "offset", 0, 65536, field.offset, errorMsg) ||
            !readUnsigned(fieldObj, "length", 1, 65536, field.length, errorMsg)) return config;
        
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
        } else if (typeStr == "float32") {
            field.type = FieldType::Float32;
        } else if (typeStr == "float64") {
            field.type = FieldType::Float64;
        } else {
            errorMsg = QCoreApplication::translate("Backend", "字段 %1 的类型无效：%2").arg(field.name, typeStr);
            return config;
        }
        
        // 解析字节序
        QString endiannessStr = fieldObj["endianness"].toString().toLower();
        if (endiannessStr == "big") {
            field.endianness = Endianness::BigEndian;
        } else if (endiannessStr == "little") {
            field.endianness = Endianness::LittleEndian;
        } else {
            errorMsg = QCoreApplication::translate("Backend", "字段 %1 的字节序无效：%2").arg(field.name, endiannessStr);
            return config;
        }
        
        if (fieldObj.contains("bitOffset") && !readUnsigned(fieldObj,"bitOffset",0,63,field.bitOffset,errorMsg)) return config;
        if (fieldObj.contains("bitWidth") && !readUnsigned(fieldObj,"bitWidth",0,64,field.bitWidth,errorMsg)) return config;
        for (const auto& key : {QString("scale"),QString("bias")}) {
            if (fieldObj.contains(key) && !fieldObj[key].isDouble()) { errorMsg = key + QCoreApplication::translate("Backend", " 必须是数值"); return config; }
        }
        field.scale = fieldObj["scale"].toDouble(1);
        field.bias = fieldObj["bias"].toDouble(0);
        if (fieldObj.contains("unit") && !fieldObj["unit"].isString()) { errorMsg = QCoreApplication::translate("Backend", "unit 必须是字符串"); return config; }
        field.unit = fieldObj["unit"].toString();
        if (fieldObj.contains("enumValues") && !fieldObj["enumValues"].isObject()) { errorMsg = QCoreApplication::translate("Backend", "enumValues 必须是对象"); return config; }
        const auto enums = fieldObj["enumValues"].toObject();
        for (auto it=enums.begin();it!=enums.end();++it) {
            if (!it.value().isString()) { errorMsg = QCoreApplication::translate("Backend", "枚举标签必须是字符串"); return config; }
            field.enumValues.insert(it.key(),it.value().toString());
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
        case TransportProtocol::Other:
            transportStr = "Other";
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
            case FieldType::Float32: typeStr = "float32"; break;
            case FieldType::Float64: typeStr = "float64"; break;
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
        if (field.bitWidth) { fieldObj["bitOffset"] = int(field.bitOffset); fieldObj["bitWidth"] = int(field.bitWidth); }
        if (field.scale != 1) fieldObj["scale"] = field.scale;
        if (field.bias != 0) fieldObj["bias"] = field.bias;
        if (!field.unit.isEmpty()) fieldObj["unit"] = field.unit;
        if (!field.enumValues.isEmpty()) {
            QJsonObject enums;
            for (auto it=field.enumValues.cbegin();it!=field.enumValues.cend();++it) enums[it.key()] = it.value();
            fieldObj["enumValues"] = enums;
        }
        
        fieldsArray.append(fieldObj);
    }
    json["fields"] = fieldsArray;
    
    return json;
}
