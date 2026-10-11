import QtQuick
import QtQuick.Controls.Basic
import FluentUI 1.0

// Keep the standard popup lifecycle while sharing Fluent surfaces and typography.
Dialog {
    id: dialog
    padding: 24
    topPadding: 16
    modal: true
    focus: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    background: Rectangle {
        radius: 8
        color: FluTheme.dark ? "#2b2b2b" : "#ffffff"
        border.color: FluTheme.dark ? "#404040" : "#dedede"
        FluShadow {}
    }
    header: FluText {
        text: dialog.title
        font: FluTextStyle.Subtitle
        wrapMode: Text.WordWrap
        leftPadding: 24
        rightPadding: 24
        topPadding: 24
        bottomPadding: 8
    }
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, FluTheme.dark ? 0.45 : 0.25) }
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: FluTheme.animationEnabled ? 120 : 0 } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: FluTheme.animationEnabled ? 80 : 0 } }
}
