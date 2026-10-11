#include <QtTest/QtTest>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QFontDatabase>
#include <QTemporaryDir>
#include <QClipboard>
#include "CaptureController.h"
#include "ProtocolParser.h"
#include "LanguageSettings.h"
#include "SettingsStorage.h"
#include "DesktopIntegration.h"
#include <QSettings>
#include <QIcon>
#include <QAction>

class QmlInterfaceTests : public QObject
{
    Q_OBJECT
private:
    void verifyHexCopy(QQuickItem* hex, QQuickWindow* window, const QString& rawHex)
    {
        QString plain;
        QVERIFY(QMetaObject::invokeMethod(hex, "getText", Q_RETURN_ARG(QString, plain),
            Q_ARG(int, 0), Q_ARG(int, hex->property("length").toInt())));
        const auto firstRow = plain.indexOf("00000000");
        const auto secondRow = plain.indexOf("00000010");
        QVERIFY(firstRow >= 0 && secondRow > firstRow);
        const auto from = plain.indexOf("0E 0F", firstRow);
        QVERIFY(from >= 0 && from < secondRow);
        auto select = [&](int start, int end) {
            return QMetaObject::invokeMethod(hex, "select", Q_ARG(int,start), Q_ARG(int,end));
        };
        QVERIFY(select(from, secondRow+18));
        hex->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QGuiApplication::clipboard()->text(), QString("0E 0F 00 01 02"));
        auto* copyAction = hex->findChild<QObject*>("hexCopySelectionAction");
        QVERIFY(copyAction);
        QGuiApplication::clipboard()->setText("sentinel");
        QVERIFY(QMetaObject::invokeMethod(copyAction,"triggered"));
        QCOMPARE(QGuiApplication::clipboard()->text(), QString("0E 0F 00 01 02"));
        QVERIFY(select(from+1,from+2));
        QVERIFY(QMetaObject::invokeMethod(hex,"copyHexSelection"));
        QCOMPARE(QGuiApplication::clipboard()->text(), QString("0E"));
        const auto ascii = plain.indexOf('|',firstRow);
        QVERIFY(ascii >= 0);
        for (const auto& range : {QPair<int,int>{firstRow,firstRow+8}, QPair<int,int>{ascii+1,ascii+4}}) {
            QVERIFY(select(range.first,range.second));
            QGuiApplication::clipboard()->setText("sentinel");
            QTest::keyClick(window,Qt::Key_C,Qt::ControlModifier);
            QCOMPARE(QGuiApplication::clipboard()->text(),QString("sentinel"));
        }
        QStringList bytes;
        for (int i=0;i<rawHex.size();i+=2) bytes << rawHex.mid(i,2).toUpper();
        QVERIFY(QMetaObject::invokeMethod(hex,"selectAll"));
        QTest::keyClick(window,Qt::Key_C,Qt::ControlModifier);
        QCOMPARE(QGuiApplication::clipboard()->text(),bytes.join(' '));
        QGuiApplication::clipboard()->setText("sentinel");
        QVERIFY(QMetaObject::invokeMethod(hex,"copyAllHex"));
        QCOMPARE(QGuiApplication::clipboard()->text(),bytes.join(' '));
        QVERIFY(QMetaObject::invokeMethod(hex,"deselect"));
    }
    static QQuickItem* findVisualItem(QQuickItem* root,const QString& name) {
        if (root->objectName() == name && root->isVisible() && root->width() > 0 && root->height() > 0) return root;
        for (auto* child : root->childItems())
            if (auto* found=findVisualItem(child,name)) return found;
        return nullptr;
    }
    void verifyPacketSelectionAfterStatusFilter(CaptureController& controller, QQuickWindow* window)
    {
        auto* status=window->findChild<QObject*>("statusFilter");
        auto* browser=window->findChild<QObject*>("packetBrowser");
        auto* details=window->findChild<QObject*>("packetDetails");
        QVERIFY(status && browser && details);
        RawPacketOfTool packet{};
        packet.protocol=TransportProtocol::UDP;
        packet.sourceIP="192.0.2.1";
        packet.destIP="192.0.2.2";
        packet.destPort=8080;
        for (int i=0;i<4;++i) {
            packet.sequence=i+1;
            packet.data=QByteArray::fromHex(i==1 ? "0102" : "01020304");
            packet.length=packet.data.size();
            packet.destPort=i==2 ? 9090 : 8080;
            QVERIFY(QMetaObject::invokeMethod(&controller,"onPacketCaptured",Qt::DirectConnection,Q_ARG(RawPacketOfTool,packet)));
        }
        QTRY_COMPARE(controller.totalPackets(),4);
        auto verifyClick=[&](quint64 sequence,int parseStatus) {
            QQuickItem* row=nullptr;
            QTRY_VERIFY((row=findVisualItem(window->contentItem(),"packetRow_"+QString::number(sequence))));
            QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,row->mapToScene(QPointF(20,row->height()/2)).toPoint());
            QTRY_COMPARE(details->property("packetDetails").toMap().value("sequence").toULongLong(),sequence);
            QCOMPARE(details->property("packetDetails").toMap().value("parseStatus").toInt(),parseStatus);
            QVERIFY(!details->property("packetDetails").toMap().value("rawData").toString().isEmpty());
        };
        status->setProperty("currentIndex",1); // Unparsed, before loading any configuration.
        QTest::qWait(100);
        verifyClick(1,0);
        auto* unparsedHex = findVisualItem(window->contentItem(), "parsedHexPreview");
        auto* unparsedFields = findVisualItem(window->contentItem(), "parsedFieldsPane");
        QVERIFY(unparsedHex && unparsedFields);
        QVERIFY(unparsedHex->height() > 220);
        QVERIFY(unparsedFields->height() <= 60);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/hex-unparsed-expanded-preview.png"));
        QVERIFY(controller.applyProtocolDraft(QStringLiteral(R"({"protocolName":"Selection","transportType":"UDP","port":8080,"length":{"type":"fixed","fixedValue":4},"fields":[{"name":"Value","offset":0,"length":4,"type":"uint32","endianness":"big"}]})")));
        QTRY_COMPARE(controller.visiblePackets(),0);
        for (bool tableMode : {true,false}) {
            browser->setProperty("tableMode",tableMode);
            controller.getDisplayModel()->sortColumn("sequence",true);
            for (int index : {4,3,2,0,4,3}) {
                status->setProperty("currentIndex",index);
                QTest::qWait(100);
                QCOMPARE(controller.visiblePackets(),index==3 ? 2 : index==0 ? 4 : 1);
                verifyClick(index==4 ? 2 : index==2 ? 3 : 1,index==4 ? 3 : index==2 ? 1 : 2);
                if (index==3) verifyClick(4,2);
            }
        }
        browser->setProperty("tableMode",true);
        QVERIFY(QMetaObject::invokeMethod(window,"resetDisplayFilters"));
        controller.clearPackets();
    }

