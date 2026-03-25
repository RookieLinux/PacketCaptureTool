import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import FluentUI

FluWindow {
    id: mainWindow
    visible: true
    width: 1200
    height: 800
    title: qsTr("Packet Capture Tool")

    Component.onCompleted: {
        FluApp.init(mainWindow)
    }

    // Config dialogs
    FileDialog {
        id: configFileDialog
        title: qsTr("Select Protocol Configuration File")
        nameFilters: ["JSON files (*.json)", "All files (*)"]
        onAccepted: {
            var filePath = selectedFile.toString().replace("file://", "")
            captureController.loadProtocolConfig(filePath)
        }
    }
    // Save PCAP file dialogs
    FileDialog {
        id: saveFileDialog
        title: qsTr("Save Packets to PCAP File")
        nameFilters: ["PCAP files (*.pcap)", "All files (*)"]
        fileMode: FileDialog.SaveFile
        onAccepted: {
            var filePath = selectedFile.toString().replace("file://", "")
            captureController.savePackets(filePath)
        }
    }
    // Load PCAP file dialogs
    FileDialog {
        id: loadFileDialog
        title: qsTr("Load Packets from PCAP File")
        nameFilters: ["PCAP files (*.pcap)", "All files (*)"]
        onAccepted: {
            var filePath = selectedFile.toString().replace("file://", "")
            captureController.loadPackets(filePath)
        }
    }

    appBar: FluAppBar {
        title: qsTr("Packet Capture Tool")
        showDark: true
        showMinimize: true
        showMaximize: true
        showClose: true
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 0
        spacing: 0
        //control panel
        FluFrame {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            padding: 5
            radius: 0
            border.width: 0
            border.color: "transparent"
            color: FluTheme.dark ? Qt.rgba(45/255, 45/255, 45/255, 1) : Qt.rgba(249/255, 249/255, 249/255, 1)
            RowLayout {
                spacing: 10

                FluText {
                    text: qsTr("Interface")
                    font: FluTextStyle.Body
                }

                FluComboBox {
                    id: interfaceSelector
                    Layout.preferredWidth: 300
                    model: captureController.getInterfaces()
                    textRole: "displayName"
                }

                FluButton {
                    text: qsTr("Load Config")
                    onClicked: {
                        configFileDialog.open()
                    }
                }

                FluFilledButton {
                    id: startButton
                    text: qsTr("Start")
                    disabled: captureController.isCapturing || interfaceSelector.currentText === ""
                    onClicked: {
                        var currentItem = interfaceSelector.model[interfaceSelector.currentIndex]
                        if (currentItem) {
                            captureController.startCapture(currentItem.name)
                        }
                    }
                }

                FluButton {
                    id: stopButton
                    text: qsTr("Stop")
                    disabled: !captureController.isCapturing
                    onClicked: {
                        captureController.stopCapture()
                    }
                }

                FluText {
                    text: qsTr("Filter")
                    font: FluTextStyle.Body
                }

                FluComboBox {
                    id: transportFilter
                    model: ["UDP", "TCP", "Both"]
                    currentIndex: 2
                    onCurrentIndexChanged: {
                        captureController.setTransportFilter(currentIndex)
                    }
                }

                FluIconButton {
                    iconSource: FluentIcons.Save
                    text: qsTr("Save to PCAP")
                    onClicked: saveFileDialog.open()
                }

                FluIconButton {
                    iconSource: FluentIcons.OpenFile
                    text: qsTr("Load from PCAP")
                    onClicked: loadFileDialog.open()
                }

                FluIconButton {
                    iconSource: FluentIcons.Delete
                    text: qsTr("Clear Packets")
                    onClicked: captureController.clearPackets()
                }

                Item {
                    Layout.fillWidth: true
                }
            }
        }

        // Main content area - FluSplitLayout
        FluSplitLayout {
            id: mainArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            // Left side: Packet list
            FluFrame {
                SplitView.preferredWidth: 300
                SplitView.minimumWidth: 250
                Layout.fillHeight: true
                padding: 0
                radius: 0
                
                ListView {
                    id: packetListView
                    anchors.fill: parent
                    model: packetModel
                    clip: true
                    spacing: 0
                    currentIndex: -1
                    focus: true
                    keyNavigationEnabled: true
                    onCurrentIndexChanged: {
                        if (currentIndex >= 0 && currentIndex < count) {
                            packetDetailView.packetDetails = packetModel.getPacketDetails(currentIndex)
                        }
                    }

                    delegate: Item {
                        id: packetDelegate
                        width: packetListView.width
                        height: 90
                        
                        Rectangle {
                            id: selectionHighlight
                            anchors.fill: parent
                            anchors.margins: 2
                            radius: 4
                            color: {
                                if (packetListView.currentIndex === index) {
                                    return FluTheme.dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.08)
                                }
                                if (mouseArea.containsMouse) {
                                    return FluTheme.dark ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(0, 0, 0, 0.03)
                                }
                                return "transparent"
                            }
                            
                            // Selection indicator on the left
                            Rectangle {
                                width: 4
                                height: parent.height * 0.4
                                radius: 2
                                color: FluTheme.primaryColor
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                visible: packetListView.currentIndex === index
                            }
                        }

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                packetListView.currentIndex = index
                                packetListView.forceActiveFocus()
                            }
                        }

                        ColumnLayout {
                            anchors.fill: selectionHighlight
                            anchors.margins: 10
                            spacing: 6
                            // Top row: Packet number, timestamp, length
                            RowLayout {
                                Layout.fillWidth: true
                                
                                FluText {
                                    text: "#" + (index + 1)
                                    font: FluTextStyle.BodyStrong
                                }

                                FluText {
                                    text: new Date(model.timestamp).toLocaleTimeString(Qt.locale(), "hh:mm:ss.zzz")
                                    font: FluTextStyle.Caption
                                    color: FluTheme.dark ? "#AAAAAA" : "#666666"
                                }

                                Item { Layout.fillWidth: true }

                                FluText {
                                    text: model.length + " bytes"
                                    font: FluTextStyle.Caption
                                }
                            }
                            // Middle row: Source and destination
                            RowLayout {
                                Layout.fillWidth: true
                                
                                FluText {
                                    text: model.sourceIP + ":" + (model.sourcePort || "")
                                    font: FluTextStyle.Body
                                    color: FluTheme.primaryColor
                                    elide: Text.ElideRight
                                }

                                FluIcon {
                                    iconSource: FluentIcons.Forward
                                    iconSize: 12
                                }

                                FluText {
                                    text: model.destIP + ":" + (model.destPort || "")
                                    font: FluTextStyle.Body
                                    color: FluTheme.dark ? "#FF7F50" : "#CC3300"
                                    elide: Text.ElideRight
                                }
                            }
                            // Bottom row: Protocol and validity
                            RowLayout {
                                Layout.fillWidth: true
                                
                                FluBadge {
                                    count: model.protocol === 0 ? "UDP" : "TCP"
                                    color: model.protocol === 0 ? "#228B22" : "#DAA520"
                                }
                                
                                FluText {
                                    text: model.isValid ? "✓ Parsed" : "✗ Invalid"
                                    font: FluTextStyle.Caption
                                    color: model.isValid ? "#228B22" : "#CC0000"
                                    visible: model.parsedFields && model.parsedFields.length > 0
                                }
                            }
                        }
                    }

                    ScrollBar.vertical: FluScrollBar {
                        id: scroll_v
                    }
                }
            }

            // Right side: Packet details
            FluFrame {
                id: packetDetailFrame
                width:mainArea.width - packetListView.width
                Layout.fillHeight: true
                padding: 10
                radius: 0
                clip: true

                ColumnLayout {
                    id: packetDetailView
                    anchors.fill: parent
                    spacing: 10
                    property var packetDetails: ({})

                    FluText {
                        text: packetDetailView.packetDetails && packetDetailView.packetDetails.index !== undefined ?
                            qsTr("Packet") + " #" +(packetDetailView.packetDetails.index + 1) + qsTr(" Details") :
                            qsTr("Select a packet to view details")
                        font: FluTextStyle.Subtitle
                        Layout.alignment: Qt.AlignHCenter
                    }

                    FluPivot {
                        id: detailPivot
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        headerHeight: 40
                        font: FluTextStyle.BodyStrong

                        FluPivotItem {
                            title: qsTr("Parsed Fields")
                            contentItem: Component {
                                Item {
                                    width: detailPivot.width
                                    height: detailPivot.height - detailPivot.headerHeight
                                    
                                    FluScrollablePage {
                                        anchors.fill: parent
                                        
                                        ColumnLayout {
                                            width: parent.width
                                            spacing: 10

                                            // Error message
                                            FluFrame {
                                                Layout.fillWidth: true
                                                padding: 10
                                                color: FluTheme.dark ? Qt.rgba(68/255, 39/255, 38/255, 1) : Qt.rgba(253/255, 231/255, 233/255, 1)
                                                border.color: FluTheme.dark ? Qt.rgba(67/255, 39/255, 38/255, 1) : Qt.rgba(238/255, 217/255, 219/255, 1)
                                                visible: packetDetailView.packetDetails && packetDetailView.packetDetails.isValid !== undefined && packetDetailView.packetDetails.isValid === false
                                                
                                                RowLayout {
                                                    anchors.fill: parent
                                                    spacing: 10
                                                    
                                                    FluIcon {
                                                        iconSource: FluentIcons.StatusErrorFull
                                                        iconColor: FluTheme.dark ? Qt.rgba(255/255,153/255,164/255,1) : Qt.rgba(196/255,43/255,28/255,1)
                                                        iconSize: 16
                                                    }
                                                    
                                                    FluText {
                                                        text: (packetDetailView.packetDetails && packetDetailView.packetDetails.errorMessage) ? packetDetailView.packetDetails.errorMessage : ""
                                                        font: FluTextStyle.Body
                                                        Layout.fillWidth: true
                                                        wrapMode: Text.WordWrap
                                                    }
                                                }
                                            }

                                            // Fields table-like display
                                            Repeater {
                                                model: packetDetailView.packetDetails && packetDetailView.packetDetails.parsedFields ? 
                                                       packetDetailView.packetDetails.parsedFields : []
                                                
                                                FluFrame {
                                                    Layout.fillWidth: true
                                                    padding: 10
                                                    
                                                    RowLayout {
                                                        anchors.fill: parent
                                                        FluText {
                                                            text: modelData.name || ""
                                                            font: FluTextStyle.BodyStrong
                                                            Layout.preferredWidth: parent.width * 0.3
                                                        }
                                                        
                                                        FluText {
                                                            text: modelData.type || ""
                                                            font: FluTextStyle.Caption
                                                            color: "#888888"
                                                            Layout.preferredWidth: parent.width * 0.2
                                                        }
                                                        
                                                        FluCopyableText {
                                                            text: modelData.displayValue || modelData.value || ""
                                                            font: FluTextStyle.Body
                                                            color: FluTheme.primaryColor
                                                            Layout.fillWidth: true
                                                        }
                                                    }
                                                }
                                            }

                                            FluText {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 100
                                                visible: !packetDetailView.packetDetails || 
                                                         !packetDetailView.packetDetails.parsedFields || 
                                                         packetDetailView.packetDetails.parsedFields.length === 0
                                                text: packetDetailView.packetDetails && packetDetailView.packetDetails.index !== undefined ?
                                                    qsTr("No parsed fields available for this packet") :
                                                    qsTr("Select a packet to view parsed fields")
                                                font: FluTextStyle.Body
                                                color: "#999999"
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        FluPivotItem {
                            title: qsTr("Frame Data")
                            contentItem: Component {
                                Item {
                                    width: detailPivot.width
                                    height: detailPivot.height - detailPivot.headerHeight
                                    
                                    FluMultilineTextBox {
                                        anchors.fill: parent
                                        readOnly: true
                                        font.family: "Monospace"
                                        font.pixelSize: 12
                                        text: {
                                            if (!packetDetailView.packetDetails || packetDetailView.packetDetails.index === undefined) {
                                                return qsTr("Select a packet to view raw data")
                                            }
                                            
                                            var rawData = packetDetailView.packetDetails.rawData
                                            if (!rawData) {
                                                return qsTr("No raw data available")
                                            }
                                            
                                            // Convert to hex display (simplified for preview)
                                            var hexString = ""
                                            for (var i = 0; i < rawData.length; i++) {
                                                var b = rawData.charCodeAt(i) & 0xFF
                                                var s = b.toString(16).toUpperCase()
                                                if (s.length < 2) s = "0" + s
                                                hexString += s + " "
                                                if ((i + 1) % 16 === 0) hexString += "\n"
                                                else if ((i + 1) % 8 === 0) hexString += "  "
                                            }
                                            return hexString
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Status bar
        FluFrame {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            padding: 5
            radius: 0
            border.width: 0
            color: FluTheme.dark ? Qt.rgba(30/255, 30/255, 30/255, 1) : Qt.rgba(238/255, 238/255, 238/255, 1)

            RowLayout {
                spacing: 20

                FluText {
                    text: captureController.isCapturing ? qsTr("Status: Capturing") : qsTr("Status: Stopped")
                    font: FluTextStyle.BodyStrong
                    color: captureController.isCapturing ? "#009900" : "#666666"
                }

                FluText {
                    text: "Total: " + captureController.totalPackets
                    font: FluTextStyle.Caption
                }

                FluText {
                    text: "UDP: " + captureController.udpPackets
                    font: FluTextStyle.Caption
                    color: "#009900"
                }

                FluText {
                    text: "TCP: " + captureController.tcpPackets
                    font: FluTextStyle.Caption
                    color: "#DAA520"
                }

                FluText {
                    text: "Invalid: " + (captureController.totalPackets - captureController.validPackets)
                    font: FluTextStyle.Caption
                    color: "#CC0000"
                }

                Item { Layout.fillWidth: true }

                FluText {
                    text: "Config: " + (captureController.configLoaded ? captureController.configFileName : "None")
                    font: FluTextStyle.Caption
                    elide: Text.ElideLeft
                    Layout.maximumWidth: 300
                }
            }
        }
    }
}
