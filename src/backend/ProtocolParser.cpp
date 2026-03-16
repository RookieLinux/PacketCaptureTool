#include "ProtocolParser.h"

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
        result.errorMessage = "No protocol configuration loaded";
        return result;
    }
    
    // Check if this packet matches our protocol
    if (packet.protocol == TransportProtocol::UDP && m_config.transportType != TransportProtocol::UDP) {
        result.errorMessage = "Packet protocol doesn't match configuration";
        return result;
    }
    if (packet.protocol == TransportProtocol::TCP && m_config.transportType != TransportProtocol::TCP) {
        result.errorMessage = "Packet protocol doesn't match configuration";
        return result;
    }
    
    // Check if port matches
    if (packet.destPort != m_config.port && packet.sourcePort != m_config.port) {
        result.errorMessage = "Port doesn't match configuration";
        return result;
    }
    
    // Determine packet length
    int packetLength = determinePacketLength(packet.data);
    if (packetLength <= 0) {
        result.errorMessage = "Invalid packet length";
        return result;
    }
    
    // Extract fields
    for (const auto& fieldDef : m_config.fields) {
        ParsedField parsedField;
        parsedField.name = fieldDef.name;
        
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
        }
        
        parsedField.value = extractField(packet.data, fieldDef);
        
        // Format display value
        if (parsedField.value.isValid()) {
            if (fieldDef.type == FieldType::String || fieldDef.type == FieldType::ByteArray) {
                parsedField.displayValue = parsedField.value.toString();
            } else {
                parsedField.displayValue = QString::number(parsedField.value.toULongLong());
            }
        } else {
            parsedField.displayValue = "Invalid";
        }
        
        result.fields.append(parsedField);
    }
    
    result.isValid = true;
    result.errorMessage = "";
    return result;
}

QVariant ProtocolParser::extractField(const QByteArray& data, const FieldDefinition& field)
{
    // Check bounds
    if (field.offset + field.length > static_cast<quint32>(data.size())) {
        return QVariant();
    }
    
    // Extract bytes
    QByteArray fieldData = data.mid(field.offset, field.length);
    
    // Convert based on field type
    switch (field.type) {
        case FieldType::UInt8: {
            if (fieldData.size() < 1) return QVariant();
            quint8 value = static_cast<quint8>(fieldData[0]);
            return QVariant(value);
        }
        case FieldType::UInt16: {
            if (fieldData.size() < 2) return QVariant();
            quint16 value = *reinterpret_cast<const quint16*>(fieldData.constData());
            value = convertEndianness(value, field.endianness);
            return QVariant(value);
        }
        case FieldType::UInt32: {
            if (fieldData.size() < 4) return QVariant();
            quint32 value = *reinterpret_cast<const quint32*>(fieldData.constData());
            value = convertEndianness(value, field.endianness);
            return QVariant(value);
        }
        case FieldType::UInt64: {
            if (fieldData.size() < 8) return QVariant();
            quint64 value = *reinterpret_cast<const quint64*>(fieldData.constData());
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
            qint16 value = *reinterpret_cast<const qint16*>(fieldData.constData());
            value = convertEndianness(value, field.endianness);
            return QVariant(value);
        }
        case FieldType::Int32: {
            if (fieldData.size() < 4) return QVariant();
            qint32 value = *reinterpret_cast<const qint32*>(fieldData.constData());
            value = convertEndianness(value, field.endianness);
            return QVariant(value);
        }
        case FieldType::Int64: {
            if (fieldData.size() < 8) return QVariant();
            qint64 value = *reinterpret_cast<const qint64*>(fieldData.constData());
            value = convertEndianness(value, field.endianness);
            return QVariant(value);
        }
        case FieldType::String: {
            // Convert to string, trim null characters
            QString str = QString::fromUtf8(fieldData);
            str = str.trimmed();
            return QVariant(str);
        }
        case FieldType::ByteArray: {
            return QVariant(fieldData);
        }
    }
    
    return QVariant();
}

int ProtocolParser::determinePacketLength(const QByteArray& data)
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
                QVariant lengthValue = extractField(data, field);
                if (lengthValue.isValid()) {
                    return lengthValue.toInt();
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
        T result = 0;
        for (size_t i = 0; i < sizeof(T); i++) {
            result = (result << 8) | ((value >> (i * 8)) & 0xFF);
        }
        return result;
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