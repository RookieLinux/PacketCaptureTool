#include <QCoreApplication>
#include "PacketModel.h"
#include "ProtocolParser.h"
#include <cmath>
#include <cstdlib>
#include <cerrno>
#include <QRegularExpression>

namespace {
long double numericValue(const QString& text, bool& ok)
{
    static const QRegularExpression syntax(QStringLiteral("^[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?$"));
    const auto bytes = text.trimmed().toLatin1();
    ok = syntax.match(QString::fromLatin1(bytes)).hasMatch();
    if (!ok) return 0;
    errno = 0;
    const auto value = std::strtold(bytes.constData(), nullptr);
    ok = std::isfinite(value) && errno != ERANGE;
    return value;
}
}


PacketDisplayModel::PacketDisplayModel(QObject* parent) : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void PacketDisplayModel::setTransportFilter(int protocol)
{
    if (protocol < 0 || protocol > 2 || protocol == m_transport) return;
    m_transport = protocol;
    invalidateFilter();
}

void PacketDisplayModel::setSearchText(const QString& text)
{
    const QString search = text.trimmed().toLower();
    if (search == m_search) return;
    m_search = search;
    invalidateFilter();
}

void PacketDisplayModel::setStatusFilter(int status)
{
    if (status < -1 || status > 3 || status == m_status) return;
    m_status = status;
    invalidateFilter();
}

QVariantMap PacketDisplayModel::getPacketDetails(int row) const
{
    if (row < 0 || row >= rowCount()) return {};
    const auto* model = qobject_cast<PacketModel*>(sourceModel());
    return model ? model->getPacketDetails(mapToSource(index(row, 0)).row()) : QVariantMap{};
}

bool PacketDisplayModel::filterAcceptsRow(int row, const QModelIndex& parent) const
{
    const QModelIndex packet = sourceModel()->index(row, 0, parent);
    auto value = [&](int role) { return sourceModel()->data(packet, role); };
    if (m_transport != 2 && value(PacketModel::ProtocolRole).toInt() != m_transport) return false;
    if (m_status >= 0 && value(PacketModel::ParseStatusRole).toInt() != m_status) return false;
    if (!m_conditions.isEmpty()) {
        bool matches = !m_matchAny;
        for (const auto& condition : m_conditions) {
            const auto rule = condition.toMap();
            const QString key = rule.value("column").toString(), op = rule.value("operator").toString();
            const auto actual = columnValue(packet, key);
            const auto expected = rule.value("value");
            bool match = false;
            if (op == "exists") match = actual.isValid();
            else if (actual.isValid()) {
                const QString a = actual.toString(), b = expected.toString();
                bool an = false, bn = false;
                const auto av = numericValue(a, an), bv = numericValue(b, bn);
                const int cmp = an && bn ? (av < bv ? -1 : av > bv ? 1 : 0) : QString::compare(a, b, Qt::CaseInsensitive);
                if (op == "eq") match = cmp == 0;
                else if (op == "ne") match = cmp != 0;
                else if (op == "contains") match = a.contains(b, Qt::CaseInsensitive);
                else if (an && bn) {
                    if (op == "gt") match = av > bv;
                    if (op == "ge") match = av >= bv;
                    if (op == "lt") match = av < bv;
                    if (op == "le") match = av <= bv;
                }
            }
            if (m_matchAny) matches = matches || match;
            else matches = matches && match;
        }
        if (!matches) return false;
    }
    if (m_search.isEmpty()) return true;
    QStringList searchable{QString::number(row + 1), value(PacketModel::SourceIPRole).toString(),
        value(PacketModel::DestIPRole).toString(), value(PacketModel::SourcePortRole).toString(),
        value(PacketModel::DestPortRole).toString(), value(PacketModel::ErrorMessageRole).toString(),
        value(PacketModel::ProtocolNameRole).toString()};
    for (const auto& field : value(PacketModel::ParsedFieldsRole).toList()) {
        const auto map = field.toMap();
        searchable << map.value("name").toString() << map.value("displayValue").toString();
    }
    if (searchable.join(' ').contains(m_search, Qt::CaseInsensitive)) return true;
    QString hexSearch = m_search;
    hexSearch.remove(' ');
    if (hexSearch.isEmpty() || hexSearch.size() % 2 != 0) return false;
    for (const QChar c : hexSearch)
        if (!c.isDigit() && (c < 'a' || c > 'f')) return false;
    const QString hex = value(PacketModel::RawDataRole).toString();
    int position = hex.indexOf(hexSearch);
    while (position >= 0) {
        if (position % 2 == 0) return true;
        position = hex.indexOf(hexSearch, position + 1);
    }
    return false;
}

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
    case SequenceRole:
        return QVariant::fromValue(packet.raw.sequence ? packet.raw.sequence : quint64(index.row() + 1));
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
        return packet.raw.data.length();
    case ParsedFieldsRole: {
        QVariantList fieldsList;
        for (const auto& field : packet.parsed.fields) {
            QVariantMap fieldMap;
            fieldMap["name"] = field.name;
            fieldMap["value"] = field.value;
            fieldMap["type"] = field.type;
            fieldMap["displayValue"] = field.displayValue;
            fieldMap["offset"] = field.offset;
            fieldMap["length"] = field.length;
            fieldsList.append(fieldMap);
        }
        return fieldsList;
    }
    case RawDataRole:
        return QString::fromLatin1(packet.raw.data.toHex());
    case IsValidRole:
        return packet.parsed.isValid;
    case ErrorMessageRole:
        return packet.parsed.errorMessage;
    case ParseStatusRole:
        return static_cast<int>(packet.parsed.status);
    case ProtocolNameRole:
        return packet.raw.protocolName.isEmpty() ?
            (packet.raw.protocol == TransportProtocol::UDP ? "UDP" : "TCP") : packet.raw.protocolName;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> PacketModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[SequenceRole] = "sequence";
    // Keep the source row distinct from QML ListView's filtered delegate index.
    roles[IndexRole] = "sourceIndex";
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
    roles[ParseStatusRole] = "parseStatus";
    roles[ProtocolNameRole] = "protocolName";
    return roles;
}

