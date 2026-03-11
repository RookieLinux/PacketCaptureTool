#ifndef FILTERENGINE_H
#define FILTERENGINE_H

#include <QObject>
#include "DataTypes.h"

class FilterEngine : public QObject
{
    Q_OBJECT

public:
    explicit FilterEngine(QObject *parent = nullptr);

    void setTransportFilter(TransportProtocol protocol);
    void setProtocolConfig(const ProtocolConfiguration& config);
    void processPacket(const RawPacketOfTool& packet);

signals:
    void packetFiltered(const RawPacketOfTool& packet, bool matchesProtocol);

private:
    TransportProtocol m_transportFilter;
    ProtocolConfiguration m_protocolConfig;
    bool m_hasProtocolConfig;
};

#endif // FILTERENGINE_H
