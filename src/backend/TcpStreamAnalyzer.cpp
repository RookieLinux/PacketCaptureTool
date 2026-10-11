#include <QCoreApplication>
#include "TcpStreamAnalyzer.h"
#include "ProtocolParser.h"
#include <TcpReassembly.h>
#include <QMap>

namespace {
struct Direction {
    QByteArray pending;
    quint64 offset = 0;
    quint64 bytes = 0;
    bool unreliable = false;
    QString diagnostic;
};
struct Flow {
    QString source, destination;
    Direction sides[2];
    QString state = QCoreApplication::translate("Backend", "窗口结束（连接可能仍在继续）");
};
struct Analysis {
    ProtocolConfiguration config;
    ProtocolParser parser;
    QMap<quint32, Flow> flows;
    QVariantList messages;
    int inputPackets = 0;
    qint64 inputBytes = 0, outputBytes = 0;
    bool limited = false;

    static QString endpoint(const pcpp::IPAddress& ip, quint16 port) {
        const auto address = QString::fromStdString(ip.toString());
        return (address.contains(':') ? "[" + address + "]" : address) + ":" + QString::number(port);
    }
    static void start(const pcpp::ConnectionData& connection, void* cookie) {
        auto& a = *static_cast<Analysis*>(cookie);
        if (a.flows.size() >= 128) { a.limited = true; return; }
        Flow flow;
        flow.source = endpoint(connection.srcIP, connection.srcPort);
        flow.destination = endpoint(connection.dstIP, connection.dstPort);
        a.flows.insert(connection.flowKey, flow);
    }
    static void end(const pcpp::ConnectionData& connection, pcpp::TcpReassembly::ConnectionEndReason reason, void* cookie) {
        auto& a = *static_cast<Analysis*>(cookie);
        auto it = a.flows.find(connection.flowKey);
        if (it != a.flows.end() && reason == pcpp::TcpReassembly::TcpReassemblyConnectionClosedByFIN_RST)
            it->state = QCoreApplication::translate("Backend", "FIN / RST 结束");
    }
    int messageLength(Direction& side) {
        if (config.isFixedLength) return static_cast<int>(config.fixedLength);
        for (const auto& field : config.fields) {
            if (field.name != config.lengthFieldName) continue;
            if (quint64(side.pending.size()) < quint64(field.offset) + field.length) return 0;
            quint64 size = 0;
            for (quint32 i = 0; i < field.length; ++i) {
                const auto index = field.offset + (field.endianness == Endianness::BigEndian ? i : field.length - i - 1);
                size = (size << 8) | static_cast<quint8>(side.pending[index]);
            }
            if (size < quint64(field.offset) + field.length || size > 65536) return -1;
            return static_cast<int>(size);
        }
        return -1;
    }
    static void data(int8_t sideIndex, const pcpp::TcpStreamData& data, void* cookie) {
        auto& a = *static_cast<Analysis*>(cookie);
        auto it = a.flows.find(data.getConnectionData().flowKey);
        if (it == a.flows.end() || a.limited || sideIndex < 0 || sideIndex > 1) return;
        auto& side = it->sides[sideIndex];
        side.bytes += data.getDataLength();
        if (data.isBytesMissing()) {
            side.pending.clear();
            side.unreliable = true;
            side.diagnostic = QCoreApplication::translate("Backend", "缺失 %1 字节，停止该方向分帧，避免跨缺口误解析").arg(data.getMissingByteCount());
        }
        if (side.unreliable) return;
        side.pending.append(reinterpret_cast<const char*>(data.getData()),static_cast<qsizetype>(data.getDataLength()));
        while (!side.pending.isEmpty()) {
            const int length = a.messageLength(side);
            if (length < 0) {
                side.unreliable = true;
                side.diagnostic = QCoreApplication::translate("Backend", "消息长度无效，停止该方向分帧；请核对配置或窗口起始边界");
                side.pending.clear();
                return;
            }
            if (!length || side.pending.size() < length) return;
            if (a.messages.size() >= 1000 || a.outputBytes + length > 4 * 1024 * 1024) {
                a.limited = true;
                return;
            }
            const QByteArray payload = side.pending.left(length);
            RawPacketOfTool raw;
            raw.protocol = TransportProtocol::TCP;
            raw.sourcePort = data.getConnectionData().srcPort;
            raw.destPort = data.getConnectionData().dstPort;
            raw.data = payload;
            raw.length = length;
            raw.payloadLength = length;
            const auto parsed = a.parser.parsePacket(raw);
            QVariantList fields;
            for (const auto& field : parsed.fields.mid(0,64))
                fields.append(QVariantMap{{"name",field.name},{"value",field.displayValue},{"offset",field.offset},{"length",field.length}});
            a.messages.append(QVariantMap{{"flow",QString::number(data.getConnectionData().flowKey)},
                {"direction",sideIndex},{"offset",QVariant::fromValue(side.offset)},
                {"source",sideIndex == 0 ? it->source : it->destination},
                {"destination",sideIndex == 0 ? it->destination : it->source},
                {"length",length},{"hex",QString::fromLatin1(payload.toHex(' '))},
                {"valid",parsed.isValid},{"error",parsed.errorMessage},{"fields",fields},
                {"fieldsLimited",parsed.fields.size() > 64}});
            a.outputBytes += length;
            side.offset += length;
            side.pending.remove(0,length);
        }
    }
};
}

