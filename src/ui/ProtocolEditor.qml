import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import FluentUI 1.0

StyledDialog {
    id: editor
    objectName: "protocolEditor"
    title: qsTr("协议编辑器")
    width: Math.min(1000,parent ? parent.width-32 : 1000)
    height: Math.min(680,parent ? parent.height-24 : 680)
    property var host
    property var draft: ({fields:[],length:{type:"fixed",fixedValue:4}})
    property var preview: ({})
    property bool dirty: false
    property int activeMode: 0
    property int advancedIndex: -1
    function source() { return activeMode === 1 ? jsonSource.text : JSON.stringify(draft) }
    function changed() { dirty=true; preview=({}) }
    function loadDraft(text) {
        var data=JSON.parse(text)
        if (!data || !data.length || !Array.isArray(data.fields)) throw new Error(qsTr("需要 length 对象及 fields 数组"))
        draft=data; jsonSource.text=JSON.stringify(data,null,2); preview=({})
    }
    function selectMode(next) {
        if (activeMode === 1) {
            var checked=captureController.previewProtocolDraft(jsonSource.text,"")
            if (!checked.validConfig) { preview=checked; mode.currentIndex=activeMode; return }
            try { loadDraft(jsonSource.text) } catch(e) { preview=({error:qsTr("JSON 不能转换为表单：")+e}); mode.currentIndex=activeMode; return }
        }
        if (next === 1) jsonSource.text=JSON.stringify(draft,null,2)
        activeMode=next
    }
    function updateField(index,key,value) { draft.fields[index][key]=value; changed() }
    function inspect() { preview=captureController.previewProtocolDraft(source(),sampleHex.text) }
    onOpened: {
        if (!dirty) { loadDraft(captureController.protocolDraft()); mode.currentIndex=0; activeMode=0 }
    }
    FileDialog {
        id: openDraft
        currentFolder: settingsStorage.directoryUrl
        title:qsTr("导入协议草稿")
        nameFilters:["JSON (*.json)"]
        onAccepted: {
            var result=captureController.readProtocolDraft(host.localFilePath(selectedFile))
            if (result.error) { editor.preview=({error:result.error}); return }
            editor.loadDraft(result.json); editor.dirty=true
        }
    }
    FileDialog {
        id: saveDraft
        currentFolder: settingsStorage.directoryUrl
        title:qsTr("保存协议草稿")
        nameFilters:["JSON (*.json)"]
        fileMode:FileDialog.SaveFile
        onAccepted: {
            if (captureController.saveProtocolDraft(editor.source(),host.localFilePath(selectedFile))) {
                editor.preview=({error:qsTr("协议已保存；点击应用可用于当前会话")})
            } else editor.preview=({error:captureController.lastError})
        }
    }
    ColumnLayout {
        anchors.fill:parent
        spacing:12
        RowLayout {
            FluComboBox { id:mode; model:[qsTr("协议表单"),qsTr("JSON 源码"),qsTr("Hex 预览")]; onActivated:editor.selectMode(currentIndex); Accessible.name:qsTr("编辑器视图") }
            FluButton { text:qsTr("导入 JSON"); onClicked:openDraft.open() }
            FluButton { text:qsTr("重置为当前配置"); onClicked: { editor.loadDraft(captureController.protocolDraft()); editor.dirty=false; mode.currentIndex=0; editor.activeMode=0 } }
            Item { Layout.fillWidth:true }
            FluText { font:FluTextStyle.Caption; color:FluTheme.fontSecondaryColor; text:editor.dirty ? qsTr("草稿已修改，关闭后仍保留") : qsTr("当前配置副本") }
        }
        StackLayout {
            Layout.fillWidth:true
            Layout.fillHeight:true
            currentIndex:editor.activeMode
            ColumnLayout {
                spacing:12
                RowLayout {
                    FluText { text:qsTr("名称") }
                    FluTextBox { Layout.fillWidth:true; text:editor.draft.protocolName || ""; onTextEdited: { editor.draft.protocolName=text; editor.changed() } Accessible.name:qsTr("协议名称") }
                    FluComboBox { model:["UDP","TCP"]; currentIndex:editor.draft.transportType === "TCP" ? 1 : 0; onActivated: { editor.draft.transportType=currentText; editor.changed() } Accessible.name:qsTr("协议传输类型") }
                    FluText { text:qsTr("端口") }
                    FluTextBox { Layout.preferredWidth:110; cleanEnabled:false; text:String(editor.draft.port || 8080); validator:IntValidator { bottom:1; top:65535 } onTextEdited: { editor.draft.port=acceptableInput ? Number(text) : 0; editor.changed() } Accessible.name:qsTr("协议端口") }
                }
                RowLayout {
                    FluText { text:qsTr("消息长度") }
                    FluComboBox { model:[qsTr("固定长度"),qsTr("长度字段")]; currentIndex:editor.draft.length.type === "variable" ? 1 : 0; onActivated: { editor.draft.length.type=currentIndex ? "variable" : "fixed"; if (!currentIndex && !editor.draft.length.fixedValue) editor.draft.length.fixedValue=4; editor.draft=Object.assign({},editor.draft); editor.changed() } Accessible.name:qsTr("消息长度类型") }
                    FluSpinBox { visible:editor.draft.length.type === "fixed"; from:1; to:65536; editable:true; value:editor.draft.length.fixedValue || 4; onValueModified: { editor.draft.length.fixedValue=value; editor.changed() } Accessible.name:qsTr("固定消息长度") }
                    FluTextBox { visible:editor.draft.length.type === "variable"; Layout.fillWidth:true; text:editor.draft.length.fieldName || ""; placeholderText:qsTr("已定义的无符号整数字段名"); onTextEdited: { editor.draft.length.fieldName=text; editor.changed() } Accessible.name:qsTr("消息长度字段名") }
                    Item { Layout.fillWidth:true; visible:editor.draft.length.type === "fixed" }
                    FluButton { text:qsTr("添加字段"); disabled:editor.draft.fields.length >= 1024; onClicked: {
                        editor.draft.fields=editor.draft.fields.concat([{name:qsTr("字段")+(editor.draft.fields.length+1),offset:0,length:1,type:"uint8",endianness:"big"}]); editor.draft=Object.assign({},editor.draft); editor.changed()
                    } }
                }
                FluText { Layout.fillWidth:true; font:FluTextStyle.Caption; color:FluTheme.fontSecondaryColor; wrapMode:Text.Wrap; text:qsTr("字段偏移以应用负载为起点；长度字段表示完整消息字节数。高级设置支持位域、枚举、比例、偏置和单位。") }
                RowLayout {
                    Layout.fillWidth:true; Layout.rightMargin:12; spacing:8
                    FluText { Layout.fillWidth:true; text:qsTr("字段名称"); font:FluTextStyle.Caption; color:FluTheme.fontSecondaryColor }
                    FluText { Layout.preferredWidth:70; text:qsTr("字节偏移"); font:FluTextStyle.Caption; color:FluTheme.fontSecondaryColor }
                    FluText { Layout.preferredWidth:70; text:qsTr("字节长度"); font:FluTextStyle.Caption; color:FluTheme.fontSecondaryColor }
                    FluText { Layout.preferredWidth:112; text:qsTr("类型"); font:FluTextStyle.Caption; color:FluTheme.fontSecondaryColor }
                    FluText { Layout.preferredWidth:100; text:qsTr("字节序"); font:FluTextStyle.Caption; color:FluTheme.fontSecondaryColor }
                    Item { Layout.preferredWidth:108 }
                }
                ListView {
                    id: fields
                    Layout.fillWidth:true; Layout.fillHeight:true; clip:true; spacing:8
                    model:editor.draft.fields
                    ScrollBar.vertical:FluScrollBar {}
                    delegate: RowLayout {
                        required property var modelData
                        required property int index
                        width:fields.width-12; height:36; spacing:8
                        FluTextBox { Layout.fillWidth:true; Layout.minimumWidth:80; text:modelData.name; placeholderText:qsTr("字段名"); onTextEdited:editor.updateField(index,"name",text); Accessible.name:qsTr("字段名称") }
                        FluTextBox { Layout.preferredWidth:70; cleanEnabled:false; text:String(modelData.offset); placeholderText:qsTr("偏移"); validator:IntValidator { bottom:0; top:65536 } onTextEdited:editor.updateField(index,"offset",acceptableInput ? Number(text) : -1); Accessible.name:qsTr("字段字节偏移") }
                        FluTextBox { Layout.preferredWidth:70; cleanEnabled:false; text:String(modelData.length); placeholderText:qsTr("长度"); validator:IntValidator { bottom:1; top:65536 } onTextEdited:editor.updateField(index,"length",acceptableInput ? Number(text) : -1); Accessible.name:qsTr("字段字节长度") }
                        FluComboBox { Layout.preferredWidth:112; model:["uint8","uint16","uint32","uint64","int8","int16","int32","int64","float32","float64","string","bytearray"]; currentIndex:model.indexOf(modelData.type); onActivated:editor.updateField(index,"type",currentText); Accessible.name:qsTr("字段数据类型") }
                        FluComboBox { Layout.preferredWidth:100; model:[qsTr("大端"),qsTr("小端")]; currentIndex:modelData.endianness === "little" ? 1 : 0; onActivated:editor.updateField(index,"endianness",currentIndex ? "little" : "big"); Accessible.name:qsTr("字段字节序") }
                        FluButton { objectName:"protocolFieldAdvanced_"+index; text:qsTr("高级"); onClicked: {
                            editor.advancedIndex=index
                            advancedSource.text=JSON.stringify({bitOffset:modelData.bitOffset || 0,bitWidth:modelData.bitWidth || 0,scale:modelData.scale === undefined ? 1 : modelData.scale,bias:modelData.bias || 0,unit:modelData.unit || "",enumValues:modelData.enumValues || {}},null,2)
                            advancedError.text=""; advancedDialog.open()
                        } }
                        FluButton { text:qsTr("删除"); onClicked: { var rows=editor.draft.fields.slice(); rows.splice(index,1); editor.draft.fields=rows; editor.draft=Object.assign({},editor.draft); editor.changed() } }
                    }
                }
            }
            ScrollView {
                clip:true
                ScrollBar.vertical:FluScrollBar {}
                FluMultilineTextBox { id:jsonSource; objectName:"protocolJsonSource"; textFormat:TextEdit.PlainText; wrapMode:TextEdit.WrapAnywhere; font.family:"Consolas"; onTextChanged:if (editor.activeMode === 1) editor.changed(); Accessible.name:qsTr("协议 JSON 源码") }
            }
            ColumnLayout {
                FluText { Layout.fillWidth:true; wrapMode:Text.Wrap; text:qsTr("输入单条消息的应用负载 Hex，不包含以太网 / IP / TCP / UDP 首部；支持连续 Hex、空格和换行。") }
                ScrollView {
                    Layout.fillWidth:true; Layout.preferredHeight:100; clip:true
                    FluMultilineTextBox { id:sampleHex; objectName:"protocolSampleHex"; placeholderText:qsTr("例如 00 00 00 2A"); onTextChanged:editor.preview=({}); Accessible.name:qsTr("预览应用负载 Hex") }
                }
                ListView {
                    Layout.fillWidth:true; Layout.fillHeight:true; clip:true; spacing:8
                    model:editor.preview.fields || []
                    ScrollBar.vertical:FluScrollBar {}
                    delegate: FluText {
                        required property var modelData
                        width:ListView.view.width-12; wrapMode:Text.WrapAnywhere
                        text:modelData.name + " · " + modelData.type + " @" + modelData.offset + " +" + modelData.length + qsTr("\n原始：") + modelData.raw + qsTr("    显示：") + modelData.value
                    }
                }
            }
        }
        FluText { Layout.fillWidth:true; wrapMode:Text.Wrap; maximumLineCount:3; elide:Text.ElideRight; color:editor.preview.validConfig === false || editor.preview.validPreview === false ? (FluTheme.dark ? "#ff9999" : "#b91c1c") : FluTheme.fontSecondaryColor; text:editor.preview.error || qsTr("验证与预览不影响当前会话；应用草稿会重新解析已有报文，保存 JSON 不会自动应用。") }
    }
    footer: DialogActionBar {
        FluButton { Layout.preferredWidth:104; Layout.preferredHeight:32; text:qsTr("关闭"); onClicked:editor.close() }
        FluButton { Layout.preferredWidth:112; Layout.preferredHeight:32; text:qsTr("保存 JSON"); onClicked: { editor.inspect(); if (editor.preview.validConfig) saveDraft.open() } }
        FluButton { objectName:"protocolPreviewButton"; Layout.preferredWidth:120; Layout.preferredHeight:32; text:qsTr("验证与预览"); onClicked:editor.inspect() }
        FluFilledButton { Layout.preferredWidth:112; Layout.preferredHeight:32; text:qsTr("应用草稿"); disabled:captureController.isLoading || captureController.isCapturing; onClicked: { if (captureController.applyProtocolDraft(editor.source())) editor.preview=({error:qsTr("已应用草稿并重新解析当前会话；请保存 JSON 以便下次使用")}); else editor.preview=({error:captureController.lastError}) } }
    }
    StyledDialog {
        id:advancedDialog
        objectName:"protocolAdvancedDialog"
        title:qsTr("字段高级设置")
        width:550; height:470
        ColumnLayout {
            anchors.fill:parent
            FluText { Layout.fillWidth:true; wrapMode:Text.Wrap; text:qsTr("bitOffset 从最低有效位开始，bitWidth=0 表示完整整数。显示值=原始值×scale+bias；enumValues 使用十进制字符串键，如 {\"1\":\"开启\"}。") }
            ScrollView { Layout.fillWidth:true; Layout.fillHeight:true; clip:true; FluMultilineTextBox { id:advancedSource; font.family:"Consolas" } }
            FluText { id:advancedError; Layout.fillWidth:true; wrapMode:Text.Wrap; color:FluTheme.dark ? "#ff9999" : "#b91c1c" }
        }
        footer:DialogActionBar {
            FluButton { Layout.preferredWidth:104; Layout.preferredHeight:32; text:qsTr("取消"); onClicked:advancedDialog.close() }
            FluFilledButton { Layout.preferredWidth:104; Layout.preferredHeight:32; text:qsTr("确认"); onClicked: {
                try {
                    var changes=JSON.parse(advancedSource.text)
                    if (!changes || Array.isArray(changes) || typeof changes !== "object" || Object.keys(changes).some(function(key) { return ["bitOffset","bitWidth","scale","bias","unit","enumValues"].indexOf(key) < 0 })) { advancedError.text=qsTr("仅允许编辑位域、枚举、比例、偏置和单位属性"); return }
                    var copy=JSON.parse(JSON.stringify(editor.draft))
                    copy.fields[editor.advancedIndex]=Object.assign({},copy.fields[editor.advancedIndex],changes)
                    var result=captureController.previewProtocolDraft(JSON.stringify(copy),"")
                    if (!result.validConfig) { advancedError.text=result.error; return }
                    editor.draft=copy; editor.changed(); advancedDialog.close()
                } catch(e) { advancedError.text=qsTr("JSON 无效：")+e }
            } }
        }
    }
}
