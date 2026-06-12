import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import FluentUI

FluWindow {
    id: mainWindow
    visible: true
    width: 1300
    height: 850
    title: qsTr("Packet Capture Tool")

    Component.onCompleted: {
        FluApp.init(mainWindow)
        FluTheme.nativeText = false
    }

    function localFilePath(fileUrl) {
        if (fileUrl === undefined || fileUrl === null) {
            return ""
        }

        if (fileUrl.toLocalFile) {
            var localPath = fileUrl.toLocalFile()
            if (localPath && localPath.length > 0) {
                return localPath
            }
        }

        var urlString = (typeof fileUrl === "string") ? fileUrl : fileUrl.toString()
        var path = decodeURIComponent(urlString)

        if (path.indexOf("file://") === 0) {
            path = path.substring(7)
        }

        // Windows 路径可能被表示为 /C:/...，去掉多余前导 /
        if (/^\/[A-Za-z]:[\\/]/.test(path)) {
            path = path.substring(1)
        }

        return path
    }

    function refreshSelectedPacketDetails() {
        if (packetListView.currentIndex >= 0 && packetListView.currentIndex < packetListView.count) {
            packetDetailView.packetDetails = packetModel.getPacketDetails(packetListView.currentIndex)
        }
    }

    // File dialogs
    FileDialog {
        id: configFileDialog
        title: qsTr("Select Protocol Configuration File")
        nameFilters: ["JSON files (*.json)", "All files (*)"]
        onAccepted: {
            var filePath = mainWindow.localFilePath(selectedFile)
            captureController.loadProtocolConfig(filePath)
        }
    }

    FileDialog {
        id: saveFileDialog
        title: qsTr("Save Packets to PCAP File")
        nameFilters: ["PCAP files (*.pcap)", "All files (*)"]
        fileMode: FileDialog.SaveFile
        onAccepted: {
            var filePath = mainWindow.localFilePath(selectedFile)
            captureController.savePackets(filePath)
        }
    }

    FileDialog {
        id: loadFileDialog
        title: qsTr("Load Packets from PCAP File")
        nameFilters: ["PCAP files (*.pcap)", "All files (*)"]
        onAccepted: {
            var filePath = mainWindow.localFilePath(selectedFile)
            captureController.loadPackets(filePath)
        }
    }

    Connections {
        target: captureController
        function onProtocolConfigLoaded(protocolName) {
            mainWindow.showSuccess(qsTr("Loaded config: ") + protocolName, 2000)
            mainWindow.refreshSelectedPacketDetails()
        }

        function onErrorOccurred(errorMsg) {
            mainWindow.showError(errorMsg, 4000)
        }
    }

    Connections {
        target: packetModel
        function onDataChanged(topLeft, bottomRight, roles) {
            mainWindow.refreshSelectedPacketDetails()
        }

        function onRowsInserted(parent, first, last) {
            mainWindow.refreshSelectedPacketDetails()
        }

        function onModelReset() {
            packetDetailView.packetDetails = ({})
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

        FluFrame {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            Layout.margins: 10
            Layout.bottomMargin: 5
            padding: 5
            radius: 8
            border.width: 0
            border.color: "transparent"
            color: FluTheme.dark ? Qt.rgba(45/255, 45/255, 45/255, 1) : Qt.rgba(249/255, 249/255, 249/255, 1)

            RowLayout {
                anchors.fill: parent
                spacing: 10

                FluText {
                    text: qsTr("Interface")
                    font: FluTextStyle.Body
                }

                FluComboBox {
                    id: interfaceSelector
                    Layout.preferredWidth: 400
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
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 10
            Layout.topMargin: 0
            orientation: Qt.Horizontal

            // Left side: Packet list
            FluFrame {
                SplitView.preferredWidth: 400
                SplitView.minimumWidth: 300
                Layout.fillHeight: true
                padding: 0
                radius: 8
                
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
                        mainWindow.refreshSelectedPacketDetails()
                    }

                    delegate: Item {
                        id: packetDelegate
                        width: packetListView.width
                        height: 90
                        
                        Rectangle {
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
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 6

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
                width:anchors.fillWidth - packetListView.width
                Layout.fillHeight: true
                padding: 10
                radius: 8
                clip: true

                ColumnLayout {
                    id: packetDetailView
                    anchors.fill: parent
                    spacing: 10
                    property var packetDetails: ({})
                    property string frameNormalColor: FluTheme.dark ? "#EDEBE9" : "#201F1E"
                    property string frameEthernetColor: FluTheme.dark ? "#FFD166" : "#8A5A00"
                    property string frameIpColor: FluTheme.dark ? "#7DD3FC" : "#0067B8"
                    property string frameTransportColor: FluTheme.dark ? "#F0ABFC" : "#B146C2"

                    function hexByte(rawDataHex, byteIndex) {
                        if (byteIndex * 2 + 2 > rawDataHex.length) return "00"
                        return rawDataHex.substring(byteIndex * 2, byteIndex * 2 + 2).toUpperCase()
                    }

                    function parseFrameLayers(rawDataHex) {
                        var byteCount = Math.floor(rawDataHex.length / 2)
                        var layers = []
                        if (byteCount < 14) return layers

                        var etherTypeOffset = 12
                        var ipStart = 14
                        var etherType = hexByte(rawDataHex, etherTypeOffset) + hexByte(rawDataHex, etherTypeOffset + 1)

                        while ((etherType === "8100" || etherType === "88A8" || etherType === "9100") && byteCount >= ipStart + 4) {
                            etherTypeOffset += 4
                            ipStart += 4
                            etherType = hexByte(rawDataHex, etherTypeOffset) + hexByte(rawDataHex, etherTypeOffset + 1)
                        }

                        layers.push({ start: 0, end: ipStart, color: frameEthernetColor })

                        if (etherType !== "0800" || byteCount <= ipStart) return layers

                        var versionAndIhl = parseInt(hexByte(rawDataHex, ipStart), 16)
                        var ipVersion = versionAndIhl >> 4
                        var ipHeaderLength = (versionAndIhl & 0x0F) * 4
                        if (ipVersion !== 4 || ipHeaderLength < 20 || byteCount < ipStart + ipHeaderLength) return layers

                        var transportStart = ipStart + ipHeaderLength
                        layers.push({ start: ipStart, end: transportStart, color: frameIpColor })

                        if (byteCount <= transportStart) return layers

                        var protocol = parseInt(hexByte(rawDataHex, ipStart + 9), 16)
                        var transportLength = 0
                        if (protocol === 6 && byteCount > transportStart + 12) {
                            transportLength = (parseInt(hexByte(rawDataHex, transportStart + 12), 16) >> 4) * 4
                        } else if (protocol === 17) {
                            transportLength = 8
                        }

                        if (transportLength > 0) {
                            layers.push({
                                start: transportStart,
                                end: Math.min(byteCount, transportStart + transportLength),
                                color: frameTransportColor
                            })
                        }
                        return layers
                    }

                    function frameByteColor(byteIndex, layers) {
                        for (var i = 0; i < layers.length; i++) {
                            if (byteIndex >= layers[i].start && byteIndex < layers[i].end) return layers[i].color
                        }
                        return ""
                    }

                    function formatFrameData(rawDataHex) {
                        if (!rawDataHex) return ""
                        var layers = parseFrameLayers(rawDataHex)
                        var html = "<pre style=\"font-family: 'Consolas', 'Cascadia Mono', 'Courier New', monospace; font-size: 12px; line-height: 1.45; color: " +
                                   frameNormalColor + "; margin: 0;\">"

                        var currentColor = ""
                        var openedSpan = false

                        for (var i = 0; i < rawDataHex.length; i += 2) {
                            var byteIndex = i / 2
                            var color = frameByteColor(byteIndex, layers)
                            var s = rawDataHex.substring(i, i + 2).toUpperCase()

                            if (color !== currentColor) {
                                if (openedSpan) {
                                    html += "</span>"
                                    openedSpan = false
                                }
                                if (color !== "") {
                                    html += "<span style=\"color: " + color + ";\">"
                                    openedSpan = true
                                }
                                currentColor = color
                            }

                            html += s

                            if ((byteIndex + 1) % 16 === 0) {
                                if (openedSpan) {
                                    html += "</span>"
                                    openedSpan = false
                                    currentColor = ""
                                }
                                html += "\n"
                            } else if (i + 2 < rawDataHex.length) {
                                html += ((byteIndex + 1) % 8 === 0) ? "  " : " "
                            }
                        }

                        if (openedSpan) html += "</span>"
                        html += "</pre>"
                        return html
                    }

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
                                            Layout.fillWidth: true
                                            spacing: 10

                                            // Error message
                                            FluFrame {
                                                Layout.fillWidth: true
                                                padding: 10
                                                color: FluTheme.dark ? Qt.rgba(68/255, 39/255, 38/255, 1) : Qt.rgba(253/255, 231/255, 233/255, 1)
                                                border.color: FluTheme.dark ? Qt.rgba(67/255, 39/255, 38/255, 1) : Qt.rgba(238/255, 217/255, 219/255, 1)
                                                visible: packetDetailView.packetDetails && packetDetailView.packetDetails.isValid !== undefined && packetDetailView.packetDetails.isValid === false
                                                implicitWidth: 0

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
                                                    radius: 4
                                                    implicitWidth: 0

                                                    RowLayout {
                                                        anchors.fill: parent
                                                        
                                                        FluText {
                                                            text: modelData.name || ""
                                                            font: FluTextStyle.BodyStrong
                                                            Layout.fillWidth: true
                                                            Layout.preferredWidth: 150
                                                        }
                                                        
                                                        FluText {
                                                            text: modelData.type || ""
                                                            font: FluTextStyle.Caption
                                                            color: "#888888"
                                                            Layout.fillWidth: true
                                                            Layout.preferredWidth: 100
                                                        }
                                                        
                                                        FluCopyableText {
                                                            text: modelData.displayValue || modelData.value || ""
                                                            font: FluTextStyle.Body
                                                            color: FluTheme.primaryColor
                                                            Layout.fillWidth: true
                                                            Layout.preferredWidth: 250
                                                        }
                                                    }
                                                }
                                            }

                                            FluText {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 100
                                                visible: !packetDetailView.packetDetails || 
                                                         (packetDetailView.packetDetails.index === undefined)
                                                text: qsTr("Select a packet to view parsed fields")
                                                font: FluTextStyle.Body
                                                color: "#999999"
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }

                                            FluText {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 100
                                                visible: packetDetailView.packetDetails &&
                                                         packetDetailView.packetDetails.index !== undefined &&
                                                         packetDetailView.packetDetails.isValid === true &&
                                                         (!packetDetailView.packetDetails.parsedFields ||
                                                          packetDetailView.packetDetails.parsedFields.length === 0)
                                                text: qsTr("Protocol matched, but no fields defined in configuration")
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

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 4
                                        color: FluTheme.dark ? Qt.rgba(30/255, 30/255, 30/255, 1) : Qt.rgba(1, 1, 1, 1)
                                        border.color: FluTheme.dark ? Qt.rgba(64/255, 64/255, 64/255, 1) : Qt.rgba(216/255, 216/255, 216/255, 1)
                                        clip: true

                                        Flickable {
                                            id: frameDataFlickable
                                            anchors.fill: parent
                                            anchors.margins: 8
                                            contentWidth: Math.max(width, frameDataText.contentWidth)
                                            contentHeight: Math.max(height, frameDataText.contentHeight)
                                            boundsBehavior: Flickable.StopAtBounds
                                            clip: true

                                            TextEdit {
                                                id: frameDataText
                                                width: Math.max(frameDataFlickable.width, contentWidth)
                                                height: Math.max(frameDataFlickable.height, contentHeight)
                                                readOnly: true
                                                selectByMouse: true
                                                activeFocusOnPress: true
                                                textFormat: TextEdit.RichText
                                                wrapMode: TextEdit.NoWrap
                                                color: packetDetailView.frameNormalColor
                                                selectedTextColor: color
                                                selectionColor: FluTools.withOpacity(FluTheme.primaryColor, 0.5)
                                                renderType: Text.QtRendering
                                                font.family: "Consolas"
                                                font.pixelSize: 12
                                                text: {
                                                    if (!packetDetailView.packetDetails || packetDetailView.packetDetails.index === undefined) {
                                                        return qsTr("Select a packet to view raw data")
                                                    }

                                                    var rawDataHex = packetDetailView.packetDetails.rawData
                                                    if (!rawDataHex) {
                                                        return qsTr("No raw data available")
                                                    }

                                                    return packetDetailView.formatFrameData(rawDataHex)
                                                }
                                            }

                                            ScrollBar.vertical: FluScrollBar {}
                                            ScrollBar.horizontal: FluScrollBar {}
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.IBeamCursor
                                            acceptedButtons: Qt.RightButton
                                            onClicked: {
                                                if (frameDataText.text !== "") {
                                                    frameDataMenuLoader.popup()
                                                }
                                            }
                                        }

                                        FluLoader {
                                            id: frameDataMenuLoader
                                            function popup() {
                                                sourceComponent = frameDataMenu
                                            }
                                        }

                                        Component {
                                            id: frameDataMenu
                                            FluTextBoxMenu {
                                                inputItem: frameDataText
                                                Component.onCompleted: popup()
                                                onClosed: frameDataMenuLoader.sourceComponent = undefined
                                            }
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
            Layout.margins: 10
            Layout.topMargin: 5
            padding: 5
            radius: 8
            border.width: 0
            color: FluTheme.dark ? Qt.rgba(30/255, 30/255, 30/255, 1) : Qt.rgba(238/255, 238/255, 238/255, 1)

            RowLayout {
                anchors.fill: parent
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
                    text: "Invalid: " + (captureController.totalPackets - captureController.matchedPackets)
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
