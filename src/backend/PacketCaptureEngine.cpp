#include <QCoreApplication>
#include "PacketCaptureEngine.h"
#include "PcapLiveDevice.h"
#include "PcapLiveDeviceList.h"
#include "PcapFileDevice.h"
#include "EthLayer.h"
#include "IPv4Layer.h"
#include "IPv6Layer.h"
#include "ArpLayer.h"
#include "SystemUtils.h"
#include "TcpLayer.h"
#include "UdpLayer.h"
#include "Packet.h"
#include <QDateTime>
#include <QSaveFile>
#include <QDataStream>
#include <memory>

PacketCaptureEngine::PacketCaptureEngine(QObject *parent)
    : QObject(parent),
      m_captureDevice(nullptr),
      m_isCapturing(false)
{
}

PacketCaptureEngine::~PacketCaptureEngine()
{
    stopCapture();

}

QList<NetworkInterface> PacketCaptureEngine::getAvailableInterfaces()
{
    QList<NetworkInterface> interfaces;
    m_devTable.clear();
    try {
        std::vector<pcpp::PcapLiveDevice*> devList = pcpp::PcapLiveDeviceList::getInstance().getPcapLiveDevicesList();
        for (auto* dev : devList) {
            if (!dev || dev->getName().empty()) {
                continue;
            }
            NetworkInterface iface;
            iface.name = QString::fromStdString(dev->getName());
            iface.description = QString::fromStdString(dev->getDesc());
            if (!dev->getIPAddresses().empty()) {
                for (const auto& addr : dev->getIPAddresses()) {
                    if (addr.getType() == pcpp::IPAddress::IPv4AddressType) {
                        iface.ipAddress = QString::fromStdString(addr.toString());
                        break;
                    }
                }
            }
            interfaces.append(iface);
            m_devTable.insert(std::pair(iface.name, dev));
        }
    } catch (const std::exception& e) {
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
    if (bufferState().full && !m_archiveEnabled) {
        errorMsg = QCoreApplication::translate("Backend", "内存窗口已满；请保存并清空或提高容量");
        return false;
    }
    stopCapture();

    try {
        pcpp::PcapLiveDevice* dev = nullptr;
        if (m_devTable.find(interfaceName) != m_devTable.end()) {
            dev = m_devTable[interfaceName];
        };

        if (dev == nullptr) {
            errorMsg = QCoreApplication::translate("Backend", "找不到网卡：%1").arg(interfaceName);
            return false;
        }

        if (!dev->open()) {
            errorMsg = QCoreApplication::translate("Backend", "无法打开网卡 %1，请检查 Npcap 驱动及权限").arg(interfaceName);
            return false;
        }

        if (m_archiveEnabled && !m_archive.state().active) {
            m_archiveFailureReported = false;
            if (!m_archive.start(m_archiveOptions, errorMsg)) { dev->close(); return false; }
            for (const auto& packet : getCapturedPackets()) {
                if (packet.sequence <= m_savedThroughSequence) continue;
                if (!m_archive.enqueue(packet)) {
                    errorMsg = m_archive.state().error;
                    m_archive.stop();
                    m_savedThroughSequence = qMax(m_savedThroughSequence, m_archive.state().writtenSequence);
                    dev->close();
                    return false;
                }
            }
        }
        m_captureDevice = dev;
        // Start capturing (non-blocking)
        if (!dev->startCapture(onPacketArrivesStatic, this)) {
            dev->close();
            m_captureDevice = nullptr;
            errorMsg = QCoreApplication::translate("Backend", "无法启动网卡采集，请检查 Npcap 驱动");
            return false;
        }
        m_isCapturing = true;

        return true;

    } catch (const std::exception& e) {
        errorMsg = QCoreApplication::translate("Backend", "启动采集失败：%1").arg(e.what());
        return false;
    }
}

void PacketCaptureEngine::stopCapture()
{
    if (m_captureDevice && m_isCapturing) {
        try {
            m_captureDevice->stopCapture();
            pcpp::IPcapDevice::PcapStats stats{};
            m_captureDevice->getStatistics(stats);
            m_driverReceived += stats.packetsRecv;
            m_driverDropped += stats.packetsDrop;
            m_interfaceDropped += stats.packetsDropByInterface;
            m_driverStatsAvailable = true;
            m_captureDevice->close();
            m_captureDevice = nullptr;
        } catch (const std::exception& e) {
            qWarning() << "Error stopping capture:" << e.what();
        }
    }

    m_isCapturing = false;
    m_archive.stop();
    QMutexLocker lock(&m_packetsMutex);
    m_savedThroughSequence = qMax(m_savedThroughSequence, m_archive.state().writtenSequence);
}

bool PacketCaptureEngine::saveToPcap(const QString& filePath, QString& errorMsg)
{
    const auto packets = getCapturedPackets();
    if (packets.isEmpty()) { errorMsg = QCoreApplication::translate("Backend", "当前窗口没有可保存的报文"); return false; }
    const auto linkType = packets.first().linkLayerType;
    for (const auto& packet : packets) {
        if (packet.linkLayerType != linkType) {
            errorMsg = QCoreApplication::translate("Backend", "会话包含多个链路类型，无法保存为单一 PCAP");
            return false;
        }
    }
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) { errorMsg = QCoreApplication::translate("Backend", "无法写入保存文件：") + file.errorString(); return false; }
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << quint32(0xa1b2c3d4) << quint16(2) << quint16(4) << quint32(0) << quint32(0)
        << quint32(0xffffffff) << quint32(linkType);
    for (const auto& packet : packets) {
        stream << quint32(packet.timestamp / 1000000) << quint32(packet.timestamp % 1000000)
            << quint32(packet.data.size()) << quint32(packet.data.size());
        if (stream.status() != QDataStream::Ok || file.write(packet.data) != packet.data.size()) {
            errorMsg = QCoreApplication::translate("Backend", "报文保存失败：") + file.errorString();
            return false;
        }
    }
    if (!file.commit()) { errorMsg = QCoreApplication::translate("Backend", "保存文件提交失败：") + file.errorString(); return false; }
    QMutexLocker lock(&m_packetsMutex);
    if (packets.first().sessionId == m_sessionId)
        m_savedThroughSequence = qMax(m_savedThroughSequence, packets.last().sequence);
    return true;
}

