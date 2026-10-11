#pragma once

#include <QObject>
#include <QTranslator>
#include <QSettings>
#include <memory>

class QQmlApplicationEngine;

class LanguageSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY languageChanged)
    Q_PROPERTY(bool changingLanguage READ changingLanguage NOTIFY languageChanging)
public:
    explicit LanguageSettings(QObject* parent = nullptr);
    QString language() const { return m_language; }
    QString fontFamily() const { return m_fontFamily; }
    bool changingLanguage() const { return m_changingLanguage; }
    void attachEngine(QQmlApplicationEngine* engine);
    void setSettingsFile(const QString& path);
    Q_INVOKABLE void setLanguage(const QString& language);
signals:
    void languageChanged();
    void languageChanging();
private:
    QString m_language;
    QString m_fontFamily;
    bool m_changingLanguage = false;
    QTranslator m_translator;
    std::unique_ptr<QSettings> m_settings;
};
