#pragma once
#include "DataTypes.h"
#include <QVariantMap>
#include <atomic>

// Analyze a bounded snapshot; offsets in results refer to application messages.
class TcpStreamAnalyzer {
public:
    static QVariantMap analyze(const QList<RawPacketOfTool>& packets,
                               const ProtocolConfiguration& config,
                               const std::atomic_bool& cancel);
};
