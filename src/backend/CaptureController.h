#ifndef CAPTURECONTROLLER_H
#define CAPTURECONTROLLER_H

#include <QObject>
#include <QVariantList>
#include "DataTypes.h"
#include "PacketCaptureEngine.h"
#include "FilterEngine.h"
#include "ProtocolParser.h"
#include "ConfigurationLoader.h"
#include "PacketModel.h"

class CaptureController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isCapturing READ isCapturing NOTIFY capturingChanged)
    Q_PROPERTY(int totalPackets READ totalPackets NOTIFY statisticsChanged)
    Q_PROPERTY(int udpPackets READ udpPackets NOTIFY statisticsChanged)
    Q_PROPERTY(int tcpPackets READ tcpPackets NOTIFY statisticsChanged)
    Q_PROPERTY(int matchedPackets READ matchedPackets NOTIFY statisticsChanged)

public:
    explicit CaptureController(QObject *parent = nullptr);
    ~CaptureController();

    Q_INVOKABLE QVariantList getInterfaces();
    Q_INVOKABLE bool loadProtocolConfig(const QString& filePath);
    Q_INVOKABLE bool startCapture(const QString& interfaceName);
    Q_INVOKABLE void stopCapture();
    Q_INVOKABLE void setTransportFilter(int protocol) const;
    Q_INVOKABLE bool savePackets(const QString& filePath);
    Q_INVOKABLE bool loadPackets(const QString& filePath);
    Q_INVOKABLE void clearPackets();

    bool isCapturing() const { return m_isCapturing; }
    int totalPackets() const { return m_totalPackets; }
    int udpPackets() const { return m_udpPackets; }
    int tcpPackets() const { return m_tcpPackets; }
    int matchedPackets() const { return m_matchedPackets; }

    PacketModel* getPacketModel() { return m_packetModel; }

signals:
    void capturingChanged();
    void statisticsChanged();
    void errorOccurred(const QString& errorMsg);
    void configLoaded(const QString& protocolName);

private slots:
    void onPacketCaptured(const RawPacketOfTool& packet) const;
    void onPacketFiltered(const RawPacketOfTool& packet, bool matchesProtocol);

private:
    PacketCaptureEngine* m_captureEngine;
    FilterEngine* m_filterEngine;
    ProtocolParser* m_parser;
    ConfigurationLoader* m_configLoader;
    PacketModel* m_packetModel;

    bool m_isCapturing;
    int m_totalPackets;
    int m_udpPackets;
    int m_tcpPackets;
    int m_matchedPackets;
};

#endif // CAPTURECONTROLLER_H
