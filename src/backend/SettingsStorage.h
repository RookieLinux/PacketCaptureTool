#pragma once
#include <QObject>
#include <QUrl>

// The native settings keep only the location; editable parameters live in an INI file.
class SettingsStorage : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString directory READ directory NOTIFY directoryChanged)
    Q_PROPERTY(QUrl directoryUrl READ directoryUrl NOTIFY directoryChanged)
    Q_PROPERTY(QString settingsFile READ settingsFile NOTIFY directoryChanged)
public:
    explicit SettingsStorage(const QString& bootstrapFile = {}, QObject* parent = nullptr);
    QString directory() const;
    QUrl directoryUrl() const { return QUrl::fromLocalFile(directory()); }
    QString settingsFile() const { return m_settingsFile; }
    QString initializationError() const { return m_initializationError; }
    Q_INVOKABLE bool setDirectory(const QUrl& directory);
    Q_INVOKABLE bool openDirectory() const;
signals:
    void aboutToChangeDirectory();
    void directoryChanged();
    void errorOccurred(const QString& message);
private:
    QString m_bootstrapFile;
    QString m_settingsFile;
    QString m_initializationError;
};
