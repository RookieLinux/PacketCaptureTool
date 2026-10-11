#include <QCoreApplication>
#include "ProtocolParser.h"
#include <cstring>
#include <limits>
#include <cmath>

ProtocolParser::ProtocolParser(QObject *parent)
    : QObject(parent), m_hasConfig(false)
{
}

void ProtocolParser::setProtocolConfig(const ProtocolConfiguration& config)
{
    m_config = config;
    m_hasConfig = true;
}

ParsedPacket ProtocolParser::parsePacket(const RawPacketOfTool& packet)
{
    ParsedPacket result;
    result.isValid = false;
    result.rawData = packet.data;
    
    if (!m_hasConfig) {
        result.errorMessage = QCoreApplication::translate("Backend", "未加载协议配置");
        return result;
    }
    
    // Check if this packet matches our protocol
    result.status = ParseStatus::Unmatched;
    if (packet.protocol == TransportProtocol::Other) {
        result.errorMessage = QCoreApplication::translate("Backend", "此网络协议没有自定义解析配置");
        return result;
    }
    if (packet.protocol == TransportProtocol::UDP && m_config.transportType != TransportProtocol::UDP) {
        result.errorMessage = QCoreApplication::translate("Backend", "传输协议与配置不匹配");
        return result;
    }
    if (packet.protocol == TransportProtocol::TCP && m_config.transportType != TransportProtocol::TCP) {
        result.errorMessage = QCoreApplication::translate("Backend", "传输协议与配置不匹配");
        return result;
    }
    
    // Check if port matches
    if (m_config.port != 0 && packet.destPort != m_config.port && packet.sourcePort != m_config.port) {
        result.errorMessage = QCoreApplication::translate("Backend", "端口不匹配（配置 %1，源端口 %2，目标端口 %3）")
                               .arg(m_config.port).arg(packet.sourcePort).arg(packet.destPort);
        return result;
    }
    
    // Determine packet length
    result.status = ParseStatus::Failed;
    if (packet.length != static_cast<quint32>(packet.data.size())) {
        result.errorMessage = QCoreApplication::translate("Backend", "捕获长度与实际数据不一致");
        return result;
    }
    if (packet.length < packet.headerLength) {
        result.errorMessage = QCoreApplication::translate("Backend", "报文头长度超过报文总长");
        return result;
    }
    int packetPayloadLength = packet.payloadLength >= 0 ? packet.payloadLength : packet.length - packet.headerLength;
    if (static_cast<quint64>(packet.headerLength) + packetPayloadLength > static_cast<quint64>(packet.data.size())) {
        result.errorMessage = QCoreApplication::translate("Backend", "捕获的应用负载不完整");
        return result;
    }
    
    // Validation based on requirements
    if (m_config.isFixedLength) {
        // 当配置文件中length的type为fixed时fixedValue的长度为包的总长度减去以太网帧头、IP头、传输层头
        if (m_config.fixedLength != static_cast<quint32>(packetPayloadLength)) {
            result.errorMessage = QCoreApplication::translate("Backend", "负载长度不匹配（配置 %1，实际 %2）")
                                   .arg(m_config.fixedLength).arg(packetPayloadLength);
            return result;
        }
        
    }

    // Validate ranges, including offsets, before extracting any field.
    for (const auto& field : m_config.fields) {
        if (static_cast<quint64>(field.offset) + field.length > static_cast<quint64>(packetPayloadLength)) {
            result.errorMessage = QCoreApplication::translate("Backend", "字段 %1 超出负载范围（偏移 %2，长度 %3，负载 %4）")
                .arg(field.name).arg(field.offset).arg(field.length).arg(packetPayloadLength);
            return result;
        }
    }
    if (!m_config.isFixedLength && !m_config.lengthFieldName.isEmpty()) {
        const int declaredLength = determinePacketLength(packet.data, packet.headerLength);
        if (declaredLength != packetPayloadLength) {
            result.errorMessage = QCoreApplication::translate("Backend", "声明长度不匹配（声明 %1，实际 %2）")
                .arg(declaredLength).arg(packetPayloadLength);
            return result;
        }
    }

    // Extract fields
    if (m_config.fields.isEmpty()) {
        ParsedField infoField;
        infoField.name = QCoreApplication::translate("Backend", "提示");
        infoField.type = "info";
        infoField.value = QCoreApplication::translate("Backend", "配置未定义字段");
        infoField.displayValue = QCoreApplication::translate("Backend", "配置未定义字段");
        result.fields.append(infoField);
    }

    for (const auto& fieldDef : m_config.fields) {
        ParsedField parsedField;
        parsedField.name = fieldDef.name;
        parsedField.offset = packet.headerLength + fieldDef.offset;
        parsedField.length = fieldDef.length;
        
        // Convert FieldType enum to string
        switch (fieldDef.type) {
            case FieldType::UInt8: parsedField.type = "uint8"; break;
            case FieldType::UInt16: parsedField.type = "uint16"; break;
            case FieldType::UInt32: parsedField.type = "uint32"; break;
            case FieldType::UInt64: parsedField.type = "uint64"; break;
            case FieldType::Int8: parsedField.type = "int8"; break;
            case FieldType::Int16: parsedField.type = "int16"; break;
            case FieldType::Int32: parsedField.type = "int32"; break;
            case FieldType::Int64: parsedField.type = "int64"; break;
            case FieldType::String: parsedField.type = "string"; break;
            case FieldType::ByteArray: parsedField.type = "bytearray"; break;
            case FieldType::Float32: parsedField.type = "float32"; break;
            case FieldType::Float64: parsedField.type = "float64"; break;
        }
        
        parsedField.value = extractField(packet.data, fieldDef, packet.headerLength);
        
        // Format display value
        if (parsedField.value.isValid()) {
            if (fieldDef.type == FieldType::String) {
                parsedField.displayValue = parsedField.value.toString();
            } else if (fieldDef.type == FieldType::ByteArray) {
                parsedField.displayValue = parsedField.value.toByteArray().toHex(' ').toUpper();
            } else if (fieldDef.type == FieldType::Float32 || fieldDef.type == FieldType::Float64) {
                parsedField.displayValue = QString::number(parsedField.value.toDouble(),'g',15);
            } else if (fieldDef.type == FieldType::Int8 || fieldDef.type == FieldType::Int16 ||
                       fieldDef.type == FieldType::Int32 || fieldDef.type == FieldType::Int64) {
                parsedField.displayValue = QString::number(parsedField.value.toLongLong());
            } else {
                parsedField.displayValue = QString::number(parsedField.value.toULongLong());
            }
        } else {
            parsedField.displayValue = QCoreApplication::translate("Backend", "无效");
            result.errorMessage = QCoreApplication::translate("Backend", "无法解析字段 %1").arg(fieldDef.name);
            result.fields.append(parsedField);
            return result;
        }
        const QString rawValue = parsedField.displayValue;
        if (fieldDef.scale != 1 || fieldDef.bias != 0) {
            const double converted = parsedField.value.toDouble()*fieldDef.scale + fieldDef.bias;
            if (!std::isfinite(converted)) { result.errorMessage = QCoreApplication::translate("Backend", "字段 %1 换算结果不是有限数值").arg(fieldDef.name); return result; }
            parsedField.displayValue = QString::number(converted,'g',15);
        }
        if (!fieldDef.unit.isEmpty()) parsedField.displayValue += " " + fieldDef.unit;
        if (fieldDef.enumValues.contains(rawValue)) {
            const QString label = fieldDef.enumValues.value(rawValue) + "（" + rawValue + "）";
            parsedField.displayValue = (fieldDef.scale == 1 && fieldDef.bias == 0 && fieldDef.unit.isEmpty()) ?
                label : label + " · " + parsedField.displayValue;
        }
        
        result.fields.append(parsedField);
    }
    
    result.isValid = true;
    result.status = ParseStatus::Parsed;
    result.errorMessage = "";
    return result;
}

