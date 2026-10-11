import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import FluentUI 1.0

FluWindow {
    id: mainWindow
    visible: true
    width: 1300
    height: 850
    title: qsTr("报文采集与分析")
    windowIcon: "qrc:/icons/packet-capture.ico"
    minimumWidth: 1000
    minimumHeight: 650

    property bool preferencesReady: false
    property bool closeToTray: true
    function showMainWindow() { showNormal(); raise(); requestActivate() }
    function exitApplication() {
        showMainWindow()
        if (captureController.isLoading) { captureController.cancelLoad(); closeDelay.start(); return }
        requestAction("close")
    }
    Binding { target: desktopIntegration; property: "canStart"; value: !startButton.disabled }
    Connections {
        target: desktopIntegration
        function onShowRequested() { mainWindow.showMainWindow() }
        function onToggleCaptureRequested() {
            if (captureController.isCapturing) captureController.stopCapture()
            else if (!startButton.disabled) startButton.clicked()
        }
        function onExitRequested() { mainWindow.exitApplication() }
    }
    Connections {
        target: settingsStorage
        function onErrorOccurred(message) { mainWindow.showError(message) }
    }
    function updateLanguageFonts() {
        var family = languageSettings.fontFamily
        FluTextStyle.family = family
        var styles = ["Caption", "Body", "BodyStrong", "Subtitle", "Title", "TitleLarge", "Display"]
        for (var i = 0; i < styles.length; ++i) {
            var old = FluTextStyle[styles[i]]
            FluTextStyle[styles[i]] = Qt.font({family:family, pixelSize:old.pixelSize, weight:old.weight})
        }
    }
    Connections {
        target: languageSettings
        function onLanguageChanged() { mainWindow.updateLanguageFonts() }
    }
    StyledDialog {
        id: aboutDialog
        objectName: "aboutDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: qsTr("关于软件")
        width: 460
        ColumnLayout {
            width: parent.width
            spacing: 14
            FluText { text: qsTr("报文采集与分析"); font: FluTextStyle.Subtitle }
            FluText { objectName:"aboutVersion"; text: qsTr("版本：") + Qt.application.version }
            FluText { text: qsTr("作者：Rookie") }
            FluText { text: qsTr("版权所有 © Rookie") }
            FluText { text: "GitHub"; font: FluTextStyle.BodyStrong }
            FluText {
                objectName: "githubLink"
                Layout.fillWidth: true
                textFormat: Text.RichText
                wrapMode: Text.WrapAnywhere
                text: '<a href="https://github.com/RookieLinux/PacketCaptureTool">https://github.com/RookieLinux/PacketCaptureTool</a>'
                onLinkActivated: function(link) { Qt.openUrlExternally(link) }
            }
        }
        footer: DialogActionBar {
            FluFilledButton { Layout.preferredWidth:104; Layout.preferredHeight:32; text:qsTr("关闭"); onClicked:aboutDialog.close() }
        }
    }
    property bool allowClose: false
    property string pendingAction: ""
    closeListener: function(event) {
        if (allowClose) return
        event.accepted = false
        if (closeToTray && desktopIntegration.trayAvailable) { hide(); return }
        if (captureController.isLoading) { captureController.cancelLoad(); closeDelay.start(); return }
        requestAction("close")
    }
    Timer { id: closeDelay; interval:100; repeat:true; onTriggered:if (!captureController.isLoading) { stop(); mainWindow.requestAction("close") } }
    function requestAction(action) {
        captureController.stopCapture()
        if (captureController.hasUnsavedChanges) { pendingAction=action; unsavedDialog.open() }
        else performAction(action)
    }
    function performAction(action) {
        pendingAction=""
        if (action === "clear") captureController.clearPackets()
        else if (action === "open") loadFileDialog.open()
        else if (action === "close") { remember("windowWidth",width); remember("windowHeight",height); remember("splitWidth",listFrame.width); captureController.flushPreferences(); allowClose=true; mainWindow.close(); Qt.quit() }
    }
    function setDarkMode(dark) { FluTheme.darkMode=dark ? FluThemeType.Dark : FluThemeType.Light }
    function remember(key,value) { if (preferencesReady) captureController.setPreference(key,value) }
    Timer { id: geometryDelay; interval:300; onTriggered: { mainWindow.remember("windowWidth",mainWindow.width); mainWindow.remember("windowHeight",mainWindow.height); mainWindow.remember("splitWidth",listFrame.width) } }
    onWidthChanged: if (preferencesReady) geometryDelay.restart()
    onHeightChanged: if (preferencesReady) geometryDelay.restart()
    Connections { target:FluTheme; function onDarkModeChanged() { mainWindow.remember("dark",FluTheme.dark) } }
    Shortcut { sequence:"Ctrl+O"; enabled:!captureController.isLoading; onActivated:mainWindow.requestAction("open") }
    Shortcut { sequence:"Ctrl+S"; onActivated: { captureController.stopCapture(); saveFileDialog.open() } }
    Shortcut { sequence:"Ctrl+F"; onActivated:searchBox.forceActiveFocus() }
    Shortcut { sequence:"F6"; onActivated:packetListView.focusList() }
    Shortcut { sequence:"Escape"; onActivated:if (captureController.isLoading) captureController.cancelLoad(); else searchBox.text="" }
    StyledDialog {
        id:unsavedDialog; objectName:"unsavedDialog"; parent:Overlay.overlay; anchors.centerIn:parent; modal:true; title:qsTr("当前会话有未保存报文"); width:500; closePolicy:Popup.NoAutoClose
        ColumnLayout {
            width:parent.width
            FluText { Layout.fillWidth:true; wrapMode:Text.Wrap; text:qsTr("保存后继续，或放弃未保存报文。已写入的自动归档文件不会删除。") }
        }
        footer: DialogActionBar {
            FluFilledButton { Layout.fillWidth:true; Layout.preferredWidth:128; Layout.preferredHeight:32; text:qsTr("保存后继续"); onClicked: { unsavedDialog.close(); saveFileDialog.open() } }
            FluButton { Layout.fillWidth:true; Layout.preferredWidth:128; Layout.preferredHeight:32; text:qsTr("放弃并继续"); onClicked: { unsavedDialog.close(); mainWindow.performAction(mainWindow.pendingAction) } }
            FluButton { objectName:"unsavedCancel"; Layout.fillWidth:true; Layout.preferredWidth:128; Layout.preferredHeight:32; text:qsTr("取消"); onClicked: { mainWindow.pendingAction=""; unsavedDialog.close() } }
        }
    }
    ProtocolEditor { id:protocolEditor; host:mainWindow }
    function resetDisplayFilters() {
        searchBox.text=""
        transportFilter.currentIndex=2
        statusFilter.currentIndex=0
        packetModel.clearConditions()
        packetModel.sortColumn("")
        packetListView.sortKey=""
    }
    function resetSelection() {
        packetListView.selectedSequence=0
        packetListView.currentIndex = -1
        packetDetailView.packetDetails = ({})
    }

    function endpoint(address, port) {
        if (!port) return address
        return (address.indexOf(":") >= 0 ? "[" + address + "]" : address) + ":" + port
    }

    Component.onCompleted: {
        FluApp.init(mainWindow)
        mainWindow.setHitTestVisible(titleLanguageAction)
        mainWindow.setHitTestVisible(titleAboutAction)
        mainWindow.setHitTestVisible(titleSettingsAction)
        updateLanguageFonts()
        FluTheme.nativeText = false
        var p=captureController.preferences
        closeToTray=p.closeToTray === undefined ? true : p.closeToTray === true || p.closeToTray === "true"
        width=Math.max(1000,Math.min(2400,Number(p.windowWidth)||1300))
        height=Math.max(650,Math.min(1600,Number(p.windowHeight)||850))
        listFrame.SplitView.preferredWidth=Math.max(300,Math.min(width-450,Number(p.splitWidth)||600))
        compactPackets.checked=p.compact === undefined ? true : p.compact === true || p.compact === "true"
        followPackets.checked=p.follow === true || p.follow === "true"
        packetDetailView.payloadOnly=p.payloadOnly === true || p.payloadOnly === "true"
        if (p.dark !== undefined) FluTheme.darkMode=p.dark === true || p.dark === "true" ? FluThemeType.Dark : FluThemeType.Light
        for (var i=0; i<interfaceSelector.model.length; i++) if (interfaceSelector.model[i].name === p.interface) interfaceSelector.currentIndex=i
        capacitySelector.currentIndex=p.capacityIndex === undefined ? 1 : Math.max(0,Math.min(2,Number(p.capacityIndex)||0))
        captureController.setBufferLimits([10000,50000,100000][capacitySelector.currentIndex],[16,64,128][capacitySelector.currentIndex])
        preferencesReady=true
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

    function refreshSelectedPacketDetails(force) {
        if (packetListView.changing) return
        if (packetListView.currentIndex >= 0 && packetListView.currentIndex < packetListView.count) {
            if (!force && packetDetailView.packetDetails.sequence === packetModel.cellValue(packetListView.currentIndex,"sequence")) return
            packetDetailView.packetDetails = packetModel.getPacketDetails(packetListView.currentIndex)
        } else packetDetailView.packetDetails=({})
    }

    // File dialogs
    FolderDialog {
        id: settingsDirectoryDialog
        title: qsTr("选择配置文件存放目录")
        currentFolder: settingsStorage.directoryUrl
        onAccepted: if (settingsStorage.setDirectory(selectedFolder)) mainWindow.showSuccess(qsTr("配置目录已更新，当前设置已迁移"))
    }
    StyledDialog {
        id: settingsDialog
        objectName: "settingsDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: qsTr("配置文件存放目录")
        width: 560
        ColumnLayout {
            width: parent.width
            spacing: 14
            FluText { Layout.fillWidth:true; text:qsTr("软件参数文件：settings.ini"); font:FluTextStyle.BodyStrong }
            FluText { objectName:"settingsDirectoryPath"; Layout.fillWidth:true; text:settingsStorage.directory; wrapMode:Text.WrapAnywhere }
            FluText { Layout.fillWidth:true; wrapMode:Text.Wrap; text:qsTr("更换目录会迁移当前软件设置，并作为协议配置文件打开、保存的默认目录。原目录中的文件会保留。") }
            RowLayout {
                FluButton { text:qsTr("更换目录…"); onClicked:settingsDirectoryDialog.open() }
                FluButton { text:qsTr("打开目录"); onClicked:if (!settingsStorage.openDirectory()) mainWindow.showError(qsTr("无法打开配置目录")) }
            }
        }
        footer: DialogActionBar { FluFilledButton { Layout.preferredWidth:104; Layout.preferredHeight:32; text:qsTr("关闭"); onClicked:settingsDialog.close() } }
    }
    FileDialog {
        id: configFileDialog
        currentFolder: settingsStorage.directoryUrl
        title: qsTr("选择协议配置文件")
        nameFilters: [qsTr("JSON 文件 (*.json)"), qsTr("所有文件 (*)")]
        onAccepted: {
            var filePath = mainWindow.localFilePath(selectedFile)
            captureController.loadProtocolConfig(filePath)
        }
    }

    FileDialog {
        id: saveFileDialog
        title: qsTr("保存当前内存窗口报文")
        nameFilters: [qsTr("PCAP 文件 (*.pcap)"), qsTr("所有文件 (*)")]
        onRejected: mainWindow.pendingAction=""
        fileMode: FileDialog.SaveFile
        onAccepted: {
            var filePath = mainWindow.localFilePath(selectedFile)
            if (captureController.savePackets(filePath)) {
                mainWindow.showSuccess(qsTr("报文已保存"))
                if (mainWindow.pendingAction) mainWindow.performAction(mainWindow.pendingAction)
            }
        }
    }

    FileDialog {
        id: loadFileDialog
        title: qsTr("导入抓包文件")
        nameFilters: [qsTr("抓包文件 (*.pcap *.pcapng)"), qsTr("所有文件 (*)")]
        onAccepted: {
            var filePath = mainWindow.localFilePath(selectedFile)
            captureController.loadPacketsAsync(filePath)
        }
    }

    Connections {
        target: captureController
        function onCapturingChanged() { desktopIntegration.setCapturing(captureController.isCapturing) }
        function onProtocolConfigLoaded(protocolName) {
            mainWindow.showSuccess(qsTr("已加载协议：") + protocolName, 2000)
            mainWindow.refreshSelectedPacketDetails(true)
        }

        function onFileLoadFinished(success, canceled) {
            if (success) mainWindow.showSuccess(qsTr("文件导入完成"), 2000)
            else if (canceled) mainWindow.showInfo(qsTr("已取消导入，当前会话已保留"), 2000)
        }

        function onErrorOccurred(errorMsg) {
            mainWindow.showError(errorMsg, 4000)
        }
    }

    Connections {
        target: packetModel
        function onDataChanged(topLeft, bottomRight, roles) {
            mainWindow.refreshSelectedPacketDetails(true)
        }

        function onRowsInserted(parent, first, last) {
            if (followPackets.checked) packetListView.positionViewAtEnd()
        }

        function onRowsRemoved(parent, first, last) { Qt.callLater(mainWindow.refreshSelectedPacketDetails) }
        function onLayoutChanged() { Qt.callLater(mainWindow.refreshSelectedPacketDetails) }

        function onModelReset() {
            mainWindow.resetSelection()
        }
    }

    appBar: FluAppBar {
        id: windowToolBar
        objectName: "topToolBar"
        titleVisible: false
        title: qsTr("报文采集与分析")
        darkText: qsTr("深色模式")
        lightText: qsTr("浅色模式")
        minimizeText: qsTr("最小化")
        restoreText: qsTr("还原")
        maximizeText: qsTr("最大化")
        closeText: qsTr("关闭")
        stayTopText: qsTr("窗口置顶")
        stayTopCancelText: qsTr("取消置顶")
        showDark: true
        showMinimize: true
        showMaximize: true
        showClose: true
        Row {
            id: titleBarActions
            objectName: "titleBarActions"
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height
            spacing: 0
            Image { width:20; height:20; anchors.verticalCenter:parent.verticalCenter; source:"qrc:/icons/packet-capture.ico" }
            Item { width:8; height:parent.height }
            FluText {
                objectName: "titleBarCaption"
                height: parent.height
                text: windowToolBar.title
                font: FluTextStyle.Body
                verticalAlignment: Text.AlignVCenter
                rightPadding: 16
            }
            Rectangle {
                width: 1
                height: 14
                anchors.verticalCenter: parent.verticalCenter
                color: FluTheme.dark ? "#454545" : "#dedede"
            }
            Item { width: 6; height: parent.height }
            TitleBarMenuButton {
                id: titleSettingsAction
                objectName: "settingsButton"
                height: parent.height
                menuOpen: settingsMenu.visible
                text: qsTr("设置")
                Accessible.name: text
                onClicked: settingsMenu.open()
                FluMenu {
                    id: settingsMenu
                    objectName: "settingsMenu"
                    y: parent.height
                    width: 270
                    FluMenuItem { objectName:"settingsDirectoryAction"; text:qsTr("配置文件存放目录…"); onTriggered:settingsDialog.open() }
                    FluMenuItem {
                        id: closeToTrayMenuItem
                        objectName:"closeToTrayAction"
                        text:qsTr("关闭窗口时收起到托盘")
                        checkable:true
                        checked:mainWindow.closeToTray
                        enabled:desktopIntegration.trayAvailable
                        contentItem: Item {
                            Row {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 5
                                anchors.right: parent.right
                                anchors.rightMargin: closeToTrayMenuItem.indicator.width + closeToTrayMenuItem.spacing + 5
                                spacing: closeToTrayMenuItem.iconSpacing
                                FluLoader {
                                    id: closeToTrayIcon
                                    sourceComponent: closeToTrayMenuItem.iconDelegate
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                FluText {
                                    width: parent.width - closeToTrayIcon.width - parent.spacing
                                    text: closeToTrayMenuItem.text
                                    font: closeToTrayMenuItem.font
                                    color: closeToTrayMenuItem.textColor
                                    elide: Text.ElideRight
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                        indicator: FluIcon {
                            x: closeToTrayMenuItem.width - width - closeToTrayMenuItem.rightPadding - 5
                            y: closeToTrayMenuItem.topPadding + (closeToTrayMenuItem.availableHeight - height) / 2
                            visible: closeToTrayMenuItem.checked
                            iconSource: FluentIcons.CheckMark
                            iconSize: 16
                        }
                        onTriggered: { mainWindow.closeToTray=checked; mainWindow.remember("closeToTray",checked) }
                    }
                    FluMenuItem { text:qsTr("收起到托盘"); enabled:desktopIntegration.trayAvailable; onTriggered:mainWindow.hide() }
                    FluMenuItem { text:qsTr("退出软件"); onTriggered:mainWindow.exitApplication() }
                }
            }
            TitleBarMenuButton {
                id: titleLanguageAction
                objectName: "languageButton"
                height: parent.height
                menuOpen: languageMenu.visible
                text: qsTr("语言")
                Accessible.name: text
                onClicked: languageMenu.open()
                FluMenu {
                    id: languageMenu
                    objectName: "languageMenu"
                    y: parent.height
                    FluMenuItem {
                        objectName: "chineseLanguageAction"
                        text: "简体中文"
                        checkable: true
                        autoExclusive: true
                        checked: languageSettings.language === "zh_CN"
                        onTriggered: languageSettings.setLanguage("zh_CN")
                    }
                    FluMenuItem {
                        objectName: "englishLanguageAction"
                        text: "English"
                        checkable: true
                        autoExclusive: true
                        checked: languageSettings.language === "en_US"
                        onTriggered: languageSettings.setLanguage("en_US")
                    }
                }
            }
            TitleBarMenuButton {
                id: titleAboutAction
                objectName: "aboutButton"
                height: parent.height
                text: qsTr("关于软件")
                Accessible.name: text
                onClicked: aboutDialog.open()
            }
        }
    }

    Control {
        id:mainControls
        anchors.fill:parent
        palette.window:FluTheme.dark ? "#252525" : "#ffffff"
        palette.windowText:FluTheme.dark ? "#eeeeee" : "#222222"
        palette.text:FluTheme.dark ? "#eeeeee" : "#222222"
        palette.buttonText:FluTheme.dark ? "#eeeeee" : "#222222"
        palette.button:FluTheme.dark ? "#383838" : "#eeeeee"
        palette.base:FluTheme.dark ? "#252525" : "#ffffff"
        palette.highlight:FluTheme.primaryColor
        palette.highlightedText:"#ffffff"
        contentItem:ColumnLayout {
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
                    text: qsTr("网卡")
                    font: FluTextStyle.Body
                }

                InterfaceSelector {
                    id: interfaceSelector
                    objectName: "interfaceSelector"
                    Layout.preferredWidth: mainWindow.width < 1150 ? 200 : 280
                    model: captureController.getInterfaces()
                    onActivated: mainWindow.remember("interface", model[currentIndex].name)
                }

                FluButton {
                    text: qsTr("协议配置")
                    Accessible.name:text
                    disabled: captureController.isLoading
                    onClicked: {
                        configFileDialog.open()
                    }
                }

                FluButton { text:qsTr("协议编辑"); onClicked:protocolEditor.open() }

                FluFilledButton {
                    id: startButton
                    text: qsTr("开始")
                    Accessible.name:text
                    disabled: captureController.isLoading || (captureController.bufferFull && !captureController.archiveEnabled) || captureController.isCapturing || interfaceSelector.currentText === ""
                    onClicked: {
                        var currentItem = interfaceSelector.model[interfaceSelector.currentIndex]
                        if (currentItem) {
                            captureController.startCapture(currentItem.name)
                        }
                    }
                }

                FluButton {
                    id: stopButton
                    text: qsTr("停止")
                    Accessible.name:text
                    disabled: !captureController.isCapturing
                    onClicked: {
                        captureController.stopCapture()
                    }
                }

                FluText {
                    text: qsTr("显示")
                    font: FluTextStyle.Body
                }

                FluComboBox {
                    id: transportFilter
                    model: ["UDP", "TCP", qsTr("全部")]
                    currentIndex: 2
                    onCurrentIndexChanged: {
                        if (languageSettings.changingLanguage) return
                        captureController.setTransportFilter(currentIndex)
                        mainWindow.resetSelection()
                    }
                }

                FluIconButton {
                    iconSource: FluentIcons.Save
                    text: qsTr("保存全部窗口报文（Ctrl+S）")
                    Accessible.name:text
                    onClicked: { captureController.stopCapture(); saveFileDialog.open() }
                }

                FluIconButton {
                    iconSource: FluentIcons.OpenFile
                    text: qsTr("导入抓包文件（Ctrl+O）")
                    Accessible.name:text
                    disabled: captureController.isLoading
                    onClicked: mainWindow.requestAction("open")
                }

                FluIconButton {
                    iconSource: FluentIcons.Delete
                    text: qsTr("清空会话")
                    Accessible.name:text
                    disabled: captureController.isLoading
                    onClicked: mainWindow.requestAction("clear")
                }

                Item {
                    Layout.fillWidth: true
                }
            }
        }

        RowLayout {
            visible: captureController.isLoading
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.bottomMargin: 8
            FluProgressBar { Layout.preferredWidth: 160; indeterminate: true }
            FluText { text: qsTr("后台读取与解析：") + captureController.loadedPackets + qsTr(" 包已读取") }
            Item { Layout.fillWidth: true }
            FluButton { text: qsTr("取消导入"); onClicked: captureController.cancelLoad() }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.bottomMargin: 8
            FluText { text: qsTr("容量") }
            FluComboBox {
                id: capacitySelector
                objectName: "capacitySelector"
                model: [qsTr("1万 / 16 MiB"), qsTr("5万 / 64 MiB"), qsTr("10万 / 128 MiB")]
                currentIndex: 1
                enabled: !captureController.isCapturing && !captureController.isLoading
                onActivated: {
                    var counts = [10000, 50000, 100000]
                    var sizes = [16, 64, 128]
                    if (captureController.setBufferLimits(counts[currentIndex], sizes[currentIndex])) mainWindow.remember("capacityIndex",currentIndex)
                    else {
                        currentIndex = captureController.capacityMiB === 16 ? 0 : captureController.capacityMiB === 128 ? 2 : 1
                    }
                }
            }
            FluCheckBox {
                objectName: "pauseDisplay"
                text: qsTr("暂停显示")
                checked: captureController.displayPaused
                onToggled: captureController.displayPaused = checked
            }
            FluText {
                text: qsTr("窗口：") + captureController.storedPackets + qsTr(" 包 · ") + captureController.storedMiB.toFixed(1) + " / " + captureController.capacityMiB + " MiB"
            }
            Item { Layout.fillWidth: true }
            FluText {
                visible: captureController.bufferFull
                color: FluTheme.dark ? "#f7c36b" : "#9d5d00"
                text: qsTr("容量已满") + (captureController.rejectedPackets ? qsTr(" · 拒收：") + captureController.rejectedPackets : "")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.bottomMargin: 8
            FluTextBox {
                id: searchBox
                objectName: "packetSearch"
                Layout.fillWidth: true
                Accessible.name: qsTr("报文搜索")
                placeholderText: qsTr("搜索 IP、端口、字段值或 Hex 字节（Ctrl+F）")
                onTextChanged: searchDelay.restart()
            }
            Timer {
                id: searchDelay
                interval: 200
                onTriggered: {
                    packetModel.setSearchText(searchBox.text)
                    mainWindow.resetSelection()
                }
            }
            FluComboBox {
                id: statusFilter
                objectName: "statusFilter"
                model: [qsTr("全部状态"), qsTr("未解析"), qsTr("未匹配"), qsTr("成功"), qsTr("失败")]
                onCurrentIndexChanged: {
                    if (languageSettings.changingLanguage) return
                    packetModel.setStatusFilter(currentIndex - 1)
                    mainWindow.resetSelection()
                }
            }
            FluCheckBox { id: compactPackets; text: qsTr("紧凑"); checked: true; onClicked:mainWindow.remember("compact",checked) }
            FluCheckBox { id: followPackets; text: qsTr("跟随最新"); onClicked:mainWindow.remember("follow",checked) }
            FluText { text: qsTr("可见：") + captureController.visiblePackets }
        }

        AnalysisControls { id:analysisControls; objectName:"analysisControls"; host:mainWindow; Layout.fillWidth:true; Layout.leftMargin:10; Layout.rightMargin:10; Layout.bottomMargin:5 }

        // Main content area - FluSplitLayout
        FluSplitLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 10
            Layout.topMargin: 0
            orientation: Qt.Horizontal

            // Left side: Packet list
            FluFrame {
                id:listFrame
                onWidthChanged:if (mainWindow.preferencesReady) geometryDelay.restart()
                SplitView.preferredWidth: 600
                SplitView.minimumWidth: 300
                Layout.fillHeight: true
                padding: 0
                radius: 8
                
                PacketBrowser {
                    id:packetListView
                    objectName:"packetBrowser"
                    anchors.fill:parent
                    anchors.margins: 1
                    anchors.topMargin: 5
                    model:packetModel
                    columns:analysisControls.visibleColumns
                    tableMode:analysisControls.tableMode
                    compact:compactPackets.checked
                    onCurrentIndexChanged:mainWindow.refreshSelectedPacketDetails()
                    onChangingChanged:if (!changing) Qt.callLater(mainWindow.refreshSelectedPacketDetails)
                }
            }

            // Right side: Packet details
            FluFrame {
                id: packetDetailFrame
                implicitHeight: 0
                SplitView.fillWidth: true
                SplitView.minimumWidth: 450
                Layout.fillHeight: true
                padding: 10
                radius: 8
                clip: true

                ColumnLayout {
                    id: packetDetailView
                    objectName: "packetDetails"
                    anchors.fill: parent
                    spacing: 10
                    property var packetDetails: ({})
                    property int highlightedOffset: -1
                    property int highlightedLength: 0
                    property bool payloadOnly: false
                    property var selectedFields: []
                    property string byteDescription: ""
                    onPayloadOnlyChanged:mainWindow.remember("payloadOnly",payloadOnly)
                    function selectByte(offset) {
                        var fields=packetModel.fieldsAtByte(packetListView.currentIndex,offset)
                        selectedFields=fields.map(function(f) { return f.name })
                        highlightedOffset=fields.length ? fields[0].offset : offset
                        highlightedLength=fields.length ? fields[0].length : 1
                        byteDescription=qsTr("帧偏移 ") + offset + "：" + (fields.length ? selectedFields.join("、") : qsTr("未定义字段或报文头"))
                    }
                    onPacketDetailsChanged: { highlightedOffset = -1; highlightedLength = 0; selectedFields=[]; byteDescription="" }
                    property string frameNormalColor: FluTheme.dark ? "#EDEBE9" : "#201F1E"
                    property string frameEthernetColor: FluTheme.dark ? "#FFD166" : "#8A5A00"
                    property string frameIpColor: FluTheme.dark ? "#7DD3FC" : "#0067B8"
                    property string frameTransportColor: FluTheme.dark ? "#F0ABFC" : "#B146C2"

                    function hexByte(rawDataHex, byteIndex) {
                        if (byteIndex * 2 + 2 > rawDataHex.length) return "00"
                        return rawDataHex.substring(byteIndex * 2, byteIndex * 2 + 2).toUpperCase()
                    }

                    function parseFrameLayers(rawDataHex) {
                        var layers = []
                        var network = packetDetails.networkOffset
                        var transport = packetDetails.transportOffset
                        var headers = packetDetails.headerLength || 0
                        if (network > 0) layers.push({ start: 0, end: network, color: frameEthernetColor })
                        if (network >= 0) layers.push({ start: network, end: transport >= 0 ? transport : headers, color: frameIpColor })
                        if (transport >= 0) layers.push({ start: transport, end: headers, color: frameTransportColor })
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

                        var start = payloadOnly ? (packetDetails.headerLength || 0) : 0
                        var byteCount = Math.floor(rawDataHex.length / 2)
                        if (payloadOnly && packetDetails.payloadLength >= 0)
                            byteCount = Math.min(byteCount, start + packetDetails.payloadLength)
                        for (var row = start; row < byteCount; row += 16) {
                            html += ("00000000" + row.toString(16).toUpperCase()).slice(-8) + "  "
                            var ascii = ""
                            for (var column = 0; column < 16; column++) {
                                var byteIndex = row + column
                                if (byteIndex < byteCount) {
                                    var color = frameByteColor(byteIndex, layers) || frameNormalColor
                                    var selected = byteIndex >= highlightedOffset && byteIndex < highlightedOffset + highlightedLength
                                    html += "<a href=\"byte:" + byteIndex + "\" style=\"text-decoration:none;color:" + color + ";" +
                                        (selected ? "background-color:" + (FluTheme.dark ? "#365A78" : "#CFE8FF") + ";" : "") + "\">" +
                                        hexByte(rawDataHex, byteIndex) + "</a> "
                                    var value = parseInt(hexByte(rawDataHex, byteIndex), 16)
                                    var character = value >= 32 && value <= 126 ? String.fromCharCode(value) : "."
                                    ascii += character.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
                                } else {
                                    html += "   "
                                }
                                if (column === 7) html += " "
                            }
                            html += " |" + ascii + "|\n"
                        }
                        html += "</pre>"
                        return html
                    }

                    FluText {
                        text: packetDetailView.packetDetails && packetDetailView.packetDetails.index !== undefined ?
                            qsTr("报文") + " #" +packetDetailView.packetDetails.sequence + qsTr(" 详情") :
                            qsTr("选择报文查看详情")
                        font: FluTextStyle.Subtitle
                        Layout.alignment: Qt.AlignLeft
                        leftPadding: 4
                    }

                    FluPivot {
                        id: detailPivot
                        objectName: "packetDetailsTabs"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        headerHeight: 40
                        font: FluTextStyle.BodyStrong

                        FluPivotItem {
                            title: qsTr("解析字段")
                            contentItem: Component {
                                Item {
                                    width: detailPivot.width
                                    height: detailPivot.height - detailPivot.headerHeight
                                    
                                    SplitView {
                                        id: parsedDetailsSplit
                                        objectName: "parsedDetailsSplit"
                                        anchors.fill: parent
                                        orientation: Qt.Vertical
                                        handle: Rectangle {
                                            id: hexSplitHandle
                                            objectName: "parsedHexResizeHandle"
                                            implicitHeight: 8
                                            color: SplitHandle.pressed || SplitHandle.hovered ?
                                                FluTools.withOpacity(FluTheme.primaryColor, 0.18) : "transparent"
                                            HoverHandler { cursorShape: Qt.SizeVerCursor }
                                            Rectangle {
                                                anchors.centerIn: parent
                                                width: 36
                                                height: 2
                                                radius: 1
                                                color: hexSplitHandle.SplitHandle.pressed || hexSplitHandle.SplitHandle.hovered ?
                                                    FluTheme.primaryColor : (FluTheme.dark ? "#606060" : "#c5c5c5")
                                            }
                                        }
                                        ScrollView {
                                            id: parsedFieldsScroll
                                            objectName: "parsedFieldsPane"
                                            SplitView.minimumHeight: 36
                                            SplitView.preferredHeight: Math.min(fieldsColumn.implicitHeight,
                                                parsedDetailsSplit.height * 0.45)
                                            clip: true
                                            contentWidth: availableWidth
                                            contentHeight: fieldsColumn.implicitHeight
                                            ScrollBar.vertical: FluScrollBar {}
                                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                        ColumnLayout {
                                            id: fieldsColumn
                                            width: parsedFieldsScroll.availableWidth
                                            spacing: 10

                                            FluText {
                                                Layout.fillWidth: true
                                                wrapMode: Text.WordWrap
                                                visible: packetDetailView.packetDetails.parseStatus === 0 || packetDetailView.packetDetails.parseStatus === 1
                                                text: packetDetailView.packetDetails.parseStatus === 0 ?
                                                    qsTr("加载协议配置可解析字段，原始数据如下。") :
                                                    qsTr("此报文未匹配当前协议配置。")
                                            }

                                            // Error message
                                            FluFrame {
                                                Layout.fillWidth: true
                                                implicitHeight: errorRow.implicitHeight + 20
                                                padding: 10
                                                color: FluTheme.dark ? Qt.rgba(68/255, 39/255, 38/255, 1) : Qt.rgba(253/255, 231/255, 233/255, 1)
                                                border.color: FluTheme.dark ? Qt.rgba(67/255, 39/255, 38/255, 1) : Qt.rgba(238/255, 217/255, 219/255, 1)
                                                visible: packetDetailView.packetDetails && packetDetailView.packetDetails.parseStatus === 3
                                                implicitWidth: 0

                                                RowLayout {
                                                    id: errorRow
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
                                                    implicitHeight:Math.max(44,fieldRow.implicitHeight+20)
                                                    id:fieldFrame
                                                    objectName:"parsedField_" + modelData.name
                                                    Layout.fillWidth: true
                                                    Accessible.role:Accessible.Button
                                                    Accessible.name:modelData.name + qsTr("，偏移 ") + modelData.offset
                                                    activeFocusOnTab:true
                                                    color:packetDetailView.selectedFields.indexOf(modelData.name) >= 0 ? FluTools.withOpacity(FluTheme.primaryColor,FluTheme.dark ? 0.18 : 0.10) : "transparent"
                                                    Keys.onReturnPressed: { packetDetailView.highlightedOffset=modelData.offset; packetDetailView.highlightedLength=modelData.length; packetDetailView.selectedFields=[modelData.name] }
                                                    padding: 10
                                                    radius: 4
                                                    implicitWidth: 0

                                                    TapHandler {
                                                        onTapped: {
                                                            packetDetailView.highlightedOffset = modelData.offset
                                                            packetDetailView.highlightedLength = modelData.length
                                                            packetDetailView.selectedFields=[modelData.name]
                                                        }
                                                    }

                                                    MouseArea {
                                                        anchors.fill:parent
                                                        acceptedButtons:Qt.RightButton
                                                        z:1
                                                        onClicked:function(mouse) { fieldMenu.popup(mouse.x,mouse.y) }
                                                    }
                                                    Keys.onMenuPressed:fieldMenu.popup()
                                                    FluMenu {
                                                        id:fieldMenu
                                                        objectName:"fieldContextMenu"
                                                        parent:fieldFrame
                                                        FluMenuItem { objectName:"fieldFilterEquals"; text:qsTr("筛选：等于此值"); onTriggered:analysisControls.quickFilter(modelData.name,"eq",modelData.value) }
                                                        FluMenuItem { text:qsTr("筛选：不等于此值"); onTriggered:analysisControls.quickFilter(modelData.name,"ne",modelData.value) }
                                                        FluMenuItem { text:qsTr("筛选：包含此值"); onTriggered:analysisControls.quickFilter(modelData.name,"contains",modelData.displayValue) }
                                                        FluMenuItem { text:qsTr("筛选：字段存在"); onTriggered:analysisControls.quickFilter(modelData.name,"exists","") }
                                                    }
                                                    RowLayout {
                                                        id:fieldRow
                                                        anchors.fill: parent
                                                        spacing:12
                                                        
                                                        FluText {
                                                            text: modelData.name || ""
                                                            font: FluTextStyle.BodyStrong
                                                            Layout.fillWidth: true
                                                            Layout.preferredWidth: 150
                                                            Layout.minimumWidth:60
                                                            elide:Text.ElideRight
                                                        }
                                                        
                                                        FluText {
                                                            text: (modelData.type || "") + " @" + modelData.offset + " +" + modelData.length
                                                            font: FluTextStyle.Caption
                                                            color: FluTheme.fontSecondaryColor
                                                            Layout.preferredWidth: implicitWidth
                                                            Layout.minimumWidth: implicitWidth
                                                        }
                                                        
                                                        FluCopyableText {
                                                            text: modelData.displayValue || modelData.value || ""
                                                            font: FluTextStyle.Body
                                                            color: FluTheme.primaryColor
                                                            Layout.fillWidth: true
                                                            Layout.preferredWidth: 250
                                                            Layout.minimumWidth:60
                                                            wrapMode:TextEdit.WrapAnywhere
                                                        }
                                                    }
                                                }
                                            }

                                            FluText {
                                                Layout.fillWidth: true
                                                visible: !packetDetailView.packetDetails || packetDetailView.packetDetails.index === undefined
                                                text: qsTr("选择报文查看字段")
                                                font: FluTextStyle.Body
                                                color: FluTheme.fontSecondaryColor
                                                wrapMode: Text.Wrap
                                            }
                                            FluText {
                                                Layout.fillWidth: true
                                                visible: packetDetailView.packetDetails && packetDetailView.packetDetails.index !== undefined &&
                                                    packetDetailView.packetDetails.isValid === true &&
                                                    (!packetDetailView.packetDetails.parsedFields || packetDetailView.packetDetails.parsedFields.length === 0)
                                                text: qsTr("协议已匹配，但配置未定义字段")
                                                font: FluTextStyle.Body
                                                color: FluTheme.fontSecondaryColor
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                        }
                                        ColumnLayout {
                                            objectName: "parsedHexPane"
                                            SplitView.fillHeight: true
                                            SplitView.minimumHeight: Math.max(160, payloadToggle.implicitHeight + byteHint.implicitHeight +
                                                offsetControls.implicitHeight + 3 * spacing + 64)
                                            spacing: 8
                                            FluCheckBox {
                                                id: payloadToggle
                                                text: qsTr("仅应用负载")
                                                checked: packetDetailView.payloadOnly
                                                onClicked: packetDetailView.payloadOnly = checked
                                            }
                                            FluText { id:byteHint; Layout.fillWidth:true; wrapMode:Text.Wrap; text:packetDetailView.byteDescription || qsTr("点击字段定位字节；点击 Hex 字节反向定位；字段右键可筛选。") }
                                            RowLayout {
                                                id: offsetControls
                                                FluText { text:qsTr("定位帧偏移") }
                                                FluSpinBox { from:0; to:Math.max(0,(packetDetailView.packetDetails.length || 1)-1); editable:true; Accessible.name:qsTr("字节偏移反向定位"); onValueModified:packetDetailView.selectByte(value) }
                                            }
                                            ScrollView {
                                                id: hexPreviewScroll
                                                objectName: "parsedHexPreview"
                                                Layout.fillWidth: true
                                                Layout.fillHeight: true
                                                Layout.minimumHeight: 64
                                                clip: true
                                                contentWidth: Math.max(availableWidth, frameHexText.contentWidth + frameHexText.leftPadding + frameHexText.rightPadding)
                                                contentHeight: Math.max(availableHeight, frameHexText.contentHeight + frameHexText.topPadding + frameHexText.bottomPadding)
                                                ScrollBar.vertical: FluScrollBar {}
                                                ScrollBar.horizontal: FluScrollBar {}
                                                HexTextView {
                                                    id: frameHexText
                                                    width: hexPreviewScroll.contentWidth
                                                    height: hexPreviewScroll.contentHeight
                                                    readOnly: true
                                                    selectByMouse: true
                                                    textFormat: TextEdit.RichText
                                                    wrapMode: TextEdit.NoWrap
                                                    objectName:"frameHex"
                                                    Accessible.name:qsTr("Hex 与 ASCII 原始数据，点击字节可定位字段")
                                                    onLinkActivated:function(link) { if (link.indexOf("byte:") === 0) packetDetailView.selectByte(Number(link.substring(5))) }
                                                    text: packetDetailView.formatFrameData(packetDetailView.packetDetails.rawData || "")
                                                    background: Rectangle { radius:4; color: FluTheme.dark ? "#252525" : "#FAFAFA"; border.color:FluTheme.dark ? "#404040" : "#e5e5e5" }
                                                }
                                            }

                                        }
                                    }
                                }
                            }
                        }

                        FluPivotItem {
                            title: qsTr("完整帧")
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

                                            HexTextView {
                                                id: frameDataText
                                                objectName: "fullFrameHex"
                                                background: null
                                                Accessible.name:qsTr("完整帧原始数据")
                                                onLinkActivated:function(link) { if (link.indexOf("byte:") === 0) packetDetailView.selectByte(Number(link.substring(5))) }
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
                                                        return qsTr("选择报文查看原始数据")
                                                    }

                                                    var rawDataHex = packetDetailView.packetDetails.rawData
                                                    if (!rawDataHex) {
                                                        return qsTr("暂无原始数据")
                                                    }

                                                    return packetDetailView.formatFrameData(rawDataHex)
                                                }
                                            }

                                            ScrollBar.vertical: FluScrollBar {}
                                            ScrollBar.horizontal: FluScrollBar {}
                                        }

                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        RowLayout {
            Layout.fillWidth:true; Layout.leftMargin:10; Layout.rightMargin:10
            FluText { font:FluTextStyle.Caption; color:FluTheme.fontSecondaryColor; text:qsTr("采集 ") + captureController.captureHealth.accepted + qsTr(" · 归档 ") + captureController.captureHealth.written + qsTr(" · 拒收 ") + captureController.rejectedPackets + qsTr(" · 无法识别 ") + captureController.captureHealth.invalid }
            Item { Layout.fillWidth:true }
            FluText { font:FluTextStyle.Caption; color:FluTheme.fontSecondaryColor; text:captureController.captureHealth.available ? qsTr("驱动接收 ") + captureController.captureHealth.received + qsTr(" / 丢包 ") + captureController.captureHealth.dropped + qsTr(" / 接口丢包 ") + captureController.captureHealth.interfaceDropped : qsTr("驱动统计不可用") }
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
                    text: captureController.isCapturing ? qsTr("状态：采集中") : qsTr("状态：已停止")
                    font: FluTextStyle.BodyStrong
                    color: captureController.isCapturing ? (FluTheme.dark ? "#66bb6a" : "#157a20") : (FluTheme.dark ? "#bbbbbb" : "#666666")
                }

                FluText {
                    text: qsTr("分析：") + captureController.totalPackets
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
                    color: FluTheme.dark ? "#e3b341" : "#806000"
                }

                FluText {
                    text: qsTr("成功：") + captureController.parsedPackets + qsTr("  失败：") + captureController.failedPackets
                    font: FluTextStyle.Caption
                    color: FluTheme.dark ? "#ff8c8c" : "#b91c1c"
                }

                Item { Layout.fillWidth: true }

                FluText {
                    text: qsTr("配置：") + (captureController.configLoaded ? captureController.configFileName : qsTr("无"))
                    font: FluTextStyle.Caption
                    elide: Text.ElideLeft
                    Layout.maximumWidth: 300
                }
            }
        }
        }
    }
}