QVariantMap TcpStreamAnalyzer::analyze(const QList<RawPacketOfTool>& packets,
                                     const ProtocolConfiguration& config, const std::atomic_bool& cancel)
{
    Analysis a;
    a.config = config;
    a.parser.setProtocolConfig(config);
    // Do not flush gaps merely because the other direction sent data.
    pcpp::TcpReassembly reassembly(Analysis::data,&a,Analysis::start,Analysis::end,
                                   pcpp::TcpReassemblyConfiguration(false,1,128,128,false));
    for (const auto& packet : packets) {
        if (cancel) return {{"canceled",true}};
        if (packet.protocol != TransportProtocol::TCP ||
            (packet.sourcePort != config.port && packet.destPort != config.port)) continue;
        if (a.inputPackets >= 50000 || a.inputBytes + packet.data.size() > 16 * 1024 * 1024 || a.limited) {
            a.limited = true;
            break;
        }
        ++a.inputPackets;
        a.inputBytes += packet.data.size();
        timeval timestamp{};
        timestamp.tv_sec = packet.timestamp / 1000000;
        timestamp.tv_usec = packet.timestamp % 1000000;
        pcpp::RawPacket raw(reinterpret_cast<const uint8_t*>(packet.data.constData()),packet.data.size(),
                             timestamp,false,static_cast<pcpp::LinkLayerType>(packet.linkLayerType));
        reassembly.reassemblePacket(&raw);
    }
    reassembly.closeAllConnections();
    QVariantList flows;
    for (auto it = a.flows.cbegin(); it != a.flows.cend(); ++it) {
        QStringList diagnostics;
        for (int i = 0; i < 2; ++i) {
            const auto& side = it->sides[i];
            if (!side.diagnostic.isEmpty()) diagnostics << QCoreApplication::translate("Backend", "方向 %1：%2").arg(i+1).arg(side.diagnostic);
            if (!side.pending.isEmpty()) diagnostics << QCoreApplication::translate("Backend", "方向 %1：剩余 %2 字节，不完整消息未解析").arg(i+1).arg(side.pending.size());
        }
        flows.append(QVariantMap{{"id",QString::number(it.key())},{"displayName",it->source + " ↔ " + it->destination},{"source",it->source},
            {"destination",it->destination},{"state",it->state},{"diagnostic",diagnostics.join("；")}});
    }
    return {{"flows",flows},{"messages",a.messages},{"limited",a.limited},{"packets",a.inputPackets},
        {"notice",QCoreApplication::translate("Backend", "基于当前内存窗口快照，假设每个方向首段位于消息边界；缺包后停止该方向分帧。最多分析 5 万个匹配段 / 16 MiB，128 条连接、1000 条消息 / 4 MiB；每消息展示前 64 个字段。%1")
            .arg(a.limited ? QCoreApplication::translate("Backend", "已达到分析上限，结果不完整。") : "")}};
}
