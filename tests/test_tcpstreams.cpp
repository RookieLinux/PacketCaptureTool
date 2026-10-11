#include <QtTest/QtTest>
#include "TcpStreamAnalyzer.h"

class TcpStreamTests : public QObject {
    Q_OBJECT
    static RawPacketOfTool packet(quint32 seq, const QByteArray& payload, bool reverse=false, quint8 flags=0x18) {
        QByteArray frame=QByteArray::fromHex("00112233445566778899aabb08004500002800010000400600000a0000010a00000203e81f9000000000000000005018200000000000");
        const int length=40+payload.size();
        frame[16]=char(length>>8); frame[17]=char(length);
        for (int i=0;i<4;++i) frame[38+i]=char(seq>>(24-i*8));
        frame[47]=char(flags);
        if (reverse) { frame.replace(26,8,QByteArray::fromHex("0a0000020a000001")); frame.replace(34,4,QByteArray::fromHex("1f9003e8")); }
        frame.append(payload);
        RawPacketOfTool raw;
        raw.data=frame; raw.length=frame.size(); raw.headerLength=54; raw.payloadLength=payload.size();
        raw.protocol=TransportProtocol::TCP; raw.sourcePort=reverse ? 8080 : 1000; raw.destPort=reverse ? 1000 : 8080;
        raw.timestamp=1700000000000000; raw.sourceIP=reverse ? "10.0.0.2" : "10.0.0.1"; raw.destIP=reverse ? "10.0.0.1" : "10.0.0.2";
        return raw;
    }
    static ProtocolConfiguration config() {
        ProtocolConfiguration c;
        c.protocolName="Messages"; c.transportType=TransportProtocol::TCP; c.port=8080; c.isFixedLength=true; c.fixedLength=4;
        c.fields.append({"Value",0,4,FieldType::UInt32,Endianness::BigEndian});
        return c;
    }
private slots:
    void splitCoalescedRetransmittedOutOfOrderAndBidirectional() {
        const QList<RawPacketOfTool> frames{
            packet(100,{},false,2), packet(101,QByteArray::fromHex("0102")),packet(103,QByteArray::fromHex("0304")),
            packet(103,QByteArray::fromHex("0304")),packet(107,QByteArray::fromHex("0708")),packet(105,QByteArray::fromHex("0506")),
            packet(109,QByteArray::fromHex("090a0b0c0d0e0f10")),packet(500,{},true,2),packet(501,QByteArray::fromHex("11121314"),true),
            packet(117,{},false,1),packet(505,{},true,1)};
        std::atomic_bool cancel{false};
        const auto result=TcpStreamAnalyzer::analyze(frames,config(),cancel);
        const auto messages=result["messages"].toList();
        QCOMPARE(messages.size(),5);
        QCOMPARE(messages[0].toMap()["hex"].toString(),QString("01 02 03 04"));
        QCOMPARE(messages[1].toMap()["hex"].toString(),QString("05 06 07 08"));
        QCOMPARE(messages[3].toMap()["hex"].toString(),QString("0d 0e 0f 10"));
        QCOMPARE(messages[4].toMap()["direction"].toInt(),1);
        QVERIFY(messages[0].toMap()["valid"].toBool());
        QCOMPARE(result["flows"].toList().first().toMap()["state"].toString(),QString("FIN / RST 结束"));
    }
    void variableHeaderAndTail() {
        auto c=config(); c.isFixedLength=false; c.lengthFieldName="Size";
        c.fields={{"Size",0,2,FieldType::UInt16,Endianness::BigEndian}};
        std::atomic_bool cancel{false};
        const auto result=TcpStreamAnalyzer::analyze({packet(100,{},false,2),packet(101,QByteArray::fromHex("00")),
            packet(102,QByteArray::fromHex("04aabb0005ccddee00"))},c,cancel);
        QCOMPARE(result["messages"].toList().size(),2);
        QCOMPARE(result["messages"].toList()[1].toMap()["length"].toInt(),5);
        QVERIFY(result["flows"].toList().first().toMap()["diagnostic"].toString().contains("剩余 1 字节"));
    }
    void gapAndInvalidLengthNeverJoinMessages() {
        std::atomic_bool cancel{false};
        const auto gap=TcpStreamAnalyzer::analyze({packet(100,{},false,2),packet(101,QByteArray::fromHex("0102")),
            packet(105,QByteArray::fromHex("05060708"))},config(),cancel);
        QVERIFY(gap["messages"].toList().isEmpty());
        QVERIFY(gap["flows"].toList().first().toMap()["diagnostic"].toString().contains("缺失"));
        auto c=config(); c.isFixedLength=false; c.lengthFieldName="Size"; c.fields={{"Size",0,2,FieldType::UInt16,Endianness::LittleEndian}};
        const auto invalid=TcpStreamAnalyzer::analyze({packet(100,QByteArray::fromHex("0100aabb"))},c,cancel);
        QVERIFY(invalid["messages"].toList().isEmpty());
        QVERIFY(invalid["flows"].toList().first().toMap()["diagnostic"].toString().contains("长度无效"));
        const auto valid=TcpStreamAnalyzer::analyze({packet(100,QByteArray::fromHex("0400aabb"))},c,cancel);
        QCOMPARE(valid["messages"].toList().size(),1);
    }
    void limitsAndCancellation() {
        std::atomic_bool cancel{false};
        const auto result=TcpStreamAnalyzer::analyze({packet(100,QByteArray(4004,'a'))},config(),cancel);
        QCOMPARE(result["messages"].toList().size(),1000);
        QVERIFY(result["limited"].toBool());
        cancel=true;
        QVERIFY(TcpStreamAnalyzer::analyze({packet(100,QByteArray(4,'a'))},config(),cancel)["canceled"].toBool());
    }
    void ipv6AndConnectionLimit() {
        auto v6=packet(100,QByteArray::fromHex("01020304"));
        const auto tcp=v6.data.mid(34);
        v6.data=QByteArray::fromHex("00112233445566778899aabb86dd600000000018064020010db800000000000000000000000120010db8000000000000000000000002")+tcp;
        v6.length=v6.data.size(); v6.headerLength=74;
        std::atomic_bool cancel{false};
        const auto result=TcpStreamAnalyzer::analyze({v6},config(),cancel);
        QCOMPARE(result["messages"].toList().size(),1);
        QVERIFY(result["messages"].toList().first().toMap()["source"].toString().startsWith("[2001:db8::1]"));
        QList<RawPacketOfTool> flows;
        for (int i=0;i<129;++i) {
            auto raw=packet(100,QByteArray::fromHex("01020304"));
            raw.data[34]=char((1000+i)>>8); raw.data[35]=char(1000+i);
            flows.append(raw);
        }
        const auto limited=TcpStreamAnalyzer::analyze(flows,config(),cancel);
        QCOMPARE(limited["flows"].toList().size(),128);
        QVERIFY(limited["limited"].toBool());
    }
};
QTEST_APPLESS_MAIN(TcpStreamTests)
#include "test_tcpstreams.moc"
