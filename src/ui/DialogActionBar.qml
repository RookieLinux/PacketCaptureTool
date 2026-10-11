import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import FluentUI 1.0

Control {
    default property alias actions: actionRow.data
    padding: 24
    topPadding: 16
    bottomPadding: 16
    background: Rectangle {
        radius: 8
        color: FluTheme.dark ? "#202020" : "#f3f3f3"
        Rectangle {
            width: parent.width
            height: 8
            color: parent.color
        }
        Rectangle {
            width: parent.width
            height: 1
            color: FluTheme.dark ? "#3b3b3b" : "#e5e5e5"
        }
    }
    contentItem: RowLayout {
        id: actionRow
        spacing: 8
        Item { Layout.fillWidth: true }
    }
}