CaptureFileReadResult PacketCaptureEngine::readCaptureFile(const QString& filePath,
    CaptureBufferLimits limits, const std::atomic_bool* cancel,
    const std::function<void(qint64, qint64)>& progress)
{
    CaptureFileReadResult result;
    auto canceled = [&] { return cancel && cancel->load(); };
    try {
        if (canceled()) { result.canceled = true; return result; }
        std::string filePathStr = filePath.toStdString();
        std::unique_ptr<pcpp::IFileReaderDevice> reader(pcpp::IFileReaderDevice::getReader(filePathStr));

        if (reader == nullptr || !reader->open()) {
            result.error = QCoreApplication::translate("Backend", "无法打开抓包文件");
            return result;
        }

        pcpp::RawPacket rawPacket;
        while (reader->getNextPacket(rawPacket)) {
            if (canceled()) { result.canceled = true; result.packets.clear(); return result; }
            RawPacketOfTool parsedPacket;
            if (parseRawPacketOfTool(&rawPacket, parsedPacket)) {
                if (result.packets.size() >= limits.maxPackets ||
                    parsedPacket.data.size() > limits.maxRawBytes - result.rawBytes) {
                    result.error = QCoreApplication::translate("Backend", "文件超过窗口容量；请提高容量后重试");
                    result.packets.clear();
                    return result;
                }
                result.rawBytes += parsedPacket.data.size();
                result.packets.append(std::move(parsedPacket));
                if (progress && result.packets.size() % 128 == 0)
                    progress(result.packets.size(), result.rawBytes);
            }
        }
        reader->close();
        if (progress) progress(result.packets.size(), result.rawBytes);
        result.canceled = canceled();
        result.success = !result.canceled;
        if (result.canceled) result.packets.clear();

    } catch (const std::exception& e) {
        result.error = QCoreApplication::translate("Backend", "读取抓包文件失败：%1").arg(e.what());
        result.packets.clear();
    }
    return result;
}

