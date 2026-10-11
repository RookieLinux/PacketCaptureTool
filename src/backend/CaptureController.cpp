#include <QCoreApplication>
#include "CaptureController.h"
#include "TcpStreamAnalyzer.h"
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>

struct CaptureController::FileLoadJob {
    std::atomic_bool cancel{false};
    std::atomic<qint64> count{0};
    CaptureFileReadResult result;
    QList<PacketModel::PacketData> decoded;
};


CaptureController::CaptureController(QObject *parent)
    : QObject(parent),
      m_isCapturing(false),
      m_configLoaded(false),
      m_totalPackets(0),
      m_udpPackets(0),
      m_tcpPackets(0),
      m_matchedPackets(0)
{
    const auto settingsFile = qEnvironmentVariable("PACKET_CAPTURE_SETTINGS_FILE");
    m_settings = settingsFile.isEmpty() ? std::make_unique<QSettings>() :
        std::make_unique<QSettings>(settingsFile, QSettings::IniFormat);
    m_captureEngine = new PacketCaptureEngine(this);
    m_filterEngine = new FilterEngine(this);
    m_parser = new ProtocolParser(this);
    m_configLoader = new ConfigurationLoader(this);
    m_packetModel = new PacketModel(this);
    m_displayModel = new PacketDisplayModel(this);
    m_displayModel->setSourceModel(m_packetModel);
    connect(m_displayModel, &QAbstractItemModel::rowsInserted, this, &CaptureController::statisticsChanged);
    connect(m_displayModel, &QAbstractItemModel::rowsRemoved, this, &CaptureController::statisticsChanged);
    connect(m_displayModel, &QAbstractItemModel::modelReset, this, &CaptureController::statisticsChanged);

    connect(m_captureEngine, &PacketCaptureEngine::archiveFailed, this, [this](quint64 session, const QString& error) {
        if (session != m_captureEngine->sessionId()) return;
        m_archiveError = error;
        stopCapture();
        reportError(error);
    }, Qt::QueuedConnection);
    // Connect signals
    connect(m_captureEngine, &PacketCaptureEngine::packetsLoaded,
            this, &CaptureController::appendPackets);
    connect(m_captureEngine, &PacketCaptureEngine::bufferLimitReached, this, [this](quint64 session) {
        if (session != m_captureEngine->sessionId() || !m_captureEngine->bufferState().full) return;
        stopCapture();
        updateBuffer();
        reportError(QCoreApplication::translate("Backend", "已达到窗口容量，采集停止；请保存并清空或提高容量"));
    }, Qt::QueuedConnection);
    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(50);
    connect(m_flushTimer, &QTimer::timeout, this, [this] {
        const auto first = m_captureEngine->bufferState().firstSequence;
        const auto removed = m_packetModel->trimBefore(first);
        for (const auto& entry : removed) {
            --m_totalPackets;
            --m_statusCounts[static_cast<int>(entry.parsed.status)];
            if (entry.raw.protocol == TransportProtocol::UDP) --m_udpPackets;
            if (entry.raw.protocol == TransportProtocol::TCP) --m_tcpPackets;
        }
        if (!removed.isEmpty()) { m_matchedPackets = parsedPackets() + failedPackets(); emit statisticsChanged(); }
        if (!m_displayPaused) appendPackets(m_captureEngine->takePendingPackets());
        updateBuffer();
    });
    m_flushTimer->start();
    m_loadTimer = new QTimer(this);
    m_loadTimer->setInterval(100);
    connect(m_loadTimer, &QTimer::timeout, this, &CaptureController::loadingChanged);
    connect(m_captureEngine, &PacketCaptureEngine::sessionReset,
            this, &CaptureController::resetSession);
    connect(m_filterEngine, &FilterEngine::packetFiltered,
            this, &CaptureController::onPacketFiltered);
}

CaptureController::~CaptureController()
{
    cancelTcpAnalysis();
    if (m_tcpThread) m_tcpThread->wait();
    if (m_loadJob) m_loadJob->cancel = true;
    if (m_loadThread) m_loadThread->wait();
    m_captureEngine->stopCapture();
    flushPreferences();
}

void CaptureController::cancelTcpAnalysis()
{
    if (m_tcpCancel) *m_tcpCancel = true;
}

void CaptureController::invalidateTcpAnalysis()
{
    cancelTcpAnalysis();
    ++m_tcpGeneration;
    m_tcpAnalysis.clear();
    emit tcpAnalysisChanged();
}

