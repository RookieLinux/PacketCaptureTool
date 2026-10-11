#include <QCoreApplication>
#include "CaptureArchive.h"
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QUuid>

CaptureArchive::~CaptureArchive() { stop(); }

bool CaptureArchive::start(const ArchiveOptions& options, QString& error)
{
    stop();
    if (!QFileInfo(options.directory).isDir() || !QFileInfo(options.directory).isWritable() ||
        options.fileBytes < 24 || options.fileSeconds < 1 || options.maxFiles < 1 ||
        options.queueBytes < 1 || options.queuePackets < 1) {
        error = QCoreApplication::translate("Backend", "归档目录不可写或轮转设置无效");
        return false;
    }
    {
        std::lock_guard lock(m_mutex);
        m_options = options;
        m_state = {};
        m_state.active = true;
        m_stopping = false;
        m_queue.clear();
        m_queuedBytes = 0;
        m_prefix = "capture-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    m_worker = std::thread([this] { run(); });
    return true;
}

bool CaptureArchive::enqueue(const RawPacketOfTool& packet)
{
    std::lock_guard lock(m_mutex);
    if (!m_state.active || m_stopping || !m_state.error.isEmpty()) return false;
    if (m_queue.size() >= size_t(m_options.queuePackets) ||
        packet.data.size() > m_options.queueBytes - m_queuedBytes) {
        m_state.error = QCoreApplication::translate("Backend", "磁盘写入队列已满，采集已停止；请检查磁盘速度");
        m_ready.notify_one();
        return false;
    }
    m_queue.push_back(packet);
    m_queuedBytes += packet.data.size();
    ++m_state.pendingPackets;
    m_ready.notify_one();
    return true;
}

void CaptureArchive::stop()
{
    {
        std::lock_guard lock(m_mutex);
        m_stopping = true;
    }
    m_ready.notify_one();
    if (m_worker.joinable()) m_worker.join();
}

ArchiveState CaptureArchive::state() const
{
    std::lock_guard lock(m_mutex);
    return m_state;
}

void CaptureArchive::reset()
{
    stop();
    std::lock_guard lock(m_mutex);
    m_state = {};
}

void CaptureArchive::run()
{
    QFile file;
    QElapsedTimer age;
    int linkType = -1;
    int files = 0;
    auto fail = [&](const QString& message) {
        std::lock_guard lock(m_mutex);
        m_state.error = message;
        m_stopping = true;
    };
    bool failed = false;
    while (!failed) {
        QList<RawPacketOfTool> batch;
        {
            std::unique_lock lock(m_mutex);
            m_ready.wait(lock, [&] { return m_stopping || !m_queue.empty(); });
            if (m_queue.empty() && m_stopping) break;
            while (!m_queue.empty() && batch.size() < 128) {
                m_queuedBytes -= m_queue.front().data.size();
                batch.append(std::move(m_queue.front()));
                m_queue.pop_front();
            }
        }
        for (const auto& packet : batch) {
            if (!file.isOpen() || linkType != packet.linkLayerType ||
                (file.size() > 24 && (file.size() + 16 + packet.data.size() > m_options.fileBytes ||
                    age.elapsed() >= qint64(m_options.fileSeconds) * 1000))) {
                if (file.isOpen()) {
                    if (!file.flush()) { fail(QCoreApplication::translate("Backend", "归档文件刷新失败：") + file.errorString()); failed = true; break; }
                    file.close();
                }
                if (files >= m_options.maxFiles) {
                    fail(QCoreApplication::translate("Backend", "已达到归档文件数量上限，采集停止；已有文件已保留"));
                    failed = true;
                    break;
                }
                file.setFileName(QDir(m_options.directory).filePath(m_prefix + QString("-%1.pcap").arg(++files, 5, 10, QLatin1Char('0'))));
                if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
                    fail(QCoreApplication::translate("Backend", "无法创建归档文件：") + file.errorString()); failed = true; break;
                }
                QDataStream stream(&file);
                stream.setByteOrder(QDataStream::LittleEndian);
                stream << quint32(0xa1b2c3d4) << quint16(2) << quint16(4) << quint32(0) << quint32(0)
                    << quint32(0xffffffff) << quint32(packet.linkLayerType);
                age.start();
                linkType = packet.linkLayerType;
                std::lock_guard lock(m_mutex);
                m_state.files.append(file.fileName());
            }
            QDataStream stream(&file);
            stream.setByteOrder(QDataStream::LittleEndian);
            stream << quint32(packet.timestamp / 1000000) << quint32(packet.timestamp % 1000000)
                << quint32(packet.data.size()) << quint32(packet.data.size());
            if (stream.status() != QDataStream::Ok || file.write(packet.data) != packet.data.size() || !file.flush()) {
                fail(QCoreApplication::translate("Backend", "归档写入失败：") + file.errorString()); failed = true; break;
            }
            {
                std::lock_guard lock(m_mutex);
                ++m_state.writtenPackets;
                --m_state.pendingPackets;
                m_state.writtenSequence = packet.sequence;
            }
        }
    }
    if (file.isOpen()) file.close();
    std::lock_guard lock(m_mutex);
    m_state.active = false;
    m_queue.clear();
    m_queuedBytes = 0;
}
