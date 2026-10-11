import QtQuick
import QtQuick.Controls.Basic
import FluentUI 1.0

FluComboBox {
    id: selector
    textRole: "displayName"
    Accessible.name: qsTr("采集网卡")
    Accessible.description: currentText
    property real popupWindowX: mapToItem(null, 0, 0).x
    property real expandedWidth: {
        var longest = 0
        for (var i = 0; i < count; ++i) {
            var item = model[i]
            longest = Math.max(longest, metrics.advanceWidth(item.description || item.name || ""),
                               metrics.advanceWidth(item.ipAddress || ""))
        }
        return Math.min(Math.max(width, 480, longest + 48), 760, Math.max(width, Window.width - 24))
    }
    FontMetrics { id: metrics; font: selector.font }
    contentItem: FluText {
        text: selector.displayText
        font: selector.font
        leftPadding: 10
        rightPadding: 4
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        color: selector.enabled ? FluTheme.fontPrimaryColor : FluTheme.fontSecondaryColor
    }
    delegate: FluItemDelegate {
        id: option
        required property var modelData
        required property int index
        objectName: "interfaceOption_" + index
        width: interfaceList.width
        horizontalPadding: 12
        verticalPadding: 10
        text: modelData.description || modelData.name || ""
        font: selector.font
        highlighted: selector.highlightedIndex === index
        hoverEnabled: true
        Accessible.name: modelData.displayName || text
        contentItem: Column {
            spacing: 4
            FluText {
                objectName: "interfaceName_" + option.index
                width: parent.width
                text: option.text
                font: selector.font
                wrapMode: Text.Wrap
            }
            FluText {
                width: parent.width
                text: option.modelData.ipAddress ? "IP · " + option.modelData.ipAddress : qsTr("无 IP 地址")
                font: FluTextStyle.Caption
                color: FluTheme.fontSecondaryColor
                wrapMode: Text.WrapAnywhere
            }
        }
    }
    popup: Popup {
        objectName: "interfacePopup"
        y: selector.height + 4
        x: Math.min(0, selector.Window.width - selector.popupWindowX - width - 12)
        width: selector.expandedWidth
        height: Math.min(420, interfaceList.contentHeight + topPadding + bottomPadding,
                         Math.max(100, selector.Window.height - 48))
        padding: 6
        topMargin: 36
        bottomMargin: 12
        leftMargin: 12
        rightMargin: 12
        modal: true
        dim: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        contentItem: ListView {
            id: interfaceList
            objectName: "interfaceOptions"
            clip: true
            model: selector.delegateModel
            currentIndex: selector.highlightedIndex
            highlightMoveDuration: 0
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: FluScrollBar {}
        }
        onOpened: {
            selector.forceActiveFocus()
            interfaceList.positionViewAtIndex(selector.currentIndex, ListView.Contain)
        }
        background: Rectangle {
            radius: 6
            color: FluTheme.dark ? "#2b2b2b" : "#ffffff"
            border.color: FluTheme.dark ? "#404040" : "#dedede"
            FluShadow { radius: 6 }
        }
        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: FluTheme.animationEnabled ? 83 : 0 } }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: FluTheme.animationEnabled ? 83 : 0 } }
    }
}
