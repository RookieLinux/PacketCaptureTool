#include "LanguageSettings.h"
#include <QGuiApplication>
#include <QFontDatabase>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QPointer>

LanguageSettings::LanguageSettings(QObject* parent) : QObject(parent)
{
    const auto settingsFile = qEnvironmentVariable("PACKET_CAPTURE_SETTINGS_FILE");
    m_settings = settingsFile.isEmpty() ? std::make_unique<QSettings>() :
        std::make_unique<QSettings>(settingsFile, QSettings::IniFormat);
    auto saved = m_settings->value("language", "zh_CN").toString();
    setLanguage(saved == "en_US" ? saved : "zh_CN");
}

void LanguageSettings::setSettingsFile(const QString& path)
{
    m_settings->sync();
    m_settings = std::make_unique<QSettings>(path, QSettings::IniFormat);
}

void LanguageSettings::attachEngine(QQmlApplicationEngine* engine)
{
    connect(this, &LanguageSettings::languageChanged, engine, [engine] {
        // Retranslating a ComboBox model resets its selection. Keep indices stable
        // so changing language cannot change filters, limits or editor choices.
        QList<QPair<QPointer<QObject>, int>> selections;
        for (auto* root : engine->rootObjects()) {
            for (auto* object : root->findChildren<QObject*>()) {
                if (object->inherits("QQuickComboBox"))
                    selections.append({object, object->property("currentIndex").toInt()});
            }
        }
        engine->retranslate();
        for (const auto& selection : selections) {
            if (selection.first) selection.first->setProperty("currentIndex", selection.second);
        }
    });
}

void LanguageSettings::setLanguage(const QString& language)
{
    if ((language != "zh_CN" && language != "en_US") || language == m_language) return;
    if (language == "en_US" && !m_translator.load(":/i18n/app_en.qm")) return;
    m_changingLanguage = true;
    emit languageChanging();
    QCoreApplication::removeTranslator(&m_translator);
    if (language == "en_US") QCoreApplication::installTranslator(&m_translator);
    m_language = language;
    QLocale::setDefault(QLocale(language));
    const QStringList candidates = language == "zh_CN" ?
        QStringList{"Microsoft YaHei UI", "Microsoft YaHei", "Noto Sans CJK SC", "PingFang SC"} :
        QStringList{"Segoe UI", "Noto Sans", "Helvetica Neue"};
    m_fontFamily = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
    const auto installed = QFontDatabase::families();
    for (const auto& family : candidates) {
        if (installed.contains(family)) { m_fontFamily = family; break; }
    }
    auto font = QGuiApplication::font();
    font.setFamily(m_fontFamily);
    QGuiApplication::setFont(font);
    m_settings->setValue("language", language);
    m_settings->sync();
    emit languageChanged();
    m_changingLanguage = false;
    emit languageChanging();
}
