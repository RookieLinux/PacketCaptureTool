#include <QtTest/QtTest>
#include "PacketCaptureEngine.h"
#include "CaptureController.h"
#include "PacketModel.h"
#include "ProtocolParser.h"
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QDataStream>

class PacketCaptureTests : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_preferences;
    static QString captureFile(const QTemporaryDir& dir, int count) {
        const auto path = dir.filePath(QString("batch-%1.pcap").arg(count));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) return {};
        file.write(QByteArray::fromHex("d4c3b2a1020004000000000000000000ffff000001000000"));
        const auto frame = QByteArray::fromHex("00112233445566778899aabb0800450000200001000040110000c0000201c000020203e804d2000c000001020304");
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        for (int i = 0; i < count; ++i) {
            stream << quint32(1700000000) << quint32(i) << quint32(frame.size()) << quint32(frame.size());
            file.write(frame);
        }
        return path;
    }

private slots:
    void initTestCase() {
        qputenv("PACKET_CAPTURE_SETTINGS_FILE", m_preferences.filePath("preferences.ini").toUtf8());
    }
    void testArchiveRotationAndLimits() {
        QTemporaryDir dir;
        const auto frame = QByteArray::fromHex("00112233445566778899aabb0800450000200001000040110000c0000201c000020203e804d2000c000001020304");
        CaptureArchive archive;
        QString error;
        ArchiveOptions options{dir.path(), 24 + 16 + frame.size(), 1, 10};
        QVERIFY(archive.start(options, error));
        RawPacketOfTool packet;
        packet.data = frame; packet.timestamp = 1700000000123456; packet.linkLayerType = 1;
        for (quint64 i=1; i<=3; ++i) { packet.sequence=i; QVERIFY(archive.enqueue(packet)); }
        QTRY_COMPARE(archive.state().writtenPackets, 3);
        archive.stop();
        QCOMPARE(archive.state().files.size(), 3);
        for (const auto& path : archive.state().files) {
            PacketCaptureEngine reader;
            QVERIFY2(reader.loadFromPcap(path, error), qPrintable(error));
            QCOMPARE(reader.getCapturedPackets().size(), 1);
            QCOMPARE(reader.getCapturedPackets().first().timestamp, packet.timestamp);
            QCOMPARE(reader.getCapturedPackets().first().data, frame);
        }
        const auto firstRun = archive.state().files;
        options.maxFiles=2;
        QVERIFY(archive.start(options,error));
        for (quint64 i=1; i<=3; ++i) { packet.sequence=i; QVERIFY(archive.enqueue(packet)); }
        QTRY_VERIFY(!archive.state().error.isEmpty());
        archive.stop();
        QCOMPARE(archive.state().writtenPackets,2);
        QCOMPARE(archive.state().files.size(),2);
        for (const auto& path : firstRun) QVERIFY(QFile::exists(path));
        QVERIFY(!firstRun.contains(archive.state().files.first()));
        options.queueBytes=frame.size()-1;
        QVERIFY(archive.start(options,error));
        QVERIFY(!archive.enqueue(packet));
        archive.stop();
        QVERIFY(!archive.state().error.isEmpty());
        QCOMPARE(archive.state().writtenPackets,0);
        options.directory=dir.filePath("missing-directory");
        QVERIFY(!archive.start(options,error));
    }

    void testArchiveTimeAndLinkRotation() {
        QTemporaryDir dir;
        CaptureArchive archive;
        QString error;
        QVERIFY(archive.start({dir.path(), 1024*1024, 1, 10},error));
        RawPacketOfTool packet;
        packet.sequence=1; packet.data=QByteArray::fromHex("00112233445566778899aabb0800450000200001000040110000c0000201c000020203e804d2000c000001020304");
        QVERIFY(archive.enqueue(packet));
        QTRY_COMPARE(archive.state().writtenPackets,1);
        QTest::qWait(1050);
        packet.sequence=2;
        QVERIFY(archive.enqueue(packet));
        QTRY_COMPARE(archive.state().writtenPackets,2);
        packet.sequence=3; packet.linkLayerType=113;
        packet.data=QByteArray::fromHex("00000001000666778899aabb00000800450000200001000040110000c0000201c000020203e804d2000c000001020304");
        QVERIFY(archive.enqueue(packet));
        QTRY_COMPARE(archive.state().writtenPackets,3);
        archive.stop();
        QCOMPARE(archive.state().files.size(),3);
        PacketCaptureEngine reader;
        QVERIFY(reader.loadFromPcap(archive.state().files.last(),error));
        QCOMPARE(reader.getCapturedPackets().first().linkLayerType,113);
    }

    void testRollingWindowAndUnsavedState() {
        QTemporaryDir dir;
        CaptureController controller;
        auto* engine=controller.findChild<PacketCaptureEngine*>();
        QString error;
        QVERIFY(engine->setBufferLimits({2,1024},error));
        QVERIFY(controller.configureArchive(true,dir.path(),1,60,10));
        const auto frame=QByteArray::fromHex("00112233445566778899aabb0800450000200001000040110000c0000201c000020203e804d2000c000001020304");
        timeval time{};
        pcpp::RawPacket raw(reinterpret_cast<const uint8_t*>(frame.constData()),frame.size(),time,false);
        for (int i=1; i<=6; ++i) {
            PacketCaptureEngine::onPacketArrivesStatic(&raw,nullptr,engine);
            QTRY_COMPARE(engine->archiveState().writtenPackets,i);
        }
        QTRY_COMPARE(controller.totalPackets(),2);
        QTRY_COMPARE(controller.getDisplayModel()->getPacketDetails(0).value("sequence").toULongLong(),5);
        QCOMPARE(controller.getDisplayModel()->getPacketDetails(1).value("sequence").toULongLong(),6);
        QCOMPARE(engine->bufferState().acceptedPackets,6);
        QCOMPARE(engine->bufferState().packets,2);
        QVERIFY(!controller.bufferFull());
        QVERIFY(!controller.hasUnsavedChanges());
        controller.stopCapture();
        const auto files=engine->archiveState().files;
        PacketCaptureEngine reader;
        QVERIFY(reader.loadFromPcap(files.first(),error));
        QCOMPARE(reader.getCapturedPackets().size(),6);
        controller.clearPackets();
        QVERIFY(controller.configureArchive(false,dir.path()));
        PacketCaptureEngine::onPacketArrivesStatic(&raw,nullptr,engine);
        QVERIFY(controller.hasUnsavedChanges());
        QVERIFY(!controller.savePackets(dir.filePath("missing/save.pcap")));
        QVERIFY(controller.hasUnsavedChanges());
        QVERIFY(controller.savePackets(dir.filePath("manual.pcap")));
        QVERIFY(!controller.hasUnsavedChanges());
        PacketCaptureEngine::onPacketArrivesStatic(&raw,nullptr,engine);
        QVERIFY(controller.hasUnsavedChanges());
        controller.clearPackets();
        QVERIFY(!controller.hasUnsavedChanges());
        QVERIFY(controller.loadPackets(dir.filePath("manual.pcap")));
        QVERIFY(!controller.hasUnsavedChanges());
        QVERIFY(!controller.captureHealth().value("available").toBool());
    }

    void testStructuredFiltersSortingAndReverseLookup() {
        PacketModel source;
        PacketDisplayModel display;
        display.setSourceModel(&source);
        RawPacketOfTool raw;
        raw.sourceIP="192.0.2.1"; raw.destIP="192.0.2.2"; raw.sourcePort=1000; raw.destPort=1234;
        raw.sequence=10; raw.data=QByteArray::fromHex("00010203");
        ParsedPacket parsed;
        parsed.status=ParseStatus::Parsed;
        parsed.fields.append({0,2,"Counter", QVariant::fromValue(quint64(18446744073709551614ULL)), "uint64", "18446744073709551614"});
        parsed.fields.append({1,2,"Overlap",42,"uint16","42"});
        source.addPacket(raw,parsed);
        raw.sequence=20; raw.sourcePort=2000;
        parsed.fields[0].value=QVariant::fromValue(quint64(18446744073709551615ULL));
        parsed.fields[0].displayValue="18446744073709551615";
        source.addPacket(raw,parsed);
        QVERIFY(display.setConditions({QVariantMap{{"column","sourcePort"},{"operator","ge"},{"value","1500"}},
            QVariantMap{{"column","field:Overlap"},{"operator","eq"},{"value","42"}}}));
        QCOMPARE(display.rowCount(),1);
        QCOMPARE(display.getPacketDetails(0).value("sequence").toULongLong(),20);
        QVERIFY(!display.setConditions({QVariantMap{{"column","length"},{"operator","gt"},{"value","NaN"}}}));
        QCOMPARE(display.rowCount(),1); // invalid editing keeps active filters
        QVERIFY(display.setConditions({QVariantMap{{"column","sourcePort"},{"operator","eq"},{"value","1000"}},
            QVariantMap{{"column","sourcePort"},{"operator","eq"},{"value","2000"}}},true));
        QCOMPARE(display.rowCount(),2);
        QVERIFY(display.setConditions({QVariantMap{{"column","field:Counter"},{"operator","eq"},{"value","18446744073709551614"}}}));
        QCOMPARE(display.rowCount(),1);
        display.clearConditions();
        display.sortColumn("sourcePort",true);
        QCOMPARE(display.rowForSequence(20),0);
        QCOMPARE(display.cellValue(0,"sourcePort").toInt(),2000);
        QCOMPARE(display.fieldsAtByte(0,1).size(),2);
        QCOMPARE(display.fieldsAtByte(0,3).size(),0);
        QCOMPARE(display.fieldsAtByte(-1,1).size(),0);
        display.sortColumn("field:Counter");
        QCOMPARE(display.rowForSequence(10),0);
        source.trimBefore(20);
        QCOMPARE(display.rowCount(),1);
        QCOMPARE(display.getPacketDetails(0).value("sequence").toULongLong(),20);
        QCOMPARE(display.rowForSequence(10),-1);
    }

    void testPreferencesSurviveControllerRestart() {
        CaptureController first;
        first.setPreference("interface","stable-device-id");
        first.setPreference("splitWidth",620);
        first.setPreference("columns","[{\"key\":\"sequence\",\"width\":120,\"visible\":true}]");
        first.setPreference("notAllowed",true);
        first.flushPreferences();
        CaptureController second;
        QCOMPARE(second.preferences().value("interface").toString(),QString("stable-device-id"));
        QCOMPARE(second.preferences().value("splitWidth").toInt(),620);
        QVERIFY(!second.preferences().contains("notAllowed"));
        first.setPreference("columns",QString());
        first.flushPreferences();
    }
    void testAsyncImportAndCancellation() {
        QTemporaryDir dir;
        const auto seed = captureFile(dir, 1);
        const auto many = captureFile(dir, 20000);
        CaptureController controller;
        QVERIFY(controller.loadPackets(seed));
        QSignalSpy finished(&controller, &CaptureController::fileLoadFinished);
        QVERIFY(controller.loadPacketsAsync(many));
        QVERIFY(controller.isLoading());
        QCOMPARE(controller.totalPackets(), 1);
        QVERIFY(!controller.loadPacketsAsync(seed));
        QVERIFY(!controller.loadProtocolConfig("missing.json"));
        controller.clearPackets();
        QCOMPARE(controller.totalPackets(), 1);
        controller.cancelLoad();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.isLoading(), 5000);
        QCOMPARE(controller.totalPackets(), 1);
        QVERIFY(finished.last().at(1).toBool());
        finished.clear();
        QTimer heartbeat;
        int ticks = 0;
        connect(&heartbeat, &QTimer::timeout, [&] { ++ticks; });
        heartbeat.start(1);
        QSignalSpy resets(controller.getPacketModel(), &QAbstractItemModel::modelReset);
        QVERIFY(controller.loadPacketsAsync(many));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.isLoading(), 10000);
        QVERIFY(ticks > 0);
        QCOMPARE(controller.totalPackets(), 20000);
        QCOMPARE(controller.unparsedPackets(), 20000);
        QVERIFY(finished.last().at(0).toBool());
        QCOMPARE(resets.size(), 2); // clear old session, then install one complete model
        QVERIFY(controller.savePackets(dir.filePath("async-saved.pcap")));
        PacketCaptureEngine reader;
        QString error;
        QVERIFY(reader.loadFromPcap(dir.filePath("async-saved.pcap"), error));
        QCOMPARE(reader.getCapturedPackets().size(), 20000);
        QVERIFY(controller.loadPacketsAsync("missing.pcap"));
        QTRY_VERIFY(!controller.isLoading());
        QCOMPARE(controller.totalPackets(), 20000);
        QVERIFY(controller.setBufferLimits(20000, 16));
        QVERIFY(controller.loadPacketsAsync(captureFile(dir, 20001)));
        QTRY_VERIFY(!controller.isLoading());
        QCOMPARE(controller.totalPackets(), 20000);
        QVERIFY(!finished.last().at(0).toBool());
        auto disposable = std::make_unique<CaptureController>();
        QVERIFY(disposable->loadPacketsAsync(many));
        disposable.reset(); // cancel and join without callback touching a destroyed controller
    }

    void testCaptureBatchesAndStaleLimitSignal() {
        CaptureController controller;
        auto* engine = controller.findChild<PacketCaptureEngine*>();
        QSignalSpy rows(controller.getPacketModel(), &QAbstractItemModel::rowsInserted);
        QSignalSpy errors(&controller, &CaptureController::errorOccurred);
        const auto frame = QByteArray::fromHex("00112233445566778899aabb0800450000200001000040110000c0000201c000020203e804d2000c000001020304");
        timeval time{};
        pcpp::RawPacket raw(reinterpret_cast<const uint8_t*>(frame.constData()), frame.size(), time, false);
        for (int i = 0; i < 3000; ++i) PacketCaptureEngine::onPacketArrivesStatic(&raw, nullptr, engine);
        QCOMPARE(controller.totalPackets(), 0);
        QTRY_COMPARE(controller.totalPackets(), 3000);
        QCOMPARE(rows.size(), 3);
        controller.clearPackets();
        QString error;
        QVERIFY(engine->setBufferLimits({1, 1024}, error));
        PacketCaptureEngine::onPacketArrivesStatic(&raw, nullptr, engine);
        controller.clearPackets();
        QTest::qWait(100);
        QCOMPARE(controller.totalPackets(), 0);
        QCOMPARE(controller.storedPackets(), 0);
        QVERIFY(errors.isEmpty());
    }

    void testBoundedCaptureAndPausedDisplay() {
        CaptureController controller;
        auto* engine = controller.findChild<PacketCaptureEngine*>();
        QVERIFY(engine);
        QString error;
        QVERIFY(engine->setBufferLimits({2, 1024}, error));
        QSignalSpy reached(engine, &PacketCaptureEngine::bufferLimitReached);
        QSignalSpy rows(controller.getPacketModel(), &QAbstractItemModel::rowsInserted);
        const auto frame = QByteArray::fromHex("00112233445566778899aabb0800450000200001000040110000c0000201c000020203e804d2000c000001020304");
        timeval time{};
        pcpp::RawPacket raw(reinterpret_cast<const uint8_t*>(frame.constData()), frame.size(), time, false);
        controller.setDisplayPaused(true);
        for (int i = 0; i < 3; ++i) PacketCaptureEngine::onPacketArrivesStatic(&raw, nullptr, engine);
        QTest::qWait(100);
        QCOMPARE(controller.totalPackets(), 0);
        QCOMPARE(controller.storedPackets(), 2);
        QCOMPARE(controller.rejectedPackets(), 1);
        QVERIFY(controller.bufferFull());
        QCOMPARE(reached.size(), 1);
        QTemporaryDir dir;
        QVERIFY(controller.savePackets(dir.filePath("paused.pcap")));
        PacketCaptureEngine saved;
        QVERIFY(saved.loadFromPcap(dir.filePath("paused.pcap"), error));
        QCOMPARE(saved.getCapturedPackets().size(), 2);
        controller.setDisplayPaused(false);
        QTRY_COMPARE(controller.totalPackets(), 2);
        QCOMPARE(rows.size(), 1);
        controller.clearPackets();
        QVERIFY(!controller.bufferFull());
        QCOMPARE(controller.rejectedPackets(), 0);
        QVERIFY(engine->takePendingPackets().isEmpty());
        QVERIFY(engine->setBufferLimits({100, frame.size() * 2 - 1}, error));
        for (int i = 0; i < 2; ++i) PacketCaptureEngine::onPacketArrivesStatic(&raw, nullptr, engine);
        QCOMPARE(engine->bufferState().packets, 1);
        QVERIFY(engine->bufferState().full);
        QVERIFY(!engine->setBufferLimits({1, 1}, error));
        std::atomic_bool cancel{true};
        const auto result = PacketCaptureEngine::readCaptureFile(captureFile(dir, 3), {}, &cancel);
        QVERIFY(result.canceled);
        QVERIFY(result.packets.isEmpty());
    }
    void testNetworkProtocolsAndEthernetPadding()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QList<QByteArray> frames{
            QByteArray::fromHex("00112233445566778899aabb0800450000200001000040110000c0000201c0000202"
                                "03e804d2000c000001020304") + QByteArray(14, '\0'),
            QByteArray::fromHex("00112233445566778899aabb86dd60000000000c1140"
                "20010db800000000000000000000000120010db8000000000000000000000002"
                "03e804d2000c000001020304"),
            QByteArray::fromHex("00112233445566778899aabb08060001080006040001"
                "66778899aabbc0000201000000000000c0000202"),
            QByteArray::fromHex("00112233445566778899aabb08004500001c0001000040010000c0000201c0000202"
                "0800000000010001"),
            QByteArray::fromHex("00112233445566778899aabb86dd6000000000083a40"
                "20010db800000000000000000000000120010db8000000000000000000000002"
                "8000000000010001")
        };
        QFile file(dir.filePath("network.pcap"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray::fromHex("d4c3b2a1020004000000000000000000ffff000001000000"));
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        for (const auto& frame : frames) {
            stream << quint32(1700000000) << quint32(0) << quint32(frame.size()) << quint32(frame.size());
            file.write(frame);
        }
        file.close();
        PacketCaptureEngine engine;
        QString error;
        QVERIFY2(engine.loadFromPcap(file.fileName(), error), qPrintable(error));
        const auto captured = engine.getCapturedPackets();
        QCOMPARE(captured.size(), 5);
        QCOMPARE(captured[0].payloadLength, 4);
        QCOMPARE(captured[1].sourceIP, QString("2001:db8::1"));
        QCOMPARE(captured[1].protocolName, QString("UDP/IPv6"));
        QCOMPARE(captured[2].protocolName, QString("ARP"));
        QCOMPARE(captured[3].protocolName, QString("ICMP"));
        QCOMPARE(captured[4].protocolName, QString("ICMPv6"));
        ProtocolConfiguration config{};
        config.transportType = TransportProtocol::UDP;
        config.port = 1234;
        config.isFixedLength = true;
        config.fixedLength = 4;
        ProtocolParser parser;
        parser.setProtocolConfig(config);
        QVERIFY(parser.parsePacket(captured[0]).isValid);
        QVERIFY(parser.parsePacket(captured[1]).isValid);
        QCOMPARE(parser.parsePacket(captured[2]).status, ParseStatus::Unmatched);
        QVERIFY(engine.saveToPcap(dir.filePath("saved.pcap"), error));
        QVERIFY(engine.loadFromPcap(dir.filePath("saved.pcap"), error));
        QCOMPARE(engine.getCapturedPackets().size(), 5);
        pcpp::PcapNgFileWriterDevice ngWriter(dir.filePath("network.pcapng").toStdString());
        QVERIFY(ngWriter.open());
        for (const auto& packet : captured) {
            timeval timestamp{};
            timestamp.tv_sec = 1700000000;
            pcpp::RawPacket raw(reinterpret_cast<const uint8_t*>(packet.data.constData()),
                packet.data.size(), timestamp, false);
            QVERIFY(ngWriter.writePacket(raw));
        }
        ngWriter.close();
        QVERIFY(engine.loadFromPcap(dir.filePath("network.pcapng"), error));
        QCOMPARE(engine.getCapturedPackets().size(), 5);
    }

    void testCookedLinkTypeSurvivesSave()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QByteArray frame = QByteArray::fromHex(
            "00000001000666778899aabb00000800450000200001000040110000c0000201c0000202"
            "03e804d2000c000001020304");
        QFile file(dir.filePath("cooked.pcap"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray::fromHex("d4c3b2a1020004000000000000000000ffff000071000000"));
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << quint32(1700000000) << quint32(0) << quint32(frame.size()) << quint32(frame.size());
        file.write(frame);
        file.close();
        PacketCaptureEngine engine;
        QString error;
        QVERIFY(engine.loadFromPcap(file.fileName(), error));
        QCOMPARE(engine.getCapturedPackets().size(), 1);
        QCOMPARE(engine.getCapturedPackets().first().networkOffset, 16);
        QVERIFY(engine.saveToPcap(dir.filePath("saved.pcap"), error));
        QVERIFY(engine.loadFromPcap(dir.filePath("saved.pcap"), error));
        QCOMPARE(engine.getCapturedPackets().size(), 1);
        QCOMPARE(engine.getCapturedPackets().first().linkLayerType, 113);
        QCOMPARE(engine.getCapturedPackets().first().data, frame);
    }

    void testDisplayFiltersPreserveSourceAndPacketIdentity()
    {
        PacketModel source;
        PacketDisplayModel display;
        display.setSourceModel(&source);
        RawPacketOfTool udp{};
        udp.protocol = TransportProtocol::UDP;
        udp.sourceIP = "192.0.2.1";
        udp.destIP = "192.0.2.2";
        udp.destPort = 1234;
        udp.data = QByteArray::fromHex("ab12cd34");
        RawPacketOfTool tcp = udp;
        tcp.protocol = TransportProtocol::TCP;
        tcp.sourceIP = "198.51.100.1";
        ParsedPacket parsed{};
        parsed.isValid = true;
        parsed.status = ParseStatus::Parsed;
        source.addPacket(udp, parsed);
        parsed.isValid = false;
        parsed.status = ParseStatus::Failed;
        source.addPacket(tcp, parsed);
        display.setTransportFilter(1);
        QCOMPARE(display.rowCount(), 1);
        QCOMPARE(display.getPacketDetails(0)["index"].toInt(), 1);
        display.setTransportFilter(2);
        display.setSearchText("AB 12");
        QCOMPARE(display.rowCount(), 2);
        display.setSearchText("b1"); // Half-byte matches must not match Hex bytes.
        QCOMPARE(display.rowCount(), 0);
        display.setSearchText("192.0.2.1");
        QCOMPARE(display.rowCount(), 1);
        display.setStatusFilter(3);
        QCOMPARE(display.rowCount(), 0);
        display.setSearchText("");
        QCOMPARE(display.rowCount(), 1);
        QCOMPARE(source.rowCount(), 2);
        display.setStatusFilter(-1);
        QCOMPARE(display.rowCount(), 2);
    }

    void testSessionClearAndFailedOperationsPreserveData()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString input = dir.filePath("input.pcap");
        const QByteArray frame = QByteArray::fromHex(
            "00112233445566778899aabb0800450000200001000040110000c0000201c0000202"
            "03e804d2000c000001020304");
        QFile file(input);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray::fromHex("d4c3b2a1020004000000000000000000ffff000001000000"));
        QDataStream stream(&file);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << quint32(1700000000) << quint32(123456) << quint32(frame.size()) << quint32(frame.size());
        file.write(frame);
        file.close();

        CaptureController controller;
        QVERIFY(controller.loadPackets(input));
        QCOMPARE(controller.totalPackets(), 1);
        QCOMPARE(controller.unparsedPackets(), 1);
        QCOMPARE(controller.failedPackets(), 0);
        QVERIFY(!controller.startCapture("nonexistent"));
        QVERIFY(!controller.loadPackets(dir.filePath("missing.pcap")));
        QCOMPARE(controller.totalPackets(), 1);

        QFile config(dir.filePath("protocol.json"));
        QVERIFY(config.open(QIODevice::WriteOnly));
        config.write(R"({"protocolName":"Test","transportType":"UDP","port":1234,
            "length":{"type":"fixed","fixedValue":4},"fields":[]})");
        config.close();
        QVERIFY(controller.loadProtocolConfig(config.fileName()));
        QCOMPARE(controller.parsedPackets(), 1);
        QCOMPARE(controller.matchedPackets(), 1);
        QVERIFY(controller.savePackets(dir.filePath("saved.pcap")));
        PacketCaptureEngine reader;
        QString error;
        QVERIFY(reader.loadFromPcap(dir.filePath("saved.pcap"), error));
        QCOMPARE(reader.getCapturedPackets().size(), 1);
        QCOMPARE(reader.getCapturedPackets().first().timestamp, qint64(1700000000123456));
        controller.clearPackets();
        QCOMPARE(controller.totalPackets(), 0);
        QVERIFY(!controller.savePackets(dir.filePath("empty.pcap")));
        QVERIFY(controller.loadPackets(input));
        QVERIFY(controller.loadPackets(input));
        QVERIFY(controller.savePackets(dir.filePath("reloaded.pcap")));
        QVERIFY(reader.loadFromPcap(dir.filePath("reloaded.pcap"), error));
        QCOMPARE(reader.getCapturedPackets().size(), 1);
    }


    void testGetInterfaces()
    {
        PacketCaptureEngine engine;
        QList<NetworkInterface> interfaces = engine.getAvailableInterfaces();
        // 至少应该能发现回环接口或物理接口，但在某些CI环境可能为空
        qDebug() << "Found" << interfaces.size() << "interfaces";
        for (const auto& iface : interfaces) {
            QVERIFY(!iface.name.isEmpty());
        }
    }

    void testCaptureEngineState()
    {
        PacketCaptureEngine engine;
        QVERIFY(!engine.isCapturing());
        
        QString errorMsg;
        // 尝试开启一个不存在的网卡，应该返回false
        bool started = engine.startCapture("non_existent_interface", errorMsg);
        QVERIFY(!started);
        QVERIFY(!errorMsg.isEmpty());
        QVERIFY(!engine.isCapturing());
        
        engine.stopCapture();
        QVERIFY(!engine.isCapturing());
    }

    void testControllerInitialization()
    {
        CaptureController controller;
        QVERIFY(!controller.isCapturing());
        QCOMPARE(controller.totalPackets(), 0);
        QCOMPARE(controller.udpPackets(), 0);
        QCOMPARE(controller.tcpPackets(), 0);
        QCOMPARE(controller.matchedPackets(), 0);
        QVERIFY(controller.getPacketModel() != nullptr);
    }

    void testControllerInterfaceList()
    {
        CaptureController controller;
        QVariantList interfaces = controller.getInterfaces();
        qDebug() << "Controller found" << interfaces.size() << "interfaces";
        // 验证返回的是QVariantMap列表
        for (const auto& var : interfaces) {
            QVariantMap map = var.toMap();
            QVERIFY(map.contains("name"));
            QVERIFY(map.contains("description"));
            QVERIFY(map.contains("ipAddress"));
        }
    }

    void testProtocolParser()
    {
        ProtocolParser parser;
        ProtocolConfiguration config;
        config.protocolName = "TestProtocol";
        config.transportType = TransportProtocol::UDP;
        config.port = 1234;
        config.isFixedLength = true;
        config.fixedLength = 4;

        FieldDefinition field;
        field.name = "TestField";
        field.offset = 0;
        field.length = 4;
        field.type = FieldType::UInt32;
        field.endianness = Endianness::BigEndian;
        config.fields.append(field);

        parser.setProtocolConfig(config);

        RawPacketOfTool packet;
        packet.protocol = TransportProtocol::UDP;
        packet.destPort = 1234;
        // 模拟4字节数据 0x00000001 (BigEndian)
        packet.data = QByteArray::fromHex("00000001");
        packet.length = packet.data.length();
        packet.headerLength = 0;
        
        ParsedPacket parsed = parser.parsePacket(packet);
        QVERIFY(parsed.isValid);
        QCOMPARE(parsed.fields.size(), 1);
        QCOMPARE(parsed.fields[0].name, QString("TestField"));
        QCOMPARE(parsed.fields[0].value.toUInt(), 1u);
    }

    void testPacketModelUsesDisplayedRawDataLength()
    {
        PacketModel model;

        RawPacketOfTool packet;
        packet.data = QByteArray::fromHex("00ff8041");
        packet.length = 99;
        packet.timestamp = 123456;
        packet.sourceIP = "192.0.2.1";
        packet.destIP = "192.0.2.2";
        packet.sourcePort = 1000;
        packet.destPort = 2000;
        packet.protocol = TransportProtocol::UDP;

        ParsedPacket parsed;
        parsed.isValid = false;
        parsed.errorMessage = "No protocol configuration loaded";
        parsed.rawData = packet.data;

        model.addPacket(packet, parsed);

        const QModelIndex packetIndex = model.index(0, 0);
        QCOMPARE(model.data(packetIndex, PacketModel::LengthRole).toInt(), packet.data.length());
        QCOMPARE(model.data(packetIndex, PacketModel::RawDataRole).toString(), QString("00ff8041"));

        const QVariantMap details = model.getPacketDetails(0);
        QCOMPARE(details["length"].toInt(), packet.data.length());
        QCOMPARE(details["rawData"].toString(), QString("00ff8041"));
    }

    void testPacketModelReparsesExistingPacketsAfterConfigLoad()
    {
        PacketModel model;
        ProtocolParser parser;

        RawPacketOfTool packet;
        packet.data = QByteArray::fromHex("01020304");
        packet.length = packet.data.length();
        packet.headerLength = 0;
        packet.timestamp = 123456;
        packet.sourceIP = "192.0.2.1";
        packet.destIP = "192.0.2.2";
        packet.sourcePort = 53913;
        packet.destPort = 443;
        packet.protocol = TransportProtocol::UDP;

        ParsedPacket parsedWithoutConfig = parser.parsePacket(packet);
        model.addPacket(packet, parsedWithoutConfig);

        QVariantMap details = model.getPacketDetails(0);
        QVERIFY(!details["isValid"].toBool());
        QCOMPARE(details["errorMessage"].toString(), QString("未加载协议配置"));

        ProtocolConfiguration config;
        config.protocolName = "TestProtocol";
        config.transportType = TransportProtocol::UDP;
        config.port = 53913;
        config.isFixedLength = true;
        config.fixedLength = 4;
        parser.setProtocolConfig(config);

        model.reparsePackets(&parser);

        details = model.getPacketDetails(0);
        QVERIFY(details["isValid"].toBool());
        QCOMPARE(details["errorMessage"].toString(), QString(""));
    }

    void cleanupTestCase()
    {
        // Cleanup code
    }
};

QTEST_MAIN(PacketCaptureTests)
#include "test_packetcapture.moc"
