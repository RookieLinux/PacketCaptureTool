import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FluentUI 1.0

StyledDialog {
    id: dialog
    objectName: "tcpAnalysisDialog"
    title: qsTr("TCP 流重组与消息分帧")
    width: Math.min(900, parent ? parent.width - 32 : 900)
    height: Math.min(620, parent ? parent.height - 24 : 620)
    property var flows: captureController.tcpAnalysis.flows || []
    property var selectedFlow: flows[flowSelector.currentIndex] || ({})
    property var messages: (captureController.tcpAnalysis.messages || []).filter(function(message) { return message.flow === dialog.selectedFlow.id })
    onFlowsChanged: flowSelector.currentIndex = flows.length ? 0 : -1
    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        RowLayout {
            FluText { text: qsTr("连接") }
            FluComboBox {
                id: flowSelector
                Layout.fillWidth: true
                model: dialog.flows
                textRole: "displayName"
                Accessible.name: qsTr("TCP 连接")
            }
            FluText { text: dialog.messages.length + qsTr(" 条消息") }
        }
        FluText {
            Layout.fillWidth: true
            font: FluTextStyle.Caption
            color: FluTheme.fontSecondaryColor
            wrapMode: Text.Wrap
            text: captureController.tcpAnalysis.notice || qsTr("加载 TCP 协议配置后，可分析当前内存窗口的双向 TCP 消息。分析不会修改原始报文。")
        }
        FluText {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: dialog.selectedFlow.diagnostic ? (FluTheme.dark ? "#f7c36b" : "#9d5d00") : FluTheme.fontSecondaryColor
            text: (dialog.selectedFlow.state || "") + (dialog.selectedFlow.diagnostic ? " · " + dialog.selectedFlow.diagnostic : "")
        }
        FluProgressBar { visible:captureController.tcpAnalysisBusy; Layout.fillWidth:true }
        ListView {
            id: messageList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 10
            model: dialog.messages
            ScrollBar.vertical: FluScrollBar {}
            delegate: FluFrame {
                required property var modelData
                required property int index
                width: messageList.width - 12
                padding: 12
                radius: 6
                implicitHeight: messageBody.implicitHeight + 24
                ColumnLayout {
                    id: messageBody
                    anchors.fill: parent
                    spacing: 8
                    FluText {
                        Layout.fillWidth: true
                        font: FluTextStyle.BodyStrong
                        wrapMode: Text.Wrap
                        text: qsTr("消息 ") + (index+1) + " · " + modelData.source + " → " + modelData.destination + " · " + modelData.length + qsTr(" 字节 · 流偏移 ") + modelData.offset
                    }
                    FluText { Layout.fillWidth:true; visible:!modelData.valid; wrapMode:Text.Wrap; color:FluTheme.dark ? "#ff9999" : "#b91c1c"; text:modelData.error }
                    Repeater {
                        model: modelData.fields
                        FluText {
                            required property var modelData
                            Layout.fillWidth: true
                            text: modelData.name + "：" + modelData.value + qsTr("（消息偏移 ") + modelData.offset + "）"
                            maximumLineCount: 2
                            wrapMode: Text.WrapAnywhere
                            elide: Text.ElideRight
                        }
                    }
                    FluText { visible:modelData.fieldsLimited; text:qsTr("仅展示前 64 个字段"); color:FluTheme.fontSecondaryColor }
                    FluCopyableText { Layout.fillWidth:true; wrapMode:TextEdit.WrapAnywhere; text:modelData.hex.substring(0,1536); font.family:"Consolas" }
                    FluText { visible:modelData.length > 512; text:qsTr("Hex 仅预览前 512 字节"); color:FluTheme.fontSecondaryColor }
                }
            }
            FluText {
                anchors.centerIn: parent
                width: parent.width - 24
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible:messageList.count === 0
                text:captureController.tcpAnalysisBusy ? qsTr("正在重组 TCP 流…") : qsTr("当前连接没有完整消息；请查看缺包、尾部残留诊断或重新分析。")
            }
        }
    }
    footer: DialogActionBar {
        FluButton { visible:captureController.tcpAnalysisBusy; Layout.preferredWidth:104; Layout.preferredHeight:32; text:qsTr("取消分析"); onClicked:captureController.cancelTcpAnalysis() }
        FluButton { Layout.preferredWidth:104; Layout.preferredHeight:32; text:qsTr("关闭"); onClicked:dialog.close() }
        FluFilledButton { Layout.preferredWidth:104; Layout.preferredHeight:32; text:qsTr("重新分析"); disabled:captureController.tcpAnalysisBusy || captureController.isLoading; onClicked:captureController.analyzeTcpStreams() }
    }
}
