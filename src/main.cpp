#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include "CaptureController.h"
#include "PacketModel.h"
#include "DataTypes.h"
#include "build_info.h"

int main(int argc, char *argv[])
{
    qputenv("QT_QUICK_FLICKABLE_WHEEL_DECELERATION", "1000");
    QGuiApplication app(argc, argv);

    // Set application metadata
    app.setOrganizationName("PacketCaptureTool");
    app.setApplicationName("Packet Capture Tool");
    app.setApplicationVersion(SOFTWARE_VERSION);

    // Set QML style (optional)
    QQuickStyle::setStyle("Material");

    // Create QML engine
    QQmlApplicationEngine engine;
    engine.addImportPath(CMAKE_LISTS_DIR + QStringLiteral("/3rdlib"));
    // Create CaptureController instance
    CaptureController* captureController = new CaptureController(&app);
    engine.rootContext()->setContextProperty("captureController", captureController);
    engine.rootContext()->setContextProperty("packetModel", captureController->getPacketModel());
    // Register PacketModel for QML
    qmlRegisterType<PacketModel>("PacketCapture", 1, 0, "PacketModel");
    // Load main QML file
    const QUrl url(QStringLiteral("qrc:/MainWindow.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