bool PacketCaptureEngine::replacePackets(QList<RawPacketOfTool> packets, QString& errorMsg)
{
    qint64 bytes = 0;
    for (const auto& packet : packets) bytes += packet.data.size();
    const auto limits = bufferState().limits;
    if (packets.size() > limits.maxPackets || bytes > limits.maxRawBytes) {
        errorMsg = QCoreApplication::translate("Backend", "报文超过窗口容量");
        return false;
    }
    stopCapture();
    m_archive.reset();
    {
        QMutexLocker locker(&m_packetsMutex);
        ++m_sessionId;
        m_archiveFailureReported = false;
        m_driverReceived = m_driverDropped = m_interfaceDropped = 0;
        m_driverStatsAvailable = false;
        m_nextSequence = 1;
        for (auto& packet : packets) { packet.sessionId = m_sessionId; packet.sequence = m_nextSequence++; }
        m_savedThroughSequence = m_nextSequence - 1;
        m_capturedPackets = std::move(packets);
        m_pendingPackets.clear();
        m_buffer = {int(m_capturedPackets.size()), bytes, 0,
            m_capturedPackets.size() >= limits.maxPackets || bytes >= limits.maxRawBytes, limits};
        m_buffer.firstSequence = m_capturedPackets.isEmpty() ? 0 : m_capturedPackets.first().sequence;
    }
    emit sessionReset();
    return true;
}

bool PacketCaptureEngine::configureArchive(bool enabled, const ArchiveOptions& options, QString& error)
{
    if (isCapturing()) { error = QCoreApplication::translate("Backend", "请先停止采集再修改归档设置"); return false; }
    if (enabled && !getCapturedPackets().isEmpty()) { error = QCoreApplication::translate("Backend", "请保存并清空当前会话后启用自动落盘"); return false; }
    if (enabled && !m_archive.start(options, error)) return false;
    if (!enabled) m_archive.stop();
    m_archiveFailureReported = false;
    m_archiveOptions = options;
    m_archiveEnabled = enabled;
    return true;
}

bool PacketCaptureEngine::hasUnsavedPackets() const
{
    QMutexLocker lock(&m_packetsMutex);
    return m_nextSequence - 1 > qMax(m_savedThroughSequence, m_archive.state().writtenSequence);
}

QVariantMap PacketCaptureEngine::driverStatistics() const
{
    quint64 received = m_driverReceived, dropped = m_driverDropped, interfaceDropped = m_interfaceDropped;
    if (m_captureDevice && m_isCapturing) {
        pcpp::IPcapDevice::PcapStats stats{};
        m_captureDevice->getStatistics(stats);
        received += stats.packetsRecv;
        dropped += stats.packetsDrop;
        interfaceDropped += stats.packetsDropByInterface;
    }
    return {{"available", m_driverStatsAvailable || m_isCapturing}, {"received", QVariant::fromValue(received)},
        {"dropped", QVariant::fromValue(dropped)}, {"interfaceDropped", QVariant::fromValue(interfaceDropped)}};
}

bool PacketCaptureEngine::loadFromPcap(const QString& filePath, QString& errorMsg)
{
    auto result = readCaptureFile(filePath, bufferState().limits);
    if (!result.success) { errorMsg = result.error; return false; }
    if (!replacePackets(std::move(result.packets), errorMsg)) return false;
    emit packetsLoaded(getCapturedPackets());
    return true;
}

CaptureBufferState PacketCaptureEngine::bufferState() const
{
    QMutexLocker locker(&m_packetsMutex);
    return m_buffer;
}

bool PacketCaptureEngine::setBufferLimits(CaptureBufferLimits limits, QString& errorMsg)
{
    QMutexLocker locker(&m_packetsMutex);
    if (limits.maxPackets <= 0 || limits.maxRawBytes <= 0 ||
        limits.maxPackets < m_buffer.packets || limits.maxRawBytes < m_buffer.rawBytes) {
        errorMsg = QCoreApplication::translate("Backend", "容量必须为正值，且能容纳当前会话");
        return false;
    }
    m_buffer.limits = limits;
    m_buffer.full = m_buffer.packets >= limits.maxPackets || m_buffer.rawBytes >= limits.maxRawBytes;
    return true;
}

QList<RawPacketOfTool> PacketCaptureEngine::takePendingPackets(int maximum)
{
    QMutexLocker locker(&m_packetsMutex);
    QList<RawPacketOfTool> packets;
    while (maximum-- > 0 && !m_pendingPackets.empty()) {
        if (m_pendingPackets.front().sequence >= m_buffer.firstSequence)
            packets.append(std::move(m_pendingPackets.front()));
        m_pendingPackets.pop_front();
    }
    return packets;
}

bool PacketCaptureEngine::isCapturing() const
{
    return m_isCapturing;
}

QList<RawPacketOfTool> PacketCaptureEngine::getCapturedPackets() const
{
    QMutexLocker locker(&m_packetsMutex);
    return m_capturedPackets;
}

