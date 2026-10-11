#ifndef CAPTURECONTROLLER_H
#define CAPTURECONTROLLER_H

#include <QObject>
#include <QVariantList>
#include <memory>
#include <QTimer>
#include <QSettings>
#include "DataTypes.h"
#include "PacketCaptureEngine.h"
#include "FilterEngine.h"
#include "ProtocolParser.h"
#include "ConfigurationLoader.h"
#include "PacketModel.h"

class CaptureController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool tcpAnalysisBusy READ tcpAnalysisBusy NOTIFY tcpAnalysisChanged)
    Q_PROPERTY(QVariantMap tcpAnalysis READ tcpAnalysis NOTIFY tcpAnalysisChanged)
    Q_PROPERTY(bool hasUnsavedChanges READ hasUnsavedChanges NOTIFY bufferChanged)
    Q_PROPERTY(QVariantMap captureHealth READ captureHealth NOTIFY bufferChanged)
    Q_PROPERTY(bool archiveEnabled READ archiveEnabled NOTIFY bufferChanged)
    Q_PROPERTY(QVariantMap preferences READ preferences NOTIFY preferencesChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(qint64 loadedPackets READ loadedPackets NOTIFY loadingChanged)
    Q_PROPERTY(bool displayPaused READ displayPaused WRITE setDisplayPaused NOTIFY displayPausedChanged)
    Q_PROPERTY(int storedPackets READ storedPackets NOTIFY bufferChanged)
    Q_PROPERTY(double storedMiB READ storedMiB NOTIFY bufferChanged)
    Q_PROPERTY(int capacityMiB READ capacityMiB NOTIFY bufferChanged)
    Q_PROPERTY(bool bufferFull READ bufferFull NOTIFY bufferChanged)
    Q_PROPERTY(qint64 rejectedPackets READ rejectedPackets NOTIFY bufferChanged)
    Q_PROPERTY(bool isCapturing READ isCapturing NOTIFY capturingChanged)
    Q_PROPERTY(int totalPackets READ totalPackets NOTIFY statisticsChanged)
    Q_PROPERTY(int udpPackets READ udpPackets NOTIFY statisticsChanged)
    Q_PROPERTY(int tcpPackets READ tcpPackets NOTIFY statisticsChanged)
    Q_PROPERTY(int matchedPackets READ matchedPackets NOTIFY statisticsChanged)
    Q_PROPERTY(int parsedPackets READ parsedPackets NOTIFY statisticsChanged)
    Q_PROPERTY(int failedPackets READ failedPackets NOTIFY statisticsChanged)
    Q_PROPERTY(int unparsedPackets READ unparsedPackets NOTIFY statisticsChanged)
    Q_PROPERTY(int unmatchedPackets READ unmatchedPackets NOTIFY statisticsChanged)
    Q_PROPERTY(int visiblePackets READ visiblePackets NOTIFY statisticsChanged)
    Q_PROPERTY(bool configLoaded READ isConfigLoaded NOTIFY configStateChanged)
    Q_PROPERTY(QString configFileName READ configFileName NOTIFY configStateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)

public:
    explicit CaptureController(QObject *parent = nullptr);
    ~CaptureController();

    Q_INVOKABLE QVariantList getInterfaces();
    Q_INVOKABLE QString protocolDraft() const;
    Q_INVOKABLE QVariantMap readProtocolDraft(const QString& path);
    Q_INVOKABLE QVariantMap previewProtocolDraft(const QString& json, const QString& hex);
    Q_INVOKABLE bool applyProtocolDraft(const QString& json);
    Q_INVOKABLE bool saveProtocolDraft(const QString& json, const QString& path);
    Q_INVOKABLE bool analyzeTcpStreams();
    Q_INVOKABLE void cancelTcpAnalysis();
    bool tcpAnalysisBusy() const { return m_tcpThread != nullptr; }
    QVariantMap tcpAnalysis() const { return m_tcpAnalysis; }
    Q_INVOKABLE bool loadProtocolConfig(const QString& filePath);
    Q_INVOKABLE bool startCapture(const QString& interfaceName);
    Q_INVOKABLE void stopCapture();
    Q_INVOKABLE void setTransportFilter(int protocol) const;
    Q_INVOKABLE bool savePackets(const QString& filePath);
    Q_INVOKABLE bool loadPackets(const QString& filePath);
    Q_INVOKABLE void clearPackets();
    Q_INVOKABLE bool loadPacketsAsync(const QString& filePath);
    Q_INVOKABLE void cancelLoad();
    Q_INVOKABLE bool configureArchive(bool enabled, const QString& directory, int fileMiB = 64, int seconds = 300, int maxFiles = 1000);
    Q_INVOKABLE void setPreference(const QString& key, const QVariant& value);
    Q_INVOKABLE void flushPreferences();
    QVariantMap preferences() const;
    void setSettingsFile(const QString& path);
    QVariantMap captureHealth() const;
    bool hasUnsavedChanges() const { return m_captureEngine->hasUnsavedPackets(); }
    bool archiveEnabled() const { return m_captureEngine->archiveEnabled(); }
    Q_INVOKABLE bool setBufferLimits(int packets, int megabytes);
    void setDisplayPaused(bool paused);
    bool displayPaused() const { return m_displayPaused; }
    bool isLoading() const { return bool(m_loadJob); }
    qint64 loadedPackets() const;
    int storedPackets() const { return m_buffer.packets; }
    double storedMiB() const { return m_buffer.rawBytes / 1048576.0; }
    int capacityMiB() const { return m_buffer.limits.maxRawBytes / 1048576; }
    bool bufferFull() const { return m_buffer.full; }
    qint64 rejectedPackets() const { return m_buffer.rejectedPackets; }

    bool isCapturing() const { return m_isCapturing; }
    int totalPackets() const { return m_totalPackets; }
    int udpPackets() const { return m_udpPackets; }
    int tcpPackets() const { return m_tcpPackets; }
    int matchedPackets() const { return m_matchedPackets; }
    int parsedPackets() const { return m_statusCounts[2]; }
    int failedPackets() const { return m_statusCounts[3]; }
    int unparsedPackets() const { return m_statusCounts[0]; }
    int unmatchedPackets() const { return m_statusCounts[1]; }
    int visiblePackets() const { return m_displayModel->rowCount(); }
    bool isConfigLoaded() const { return m_configLoaded; }
    QString configFileName() const { return m_configFileName; }
    QString lastError() const { return m_lastError; }

    PacketModel* getPacketModel() { return m_packetModel; }
    PacketDisplayModel* getDisplayModel() { return m_displayModel; }

signals:
    void tcpAnalysisChanged();
    void preferencesChanged();
    void loadingChanged();
    void fileLoadFinished(bool success, bool canceled);
    void displayPausedChanged();
    void bufferChanged();
    void capturingChanged();
    void statisticsChanged();
    void configStateChanged();
    void errorChanged();
    void errorOccurred(const QString& errorMsg);
    void protocolConfigLoaded(const QString& protocolName);

private slots:
    void onPacketCaptured(const RawPacketOfTool& packet) const;
    void onPacketFiltered(const RawPacketOfTool& packet, bool matchesProtocol);

private:
    bool activateProtocol(const ProtocolConfiguration& config, const QString& label);
    void invalidateTcpAnalysis();
    QThread* m_tcpThread = nullptr;
    std::shared_ptr<std::atomic_bool> m_tcpCancel;
    QVariantMap m_tcpAnalysis;
    quint64 m_tcpGeneration = 0;
    void resetSession();
    bool reportError(const QString& error);
    bool requireIdle();
    void appendPackets(const QList<RawPacketOfTool>& packets);
    void updateBuffer();
    struct FileLoadJob;
    std::shared_ptr<FileLoadJob> m_loadJob;
    QThread* m_loadThread = nullptr;
    QTimer* m_flushTimer;
    QTimer* m_loadTimer;
    ProtocolConfiguration m_currentConfig;
    CaptureBufferState m_buffer;
    bool m_displayPaused = false;
    std::unique_ptr<QSettings> m_settings;
    QString m_archiveError;
    QVariantMap m_health;
    PacketCaptureEngine* m_captureEngine;
    FilterEngine* m_filterEngine;
    ProtocolParser* m_parser;
    ConfigurationLoader* m_configLoader;
    PacketModel* m_packetModel;
    PacketDisplayModel* m_displayModel;

    bool m_isCapturing;
    bool m_configLoaded;
    int m_totalPackets;
    int m_udpPackets;
    int m_tcpPackets;
    int m_matchedPackets;
    int m_statusCounts[4] = {};
    QString m_configFileName;
    QString m_lastError;
};

#endif // CAPTURECONTROLLER_H
