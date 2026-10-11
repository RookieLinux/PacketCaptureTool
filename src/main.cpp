#include <QApplication>
#include <QQuickWindow>
#include <QIcon>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QFont>
#include "CaptureController.h"
#include "PacketModel.h"
#include "DataTypes.h"
#include "build_info.h"
#include "LanguageSettings.h"
#include "SettingsStorage.h"
#include "DesktopIntegration.h"

int main(int argc, char *argv[])
{
#ifdef Q_OS_LINUX
    qputenv("GIO_USE_VFS", "local");
    qputenv("DCONF_BEHIND_BACKEND", "memory");
#endif
    qputenv("QT_QUICK_FLICKABLE_WHEEL_DECELERATION", "1000");
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon(":/icons/packet-capture.ico"));

    app.setOrganizationName("PacketCaptureTool");
    app.setApplicationName("Packet Capture Tool");
    app.setApplicationVersion(SOFTWARE_VERSION);
    SettingsStorage settingsStorage;
    if (!settingsStorage.initializationError().isEmpty()) {
        QMessageBox::critical(nullptr, app.applicationName(), settingsStorage.initializationError());
        return 1;
    }
    qputenv("PACKET_CAPTURE_SETTINGS_FILE", settingsStorage.settingsFile().toUtf8());
    LanguageSettings languageSettings;
    DesktopIntegration desktopIntegration;
    QObject::connect(&languageSettings, &LanguageSettings::languageChanged,
                     &desktopIntegration, &DesktopIntegration::retranslate);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("languageSettings", &languageSettings);
    engine.rootContext()->setContextProperty("settingsStorage", &settingsStorage);
    engine.rootContext()->setContextProperty("desktopIntegration", &desktopIntegration);
    languageSettings.attachEngine(&engine);
    engine.addImportPath(app.applicationDirPath() + "/qml");
    // engine.addImportPath(FLUENTUI_QML_DIR);

    CaptureController* captureController = new CaptureController(&app);
    QObject::connect(&settingsStorage, &SettingsStorage::aboutToChangeDirectory,
                     captureController, &CaptureController::flushPreferences);
    QObject::connect(&settingsStorage, &SettingsStorage::directoryChanged, &app, [&] {
        captureController->setSettingsFile(settingsStorage.settingsFile());
        languageSettings.setSettingsFile(settingsStorage.settingsFile());
        qputenv("PACKET_CAPTURE_SETTINGS_FILE", settingsStorage.settingsFile().toUtf8());
    });
    engine.rootContext()->setContextProperty("captureController", captureController);
    engine.rootContext()->setContextProperty("packetModel", captureController->getDisplayModel());
    qmlRegisterType<PacketModel>("PacketCapture", 1, 0, "PacketModel");

    const QUrl url(QStringLiteral("qrc:/MainWindow.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