bool CaptureController::analyzeTcpStreams()
{
    if (tcpAnalysisBusy() || isLoading()) return reportError(QCoreApplication::translate("Backend", "请等待当前后台任务完成"));
    if (!m_configLoaded || m_currentConfig.transportType != TransportProtocol::TCP)
        return reportError(QCoreApplication::translate("Backend", "请先加载 TCP 协议配置，定义固定消息长度或消息长度字段"));
    auto packets = m_captureEngine->getCapturedPackets();
    auto config = m_currentConfig;
    const auto generation = ++m_tcpGeneration;
    m_tcpAnalysis.clear();
    m_tcpCancel = std::make_shared<std::atomic_bool>(false);
    auto cancel = m_tcpCancel;
    auto result = std::make_shared<QVariantMap>();
    m_tcpThread = QThread::create([packets,config,cancel,result] {
        *result = TcpStreamAnalyzer::analyze(packets,config,*cancel);
    });
    m_tcpThread->setParent(this);
    connect(m_tcpThread,&QThread::finished,this,[this,generation,result,cancel] {
        auto* finished = m_tcpThread;
        m_tcpThread = nullptr;
        m_tcpCancel.reset();
        if (generation == m_tcpGeneration && !*cancel) m_tcpAnalysis = *result;
        finished->deleteLater();
        emit tcpAnalysisChanged();
    });
    m_tcpThread->start();
    emit tcpAnalysisChanged();
    return true;
}

bool CaptureController::reportError(const QString& error)
{
    m_lastError = error;
    emit errorChanged();
    emit errorOccurred(error);
    return false;
}

bool CaptureController::requireIdle()
{
    return !isLoading() || reportError(QCoreApplication::translate("Backend", "请等待导入完成或先取消导入"));
}

qint64 CaptureController::loadedPackets() const
{
    return m_loadJob ? m_loadJob->count.load() : 0;
}

void CaptureController::cancelLoad()
{
    if (m_loadJob) m_loadJob->cancel = true;
}

void CaptureController::setDisplayPaused(bool paused)
{
    if (m_displayPaused == paused) return;
    m_displayPaused = paused;
    emit displayPausedChanged();
}

QVariantMap CaptureController::preferences() const
{
    QVariantMap result;
    for (const auto& key : m_settings->allKeys()) result[key] = m_settings->value(key);
    return result;
}

void CaptureController::setSettingsFile(const QString& path)
{
    m_settings->sync();
    m_settings = std::make_unique<QSettings>(path, QSettings::IniFormat);
    emit preferencesChanged();
}

void CaptureController::setPreference(const QString& key, const QVariant& value)
{
    const QStringList allowed{"windowWidth", "windowHeight", "splitWidth", "interface", "compact", "follow", "tableMode", "columns",
        "payloadOnly", "dark", "capacityIndex", "archiveDirectory", "fileMiB", "fileSeconds", "maxFiles", "closeToTray"};
    if (!allowed.contains(key) || m_settings->value(key) == value) return;
    m_settings->setValue(key, value);
    emit preferencesChanged();
}

void CaptureController::flushPreferences() { m_settings->sync(); }

QVariantMap CaptureController::captureHealth() const
{
    auto result = m_captureEngine->driverStatistics();
    const auto buffer = m_captureEngine->bufferState();
    const auto archive = m_captureEngine->archiveState();
    result["accepted"] = QVariant::fromValue(buffer.acceptedPackets);
    result["invalid"] = QVariant::fromValue(buffer.invalidPackets);
    result["written"] = archive.writtenPackets;
    result["pending"] = archive.pendingPackets;
    result["files"] = archive.files;
    result["archiveError"] = archive.error;
    result["unsaved"] = hasUnsavedChanges();
    return result;
}

bool CaptureController::configureArchive(bool enabled, const QString& directory, int fileMiB, int seconds, int maxFiles)
{
    if (!requireIdle()) return false;
    if (fileMiB < 1 || fileMiB > 1024 || seconds < 1 || seconds > 86400 || maxFiles < 1 || maxFiles > 10000)
        return reportError(QCoreApplication::translate("Backend", "轮转范围：1–1024 MiB、1–86400 秒、1–10000 个文件"));
    QString error;
    if (!m_captureEngine->configureArchive(enabled, {directory, qint64(fileMiB) * 1048576, seconds, maxFiles}, error)) return reportError(error);
    m_archiveError.clear();
    setPreference("archiveDirectory", directory);
    setPreference("fileMiB", fileMiB);
    setPreference("fileSeconds", seconds);
    setPreference("maxFiles", maxFiles);
    updateBuffer();
    emit bufferChanged();
    return true;
}

