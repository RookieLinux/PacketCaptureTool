#include "PacketCaptureEngine.h"
#include <pcapplusplus/PcapLiveDevice.h>
#include <pcapplusplus/PcapLiveDeviceList.h>
#include <pcapplusplus/PcapFileDevice.h>
#include <pcapplusplus/EthLayer.h>
#include <pcapplusplus/IPv4Layer.h>
#include <pcapplusplus/TcpLayer.h>
#include <pcapplusplus/UdpLayer.h>
#include <pcapplusplus/Packet.h>
#include <QDateTime>
#include <QNetworkInterface>
#include <memory>

PacketCaptureEngine::PacketCaptureEngine(QObject *parent)
    : QObject(parent),
      m_captureDevice(nullptr),
      m_fileWriter(nullptr),
      m_isCapturing(false),
      m_captureThread(nullptr)
{
}

PacketCaptureEngine::~PacketCaptureEngine()
{
    stopCapture();

    if (m_fileWriter) {
        delete m_fileWriter;
        m_fileWriter = nullptr;
    }
}

QList<NetworkInterface> PacketCaptureEngine::getAvailableInterfaces()
{
    QList<NetworkInterface> interfaces;

    try {
        // Get the list of available interfaces
        std::vector<pcpp::PcapLiveDevice*> devList = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDevicesList();

        for (auto* dev : devList) {
            const QString devName = QString::fromStdString(dev->getName());
            const QNetworkInterface qtIface = QNetworkInterface::interfaceFromName(devName);
            if (!qtIface.isValid()) {
                continue;
            }
            const auto flags = qtIface.flags();
            if (!(flags & QNetworkInterface::IsUp) ||
                !(flags & QNetworkInterface::IsRunning) ||
                (flags & QNetworkInterface::IsLoopBack)) {
                continue;
            }
            NetworkInterface iface;
            iface.name = QString::fromStdString(dev->getName());
            iface.description = QString::fromStdString(dev->getDesc());
            // Get IPv4 address
            if (!dev->getIPAddresses().empty()) {
                pcpp::IPv4Address ipAddr;
                for (const auto& addr : dev->getIPAddresses()) {
                    if (addr.getType() == pcpp::IPAddress::IPv4AddressType) {
                        ipAddr = addr.getIPv4();
                        if (pcpp::IPv4Address::isValidIPv4Address(ipAddr.toString())) {
                            iface.ipAddress = QString::fromStdString(ipAddr.toString());
                            break;
                        }
                    }
                }
            }
            interfaces.append(iface);
        }
    } catch (const std::exception& e) {
        // Log error but return empty list
        qWarning() << "Error getting network interfaces:" << e.what();
    }

    return interfaces;
}

// Static callback function for packet capture
void PacketCaptureEngine::onPacketArrivesStatic(pcpp::RawPacket* rawPacket, [[__maybe_unused__]] pcpp::PcapLiveDevice* dev, void* userData)
{
    if (PacketCaptureEngine* engine = static_cast<PacketCaptureEngine*>(userData)) {
        engine->onPacketArrived(rawPacket);
    }
}

bool PacketCaptureEngine::startCapture(const QString& interfaceName, QString& errorMsg)
{
    // Stop any existing capture
    stopCapture();

    try {
        // Find the device
        std::string devName = interfaceName.toStdString();
        pcpp::PcapLiveDevice* dev = pcpp::PcapLiveDeviceList::getInstance().getDeviceByIpOrName(devName);

        if (dev == nullptr) {
            errorMsg = QString("Interface not found: %1").arg(interfaceName);
            return false;
        }

        // Try to open the device
        if (!dev->open()) {
            errorMsg = QString("Cannot open device %1").arg(interfaceName);
            return false;
        }

        // Set the packet capture callback
        m_captureDevice = dev;

        // Start capturing (non-blocking)
        dev->startCapture(onPacketArrivesStatic, this);
        m_isCapturing = true;

        return true;

    } catch (const std::exception& e) {
        errorMsg = QString("Error starting capture: %1").arg(e.what());
        return false;
    }
}

void PacketCaptureEngine::stopCapture()
{
    if (m_captureDevice && m_isCapturing) {
        try {
            m_captureDevice->stopCapture();
            m_captureDevice->close();
            m_captureDevice = nullptr;
        } catch (const std::exception& e) {
            qWarning() << "Error stopping capture:" << e.what();
        }
    }

    m_isCapturing = false;
}

