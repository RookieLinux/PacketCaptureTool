import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import FluentUI 1.0

RowLayout {
    id: controls
    spacing: 8
    property color foreground:FluTheme.fontPrimaryColor
    property color surface:FluTheme.dark ? "#252525" : "#ffffff"
    property var host
    property bool tableMode: true
    property var columnNames: ({
        sequence:qsTr("编号"), timestamp:qsTr("时间"), sourceIP:qsTr("源地址"), sourcePort:qsTr("源端口"),
        destIP:qsTr("目标地址"), destPort:qsTr("目标端口"), protocol:qsTr("协议"), length:qsTr("长度"),
        status:qsTr("解析状态"), error:qsTr("错误信息")
    })
    property var columns: [
        {key:"sequence",title:qsTr("编号"),width:72,visible:true}, {key:"timestamp",title:qsTr("时间"),width:118,visible:true},
        {key:"sourceIP",title:qsTr("源地址"),width:150,visible:true}, {key:"sourcePort",title:qsTr("源端口"),width:80,visible:true},
        {key:"destIP",title:qsTr("目标地址"),width:150,visible:true}, {key:"destPort",title:qsTr("目标端口"),width:80,visible:true},
        {key:"protocol",title:qsTr("协议"),width:100,visible:true}, {key:"length",title:qsTr("长度"),width:80,visible:true},
        {key:"status",title:qsTr("解析状态"),width:100,visible:true}, {key:"error",title:qsTr("错误信息"),width:200,visible:false}
    ]
    property var visibleColumns: columns.filter(function(c) { return c.visible }).map(function(c) {
        return {key:c.key, title:controls.columnNames[c.key] || c.title, width:c.width, visible:c.visible}
    })
    property var rules: []
    property bool anyRule: false
    property var operatorKeys: ["eq","ne","contains","gt","ge","lt","le","exists"]
    property var operatorNames: [qsTr("等于"),qsTr("不等于"),qsTr("包含"),qsTr("大于"),qsTr("大于等于"),qsTr("小于"),qsTr("小于等于"),qsTr("字段存在")]
    property var keys: ["sequence","timestamp","sourceIP","destIP","sourcePort","destPort","protocol","length","status","error"]
    property bool ready: false
    function quickFilter(name, operation, value) {
        var list = packetModel.conditions().slice()
        list.push({column:"field:"+name,operator:operation,value:String(value)})
        anyRule = false
        if (!packetModel.setConditions(list,false)) host.showError(packetModel.filterError)
    }
    function changeColumn(index, key, value) {
        if (key === "visible" && !value && visibleColumns.length === 1) return
        var list = columns.map(function(c) { return {key:c.key,title:c.title,width:c.width,visible:c.visible} })
        list[index][key] = value
        columns = list
        captureController.setPreference("columns",JSON.stringify(columns))
    }
    function moveColumn(index, delta) {
        var target = index + delta
        if (target < 0 || target >= columns.length) return
        var list = columns.slice(), item = list.splice(index,1)[0]
        list.splice(target,0,item)
        columns = list
        captureController.setPreference("columns",JSON.stringify(columns))
    }
    Component.onCompleted: {
        var p = captureController.preferences
        tableMode = p.tableMode === undefined ? true : p.tableMode === true || p.tableMode === "true"
        try {
            var saved = JSON.parse(p.columns || "null")
            if (Array.isArray(saved) && saved.length && saved.length <= 40 && saved.some(function(c) { return c.visible }) && saved.every(function(c) {
                return c && typeof c.key === "string" && (keys.indexOf(c.key) >= 0 || c.key.indexOf("field:") === 0) && typeof c.title === "string" && typeof c.visible === "boolean" && c.width >= 48 && c.width <= 600
            })) columns = saved
        } catch (e) {}
        archivePath.text = p.archiveDirectory || ""
        fileMiB.value = Math.max(1,Math.min(1024,Number(p.fileMiB)||64))
        fileSeconds.value = Math.max(1,Math.min(86400,Number(p.fileSeconds)||300))
        maxFiles.value = Math.max(1,Math.min(10000,Number(p.maxFiles)||1000))
        ready = true
    }
    FluCheckBox { text:qsTr("表格视图"); checked:controls.tableMode; onToggled: { controls.tableMode=checked; if (controls.ready) captureController.setPreference("tableMode",checked) } }
    FluButton { text:qsTr("配置列"); onClicked:columnsDialog.open() }
    FluButton { text:qsTr("条件筛选"); onClicked:conditionsDialog.open() }
    FluButton { text:qsTr("重置条件"); onClicked: { host.resetDisplayFilters(); controls.anyRule=false } }
    FluButton { text:qsTr("TCP 流分析"); onClicked: { tcpDialog.open(); captureController.analyzeTcpStreams() } }
    FluButton { text:captureController.archiveEnabled ? qsTr("自动落盘：已启用") : qsTr("自动落盘设置"); onClicked:archiveDialog.open() }
    Item { Layout.fillWidth:true }
    FluText { text:captureController.hasUnsavedChanges ? qsTr("● 未保存") : qsTr("已保存"); color:captureController.hasUnsavedChanges ? (FluTheme.dark ? "#f7c36b" : "#9d5d00") : FluTheme.fontSecondaryColor }
    FluButton { text:qsTr("快捷键 / 统计说明"); onClicked:helpDialog.open() }

    FolderDialog { id:archiveFolder; title:qsTr("选择自动落盘目录"); onAccepted:archivePath.text=host.localFilePath(selectedFolder) }
    TcpAnalysisDialog { id:tcpDialog }
    StyledDialog {
        id:archiveDialog; objectName:"archiveDialog"; parent:Overlay.overlay; anchors.centerIn:parent; modal:true; title:qsTr("自动落盘与文件轮转"); width:590
        ColumnLayout {
            width:parent.width
            spacing: 12
            FluText { Layout.fillWidth:true; wrapMode:Text.Wrap; text:qsTr("启用前请保存并清空会话。内存保留最近报文，完整采集写入归档目录。达到文件上限或写入失败后停止采集，已有文件不会覆盖或删除。文件数上限按每次归档启动计。") }
            RowLayout {
                FluTextBox { id:archivePath; Layout.fillWidth:true; placeholderText:qsTr("归档目录"); Accessible.name:qsTr("归档目录") }
                FluButton { text:qsTr("选择目录"); onClicked:archiveFolder.open() }
            }
            RowLayout { FluText { text:qsTr("每文件 MiB") } FluSpinBox { id:fileMiB; from:1; to:1024; value:64; editable:true; Accessible.name:qsTr("轮转文件大小") } FluText { text:qsTr("或秒数") } FluSpinBox { id:fileSeconds; from:1; to:86400; value:300; editable:true; Accessible.name:qsTr("轮转间隔") } }
            RowLayout { FluText { text:qsTr("最多文件数") } FluSpinBox { id:maxFiles; from:1; to:10000; value:1000; editable:true; Accessible.name:qsTr("文件数量上限") } }
            FluText { Layout.fillWidth:true; wrapMode:Text.Wrap; text:qsTr("文件 ") + (captureController.captureHealth.files || []).length + qsTr(" · 已写入 ") + captureController.captureHealth.written + qsTr(" · 待写入 ") + captureController.captureHealth.pending }
            FluText { Layout.fillWidth:true; maximumLineCount:3; elide:Text.ElideMiddle; wrapMode:Text.Wrap; text:(captureController.captureHealth.files || []).join("\n") }
        }
        footer: DialogActionBar {
            FluButton { Layout.preferredWidth:120; Layout.preferredHeight:32; text:qsTr("关闭自动落盘"); enabled:!captureController.isCapturing && !captureController.isLoading; onClicked:if (captureController.configureArchive(false,archivePath.text)) archiveDialog.close() }
            FluButton { Layout.preferredWidth:104; Layout.preferredHeight:32; text:qsTr("关闭设置"); onClicked:archiveDialog.close() }
            FluFilledButton { Layout.preferredWidth:120; Layout.preferredHeight:32; text:qsTr("启用 / 更新"); enabled:!captureController.isCapturing && !captureController.isLoading; onClicked:if (captureController.configureArchive(true,archivePath.text,fileMiB.value,fileSeconds.value,maxFiles.value)) archiveDialog.close() }
        }
    }
    StyledDialog {
        id:conditionsDialog; objectName:"conditionDialog"; parent:Overlay.overlay; anchors.centerIn:parent; modal:true; title:qsTr("结构化条件筛选"); width:760; height:440
        onOpened: { controls.rules=packetModel.conditions().map(function(c) { return {column:c.column,operator:c.operator,value:c.value} }); mode.currentIndex=controls.anyRule ? 1 : 0 }
        ColumnLayout {
            anchors.fill:parent
            spacing: 12
            RowLayout {
                FluText { text:qsTr("组合方式") }
                FluComboBox { id:mode; model:[qsTr("全部满足（AND）"),qsTr("任一满足（OR）")]; onActivated:controls.anyRule=currentIndex === 1; Accessible.name:qsTr("条件组合方式") }
                FluButton { text:qsTr("添加条件"); enabled:controls.rules.length < 32; onClicked:controls.rules=controls.rules.concat([{column:"sourceIP",operator:"eq",value:""}]) }
                FluButton { text:qsTr("清除全部"); onClicked:controls.rules=[] }
            }
            FluText { Layout.fillWidth:true; wrapMode:Text.Wrap; text:qsTr("源/目标地址：sourceIP/destIP；端口：sourcePort/destPort；字段填写 field:字段名；状态 status：0 未解析、1 未匹配、2 成功、3 失败。") }
            ScrollView {
                Layout.fillWidth:true; Layout.fillHeight:true; clip:true
                ScrollBar.vertical: FluScrollBar {}
                ColumnLayout {
                    width:conditionsDialog.availableWidth
                    Repeater {
                        model:controls.rules
                        RowLayout {
                            required property var modelData
                            required property int index
                            FluTextBox { Layout.preferredWidth:200; text:modelData.column; placeholderText:qsTr("属性或 field:字段名"); onTextEdited:controls.rules[index].column=text; Accessible.name:qsTr("筛选属性或字段名") }
                            FluComboBox { Layout.preferredWidth:135; model:controls.operatorNames; currentIndex:controls.operatorKeys.indexOf(modelData.operator); onActivated:controls.rules[index].operator=controls.operatorKeys[currentIndex]; Accessible.name:qsTr("比较运算符") }
                            FluTextBox { Layout.fillWidth:true; text:String(modelData.value === undefined ? "" : modelData.value); onTextEdited:controls.rules[index].value=text; Accessible.name:qsTr("筛选值") }
                            FluButton { text:qsTr("删除"); onClicked: { var list=controls.rules.slice(); list.splice(index,1); controls.rules=list } }
                        }
                    }
                }
            }
            FluText { Layout.fillWidth:true; wrapMode:Text.Wrap; text:packetModel.filterError; color:FluTheme.dark ? "#ff9999" : "#b91c1c" }
        }
        footer: DialogActionBar {
            FluButton { Layout.preferredWidth:104; Layout.preferredHeight:32; text:qsTr("取消"); onClicked:conditionsDialog.close() }
            FluFilledButton { Layout.preferredWidth:104; Layout.preferredHeight:32; text:qsTr("应用"); onClicked:if (packetModel.setConditions(controls.rules,controls.anyRule)) conditionsDialog.close() }
        }
    }
    StyledDialog {
        id:columnsDialog; objectName:"columnsDialog"; parent:Overlay.overlay; anchors.centerIn:parent; modal:true; title:qsTr("表格列：显示、顺序与宽度"); width:600; height:500
        ColumnLayout {
            anchors.fill:parent
            spacing: 12
            ScrollView {
                Layout.fillWidth:true; Layout.fillHeight:true; clip:true
                ScrollBar.vertical: FluScrollBar {}
                ColumnLayout {
                    width:columnsDialog.availableWidth
                    Repeater {
                        model:controls.columns
                        RowLayout {
                            required property var modelData
                            required property int index
                            FluCheckBox { Layout.preferredWidth:160; text:controls.columnNames[modelData.key] || modelData.title; checked:modelData.visible; enabled:modelData.visible ? controls.visibleColumns.length > 1 : true; onToggled:controls.changeColumn(index,"visible",checked) }
                            FluTextBox { Layout.preferredWidth:90; cleanEnabled:false; text:String(modelData.width); validator:IntValidator { bottom:48; top:600 } onEditingFinished:if (acceptableInput) controls.changeColumn(index,"width",Number(text)); Accessible.name:(controls.columnNames[modelData.key] || modelData.title) + qsTr("列宽") }
                            FluButton { text:qsTr("上移"); enabled:index > 0; onClicked:controls.moveColumn(index,-1) }
                            FluButton { text:qsTr("下移"); enabled:index < controls.columns.length-1; onClicked:controls.moveColumn(index,1) }
                        }
                    }
                }
            }
            RowLayout {
                FluTextBox { id:fieldColumnName; Layout.fillWidth:true; placeholderText:qsTr("自定义协议字段名"); Accessible.name:qsTr("自定义字段列名称") }
                FluButton { text:qsTr("添加字段列"); enabled:fieldColumnName.text.trim() !== "" && controls.columns.length < 40; onClicked: {
                    var key="field:"+fieldColumnName.text.trim()
                    if (!controls.columns.some(function(c) { return c.key === key })) {
                        controls.columns=controls.columns.concat([{key:key,title:fieldColumnName.text.trim(),width:140,visible:true}]); captureController.setPreference("columns",JSON.stringify(controls.columns))
                    }
                    fieldColumnName.text=""
                } }
            }
            FluText { text:qsTr("点击列标题排序，再次点击切换升降序。") }
        }
        footer: DialogActionBar {
            FluFilledButton { Layout.preferredWidth:104; Layout.preferredHeight:32; text:qsTr("完成"); onClicked:columnsDialog.close() }
        }
    }
    StyledDialog {
        id:helpDialog; objectName:"helpDialog"; parent:Overlay.overlay; anchors.centerIn:parent; modal:true; title:qsTr("快捷键与统计口径"); width:570
        ColumnLayout { width:parent.width; spacing:20
        FluText { Layout.fillWidth:true; wrapMode:Text.Wrap; text:qsTr("Ctrl+O 导入；Ctrl+S 保存；Ctrl+F 搜索；F6 聚焦报文列表；方向键选择报文；Esc 清除搜索或取消导入。\n\n点击字段定位字节，点击 Hex 字节反向定位字段；字段右键可添加筛选条件。\n\n采集数是应用接受的有效报文，窗口数是当前保留数据，分析数是已进入列表的数据。驱动接收 / 丢包统计由 Npcap 提供，未采集时显示不可用，不等于应用拒收数。MiB 限制仅计算原始数据，解析数据另占内存。") }
        }
        footer: DialogActionBar {
            FluFilledButton { Layout.preferredWidth:104; Layout.preferredHeight:32; text:qsTr("知道了"); onClicked:helpDialog.close() }
        }
    }
}