void PacketModel::addPacket(const RawPacketOfTool& raw, const ParsedPacket& parsed)
{
    addPackets({{raw, parsed}});
}

void PacketModel::addPackets(QList<PacketData> packets)
{
    if (packets.isEmpty()) return;
    const int first = m_packets.size();
    beginInsertRows(QModelIndex(), first, first + packets.size() - 1);
    m_packets.append(std::move(packets));
    endInsertRows();
}

void PacketModel::replacePackets(QList<PacketData> packets)
{
    beginResetModel();
    m_packets = std::move(packets);
    endResetModel();
}

void PacketModel::reparsePackets(ProtocolParser* parser)
{
    if (!parser || m_packets.isEmpty())
        return;

    for (PacketData& packet : m_packets) {
        packet.parsed = parser->parsePacket(packet.raw);
    }

    emit dataChanged(index(0, 0),
                     index(m_packets.count() - 1, 0),
                     {ParsedFieldsRole, IsValidRole, ErrorMessageRole, ParseStatusRole});
}

int PacketModel::countStatus(ParseStatus status) const
{
    int count = 0;
    for (const auto& packet : m_packets)
        if (packet.parsed.status == status) ++count;
    return count;
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
    result["sequence"] = data(this->index(index, 0), SequenceRole);
    result["timestamp"] = packet.raw.timestamp;
    result["sourceIP"] = packet.raw.sourceIP;
    result["destIP"] = packet.raw.destIP;
    result["sourcePort"] = packet.raw.sourcePort;
    result["destPort"] = packet.raw.destPort;
    result["protocol"] = data(this->index(index, 0), ProtocolNameRole);
    result["length"] = packet.raw.data.length();
    result["headerLength"] = packet.raw.headerLength;
    result["payloadLength"] = packet.raw.payloadLength;
    result["networkOffset"] = packet.raw.networkOffset;
    result["transportOffset"] = packet.raw.transportOffset;
    result["isValid"] = packet.parsed.isValid;
    result["parseStatus"] = static_cast<int>(packet.parsed.status);
    result["errorMessage"] = packet.parsed.errorMessage;
    result["rawData"] = QString::fromLatin1(packet.raw.data.toHex());
    
    // Convert parsed fields to QVariantList
    QVariantList fieldsList;
    for (const auto& field : packet.parsed.fields) {
        QVariantMap fieldMap;
        fieldMap["name"] = field.name;
        fieldMap["value"] = field.value;
        fieldMap["type"] = field.type;
        fieldMap["displayValue"] = field.displayValue;
        fieldMap["offset"] = field.offset;
        fieldMap["length"] = field.length;
        fieldsList.append(fieldMap);
    }
    result["parsedFields"] = fieldsList;
    
    return result;
}

