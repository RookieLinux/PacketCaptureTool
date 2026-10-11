#include "SettingsStorage.h"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QDesktopServices>
#include <memory>

static std::unique_ptr<QSettings> locationSettings(const QString& path)
{
    return path.isEmpty() ? std::make_unique<QSettings>() :
        std::make_unique<QSettings>(path, QSettings::IniFormat);
}

SettingsStorage::SettingsStorage(const QString& bootstrapFile, QObject* parent)
    : QObject(parent), m_bootstrapFile(bootstrapFile)
{
    const auto location = locationSettings(m_bootstrapFile);
    const auto overrideFile = qEnvironmentVariable("PACKET_CAPTURE_SETTINGS_FILE");
    const auto defaultDirectory = overrideFile.isEmpty() ?
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) : QFileInfo(overrideFile).absolutePath();
    const auto folder = location->value("settingsDirectory", defaultDirectory).toString();
    if (!QDir().mkpath(folder)) { m_initializationError = tr("无法创建配置目录，请检查目录权限"); return; }
    m_settingsFile = QDir(folder).filePath("settings.ini");
    if (!QFileInfo::exists(m_settingsFile)) {
        QSettings target(m_settingsFile, QSettings::IniFormat);
        const auto source = overrideFile.isEmpty() ? locationSettings({}) : locationSettings(overrideFile);
        for (const auto& key : source->allKeys()) {
            if (key != "settingsDirectory") target.setValue(key, source->value(key));
        }
        target.setValue("settingsVersion", 1);
        target.sync();
        if (target.status() != QSettings::NoError) m_initializationError = tr("无法写入配置目录，请检查目录权限");
    }
    QSettings verify(m_settingsFile, QSettings::IniFormat);
    if (verify.status() != QSettings::NoError) m_initializationError = tr("无法读取当前配置文件");
}

QString SettingsStorage::directory() const { return QFileInfo(m_settingsFile).absolutePath(); }

bool SettingsStorage::setDirectory(const QUrl& url)
{
    auto fail = [this](const QString& message) { emit errorOccurred(message); return false; };
    if (!url.isLocalFile() || url.toLocalFile().isEmpty()) return fail(tr("请选择本地配置目录"));
    const auto folder = QDir::cleanPath(QFileInfo(url.toLocalFile()).absoluteFilePath());
    if (!QDir().mkpath(folder)) return fail(tr("无法创建配置目录，请检查目录权限"));
    const auto targetPath = QDir(folder).filePath("settings.ini");
    if (QFileInfo(targetPath).absoluteFilePath() == QFileInfo(m_settingsFile).absoluteFilePath()) return true;
    emit aboutToChangeDirectory();
    QFile source(m_settingsFile);
    if (!source.open(QIODevice::ReadOnly)) return fail(tr("无法读取当前配置文件"));
    const auto contents = source.readAll();
    if (source.error() != QFileDevice::NoError) return fail(tr("无法读取当前配置文件"));
    if (QFileInfo::exists(targetPath)) {
        QFile existing(targetPath);
        if (!existing.open(QIODevice::ReadOnly) || existing.readAll() != contents)
            return fail(tr("目标目录已有不同的 settings.ini，请选择其他目录以免覆盖配置"));
    } else {
        QSaveFile target(targetPath);
        if (!target.open(QIODevice::WriteOnly) || target.write(contents) != contents.size() || !target.commit())
            return fail(tr("无法写入配置目录，请检查目录权限"));
    }
    auto location = locationSettings(m_bootstrapFile);
    const auto oldDirectory = location->value("settingsDirectory");
    location->setValue("settingsDirectory", folder);
    location->sync();
    if (location->status() != QSettings::NoError) {
        if (oldDirectory.isValid()) location->setValue("settingsDirectory", oldDirectory);
        else location->remove("settingsDirectory");
        return fail(tr("无法保存配置目录位置"));
    }
    m_settingsFile = targetPath;
    emit directoryChanged();
    return true;
}

bool SettingsStorage::openDirectory() const { return QDesktopServices::openUrl(directoryUrl()); }