quint64 PacketCaptureEngine::sessionId() const
{
    QMutexLocker locker(&m_packetsMutex);
    return m_sessionId;
}

void PacketCaptureEngine::clearPackets()
{
    stopCapture();
    m_archive.reset();
    {
        QMutexLocker locker(&m_packetsMutex);
        m_capturedPackets.clear();
        m_pendingPackets.clear();
        m_buffer = {0, 0, 0, false, m_buffer.limits};
        ++m_sessionId;
        m_nextSequence = 1;
        m_savedThroughSequence = 0;
        m_archiveFailureReported = false;
        m_driverReceived = m_driverDropped = m_interfaceDropped = 0;
        m_driverStatsAvailable = false;
    }
    emit sessionReset();
}

void PacketCaptureEngine::onPacketArrived(pcpp::RawPacket* rawPacket)
{
    const quint64 captureSession = sessionId();
    RawPacketOfTool packet;
    if (!parseRawPacketOfTool(rawPacket, packet)) {
        QMutexLocker lock(&m_packetsMutex);
        if (captureSession == m_sessionId) ++m_buffer.invalidPackets;
        return;
    }
    bool reached = false, accepted = false, reportArchiveFailure = false;
    QString archiveError;
    {
        QMutexLocker lock(&m_packetsMutex);
        if (captureSession != m_sessionId) return;
        const auto archive = m_archive.state();
        // Only already flushed packets can leave the live window.
        while (m_archiveEnabled && archive.error.isEmpty() && !m_capturedPackets.isEmpty() &&
            (m_buffer.packets >= m_buffer.limits.maxPackets || packet.data.size() > m_buffer.limits.maxRawBytes - m_buffer.rawBytes) &&
            m_capturedPackets.first().sequence <= qMax(archive.writtenSequence, m_savedThroughSequence)) {
            m_buffer.rawBytes -= m_capturedPackets.first().data.size();
            m_capturedPackets.removeFirst();
            --m_buffer.packets;
        }
        while (!m_pendingPackets.empty() && !m_capturedPackets.isEmpty() &&
            m_pendingPackets.front().sequence < m_capturedPackets.first().sequence) m_pendingPackets.pop_front();
        if (m_buffer.packets >= m_buffer.limits.maxPackets || packet.data.size() > m_buffer.limits.maxRawBytes - m_buffer.rawBytes) {
            reached = !m_buffer.full;
            m_buffer.full = true;
            ++m_buffer.rejectedPackets;
        } else {
            packet.sessionId = m_sessionId;
            packet.sequence = m_nextSequence;
            if (m_archiveEnabled && !m_archive.enqueue(packet)) {
                archiveError = m_archive.state().error;
                if (archiveError.isEmpty()) archiveError = QCoreApplication::translate("Backend", "归档未运行，采集已停止");
                ++m_buffer.rejectedPackets;
                reportArchiveFailure = !m_archiveFailureReported;
                m_archiveFailureReported = true;
            } else {
                ++m_nextSequence;
                ++m_buffer.acceptedPackets;
                m_capturedPackets.append(packet);
                m_pendingPackets.push_back(packet);
                ++m_buffer.packets;
                m_buffer.rawBytes += packet.data.size();
                m_buffer.firstSequence = m_capturedPackets.first().sequence;
                accepted = true;
                reached = !m_archiveEnabled && (m_buffer.packets >= m_buffer.limits.maxPackets || m_buffer.rawBytes >= m_buffer.limits.maxRawBytes);
                m_buffer.full = reached;
            }
        }
    }
    if (accepted) emit packetCaptured(packet);
    if (reached) emit bufferLimitReached(captureSession);
    if (reportArchiveFailure) emit archiveFailed(captureSession, archiveError);
}

