#include "FilterEngine.h"

FilterEngine::FilterEngine(QObject *parent)
    : QObject(parent), m_transportFilter(TransportProtocol::Both), m_hasProtocolConfig(false)
{
}

void FilterEngine::setTransportFilter(TransportProtocol protocol)
{
    m_transportFilter = protocol;
}

void FilterEngine::setProtocolConfig(const ProtocolConfiguration& config)
{
    m_protocolConfig = config;
    m_hasProtocolConfig = true;
}

void FilterEngine::processPacket(const RawPacketOfTool& packet)
{
    bool matchesProtocol = false;
    
    // Check transport layer filter
    bool passesTransportFilter = false;
    switch (m_transportFilter) {
        case TransportProtocol::UDP:
            passesTransportFilter = (packet.protocol == TransportProtocol::UDP);
            break;
        case TransportProtocol::TCP:
            passesTransportFilter = (packet.protocol == TransportProtocol::TCP);
            break;
        case TransportProtocol::Both:
            passesTransportFilter = (packet.protocol == TransportProtocol::UDP || 
                                     packet.protocol == TransportProtocol::TCP);
            break;
    }
    
    if (!passesTransportFilter) {
        // Packet doesn't pass transport filter, don't emit signal
        return;
    }
    
    // Check if packet matches protocol configuration
    if (m_hasProtocolConfig) {
        // Check protocol type
        bool protocolTypeMatches = false;
        if (packet.protocol == TransportProtocol::UDP && m_protocolConfig.transportType == TransportProtocol::UDP) {
            protocolTypeMatches = true;
        } else if (packet.protocol == TransportProtocol::TCP && m_protocolConfig.transportType == TransportProtocol::TCP) {
            protocolTypeMatches = true;
        }
        
        // Check port
        bool portMatches = (packet.destPort == m_protocolConfig.port || 
                           packet.sourcePort == m_protocolConfig.port);
        
        matchesProtocol = protocolTypeMatches && portMatches;
    }
    
    emit packetFiltered(packet, matchesProtocol);
}
