import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FluentUI 1.0

Item {
    id: browser
    property alias currentIndex: list.currentIndex
    property alias count: list.count
    property var model
    property var columns: []
    property bool tableMode: true
    property bool compact: true
    property real selectedSequence: 0
    property bool changing: false
    property string sortKey: ""
    property bool descending: false
    function selectRow(row) {
        if (row < 0 || row >= list.count) return
        selectedSequence = model.cellValue(row,"sequence") || 0
        changing = false
        list.currentIndex = row
        list.forceActiveFocus()
    }
    function positionViewAtEnd() { list.positionViewAtEnd() }
    function focusList() { list.forceActiveFocus() }
    function restoreSelection() {
        changing = false
        if (selectedSequence) list.currentIndex = model.rowForSequence(selectedSequence)
        if (list.currentIndex < 0) selectedSequence = 0
        else if (!selectedSequence) selectedSequence = model.cellValue(list.currentIndex,"sequence") || 0
    }
    function value(data, key) {
        if (key === "timestamp") return new Date(data.timestamp / 1000).toLocaleTimeString(Qt.locale(), "hh:mm:ss.zzz")
        if (key === "status") return [qsTr("未解析"), qsTr("未匹配"), qsTr("成功"), qsTr("失败")][data.status]
        if (key.indexOf("field:") === 0) {
            for (var i=0; i<data.fields.length; i++) if (data.fields[i].name === key.substring(6)) return String(data.fields[i].displayValue)
            return "—"
        }
        return String(data[key] === undefined ? "" : data[key])
    }
    Connections {
        target: browser.model
        function onRowsAboutToBeInserted() { browser.changing = true }
        function onRowsAboutToBeRemoved() { browser.changing = true }
        function onLayoutAboutToBeChanged() { browser.changing = true }
        function onRowsInserted() { Qt.callLater(browser.restoreSelection) }
        function onRowsRemoved() { Qt.callLater(browser.restoreSelection) }
        function onLayoutChanged() { Qt.callLater(browser.restoreSelection) }
        function onModelReset() { browser.changing = false; browser.selectedSequence = 0; list.currentIndex = -1 }
    }
    Flickable {
        id: horizontal
        anchors.fill: parent
        contentWidth: browser.tableMode ? Math.max(width, browser.columns.reduce(function(sum,c) { return sum + c.width },0)) : width
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.horizontal: FluScrollBar { policy: horizontal.contentWidth > horizontal.width ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff }
        ColumnLayout {
            width: horizontal.contentWidth
            height: horizontal.height - (browser.tableMode && horizontal.contentWidth > horizontal.width ? 14 : 0)
            spacing: 0
            Item {
                objectName: "packetTableHeader"
                visible: browser.tableMode
                Layout.fillWidth: true
                Layout.preferredHeight: 38
                Row {
                    anchors.fill: parent
                    Repeater {
                        model: browser.columns
                        FluButton {
                            required property var modelData
                            objectName: "sort_" + modelData.key
                            width: modelData.width; height: 38
                            horizontalPadding: 8
                            background: Rectangle {
                                radius: 4
                                color: parent.down ? FluTools.withOpacity(FluTheme.fontPrimaryColor, 0.08) : parent.hovered ? FluTools.withOpacity(FluTheme.fontPrimaryColor, 0.04) : "transparent"
                                FluFocusRectangle { anchors.fill:parent; visible:parent.parent.activeFocus; radius:4 }
                            }
                            contentItem: FluText {
                                text: parent.text
                                font: FluTextStyle.BodyStrong
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            text: modelData.title + (browser.sortKey === modelData.key ? (browser.descending ? " ↓" : " ↑") : "")
                            Accessible.name: modelData.title + qsTr("排序")
                            onClicked: {
                                browser.descending = browser.sortKey === modelData.key ? !browser.descending : false
                                browser.sortKey = modelData.key
                                browser.model.sortColumn(modelData.key, browser.descending)
                            }
                        }
                    }
                }
                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: FluTheme.dark ? "#404040" : "#e5e5e5"
                }
            }
            ListView {
                id: list
                objectName: "packetList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: browser.model
                currentIndex: -1
                keyNavigationEnabled: true
                focus: true
                clip: true
                Accessible.role: Accessible.List
                Accessible.name: qsTr("报文列表，方向键选择报文")
                onCurrentIndexChanged: {
                    if (!browser.changing && currentIndex >= 0 && currentIndex < count) browser.selectedSequence = browser.model.getPacketDetails(currentIndex).sequence || 0
                }
                delegate: Item {
                    id: row
                    objectName: "packetRow_" + values.sequence
                    width: list.width
                    height: browser.tableMode ? (browser.compact ? 34 : 46) : (browser.compact ? 74 : 94)
                    property var values: ({sequence:model.sequence, timestamp:model.timestamp, sourceIP:model.sourceIP, sourcePort:model.sourcePort,
                        destIP:model.destIP,destPort:model.destPort,protocol:model.protocolName,length:model.length,status:model.parseStatus,error:model.errorMessage,fields:model.parsedFields})
                    Accessible.role: Accessible.ListItem
                    Accessible.name: qsTr("报文 ") + values.sequence + "，" + values.sourceIP + qsTr(" 到 ") + values.destIP + "，" + browser.value(values,"status")
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 2
                        radius: 4
                        color: list.currentIndex === index ? FluTools.withOpacity(FluTheme.primaryColor,FluTheme.dark ? 0.18 : 0.10) : rowHover.hovered ? (FluTheme.dark ? "#323232" : "#f3f3f3") : "transparent"
                        border.color: list.activeFocus && list.currentIndex === index ? FluTheme.primaryColor : "transparent"
                        Rectangle {
                            visible:list.currentIndex === index
                            width:3; height:Math.min(20,parent.height-8); radius:2
                            anchors.left:parent.left; anchors.verticalCenter:parent.verticalCenter
                            color:FluTheme.primaryColor
                        }
                    }
                    HoverHandler { id:rowHover }
                    MouseArea { anchors.fill: parent; onClicked: browser.selectRow(index) }
                    Row {
                        visible: browser.tableMode
                        Repeater {
                            model: browser.columns
                            FluText {
                                required property var modelData
                                width: modelData.width; height: row.height
                                color: FluTheme.fontPrimaryColor
                                text: browser.value(row.values,modelData.key)
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 8; rightPadding: 8; elide: Text.ElideRight
                            }
                        }
                    }
                    Column {
                        visible: !browser.tableMode
                        anchors.fill: parent; anchors.margins: 8; spacing: 4
                        FluText { color:FluTheme.fontPrimaryColor; text: "#" + row.values.sequence + "  " + browser.value(row.values,"timestamp") + "  " + row.values.length + qsTr(" 字节") }
                        FluText { color:FluTheme.fontPrimaryColor; width: parent.width; elide: Text.ElideRight; text: row.values.sourceIP + ":" + row.values.sourcePort + " → " + row.values.destIP + ":" + row.values.destPort }
                        FluText { color:FluTheme.fontPrimaryColor; text: row.values.protocol + " · " + browser.value(row.values,"status") }
                    }
                }
                ScrollBar.vertical: FluScrollBar {}
                FluText {
                    anchors.verticalCenter: parent.verticalCenter
                    x: horizontal.contentX + 15
                    width: horizontal.width - 30
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    visible: list.count === 0
                    text: captureController.totalPackets === 0 ? qsTr("开始采集或导入 PCAP / PCAPNG 文件") : qsTr("没有符合筛选条件的报文")
                }
            }
        }
    }
}