void CaptureController::updateBuffer()
{
    const auto state = m_captureEngine->bufferState();
    const auto health = captureHealth();
    if (!health.value("archiveError").toString().isEmpty() && health.value("archiveError").toString() != m_archiveError) {
        m_archiveError = health.value("archiveError").toString();
        stopCapture();
        reportError(m_archiveError);
    }
    if (state.packets == m_buffer.packets && state.rawBytes == m_buffer.rawBytes &&
        state.rejectedPackets == m_buffer.rejectedPackets && state.full == m_buffer.full &&
        state.limits.maxPackets == m_buffer.limits.maxPackets && state.limits.maxRawBytes == m_buffer.limits.maxRawBytes && health == m_health) return;
    m_buffer = state;
    m_health = health;
    emit bufferChanged();
}

bool CaptureController::setBufferLimits(int packets, int megabytes)
{
    if (!requireIdle()) return false;
    if (isCapturing()) return reportError(QCoreApplication::translate("Backend", "请先停止采集再调整容量"));
    if (packets < 1 || packets > 1000000 || megabytes < 1 || megabytes > 512)
        return reportError(QCoreApplication::translate("Backend", "容量设置超出支持范围"));
    QString error;
    if (!m_captureEngine->setBufferLimits({packets, qint64(megabytes) * 1048576}, error)) return reportError(error);
    updateBuffer();
    return true;
}

bool CaptureController::loadPacketsAsync(const QString& filePath)
{
    if (!requireIdle()) return false;
    auto job = std::make_shared<FileLoadJob>();
    m_loadJob = job;
    const auto limits = m_captureEngine->bufferState().limits;
    const auto config = m_currentConfig;
    const bool hasConfig = m_configLoaded;
    m_loadThread = QThread::create([job, filePath, limits, config, hasConfig] {
        job->result = PacketCaptureEngine::readCaptureFile(filePath, limits, &job->cancel,
            [job](qint64 packets, qint64) { job->count = packets; });
        if (!job->result.success) return;
        ProtocolParser parser;
        if (hasConfig) parser.setProtocolConfig(config);
        job->decoded.reserve(job->result.packets.size());
        for (const auto& raw : job->result.packets) {
            if (job->cancel) { job->result.canceled = true; return; }
            job->decoded.append({raw, parser.parsePacket(raw)});
        }
    });
    m_loadThread->setParent(this);
    connect(m_loadThread, &QThread::finished, this, [this, job] {
        bool success = false;
        const bool canceled = job->cancel || job->result.canceled;
        QString error = job->result.error;
        if (!canceled && job->result.success &&
            m_captureEngine->replacePackets(std::move(job->result.packets), error)) {
            const auto session = m_captureEngine->sessionId();
            quint64 sequence = 1;
            for (auto& entry : job->decoded) {
                entry.raw.sessionId = session;
                entry.raw.sequence = sequence++;
                ++m_statusCounts[static_cast<int>(entry.parsed.status)];
                if (entry.raw.protocol == TransportProtocol::UDP) ++m_udpPackets;
                if (entry.raw.protocol == TransportProtocol::TCP) ++m_tcpPackets;
            }
            m_totalPackets = job->decoded.size();
            m_matchedPackets = parsedPackets() + failedPackets();
            m_packetModel->replacePackets(std::move(job->decoded));
            m_isCapturing = false;
            emit capturingChanged();
            emit statisticsChanged();
            updateBuffer();
            success = true;
        }
        m_loadThread->deleteLater();
        m_loadThread = nullptr;
        m_loadJob.reset();
        m_loadTimer->stop();
        if (!canceled && !success) reportError(error);
        if (success) { m_lastError.clear(); emit errorChanged(); }
        emit loadingChanged();
        emit fileLoadFinished(success, canceled);
    });
    m_loadTimer->start();
    emit loadingChanged();
    m_loadThread->start();
    return true;
}