private slots:
    void testConfigurationDirectoryMigration()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto legacyPath = temporary.filePath("legacy.ini");
        { QSettings legacy(legacyPath,QSettings::IniFormat); legacy.setValue("compact",false); legacy.sync(); }
        qputenv("PACKET_CAPTURE_SETTINGS_FILE",legacyPath.toUtf8());
        const auto bootstrap = temporary.filePath("location.ini");
        SettingsStorage storage(bootstrap);
        QVERIFY(storage.initializationError().isEmpty());
        qputenv("PACKET_CAPTURE_SETTINGS_FILE",storage.settingsFile().toUtf8());
        CaptureController controller;
        LanguageSettings language;
        connect(&storage,&SettingsStorage::aboutToChangeDirectory,&controller,&CaptureController::flushPreferences);
        connect(&storage,&SettingsStorage::directoryChanged,&controller,[&] {
            controller.setSettingsFile(storage.settingsFile());
            language.setSettingsFile(storage.settingsFile());
        });
        QCOMPARE(controller.preferences().value("compact").toBool(),false);
        controller.setPreference("windowWidth",1234);
        controller.setPreference("closeToTray",false);
        language.setLanguage("en_US");
        const auto oldFile = storage.settingsFile();
        const auto newFolder = temporary.filePath(QString::fromUtf8("配置目录"));
        QVERIFY(storage.setDirectory(QUrl::fromLocalFile(newFolder)));
        QVERIFY(QFileInfo::exists(oldFile));
        QCOMPARE(storage.directory(),newFolder);
        QCOMPARE(controller.preferences().value("windowWidth").toInt(),1234);
        controller.setPreference("windowWidth",1400);
        controller.flushPreferences();
        language.setLanguage("zh_CN");
        QSettings copied(storage.settingsFile(),QSettings::IniFormat);
        QCOMPARE(copied.value("windowWidth").toInt(),1400);
        QCOMPARE(copied.value("language").toString(),QString("zh_CN"));
        QCOMPARE(copied.value("closeToTray").toBool(),false);
        SettingsStorage restored(bootstrap);
        QCOMPARE(restored.settingsFile(),storage.settingsFile());
        const auto conflictFolder = temporary.filePath("existing");
        QVERIFY(QDir().mkpath(conflictFolder));
        const auto conflictPath = QDir(conflictFolder).filePath("settings.ini");
        { QFile file(conflictPath); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("do not overwrite"); }
        QSignalSpy errors(&storage,&SettingsStorage::errorOccurred);
        QVERIFY(!storage.setDirectory(QUrl::fromLocalFile(conflictFolder)));
        QCOMPARE(errors.count(),1);
        QCOMPARE(storage.directory(),newFolder);
        QFile conflict(conflictPath); QVERIFY(conflict.open(QIODevice::ReadOnly));
        QCOMPARE(conflict.readAll(),QByteArray("do not overwrite"));
        QVERIFY(!storage.setDirectory(QUrl("https://example.com")));
    }
    void testDesktopIntegration()
    {
        QIcon icon(":/icons/packet-capture.ico");
        QVERIFY(!icon.isNull());
        for (int size : {16,32,48,256}) QVERIFY(!icon.pixmap(size,size).isNull());
        DesktopIntegration desktop;
        if (QGuiApplication::platformName() == "windows") QVERIFY(desktop.trayAvailable());
        const auto actions = desktop.menu()->actions();
        QCOMPARE(actions.size(),4);
        QSignalSpy show(&desktop,&DesktopIntegration::showRequested);
        QSignalSpy capture(&desktop,&DesktopIntegration::toggleCaptureRequested);
        QSignalSpy exit(&desktop,&DesktopIntegration::exitRequested);
        QVERIFY(!actions[1]->isEnabled());
        desktop.setCanStart(true);
        QVERIFY(actions[1]->isEnabled());
        actions[0]->trigger(); actions[1]->trigger(); actions[3]->trigger();
        QCOMPARE(show.count(),1); QCOMPARE(capture.count(),1); QCOMPARE(exit.count(),1);
        const auto stoppedText = actions[1]->text();
        desktop.setCapturing(true); desktop.setCanStart(false);
        QVERIFY(actions[1]->isEnabled());
        QVERIFY(actions[1]->text() != stoppedText);
        desktop.setCapturing(false);
        QVERIFY(!actions[1]->isEnabled());
    }
    void testLanguagePreferenceRestored()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("language.ini");
        qputenv("PACKET_CAPTURE_SETTINGS_FILE", path.toUtf8());
        {
            LanguageSettings settings;
            QCOMPARE(settings.language(), QString("zh_CN"));
            settings.setLanguage("en_US");
            QCOMPARE(settings.language(), QString("en_US"));
        }
        {
            LanguageSettings restored;
            QCOMPARE(restored.language(), QString("en_US"));
            restored.setLanguage("unsupported");
            QCOMPARE(restored.language(), QString("en_US"));
            restored.setLanguage("zh_CN");
        }
    }
    void testWindowLoadsAndRenders()
    {
#ifdef Q_OS_WIN
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/consola.ttf");
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyhbd.ttc");
        QGuiApplication::setFont(QFont("Microsoft YaHei", 9));
#endif
        QTemporaryDir preferences;
        qputenv("PACKET_CAPTURE_SETTINGS_FILE",preferences.filePath("prefs.ini").toUtf8());
        SettingsStorage storage(preferences.filePath("location.ini"));
        qputenv("PACKET_CAPTURE_SETTINGS_FILE",storage.settingsFile().toUtf8());
        DesktopIntegration desktopIntegration;
        CaptureController controller;
        LanguageSettings languageSettings;
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("languageSettings", &languageSettings);
        engine.rootContext()->setContextProperty("settingsStorage", &storage);
        engine.rootContext()->setContextProperty("desktopIntegration", &desktopIntegration);
        languageSettings.attachEngine(&engine);
        engine.addImportPath(QCoreApplication::applicationDirPath() + "/../qml");
        engine.rootContext()->setContextProperty("captureController", &controller);
        engine.rootContext()->setContextProperty("packetModel", controller.getDisplayModel());
        QList<QQmlError> warnings;
        connect(&engine, &QQmlEngine::warnings, this,
            [&](const QList<QQmlError>& errors) { warnings.append(errors); });
        engine.load(QUrl("qrc:/MainWindow.qml"));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        QVERIFY(window);
        QTest::qWait(400);
        auto* languageButton = window->findChild<QQuickItem*>("languageButton");
        auto* languageMenu = window->findChild<QObject*>("languageMenu");
        auto* englishAction = window->findChild<QObject*>("englishLanguageAction");
        auto* chineseAction = window->findChild<QObject*>("chineseLanguageAction");
        auto* aboutButton = window->findChild<QQuickItem*>("aboutButton");
        auto* settingsButton = window->findChild<QQuickItem*>("settingsButton");
        auto* settingsDialog = window->findChild<QObject*>("settingsDialog");
        QVERIFY(settingsButton && settingsDialog);
        auto* settingsMenu = window->findChild<QObject*>("settingsMenu");
        QVERIFY(settingsMenu);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,
            settingsButton->mapToScene(QPointF(settingsButton->width()/2,settingsButton->height()/2)).toPoint());
        QTRY_VERIFY(settingsMenu->property("visible").toBool());
        QTest::qWait(200);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/settings-menu-preview.png"));
        QVERIFY(QMetaObject::invokeMethod(settingsMenu,"close"));
        QTest::qWait(200);
        auto* settingsAction = window->findChild<QObject*>("settingsDirectoryAction");
        QVERIFY(settingsAction);
        QVERIFY(QMetaObject::invokeMethod(settingsAction,"triggered"));
        QTRY_VERIFY(settingsDialog->property("visible").toBool());
        QTest::qWait(120);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/settings-directory-preview.png"));
        QVERIFY(QMetaObject::invokeMethod(settingsDialog,"close"));
        QTRY_VERIFY(!settingsDialog->property("visible").toBool());
        QTest::qWait(200);
        auto* aboutDialog = window->findChild<QObject*>("aboutDialog");
        auto* aboutVersion = window->findChild<QObject*>("aboutVersion");
        auto* githubLink = window->findChild<QObject*>("githubLink");
        QVERIFY(languageButton && languageMenu && englishAction && chineseAction);
        QVERIFY(aboutButton && aboutDialog && aboutVersion && githubLink);
        auto* topToolBar = window->findChild<QQuickItem*>("topToolBar");
        QVERIFY(topToolBar);
        for (auto* button : {languageButton, aboutButton, settingsButton}) {
            const auto position = button->mapToItem(topToolBar, QPointF());
            QVERIFY(position.y() >= 0);
            QVERIFY(position.y()+button->height() <= topToolBar->height());
            QVERIFY(position.x()+button->width() < window->width()/2);
        }
        auto* capacity = window->findChild<QObject*>("capacitySelector");
        auto* statusSelection = window->findChild<QObject*>("statusFilter");
        QVERIFY(capacity && statusSelection);
        const auto capacityIndex = capacity->property("currentIndex");
        statusSelection->setProperty("currentIndex", 3);
        const auto chineseFamily = languageSettings.fontFamily();
        QCoreApplication::setApplicationVersion("1.0.0");
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            languageButton->mapToScene(QPointF(languageButton->width()/2, languageButton->height()/2)).toPoint());
        QTRY_VERIFY(languageMenu->property("visible").toBool());
        QTest::qWait(120);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/language-titlebar-preview.png"));
        QVERIFY(QMetaObject::invokeMethod(englishAction, "triggered"));
        QVERIFY(QMetaObject::invokeMethod(languageMenu, "close"));
        QTRY_COMPARE(languageSettings.language(), QString("en_US"));
        QTRY_COMPARE(window->title(), QString("Packet Capture and Analysis"));
        QCOMPARE(languageButton->property("text").toString(), QString("Language"));
        QCOMPARE(aboutButton->property("text").toString(), QString("About"));
        QCOMPARE(settingsButton->property("text").toString(), QString("Settings"));
        QCOMPARE(capacity->property("currentIndex"), capacityIndex);
        QCOMPARE(statusSelection->property("currentIndex").toInt(), 3);
        QCOMPARE(englishAction->property("font").value<QFont>().family(), languageSettings.fontFamily());
        QCOMPARE(QGuiApplication::font().family(), languageSettings.fontFamily());
        QSettings savedLanguage(storage.settingsFile(), QSettings::IniFormat);
        QCOMPARE(savedLanguage.value("language").toString(), QString("en_US"));
        QCOMPARE(QCoreApplication::translate("Backend", "未加载协议配置"), QString("No protocol configuration loaded"));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            aboutButton->mapToScene(QPointF(aboutButton->width()/2, aboutButton->height()/2)).toPoint());
        QTRY_VERIFY(aboutDialog->property("visible").toBool());
        QCOMPARE(aboutVersion->property("text").toString(), QString("Version: 1.0.0"));
        QVERIFY(githubLink->property("text").toString().contains("https://github.com/RookieLinux/PacketCaptureTool"));
        QTest::qWait(160);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/about-english-preview.png"));
        QVERIFY(QMetaObject::invokeMethod(chineseAction, "triggered"));
        QTRY_COMPARE(window->title(), QStringLiteral("报文采集与分析"));
        QCOMPARE(languageSettings.fontFamily(), chineseFamily);
        QCOMPARE(aboutDialog->property("title").toString(), QStringLiteral("关于软件"));
        QTest::qWait(120);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/about-chinese-preview.png"));
        QVERIFY(QMetaObject::invokeMethod(aboutDialog, "close"));
        QTRY_VERIFY(!aboutDialog->property("visible").toBool());
        QTest::qWait(120);
        statusSelection->setProperty("currentIndex", 0);
        verifyPacketSelectionAfterStatusFilter(controller,window);
        QVERIFY(!QTest::currentTestFailed());
        auto* interfaces = window->findChild<QQuickItem*>("interfaceSelector");
        QVERIFY(interfaces);
        const QVariant originalInterfaces = interfaces->property("model");
        const int originalInterfaceIndex = interfaces->property("currentIndex").toInt();
        const QString longName = QString("Intel(R) Ethernet Connection / USB Network Adapter / Virtual Network Interface ").repeated(5);
        QVariantList interfaceFixtures;
        for (int i = 0; i < 24; ++i) {
            const QString name = i == 0 ? longName : QString("测试网卡 %1").arg(i);
            interfaceFixtures.append(QVariantMap{{"name",QString("test-interface-%1").arg(i)},
                {"description",name},{"ipAddress",QString("192.0.2.%1").arg(i+1)},
                {"displayName",name+QString(" (192.0.2.%1)").arg(i+1)}});
        }
        QVERIFY(interfaces->setProperty("model", interfaceFixtures));
        interfaces->setProperty("currentIndex",0);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,interfaces->mapToScene(QPointF(interfaces->width()/2,interfaces->height()/2)).toPoint());
        auto* interfacePopup = interfaces->findChild<QObject*>("interfacePopup");
        QVERIFY(interfacePopup);
        QTRY_VERIFY(interfacePopup->property("visible").toBool());
        QTest::qWait(120);
        QVERIFY(interfacePopup->property("width").toReal() > interfaces->width());
        QVERIFY(interfacePopup->property("width").toReal() <= window->width()-24);
        auto* fullName = findVisualItem(window->contentItem(),"interfaceName_0");
        QVERIFY(fullName);
        QCOMPARE(fullName->property("text").toString(),longName);
        QVERIFY(fullName->property("lineCount").toInt() > 1);
        QVERIFY(!fullName->property("truncated").toBool());
        auto* options = interfaces->findChild<QObject*>("interfaceOptions");
        QVERIFY(options);
        QVERIFY(options->property("contentHeight").toReal() > options->property("height").toReal());
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/interface-selector-preview.png"));
        QVERIFY(QMetaObject::invokeMethod(window,"setDarkMode",Q_ARG(QVariant,true)));
        QTest::qWait(120);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/interface-selector-dark-preview.png"));
        QVERIFY(QMetaObject::invokeMethod(window,"setDarkMode",Q_ARG(QVariant,false)));
        QTest::keyClick(window,Qt::Key_Down);
        QTest::keyClick(window,Qt::Key_Return);
        QTRY_COMPARE(interfaces->property("currentIndex").toInt(),1);
        QTRY_VERIFY(!interfacePopup->property("visible").toBool());
        window->setWidth(1000);
        window->setHeight(650);
        QTest::qWait(120);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,interfaces->mapToScene(QPointF(interfaces->width()/2,interfaces->height()/2)).toPoint());
        QTRY_VERIFY(interfacePopup->property("visible").toBool());
        QTest::qWait(120);
        auto* interfaceListItem = qobject_cast<QQuickItem*>(options);
        QVERIFY(interfaceListItem);
        const auto popupLeft = interfaceListItem->mapToScene(QPointF(0,0)).x();
        QVERIFY(popupLeft >= 0);
        QVERIFY(popupLeft + interfaceListItem->width() <= window->width());
        auto* secondOption = findVisualItem(window->contentItem(),"interfaceOption_2");
        QVERIFY(secondOption);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,secondOption->mapToScene(QPointF(secondOption->width()/2,secondOption->height()/2)).toPoint());
        QTRY_COMPARE(interfaces->property("currentIndex").toInt(),2);
        QTRY_VERIFY(!interfacePopup->property("visible").toBool());
        QCOMPARE(controller.preferences().value("interface").toString(),QString("test-interface-2"));
        window->setWidth(1300);
        window->setHeight(850);
        QVERIFY(interfaces->setProperty("model",originalInterfaces));
        interfaces->setProperty("currentIndex",originalInterfaceIndex);
        auto* pause = window->findChild<QObject*>("pauseDisplay");
        QVERIFY(pause);
        controller.setDisplayPaused(true);
        QCOMPARE(pause->property("checked").toBool(), true);
        controller.setDisplayPaused(false);
        QVERIFY(window->findChild<QObject*>("capacitySelector"));
        QVERIFY(controller.loadPacketsAsync("missing-preview.pcap"));
        controller.cancelLoad();
        QTRY_VERIFY(!controller.isLoading());
        auto* search = window->findChild<QObject*>("packetSearch");
        QVERIFY(search);
        search->setProperty("text", "192.0.2.1");
        QTest::qWait(250);
        QCOMPARE(controller.visiblePackets(), 0);
        RawPacketOfTool packet{};
        packet.sourceIP = "192.0.2.1";
        packet.sessionId = controller.findChild<PacketCaptureEngine*>()->sessionId();
        packet.destIP = "192.0.2.2";
        packet.protocol = TransportProtocol::UDP;
        packet.sourcePort = 1000;
        packet.destPort = 1234;
        packet.timestamp = 1700000000123456;
        packet.data = QByteArray::fromHex("0004fffe");
        packet.length = 4;
        ProtocolConfiguration config{};
        config.protocolName = "Preview";
        config.transportType = TransportProtocol::UDP;
        config.port = 1234;
        config.isFixedLength = true;
        config.fixedLength = 4;
        config.fields.append({"Length", 0, 2, FieldType::UInt16, Endianness::BigEndian});
        config.fields.append({"Value", 2, 2, FieldType::Int16, Endianness::BigEndian});
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        ConfigurationLoader loader;
        QString error;
        QVERIFY(loader.saveToFile(config, dir.filePath("preview.json"), error));
        QVERIFY(controller.loadProtocolConfig(dir.filePath("preview.json")));
        QVERIFY(QMetaObject::invokeMethod(&controller, "onPacketCaptured", Qt::DirectConnection,
            Q_ARG(RawPacketOfTool, packet)));
        QCOMPARE(controller.visiblePackets(), 1);
        auto* list = window->findChild<QObject*>("packetList");
        QVERIFY(list);
        list->setProperty("currentIndex", 0);
        auto* details = window->findChild<QObject*>("packetDetails");
        QVERIFY(details);
        QVERIFY(QMetaObject::invokeMethod(details,"selectByte",Q_ARG(QVariant,2)));
        QCOMPARE(details->property("highlightedOffset").toInt(),2);
        QCOMPARE(details->property("highlightedLength").toInt(),2);
        auto* valueField=findVisualItem(window->contentItem(),"parsedField_Value");
        QTRY_VERIFY((valueField=findVisualItem(window->contentItem(),"parsedField_Value")));
        QTRY_VERIFY(valueField->width() > 100 && valueField->height() > 20);
        auto* hexViewport = findVisualItem(window->contentItem(), "parsedHexPreview");
        auto* hexHandle = findVisualItem(window->contentItem(), "parsedHexResizeHandle");
        QVERIFY(hexViewport && hexHandle);
        const auto originalHexHeight = hexViewport->height();
        const auto originalDetails = details->property("packetDetails").toMap();
        const auto handlePoint = hexHandle->mapToScene(QPointF(hexHandle->width()/2, hexHandle->height()/2)).toPoint();
        const auto resizedPoint = handlePoint + QPoint(0, 80);
        QTest::mouseMove(window, handlePoint);
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, handlePoint);
        QTest::mouseMove(window, resizedPoint, 60);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, resizedPoint);
        QTRY_VERIFY(hexViewport->height() < originalHexHeight - 30);
        QCOMPARE(details->property("packetDetails").toMap(), originalDetails);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/hex-resized-preview.png"));
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, resizedPoint);
        QTest::mouseMove(window, handlePoint, 60);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, handlePoint);
        QTRY_VERIFY(qAbs(hexViewport->height() - originalHexHeight) < 5);
        auto* hexText = findVisualItem(window->contentItem(), "frameHex");
        QVERIFY(hexText);
        auto largePreview = originalDetails;
        largePreview["rawData"] = QString("000102030405060708090a0b0c0d0e0f").repeated(512);
        largePreview["length"] = 8192;
        QVERIFY(details->setProperty("packetDetails", largePreview));
        QTRY_VERIFY(hexViewport->property("contentHeight").toReal() > hexViewport->height());
        const auto originalWindowWidth = window->width();
        window->setWidth(1000);
        QTRY_VERIFY(hexViewport->property("contentWidth").toReal() > hexViewport->width());
        QCOMPARE(hexText->property("height").toReal(), hexViewport->property("contentHeight").toReal());
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/hex-large-packet-preview.png"));
        verifyHexCopy(hexText, window, largePreview["rawData"].toString());
        QVERIFY(!QTest::currentTestFailed());
        auto* detailTabs = window->findChild<QObject*>("packetDetailsTabs");
        QVERIFY(detailTabs);
        detailTabs->setProperty("currentIndex",1);
        QQuickItem* fullFrameHex = nullptr;
        QTRY_VERIFY((fullFrameHex=findVisualItem(window->contentItem(),"fullFrameHex")));
        verifyHexCopy(fullFrameHex, window, largePreview["rawData"].toString());
        QVERIFY(!QTest::currentTestFailed());
        detailTabs->setProperty("currentIndex",0);
        QVERIFY(details->setProperty("packetDetails", originalDetails));
        window->setWidth(originalWindowWidth);
        QVERIFY(QMetaObject::invokeMethod(details,"selectByte",Q_ARG(QVariant,2)));
        QTest::mouseClick(window,Qt::RightButton,Qt::NoModifier,valueField->mapToScene(QPointF(valueField->width()/2,valueField->height()/2)).toPoint());
        auto* contextMenu=valueField->findChild<QObject*>("fieldContextMenu");
        QVERIFY(contextMenu);
        QTRY_VERIFY(contextMenu->property("visible").toBool());
        QTest::qWait(80);
        auto* fieldAction=valueField->findChild<QQuickItem*>("fieldFilterEquals");
        QVERIFY(fieldAction);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,fieldAction->mapToScene(QPointF(fieldAction->width()/2,fieldAction->height()/2)).toPoint());
        QTRY_COMPARE(controller.getDisplayModel()->conditions().size(),1);
        QCOMPARE(controller.getDisplayModel()->conditions().first().toMap().value("column").toString(),QString("field:Value"));
        controller.getDisplayModel()->clearConditions();
        auto* panel=window->findChild<QObject*>("analysisControls");
        QVERIFY(panel);
        QVERIFY(QMetaObject::invokeMethod(panel,"quickFilter",Q_ARG(QVariant,"Value"),Q_ARG(QVariant,"eq"),Q_ARG(QVariant,"-2")));
        QCOMPARE(controller.visiblePackets(),1);
        controller.getDisplayModel()->clearConditions();
        for (const QString& name : {QString("columnsDialog"),QString("conditionDialog"),QString("archiveDialog"),QString("helpDialog")}) {
            auto* dialog=window->findChild<QObject*>(name);
            QVERIFY(dialog);
            QVERIFY(QMetaObject::invokeMethod(dialog,"open"));
            QTest::qWait(160);
            QVERIFY(dialog->property("visible").toBool());
            const auto preview=window->grabWindow();
            QVERIFY(!preview.isNull());
            QVERIFY(preview.save(QCoreApplication::applicationDirPath()+"/"+name+"-preview.png"));
            QVERIFY(QMetaObject::invokeMethod(dialog,"close"));
            QTest::qWait(100);
        }
        details->setProperty("highlightedOffset", 2);
        details->setProperty("highlightedLength", 2);
        QTest::qWait(150);
        window->setWidth(1000);
        window->setHeight(650);
        QTest::qWait(100);
        QVERIFY(!window->grabWindow().isNull());
        window->setWidth(1300);
        window->setHeight(850);
        QTest::qWait(100);
        const QImage screenshot = window->grabWindow();
        QVERIFY(!screenshot.isNull());
        QVERIFY(screenshot.save(QCoreApplication::applicationDirPath() + "/interface-preview.png"));
        languageSettings.setLanguage("en_US");
        QVERIFY(QMetaObject::invokeMethod(details,"selectByte",Q_ARG(QVariant,2)));
        QTest::qWait(120);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/interface-english-preview.png"));
        languageSettings.setLanguage("zh_CN");
        QVERIFY(QMetaObject::invokeMethod(details,"selectByte",Q_ARG(QVariant,2)));
        QVERIFY(QMetaObject::invokeMethod(window,"setDarkMode",Q_ARG(QVariant,true)));
        QTest::qWait(100);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/interface-dark-preview.png"));
        for (const QString& name : {QString("columnsDialog"),QString("conditionDialog"),QString("archiveDialog"),QString("helpDialog")}) {
            auto* dialog=window->findChild<QObject*>(name);
            QVERIFY(dialog);
            QVERIFY(QMetaObject::invokeMethod(dialog,"open"));
            QTest::qWait(160);
            QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/"+name+"-dark-preview.png"));
            QVERIFY(QMetaObject::invokeMethod(dialog,"close"));
            QTest::qWait(100);
        }
        QVERIFY(QMetaObject::invokeMethod(window,"setDarkMode",Q_ARG(QVariant,false)));
        QTest::qWait(120);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/interface-fluent-preview.png"));
        window->setWidth(1000);
        window->setHeight(650);
        QTest::qWait(120);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/interface-compact-preview.png"));
        window->setWidth(1300);
        window->setHeight(850);
        QTest::qWait(120);
        controller.clearPackets();
        auto* capture=controller.findChild<PacketCaptureEngine*>();
        auto frame=QByteArray::fromHex("00112233445566778899aabb0800450000200001000040110000c0000201c000020203e804d2000c000001020304");
        timeval time{};
        pcpp::RawPacket raw(reinterpret_cast<const uint8_t*>(frame.constData()),frame.size(),time,false);
        PacketCaptureEngine::onPacketArrivesStatic(&raw,nullptr,capture);
        frame.replace(34,2,QByteArray::fromHex("07d0"));
        pcpp::RawPacket second(reinterpret_cast<const uint8_t*>(frame.constData()),frame.size(),time,false);
        PacketCaptureEngine::onPacketArrivesStatic(&second,nullptr,capture);
        QTRY_COMPARE(controller.totalPackets(),2);
        auto* browser=window->findChild<QObject*>("packetBrowser");
        QVERIFY(browser);
        QTRY_VERIFY(!browser->property("changing").toBool());
        QVERIFY(QMetaObject::invokeMethod(browser,"selectRow",Q_ARG(QVariant,0)));
        controller.getDisplayModel()->sortColumn("sourcePort",true);
        QTRY_COMPARE(list->property("currentIndex").toInt(),1);
        QCOMPARE(details->property("packetDetails").toMap().value("sequence").toULongLong(),1);
        details->setProperty("highlightedOffset",2);
        details->setProperty("highlightedLength",2);
        frame.replace(34,2,QByteArray::fromHex("0bb8"));
        pcpp::RawPacket third(reinterpret_cast<const uint8_t*>(frame.constData()),frame.size(),time,false);
        PacketCaptureEngine::onPacketArrivesStatic(&third,nullptr,capture);
        QTRY_COMPARE(controller.totalPackets(),3);
        QTRY_COMPARE(list->property("currentIndex").toInt(),2);
        QCOMPARE(details->property("packetDetails").toMap().value("sequence").toULongLong(),1);
        QCOMPARE(details->property("highlightedOffset").toInt(),2);
        auto* portHeader=findVisualItem(window->contentItem(),"sort_sourcePort");
        QVERIFY(portHeader);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,portHeader->mapToScene(QPointF(portHeader->width()/2,portHeader->height()/2)).toPoint());
        QTRY_COMPARE(list->property("currentIndex").toInt(),0);
        QCOMPARE(details->property("packetDetails").toMap().value("sequence").toULongLong(),1);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,portHeader->mapToScene(QPointF(portHeader->width()/2,portHeader->height()/2)).toPoint());
        QTRY_COMPARE(list->property("currentIndex").toInt(),2);
        QVERIFY(controller.hasUnsavedChanges());
        window->hide();
        QVERIFY(QMetaObject::invokeMethod(&desktopIntegration,"showRequested"));
        QTRY_VERIFY(window->isVisible());
        window->hide();
        QVERIFY(QMetaObject::invokeMethod(&desktopIntegration,"exitRequested"));
        QTRY_VERIFY(window->isVisible());
        auto* unsaved=window->findChild<QObject*>("unsavedDialog");
        QVERIFY(unsaved);
        QVERIFY(unsaved->property("visible").toBool());
        QCOMPARE(capture->getCapturedPackets().size(),3);
        QTest::qWait(160);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/unsavedDialog-preview.png"));
        QVERIFY(QMetaObject::invokeMethod(window,"setDarkMode",Q_ARG(QVariant,true)));
        QTest::qWait(120);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/unsavedDialog-dark-preview.png"));
        QVERIFY(QMetaObject::invokeMethod(window,"setDarkMode",Q_ARG(QVariant,false)));
        QTest::qWait(120);
        auto* cancel=window->findChild<QQuickItem*>("unsavedCancel");
        QVERIFY(cancel);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,cancel->mapToScene(QPointF(cancel->width()/2,cancel->height()/2)).toPoint());
        QTRY_VERIFY(!unsaved->property("visible").toBool());
        QCOMPARE(capture->getCapturedPackets().size(),3);
        QVERIFY(QMetaObject::invokeMethod(panel,"moveColumn",Q_ARG(QVariant,0),Q_ARG(QVariant,1)));
        QVERIFY(controller.preferences().value("columns").toString().startsWith("[{\"key\":\"timestamp\""));
        const auto columnsBeforeLanguageChange = controller.preferences().value("columns");
        const auto selectedPacketBeforeLanguageChange = list->property("currentIndex");
        languageSettings.setLanguage("en_US");
        QCOMPARE(controller.preferences().value("columns"), columnsBeforeLanguageChange);
        QCOMPARE(list->property("currentIndex"), selectedPacketBeforeLanguageChange);
        const auto translatedColumns = panel->property("visibleColumns").value<QJSValue>().toVariant().toList();
        QVERIFY(!translatedColumns.isEmpty());
        QCOMPARE(translatedColumns.first().toMap().value("key").toString(), QString("timestamp"));
        QCOMPARE(translatedColumns.first().toMap().value("title").toString(), QString("Time"));
        languageSettings.setLanguage("zh_CN");
        controller.clearPackets();
        config.transportType=TransportProtocol::TCP;
        config.port=8080;
        QVERIFY(loader.saveToFile(config,dir.filePath("tcp-preview.json"),error));
        QVERIFY(controller.loadProtocolConfig(dir.filePath("tcp-preview.json")));
        const auto tcpFrame=QByteArray::fromHex("00112233445566778899aabb08004500002c0001000040060000c0000201c000020203e81f900000006400000000501820000000000001020304");
        pcpp::RawPacket tcpRaw(reinterpret_cast<const uint8_t*>(tcpFrame.constData()),tcpFrame.size(),time,false);
        PacketCaptureEngine::onPacketArrivesStatic(&tcpRaw,nullptr,capture);
        QTRY_COMPARE(controller.totalPackets(),1);
        QVERIFY(controller.analyzeTcpStreams());
        QTRY_VERIFY(!controller.tcpAnalysisBusy());
        QCOMPARE(controller.tcpAnalysis().value("messages").toList().size(),1);
        auto* tcpDialog=window->findChild<QObject*>("tcpAnalysisDialog");
        QVERIFY(tcpDialog);
        QVERIFY(QMetaObject::invokeMethod(tcpDialog,"open"));
        QTest::qWait(2100); // Let transient notifications disappear from documentation screenshots.
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/tcp-analysis-preview.png"));
        languageSettings.setLanguage("en_US");
        QVERIFY(controller.analyzeTcpStreams());
        QTRY_VERIFY(!controller.tcpAnalysisBusy());
        QTest::qWait(120);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/tcp-analysis-english-preview.png"));
        languageSettings.setLanguage("zh_CN");
        QVERIFY(controller.analyzeTcpStreams());
        QTRY_VERIFY(!controller.tcpAnalysisBusy());
        QVERIFY(QMetaObject::invokeMethod(window,"setDarkMode",Q_ARG(QVariant,true)));
        QTest::qWait(120);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/tcp-analysis-dark-preview.png"));
        QVERIFY(QMetaObject::invokeMethod(tcpDialog,"close"));
        QVERIFY(controller.analyzeTcpStreams());
        controller.clearPackets();
        QTRY_VERIFY(!controller.tcpAnalysisBusy());
        QVERIFY(controller.tcpAnalysis().isEmpty());
        const auto activeDraft=controller.protocolDraft();
        auto* protocolEditor=window->findChild<QObject*>("protocolEditor");
        QVERIFY(protocolEditor);
        QVERIFY(QMetaObject::invokeMethod(protocolEditor,"open"));
        QTest::qWait(2100); // Keep the editor preview free of protocol-load notifications.
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/protocol-editor-preview.png"));
        auto* advancedField=findVisualItem(window->contentItem(),"protocolFieldAdvanced_0");
        QVERIFY(advancedField);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,advancedField->mapToScene(QPointF(advancedField->width()/2,advancedField->height()/2)).toPoint());
        auto* advancedDialog=window->findChild<QObject*>("protocolAdvancedDialog");
        QVERIFY(advancedDialog);
        QTRY_VERIFY(advancedDialog->property("visible").toBool());
        QTest::qWait(160);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/protocol-advanced-preview.png"));
        languageSettings.setLanguage("en_US");
        QTest::qWait(120);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/protocol-advanced-english-preview.png"));
        languageSettings.setLanguage("zh_CN");
        QVERIFY(QMetaObject::invokeMethod(advancedDialog,"close"));
        QTest::qWait(100);
        auto* sampleHex=window->findChild<QObject*>("protocolSampleHex");
        QVERIFY(sampleHex);
        sampleHex->setProperty("text","01 02 03 04");
        QVERIFY(QMetaObject::invokeMethod(protocolEditor,"selectMode",Q_ARG(QVariant,2)));
        QVERIFY(QMetaObject::invokeMethod(protocolEditor,"inspect"));
        QVERIFY(protocolEditor->property("preview").toMap().value("validPreview").toBool());
        QTest::qWait(120);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/protocol-preview-dark.png"));
        QCOMPARE(controller.protocolDraft(),activeDraft);
        QVERIFY(!controller.previewProtocolDraft(activeDraft,"0G").value("validPreview").toBool());
        QVERIFY(!controller.previewProtocolDraft(activeDraft,"123").value("validPreview").toBool());
        QVERIFY(!controller.applyProtocolDraft("{}"));
        QCOMPARE(controller.protocolDraft(),activeDraft);
        const auto draftPath=dir.filePath("saved-editor.json");
        QVERIFY(controller.saveProtocolDraft(activeDraft,draftPath));
        QVERIFY(controller.readProtocolDraft(draftPath).contains("json"));
        QFile savedDraft(draftPath);
        QVERIFY(savedDraft.open(QIODevice::ReadOnly));
        const auto savedBytes=savedDraft.readAll(); savedDraft.close();
        QVERIFY(!controller.saveProtocolDraft("{}",draftPath));
        QVERIFY(savedDraft.open(QIODevice::ReadOnly));
        QCOMPARE(savedDraft.readAll(),savedBytes); savedDraft.close();
        QVERIFY(controller.applyProtocolDraft(activeDraft));
        QCOMPARE(controller.protocolDraft(),activeDraft);
        QVERIFY(QMetaObject::invokeMethod(window,"setDarkMode",Q_ARG(QVariant,false)));
        QVERIFY(QMetaObject::invokeMethod(protocolEditor,"selectMode",Q_ARG(QVariant,0)));
        QTest::qWait(120);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/protocol-editor-light-preview.png"));
        window->setWidth(1000); window->setHeight(650);
        QTest::qWait(120);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/protocol-editor-compact-preview.png"));
        QVERIFY(QMetaObject::invokeMethod(protocolEditor,"close"));
        QTest::qWait(120);
        languageSettings.setLanguage("en_US");
        QTest::qWait(120);
        for (auto* button : {languageButton, aboutButton}) {
            const auto position = button->mapToScene(QPointF());
            QVERIFY(position.x() >= 0);
            QVERIFY(position.x()+button->width() <= window->width());
        }
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/main-english-compact-preview.png"));
        QVERIFY(QMetaObject::invokeMethod(window,"setDarkMode",Q_ARG(QVariant,true)));
        QTest::qWait(120);
        QVERIFY(window->grabWindow().save(QCoreApplication::applicationDirPath()+"/main-english-compact-dark-preview.png"));
        QVERIFY(QMetaObject::invokeMethod(window,"setDarkMode",Q_ARG(QVariant,false)));
        languageSettings.setLanguage("zh_CN");
        for (const auto& warning : warnings) qWarning().noquote() << warning.toString();
        QVERIFY2(warnings.isEmpty(), "QML emitted runtime warnings");
    }
};

QTEST_MAIN(QmlInterfaceTests)
#include "test_qmlinterface.moc"
