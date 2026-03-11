#ifndef DATATYPES_H
#define DATATYPES_H

#include <QString>
#include <QByteArray>
#include <QList>
#include <QVariant>

// 传输层协议枚举
enum class TransportProtocol {
    UDP,
    TCP,
    Both
};

// 字段数据类型枚举
enum class FieldType {
    UInt8,
    UInt16,
    UInt32,
    UInt64,
    Int8,
    Int16,
    Int32,
    Int64,
    String,
    ByteArray
};

// 字节序枚举
enum class Endianness {
    BigEndian,
    LittleEndian
};

// 网络接口结构体
struct NetworkInterface {
    QString name;        // 接口名称（如 eth0）
    QString description; // 接口描述
    QString ipAddress;   // IP地址
    
    // 相等性比较运算符（用于测试）
    bool operator==(const NetworkInterface& other) const {
        return name == other.name &&
               description == other.description &&
               ipAddress == other.ipAddress;
    }
    
    bool operator!=(const NetworkInterface& other) const {
        return !(*this == other);
    }
};

// 原始数据包结构体
struct RawPacketOfTool {
    QByteArray data;           // 完整数据包数据
    qint64 timestamp;          // 捕获时间戳（微秒）
    quint32 length;            // 数据包长度
    QString sourceIP;          // 源IP地址
    QString destIP;            // 目标IP地址
    quint16 sourcePort;        // 源端口
    quint16 destPort;          // 目标端口
    TransportProtocol protocol; // UDP或TCP
    
    // 相等性比较运算符（用于测试）
    bool operator==(const RawPacketOfTool& other) const {
        return data == other.data &&
               timestamp == other.timestamp &&
               length == other.length &&
               sourceIP == other.sourceIP &&
               destIP == other.destIP &&
               sourcePort == other.sourcePort &&
               destPort == other.destPort &&
               protocol == other.protocol;
    }
    
    bool operator!=(const RawPacketOfTool& other) const {
        return !(*this == other);
    }
};

// 解析后的字段结构体
struct ParsedField {
    QString name;                    // 字段名称
    QVariant value;                  // 字段值
    QString type;                    // 字段类型
    QString displayValue;            // 显示值（格式化后）
    
    // 相等性比较运算符（用于测试）
    bool operator==(const ParsedField& other) const {
        return name == other.name &&
               value == other.value &&
               type == other.type &&
               displayValue == other.displayValue;
    }
    
    bool operator!=(const ParsedField& other) const {
        return !(*this == other);
    }
};

// 解析后的数据包结构体
struct ParsedPacket {
    bool isValid;                    // 解析是否成功
    QString errorMessage;            // 错误信息
    QList<ParsedField> fields;       // 解析的字段列表
    QByteArray rawData;              // 原始数据
    
    // 相等性比较运算符（用于测试）
    bool operator==(const ParsedPacket& other) const {
        return isValid == other.isValid &&
               errorMessage == other.errorMessage &&
               fields == other.fields &&
               rawData == other.rawData;
    }
    
    bool operator!=(const ParsedPacket& other) const {
        return !(*this == other);
    }
};

// 字段定义结构体
struct FieldDefinition {
    QString name;                    // 字段名称
    quint32 offset;                  // 字节偏移量
    quint32 length;                  // 字段长度（字节）
    FieldType type;                  // 数据类型
    Endianness endianness;           // 字节序
    
    // 相等性比较运算符（用于测试）
    bool operator==(const FieldDefinition& other) const {
        return name == other.name &&
               offset == other.offset &&
               length == other.length &&
               type == other.type &&
               endianness == other.endianness;
    }
    
    bool operator!=(const FieldDefinition& other) const {
        return !(*this == other);
    }
};

// 协议配置结构体
struct ProtocolConfiguration {
    QString protocolName;            // 协议名称
    TransportProtocol transportType; // 传输层协议类型
    quint16 port;                    // 端口号
    
    // 包长度配置
    bool isFixedLength;              // 是否固定长度
    quint32 fixedLength;             // 固定长度值
    QString lengthFieldName;         // 长度字段名称
    quint32 lengthFieldOffset;       // 长度字段偏移量
    
    // 字段定义
    QList<FieldDefinition> fields;
    
    // 相等性比较运算符（用于测试）
    bool operator==(const ProtocolConfiguration& other) const {
        return protocolName == other.protocolName &&
               transportType == other.transportType &&
               port == other.port &&
               isFixedLength == other.isFixedLength &&
               fixedLength == other.fixedLength &&
               lengthFieldName == other.lengthFieldName &&
               lengthFieldOffset == other.lengthFieldOffset &&
               fields == other.fields;
    }
    
    bool operator!=(const ProtocolConfiguration& other) const {
        return !(*this == other);
    }
};

#endif // DATATYPES_H