void CaptureController::appendPackets(const QList<RawPacketOfTool>& packets)
{
    QList<PacketModel::PacketData> decoded;
    decoded.reserve(packets.size());
    const auto session = m_captureEngine->sessionId();
    for (const auto& raw : packets) {
        if (raw.sessionId != session) continue;
        auto parsed = m_parser->parsePacket(raw);
        ++m_statusCounts[static_cast<int>(parsed.status)];
        ++m_totalPackets;
        if (raw.protocol == TransportProtocol::UDP) ++m_udpPackets;
        if (raw.protocol == TransportProtocol::TCP) ++m_tcpPackets;
        decoded.append({raw, std::move(parsed)});
    }
    if (decoded.isEmpty()) return;
    m_matchedPackets = parsedPackets() + failedPackets();
    m_packetModel->addPackets(std::move(decoded));
    emit statisticsChanged();
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
        const QString label = iface.description.isEmpty() ? iface.name : iface.description;
        ifaceMap["displayName"] = iface.ipAddress.isEmpty() ? label : QString("%1 (%2)").arg(label, iface.ipAddress);
        result.append(ifaceMap);
    }
    
    return result;
}

bool CaptureController::loadProtocolConfig(const QString& filePath)
{
    if (!requireIdle()) return false;
    QString errorMsg;
    ProtocolConfiguration config = m_configLoader->loadFromFile(filePath, errorMsg);
    
    if (!errorMsg.isEmpty()) {
        m_lastError = errorMsg;
        emit errorChanged();
        emit errorOccurred(errorMsg);
        return false;
    }
    
    return activateProtocol(config,QFileInfo(filePath).fileName());
}

bool CaptureController::activateProtocol(const ProtocolConfiguration& config, const QString& label)
{
    invalidateTcpAnalysis();
    m_currentConfig = config;
    m_parser->setProtocolConfig(config);
    m_filterEngine->setProtocolConfig(config);
    m_configLoaded = true;
    m_configFileName = label;
    m_lastError.clear();
    emit configStateChanged();
    emit errorChanged();
    m_packetModel->reparsePackets(m_parser);
    for (int i = 0; i < 4; ++i)
        m_statusCounts[i] = m_packetModel->countStatus(static_cast<ParseStatus>(i));
    m_matchedPackets = parsedPackets() + failedPackets();
    emit statisticsChanged();
    emit protocolConfigLoaded(config.protocolName);
    
    return true;
}

QString CaptureController::protocolDraft() const
{
    if (!m_configLoaded) return QStringLiteral(R"({"protocolName":"新协议","transportType":"UDP","port":8080,"length":{"type":"fixed","fixedValue":4},"fields":[{"name":"数值","offset":0,"length":4,"type":"uint32","endianness":"big"}]})");
    return QString::fromUtf8(QJsonDocument(m_configLoader->toJson(m_currentConfig)).toJson(QJsonDocument::Indented));
}

QVariantMap CaptureController::readProtocolDraft(const QString& path)
{
    QString error;
    const auto config=m_configLoader->loadFromFile(path,error);
    if (!error.isEmpty()) return {{"error",error}};
    return {{"json",QString::fromUtf8(QJsonDocument(m_configLoader->toJson(config)).toJson(QJsonDocument::Indented))}};
}

QVariantMap CaptureController::previewProtocolDraft(const QString& json, const QString& hex)
{
    QString error;
    const auto config=m_configLoader->loadFromJson(json,error);
    if (!error.isEmpty()) return {{"validConfig",false},{"error",error}};
    QVariantMap result{{"validConfig",true},{"fields",QVariantList{}}};
    if (hex.trimmed().isEmpty()) { result["error"]=QCoreApplication::translate("Backend", "配置有效；输入应用负载 Hex 可预览"); return result; }
    result["validPreview"]=false;
    if (hex.size() > 262144 || !QRegularExpression("^[0-9a-fA-F\\s]+$").match(hex).hasMatch()) {
        result["error"]=QCoreApplication::translate("Backend", "Hex 仅允许十六进制字符、空格和换行，输入不能超过 256 KiB"); return result;
    }
    QString compact=hex;
    compact.remove(QRegularExpression("\\s"));
    if (compact.size()%2 || compact.size() > 131072) { result["error"]=QCoreApplication::translate("Backend", "Hex 必须由完整字节组成，负载不能超过 64 KiB"); return result; }
    RawPacketOfTool packet;
    packet.data=QByteArray::fromHex(compact.toLatin1());
    packet.length=packet.data.size(); packet.payloadLength=packet.data.size();
    packet.protocol=config.transportType; packet.destPort=config.port;
    ProtocolParser parser;
    parser.setProtocolConfig(config);
    const auto parsed=parser.parsePacket(packet);
    QVariantList fields;
    for (const auto& field : parsed.fields)
        fields.append(QVariantMap{{"name",field.name},{"raw",field.type == "bytearray" ? QString::fromLatin1(field.value.toByteArray().toHex(' ').toUpper()) : field.value.toString()},{"value",field.displayValue},{"offset",field.offset},{"length",field.length},{"type",field.type}});
    result["validPreview"]=parsed.isValid;
    result["error"]=parsed.isValid ? QCoreApplication::translate("Backend", "配置与 Hex 解析有效") : parsed.errorMessage;
    result["fields"]=fields;
    return result;
}