QList<PacketModel::PacketData> PacketModel::trimBefore(quint64 sequence)
{
    int count = 0;
    while (count < m_packets.size() && m_packets[count].raw.sequence && m_packets[count].raw.sequence < sequence) ++count;
    if (!count) return {};
    const auto removed = m_packets.first(count);
    beginRemoveRows({}, 0, count - 1);
    m_packets.remove(0, count);
    endRemoveRows();
    return removed;
}

QVariant PacketDisplayModel::columnValue(const QModelIndex& index, const QString& column) const
{
    const QHash<QString, int> roles{{"sequence", PacketModel::SequenceRole}, {"timestamp", PacketModel::TimestampRole},
        {"sourceIP", PacketModel::SourceIPRole}, {"destIP", PacketModel::DestIPRole}, {"sourcePort", PacketModel::SourcePortRole},
        {"destPort", PacketModel::DestPortRole}, {"protocol", PacketModel::ProtocolNameRole}, {"length", PacketModel::LengthRole},
        {"status", PacketModel::ParseStatusRole}, {"error", PacketModel::ErrorMessageRole}};
    if (roles.contains(column)) return sourceModel()->data(index, roles.value(column));
    if (column.startsWith("field:")) {
        for (const auto& value : sourceModel()->data(index, PacketModel::ParsedFieldsRole).toList()) {
            const auto field = value.toMap();
            if (field.value("name").toString() == column.mid(6)) return field.value("value");
        }
    }
    return {};
}

bool PacketDisplayModel::setConditions(const QVariantList& conditions, bool matchAny)
{
    const QStringList columns{"sequence", "timestamp", "sourceIP", "destIP", "sourcePort", "destPort", "protocol", "length", "status", "error"};
    const QStringList operations{"eq", "ne", "contains", "gt", "ge", "lt", "le", "exists"};
    if (conditions.size() > 32) { m_filterError = QCoreApplication::translate("Backend", "最多支持 32 个筛选条件"); emit conditionsChanged(); return false; }
    for (const auto& item : conditions) {
        const auto rule = item.toMap();
        const QString column = rule.value("column").toString(), op = rule.value("operator").toString();
        bool numeric = false;
        const auto number = numericValue(rule.value("value").toString(), numeric);
        if ((!columns.contains(column) && !(column.startsWith("field:") && column.size() > 6)) || !operations.contains(op) ||
            ((op == "gt" || op == "ge" || op == "lt" || op == "le") && (!numeric || !std::isfinite(number))) ||
            (op != "exists" && !rule.contains("value"))) {
            m_filterError = QCoreApplication::translate("Backend", "筛选条件无效；数值比较需要有效数字"); emit conditionsChanged(); return false;
        }
    }
    m_conditions = conditions;
    m_matchAny = matchAny;
    m_filterError.clear();
    invalidateFilter();
    emit conditionsChanged();
    return true;
}

void PacketDisplayModel::clearConditions() { setConditions({}); }

int PacketDisplayModel::rowForSequence(quint64 sequence) const
{
    for (int row = 0; row < rowCount(); ++row)
        if (data(index(row, 0), PacketModel::SequenceRole).toULongLong() == sequence) return row;
    return -1;
}

QVariantList PacketDisplayModel::fieldsAtByte(int row, int byteOffset) const
{
    QVariantList fields;
    for (const auto& value : getPacketDetails(row).value("parsedFields").toList()) {
        const auto field = value.toMap();
        if (byteOffset >= field.value("offset").toInt() && byteOffset - field.value("offset").toInt() < field.value("length").toInt())
            fields.append(field);
    }
    return fields;
}

QVariant PacketDisplayModel::cellValue(int row, const QString& column) const
{
    if (row < 0 || row >= rowCount()) return {};
    return columnValue(mapToSource(index(row, 0)), column);
}

void PacketDisplayModel::sortColumn(const QString& column, bool descending)
{
    m_sortColumn = column;
    sort(column.isEmpty() ? -1 : 0, descending ? Qt::DescendingOrder : Qt::AscendingOrder);
}

bool PacketDisplayModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const auto a = columnValue(left, m_sortColumn), b = columnValue(right, m_sortColumn);
    if (!a.isValid() || !b.isValid()) return !a.isValid() && b.isValid();
    bool an = false, bn = false;
    const auto av = numericValue(a.toString(), an), bv = numericValue(b.toString(), bn);
    if (an && bn && av != bv) return av < bv;
    const int comparison = QString::compare(a.toString(), b.toString(), Qt::CaseInsensitive);
    if (comparison) return comparison < 0;
    return sourceModel()->data(left, PacketModel::SequenceRole).toULongLong() < sourceModel()->data(right, PacketModel::SequenceRole).toULongLong();
}
