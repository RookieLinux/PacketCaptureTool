#include "PacketModel.h"

PacketModel::PacketModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PacketModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return m_packets.count();
}

QVariant PacketModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_packets.count())
        return QVariant();

    const PacketData& packet = m_packets.at(index.row());

    switch (role) {
    case IndexRole:
        return index.row();
    case TimestampRole:
        return packet.raw.timestamp;
    case SourceIPRole:
        return packet.raw.sourceIP;
    case SourcePortRole:
        return packet.raw.sourcePort;
    case DestIPRole:
        return packet.raw.destIP;
    case DestPortRole:
        return packet.raw.destPort;
    case ProtocolRole:
        return static_cast<int>(packet.raw.protocol);
    case LengthRole:
        return packet.raw.length;
    case ParsedFieldsRole: {
        QVariantList fieldsList;
        for (const auto& field : packet.parsed.fields) {
            QVariantMap fieldMap;
            fieldMap["name"] = field.name;
            fieldMap["value"] = field.value;
            fieldMap["type"] = field.type;
            fieldMap["displayValue"] = field.displayValue;
            fieldsList.append(fieldMap);
        }
        return fieldsList;
    }
    case RawDataRole:
        return QString(packet.raw.data.toHex());
    case IsValidRole:
        return packet.parsed.isValid;
    case ErrorMessageRole:
        return packet.parsed.errorMessage;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> PacketModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IndexRole] = "index";
    roles[TimestampRole] = "timestamp";
    roles[SourceIPRole] = "sourceIP";
    roles[DestIPRole] = "destIP";
    roles[SourcePortRole] = "sourcePort";
    roles[DestPortRole] = "destPort";
    roles[ProtocolRole] = "protocol";
    roles[LengthRole] = "length";
    roles[ParsedFieldsRole] = "parsedFields";
    roles[RawDataRole] = "rawData";
    roles[IsValidRole] = "isValid";
    roles[ErrorMessageRole] = "errorMessage";
    return roles;
}

void PacketModel::addPacket(const RawPacketOfTool& raw, const ParsedPacket& parsed)
{
    beginInsertRows(QModelIndex(), m_packets.count(), m_packets.count());
    PacketData data;
    data.raw = raw;
    data.parsed = parsed;
    m_packets.append(data);
    endInsertRows();
}

void PacketModel::clear()
{
    beginResetModel();
    m_packets.clear();
    endResetModel();
}

QVariantMap PacketModel::getPacketDetails(int index) const
{
    QVariantMap result;
    
    if (index < 0 || index >= m_packets.count())
        return result;

    const PacketData& packet = m_packets.at(index);
    
    result["index"] = index;
    result["timestamp"] = packet.raw.timestamp;
    result["sourceIP"] = packet.raw.sourceIP;
    result["destIP"] = packet.raw.destIP;
    result["sourcePort"] = packet.raw.sourcePort;
    result["destPort"] = packet.raw.destPort;
    result["protocol"] = packet.raw.protocol == TransportProtocol::UDP ? "UDP" : "TCP";
    result["length"] = packet.raw.length;
    result["isValid"] = packet.parsed.isValid;
    result["errorMessage"] = packet.parsed.errorMessage;
    result["rawData"] = QString(packet.raw.data);
    
    // Convert parsed fields to QVariantList
    QVariantList fieldsList;
    for (const auto& field : packet.parsed.fields) {
        QVariantMap fieldMap;
        fieldMap["name"] = field.name;
        fieldMap["value"] = field.value;
        fieldMap["type"] = field.type;
        fieldMap["displayValue"] = field.displayValue;
        fieldsList.append(fieldMap);
    }
    result["parsedFields"] = fieldsList;
    
    return result;
}