bool CaptureController::applyProtocolDraft(const QString& json)
{
    if (!requireIdle()) return false;
    if (isCapturing()) return reportError(QCoreApplication::translate("Backend", "请先停止采集再应用协议草稿"));
    QString error;
    const auto config=m_configLoader->loadFromJson(json,error);
    if (!error.isEmpty()) return reportError(error);
    return activateProtocol(config,QCoreApplication::translate("Backend", "编辑器草稿（未关联文件）"));
}

bool CaptureController::saveProtocolDraft(const QString& json, const QString& path)
{
    QString error;
    const auto config=m_configLoader->loadFromJson(json,error);
    if (!error.isEmpty() || !m_configLoader->saveToFile(config,path,error)) return reportError(error);
    return true;
}

bool CaptureController::startCapture(const QString& interfaceName)
{
    if (!requireIdle()) return false;
    QString errorMsg;
    if (m_captureEngine->startCapture(interfaceName, errorMsg)) {
        m_archiveError.clear();
        m_isCapturing = true;
        emit capturingChanged();
        return true;
    }
    m_isCapturing = m_captureEngine->isCapturing();
    emit capturingChanged();
    m_lastError = errorMsg;
    emit errorChanged();
    emit errorOccurred(errorMsg);
    return false;
}

void CaptureController::stopCapture()
{
    m_captureEngine->stopCapture();
    m_isCapturing = false;
    emit capturingChanged();
    emit bufferChanged();
}

void CaptureController::setTransportFilter(int protocol) const {
    m_displayModel->setTransportFilter(protocol);
}

bool CaptureController::savePackets(const QString& filePath)
{
    QString errorMsg;
    if (m_captureEngine->saveToPcap(filePath, errorMsg)) {
        updateBuffer();
        return true;
    }
    
    m_lastError = errorMsg;
    emit errorChanged();
    emit errorOccurred(errorMsg);
    return false;
}

bool CaptureController::loadPackets(const QString& filePath)
{
    if (!requireIdle()) return false;
    qDebug()<< "Loading packets from PCAP file:" << filePath;
    QString errorMsg;
    if (m_captureEngine->loadFromPcap(filePath, errorMsg)) {
        m_isCapturing = false;
        emit capturingChanged();
        return true;
    }
    
    m_lastError = errorMsg;
    emit errorChanged();
    emit errorOccurred(errorMsg);
    return false;
}

void CaptureController::clearPackets()
{
    if (!requireIdle()) return;
    stopCapture();
    m_captureEngine->clearPackets();
}

void CaptureController::resetSession()
{
    invalidateTcpAnalysis();
    updateBuffer();
    m_packetModel->clear();
    m_totalPackets = 0;
    m_udpPackets = 0;
    m_tcpPackets = 0;
    m_matchedPackets = 0;
    for (int& count : m_statusCounts) count = 0;
    emit statisticsChanged();
}

void CaptureController::onPacketCaptured(const RawPacketOfTool& packet) const {
    if (packet.sessionId != m_captureEngine->sessionId()) return;
    // qDebug()<< "Packet captured: len=" <<packet.length;
    m_filterEngine->processPacket(packet);
}

void CaptureController::onPacketFiltered(const RawPacketOfTool& packet, bool matchesProtocol)
{
    // qDebug()<< "Packet filtered: matchesProtocol=" << matchesProtocol<<"packet len="<<packet.length;
    ParsedPacket parsed = m_parser->parsePacket(packet);
    m_packetModel->addPacket(packet, parsed);
    ++m_statusCounts[static_cast<int>(parsed.status)];
    
    m_totalPackets++;
    if (packet.protocol == TransportProtocol::UDP) {
        m_udpPackets++;
    } else if (packet.protocol == TransportProtocol::TCP) {
        m_tcpPackets++;
    }
    
    Q_UNUSED(matchesProtocol);
    if (parsed.status == ParseStatus::Parsed || parsed.status == ParseStatus::Failed) {
        m_matchedPackets++;
    }
    
    emit statisticsChanged();
}
