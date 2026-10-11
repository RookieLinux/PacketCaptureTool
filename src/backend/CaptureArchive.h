#pragma once

#include "DataTypes.h"
#include <QStringList>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

struct ArchiveOptions {
    QString directory;
    qint64 fileBytes = 64LL * 1024 * 1024;
    int fileSeconds = 300;
    int maxFiles = 1000;
    qint64 queueBytes = 16LL * 1024 * 1024;
    int queuePackets = 8192;
};

struct ArchiveState {
    bool active = false;
    quint64 writtenSequence = 0;
    qint64 writtenPackets = 0;
    qint64 pendingPackets = 0;
    QStringList files;
    QString error;
};

// A bounded queue separates capture callbacks from disk I/O. Only flushed packets
// may leave the in-memory window. Files are unique, never overwritten or deleted.
class CaptureArchive {
public:
    ~CaptureArchive();
    bool start(const ArchiveOptions& options, QString& error);
    bool enqueue(const RawPacketOfTool& packet);
    void stop();
    void reset();
    ArchiveState state() const;
private:
    void run();
    mutable std::mutex m_mutex;
    std::condition_variable m_ready;
    std::thread m_worker;
    std::deque<RawPacketOfTool> m_queue;
    qint64 m_queuedBytes = 0;
    bool m_stopping = false;
    ArchiveOptions m_options;
    ArchiveState m_state;
    QString m_prefix;
};
