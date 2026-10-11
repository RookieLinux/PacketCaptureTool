#ifndef PACKETCAPTUREENGINE_H
#define PACKETCAPTUREENGINE_H

#include <QObject>
#include <QString>
#include <QList>
#include <QByteArray>
#include <QThread>
#include <QVariantMap>
#include <QMutex>
#include <atomic>
#include <functional>
#include <deque>
#include "DataTypes.h"
#include "CaptureArchive.h"
#include "PcapLiveDevice.h"
#include "PcapFileDevice.h"
#include "RawPacket.h"

struct CaptureBufferLimits {
    int maxPackets = 50000;
    qint64 maxRawBytes = 64LL * 1024 * 1024;
};

struct CaptureBufferState {
    int packets = 0;
    qint64 rawBytes = 0;
    qint64 rejectedPackets = 0;
    bool full = false;
    CaptureBufferLimits limits;
    quint64 acceptedPackets = 0;
    quint64 invalidPackets = 0;
    quint64 firstSequence = 0;
};

struct CaptureFileReadResult {
    bool success = false;
    bool canceled = false;
    QString error;
    QList<RawPacketOfTool> packets;
    qint64 rawBytes = 0;
};

class PacketCaptureEngine : public QObject
{
    Q_OBJECT

public:
    explicit PacketCaptureEngine(QObject *parent = nullptr);
    ~PacketCaptureEngine();

    QList<NetworkInterface> getAvailableInterfaces();
    bool startCapture(const QString& interfaceName, QString& errorMsg);
    void stopCapture();
    bool saveToPcap(const QString& filePath, QString& errorMsg);
    bool loadFromPcap(const QString& filePath, QString& errorMsg);
    bool isCapturing() const;
    void clearPackets();
    quint64 sessionId() const;
    QList<RawPacketOfTool> getCapturedPackets() const;
    CaptureBufferState bufferState() const;
    bool configureArchive(bool enabled, const ArchiveOptions& options, QString& error);
    ArchiveState archiveState() const { return m_archive.state(); }
    bool archiveEnabled() const { return m_archiveEnabled; }
    void stopArchive() { m_archive.stop(); }
    bool hasUnsavedPackets() const;
    QVariantMap driverStatistics() const;
    bool setBufferLimits(CaptureBufferLimits limits, QString& errorMsg);
    QList<RawPacketOfTool> takePendingPackets(int maximum = 1024);
    bool replacePackets(QList<RawPacketOfTool> packets, QString& errorMsg);
    static CaptureFileReadResult readCaptureFile(const QString& filePath,
        CaptureBufferLimits limits = {}, const std::atomic_bool* cancel = nullptr,
        const std::function<void(qint64, qint64)>& progress = {});
    static void onPacketArrivesStatic(pcpp::RawPacket* rawPacket, [[__maybe_unused__]] pcpp::PcapLiveDevice* dev, void* userData);

signals:
    void packetCaptured(const RawPacketOfTool& packet);
    void captureError(const QString& errorMsg);
    void packetLoaded(const RawPacketOfTool& packet);
    void packetsLoaded(const QList<RawPacketOfTool>& packets);
    void bufferLimitReached(quint64 sessionId);
    void archiveFailed(quint64 sessionId, const QString& error);
    void sessionReset();

private slots:
    void onPacketArrived(pcpp::RawPacket* rawPacket);

private:
    static bool parseRawPacketOfTool(pcpp::RawPacket* rawPacket, RawPacketOfTool& result);
    
    pcpp::PcapLiveDevice* m_captureDevice;
    QList<RawPacketOfTool> m_capturedPackets;
    mutable QMutex m_packetsMutex;
    quint64 m_sessionId = 0;
    std::deque<RawPacketOfTool> m_pendingPackets;
    CaptureBufferState m_buffer;
    CaptureArchive m_archive;
    ArchiveOptions m_archiveOptions;
    bool m_archiveEnabled = false;
    bool m_archiveFailureReported = false;
    quint64 m_nextSequence = 1;
    quint64 m_savedThroughSequence = 0;
    quint64 m_driverReceived = 0;
    quint64 m_driverDropped = 0;
    quint64 m_interfaceDropped = 0;
    bool m_driverStatsAvailable = false;
    bool m_isCapturing;
    std::map<QString, pcpp::PcapLiveDevice*> m_devTable;
};

#endif // PACKETCAPTUREENGINE_H
