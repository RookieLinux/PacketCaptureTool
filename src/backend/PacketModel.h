#ifndef PACKETMODEL_H
#define PACKETMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include <QVariantMap>
#include <QSortFilterProxyModel>
#include "DataTypes.h"

class ProtocolParser;

class PacketModel : public QAbstractListModel
{
    Q_OBJECT

public:
    struct PacketData {
        RawPacketOfTool raw;
        ParsedPacket parsed;
    };
    enum PacketRoles {
        IndexRole = Qt::UserRole + 1,
        TimestampRole,
        SourceIPRole,
        DestIPRole,
        SourcePortRole,
        DestPortRole,
        ProtocolRole,
        LengthRole,
        ParsedFieldsRole,
        RawDataRole,
        IsValidRole,
        ErrorMessageRole,
        ParseStatusRole,
        SequenceRole,
        ProtocolNameRole
    };

    explicit PacketModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addPacket(const RawPacketOfTool& raw, const ParsedPacket& parsed);
    void addPackets(QList<PacketData> packets);
    void replacePackets(QList<PacketData> packets);
    void reparsePackets(ProtocolParser* parser);
    void clear();
    QList<PacketData> trimBefore(quint64 sequence);
    int countStatus(ParseStatus status) const;

    Q_INVOKABLE QVariantMap getPacketDetails(int index) const;

private:
    QVector<PacketData> m_packets;
};

// Display filters never discard captured packets or change saved data.
class PacketDisplayModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit PacketDisplayModel(QObject* parent = nullptr);
    void setTransportFilter(int protocol);
    Q_INVOKABLE void setSearchText(const QString& text);
    Q_INVOKABLE void setStatusFilter(int status);
    Q_INVOKABLE QVariantMap getPacketDetails(int row) const;
    Q_INVOKABLE bool setConditions(const QVariantList& conditions, bool matchAny = false);
    Q_INVOKABLE void clearConditions();
    Q_INVOKABLE QVariantList conditions() const { return m_conditions; }
    Q_INVOKABLE int rowForSequence(quint64 sequence) const;
    Q_INVOKABLE QVariantList fieldsAtByte(int row, int byteOffset) const;
    Q_INVOKABLE QVariant cellValue(int row, const QString& column) const;
    Q_INVOKABLE void sortColumn(const QString& column, bool descending = false);
    Q_PROPERTY(QString filterError READ filterError NOTIFY conditionsChanged)
    QString filterError() const { return m_filterError; }
 signals:
    void conditionsChanged();
protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override;
private:
    int m_transport = 2;
    int m_status = -1;
    QString m_search;
    QVariantList m_conditions;
    bool m_matchAny = false;
    QString m_filterError;
    QString m_sortColumn;
    QVariant columnValue(const QModelIndex& index, const QString& column) const;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};

#endif // PACKETMODEL_H