bool PacketCaptureEngine::saveToPcap(const QString& filePath, QString& errorMsg)
{
    try {
        QMutexLocker locker(&m_packetsMutex);

        if (m_capturedPackets.isEmpty()) {
            errorMsg = "No packets to save";
            return false;
        }

        // Create a PCAP file writer
        std::string filePathStr = filePath.toStdString();
        pcpp::PcapFileWriterDevice writer(filePathStr);

        if (!writer.open()) {
            errorMsg = "Cannot open file for writing";
            return false;
        }

        // Write all captured packets
        for (const auto& packet : m_capturedPackets) {
            // Convert our packet to PcapPlusPlus format and write
            struct timeval tv;
            tv.tv_sec = packet.timestamp / 1000000;
            tv.tv_usec = packet.timestamp % 1000000;

            pcpp::RawPacket rawPacket(reinterpret_cast<const uint8_t*>(packet.data.constData()),
                                     packet.data.length(), tv, false);
            writer.writePacket(rawPacket);
        }

        writer.close();
        return true;

    } catch (const std::exception& e) {
        errorMsg = QString("Error saving PCAP: %1").arg(e.what());
        return false;
    }
}

bool PacketCaptureEngine::loadFromPcap(const QString& filePath, QString& errorMsg)
{
    try {
        std::string filePathStr = filePath.toStdString();
        std::unique_ptr<pcpp::IFileReaderDevice> reader(pcpp::IFileReaderDevice::getReader(filePathStr));

        if (reader == nullptr || !reader->open()) {
            errorMsg = "Cannot open PCAP file";
            return false;
        }

        pcpp::RawPacket rawPacket;
        while (reader->getNextPacket(rawPacket)) {
            // Parse the raw packet
            RawPacketOfTool parsedPacket;
            if (parseRawPacketOfTool(&rawPacket, parsedPacket)) {
                m_capturedPackets.append(parsedPacket);
                emit packetLoaded(parsedPacket);
            }
        }
        qDebug()<< "loadFromPcap finished," << "total packets loaded:" << m_capturedPackets.size();
        reader->close();
        return true;

    } catch (const std::exception& e) {
        errorMsg = QString("Error loading PCAP: %1").arg(e.what());
        return false;
    }
}

bool PacketCaptureEngine::isCapturing() const
{
    return m_isCapturing;
}

void PacketCaptureEngine::onPacketArrived(pcpp::RawPacket* rawPacket)
{
    RawPacketOfTool parsedPacket;
    if (parseRawPacketOfTool(rawPacket, parsedPacket)) {
        // Add to captured packets list
        {
            QMutexLocker locker(&m_packetsMutex);
            m_capturedPackets.append(parsedPacket);
        }

        // Emit signal
        emit packetCaptured(parsedPacket);
    }
}

bool PacketCaptureEngine::parseRawPacketOfTool(pcpp::RawPacket* rawPacket, RawPacketOfTool& result)
{
    try {
        // Parse the packet
        pcpp::Packet parsedPacket(rawPacket);

        // Get timestamp
        result.timestamp = rawPacket->getPacketTimeStamp().tv_sec * 1000000LL +
                          rawPacket->getPacketTimeStamp().tv_nsec/1000; // Convert to microseconds

        // Get packet data
        result.data = QByteArray(reinterpret_cast<const char*>(rawPacket->getRawData()),
                                rawPacket->getRawDataLen());
        result.length = rawPacket->getRawDataLen();

        // Initialize with default values
        result.sourceIP = "0.0.0.0";
        result.destIP = "0.0.0.0";
        result.sourcePort = 0;
        result.destPort = 0;
        result.protocol = TransportProtocol::UDP; // Default

        // Parse Ethernet layer
        pcpp::EthLayer* ethLayer = parsedPacket.getLayerOfType<pcpp::EthLayer>();
        if (!ethLayer) {
            // Check if it's SLL (Linux Cooked Capture)
            if (rawPacket->getLinkLayerType() != pcpp::LINKTYPE_LINUX_SLL) {
                return false; // Not an Ethernet or SLL packet
            }
        }

        // Parse IP layer
        pcpp::IPv4Layer* ipLayer = parsedPacket.getLayerOfType<pcpp::IPv4Layer>();
        if (!ipLayer) {
            return false; // Not an IPv4 packet
        }

        result.sourceIP = QString::fromStdString(ipLayer->getSrcIPAddress().toString());
        result.destIP = QString::fromStdString(ipLayer->getDstIPAddress().toString());

        // Parse transport layer
        pcpp::TcpLayer* tcpLayer = parsedPacket.getLayerOfType<pcpp::TcpLayer>();
        pcpp::UdpLayer* udpLayer = parsedPacket.getLayerOfType<pcpp::UdpLayer>();

        if (tcpLayer) {
            result.protocol = TransportProtocol::TCP;
            result.sourcePort = tcpLayer->getSrcPort();
            result.destPort = tcpLayer->getDstPort();
        } else if (udpLayer) {
            result.protocol = TransportProtocol::UDP;
            result.sourcePort = udpLayer->getSrcPort();
            result.destPort = udpLayer->getDstPort();
        } else {
            // Not TCP or UDP
            return false;
        }

        return true;

    } catch (const std::exception& e) {
        qWarning() << "Error parsing packet:" << e.what();
        return false;
    }
}