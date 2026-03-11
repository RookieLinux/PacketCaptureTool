#ifndef PACKETCAPTUREENGINE_H
#define PACKETCAPTUREENGINE_H

#include <QObject>
#include <QString>
#include <QList>
#include <QByteArray>
#include <QThread>
#include <QMutex>
#include "DataTypes.h"

// Forward declarations for PcapPlusPlus
namespace pcpp {
    class PcapLiveDevice;
    class PcapFileWriterDevice;
    class PcapFileReaderDevice;
    class RawPacket;
}

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
    QList<RawPacketOfTool> getCapturedPackets() const;
    static void onPacketArrivesStatic(pcpp::RawPacket* rawPacket, [[__maybe_unused__]] pcpp::PcapLiveDevice* dev, void* userData);

signals:
    void packetCaptured(const RawPacketOfTool& packet);
    void captureError(const QString& errorMsg);
    void packetLoaded(const RawPacketOfTool& packet);

private slots:
    void onPacketArrived(pcpp::RawPacket* rawPacket);

private:
    bool parseRawPacketOfTool(pcpp::RawPacket* rawPacket, RawPacketOfTool& result);
    
    pcpp::PcapLiveDevice* m_captureDevice;
    pcpp::PcapFileWriterDevice* m_fileWriter;
    QList<RawPacketOfTool> m_capturedPackets;
    QMutex m_packetsMutex;
    bool m_isCapturing;
    QThread* m_captureThread;
};

#endif // PACKETCAPTUREENGINE_H