bool PacketCaptureEngine::parseRawPacketOfTool(pcpp::RawPacket* rawPacket, RawPacketOfTool& result)
{
    try {
        pcpp::Packet parsedPacket(rawPacket);

        result.timestamp = rawPacket->getPacketTimeStamp().tv_sec * 1000000LL +
                          rawPacket->getPacketTimeStamp().tv_nsec/1000;
        result.data = QByteArray(reinterpret_cast<const char*>(rawPacket->getRawData()),
                                rawPacket->getRawDataLen());
        result.length = rawPacket->getRawDataLen();
        result.linkLayerType = static_cast<int>(rawPacket->getLinkLayerType());
        result.headerLength = 0;
        result.sourceIP = "0.0.0.0";
        result.destIP = "0.0.0.0";
        result.sourcePort = 0;
        result.destPort = 0;
        result.protocol = TransportProtocol::Other;
        result.protocolName = "Ethernet";

        if (!parsedPacket.getFirstLayer()) return false;

        pcpp::IPv4Layer* ipLayer = parsedPacket.getLayerOfType<pcpp::IPv4Layer>();
        auto* ipv6Layer = parsedPacket.getLayerOfType<pcpp::IPv6Layer>();
        auto* arpLayer = parsedPacket.getLayerOfType<pcpp::ArpLayer>();
        if (ipLayer) {
            result.sourceIP = QString::fromStdString(ipLayer->getSrcIPAddress().toString());
            result.destIP = QString::fromStdString(ipLayer->getDstIPAddress().toString());
            result.protocolName = "IPv4";
        } else if (ipv6Layer) {
            result.sourceIP = QString::fromStdString(ipv6Layer->getSrcIPAddress().toString());
            result.destIP = QString::fromStdString(ipv6Layer->getDstIPAddress().toString());
            result.protocolName = "IPv6";
        } else if (arpLayer) {
            result.sourceIP = QString::fromStdString(arpLayer->getSenderIpAddr().toString());
            result.destIP = QString::fromStdString(arpLayer->getTargetIpAddr().toString());
            result.protocolName = "ARP";
        }

        pcpp::TcpLayer* tcpLayer = parsedPacket.getLayerOfType<pcpp::TcpLayer>();
        pcpp::UdpLayer* udpLayer = parsedPacket.getLayerOfType<pcpp::UdpLayer>();

        if (tcpLayer) {
            result.protocol = TransportProtocol::TCP;
            result.protocolName = ipv6Layer ? "TCP/IPv6" : "TCP";
            result.sourcePort = tcpLayer->getSrcPort();
            result.destPort = tcpLayer->getDstPort();
        } else if (udpLayer) {
            result.protocol = TransportProtocol::UDP;
            result.protocolName = ipv6Layer ? "UDP/IPv6" : "UDP";
            result.sourcePort = udpLayer->getSrcPort();
            result.destPort = udpLayer->getDstPort();
        } else if (parsedPacket.isPacketOfType(pcpp::ICMPv6)) {
            result.protocolName = "ICMPv6";
        } else if (parsedPacket.isPacketOfType(pcpp::ICMP)) {
            result.protocolName = "ICMP";
        }

        // Calculate total header length (Eth + IP + Transport)
        quint32 headerLen = 0;
        pcpp::Layer* curLayer = parsedPacket.getFirstLayer();
        while (curLayer != nullptr) {
            if (curLayer == ipLayer || curLayer == ipv6Layer || curLayer == arpLayer)
                result.networkOffset = headerLen;
            if (curLayer == tcpLayer || curLayer == udpLayer)
                result.transportOffset = headerLen;
            // We sum headers until we reach the payload or a protocol we don't recognize as part of the "headers"
            // In our case, it's Eth -> IP -> TCP/UDP. The layer after TCP/UDP is usually the payload.
            headerLen += curLayer->getHeaderLen();
            if (curLayer->getProtocol() == pcpp::TCP || curLayer->getProtocol() == pcpp::UDP ||
                curLayer->getProtocol() == pcpp::ARP || curLayer->getProtocol() == pcpp::ICMP ||
                curLayer->getProtocol() == pcpp::ICMPv6) {
                break; // Stop after transport layer
            }
            curLayer = curLayer->getNextLayer();
        }
        result.headerLength = headerLen;
        if (tcpLayer || udpLayer) {
            int networkEnd = result.data.size();
            if (ipLayer) networkEnd = result.networkOffset + pcpp::netToHost16(ipLayer->getIPv4Header()->totalLength);
            else if (ipv6Layer) networkEnd = result.networkOffset + 40 + pcpp::netToHost16(ipv6Layer->getIPv6Header()->payloadLength);
            result.payloadLength = networkEnd - static_cast<int>(headerLen);
            if (udpLayer) result.payloadLength = pcpp::netToHost16(udpLayer->getUdpHeader()->length) - 8;
            if (result.payloadLength < 0) return false;
        }

        return true;

    } catch (const std::exception& e) {
        qWarning() << "Error parsing packet:" << e.what();
        return false;
    }
}