QVariant ProtocolParser::extractField(const QByteArray& data, const FieldDefinition& field, quint32 baseOffset)
{
    // Check bounds
    if (static_cast<quint64>(baseOffset) + field.offset + field.length > static_cast<quint64>(data.size())) {
        return QVariant();
    }
    
    // Extract bytes
    QByteArray fieldData = data.mid(baseOffset + field.offset, field.length);
    if (field.bitWidth) {
        if (field.type > FieldType::UInt64 || field.length > 8 || field.bitWidth > 64 ||
            quint64(field.bitOffset)+field.bitWidth > quint64(field.length)*8) return {};
        quint64 bits=0;
        for (quint32 i=0;i<field.length;++i)
            bits=(bits<<8) | static_cast<quint8>(fieldData[field.endianness == Endianness::BigEndian ? i : field.length-i-1]);
        const quint64 mask=field.bitWidth == 64 ? std::numeric_limits<quint64>::max() : ((quint64(1)<<field.bitWidth)-1);
        return QVariant::fromValue((bits>>field.bitOffset)&mask);
    }
    
    // Convert based on field type
    switch (field.type) {
        case FieldType::UInt8: {
            if (fieldData.size() < 1) return QVariant();
            quint8 value = static_cast<quint8>(fieldData[0]);
            return QVariant(value);
        }
        case FieldType::UInt16: {
            if (fieldData.size() < 2) return QVariant();
            quint16 value;
            std::memcpy(&value, fieldData.constData(), sizeof(value));
            value = convertEndianness(value, field.endianness);
            return QVariant(value);
        }
        case FieldType::UInt32: {
            if (fieldData.size() < 4) return QVariant();
            quint32 value;
            std::memcpy(&value, fieldData.constData(), sizeof(value));
            value = convertEndianness(value, field.endianness);
            return QVariant(value);
        }
        case FieldType::UInt64: {
            if (fieldData.size() < 8) return QVariant();
            quint64 value;
            std::memcpy(&value, fieldData.constData(), sizeof(value));
            value = convertEndianness(value, field.endianness);
            return QVariant(value);
        }
        case FieldType::Int8: {
            if (fieldData.size() < 1) return QVariant();
            qint8 value = static_cast<qint8>(fieldData[0]);
            return QVariant(value);
        }
        case FieldType::Int16: {
            if (fieldData.size() < 2) return QVariant();
            qint16 value;
            std::memcpy(&value, fieldData.constData(), sizeof(value));
            value = convertEndianness(value, field.endianness);
            return QVariant(value);
        }
        case FieldType::Int32: {
            if (fieldData.size() < 4) return QVariant();
            qint32 value;
            std::memcpy(&value, fieldData.constData(), sizeof(value));
            value = convertEndianness(value, field.endianness);
            return QVariant(value);
        }
        case FieldType::Int64: {
            if (fieldData.size() < 8) return QVariant();
            qint64 value;
            std::memcpy(&value, fieldData.constData(), sizeof(value));
            value = convertEndianness(value, field.endianness);
            return QVariant(value);
        }
        case FieldType::String: {
            // Convert to string, trim null characters
            QString str = QString::fromUtf8(fieldData);
            while (str.endsWith(QChar(0))) str.chop(1);
            return QVariant(str);
        }
        case FieldType::ByteArray: {
            return QVariant(fieldData);
        }
        case FieldType::Float32: {
            if (fieldData.size() != 4) return {};
            quint32 bits; std::memcpy(&bits,fieldData.constData(),4);
            bits = convertEndianness(bits,field.endianness);
            float number; std::memcpy(&number,&bits,4);
            return std::isfinite(number) ? QVariant(double(number)) : QVariant();
        }
        case FieldType::Float64: {
            if (fieldData.size() != 8) return {};
            quint64 bits; std::memcpy(&bits,fieldData.constData(),8);
            bits = convertEndianness(bits,field.endianness);
            double number; std::memcpy(&number,&bits,8);
            return std::isfinite(number) ? QVariant(number) : QVariant();
        }
    }
    
    return QVariant();
}

