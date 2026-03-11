#ifndef CONFIGURATIONLOADER_H
#define CONFIGURATIONLOADER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include "DataTypes.h"

class ConfigurationLoader : public QObject
{
    Q_OBJECT

public:
    explicit ConfigurationLoader(QObject *parent = nullptr);

    ProtocolConfiguration loadFromFile(const QString& filePath, QString& errorMsg);
    bool saveToFile(const ProtocolConfiguration& config, const QString& filePath, QString& errorMsg);
    bool validateConfig(const ProtocolConfiguration& config, QString& errorMsg);

private:
    ProtocolConfiguration parseJson(const QJsonObject& json, QString& errorMsg);
    QJsonObject toJson(const ProtocolConfiguration& config);
};

#endif // CONFIGURATIONLOADER_H
