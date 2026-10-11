import QtQuick
import QtQuick.Controls.Basic
import FluentUI 1.0

Button {
    id: control
    property bool menuOpen: false
    implicitWidth: Math.max(60, contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: 30
    leftPadding: 12
    rightPadding: 12
    topPadding: 0
    bottomPadding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    font: FluTextStyle.Body
    Accessible.name: text
    contentItem: FluText {
        text: control.text
        font: control.font
        color: FluTheme.fontPrimaryColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        color: control.down ? FluTheme.itemPressColor :
               control.hovered || control.menuOpen ? FluTheme.itemHoverColor : "transparent"
        border.width: control.visualFocus ? 1 : 0
        border.color: FluTheme.primaryColor
    }
}