int ProtocolParser::determinePacketLength(const QByteArray& data, quint32 baseOffset)
{
    if (!m_hasConfig) {
        return -1;
    }
    
    if (m_config.isFixedLength) {
        return m_config.fixedLength;
    } else {
        // Variable length - find the length field
        for (const auto& field : m_config.fields) {
            if (field.name == m_config.lengthFieldName) {
                // Extract the length field value
                QVariant lengthValue = extractField(data, field, baseOffset);
                if (lengthValue.isValid()) {
                    const auto length = lengthValue.toULongLong();
                    return length <= static_cast<quint64>(std::numeric_limits<int>::max()) ? static_cast<int>(length) : -1;
                }
                break;
            }
        }
        return -1;
    }
}

template<typename T>
T ProtocolParser::convertEndianness(T value, Endianness endianness)
{
    // Check if we need to swap bytes
    bool needsSwap = false;
    
    // Determine native endianness
    union {
        uint32_t i;
        char c[4];
    } test = {0x01020304};
    
    bool isLittleEndian = (test.c[0] == 4);
    
    if ((isLittleEndian && endianness == Endianness::BigEndian) ||
        (!isLittleEndian && endianness == Endianness::LittleEndian)) {
        needsSwap = true;
    }
    
    if (needsSwap) {
        // Swap bytes
        using UnsignedT = typename std::make_unsigned<T>::type;
        UnsignedT uValue = static_cast<UnsignedT>(value);
        UnsignedT uResult = 0;
        for (size_t i = 0; i < sizeof(T); i++) {
            uResult = (uResult << 8) | ((uValue >> (i * 8)) & 0xFF);
        }
        return static_cast<T>(uResult);
    }
    
    return value;
}

// Explicit template instantiations
template quint8 ProtocolParser::convertEndianness<quint8>(quint8 value, Endianness endianness);
template quint16 ProtocolParser::convertEndianness<quint16>(quint16 value, Endianness endianness);
template quint32 ProtocolParser::convertEndianness<quint32>(quint32 value, Endianness endianness);
template quint64 ProtocolParser::convertEndianness<quint64>(quint64 value, Endianness endianness);
template qint8 ProtocolParser::convertEndianness<qint8>(qint8 value, Endianness endianness);
template qint16 ProtocolParser::convertEndianness<qint16>(qint16 value, Endianness endianness);
template qint32 ProtocolParser::convertEndianness<qint32>(qint32 value, Endianness endianness);
template qint64 ProtocolParser::convertEndianness<qint64>(qint64 value, Endianness endianness);
