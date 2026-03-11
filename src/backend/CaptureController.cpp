#include "CaptureController.h"

CaptureController::CaptureController(QObject *parent)
    : QObject(parent),
      m_isCapturing(false),
      m_totalPackets(0),
      m_udpPackets(0),
      m_tcpPackets(0),
      m_matchedPackets(0)
{
    m_captureEngine = new PacketCaptureEngine(this);
    m_filterEngine = new FilterEngine(this);
    m_parser = new ProtocolParser(this);
    m_configLoader = new ConfigurationLoader(this);
    m_packetModel = new PacketModel(this);

    // Connect signals
    connect(m_captureEngine, &PacketCaptureEngine::packetCaptured,
            this, &CaptureController::onPacketCaptured);
    connect(m_captureEngine, &PacketCaptureEngine::packetLoaded,
            this, &CaptureController::onPacketCaptured);
    connect(m_filterEngine, &FilterEngine::packetFiltered,
            this, &CaptureController::onPacketFiltered);
}

CaptureController::~CaptureController()
{
}

QVariantList CaptureController::getInterfaces()
{
    QVariantList result;
    QList<NetworkInterface> interfaces = m_captureEngine->getAvailableInterfaces();
    
    for (const auto& iface : interfaces) {
        QVariantMap ifaceMap;
        ifaceMap["name"] = iface.name;
        ifaceMap["description"] = iface.description;
        ifaceMap["ipAddress"] = iface.ipAddress;
        ifaceMap["displayName"] = QString("%1 (%2)").arg(iface.name).arg(iface.ipAddress);
        result.append(ifaceMap);
    }
    
    return result;
}

bool CaptureController::loadProtocolConfig(const QString& filePath)
{
    QString errorMsg;
    ProtocolConfiguration config = m_configLoader->loadFromFile(filePath, errorMsg);
    
    if (!errorMsg.isEmpty()) {
        emit errorOccurred(errorMsg);
        return false;
    }
    
    m_parser->setProtocolConfig(config);
    m_filterEngine->setProtocolConfig(config);
    emit configLoaded(config.protocolName);
    
    return true;
}

bool CaptureController::startCapture(const QString& interfaceName)
{
    QString errorMsg;
    if (m_captureEngine->startCapture(interfaceName, errorMsg)) {
        m_isCapturing = true;
        emit capturingChanged();
        return true;
    }
    
    emit errorOccurred(errorMsg);
    return false;
}

void CaptureController::stopCapture()
{
    m_captureEngine->stopCapture();
    m_isCapturing = false;
    emit capturingChanged();
}

void CaptureController::setTransportFilter(int protocol) const {
    TransportProtocol tp = static_cast<TransportProtocol>(protocol);
    m_filterEngine->setTransportFilter(tp);
}

bool CaptureController::savePackets(const QString& filePath)
{
    QString errorMsg;
    if (m_captureEngine->saveToPcap(filePath, errorMsg)) {
        return true;
    }
    
    emit errorOccurred(errorMsg);
    return false;
}

bool CaptureController::loadPackets(const QString& filePath)
{
    qDebug()<< "Loading packets from PCAP file:" << filePath;
    clearPackets();
    QString errorMsg;
    if (m_captureEngine->loadFromPcap(filePath, errorMsg)) {
        return true;
    }
    
    emit errorOccurred(errorMsg);
    return false;
}

void CaptureController::clearPackets()
{
    m_packetModel->clear();
    m_totalPackets = 0;
    m_udpPackets = 0;
    m_tcpPackets = 0;
    m_matchedPackets = 0;
    emit statisticsChanged();
}

void CaptureController::onPacketCaptured(const RawPacketOfTool& packet) const {
    // qDebug()<< "Packet captured: len=" <<packet.length;
    m_filterEngine->processPacket(packet);
}

void CaptureController::onPacketFiltered(const RawPacketOfTool& packet, bool matchesProtocol)
{
    // qDebug()<< "Packet filtered: matchesProtocol=" << matchesProtocol<<"packet len="<<packet.length;
    ParsedPacket parsed = m_parser->parsePacket(packet);
    m_packetModel->addPacket(packet, parsed);
    
    m_totalPackets++;
    if (packet.protocol == TransportProtocol::UDP) {
        m_udpPackets++;
    } else if (packet.protocol == TransportProtocol::TCP) {
        m_tcpPackets++;
    }
    
    if (matchesProtocol) {
        m_matchedPackets++;
    }
    
    emit statisticsChanged();
}
