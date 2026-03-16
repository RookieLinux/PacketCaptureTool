#ifndef PACKETMODEL_H
#define PACKETMODEL_H

#include <QAbstractListModel>
#include <QVector>
#include <QVariantMap>
#include "DataTypes.h"

class PacketModel : public QAbstractListModel
{
    Q_OBJECT

public:
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
        ErrorMessageRole
    };

    explicit PacketModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addPacket(const RawPacketOfTool& raw, const ParsedPacket& parsed);
    void clear();

    Q_INVOKABLE QVariantMap getPacketDetails(int index) const;

private:
    struct PacketData {
        RawPacketOfTool raw;
        ParsedPacket parsed;
    };

    QVector<PacketData> m_packets;
};

#endif // PACKETMODEL_H
