#ifndef PROTOCOLPARSER_H
#define PROTOCOLPARSER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include "DataTypes.h"

class ProtocolParser : public QObject
{
    Q_OBJECT

public:
    explicit ProtocolParser(QObject *parent = nullptr);

    void setProtocolConfig(const ProtocolConfiguration& config);
    ParsedPacket parsePacket(const RawPacketOfTool& packet);

private:
    QVariant extractField(const QByteArray& data, const FieldDefinition& field);
    int determinePacketLength(const QByteArray& data);
    template<typename T>
    T convertEndianness(T value, Endianness endianness);

    ProtocolConfiguration m_config;
    bool m_hasConfig;
};

#endif // PROTOCOLPARSER_H
