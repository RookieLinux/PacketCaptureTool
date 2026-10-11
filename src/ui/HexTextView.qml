import QtQuick
import QtQuick.Controls.Basic
import FluentUI 1.0

TextArea {
    id: control
    readOnly: true
    selectByMouse: true
    activeFocusOnPress: true
    textFormat: TextEdit.RichText
    wrapMode: TextEdit.NoWrap
    font: FluTextStyle.Body
    padding: 8
    leftPadding: 12
    renderType: Text.QtRendering
    color: FluTheme.fontPrimaryColor
    selectedTextColor: color
    selectionColor: FluTools.withOpacity(FluTheme.primaryColor, 0.5)

    function hexInRange(start, end) {
        if (start >= end) return ""
        // Use positions in the entire plain-text document, rather than stripping
        // selectedText: a partial selection can start in an address or ASCII.
        var lines = getText(0, length).replace(/[\u2028\u2029]/g, "\n").split("\n")
        var position = 0
        var bytes = []
        for (var i = 0; i < lines.length && position < end; ++i) {
            var line = lines[i]
            if (position + line.length > start && /^[0-9a-f]{8}  /i.test(line)) {
                var asciiStart = line.indexOf("|", 10)
                if (asciiStart >= 10) {
                    var hex = line.substring(10, asciiStart)
                    var bytePattern = /[0-9a-f]{2}/gi
                    var match
                    while ((match = bytePattern.exec(hex)) !== null) {
                        var byteStart = position + 10 + match.index
                        if (byteStart < end && byteStart + 2 > start)
                            bytes.push(match[0].toUpperCase())
                    }
                }
            }
            position += line.length + 1
        }
        return bytes.join(" ")
    }
    function copyHexSelection() {
        var hex = hexInRange(selectionStart, selectionEnd)
        if (hex) FluTools.clipText(hex)
    }
    function copyAllHex() {
        var hex = hexInRange(0, length)
        if (hex) FluTools.clipText(hex)
    }
    Keys.priority: Keys.BeforeItem
    Keys.onPressed: function(event) {
        if (event.matches(StandardKey.Copy)) {
            copyHexSelection()
            event.accepted = true
        }
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        cursorShape: Qt.IBeamCursor
        onClicked: function(mouse) { hexMenu.popup(mouse.x, mouse.y) }
    }
    FluMenu {
        id: hexMenu
        objectName: "hexCopyMenu"
        width: 220
        FluMenuItem {
            objectName: "hexCopySelectionAction"
            text: qsTr("复制报文 Hex 数据")
            enabled: control.hexInRange(control.selectionStart, control.selectionEnd) !== ""
            onTriggered: control.copyHexSelection()
        }
        FluMenuItem {
            objectName: "hexCopyAllAction"
            text: qsTr("复制全部 Hex 数据")
            enabled: control.hexInRange(0, control.length) !== ""
            onTriggered: control.copyAllHex()
        }
        FluMenuItem {
            text: qsTr("全选")
            onTriggered: control.selectAll()
        }
    }
}
